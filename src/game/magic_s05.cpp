// Two spell units compiled into the exe, round nine group S05
// (docs/magic_s05.md): the PSX's MAGIC017.EMI (row 43) and MAGIC018/019.EMI
// (rows 5, 6, 53, 54, 68; MAGIC019's one row sits inside MAGIC018's code, so
// the two are one unit). Read one id down (docs/cut-content.md section 2) the
// sibling's labels are Astral Warp / Shadowwalk (row 43), Giant Growth (5),
// Aura (6), SpiritBlast (53), Double Blow (54) and Multistrike / Triple Blow
// (68) - hypotheses; the names below say only which unit and row, and what
// the code does.
//
// Every row's start does the same thing with different counts: the task takes
// the owner's direction and position, plays the actor's animation 0xC, and
// copies the acting actor's record (0x80 bytes: party 0x802D40 + 0x14C i, or
// enemy 0x93B960 + 0x128 (i - 3)) into new child tasks - the images:
//
//   - MAGIC017 0x4A11E0..0x4A1976: six images (kind 1, 0x21), run by this
//     unit's own ten-step child (Magic017Image_*): each fades in, drifts
//     out, jumps to the source sprite (0x904B4C), comes back, and the one
//     with +0xB 0 plays the strike while the others free themselves; the
//     task waits for them, the fx CLUT row is restored, done;
//   - MAGIC018/019 0x4A1980..0x4A218F: one image (rows 5, 6, 54, 68) or
//     three (row 53) of kind 1, 0x1F - MAGIC008's code, group S06's - with
//     the type +1 of 0, 1, 3, 4 or 5; the task waits for the image to count
//     its +0xB down (to 0xFF, or to 0 for row 53), then the animation 4.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s05.h"

#include <cstdint>

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
using move_script::Long;
using move_script::SetLong;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
std::uint32_t Key(const void* p) { return Low32(p); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The acting actor's record as the originals index it: a member (0x802D40,
// stride 0x14C) at 0..2, else the enemy by index - 3 (0x93B960, stride
// 0x128), unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor <= 2) return Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride);
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(actor) - 3) * at::kEnemyStride);
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The step handlers of other groups the tables hold, by their addresses (a
// phase is always called through the address its table holds): group E's
// done flag and free (0x43FE80) and target flag 0x40 with the done flag and
// free (0x43F460), round eight's task free (0x4AEE90).
constexpr ::bof3::image::CodeVa kDoneAndFree{bof3::addr::MagicFx_DoneAndFree};
constexpr ::bof3::image::CodeVa kFlagTargetEnd{bof3::addr::MagicFx_FlagTargetEnd};

constexpr unsigned kImage008 = 0x1F;   // MAGIC008's (group S06's)

// The start every row shares: the owner's direction +8 and position
// +0x34 / +0x38 / +0x3C to the task, +0xB and +9 0, +1 on, then the actor's
// animation 0xC with `arg`. Each field is read and written through the cells
// afresh, as the originals do.
void TakeOwnerAndAnimate(unsigned arg) {
    {
        const unsigned char facing = Owner()[8];
        Sc()[8] = facing;
    }
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, arg);
}

// One image: a kind-1 task of `parameter` (the slot unchecked: 0xFF, none
// free, would write past the 48 slots), the actor byte read after the call,
// and the actor's record's first 0x80 bytes copied over the slot a dword at
// a time (rep movsd). Its fields are the caller's to set.
unsigned char* SpawnImage(unsigned parameter) {
    const unsigned slot = MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu;
    unsigned char* const t = TaskSlot(slot);
    const unsigned char* const src = ActorRecord(ActorByte());
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(t + k, Long(src + k));
    return t;
}
// The fields each image of MAGIC018/019 gets: +0x80 this task, +1 its type,
// +2 0, +6 1, +5 the parameter.
void SetImage008(unsigned char* t, unsigned char type) {
    rv::Store<std::int32_t>(rv::As<records::BattleTask>(t)->owner, static_cast<std::int32_t>(Key(Sc())));
    t[1] = type;
    t[2] = 0;
    t[6] = 1;
    t[5] = static_cast<unsigned char>(kImage008);
}

// The owner's sprite into the fx CLUT row: +0x27 the row CopyToFxRow
// answers, +0x28 / +0x24 the owner's, then the row's STP bits.
void OwnerClutToFxRow() {
    const unsigned row = MH_CALL(SpriteClut_CopyToFxRow)(Owner());
    Sc()[0x27] = static_cast<unsigned char>(row);
    Sc()[0x28] = Owner()[0x28];
    Sc()[0x24] = Owner()[0x24];
    MH_CALL(SpriteClut_SetStp)(Sc());
}

}  // namespace

#define S05_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC017 (row 43)

// ===========================================================================
// MAGIC018/019 (rows 5, 6, 53, 54, 68)

void MagicS05_Inject() {
    if (bof3::WantsShadow("magic_s05")) magic_s05::SelfTest();
}
