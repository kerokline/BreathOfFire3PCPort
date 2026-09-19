// The file layer: a 16-slot table of CRT streams addressed by slot index. The
// slot index is the "handle" the asset loaders pass around.
//
// The streams in File_Slots belong to the MSVC6 CRT linked into BOF3.exe, not
// to ours. Until the whole layer is ours, anything that touches one goes
// through the original CRT (Crt_*), never <cstdio>.
#include "game/file_io.h"

#include <windows.h>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// original 0x5A7380. Opens `path` for binary reading; if that fails, retries
// with the drive-root prefix (File_CdRoot) in front. Returns the slot, or -1
// when the open failed or all 16 slots are in use. The second and third
// arguments are pushed by every caller and read by nobody.
//
// Two things the original does that are kept, and one that is not:
//   - kept: the prefixed path is built with sprintf into a 0x50-byte stack
//     buffer with no length check. Unreachable with the shipped file names.
//   - kept: no slot is claimed until the open succeeds.
//   - not reproduced: the original's scan reads one dword past the table
//     (slot 16) before its bound check notices. The value read never changes
//     the result - 16 taken slots return -1 either way - so this is not an
//     observable difference and is not ledgered.
//
// The log line per open is ours. It changes nothing the game can observe; it
// is how docs/attract-mode.md knows which files a scene loads.
extern "C" int __cdecl File_Open(const char* path, int, int) {
    int slot = 0;
    while (slot < static_cast<int>(File_Slots_count) && File_Slots[slot]) ++slot;
    if (slot == static_cast<int>(File_Slots_count)) return -1;

    void* stream = Crt_fopen(path, "rb");
    if (!stream) {
        char prefixed[0x50];
        Crt_sprintf(prefixed, "%s%s", File_CdRoot(), path);
        stream = Crt_fopen(prefixed, "rb");
        if (!stream) {
            bof3::Log("open  t=%lu  FAILED  %s", GetTickCount(), path);
            return -1;
        }
    }
    File_Slots[slot] = stream;
    bof3::Log("open  t=%lu  slot %d  %s", GetTickCount(), slot, path);
    return slot;
}

// original 0x5A7470. No bounds check on `handle` and no check that the slot is
// open: the original has neither, and its five callers only pass what File_Open
// returned after testing it against -1.
extern "C" unsigned __cdecl File_Read(int handle, void* dst, unsigned size) {
    BOF3_LOG_FIRST_CALL("File_Read(handle=%d, size=%u)", handle, size);
    return Crt_fread(dst, 1, size, File_Slots[handle]);
}

void FileIo_Inject() {
    BOF3_INJECT(File_Open);
    BOF3_INJECT(File_Read);
}
