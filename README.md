# microWAV

Streaming WAV header parser for embedded devices. Parses WAV headers byte-by-byte with a 4-byte internal accumulator. No buffering required.

[![A project from the Open Home Foundation](https://www.openhomefoundation.org/badges/ohf-project.png)](https://www.openhomefoundation.org/)

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
if (result == micro_wav::WAV_PARSER_HEADER_READY) {
    uint32_t rate = parser.sample_rate();
    uint16_t channels = parser.num_channels();
    uint16_t bps = parser.bits_per_sample();
    auto fmt = parser.audio_format();             // WAV_FORMAT_PCM, WAV_FORMAT_IEEE_FLOAT, etc.
    uint32_t data_size = parser.data_chunk_size();
}
```

## Result Codes

`parse()` returns `WAVParseResult`: non-negative values indicate success/informational states, negative values indicate errors. See `wav_header_parser.h` for the full enum.

| Value | Description |
|---|---|
| `WAV_PARSER_NEED_MORE_DATA` | More bytes needed; call `parse()` again with additional data |
| `WAV_PARSER_HEADER_READY` | Header fully parsed; remaining bytes (`input + bytes_consumed`) are audio data |
| `WAV_PARSER_ERROR_FAILED` | Generic parse failure (e.g., malformed chunk) |
| `WAV_PARSER_ERROR_NO_WAVE` | RIFF container found but missing WAVE identifier |
| `WAV_PARSER_ERROR_NO_RIFF` | Input does not start with a RIFF tag |

## Audio Formats

`audio_format()` returns `WAVAudioFormat` after the header is parsed. Known format tags are mapped to named values; unrecognized tags return `WAV_FORMAT_UNKNOWN`.

| Value | Description |
|---|---|
| `WAV_FORMAT_PCM` | Uncompressed integer PCM |
| `WAV_FORMAT_IEEE_FLOAT` | IEEE 754 floating-point |
| `WAV_FORMAT_ALAW` | A-law companded |
| `WAV_FORMAT_MULAW` | μ-law companded |
| `WAV_FORMAT_UNKNOWN` | Unrecognized format tag |

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

## Links

- [Wave File Specifications](https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html) — McGill University WAV format reference
- [wav-decoder](https://github.com/synesthesiam/wav-decoder) — WAV decoder by Mike Hansen
