#pragma once
#include "nvs_flash.h"
#include "nvs.h"
#include <string>

class NvsStorage {
public:
    NvsStorage(const std::string& nvsNamespace = "app_data");
    ~NvsStorage() = default;

    // Initialize NVS
    esp_err_t init();

    // Generic get/set string
    std::string getString(const std::string& key);
    esp_err_t setString(const std::string& key, const std::string& value);

private:
    std::string nvsNamespace;
};
