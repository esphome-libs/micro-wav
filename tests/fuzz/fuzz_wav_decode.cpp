// Copyright 2026 Kevin Ahrendt
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

// Fuzz harness for micro_wav::WAVDecoder.
//
// Feeds raw WAV container bytes through the streaming decoder in variably-sized
// chunks, so libFuzzer's coverage feedback explores both chunk-boundary bugs
// (header staging, partial-sample buffering across calls) and the audio sample
// conversion paths (PCM 8/16/24/32-bit, G.711 A-law/mu-law, IEEE float).
//
// Unlike a codec with constructor options, WAVDecoder takes no configuration:
// it is default-constructed and reused via reset(). The only construction-level
// knob is therefore a single replay bit. The interesting per-call dimensions
// are instead the INPUT chunk size and the OUTPUT buffer size, both of which the
// caller owns and both of which the harness varies on every decode():
//   - small input chunks split a single sample across calls, exercising the
//     4-byte partial-sample accumulator (buf_);
//   - small output buffers cap how many samples fit, exercising the output-avail
//     clamp and the multi-call drain of one input chunk.
// These control bytes are consumed from the TAIL of the input (via
// FuzzedDataProvider) so the front stays an intact WAV payload, while a small
// set of Tier 1 structural invariants is asserted on every decode (see the
// oracle block in run_decode_pass).
//
// Two build modes:
//   1. libFuzzer:  compile with -fsanitize=fuzzer,address,undefined, which
//      exposes LLVMFuzzerTestOneInput. Use with a corpus directory:
//          ./fuzz_wav_decode corpus_wav/
//   2. Standalone: compile with FUZZ_STANDALONE defined. Takes file paths on
//      argv for crash reproduction, or with no args runs a torture battery.

#include "micro_wav/wav_decoder.h"
#include <fuzzer/FuzzedDataProvider.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using micro_wav::WAVDecoder;
using micro_wav::WAVDecoderResult;

// Disable ASan's container-overflow check for this binary. libFuzzer's prebuilt
// runtime (e.g. Homebrew LLVM's libclang_rt.fuzzer) is compiled without libc++
// container annotations. This harness instantiates std::vector<uint8_t>, the same
// type libFuzzer's dictionary parser uses, but WITH annotations under
// -fsanitize=address. The two get ODR-merged at link time, and the mismatched
// poisoning makes libFuzzer's own ParseOneDictionaryEntry false-positive while
// loading a -dict file. The decoder under test uses no STL containers (raw arrays
// only) and the harness allocates its output buffers exactly sized, so over-writes
// still land in ASan's heap red zone. Turning this one check off costs no real
// coverage. Env ASAN_OPTIONS still overrides this default.
// NOLINTNEXTLINE(bugprone-reserved-identifier,readability-identifier-naming): fixed ASan hook name
extern "C" const char* __asan_default_options() {
    return "detect_container_overflow=0";
}

// Upper bound on the control bytes pulled from the TAIL (via FuzzedDataProvider).
// Each decode() consumes two: one for the input chunk size, one for the output
// buffer size. 128 control bytes thus yield 64 distinct (chunk, out) pairs before
// cycling. Reading them off the back leaves the front payload intact for the
// decoder, so libFuzzer mutates WAV bytes rather than bytes the harness eats.
static constexpr size_t MAX_CONTROL_BYTES = 128;

// One streaming pass: feed `payload` to `decoder` in control-byte-sized chunks,
// writing into a fresh exactly-sized output buffer each call so ASan red-zones
// any over-write immediately. Asserts the Tier 1 oracle on every decode.
// Factored out so it can run twice across a reset() to exercise the re-stream
// path that a single pass misses.

// Map a control byte to a buffer size. Bytes below CTRL_FINE_MAX map 1:1 to small
// sizes (1..32) so the fuzzer can split individual header fields and 24/32-bit
// samples across calls, forcing the partial-field/partial-sample accumulator;
// higher bytes scale by CTRL_BULK_STEP to bulk sizes (up to ~8 KiB) so the memcpy
// fast path and large output buffers also run.
static constexpr size_t CTRL_FINE_MAX = 32;
static constexpr size_t CTRL_BULK_STEP = 36;
static size_t ctrl_to_size(uint8_t b) {
    const size_t v = b;
    return v < CTRL_FINE_MAX ? v + 1 : (v - (CTRL_FINE_MAX - 1)) * CTRL_BULK_STEP;
}

static void run_decode_pass(WAVDecoder& decoder, const std::vector<uint8_t>& payload,
                            const std::vector<uint8_t>& ctrl) {
    size_t offset = 0;
    size_t ctrl_idx = 0;
    size_t iterations = 0;

    // A consuming iteration advances `offset` by >= 1 byte, so payload size bounds
    // those. One extra non-advancing iteration can occur per consuming one: when a
    // prior call left a sample buffered exactly full, the next decode() flushes it
    // (decoded > 0, consumed == 0) before processing new input. The *2 covers that
    // pairing; the constant covers header staging. Either way iterations is finite.
    const size_t max_iterations = payload.size() * 2 + 4096;

    while (offset < payload.size() && iterations < max_iterations) {
        ++iterations;

        // Derive an input chunk size from the next control byte (1..~8 KiB). Small
        // chunks split header fields and 24/32-bit samples across calls, forcing
        // the partial accumulator; large chunks drive the bulk decode path.
        const uint8_t cs_byte = ctrl[ctrl_idx++ % ctrl.size()];
        size_t chunk_size = ctrl_to_size(cs_byte);
        if (chunk_size > payload.size() - offset) {
            chunk_size = payload.size() - offset;
        }

        // Derive an output buffer size from the next control byte, but never below
        // one full output sample once the header is known, so the decode always
        // makes forward progress (the null/too-small guard is covered separately in
        // the standalone torture battery). Allocated exactly so an over-write lands
        // in ASan's red zone.
        const uint8_t os_byte = ctrl[ctrl_idx++ % ctrl.size()];
        size_t out_size = ctrl_to_size(os_byte);
        const size_t min_out =
            decoder.is_header_ready() ? decoder.get_bytes_per_output_sample() : 1;
        if (out_size < min_out) {
            out_size = min_out;
        }
        std::vector<uint8_t> out(out_size);

        size_t consumed = 0;
        size_t decoded = 0;
        WAVDecoderResult result = decoder.decode(payload.data() + offset, chunk_size, out.data(),
                                                 out_size, consumed, decoded);

        // ---- Tier 1 oracle: structural invariants on a single decode ----------
        // The decoder must never report consuming more than it was handed.
        if (consumed > chunk_size) {
            std::abort();
        }
        if (decoder.is_header_ready()) {
            const size_t bps = decoder.get_bytes_per_output_sample();
            // Output sample width is bounded (8..32-bit => 1..4 bytes) and the
            // reported bit depth must agree with it.
            if (bps == 0 || bps > 4) {
                std::abort();
            }
            if (decoder.get_bits_per_sample() != bps * 8) {
                std::abort();
            }
            // A valid header always resolves a positive channel count, a positive
            // sample rate, and a recognized format. The decoder errors out
            // otherwise rather than reporting the header ready.
            if (decoder.get_channels() == 0 || decoder.get_sample_rate() == 0) {
                std::abort();
            }
            if (decoder.get_audio_format() == micro_wav::WAV_FORMAT_UNKNOWN) {
                std::abort();
            }
            // The core memory-safety bound: a single decode() must never write
            // more sample bytes than the output buffer holds. Written in the
            // division form (bps >= 1, checked above) so a corrupt `decoded` from
            // a decoder bug can't overflow the product and slip past the check.
            if (decoded > out_size / bps) {
                std::abort();
            }
        }
        // SUCCESS must mean at least one sample was produced.
        if (result == micro_wav::WAV_DECODER_SUCCESS && decoded == 0) {
            std::abort();
        }

        offset += consumed;

        if (result == micro_wav::WAV_DECODER_END_OF_STREAM) {
            break;
        }
        if (result < 0) {
            break;  // unrecoverable header/format error
        }
        // Forward-progress guarantee: if nothing was consumed and nothing was
        // produced, bail out instead of spinning.
        if (consumed == 0 && decoded == 0) {
            break;
        }
    }
}

// NOLINTNEXTLINE(readability-identifier-naming): fixed libFuzzer entry point name
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    FuzzedDataProvider fdp(data, size);

    // Decoder configuration, consumed from the TAIL of the input. FuzzedDataProvider
    // integral reads come off the back of the buffer, so the payload PREFIX is
    // preserved: only the trailing config/control bytes are peeled off, leaving the
    // front of a real .wav seed intact. WAVDecoder takes no constructor options, so
    // the lone config bit replays the stream a second time across a reset(),
    // exercising the re-stream path (state reset, second HEADER_READY). An exhausted
    // provider reads 0, i.e. the default: single pass.
    const uint8_t cfg = fdp.ConsumeIntegral<uint8_t>();
    const bool replay = (cfg & 0x01) != 0;

    // Reserve up to ~1/8 of the input (capped) for chunk/output control bytes. If
    // the input is tiny, fall back to a single neutral control byte so the decoder
    // still sees the full payload at a moderate chunk size.
    const size_t ctrl_len = std::min(MAX_CONTROL_BYTES, fdp.remaining_bytes() / 8);
    std::vector<uint8_t> ctrl;
    ctrl.reserve(ctrl_len + 1);
    for (size_t i = 0; i < ctrl_len; i++) {
        ctrl.push_back(fdp.ConsumeIntegral<uint8_t>());
    }
    if (ctrl.empty()) {
        ctrl.push_back(0x20);  // neutral default: ~1 KiB chunks and buffers
    }

    const std::vector<uint8_t> payload = fdp.ConsumeRemainingBytes<uint8_t>();
    if (payload.empty()) {
        return 0;
    }

    WAVDecoder decoder;
    run_decode_pass(decoder, payload, ctrl);

    if (replay) {
        decoder.reset();
        run_decode_pass(decoder, payload, ctrl);
    }

    decoder.reset();
    return 0;
}

#ifdef FUZZ_STANDALONE

namespace {

std::vector<uint8_t> read_file(const char* path) {
    std::vector<uint8_t> out;
    FILE* f = std::fopen(path, "rb");
    if (!f)
        return out;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n > 0) {
        out.resize(static_cast<size_t>(n));
        size_t got = std::fread(out.data(), 1, out.size(), f);
        out.resize(got);
    }
    std::fclose(f);
    return out;
}

uint32_t lcg_next(uint32_t& state) {
    state = state * 1664525u + 1013904223u;
    return state;
}

// Append the harness config tail to a raw WAV payload so the WHOLE payload
// survives the FuzzedDataProvider tail reads (which would otherwise peel bytes
// off the back of the .wav). Layout, by consumption order off the back: the cfg
// byte last, preceded by neutral chunk/output control bytes.
std::vector<uint8_t> with_config_tail(std::vector<uint8_t> payload, uint8_t cfg) {
    payload.insert(payload.end(), MAX_CONTROL_BYTES, 0x20);  // ~1 KiB chunks/buffers
    payload.push_back(cfg);
    return payload;
}

// A minimal, valid 16-bit mono PCM WAV with `data_samples` 16-bit samples.
std::vector<uint8_t> make_valid_wav(uint16_t format, uint16_t channels, uint32_t sample_rate,
                                    uint16_t bits, uint32_t data_size) {
    auto u16 = [](std::vector<uint8_t>& v, uint16_t x) {
        v.push_back(static_cast<uint8_t>(x & 0xFF));
        v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
    };
    auto u32 = [](std::vector<uint8_t>& v, uint32_t x) {
        v.push_back(static_cast<uint8_t>(x & 0xFF));
        v.push_back(static_cast<uint8_t>((x >> 8) & 0xFF));
        v.push_back(static_cast<uint8_t>((x >> 16) & 0xFF));
        v.push_back(static_cast<uint8_t>((x >> 24) & 0xFF));
    };
    const uint16_t block_align = static_cast<uint16_t>(channels * (bits / 8));
    const uint32_t byte_rate = sample_rate * block_align;

    std::vector<uint8_t> w;
    w.insert(w.end(), {'R', 'I', 'F', 'F'});
    u32(w, 36 + data_size);
    w.insert(w.end(), {'W', 'A', 'V', 'E'});
    w.insert(w.end(), {'f', 'm', 't', ' '});
    u32(w, 16);
    u16(w, format);
    u16(w, channels);
    u32(w, sample_rate);
    u32(w, byte_rate);
    u16(w, block_align);
    u16(w, bits);
    w.insert(w.end(), {'d', 'a', 't', 'a'});
    u32(w, data_size);
    w.insert(w.end(), data_size, 0);
    return w;
}

void mutate_in_place(std::vector<uint8_t>& buf, uint32_t& rng_state) {
    if (buf.empty())
        return;
    int n = 1 + static_cast<int>((lcg_next(rng_state) >> 24) & 0x07);
    for (int i = 0; i < n; i++) {
        uint32_t r = lcg_next(rng_state);
        size_t pos = r % buf.size();
        uint32_t kind = (r >> 24) & 0x07;
        switch (kind) {
            case 0:
            case 1:
                buf[pos] ^= static_cast<uint8_t>(1u << ((r >> 8) & 0x07));
                break;
            case 2:
            case 3:
                buf[pos] = static_cast<uint8_t>(r >> 16);
                break;
            case 4: {
                static const uint8_t interesting[] = {0x00, 0x01, 0x7F, 0x80,
                                                      0xFF, 0xFE, 0x55, 0xAA};
                buf[pos] = interesting[(r >> 16) & 0x07];
                break;
            }
            case 5: {
                size_t run = 1 + ((r >> 16) & 0x0F);
                for (size_t k = 0; k < run && pos + k < buf.size(); k++) {
                    buf[pos + k] = 0;
                }
                break;
            }
            case 6:
                buf[pos] = static_cast<uint8_t>(buf[pos] + 1);
                break;
            default:
                buf[pos] = static_cast<uint8_t>(buf[pos] - 1);
                break;
        }
    }
}

std::vector<uint8_t> build_random_blob(uint32_t seed, size_t len) {
    std::vector<uint8_t> buf(len);
    uint32_t state = seed;
    for (size_t i = 0; i < len; i++) {
        buf[i] = static_cast<uint8_t>(lcg_next(state) >> 24);
    }
    // Plant a RIFF/WAVE/fmt /data skeleton at the front so the parser engages
    // on otherwise-random data.
    if (buf.size() >= 12) {
        const uint8_t hdr[] = {'R', 'I', 'F', 'F', 0xFF, 0xFF, 0xFF, 0xFF, 'W', 'A', 'V', 'E'};
        std::memcpy(buf.data(), hdr, sizeof(hdr));
    }
    // Sprinkle chunk tags through the body so chunk-walking gets exercised.
    static const char* const tags[] = {"fmt ", "data", "LIST", "fact", "JUNK"};
    for (size_t i = 12; i + 4 < buf.size(); i += 60 + (seed % 200)) {
        const char* tag = tags[(i / 4) % 5];
        std::memcpy(buf.data() + i, tag, 4);
    }
    return buf;
}

}  // namespace

int main(int argc, char** argv) {
    // Mutation mode: "./fuzz_wav_decode -mutate <seedfile>"
    if (argc >= 3 && std::strcmp(argv[1], "-mutate") == 0) {
        std::vector<uint8_t> seed = read_file(argv[2]);
        if (seed.empty()) {
            std::fprintf(stderr, "[fuzz] seed file %s is empty or missing\n", argv[2]);
            return 1;
        }
        const char* iter_env = std::getenv("FUZZ_ITERATIONS");
        const int iters = iter_env ? std::atoi(iter_env) : 2000;
        std::printf("[fuzz] mutation mode: seed=%s (%zu bytes), %d iterations\n", argv[2],
                    seed.size(), iters);

        uint32_t rng_state = 0xC0FFEEu;
        std::vector<uint8_t> scratch;
        scratch.reserve(seed.size());

        LLVMFuzzerTestOneInput(seed.data(), seed.size());

        for (int i = 0; i < iters; i++) {
            scratch = seed;
            mutate_in_place(scratch, rng_state);
            LLVMFuzzerTestOneInput(scratch.data(), scratch.size());
            if ((i + 1) % 200 == 0) {
                std::printf("[fuzz] %d/%d mutated iterations ok\n", i + 1, iters);
            }
        }
        std::printf("[fuzz] mutation fuzzing complete, no sanitizer failures\n");
        return 0;
    }

    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            std::vector<uint8_t> data = read_file(argv[i]);
            std::printf("[fuzz] %s (%zu bytes)\n", argv[i], data.size());
            LLVMFuzzerTestOneInput(data.data(), data.size());
        }
        std::printf("[fuzz] %d file(s) processed cleanly\n", argc - 1);
        return 0;
    }

    std::printf("[fuzz] standalone torture mode\n");

    // Empty / tiny inputs.
    {
        const uint8_t nothing[1] = {0};
        LLVMFuzzerTestOneInput(nothing, 0);
        LLVMFuzzerTestOneInput(nothing, 1);
    }

    // Bare / truncated RIFF captures.
    {
        const uint8_t riff[4] = {'R', 'I', 'F', 'F'};
        LLVMFuzzerTestOneInput(riff, sizeof(riff));
        const uint8_t riff_wave[12] = {'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E'};
        LLVMFuzzerTestOneInput(riff_wave, sizeof(riff_wave));
    }

    // A header claiming a huge data size with almost no data behind it.
    {
        std::vector<uint8_t> w = make_valid_wav(/*PCM*/ 1, 1, 16000, 16, /*data_size=*/0);
        // Rewrite the data chunk size field (last 4 bytes) to a huge value.
        w[w.size() - 4] = 0xFF;
        w[w.size() - 3] = 0xFF;
        w[w.size() - 2] = 0xFF;
        w[w.size() - 1] = 0x7F;
        std::vector<uint8_t> in = with_config_tail(w, 0);
        LLVMFuzzerTestOneInput(in.data(), in.size());
    }

    // Valid streams across every supported format, single pass and replay.
    {
        struct Variant {
            uint16_t format;
            uint16_t channels;
            uint32_t rate;
            uint16_t bits;
            uint32_t data;
        };
        static const Variant variants[] = {
            {1, 1, 16000, 8, 64},   {1, 2, 44100, 16, 64}, {1, 1, 48000, 24, 96},
            {1, 2, 48000, 32, 128}, {3, 2, 48000, 32, 64}, {6, 1, 8000, 8, 32},
            {7, 1, 8000, 8, 32},
        };
        for (const Variant& v : variants) {
            std::vector<uint8_t> w = make_valid_wav(v.format, v.channels, v.rate, v.bits, v.data);
            for (uint8_t cfg : {uint8_t{0}, uint8_t{1}}) {  // single pass, then replay
                std::vector<uint8_t> in = with_config_tail(w, cfg);
                LLVMFuzzerTestOneInput(in.data(), in.size());
            }
        }
    }

    // Direct decode() calls covering the input/output guard paths that the
    // streaming loop deliberately avoids (it always supplies a full-sample
    // buffer and never a null input).
    {
        std::vector<uint8_t> w = make_valid_wav(/*PCM*/ 1, 1, 16000, 16, 64);
        WAVDecoder decoder;
        size_t consumed = 0;
        size_t decoded = 0;
        uint8_t one_byte = 0;

        // Parse the header fully.
        WAVDecoderResult r = decoder.decode(w.data(), w.size(), &one_byte, 0, consumed, decoded);
        while (r == micro_wav::WAV_DECODER_NEED_MORE_DATA && consumed < w.size()) {
            size_t off = consumed;
            size_t ate = 0;
            r = decoder.decode(w.data() + off, w.size() - off, &one_byte, 0, ate, decoded);
            consumed += ate;
        }
        // Output buffer too small for one sample.
        decoder.decode(w.data() + consumed, w.size() - consumed, &one_byte, 0, consumed, decoded);
        // Null input with a non-zero length: invalid-input guard. decode() returns
        // early here without writing bytes_consumed, so reset first rather than
        // carry a stale value.
        consumed = 0;
        decoded = 0;
        decoder.decode(nullptr, 4, &one_byte, sizeof(one_byte), consumed, decoded);
    }

    // Random blobs with planted RIFF/WAVE skeletons and chunk tags.
    const char* iter_env = std::getenv("FUZZ_ITERATIONS");
    const int kIterations = iter_env ? std::atoi(iter_env) : 200;
    for (int i = 0; i < kIterations; i++) {
        size_t len = 64 + (static_cast<size_t>(i) * 37) % (32 * 1024);
        std::vector<uint8_t> blob = build_random_blob(static_cast<uint32_t>(i) * 2654435761u, len);
        LLVMFuzzerTestOneInput(blob.data(), blob.size());
        if ((i + 1) % 200 == 0) {
            std::printf("[fuzz] %d/%d random iterations ok\n", i + 1, kIterations);
        }
    }

    std::printf("[fuzz] standalone torture complete, no sanitizer failures\n");
    return 0;
}

#endif  // FUZZ_STANDALONE
