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
/// @brief Streaming WAV decoder for resource-constrained embedded devices
///
/// Spec: http://soundfile.sapp.org/doc/WaveFormat/

#pragma once

#include <cstddef>
#include <cstdint>

namespace micro_wav {

// ============================================================================
// Public Types
// ============================================================================

/// @brief Result codes returned by WAVDecoder::decode()
/// Positive values indicate success/informational states, negative values indicate errors.
enum WAVDecoderResult : int8_t {
    // Success / informational (>= 0)
    WAV_DECODER_SUCCESS = 0,         // Audio samples decoded into output buffer
    WAV_DECODER_HEADER_READY = 1,    // Header parsed, accessors valid, ready for audio data
    WAV_DECODER_END_OF_STREAM = 2,   // All audio data in the data chunk consumed
    WAV_DECODER_NEED_MORE_DATA = 3,  // Not enough input bytes; supply more data

    // Errors (< 0)
    WAV_DECODER_ERROR_NO_RIFF = -6,             // Input does not start with a valid RIFF tag
    WAV_DECODER_ERROR_NO_WAVE = -5,             // RIFF container missing WAVE form type
    WAV_DECODER_ERROR_FAILED = -4,              // Structural error (e.g., fmt chunk too small)
    WAV_DECODER_ERROR_UNSUPPORTED = -3,         // Audio format or bit depth not supported
    WAV_DECODER_WARNING_INVALID_INPUT = -2,     // Input pointer null with non-zero length
    WAV_DECODER_WARNING_OUTPUT_TOO_SMALL = -1,  // Output buffer null or too small for one sample
};

/// @brief Audio format tags recognized by the decoder
/// Values match the WAV spec's 16-bit wFormatTag field. WAVE_FORMAT_EXTENSIBLE (0xFFFE) is
/// resolved to the underlying format via the SubFormat GUID; unrecognized tags map to
/// WAV_FORMAT_UNKNOWN
enum WAVAudioFormat : uint16_t {     // NOLINT(performance-enum-size): matches WAV spec's 16-bit
                                     // wFormatTag
    WAV_FORMAT_UNKNOWN = 0x0000,     // Unrecognized or unsupported format tag
    WAV_FORMAT_PCM = 0x0001,         // Linear PCM (8/16/24/32-bit)
    WAV_FORMAT_IEEE_FLOAT = 0x0003,  // IEEE 754 floating-point (32-bit)
    WAV_FORMAT_ALAW = 0x0006,        // ITU-T G.711 A-law (8-bit input, decoded to 16-bit PCM)
    WAV_FORMAT_MULAW = 0x0007,       // ITU-T G.711 mu-law (8-bit input, decoded to 16-bit PCM)
};

// ============================================================================
// WAVDecoder
// ============================================================================

/**
 * @brief Streaming WAV decoder for resource-constrained embedded devices
 *
 * Single-pass byte-by-byte WAV decoder with zero dynamic allocation. Supports
 * PCM (8/16/24/32-bit), G.711 A-law/mu-law, IEEE float 32-bit, and
 * WAVE_FORMAT_EXTENSIBLE headers.
 *
 * Usage:
 * 1. Create a WAVDecoder instance
 * 2. Call decode() in a loop, feeding input data and receiving PCM output
 * 3. Use reset() to decode a new stream
 *
 * @code
 * WAVDecoder decoder;
 * uint8_t output[512];
 * size_t consumed = 0, samples = 0;
 *
 * while (has_data) {
 *     auto result = decoder.decode(input, input_len, output, sizeof(output), consumed, samples);
 *     input += consumed; input_len -= consumed;
 *
 *     if (result == WAV_DECODER_HEADER_READY) {
 *         // Accessors now valid: sample_rate, channels, bits_per_sample, etc.
 *     } else if (result == WAV_DECODER_SUCCESS) {
 *         // Process 'samples' decoded samples in output buffer
 *     } else if (result == WAV_DECODER_NEED_MORE_DATA) {
 *         // Feed more input data
 *     } else if (result == WAV_DECODER_END_OF_STREAM) {
 *         break;
 *     }
 * }
 * @endcode
 */
class WAVDecoder {
public:
    // ========================================
    // Lifecycle
    // ========================================

    /// @brief Reset the decoder to its initial state, ready to decode a new stream
    void reset();

    // ========================================
    // Core Decoding API
    // ========================================

    /// @brief Decode WAV data from a streaming input
    ///
    /// Handles both header parsing and audio decoding in a single streaming API.
    /// The output buffer is ignored during header parsing, so it is safe to pass
    /// the same buffer throughout.
    ///
    /// @param input Pointer to input data buffer
    /// @param input_len Number of bytes available in input buffer
    /// @param output Pointer to output buffer for decoded PCM samples
    /// @param output_size_bytes Size of the output buffer in bytes
    /// @param bytes_consumed [out] Number of input bytes consumed by this call
    /// @param samples_decoded [out] Number of samples decoded (per channel)
    /// @return WAV_DECODER_HEADER_READY when header is complete (accessors now valid)
    ///         WAV_DECODER_SUCCESS when audio samples were decoded
    ///         WAV_DECODER_NEED_MORE_DATA when more input is needed
    ///         WAV_DECODER_END_OF_STREAM when all audio data is consumed
    ///         Negative error code on failure
    WAVDecoderResult decode(const uint8_t* input, size_t input_len, uint8_t* output,
                            size_t output_size_bytes, size_t& bytes_consumed,
                            size_t& samples_decoded);

    // ========================================
    // Stream Information
    // ========================================

    /// @brief Get the audio format tag from the WAV header
    /// @return Audio format tag (WAV_FORMAT_UNKNOWN if unrecognized)
    WAVAudioFormat get_audio_format() const;

    /// @brief Get the output bit depth per sample
    ///
    /// @note For A-law/mu-law sources this returns 16 (the decoded output width),
    ///   not the original 8-bit WAV header value. For IEEE float sources, the output
    ///   is 32-bit signed integer PCM, so this returns 32.
    ///
    /// @return Bits per output sample
    uint16_t get_bits_per_sample() const {
        return this->bits_per_sample_;
    }

    /// @brief Get the number of bytes per decoded output sample
    /// @return Bytes per output sample
    uint8_t get_bytes_per_output_sample() const {
        return this->bytes_per_output_sample_;
    }

    /// @brief Get the number of audio data bytes remaining to be decoded
    /// @return Remaining bytes in the data chunk
    uint32_t get_bytes_remaining() const {
        return this->data_bytes_remaining_;
    }

    /// @brief Get the number of audio channels
    /// @return Number of channels
    uint16_t get_channels() const {
        return this->num_channels_;
    }

    /// @brief Get the size of the data chunk in bytes
    ///
    /// @note A header-reported size of 0 (the streaming sentinel used by some
    ///   live HTTP WAV sources) is normalized to UINT32_MAX so the decoder treats
    ///   the data section as unbounded; callers comparing against UINT32_MAX can
    ///   detect the unknown-length case.
    ///
    /// @return Data chunk size
    uint32_t get_data_chunk_size() const {
        return this->data_chunk_size_;
    }

    /// @brief Check if header parsing is complete and accessors are valid
    /// @return true if header has been parsed
    bool is_header_ready() const {
        return this->bytes_per_output_sample_ > 0;
    }

    /// @brief Get the sample rate in Hz
    /// @return Sample rate
    uint32_t get_sample_rate() const {
        return this->sample_rate_;
    }

private:
    // ========================================
    // Private Types
    // ========================================

    /// @brief Header parsing state machine
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

    /// @brief Pending chunk type during header parsing
    enum class PendingChunk : uint8_t {
        FMT,
        DATA,
        UNKNOWN,
    };

    // ========================================
    // Header Parsing
    // ========================================

    /// @brief Internal header parser; feeds bytes through the state machine
    WAVDecoderResult parse(const uint8_t* input, size_t input_len, size_t& bytes_consumed);

    /// @brief Process the accumulated field buffer
    WAVDecoderResult process_field();

    // ========================================
    // Member Variables
    // ========================================

    // 32-bit fields
    uint32_t current_chunk_size_{0};
    uint32_t data_bytes_remaining_{0};
    uint32_t data_chunk_size_{0};
    uint32_t sample_rate_{0};
    uint32_t skip_bytes_{0};

    // 16-bit fields
    uint16_t audio_format_{0};
    uint16_t bits_per_sample_{0};
    uint16_t num_channels_{0};

    // 8-bit fields
    uint8_t buf_[4]{};
    uint8_t buf_len_{0};
    uint8_t buf_needed_{4};
    uint8_t bytes_per_input_sample_{0};
    uint8_t bytes_per_output_sample_{0};
    PendingChunk pending_chunk_type_{PendingChunk::UNKNOWN};
    State state_{State::RIFF_TAG};
};

}  // namespace micro_wav
