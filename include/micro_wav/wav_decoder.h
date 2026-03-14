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

/// Result codes returned by WAVDecoder::decode().
/// Negative values indicate errors or warnings; non-negative values indicate progress.
enum WAVDecoderResult : int8_t {
    /// Input does not start with a valid RIFF tag.
    WAV_DECODER_ERROR_NO_RIFF = -6,
    /// RIFF container does not contain a WAVE form type.
    WAV_DECODER_ERROR_NO_WAVE = -5,
    /// Structural error in the WAV file (e.g., fmt chunk too small).
    WAV_DECODER_ERROR_FAILED = -4,
    /// Audio format or bit depth is not supported by this decoder.
    WAV_DECODER_ERROR_UNSUPPORTED = -3,
    /// Input pointer is null while input length is non-zero.
    WAV_DECODER_WARNING_INVALID_INPUT = -2,
    /// Output buffer is null or too small to hold one sample.
    WAV_DECODER_WARNING_OUTPUT_TOO_SMALL = -1,
    /// One or more audio samples were decoded into the output buffer.
    WAV_DECODER_SUCCESS = 0,
    /// Header parsing is complete; accessors are now valid and the decoder is ready for audio data.
    WAV_DECODER_HEADER_READY = 1,
    /// All audio data in the data chunk has been consumed.
    WAV_DECODER_END_OF_STREAM = 2,
    /// Not enough input bytes to make progress; supply more data.
    WAV_DECODER_NEED_MORE_DATA = 3,
};

/// Audio format tags recognized by the decoder.
/// Values match the WAV spec's 16-bit wFormatTag field. WAVE_FORMAT_EXTENSIBLE (0xFFFE) is
/// resolved to the underlying format via the SubFormat GUID; unrecognized tags map to
/// WAV_FORMAT_UNKNOWN.
enum WAVAudioFormat : uint16_t {  // NOLINT(performance-enum-size): matches WAV spec's 16-bit
                                  // wFormatTag
    /// Unrecognized or unsupported format tag.
    WAV_FORMAT_UNKNOWN = 0x0000,
    /// Linear pulse-code modulation (8/16/24/32-bit).
    WAV_FORMAT_PCM = 0x0001,
    /// IEEE 754 floating-point (32-bit).
    WAV_FORMAT_IEEE_FLOAT = 0x0003,
    /// ITU-T G.711 A-law (8-bit input, decoded to 16-bit PCM).
    WAV_FORMAT_ALAW = 0x0006,
    /// ITU-T G.711 mu-law (8-bit input, decoded to 16-bit PCM).
    WAV_FORMAT_MULAW = 0x0007,
};

class WAVDecoder {
public:
    /// Decodes WAV data: parses header fields, then decodes audio samples.
    /// The output buffer is ignored during header parsing, so it is safe to pass the same
    /// buffer throughout. Returns SUCCESS when samples are decoded, END_OF_STREAM when all
    /// data is consumed.
    WAVDecoderResult decode(const uint8_t* input, size_t input_len, uint8_t* output,
                            size_t output_size_bytes, size_t& bytes_consumed,
                            size_t& samples_decoded);

    /// Reset the decoder to its initial state.
    void reset();

    /// @name Accessors (valid after HEADER_READY)
    /// @{

    /// @brief Returns the sample rate in Hz.
    uint32_t get_sample_rate() const {
        return sample_rate_;
    }
    /// @brief Returns the number of audio channels.
    uint16_t get_channels() const {
        return num_channels_;
    }
    /// @brief Returns the output bit depth per sample.
    /// @note For A-law/mu-law sources this returns 16 (the decoded output width),
    ///   not the original 8-bit WAV header value. For IEEE float sources, the output
    ///   is 32-bit signed integer PCM, so this returns 32.
    uint16_t get_bits_per_sample() const {
        return bits_per_sample_;
    }
    /// @brief Returns the audio format tag from the WAV header.
    WAVAudioFormat get_audio_format() const;
    /// @brief Returns the size of the data chunk in bytes.
    uint32_t get_data_chunk_size() const {
        return data_chunk_size_;
    }
    /// @brief Returns the number of bytes per decoded output sample.
    uint8_t get_bytes_per_output_sample() const {
        return bytes_per_output_sample_;
    }
    /// @brief Returns the number of audio data bytes remaining to be decoded.
    uint32_t get_bytes_remaining() const {
        return data_bytes_remaining_;
    }

    /// @}

    /// @brief Returns true if header parsing is complete and accessors are valid.
    bool is_header_ready() const {
        return bytes_per_output_sample_ > 0;
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
