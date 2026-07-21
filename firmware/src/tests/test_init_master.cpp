#include "drivers/rs485.hpp"
#include "logging/logger.hpp"
#include "logging/stream_sink.hpp"
#include "core/protocol/transport.hpp"

#include "core/protocol/message.hpp"
#include "core/data_provider.hpp"

#include <Arduino.h>

using namespace osmium;

static constexpr uint8_t NODE_ID = 1;

static constexpr int RX485 = 18;
static constexpr int TX485 = 17;
static constexpr int DD485 = 16;

RS485 rs485(RX485, TX485, DD485);
static NodebusTransport transport(rs485, NODE_ID);


static void handle_request(
    uint8_t sender,
    uint16_t request_id,
    const uint8_t *data,
    size_t size
) {
    static Logger &log = Logger::global();

    log.info("Request from node %d, size=%d", sender, size);

    InitRequestMessage req;

    if (!req.deserialize(&req, const_cast<uint8_t *>(data), size)) {
        log.warn("Failed to deserialize InitRequest");
        return;
    }

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

    auto status = transport.await_send_response(
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


void setup() {
    static Logger &log = Logger::global();
    static StreamSink sink = { Serial };

    Serial.begin(115200);

    log.set_level(LogLevel::TRACE);
    sink.set_level(LogLevel::TRACE);
    log.add_sink(sink);

    rs485.begin(115200);

    transport.set_request_handler(handle_request);

    log.info("Master node started. ID=%d", NODE_ID);
}


void loop() {
    delay(10);
}
