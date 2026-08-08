#pragma once

#include <string>

#include "esp_log.h"
#include "esp_err.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class UartAPI {
  static constexpr int JPEG_MAX_SIZE = 110000;
  static constexpr int JPEG_POOL_SIZE = 3;

  struct JpegBuffer {
      uint8_t* data;
      size_t len;
      bool in_use;
  };

public:
  UartAPI();
  esp_err_t init(int uart_num, int txPin, int rxPin, const QueueHandle_t jpegQueue);
  void start();
  void run();
  void request(const std::string& request);
  void release_buffer(JpegBuffer *buf);

  struct JpegPacket {
      uint8_t* data;
      size_t len;
      JpegBuffer* buf;
  };

private:
  TaskHandle_t _taskHandle = nullptr;
  QueueHandle_t _jpegQueue;
  uint8_t* jpeg_buffer = nullptr;
  size_t jpeg_write_index = 0;
  size_t expected_len = 0;

  JpegBuffer* _active_jpeg = nullptr;
  JpegBuffer _jpeg_pool[JPEG_POOL_SIZE];

  static void task_wrapper(void* arg);
  void process_uart_bytes(const uint8_t* input, size_t len);
  void on_command(const std::string& cmd);
  void on_response(const std::string& resp);
  void on_data(const uint8_t* data, size_t len);
  void init_pool();
  JpegBuffer* alloc_buffer();
};
