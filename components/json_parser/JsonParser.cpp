#include "JsonParser.h"
#include "esp_log.h"

static const char* TAG = "JsonParser";

bool JsonParser::parse(const std::string& jsonStr) {
    cJSON* root = cJSON_Parse(jsonStr.c_str());
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return false;
    }

    const cJSON* item = nullptr;

    item = cJSON_GetObjectItem(root, "name");
    if (item && cJSON_IsString(item)) name = item->valuestring;

    item = cJSON_GetObjectItem(root, "version");
    if (item && cJSON_IsString(item)) version = item->valuestring;

    item = cJSON_GetObjectItem(root, "date");
    if (item && cJSON_IsString(item)) date = item->valuestring;

    cJSON_Delete(root);
    return true;
}
