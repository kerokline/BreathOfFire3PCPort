// Four spell overlays compiled into the exe, round nine group S07
// (docs/magic_s07.md): the PSX's MAGIC021, MAGIC038, MAGIC039 and
// MAGIC040.EMI, Magic_Rows rows 33, 121, 78 and 79. Read one id down
// (docs/cut-content.md section 2) the sibling labels them Bonebreak, War
// Shout, Focus / Meditation and Enlighten; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC021 0x4A3B20..0x4A4436: a copy of the acting actor's record as a
//     child (kind 1, 9) whose script runs, then twelve motes from a pool of
//     its own (64 records at 0x679B40, walked by the kind-2 task) that fly out
//     from the source sprite drawing a gouraud fan and ring each, and a screen
//     shade (MAGIC008's 0x4A29C0);
//   - MAGIC038 0x4A4440..0x4A4C06: sixteen motes (kind 1, 0x59) that circle
//     the side's centre, then one buff popup per living actor of the target's
//     side (MagicFx_BuffPopup);
//   - MAGIC039 0x4A4C10..0x4A5A06: an aura (kind 1, 0x3B) that draws a ring of
//     64 projected quads and a disc of 16 projected triangles and tints the
//     source sprite, and eight rising motes; its shades by the ability id (0xA3
//     the second of its two ids);
//   - MAGIC040 0x4A5A10..0x4A6434: rays and a ring (kind 1, 0x3C) at the
//     actor the target byte names, then the buff (MagicFx_ApplyBuff) and its
//     popup task.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s07.h"

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

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// kFormIndex: now BattlePhases_B89 (symbols.toml). u8: the index a party record with +0x134 bit 1 uses

// This group's .data (symbols.toml [[data]]).
// kMotePool: now BonebreakMote_Pool (symbols.toml). BonebreakMote_Pool: 64 records of 0x84 bytes
// kWarShoutAnimations: now WarShoutMote_Animations (symbols.toml). 16 bytes
// kDiscShades: now FocusAura_DiscShades (symbols.toml). FocusAura_DiscShades: 2 x 3 bytes
// kMoteShades: now FocusMote_Shades (symbols.toml). FocusMote_Shades
// kRingShades: now FocusAura_RingShades (symbols.toml). FocusAura_RingShades
// kMoteDelays: now FocusMote_Delays (symbols.toml). FocusMote_Delays: 8 bytes
// kAnchorOffsets: now Enlighten_AnchorOffsets (symbols.toml). Enlighten_AnchorOffsets: (dx, dy) words
// kAnchorOffsetsB: now Enlighten_AnchorOffsetsB (symbols.toml). Enlighten_AnchorOffsetsB
// kAnchorIndexB: now Enlighten_AnchorIndexB (symbols.toml). Enlighten_AnchorIndexB: bytes

// The phase handlers and callees of other units (docs/magic_s07.md section 3)
// are ours now and called by name: S05's Magic017_Wait, S06's
// Magic008_DrawFlash, S37's MagicFx_FreeCurrentRecord, S03's
// Chlorine_WaitChildren, S11's MagicFx_UncountAndFree.

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }
std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return Low32(p); }
void AddLong(unsigned char* at, std::uint32_t v) {
    SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) + v));
}

// `add r32, r32`: the 32-bit sum, wrapped.
std::int32_t Sum(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
// `imul r32, r32`: the 32-bit product, wrapped.
int Imul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }
// `imul` then `sar 0xC`.
int Mul12(int a, int b) { return Imul(a, b) >> 12; }
// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions and other units' called by address, as the originals
// call them: in the game the jmp Inject put there (or Capcom's code), in the
// fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using ByteFn = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
}

// A primitive linked at the current task's position (+0x34 / +0x38, read at
// the call) on layer `dy`.
void LinkAtSprite(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy,
                                size);
}
// A draw-mode packet (tpage `tpage`, dithered) at Gfx_PacketNext.
void DrawMode(unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0); }

// The task's position (+0x34 / +0x38 / +0x3C) the owner's, each read again.
void PositionFromOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// Row 26 of Gfx_ClutStrip, its 32 words, back from its source.
void RestoreRow26() {
    for (unsigned k = 0; k < 0x10; ++k) {
        Gfx_ClutStrip[0x1A00 + k] = Gfx_ClutStripSource[0x1A00 + k];
        Gfx_ClutStrip[0x1A10 + k] = Gfx_ClutStripSource[0x1A10 + k];
    }
}

}  // namespace

#define S07_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC021 (row 33, Bonebreak read one id down)

// ===========================================================================
// MAGIC038 (row 121, War Shout read one id down)

// ===========================================================================
// MAGIC039 (row 78, Focus and Meditation read one id down)

// ===========================================================================
// MAGIC040 (row 79, Enlighten read one id down)

void MagicS07_Inject() {
    if (bof3::WantsShadow("magic_s07")) magic_s07::SelfTest();
}
