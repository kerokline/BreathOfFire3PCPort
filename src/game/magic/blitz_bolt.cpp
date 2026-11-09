// Moved here by tools/move_funcs.py from magic_s03.cpp (docs/source-layout-round.md).
#include "game/magic/blitz_bolt.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s03.h"
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

// CLUT row 26 of Gfx_ClutStrip (and of its source), in words.
constexpr unsigned kRow26 = 0x1A00;

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned ActorByte() { return static_cast<unsigned>(Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }

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

std::int32_t Sub(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b));
}

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }

// `shl n` then `sar m` on a dword.
int ShlSar(std::uint32_t v, unsigned n, unsigned m) { return static_cast<int>(v << n) >> m; }

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

unsigned char* EnemyRecord(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

using Fn2 = void (__cdecl*)(unsigned, unsigned);

void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_WORD(Fn2, address)(a, b); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr ::bof3::image::CodeVa kTurnOffset{bof3::addr::Battle_TurnVectorC};

using TaskFn = void (__cdecl*)(unsigned char*);

void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset.v)(task); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

float AsFloat(std::int32_t v) { return static_cast<float>(v); }

void LinkAtSprite(int dy, unsigned size) {
    const unsigned char* const s = Sc();
    const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(ts->x), static_cast<unsigned long>(ts->z), dy,
                                size);
}

// A blade's direction and speed from its task's +0xC..: the owner's position
// + the pair 0x446770 turned, the height the owner's + 0x100, +0xB Rand & 7,
// +9 0x10, +0xA 0, +2 on (BlitzBolt_Start and _Next, which differ in the
// launch distance).
void LaunchBolt(std::int32_t distance) {
    Sc()[8] = Owner()[8];
    rv::Current<records::BattleTask>()->step_x = distance;
    rv::Current<records::BattleTask>()->step_z = 0;
    Turn(Sc());
    rv::Store(rv::Current<records::BattleTask>()->x, Add(rv::As<records::SpriteHead>(Owner())->x, rv::Current<records::BattleTask>()->step_x));
    rv::Store(rv::Current<records::BattleTask>()->z, Add(rv::As<records::SpriteHead>(Owner())->z, rv::Current<records::BattleTask>()->step_z));
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, rv::As<records::SpriteHead>(Owner())->height_whole + 0x100u);
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 7);
    Sc()[9] = 0x10;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// The record Blitz's bolt flies at: its +4 an enemy index when the target
// byte has 0x40, else a party index.
unsigned char* BoltRecord(const unsigned char* s) {
    return TargetByte() & 0x40 ? EnemyRecord(s[4]) : PartyRecord(s[4]);
}

// CLUT row 26's words `from`..`to` - 1 from the source with the
// semi-transparency bit set.
void Row26Stp(unsigned from, unsigned to) {
    for (unsigned k = kRow26 + from; k < kRow26 + to; ++k)
        Gfx_ClutStrip[k] = static_cast<unsigned short>(Gfx_ClutStripSource[k] | 0x8000);
}

}  // namespace

#define S03_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x49E000: the kind-2 task. A two-entry stack table by +1:
// Blitz_Start, Blitz_End.
S03_EXPORT void __cdecl Blitz_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Blitz_Start, bof3::addr::Blitz_End};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("Blitz_Task", phase, 2);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x49E030: the owner's direction and position; +0xB 0; one bolt
// (kind 1, 0x37) per actor of the targeted side not out - the eight enemies
// (battle index i + 3) when the target byte has 0x40, else the three party
// members: +0x80 this task, +3 the battle index, +4 the side's index, +9 1 +
// 8 per bolt made, counted in +0xB. CLUT row 26 with its STP bits, but its
// first word; +1 on.
S03_EXPORT void __cdecl Blitz_Start(void) {
    Sc()[8] = Owner()[8];
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0xB] = 0;
    const bool enemies = (TargetByte() & 0x40) != 0;
    unsigned char shade_step = 1;
    const unsigned count = enemies ? 8 : 3;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned index = enemies ? i + 3 : i;
        if (MH_CALL(Battle_ActorIsOut)(index) != 0) continue;
        const unsigned slot = NewTask(0x37);
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[3] = static_cast<unsigned char>(index);
        child[4] = static_cast<unsigned char>(i);
        child[9] = shade_step;
        shade_step = static_cast<unsigned char>(shade_step + 8);
        Inc(s[0xB]);
    }
    Row26Stp(0, 0x100);
    const unsigned short first = Gfx_ClutStripSource[kRow26];
    Gfx_ClutStripDirty = 1;
    Gfx_ClutStrip[kRow26] = first;
    Inc(Sc()[1]);
}

// original 0x49E1C0: once every bolt has ended (+0xB 0): the acting actor's
// word +0x98 (party) / +0xA4 (enemy, battle index - 3) halved, 1 if that made
// it 0; the effect's done flag; the task freed.
S03_EXPORT void __cdecl Blitz_End(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned a = ActorByte();
    unsigned char* const word = a < 3 ? PartyRecord(a) + 0x98 : EnemyByBattleIndex(a) + 0xA4;
    SetWord(word, Word(word) >> 1);
    if (Word(word) == 0) SetWord(word, 1);
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49E240: the bolt's kind-1 task, a jmp through
// BlitzBolt_TaskTable (one entry) by +1, unchecked.
S03_EXPORT void __cdecl BlitzBolt_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("BlitzBolt_Task", phase, 1);
    magic_harness::Phase(bof3::addr::BlitzBolt_Run)();
}

// original 0x49E260: a jmp through BlitzBolt_Steps (twelve entries) by +2,
// unchecked: _Start, then (_Seek, _Bounce, _Next) three times, MAGIC058's
// 0x4AF490, _Drift.
S03_EXPORT void __cdecl BlitzBolt_Run(void) {
    static constexpr std::uint32_t kSteps[12] = {
        bof3::addr::BlitzBolt_Start, bof3::addr::BlitzBolt_Seek,         bof3::addr::BlitzBolt_Bounce,
        bof3::addr::BlitzBolt_Next,  bof3::addr::BlitzBolt_Seek,         bof3::addr::BlitzBolt_Bounce,
        bof3::addr::BlitzBolt_Next,  bof3::addr::BlitzBolt_Seek,         bof3::addr::BlitzBolt_Bounce,
        bof3::addr::BlitzBolt_Next,  bof3::addr::MagicFx_UncountAndFree, bof3::addr::BlitzBolt_Drift};
    const unsigned phase = Sc()[2];
    if (phase >= 12) PastTable("BlitzBolt_Run", phase, 12);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x49E280: +9 down; at 0 the bolt 0x20000 in front of the owner
// (LaunchBolt), +2 on.
S03_EXPORT void __cdecl BlitzBolt_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    LaunchBolt(0x20000);
}

// original 0x49E340: +0xA up on odd frames; a step (speed 0x40) toward its
// actor's point (the record by +4, the enemies' when the target byte has
// 0x40) at (height + 0xC0) / 2. If that actor is out (Battle_ActorIsOut of
// +3): its point into +0xC..+0x14 and +2 to 11 (_Drift). At the actor
// (MagicFx_NearSprite, 0x8000): byte 0x904AA9 bit 0x20, +3 flagged 0x40,
// sound 0x203, the heading back to the owner in +0xC, +0x14 0x40, +0x20 -8,
// +2 on. Then the screen point and the bolt (BlitzBolt_Draw(+0xB, 0)).
S03_EXPORT void __cdecl BlitzBolt_Seek(void) {
    if (Frame_Counter & 1) Inc(Sc()[0xA]);
    unsigned char* s = Sc();
    unsigned char* const record = BoltRecord(s);
    const int y = (S16(record + 0x3E) + 0xC0) >> 1;
    const int x = (Long(record + 0x34) >> 9) - 0x4000;
    const int z = (Long(record + 0x38) >> 9) - 0x4000;
    if (MH_CALL(Battle_ActorIsOut)(s[3]) != 0) {
        rv::Store(rv::Current<records::BattleTask>()->step_x, (Long(record + 0x34) >> 9) - 0x4000);
        rv::Store(rv::Current<records::BattleTask>()->step_z, (Long(record + 0x38) >> 9) - 0x4000);
        rv::Store(rv::Current<records::BattleTask>()->step_y, (S16(record + 0x3E) + 0xC0) >> 1);
        Sc()[2] = 0xB;
    }
    // the fourth word is the original's uninitialised stack; the callee reads none of it
    MH_CALL(MagicFx_StepTowardPoint)(static_cast<unsigned>(x), static_cast<unsigned>(z), static_cast<unsigned>(y), 0, 0x40);
    if (MH_CALL(MagicFx_NearSprite)(record, 0x8000) != 0) {
        Mem(::bof3::Img(0x904AA9))[0] |= 0x20;
        MH_CALL(Battle_SetTargetFlag40)(Sc()[3]);
        MH_CALL(Sound_PlayById)(0x203);
        const unsigned char* const owner = Owner();
        s = Sc();
        const std::int32_t dz = Sub(rv::As<records::SpriteHead>(owner)->z, rv::As<records::BattleTask>(s)->z);
        const std::int32_t dx = Sub(rv::As<records::SpriteHead>(owner)->x, rv::As<records::BattleTask>(s)->x);
        const int heading = MH_CALL(Math_Ratan2)(AsFloat(dx), AsFloat(dz));
        rv::Current<records::BattleTask>()->step_x = heading;
        rv::Store(rv::Current<records::BattleTask>()->step_y, 0x40);
        rv::Store(rv::Current<records::BattleTask>()->work_20, -8);
        Inc(Sc()[2]);
    }
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call2(bof3::addr::BlitzBolt_Draw, Sc()[0xB], 0);
}

// original 0x49E4F0: +0xA up; the position on by 2 x (sin, cos) of the
// heading +0xC; the rise +0x14 on by the fall +0x20 and the height word +0x3E
// by the rise; +9 down, at 0 +2 on. Then the screen point and the bolt
// (BlitzBolt_Draw(+0xB, 1)).
S03_EXPORT void __cdecl BlitzBolt_Bounce(void) {
    Inc(Sc()[0xA]);
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        unsigned char* const px = s + 0x34;
        const int v = MH_CALL(Math_Sin)(ts->step_x);
        SetLong(px, Add(Long(px), ShlSar(static_cast<std::uint32_t>(v), 13, 12)));
    }
    {
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        unsigned char* const pz = s + 0x38;
        const int v = MH_CALL(Math_Cos)(ts->step_x);
        SetLong(pz, Add(Long(pz), ShlSar(static_cast<std::uint32_t>(v), 13, 12)));
    }
    unsigned char* const s = Sc();
    rv::Store(rv::As<records::BattleTask>(s)->step_y, Add(rv::As<records::BattleTask>(s)->step_y, rv::As<records::BattleTask>(s)->work_20));
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, rv::As<records::BattleTask>(s)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
    Dec(s[9]);
    if (s[9] == 0) Inc(s[2]);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call2(bof3::addr::BlitzBolt_Draw, Sc()[0xB], 1);
}

// original 0x49E590: if +3 is out, +2 to 10 (MAGIC058's free). Else, once its
// actor's state byte +1 is not 6 (the reaction state), the bolt again 0x10000
// in front of the owner (LaunchBolt), +2 on.
S03_EXPORT void __cdecl BlitzBolt_Next(void) {
    if (MH_CALL(Battle_ActorIsOut)(Sc()[3]) != 0) {
        Sc()[2] = 0xA;
        return;
    }
    if (BoltRecord(Sc())[1] == 6) return;
    LaunchBolt(0x10000);
}

// original 0x49E6A0: +0xA up on odd frames; a step (speed 0x30) toward the
// point _Seek left in +0xC..+0x14; +9 down, at 0 +2 back one (to 10,
// MAGIC058's free); the screen point and the bolt (BlitzBolt_Draw(+0xB, 0)).
S03_EXPORT void __cdecl BlitzBolt_Drift(void) {
    if (Frame_Counter & 1) Inc(Sc()[0xA]);
    {
        const unsigned char* const s = Sc();
        const records::BattleTask* const ts = rv::AsConst<records::BattleTask>(s);
        // the fourth word is the original's uninitialised stack; the callee reads none of it
        MH_CALL(MagicFx_StepTowardPoint)(static_cast<unsigned>(ts->step_x), static_cast<unsigned>(ts->step_z),
                                         static_cast<unsigned>(ts->step_y), 0, 0x30);
    }
    unsigned char* const s = Sc();
    Dec(s[9]);
    if (s[9] == 0) Dec(s[2]);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call2(bof3::addr::BlitzBolt_Draw, Sc()[0xB], 0);
}

// original 0x49E720: the bolt, one textured quad (tpage 0x340 / 0x100 with
// the blend `abr`, CLUT row 0x1FA) round the screen point at radius 0x24 and
// the angles (+0xA -3, -5, +3, +5) x 0x100 (the dword 0x903858 read back for
// each sine), its u the eight 32-wide cells by +0xB, its shade +9 x 8 (the
// dword 0x903854); linked at the task's point, layer offset 2. The first
// argument is not read.
S03_EXPORT void __cdecl BlitzBolt_Draw(unsigned unused, unsigned abr) {
    (void)unused;
    const unsigned e = abr & 0xFF;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, ((e & 3) << 5) | 0x15, 0);
    LinkAtSprite(2, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SetSD(0, 0x24);
    SetSD(4, static_cast<std::uint32_t>(Sc()[9]) << 3);
    MH_CALL(Gpu_SetPolyFT4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, e);
    static constexpr int kSteps[4] = {-3, -5, 3, 5};
    for (unsigned k = 0; k < 4; ++k) {
        const std::uint32_t angle = ((static_cast<std::uint32_t>(Sc()[0xA]) + static_cast<std::uint32_t>(kSteps[k])) & 0xF)
                                    << 8;
        SetSD(8, angle);
        int v = MH_CALL(Math_Cos)(static_cast<int>(angle));
        PutFloat(p + 8 + 0x10 * k, Add(Mul12(v, SD(0)), S16(Sc() + 0x2E)));
        v = MH_CALL(Math_Sin)(SD(8));
        PutFloat(p + 0xC + 0x10 * k, Add(Mul12(v, SD(0)), S16(Sc() + 0x30)));
    }
    const unsigned tpage = MH_CALL(Gpu_GetTPage)(1, e, 0x340, 0x100);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, tpage);
    const unsigned clut = MH_CALL(Gpu_GetClut)(0, 0x1FA);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, clut);
    p[0x15] = 0;
    p[0x14] = static_cast<unsigned char>(Sc()[0xB] << 5);
    p[0x25] = 0;
    p[0x24] = static_cast<unsigned char>((Sc()[0xB] << 5) + 0x1F);
    p[0x35] = 0x40;
    p[0x34] = static_cast<unsigned char>(Sc()[0xB] << 5);
    p[0x45] = 0x40;
    p[0x44] = static_cast<unsigned char>((Sc()[0xB] << 5) + 0x1F);
    p[4] = SB(4);
    p[5] = SB(4);
    p[6] = SB(4);
    LinkAtSprite(2, 0x48);
}

void BlitzBolt_Inject() {
    BOF3_INJECT(Blitz_Task);
    BOF3_INJECT(Blitz_Start);
    BOF3_INJECT(Blitz_End);
    BOF3_INJECT(BlitzBolt_Task);
    BOF3_INJECT(BlitzBolt_Run);
    BOF3_INJECT(BlitzBolt_Start);
    BOF3_INJECT(BlitzBolt_Seek);
    BOF3_INJECT(BlitzBolt_Bounce);
    BOF3_INJECT(BlitzBolt_Next);
    BOF3_INJECT(BlitzBolt_Drift);
    BOF3_INJECT(BlitzBolt_Draw);
}
