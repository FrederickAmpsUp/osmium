#pragma once

#include "drivers/rs485.hpp"
#include "core/protocol/transport.hpp"

namespace osmium {

class NodeManager {
public:

private:
  RS485 rs485;
  NodebusTransport transport;
};

} // namespace osmium
