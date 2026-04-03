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

/// @file wav_test_data.h
/// @brief Hand-constructed WAV headers for testing.
///
/// Valid test headers were validated against sox/soxi using write_test_wavs.cpp.
/// To re-validate: build with -DBUILD_TEST_WAV_WRITER=ON, run the writer,
/// then check each output file with: soxi <file>.wav

#pragma once

#include <cstddef>
#include <cstdint>

namespace test_data {

// RIFF size = 4(WAVE) + sum of all chunks (each = 8 header + chunk_data_size [+1 pad if odd])
// Standard: RIFF size = 4 + (8+16) + (8+data_size) = 36 + data_size

// ============================================================================
// Standard PCM: 16-bit mono 16000 Hz
// block_align=2, byte_rate=32000, data_size=1000, RIFF_size=1036
// Expected: format=PCM, channels=1, sample_rate=16000, bits=16, data_size=1000
// ============================================================================
static const uint8_t pcm_16bit_mono_16000hz[] = {
    'R', 'I', 'F', 'F',
    0x0C, 0x04, 0x00, 0x00,  // RIFF size = 1036 (0x040C)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x01, 0x00,              // audio format = 1 (PCM)
    0x01, 0x00,              // channels = 1
    0x80, 0x3E, 0x00, 0x00,  // sample rate = 16000
    0x00, 0x7D, 0x00, 0x00,  // byte rate = 32000
    0x02, 0x00,              // block align = 2
    0x10, 0x00,              // bits per sample = 16
    'd', 'a', 't', 'a',
    0xE8, 0x03, 0x00, 0x00,  // data size = 1000
};
static const size_t pcm_16bit_mono_16000hz_len = sizeof(pcm_16bit_mono_16000hz);

// ============================================================================
// Standard PCM: 16-bit stereo 44100 Hz
// block_align=4, byte_rate=176400, data_size=8820, RIFF_size=8856
// Expected: format=PCM, channels=2, sample_rate=44100, bits=16, data_size=8820
// ============================================================================
static const uint8_t pcm_16bit_stereo_44100hz[] = {
    'R', 'I', 'F', 'F',
    0x98, 0x22, 0x00, 0x00,  // RIFF size = 8856 (0x2298)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x01, 0x00,              // audio format = 1 (PCM)
    0x02, 0x00,              // channels = 2
    0x44, 0xAC, 0x00, 0x00,  // sample rate = 44100
    0x10, 0xB1, 0x02, 0x00,  // byte rate = 176400 (0x02B110)
    0x04, 0x00,              // block align = 4
    0x10, 0x00,              // bits per sample = 16
    'd', 'a', 't', 'a',
    0x74, 0x22, 0x00, 0x00,  // data size = 8820 (0x2274)
};
static const size_t pcm_16bit_stereo_44100hz_len = sizeof(pcm_16bit_stereo_44100hz);

// ============================================================================
// Standard PCM: 32-bit mono 48000 Hz
// block_align=4, byte_rate=192000, data_size=4800, RIFF_size=4836
// Expected: format=PCM, channels=1, sample_rate=48000, bits=32, data_size=4800
// ============================================================================
static const uint8_t pcm_32bit_mono_48000hz[] = {
    'R', 'I', 'F', 'F',
    0xE4, 0x12, 0x00, 0x00,  // RIFF size = 4836 (0x12E4)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x01, 0x00,              // audio format = 1 (PCM)
    0x01, 0x00,              // channels = 1
    0x80, 0xBB, 0x00, 0x00,  // sample rate = 48000
    0x00, 0xEE, 0x02, 0x00,  // byte rate = 192000 (0x02EE00)
    0x04, 0x00,              // block align = 4
    0x20, 0x00,              // bits per sample = 32
    'd', 'a', 't', 'a',
    0xC0, 0x12, 0x00, 0x00,  // data size = 4800 (0x12C0)
};
static const size_t pcm_32bit_mono_48000hz_len = sizeof(pcm_32bit_mono_48000hz);

// ============================================================================
// IEEE Float: 32-bit stereo 48000 Hz
// block_align=8, byte_rate=384000, data_size=3200, RIFF_size=3236
// Expected: format=IEEE_FLOAT, channels=2, sample_rate=48000, bits=32, data_size=3200
// ============================================================================
static const uint8_t float_32bit_stereo_48000hz[] = {
    'R', 'I', 'F', 'F',
    0xA4, 0x0C, 0x00, 0x00,  // RIFF size = 3236 (0x0CA4)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x03, 0x00,              // audio format = 3 (IEEE float)
    0x02, 0x00,              // channels = 2
    0x80, 0xBB, 0x00, 0x00,  // sample rate = 48000
    0x00, 0xDC, 0x05, 0x00,  // byte rate = 384000 (0x05DC00)
    0x08, 0x00,              // block align = 8
    0x20, 0x00,              // bits per sample = 32
    'd', 'a', 't', 'a',
    0x80, 0x0C, 0x00, 0x00,  // data size = 3200 (0x0C80)
};
static const size_t float_32bit_stereo_48000hz_len = sizeof(float_32bit_stereo_48000hz);

// ============================================================================
// A-law: 8-bit mono 8000 Hz
// block_align=1, byte_rate=8000, data_size=2000, RIFF_size=2036
// Expected: format=ALAW, channels=1, sample_rate=8000, bits=8, data_size=2000
// ============================================================================
static const uint8_t alaw_8bit_mono_8000hz[] = {
    'R', 'I', 'F', 'F',
    0xF4, 0x07, 0x00, 0x00,  // RIFF size = 2036 (0x07F4)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x06, 0x00,              // audio format = 6 (A-law)
    0x01, 0x00,              // channels = 1
    0x40, 0x1F, 0x00, 0x00,  // sample rate = 8000
    0x40, 0x1F, 0x00, 0x00,  // byte rate = 8000
    0x01, 0x00,              // block align = 1
    0x08, 0x00,              // bits per sample = 8
    'd', 'a', 't', 'a',
    0xD0, 0x07, 0x00, 0x00,  // data size = 2000 (0x07D0)
};
static const size_t alaw_8bit_mono_8000hz_len = sizeof(alaw_8bit_mono_8000hz);

// ============================================================================
// mu-law: 8-bit mono 8000 Hz
// block_align=1, byte_rate=8000, data_size=2000, RIFF_size=2036
// Expected: format=MULAW, channels=1, sample_rate=8000, bits=8, data_size=2000
// ============================================================================
static const uint8_t mulaw_8bit_mono_8000hz[] = {
    'R', 'I', 'F', 'F',
    0xF4, 0x07, 0x00, 0x00,  // RIFF size = 2036 (0x07F4)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x07, 0x00,              // audio format = 7 (mu-law)
    0x01, 0x00,              // channels = 1
    0x40, 0x1F, 0x00, 0x00,  // sample rate = 8000
    0x40, 0x1F, 0x00, 0x00,  // byte rate = 8000
    0x01, 0x00,              // block align = 1
    0x08, 0x00,              // bits per sample = 8
    'd', 'a', 't', 'a',
    0xD0, 0x07, 0x00, 0x00,  // data size = 2000 (0x07D0)
};
static const size_t mulaw_8bit_mono_8000hz_len = sizeof(mulaw_8bit_mono_8000hz);

// ============================================================================
// Unknown chunk before fmt: tests chunk skipping
// LIST chunk: 8 hdr + 5 data + 1 pad = 14 bytes
// RIFF_size = 4 + 14 + 24 + 8 + 1000 = 1050
// Expected: format=PCM, channels=1, sample_rate=16000, bits=16, data_size=1000
// ============================================================================
static const uint8_t pcm_with_unknown_chunk[] = {
    'R', 'I', 'F', 'F',
    0x1A, 0x04, 0x00, 0x00,  // RIFF size = 1050 (0x041A)
    'W', 'A', 'V', 'E',
    // Unknown "LIST" chunk (odd size, tests padding)
    'L', 'I', 'S', 'T',
    0x05, 0x00, 0x00, 0x00,  // chunk size = 5 (odd, padded to 6)
    0x01, 0x02, 0x03, 0x04, 0x05,  // dummy data
    0x00,                    // padding byte
    // fmt chunk
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x01, 0x00,              // audio format = 1 (PCM)
    0x01, 0x00,              // channels = 1
    0x80, 0x3E, 0x00, 0x00,  // sample rate = 16000
    0x00, 0x7D, 0x00, 0x00,  // byte rate = 32000
    0x02, 0x00,              // block align = 2
    0x10, 0x00,              // bits per sample = 16
    'd', 'a', 't', 'a',
    0xE8, 0x03, 0x00, 0x00,  // data size = 1000
};
static const size_t pcm_with_unknown_chunk_len = sizeof(pcm_with_unknown_chunk);

// ============================================================================
// Unknown chunk between fmt and data: tests chunk skipping after fmt
// fact chunk: 8 hdr + 8 data = 16 bytes
// RIFF_size = 4 + 24 + 16 + 8 + 1000 = 1052
// Expected: format=PCM, channels=1, sample_rate=16000, bits=16, data_size=1000
// ============================================================================
static const uint8_t pcm_with_unknown_after_fmt[] = {
    'R', 'I', 'F', 'F',
    0x1C, 0x04, 0x00, 0x00,  // RIFF size = 1052 (0x041C)
    'W', 'A', 'V', 'E',
    // fmt chunk
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x01, 0x00,              // audio format = 1 (PCM)
    0x01, 0x00,              // channels = 1
    0x80, 0x3E, 0x00, 0x00,  // sample rate = 16000
    0x00, 0x7D, 0x00, 0x00,  // byte rate = 32000
    0x02, 0x00,              // block align = 2
    0x10, 0x00,              // bits per sample = 16
    // Unknown "fact" chunk (even size)
    'f', 'a', 'c', 't',
    0x08, 0x00, 0x00, 0x00,  // chunk size = 8
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // data chunk
    'd', 'a', 't', 'a',
    0xE8, 0x03, 0x00, 0x00,  // data size = 1000
};
static const size_t pcm_with_unknown_after_fmt_len = sizeof(pcm_with_unknown_after_fmt);

// ============================================================================
// WAVE_FORMAT_EXTENSIBLE: PCM 24-bit stereo 96000 Hz
// block_align=6, byte_rate=576000, data_size=5760
// fmt chunk: 8 hdr + 40 data = 48 bytes
// RIFF_size = 4 + 48 + 8 + 5760 = 5820
// Expected: format=PCM, channels=2, sample_rate=96000, bits=24, data_size=5760
// ============================================================================
static const uint8_t extensible_pcm_24bit_stereo_96000hz[] = {
    'R', 'I', 'F', 'F',
    0xBC, 0x16, 0x00, 0x00,  // RIFF size = 5820 (0x16BC)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x28, 0x00, 0x00, 0x00,  // chunk size = 40
    0xFE, 0xFF,              // audio format = 0xFFFE (EXTENSIBLE)
    0x02, 0x00,              // channels = 2
    0x00, 0x77, 0x01, 0x00,  // sample rate = 96000 (0x017700)
    0x00, 0xCA, 0x08, 0x00,  // byte rate = 576000 (0x08CA00)
    0x06, 0x00,              // block align = 6
    0x18, 0x00,              // bits per sample = 24
    // Extension:
    0x16, 0x00,              // cbSize = 22
    0x18, 0x00,              // validBitsPerSample = 24
    0x03, 0x00, 0x00, 0x00,  // channelMask = 0x03 (FL | FR)
    // SubFormat GUID: PCM = {00000001-0000-0010-8000-00aa00389b71}
    0x01, 0x00,              // subformat tag = 1 (PCM)
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00,
    0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
    // data chunk
    'd', 'a', 't', 'a',
    0x80, 0x16, 0x00, 0x00,  // data size = 5760 (0x1680)
};
static const size_t extensible_pcm_24bit_stereo_96000hz_len =
    sizeof(extensible_pcm_24bit_stereo_96000hz);

// ============================================================================
// WAVE_FORMAT_EXTENSIBLE: IEEE Float 32-bit mono 44100 Hz
// block_align=4, byte_rate=176400, data_size=4410
// fmt chunk: 8 hdr + 40 data = 48 bytes
// RIFF_size = 4 + 48 + 8 + 4410 = 4470
// Expected: format=IEEE_FLOAT, channels=1, sample_rate=44100, bits=32, data_size=4410
// ============================================================================
static const uint8_t extensible_float_32bit_mono_44100hz[] = {
    'R', 'I', 'F', 'F',
    0x76, 0x11, 0x00, 0x00,  // RIFF size = 4470 (0x1176)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x28, 0x00, 0x00, 0x00,  // chunk size = 40
    0xFE, 0xFF,              // audio format = 0xFFFE (EXTENSIBLE)
    0x01, 0x00,              // channels = 1
    0x44, 0xAC, 0x00, 0x00,  // sample rate = 44100
    0x10, 0xB1, 0x02, 0x00,  // byte rate = 176400 (0x02B110)
    0x04, 0x00,              // block align = 4
    0x20, 0x00,              // bits per sample = 32
    // Extension:
    0x16, 0x00,              // cbSize = 22
    0x20, 0x00,              // validBitsPerSample = 32
    0x04, 0x00, 0x00, 0x00,  // channelMask = 0x04 (FC)
    // SubFormat GUID: IEEE_FLOAT = {00000003-0000-0010-8000-00aa00389b71}
    0x03, 0x00,              // subformat tag = 3 (IEEE float)
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x80, 0x00,
    0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71,
    // data chunk
    'd', 'a', 't', 'a',
    0x3A, 0x11, 0x00, 0x00,  // data size = 4410 (0x113A)
};
static const size_t extensible_float_32bit_mono_44100hz_len =
    sizeof(extensible_float_32bit_mono_44100hz);

// ============================================================================
// Non-standard fmt chunk size (18 bytes with cbSize field, no extensible)
// Some encoders write fmt chunks with cbSize=0 even for PCM
// RIFF_size = 4 + (8+18) + 8 + 1000 = 1038
// Expected: format=PCM, channels=1, sample_rate=16000, bits=16, data_size=1000
// ============================================================================
static const uint8_t pcm_fmt_size_18[] = {
    'R', 'I', 'F', 'F',
    0x0E, 0x04, 0x00, 0x00,  // RIFF size = 1038 (0x040E)
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x12, 0x00, 0x00, 0x00,  // chunk size = 18
    0x01, 0x00,              // audio format = 1 (PCM)
    0x01, 0x00,              // channels = 1
    0x80, 0x3E, 0x00, 0x00,  // sample rate = 16000
    0x00, 0x7D, 0x00, 0x00,  // byte rate = 32000
    0x02, 0x00,              // block align = 2
    0x10, 0x00,              // bits per sample = 16
    0x00, 0x00,              // cbSize = 0
    'd', 'a', 't', 'a',
    0xE8, 0x03, 0x00, 0x00,  // data size = 1000
};
static const size_t pcm_fmt_size_18_len = sizeof(pcm_fmt_size_18);

// ============================================================================
// Error case: missing RIFF tag
// Expected: WAV_PARSER_ERROR_NO_RIFF
// ============================================================================
static const uint8_t error_no_riff[] = {
    'X', 'I', 'F', 'F',
    0x00, 0x00, 0x00, 0x00,
    'W', 'A', 'V', 'E',
};
static const size_t error_no_riff_len = sizeof(error_no_riff);

// ============================================================================
// Error case: missing WAVE tag
// Expected: WAV_PARSER_ERROR_NO_WAVE
// ============================================================================
static const uint8_t error_no_wave[] = {
    'R', 'I', 'F', 'F',
    0x00, 0x00, 0x00, 0x00,
    'X', 'A', 'V', 'E',
};
static const size_t error_no_wave_len = sizeof(error_no_wave);

// ============================================================================
// Error case: fmt chunk too small (size < 16)
// Expected: WAV_PARSER_ERROR_FAILED
// ============================================================================
static const uint8_t error_fmt_too_small[] = {
    'R', 'I', 'F', 'F',
    0x14, 0x00, 0x00, 0x00,  // RIFF size = 20
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x08, 0x00, 0x00, 0x00,  // chunk size = 8 (too small)
    0x01, 0x00, 0x01, 0x00,
    0x80, 0x3E, 0x00, 0x00,
};
static const size_t error_fmt_too_small_len = sizeof(error_fmt_too_small);

// ============================================================================
// Error case: EXTENSIBLE format but chunk too small for extension
// Expected: WAV_PARSER_ERROR_FAILED
// ============================================================================
static const uint8_t error_extensible_too_small[] = {
    'R', 'I', 'F', 'F',
    0x1C, 0x00, 0x00, 0x00,  // RIFF size = 28
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16 (too small for extensible)
    0xFE, 0xFF,              // audio format = 0xFFFE (EXTENSIBLE)
    0x01, 0x00,
    0x80, 0x3E, 0x00, 0x00,
    0x00, 0x7D, 0x00, 0x00,
    0x02, 0x00,
    0x10, 0x00,
};
static const size_t error_extensible_too_small_len = sizeof(error_extensible_too_small);

// ============================================================================
// Decode test data: headers with appended audio samples
// ============================================================================

// PCM 8-bit mono 8000 Hz, 4 samples
// Unsigned input: [0x00, 0x80, 0xFF, 0x40]
// Expected signed output (XOR 0x80): [0x80, 0x00, 0x7F, 0xC0]
static const uint8_t decode_pcm_8bit_mono[] = {
    'R', 'I', 'F', 'F',
    0x28, 0x00, 0x00, 0x00,  // RIFF size = 40
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,  // chunk size = 16
    0x01, 0x00,              // PCM
    0x01, 0x00,              // mono
    0x40, 0x1F, 0x00, 0x00,  // 8000 Hz
    0x40, 0x1F, 0x00, 0x00,  // byte rate = 8000
    0x01, 0x00,              // block align = 1
    0x08, 0x00,              // 8 bits per sample
    'd', 'a', 't', 'a',
    0x04, 0x00, 0x00, 0x00,  // data size = 4
    // Audio samples (unsigned)
    0x00, 0x80, 0xFF, 0x40,
};
static const size_t decode_pcm_8bit_mono_len = sizeof(decode_pcm_8bit_mono);

// PCM 16-bit mono 16000 Hz, 3 samples (6 bytes)
// Samples (LE): [0, 32767, -32768]
static const uint8_t decode_pcm_16bit_mono[] = {
    'R', 'I', 'F', 'F',
    0x2A, 0x00, 0x00, 0x00,  // RIFF size = 42
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,
    0x01, 0x00,              // PCM
    0x01, 0x00,              // mono
    0x80, 0x3E, 0x00, 0x00,  // 16000 Hz
    0x00, 0x7D, 0x00, 0x00,  // byte rate = 32000
    0x02, 0x00,              // block align = 2
    0x10, 0x00,              // 16 bits
    'd', 'a', 't', 'a',
    0x06, 0x00, 0x00, 0x00,  // data size = 6
    // Audio samples (LE int16)
    0x00, 0x00,  // 0
    0xFF, 0x7F,  // 32767
    0x00, 0x80,  // -32768
};
static const size_t decode_pcm_16bit_mono_len = sizeof(decode_pcm_16bit_mono);

// PCM 24-bit mono 8000 Hz, 2 samples (6 bytes)
// Pass-through test
static const uint8_t decode_pcm_24bit_mono[] = {
    'R', 'I', 'F', 'F',
    0x2A, 0x00, 0x00, 0x00,  // RIFF size = 42
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,
    0x01, 0x00,              // PCM
    0x01, 0x00,              // mono
    0x40, 0x1F, 0x00, 0x00,  // 8000 Hz
    0xC0, 0x5D, 0x00, 0x00,  // byte rate = 24000
    0x03, 0x00,              // block align = 3
    0x18, 0x00,              // 24 bits
    'd', 'a', 't', 'a',
    0x06, 0x00, 0x00, 0x00,  // data size = 6
    // Audio samples (LE int24)
    0x01, 0x02, 0x03,
    0x04, 0x05, 0x06,
};
static const size_t decode_pcm_24bit_mono_len = sizeof(decode_pcm_24bit_mono);

// A-law mono 8000 Hz, 4 samples
// Input: [0xD5, 0x55, 0x2A, 0xAA]
// After XOR 0x55: [0x80, 0x00, 0x7F, 0xFF]
// Decoded int16: [+8, -8, -32256, +32256]
static const uint8_t decode_alaw_mono[] = {
    'R', 'I', 'F', 'F',
    0x28, 0x00, 0x00, 0x00,  // RIFF size = 40
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,
    0x06, 0x00,              // A-law
    0x01, 0x00,              // mono
    0x40, 0x1F, 0x00, 0x00,  // 8000 Hz
    0x40, 0x1F, 0x00, 0x00,  // byte rate = 8000
    0x01, 0x00,              // block align = 1
    0x08, 0x00,              // 8 bits
    'd', 'a', 't', 'a',
    0x04, 0x00, 0x00, 0x00,  // data size = 4
    // Audio samples
    0xD5, 0x55, 0x2A, 0xAA,
};
static const size_t decode_alaw_mono_len = sizeof(decode_alaw_mono);

// mu-law mono 8000 Hz, 4 samples
// Input: [0x7F, 0xFF, 0x00, 0x80]
// Decoded int16: [0, 0, +32124, -32124]
static const uint8_t decode_mulaw_mono[] = {
    'R', 'I', 'F', 'F',
    0x28, 0x00, 0x00, 0x00,  // RIFF size = 40
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,
    0x07, 0x00,              // mu-law
    0x01, 0x00,              // mono
    0x40, 0x1F, 0x00, 0x00,  // 8000 Hz
    0x40, 0x1F, 0x00, 0x00,  // byte rate = 8000
    0x01, 0x00,              // block align = 1
    0x08, 0x00,              // 8 bits
    'd', 'a', 't', 'a',
    0x04, 0x00, 0x00, 0x00,  // data size = 4
    // Audio samples
    0x7F, 0xFF, 0x00, 0x80,
};
static const size_t decode_mulaw_mono_len = sizeof(decode_mulaw_mono);

// IEEE float 32-bit mono 48000 Hz, 2 samples (8 bytes)
// Input floats: [0.0f, 0.5f]
// Expected int32: [0, 1073741823]
static const uint8_t decode_float_mono[] = {
    'R', 'I', 'F', 'F',
    0x2C, 0x00, 0x00, 0x00,  // RIFF size = 44
    'W', 'A', 'V', 'E',
    'f', 'm', 't', ' ',
    0x10, 0x00, 0x00, 0x00,
    0x03, 0x00,              // IEEE float
    0x01, 0x00,              // mono
    0x80, 0xBB, 0x00, 0x00,  // 48000 Hz
    0x00, 0xEE, 0x02, 0x00,  // byte rate = 192000
    0x04, 0x00,              // block align = 4
    0x20, 0x00,              // 32 bits
    'd', 'a', 't', 'a',
    0x08, 0x00, 0x00, 0x00,  // data size = 8
    // Audio samples (LE float32)
    0x00, 0x00, 0x00, 0x00,  // 0.0f
    0x00, 0x00, 0x00, 0x3F,  // 0.5f
};
static const size_t decode_float_mono_len = sizeof(decode_float_mono);

}  // namespace test_data
