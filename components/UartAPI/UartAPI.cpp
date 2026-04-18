#include "UartAPI.h"

#include <vector>
#include <algorithm>

#include "driver/uart.h"

static const char *TAG = "ESP32CAM";

uart_port_t _uart_num;

#define LOG_ERR(tag, err, msg) \
    ESP_LOGE(tag, "%s: %s (0x%x)", msg, esp_err_to_name(err), (unsigned int)(err))

UartAPI::UartAPI() {}

esp_err_t UartAPI::init(int uart_num, int txPin, int rxPin) {
    _uart_num = static_cast<uart_port_t>(uart_num);

    esp_err_t status;

    uart_config_t uart_config = {};
    uart_config.baud_rate = 115200;
    uart_config.data_bits = UART_DATA_8_BITS;
    uart_config.parity    = UART_PARITY_DISABLE;
    uart_config.stop_bits  = UART_STOP_BITS_1;
    uart_config.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uart_config.source_clk = UART_SCLK_DEFAULT;

    esp_err_t err = uart_driver_install(_uart_num, 2048, 2048, 0, nullptr, 0);
    if (ESP_OK == err) {
        err = uart_param_config(_uart_num, &uart_config);
        if (ESP_OK == err) {
            err = uart_set_pin(_uart_num,
                            txPin,
                            rxPin,
                            UART_PIN_NO_CHANGE,
                            UART_PIN_NO_CHANGE);
            status = ESP_OK;
            ESP_LOGI(TAG, "UART ready");
            if (ESP_OK != err) {
                status = err;
                LOG_ERR(TAG, err, "cannot set UART pins");
            }
        }
        else {
            status = err;
            LOG_ERR(TAG, err, "cannot config UART");
        }
    }
    else {
      status = err;
      LOG_ERR(TAG, err, "cannot install UART driver");
    }

    return status;
}

void UartAPI::start() {
    xTaskCreate(task_wrapper, "UartAPITask", 4096, this, 5, &_taskHandle);
}

void UartAPI::task_wrapper(void* arg) {
    UartAPI* self = static_cast<UartAPI*>(arg);
    self->run();
}

void UartAPI::run() {
    uint8_t rx_buf[128];

    int written = uart_write_bytes(_uart_num, "#ok#", 4);
    if (written < 0) {
        ESP_LOGE(TAG, "cannot write to uart");
    }
    else {
        ESP_LOGI(TAG, "wrote %d bytes", written);
    }

    while (true)
    {
//        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "#ok#");

        // int len;
        // do {
        //     len = uart_read_bytes(_uart_num, rx_buf, sizeof(rx_buf), pdMS_TO_TICKS(200));
        //     if (len > 0) {
        //         process_uart_bytes(rx_buf, len);
        //     }
        // } while (len > 0);

        int len = uart_read_bytes(_uart_num,
                                rx_buf,
                                sizeof(rx_buf),
                                pdMS_TO_TICKS(20));
        ESP_LOGI(TAG, "read %d bytes from uart", len);
        if (len > 0)
        {
            process_uart_bytes(rx_buf, len);
        }
    }
}  // void UartAPI::run() {




void UartAPI::on_command(const std::string& cmd) {
  ESP_LOGI(TAG, "on_command %s", cmd.c_str());
}

void UartAPI::on_response(const std::string& resp) {
  ESP_LOGI(TAG, "on_response %s", resp.c_str());
}

void UartAPI::on_data(const uint8_t* data, size_t len) {
  ESP_LOGI(TAG, "on_data %d, %s", len, data);
}






static std::string buffer;
static std::string cmd;
static std::vector<uint8_t> data;

/*
 * command frame:
 * #ok#
 * #i:c#
 * response frame:
 * @ok@
 * data frame:
 * !94012!<binary bytes>
 */
void UartAPI::process_uart_bytes(const uint8_t* input, size_t len)
{
    // 1. append incoming bytes
    buffer.append(reinterpret_cast<const char*>(input), len);

    // safety: prevent runaway buffer if desync happens
    if (buffer.size() > 4096)
    {
        buffer.clear();
        return;
    }

    // 2. parse as many complete frames as possible
    while (true)
    {
        // -----------------------------
        // COMMAND FRAME: #...#
        // -----------------------------
        size_t cmd_start = buffer.find('#');

        // RESPONSE FRAME: @...@
        size_t resp_start = buffer.find('@');

        // DATA FRAME: !LEN!...
        size_t data_start = buffer.find('!');

        // choose earliest valid frame start
        size_t first = std::min({cmd_start, resp_start, data_start});

        if (first == std::string::npos)
            break;

        // erase garbage before frame start
        if (first > 0)
            buffer.erase(0, first);

        // refresh positions after trimming
        if (buffer.empty())
            break;

        char type = buffer[0];

        // -----------------------------
        // COMMAND FRAME
        // -----------------------------
        if (type == '#')
        {
            size_t end = buffer.find('#', 1);
            if (end == std::string::npos)
                break; // incomplete frame

            std::string cmd = buffer.substr(1, end - 1);
            buffer.erase(0, end + 1);

            on_command(cmd);
            continue;
        }

        // -----------------------------
        // RESPONSE FRAME
        // -----------------------------
        if (type == '@')
        {
            size_t end = buffer.find('@', 1);
            if (end == std::string::npos)
                break;

            std::string resp = buffer.substr(1, end - 1);
            buffer.erase(0, end + 1);

            on_response(resp);
            continue;
        }

        // -----------------------------
        // DATA FRAME: !LEN!<bytes>
        // -----------------------------
        if (type == '!')
        {
            size_t len_sep = buffer.find('!', 1);
            if (len_sep == std::string::npos)
                break;

            std::string len_str = buffer.substr(1, len_sep - 1);
            size_t data_len = std::strtoul(len_str.c_str(), nullptr, 10);

            size_t needed = len_sep + 1 + data_len;

            if (buffer.size() < needed)
                break; // wait for full payload

            const uint8_t* data_ptr =
                reinterpret_cast<const uint8_t*>(buffer.data() + len_sep + 1);

            on_data(data_ptr, data_len);

            buffer.erase(0, needed);
            continue;
        }

        // unknown byte → drop it (resync safety)
        buffer.erase(0, 1);
    }
}
