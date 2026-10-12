// rdata_consts.h's copies against BOF3.exe's .rdata (docs/exe-import-engine.md section 2).
#include "game/rdata_consts.h"

#define DIRECTINPUT_VERSION 0x0700
#include <windows.h>
#include <dinput.h>

#include <cstring>

#include "hook/exe_host.h"
#include "hook/host.h"
#include "hook/log.h"

namespace rdata {

static_assert(sizeof kEntries / sizeof kEntries[0] == 44, "40 literals and 4 GUIDs: the doc's count");
static_assert(kLo == kEntries[0].pc, "kLo is the first entry");

consteval bool Sorted() {
    std::uint32_t end = 0;
    for (const Entry& e : kEntries) {
        if (e.pc < end || e.pc % 4 || (e.size == 8 && (e.pc - kLo) % 8) || e.pc + e.size > kHi) return false;
        end = e.pc + e.size;
    }
    return true;
}
static_assert(Sorted(), "entries sorted, aligned, not overlapping, inside [kLo, kHi)");

namespace {

constexpr std::uint32_t kKeyboardFormat = 0x5C4948;   // DInput_KeyboardFormat, Capcom's c_dfDIKeyboard

unsigned char* At(std::uint32_t va) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(va)); }

// `n` bytes of BOF3.exe at `va`: the mapped image hosted, the file in the exe host.
void Theirs(std::uint32_t va, void* to, std::uint32_t n) {
    if (bof3::IsExeHost()) bof3::ExeFile_Read(va, to, n);
    else std::memcpy(to, At(va), n);
}

// c_dfDIKeyboard against Capcom's: the header's five counts, then each
// object's GUID (by value), offset, type and flags.
void VerifyKeyboardFormat() {
    const DIDATAFORMAT& ours = c_dfDIKeyboard;
    DIDATAFORMAT theirs;
    static_assert(sizeof theirs == 0x18, "DIDATAFORMAT is 24 bytes on i686");
    Theirs(kKeyboardFormat, &theirs, sizeof theirs);
    if (ours.dwSize != theirs.dwSize || ours.dwObjSize != theirs.dwObjSize || ours.dwFlags != theirs.dwFlags ||
        ours.dwDataSize != theirs.dwDataSize || ours.dwNumObjs != theirs.dwNumObjs)
        bof3::Fatal("rdata: c_dfDIKeyboard's header (0x%lX 0x%lX 0x%lX 0x%lX %lu) is not BOF3.exe's at 0x%06X "
                    "(0x%lX 0x%lX 0x%lX 0x%lX %lu)", ours.dwSize, ours.dwObjSize, ours.dwFlags, ours.dwDataSize,
                    ours.dwNumObjs, (unsigned)kKeyboardFormat, theirs.dwSize, theirs.dwObjSize, theirs.dwFlags,
                    theirs.dwDataSize, theirs.dwNumObjs);
    const auto table = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(theirs.rgodf));
    for (DWORD i = 0; i < ours.dwNumObjs; ++i) {
        DIOBJECTDATAFORMAT obj;
        static_assert(sizeof obj == 16, "DIOBJECTDATAFORMAT is 16 bytes on i686");
        Theirs(table + 16 * i, &obj, sizeof obj);
        const DIOBJECTDATAFORMAT& mine = ours.rgodf[i];
        GUID guid{};
        const auto at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(obj.pguid));
        if (at) Theirs(at, &guid, sizeof guid);
        const bool guid_same =
            (at == 0) == (mine.pguid == nullptr) && (!at || std::memcmp(&guid, mine.pguid, sizeof guid) == 0);
        if (!guid_same || obj.dwOfs != mine.dwOfs || obj.dwType != mine.dwType || obj.dwFlags != mine.dwFlags)
            bof3::Fatal("rdata: c_dfDIKeyboard's object %lu (offset %lu, type 0x%lX, flags 0x%lX) is not BOF3.exe's at "
                        "0x%06X (offset %lu, type 0x%lX, flags 0x%lX, the GUID %s)", i, mine.dwOfs, mine.dwType,
                        mine.dwFlags, (unsigned)(table + 16 * i), obj.dwOfs, obj.dwType, obj.dwFlags,
                        guid_same ? "equal" : "different");
    }
    bof3::Log("rdata: the keyboard format is the SDK's c_dfDIKeyboard, equal to BOF3.exe's at 0x%06X (its %lu objects "
              "at 0x%06X, their GUIDs by value)", (unsigned)kKeyboardFormat, ours.dwNumObjs, (unsigned)table);
}

}  // namespace

void Verify() {
    if (!bof3::ExeFile_Present()) {
        bof3::Log("rdata: %u constants (%s .. %s) and the keyboard format held by the engine; no BOF3.exe to compare "
                  "them with", (unsigned)(sizeof kEntries / sizeof kEntries[0]), kEntries[0].what,
                  kEntries[sizeof kEntries / sizeof kEntries[0] - 1].what);
        return;
    }
    const auto* ours = reinterpret_cast<const unsigned char*>(kCopies.data());
    for (const Entry& e : kEntries) {
        unsigned char theirs[16];
        Theirs(e.pc, theirs, e.size);
        if (std::memcmp(ours + (e.pc - kLo), theirs, e.size) != 0)
            bof3::Fatal("rdata: 0x%06X (%s): the engine's copy is not BOF3.exe's .rdata", (unsigned)e.pc, e.what);
    }
    bof3::Log("rdata: %u constants (%s .. %s) held by the engine, each equal to BOF3.exe's .rdata%s",
              (unsigned)(sizeof kEntries / sizeof kEntries[0]), kEntries[0].what,
              kEntries[sizeof kEntries / sizeof kEntries[0] - 1].what, bof3::IsExeHost() ? " (the file's)" : "");
    VerifyKeyboardFormat();
}

void Map() {
    if (!bof3::IsExeHost()) bof3::Fatal("rdata::Map in the hosted build: Capcom's .rdata is the loader's there");
    const auto* ours = reinterpret_cast<const unsigned char*>(kCopies.data());
    for (const Entry& e : kEntries) std::memcpy(At(e.pc), ours + (e.pc - kLo), e.size);
    std::memcpy(At(kKeyboardFormat), &c_dfDIKeyboard, sizeof c_dfDIKeyboard);
    bof3::Log("rdata: .rdata the engine's - %u constants at their addresses and c_dfDIKeyboard's header at 0x%06X (its "
              "objects the SDK's, at %p); the rest zero", (unsigned)(sizeof kEntries / sizeof kEntries[0]),
              (unsigned)kKeyboardFormat, static_cast<const void*>(c_dfDIKeyboard.rgodf));
}

const ::_DIDATAFORMAT* KeyboardFormat() { return &c_dfDIKeyboard; }

}  // namespace rdata
