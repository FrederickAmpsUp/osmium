#pragma once

#include "drivers/nodebus_parser.hpp"
#include "drivers/nodebus_sender.hpp"
#include "logging/log_level.hpp"

namespace osmium {

class App {
public:
  App();

  void setup();
  void tick();

  static constexpr LogLevel LOG_LEVEL = LogLevel::TRACE;
private:
  NodebusParser nodebus_parser;
  NodebusSender nodebus_sender;
};

} // namespace osmium
