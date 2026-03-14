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
#include "wav_test_data.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace micro_wav;

static int tests_run = 0;
static int tests_passed = 0;

#define CHECK(cond, msg)                                                              \
    do {                                                                              \
        if (!(cond)) {                                                                \
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
// Helper: parse entire buffer in one call
// ---------------------------------------------------------------------------
static WAVParseResult parse_all(WAVHeaderParser& parser, const uint8_t* data, size_t len) {
    size_t consumed = 0;
    WAVParseResult result = WAV_PARSER_NEED_MORE_DATA;
    while (consumed < len && result == WAV_PARSER_NEED_MORE_DATA) {
        size_t ate = 0;
        result = parser.parse(data + consumed, len - consumed, ate);
        consumed += ate;
    }
    return result;
}

// ---------------------------------------------------------------------------
// Helper: parse by feeding chunk_size bytes at a time (streaming test)
// ---------------------------------------------------------------------------
static WAVParseResult parse_chunked(WAVHeaderParser& parser, const uint8_t* data, size_t len,
                                    size_t chunk_size) {
    size_t pos = 0;
    WAVParseResult result = WAV_PARSER_NEED_MORE_DATA;
    while (pos < len && result == WAV_PARSER_NEED_MORE_DATA) {
        size_t remaining = len - pos;
        size_t feed = remaining < chunk_size ? remaining : chunk_size;
        size_t ate = 0;
        result = parser.parse(data + pos, feed, ate);
        pos += ate;
    }
    return result;
}

// ============================================================================
// Correctness tests
// ============================================================================

static bool test_pcm_16bit_mono_16000hz() {
    WAVHeaderParser parser;
    WAVParseResult result =
        parse_all(parser, test_data::pcm_16bit_mono_16000hz, test_data::pcm_16bit_mono_16000hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(parser.num_channels() == 1, "channels");
    CHECK(parser.sample_rate() == 16000, "sample rate");
    CHECK(parser.bits_per_sample() == 16, "bits per sample");
    CHECK(parser.data_chunk_size() == 1000, "data chunk size");
    return true;
}

static bool test_pcm_16bit_stereo_44100hz() {
    WAVHeaderParser parser;
    WAVParseResult result = parse_all(parser, test_data::pcm_16bit_stereo_44100hz,
                                      test_data::pcm_16bit_stereo_44100hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(parser.num_channels() == 2, "channels");
    CHECK(parser.sample_rate() == 44100, "sample rate");
    CHECK(parser.bits_per_sample() == 16, "bits per sample");
    CHECK(parser.data_chunk_size() == 8820, "data chunk size");
    return true;
}

static bool test_pcm_32bit_mono_48000hz() {
    WAVHeaderParser parser;
    WAVParseResult result = parse_all(parser, test_data::pcm_32bit_mono_48000hz,
                                      test_data::pcm_32bit_mono_48000hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(parser.num_channels() == 1, "channels");
    CHECK(parser.sample_rate() == 48000, "sample rate");
    CHECK(parser.bits_per_sample() == 32, "bits per sample");
    CHECK(parser.data_chunk_size() == 4800, "data chunk size");
    return true;
}

static bool test_float_32bit_stereo_48000hz() {
    WAVHeaderParser parser;
    WAVParseResult result = parse_all(parser, test_data::float_32bit_stereo_48000hz,
                                      test_data::float_32bit_stereo_48000hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_IEEE_FLOAT, "audio format");
    CHECK(parser.num_channels() == 2, "channels");
    CHECK(parser.sample_rate() == 48000, "sample rate");
    CHECK(parser.bits_per_sample() == 32, "bits per sample");
    CHECK(parser.data_chunk_size() == 3200, "data chunk size");
    return true;
}

static bool test_alaw_8bit_mono_8000hz() {
    WAVHeaderParser parser;
    WAVParseResult result =
        parse_all(parser, test_data::alaw_8bit_mono_8000hz, test_data::alaw_8bit_mono_8000hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_ALAW, "audio format");
    CHECK(parser.num_channels() == 1, "channels");
    CHECK(parser.sample_rate() == 8000, "sample rate");
    CHECK(parser.bits_per_sample() == 8, "bits per sample");
    CHECK(parser.data_chunk_size() == 2000, "data chunk size");
    return true;
}

static bool test_mulaw_8bit_mono_8000hz() {
    WAVHeaderParser parser;
    WAVParseResult result = parse_all(parser, test_data::mulaw_8bit_mono_8000hz,
                                      test_data::mulaw_8bit_mono_8000hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_MULAW, "audio format");
    CHECK(parser.num_channels() == 1, "channels");
    CHECK(parser.sample_rate() == 8000, "sample rate");
    CHECK(parser.bits_per_sample() == 8, "bits per sample");
    CHECK(parser.data_chunk_size() == 2000, "data chunk size");
    return true;
}

static bool test_unknown_chunk_before_fmt() {
    WAVHeaderParser parser;
    WAVParseResult result = parse_all(parser, test_data::pcm_with_unknown_chunk,
                                      test_data::pcm_with_unknown_chunk_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(parser.num_channels() == 1, "channels");
    CHECK(parser.sample_rate() == 16000, "sample rate");
    CHECK(parser.bits_per_sample() == 16, "bits per sample");
    CHECK(parser.data_chunk_size() == 1000, "data chunk size");
    return true;
}

static bool test_unknown_chunk_after_fmt() {
    WAVHeaderParser parser;
    WAVParseResult result = parse_all(parser, test_data::pcm_with_unknown_after_fmt,
                                      test_data::pcm_with_unknown_after_fmt_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(parser.num_channels() == 1, "channels");
    CHECK(parser.sample_rate() == 16000, "sample rate");
    CHECK(parser.bits_per_sample() == 16, "bits per sample");
    CHECK(parser.data_chunk_size() == 1000, "data chunk size");
    return true;
}

static bool test_extensible_pcm_24bit() {
    WAVHeaderParser parser;
    WAVParseResult result =
        parse_all(parser, test_data::extensible_pcm_24bit_stereo_96000hz,
                  test_data::extensible_pcm_24bit_stereo_96000hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_PCM, "audio format (from subformat)");
    CHECK(parser.num_channels() == 2, "channels");
    CHECK(parser.sample_rate() == 96000, "sample rate");
    CHECK(parser.bits_per_sample() == 24, "bits per sample");
    CHECK(parser.data_chunk_size() == 5760, "data chunk size");
    return true;
}

static bool test_extensible_float_32bit() {
    WAVHeaderParser parser;
    WAVParseResult result =
        parse_all(parser, test_data::extensible_float_32bit_mono_44100hz,
                  test_data::extensible_float_32bit_mono_44100hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_IEEE_FLOAT, "audio format (from subformat)");
    CHECK(parser.num_channels() == 1, "channels");
    CHECK(parser.sample_rate() == 44100, "sample rate");
    CHECK(parser.bits_per_sample() == 32, "bits per sample");
    CHECK(parser.data_chunk_size() == 4410, "data chunk size");
    return true;
}

static bool test_fmt_size_18() {
    WAVHeaderParser parser;
    WAVParseResult result =
        parse_all(parser, test_data::pcm_fmt_size_18, test_data::pcm_fmt_size_18_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "parse result");
    CHECK(parser.audio_format() == WAV_FORMAT_PCM, "audio format");
    CHECK(parser.num_channels() == 1, "channels");
    CHECK(parser.sample_rate() == 16000, "sample rate");
    CHECK(parser.bits_per_sample() == 16, "bits per sample");
    CHECK(parser.data_chunk_size() == 1000, "data chunk size");
    return true;
}

// ============================================================================
// Error tests
// ============================================================================

static bool test_error_no_riff() {
    WAVHeaderParser parser;
    WAVParseResult result =
        parse_all(parser, test_data::error_no_riff, test_data::error_no_riff_len);
    CHECK(result == WAV_PARSER_ERROR_NO_RIFF, "expected NO_RIFF error");
    return true;
}

static bool test_error_no_wave() {
    WAVHeaderParser parser;
    WAVParseResult result =
        parse_all(parser, test_data::error_no_wave, test_data::error_no_wave_len);
    CHECK(result == WAV_PARSER_ERROR_NO_WAVE, "expected NO_WAVE error");
    return true;
}

static bool test_error_fmt_too_small() {
    WAVHeaderParser parser;
    WAVParseResult result =
        parse_all(parser, test_data::error_fmt_too_small, test_data::error_fmt_too_small_len);
    CHECK(result == WAV_PARSER_ERROR_FAILED, "expected FAILED error");
    return true;
}

static bool test_error_extensible_too_small() {
    WAVHeaderParser parser;
    WAVParseResult result = parse_all(parser, test_data::error_extensible_too_small,
                                      test_data::error_extensible_too_small_len);
    CHECK(result == WAV_PARSER_ERROR_FAILED, "expected FAILED error");
    return true;
}

// ============================================================================
// Streaming tests: feed each valid header 1 byte at a time
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

static const StreamingTestCase streaming_cases[] = {
    {"pcm_16bit_mono_16000hz", test_data::pcm_16bit_mono_16000hz,
     test_data::pcm_16bit_mono_16000hz_len, WAV_FORMAT_PCM, 1, 16000, 16, 1000},
    {"pcm_16bit_stereo_44100hz", test_data::pcm_16bit_stereo_44100hz,
     test_data::pcm_16bit_stereo_44100hz_len, WAV_FORMAT_PCM, 2, 44100, 16, 8820},
    {"pcm_32bit_mono_48000hz", test_data::pcm_32bit_mono_48000hz,
     test_data::pcm_32bit_mono_48000hz_len, WAV_FORMAT_PCM, 1, 48000, 32, 4800},
    {"float_32bit_stereo_48000hz", test_data::float_32bit_stereo_48000hz,
     test_data::float_32bit_stereo_48000hz_len, WAV_FORMAT_IEEE_FLOAT, 2, 48000, 32, 3200},
    {"alaw_8bit_mono_8000hz", test_data::alaw_8bit_mono_8000hz,
     test_data::alaw_8bit_mono_8000hz_len, WAV_FORMAT_ALAW, 1, 8000, 8, 2000},
    {"mulaw_8bit_mono_8000hz", test_data::mulaw_8bit_mono_8000hz,
     test_data::mulaw_8bit_mono_8000hz_len, WAV_FORMAT_MULAW, 1, 8000, 8, 2000},
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
    for (const auto& tc : streaming_cases) {
        WAVHeaderParser parser;
        WAVParseResult result = parse_chunked(parser, tc.data, tc.len, 1);
        if (result != WAV_PARSER_HEADER_READY) {
            fprintf(stderr, "  FAIL: streaming byte-by-byte %s: bad parse result %d\n", tc.name,
                    result);
            return false;
        }
        if (parser.audio_format() != tc.expected_format ||
            parser.num_channels() != tc.expected_channels ||
            parser.sample_rate() != tc.expected_sample_rate ||
            parser.bits_per_sample() != tc.expected_bits ||
            parser.data_chunk_size() != tc.expected_data_size) {
            fprintf(stderr, "  FAIL: streaming byte-by-byte %s: field mismatch\n", tc.name);
            return false;
        }
    }
    return true;
}

static bool test_streaming_two_bytes() {
    for (const auto& tc : streaming_cases) {
        WAVHeaderParser parser;
        WAVParseResult result = parse_chunked(parser, tc.data, tc.len, 2);
        if (result != WAV_PARSER_HEADER_READY) {
            fprintf(stderr, "  FAIL: streaming 2-byte %s: bad parse result %d\n", tc.name, result);
            return false;
        }
        if (parser.audio_format() != tc.expected_format ||
            parser.num_channels() != tc.expected_channels ||
            parser.sample_rate() != tc.expected_sample_rate ||
            parser.bits_per_sample() != tc.expected_bits ||
            parser.data_chunk_size() != tc.expected_data_size) {
            fprintf(stderr, "  FAIL: streaming 2-byte %s: field mismatch\n", tc.name);
            return false;
        }
    }
    return true;
}

static bool test_streaming_three_bytes() {
    for (const auto& tc : streaming_cases) {
        WAVHeaderParser parser;
        WAVParseResult result = parse_chunked(parser, tc.data, tc.len, 3);
        if (result != WAV_PARSER_HEADER_READY) {
            fprintf(stderr, "  FAIL: streaming 3-byte %s: bad parse result %d\n", tc.name, result);
            return false;
        }
        if (parser.audio_format() != tc.expected_format ||
            parser.num_channels() != tc.expected_channels ||
            parser.sample_rate() != tc.expected_sample_rate ||
            parser.bits_per_sample() != tc.expected_bits ||
            parser.data_chunk_size() != tc.expected_data_size) {
            fprintf(stderr, "  FAIL: streaming 3-byte %s: field mismatch\n", tc.name);
            return false;
        }
    }
    return true;
}

static bool test_streaming_five_bytes() {
    for (const auto& tc : streaming_cases) {
        WAVHeaderParser parser;
        WAVParseResult result = parse_chunked(parser, tc.data, tc.len, 5);
        if (result != WAV_PARSER_HEADER_READY) {
            fprintf(stderr, "  FAIL: streaming 5-byte %s: bad parse result %d\n", tc.name, result);
            return false;
        }
        if (parser.audio_format() != tc.expected_format ||
            parser.num_channels() != tc.expected_channels ||
            parser.sample_rate() != tc.expected_sample_rate ||
            parser.bits_per_sample() != tc.expected_bits ||
            parser.data_chunk_size() != tc.expected_data_size) {
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
    WAVHeaderParser parser;

    // Parse first header
    WAVParseResult result =
        parse_all(parser, test_data::pcm_16bit_mono_16000hz, test_data::pcm_16bit_mono_16000hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "first parse");
    CHECK(parser.sample_rate() == 16000, "first sample rate");

    // Reset and parse a different header
    parser.reset();
    result = parse_all(parser, test_data::pcm_16bit_stereo_44100hz,
                       test_data::pcm_16bit_stereo_44100hz_len);
    CHECK(result == WAV_PARSER_HEADER_READY, "second parse after reset");
    CHECK(parser.sample_rate() == 44100, "second sample rate");
    CHECK(parser.num_channels() == 2, "second channels");
    return true;
}

// ============================================================================
// Incomplete data test
// ============================================================================

static bool test_incomplete_data() {
    WAVHeaderParser parser;
    // Feed only the first 10 bytes (partial RIFF header)
    size_t consumed = 0;
    WAVParseResult result =
        parser.parse(test_data::pcm_16bit_mono_16000hz, 10, consumed);
    CHECK(result == WAV_PARSER_NEED_MORE_DATA, "partial feed returns NEED_MORE_DATA");
    return true;
}

// ============================================================================
// main
// ============================================================================

int main() {
    printf("=== Correctness tests ===\n");
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

    printf("\n=== Streaming tests ===\n");
    RUN_TEST(test_streaming_byte_by_byte);
    RUN_TEST(test_streaming_two_bytes);
    RUN_TEST(test_streaming_three_bytes);
    RUN_TEST(test_streaming_five_bytes);

    printf("\n=== Other tests ===\n");
    RUN_TEST(test_reset);
    RUN_TEST(test_incomplete_data);

    printf("\n%d/%d tests passed\n", tests_passed, tests_run);
    return tests_passed == tests_run ? EXIT_SUCCESS : EXIT_FAILURE;
}
