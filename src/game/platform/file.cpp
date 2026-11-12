// Moved here by tools/move_funcs.py from file_io.cpp, shell.cpp (docs/source-layout-round.md).
#include "game/platform/file.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/shell.h"
#include "game/shell_callees.h"
#include "hook/detour.h"
#include "hook/exe_host.h"
#include "hook/host.h"
#include "hook/log.h"
#include "input/bindings.h"
#include "os/os.h"

using namespace shell;

namespace shell {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

const char* Str(U address) { return reinterpret_cast<const char*>(static_cast<std::uintptr_t>(address)); }

}  // namespace

}  // namespace shell

// original 0x5A72C0; sole caller WinMain (ours), with CAPCOM.AVI and BOF3.EXE.
// The local file opens: closed again, the root emptied (byte 0 of
// File_CdRootBuf to 0, so File_Open's paths are relative), 1. Otherwise each
// drive letter C..L in turn goes into byte 0 of the root "C:\", and on a
// CD-ROM drive (GetDriveTypeA, read from its import slot once, before the
// loop) the disc file is tried at root + name: found, closed, 1, the root
// left naming the drive. None: 0, the root left at L. The path is built in a
// 0x50-byte buffer as the original's; WinMain's two names fit it.
extern "C" int __cdecl Disc_Probe(const char* local_file, const char* disc_file) {
    void* const local = g.open(local_file, Str(::bof3::Img(kModeRb.v)));
    if (local != nullptr) {
        g.close(local);
        File_CdRootBuf[0] = 0;
        return 1;
    }
    std::uint32_t drive_type_word;   // the import slot is a 32-bit word (the function, or its thunk on x86-64)
    std::memcpy(&drive_type_word, At(g.drive_type_slot), sizeof drive_type_word);
    const auto drive_type = reinterpret_cast<DriveTypeFn>(static_cast<std::uintptr_t>(drive_type_word));
    char path[0x50];
    for (U i = 0; i < 10; ++i) {
        File_CdRootBuf[0] = static_cast<unsigned char>('C' + i);
        if (drive_type(Str(::bof3::Img(File_CdRootBuf_addr))) != kDriveCdrom) continue;
        g.format(path, Str(::bof3::Img(kRootFormat.v)), Str(::bof3::Img(File_CdRootBuf_addr)), disc_file);
        void* const disc = g.open(path, Str(::bof3::Img(kModeRb.v)));
        if (disc != nullptr) {
            g.close(disc);
            return 1;
        }
    }
    return 0;
}

// original 0x5A7370, two instructions. The drive-root prefix, "C:" and a
// separator as shipped. Its only writer is 0x5A72C0, which is not ours.
extern "C" char* __cdecl File_CdRoot(void) {
    return File_CdRootBuf;
}

// original 0x5A7380. Opens `path` for binary reading; if that fails, retries
// with the drive-root prefix (File_CdRoot) in front. Returns the slot, or -1
// when the open failed or all 16 slots are in use. The second and third
// arguments are pushed by every caller and read by nobody.
//
// One thing the original does that is kept, and two that are not:
//   - DIV-0087: the prefixed path is built with sprintf into a 0x50-byte
//     stack buffer with no length check. Unreachable with the shipped file
//     names, but the music cache's paths are absolute and may be long
//     (music_seq.cpp): a path whose prefixed form would not fit is not
//     retried, and the open fails. A retry could not succeed for such a path
//     anyway - File_CdRoot in front of an absolute path names no file.
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
        if (std::strlen(File_CdRoot()) + std::strlen(path) < sizeof prefixed) {  // DIV-0087
            Crt_sprintf(prefixed, "%s%s", File_CdRoot(), path);
            stream = Crt_fopen(prefixed, "rb");
        }
        if (!stream) {
            bof3::Log("open  t=%lu  FAILED  %s", static_cast<unsigned long>(bof3::os::TickMs32()), path);
            return -1;
        }
    }
    File_Slots[slot] = stream;
    bof3::Log("open  t=%lu  slot %d  %s", static_cast<unsigned long>(bof3::os::TickMs32()), slot, path);
    return slot;
}

// original 0x5A7420. The write-side opener: same slot scan as File_Open, mode
// "wb", no drive-root retry. Sole caller Save_WriteFile.
//
// DIVERGENCE DIV-0003: the original stores the fopen result in the slot and
// returns the slot index without testing it, so a failed open (a read-only
// game directory) is reported as success and the File_Write that follows
// hands a null stream to the CRT, which dereferences it. Ours returns -1 and
// claims no slot, as File_Open does. Both call sites of Save_WriteFile
// already test for -1.
extern "C" int __cdecl File_OpenWrite(const char* path) {
    int slot = 0;
    while (slot < static_cast<int>(File_Slots_count) && File_Slots[slot]) ++slot;
    if (slot == static_cast<int>(File_Slots_count)) return -1;

    void* stream = Crt_fopen(path, "wb");
    if (!stream) {  // DIV-0003
        bof3::Log("openw t=%lu  FAILED  %s", static_cast<unsigned long>(bof3::os::TickMs32()), path);
        return -1;
    }
    File_Slots[slot] = stream;
    return slot;
}

// original 0x5A7470. No bounds check on `handle` and no check that the slot is
// open: the original has neither, and its five callers only pass what File_Open
// returned after testing it against -1.
extern "C" unsigned __cdecl File_Read(int handle, void* dst, unsigned size) {
    BOF3_LOG_FIRST_CALL("File_Read(handle=%d, size=%u)", handle, size);
    return Crt_fread(dst, 1, size, File_Slots[handle]);
}

// original 0x5A74A0. File_Read's twin; same absence of checks.
extern "C" unsigned __cdecl File_Write(int handle, const void* src, unsigned size) {
    BOF3_LOG_FIRST_CALL("File_Write(handle=%d, size=%u)", handle, size);
    return Crt_fwrite(src, 1, size, File_Slots[handle]);
}

// original 0x5A74D0.
extern "C" int __cdecl File_Size(int handle) {
    return Crt_filelength(Crt_fileno(File_Slots[handle]));
}

// original 0x5A74F0. Always from the start of the file: the original passes a
// constant 0 (SEEK_SET) as whence.
extern "C" int __cdecl File_Seek(int handle, int offset) {
    BOF3_LOG_FIRST_CALL("File_Seek(handle=%d, offset=%d)", handle, offset);
    return Crt_fseek(File_Slots[handle], offset, 0);
}

// original 0x5A7510. The fclose result is discarded, and the slot is freed
// whatever it was.
extern "C" void __cdecl File_Close(int handle) {
    Crt_fclose(File_Slots[handle]);
    File_Slots[handle] = nullptr;
}

void File_Inject() {
    BOF3_INJECT(Disc_Probe);
    BOF3_INJECT(File_CdRoot);
    BOF3_INJECT(File_Open);
    BOF3_INJECT(File_OpenWrite);
    BOF3_INJECT(File_Read);
    BOF3_INJECT(File_Write);
    BOF3_INJECT(File_Size);
    BOF3_INJECT(File_Seek);
    BOF3_INJECT(File_Close);
}
