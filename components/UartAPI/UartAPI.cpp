#include "UartAPI.h"

#include <vector>
#include <algorithm>

#include "driver/uart.h"
#include "driver/gpio.h"

static const char *TAG = "ESP32CAM";

uart_port_t _uart_num;

#define LOG_ERR(tag, err, msg) \
    ESP_LOGE(tag, "%s: %s (0x%x)", msg, esp_err_to_name(err), (unsigned int)(err))

UartAPI::UartAPI() {}

esp_err_t UartAPI::init(int uart_num, int txPin, int rxPin, const QueueHandle_t jpegQueue) {
    _uart_num = static_cast<uart_port_t>(uart_num);
    _jpegQueue = jpegQueue;

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
            gpio_set_pull_mode(static_cast<gpio_num_t>(rxPin), GPIO_PULLUP_ONLY);
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

    while (true)
    {
        int len = uart_read_bytes(_uart_num,
                                  rx_buf,
                                  sizeof(rx_buf),
                                  pdMS_TO_TICKS(20));
        if (len > 0)
        {
            process_uart_bytes(rx_buf, len);
        }

        // Prevent freertos from resetting the task.
        // task can monopolise CPU1 during image reception
        // Task watchdog got triggered. The following tasks/users did not reset the watchdog in time:
        // - IDLE1 (CPU 1)
        vTaskDelay(1);
    }
}  // void UartAPI::run() {

void UartAPI::request(const std::string& request) {
    int written = uart_write_bytes(_uart_num, request.c_str(), request.length());
    if (written < 0) {
        ESP_LOGE(TAG, "cannot write to uart");
    }
    else {
        ESP_LOGI(TAG, "wrote %s", request.c_str());
    }
}

void UartAPI::on_command(const std::string& cmd) {
  ESP_LOGI(TAG, "on_command %s", cmd.c_str());
}

void UartAPI::on_response(const std::string& resp) {
  ESP_LOGI(TAG, "on_response %s", resp.c_str());
}

void UartAPI::on_data(const uint8_t* data, size_t len) {
    ESP_LOGI(TAG, "on_data len=%d", len);
}

static std::string buffer;
static std::string cmd;
static std::vector<uint8_t> data;

static constexpr bool UART_DEBUG = true;
size_t payload_received = 0;
bool receiving_data = false;
size_t expected_len = 0;
size_t buffer_offset = 0;

/*
 * command frame:
 * #ok#
 * #i:c#
 * response frame:
 * @ok@
 * data frame:
 * !94012!<binary bytes>
 */

static constexpr size_t MAX_BUFFER = 4096;

/*
void UartAPI::process_uart_bytes(const uint8_t* input, size_t len)
{
    // Append incoming bytes
    buffer.insert(buffer.end(), input, input + len);

    // Safety cap ONLY in text mode
    if (!receiving_data && buffer.size() > MAX_BUFFER)
    {
        ESP_LOGW(TAG, "buffer overflow, clearing");
        buffer.clear();
        buffer_offset = 0;
        return;
    }

    while (true)
    {
        // =========================================================
        // BINARY MODE (strict streaming, no parser interaction)
        // =========================================================
        if (receiving_data)
        {
            size_t available = buffer.size();
            size_t remaining = expected_len - payload_received;

            if (available == 0 || remaining == 0)
                return;

            size_t to_consume = std::min(available, remaining);

            const uint8_t *ptr = reinterpret_cast<const uint8_t *>(buffer.data());

            on_data(ptr, to_consume);

            payload_received += to_consume;

            // remove consumed bytes from front only
            buffer.erase(buffer.begin(),
                         buffer.begin() + to_consume);

            buffer_offset = 0;

            // completion check (ONLY valid exit point)
            if (payload_received == expected_len)
            {
                // if (ptr[to_consume - 2] == 0xFF && ptr[to_consume - 1] == 0xD9) // end of jpeg
                ESP_LOGI(TAG, "************************************************ IMAGE_COMPLETE");

                receiving_data = false;
                expected_len = 0;
                payload_received = 0;

                return;
            }

            // IMPORTANT: stay in binary mode until fully complete
            return;
        }

        // =========================================================
        // TEXT MODE: locate next frame start
        // =========================================================
        if (buffer.empty())
            return;

        size_t cmd  = buffer.size();
        size_t resp = buffer.size();
        size_t data = buffer.size();

        for (size_t i = 0; i < buffer.size(); i++)
        {
            if (buffer[i] == '#') { cmd = i; break; }
            if (buffer[i] == '@') { resp = i; break; }
            if (buffer[i] == '!') { data = i; break; }
        }

        size_t first = std::min({cmd, resp, data});

        if (first == buffer.size())
        {
            buffer.clear();
            buffer_offset = 0;
            return;
        }

        // drop garbage
        if (first > 0)
        {
            buffer.erase(buffer.begin(),
                         buffer.begin() + first);
        }

        if (buffer.empty())
            return;

        char type = buffer[0];

        // =========================================================
        // COMMAND: #...#
        // =========================================================
        if (type == '#')
        {
            size_t end = buffer.find('#', 1);
            if (end == std::string::npos)
                return;

            std::string cmd(buffer.begin() + 1,
                            buffer.begin() + end);

            buffer.erase(buffer.begin(),
                         buffer.begin() + end + 1);

            on_command(cmd);
            continue;
        }

        // =========================================================
        // RESPONSE: @...@
        // =========================================================
        if (type == '@')
        {
            size_t end = buffer.find('@', 1);
            if (end == std::string::npos)
                return;

            std::string resp(buffer.begin() + 1,
                             buffer.begin() + end);

            buffer.erase(buffer.begin(),
                         buffer.begin() + end + 1);

            on_response(resp);
            continue;
        }

        // =========================================================
        // DATA HEADER: !LEN!
        // =========================================================
        if (type == '!')
        {
            size_t sep = buffer.find('!', 1);
            if (sep == std::string::npos)
                return;

            std::string len_str(buffer.begin() + 1,
                                buffer.begin() + sep);

            expected_len = std::strtoul(len_str.c_str(), nullptr, 10);

            if (expected_len > 200000)
            {
                ESP_LOGE(TAG, "invalid length %u", expected_len);
                buffer.clear();
                buffer_offset = 0;
                return;
            }

            receiving_data = true;
            payload_received = 0;

            // remove "!LEN!" header
            buffer.erase(buffer.begin(),
                         buffer.begin() + sep + 1);

            buffer_offset = 0;

            return;
        }

        // =========================================================
        // RESYNC SAFETY
        // =========================================================
        buffer.erase(buffer.begin());
    }
}
    */

void UartAPI::process_uart_bytes(const uint8_t* input, size_t len)
{
    // Append incoming bytes
    buffer.insert(buffer.end(), input, input + len);

    // Safety cap ONLY in text mode
    if (!receiving_data && buffer.size() > MAX_BUFFER)
    {
        ESP_LOGW(TAG, "buffer overflow, clearing");
        buffer.clear();
        return;
    }

    while (true)
    {
        // =========================================================
        // BINARY MODE
        // =========================================================
        if (receiving_data)
        {
            if (!jpeg_buffer)
            {
                ESP_LOGE(TAG, "NULL jpeg_buffer in binary mode");
                receiving_data = false;
                return;
            }

            size_t available = buffer.size();
            size_t remaining = expected_len - payload_received;

            if (available == 0 || remaining == 0)
                return;

            size_t to_consume = std::min(available, remaining);

            // SAFETY: prevent overflow (critical)
            if (jpeg_write_index + to_consume > expected_len)
            {
                ESP_LOGE(TAG, "JPEG overflow detected! idx=%u remaining=%u expected=%u",
                         jpeg_write_index, (unsigned)to_consume, expected_len);

                free(jpeg_buffer);
                jpeg_buffer = nullptr;
                receiving_data = false;
                expected_len = 0;
                payload_received = 0;
                jpeg_write_index = 0;

                buffer.clear();
                return;
            }

            const uint8_t* ptr = reinterpret_cast<const uint8_t*>(buffer.data());

            memcpy(jpeg_buffer + jpeg_write_index, ptr, to_consume);

            jpeg_write_index += to_consume;
            payload_received  += to_consume;

            buffer.erase(buffer.begin(), buffer.begin() + to_consume);

            // =====================================================
            // COMPLETE FRAME
            // =====================================================
            if (payload_received == expected_len)
            {
                ESP_LOGI(TAG, "************************************************ IMAGE_COMPLETE %d", payload_received);

                // At this point jpeg_buffer is valid and complete.

                // Build packet (ownership transfer)
                JpegPacket pkt;
                pkt.data = jpeg_buffer;
                pkt.len  = payload_received;

                // Send to consumer task
                if (_jpegQueue != nullptr)
                {
                    if (xQueueSend(_jpegQueue, &pkt, pdMS_TO_TICKS(100)) != pdPASS)
                    {
                        ESP_LOGE(TAG, "JPEG queue full - dropping frame");

                        // If queue rejected it, we MUST free it here
                        free(pkt.data);
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "_jpegQueue not initialised - freeing frame");

                    // No consumer exists → prevent leak
                    free(pkt.data);
                }

                receiving_data = false;
                jpeg_write_index = 0;
                expected_len = 0;
                payload_received = 0;
                jpeg_buffer = nullptr;

                return;
            }

            return;
        }

        // =========================================================
        // TEXT MODE
        // =========================================================
        if (buffer.empty())
            return;

        size_t cmd  = buffer.size();
        size_t resp = buffer.size();
        size_t data = buffer.size();

        for (size_t i = 0; i < buffer.size(); i++)
        {
            if (buffer[i] == '#') { cmd = i; break; }
            if (buffer[i] == '@') { resp = i; break; }
            if (buffer[i] == '!') { data = i; break; }
        }

        size_t first = std::min({cmd, resp, data});

        if (first == buffer.size())
        {
            buffer.clear();
            return;
        }

        if (first > 0)
        {
            buffer.erase(buffer.begin(), buffer.begin() + first);
        }

        if (buffer.empty())
            return;

        char type = buffer[0];

        // =========================================================
        // COMMAND: #...#
        // =========================================================
        if (type == '#')
        {
            size_t end = buffer.find('#', 1);
            if (end == std::string::npos)
                return;

            std::string cmd(buffer.begin() + 1, buffer.begin() + end);

            buffer.erase(buffer.begin(), buffer.begin() + end + 1);

            on_command(cmd);
            continue;
        }

        // =========================================================
        // RESPONSE: @...@
        // =========================================================
        if (type == '@')
        {
            size_t end = buffer.find('@', 1);
            if (end == std::string::npos)
                return;

            std::string resp(buffer.begin() + 1, buffer.begin() + end);

            buffer.erase(buffer.begin(), buffer.begin() + end + 1);

            on_response(resp);
            continue;
        }

        // =========================================================
        // DATA HEADER: !LEN!
        // =========================================================
        if (type == '!')
        {
            // Prevent re-entry corruption
            if (receiving_data)
            {
                ESP_LOGE(TAG, "re-entrant binary start detected - dropping");
                buffer.clear();
                return;
            }

            size_t sep = buffer.find('!', 1);
            if (sep == std::string::npos)
                return;

            std::string len_str(buffer.begin() + 1, buffer.begin() + sep);

            size_t len_val = std::strtoul(len_str.c_str(), nullptr, 10);

            if (len_val == 0 || len_val > 200000)
            {
                ESP_LOGE(TAG, "invalid length %u", len_val);
                buffer.clear();
                return;
            }

            uint8_t* new_buf = (uint8_t*)malloc(len_val);
            if (!new_buf)
            {
                ESP_LOGE(TAG, "malloc failed (%u bytes)", len_val);
                buffer.clear();
                return;
            }

            jpeg_buffer = new_buf;
            expected_len = len_val;
            jpeg_write_index = 0;
            payload_received = 0;
            receiving_data = true;

            buffer.erase(buffer.begin(), buffer.begin() + sep + 1);

            return;
        }

        // =========================================================
        // RESYNC SAFETY
        // =========================================================
        buffer.erase(buffer.begin());
    }
}