# Sensor Send HTTPS

A simple ESP32 component based project for sending sensor data to a website over wifi via HTTPS POST. The app connects to wifi, downloads a JSON file over https and parses it. It then POSTs a file over https and receives the POST response content and headers and disconnects from the server.

The project is adapted from the esp-idf v6 sample:
```
~/.espressif/v6.0.0/esp-idf/examples/protocols/https_mbedtls
```
and refactored into components implemented as C++ classes.

Developed on a Raspberry Pi 4b+.

## Requirements
* ESP-IDF v6.0.0+
* An ESP32 board with onboard wifi.

## sdkconfig defaults
```
sdkconfig.defaults
```
contains project defaults.

## Configure WifiClient
```
idf.py menuconfig
Components -> WiFi Configuration
WIFI_SSID
WIFI_PASSWORD
```

## Configure HTTPS options
```
idf.py menuconfig
Components -> SensorSend Configuration
```

## getting flash size of device
```
esptool --port /dev/ttyUSB0 flash-id
```
