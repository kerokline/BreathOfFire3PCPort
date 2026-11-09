// Two spell overlays compiled into the exe, round nine group S33
// (docs/magic_s33.md): the PSX's MAGIC151 and MAGIC154.EMI, Magic_Rows rows
// 86 and 35. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Accession and Mighty Chop; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC151 0x4EAE70..0x4ED0B1: a banner with the name of the kind in
//     0x904B89, the acting member's script run, a controller child (kind 1,
//     0x44) that raises an orb (a dome of flat quads and a glow of gouraud
//     quads), sparks (a line fan), a bolt (three band rows) and two rings;
//     meanwhile a DAT file chosen by 0x904B89 and the party set is loaded and
//     the acting member's sprite rebuilt - one way (entry 0 of the task's
//     second table) or, when the phase starts at 1, by copying the acting
//     member's record over party record 0, clearing members 1 and 2, and
//     rebuilding the battle windows;
//   - MAGIC154 0x4ED0C0..0x4ED66E: the acting actor's animation 0xC, a copy of
//     its record (kind 1, 0x52) that plays its script, and six blades (the
//     same kind, phase 0) thrown from the source sprite, each a sprite that
//     grows and shrinks.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s33.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

constexpr ::bof3::image::Va kWindowRows{0x803337};    // the battle windows 0xD.. (Window_Alloc's records +3, stride 0x24)

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return Low32(p); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }
// A signed byte widened (movsx).
int SignedByte(unsigned char b) { return static_cast<signed char>(b); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S33_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four projected points at +8, +8 + step, +8 + 2 step, +8 + 3 step
// (0xC a flat quad's, 0x10 a gouraud one's).
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S33_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

// The owner's position +0x34 / +0x38 / +0x3C to the task's.
void AtOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// A new kind-1 child (0x44) owned by this task with phase `phase`, counted in
// +0xB; the child's slot.
unsigned char* Child44(unsigned char phase) {
    const unsigned slot = NewTask(0x44);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(s));
    child[1] = phase;
    return child;
}

}  // namespace

// ===========================================================================
// MAGIC151 (row 86, Accession read one id down)

// ===========================================================================
// MAGIC154 (row 35, Mighty Chop read one id down)

void MagicS33_Inject() {
    if (bof3::WantsShadow("magic_s33")) magic_s33::SelfTest();
}
