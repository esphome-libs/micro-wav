# microWAV

Streaming WAV decoder for embedded devices. Decodes WAV audio byte-by-byte with a 4-byte internal accumulator. No buffering required.

[![A project from the Open Home Foundation](https://www.openhomefoundation.org/badges/ohf-project.png)](https://www.openhomefoundation.org/)

## Features

- Byte-by-byte streaming: feed data in any chunk size
- Unified `decode()` API for header parsing and audio decoding
- Decodes PCM (8/16/24/32-bit), G.711 A-law/mu-law, and IEEE float 32-bit
- Automatically skips unknown chunks (LIST, INFO, etc.)
- Handles standard and extended fmt chunks (WAVE_FORMAT_EXTENSIBLE)
- No dynamic allocation
- C++11, no STL dependencies

## Usage

```cpp
#include "micro_wav/wav_decoder.h"

micro_wav::WAVDecoder decoder;
size_t bytes_consumed = 0;
size_t samples_decoded = 0;

// Header phase: feed data with output=nullptr until HEADER_READY
auto result = decoder.decode(data, len, nullptr, 0, bytes_consumed, samples_decoded);

if (result == micro_wav::WAV_DECODER_HEADER_READY) {
    uint32_t rate = decoder.sample_rate();
    uint16_t channels = decoder.num_channels();
    uint16_t bps = decoder.bits_per_sample();  // output depth
    auto fmt = decoder.audio_format();

    // Audio phase: decode samples into output buffer
    uint8_t output[1024];
    result = decoder.decode(audio_data, audio_len, output, sizeof(output),
                            bytes_consumed, samples_decoded);
    // WAV_DECODER_SUCCESS = samples decoded
    // WAV_DECODER_END_OF_STREAM = all data consumed
}
```

## Result Codes

`decode()` returns `WAVDecoderResult`: non-negative values indicate success/informational states, negative values indicate errors. See `wav_decoder.h` for the full enum.

| Value | Description |
|---|---|
| `WAV_DECODER_SUCCESS` | Samples decoded (check `samples_decoded`) |
| `WAV_DECODER_HEADER_READY` | Header fully parsed; stream info available |
| `WAV_DECODER_END_OF_STREAM` | All data chunk bytes consumed |
| `WAV_DECODER_NEED_MORE_DATA` | More bytes needed; call `decode()` again with additional data |
| `WAV_DECODER_ERROR_UNSUPPORTED` | Audio format not supported (e.g., 64-bit float, unknown codec) |
| `WAV_DECODER_ERROR_FAILED` | Generic decode failure (e.g., malformed chunk) |
| `WAV_DECODER_ERROR_NO_WAVE` | RIFF container found but missing WAVE identifier |
| `WAV_DECODER_ERROR_NO_RIFF` | Input does not start with a RIFF tag |

## Audio Formats

`audio_format()` returns `WAVAudioFormat` after the header is parsed. Known format tags are mapped to named values; unrecognized tags return `WAV_FORMAT_UNKNOWN`.

| Value | Description |
|---|---|
| `WAV_FORMAT_PCM` | Uncompressed integer PCM |
| `WAV_FORMAT_IEEE_FLOAT` | IEEE 754 floating-point |
| `WAV_FORMAT_ALAW` | A-law companded |
| `WAV_FORMAT_MULAW` | mu-law companded |
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
