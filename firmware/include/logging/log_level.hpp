#pragma once

namespace osmium {

enum class LogLevel {
  TRACE, DEBUG, INFO,
  WARN, ERROR
};

static constexpr LogLevel LOG_LEVEL_DEFAULT = LogLevel::WARN;

} // namespace osmium
