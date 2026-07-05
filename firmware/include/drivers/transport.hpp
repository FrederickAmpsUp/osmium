#pragma once

#include "drivers/nodebus.hpp"
#include "nodebus_parser.hpp"
#include "nodebus_sender.hpp"
#include "util/mutex.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>

namespace osmium {

class NodebusTransport {
public:
  // --- Public Types ---
  enum ResponseStatus {
    OK = 0,
    TIMED_OUT,
    REQUEST_OVERSIZED,
    REQUEST_UNAVAILABLE,
  };

  using RequestHandler = void (*)(uint8_t, const uint8_t *, size_t);

  // --- Lifecycle ---
  NodebusTransport(Stream &stream, uint8_t id);
  ~NodebusTransport();

  // --- Configuration ---
  inline void set_timeout_ms(uint32_t timeout_ms) {
    this->timeout = pdMS_TO_TICKS(timeout_ms);
  }

  inline void set_request_handler(RequestHandler handler) {
    this->request_handler = handler;
  }

  // --- Communication API ---
  
  // Sync-safe wrapper over this->sender.send()
  void send(uint8_t dest, const uint8_t *payload, size_t payload_size);

  // Suspends calling task until a response is received or times out.
  // Writes up to min(response_capacity, NODEBUS_PAYLOAD_MAX_SIZE) to response buffer.
  ResponseStatus await_send_request(uint8_t dest, const uint8_t *request, size_t request_size, 
                                    uint8_t *response, size_t response_capacity, size_t *response_size);

private:
  // --- Task Constants ---
  static constexpr size_t TICK_TASK_STACK_SIZE = 4096;
  static constexpr int TICK_TASK_PRIORITY = 6;
  
  static constexpr size_t WORK_TASK_STACK_SIZE = 4096;
  static constexpr int WORK_TASK_PRIORITY = 5;

  static constexpr size_t REQUEST_QUEUE_SIZE = 4 * NODEBUS_PAYLOAD_MAX_SIZE;

  // --- Protocol Layouts ---
  static constexpr uint8_t VERSION_V1 = 0;
  static constexpr uint8_t META_STATUS_REQUEST = 0;
  static constexpr uint8_t META_STATUS_RES_OK = 1;
  // tells the sender to re-send the packet
  static constexpr uint8_t META_STATUS_RES_RETRY = 2;

  static constexpr size_t MAX_REQUEST_RETRIES = 8;
  static constexpr uint32_t REQUEST_RETRY_DELAY_MS = 10;

  struct Metadata {
    uint8_t version;
    // status == 0   -> request
    // status == 1   -> response_ok
    // anything else -> error
    uint8_t status;
    // bits 0-4  -> slot
    // bits 5-15 -> generation
    uint16_t request_id;
  };

  struct ResponseFut {
    uint8_t status;
    uint8_t response[NODEBUS_PAYLOAD_MAX_SIZE];
    size_t response_size;
    TaskHandle_t waiter;
    uint16_t generation = 0; // Incremented on allocation, encoded into request_id
  };

  // --- Internal Task Execution ---
  void tick();
  static void task_tick(void *arg);
  
  void worker();
  static void task_worker(void *arg);

  // --- Future Slot Management ---
  size_t allocate_fut();
  void free_fut(size_t id);

  // --- Driver Dependencies ---
  Stream &stream;
  NodebusParser parser;
  Mutex<NodebusSender> sender;

  // --- Sync & Allocation State ---
  Mutex<uint32_t> used_futs_mask = Mutex<uint32_t>(0);
  ResponseFut response_futs[32];

  // --- Application Worker Queue ---
  RingbufHandle_t request_queue;
  RequestHandler request_handler = nullptr;

  // --- OS Runtime Handles ---
  TickType_t timeout = pdMS_TO_TICKS(1000);
  bool cancel = false;
  TaskHandle_t shutdown_waiter;
  TaskHandle_t loop;
  TaskHandle_t work;
};

} // namespace osmium
