#include <Arduino.h>
#include "logging/logger.hpp"
#include "logging/stream_sink.hpp"

#include "drivers/nodebus_parser.hpp"

using namespace osmium;

void setup() {
  static Logger &log = Logger::global();
  static StreamSink ser_sink = { Serial };

  Serial.begin(115200); // change me
  
  log.set_level(LogLevel::TRACE);
  ser_sink.set_level(LogLevel::TRACE);

  log.add_sink(ser_sink);

  log.debug("ready");
}

void loop() {
  static Logger &log = Logger::global();
  static int loops = 0;
  static unsigned long long start_millis = millis();
  
  static NodebusParser nodebus_parser = { 1 };
  if (nodebus_parser.update(Serial) > 0) {
    log.info("Recieved packet:");
    log.info("  %s", nodebus_parser.get_payload());
  }

  if (millis() - start_millis > 1000) {
    int avg_ips = 1000 * loops / (millis() - start_millis);
    //log.info("AVERAGE IPS: %d", avg_ips);

    loops = 0;
    start_millis = millis();
  }

  ++loops;
}
