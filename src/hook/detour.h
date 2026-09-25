// The detour layer: redirect one original function to one of ours.
//
// Scaffolding, built to be dismantled (docs/PLAN.md section 2). It is a
// five-byte near jmp and nothing else - no trampoline, no disassembler -
// which is sound only because of two commitments made elsewhere:
//
//   * BOF3.exe is /FIXED at 0x400000 with no .reloc, so a literal address is
//     the function, every run (docs/PLAN.md section 1).
//   * A replaced function is replaced WHOLE. We never resume into the original
//     body, so the prologue bytes the jmp destroys never execute again
//     (CLAUDE.md rule 4 - no stubs, and no wrappers either).
//
// Technique as described in docs/prior-art/tr1x.md section 2.1; implemented
// from that description, not from their source.
#pragma once

#include <cstdint>

namespace bof3 {

// Refuses to continue unless the main module is the exact BOF3.exe that
// symbols.toml describes. Call once, before any Inject.
void VerifyImage();

// enabled:  original -> ours. Every caller in the game reaches our function.
// disabled: ours -> original. Every caller, including our own code, reaches
//           Capcom's function - original behaviour, same process, same loader.
//
// A function is disabled by listing its name in the BOF3X_ORIGINAL environment
// variable (comma or space separated), or all of them with BOF3X_ORIGINAL=*;
// `-NAME` takes one back out of `*` (NameListed in detour.cpp). Disabling
// writes the jmp over the start of OUR function, not Capcom's.
void Inject(const char* name, std::uint32_t original, void* ours);

// Re-aims ONE relative call inside an original function at ours, leaving the
// callee and its other callers alone: for a divergence that belongs to one
// call site of a function with many. Refuses unless `site` holds a CALL to
// `expected`. BOF3X_ORIGINAL=<name> leaves the site untouched - unless
// `instrument`: a probe that replaces nothing of Capcom's (the input recipe
// and recorder) is not game code, and `*` must not switch it off
// (docs/input-script.md section 5a).
void RetargetCall(const char* name, std::uint32_t site, std::uint32_t expected, void* ours,
                  bool instrument = false);

// Replaces `count` bytes of original code or data in place: for a divergence
// that is an operand - a constant the original pushes - and nothing else.
// Refuses unless the bytes there are `expected`. BOF3X_ORIGINAL=<name> leaves
// them alone.
void PatchBytes(const char* name, std::uint32_t at, const std::uint8_t* expected,
                const std::uint8_t* replacement, std::uint32_t count);

// A runnable byte-copy of an original function, for shadow-checking our
// replacement against it in the same process (BOF3X_SHADOW). Not a trampoline:
// nothing resumes into the original body. Sound only for a function whose
// every relative transfer stays inside [original, original + size) - calls
// through absolute or register addresses are fine - which the caller must
// have established from the disassembly. Call BEFORE Inject, which destroys
// the first five bytes. The copy lives in process memory only.
//
// A relative CALL - or a tail JMP - that does leave the range is named in
// `calls`: the offset of its E8 or E9 byte, and where the copy should call
// instead - null for "where the original called", or another clone, so that a
// cloned caller reaches the cloned callee and never ours. An entry that is itself a CALL or JMP (E8 or
// E9 at offset 0) is refused as a patch unless `calls` names offset 0; an
// int3 (CC) at the entry is always refused.
//
// `expected` is what the original calls there - the callee's address in the
// image - when the caller knows it: the copy is refused if the site reaches
// anything else, which is how a site the tracer or a RetargetCall has already
// re-aimed is caught instead of copied. 0 leaves it unchecked; a null target
// is then still refused if the site leaves the image. A null target that
// lands on an entry Inject has patched is logged, not refused: that callee is
// an earlier module's, checked against its own copy, and runs on both sides.
struct CloneCall {
    std::uint32_t offset;
    const void* target;
    std::uint32_t expected = 0;
};
void* CloneOriginal(const char* name, std::uint32_t original, std::uint32_t size,
                    const CloneCall* calls = nullptr, int n_calls = 0);

// True if `name` is listed in the BOF3X_SHADOW environment variable (the same
// list syntax as BOF3X_ORIGINAL).
bool WantsShadow(const char* name);

// True if this original address has been passed to Inject - in either
// direction, so the answer is the same with and without BOF3X_ORIGINAL. For
// tooling that must treat "a function we own" alike in both configurations.
bool IsOwned(std::uint32_t original);

// Summary line for the log once every module has registered.
void InjectReport();

}  // namespace bof3

// `name` is bound in bof3/symbols.gen.h: bof3::addr::name is the address,
// ::name is our function. Keep each line next to the code it installs, so an
// address and its implementation are reviewed in one diff (CLAUDE.md rule 3).
#define BOF3_INJECT(name) \
    ::bof3::Inject(#name, ::bof3::addr::name, reinterpret_cast<void*>(&::name))
