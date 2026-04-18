#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class ImageReceiver {
public:
    ImageReceiver(int uart_num);
    esp_err_t init();
    void start();
    void setQueue(QueueHandle_t imageQueue);

    struct ImageMessage {
        uint8_t* data;
        size_t len;
    };

private:
    static void taskEntry(void* arg);
    void run();


// enum State {
//     WAIT_OK,
//     WAIT_IMG_PREFIX,
//     READ_LEN,
//     READ_IMAGE
// };

    enum State {
        WAIT_OK,
        WAIT_IMG_TAG,
        READ_LEN,
        READ_DATA
    };

    int _uart_num;
    QueueHandle_t _imageQueue;

    static constexpr const char* OK_PING = "ok#";
    static constexpr const char* IMG_REQ = "i:c#";
    static constexpr const char* IMG_TAG = "IMG:";
};