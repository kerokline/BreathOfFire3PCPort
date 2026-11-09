// Moved here by tools/move_funcs.py from magic_s07.cpp (docs/source-layout-round.md).
#include "game/magic/war_shout_mote.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s07.h"
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

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// four SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read
// again after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The cells the overlays read beyond the harness's names.
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};

constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

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

// `shl n` on a dword.
int Shl(int v, unsigned n) { return static_cast<int>(static_cast<std::uint32_t>(v) << n); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the enemy's
// by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A stack table's (or a .data table's) dispatch: entry `phase`, which the
// originals do not check.
void Dispatch(const std::uint32_t* phases, unsigned n, unsigned phase, const char* who) {
    if (phase >= n) PastTable(who, phase, n);
    magic_harness::Phase(phases[phase])();
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

namespace {

// The motes' orbit, one step: +0xC up, the position (the owner's, read again
// after each call) + sin / cos of (+0xC & 0x7F) sl 5 at radius sl 5 (the
// angle kept in dword 0x903850 and read again for the cos); +0x14 up, the
// height the owner's + sin((+0x14 & 0x3F) sl 6) x dword 0x903854 (0x800, read
// after the call).
void OrbitStep() {
    AddLong(Sc() + 0xC, 1);
    std::uint32_t angle = (static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x) & 0x7F) << 5;
    SetSD(0, angle);
    int v = MH_CALL(Math_Sin)(static_cast<int>(angle));
    rv::Store(rv::Current<records::BattleTask>()->x, Sum(Shl(v, 0x11) >> 12, rv::As<records::SpriteHead>(Owner())->x));
    v = MH_CALL(Math_Cos)(SD(0));
    rv::Store(rv::Current<records::BattleTask>()->z, Sum(Shl(v, 0x11) >> 12, rv::As<records::SpriteHead>(Owner())->z));
    AddLong(Sc() + 0x14, 1);
    angle = (static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_y) & 0x3F) << 6;
    SetSD(4, 0x800);
    SetSD(0, angle);
    v = MH_CALL(Math_Sin)(static_cast<int>(angle));
    rv::Store(rv::Current<records::BattleTask>()->height, Sum(Imul(v, SD(4)), rv::As<records::SpriteHead>(Owner())->height));
}

}  // namespace

// original 0x4A4440: the kind-2 task. A three-entry stack table by +1:
// WarShout_Start, _Rally, MagicFx_EndWhenChildrenDone.
S07_EXPORT void __cdecl WarShout_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::WarShout_Start, bof3::addr::WarShout_Rally,
                                                 bof3::addr::MagicFx_EndWhenChildrenDone};
    Dispatch(kPhases, 3, Sc()[1], "WarShout_Task");
}

// original 0x4A4470: the task to the side's centre, 0x200 up (+0x3C +
// 0x2000000); +0xB and +9 0, +1 on; sixteen motes (kind 1, 0x59): +0x80 this
// task (read after each create), +1 0, +0xB the number, +9 the delay number
// x 8 + 1; +0xB up for each. Row 26 of the CLUT strip back (32 words); row
// 2's first 16 words from the source with STP, then its word 0 without;
// Gfx_ClutStripDirty; sound 0x100.
S07_EXPORT void __cdecl WarShout_Start(void) {
    MH_CALL(MagicFx_CenterOnSide)();
    AddLong(Sc() + 0x3C, 0x2000000);
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    for (unsigned i = 0; i < 0x10; ++i) {
        const unsigned slot = NewTask(0x59);
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>((i << 3) + 1);
        Inc(self[0xB]);
    }
    RestoreRow26();
    for (unsigned k = 0; k < 0x10; ++k) Gfx_ClutStrip[0x200 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x200 + k] | 0x8000);
    Gfx_ClutStrip[0x200] = Gfx_ClutStripSource[0x200];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4A4570: once +0xB (the motes alive) is 0: sound 0x101; one child
// (kind 1, 0x59) on every actor of the target's side (the enemies 3..10 with
// target bit 6, else the party 0..2) Battle_ActorIsOut does not answer for:
// +0x80 this task, +1 1, +3 the side's index, +4 2, its position the actor
// record's (read after the create); +0xB up for each; +1 on.
S07_EXPORT void __cdecl WarShout_Rally(void) {
    if (Sc()[0xB] != 0) return;
    MH_CALL(Sound_PlayById)(0x101);
    const bool enemies = (TargetByte() & 0x40) != 0;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(enemies ? i + 3 : i) != 0) continue;
        const unsigned slot = NewTask(0x59);
        const unsigned char* const rec = enemies ? Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride) : PartyRecord(i);
        const std::int32_t x = rv::As<records::SpriteHead>(rec)->x;
        unsigned char* const self = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(self));
        child[1] = 1;
        child[3] = static_cast<unsigned char>(i);
        child[4] = 2;
        tchild->x = x;
        tchild->z = rv::As<records::SpriteHead>(rec)->z;
        rv::Store(tchild->height, rv::As<records::SpriteHead>(rec)->height);
        Inc(self[0xB]);
    }
    Inc(Sc()[1]);
}

// original 0x4A46F0: the children's kind-1 task, a jmp through
// WarShoutChild_Kinds (two entries: the mote, the buff) by +1, unchecked.
S07_EXPORT void __cdecl WarShoutChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::WarShoutMote_Run, bof3::addr::WarShoutBuff_Run};
    Dispatch(kKinds, 2, Sc()[1], "WarShoutChild_Task");
}

// original 0x4A4710: the frame-offset table 0x9039D8 the effects'
// (0x8E3580); a call through WarShoutMote_Steps (five entries: _Appear,
// _Rise, _Circle, _Fade, MAGIC058's 0x4AF490) by +2; with bytes +0 and +2 not
// 0 Sprite_UpdateScreen; the table the battle's again (0x8B3580).
S07_EXPORT void __cdecl WarShoutMote_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::WarShoutMote_Appear, bof3::addr::WarShoutMote_Rise,
                                                bof3::addr::WarShoutMote_Circle, bof3::addr::WarShoutMote_Fade,
                                                bof3::addr::MagicFx_UncountAndFree};
    const unsigned phase = Sc()[2];
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    Dispatch(kSteps, 5, phase, "WarShoutMote_Run");
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4A4750: +9 down; at 0: +0xC and +0x14 0; the position the
// owner's + sin / cos(0) at radius sl 6 and the height + sin(0) sl 11 (the
// angles in dword 0x903850, the cos's read again); the sprite fields: +0x25
// 0x1D, +0x26 0, +0x27 0x20, +0x28 0, +0x24 0, +0 bit 5, +0x5C 1, +0x5D..+0x5F
// 0x80, +0x2A 0, +0x29 4, word +0x2C 0, +0x2B 0; its animation
// WarShoutMote_Animations[+0xB]; +2 on.
S07_EXPORT void __cdecl WarShoutMote_Appear(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    rv::Current<records::BattleTask>()->step_x = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0);
    std::uint32_t angle = (static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x) & 0x7F) << 5;
    SetSD(0, angle);
    int v = MH_CALL(Math_Sin)(static_cast<int>(angle));
    rv::Store(rv::Current<records::BattleTask>()->x, Sum(Shl(v, 0x12) >> 12, rv::As<records::SpriteHead>(Owner())->x));
    v = MH_CALL(Math_Cos)(SD(0));
    rv::Store(rv::Current<records::BattleTask>()->z, Sum(Shl(v, 0x12) >> 12, rv::As<records::SpriteHead>(Owner())->z));
    angle = (static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_y) & 0x3F) << 6;
    SetSD(0, angle);
    v = MH_CALL(Math_Sin)(static_cast<int>(angle));
    rv::Store(rv::Current<records::BattleTask>()->height, Sum(Shl(v, 0x17) >> 12, rv::As<records::SpriteHead>(Owner())->height));
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x27] = 0x20;
    s[0x28] = 0;
    s[0x24] = 0;
    s[0] |= 0x20;
    s[0x5C] = 1;
    s[0x5D] = 0x80;
    s[0x5E] = 0x80;
    s[0x5F] = 0x80;
    s[0x2A] = 0;
    s[0x29] = 4;
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->column, 0);
    s[0x2B] = 0;
    MH_CALL(Sprite_SetAnimation)(Mem(::bof3::Img(WarShoutMote_Animations_addr) + s[0xB])[0]);
    Inc(Sc()[2]);
}

// original 0x4A48B0: the script ticked; the colour +0x5D..+0x5F up by 4; an
// orbit step; at +0x5D 0xC0 bit 5 of +0 cleared, +0x5C..+0x5F 0, +2 on.
S07_EXPORT void __cdecl WarShoutMote_Rise(void) {
    MH_CALL(Sprite_ScriptTick)();
    AddB(Sc()[0x5D], 4);
    AddB(Sc()[0x5E], 4);
    AddB(Sc()[0x5F], 4);
    OrbitStep();
    unsigned char* const s = Sc();
    if (s[0x5D] != 0xC0) return;
    s[0] &= 0xDF;
    s[0x5C] = 0;
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    Inc(s[2]);
}

// original 0x4A49D0: the script ticked; an orbit step; at +0xC 0x100 bit 5 of
// +0 set, +0x5C 1, +0x5D..+0x5F 0xC0, +2 on.
S07_EXPORT void __cdecl WarShoutMote_Circle(void) {
    MH_CALL(Sprite_ScriptTick)();
    OrbitStep();
    unsigned char* const s = Sc();
    records::BattleTask* const ts = rv::As<records::BattleTask>(s);
    if (ts->step_x != 0x100) return;
    s[0] |= 0x20;
    s[0x5C] = 1;
    s[0x5D] = 0xC0;
    s[0x5E] = 0xC0;
    s[0x5F] = 0xC0;
    Inc(s[2]);
}

// original 0x4A4AD0: the colour +0x5D..+0x5F down by 4; the script ticked; an
// orbit step; +0x5D 0x80; +2 on.
S07_EXPORT void __cdecl WarShoutMote_Fade(void) {
    AddB(Sc()[0x5D], 0xFC);
    AddB(Sc()[0x5E], 0xFC);
    AddB(Sc()[0x5F], 0xFC);
    MH_CALL(Sprite_ScriptTick)();
    OrbitStep();
    Sc()[0x5D] = 0x80;
    Inc(Sc()[2]);
}

// original 0x4A4BC0: the buff child, a jmp through WarShoutBuff_Steps (two
// entries: _Start, MagicFx_EndWithChildren) by +2, unchecked.
S07_EXPORT void __cdecl WarShoutBuff_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::WarShoutBuff_Start, bof3::addr::MagicFx_EndWithChildren};
    Dispatch(kSteps, 2, Sc()[2], "WarShoutBuff_Run");
}

// original 0x4A4BE0: MagicFx_BuffPopup(+4, +3) - kind 2, the side's index;
// +0xB 1, +2 on.
S07_EXPORT void __cdecl WarShoutBuff_Start(void) {
    const unsigned char* const s = Sc();
    MH_CALL(MagicFx_BuffPopup)(s[4], s[3]);
    Sc()[0xB] = 1;
    Inc(Sc()[2]);
}

void WarShoutMote_Inject() {
    BOF3_INJECT(WarShout_Task);
    BOF3_INJECT(WarShout_Start);
    BOF3_INJECT(WarShout_Rally);
    BOF3_INJECT(WarShoutChild_Task);
    BOF3_INJECT(WarShoutMote_Run);
    BOF3_INJECT(WarShoutMote_Appear);
    BOF3_INJECT(WarShoutMote_Rise);
    BOF3_INJECT(WarShoutMote_Circle);
    BOF3_INJECT(WarShoutMote_Fade);
    BOF3_INJECT(WarShoutBuff_Run);
    BOF3_INJECT(WarShoutBuff_Start);
}
