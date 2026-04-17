#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "esp_log.h"
#include <string>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/error.h"
#ifdef CONFIG_MBEDTLS_SSL_PROTO_TLS1_3
#include "psa/crypto.h"
#endif
#include "esp_crt_bundle.h"

#include "HttpsClient.h"

#include "sdkconfig.h"

static constexpr const char* TAG = "HttpsClient";

mbedtls_ssl_context ssl_;
mbedtls_x509_crt cacert_;
mbedtls_ssl_config conf_;
mbedtls_net_context server_fd_;

enum class HttpPhase {
    HeadersOnly,
    FullRequest
};

bool write_request(const uint8_t* request,
                   size_t len,
                   std::string* out_body,
                   std::string* out_headers,
                   HttpPhase phase);

HttpsClient::HttpsClient(const char* server, const char* port, const char* userAgent)
    : server_(server), port_(port), userAgent_(userAgent), connected_(false) {
    mbedtls_ssl_init(&ssl_);
    mbedtls_x509_crt_init(&cacert_);
    mbedtls_ssl_config_init(&conf_);
    mbedtls_net_init(&server_fd_);
}

HttpsClient::~HttpsClient() {
    disconnect();
    mbedtls_ssl_free(&ssl_);
    mbedtls_x509_crt_free(&cacert_);
    mbedtls_ssl_config_free(&conf_);
    mbedtls_net_free(&server_fd_);
}

// Establish SSL/TLS connection
bool HttpsClient::connect() {
    int ret = 0;

    if ((ret = esp_crt_bundle_attach(&conf_)) < 0) {
        ESP_LOGE(TAG, "esp_crt_bundle_attach returned -0x%x", -ret);
        return false;
    }

    if ((ret = mbedtls_ssl_set_hostname(&ssl_, server_)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
        return false;
    }

    if ((ret = mbedtls_ssl_config_defaults(&conf_,
                                            MBEDTLS_SSL_IS_CLIENT,
                                            MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        return false;
    }

    mbedtls_ssl_conf_authmode(&conf_, MBEDTLS_SSL_VERIFY_REQUIRED);
    mbedtls_ssl_conf_ca_chain(&conf_, &cacert_, nullptr);

#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&conf_, CONFIG_MBEDTLS_DEBUG_LEVEL);
#endif

    if ((ret = mbedtls_ssl_setup(&ssl_, &conf_)) != 0) {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x", -ret);
        return false;
    }

    if ((ret = mbedtls_net_connect(&server_fd_, server_, port_, MBEDTLS_NET_PROTO_TCP)) != 0) {
        ESP_LOGE(TAG, "mbedtls_net_connect returned -0x%x", -ret);
        return false;
    }

    mbedtls_ssl_set_bio(&ssl_, &server_fd_, mbedtls_net_send, mbedtls_net_recv, nullptr);

    while ((ret = mbedtls_ssl_handshake(&ssl_)) != 0) {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
            return false;
        }
    }

    int flags = mbedtls_ssl_get_verify_result(&ssl_);
    if (flags != 0) {
        char buf[512];
        mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
        ESP_LOGW(TAG, "Failed to verify peer certificate: %s", buf);
    }

    connected_ = true;
    return true;
}

// Disconnect and cleanup
void HttpsClient::disconnect() {
    if (connected_) {
        mbedtls_ssl_close_notify(&ssl_);
        mbedtls_ssl_session_reset(&ssl_);
        mbedtls_net_free(&server_fd_);
        connected_ = false;
    }
}

// Simple GET request
esp_err_t HttpsClient::get(const char* path,
    std::string* out_body,
    std::string* out_headers) {
    if (!connected_) return ESP_ERR_INVALID_STATE;

    char request[512];
    int requestLength = snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: %s\r\n"
             "\r\n",
             path,
             server_,
             userAgent_);

    if (!write_request((const uint8_t*)request, requestLength,
        out_body, out_headers, HttpPhase::FullRequest)) {
        ESP_LOGE(TAG, "Failed to write request");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Simple POST request with JSON
bool HttpsClient::post(const char* path, const char* apiKey, const uint8_t* data,
    size_t dataLength, std::string* out_body, std::string* out_headers) {
    if (!connected_) return false;

    char headers[512];
    int headersLength = snprintf(headers, sizeof(headers),
                    "POST %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "User-Agent: %s\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n"
                    "X-API-KEY: %s\r\n"
                    "\r\n",
                    path,
                    server_,
                    userAgent_,
                    (unsigned)dataLength,
                    apiKey);

    ESP_LOGI(TAG, "LENGTH: %d", headersLength);

    if (headersLength <= 0 || headersLength >= (int)sizeof(headers)) {
        ESP_LOGE(TAG, "POST request too large for buffer");
        return false;
    }

    // Send the headers...
    bool result = write_request((const uint8_t*)headers, headersLength,
        out_body, out_headers, HttpPhase::HeadersOnly);
    ESP_LOGI(TAG, "POST headers result = %d", result);

    // ...then the body
    result = write_request((const uint8_t*)data, dataLength,
        out_body, out_headers, HttpPhase::FullRequest);
    ESP_LOGI(TAG, "POST body result = %d", result);

    return result;
}



bool write_request(const uint8_t* request,
                   size_t len,
                   std::string* out_body,
                   std::string* out_headers,
                   HttpPhase phase)
{
    size_t written = 0;
    int ret;

    // -------------------------
    // SEND REQUEST
    // -------------------------
    while (written < len) {
        ret = mbedtls_ssl_write(&ssl_, request + written, len - written);

        if (ret > 0) {
            written += ret;
        }
        else if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
                 ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
        }
        else {
            ESP_LOGE(TAG, "ssl_write error: -0x%x", -ret);
            return false;
        }
    }

    /* Get is a FullRequest where a response is elicited but
     * POST is sent headers first HeadersOnly then the body FullRequest
     */
    if (phase != HttpPhase::FullRequest) {
        return true;
    }

    // -------------------------
    // READ RESPONSE (RAW)
    // -------------------------
    std::string response;
    char buf[512];

    while (true) {
        ret = mbedtls_ssl_read(&ssl_, (unsigned char*)buf, sizeof(buf));

        if (ret > 0) {
            response.append(buf, ret);
        }
        else if (ret == 0 ||
                 ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            break;
        }
        else if (ret == MBEDTLS_ERR_SSL_WANT_READ ||
                 ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }
        else {
            ESP_LOGE(TAG, "ssl_read error: %d", ret);
            return false;
        }
    }

    // -------------------------
    // SPLIT HEADERS / BODY
    // -------------------------
    size_t pos = response.find("\r\n\r\n");

    if (pos == std::string::npos) {
        if (out_headers) out_headers->clear();
        if (out_body) *out_body = std::move(response);
        return true;
    }

    if (out_headers) {
        *out_headers = response.substr(0, pos);
    }

    if (out_body) {
        *out_body = response.substr(pos + 4);
    }

    return true;
}
