# tools/bgm/host - host builds of the music code

The SPU model (`src/audio/spu.cpp`) built with the system compiler, and its
unit tests. Nothing here needs game data or the llvm-mingw toolchain: run
`cmake -S tools/bgm/host -B /tmp/spu_build && cmake --build /tmp/spu_build && /tmp/spu_build/spu_tests`
from the repository root (CMake 3.20 or later, any C++17 compiler; the tests
use `fork` to check the aborts, so a POSIX host). The run prints one line per
test group, a few measured numbers (the noise period, the reverb echo
positions, each reverb preset's tail), and exits non-zero on any failure.
What the tests establish, and the readings of the hardware description they
pin down, is in [`docs/spu-model.md`](../../../docs/spu-model.md).

## The player and the render check

The same build makes `seq_tests` (the sequencer on a synthetic song and bank: the VSync carry across render calls, the loop period in VSyncs, note2pitch, the aborts) and `synth_render` (`src/audio/seq.cpp`, `song.cpp`: the
sequencer of [`docs/libsnd-reading.md`](../../../docs/libsnd-reading.md) over
the SPU model), which plays one song of the importer's cache to a WAV:

    synth_render --cache <cache root> --song N --out songN.wav --seconds 120

(`--volume` / `--frames` are Music_Play's crescendo, default the title's 100
over 8; `--trace` prints the VSync of each loop jump; `--ticks FILE` what each
VSync's flush wrote; `--solo V` one voice's stem; `--phase`, `--offsets` the
VSync timing experiments of libsnd-reading.md section 9). The cache and the
WAVs are game data: scratch only.

`tools/bgm/synth_check.py` renders every song that has a Mednafen render
(`/workspace/scratch/renders`, then `renders_cloud` where `.done`), aligns,
and prints / writes the table of libsnd-reading.md 9.1; `--oracle` adds the
score with each key-on VSync moved to the render's timing.
