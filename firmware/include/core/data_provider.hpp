#pragma once

#include <cstdlib>
#include <cstdint>

namespace osmium {

class DataProvider {
public:
  struct Metadata {
    const char *label;
    const char *unit;
    
    // e.g. "f32", "u8"
    const char *dtype;
    size_t      dsize;
  };

  virtual const Metadata *get_metadata() const = 0;
  // writes get_metadata().dsize bytes
  virtual int sample(uint8_t *buffer) = 0;

  static constexpr int SAMPLE_OK = 0;

  virtual ~DataProvider() = default;
private:
};

} // namespace osmium
