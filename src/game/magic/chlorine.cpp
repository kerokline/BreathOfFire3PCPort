// Moved here by tools/move_funcs.py from magic_s03.cpp (docs/source-layout-round.md).
#include "game/magic/chlorine.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
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

using move_script::SetWord;

using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words or dwords by function) and the
// SVECTORs of Prim_VertexScratch (0x9037A0..). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

// The overlays' own .data the functions read in place (symbols.toml [[data]]).
// kBladeRadii: now MindSword_BladeRadii (symbols.toml). MindSword_BladeRadii: four words, [1..3] read
// kCloudOffsets: now ChlorineCloud_Offsets (symbols.toml). ChlorineCloud_Offsets: three (dx, dz, dy) dwords
constexpr unsigned kCloudOffsetCount = 3;

// CLUT row 26 of Gfx_ClutStrip (and of its source), in words.
constexpr unsigned kRow26 = 0x1A00;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Src() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned ActorByte() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

std::int32_t SD(unsigned k) { return Long(Mem(::bof3::Img(kS.v) + k)); }

void SetSD(unsigned k, std::uint32_t v) { SetLong(Mem(::bof3::Img(kS.v) + k), static_cast<std::int32_t>(v)); }

short S16(const unsigned char* at) { return static_cast<short>(move_script::Word(at)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

// A dword a + b (or a - b) as the originals' add / sub: wrapping.
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records unchecked: the party's by the index, the
// enemies' by the battle index - 3 (the acting actor) or by an enemy index
// (Blitz's bolts).
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* EnemyByBattleIndex(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + (battle_index - 3u) * at::kEnemyStride);
}

unsigned char* ActorRecord(unsigned a) { return a < 3 ? PartyRecord(a) : EnemyByBattleIndex(a); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void LinkAtSprite(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy,
                                size);
}

// CLUT row 26's words `from`..`to` - 1 from the source with the
// semi-transparency bit set.
void Row26Stp(unsigned from, unsigned to) {
    for (unsigned k = kRow26 + from; k < kRow26 + to; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
}

}  // namespace

#define S03_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x49D840: the kind-2 task. A five-entry stack table by +1:
// Chlorine_Start, _Release, SpellFx_Countdown, _WaitChildren, _End.
S03_EXPORT void __cdecl Chlorine_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::Chlorine_Start, bof3::addr::Chlorine_Release,
                                                 bof3::addr::SpellFx_Countdown, bof3::addr::Chlorine_WaitChildren,
                                                 bof3::addr::Chlorine_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("Chlorine_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x49D880: +0xB 0, +1 on; the acting actor's record +0x4B 0xFF;
// BattleActor_SetAnimation(0x2C, 6); a copy (kind 1, 0x2A) of the acting
// actor's record's first 0x80 bytes (read again after the create), with +0x80
// this task, +1 1, +2 0, +6 1, +5 0x2A, counted in +0xB; CLUT row 26's words
// 0x21..0x2F with their STP bit, word 0x20 0; the owner's +0 bit 0x40.
S03_EXPORT void __cdecl Chlorine_Start(void) {
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    ActorRecord(ActorByte())[0x4B] = 0xFF;
    MH_CALL(BattleActor_SetAnimation)(0x2C, 6);
    const unsigned slot = NewTask(0x2A);
    const unsigned char* const from = ActorRecord(ActorByte());
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    // rep movsd: 32 dwords, forward
    for (unsigned k = 0; k < 0x80; k += 4) std::memmove(child + k, from + k, 4);
    unsigned char* const s = Sc();
    tchild->owner = static_cast<std::int32_t>(Key(s));
    child[1] = 1;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x2A;
    Inc(s[0xB]);
    Row26Stp(0x21, 0x30);
    Gfx_ClutStrip[kRow26 + 0x20] = 0;
    Gfx_ClutStripDirty = 1;
    Owner()[0] |= 0x40;
}

// original 0x49D9B0: once the copy is gone (+0xB 0): three clouds (kind 1,
// 0x2A, +1 0, +0xB i, +9 4i + 1) counted in +0xB; BattleActor_SetAnimation(4,
// 0); the owner's +0 bit 0x40 cleared; sound 0x100; +9 8, +1 on.
S03_EXPORT void __cdecl Chlorine_Release(void) {
    if (Sc()[0xB] != 0) return;
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned slot = NewTask(0x2A);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>((i << 2) + 1);
        Inc(s[0xB]);
    }
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[9] = 8;
    Inc(Sc()[1]);
}

// original 0x49DA50: +1 on once +0xB (the children) is 0.
S03_EXPORT void __cdecl Chlorine_WaitChildren(void) {
    unsigned char* const s = Sc();
    if (s[0xB] == 0) Inc(s[1]);
}

// original 0x49DA60: the flags word 0x904AA8 |= 0x2004; the target flagged
// 0x40; the task freed.
S03_EXPORT void __cdecl Chlorine_End(void) {
    const unsigned char target = TargetByte();
    SetWord(Mem(::bof3::Img(at::kFlags)), Word(Mem(::bof3::Img(at::kFlags))) | 0x2004u);
    MH_CALL(Battle_SetTargetFlag40)(target);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49DA80: the kind-1 task, a jmp through ChlorineChild_Kinds (two
// entries) by +1, unchecked.
S03_EXPORT void __cdecl ChlorineChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::ChlorineCloud_Run, bof3::addr::ChlorineCopy_Task};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("ChlorineChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x49DAA0: a call through ChlorineCloud_Steps (three entries) by
// +2, unchecked; then while +0 and +2 are set the cloud (a tail jmp).
S03_EXPORT void __cdecl ChlorineCloud_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::ChlorineCloud_Start, bof3::addr::ChlorineCloud_Grow,
                                                bof3::addr::EnlightenRays_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("ChlorineCloud_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    Call0(bof3::addr::ChlorineCloud_Draw);
}

// original 0x49DAD0: +9 down; at 0 the source sprite's direction; the offset
// ChlorineCloud_Offsets[+0xB] (read in place) turned by it from the source
// sprite, its dy on the source's height; the screen point; +0xA 0, +2 on.
S03_EXPORT void __cdecl ChlorineCloud_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned char* const src = Src();
    Sc()[8] = src[8];
    auto offset = [](unsigned field) {
        const unsigned i = Sc()[0xB];
        if (i >= kCloudOffsetCount) bof3::Fatal("ChlorineCloud_Start: offset %u, past the %u-entry table", i, kCloudOffsetCount);
        return Long(Mem(::bof3::Img(ChlorineCloud_Offsets_addr) + 12 * i + field));
    };
    rv::Store(rv::Current<records::BattleTask>()->step_x, offset(0));
    rv::Store(rv::Current<records::BattleTask>()->step_z, offset(4));
    Turn(Sc());
    rv::Store(rv::Current<records::BattleTask>()->x, Add(rv::Current<records::BattleTask>()->step_x, rv::As<records::SpriteHead>(src)->x));
    rv::Store(rv::Current<records::BattleTask>()->z, Add(rv::Current<records::BattleTask>()->step_z, rv::As<records::SpriteHead>(src)->z));
    {
        const std::int32_t dy = offset(8);
        rv::Store(rv::Current<records::BattleTask>()->height, Add(dy, rv::As<records::SpriteHead>(src)->height));
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x49DB90: +9 and +0xA up by 2; at a +9 of 0x10 +2 on. (Reached by
// nine overlays' tables.)
S03_EXPORT void __cdecl ChlorineCloud_Grow(void) {
    unsigned char* const s = Sc();
    s[9] = static_cast<unsigned char>(s[9] + 2);
    s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (s[9] == 0x10) Inc(s[2]);
}

// original 0x49DBC0: one semi-transparent textured quad (tpage 0x340 / 0x100,
// CLUT row 0x1FA, x 0x20) round the screen point, its corners at the angles
// 0x200, 0x600, 0xE00, 0xA00 and the radius +9 x 2 (the dword 0x903850, read
// back after every call); shade (+0xA x 10, 0, +0xA x 9); linked at the
// task's point, layer offset 2.
S03_EXPORT void __cdecl ChlorineCloud_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    LinkAtSprite(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SetSD(0, static_cast<std::uint32_t>(Sc()[9]) << 1);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    static constexpr int kAngles[4] = {0x200, 0x600, 0xE00, 0xA00};
    for (unsigned k = 0; k < 4; ++k) {
        int v = MH_CALL(Math_Cos)(kAngles[k]);
        PutFloat(p + 8 + 0x10 * k, Add(Mul12(v, SD(0)), S16(Sc() + 0x2E)));
        v = MH_CALL(Math_Sin)(kAngles[k]);
        PutFloat(p + 0xC + 0x10 * k, Add(Mul12(v, SD(0)), S16(Sc() + 0x30)));
    }
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(0, 1, 0x340, 0x100);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0x20, 0x1FA);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, clut);
    p[0x14] = 0x80;
    p[0x34] = 0x80;
    p[0x15] = 0x20;
    p[0x24] = 0xA0;
    p[0x25] = 0x20;
    p[0x35] = 0x40;
    p[0x44] = 0xA0;
    p[0x45] = 0x40;
    p[5] = 0;
    p[4] = static_cast<unsigned char>(Sc()[0xA] * 10u);
    p[6] = static_cast<unsigned char>(Sc()[0xA] * 9u);
    LinkAtSprite(2, 0x48);
}

// original 0x49DE00: the copy's kind-1 task. A four-entry stack table by +2
// (_Size, _Play, _Wait, BattleFx_FreeTask); then with +0 set the sprite's
// screen update.
S03_EXPORT void __cdecl ChlorineCopy_Task(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::ChlorineCopy_Size, bof3::addr::ChlorineCopy_Play,
                                                bof3::addr::ChlorineCopy_Wait, bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ChlorineCopy_Task", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x49DE50: +9 = BattleActor_FxSizeB(); +2 on.
S03_EXPORT void __cdecl ChlorineCopy_Size(void) {
    const unsigned char size = MH_CALL(BattleActor_FxSizeB)();
    Sc()[9] = size;
    Inc(Sc()[2]);
}

// original 0x49DE70: +9 (unless 0xFF) down, at 0 BattleActor_PlaySound(2, 5);
// the script ticked once; at its end +9 8 and +2 on.
S03_EXPORT void __cdecl ChlorineCopy_Play(void) {
    {
        unsigned char* const s = Sc();
        const unsigned char b = s[9];
        if (b != 0xFF) {
            s[9] = static_cast<unsigned char>(b - 1);
            if (Sc()[9] == 0) MH_CALL(BattleActor_PlaySound)(2, 5);
        }
    }
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x49DEC0: +9 down; at 0 the owner's +0xB down and +2 on.
S03_EXPORT void __cdecl ChlorineCopy_Wait(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

void Chlorine_Inject() {
    BOF3_INJECT(Chlorine_Task);
    BOF3_INJECT(Chlorine_Start);
    BOF3_INJECT(Chlorine_Release);
    BOF3_INJECT(Chlorine_WaitChildren);
    BOF3_INJECT(Chlorine_End);
    BOF3_INJECT(ChlorineChild_Task);
    BOF3_INJECT(ChlorineCloud_Run);
    BOF3_INJECT(ChlorineCloud_Start);
    BOF3_INJECT(ChlorineCloud_Grow);
    BOF3_INJECT(ChlorineCloud_Draw);
    BOF3_INJECT(ChlorineCopy_Task);
    BOF3_INJECT(ChlorineCopy_Size);
    BOF3_INJECT(ChlorineCopy_Play);
    BOF3_INJECT(ChlorineCopy_Wait);
}
