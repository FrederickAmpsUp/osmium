#include "drivers/transport.hpp"
#include "drivers/nodebus.hpp"
#include "logging/logger.hpp"

namespace osmium {

// --- Lifecycle ---

NodebusTransport::NodebusTransport(Stream &stream, uint8_t id) : stream(stream), parser(id), sender(id) {
  xTaskCreate(
    task_tick,
    "NodebusTransport::task_tick",
    TICK_TASK_STACK_SIZE,
    this,
    TICK_TASK_PRIORITY, &this->loop
  );

  this->request_queue = xRingbufferCreate(REQUEST_QUEUE_SIZE, RINGBUF_TYPE_NOSPLIT);

  xTaskCreate(
    task_worker,
    "NodebusTransport::task_worker",
    WORK_TASK_STACK_SIZE,
    this,
    WORK_TASK_PRIORITY, &this->work
  );
}

NodebusTransport::~NodebusTransport() {
  this->shutdown_waiter = xTaskGetCurrentTaskHandle();

  this->cancel = true;
  xTaskNotifyGive(this->loop);

  ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
}

// --- Communication API ---

void NodebusTransport::send(uint8_t dest, const uint8_t *payload, size_t payload_size) {
  this->sender.lock()->send(this->stream, dest, payload, payload_size);
}

NodebusTransport::ResponseStatus NodebusTransport::await_send_request(uint8_t dest, const uint8_t *request, size_t request_size, uint8_t *response, size_t response_capacity, size_t *response_size) {
  static Logger &log = Logger::global();  

  uint8_t data[NODEBUS_PAYLOAD_MAX_SIZE];
  size_t data_size = request_size + sizeof(Metadata);

  if (data_size > sizeof(data)) {
    return REQUEST_OVERSIZED;
  }

  size_t fut_id = this->allocate_fut();
  if (fut_id == SIZE_MAX)
    return REQUEST_UNAVAILABLE;

  uint16_t request_id = (uint16_t)fut_id | (this->response_futs[fut_id].generation << 5);

  Metadata metadata = {
    .version = VERSION_V1,
    .status = 0,
    .request_id = request_id,
  };

  memcpy(data, &metadata, sizeof(metadata));
  memcpy(data + sizeof(metadata), request, request_size);
 
  auto &res = this->response_futs[fut_id];
  res.waiter = xTaskGetCurrentTaskHandle();
  res.response_size = 0;
 
  bool success = false;

  for (size_t i = 0; i < MAX_REQUEST_RETRIES; ++i) {
    if (i > 0) {
      log.debug("Request failed, retrying after %d ms", REQUEST_RETRY_DELAY_MS);
      vTaskDelay(pdMS_TO_TICKS(REQUEST_RETRY_DELAY_MS));
    }
    
    this->send(dest, data, data_size);

    if (!ulTaskNotifyTake(pdTRUE, this->timeout)) {
      continue; // timed out, try next retry
    }

    uint8_t status = response_futs[fut_id].status;

    if (status == META_STATUS_RES_RETRY) {
      continue; // peer requested a retry
    }

    success = (status == META_STATUS_RES_OK);
    break;
  }

  if (success) {
    if (response_capacity > NODEBUS_PAYLOAD_MAX_SIZE)
      response_capacity = NODEBUS_PAYLOAD_MAX_SIZE;

    memcpy(response, res.response, response_capacity);
    *response_size = res.response_size;
  }

  this->free_fut(fut_id);

  // prevents ghost immediate-unblocks if a packet arrived late
  ulTaskNotifyValueClear(nullptr, 0xFFFFFFFF);

  return success ? OK : TIMED_OUT;
}

// --- Internal Task Execution ---

void NodebusTransport::tick() {
  static Logger &log = Logger::global();

  if (this->parser.update(this->stream) <= 0) return; // nothing to parse

  uint8_t payload_size = 0;
  const uint8_t *payload = this->parser.get_payload(&payload_size);

  if (payload_size < sizeof(Metadata)) {
    log.debug("Packet contains no metadata");
    return;
  }
  Metadata *metadata = (Metadata *)payload;

  if (metadata->version != VERSION_V1) {
    log.debug("Packet has bad version");
  }

  if (metadata->status != META_STATUS_REQUEST) { // packet is a response
    size_t slot = metadata->request_id & 0x1F;
    uint16_t generation = metadata->request_id >> 5;
    
    // scope lock protects both the mask validation and the future modifications
    auto futs_lock = this->used_futs_mask.lock();
    if (!(*futs_lock & 1 << slot)) {
      log.warn("Received packet for slot %d but it is not filled.", slot);
      return;
    }
    ResponseFut &fut = this->response_futs[slot];
    if (fut.generation != generation) {
      log.warn("Received stale packet (generation %d).", generation);
      return;
    }

    TaskHandle_t task = fut.waiter;
    if (!task) {
      log.warn("Task in slot %d not set.", slot);
      return;
    }

    fut.status = metadata->status;

    size_t metadata_size = sizeof(Metadata);
    fut.response_size = payload_size - metadata_size;
    memcpy(fut.response, payload + sizeof(Metadata), fut.response_size);

    xTaskNotifyGive(fut.waiter);
  } else { // packet is a request
    size_t metadata_size = sizeof(Metadata);
    size_t req_size = payload_size - metadata_size;

    void *item = nullptr;

    if (xRingbufferSendAcquire(this->request_queue, (void **)&item, 1 + req_size, 0) != pdPASS) {
      log.error("Failed to allocate memory for request.");
      return;
    }

    uint8_t *buffer = (uint8_t *)item;
    buffer[0] = this->parser.get_sender_id();
    memcpy(buffer + 1, payload + metadata_size, req_size);
    if (xRingbufferSendComplete(this->request_queue, item) != pdTRUE) {
      log.error("Failed to post request to ringbuffer.");
    }
  }
}

void NodebusTransport::task_tick(void *arg) {
  NodebusTransport *this_ = (NodebusTransport *)arg;

  while (!this_->cancel) {
    this_->tick();
    vTaskDelay(pdMS_TO_TICKS(1)); // let the cpu breathe
  }

  xTaskNotifyGive(this_->shutdown_waiter);
  vTaskDelete(nullptr);
}

void NodebusTransport::worker() {
  size_t payload_size = 0;
  const uint8_t *payload = (const uint8_t *)xRingbufferReceive(this->request_queue, &payload_size, portMAX_DELAY);
  
  if (!payload) return; 
  
  if (this->request_handler)
    this->request_handler(payload[0], payload + 1, payload_size - 1);
    
  vRingbufferReturnItem(this->request_queue, (void *)payload);
}

void NodebusTransport::task_worker(void *arg) {
  NodebusTransport *this_ = (NodebusTransport *)arg;

  while (!this_->cancel) {
    this_->worker();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// --- Future Slot Management ---

size_t NodebusTransport::allocate_fut() {
  auto used_futs_mask = this->used_futs_mask.lock();

  uint32_t free_mask = ~*used_futs_mask;

  if (!free_mask) return SIZE_MAX;

  int i = __builtin_ctz(free_mask);
  *used_futs_mask |= (1u << i);

  this->response_futs[i].generation++;

  return i;
}

void NodebusTransport::free_fut(size_t id) {
  *this->used_futs_mask.lock() &= ~(1u << id);
}

} // namespace osmium
