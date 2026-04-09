# Embed the server root certificate into the final binary
#
# (If this was a component, we would set COMPONENT_EMBED_TXTFILES here.)
idf_component_register(SRCS "sensor_send.cpp"
                    PRIV_REQUIRES esp_event
                                  nvs_flash
                                  esp_netif
                                  esp_wifi
                                  mbedtls
                                  wifi_client
                                  https_client
                                  json_parser
                                  nvs_storage
                                  a7670
                    INCLUDE_DIRS ".")

set(COMPONENT_KCONFIG_DIRS "${CMAKE_CURRENT_SOURCE_DIR}")
