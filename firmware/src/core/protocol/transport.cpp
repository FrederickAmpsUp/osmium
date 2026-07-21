#include "core/protocol/transport.hpp"
#include "core/protocol/nodebus.hpp"
#include "logging/logger.hpp"
#include "util/mutex.hpp"

namespace osmium {

// --- Lifecycle ---

NodebusTransport::NodebusTransport(Stream &stream, uint8_t id) : stream(stream), parser(id), sender(id) {
  xTaskCreate(
    task_tick,
    "NodebusTransport::task_tick",
    TICK_TASK_STACK_SIZE,
    this,
    TICK_TASK_PRIORITY, &this->hnd_tick
  );

  this->request_queue = xRingbufferCreate(REQUEST_QUEUE_SIZE, RINGBUF_TYPE_NOSPLIT);

  xTaskCreate(
    task_worker,
    "NodebusTransport::task_worker",
    WORK_TASK_STACK_SIZE,
    this,
    WORK_TASK_PRIORITY, &this->hnd_worker
  );
}

NodebusTransport::~NodebusTransport() {
  this->shutdown_waiter = xTaskGetCurrentTaskHandle();

  ulTaskNotifyValueClear(NULL, 0xFFFFFFFF); // reset notification value to 0

  this->cancel = true; // tell the tasks to exit when possible

  // each call decrements the notification value by 1 on exit
  // each child task increments it by 1
  ulTaskNotifyTake(pdFALSE, portMAX_DELAY); // wait for notification value to be >0
  ulTaskNotifyTake(pdFALSE, portMAX_DELAY); // wait again since we have 2 tasks to exit
}

// --- Communication API ---

void NodebusTransport::send(uint8_t dest, const uint8_t *payload, size_t payload_size) {
  this->sender.lock()->send(this->stream, dest, payload, payload_size);
}

NodebusTransport::TransactionStatus NodebusTransport::await_transaction(uint8_t dest, uint8_t meta_status, uint16_t transaction_id, uint8_t promise_id,
                                                                        const uint8_t *packet, size_t packet_size,
                                                                        uint8_t *recv, size_t recv_capacity, size_t *recv_size) {
  static Logger &log = Logger::global();  

  uint8_t data[NODEBUS_PAYLOAD_MAX_SIZE];
  size_t data_size = packet_size + sizeof(Metadata);

  if (data_size > sizeof(data)) {
    log->error("Packet was oversized");
    return DATA_OVERSIZED;
  }

  auto &promise = this->promises[promise_id];

  Metadata metadata = {
    .version = VERSION_V1,
    .status = meta_status,
    .transaction_id = transaction_id,
  };

  // cpy metadata to data+0
  memcpy(data, &metadata, sizeof(metadata));
  // cpy packet after metadata
  memcpy(data + sizeof(metadata), packet, packet_size);

  // fill in promise fields
  promise.fut_waiter = xTaskGetCurrentTaskHandle();
  promise.transaction_id = transaction_id;

  // initialize to safe state
  promise.data_size = 0;
  promise.status = META_STATUS_RETRY;
 
  bool success = false;

  for (size_t i = 0; i < MAX_REQUEST_RETRIES; ++i) {
    this->send(dest, data, data_size);

    if (!ulTaskNotifyTake(pdTRUE, this->timeout)) {
      goto failed; // timed out, try next retry
    }

    { // codeblock so we can jump past status initialization
      uint8_t status = promise.status;

      if (status == META_STATUS_RETRY) {
        goto failed; // peer requested a retry
      }

      // RESPONSE or ACK
      success = (status <= META_STATUS_ACK);
    }
    break;

  failed:
    log.debug("Transaction failed, retrying after %d ms", REQUEST_RETRY_DELAY_MS);
    vTaskDelay(pdMS_TO_TICKS(REQUEST_RETRY_DELAY_MS));
  }

  if (success) {
    size_t n = std::min({
        recv_capacity,        // how much can we hold
        promise.data_size,    // how much is there to copy
        (size_t)NODEBUS_PAYLOAD_MAX_SIZE // how much does the protocol support
    });

    // copy payload to receive buffer
    memcpy(recv, promise.data, n);
    // report how many bytes were copied
    if (recv_size) *recv_size = n;
  }

  // prevents ghost immediate-unblocks if a packet arrived late
  ulTaskNotifyValueClear(nullptr, 0xFFFFFFFF);

  // maybe a different status than TIMED_OUT is appropriate here
  return success ? OK : TIMED_OUT;
}

NodebusTransport::TransactionStatus NodebusTransport::await_send_request(uint8_t dest, const uint8_t *request, size_t request_size, 
                                                                         uint8_t *response, size_t response_capacity, size_t *response_size) {
  static Logger &log = Logger::global();

  size_t promise_id = this->allocate_promise();
  if (promise_id == SIZE_MAX) return PROMISE_UNAVAILABLE;

  // generate a new transaction id
  uint16_t transaction_id = promise_id | (this->promises[promise_id].generation << 5);

  // send, wait for response
  TransactionStatus status = await_transaction(dest, META_STATUS_REQUEST, transaction_id, promise_id, request, request_size, response, response_capacity, response_size);
  this->free_promise(promise_id); // we are done with it here
  if (status != OK) {
    return status;
  }

  // send an ACK with the same transcation id
  Metadata ack = {
    .version = VERSION_V1,
    .status = META_STATUS_ACK,
    .transaction_id = transaction_id
  };
  // we don't use await_transaction because we don't care about a reply
  this->send(dest, (uint8_t *)&ack, sizeof(ack));
  return OK;
}

NodebusTransport::TransactionStatus NodebusTransport::await_send_response(uint8_t dest, uint16_t request_id, const uint8_t *response, size_t response_size) {
  size_t promise_id = this->allocate_promise();
  if (promise_id == SIZE_MAX) return PROMISE_UNAVAILABLE;

  // send, wait for ACK
  TransactionStatus status = await_transaction(dest, META_STATUS_RESPONSE, request_id, promise_id, response, response_size, nullptr, 0, nullptr);
  this->free_promise(promise_id);
  return status;
}

// --- Internal Task Execution ---

void NodebusTransport::tick_request(const Metadata &metadata, const uint8_t *payload, size_t payload_size) {
  static Logger &log = Logger::global();
  
  QueuedRequest queued_request = {
    .sender = this->parser.get_sender_id(),
    .request_id = metadata.transaction_id
  };

  // bytes remaining after metadata
  size_t req_size = payload_size - sizeof(metadata);

  void *item = nullptr;
  // allocate (sizeof(queue_request) + req_size) bytes in the ringbuffer
  if (xRingbufferSendAcquire(this->request_queue, &item, sizeof(queued_request) + req_size, 0) != pdPASS) {
    log.error("Failed to allocate memory for request.");
    return;
  }

  // copy the request info (sender, id) to the buffer
  memcpy(item, &queued_request, sizeof(queued_request));
  // copy the payload after request info
  memcpy((uint8_t *)item + sizeof(queued_request), payload + sizeof(metadata), req_size);
  if (xRingbufferSendComplete(this->request_queue, item) != pdTRUE) {
    log.error("Failed to post request to ringbuffer.");
  }
}

void NodebusTransport::tick_response(const Metadata &metadata, const uint8_t *payload, size_t payload_size) {
    static Logger &log = Logger::global();

    size_t promise_id = this->find_promise(metadata.transaction_id);
    if (promise_id == SIZE_MAX) {
      log.warn("Could not find promise matching transaction ID 0x%04X.", metadata.transaction_id);
      return;
    }

    TransactionPromise &promise = this->promises[promise_id];

    TaskHandle_t task = promise.fut_waiter;
    if (!task) {
      log.warn("Task in slot %d not set.", promise_id);
      return;
    }

    // forward response status
    promise.status = metadata.status;

    // bytes remaining after metadata
    promise.data_size = payload_size - sizeof(metadata);
    // copy response payload into promise buffer
    memcpy(promise.data, payload + sizeof(metadata), promise.data_size);

    // wake the task
    xTaskNotifyGive(task);
}

void NodebusTransport::tick_ack(const Metadata &metadata) {
    static Logger &log = Logger::global();
  
    size_t promise_id = this->find_promise(metadata.transaction_id);
    if (promise_id == SIZE_MAX) {
      log.warn("Could not find promise matching transaction ID 0x%04X.", metadata.transaction_id);
      return;
    }

    TransactionPromise &promise = this->promises[promise_id];

    TaskHandle_t task = promise.fut_waiter;
    if (!task) {
      log.warn("Task in slot %d not set.", promise_id);
      return;
    }

    promise.status = metadata.status;

    promise.data_size = 0; // ACKs carry no data

    // wake the task
    xTaskNotifyGive(task);
}

void NodebusTransport::tick() {
  static Logger &log = Logger::global();

  // if update > 0 we have a new packet available
  if (this->parser.update(this->stream) <= 0) return;

  uint8_t payload_size = 0;
  const uint8_t *payload = this->parser.get_payload(&payload_size);

  if (payload_size < sizeof(Metadata)) {
    log.debug("Packet contains no metadata");
    return;
  }

  // copy metadata out of payload
  Metadata metadata;
  memcpy(&metadata, payload, sizeof(metadata));

  if (metadata.version != VERSION_V1) {
    log.debug("Packet has bad version");
    return;
  }

  switch (metadata.status) {
    case META_STATUS_REQUEST:
      this->tick_request(metadata, payload, payload_size);
    break;
    case META_STATUS_RESPONSE:
      this->tick_response(metadata, payload, payload_size);
    break;
    case META_STATUS_ACK:
      this->tick_ack(metadata);
    break;
    default: // nominally unreachable
      log.warn("Unknown metadata status.");
    break;
  }
}

void NodebusTransport::task_tick(void *arg) {
  NodebusTransport *this_ = (NodebusTransport *)arg;

  while (!this_->cancel) {
    this_->tick();
    vTaskDelay(pdMS_TO_TICKS(1)); // let the cpu breathe
  }

  // let the destructor task know we have shut down
  xTaskNotifyGive(this_->shutdown_waiter);
  vTaskDelete(nullptr);
}

void NodebusTransport::worker() {
  size_t payload_size = 0;
  const uint8_t *payload = (const uint8_t *)xRingbufferReceive(this->request_queue, &payload_size, pdMS_TO_TICKS(10));
  
  if (!payload) return; 

  QueuedRequest queued_request;
  memcpy(&queued_request, payload, sizeof(queued_request));
  
  if (this->request_handler)
    this->request_handler(queued_request.sender, queued_request.request_id, payload + sizeof(queued_request), payload_size - sizeof(queued_request));
    
  vRingbufferReturnItem(this->request_queue, (void *)payload);
}

void NodebusTransport::task_worker(void *arg) {
  NodebusTransport *this_ = (NodebusTransport *)arg;

  while (!this_->cancel) {
    this_->worker();
    vTaskDelay(pdMS_TO_TICKS(1));
  }

  // let the destructor task know we have shut down
  xTaskNotifyGive(this_->shutdown_waiter);
  vTaskDelete(nullptr);
}

// --- Future Slot Management ---

size_t NodebusTransport::allocate_promise() {
  // hold the mutex lock until this function exits
  // prevents conflicting allocations
  auto used_promises_mask = this->used_promises_mask.lock();

  uint32_t free_mask = ~*used_promises_mask;

  // no free slots
  if (!free_mask) return SIZE_MAX;

  // gives the index of the first set bit in the mask
  int i = __builtin_ctz(free_mask);
  // set the used bit
  *used_promises_mask |= (1u << i);

  this->promises[i].generation++;

  return i;
}

void NodebusTransport::free_promise(size_t id) {
  // clear the bit in the mask
  *this->used_promises_mask.lock() &= ~(1u << id);
}

size_t NodebusTransport::find_promise(uint16_t transaction_id) {
  auto lock = this->used_promises_mask.lock();
  // basic linear-time search for (transaction_id matches) and (slot is filled)
  for (size_t i = 0; i < sizeof(this->promises) / sizeof(this->promises[0]); ++i) {
    if (this->promises[i].transaction_id == transaction_id && (*lock & 1 << i))
      return i;
  }

  return SIZE_MAX;
}

} // namespace osmium
