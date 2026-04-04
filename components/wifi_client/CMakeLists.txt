idf_component_register(
    SRCS "WifiClient.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_netif
)