#include "core/proto/node_master.hpp"
#include "core/protocol/transport.hpp"
#include "core/protocol/message.hpp"

#include "logging/logger.hpp"

extern osmium::NodebusTransport *transport;
namespace osmium {

static void handle_init_request(
  uint8_t sender,
  uint16_t request_id,
  const uint8_t *data,
  size_t size
) {
  static Logger &log = Logger::global();
  InitRequestMessage req;

  if (!deserialize(&req, data, size)) return;

  log.info("Init request:");
  log.info("  Requested id: %d", req.id);
  log.info("  Providers: %lu", req.num_data_providers);

  for (uint32_t i = 0; i < req.num_data_providers; ++i) {
    auto *m = req.data_providers[i];

    log.info(
      "  [%lu] %s (%s) type=%s size=%lu",
      i,
      m->label,
      m->unit,
      m->dtype,
      m->dsize
    );
  }

  uint8_t assigned_id = sender;

  if (req.id != 0) {
    assigned_id = req.id;
  }

  InitResponseMessage response;
  response.id = assigned_id;

  uint8_t buf[64];

  size_t response_size = response.serialize(buf, sizeof(buf));

  if (!response_size) {
    log.warn("Failed serializing InitResponse");
    std::move(req).free();
    return;
  }

  auto status = transport->await_send_response(
    sender,
    request_id,
    buf,
    response_size
  );

  if (status != NodebusTransport::OK) {
    log.warn("Failed sending InitResponse: %d", status);
  } else {
    log.info("Sent assigned ID=%d", assigned_id);
  }

  std::move(req).free();
}

static void handle_request(
    uint8_t sender,
    uint16_t request_id,
    const uint8_t *data,
    size_t size
) {
  static Logger &log = Logger::global();

  log.info("Request from node %d, size=%d", sender, size);

  MessageType type = get_type(data, size);
  switch (type) {
    case MessageType::INIT_REQUEST_MESSAGE:
      handle_init_request(sender, request_id, data, size);
      break;
    default:
      log.warn("Unknown request type %02x", type);
  }
}

void setup_master() {
  static Logger &log = Logger::global();

  log.info(" === MASTER NODE SETUP === ");

  transport->set_request_handler(handle_request);
}

void loop_master() {
  // TODO: request data, forward to webui through ws

  delay(10);
}

} // namespace osmium
