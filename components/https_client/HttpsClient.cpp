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

bool write_request(const char* request);

HttpsClient::HttpsClient(const char* server, const char* port)
    : server_(server), port_(port), connected_(false) {
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
bool HttpsClient::get(const char* path) {
    if (!connected_) return false;

    std::string request = "GET ";
    request += path;
    request += " HTTP/1.0\r\n";
    request += "Host: ";
    request += server_;
    request += "\r\nUser-Agent: esp-idf/1.0 esp32\r\n\r\n";

    return write_request(request.c_str());
}

// Simple POST request with JSON
bool HttpsClient::post(const char* path, const char* json_data) {
    if (!connected_) return false;

    int content_length = strlen(json_data);
    char request[512];
    int n = snprintf(request, sizeof(request),
                    "POST %s HTTP/1.0\r\n"
                    "Host: %s\r\n"
                    "User-Agent: %s\r\n"
                    "Content-Type: application/json\r\n"
                    "Content-Length: %d\r\n"
                    "\r\n"
                    "%s",
                    path,
                    server_,
                    CONFIG_USER_AGENT,  // <- replace the hardcoded string
                    content_length,
                    json_data);    

    if (n <= 0 || n >= (int)sizeof(request)) {
        ESP_LOGE(TAG, "POST request too large for buffer");
        return false;
    }

    return write_request(request);
}

bool write_request(const char* request) {
    size_t written = 0;
    int ret;
    size_t len = strlen(request);

    while (written < len) {
        ret = mbedtls_ssl_write(&ssl_, (const unsigned char*)request + written, len - written);
        if (ret >= 0) {
            written += ret;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -ret);
            return false;
        }
    }

    // Read and print response (optional)
    char buf[512];
    do {
        memset(buf, 0, sizeof(buf));
        ret = mbedtls_ssl_read(&ssl_, (unsigned char*)buf, sizeof(buf)-1);
        if (ret > 0) {
            printf("%.*s", ret, buf);
        } else if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
            break;
        } else if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
            ESP_LOGE(TAG, "mbedtls_ssl_read returned -0x%x", -ret);
            return false;
        }
    } while (true);

    return true;
}

