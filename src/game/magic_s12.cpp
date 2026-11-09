// Two spell overlays compiled into the exe, round nine group S12
// (docs/magic_s12.md): the PSX's MAGIC060 and MAGIC062.EMI, Magic_Rows rows
// 39 and 95. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Identify and Celerity; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC060 0x4B0D50..0x4B1CAA: a roll by the actor's and the target's
//     bytes, remembered as a bit per enemy kind (0x9040A8); three children
//     (kind 1, 0x12) - the screen dimmed, a grey disc grown on screen, the
//     target's tint pulsed - and a panel of text drawn over them: the
//     target's name, two numbers, six element glyphs and two item names for
//     an enemy it identified, question marks otherwise, first for 16 frames,
//     then until a key is pressed. The children count the parent's +0xB down
//     to 0x80 as they end.
//   - MAGIC062 0x4B1CB0..0x4B2F34: MAGIC082's buff (group S18's Buff_*) with
//     its own colours: a ring child and four sparks (kind 1, 0x4A), the
//     target's tint faded, four stats moved by the ability's step
//     (Celerity_ApplyStat) with a popup child each (kind 1, 0x48).
//
// Every call goes through the harness (MH_CALL / MH_AT, the .data tables read
// in place), so the start-up fuzz can stand recorders in for ours as for the
// originals' copies. No divergence: each is a faithful replacement, except
// that an index past a dispatch table aborts where the original would call
// through the bytes after it (docs/magic_fx_reached.md section 3, the
// precedent), and CeleritySpark_Draw aborts where the original would store a
// depth outside its four-entry stack array (group S18's BuffSpike_Draw, the
// same code).
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/text_advance.h"
#include "game/magic_s12.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Handler;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::SetLong;
using move_script::Word;

// Celerity_Start's CLUT strip: 16 words at +0 and 16 at +0x20, copied over.
// kClutFrom: now Gfx_ClutStripSource[0x1a00] (symbols.toml).
// kClutTo: now Gfx_ClutStrip[0x1a00] (symbols.toml).

unsigned char* SC() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned char* Slot(unsigned k) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (k & 0xFFu) * at::kTaskStride); }
std::int32_t Ptr(const void* p) { return static_cast<std::int32_t>(Low32(p)); }
std::uint32_t U32(const void* p) { return Low32(p); }
void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

// The records as the originals index them, unchecked: a member by its index
// (stride 0x14C), an enemy by its index (stride 0x128) - the battle index
// less 3, which a party index sends below the enemy records.
unsigned char* PartyRec(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
unsigned char* EnemyRec(int i) { return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(i) * at::kEnemyStride); }

// The scratch words SharedScratch + k and the vertex scratch
// Prim_VertexScratch + k (SVECTORs at +0, +8, +0x10, +0x18).
constexpr ::bof3::image::Va kDs{SharedScratch_addr};
unsigned char DsByte(unsigned k) { return Mem(::bof3::Img(kDs.v) + k)[0]; }

// `imul` (a 32-bit product that wraps) then `sar n`; `shl l` then `sar r`.
int MulSar(int a, int b, int n) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

// A .data dispatch table read in place: the cell called, the index checked
// (the originals' is not).
void CallCell(const unsigned long* table, unsigned entries, unsigned index, const char* who) {
    if (index >= entries) bof3::Fatal("%s: index %u, past the %u-entry table", who, index, entries);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// A stack table's handler by the phase +1.
void CallPhase(const std::uint32_t* phases, unsigned entries, const char* who) {
    const unsigned phase = SC()[1];
    if (phase >= entries) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
    magic_harness::Phase(phases[phase])();
}

// This group's functions called directly, by their addresses as the originals
// call them: in the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }

// A draw-mode packet (tpage `tpage`, dithered) committed to `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

const unsigned char* DrawAt(int x, int y, int count, const unsigned char* text) {
    return MH_CALL(Text_DrawAt)(x, y, 0, count, text);
}
unsigned CharCount(const unsigned char* text) { return MH_CALL(Text_CharCount)(text); }
void DrawCentredCounted(const unsigned char* text, int y) {
    const unsigned n = CharCount(text) & 0xFFu;
    DrawAt(0xA0 - 6 * static_cast<int>(n), y, static_cast<int>(n), text);
}

}  // namespace

#define MS12_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC060 (row 39): the task, the roll, the panel

// ===========================================================================
// MAGIC060's children (kind 1, parameter 0x12)

// ===========================================================================
// MAGIC062 (row 95): the task, the ring and the four sparks

void MagicS12_Inject() {
    if (bof3::WantsShadow("magic_s12")) magic_s12::SelfTest();
}
