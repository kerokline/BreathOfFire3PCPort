// The words of the span that name something by where it lies, and the tests
// that find them: shared by the state hash, which hashes such a word by what
// it names (docs/state-hash.md sections 7 and 8), and the quicksave, which
// writes it by what it names at a take and puts it back at a load
// (docs/state-by-name-format.md section 3). One text of each test, so that
// the hash and the quicksave cannot disagree about which words they are.
//
// Moved here from statehash.cpp unchanged (the state-by-name round's group
// A1, 2026-11-25): the fold (Fold, Fold24 and the extent), our module's
// sections a pointer can name (OurImage, statehash.cpp's ReadDllImage and
// Named), and the run-time slots the battle's messages fill. What the hash
// computes did not change: the same functions over the same values.
//
// New here, for the quicksave: Capcom's pointer slots as the image held them
// before the injects (BeforeInjects), the census the quicksave keeps a text
// pointer of ours by.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include "common/image_base.h"
#include "hook/exe_layout.h"
#include "hook/log.h"
#include "os/os.h"

namespace bof3::span_words {

// BOF3.exe's image from its PE header: ImageBase 0x400000, SizeOfImage
// 0x53F000 (VerifyImage has checked the timestamp before any of this runs).
// A word of .data holding a value in it when the loader has mapped the image
// is a pointer slot of Capcom's: a table entry, a record's string pointer.
constexpr std::uint32_t kExeLo = 0x400000;
constexpr std::uint32_t kExeHi = 0x93F000;
// A folded range's extent (statehash.cpp's comment): the image to the end of
// .pool, which bof3x.exe carries after Capcom's range (exe_layout.h: .pool
// begins at kExeHi) and which moves with the image - 0x587000, as host.cpp's
// fence and input_script.cpp's AsVa reckon the image. Recorded in the state
// hash's header (BOF3SH5); tools/statehash.py reads the same constants and
// asserts the value. No static_assert here: tools/reloc_census.py counts each
// comparison of an image address as a site, and the layout is checked where
// it lives.
constexpr std::uint32_t kImageExtent = exe_layout::kPoolVa + exe_layout::kPoolSize - exe_layout::kImageBase;

// .data as the state hash reads it: BOF3.exe's .data section, 0x5DA000 to the
// end of its last page (statehash.cpp kBase, kEnd). The words the tests below
// were measured on (the 227 raw dumps of docs/state-hash.md 8) lie here.
constexpr std::uint32_t kDataLo = 0x5DA000;
constexpr std::uint32_t kDataHi = 0x93E000;

// A value in a folded range, as it would read with the image at 0x400000.
inline bool Fold(const std::uint32_t* folds, unsigned n, std::uint32_t v, std::uint32_t* out) {
    for (unsigned i = 0; i < n; ++i)
        if (v - folds[i] < kImageExtent) {
            *out = kExeLo + (v - folds[i]);
            return true;
        }
    return false;
}

// A word whose low 24 bits lie in a folded range (an ordering-table link into
// the moved image, its length byte on top), as it would read with the image
// at 0x400000: those 24 bits folded, the top byte kept. Tried only when the
// whole word is not folded.
inline bool Fold24(const std::uint32_t* folds, unsigned n, std::uint32_t v, std::uint32_t* out) {
    std::uint32_t c;
    if ((v & 0xFF000000u) == 0 || !Fold(folds, n, v & 0xFFFFFFu, &c) || c > 0xFFFFFFu) return false;
    *out = (v & 0xFF000000u) | c;
    return true;
}

// Words Capcom's code fills at run time with Msg_SystemPtr's result, zero
// when the census runs and so not slots of his: the battle message ring
// 0x93C2C4 (16 entries of 8 bytes, +0 the text pointer; battle_actor_copies.cpp
// queues the item under the cursor's description there) and the battle
// result's current message 0x93B8E4 (battle_result.cpp). Under a language
// overlay the pointer names MsgPool_Relocate's static pool in our DLL
// (DIV-0007), at the same offset in every build and a different address:
// two builds of ours on the combat route differed on those two pages from
// the first battle message on (docs/state-hash.md section 7, measured 3,
// 2026-10-11), so they join the slots. Without an overlay they hold a
// pointer into MessagePools in BOF3.exe, hashed raw as before.
constexpr std::uint32_t kMsgRing = 0x93C2C4;
constexpr std::uint32_t kMsgRingEntries = 16;
constexpr std::uint32_t kMsgRingStride = 8;
constexpr std::uint32_t kMsgCurrent = 0x93B8E4;
constexpr std::uint32_t kRuntimeSlots = kMsgRingEntries + 1;
// The run-time slot i (0 .. kRuntimeSlots - 1) as a VA.
constexpr std::uint32_t RuntimeSlot(std::uint32_t i) { return i < kMsgRingEntries ? kMsgRing + i * kMsgRingStride : kMsgCurrent; }

// A section of ours a pointer can name: every one the loader keeps. The
// discardable ones (.reloc and the DWARF sections, 41 of the image's 94 MB in
// the build of 2026-10-10) are mapped but nothing points into them, and a
// language overlay's text written in place over one of Capcom's slots can
// look like an address there (docs/state-hash.md section 7: "Para" at
// 0x65D1D0), so a value inside one is hashed raw.
struct Section {
    std::uint32_t lo, hi;
    bool content;   // readable and not executable: hashed by its bytes
};

// Our module's image as the loader mapped it: its whole extent and the
// sections a pointer can name (statehash.cpp's ReadDllImage and Named until
// 2026-11-25). Read once, by whichever asks first; the state hash and the
// quicksave read the same module, so the second read would read the same.
struct OurImage {
    bool read = false;
    std::uint32_t lo = 0, hi = 0;               // the whole image, for the census
    std::uint32_t named_lo = 0, named_hi = 0;   // the span of sections, a quick reject
    std::vector<Section> sections;

    void Read(void* module) {
        if (read) return;
        // The loader's image of the module (os/module.h): the PE headers on
        // Windows - every section but the discardable ones - the PT_LOAD
        // segments on POSIX. `module` is the module's handle, its base, on
        // Windows; any address inside it will do.
        os::ModuleImageInfo image;
        if (!os::ModuleImage(module, &image))
            Fatal("statehash: our module at %p has no image headers the loader mapped", module);
        const auto base = Low32(reinterpret_cast<const void*>(image.base));
        lo = base;
        hi = base + static_cast<std::uint32_t>(image.extent);
        named_lo = hi;
        named_hi = lo;
        for (const os::ModulePart& s : image.parts) {
            const bool content = s.readable && !s.executable;
            const std::uint32_t slo = base + static_cast<std::uint32_t>(s.lo - image.base),
                                shi = slo + static_cast<std::uint32_t>(s.hi - s.lo);
            sections.push_back({slo, shi, content});
            if (slo < named_lo) named_lo = slo;
            if (shi > named_hi) named_hi = shi;
        }
        if (sections.empty()) Fatal("statehash: our module at 0x%08lX has no section a pointer can name", (unsigned long)base);
        read = true;
    }

    bool InImage(std::uint32_t v) const { return v >= lo && v < hi; }

    // The section of ours a value names, or null.
    const Section* Named(std::uint32_t v) const {
        if (v < named_lo || v >= named_hi) return nullptr;
        for (const Section& s : sections)
            if (v >= s.lo && v < s.hi) return &s;
        return nullptr;
    }
};

inline OurImage g_ours;

// Capcom's pointer slots for the quicksave: the byte offsets from kDataLo of
// the aligned words of .data whose value, before the injects, lay in BOF3.exe's
// image at its place (Img(0x400000) .. Img(0x93F000)), ascending - the state
// hash's census test (statehash.cpp InImage) without its folds of another
// base, which the quicksave does not fold across (docs/state-by-name-format.md
// 3.2). Filled by BeforeInjects; empty before it.
inline std::vector<std::uint32_t> g_slots;
inline bool g_slots_taken = false;

// Called by StateHash_BeforeInjects in every host, after the hash's own
// self-test and before InjectAll, whatever BOF3X_STATEHASH says: our image
// read, and the slots listed while they still hold Capcom's values (a
// language overlay's inject re-aims some at text of ours).
inline void BeforeInjects(void* module) {
    g_ours.Read(module);
    g_slots.clear();
    const std::uint32_t data = Img(kDataLo), lo = Img(kExeLo), size = kExeHi - kExeLo;
    for (std::uint32_t off = 0; off < kDataHi - kDataLo; off += 4) {
        std::uint32_t w;
        std::memcpy(&w, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(data + off)), 4);
        if (w - lo < size) g_slots.push_back(off);
    }
    g_slots_taken = true;
    Log("quicksave   %lu aligned words of .data hold a pointer into BOF3.exe's image before the injects: Capcom's "
        "pointer slots, where a text pointer of ours is kept as the process has it (span_words.h)",
        (unsigned long)g_slots.size());
}

// Whether the byte offset `off` from kDataLo is one of g_slots.
inline bool IsSlot(std::uint32_t off) { return std::binary_search(g_slots.begin(), g_slots.end(), off); }

}  // namespace bof3::span_words
