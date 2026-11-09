// Two spell overlays compiled into the exe, round nine group S30
// (docs/magic_s30.md): the PSX's MAGIC130 and MAGIC131.EMI, Magic_Rows rows
// 141 and 144. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Venom and KaiserBreath; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC130 0x4E4420..0x4E4C45: a kind-2 task at the side's centre, one
//     sprite child (a sprite that shades up, holds and shades down) and six
//     ring children (a gouraud fan and band round a screen point), CLUT row
//     26 given its stp bit; three of its functions are shared with MAGIC126
//     and MAGIC129, and its count-down 0x4E47F0 with eight overlays;
//   - MAGIC131 0x4E4C50..0x4E6944: a kind-2 task of eight steps that loads a
//     file by 0x904B89, hides the party's sprites, reloads the party set's
//     files and shows the party again; four kinds of kind-1 child (a sprite
//     that glides in and out with a cycling CLUT, a full-screen flash, a
//     textured pillar, a textured ring) and a pool of 48 motes of its own
//     (0x6A4E38, 0x2C bytes each) drawn as gouraud fans; its end 0x4E5200 is
//     shared with eighteen overlays.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack or .data dispatch table aborts where the original would call through
// whatever follows it (docs/magic_fx_reached.md section 3, the precedent), and
// Kaiser_LoadSetFile aborts where the original reads a file index through a
// null table entry (docs/magic_s30.md section 8).
#include "game/magic_s30.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: sixteen bytes at
// 0x903850 (words; Scratch_Swap is +0xC) and the four SVECTORs of
// Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).
// The dword the sprite children point at a sprite bank while their phase
// runs (0x8E3580 / 0x813580), and put back to 0x8B3580 after.
// kSpriteBank: now BattleE2_AnimSet (symbols.toml).
// MAGIC131's mote pool: 48 records of 0x2C bytes, and the cell holding the
// mote being run (KaiserMote_Pool, KaiserMote_Current).
// kMotePool: now KaiserMote_Pool (symbols.toml).
constexpr std::uint32_t kMoteStride = 0x2C;
constexpr unsigned kMotes = 48;
// kMoteCurrent: now KaiserMote_Current (symbols.toml).
// The battle bytes MAGIC131 reads beyond the harness's: 0x904AAC (the
// battle's facing, battle region), 0x904B89 (read by the engine as the
// form index, docs/magic_lib.md), 0x90412C (the loaded party set,
// docs/field-event.md), and the table of file-index tables by 0x904B89.
constexpr ::bof3::image::Va kFacing{0x904AAC};

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

short S16(const unsigned char* a) { return static_cast<short>(Word(a)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
void AddW(unsigned char* a, unsigned v) { SetWord(a, (Word(a) + v) & 0xFFFF); }
void AddL(unsigned char* a, std::int32_t v) {
    SetLong(a, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a)) + static_cast<std::uint32_t>(v)));
}
// A data item's address (symbols.toml's macros are typed pointers).
std::uint32_t Addr(const void* p) { return Low32(p); }
std::int32_t PtrValue(const void* p) {
    return static_cast<std::int32_t>(Low32(p));
}

// `imul` then `sar 0xC` (or `sar 3`): the 32-bit product wraps, the shift is
// arithmetic.
int MulSar(int a, int b, int by) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> by;
}
int Mul12(int a, int b) { return MulSar(a, b, 12); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* a, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(a, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
unsigned char* Mote(unsigned i) { return Mem(::bof3::Img(KaiserMote_Pool_addr) + i * kMoteStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }
int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }
int Cos(int angle) { return MH_CALL(Math_Cos)(angle); }
void DrawMode(unsigned char* prim, unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(prim, 0, 1, tpage, 0); }
void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

// This group's functions called directly, as the originals call them: by
// address, which in the game is the jmp Inject put there (or Capcom's code
// under BOF3X_ORIGINAL) and in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S30_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp4(unsigned char* prim) {
    long depth, flag;
    S30_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 0x18, prim + 0x28, prim + 0x38,
                                     &depth, &flag);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A .data dispatch table read in place (the fuzz swaps its entries for
// recorders), the index checked against the table's length.
magic_harness::Handler Entry(const char* who, std::uint32_t table, unsigned entries, unsigned index) {
    if (index >= entries) PastTable(who, index, entries);
    return reinterpret_cast<magic_harness::Handler>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Gfx_ClutStrip / Gfx_ClutStripSource word indices: row 26 (0x812980 /
// 0x80E980), row 1 (0x80F980 / 0x80B980) and row 1 + 0x21 (0x80F9C2).
constexpr unsigned kRow26 = 0x1A00, kRow1 = 0x200;

// The sprite-control fields both sprite children's starts write, in their
// order (no call between: the order is the originals' for reading only).
void SpriteFields(unsigned char c25, unsigned char c27, unsigned char shade, unsigned char c5c) {
    Sc()[0x25] = c25;
    Sc()[0x26] = 0;
    Sc()[0x27] = c27;
    Sc()[0x28] = 1;
    Sc()[0x24] = 0x80;
    rv::Store(rv::Current<records::BattleTask>()->scale_x, 0x10000);
    rv::Store(rv::Current<records::BattleTask>()->scale_y, 0x10000);
    Sc()[0x48] = 0;
    Sc()[0] = static_cast<unsigned char>(Sc()[0] | 0x20);
    Sc()[0x5C] = c5c;
    Sc()[0x5D] = shade;
    Sc()[0x5E] = shade;
    Sc()[0x5F] = shade;
}

}  // namespace

#define S30_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC130 (row 141, Venom read one id down)

// ===========================================================================
// MAGIC131 (row 144, KaiserBreath read one id down)

void MagicS30_Inject() {
    if (bof3::WantsShadow("magic_s30")) magic_s30::SelfTest();
}
