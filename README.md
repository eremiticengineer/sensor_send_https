# Sensor Send HTTPS

A simple ESP32 component based project for sending sensor data to a website over wifi via HTTPS POST.

The project is adapted from the esp-idf v6 sample:
```
~/.espressif/v6.0.0/esp-idf/examples/protocols/https_mbedtls
```
and refactored into components implemented as C++ classes.

Developed on a Raspberry Pi 4b+.

## Requirements
* ESP-IDF v6.0.0+
* An ESP32 board with onboard wifi.

## Set up the default sdkconfig
```
cp sdkconfig.defaults sdkconfig
```

## Configure WifiClient
```
idf.py menuconfig
Components -> WiFi Configuration
WIFI_SSID
WIFI_PASSWORD
```

## Configure HttpsClient
```
idf.py menuconfig
Components -> HTTPS Client Configuration
USER_AGENT
```

## Configure sensor data destination
```
idf.py menuconfig
Components -> SensorSend Configuration
CONFIG_SENSOR_SEND_WEB_SERVER
CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_PORT
CONFIG_SENSOR_SEND_WEB_SERVER_HTTPS_POST_URL
```
