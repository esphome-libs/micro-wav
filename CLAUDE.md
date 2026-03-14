# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

microWAV is a byte-by-byte streaming WAV header parser for resource-constrained embedded devices. It's a single-header/single-source C++11 static library with zero dynamic allocation and no external dependencies.

## Build Commands

```bash
# Build (host)
cmake -B build -DENABLE_WERROR=ON && cmake --build build

# Lint (clang-tidy)
./script/clang-tidy.sh        # check
./script/clang-tidy.sh --fix  # auto-fix

# Pre-commit checks (formatting, markdown lint, etc.)
pre-commit run --all-files
```

There is no unit test suite; validation relies on clang-tidy static analysis and CI build checks.

## Architecture

The library is two files: `include/micro_wav/wav_header_parser.h` (public API) and `src/wav_header_parser.cpp` (implementation).

**`WAVHeaderParser`** uses a state machine to parse WAV headers incrementally. Callers feed arbitrary-sized byte chunks via `parse()`, which returns `WAV_PARSER_NEED_MORE_DATA`, `WAV_PARSER_HEADER_READY`, or an error. Internally, a 4-byte accumulator (`buf_[4]`) collects field data, and a skip counter handles unknown chunks. States progress: RIFF tag → RIFF size → WAVE tag → chunk identification → fmt field parsing (or skip) → data chunk detection.

The parser handles both standard and extended fmt chunks (WAVE_FORMAT_EXTENSIBLE) and auto-skips unknown chunks with RIFF-compliant even-byte alignment.

## Code Style

- Google C++ base style (clang-format), 4-space indent, 100-char column limit
- Private members suffixed with `_`
- No exceptions, no STL containers — enum return codes, raw arrays, bit-shifting for endianness
- Strict warnings: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion`

## Build Targets

The CMake config auto-detects ESP-IDF vs host builds via `cmake/esp-idf.cmake` and `cmake/host.cmake`. Distribution supports ESP-IDF Component Manager, PlatformIO, and CMake subdirectory integration.
