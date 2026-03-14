# microWAV

Streaming WAV decoder for embedded devices. Decodes WAV audio byte-by-byte with a 4-byte internal accumulator. No buffering required.

[![A project from the Open Home Foundation](https://www.openhomefoundation.org/badges/ohf-project.png)](https://www.openhomefoundation.org/)

## Features

- Byte-by-byte streaming: feed data in any chunk size
- Single `decode()` call for both header parsing and audio decoding
- Decodes PCM (8/16/24/32-bit), G.711 A-law/mu-law, and IEEE float 32-bit
- Automatically skips unknown chunks (LIST, INFO, etc.)
- Handles standard and extended fmt chunks (WAVE_FORMAT_EXTENSIBLE)
- No dynamic allocation
- C++11, no STL dependencies

## Usage

```cpp
#include "micro_wav/wav_decoder.h"

micro_wav::WAVDecoder decoder;
uint8_t data[512];
uint8_t output[1024];
size_t bytes_consumed = 0;
size_t samples_decoded = 0;

// Feed chunks as they arrive from a file, network socket, etc.
while (size_t len = read_chunk(data, sizeof(data))) {
    const uint8_t* p = data;

    while (len > 0) {
        auto result = decoder.decode(p, len, output, sizeof(output),
                                     bytes_consumed, samples_decoded);
        p += bytes_consumed;
        len -= bytes_consumed;

        if (result == micro_wav::WAV_DECODER_HEADER_READY) {
            // Header parsed, stream info now available
            uint32_t rate = decoder.get_sample_rate();
            uint16_t channels = decoder.get_channels();
            uint16_t bps = decoder.get_bits_per_sample();
        } else if (result == micro_wav::WAV_DECODER_SUCCESS) {
            // output contains samples_decoded decoded samples
            process_audio(output, samples_decoded);
        } else if (result == micro_wav::WAV_DECODER_END_OF_STREAM) {
            return;
        } else if (result < 0) {
            handle_error(result);
            return;
        }
        // NEED_MORE_DATA: inner loop exits, outer loop reads next chunk
    }
}
```

## Result Codes

`decode()` returns `WAVDecoderResult`: non-negative values indicate success/informational states, negative values indicate errors or warnings. See `wav_decoder.h` for the full enum.

| Value | Description |
|---|---|
| `WAV_DECODER_SUCCESS` | Samples decoded (check `samples_decoded`) |
| `WAV_DECODER_HEADER_READY` | Header fully parsed; stream info available |
| `WAV_DECODER_END_OF_STREAM` | All data chunk bytes consumed |
| `WAV_DECODER_NEED_MORE_DATA` | More bytes needed; call `decode()` again with additional data |
| `WAV_DECODER_WARNING_OUTPUT_TOO_SMALL` | Output buffer is null or too small for one sample |
| `WAV_DECODER_ERROR_UNSUPPORTED` | Audio format not supported; e.g., 64-bit float, unknown codec |
| `WAV_DECODER_ERROR_FAILED` | Generic decode failure; e.g., malformed chunk) |
| `WAV_DECODER_ERROR_NO_WAVE` | RIFF container found but missing WAVE identifier |
| `WAV_DECODER_ERROR_NO_RIFF` | Input does not start with a RIFF tag |

## Audio Formats

`get_audio_format()` returns `WAVAudioFormat` after the header is parsed. Known format tags are mapped to named values; unrecognized tags return `WAV_FORMAT_UNKNOWN`.

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

## Limitations

- IEEE float decoding assumes the host platform uses little-endian IEEE 754 floats. This is true for all common targets (ESP32, x86, ARM Cortex) but will produce incorrect results on big-endian platforms.

## License

Apache 2.0

## Links

- [Wave File Specifications](https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html) - McGill University WAV format reference
- [wav-decoder](https://github.com/synesthesiam/wav-decoder) - WAV decoder by Mike Hansen
