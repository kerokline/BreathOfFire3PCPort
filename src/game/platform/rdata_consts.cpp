// rdata_consts.h's copies against BOF3.exe's .rdata (docs/exe-import-engine.md section 2).
#include "game/rdata_consts.h"

#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>

#include <cstring>

#include "common/image_base.h"
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

constexpr ::bof3::image::Va kKeyboardFormat{0x5C4948};   // DInput_KeyboardFormat, Capcom's c_dfDIKeyboard

unsigned char* At(std::uint32_t va) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(va)); }

// `n` bytes of BOF3.exe at `va`: the mapped image hosted, the file in the exe host.
void Theirs(std::uint32_t va, void* to, std::uint32_t n) {
    if (bof3::IsExeHost()) bof3::ExeFile_Read(va, to, n);
    else std::memcpy(to, At(va), n);
}

// BOF3.exe's bytes of a DIDATAFORMAT and of a DIOBJECTDATAFORMAT: the 32-bit
// image's layouts, where a pointer (rgodf, pguid) is a 4-byte word. The
// SDK's structs have that shape on i686 alone (on x86-64 they are 32 and 24
// bytes), so Capcom's bytes are read into these, as words, on every width.
struct ImageDataFormat {
    std::uint32_t size, obj_size, flags, data_size, num_objs;
    std::uint32_t rgodf;   // the object table's VA
};
static_assert(sizeof(ImageDataFormat) == 0x18, "DIDATAFORMAT is 24 bytes in the 32-bit image");
struct ImageObjectFormat {
    std::uint32_t pguid;   // the GUID's VA, or 0
    std::uint32_t ofs, type, flags;
};
static_assert(sizeof(ImageObjectFormat) == 16, "DIOBJECTDATAFORMAT is 16 bytes in the 32-bit image");

// The SDK's c_dfDIKeyboard in the 32-bit image's layout: the two struct
// sizes the image's (0x18 and 0x10 - the SDK's own on i686; 0x20 and 0x18
// on x86-64, where the same format is described with 8-byte pointers), the
// three counts the SDK's, the object table's place as a 32-bit word (the
// SDK's table in our DLL, below 4 GB on every width; the game never reads
// the word - pad_read.cpp passes KeyboardFormat() to DirectInput).
ImageDataFormat KeyboardFormatAsImage() {
    const DIDATAFORMAT& ours = c_dfDIKeyboard;
    return ImageDataFormat{sizeof(ImageDataFormat), sizeof(ImageObjectFormat), ours.dwFlags, ours.dwDataSize,
                           ours.dwNumObjs, Low32(ours.rgodf)};
}

// c_dfDIKeyboard against Capcom's: the header's five counts (the two sizes
// in the image's layout), then each object's GUID (by value), offset, type
// and flags.
void VerifyKeyboardFormat() {
    const DIDATAFORMAT& ours = c_dfDIKeyboard;
    const ImageDataFormat mine = KeyboardFormatAsImage();
    ImageDataFormat theirs;
    Theirs(kKeyboardFormat.v, &theirs, sizeof theirs);
    if (mine.size != theirs.size || mine.obj_size != theirs.obj_size || mine.flags != theirs.flags ||
        mine.data_size != theirs.data_size || mine.num_objs != theirs.num_objs)
        bof3::Fatal("rdata: c_dfDIKeyboard's header (0x%lX 0x%lX 0x%lX 0x%lX %lu) is not BOF3.exe's at 0x%06X "
                    "(0x%lX 0x%lX 0x%lX 0x%lX %lu)", static_cast<unsigned long>(mine.size),
                    static_cast<unsigned long>(mine.obj_size), static_cast<unsigned long>(mine.flags),
                    static_cast<unsigned long>(mine.data_size), static_cast<unsigned long>(mine.num_objs),
                    kKeyboardFormat.v, static_cast<unsigned long>(theirs.size),
                    static_cast<unsigned long>(theirs.obj_size), static_cast<unsigned long>(theirs.flags),
                    static_cast<unsigned long>(theirs.data_size), static_cast<unsigned long>(theirs.num_objs));
    const std::uint32_t table = theirs.rgodf;
    for (DWORD i = 0; i < ours.dwNumObjs; ++i) {
        ImageObjectFormat obj;
        Theirs(table + 16 * i, &obj, sizeof obj);
        const DIOBJECTDATAFORMAT& mine = ours.rgodf[i];
        GUID guid{};
        const std::uint32_t at = obj.pguid;
        if (at) Theirs(at, &guid, sizeof guid);
        const bool guid_same =
            (at == 0) == (mine.pguid == nullptr) && (!at || std::memcmp(&guid, mine.pguid, sizeof guid) == 0);
        if (!guid_same || obj.ofs != mine.dwOfs || obj.type != mine.dwType || obj.flags != mine.dwFlags)
            bof3::Fatal("rdata: c_dfDIKeyboard's object %lu (offset %lu, type 0x%lX, flags 0x%lX) is not BOF3.exe's at "
                        "0x%06X (offset %lu, type 0x%lX, flags 0x%lX, the GUID %s)", i, mine.dwOfs, mine.dwType,
                        mine.dwFlags, (unsigned)(table + 16 * i), static_cast<unsigned long>(obj.ofs),
                        static_cast<unsigned long>(obj.type), static_cast<unsigned long>(obj.flags),
                        guid_same ? "equal" : "different");
    }
    bof3::Log("rdata: the keyboard format is the SDK's c_dfDIKeyboard, equal to BOF3.exe's at 0x%06X (its %lu objects "
              "at 0x%06X, their GUIDs by value)", kKeyboardFormat.v, ours.dwNumObjs, (unsigned)table);
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
    for (const Entry& e : kEntries) std::memcpy(At(::bof3::Img(e.pc)), ours + (e.pc - kLo), e.size);   // VA placed
    const ImageDataFormat keyboard = KeyboardFormatAsImage();   // the 32-bit layout, on every width
    std::memcpy(At(::bof3::Img(kKeyboardFormat.v)), &keyboard, sizeof keyboard);
    bof3::Log("rdata: .rdata the engine's - %u constants at their addresses and c_dfDIKeyboard's header at 0x%06X (its "
              "objects the SDK's, at %p); the rest zero", (unsigned)(sizeof kEntries / sizeof kEntries[0]),
              kKeyboardFormat.v, static_cast<const void*>(c_dfDIKeyboard.rgodf));
}

const ::_DIDATAFORMAT* KeyboardFormat() { return &c_dfDIKeyboard; }

}  // namespace rdata
