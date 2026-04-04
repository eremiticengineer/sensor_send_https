#include "WifiClient.h"
#include "esp_log.h"
#include <string.h>

#define EXAMPLE_NETIF_DESC_STA "example_netif_sta"
#define CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY 6
static const char* TAG = "WifiClient";

WifiClient::WifiClient()
    : sta_netif_(nullptr), sem_ip_(nullptr), retry_count_(0) {}

WifiClient::~WifiClient() {
    shutdown();
}

void WifiClient::start() {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_netif_inherent_config_t netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    netif_config.if_desc = EXAMPLE_NETIF_DESC_STA;
    netif_config.route_prio = 128;

    sta_netif_ = esp_netif_create_wifi(WIFI_IF_STA, &netif_config);
    esp_wifi_set_default_wifi_sta_handlers();

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void WifiClient::stop() {
    esp_err_t err = esp_wifi_stop();
    if (err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_ERROR_CHECK(err);
        ESP_ERROR_CHECK(esp_wifi_deinit());
    }

    if (sta_netif_) {
        ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(sta_netif_));
        esp_netif_destroy(sta_netif_);
        sta_netif_ = nullptr;
    }
}

esp_err_t WifiClient::connect(const char* ssid, const char* password, bool wait_for_ip) {
    start();

    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.rssi = -127;

    return sta_do_connect(wifi_config, wait_for_ip);
}

esp_err_t WifiClient::sta_do_connect(wifi_config_t& wifi_config, bool wait) {
    if (wait) {
        sem_ip_ = xSemaphoreCreateBinary();
        if (!sem_ip_) {
            return ESP_ERR_NO_MEM;
        }
    }

    retry_count_ = 0;

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                               &WifiClient::on_wifi_disconnect, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &WifiClient::on_sta_got_ip, this));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                               &WifiClient::on_wifi_connect, sta_netif_));

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi connect failed! ret:%x", ret);
        return ret;
    }

    if (wait) {
        ESP_LOGI(TAG, "Waiting for IP...");
        xSemaphoreTake(sem_ip_, portMAX_DELAY);
        vSemaphoreDelete(sem_ip_);
        sem_ip_ = nullptr;

        if (retry_count_ > CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY) {
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t WifiClient::disconnect() {
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED,
                                                 &WifiClient::on_wifi_disconnect));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 &WifiClient::on_sta_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED,
                                                 &WifiClient::on_wifi_connect));
    return esp_wifi_disconnect();
}

void WifiClient::shutdown() {
    disconnect();
    stop();
}

bool WifiClient::is_our_netif(const char* prefix, esp_netif_t* netif) {
    return strncmp(prefix, esp_netif_get_desc(netif), strlen(prefix) - 1) == 0;
}

// ---------------- Static event handlers ---------------- //

void WifiClient::on_wifi_disconnect(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data) {
    WifiClient* self = static_cast<WifiClient*>(arg);
    self->retry_count_++;
    wifi_event_sta_disconnected_t* disconn = static_cast<wifi_event_sta_disconnected_t*>(event_data);

    if (self->retry_count_ > CONFIG_EXAMPLE_WIFI_CONN_MAX_RETRY) {
        ESP_LOGI(TAG, "WiFi Connect failed %d times, stop reconnect.", self->retry_count_);
        if (self->sem_ip_) {
            xSemaphoreGive(self->sem_ip_);
        }
        self->disconnect();
        return;
    }

    if (disconn->reason == WIFI_REASON_ROAMING) {
        ESP_LOGD(TAG, "station roaming, do nothing");
        return;
    }

    ESP_LOGI(TAG, "Wi-Fi disconnected %d, trying to reconnect...", disconn->reason);
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) return;
    ESP_ERROR_CHECK(err);
}

void WifiClient::on_wifi_connect(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data) {
    // optional logic
}

void WifiClient::on_sta_got_ip(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data) {
    WifiClient* self = static_cast<WifiClient*>(arg);
    ip_event_got_ip_t* event = static_cast<ip_event_got_ip_t*>(event_data);

    if (!is_our_netif(EXAMPLE_NETIF_DESC_STA, event->esp_netif)) return;

    self->retry_count_ = 0;

    ESP_LOGI(TAG, "Got IPv4: %s, IP: " IPSTR,
             esp_netif_get_desc(event->esp_netif),
             IP2STR(&event->ip_info.ip));

    if (self->sem_ip_) {
        xSemaphoreGive(self->sem_ip_);
    }
}