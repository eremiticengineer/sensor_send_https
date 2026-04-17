#pragma once
#include <string>
#include "mbedtls/ssl.h"

class HttpsClient {
public:
    HttpsClient(const char* server, const char* port, const char* userAgent);
    ~HttpsClient();

    bool connect();
    void disconnect();
    esp_err_t get(const char* path,
        std::string* out_body = nullptr,
        std::string* headers = nullptr);

    bool post(const char* path,
        const char* apiKey,
        const uint8_t* data,
        size_t dataLength,
        std::string* out_body,
        std::string* out_headers);

private:
    //bool write_request(const char* request);

    const char* server_;
    const char* port_;
    const char* userAgent_;
    bool connected_;
};