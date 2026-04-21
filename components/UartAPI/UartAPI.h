#pragma once

#include <string>

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class UartAPI {

public:
  UartAPI();
  esp_err_t init(int uart_num, int txPin, int rxPin, const QueueHandle_t jpegQueue);
  void start();
  void run();
  void request(const std::string& request);

  struct JpegPacket {
      uint8_t* data;
      size_t len;
  };

private:
  TaskHandle_t _taskHandle = nullptr;
  QueueHandle_t _jpegQueue;
  uint8_t* jpeg_buffer = nullptr;
  size_t jpeg_write_index = 0;
  size_t expected_len = 0;

  static void task_wrapper(void* arg);
  void process_uart_bytes(const uint8_t* input, size_t len);
  void on_command(const std::string& cmd);
  void on_response(const std::string& resp);
  void on_data(const uint8_t* data, size_t len);
};
