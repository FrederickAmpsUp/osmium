#include <Arduino.h>
#include <Preferences.h>
#include "core/protocol/transport.hpp"
#include "drivers/rs485.hpp"

#include "core/proto/node_master.hpp"
#include "core/proto/node_slave.hpp"

using namespace osmium;

enum {
  MASTER, SLAVE
} role = SLAVE;

static constexpr int RX485 = 18;
static constexpr int TX485 = 17;
static constexpr int DD485 = 16;

Preferences prefs;
NodebusTransport *transport;
RS485 rs485(RX485, TX485, DD485);

void parse_role(const String& input) {
  if (input.equalsIgnoreCase("MASTER")) {
    prefs.putUChar("role", MASTER);
    ESP.restart();
  } else if (input.equalsIgnoreCase("SLAVE")) {
    prefs.putUChar("role", SLAVE);
    ESP.restart();
  }
}

void load_role() {
  role = prefs.getUChar("role", SLAVE);
}

void setup() {
  prefs.begin("osmium", false);
  load_role();

  uint8_t bus_id = role == MASTER ? 1 : 2;
  static NodebusTransport transport_v(rs485, bus_id);
  transport = &transport_v;

  if (role == MASTER) {
    setup_master();
  } else {
    setup_slave();
  }
}

void loop() {

  if (role == MASTER) {
    loop_master();
  } else {
    loop_slave();
  }

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    parse_role(input);
  }

}
