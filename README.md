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

## Enable PSRAM
PSRAM needs to be enabled in order to use the JPEG pools:
```
idf.py menuconfig

Component config
  -> ESP PSRAM
     -> Support for external, SPI-connected RAM
       -> SPI RAM config

```
change:
```
Mode (QUAD/OCT) of SPI RAM chip in use (Quad Mode PSRAM) (default value)
```
to:
```
Octal Mode PSRAM
```

to avoid the runtime error:
```
E (286) quad_psram: PSRAM chip is not connected, or wrong PSRAM line mode^M
E cpu_start: Failed to init external RAM!
abort() was called at PC 0x4200332f on core 0
```

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


https://docs.espressif.com/projects/esp-idf/en/v4.3.3/esp32/contribute/style-guide.html
https://docs.espressif.com/projects/esp-idf/en/v4.1.1/contribute/documenting-code.html