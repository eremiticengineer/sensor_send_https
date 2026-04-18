#include "ImageReceiver.h"

#include <cstring>
#include <string>

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
    ESP_LOGI(TAG, "uart_num = %d", uart_num);

    uart_config_t config = {};
    config.baud_rate = 115200;
    config.data_bits = UART_DATA_8_BITS;
    config.parity    = UART_PARITY_DISABLE;
    config.stop_bits = UART_STOP_BITS_1;
    config.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    config.source_clk = UART_SCLK_DEFAULT;

    uart_driver_install(uart_num,
                         2048,
                         2048,
                         0,
                         nullptr,
                         0);

    uart_param_config(uart_num, &config);

    // https://wiki.dfrobot.com/dfr1140/docs/23456
    uart_set_pin(uart_num,
                 17,   // TX
                 16,   // RX
                 UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE);

// while (true) {
//     ESP_LOGI(TAG, "HELLO");
//     uart_write_bytes(uart_num, "HELLO\n", 6);
//     vTaskDelay(pdMS_TO_TICKS(1000));
// }                 
    
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


void ImageReceiver::run() {
    // char tmp[64];
    // int length = 0;
    // int read = -1;

uint8_t tmp[128];

while (1) {
    ESP_LOGI(TAG, "WRITING ok#");
    uart_flush(static_cast<uart_port_t>(_uart_num));
    uart_write_bytes(static_cast<uart_port_t>(_uart_num), "ok#", 3);

    vTaskDelay(pdMS_TO_TICKS(500));

    int len = uart_read_bytes(static_cast<uart_port_t>(_uart_num), tmp, sizeof(tmp), pdMS_TO_TICKS(500));

ESP_LOGI(TAG, "LEN=%d", len);
ESP_LOG_BUFFER_HEX(TAG, tmp, len);


    // int len = uart_read_bytes(static_cast<uart_port_t>(_uart_num), tmp, sizeof(tmp) - 1, pdMS_TO_TICKS(1000));

    // if (len > 0) {
    //     tmp[len] = 0;  // SAFE because ASCII protocol

    //     ESP_LOGI(TAG, "READ %d bytes", len);
    //     ESP_LOGI(TAG, "DATA: %s", (char*)tmp);
    // }

    vTaskDelay(pdMS_TO_TICKS(1000));
}    

    /*
    while (1) {
        ESP_LOGI(TAG, "**************************** WRITING ok#");
        uart_write_bytes(static_cast<uart_port_t>(_uart_num), "ok#", strlen("ok#"));

        vTaskDelay(pdMS_TO_TICKS(500));

        while (read < length) {
            uart_get_buffered_data_len(static_cast<uart_port_t>(_uart_num), (size_t*)&length);
            ESP_LOGI(TAG, "**************************** READY %d bytes", length);

            if (length > 0) {
                read = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)tmp,
                                        length,
                                        pdMS_TO_TICKS(2000));
                ESP_LOGI(TAG, "**************************** READ %d bytes", length);
                ESP_LOGI(TAG, "**************************** DATA %s", tmp);
            }
        }

        length = 0;
        read = -1;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
        */
}



/*
void run2()
{
    State state = WAIT_OK;

    uint32_t len = 0;
    size_t received = 0;
    uint8_t* buf = nullptr;

    char rxBuf[256];

    while (true)
    {
        switch (state)
        {
            // ------------------------------------------------------------
case WAIT_OK:
{
    static std::string rx;

    uart_write_bytes(static_cast<uart_port_t>(_uart_num), OK_PING, strlen(OK_PING));

    char tmp[64];
    int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                            (uint8_t*)tmp,
                            sizeof(tmp),
                            pdMS_TO_TICKS(2000));

    if (r > 0)
    {
        rx.append(tmp, r);

        // DEBUG (safe binary print)
        ESP_LOGI(TAG, "RX chunk (%d):", r);
        for (int i = 0; i < r; i++)
            printf("%02X ", (uint8_t)tmp[i]);
        printf("\n");

        // WAIT FOR FULL TOKEN
        size_t pos = rx.find("ok#");
        if (pos != std::string::npos)
        {
            ESP_LOGI(TAG, "GOT OK -> requesting image");

            rx.clear(); // IMPORTANT

            uart_write_bytes(static_cast<uart_port_t>(_uart_num),
                             IMG_REQ,
                             strlen(IMG_REQ));

            state = WAIT_IMG_TAG;
        }
    }

    break;
}
            // ------------------------------------------------------------
            case WAIT_IMG_TAG:
            {
                ESP_LOGI(TAG, "WAIT_IMG_TAG");

                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)rxBuf,
                                        sizeof(rxBuf),
                                        pdMS_TO_TICKS(3000));

                if (r > 0)
                {
                    ESP_LOGI(TAG, "TAG RX (%d bytes):", r);

                    if (memmem(rxBuf, r, IMG_TAG, 4))
                    {
                        ESP_LOGI(TAG, "GOT IMG_TAG");
                        state = READ_LEN;
                    }
                }

                break;
            }

            // ------------------------------------------------------------
            case READ_LEN:
            {
                ESP_LOGI(TAG, "READ_LEN");

                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)&len,
                                        sizeof(len),
                                        pdMS_TO_TICKS(3000));

                if (r == sizeof(len) && len > 0 && len < (2 * 1024 * 1024))
                {
                    ESP_LOGI(TAG, "IMAGE LEN = %lu", (unsigned long)len);

                    buf = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_DEFAULT);
                    if (!buf)
                    {
                        ESP_LOGE(TAG, "malloc failed");
                        state = WAIT_OK;
                        break;
                    }

                    received = 0;
                    state = READ_DATA;
                }
                else
                {
                    ESP_LOGE(TAG, "Invalid LEN (%d bytes)", r);
                    state = WAIT_OK;
                }

                break;
            }

            // ------------------------------------------------------------
            case READ_DATA:
            {
                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        buf + received,
                                        len - received,
                                        pdMS_TO_TICKS(3000));

                if (r > 0)
                {
                    received += r;
                    ESP_LOGI(TAG, "RX image %d / %lu",
                             received,
                             (unsigned long)len);
                }

                if (received >= len)
                {
                    ESP_LOGI(TAG, "IMAGE COMPLETE");

                    ImageMessage msg;
                    msg.data = buf;
                    msg.len  = len;

                    if (xQueueSend(_imageQueue, &msg, portMAX_DELAY) != pdTRUE)
                    {
                        ESP_LOGE(TAG, "Queue full, dropping image");
                        heap_caps_free(buf);
                    }

                    buf = nullptr;
                    state = WAIT_OK;
                }

                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10)); // keep RTOS happy
    }
}
*/



/*
void ImageReceiver::run()
{
    State state = WAIT_OK;

    uint32_t len = 0;
    size_t received = 0;
    uint8_t* buf;

    while (true) {

        switch (state) {

            case WAIT_OK: {
                ESP_LOGI(TAG, "WAIT_OK");
                uart_write_bytes(static_cast<uart_port_t>(_uart_num), OK_PING, strlen(OK_PING));

                char resp[64] = {0};
                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)resp,
                                        2,
                                        pdMS_TO_TICKS(1000));
                ESP_LOGI(TAG, "%s", resp);

if (r > 0) {
    ESP_LOGI(TAG, "RX (%d bytes):", r);

    for (int i = 0; i < r; i++) {
        printf("%02X ", (uint8_t)resp[i]);
    }
    printf("\n");
}                

                if (r >= 2 && strncmp(resp, "ok", 2) == 0) {
                    ESP_LOGI(TAG, "GOT OK, WRITING IMG CMD");
                    uart_write_bytes(static_cast<uart_port_t>(_uart_num), IMG_REQ, strlen(IMG_REQ));
                    state = WAIT_IMG_TAG;
                }

                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
            } // case WAIT_OK

            case WAIT_IMG_TAG: {
                ESP_LOGI(TAG, "WAIT_IMG_TAG");
                char tag[4] = {0};

                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)tag,
                                        4,
                                        pdMS_TO_TICKS(2000));

                if (r == 4 && memcmp(tag, IMG_TAG, 4) == 0) {
                    ESP_LOGI(TAG, "GOT IMG_TAG");
                    state = READ_LEN;
                }
                break;
            } // case WAIT_IMG_TAG

            case READ_LEN: {
                ESP_LOGI(TAG, "READ_LEN");
                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        (uint8_t*)&len,
                                        sizeof(len),
                                        pdMS_TO_TICKS(2000));

                if (r == sizeof(len)) {
                    ESP_LOGI(TAG, "GOT LEN: %d", len);
                    buf = (uint8_t*)heap_caps_malloc(len, MALLOC_CAP_DEFAULT);
                    received = 0;
                    state = READ_DATA;
                } else {
                    state = WAIT_OK;
                }
                break;
            } // ase READ_LEN

            case READ_DATA: {
                ESP_LOGI(TAG, "READ_DATA");
                int r = uart_read_bytes(static_cast<uart_port_t>(_uart_num),
                                        buf + received,
                                        len - received,
                                        pdMS_TO_TICKS(2000));

                if (r > 0) {
                    received += r;
                }

                if (received >= len) {
                    ESP_LOGI(TAG, "GOT DATA");
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
*/