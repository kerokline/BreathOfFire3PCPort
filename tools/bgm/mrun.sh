#!/bin/bash
# mrun.sh SECONDS OUT.wav CUE [extra mednafen args...]
# Runs the sibling's Mednafen (1.32.1, GPL-2.0, used as a tool, nothing of it vendored)
# for SECONDS with a base directory of its own under $BGM_SCRATCH (so the owner's
# Mednafen settings are never touched), recording its sound output (-soundrecord)
# at 44.1 kHz through DirectSound with the volume at 0 (the recording is taken
# before the volume). The timeout is wall-clock from the process's launch and
# Mednafen starts up before the recorder opens - 0.3 s on a quiet machine, up
# to 9 s with three workers copying discs at once (measured over 153 renders,
# 2026-10-08) - so a WAV holds that much less than SECONDS of audio;
# measure_loops.render_plan pads 15 s. A killed run leaves the WAV header's sizes unset; wavread.py
# reads it anyway. The BIOS (SCPH5500.BIN for psx-jp) is copied from the sibling's
# mednafen/firmware into the base directory once. Mednafen writes stdout.txt and
# stderr.txt beside its exe (the sibling's mednafen/), as it always does.
#   e.g.  python patch_disc.py 0 "$BGM_SCRATCH/disc000" 0xD1
#         bash mrun.sh 330 song000.wav "$BGM_SCRATCH/disc000/bof3jp.cue"
# Paths as every music tool finds them (bgm_paths.py: the sibling beside the main checkout, the
# scratch under its analysis/bgm/); BOF3_SIBLING and BGM_SCRATCH override, as there.
HERE=$(cd "$(dirname "$0")" && pwd)
SIB=${BOF3_SIBLING:-$(python "$HERE/bgm_paths.py" SIB)}
SCR=${BGM_SCRATCH:-$(python "$HERE/bgm_paths.py" SCRATCH)}
[ -n "$SIB" ] && [ -n "$SCR" ] || { echo "mrun.sh: no paths from bgm_paths.py" >&2; exit 2; }
mkdir -p "$SCR/mdfn_home/firmware"
[ -f "$SCR/mdfn_home/firmware/SCPH5500.BIN" ] || cp "$SIB/mednafen/firmware/SCPH5500.BIN" "$SCR/mdfn_home/firmware/"
export MEDNAFEN_HOME=$(cygpath -w "$SCR/mdfn_home")
secs=$1; out=$2; cue=$3; shift 3
timeout "$secs" "$SIB/mednafen/mednafen.exe" -sound.volume 0 -sound.driver dsound "$@" \
    -sound.rate 44100 -soundrecord "$out" "$cue"
echo "exit $? (124 = stopped by the timeout, as intended)"
ls -la "$out"
