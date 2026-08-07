#include "core/proto/node_slave.hpp"
#include "core/protocol/transport.hpp"
#include "core/protocol/message.hpp"

#include "data_providers/moisture_sensor.hpp"

#include "logging/logger.hpp"

extern osmium::NodebusTransport *transport;

namespace osmium {

static constexpr int MOISTURE_CHARGE_PIN = 0;
static constexpr int MOISTURE_SENSE_PIN  = 0;
MoistureSensorDataProvider moisture_provider(MOISTURE_CHARGE_PIN, MOISTURE_SENSE_PIN);

static void handle_request(
  uint8_t sender,
  uint16_t request_id,
  const uint8_t *data,
  size_t size
) {
  static Logger &log = Logger::global();

  log.info("Request from node %d, size=%d", sender, size);

  // don't care about the request for now, just send the data we have

  (void)moisture_provider.sample();
  auto moisture_data = moisture_provider.lock_data();

  DataResponseMessage response;
  response.size = (size_t)moisture_provider.get_metadata()->dsize;
  response.data = (uint8_t *)*moisture_data;

  uint8_t buf[16];

  size_t response_size = response.serialize(buf, sizeof(buf));

  if (!response_size) {
    log.warn("Failed serializing DataResponse");
  }

  auto status = transport->await_send_response(
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

  moisture_provider.begin();

  transport->set_request_handler(handle_request);

  InitRequestMessage req;

  req.id = 2;

  const DataProvider::Metadata *providers[] = {
    moisture_provider.get_metadata()
  };

  req.num_data_providers = sizeof(providers) / sizeof(providers[0]);
  req.data_providers = providers;

  uint8_t buf[128];

  size_t size = req.serialize(buf, sizeof(buf));

  if (!size) {
    log.error("Failed serializing InitRequest");
    ESP.restart();
  }

  log.info("Sending InitRequest...");
  log.info("Size: %d bytes", size);

  uint8_t response_buf[32];
  size_t response_size = 0;

  auto status = transport->await_send_request(
    1,
    buf, size,
    response_buf, sizeof(response_buf),
    &response_size
  );

  if (status != NodebusTransport::OK) {
    log.error("Init request failed: %d", status);
    ESP.restart();
  }

  InitResponseMessage response;

  if (!InitResponseMessage::deserialize(&response, response_buf, response_size)) {
    log.error("Failed parsing InitResponse");
    ESP.restart();
  }

  log.warn("Ignoring assigned id %d", response.id);

  std::move(response).free();
}

void loop_slave() {
  delay(10);
}

}
