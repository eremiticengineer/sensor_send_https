# Sensor Send HTTPS

A simple component based HTTPS project for sending sensor data to a website via HTTPS POST.

The project uses ESP-IDF 6

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
