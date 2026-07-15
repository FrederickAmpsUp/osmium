#include "drivers/rs485.hpp"
#include "logging/logger.hpp"
#include "logging/stream_sink.hpp"
#include "drivers/transport.hpp"

#include <Arduino.h>

using namespace osmium;

static constexpr bool NODE_ROLE_REQUESTER = true;

static constexpr uint8_t NODE_ID = NODE_ROLE_REQUESTER ? 1 : 2;
static constexpr uint8_t PEER_ID = NODE_ROLE_REQUESTER ? 2 : 1;

static constexpr int RX485 = 18;
static constexpr int TX485 = 17;
static constexpr int DD485 = 16;

RS485 rs485(RX485, TX485, DD485);

static NodebusTransport transport(rs485, NODE_ID);

static void handle_request(uint8_t sender, uint16_t request_id, const uint8_t *data, size_t size) {
  static Logger &log = Logger::global();

  if (size != sizeof(uint32_t) * 2) {
    log.warn("Bad request size: %d", size);
    return;
  }

  uint32_t a;
  uint32_t b;

  memcpy(&a, data, sizeof(a));
  memcpy(&b, data + sizeof(a), sizeof(b));

  log.info("Request from node %d: %lu + %lu", sender, a, b);

  uint32_t result = a + b;

  auto status = transport.await_send_response(
      sender,
      request_id,
      reinterpret_cast<uint8_t *>(&result),
      sizeof(result)
  );

  if (status != NodebusTransport::OK) {
    log.warn("Failed sending response: %d", status);
  }
}

void setup() {
  static Logger &log = Logger::global();
  static StreamSink ser_sink = { Serial };

  Serial.begin(115200);

  log.set_level(LogLevel::TRACE);
  ser_sink.set_level(LogLevel::TRACE);
  log.add_sink(ser_sink);

  rs485.begin(115200);

  transport.set_request_handler(handle_request);

  log.info("Nodebus test node started. ID=%d", NODE_ID);

  if constexpr (NODE_ROLE_REQUESTER) {
    Serial.println("Enter two numbers separated by a space:");
  }
}

void loop() {
  static Logger &log = Logger::global();

  if constexpr (NODE_ROLE_REQUESTER) {
    if (Serial.available()) {
      uint32_t a;
      uint32_t b;

      String line = Serial.readStringUntil('\n');

      if (sscanf(line.c_str(), "%lu %lu", &a, &b) != 2) {
        Serial.println("Format: <number> <number>");
        return;
      }

      uint32_t request[2] = {a, b};

      uint32_t response = 0;
      size_t response_size = 0;

      log.info("Sending request: %lu + %lu", a, b);

      auto status = transport.await_send_request(
          PEER_ID,
          reinterpret_cast<uint8_t *>(request),
          sizeof(request),
          reinterpret_cast<uint8_t *>(&response),
          sizeof(response),
          &response_size
      );

      if (status == NodebusTransport::OK) {
        log.info("Result: %lu", response);
      } else {
        log.warn("Request failed: %d", status);
      }

      Serial.println("Enter two numbers separated by a space:");
    }
  }

  delay(10);
}
