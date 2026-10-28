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
// Stage B of the relocatable-image round (docs/relocatable-image-round.md 3):
// the base is 0x400000, so Img and Code are the identity, and the machine code
// must be the same as with the bare literal (tools/codegen_same.py). For that
// they are consteval, not constexpr: the front end then writes the folded
// address into the IR, as it does for a literal. A constexpr call is folded
// only once the optimiser inlines it, after the early passes have already run
// on a call - measured on this branch, 2026-10-28: the generated header
// spelled through constexpr helpers changed the code of 18 objects
// (mode_tasks.cpp's Title_DrawLogo the first: a different register
// allocation), through consteval ones none. Stage C makes ImageBase() a
// run-time value in both hosts (design 1.4) and Code() the bound function
// (design 4.4); the names stay, and the consteval goes with that one decision.
// Meanwhile a VA computed at run time is not an argument here.
//
// Header-only and free of Windows: the generated symbol header includes it,
// and so does the launcher (src/launcher/exe_image.cpp).
#pragma once

#include <cstdint>

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

// Where the image is. A function rather than a constant so the name survives
// stage C, where it is read once at run time (design 2.2). The literal is
// image::kBase's value: in stage B the image runs where it was linked.
consteval std::uint32_t ImageBase() { return 0x400000; }

// A VA in the image's data, as a place.
consteval std::uint32_t Img(std::uint32_t va) { return ImageBase() + (va - image::kBase); }

// A VA in the image's code, as a place to call. The identity in stage B.
consteval std::uint32_t Code(std::uint32_t va) { return va; }

// A code word known only at run time, as a place to call (design 4.4: the
// call-through helpers' fix point). Two kinds of word reach it, and in our
// source they look alike: a code VA (a cell of one of our own constant tables
// picked by an index, a helper's parameter, a ternary of two constants), and a
// word the game holds (a cell of a .data step table, an operand read from the
// code), which is already a place - stage C binds those cells to our functions
// (mode B) or rebases them (mode S). Code() cannot take either: it is consteval.
//
// Stage B: the identity. Stage C tells the two apart by the value: a word in
// the linked code range 0x401000..0x5C4000 is a VA and becomes Code(va) at run
// time (mode B: the bound function, a Fatal naming the VA on a miss; mode S:
// its place in the moved stub page); any other word is a place and passes
// through. That holds while no place of ours lies in the linked code range:
// at base 0x400000 the range is the image's own stub page, and at a forced
// base the fence reserves it (design 1.5), which the loader must check.
//
// constexpr and always_inline so it leaves no call behind (the gate,
// tools/codegen_same.py); a constant argument belongs in Code(), not here.
[[gnu::always_inline]] constexpr std::uint32_t CodePlace(std::uint32_t word) { return word; }

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
// code constant is placed as ::bof3::Code(kX.v), consteval, the only spelling
// that leaves the machine code as it was (wave 1, RB2 and RB4 finding 1), and
// named as kX.v (a log line, an Inject argument, a VA a test compares).
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

// A Va as a place. constexpr, not consteval: a Va may be computed at run time
// (a table's VA plus a scaled index).
constexpr std::uint32_t Img(image::Va va) { return ImageBase() + (va.v - image::kBase); }

}  // namespace bof3
