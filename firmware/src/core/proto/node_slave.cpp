#include "core/proto/node_slave.hpp"
#include "core/protocol/transport.hpp"
#include "core/protocol/message.hpp"

#include "core/data_provider.hpp"

#include "logging/logger.hpp"

extern osmium::NodebusTransport *transport;

namespace osmium {

static void handle_request(
  uint8_t sender,
  uint16_t request_id,
  const uint8_t *data,
  size_t size
) {
  static Logger &log = Logger::global();

  log.info("Request from node %d, size=%d", sender, size);

  // don't care about the request for now

  (void)button_provider.sample();
  auto button_data = button_provider.lock_data();

  DataResponseMessage response;
  response.size = (size_t)button_provider.get_metadata()->dsize;
  response.data = (uint8_t *)*button_data;

  uint8_t buf[16];

  size_t response_size = response.serialize(buf, sizeof(buf));

  if (!response_size) {
    log.warn("Failed serializing DataResponse");
  }

  auto status = transport.await_send_response(
    sender, request_id, buf, response_size
  );

  if (status != NodebusTransport::OK) {
      log.warn("Failed sending DataResponse: %d", status);
  } else {
      log.info("Sent data %01x", buf[0]);
  }

  std::move(response).free();
}

void setup_slave() {
  static Logger &log = Logger::global();

  log.info(" === MASTER NODE SETUP === ");

  transport->set_request_handler(handle_request);
}

void loop_slave() {

}

}
