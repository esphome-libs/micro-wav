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
#include "wav_test_data.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace micro_wav;

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, msg)                                                              \
    do {                                                                              \
        const bool check_ok_ = (cond);                                                \
        if (!check_ok_) {                                                             \
            fprintf(stderr, "  FAIL: %s (line %d)\n    condition: %s\n", msg,         \
                    __LINE__, #cond);                                                 \
            return false;                                                             \
        }                                                                             \
    } while (0)

#define RUN_TEST(fn)                                                                  \
    do {                                                                              \
        tests_run++;                                                                  \
        printf("  %-60s", #fn);                                                       \
        if (fn()) {                                                                   \
            tests_passed++;                                                           \
            printf("PASS\n");                                                         \
        } else {                                                                      \
            printf("FAIL\n");                                                         \
        }                                                                             \
    } while (0)

// ---------------------------------------------------------------------------
// Helper: decode header from entire buffer in one call
// ---------------------------------------------------------------------------
static WAVDecoderResult decode_header(WAVDecoder& decoder, const uint8_t* data, size_t len) {
    size_t consumed = 0;
    WAVDecoderResult result = WAV_DECODER_NEED_MORE_DATA;
    while (consumed < len && result == WAV_DECODER_NEED_MORE_DATA) {
        size_t ate = 0;
        size_t decoded = 0;
        result = decoder.decode(data + consumed, len - consumed, nullptr, 0, ate, decoded);
        consumed += ate;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Helper: decode header by feeding chunk_size bytes at a time (streaming test)
// ---------------------------------------------------------------------------
static WAVDecoderResult decode_header_chunked(WAVDecoder& decoder, const uint8_t* data, size_t len,
                                              size_t chunk_size) {
    size_t pos = 0;
    WAVDecoderResult result = WAV_DECODER_NEED_MORE_DATA;
    while (pos < len && result == WAV_DECODER_NEED_MORE_DATA) {
        size_t remaining = len - pos;
        size_t feed = remaining < chunk_size ? remaining : chunk_size;
        size_t ate = 0;
        size_t decoded = 0;
        result = decoder.decode(data + pos, feed, nullptr, 0, ate, decoded);
        pos += ate;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Helper: decode header then all audio in one call
// ---------------------------------------------------------------------------
static WAVDecoderResult decode_all(WAVDecoder& decoder, const uint8_t* data, size_t len,
                                   uint8_t* output, size_t output_size, size_t& total_samples) {
    size_t pos = 0;
    total_samples = 0;

    // Header phase
    while (pos < len) {
        size_t consumed = 0;
        size_t decoded = 0;
        WAVDecoderResult result =
            decoder.decode(data + pos, len - pos, nullptr, 0, consumed, decoded);
        pos += consumed;
        if (result == WAV_DECODER_HEADER_READY) {
            break;
        }
        if (result != WAV_DECODER_NEED_MORE_DATA) {
            return result;
        }
    }

    // Audio decode phase
    WAVDecoderResult last_result = WAV_DECODER_SUCCESS;
    while (pos < len && last_result == WAV_DECODER_SUCCESS) {
        size_t consumed = 0;
        size_t decoded = 0;
        last_result = decoder.decode(
            data + pos, len - pos,
            output + total_samples * decoder.get_bits_per_sample() / 8,
            output_size - total_samples * decoder.get_bits_per_sample() / 8, consumed, decoded);
        pos += consumed;
        total_samples += decoded;
    }
    return last_result;
}

// ============================================================================
// Header tests
// ============================================================================

static bool test_pcm_16bit_mono_16000hz() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::pcm_16bit_mono_16000hz,
                                            test_data::pcm_16bit_mono_16000hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(decoder.get_channels() == 1, "channels");
    CHECK(decoder.get_sample_rate() == 16000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 16, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 1000, "data chunk size");
    return true;
}

static bool test_pcm_16bit_stereo_44100hz() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::pcm_16bit_stereo_44100hz,
                                            test_data::pcm_16bit_stereo_44100hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(decoder.get_channels() == 2, "channels");
    CHECK(decoder.get_sample_rate() == 44100, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 16, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 8820, "data chunk size");
    return true;
}

static bool test_pcm_32bit_mono_48000hz() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::pcm_32bit_mono_48000hz,
                                            test_data::pcm_32bit_mono_48000hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(decoder.get_channels() == 1, "channels");
    CHECK(decoder.get_sample_rate() == 48000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 32, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 4800, "data chunk size");
    return true;
}

static bool test_float_32bit_stereo_48000hz() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::float_32bit_stereo_48000hz,
                                            test_data::float_32bit_stereo_48000hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_IEEE_FLOAT, "audio format");
    CHECK(decoder.get_channels() == 2, "channels");
    CHECK(decoder.get_sample_rate() == 48000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 32, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 3200, "data chunk size");
    return true;
}

static bool test_alaw_8bit_mono_8000hz() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::alaw_8bit_mono_8000hz,
                                            test_data::alaw_8bit_mono_8000hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_ALAW, "audio format");
    CHECK(decoder.get_channels() == 1, "channels");
    CHECK(decoder.get_sample_rate() == 8000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 16, "bits per sample (after decode setup)");
    CHECK(decoder.get_data_chunk_size() == 2000, "data chunk size");
    return true;
}

static bool test_mulaw_8bit_mono_8000hz() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::mulaw_8bit_mono_8000hz,
                                            test_data::mulaw_8bit_mono_8000hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_MULAW, "audio format");
    CHECK(decoder.get_channels() == 1, "channels");
    CHECK(decoder.get_sample_rate() == 8000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 16, "bits per sample (after decode setup)");
    CHECK(decoder.get_data_chunk_size() == 2000, "data chunk size");
    return true;
}

static bool test_unknown_chunk_before_fmt() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::pcm_with_unknown_chunk,
                                            test_data::pcm_with_unknown_chunk_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(decoder.get_channels() == 1, "channels");
    CHECK(decoder.get_sample_rate() == 16000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 16, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 1000, "data chunk size");
    return true;
}

static bool test_unknown_chunk_after_fmt() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::pcm_with_unknown_after_fmt,
                                            test_data::pcm_with_unknown_after_fmt_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(decoder.get_channels() == 1, "channels");
    CHECK(decoder.get_sample_rate() == 16000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 16, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 1000, "data chunk size");
    return true;
}

static bool test_extensible_pcm_24bit() {
    WAVDecoder decoder;
    WAVDecoderResult result =
        decode_header(decoder, test_data::extensible_pcm_24bit_stereo_96000hz,
                      test_data::extensible_pcm_24bit_stereo_96000hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_PCM, "audio format (from subformat)");
    CHECK(decoder.get_channels() == 2, "channels");
    CHECK(decoder.get_sample_rate() == 96000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 24, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 5760, "data chunk size");
    return true;
}

static bool test_extensible_float_32bit() {
    WAVDecoder decoder;
    WAVDecoderResult result =
        decode_header(decoder, test_data::extensible_float_32bit_mono_44100hz,
                      test_data::extensible_float_32bit_mono_44100hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_IEEE_FLOAT, "audio format (from subformat)");
    CHECK(decoder.get_channels() == 1, "channels");
    CHECK(decoder.get_sample_rate() == 44100, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 32, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 4410, "data chunk size");
    return true;
}

static bool test_fmt_size_18() {
    WAVDecoder decoder;
    WAVDecoderResult result =
        decode_header(decoder, test_data::pcm_fmt_size_18, test_data::pcm_fmt_size_18_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "decode result");
    CHECK(decoder.get_audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(decoder.get_channels() == 1, "channels");
    CHECK(decoder.get_sample_rate() == 16000, "sample rate");
    CHECK(decoder.get_bits_per_sample() == 16, "bits per sample");
    CHECK(decoder.get_data_chunk_size() == 1000, "data chunk size");
    return true;
}

// ============================================================================
// Error tests
// ============================================================================

static bool test_error_no_riff() {
    WAVDecoder decoder;
    WAVDecoderResult result =
        decode_header(decoder, test_data::error_no_riff, test_data::error_no_riff_len);
    CHECK(result == WAV_DECODER_ERROR_NO_RIFF, "expected NO_RIFF error");
    return true;
}

static bool test_error_no_wave() {
    WAVDecoder decoder;
    WAVDecoderResult result =
        decode_header(decoder, test_data::error_no_wave, test_data::error_no_wave_len);
    CHECK(result == WAV_DECODER_ERROR_NO_WAVE, "expected NO_WAVE error");
    return true;
}

static bool test_error_fmt_too_small() {
    WAVDecoder decoder;
    WAVDecoderResult result =
        decode_header(decoder, test_data::error_fmt_too_small, test_data::error_fmt_too_small_len);
    CHECK(result == WAV_DECODER_ERROR_FAILED, "expected FAILED error");
    return true;
}

static bool test_error_extensible_too_small() {
    WAVDecoder decoder;
    WAVDecoderResult result = decode_header(decoder, test_data::error_extensible_too_small,
                                            test_data::error_extensible_too_small_len);
    CHECK(result == WAV_DECODER_ERROR_FAILED, "expected FAILED error");
    return true;
}

// ============================================================================
// Decode tests
// ============================================================================

static bool test_decode_pcm_8bit() {
    WAVDecoder decoder;
    uint8_t output[4];
    size_t total = 0;
    WAVDecoderResult result =
        decode_all(decoder, test_data::decode_pcm_8bit_mono,
                   test_data::decode_pcm_8bit_mono_len, output, sizeof(output), total);
    CHECK(result == WAV_DECODER_END_OF_STREAM || result == WAV_DECODER_SUCCESS, "decode result");
    CHECK(total == 4, "4 samples decoded");
    CHECK(decoder.get_bits_per_sample() == 8, "output bps = 8");
    // 0x00 unsigned -> 0x80 signed, 0x80->0x00, 0xFF->0x7F, 0x40->0xC0
    CHECK(output[0] == 0x80, "sample 0");
    CHECK(output[1] == 0x00, "sample 1");
    CHECK(output[2] == 0x7F, "sample 2");
    CHECK(output[3] == 0xC0, "sample 3");
    return true;
}

static bool test_decode_pcm_16bit() {
    WAVDecoder decoder;
    uint8_t output[6];
    size_t total = 0;
    WAVDecoderResult result =
        decode_all(decoder, test_data::decode_pcm_16bit_mono,
                   test_data::decode_pcm_16bit_mono_len, output, sizeof(output), total);
    CHECK(result == WAV_DECODER_END_OF_STREAM || result == WAV_DECODER_SUCCESS, "decode result");
    CHECK(total == 3, "3 samples decoded");
    CHECK(decoder.get_bits_per_sample() == 16, "output bps = 16");
    // Pass-through: bytes should match input audio
    CHECK(output[0] == 0x00 && output[1] == 0x00, "sample 0 = 0");
    CHECK(output[2] == 0xFF && output[3] == 0x7F, "sample 1 = 32767");
    CHECK(output[4] == 0x00 && output[5] == 0x80, "sample 2 = -32768");
    return true;
}

static bool test_decode_pcm_24bit() {
    WAVDecoder decoder;
    uint8_t output[6];
    size_t total = 0;
    WAVDecoderResult result =
        decode_all(decoder, test_data::decode_pcm_24bit_mono,
                   test_data::decode_pcm_24bit_mono_len, output, sizeof(output), total);
    CHECK(result == WAV_DECODER_END_OF_STREAM || result == WAV_DECODER_SUCCESS, "decode result");
    CHECK(total == 2, "2 samples decoded");
    CHECK(decoder.get_bits_per_sample() == 24, "output bps = 24");
    // Pass-through
    CHECK(output[0] == 0x01 && output[1] == 0x02 && output[2] == 0x03, "sample 0");
    CHECK(output[3] == 0x04 && output[4] == 0x05 && output[5] == 0x06, "sample 1");
    return true;
}

static int16_t read_le16(const uint8_t* p) {
    return static_cast<int16_t>(static_cast<uint16_t>(p[0]) |
                                (static_cast<uint16_t>(p[1]) << 8));
}

static int32_t read_le32(const uint8_t* p) {
    return static_cast<int32_t>(static_cast<uint32_t>(p[0]) |
                                (static_cast<uint32_t>(p[1]) << 8) |
                                (static_cast<uint32_t>(p[2]) << 16) |
                                (static_cast<uint32_t>(p[3]) << 24));
}

static bool test_decode_alaw() {
    WAVDecoder decoder;
    uint8_t output[8];  // 4 samples x 2 bytes
    size_t total = 0;
    WAVDecoderResult result =
        decode_all(decoder, test_data::decode_alaw_mono, test_data::decode_alaw_mono_len,
                   output, sizeof(output), total);
    CHECK(result == WAV_DECODER_END_OF_STREAM || result == WAV_DECODER_SUCCESS, "decode result");
    CHECK(total == 4, "4 samples decoded");
    CHECK(decoder.get_bits_per_sample() == 16, "output bps = 16");
    CHECK(read_le16(output + 0) == 8, "sample 0 = +8");
    CHECK(read_le16(output + 2) == -8, "sample 1 = -8");
    CHECK(read_le16(output + 4) == -32256, "sample 2 = -32256");
    CHECK(read_le16(output + 6) == 32256, "sample 3 = +32256");
    return true;
}

static bool test_decode_mulaw() {
    WAVDecoder decoder;
    uint8_t output[8];  // 4 samples x 2 bytes
    size_t total = 0;
    WAVDecoderResult result =
        decode_all(decoder, test_data::decode_mulaw_mono, test_data::decode_mulaw_mono_len,
                   output, sizeof(output), total);
    CHECK(result == WAV_DECODER_END_OF_STREAM || result == WAV_DECODER_SUCCESS, "decode result");
    CHECK(total == 4, "4 samples decoded");
    CHECK(decoder.get_bits_per_sample() == 16, "output bps = 16");
    CHECK(read_le16(output + 0) == 0, "sample 0 = 0");
    CHECK(read_le16(output + 2) == 0, "sample 1 = 0");
    CHECK(read_le16(output + 4) == 32124, "sample 2 = +32124");
    CHECK(read_le16(output + 6) == -32124, "sample 3 = -32124");
    return true;
}

static bool test_decode_float() {
    WAVDecoder decoder;
    uint8_t output[8];  // 2 samples x 4 bytes
    size_t total = 0;
    WAVDecoderResult result =
        decode_all(decoder, test_data::decode_float_mono, test_data::decode_float_mono_len,
                   output, sizeof(output), total);
    CHECK(result == WAV_DECODER_END_OF_STREAM || result == WAV_DECODER_SUCCESS, "decode result");
    CHECK(total == 2, "2 samples decoded");
    CHECK(decoder.get_bits_per_sample() == 32, "output bps = 32");
    CHECK(read_le32(output + 0) == 0, "sample 0 = 0");
    // 0.5 * 2147483647 = 1073741823.5, truncated to 1073741823
    CHECK(read_le32(output + 4) == 1073741823, "sample 1 = 1073741823");
    return true;
}

static bool test_decode_end_of_stream() {
    WAVDecoder decoder;
    uint8_t output[6];
    size_t total = 0;
    decode_all(decoder, test_data::decode_pcm_16bit_mono,
               test_data::decode_pcm_16bit_mono_len, output, sizeof(output), total);

    // Next call should return END_OF_STREAM
    size_t consumed = 0;
    size_t decoded = 0;
    uint8_t dummy[2];
    WAVDecoderResult result =
        decoder.decode(nullptr, 0, dummy, sizeof(dummy), consumed, decoded);
    CHECK(result == WAV_DECODER_END_OF_STREAM, "end of stream after all data consumed");
    CHECK(decoded == 0, "no samples decoded");
    return true;
}

static bool test_decode_streaming() {
    // Feed the PCM 16-bit file one byte at a time through decode
    WAVDecoder decoder;
    const uint8_t* data = test_data::decode_pcm_16bit_mono;
    size_t len = test_data::decode_pcm_16bit_mono_len;
    size_t pos = 0;

    // Header phase: feed byte by byte
    WAVDecoderResult result = WAV_DECODER_NEED_MORE_DATA;
    while (pos < len && result == WAV_DECODER_NEED_MORE_DATA) {
        size_t consumed = 0;
        size_t decoded = 0;
        result = decoder.decode(data + pos, 1, nullptr, 0, consumed, decoded);
        pos += consumed;
    }
    CHECK(result == WAV_DECODER_HEADER_READY, "header ready after streaming");
    CHECK(decoder.get_bits_per_sample() == 16, "output bps = 16");

    // Audio phase: feed byte by byte
    uint8_t output[6];
    size_t total = 0;
    while (pos < len) {
        size_t consumed = 0;
        size_t decoded = 0;
        result = decoder.decode(data + pos, 1, output + total * 2, sizeof(output) - total * 2,
                                consumed, decoded);
        pos += consumed;
        total += decoded;
        if (result == WAV_DECODER_END_OF_STREAM) {
            break;
        }
    }
    // With byte-by-byte feeding of 16-bit samples, we only get a full sample every 2 bytes
    CHECK(total == 3, "3 samples decoded via streaming");
    CHECK(output[0] == 0x00 && output[1] == 0x00, "stream sample 0");
    CHECK(output[2] == 0xFF && output[3] == 0x7F, "stream sample 1");
    CHECK(output[4] == 0x00 && output[5] == 0x80, "stream sample 2");
    return true;
}

// ============================================================================
// Streaming header tests: feed each valid header 1 byte at a time
// ============================================================================

struct StreamingTestCase {
    const char* name;
    const uint8_t* data;
    size_t len;
    WAVAudioFormat expected_format;
    uint16_t expected_channels;
    uint32_t expected_sample_rate;
    uint16_t expected_bits;
    uint32_t expected_data_size;
};

static const StreamingTestCase STREAMING_CASES[] = {
    {"pcm_16bit_mono_16000hz", test_data::pcm_16bit_mono_16000hz,
     test_data::pcm_16bit_mono_16000hz_len, WAV_FORMAT_PCM, 1, 16000, 16, 1000},
    {"pcm_16bit_stereo_44100hz", test_data::pcm_16bit_stereo_44100hz,
     test_data::pcm_16bit_stereo_44100hz_len, WAV_FORMAT_PCM, 2, 44100, 16, 8820},
    {"pcm_32bit_mono_48000hz", test_data::pcm_32bit_mono_48000hz,
     test_data::pcm_32bit_mono_48000hz_len, WAV_FORMAT_PCM, 1, 48000, 32, 4800},
    {"float_32bit_stereo_48000hz", test_data::float_32bit_stereo_48000hz,
     test_data::float_32bit_stereo_48000hz_len, WAV_FORMAT_IEEE_FLOAT, 2, 48000, 32, 3200},
    {"alaw_8bit_mono_8000hz", test_data::alaw_8bit_mono_8000hz,
     test_data::alaw_8bit_mono_8000hz_len, WAV_FORMAT_ALAW, 1, 8000, 16, 2000},
    {"mulaw_8bit_mono_8000hz", test_data::mulaw_8bit_mono_8000hz,
     test_data::mulaw_8bit_mono_8000hz_len, WAV_FORMAT_MULAW, 1, 8000, 16, 2000},
    {"pcm_with_unknown_chunk", test_data::pcm_with_unknown_chunk,
     test_data::pcm_with_unknown_chunk_len, WAV_FORMAT_PCM, 1, 16000, 16, 1000},
    {"pcm_with_unknown_after_fmt", test_data::pcm_with_unknown_after_fmt,
     test_data::pcm_with_unknown_after_fmt_len, WAV_FORMAT_PCM, 1, 16000, 16, 1000},
    {"extensible_pcm_24bit", test_data::extensible_pcm_24bit_stereo_96000hz,
     test_data::extensible_pcm_24bit_stereo_96000hz_len, WAV_FORMAT_PCM, 2, 96000, 24, 5760},
    {"extensible_float_32bit", test_data::extensible_float_32bit_mono_44100hz,
     test_data::extensible_float_32bit_mono_44100hz_len, WAV_FORMAT_IEEE_FLOAT, 1, 44100, 32,
     4410},
    {"pcm_fmt_size_18", test_data::pcm_fmt_size_18, test_data::pcm_fmt_size_18_len, WAV_FORMAT_PCM,
     1, 16000, 16, 1000},
};

static bool test_streaming_byte_by_byte() {
    for (const auto& tc : STREAMING_CASES) {
        WAVDecoder decoder;
        WAVDecoderResult result = decode_header_chunked(decoder, tc.data, tc.len, 1);
        if (result != WAV_DECODER_HEADER_READY) {
            fprintf(stderr, "  FAIL: streaming byte-by-byte %s: bad result %d\n", tc.name, result);
            return false;
        }
        if (decoder.get_audio_format() != tc.expected_format ||
            decoder.get_channels() != tc.expected_channels ||
            decoder.get_sample_rate() != tc.expected_sample_rate ||
            decoder.get_bits_per_sample() != tc.expected_bits ||
            decoder.get_data_chunk_size() != tc.expected_data_size) {
            fprintf(stderr, "  FAIL: streaming byte-by-byte %s: field mismatch\n", tc.name);
            return false;
        }
    }
    return true;
}

static bool test_streaming_two_bytes() {
    for (const auto& tc : STREAMING_CASES) {
        WAVDecoder decoder;
        WAVDecoderResult result = decode_header_chunked(decoder, tc.data, tc.len, 2);
        if (result != WAV_DECODER_HEADER_READY) {
            fprintf(stderr, "  FAIL: streaming 2-byte %s: bad result %d\n", tc.name, result);
            return false;
        }
        if (decoder.get_audio_format() != tc.expected_format ||
            decoder.get_channels() != tc.expected_channels ||
            decoder.get_sample_rate() != tc.expected_sample_rate ||
            decoder.get_bits_per_sample() != tc.expected_bits ||
            decoder.get_data_chunk_size() != tc.expected_data_size) {
            fprintf(stderr, "  FAIL: streaming 2-byte %s: field mismatch\n", tc.name);
            return false;
        }
    }
    return true;
}

static bool test_streaming_three_bytes() {
    for (const auto& tc : STREAMING_CASES) {
        WAVDecoder decoder;
        WAVDecoderResult result = decode_header_chunked(decoder, tc.data, tc.len, 3);
        if (result != WAV_DECODER_HEADER_READY) {
            fprintf(stderr, "  FAIL: streaming 3-byte %s: bad result %d\n", tc.name, result);
            return false;
        }
        if (decoder.get_audio_format() != tc.expected_format ||
            decoder.get_channels() != tc.expected_channels ||
            decoder.get_sample_rate() != tc.expected_sample_rate ||
            decoder.get_bits_per_sample() != tc.expected_bits ||
            decoder.get_data_chunk_size() != tc.expected_data_size) {
            fprintf(stderr, "  FAIL: streaming 3-byte %s: field mismatch\n", tc.name);
            return false;
        }
    }
    return true;
}

static bool test_streaming_five_bytes() {
    for (const auto& tc : STREAMING_CASES) {
        WAVDecoder decoder;
        WAVDecoderResult result = decode_header_chunked(decoder, tc.data, tc.len, 5);
        if (result != WAV_DECODER_HEADER_READY) {
            fprintf(stderr, "  FAIL: streaming 5-byte %s: bad result %d\n", tc.name, result);
            return false;
        }
        if (decoder.get_audio_format() != tc.expected_format ||
            decoder.get_channels() != tc.expected_channels ||
            decoder.get_sample_rate() != tc.expected_sample_rate ||
            decoder.get_bits_per_sample() != tc.expected_bits ||
            decoder.get_data_chunk_size() != tc.expected_data_size) {
            fprintf(stderr, "  FAIL: streaming 5-byte %s: field mismatch\n", tc.name);
            return false;
        }
    }
    return true;
}

// ============================================================================
// Reset test
// ============================================================================

static bool test_reset() {
    WAVDecoder decoder;

    // Decode first header
    WAVDecoderResult result = decode_header(decoder, test_data::pcm_16bit_mono_16000hz,
                                            test_data::pcm_16bit_mono_16000hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "first decode");
    CHECK(decoder.get_sample_rate() == 16000, "first sample rate");

    // Reset and decode a different header
    decoder.reset();
    result = decode_header(decoder, test_data::pcm_16bit_stereo_44100hz,
                           test_data::pcm_16bit_stereo_44100hz_len);
    CHECK(result == WAV_DECODER_HEADER_READY, "second decode after reset");
    CHECK(decoder.get_sample_rate() == 44100, "second sample rate");
    CHECK(decoder.get_channels() == 2, "second channels");
    return true;
}

// ============================================================================
// Incomplete data test
// ============================================================================

static bool test_incomplete_data() {
    WAVDecoder decoder;
    // Feed only a partial RIFF header
    static constexpr size_t PARTIAL_HEADER_SIZE = 10;
    size_t consumed = 0;
    size_t decoded = 0;
    WAVDecoderResult result = decoder.decode(test_data::pcm_16bit_mono_16000hz,
                                             PARTIAL_HEADER_SIZE, nullptr, 0, consumed, decoded);
    CHECK(result == WAV_DECODER_NEED_MORE_DATA, "partial feed returns NEED_MORE_DATA");
    return true;
}

// ============================================================================
// Streaming-sentinel test: data chunk size of 0 should be treated as unknown
// length (read until input runs out), not as immediate end-of-stream.
// ============================================================================

static bool test_data_chunk_size_zero_streams() {
    // Hand-crafted minimal PCM 16-bit mono 16 kHz WAV with data chunk size = 0
    // and three PCM samples (0, +32767, -32768) appended after the header.
    static constexpr uint8_t SENTINEL_WAV[] = {
        // RIFF header
        'R', 'I', 'F', 'F',
        0x00, 0x00, 0x00, 0x00,  // RIFF size: also 0; should not affect decoder
        'W', 'A', 'V', 'E',
        // fmt chunk
        'f', 'm', 't', ' ',
        0x10, 0x00, 0x00, 0x00,  // fmt size = 16
        0x01, 0x00,              // audio format = 1 (PCM)
        0x01, 0x00,              // channels = 1
        0x80, 0x3E, 0x00, 0x00,  // sample rate = 16000
        0x00, 0x7D, 0x00, 0x00,  // byte rate = 32000
        0x02, 0x00,              // block align = 2
        0x10, 0x00,              // bits per sample = 16
        // data chunk header with sentinel size 0
        'd', 'a', 't', 'a',
        0x00, 0x00, 0x00, 0x00,  // data chunk size = 0 (streaming sentinel)
        // Three 16-bit LE PCM samples: 0, +32767, -32768
        0x00, 0x00,
        0xFF, 0x7F,
        0x00, 0x80,
    };
    static constexpr size_t SENTINEL_WAV_LEN = sizeof(SENTINEL_WAV);
    static constexpr size_t HEADER_LEN = 44;
    static constexpr size_t PCM_BYTES = SENTINEL_WAV_LEN - HEADER_LEN;

    WAVDecoder decoder;
    size_t pos = 0;

    // Header phase: feed bytes until HEADER_READY
    WAVDecoderResult result = WAV_DECODER_NEED_MORE_DATA;
    while (pos < SENTINEL_WAV_LEN) {
        size_t consumed = 0;
        size_t decoded = 0;
        result = decoder.decode(SENTINEL_WAV + pos, SENTINEL_WAV_LEN - pos, nullptr, 0, consumed,
                                decoded);
        pos += consumed;
        if (result == WAV_DECODER_HEADER_READY) {
            break;
        }
        CHECK(result == WAV_DECODER_NEED_MORE_DATA, "header phase result");
    }
    CHECK(result == WAV_DECODER_HEADER_READY, "header ready reached");
    // Sentinel zero is normalized to UINT32_MAX so the decoder treats the data
    // section as unbounded instead of immediately ending.
    CHECK(decoder.get_data_chunk_size() == UINT32_MAX,
          "sentinel zero normalized to UINT32_MAX");

    // Audio phase: decode the trailing PCM samples
    uint8_t output[PCM_BYTES];
    size_t total_decoded = 0;
    while (pos < SENTINEL_WAV_LEN) {
        size_t consumed = 0;
        size_t decoded = 0;
        result = decoder.decode(SENTINEL_WAV + pos, SENTINEL_WAV_LEN - pos,
                                output + total_decoded * 2, sizeof(output) - total_decoded * 2,
                                consumed, decoded);
        pos += consumed;
        total_decoded += decoded;
        if (result != WAV_DECODER_SUCCESS) {
            break;
        }
    }
    CHECK(total_decoded == 3, "all three PCM samples decoded (would be 0 if bug present)");
    CHECK(read_le16(output + 0) == 0, "sample 0 = 0");
    CHECK(read_le16(output + 2) == 32767, "sample 1 = +32767");
    CHECK(read_le16(output + 4) == -32768, "sample 2 = -32768");

    // With sentinel-zero, decoder must not signal end-of-stream at input exhaustion;
    // it should keep asking for more data so the caller can supply more or close.
    size_t consumed = 0;
    size_t decoded = 0;
    uint8_t dummy[2];
    result = decoder.decode(nullptr, 0, dummy, sizeof(dummy), consumed, decoded);
    CHECK(result == WAV_DECODER_NEED_MORE_DATA,
          "sentinel-zero stream stays open after input exhausted");
    return true;
}

// ============================================================================
// main
// ============================================================================

int main() {
    printf("=== Header tests ===\n");
    RUN_TEST(test_pcm_16bit_mono_16000hz);
    RUN_TEST(test_pcm_16bit_stereo_44100hz);
    RUN_TEST(test_pcm_32bit_mono_48000hz);
    RUN_TEST(test_float_32bit_stereo_48000hz);
    RUN_TEST(test_alaw_8bit_mono_8000hz);
    RUN_TEST(test_mulaw_8bit_mono_8000hz);
    RUN_TEST(test_unknown_chunk_before_fmt);
    RUN_TEST(test_unknown_chunk_after_fmt);
    RUN_TEST(test_extensible_pcm_24bit);
    RUN_TEST(test_extensible_float_32bit);
    RUN_TEST(test_fmt_size_18);

    printf("\n=== Error tests ===\n");
    RUN_TEST(test_error_no_riff);
    RUN_TEST(test_error_no_wave);
    RUN_TEST(test_error_fmt_too_small);
    RUN_TEST(test_error_extensible_too_small);

    printf("\n=== Decode tests ===\n");
    RUN_TEST(test_decode_pcm_8bit);
    RUN_TEST(test_decode_pcm_16bit);
    RUN_TEST(test_decode_pcm_24bit);
    RUN_TEST(test_decode_alaw);
    RUN_TEST(test_decode_mulaw);
    RUN_TEST(test_decode_float);
    RUN_TEST(test_decode_end_of_stream);
    RUN_TEST(test_decode_streaming);

    printf("\n=== Streaming header tests ===\n");
    RUN_TEST(test_streaming_byte_by_byte);
    RUN_TEST(test_streaming_two_bytes);
    RUN_TEST(test_streaming_three_bytes);
    RUN_TEST(test_streaming_five_bytes);

    printf("\n=== Other tests ===\n");
    RUN_TEST(test_reset);
    RUN_TEST(test_incomplete_data);
    RUN_TEST(test_data_chunk_size_zero_streams);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? EXIT_SUCCESS : EXIT_FAILURE;
}
