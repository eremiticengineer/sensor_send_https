idf_component_register(
    SRCS "HttpsClient.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_event nvs_flash esp_netif mbedtls
)