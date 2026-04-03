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

/// @file write_test_wavs.cpp
/// @brief Writes test WAV headers to files for validation with sox/soxi.
///
/// Build:  cmake -B build -DBUILD_TEST_WAV_WRITER=ON && cmake --build build
/// Run:    ./build/write_test_wavs
/// Verify: for f in test_*.wav; do echo "=== $f ==="; soxi "$f"; echo; done

#include "wav_test_data.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

/// Write a WAV header followed by zeroed audio data so sox can validate it.
static bool write_wav(const char* filename, const uint8_t* header, size_t header_len,
                      uint32_t data_size) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return false;
    }

    // Write the header
    if (fwrite(header, 1, header_len, f) != header_len) {
        fprintf(stderr, "Failed to write header to %s\n", filename);
        fclose(f);
        return false;
    }

    // Write zeroed audio data
    const size_t buf_size = 1024;
    uint8_t zeros[buf_size];
    memset(zeros, 0, buf_size);

    uint32_t remaining = data_size;
    while (remaining > 0) {
        size_t to_write = remaining < buf_size ? remaining : buf_size;
        if (fwrite(zeros, 1, to_write, f) != to_write) {
            fprintf(stderr, "Failed to write audio data to %s\n", filename);
            fclose(f);
            return false;
        }
        remaining -= static_cast<uint32_t>(to_write);
    }

    fclose(f);
    printf("Wrote %s (header=%zu bytes, data=%u bytes)\n", filename, header_len, data_size);
    return true;
}

int main() {
    bool ok = true;

    // NOLINTBEGIN(readability-magic-numbers)
    ok &= write_wav("test_pcm_16bit_mono_16000hz.wav", test_data::pcm_16bit_mono_16000hz,
                     test_data::pcm_16bit_mono_16000hz_len, 1000);

    ok &= write_wav("test_pcm_16bit_stereo_44100hz.wav", test_data::pcm_16bit_stereo_44100hz,
                     test_data::pcm_16bit_stereo_44100hz_len, 8820);

    ok &= write_wav("test_pcm_32bit_mono_48000hz.wav", test_data::pcm_32bit_mono_48000hz,
                     test_data::pcm_32bit_mono_48000hz_len, 4800);

    ok &= write_wav("test_float_32bit_stereo_48000hz.wav", test_data::float_32bit_stereo_48000hz,
                     test_data::float_32bit_stereo_48000hz_len, 3200);

    ok &= write_wav("test_alaw_8bit_mono_8000hz.wav", test_data::alaw_8bit_mono_8000hz,
                     test_data::alaw_8bit_mono_8000hz_len, 2000);

    ok &= write_wav("test_mulaw_8bit_mono_8000hz.wav", test_data::mulaw_8bit_mono_8000hz,
                     test_data::mulaw_8bit_mono_8000hz_len, 2000);

    ok &= write_wav("test_pcm_with_unknown_chunk.wav", test_data::pcm_with_unknown_chunk,
                     test_data::pcm_with_unknown_chunk_len, 1000);

    ok &= write_wav("test_pcm_with_unknown_after_fmt.wav", test_data::pcm_with_unknown_after_fmt,
                     test_data::pcm_with_unknown_after_fmt_len, 1000);

    ok &= write_wav("test_extensible_pcm_24bit_stereo_96000hz.wav",
                     test_data::extensible_pcm_24bit_stereo_96000hz,
                     test_data::extensible_pcm_24bit_stereo_96000hz_len, 5760);

    ok &= write_wav("test_extensible_float_32bit_mono_44100hz.wav",
                     test_data::extensible_float_32bit_mono_44100hz,
                     test_data::extensible_float_32bit_mono_44100hz_len, 4410);

    ok &= write_wav("test_pcm_fmt_size_18.wav", test_data::pcm_fmt_size_18,
                     test_data::pcm_fmt_size_18_len, 1000);
    // NOLINTEND(readability-magic-numbers)

    if (!ok) {
        fprintf(stderr, "\nSome files failed to write!\n");
        return EXIT_FAILURE;
    }

    printf("\nAll files written. Validate with:\n");
    printf("  for f in test_*.wav; do echo \"=== $f ===\"; soxi \"$f\"; echo; done\n");
    return EXIT_SUCCESS;
}
