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

#include "micro_wav/wav_header_parser.h"

#include <algorithm>
#include <cstring>

namespace micro_wav {

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

void WAVHeaderParser::reset() {
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
}

WAVParseResult WAVHeaderParser::parse(const uint8_t* input, size_t input_len,
                                      size_t& bytes_consumed) {
    bytes_consumed = 0;

    if (state_ == State::IN_DATA) {
        return WAVParseResult::HEADER_READY;
    }
    if (state_ == State::ERROR) {
        return WAVParseResult::ERROR_FAILED;
    }

    while (bytes_consumed < input_len) {
        // Skip phase
        if (skip_bytes_ > 0) {
            size_t can_skip =
                std::min(static_cast<size_t>(skip_bytes_), input_len - bytes_consumed);
            skip_bytes_ -= static_cast<uint32_t>(can_skip);
            bytes_consumed += can_skip;
            if (skip_bytes_ > 0) {
                return WAVParseResult::NEED_MORE_DATA;
            }
            continue;
        }

        // Accumulate phase
        while (buf_len_ < buf_needed_ && bytes_consumed < input_len) {
            buf_[buf_len_++] = input[bytes_consumed++];
        }
        if (buf_len_ < buf_needed_) {
            return WAVParseResult::NEED_MORE_DATA;
        }

        // Process phase
        WAVParseResult result = process_field();
        buf_len_ = 0;
        if (result != WAVParseResult::NEED_MORE_DATA) {
            return result;
        }
    }

    return WAVParseResult::NEED_MORE_DATA;
}

WAVParseResult WAVHeaderParser::process_field() {
    switch (state_) {
        case State::RIFF_TAG:
            if (!tag_equals(buf_, "RIFF")) {
                state_ = State::ERROR;
                return WAVParseResult::ERROR_NO_RIFF;
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
                return WAVParseResult::ERROR_NO_WAVE;
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
                    if (current_chunk_size_ < 16) {
                        state_ = State::ERROR;
                        return WAVParseResult::ERROR_FAILED;
                    }
                    state_ = State::FMT_AUDIO_FORMAT;
                    buf_needed_ = 2;
                    break;
                case PendingChunk::DATA:
                    data_chunk_size_ = current_chunk_size_;
                    state_ = State::IN_DATA;
                    return WAVParseResult::HEADER_READY;
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
            // fmt chunk has 16 bytes of standard fields.
            // If the chunk is larger, skip the extra bytes.
            // Use pad_to_even for RIFF alignment.
            if (current_chunk_size_ > 16) {
                skip_bytes_ = pad_to_even(current_chunk_size_) - 16;
            }
            state_ = State::CHUNK_TAG;
            buf_needed_ = 4;
            break;

        case State::IN_DATA:
            return WAVParseResult::HEADER_READY;

        case State::ERROR:
            return WAVParseResult::ERROR_FAILED;
    }

    return WAVParseResult::NEED_MORE_DATA;
}

}  // namespace micro_wav
