# microWAV fuzzer

libFuzzer harness for the WAV decoder.

- `fuzz_wav_decode` drives `WAVDecoder::decode()` with raw WAV container bytes in
  variably-sized chunks. Exercises RIFF/WAVE chunk walking, fmt-chunk parsing
  (standard and `WAVE_FORMAT_EXTENSIBLE`), unknown-chunk skipping, and every
  audio decode path (PCM 8/16/24/32-bit, G.711 A-law/mu-law, IEEE float 32-bit).

## What the harness varies

`WAVDecoder` takes no constructor options. It is default-constructed and reused
via `reset()`, so there is no downmix / CRC / channel-select machinery to steer
(the main difference from a codec harness). The interesting per-call dimensions
are the two buffers the *caller* owns, and the harness varies both on every
`decode()`:

- **Input chunk size** (1..8161 bytes): small chunks split a single sample across
  calls, exercising the 4-byte partial-sample accumulator (`buf_`) and header
  field staging across byte boundaries.
- **Output buffer size** (1 byte..8161 bytes, floored at one sample once the
  header is known): small buffers cap how many samples fit, exercising the
  output-available clamp and the multi-call drain of one input chunk. Each call
  gets a **freshly allocated, exactly-sized** output buffer so AddressSanitizer
  red-zones any over-write immediately.

Both control streams are consumed from the **tail** of the input.
`FuzzedDataProvider` integral reads come off the back, so the **payload prefix is
preserved**: only the trailing config/control bytes are peeled off, leaving the
front of a real `.wav` seed intact. For a bare `.wav` fed directly (no config
tail), those reads come out of its final bytes, so its tail is truncated while
the rest decodes as-is; the generated seeds avoid this by appending a config tail
(see "Seed corpus" below).

- One config byte: bit 0 replays the whole stream a second time across a
  `reset()`, exercising the re-stream path (state reset, second `HEADER_READY`).
  The remaining bits are reserved.
- An exhausted provider reads 0, i.e. the default: single pass. Tiny inputs
  therefore behave as a plain one-shot decode.

## Tier 1 oracle

On every decode the harness asserts a set of **structural invariants**
(single-decode, no reference decoder needed). A violation aborts, surfacing it
like any sanitizer finding:

- `bytes_consumed` never exceeds the chunk handed to `decode()`.
- Once the header is ready: the output sample width is 1..4 bytes, the reported
  bit depth equals 8x that width, the channel count and sample rate are both
  positive, and the format is a recognized tag (never `WAV_FORMAT_UNKNOWN`).
- The core memory-safety bound: `samples_decoded * bytes_per_output_sample`
  never exceeds the output buffer size.
- `WAV_DECODER_SUCCESS` always implies at least one decoded sample.

## Requirements

- A Clang with the libFuzzer runtime.
  - **macOS:** `brew install llvm`: Apple's stock clang omits the libFuzzer
    runtime, so the Homebrew build is required.
  - **Linux:** the system `clang++` already ships libFuzzer; no extra install.
- ffmpeg on `PATH` for corpus generation.

The build commands below use `$CLANGXX` for the compiler. Point it at the right
Clang for your platform:

```sh
export CLANGXX=$(brew --prefix llvm)/bin/clang++   # macOS / Homebrew LLVM
export CLANGXX=clang++                             # Linux / system clang
```

## Build

```sh
cd tests/fuzz
cmake -B build-libfuzzer -DCMAKE_CXX_COMPILER="$CLANGXX" .
cmake --build build-libfuzzer
```

For crash reproducers without libFuzzer:

```sh
cmake -B build-standalone -DFUZZ_USE_LIBFUZZER=OFF -DCMAKE_CXX_COMPILER="$CLANGXX" .
cmake --build build-standalone
./build-standalone/fuzz_wav_decode path/to/crashing.wav
```

With no arguments the standalone binary runs a torture battery (empty/truncated
RIFF captures, a header claiming a huge data size, a valid stream in every
supported format, the null-input and output-too-small guard paths, and random
blobs with planted RIFF/WAVE skeletons). It also has a single-seed mutation mode:

```sh
./build-standalone/fuzz_wav_decode -mutate path/to/seed.wav   # FUZZ_ITERATIONS=N
```

## Seed corpus

```sh
./generate_seeds.sh           # creates seeds_wav/
mkdir -p corpus_wav
cp seeds_wav/* corpus_wav/
```

`generate_seeds.sh` renders one seed per supported sample format plus channel,
sample-rate, and content-shape variants (multichannel files come out as
`WAVE_FORMAT_EXTENSIBLE`, and the metadata seed carries a `LIST`/`INFO` chunk the
parser must skip).

Each generated seed gets a **config tail** appended (see "What the harness
varies" above): because the harness consumes its config/control bytes from the
back of the input, a bare `.wav` would lose its tail to those reads. The tail is
128 neutral control bytes plus one `cfg` byte, so the whole `.wav` survives as
decoder payload while libFuzzer still has a mutable region to vary the chunk and
output sizes. One `cfg_replay.wav` variant pre-sets the replay bit so that path
is seeded directly rather than found by mutation.

Both directories are local-only (see `.gitignore`): `seeds_wav/` and `corpus_wav/`
are ignored so libFuzzer can grow the corpus without polluting `git status`.
Regenerate seeds any time with `./generate_seeds.sh`.

### Growing the corpus

`-merge=1` keeps only inputs that add new coverage against *this* harness, so any
external pile of `.wav` files can be folded in safely:

```sh
./build-libfuzzer/fuzz_wav_decode -merge=1 -max_len=65536 corpus_wav/ /path/to/more-wavs/
```

If the merge encounters an input that crashes, libFuzzer writes `crash-<sha>` to
the cwd, restarts, and continues, so the merge is safe to run on a dirty corpus.

## Run

```sh
./build-libfuzzer/fuzz_wav_decode -dict=wav.dict corpus_wav/
```

`wav.dict` lists the RIFF/chunk tags, format tags, sample rates, bit depths, and
the `WAVE_FORMAT_EXTENSIBLE` SubFormat GUID, so libFuzzer can splice them in
directly.

Useful flags: `-max_total_time=60`, `-jobs=4`, `-workers=4`, `-max_len=65536`,
`-rss_limit_mb=4096`.

### Why the harness sets `detect_container_overflow=0`

The harness defines `__asan_default_options()` to turn off ASan's
container-overflow check. This works around a false positive that appears when
linking against a **prebuilt** libFuzzer runtime (observed on macOS with Homebrew
LLVM): that runtime is compiled without libc++ container annotations, but this
harness instantiates `std::vector<uint8_t>`, the same type libFuzzer's dictionary
parser uses internally, but *with* annotations under `-fsanitize=address`.
The two instantiations are ODR-merged at link time, and the mismatched poisoning
makes libFuzzer's own `ParseOneDictionaryEntry` abort while loading a `-dict`
file. (A one-line dict `"\xFF\xFF\xFF\xFF"` reproduces the original abort; the
stack is entirely in libFuzzer/libc++, never the decoder.)

It is a benign annotation mismatch, not a real overflow and not a libFuzzer bug:
on a toolchain that builds the libFuzzer runtime and this code against the same
libc++ configuration (typical Linux `clang++`), it never occurs. Disabling the
one check loses no real coverage here: the decoder under test uses no STL
containers (raw arrays only), and the harness allocates its output buffers
exactly sized, so over-writes still land in ASan's heap red zone. The rest of
ASan and all of UBSan stay active, and `ASAN_OPTIONS` still overrides the default
(`ASAN_OPTIONS=detect_container_overflow=1` re-enables it).

## Sanitizers

The build enables ASan and the full UBSan check set with `-fno-sanitize-recover`,
so every finding is fatal. Unlike a fixed-point DSP, the WAV decoder uses only
unsigned shifts, bounded integer math, and clamped float-to-int conversion, so it
is UBSan-clean and no checks are suppressed.

## Corpus coverage

To see which functions in `src/` and `include/` the saved corpus exercises:

```sh
./coverage.sh           # per-function report on stdout
./coverage.sh --html    # also write cov-html/ for line-by-line browsing
```

The script builds a separate `build-cov/` with clang source-based coverage
instrumentation, replays `corpus_wav/` once via libFuzzer's `-runs=0` mode, and
renders the report with `llvm-cov`. Functions at 0% are codepaths the corpus
isn't reaching, candidates for new seeds or dict entries.

## When a crash is found

1. libFuzzer drops `crash-<sha>` in the current directory.
2. Minimize: `./build-libfuzzer/fuzz_wav_decode -minimize_crash=1 -runs=10000 crash-<sha>`.
3. Reproduce under the standalone binary for cleaner stack traces.
4. Keep the reproducer in `crashes/`; once the fix lands and the input no longer
   reproduces, move it to `crashes/fixed_verified/`. Crash inputs are local-only
   (the `crash-*` gitignore pattern keeps them out of the tree). Replay them
   after decoder changes for regression cover:

   ```sh
   ./build-libfuzzer/fuzz_wav_decode -runs=0 crashes/
   ```
