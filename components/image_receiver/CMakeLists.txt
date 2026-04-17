idf_component_register(
    SRCS "ImageReceiver.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_uart
)