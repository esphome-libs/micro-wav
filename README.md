# microWAV - Streaming WAV Decoder

[![CI](https://github.com/esphome-libs/micro-wav/actions/workflows/ci.yml/badge.svg)](https://github.com/esphome-libs/micro-wav/actions/workflows/ci.yml)

Streaming WAV parser and decoder for embedded devices. Extracts audio from WAV containers byte-by-byte with a 4-byte internal accumulator. No external buffering required.

[![A project from the Open Home Foundation](https://www.openhomefoundation.org/badges/ohf-project.png)](https://www.openhomefoundation.org/)

## Features

- **Wide format support**: PCM (8/16/24/32-bit), G.711 A-law/mu-law, and IEEE float 32-bit
- **Byte-by-byte streaming**: Feed data in any chunk size
- **Single API**: One `decode()` call for both header parsing and audio decoding
- **Chunk-aware**: Automatically skips unknown chunks (LIST, INFO, etc.)
- **Extensible headers**: Handles standard and extended fmt chunks (WAVE_FORMAT_EXTENSIBLE)
- **No dynamic allocation**: Zero heap usage

## Usage Example

### ESP-IDF Component Manager

```yaml
dependencies:
  micro-wav:
    git: https://github.com/esphome-libs/micro-wav.git
```

### PlatformIO

```ini
lib_deps = https://github.com/esphome-libs/micro-wav.git
```

### CMake Subdirectory

```cmake
add_subdirectory(micro-wav)
target_link_libraries(your_target PRIVATE micro_wav)
```

### Decoding

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

## API Reference

### Methods

| Method | Description |
| --- | --- |
| `decode(input, input_len, output, output_size, bytes_consumed, samples_decoded)` | Feed input bytes; parses header or decodes audio samples |
| `reset()` | Reset decoder to initial state for a new stream |
| `get_sample_rate()` | Sample rate in Hz |
| `get_channels()` | Number of audio channels |
| `get_bits_per_sample()` | Output bit depth per sample |
| `get_audio_format()` | Audio format tag (`WAVAudioFormat`) |
| `get_data_chunk_size()` | Size of the data chunk in bytes |
| `get_bytes_per_output_sample()` | Bytes per decoded output sample |
| `get_bytes_remaining()` | Audio data bytes remaining to decode |
| `is_header_ready()` | Whether header parsing is complete |

### Result Codes

`decode()` returns `WAVDecoderResult`: non-negative values indicate success/informational states, negative values indicate errors or warnings.

| Value | Description |
| --- | --- |
| `WAV_DECODER_SUCCESS` | Samples decoded (check `samples_decoded`) |
| `WAV_DECODER_HEADER_READY` | Header fully parsed; stream info available |
| `WAV_DECODER_END_OF_STREAM` | All data chunk bytes consumed |
| `WAV_DECODER_NEED_MORE_DATA` | More bytes needed; call `decode()` again with additional data |
| `WAV_DECODER_WARNING_OUTPUT_TOO_SMALL` | Output buffer is null or too small for one sample |
| `WAV_DECODER_ERROR_UNSUPPORTED` | Audio format not supported; e.g., 64-bit float, unknown codec |
| `WAV_DECODER_ERROR_FAILED` | Generic decode failure; e.g., malformed chunk |
| `WAV_DECODER_ERROR_NO_WAVE` | RIFF container found but missing WAVE identifier |
| `WAV_DECODER_ERROR_NO_RIFF` | Input does not start with a RIFF tag |

### Audio Formats

`get_audio_format()` returns `WAVAudioFormat` after the header is parsed. Unrecognized tags return `WAV_FORMAT_UNKNOWN`.

| Value | Description |
| --- | --- |
| `WAV_FORMAT_PCM` | Uncompressed integer PCM |
| `WAV_FORMAT_IEEE_FLOAT` | IEEE 754 floating-point |
| `WAV_FORMAT_ALAW` | A-law companded |
| `WAV_FORMAT_MULAW` | mu-law companded |
| `WAV_FORMAT_UNKNOWN` | Unrecognized format tag |

## Known Limitations

- IEEE float decoding assumes the host platform uses little-endian IEEE 754 floats. This is true for all common targets (`ESP32`, `x86`, `ARM Cortex`) but will produce incorrect results on big-endian platforms.

## License

Apache 2.0

## Links

- [Wave File Specifications](https://www.mmsp.ece.mcgill.ca/Documents/AudioFormats/WAVE/WAVE.html) - McGill University WAV format reference
- [wav-decoder](https://github.com/synesthesiam/wav-decoder) - WAV decoder by Mike Hansen
