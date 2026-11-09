// Two spell overlays compiled into the exe, round nine group S02
// (docs/magic_s02.md): the PSX's MAGIC003.EMI (Magic_Rows row 3) and
// MAGIC004.EMI, whose code the rows 88, 92, 93, 98..100 and 129..132 of ten
// files run (the linker folded MAGIC005, 029, 049, 133..136, 156 and 157 into
// it). Read one id down (docs/cut-content.md section 2) the sibling labels them
// Super Combo, and ThundrStrike, Holy Strike, Demonbane, Flame Strike,
// Pyrokinesis, Frost Strike, Wind Strike and the four Claws; the names below
// use those labels as hypotheses, and say what the code does.
//
//   - MAGIC003 0x49A7B0..0x49BAE6: a button prompt. A box and a line of text,
//     then a button asked for at random (Rand & 3) and Input_Pressed read
//     against it within a time that shortens with each hit; each hit counts
//     (+0xA, at most 0x20). Then the count shown, the caster copied into a
//     dash (kind 1, parameter 1) and four tinted after-images, a hit sprite per
//     count from a private pool of 32 (0x6769C0) that the kind-2 task runs
//     itself, and the count written to the battle byte 0x904B96;
//   - MAGIC004 0x49BAF0..0x49C3C5: the caster copied into a child (kind 1,
//     parameter 0x46) that plays an animation, a second child that plays an
//     effect sprite at the source, the target tinted by the ability's element
//     (ElemStrike_Kind maps the ability word 0x904B80 to one of twelve) and
//     faded back.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table (or a case past ElemStrike_Kind's twelve) aborts where the
// original would call through whatever follows it (docs/magic_fx_reached.md
// section 3, the precedent).
#include "game/magic_s02.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

// Rebound 2026-09-29 (round twelve group BE4, docs/battle_e4.md section 9): the constants here naming BE4's functions read
// bof3::addr::<Name>; the values are unchanged (the fuzz keys on them).

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::CodeVa kKindIndex{0x49C0CC};     // .text: ElemStrike_Kind's byte table, 0x99 entries

// ElemStrike_Kind's case by the ability word less 4: the switch the compiler
// made the byte table at kKindIndex of - the function's logic, ours since the
// function is (docs/operand-store.md). Eleven abilities have a case of their
// own (0..10); every other index, and any past the table, is case 11.
unsigned KindCase(std::uint32_t index) {
    switch (index) {
    case 0x00: return 0;
    case 0x01: return 1;
    case 0x03: return 2;
    case 0x19: return 3;
    case 0x2D: return 4;
    case 0x81: return 5;
    case 0x82: return 6;
    case 0x83: return 7;
    case 0x84: return 8;
    case 0x97: return 9;
    case 0x98: return 10;
    default: return 11;
    }
}

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned ActorByte() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Key(const void* p) { return Low32(p); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: a party
// member below 3 (the target) or at most 2 (the actor), else the enemy by
// index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}
unsigned char* ActorRecord(unsigned a) { return a <= 2 ? PartyRecord(a) : EnemyRecord(a); }

// `mov ecx, 0x20; rep movsd`: 0x80 bytes, dword by dword, forward.
void CopyRecord(unsigned char* to, const unsigned char* from) {
    for (unsigned k = 0; k < 0x80; k += 4) {
        std::uint32_t v;
        std::memcpy(&v, from + k, 4);
        std::memcpy(to + k, &v, 4);
    }
}

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Other units' functions (docs/magic_s02.md section 3) are ours now and called
// by name: S06's Magic008_DrawFlash, S37's MagicFx_FreeCurrentRecord (the
// tail jmp of SuperComboHit), S34's BattleFx_ScriptToEnd, S35's
// MagicFx_WaitOwnerChildren.

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A draw-mode packet (tpage `tpage`, dithered) committed to `layer`.
void DrawModeCommit(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

// A counter byte down by one; true when it reached 0.
bool CountDown(unsigned k) {
    Dec(Sc()[k]);
    return Sc()[k] == 0;
}

unsigned char Tick() { return MH_CALL(Sprite_ScriptTickOnce)(); }

}  // namespace

#define S02_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC003 (row 3, Super Combo read one id down)

// ===========================================================================
// MAGIC004 (rows 88, 92, 93, 98..100, 129..132: the Strikes and Claws read one
// id down)

void MagicS02_Inject() {
    {
        static std::uint8_t index[0x99];   // KindCase as the compiler's byte table, for the hosted check
        for (std::uint32_t i = 0; i < sizeof index; ++i) index[i] = static_cast<std::uint8_t>(KindCase(i));
        bof3::Operand("ElemStrike_Kind byte table", kKindIndex.v, index, sizeof index);
    }
    if (bof3::WantsShadow("magic_s02")) magic_s02::SelfTest();
}
