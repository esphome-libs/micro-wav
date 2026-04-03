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

#include "micro_wav/wav_decoder.h"

#include <algorithm>
#include <cstring>

namespace micro_wav {

// ============================================================================
// Constants
// ============================================================================

static constexpr uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;

// Standard fmt fields (audioFormat through bitsPerSample) = 16 bytes.
// Extensible header adds cbSize(2) + validBitsPerSample(2) + channelMask(4) +
// subFormat(2 used + 14 skipped) = 24 bytes, for a total of 40 bytes.
static constexpr uint32_t FMT_STANDARD_SIZE = 16;
static constexpr uint32_t FMT_EXTENSIBLE_MIN_SIZE = 40;
// Bytes consumed before SubFormat GUID's remaining 14 bytes:
// 16 (standard) + 8 (cbSize + validBits + channelMask) + 2 (subFormat tag) = 26
static constexpr uint32_t FMT_EXT_CONSUMED_BEFORE_GUID_TAIL = 26;

// ============================================================================
// Static Utilities
// ============================================================================

static uint16_t read_u16(const uint8_t* buf) {
    return static_cast<uint16_t>(static_cast<unsigned>(buf[0]) |
                                 (static_cast<unsigned>(buf[1]) << 8));
}

static uint32_t read_u32(const uint8_t* buf) {
    return static_cast<uint32_t>(buf[0]) | (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) | (static_cast<uint32_t>(buf[3]) << 24);
}

static bool tag_equals(const uint8_t* buf, const char* tag) {
    return memcmp(buf, tag, 4) == 0;
}

// Round up to even for RIFF chunk padding
static uint32_t pad_to_even(uint32_t size) {
    if (size == UINT32_MAX) {
        return size;
    }
    return size + (size & 1);
}

// ============================================================================
// Sample Conversion
// ============================================================================

// G.711 A-law constants
static constexpr uint8_t ALAW_XOR_MASK = 0x55;
static constexpr uint8_t G711_SIGN_BIT = 0x80;
static constexpr uint8_t G711_EXPONENT_MASK = 0x07;
static constexpr uint8_t G711_MANTISSA_MASK = 0x0F;
static constexpr int ALAW_SEG0_BIAS = 8;
static constexpr int ALAW_SEGN_BIAS = 0x108;

// G.711 mu-law constants
static constexpr int MULAW_BIAS = 0x84;

// Byte extraction
static constexpr uint32_t BYTE_MASK = 0xFFU;

static int16_t decode_alaw_sample(uint8_t a_val) {
    a_val ^= ALAW_XOR_MASK;
    uint8_t exponent = static_cast<uint8_t>((a_val >> 4) & G711_EXPONENT_MASK);
    uint8_t mantissa = static_cast<uint8_t>(a_val & G711_MANTISSA_MASK);
    int16_t magnitude = 0;
    if (exponent == 0) {
        magnitude = static_cast<int16_t>((mantissa << 4) + ALAW_SEG0_BIAS);
    } else {
        magnitude = static_cast<int16_t>(((mantissa << 4) + ALAW_SEGN_BIAS) << (exponent - 1));
    }
    return (a_val & G711_SIGN_BIT) ? magnitude : static_cast<int16_t>(-magnitude);
}

static int16_t decode_mulaw_sample(uint8_t mu_val) {
    mu_val = static_cast<uint8_t>(~mu_val);
    uint8_t exponent = static_cast<uint8_t>((mu_val >> 4) & G711_EXPONENT_MASK);
    uint8_t mantissa = static_cast<uint8_t>(mu_val & G711_MANTISSA_MASK);
    int magnitude = ((mantissa << 3) + MULAW_BIAS) << exponent;
    magnitude -= MULAW_BIAS;
    int16_t result = static_cast<int16_t>(magnitude);
    return (mu_val & G711_SIGN_BIT) ? result : static_cast<int16_t>(-result);
}

// NOTE: Assumes the host platform uses little-endian IEEE 754 floats.
static int32_t decode_float_to_int32(const uint8_t* bytes) {
    float f = 0.0F;
    memcpy(&f, bytes, sizeof(f));
    // NaN check: NaN != NaN is true; treat NaN as silence
    if (f != f) {  // NOLINT(misc-redundant-expression)
        return 0;
    }
    if (f > 1.0F) {
        f = 1.0F;
    }
    if (f < -1.0F) {
        f = -1.0F;
    }
    return static_cast<int32_t>(static_cast<double>(f) * 2147483647.0);
}

static void convert_sample(WAVAudioFormat fmt, uint8_t bytes_per_input, const uint8_t* src,
                           uint8_t* dst) {
    switch (fmt) {
        case WAV_FORMAT_PCM:
            if (bytes_per_input == 1) {
                dst[0] = src[0] ^ G711_SIGN_BIT;
            } else {
                memcpy(dst, src, bytes_per_input);
            }
            break;
        case WAV_FORMAT_ALAW: {
            int16_t sample = decode_alaw_sample(src[0]);
            uint16_t u = static_cast<uint16_t>(sample);
            dst[0] = static_cast<uint8_t>(u & BYTE_MASK);
            dst[1] = static_cast<uint8_t>((u >> 8) & BYTE_MASK);
            break;
        }
        case WAV_FORMAT_MULAW: {
            int16_t sample = decode_mulaw_sample(src[0]);
            uint16_t u = static_cast<uint16_t>(sample);
            dst[0] = static_cast<uint8_t>(u & BYTE_MASK);
            dst[1] = static_cast<uint8_t>((u >> 8) & BYTE_MASK);
            break;
        }
        case WAV_FORMAT_IEEE_FLOAT: {
            int32_t sample = decode_float_to_int32(src);
            uint32_t u = static_cast<uint32_t>(sample);
            dst[0] = static_cast<uint8_t>(u & BYTE_MASK);
            dst[1] = static_cast<uint8_t>((u >> 8) & BYTE_MASK);
            dst[2] = static_cast<uint8_t>((u >> 16) & BYTE_MASK);
            dst[3] = static_cast<uint8_t>((u >> 24) & BYTE_MASK);
            break;
        }
        default:
            break;
    }
}

// ============================================================================
// Lifecycle
// ============================================================================

void WAVDecoder::reset() {
    this->current_chunk_size_ = 0;
    this->data_bytes_remaining_ = 0;
    this->data_chunk_size_ = 0;
    this->sample_rate_ = 0;
    this->skip_bytes_ = 0;
    this->audio_format_ = 0;
    this->bits_per_sample_ = 0;
    this->num_channels_ = 0;
    this->pending_chunk_type_ = PendingChunk::UNKNOWN;
    this->state_ = State::RIFF_TAG;
    this->buf_len_ = 0;
    this->buf_needed_ = 4;
    this->bytes_per_input_sample_ = 0;
    this->bytes_per_output_sample_ = 0;
}

// ============================================================================
// Core Decoding API
// ============================================================================

WAVDecoderResult WAVDecoder::decode(const uint8_t* input, size_t input_len, uint8_t* output,
                                    size_t output_size_bytes, size_t& bytes_consumed,
                                    size_t& samples_decoded) {
    samples_decoded = 0;

    if (input == nullptr && input_len > 0) {
        return WAV_DECODER_WARNING_INVALID_INPUT;
    }

    // Header parsing phase
    if (this->bytes_per_output_sample_ == 0) {
        WAVDecoderResult result = this->parse(input, input_len, bytes_consumed);
        if (result != WAV_DECODER_HEADER_READY) {
            return result;
        }

        // Reject invalid headers
        if (this->num_channels_ == 0 || this->sample_rate_ == 0) {
            this->state_ = State::ERROR;
            return WAV_DECODER_ERROR_FAILED;
        }

        // Set up decode state
        this->data_bytes_remaining_ = this->data_chunk_size_;

        switch (this->get_audio_format()) {
            case WAV_FORMAT_PCM:
                if (this->bits_per_sample_ == 0 || this->bits_per_sample_ % 8 != 0 ||
                    this->bits_per_sample_ > 32) {
                    this->state_ = State::ERROR;
                    return WAV_DECODER_ERROR_UNSUPPORTED;
                }
                this->bytes_per_input_sample_ = static_cast<uint8_t>(this->bits_per_sample_ / 8);
                if (this->bits_per_sample_ == 8) {
                    this->bytes_per_output_sample_ = 1;
                } else {
                    this->bytes_per_output_sample_ = this->bytes_per_input_sample_;
                }
                break;
            case WAV_FORMAT_ALAW:
            case WAV_FORMAT_MULAW:
                if (this->bits_per_sample_ != 8) {
                    this->state_ = State::ERROR;
                    return WAV_DECODER_ERROR_UNSUPPORTED;
                }
                this->bytes_per_input_sample_ = 1;
                this->bytes_per_output_sample_ = 2;
                this->bits_per_sample_ = 16;
                break;
            case WAV_FORMAT_IEEE_FLOAT:
                if (this->bits_per_sample_ != 32) {
                    this->state_ = State::ERROR;
                    return WAV_DECODER_ERROR_UNSUPPORTED;
                }
                this->bytes_per_input_sample_ = 4;
                this->bytes_per_output_sample_ = 4;
                break;
            default:
                this->state_ = State::ERROR;
                return WAV_DECODER_ERROR_UNSUPPORTED;
        }

        // Reset buf_ for audio sample accumulation
        this->buf_len_ = 0;
        return WAV_DECODER_HEADER_READY;
    }

    // Audio decoding phase
    bytes_consumed = 0;

    // End of stream: no data remaining (discard any partial sample in buf_)
    if (this->data_bytes_remaining_ == 0 && this->buf_len_ == 0) {
        return WAV_DECODER_END_OF_STREAM;
    }

    if (output == nullptr || output_size_bytes < this->bytes_per_output_sample_) {
        return WAV_DECODER_WARNING_OUTPUT_TOO_SMALL;
    }

    WAVAudioFormat fmt = this->get_audio_format();

    // Step 1: Complete any partial sample buffered from a previous call
    if (this->buf_len_ > 0) {
        while (this->buf_len_ < this->bytes_per_input_sample_ && bytes_consumed < input_len &&
               this->data_bytes_remaining_ > 0) {
            this->buf_[this->buf_len_++] = input[bytes_consumed++];
            --this->data_bytes_remaining_;
        }
        if (this->buf_len_ < this->bytes_per_input_sample_) {
            if (this->data_bytes_remaining_ == 0) {
                this->buf_len_ = 0;
                return WAV_DECODER_END_OF_STREAM;
            }
            return WAV_DECODER_NEED_MORE_DATA;
        }
        convert_sample(fmt, this->bytes_per_input_sample_, this->buf_, output);
        this->buf_len_ = 0;
        ++samples_decoded;
    }

    // Step 2: Bulk-process complete samples directly from input
    {
        // Compute max samples we can process given input, output, and data constraints
        size_t input_avail = (input_len - bytes_consumed) / this->bytes_per_input_sample_;
        size_t output_avail =
            (output_size_bytes - samples_decoded * this->bytes_per_output_sample_) /
            this->bytes_per_output_sample_;
        size_t data_avail = this->data_bytes_remaining_ / this->bytes_per_input_sample_;
        size_t count = std::min({input_avail, output_avail, data_avail});

        if (count > 0 && fmt == WAV_FORMAT_PCM && this->bytes_per_input_sample_ > 1) {
            // PCM >= 16-bit: single memcpy for the entire run
            size_t total_bytes = count * this->bytes_per_input_sample_;
            memcpy(output + samples_decoded * this->bytes_per_output_sample_,
                   input + bytes_consumed, total_bytes);
            bytes_consumed += total_bytes;
            this->data_bytes_remaining_ -= static_cast<uint32_t>(total_bytes);
            samples_decoded += count;
        } else if (count > 0 && fmt == WAV_FORMAT_PCM) {
            // PCM 8-bit unsigned->signed: batch XOR loop without per-sample function call
            const uint8_t* src = input + bytes_consumed;
            uint8_t* dst = output + samples_decoded * this->bytes_per_output_sample_;
            for (size_t i = 0; i < count; ++i) {
                dst[i] = src[i] ^ G711_SIGN_BIT;
            }
            bytes_consumed += count;
            this->data_bytes_remaining_ -= static_cast<uint32_t>(count);
            samples_decoded += count;
        } else {
            // A-law, mu-law, IEEE float: per-sample conversion
            for (size_t i = 0; i < count; ++i) {
                convert_sample(fmt, this->bytes_per_input_sample_, input + bytes_consumed,
                               output + samples_decoded * this->bytes_per_output_sample_);
                bytes_consumed += this->bytes_per_input_sample_;
                this->data_bytes_remaining_ -= this->bytes_per_input_sample_;
                ++samples_decoded;
            }
        }
    }

    // Step 3: Buffer any trailing partial sample for the next call
    if (samples_decoded * this->bytes_per_output_sample_ + this->bytes_per_output_sample_ <=
            output_size_bytes &&
        this->data_bytes_remaining_ > 0 && bytes_consumed < input_len) {
        while (bytes_consumed < input_len && this->buf_len_ < this->bytes_per_input_sample_ &&
               this->data_bytes_remaining_ > 0) {
            this->buf_[this->buf_len_++] = input[bytes_consumed++];
            --this->data_bytes_remaining_;
        }
    }

    if (samples_decoded > 0) {
        return WAV_DECODER_SUCCESS;
    }

    if (this->data_bytes_remaining_ == 0 && this->buf_len_ == 0) {
        return WAV_DECODER_END_OF_STREAM;
    }

    return WAV_DECODER_NEED_MORE_DATA;
}

// ============================================================================
// Stream Information
// ============================================================================

WAVAudioFormat WAVDecoder::get_audio_format() const {
    switch (this->audio_format_) {
        case WAV_FORMAT_PCM:
        case WAV_FORMAT_IEEE_FLOAT:
        case WAV_FORMAT_ALAW:
        case WAV_FORMAT_MULAW:
            return static_cast<WAVAudioFormat>(this->audio_format_);
        default:
            return WAV_FORMAT_UNKNOWN;
    }
}

// ============================================================================
// Header Parsing
// ============================================================================

WAVDecoderResult WAVDecoder::parse(const uint8_t* input, size_t input_len, size_t& bytes_consumed) {
    bytes_consumed = 0;

    if (this->state_ == State::IN_DATA) {
        return WAV_DECODER_HEADER_READY;
    }
    if (this->state_ == State::ERROR) {
        return WAV_DECODER_ERROR_FAILED;
    }

    while (bytes_consumed < input_len) {
        // Skip phase
        if (this->skip_bytes_ > 0) {
            size_t can_skip =
                std::min(static_cast<size_t>(this->skip_bytes_), input_len - bytes_consumed);
            this->skip_bytes_ -= static_cast<uint32_t>(can_skip);
            bytes_consumed += can_skip;
            if (this->skip_bytes_ > 0) {
                return WAV_DECODER_NEED_MORE_DATA;
            }
            continue;
        }

        // Accumulate phase
        while (this->buf_len_ < this->buf_needed_ && bytes_consumed < input_len) {
            this->buf_[this->buf_len_++] = input[bytes_consumed++];
        }
        if (this->buf_len_ < this->buf_needed_) {
            return WAV_DECODER_NEED_MORE_DATA;
        }

        // Process phase
        WAVDecoderResult result = this->process_field();
        this->buf_len_ = 0;
        if (result != WAV_DECODER_NEED_MORE_DATA) {
            return result;
        }
    }

    return WAV_DECODER_NEED_MORE_DATA;
}

WAVDecoderResult WAVDecoder::process_field() {
    switch (this->state_) {
        case State::RIFF_TAG:
            if (!tag_equals(this->buf_, "RIFF")) {
                this->state_ = State::ERROR;
                return WAV_DECODER_ERROR_NO_RIFF;
            }
            this->state_ = State::RIFF_SIZE;
            this->buf_needed_ = 4;
            break;

        case State::RIFF_SIZE:
            // Read but don't need the RIFF chunk size
            this->state_ = State::WAVE_TAG;
            this->buf_needed_ = 4;
            break;

        case State::WAVE_TAG:
            if (!tag_equals(this->buf_, "WAVE")) {
                this->state_ = State::ERROR;
                return WAV_DECODER_ERROR_NO_WAVE;
            }
            this->state_ = State::CHUNK_TAG;
            this->buf_needed_ = 4;
            break;

        case State::CHUNK_TAG:
            if (tag_equals(this->buf_, "fmt ")) {
                this->pending_chunk_type_ = PendingChunk::FMT;
            } else if (tag_equals(this->buf_, "data")) {
                this->pending_chunk_type_ = PendingChunk::DATA;
            } else {
                this->pending_chunk_type_ = PendingChunk::UNKNOWN;
            }
            this->state_ = State::CHUNK_SIZE;
            this->buf_needed_ = 4;
            break;

        case State::CHUNK_SIZE:
            this->current_chunk_size_ = read_u32(this->buf_);
            switch (this->pending_chunk_type_) {
                case PendingChunk::FMT:
                    if (this->current_chunk_size_ < FMT_STANDARD_SIZE) {
                        this->state_ = State::ERROR;
                        return WAV_DECODER_ERROR_FAILED;
                    }
                    this->state_ = State::FMT_AUDIO_FORMAT;
                    this->buf_needed_ = 2;
                    break;
                case PendingChunk::DATA:
                    this->data_chunk_size_ = this->current_chunk_size_;
                    this->state_ = State::IN_DATA;
                    return WAV_DECODER_HEADER_READY;
                case PendingChunk::UNKNOWN:
                    this->skip_bytes_ = pad_to_even(this->current_chunk_size_);
                    this->state_ = State::CHUNK_TAG;
                    this->buf_needed_ = 4;
                    break;
            }
            break;

        case State::FMT_AUDIO_FORMAT:
            this->audio_format_ = read_u16(this->buf_);
            this->state_ = State::FMT_NUM_CHANNELS;
            this->buf_needed_ = 2;
            break;

        case State::FMT_NUM_CHANNELS:
            this->num_channels_ = read_u16(this->buf_);
            this->state_ = State::FMT_SAMPLE_RATE;
            this->buf_needed_ = 4;
            break;

        case State::FMT_SAMPLE_RATE:
            this->sample_rate_ = read_u32(this->buf_);
            this->state_ = State::FMT_BYTE_RATE;
            this->buf_needed_ = 4;
            break;

        case State::FMT_BYTE_RATE:
            // Skip byte rate
            this->state_ = State::FMT_BLOCK_ALIGN;
            this->buf_needed_ = 2;
            break;

        case State::FMT_BLOCK_ALIGN:
            // Skip block align
            this->state_ = State::FMT_BITS_PER_SAMPLE;
            this->buf_needed_ = 2;
            break;

        case State::FMT_BITS_PER_SAMPLE:
            this->bits_per_sample_ = read_u16(this->buf_);
            if (this->audio_format_ == WAVE_FORMAT_EXTENSIBLE &&
                this->current_chunk_size_ >= FMT_EXTENSIBLE_MIN_SIZE) {
                // WAVE_FORMAT_EXTENSIBLE: skip cbSize(2) + validBitsPerSample(2) +
                // channelMask(4) = 8 bytes, then read the SubFormat GUID's first
                // two bytes to get the actual audio format.
                this->skip_bytes_ = 8;
                this->state_ = State::FMT_EXT_SUB_FORMAT;
                this->buf_needed_ = 2;
            } else if (this->audio_format_ == WAVE_FORMAT_EXTENSIBLE) {
                // Extensible format but chunk too small for the required fields
                this->state_ = State::ERROR;
                return WAV_DECODER_ERROR_FAILED;
            } else if (this->current_chunk_size_ > FMT_STANDARD_SIZE) {
                this->skip_bytes_ = pad_to_even(this->current_chunk_size_) - FMT_STANDARD_SIZE;
                this->state_ = State::CHUNK_TAG;
                this->buf_needed_ = 4;
            } else {
                this->state_ = State::CHUNK_TAG;
                this->buf_needed_ = 4;
            }
            break;

        case State::FMT_EXT_SUB_FORMAT:
            this->audio_format_ = read_u16(this->buf_);
            // Skip remaining 14 bytes of SubFormat GUID + any extra chunk data
            if (this->current_chunk_size_ > FMT_EXT_CONSUMED_BEFORE_GUID_TAIL) {
                this->skip_bytes_ =
                    pad_to_even(this->current_chunk_size_) - FMT_EXT_CONSUMED_BEFORE_GUID_TAIL;
            }
            this->state_ = State::CHUNK_TAG;
            this->buf_needed_ = 4;
            break;

        case State::IN_DATA:
            return WAV_DECODER_HEADER_READY;

        case State::ERROR:
            return WAV_DECODER_ERROR_FAILED;
    }

    return WAV_DECODER_NEED_MORE_DATA;
}

}  // namespace micro_wav
