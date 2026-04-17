#include "ImageReceiver.h"

#include <cstring>

#include "esp_log.h"
#include "driver/uart.h"

static const char *TAG = "ImageReceier";

ImageReceiver::ImageReceiver(int uart_num) : _uart_num(uart_num) {}

void ImageReceiver::setQueue(QueueHandle_t imageQueue) {
    _imageQueue = imageQueue;
}

esp_err_t ImageReceiver::init()
{
    if (_imageQueue == nullptr) {
        ESP_LOGE(TAG, "Queue not set before start()");
        return ESP_ERR_INVALID_STATE;
    }

    uart_port_t uart_num = static_cast<uart_port_t>(_uart_num);

    uart_config_t config;
    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity    = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;

    uart_param_config(uart_num, &config);

    uart_driver_install(uart_num,
                         2048,
                         2048,
                         0,
                         nullptr,
                         0);

    uart_set_pin(uart_num,
                 25,   // TX
                 26,   // RX
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);
    
    return ESP_OK;
}

void ImageReceiver::start()
{
    xTaskCreate(&taskEntry,
                "cam_rx_task",
                8192,
                this,
                5,
                nullptr);
}

void ImageReceiver::taskEntry(void* arg)
{
    static_cast<ImageReceiver*>(arg)->run();
}

void ImageReceiver::run()
{
    State state = WAIT_OK;

    uint32_t len = 0;
    size_t received = 0;
    uint8_t* buf;

    while (true) {

        switch (state) {

            case WAIT_OK: {
                uart_write_bytes(static_cast<uart_port_t>(_uart_num), OK_PING, strlen(OK_PING));

                char resp[8] = {0};
                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)resp,
                                        2,
                                        pdMS_TO_TICKS(500));

                if (r >= 2 && strncmp(resp, "ok", 2) == 0) {
                    uart_write_bytes(static_cast<uart_port_t>(_uart_num), IMG_REQ, strlen(IMG_REQ));
                    state = WAIT_IMG_TAG;
                }

                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            } // case WAIT_OK

            case WAIT_IMG_TAG: {
                char tag[4] = {0};

                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)tag,
                                        4,
                                        pdMS_TO_TICKS(2000));

                if (r == 4 && memcmp(tag, IMG_TAG, 4) == 0) {
                    state = READ_LEN;
                }
                break;
            } // case WAIT_IMG_TAG

            case READ_LEN: {
                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)&len,
                                        sizeof(len),
                                        pdMS_TO_TICKS(2000));

                if (r == sizeof(len)) {
                    buf = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_DEFAULT);
                    received = 0;
                    state = READ_DATA;
                } else {
                    state = WAIT_OK;
                }
                break;
            } // ase READ_LEN

            case READ_DATA: {
                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        buf + received,
                                        len - received,
                                        pdMS_TO_TICKS(2000));

                if (r > 0) {
                    received += r;
                }

                if (received >= len) {
                    ImageMessage msg;
                    msg.data = buf;
                    msg.len = len;

                    xQueueSend(_imageQueue, &msg, portMAX_DELAY);

                    state = WAIT_OK;
                }
                break;
            } // case READ_DATA
        } // switch (state)
    } // while (true)
} // void ImageReceiver::run()