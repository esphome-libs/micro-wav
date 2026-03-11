# microWAV

Streaming WAV header parser for embedded devices. Parses WAV headers byte-by-byte with a 4-byte internal accumulator — no buffering required.

## Features

- Byte-by-byte streaming: feed data in any chunk size
- Automatically skips unknown chunks (LIST, INFO, etc.)
- Handles standard and extended fmt chunks (WAVE_FORMAT_EXTENSIBLE)
- No dynamic allocation
- C++11, no STL dependencies

## Usage

```cpp
#include "micro_wav/wav_header_parser.h"

micro_wav::WAVHeaderParser parser;

// Feed data incrementally
size_t bytes_consumed = 0;
auto result = parser.parse(data, len, bytes_consumed);

// After HEADER_READY, remaining bytes (input + bytes_consumed) are audio data
if (result == micro_wav::WAVParseResult::HEADER_READY) {
    uint32_t rate = parser.sample_rate();
    uint16_t channels = parser.num_channels();
    uint16_t bps = parser.bits_per_sample();
    uint16_t fmt = parser.audio_format();       // 1=PCM, 3=IEEE float, etc.
    uint32_t data_size = parser.data_chunk_size();
}
```

## Integration

**ESP-IDF Component Manager:**

```yaml
dependencies:
  micro-wav:
    git: https://github.com/esphome-libs/micro-wav.git
```

**PlatformIO:**

```ini
lib_deps = https://github.com/esphome-libs/micro-wav.git
```

**CMake subdirectory:**

```cmake
add_subdirectory(micro-wav)
target_link_libraries(your_target PRIVATE micro_wav)
```

## License

Apache 2.0
