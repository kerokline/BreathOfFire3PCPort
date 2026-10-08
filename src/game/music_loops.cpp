// The measured music loops (music_loops.h). The table is generated from
// analysis/bgm/loops.json by tools/bgm/gen_loop_table.py: sample positions we
// measured in the PC's MP3s, not bytes of Capcom's.
#include "game/music_loops.h"

#include <windows.h>

#include "hook/log.h"

namespace music_loops {

unsigned char g_on = 0;

namespace {

const Row kRows[] = {
#include "game/music_loops_table.inc"
};

}  // namespace

const Row* Rows(unsigned* count) {
    *count = sizeof kRows / sizeof kRows[0];
    return kRows;
}

const Row* Find(int track, std::uint32_t bytes) {
    for (const Row& r : kRows)
        if (static_cast<int>(r.track) == track && r.file_bytes == bytes) return &r;
    return nullptr;
}

void Arm() {
    char text[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_MUSIC_LOOPS", text, sizeof text);
    if (n > 1 || (n == 1 && text[0] != '0' && text[0] != '1')) bof3::Fatal("BOF3X_MUSIC_LOOPS must be 0 or 1");
    if (n == 1 && text[0] == '0') {
        bof3::Log("music_loops off (BOF3X_MUSIC_LOOPS=0): every looping track rewinds to its file's start");
        return;
    }
    for (const Row& r : kRows)
        if (r.start >= r.end) bof3::Fatal("music_loops: track %u's row starts at %u, not before its end %u", r.track, r.start, r.end);
    g_on = 1;
    bof3::Log("music_loops %u tracks loop at their measured points; the others rewind to the file's start "
              "(BOF3X_MUSIC_LOOPS=0 for the rewind everywhere)",
              static_cast<unsigned>(sizeof kRows / sizeof kRows[0]));
}

}  // namespace music_loops
