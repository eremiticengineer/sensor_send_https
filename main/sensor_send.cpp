#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"

#include "WifiClient.h"
#include "HttpsClient.h"

#include "sdkconfig.h"

static const char *POST_DATA = "{\"SensorSend\":\"https_mbedtls_espidf6\"}";

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    WifiClient wifi;
    wifi.connect(CONFIG_WIFI_SSID, CONFIG_WIFI_PASSWORD);

    HttpsClient client(CONFIG_SENSOR_SEND_WEB_SERVER, CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_PORT);
    if (!client.connect()) {
        ESP_LOGE("SENSOR_SEND", "Failed to connect to server");
        return;
    }

    client.post(CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_POST_URL, POST_DATA);
    client.disconnect();
}
