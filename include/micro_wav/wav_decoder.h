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

/// @file wav_decoder.h
/// @brief Streaming WAV decoder with byte-by-byte support

#pragma once

#include <cstddef>
#include <cstdint>

namespace micro_wav {

enum WAVDecoderResult : int8_t {
    WAV_DECODER_ERROR_NO_RIFF = -4,
    WAV_DECODER_ERROR_NO_WAVE = -3,
    WAV_DECODER_ERROR_FAILED = -2,
    WAV_DECODER_ERROR_UNSUPPORTED = -1,
    WAV_DECODER_SUCCESS = 0,
    WAV_DECODER_HEADER_READY = 1,
    WAV_DECODER_END_OF_STREAM = 2,
    WAV_DECODER_NEED_MORE_DATA = 3,
};

enum WAVAudioFormat : uint16_t {  // NOLINT(performance-enum-size): matches WAV spec's 16-bit
                                  // wFormatTag
    WAV_FORMAT_UNKNOWN = 0x0000,
    WAV_FORMAT_PCM = 0x0001,
    WAV_FORMAT_IEEE_FLOAT = 0x0003,
    WAV_FORMAT_ALAW = 0x0006,
    WAV_FORMAT_MULAW = 0x0007,
};

class WAVDecoder {
public:
    /// Unified decode: parses header then decodes audio samples.
    /// During header phase, pass output=nullptr. After HEADER_READY, provide an output buffer.
    /// Returns SUCCESS when samples are decoded, END_OF_STREAM when all data is consumed.
    WAVDecoderResult decode(const uint8_t* input, size_t input_len, uint8_t* output,
                            size_t output_size_bytes, size_t& bytes_consumed,
                            size_t& samples_decoded);

    /// Reset the decoder to its initial state.
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
    WAVAudioFormat audio_format() const;
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
        FMT_EXT_SUB_FORMAT,
        IN_DATA,
        ERROR,
    };

    enum class PendingChunk : uint8_t {
        FMT,
        DATA,
        UNKNOWN,
    };

    /// Internal header parser. Feeds bytes through the state machine.
    WAVDecoderResult parse(const uint8_t* input, size_t input_len, size_t& bytes_consumed);

    // Process the accumulated field buffer.
    WAVDecoderResult process_field();

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

    // Decode state
    uint32_t data_bytes_remaining_{0};
    uint8_t bytes_per_input_sample_{0};
    uint8_t bytes_per_output_sample_{0};
};

}  // namespace micro_wav
