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

bool write_request(const char* request,
    std::string* out_body = nullptr,
    std::string* out_headers = nullptr);

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
    snprintf(request, sizeof(request),
             "GET %s HTTP/1.0\r\n"
             "Host: %s\r\n"
             "User-Agent: %s\r\n"
             "\r\n",
             path,
             server_,
             userAgent_);

    if (!write_request(request, out_body, out_headers)) {
        ESP_LOGE(TAG, "Failed to write request");
        return ESP_FAIL;
    }

    return ESP_OK;
}

// Simple POST request with JSON
bool HttpsClient::post(const char* path, const char* apiKey, const char* json_data,
        std::string* out_body, std::string* out_headers) {
    if (!connected_) return false;

    int content_length = strlen(json_data);
    char request[512];
    int n = snprintf(request, sizeof(request),
                    "POST %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "User-Agent: %s\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n"
                    "X-API-KEY: %s\r\n"
                    "\r\n"
                    "%s",
                    path,
                    server_,
                    userAgent_,
                    content_length,
                    apiKey,
                    json_data);

    if (n <= 0 || n >= (int)sizeof(request)) {
        ESP_LOGE(TAG, "POST request too large for buffer");
        return false;
    }

    return write_request(request, out_body, out_headers);
}

bool write_request(const char* request,
                   std::string* out_body,
                   std::string* out_headers)
{
    // Write the full request
    size_t written = 0;
    size_t len = strlen(request);
    int ret;
    while (written < len) {
        ret = mbedtls_ssl_write(&ssl_, (const unsigned char*)request + written, len - written);
        if (ret >= 0) {
            written += ret;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
                   ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -ret);
            return false;
        }
    }

    // Read the full response into out_body if provided
    if (out_body) out_body->clear();
    char buf[512];
    while (true) {
        ret = mbedtls_ssl_read(&ssl_, reinterpret_cast<unsigned char*>(buf), sizeof(buf));
        if (ret > 0) {
            if (out_body) out_body->append(buf, ret);
        } else if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            break; // EOF or TLS clean close
        } else if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        } else {
            ESP_LOGE(TAG, "mbedtls_ssl_read error: %d", ret);
            return false;
        }
    }

    if (!out_body || out_body->empty()) {
        if (out_headers) out_headers->clear();
        return true; // nothing to split
    }

    // Split headers and body
    size_t pos = out_body->find("\r\n\r\n");
    if (pos == std::string::npos) {
        pos = out_body->find("\n\n"); // fallback if server uses just LF
    }

    if (pos != std::string::npos) {
        if ((out_headers) && (out_body)) {
            out_headers->assign(*out_body, 0, pos);
        }
        // skip delimiter
        pos += (out_body->compare(pos, 4, "\r\n\r\n") == 0 ? 4 : 2);
        *out_body = out_body->substr(pos);
    } else {
        // no headers found, treat entire response as body
        if (out_headers) out_headers->clear();
    }

    // Null-terminate body for cJSON if needed
    out_body->push_back('\0');

    return true;
}

