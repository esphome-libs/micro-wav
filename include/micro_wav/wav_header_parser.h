// Copyright 2026 Kevin Ahrendt
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/// @file wav_header_parser.h
/// @brief Streaming WAV header parser with byte-by-byte support

#pragma once

#include <cstddef>
#include <cstdint>

namespace micro_wav {

enum WAVParseResult : int8_t {
    WAV_PARSER_ERROR_NO_RIFF = -3,
    WAV_PARSER_ERROR_NO_WAVE = -2,
    WAV_PARSER_ERROR_FAILED = -1,
    WAV_PARSER_HEADER_READY = 0,
    WAV_PARSER_NEED_MORE_DATA = 1,
};

class WAVHeaderParser {
public:
    /// Feed bytes to the parser. Returns bytes consumed via output param.
    /// After HEADER_READY, any remaining bytes in the input are audio data.
    WAVParseResult parse(const uint8_t* input, size_t input_len, size_t& bytes_consumed);

    /// Reset the parser to its initial state.
    void reset();

    // Accessors (valid after HEADER_READY)
    uint32_t sample_rate() const {
        return sample_rate_;
    }
    uint16_t num_channels() const {
        return num_channels_;
    }
    uint16_t bits_per_sample() const {
        return bits_per_sample_;
    }
    uint16_t audio_format() const {
        return audio_format_;
    }
    uint32_t data_chunk_size() const {
        return data_chunk_size_;
    }

private:
    enum class State : uint8_t {
        RIFF_TAG,
        RIFF_SIZE,
        WAVE_TAG,
        CHUNK_TAG,
        CHUNK_SIZE,
        FMT_AUDIO_FORMAT,
        FMT_NUM_CHANNELS,
        FMT_SAMPLE_RATE,
        FMT_BYTE_RATE,
        FMT_BLOCK_ALIGN,
        FMT_BITS_PER_SAMPLE,
        IN_DATA,
        ERROR,
    };

    enum class PendingChunk : uint8_t {
        FMT,
        DATA,
        UNKNOWN,
    };

    // Process the accumulated field buffer. Returns NEED_MORE_DATA to continue
    // parsing, HEADER_READY when done, or an error.
    WAVParseResult process_field();

    // Accumulator
    uint8_t buf_[4]{};
    uint8_t buf_len_{0};
    uint8_t buf_needed_{4};
    uint32_t skip_bytes_{0};

    // State machine
    State state_{State::RIFF_TAG};
    PendingChunk pending_chunk_type_{PendingChunk::UNKNOWN};
    uint32_t current_chunk_size_{0};

    // Parsed fields
    uint32_t sample_rate_{0};
    uint16_t num_channels_{0};
    uint16_t bits_per_sample_{0};
    uint16_t audio_format_{0};
    uint32_t data_chunk_size_{0};
};

}  // namespace micro_wav
