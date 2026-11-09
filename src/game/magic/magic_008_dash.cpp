// Moved here by tools/move_funcs.py from magic_s06.cpp (docs/source-layout-round.md).
#include "game/magic/magic_008_dash.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s06.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::Word;

// The cells the overlays read beyond the harness's names.
constexpr ::bof3::image::Va kAbility{0x904B80};        // u16: the ability being cast

// kDashOffsets: now Magic008Dash_Offsets (symbols.toml). Magic008Dash_Offsets: 3 x (dword dx, dword dz)
constexpr unsigned kDashOffsetCount = 3;

// Capcom's, unnamed, in no group: 0x446770 turns the dx / dz pair +0xC / +0x10
// of the task it is given by its direction byte +8 (docs/magic_s22.md);
// 0x435A70 plays animation `animation` on enemy `actor` (BattleEnemy_SetAnimation
// with 0x939AD8 pointed at that enemy's record and put back).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

constexpr ::bof3::image::CodeVa kEnemyAnimation{bof3::addr::BattleEnemy_SetAnimationOf};   // 0x435A70, R3A's (round 14)

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned char ActorByte() { return Mem(::bof3::Img(at::kActor))[0]; }

std::uint16_t Ability() { return Word(Mem(::bof3::Img(kAbility.v))); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

// The acting actor's record as the originals index it: a member below 3, else
// the enemy by index - 3, unchecked.
unsigned char* ActorRecord(unsigned char actor) {
    if (actor < 3) return Mem(::bof3::Img(ObjTrio_addr) + actor * at::kPartyStride);
    return Mem(::bof3::Img(Battle_Enemies_addr) + (static_cast<std::uint32_t>(actor) - 3) * at::kEnemyStride);
}

// A battle actor's state byte +1, as the reaction steps read it.
unsigned char ActorState(unsigned char actor) { return ActorRecord(actor)[1]; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
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

}  // namespace

#define S06_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The dashing double's position: the owner's plus its offset (+0xC, +0x10).
void BesideOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->x) +
                                                    static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->z) +
                                                    static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_z)));
}

// On the owner (x, z only).
void OnOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
}

// The reaction both React steps share, once the target is not out: while the
// target's state +1 is 6 (reacting), nothing - answers false. Else the double
// starts its next swing - a member's animation +8 + 0xC on Sprite_Current, the
// upload that just queued cancelled (the queue count down, that entry's x, y
// and record zeroed); an enemy's animation 2 through 0x435A70, with +0x4B
// 0xFF - +9 the effect size, +2 on; answers true.
bool React() {
    const unsigned char target = TargetByte();
    if (ActorState(target) == 6) return false;
    if (ActorByte() < 3) {
        MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[8] + 0xC));
        const unsigned char n = static_cast<unsigned char>(Gfx_UploadQueueCount - 1);
        Gfx_UploadQueueCount = n;
        Gfx_UploadQueueX[n] = 0;
        Gfx_UploadQueueRecord[n] = nullptr;
        Gfx_UploadQueueY[n] = 0;
    } else {
        Sc()[0x4B] = 0xFF;
        MH_AT(void (__cdecl*)(unsigned, unsigned), kEnemyAnimation.v)(ActorByte(), 2);
    }
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
    return true;
}

}  // namespace

// original 0x4A2AF0: a six-entry stack table by +2 (_Start, _Follow, _Strike,
// MAGIC018/019's 0x4A01C0, _Fade, BattleFx_FreeTask); then while +0 and +2
// are set the screen update.
S06_EXPORT void __cdecl Magic008Dash_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::Magic008Dash_Start,  bof3::addr::Magic008Dash_Follow,
                                                bof3::addr::Magic008Dash_Strike, bof3::addr::KickImage_Tick,
                                                bof3::addr::Magic008Dash_Fade,   bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("Magic008Dash_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const sc = Sc();
    if (sc[0] != 0 && sc[2] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2B50: +0 bit 0x20, +0x5C 1, +0x2B 1, the colour 0xB0; +0x27,
// the direction and the position the owner's; the offset +0xC / +0x10 from
// Magic008Dash_Offsets by +0xB (three pairs, unchecked; ours aborts past
// them), turned by the direction (0x446770); +9 the effect size; +2 on.
S06_EXPORT void __cdecl Magic008Dash_Start(void) {
    Sc()[0] |= 0x20;
    Sc()[0x5C] = 1;
    Sc()[0x2B] = 1;
    Sc()[0x5D] = 0xB0;
    Sc()[0x5E] = 0xB0;
    Sc()[0x5F] = 0xB0;
    Sc()[0x27] = Owner()[0x27];
    Sc()[8] = Owner()[8];
    CopyOwnerPosition();
    unsigned char* const sc = Sc();
    records::BattleTask* const tsc = rv::As<records::BattleTask>(sc);
    const unsigned index = sc[0xB];
    if (index >= kDashOffsetCount) bof3::Fatal("Magic008Dash_Start: +0xB %u, past the %u offsets", index, kDashOffsetCount);
    tsc->step_x = Long(Mem(::bof3::Img(Magic008Dash_Offsets_addr) + 8 * index));
    tsc->step_z = Long(Mem(::bof3::Img(Magic008Dash_Offsets_addr) + 8 * index + 4));
    MH_AT(void (__cdecl*)(unsigned char*), kTurnOffset.v)(Sc());
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
}

// original 0x4A2C40: on odd frames on the owner, the script ticked once, +9
// down, at 0 +9 = +0xB + 4 and +2 on; on even frames beside the owner.
S06_EXPORT void __cdecl Magic008Dash_Follow(void) {
    if ((Frame_Counter & 1) == 0) {
        BesideOwner();
        return;
    }
    OnOwner();
    MH_CALL(Sprite_ScriptTickOnce)();
    if (!CountDown()) return;
    unsigned char* const sc = Sc();
    sc[9] = static_cast<unsigned char>(sc[0xB] + 4);
    Inc(Sc()[2]);
}

// original 0x4A2CD0: on odd frames on the owner, +9 down, at 0: when +0xB is 0
// (the first double) the actor's sound (2, 4) and the target flagged 0x40;
// +2 on. On even frames beside the owner.
S06_EXPORT void __cdecl Magic008Dash_Strike(void) {
    if ((Frame_Counter & 1) == 0) {
        BesideOwner();
        return;
    }
    OnOwner();
    if (!CountDown()) return;
    unsigned char* sc = Sc();
    if (sc[0xB] == 0) {
        MH_CALL(BattleActor_PlaySound)(2, 4);
        MH_CALL(Battle_SetTargetFlag40)(TargetByte());
        sc = Sc();
    }
    Inc(sc[2]);
}

// original 0x4A2D70: the colour down by 3; at 0x80 the owner's +0xB down and
// +2 on.
S06_EXPORT void __cdecl Magic008Dash_Fade(void) {
    AddColour(0xFD);
    if (Sc()[0x5D] != 0x80) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4A2DC0: a seven-entry stack table by +2 (BattleFx_SetSize,
// _Strike, _WaitTwo, _ReactTwo, _Strike, _WaitTwo, BattleFx_FreeTask); then
// while +0 is set the screen update.
S06_EXPORT void __cdecl Magic008TwoBlows_Run(void) {
    static constexpr std::uint32_t kSteps[7] = {
        bof3::addr::BattleFx_SetSize,     bof3::addr::Magic008Blow_Strike, bof3::addr::Magic008Blow_WaitTwo,
        bof3::addr::Magic008Blow_ReactTwo, bof3::addr::Magic008Blow_Strike, bof3::addr::Magic008Blow_WaitTwo,
        bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 7) PastTable("Magic008TwoBlows_Run", phase, 7);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2E20: the script ticked once until its end; then at step 5 (the
// last blow's) the owner's +0xB 0xFF; +2 on.
S06_EXPORT void __cdecl Magic008Blow_WaitTwo(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    unsigned char* sc = Sc();
    if (sc[2] == 5) {
        Owner()[0xB] = 0xFF;
        sc = Sc();
    }
    Inc(sc[2]);
}

// original 0x4A2E50: when the target is out, the owner's +0xB 0xFF and +2 6
// (the free); else React.
S06_EXPORT void __cdecl Magic008Blow_ReactTwo(void) {
    if (MH_CALL(Battle_ActorIsOut)(TargetByte()) != 0) {
        Owner()[0xB] = 0xFF;
        Sc()[2] = 6;
        return;
    }
    React();
}

// original 0x4A2F40: a ten-entry stack table by +2 (BattleFx_SetSize; _Strike,
// _WaitThree, _ReactThree twice; _Strike, _WaitThree; BattleFx_FreeTask); then
// while +0 is set the screen update.
S06_EXPORT void __cdecl Magic008ThreeBlows_Run(void) {
    static constexpr std::uint32_t kSteps[10] = {
        bof3::addr::BattleFx_SetSize,        bof3::addr::Magic008Blow_Strike,     bof3::addr::Magic008Blow_WaitThree,
        bof3::addr::Magic008Blow_ReactThree, bof3::addr::Magic008Blow_Strike,     bof3::addr::Magic008Blow_WaitThree,
        bof3::addr::Magic008Blow_ReactThree, bof3::addr::Magic008Blow_Strike,     bof3::addr::Magic008Blow_WaitThree,
        bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 10) PastTable("Magic008ThreeBlows_Run", phase, 10);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4A2FB0: the script ticked once; +9 down; at 0 the actor's sound
// (2, 0), the target flagged 0x40, +2 on.
S06_EXPORT void __cdecl Magic008Blow_Strike(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (!CountDown()) return;
    MH_CALL(BattleActor_PlaySound)(2, 0);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Inc(Sc()[2]);
}

// original 0x4A2FF0: the same at step 8.
S06_EXPORT void __cdecl Magic008Blow_WaitThree(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    unsigned char* sc = Sc();
    if (sc[2] == 8) {
        Owner()[0xB] = 0xFF;
        sc = Sc();
    }
    Inc(sc[2]);
}

// original 0x4A3020: the same with +2 9; then, unless +2 is 9, when the
// ability is 0xB and Rand & 7 is 0 (one in eight) the owner's +0xB 0xFF and
// +2 9 - the blows cut short. The target's state 6 skips React but not the
// cut.
S06_EXPORT void __cdecl Magic008Blow_ReactThree(void) {
    if (MH_CALL(Battle_ActorIsOut)(TargetByte()) != 0) {
        Owner()[0xB] = 0xFF;
        Sc()[2] = 9;
    } else {
        React();
    }
    if (Sc()[2] == 9) return;
    if (Ability() != 0xB) return;
    if ((MH_CALL(Rand)() & 7) != 0) return;
    Owner()[0xB] = 0xFF;
    Sc()[2] = 9;
}

void Magic008Dash_Inject() {
    BOF3_INJECT(Magic008Dash_Run);
    BOF3_INJECT(Magic008Dash_Start);
    BOF3_INJECT(Magic008Dash_Follow);
    BOF3_INJECT(Magic008Dash_Strike);
    BOF3_INJECT(Magic008Dash_Fade);
    BOF3_INJECT(Magic008TwoBlows_Run);
    BOF3_INJECT(Magic008Blow_WaitTwo);
    BOF3_INJECT(Magic008Blow_ReactTwo);
    BOF3_INJECT(Magic008ThreeBlows_Run);
    BOF3_INJECT(Magic008Blow_Strike);
    BOF3_INJECT(Magic008Blow_WaitThree);
    BOF3_INJECT(Magic008Blow_ReactThree);
}
