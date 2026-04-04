# Sensor Send HTTPS

A simple ESP32 component based HTTPS project for sending sensor data to a website via HTTPS POST.

The project uses ESP-IDF 6.

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
