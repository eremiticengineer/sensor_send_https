#pragma once

#include <string>

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class UartAPI {

public:
  UartAPI();
  esp_err_t init(int uart_num, int txPin, int rxPin);
  void start();
  void run();

private:
  TaskHandle_t _taskHandle = nullptr;

  static void task_wrapper(void* arg);
  void process_uart_bytes(const uint8_t* input, size_t len);
  void on_command(const std::string& cmd);
  void on_response(const std::string& resp);
  void on_data(const uint8_t* data, size_t len);
};
