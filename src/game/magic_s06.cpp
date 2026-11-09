// Two spell overlays compiled into the exe, round nine group S06
// (docs/magic_s06.md): the PSX's MAGIC008.EMI and MAGIC020.EMI, Magic_Rows
// rows 42 and 8. Read one id down (docs/cut-content.md section 2) the sibling
// labels row 42's nine abilities Gambit, Mind Flay, Blind, Devour, (no label),
// Syphon, Feign Swing, Backhand and Risky Blow, and row 8's Disembowel - names
// that are hypotheses. The names below say what the code does.
//
//   - MAGIC008 0x4A2190..0x4A3140: the kind-2 task copies the acting actor's
//     record (its first 0x80 bytes, a sprite) into a child - a double of the
//     actor - plays the actor's animation 0xC, then applies one buff to the
//     target (MagicFx_ApplyBuff, the stat by ability 7 or none) with its
//     popup, and ends when the double has. The doubles are one kind-1 task
//     (parameter 0x1F) with six bodies by +1 (0x65A6A0): a double that grows
//     and shrinks, one that glows at the field's kind-2 point, the mirror this
//     overlay makes (it strikes and flashes the screen), one that dashes
//     beside its owner, and two that strike two or three times, each waiting
//     for the target's reaction. MAGIC017, 018 and 019 make the other five.
//   - MAGIC020 0x4A3150..0x4A3B18: five doubles of the actor - four shadows
//     at staggered delays and one lead - run to the source sprite; when the
//     lead arrives the screen darkens and eight slashes (sprite bank 0x1D,
//     animations by 0x65A6DC) play one after another at the source; the last
//     flags the target; the lead then returns and fades.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table, and an index past one of the two .data tables read by +0xB,
// abort where the original would read whatever follows
// (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s06.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
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

// The cells the overlays read beyond the harness's names.
constexpr ::bof3::image::Va kAbility{0x904B80};        // u16: the ability being cast

// Magic008Dash_Run's stack table names S04's KickImage_Tick (ours now).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }
unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
std::uint16_t Ability() { return Word(Mem(::bof3::Img(kAbility.v))); }
std::uint32_t Key(const void* p) { return Low32(p); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The acting actor's record as the originals index it: a member below 3, else
// the enemy by index - 3, unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor < 3) return Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride);
    return Mem(::bof3::Img(Battle_Enemies_addr) + (static_cast<std::uint32_t>(actor) - 3) * at::kEnemyStride);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// A double: a kind-1 task (`parameter`) whose first 0x80 bytes are the acting
// actor's record (the actor byte read after the task is made), copied dword by
// dword forward as `rep movsd` does; its owner this task, +1 `body`, +2 0,
// +6 1 (kind 1), +5 `parameter`. Answers the new slot.
unsigned char* MakeDouble(unsigned parameter, unsigned char body) {
    const unsigned slot = NewTask(parameter);
    const unsigned char* const from = ActorRecord(ActorByte());
    unsigned char* const to = TaskSlot(slot);
    records::BattleTask* const tto = rv::As<records::BattleTask>(to);
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(to + k, Long(from + k));
    tto->owner = static_cast<std::int32_t>(Key(Sc()));
    to[1] = body;
    to[2] = 0;
    to[6] = 1;
    to[5] = static_cast<unsigned char>(parameter);
    return to;
}

// Sprite_Current's position (+0x34..+0x3C) from `from`, read afresh.
void CopyPosition(const unsigned char* from) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(from)->height);
}
// The same from the owner, the owner read again for each word.
void CopyOwnerPosition() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}
// The sprite's colour +0x5D..+0x5F, each moved by `by` (a byte).
void AddColour(unsigned by) {
    AddB(Sc()[0x5D], by);
    AddB(Sc()[0x5E], by);
    AddB(Sc()[0x5F], by);
}
// +9 down; true when it reaches 0 (read again).
bool CountDown() {
    Dec(Sc()[9]);
    return Sc()[9] == 0;
}

using Fn0 = void (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// A draw-mode packet (tpage `tpage`, dithered) committed to layer `layer`.
void DrawMode(unsigned tpage, unsigned layer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(layer, 0xC);
}

constexpr std::uint32_t kF0 = 0;             // 0.0f
constexpr std::uint32_t kF320 = 0x43A00000;  // 320.0f
void PutU32(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(v)); }

}  // namespace

#define S06_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC008 (row 42)

// --- body 0: a double that grows, strikes and shrinks -----------------------

// --- body 1: a double that glows at the field's kind-2 point ----------------

// --- body 2: the mirror Magic008_Start makes ---------------------------------

// --- body 3: a double that dashes beside its owner ---------------------------

// --- bodies 4 and 5: two or three blows ---------------------------------------

// ===========================================================================
// MAGIC020 (row 8)

// --- body 0: the lead -----------------------------------------------------------

// --- body 1: a shadow -----------------------------------------------------------

// --- body 2: a slash -------------------------------------------------------------

void MagicS06_Inject() {
    if (bof3::WantsShadow("magic_s06")) magic_s06::SelfTest();
}
