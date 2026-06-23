#include "logging/logger.hpp"

namespace osmium {

void Logger::add_sink(LogSink &sink) {
  if (this->num_sinks < LOGGER_MAX_SINKS) {
    this->sinks[this->num_sinks] = &sink;
    this->num_sinks++;
  }
}

Logger &Logger::global() {
  static Logger logger;
  return logger;
}

} // namespace osmium
