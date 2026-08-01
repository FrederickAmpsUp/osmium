#include "drivers/rs485.hpp"
#include "logging/logger.hpp"
#include "logging/stream_sink.hpp"
#include "core/protocol/transport.hpp"

#include "core/protocol/message.hpp"
#include "core/data_provider.hpp"

#include <Arduino.h>

using namespace osmium;

static constexpr uint8_t NODE_ID = 2;
static constexpr uint8_t MASTER_ID = 1;

static constexpr int RX485 = 18;
static constexpr int TX485 = 17;
static constexpr int DD485 = 16;

static constexpr int BUTTON_PIN = 41;

RS485 rs485(RX485, TX485, DD485);
static NodebusTransport transport(rs485, NODE_ID);

class ButtonProvider : public DataProvider {
public:
  ButtonProvider(int pin) : pin(pin) {
  }

  void begin() {
    pinMode(pin, INPUT_PULLUP);
  }

  static const Metadata metadata;

  virtual const Metadata *get_metadata() const override {
    return &metadata;
  }

  virtual int sample() override {
    val = !digitalRead(pin);
    return SAMPLE_OK;
  }

  virtual Mutex<uint8_t *>::Guard lock_data() override {
    *this->mutex.lock() = &this->val;
    return this->mutex.lock();
  }

  virtual ~ButtonProvider() override = default;
private:
  int pin;

  uint8_t val;
  Mutex<uint8_t *> mutex;
};

const DataProvider::Metadata ButtonProvider::metadata = {
  .label = "button",
  .unit  = "",
  .dtype = "bool8",
  .dsize = 1,
};

ButtonProvider button_provider(BUTTON_PIN);

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

void setup() {
    static Logger &log = Logger::global();
    static StreamSink sink = { Serial };

    Serial.begin(115200);

    log.set_level(LogLevel::TRACE);
    sink.set_level(LogLevel::TRACE);
    log.add_sink(sink);

    rs485.begin(115200);

    log.info("Slave node started. ID=%d", NODE_ID);

    button_provider.begin();

    transport.set_request_handler(handle_request);

    InitRequestMessage req;

    req.id = NODE_ID;

    const DataProvider::Metadata *providers[] = {
      button_provider.get_metadata()
    };

    req.num_data_providers = sizeof(providers) / sizeof(providers[0]);
    req.data_providers = providers;

    uint8_t buf[128];

    size_t size = req.serialize(buf, sizeof(buf));

    if (!size) {
        log.error("Failed serializing InitRequest");
        return;
    }

    log.info("Sending InitRequest...");
    log.info("Size: %d bytes", size);


    uint8_t response_buf[32];
    size_t response_size = 0;

    auto status = transport.await_send_request(
        MASTER_ID,
        buf,
        size,
        response_buf,
        sizeof(response_buf),
        &response_size
    );


    if (status != NodebusTransport::OK) {
        log.warn("Init request failed: %d", status);
        return;
    }


    InitResponseMessage response;

    if (!response.deserialize(
        &response,
        response_buf,
        response_size
    )) {
        log.warn("Failed parsing InitResponse");
        return;
    }


    log.info(
        "Received assigned ID: %d",
        response.id
    );

    std::move(response).free();
}


void loop() {
    delay(1000);
}
