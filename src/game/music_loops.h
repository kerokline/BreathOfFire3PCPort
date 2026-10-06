// DIVERGENCE (ledger entry pending; docs/bgm-comparison.md sections 11 and 12):
// a looping music track loops inside its MP3 - from its measured loop end back
// to its measured loop start, sample-accurate - instead of rewinding to the
// file's first sample as the original does (which replays the intro, and joins
// the file's end to its lead-in). The positions are our own measurements of
// each PC file against a render of the disc's sequence (tools/bgm/
// measure_loops.py -> analysis/bgm/loops.json -> tools/bgm/gen_loop_table.py
// -> music_loops_table.inc); a track with no row rewinds as before. Armed
// after every module's self-test; BOF3X_MUSIC_LOOPS=0 keeps the rewind.
#pragma once

#include <cstdint>

namespace music_loops {

// One measured track: the file it was measured on (its size in bytes, so a
// different file of the same number is never looped by it) and the loop, in
// samples of the decoded stream from its first frame: at sample `end` the
// stream continues with sample `start`. `fade` (0 for most) crossfades the
// `fade` samples after `end` into the `fade` from `start` on - for a file a
// little short of one whole body, whose loop starts on intro material standing
// in for the body's missing tail; the generator puts both stretches inside the
// frames that hold `end` and `start`.
struct Row {
    unsigned track;
    std::uint32_t file_bytes;
    std::uint32_t start;
    std::uint32_t end;
    std::uint32_t fade;
};

// 0 until Arm has run (and with BOF3X_MUSIC_LOOPS=0): Music_Decode rewinds to
// the file's start as the original does.
extern unsigned char g_on;

// The row for `track` played from a file of `bytes` bytes, or nullptr.
const Row* Find(int track, std::uint32_t bytes);

// The table's rows (for the self-test and the log).
const Row* Rows(unsigned* count);

// After every module's self-test: reads BOF3X_MUSIC_LOOPS (unset or 1 on, 0
// off; anything else is fatal) and sets g_on.
void Arm();

}  // namespace music_loops
