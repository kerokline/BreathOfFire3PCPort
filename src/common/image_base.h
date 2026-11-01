// The image's base and the helpers that turn one of Capcom's addresses
// into a place (docs/relocatable-image-design.md 2).
//
// Every address in our source is written as Capcom's address, a VA in the
// frame BOF3.exe was linked in: symbols.toml, the docs, the recipes and the
// logs all speak it. A VA is a NAME. It becomes a place only through one of
// these helpers:
//
//   Img(va)   a place in the image's data (.rdata, .data, .bss, .pool);
//   Code(va)  a place to call or jump to in the image's code.
//
// and a code word known only at run time - a VA or a word the game holds -
// becomes one through CodePlace(word).
//
// Stage B of the relocatable-image round (docs/relocatable-image-round.md 3)
// wrote every place through these helpers while they were consteval
// identities, so the machine code stayed the same (tools/codegen_same.py).
// Stage C makes ImageBase() a run-time value in both hosts (design 1.4): the
// helpers are inline functions of it, and a place is no longer a constant
// expression - a table of places is initialised at run time, and a VA kept
// for a constant context (a case label, a static_assert, an asm "i" operand)
// stays a name. Stage C1 moved the stub page with the image (mode S); stage
// C2 binds code words to our functions (mode B, design 4.4), and the stub
// page is a no-access trap: Code is the bind table's lookup in bof3x.exe.
//
// Header-only and free of Windows: the generated symbol header includes it,
// and so does the launcher (src/launcher/exe_image.cpp).
#pragma once

#include <cstdint>
#include <type_traits>

namespace bof3 {

namespace image {
// The base BOF3.exe was linked at: the frame every VA is written in, for
// ever. Not the base the image runs at - that is ImageBase(). The design
// (docs/relocatable-image-design.md 2.2) calls it kLinkedBase; it keeps the
// name it already had, so the source has one name for it (RA1's census
// found two already: this and exe_layout::kImageBase, the executable's
// header value). The generated header checks symbols.toml's image_base
// against it.
inline constexpr std::uint32_t kBase = 0x400000;
}  // namespace image

// Where the image is (design 1.4): a run-time value in both hosts, since one
// bof3x.dll serves both. Hosted it reads 0x400000 (BOF3.exe is fixed there);
// in the exe host, wherever bof3x.exe's header put it - 0x400000 for the
// player's executable, another base for a developer's (make_exe.py
// --image-base, the launcher's BOF3X_IMAGEBASE). It is the main module's
// base, what GetModuleHandle(nullptr) answers, read from the process
// environment block - x86: fs:0x30 holds the PEB, ImageBaseAddress at +8;
// x86-64: gs:0x60 holds the PEB, ImageBaseAddress at +0x10
// (docs/x64-build-design.md 8) - valid from before any DLL initialises, so
// also in the namespace-scope tables built from places that run in the DLL's
// CRT start-up, before DllMain - with no variable to set first and no test of
// one at each of the ~17,000 places. The asm is not volatile and touches no
// memory the compiler knows of, so a function reads it once (the value never
// changes in a process's life). A 32-bit value on every host: the image lies
// below 16 MB (host.cpp), and the 64-bit bof3x.exe is a fixed-base PE32+ the
// loader maps at 0x400000 or not at all (no .reloc, no DYNAMIC_BASE;
// x64-build-design.md 1), so the narrowing loses nothing. DetectHost checks
// it against GetModuleHandle(nullptr), the hosts' identities and the 16 MB
// window (host.cpp). The launcher, which includes this header through the
// generated one, never places an address.
[[gnu::always_inline]] inline std::uint32_t ImageBase() {
#if defined(__i386__)
    std::uint32_t base;
    asm("movl %%fs:0x30, %0\n\t"
        "movl 8(%0), %0"
        : "=r"(base));
    return base;
#elif defined(__x86_64__)
    std::uintptr_t peb;
    asm("movq %%gs:0x60, %0" : "=r"(peb));
    std::uintptr_t base;
    asm("movq 0x10(%1), %0" : "=r"(base) : "r"(peb));
    return static_cast<std::uint32_t>(base);   // checked by DetectHost (host.cpp)
#else
#error "ImageBase: no PEB read for this architecture (docs/x64-build-design.md 8)"
#endif
}

// A VA in the image's data, as a place: a 32-bit value on every host, since
// the image lies below 16 MB (docs/x64-build-design.md 8.1). The memory
// accessors widen it (zero-extend) where they dereference.
[[gnu::always_inline]] inline std::uint32_t Img(std::uint32_t va) { return ImageBase() + (va - image::kBase); }

// A place in the image as its VA, for a message: the logs, symbols.toml and
// the tools speak VAs (docs/relocatable-image-design.md 8, 9). The place is
// 32 bits wide by the same rule; a native pointer that may lie anywhere is
// narrowed by Low32() below, a Fatal at or above 4 GB.
[[gnu::always_inline]] inline std::uint32_t VaOf(std::uint32_t place) { return place - ImageBase() + image::kBase; }

// hook/log.h's Fatal, declared here so this header stays free of Windows (the
// generated symbol header and the launcher include it). Only Low32 and
// LowBlock name it, and only on the 64-bit build.
[[noreturn]] void Fatal(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// A native address as the 32-bit word the game's memory holds (docs/x64-
// build-design.md 2 and 8.1): on the 32-bit build the cast; on the 64-bit
// build a Fatal when the address lies at or above 4 GB - the loud trap for an
// address the low-memory design missed, named by the caller's file, line and
// function. The 64-bit process keeps everything the game can see below 4 GB by
// placement (a fixed-base executable: the main stack, the heap, every
// VirtualAlloc(nullptr), our DLL at 0x10000000), so a value that trips this is
// a defect of the design to be named, not a number to truncate.
//
// Two spellings, not one function with defaulted arguments: a
// __builtin_FUNCTION() default puts the caller's name in .rdata even where
// the branch that reads it is compiled out, and on i686 even an always-inline
// function of the cast moves blocks (host.cpp DetectHost: measured 7
// disassembly lines against the cast's 0), while the 32-bit build must stay
// byte for byte what it was (tools/codegen_same.py, which compares code and
// .rdata; the coordinator's call, 2026-11-01). So on i686 Low32 is the cast
// itself, a macro: write it unqualified, Low32(p), never ::bof3::Low32(p) and
// never as a function's address; p is evaluated once.
//
// On x86-64 it is a template over the pointer's type, not a function of
// const void*: what the game's words hold includes our functions (a handler
// table's cell, a stand-in a patch stores: `Low32(&OpenStandIn0)`), which
// never convert to a data pointer, and the volatile objects the helpers take
// (`Addr(const volatile void*)`). Anything that is not a pointer is refused
// at compile time: a word, a Va or a Slot32 is not an address to narrow.
#if UINTPTR_MAX > 0xFFFFFFFFu
template <class P>
[[gnu::always_inline]] inline std::uint32_t Low32(P p, const char* file = __builtin_FILE(), unsigned line = __builtin_LINE(),
                                                  const char* function = __builtin_FUNCTION()) {
    static_assert(std::is_pointer_v<P>, "Low32 narrows a pointer (an object's or a function's address), nothing else");
    const auto v = reinterpret_cast<std::uintptr_t>(p);
    if (v > 0xFFFFFFFFu)
        Fatal("%s:%u %s: %p lies above 4 GB, where no word of the game can hold it (docs/x64-build-design.md 2)", file,
              line, function, reinterpret_cast<const void*>(v));
    return static_cast<std::uint32_t>(v);
}
#else
#define Low32(p) (static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)))
#endif

// A heap block a word of the game may hold (docs/x64-build-design.md 5: the
// chokepoint, Crt_malloc and Crt_calloc in crt_rest.cpp): the block itself,
// checked as Low32 checks; the message names the size. Null passes - a
// refused allocation is the caller's to see, as today.
#if UINTPTR_MAX > 0xFFFFFFFFu
[[gnu::always_inline]] inline void* LowBlock(void* p, unsigned bytes, const char* function = __builtin_FUNCTION()) {
    if (reinterpret_cast<std::uintptr_t>(p) > 0xFFFFFFFFu)
        Fatal("%s: a heap block of %u bytes at %p lies above 4 GB, where no word of the game can hold it "
              "(docs/x64-build-design.md 5: the heap lies below 4 GB by the fixed-base executable's placement)",
              function, bytes, p);
    return p;
}
#else
[[gnu::always_inline]] inline void* LowBlock(void* p, unsigned) { return p; }
#endif

// The linked range of Capcom's code, 0x401000..0x5C4000 (BOF3.exe's .text to
// .rdata; .stub's extent in bof3x.exe, exe_layout.h).
namespace image {
inline constexpr std::uint32_t kCodeLo = 0x401000;
inline constexpr std::uint32_t kCodeHi = 0x5C4000;
}  // namespace image

// A VA in the image's code, as a place to call or to store where the game
// calls (docs/relocatable-image-design.md 4.4, mode B). Hosted, the VA itself:
// BOF3.exe's code is mapped there, and Inject's jmp at the start of each
// function of ours. In bof3x.exe, which maps no code of Capcom's, the VA's
// place in the bind table: an entry of a table of jumps, one per VA, to the
// function of ours symbols.toml's `impl` names (src/hook/bind.cpp) - a value
// that names its VA wherever the game copies it; a VA it does not name - a function Capcom's alone,
// a cell that is not a function - gives the trap, Img(va) on the no-access
// stub page, where a call faults and the crash reporter names the VA and its
// function. A table holds such cells legitimately (the MP3 decoder's
// functions, d3d_list's software handlers: never called by default), so this
// spelling does not stop the game for a miss; the call spellings below do.
// Valid from before DllMain: the namespace-scope tables built from places
// call it in the DLL's CRT start-up. Out of line, in src/hook/bind.cpp (the
// launcher, which includes this header through the generated one, never
// places an address, so it never needs the definition).
std::uint32_t Code(std::uint32_t va);

// The same at a call, where a VA with no function of ours behind it is a
// Fatal naming the VA, before the call (design 4.4): the call-through
// helpers (the four harnesses' *_AT and *_WORD).
std::uint32_t CodeToCall(std::uint32_t va);

// Whether a word read from the game's memory is a code word - a handler a
// .data table holds, checked before it is called (the eight dispatchers that
// refuse an index past their table, field_e1.cpp CodeAt and its kind). Hosted:
// a VA in BOF3.exe's .text. In bof3x.exe: one of our functions the bind
// table names - a bound word; the trap (an unbound cell's Img(va)) and data
// are not.
bool IsCodeWord(std::uint32_t word);

// A code word known only at run time, as a place to call (design 4.4: the
// call-through helpers' fix point). Two kinds of word reach it, and in our
// source they look alike: a code VA (a cell of one of our own constant tables
// picked by an index, a helper's parameter, a ternary of two constants), and a
// word the game holds (a cell of a .data step table, an operand read from the
// code), which is already a place - hosted Capcom's VA, its own place; in
// bof3x.exe a function of ours, bound by the loader or stored by our code.
//
// Told apart by the value: a word in the linked code range
// image::kCodeLo..kCodeHi is a VA and becomes Code(va); any other word is a
// place and passes through - in bof3x.exe a bound word, one of our functions
// in the DLL. That holds while no place of ours lies in the linked code range:
// at base 0x400000 the range is the image's own stub page, where the only
// words the game holds are the trap's (an unbound cell's Img(va), which reads
// as its VA and gives the trap again); a moved image lies wholly off the
// linked span 0x400000..0x98F000 (host.cpp refuses any other base), the
// developer launcher and the loader fence the span's free parts (launcher.cpp
// FenceLinkedSpan, exe_host.cpp Fence), and a module found in the linked code
// range is a Fatal.
[[gnu::always_inline]] inline std::uint32_t CodePlace(std::uint32_t word) {
    return word - image::kCodeLo < image::kCodeHi - image::kCodeLo ? Code(word) : word;
}

// CodePlace at a call: a VA with no function of ours is a Fatal (CodeToCall).
[[gnu::always_inline]] inline std::uint32_t CodePlaceToCall(std::uint32_t word) {
    return word - image::kCodeLo < image::kCodeHi - image::kCodeLo ? CodeToCall(word) : word;
}

// --- A VA as a type, bof3::image::Va (design 2.2; RB0's prototype) ----------
//
// A VA held in an image::Va is a name the compiler keeps apart from a place: it adds
// an offset (a Va), two of them subtract to a distance (an integer), and it
// does not convert to an integer. So a memory accessor given a Va places it
// through its Va overload, and a site that would use a VA as a value - store
// it, compare it with a place, pass it as an integer - does not compile until
// someone decides what it means: Img(va) for a place, va.v for the name.
//
// It lives in bof3::image, not bof3: gapwatch.cpp, snapshot.cpp and
// gfx_snapshot.cpp each have a function bof3::(anonymous)::Va (a pointer as
// an integer), which a bof3::Va would make ambiguous.
namespace image {
struct Va {
    std::uint32_t v;
};
constexpr Va operator+(Va a, std::int32_t n) { return Va{a.v + static_cast<std::uint32_t>(n)}; }
constexpr Va operator+(Va a, std::uint32_t n) { return Va{a.v + n}; }
constexpr Va operator-(Va a, std::int32_t n) { return Va{a.v - static_cast<std::uint32_t>(n)}; }
constexpr Va operator-(Va a, std::uint32_t n) { return Va{a.v - n}; }
constexpr std::int32_t operator-(Va a, Va b) { return static_cast<std::int32_t>(a.v - b.v); }
constexpr bool operator==(Va a, Va b) { return a.v == b.v; }
}  // namespace image

// --- A code VA as a type, bof3::image::CodeVa (design 4.4) -------------------
//
// The same shape as Va, for an address in Capcom's code: a name that adds an
// offset, subtracts to a distance, and does not convert to an integer. It is a
// separate type so neither is passed where the other is meant: a Va handed to
// a code place, or a CodeVa to a data accessor, does not compile (stage C2's
// reviewer relies on that). There is no Code(CodeVa) and no Img(CodeVa): a
// code constant is placed as ::bof3::Code(kX.v) - consteval in stage B, the
// only spelling that left the machine code as it was (wave 1, RB2 and RB4
// finding 1), the bind table's lookup since stage C2 - and named as kX.v (a
// log line, an Inject argument, a VA a test compares).
namespace image {
struct CodeVa {
    std::uint32_t v;
};
constexpr CodeVa operator+(CodeVa a, std::int32_t n) { return CodeVa{a.v + static_cast<std::uint32_t>(n)}; }
constexpr CodeVa operator+(CodeVa a, std::uint32_t n) { return CodeVa{a.v + n}; }
constexpr CodeVa operator-(CodeVa a, std::int32_t n) { return CodeVa{a.v - static_cast<std::uint32_t>(n)}; }
constexpr CodeVa operator-(CodeVa a, std::uint32_t n) { return CodeVa{a.v - n}; }
constexpr std::int32_t operator-(CodeVa a, CodeVa b) { return static_cast<std::int32_t>(a.v - b.v); }
constexpr bool operator==(CodeVa a, CodeVa b) { return a.v == b.v; }
}  // namespace image

// A Va as a place: a Va may be computed at run time (a table's VA plus a
// scaled index).
[[gnu::always_inline]] inline std::uint32_t Img(image::Va va) { return Img(va.v); }

}  // namespace bof3
