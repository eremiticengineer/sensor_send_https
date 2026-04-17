#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "WifiClient.h"
#include "HttpsClient.h"
#include "JsonParser.h"
#include "NvsStorage.h"

#include "sdkconfig.h"

static const std::string POST_DATA = "{\"SensorSend\":\"https_mbedtls_espidf6\"}";

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    for (int i = 0; i < 5; i++) {
        ESP_LOGI("OTA", "Time elapsed: %d s", i + 1);
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1000 ms = 1 second
    }

    // Test the NVS storage
    NvsStorage storage("ota_data");
    storage.init();
    std::string test = storage.getString("test");
    if (test == "") {
        ESP_LOGI("OTA", "test isn't in nvs, adding...");
        esp_err_t result = storage.setString("test", "teststring");
        if (result == ESP_OK) {
            ESP_LOGI("OTA", "...added OK!");
        }
        else {
            ESP_LOGE("OTA", "cannot add string: %d", result);
        }
    }
    else {
        ESP_LOGI("OTA", "string exists in nvs \"%s\"", test.c_str());
    }

    // Connect to wifi
    WifiClient wifi;
    wifi.connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);

    // Prepare an https client
    HttpsClient client(CONFIG_SENSOR_SEND_WEB_SERVER,
        CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_PORT,
        CONFIG_SENSOR_SEND_USER_AGENT);

    std::string content;
    std::string headers;

    // Connect to the server over https
    if (!client.connect()) {
        ESP_LOGE("SENSOR_SEND", "Failed to connect to server");
        return;
    }

    // GET a JSON file from the server over https...
    esp_err_t err = client.get(CONFIG_SENSOR_SEND_OTA_PATH, &content, &headers);
    client.disconnect();
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "GET failed: %s (%d)", esp_err_to_name(err), err);
    } else {
        ESP_LOGI("OTA", "Response: %s", content.c_str());
        ESP_LOGI("OTA", "Headers: %s", headers.c_str());
    }

    // ...and parse the returned JSON
    JsonParser parser;
    const char* json_cstr = content.c_str();
    ESP_LOGI("JSON_FROM_SERVER","%s", content.c_str());
    if (!parser.parse(json_cstr)) {
        ESP_LOGE("OTA", "cannot parse ota JSON");
    }
    else {
        ESP_LOGI("OTA", "New OTA available: %s %s %s",
            parser.getName().c_str(),
            parser.getVersion().c_str(),
            parser.getDate().c_str());
    }

    // POST a file to the server over https
    ESP_LOGI("POST", "POSTING...");
    content.clear();
    headers.clear();
    if (!client.connect()) {
        ESP_LOGE("SENSOR_SEND", "Failed to connect to server");
        return;
    }
    bool response = client.post(CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_POST_PATH,
        CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_POST_API_KEY,
        reinterpret_cast<const uint8_t*>(POST_DATA.data()),
        POST_DATA.size(),
        &content, &headers);
    client.disconnect();
    ESP_LOGI("POST", "POSTED with response %s", response ? "true" : "false");
    ESP_LOGI("POST", "content: %s", content.c_str());
    ESP_LOGI("POST", "headers: %s", headers.c_str());
}
