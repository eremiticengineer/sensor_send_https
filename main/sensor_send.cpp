#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "WifiClient.h"
#include "HttpsClient.h"
#include "JsonParser.h"
#include "NvsStorage.h"
#include "A7670.h"

#include "sdkconfig.h"

static const char *POST_DATA = "{\"SensorSend\":\"https_mbedtls_espidf6\"}";

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    for (int i = 0; i < 5; i++) {
        ESP_LOGI("OTA", "Time elapsed: %d s", i + 1);
        vTaskDelay(pdMS_TO_TICKS(1000)); // 1000 ms = 1 second
    }

/*
    static const char* otaJson = R"(
    {
        "name": "sensor_send_https",
        "version": "1.0.0",
        "date": "05-04-2026"
    }
    )";
    JsonParser parser;
    ESP_LOGI("STACK", "************ Before JSON: %u", uxTaskGetStackHighWaterMark(NULL));
    if (!parser.parse(otaJson)) {
        ESP_LOGE("OTA", "cannot parse JSON");
    }
    else {
        ESP_LOGI("OTA", "New OTA available: %s %s %s",
            parser.getName().c_str(),
            parser.getVersion().c_str(),
            parser.getDate().c_str());
    }
    ESP_LOGI("STACK", "************ After JSON: %u", uxTaskGetStackHighWaterMark(NULL));

    ESP_LOGI("STACK", "************ Before NvsStorage: %u", uxTaskGetStackHighWaterMark(NULL));
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
    ESP_LOGI("STACK", "************ After NvsStorage: %u", uxTaskGetStackHighWaterMark(NULL));

    ESP_LOGI("STACK", "************ Before Wifi Connect: %u", uxTaskGetStackHighWaterMark(NULL));
    WifiClient wifi;
    wifi.connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);
    ESP_LOGI("STACK", "************ After Wifi Connect: %u", uxTaskGetStackHighWaterMark(NULL));

    ESP_LOGI("STACK", "************ Before HTTPS Connect: %u", uxTaskGetStackHighWaterMark(NULL));
    HttpsClient client(CONFIG_SENSOR_SEND_WEB_SERVER, CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_PORT);

    std::string content;
    std::string headers;

    ESP_LOGI("STACK", "************ Before HTTPS GET: %u", uxTaskGetStackHighWaterMark(NULL));
    if (!client.connect()) {
        ESP_LOGE("SENSOR_SEND", "Failed to connect to server");
        return;
    }
    ESP_LOGI("STACK", "************ After HTTPS Connect: %u", uxTaskGetStackHighWaterMark(NULL));
    esp_err_t err = client.get("/iot/sensor_send_https_ota.json", &content, &headers);
    client.disconnect();
    if (err != ESP_OK) {
        ESP_LOGE("OTA", "GET failed: %s (%d)", esp_err_to_name(err), err);
    } else {
        ESP_LOGI("OTA", "Response: %s", content.c_str());
        ESP_LOGI("OTA", "Headers: %s", headers.c_str());
    }
    ESP_LOGI("STACK", "************ After HTTPS GET: %u", uxTaskGetStackHighWaterMark(NULL));

    ESP_LOGI("STACK", "************ Before JSON response parse: %u", uxTaskGetStackHighWaterMark(NULL));
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
    ESP_LOGI("STACK", "************ After JSON response parse: %u", uxTaskGetStackHighWaterMark(NULL));

    ESP_LOGI("POST", "POSTING...");
    content.clear();
    headers.clear();
    if (!client.connect()) {
        ESP_LOGE("SENSOR_SEND", "Failed to connect to server");
        return;
    }
    bool response = client.post(CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_POST_URL,
        POST_DATA, &content, &headers);
    client.disconnect();
    ESP_LOGI("POST", "POSTED with response %s", response ? "true" : "false");
    ESP_LOGI("POST", "content: %s", content.c_str());
    ESP_LOGI("POST", "headers: %s", headers.c_str());
*/

    A7670Modem modem;
    modem.begin(CONFIG_PHONE_NUMBER_FOR_RESPONSE, "LilyGo A7670 Device Started from esp-idf v6");

    std::string url = "https://";
    std::string json = "{\"temperature\":25.3,\"humidity\":60}";
    std::string apiKey = "";
    if (modem.httpsPOST(url, json, apiKey)) {
        ESP_LOGI("MAIN", "POST succeeded");
    } else {
        ESP_LOGE("MAIN", "POST failed");
    }
}
