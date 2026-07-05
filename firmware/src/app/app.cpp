#include "app/app.hpp"
#include "logging/logger.hpp"
#include "logging/stream_sink.hpp"

namespace osmium {

// figure out how to change id
App::App() : nodebus_parser(1), nodebus_sender(1) {}

void App::setup() {
  static Logger &log = Logger::global();
  static StreamSink serial_sink = { Serial };

  Serial.begin(115200);

  log.set_level(LOG_LEVEL);

  log.add_sink(serial_sink);

  log.debug("Setup finished.");
}

void App::tick() {
}

} // namespace osmium
