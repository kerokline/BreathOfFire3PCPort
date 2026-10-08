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
