#include "NvsStorage.h"
#include "esp_log.h"

static const char* TAG = "NvsStorage";

NvsStorage::NvsStorage(const std::string& nvsNamespace)
    : nvsNamespace(nvsNamespace) {}

esp_err_t NvsStorage::init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    return err;
}

std::string NvsStorage::getString(const std::string& key) {
    nvs_handle handle;
    if (nvs_open(nvsNamespace.c_str(), NVS_READONLY, &handle) != ESP_OK) return "";

    size_t required_size = 0;
    if (nvs_get_str(handle, key.c_str(), nullptr, &required_size) != ESP_OK) {
        nvs_close(handle);
        return "";
    }

    char* buffer = new char[required_size];
    if (nvs_get_str(handle, key.c_str(), buffer, &required_size) != ESP_OK) {
        delete[] buffer;
        nvs_close(handle);
        return "";
    }

    std::string value(buffer);
    delete[] buffer;
    nvs_close(handle);
    return value;
}

esp_err_t NvsStorage::setString(const std::string& key, const std::string& value) {
    nvs_handle handle;
    esp_err_t err = nvs_open(nvsNamespace.c_str(), NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    err = nvs_set_str(handle, key.c_str(), value.c_str());
    if (err == ESP_OK) err = nvs_commit(handle);

    nvs_close(handle);
    return err;
}
