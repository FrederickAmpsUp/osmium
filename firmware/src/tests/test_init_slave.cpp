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

RS485 rs485(RX485, TX485, DD485);
static NodebusTransport transport(rs485, NODE_ID);


static DataProvider::Metadata dummy_metadata = {
    .label = "temperature",
    .unit  = "C",
    .dtype = "f32",
    .dsize = sizeof(float)
};


void setup() {
    static Logger &log = Logger::global();
    static StreamSink sink = { Serial };

    Serial.begin(115200);

    log.set_level(LogLevel::TRACE);
    sink.set_level(LogLevel::TRACE);
    log.add_sink(sink);

    rs485.begin(115200);

    log.info("Slave node started. ID=%d", NODE_ID);


    InitRequestMessage req;

    req.id = NODE_ID;
    req.num_data_providers = 1;

    DataProvider::Metadata *providers[] = {
        &dummy_metadata
    };

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
