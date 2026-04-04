#pragma once

#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/semphr.h"

class WifiClient {
public:
    WifiClient();
    ~WifiClient();

    esp_err_t connect(const char* ssid, const char* password, bool wait_for_ip = true);
    esp_err_t disconnect();
    void shutdown();

private:
    // Wi-Fi STA interface
    esp_netif_t* sta_netif_;

    // Semaphore for waiting for IP
    SemaphoreHandle_t sem_ip_;

    int retry_count_;

    // Private helpers
    void start();
    void stop();
    esp_err_t sta_do_connect(wifi_config_t& wifi_config, bool wait);

    // Event handlers
    static void on_wifi_disconnect(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data);
    static void on_wifi_connect(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
    static void on_sta_got_ip(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data);

    // Helper
    static bool is_our_netif(const char* prefix, esp_netif_t* netif);
};