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

static constexpr uint16_t WAVE_FORMAT_EXTENSIBLE = 0xFFFE;

// Standard fmt fields (audioFormat through bitsPerSample) = 16 bytes.
// Extensible header adds cbSize(2) + validBitsPerSample(2) + channelMask(4) +
// subFormat(2 used + 14 skipped) = 24 bytes, for a total of 40 bytes.
static constexpr uint32_t FMT_STANDARD_SIZE = 16;
static constexpr uint32_t FMT_EXTENSIBLE_MIN_SIZE = 40;
// Bytes consumed before SubFormat GUID's remaining 14 bytes:
// 16 (standard) + 8 (cbSize + validBits + channelMask) + 2 (subFormat tag) = 26
static constexpr uint32_t FMT_EXT_CONSUMED_BEFORE_GUID_TAIL = 26;

static uint16_t read_u16(const uint8_t* buf) {
    return static_cast<uint16_t>(buf[0] | (buf[1] << 8));
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
    return (size + 1) & ~static_cast<uint32_t>(1);
}

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

static int32_t decode_float_to_int32(const uint8_t* bytes) {
    float f = 0.0F;
    memcpy(&f, bytes, sizeof(f));
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

WAVAudioFormat WAVDecoder::audio_format() const {
    switch (audio_format_) {
        case WAV_FORMAT_PCM:
        case WAV_FORMAT_IEEE_FLOAT:
        case WAV_FORMAT_ALAW:
        case WAV_FORMAT_MULAW:
            return static_cast<WAVAudioFormat>(audio_format_);
        default:
            return WAV_FORMAT_UNKNOWN;
    }
}

void WAVDecoder::reset() {
    buf_len_ = 0;
    buf_needed_ = 4;
    skip_bytes_ = 0;
    state_ = State::RIFF_TAG;
    pending_chunk_type_ = PendingChunk::UNKNOWN;
    current_chunk_size_ = 0;
    sample_rate_ = 0;
    num_channels_ = 0;
    bits_per_sample_ = 0;
    audio_format_ = 0;
    data_chunk_size_ = 0;
    data_bytes_remaining_ = 0;
    bytes_per_input_sample_ = 0;
    bytes_per_output_sample_ = 0;
}

WAVDecoderResult WAVDecoder::parse(const uint8_t* input, size_t input_len, size_t& bytes_consumed) {
    bytes_consumed = 0;

    if (state_ == State::IN_DATA) {
        return WAV_DECODER_HEADER_READY;
    }
    if (state_ == State::ERROR) {
        return WAV_DECODER_ERROR_FAILED;
    }

    while (bytes_consumed < input_len) {
        // Skip phase
        if (skip_bytes_ > 0) {
            size_t can_skip =
                std::min(static_cast<size_t>(skip_bytes_), input_len - bytes_consumed);
            skip_bytes_ -= static_cast<uint32_t>(can_skip);
            bytes_consumed += can_skip;
            if (skip_bytes_ > 0) {
                return WAV_DECODER_NEED_MORE_DATA;
            }
            continue;
        }

        // Accumulate phase
        while (buf_len_ < buf_needed_ && bytes_consumed < input_len) {
            buf_[buf_len_++] = input[bytes_consumed++];
        }
        if (buf_len_ < buf_needed_) {
            return WAV_DECODER_NEED_MORE_DATA;
        }

        // Process phase
        WAVDecoderResult result = process_field();
        buf_len_ = 0;
        if (result != WAV_DECODER_NEED_MORE_DATA) {
            return result;
        }
    }

    return WAV_DECODER_NEED_MORE_DATA;
}

WAVDecoderResult WAVDecoder::process_field() {
    switch (state_) {
        case State::RIFF_TAG:
            if (!tag_equals(buf_, "RIFF")) {
                state_ = State::ERROR;
                return WAV_DECODER_ERROR_NO_RIFF;
            }
            state_ = State::RIFF_SIZE;
            buf_needed_ = 4;
            break;

        case State::RIFF_SIZE:
            // Read but don't need the RIFF chunk size
            state_ = State::WAVE_TAG;
            buf_needed_ = 4;
            break;

        case State::WAVE_TAG:
            if (!tag_equals(buf_, "WAVE")) {
                state_ = State::ERROR;
                return WAV_DECODER_ERROR_NO_WAVE;
            }
            state_ = State::CHUNK_TAG;
            buf_needed_ = 4;
            break;

        case State::CHUNK_TAG:
            if (tag_equals(buf_, "fmt ")) {
                pending_chunk_type_ = PendingChunk::FMT;
            } else if (tag_equals(buf_, "data")) {
                pending_chunk_type_ = PendingChunk::DATA;
            } else {
                pending_chunk_type_ = PendingChunk::UNKNOWN;
            }
            state_ = State::CHUNK_SIZE;
            buf_needed_ = 4;
            break;

        case State::CHUNK_SIZE:
            current_chunk_size_ = read_u32(buf_);
            switch (pending_chunk_type_) {
                case PendingChunk::FMT:
                    if (current_chunk_size_ < FMT_STANDARD_SIZE) {
                        state_ = State::ERROR;
                        return WAV_DECODER_ERROR_FAILED;
                    }
                    state_ = State::FMT_AUDIO_FORMAT;
                    buf_needed_ = 2;
                    break;
                case PendingChunk::DATA:
                    data_chunk_size_ = current_chunk_size_;
                    state_ = State::IN_DATA;
                    return WAV_DECODER_HEADER_READY;
                case PendingChunk::UNKNOWN:
                    skip_bytes_ = pad_to_even(current_chunk_size_);
                    state_ = State::CHUNK_TAG;
                    buf_needed_ = 4;
                    break;
            }
            break;

        case State::FMT_AUDIO_FORMAT:
            audio_format_ = read_u16(buf_);
            state_ = State::FMT_NUM_CHANNELS;
            buf_needed_ = 2;
            break;

        case State::FMT_NUM_CHANNELS:
            num_channels_ = read_u16(buf_);
            state_ = State::FMT_SAMPLE_RATE;
            buf_needed_ = 4;
            break;

        case State::FMT_SAMPLE_RATE:
            sample_rate_ = read_u32(buf_);
            state_ = State::FMT_BYTE_RATE;
            buf_needed_ = 4;
            break;

        case State::FMT_BYTE_RATE:
            // Skip byte rate
            state_ = State::FMT_BLOCK_ALIGN;
            buf_needed_ = 2;
            break;

        case State::FMT_BLOCK_ALIGN:
            // Skip block align
            state_ = State::FMT_BITS_PER_SAMPLE;
            buf_needed_ = 2;
            break;

        case State::FMT_BITS_PER_SAMPLE:
            bits_per_sample_ = read_u16(buf_);
            if (audio_format_ == WAVE_FORMAT_EXTENSIBLE &&
                current_chunk_size_ >= FMT_EXTENSIBLE_MIN_SIZE) {
                // WAVE_FORMAT_EXTENSIBLE: skip cbSize(2) + validBitsPerSample(2) +
                // channelMask(4) = 8 bytes, then read the SubFormat GUID's first
                // two bytes to get the actual audio format.
                skip_bytes_ = 8;
                state_ = State::FMT_EXT_SUB_FORMAT;
                buf_needed_ = 2;
            } else if (audio_format_ == WAVE_FORMAT_EXTENSIBLE) {
                // Extensible format but chunk too small for the required fields
                state_ = State::ERROR;
                return WAV_DECODER_ERROR_FAILED;
            } else if (current_chunk_size_ > FMT_STANDARD_SIZE) {
                skip_bytes_ = pad_to_even(current_chunk_size_) - FMT_STANDARD_SIZE;
                state_ = State::CHUNK_TAG;
                buf_needed_ = 4;
            } else {
                state_ = State::CHUNK_TAG;
                buf_needed_ = 4;
            }
            break;

        case State::FMT_EXT_SUB_FORMAT:
            audio_format_ = read_u16(buf_);
            // Skip remaining 14 bytes of SubFormat GUID + any extra chunk data
            if (current_chunk_size_ > FMT_EXT_CONSUMED_BEFORE_GUID_TAIL) {
                skip_bytes_ = pad_to_even(current_chunk_size_) - FMT_EXT_CONSUMED_BEFORE_GUID_TAIL;
            }
            state_ = State::CHUNK_TAG;
            buf_needed_ = 4;
            break;

        case State::IN_DATA:
            return WAV_DECODER_HEADER_READY;

        case State::ERROR:
            return WAV_DECODER_ERROR_FAILED;
    }

    return WAV_DECODER_NEED_MORE_DATA;
}

WAVDecoderResult WAVDecoder::decode(const uint8_t* input, size_t input_len, uint8_t* output,
                                    size_t output_size_bytes, size_t& bytes_consumed,
                                    size_t& samples_decoded) {
    samples_decoded = 0;

    // Header parsing phase
    if (bytes_per_output_sample_ == 0) {
        WAVDecoderResult result = parse(input, input_len, bytes_consumed);
        if (result != WAV_DECODER_HEADER_READY) {
            return result;
        }

        // Set up decode state
        data_bytes_remaining_ = data_chunk_size_;

        switch (audio_format()) {
            case WAV_FORMAT_PCM:
                bytes_per_input_sample_ = static_cast<uint8_t>(bits_per_sample_ / 8);
                if (bits_per_sample_ == 8) {
                    bytes_per_output_sample_ = 1;
                } else {
                    bytes_per_output_sample_ = bytes_per_input_sample_;
                }
                break;
            case WAV_FORMAT_ALAW:
            case WAV_FORMAT_MULAW:
                bytes_per_input_sample_ = 1;
                bytes_per_output_sample_ = 2;
                bits_per_sample_ = 16;
                break;
            case WAV_FORMAT_IEEE_FLOAT:
                if (bits_per_sample_ != 32) {
                    return WAV_DECODER_ERROR_UNSUPPORTED;
                }
                bytes_per_input_sample_ = 4;
                bytes_per_output_sample_ = 4;
                break;
            default:
                return WAV_DECODER_ERROR_UNSUPPORTED;
        }

        // Reset buf_ for audio sample accumulation
        buf_len_ = 0;
        return WAV_DECODER_HEADER_READY;
    }

    // Audio decoding phase
    bytes_consumed = 0;

    // End of stream: no data remaining (discard any partial sample in buf_)
    if (data_bytes_remaining_ == 0 && buf_len_ == 0) {
        return WAV_DECODER_END_OF_STREAM;
    }

    if (output == nullptr || output_size_bytes < bytes_per_output_sample_) {
        return WAV_DECODER_HEADER_READY;
    }

    WAVAudioFormat fmt = audio_format();

    // Step 1: Complete any partial sample buffered from a previous call
    if (buf_len_ > 0) {
        while (buf_len_ < bytes_per_input_sample_ && bytes_consumed < input_len &&
               data_bytes_remaining_ > 0) {
            buf_[buf_len_++] = input[bytes_consumed++];
            --data_bytes_remaining_;
        }
        if (buf_len_ < bytes_per_input_sample_) {
            if (data_bytes_remaining_ == 0) {
                buf_len_ = 0;
                return WAV_DECODER_END_OF_STREAM;
            }
            return WAV_DECODER_NEED_MORE_DATA;
        }
        convert_sample(fmt, bytes_per_input_sample_, buf_, output);
        buf_len_ = 0;
        ++samples_decoded;
    }

    // Step 2: Bulk-process complete samples directly from input
    {
        // Compute max samples we can process given input, output, and data constraints
        size_t input_avail = (input_len - bytes_consumed) / bytes_per_input_sample_;
        size_t output_avail = (output_size_bytes - samples_decoded * bytes_per_output_sample_) /
                              bytes_per_output_sample_;
        size_t data_avail = data_bytes_remaining_ / bytes_per_input_sample_;
        size_t count = std::min({input_avail, output_avail, data_avail});

        if (count > 0 && fmt == WAV_FORMAT_PCM && bytes_per_input_sample_ > 1) {
            // PCM >= 16-bit: single memcpy for the entire run
            size_t total_bytes = count * bytes_per_input_sample_;
            memcpy(output + samples_decoded * bytes_per_output_sample_, input + bytes_consumed,
                   total_bytes);
            bytes_consumed += total_bytes;
            data_bytes_remaining_ -= static_cast<uint32_t>(total_bytes);
            samples_decoded += count;
        } else if (count > 0 && fmt == WAV_FORMAT_PCM) {
            // PCM 8-bit unsigned->signed: batch XOR loop without per-sample function call
            const uint8_t* src = input + bytes_consumed;
            uint8_t* dst = output + samples_decoded * bytes_per_output_sample_;
            for (size_t i = 0; i < count; ++i) {
                dst[i] = src[i] ^ G711_SIGN_BIT;
            }
            bytes_consumed += count;
            data_bytes_remaining_ -= static_cast<uint32_t>(count);
            samples_decoded += count;
        } else {
            // A-law, mu-law, IEEE float: per-sample conversion
            for (size_t i = 0; i < count; ++i) {
                convert_sample(fmt, bytes_per_input_sample_, input + bytes_consumed,
                               output + samples_decoded * bytes_per_output_sample_);
                bytes_consumed += bytes_per_input_sample_;
                data_bytes_remaining_ -= bytes_per_input_sample_;
                ++samples_decoded;
            }
        }
    }

    // Step 3: Buffer any trailing partial sample for the next call
    if (samples_decoded * bytes_per_output_sample_ + bytes_per_output_sample_ <=
            output_size_bytes &&
        data_bytes_remaining_ > 0 && bytes_consumed < input_len) {
        while (bytes_consumed < input_len && buf_len_ < bytes_per_input_sample_ &&
               data_bytes_remaining_ > 0) {
            buf_[buf_len_++] = input[bytes_consumed++];
            --data_bytes_remaining_;
        }
    }

    if (samples_decoded > 0) {
        return WAV_DECODER_SUCCESS;
    }

    if (data_bytes_remaining_ == 0 && buf_len_ == 0) {
        return WAV_DECODER_END_OF_STREAM;
    }

    return WAV_DECODER_NEED_MORE_DATA;
}

}  // namespace micro_wav
