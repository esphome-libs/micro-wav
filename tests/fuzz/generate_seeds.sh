#!/usr/bin/env bash
# Generate a seed corpus for fuzz_wav_decode.
#
# Outputs:
#   seeds_wav/   WAV files covering every supported sample format (PCM 8/16/24/
#                32-bit, IEEE float, A-law, mu-law), plus channel counts, sample
#                rates, WAVE_FORMAT_EXTENSIBLE, and content shapes.
#
# Requires: ffmpeg on PATH.

set -euo pipefail

here="$(cd "$(dirname "$0")" && pwd)"
cd "$here"

if ! command -v ffmpeg >/dev/null 2>&1; then
    echo "error: ffmpeg not on PATH" >&2
    exit 1
fi

rm -rf seeds_wav
mkdir -p seeds_wav

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

# Render an audio source (lavfi) to a WAV with an explicit sample codec.
#   $1 out.wav  $2 codec  $3 rate  $4 channels  $5 duration  $6 kind
gen() {
    local out=$1 codec=$2 rate=$3 chans=$4 dur=$5 kind=$6
    local filter
    case "$kind" in
        tone)    filter="sine=frequency=440:sample_rate=$rate:duration=$dur" ;;
        silence) filter="anullsrc=r=$rate:cl=mono:duration=$dur" ;;
        noise)   filter="anoisesrc=r=$rate:d=$dur:amplitude=0.5" ;;
        dc)      filter="aevalsrc=exprs=0.5:s=$rate:d=$dur" ;;
        impulse) filter="aevalsrc=exprs='if(eq(n,100),0.99,0)':s=$rate:d=$dur" ;;
        *) echo "unknown kind $kind" >&2; return 1 ;;
    esac
    ffmpeg -hide_banner -loglevel error -y -f lavfi -i "$filter" \
        -ac "$chans" -ar "$rate" -t "$dur" -c:a "$codec" "$out"
}

echo "[seeds] generating WAV variants..."

# Durations are deliberately tiny: the decoder is a byte-stream processor, so a
# few samples exercise the same paths as a long clip while keeping seeds small
# (smaller seeds mutate faster and keep -max_len low).

# One seed per supported sample format (the core decode paths).
gen seeds_wav/pcm_u8_mono_8000.wav      pcm_u8    8000  1 0.05 tone
gen seeds_wav/pcm_s16_stereo_44100.wav  pcm_s16le 44100 2 0.05 tone
gen seeds_wav/pcm_s24_mono_48000.wav    pcm_s24le 48000 1 0.05 tone
gen seeds_wav/pcm_s32_stereo_48000.wav  pcm_s32le 48000 2 0.05 tone
gen seeds_wav/float_f32_stereo_48000.wav pcm_f32le 48000 2 0.05 tone
gen seeds_wav/alaw_mono_8000.wav        pcm_alaw  8000  1 0.05 tone
gen seeds_wav/mulaw_mono_8000.wav       pcm_mulaw 8000  1 0.05 tone

# Sample-rate spread (16-bit stereo).
gen seeds_wav/pcm_s16_stereo_8000.wav   pcm_s16le 8000  2 0.05 tone
gen seeds_wav/pcm_s16_stereo_16000.wav  pcm_s16le 16000 2 0.05 tone
gen seeds_wav/pcm_s16_stereo_22050.wav  pcm_s16le 22050 2 0.05 tone
gen seeds_wav/pcm_s16_stereo_96000.wav  pcm_s16le 96000 2 0.05 tone

# Multichannel -> ffmpeg emits WAVE_FORMAT_EXTENSIBLE (exercises the SubFormat
# GUID parse + skip path).
gen seeds_wav/ext_pcm_s16_6ch_48000.wav pcm_s16le 48000 6 0.02 tone
gen seeds_wav/ext_pcm_s24_6ch_48000.wav pcm_s24le 48000 6 0.02 tone

# Content-shape seeds.
gen seeds_wav/pcm_s16_silence.wav  pcm_s16le 44100 2 0.1 silence
gen seeds_wav/pcm_s16_noise.wav    pcm_s16le 44100 2 0.1 noise
gen seeds_wav/pcm_s16_dc.wav       pcm_s16le 44100 2 0.1 dc
gen seeds_wav/pcm_s16_impulse.wav  pcm_s16le 44100 1 0.1 impulse

# Duration edge cases.
gen seeds_wav/pcm_s16_very_short.wav pcm_s16le 44100 2 0.02 tone
ffmpeg -hide_banner -loglevel error -y -f lavfi \
    -i "sine=frequency=440:sample_rate=44100" \
    -ac 2 -ar 44100 -t 0.003 -c:a pcm_s16le seeds_wav/pcm_s16_tiny.wav

# With metadata (writes a LIST/INFO chunk the parser must skip).
ffmpeg -hide_banner -loglevel error -y -f lavfi \
    -i "sine=frequency=440:sample_rate=44100:duration=0.05" \
    -ac 2 -ar 44100 -c:a pcm_s16le \
    -metadata title="Fuzz Seed" -metadata artist="microWAV" \
    seeds_wav/pcm_s16_with_metadata.wav

# ---------------------------------------------------------------------------
# Fuzzer config tails.
#
# fuzz_wav_decode reads its configuration from the BACK of each input
# (FuzzedDataProvider): one cfg byte, then up to 128 chunk/output control bytes.
# A bare .wav therefore loses ~129 bytes off its tail to those reads (a tiny seed
# loses most of itself). Appending a config tail keeps the ENTIRE .wav intact as
# decoder payload while still giving libFuzzer a mutable region for the chunk and
# output sizes. One variant sets cfg bit 0 so the reset()/replay path is seeded
# directly rather than found by mutation.
#
# cfg layout (matches the harness): bit0 = replay the stream across a reset().
# ---------------------------------------------------------------------------

# Emit one raw byte from a decimal value. Octal escape keeps this portable to
# macOS's stock bash 3.2 (whose printf lacks \xHH).
emit_byte() { printf "\\$(printf '%03o' "$1")"; }

# Append a config tail to a file: 128 neutral control bytes then the cfg byte.
# 128 control bytes == the harness MAX_CONTROL_BYTES, so every control byte comes
# from the pad and none is peeled off the real stream.
#   $1 file  $2 cfg
append_config_tail() {
    local f=$1 cfg=$2 i
    {
        for ((i=0;i<128;i++)); do emit_byte 32; done  # ~1 KiB chunks/buffers
        emit_byte "$cfg"
    } >> "$f"
}

echo "[seeds] appending fuzzer config tails"

# Snapshot the pristine bases before adding variants, so variants are not
# double-tailed by the pass below.
base_list="$tmp/base_seeds.txt"
find seeds_wav -maxdepth 1 -type f -name '*.wav' | sort > "$base_list"

# Replay variant, seeded off the canonical 16-bit stereo base.
cp seeds_wav/pcm_s16_stereo_44100.wav seeds_wav/cfg_replay.wav
append_config_tail seeds_wav/cfg_replay.wav 1

# Every pristine base: neutral single-pass tail (cfg=0) so the full .wav survives
# as payload while libFuzzer still gets a mutable control region.
while IFS= read -r f; do
    append_config_tail "$f" 0
done < "$base_list"

echo "[seeds] $(ls seeds_wav | wc -l | tr -d ' ') WAV seeds generated"
echo "[seeds] done"
