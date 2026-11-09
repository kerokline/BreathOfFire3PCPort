// Moved here by tools/move_funcs.py from magic_s05.cpp (docs/source-layout-round.md).
#include "game/magic/magic_018_row_5.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s05.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

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

}  // namespace

#define S05_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4A1980 (row 5): a four-entry stack table by +1:
// Magic018Row5_Start, Magic018Row5_Wait, MagicFx_DoneAndFree,
// MagicFx_FlagTargetEnd (never reached: the third frees the task).
S05_EXPORT void __cdecl Magic018Row5_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Magic018Row5_Start, bof3::addr::Magic018Row5_Wait,
                                                 kDoneAndFree.v, kFlagTargetEnd.v};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic018Row5_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4A19C0: the shared start with animation (0xC, 2); one image
// (kind 1, 0x1F) of type 0; the owner's +0 bit 0x40, sound 0x100.
S05_EXPORT void __cdecl Magic018Row5_Start(void) {
    TakeOwnerAndAnimate(2);
    SetImage008(SpawnImage(kImage008), 0);
    Owner()[0] |= 0x40;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A1AF0: once the image has counted +0xB down to 0xFF: the
// actor's animation (4, 0), the owner's bit 0x40 off, +1 on.
S05_EXPORT void __cdecl Magic018Row5_Wait(void) {
    if (Sc()[0xB] != 0xFF) return;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    Inc(Sc()[1]);
}

void Magic018Row5_Inject() {
    BOF3_INJECT(Magic018Row5_Task);
    BOF3_INJECT(Magic018Row5_Start);
    BOF3_INJECT(Magic018Row5_Wait);
}
