#pragma once

#include "nodebus.hpp"
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
  enum TransactionStatus {
    OK = 0,
    TIMED_OUT,
    DATA_OVERSIZED,
    PROMISE_UNAVAILABLE,
  };

  using RequestHandler = void (*)(uint8_t src, uint16_t id, const uint8_t *payload, size_t size);

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

  TransactionStatus await_transaction(uint8_t dest, uint8_t meta_status, uint16_t transaction_id, uint8_t promise_id,
                                      const uint8_t *packet, size_t packet_size,
                                      uint8_t *recv, size_t recv_capacity, size_t *recv_size);

  // Suspends calling task until a response is received or times out.
  // Writes up to min(response_capacity, NODEBUS_PAYLOAD_MAX_SIZE) to response buffer.
  TransactionStatus await_send_request(uint8_t dest, const uint8_t *request, size_t request_size, 
                                       uint8_t *response, size_t response_capacity, size_t *response_size);

  // Suspends calling task until an ACK is received or times out.
  TransactionStatus await_send_response(uint8_t dest, uint16_t request_id, const uint8_t *response, size_t response_size);

private:
  // --- Task Constants ---
  static constexpr size_t TICK_TASK_STACK_SIZE = 4096;
  static constexpr int TICK_TASK_PRIORITY = 6;
  
  static constexpr size_t WORK_TASK_STACK_SIZE = 4096;
  static constexpr int WORK_TASK_PRIORITY = 5;

  // up to 4-ish packets
  // probably only fit 3 because of metadata etc
  // but most packets aren't max size anyway
  static constexpr size_t REQUEST_QUEUE_SIZE = 4 * NODEBUS_PAYLOAD_MAX_SIZE;

  // --- Protocol Layouts ---
  static constexpr uint8_t VERSION_V1 = 0;

  // why isn't this an enum? don't ask me
  static constexpr uint8_t META_STATUS_REQUEST = 0;
  static constexpr uint8_t META_STATUS_RESPONSE = 1;
  static constexpr uint8_t META_STATUS_ACK = 2;
  // tells the sender to re-send the packet
  static constexpr uint8_t META_STATUS_RETRY = 3;

  static constexpr size_t MAX_REQUEST_RETRIES = 8;
  static constexpr uint32_t REQUEST_RETRY_DELAY_MS = 10;

  // this gets sent over the wire so we
  // make sure no bytes are wasted with ((packed))
  //
  // in the future we'll probably want
  // dedicated serialization/deserialization
  // code for this anyway so it's ABI independent
  struct __attribute__((packed)) Metadata {
    uint8_t version;
    uint8_t status;
    uint16_t transaction_id;
  };

  // something waiting on the result of a transaction
  struct TransactionPromise {
    // forwarded status from Metadata
    uint8_t status;
    // initialized to slot | (generation << 5), forwarded through replies
    // so transaction_id remains consistent across request, response, ACK
    uint16_t transaction_id;
    // the actual data from the packet
    uint8_t data[NODEBUS_PAYLOAD_MAX_SIZE];
    size_t data_size;

    // the task waiting on this transaction
    // woken when received
    TaskHandle_t fut_waiter;

    // makes sure we never process a stale packet
    // incremented in allocate_promise
    uint16_t generation = 0;
  };

  // prepended to request data in the ringbuf
  // so that request handlers know where to send
  // responses to
  struct QueuedRequest {
    uint8_t sender;
    uint16_t request_id;
  };

  // --- Internal Task Execution ---
  // tick_X is split out from switch cases
  void tick_request(const Metadata &metadata, const uint8_t *payload, size_t payload_size);
  void tick_response(const Metadata &metadata, const uint8_t *payload, size_t payload_size);
  void tick_ack(const Metadata &metadata);
  // receives data from the wire, wakes tasks
  // waiting on it if applicable and forwards
  // requests to worker() through the ringbuf
  void tick();

  // arg == this
  // calls this->tick() in a loop
  static void task_tick(void *arg);
  
  // receives requests from the ringbuf and
  // calls this->request_handler with them
  void worker();

  // arg == this
  // calls this->worker() in a loop
  static void task_worker(void *arg);

  // --- Future Slot Management ---
  // returns the slot (index) of the promise
  // or SIZE_MAX if none are available
  size_t allocate_promise();
  // marks the slot as available
  void free_promise(size_t id);

  // linear search to find a promise with
  // given transaction_id
  size_t find_promise(uint16_t transaction_id);

  // --- Driver Dependencies ---
  Stream &stream;
  NodebusParser parser;
  // mutex to support sending from multiple threads
  Mutex<NodebusSender> sender;

  // --- Sync & Allocation State ---
  // bitmask where mask & (1 << n) marks whether
  // promises[n] is available
  //
  // not really required but lets us allocate
  // promises extremely quickly, and the mutex
  // is handy in some scenarios
  Mutex<uint32_t> used_promises_mask = Mutex<uint32_t>(0);
  TransactionPromise promises[32];

  // --- Application Worker Queue ---
  // QueuedRequest followed by a data blob is
  // pushed here when a request is received
  RingbufHandle_t request_queue;
  RequestHandler request_handler = nullptr;

  // --- OS Runtime Handles ---
  // default transaction timeout = 1s
  TickType_t timeout = pdMS_TO_TICKS(1000);
  // set true in ~NodebusTransport()
  bool cancel = false;
  // so that ~NodebusTransport() can wait for
  // loop and work to shutdown
  TaskHandle_t shutdown_waiter;
  TaskHandle_t hnd_tick;
  TaskHandle_t hnd_worker;
};

} // namespace osmium
