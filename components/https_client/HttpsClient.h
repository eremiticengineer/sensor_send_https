#pragma once
#include <string.h>
#include "mbedtls/ssl.h"

class HttpsClient {
public:
    HttpsClient(const char* server, const char* port);
    ~HttpsClient();

    bool connect();
    void disconnect();
    bool get(const char* path);
    bool post(const char* path, const char* json_data);

private:
    //bool write_request(const char* request);

    const char* server_;
    const char* port_;
    bool connected_;
};