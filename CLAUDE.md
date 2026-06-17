# microWAV: Claude Development Guide

Byte-by-byte streaming WAV parser and decoder for resource-constrained embedded
devices. Single-header/single-source C++11 static library, zero dynamic
allocation, no external dependencies. Apache 2.0.

## Documentation Map

- [README.md](README.md): Public API, usage example, result codes, audio formats, testing, known limitations
- [tests/fuzz/README.md](tests/fuzz/README.md): libFuzzer harness build and run instructions

## Layout

```text
include/micro_wav/wav_decoder.h  # Public API (WAVDecoder, result codes, format enums)
src/wav_decoder.cpp              # Implementation (header parser + audio decoder)
cmake/                           # Build modules (sources.cmake, host.cmake, esp-idf.cmake)
Kconfig                          # ESP-IDF config (CONFIG_MICRO_WAV_COMPONENT library sentinel)
tests/test_wav_decoder.cpp       # ctest suite driven by hand-built fixtures
tests/wav_test_data.h            # Hand-constructed WAV byte fixtures
tests/write_test_wavs.cpp        # Fixture writer (BUILD_TEST_WAV_WRITER=ON) for regeneration
tests/fuzz/                      # libFuzzer harness (self-contained CMake project, not in ctest)
script/clang-tidy.sh             # Lint wrapper
```

## Build and Test

```bash
# Build (host), warnings as errors
cmake -B build -DENABLE_WERROR=ON && cmake --build build

# Unit tests with sanitizers
cmake -B build -DENABLE_TESTS=ON -DENABLE_SANITIZERS=ON && cmake --build build
ctest --test-dir build --output-on-failure

# Lint
./script/clang-tidy.sh        # check
./script/clang-tidy.sh --fix  # auto-fix

# Pre-commit (formatting, markdown lint, etc.)
pre-commit run --all-files
```

`-DENABLE_WERROR=ON` is off by default; add it to any host or test build to
treat warnings as errors. The build auto-detects ESP-IDF vs host via
`cmake/esp-idf.cmake` and `cmake/host.cmake`. Distribution targets ESP-IDF
Component Manager, PlatformIO, and CMake subdirectory integration.

## Architecture

`WAVDecoder` exposes one `decode()` call that handles both header parsing and
audio decoding. Callers feed arbitrary-sized byte chunks; `decode()` returns
`WAV_DECODER_NEED_MORE_DATA`, `WAV_DECODER_HEADER_READY`, `WAV_DECODER_SUCCESS`,
`WAV_DECODER_END_OF_STREAM`, or a negative error/warning code.

A 4-byte accumulator (`buf_[4]`) collects field data during header parsing and
buffers partial samples during decoding. The header parser handles standard and
extended fmt chunks (WAVE_FORMAT_EXTENSIBLE) and auto-skips unknown chunks with
RIFF-compliant even-byte alignment. Audio decoding supports PCM (8/16/24/32-bit),
G.711 A-law/mu-law (decoded to 16-bit PCM), and IEEE float 32-bit (decoded to
32-bit integer PCM).

## Working Notes

- `decode()` returns `WAV_DECODER_HEADER_READY` once when the header completes; the stream-info accessors (`get_sample_rate()`, `get_channels()`, `get_bits_per_sample()`, `get_audio_format()`) are only valid after that. Call again to get audio.
- Output sample width is `get_bytes_per_output_sample()`, which may differ from input bit depth (A-law/mu-law expand to 16-bit, float to 32-bit). Size the output buffer accordingly; too small yields `WAV_DECODER_WARNING_OUTPUT_TOO_SMALL`.
- `samples_decoded` and `bytes_consumed` are out-params updated every call; advance the input pointer by `bytes_consumed`.
- A zero-length `data` chunk is a valid streaming sentinel (unknown length); the decoder keeps returning `NEED_MORE_DATA` rather than `END_OF_STREAM` on input exhaustion. See the comment near `wav_decoder.h:183`.
- IEEE float decoding assumes little-endian IEEE 754 floats (true for ESP32/x86/ARM, wrong on big-endian).
- `reset()` returns the decoder to its initial state for a new stream; no reallocation.

## Code Style

- Google C++ base style (clang-format), 4-space indent, 100-char column limit
- Private members suffixed with `_`
- No exceptions, no STL containers; enum return codes, raw arrays, bit-shifting for endianness
- Strict warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`
