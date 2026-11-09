// Four enemy-skill overlays compiled into the exe, round nine group C2
// (docs/magic_c2.md): the PSX's MAGIC057, MAGIC081, MAGIC116 and
// MAGIC129.EMI, Magic_Rows rows 84, 85, 59 and 145. Read one id down
// (docs/cut-content.md section 2) they are TCRF's Bone Dance, RottenBreath,
// UtmostAttack and Holocaust, which TCRF lists among the enemy-only skills;
// the names below use those labels as hypotheses and say what the code does.
//
//   - MAGIC057 0x4AE7C0..0x4AF035: the caster's animation 3, a camera shake of
//     three beats, then 32 falling bones (kind-1 tasks 0x41) that land on the
//     map's elevation, and a follower kind no creator here starts;
//   - MAGIC081 0x4BF8D0..0x4C01E6: a cloud task that emits up to 48 motes
//     (the pool 0x68E1B8) which swirl and fly at a task kept on the target
//     side's centre, each a textured quad;
//   - MAGIC116 0x4D9AE0..0x4D9F35: 0x78 frames of streaks, eight a frame from a
//     pool of 128 (0x698EC0), each a gouraud quad between two moving points
//     projected through Capcom's unnamed 0x494110;
//   - MAGIC129 0x4E3260..0x4E4416: a beam from the acting actor's enemy record
//     to each party member present, drawn as a band of gouraud quads, and
//     sparks (the pool 0x6A2D38) rising round it.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// stack table aborts where the original would call through whatever follows it
// (docs/magic_fx_reached.md section 3, the precedent). The .data dispatches are
// read in place and unchecked, as the originals read them.
#include "game/magic_c2.h"

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
namespace addr = bof3::addr;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, words by function) and the first two
// words of Prim_VertexScratch (0x9037A0). Both are read again after every
// call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).
// The sprite frame-offset table pointer (magic_s17.cpp's kFrameSet): the
// bones switch it to 0x8E3580 around their phase and put back 0x8B3580.
constexpr ::bof3::image::Va kFrameSet{0x9039D8};
constexpr ::bof3::image::Va kBeamTypes{0x65BC30};        // HolocaustBeam_Types: by +1 (one entry used)
constexpr ::bof3::image::Va kBeamPhases{0x65BC34};       // HolocaustBeam_Phases: four entries by +2
constexpr ::bof3::image::Va kSparkTypes{0x65BC44};       // HolocaustSpark_Types: by +1 (one entry used)
constexpr ::bof3::image::Va kSparkPhases{0x65BC48};      // HolocaustSpark_Phases: three entries by +2
constexpr ::bof3::image::Va kSparks{0x6A2D38};           // Holocaust_Sparks: 64 records of 0x84
constexpr std::uint32_t kRecord = 0x84;
// Other units' functions (docs/magic_c2.md section 6) are ours now and called
// by name: the engine's MagicFx_WaitOwnerAnim, S38's MagicFx_CountDownFlag10,
// S37's MagicFx_FreeCurrentRecord.

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
// [[0x93B8C4] + 0x80]: the current slot's owner - the caster's sprite.
unsigned char* Caster() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(rv::As<records::BattleTask>(Pointer(::bof3::Img(BattleFlow_TaskCurrent_addr)))->owner))));
}
unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
std::uint32_t Key(const void* p) { return Low32(p); }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
int SS(unsigned k) { return static_cast<short>(SW(k)); }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
int VS(unsigned k) { return static_cast<short>(Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k))); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
void AddLong(unsigned char* at, std::int32_t v) { SetLong(at, Add(Long(at), v)); }
// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl n` then `sar 0xC`.
int Shl12(int v, unsigned n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(v) << n) >> 12; }

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

int RandCall() { return MH_CALL(Rand)(); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter); }
int Sin(int a) { return MH_CALL(Math_Sin)(a); }
int Cos(int a) { return MH_CALL(Math_Cos)(a); }
void CopyPosition(unsigned char* to, const unsigned char* from) {
    rv::Store(rv::As<records::SpriteHead>(to)->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::As<records::SpriteHead>(to)->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::As<records::SpriteHead>(to)->height, rv::As<records::SpriteHead>(from)->height);
}

// This group's functions called by address, as the originals call them: in the
// game the jmp Inject put there (or Capcom's code under BOF3X_ORIGINAL), in
// the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using FnB = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}
// A stack table's dispatch: the phase byte through its entries.
void Step(const std::uint32_t* table, unsigned entries, unsigned phase, const char* who) {
    if (phase >= entries) PastTable(who, phase, entries);
    magic_harness::Phase(table[phase])();
}
// A .data table's dispatch, read in place and unchecked.
void CallCell(std::uint32_t table, unsigned index) {
    reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))))();
}

// A task's pool walk: every record with bit 0 of +0 becomes the current task,
// its +0x80 the owner, for `fn`; both put back after each. The two are read
// after the task's own phase.
void Walk(std::uint32_t pool, unsigned count, std::uint32_t fn) {
    unsigned char* const self = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kRecord);
        if (!(r[0] & 1)) continue;
        const std::int32_t its = Long(r + 0x80);
        Sprite_Current = r;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), its);
        Call0(fn);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = self;
    }
}

// The first record of a pool without bit 0 gets it; its index, 0xFF when full.
unsigned char Alloc(std::uint32_t pool, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* const r = Mem(pool + i * kRecord);
        if (r[0] & 1) continue;
        r[0] = static_cast<unsigned char>(r[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// The caster's animation, run with Sprite_Current its sprite and put back.
void CasterAnimation(unsigned animation) {
    unsigned char* const caster = Caster();
    unsigned char* const self = Sc();
    Sprite_Current = caster;
    MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(animation));
    Sprite_Current = self;
}

}  // namespace

#define C2_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC057 (row 84, Bone Dance read one id down)

// ===========================================================================
// MAGIC081 (row 85, RottenBreath read one id down)

// ===========================================================================
// MAGIC116 (row 59, UtmostAttack read one id down)

// ===========================================================================
// MAGIC129 (row 145, Holocaust read one id down)

// original 0x4E3260: the kind-2 task. A three-entry stack table by +1 -
// Holocaust_Start, MAGIC226/227's 0x4F9F70 (+9 down, then target flag 0x10),
// BattleFx_Finish - then the walk of the 64 sparks.
C2_EXPORT void __cdecl Holocaust_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {addr::Holocaust_Start, addr::MagicFx_CountDownFlag10,
                                                 addr::BattleFx_Finish};
    Step(kPhases, 3, Sc()[1], "Holocaust_Task");
    Walk(::bof3::Img(kSparks.v), 64, addr::HolocaustSpark_Task);
}

// original 0x4E32E0: the sparks' +0..+2 cleared; the task at the target side's
// centre; +0xB 0, +9 0x38, +1 on. With the target's side bit 0x40 (the
// enemies) +1 2 and nothing more; else a beam (kind 1, parameter 0x6A) for each
// party member 0..2 with bit 0 of its record's +0: this task its owner, +1 0,
// +4 the member, +0xB the member << 4, +9 0x3C, counted in +0xB.
C2_EXPORT void __cdecl Holocaust_Start(void) {
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const r = Mem(::bof3::Img(kSparks.v) + i * kRecord);
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
    }
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Sc()[9] = 0x38;
    Inc(Sc()[1]);
    if (Mem(::bof3::Img(at::kTarget))[0] & 0x40) {
        Sc()[1] = 2;
        return;
    }
    for (unsigned member = 0; member < 3; ++member) {
        if (!(Mem(::bof3::Img(ObjTrio_addr) + member * at::kPartyStride)[0] & 1)) continue;
        const unsigned slot = NewTask(0x6A) & 0xFF;
        unsigned char* const self = Sc();
        unsigned char* const t = TaskSlot(slot);
        records::BattleTask* const tt = rv::As<records::BattleTask>(t);
        tt->owner = static_cast<std::int32_t>(Key(self));
        t[1] = 0;
        t[4] = static_cast<unsigned char>(member);
        t[0xB] = static_cast<unsigned char>(member << 4);
        t[9] = 0x3C;
        Inc(self[0xB]);
    }
}

// original 0x4E33B0: the beam (kind 1, parameter 0x6A): a jmp through
// HolocaustBeam_Types by +1, read in place, unchecked.
C2_EXPORT void __cdecl HolocaustBeam_Task(void) { CallCell(::bof3::Img(kBeamTypes.v), Sc()[1]); }

// original 0x4E33D0: a call through HolocaustBeam_Phases by +2, read in place,
// unchecked; +0xB up (the band's wave); while +0 and +2 are set, the band
// (HolocaustBeam_Draw, a tail jmp).
C2_EXPORT void __cdecl HolocaustBeam_Run(void) {
    CallCell(::bof3::Img(kBeamPhases.v), Sc()[2]);
    Inc(Sc()[0xB]);
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) Call0(addr::HolocaustBeam_Draw);
}

namespace {
unsigned char* EnemyRecord(int i) {
    return Mem(static_cast<std::uint32_t>(static_cast<int>(::bof3::Img(Battle_Enemies_addr)) + i * static_cast<int>(at::kEnemyStride)));
}
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
}  // namespace

// original 0x4E3410: +9 down; at 0 the beam aims. Its far end: the task at the
// member +4 names - enemy record +4 (no - 3) when the target byte has 0x40,
// else party record +4 - one unit on in x, its screen point to the scratch
// words 0x90385C / 0x90385E. Its near end: the acting actor's enemy record
// (actor - 3, unchecked: a party actor reads below the enemies), one unit back
// in x and 0x360 higher, its screen point to 0x903858 / 0x90385A. The step
// (+0xC, +0x10) a sixteenth of the difference, the heading +0x14 Math_Ratan2 of
// it & 0xFFF; +0x5D 0x10 (the fade), +9 0x96, +0xA 0 (the band's length), +2 on;
// sound 0x100.
C2_EXPORT void __cdecl HolocaustBeam_Aim(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    unsigned char* s = Sc();
    const unsigned char* const far_end = (Mem(::bof3::Img(at::kTarget))[0] & 0x40) ? EnemyRecord(s[4]) : PartyRecord(s[4]);
    CopyPosition(s, far_end);
    AddLong(Sc() + 0x34, 0x10000);
    MH_CALL(BattleActor_UpdateScreenXY)();
    s = Sc();
    SetSW(0xC, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(0xE, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    const unsigned char* const near_end = EnemyRecord(static_cast<int>(Mem(::bof3::Img(at::kActor))[0]) - 3);
    CopyPosition(s, near_end);
    AddLong(Sc() + 0x34, -0x10000);
    AddLong(Sc() + 0x3C, 0x3600000);
    MH_CALL(BattleActor_UpdateScreenXY)();
    s = Sc();
    SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
    SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    rv::Store(rv::As<records::BattleTask>(s)->step_x, (SS(0xC) - SS(8)) / 16);
    rv::Store(rv::Current<records::BattleTask>()->step_z, (SS(0xE) - SS(0xA)) / 16);
    const int dy = SS(0xE) - SS(0xA);
    const int dx = SS(0xC) - SS(8);
    const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(dx), static_cast<float>(dy));
    rv::Store(rv::Current<records::BattleTask>()->step_y, heading & 0xFFF);
    Sc()[0x5D] = 0x10;
    Sc()[9] = 0x96;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4E3660: the band's length +0xA up by two; at 0x10 +2 on.
C2_EXPORT void __cdecl HolocaustBeam_Grow(void) {
    unsigned char* const s = Sc();
    s[0xA] = static_cast<unsigned char>(s[0xA] + 2);
    if (Sc()[0xA] == 0x10) Inc(Sc()[2]);
}

// original 0x4E3680: while +9 is above 0x10, every fourth frame a spark
// (HolocaustSpark_Alloc, unchecked) owned by the beam's owner, +1 0, its angle
// +0xB Rand & 0x1F, its delay +9 1, counted in the owner's +0xB; +9 down, at 0
// +2 on.
C2_EXPORT void __cdecl HolocaustBeam_Emit(void) {
    const unsigned char* const s = Sc();
    if (s[9] > 0x10 && (Frame_Counter & 3) == 0) {
        const unsigned k = MH_AT(FnB, addr::HolocaustSpark_Alloc)() & 0xFFu;
        unsigned char* const r = Mem(::bof3::Img(kSparks.v) + k * kRecord);
        SetLong(r + 0x80, Long(Mem(::bof3::Img(BattleE2_Owner_addr))));
        r[1] = 0;
        r[0xB] = static_cast<unsigned char>(RandCall() & 0x1F);
        unsigned char* const owner = Owner();
        r[9] = 1;
        Inc(owner[0xB]);
    }
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4E3710: the fade +0x5D down; at 0 the owner's count down and free.
C2_EXPORT void __cdecl HolocaustBeam_Fade(void) {
    Dec(Sc()[0x5D]);
    if (Sc()[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

namespace {
// The band's quads: the ends' colours set to 1 at the band's first segment
// (i 1) and its last (i +0xA).
void BandEnds(unsigned char* p, unsigned char i) {
    if (i == Sc()[0xA]) {
        for (unsigned k = 0; k < 3; ++k) {
            p[0x24 + k] = 1;
            p[0x34 + k] = 1;
        }
    }
    if (i == 1) {
        for (unsigned k = 0; k < 3; ++k) {
            p[4 + k] = 1;
            p[0x14 + k] = 1;
        }
    }
}
unsigned char* NewG4() {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyG4)(p);
    MH_CALL(Gpu_SetSemiTrans)(p, 1);
    return p;
}
// The angle pair's first: (the dword 0x903854 +- 0x400) & 0xFFF; its second:
// (the word 0x903856 +- 0x400) & 0xFFF.
int Across54(int delta) {
    return static_cast<int>((static_cast<std::uint32_t>(Long(Mem(::bof3::Img(kS.v) + 4))) + static_cast<std::uint32_t>(delta)) & 0xFFF);
}
int Across56(int delta) { return static_cast<int>((SW(6) + static_cast<unsigned>(delta)) & 0xFFF); }
}  // namespace

// original 0x4E3740: the band. Tpage 0x35 committed at 2; the scratch words:
// 0x903850 / 0x903852 the previous and this segment's half-width, 0x903854 /
// 0x903856 their headings (the first (+0x14 - 0x800) & 0xFFF), 0x903858 /
// 0x90385A this segment's centre, 0x90385C / 0x90385E the previous one's (the
// first the beam's screen point, +0x2E / +0x30). For i 1..+0xA (read again each
// time): the half-width 8 + sin(i << 7) >> 7; the centre the screen point + i x
// the step (+0xC, +0x10, 16-bit), waved across the heading + 0x400 by
// sin((+0xB + i) & 0x3F << 6) x (sin(i << 7) x 48 >> 12) >> 12; the heading
// Math_Ratan2 from the previous centre; then four semi-transparent gouraud
// quads between the two segments (inner and outer edge, each side), colours
// +0x5D x 15 / x 12 / x 6 fading to 1 at the rims and at the band's two ends,
// committed at 2, 0x44 bytes each. Then tpage 0x15 committed at 2.
C2_EXPORT void __cdecl HolocaustBeam_Draw(void) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    {
        const unsigned char* const s = Sc();
        const unsigned char fade = s[0x5D];
        SetSW(2, 8);
        const auto c15 = static_cast<unsigned char>(fade * 15), c12 = static_cast<unsigned char>(fade * 12),
                   c6 = static_cast<unsigned char>(fade * 6);
        SetSW(6, (Word(s + 0x14) - 0x800u) & 0xFFF);
        SetSW(8, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_x));
        SetSW(0xA, static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
        if (static_cast<int>(s[0xA]) + 1 > 1) {
            unsigned char i = 1;
            do {
                SetSW(0, SW(2));
                int v = Sin(static_cast<int>(i) << 7);
                const std::uint16_t prev_x = SW(8), prev_y = SW(0xA);
                SetSW(2, static_cast<unsigned>(Shl12(v, 5) + 8));
                SetSW(0xC, prev_x);
                const unsigned char* t = Sc();
                SetSW(0xE, prev_y);
                SetSW(8, static_cast<unsigned>(Word(t + 0xC) * i + static_cast<std::uint16_t>(rv::As<records::BattleTask>(t)->screen_x)));
                SetSW(0xA, static_cast<unsigned>(Word(t + 0x10) * i + Word(t + 0x30)));
                const int across = static_cast<int>((Word(t + 0x14) + 0x400u) & 0xFFF);
                v = Sin(static_cast<int>(i) << 7);
                const int swing = static_cast<std::int32_t>(static_cast<std::uint32_t>(v) * 48u) >> 12;
                t = Sc();
                v = Sin(static_cast<int>(((static_cast<unsigned>(t[0xB]) + i) & 0x3F) << 6));
                const int wave = static_cast<short>(Mul12(v, static_cast<short>(swing)));
                v = Sin(across);
                SetSW(8, SW(8) + static_cast<unsigned>(Mul12(v, wave)));
                v = Cos(across);
                const auto y = static_cast<std::uint16_t>(SW(0xA) + static_cast<unsigned>(Mul12(v, wave)));
                SetSW(4, SW(6));
                const int ex = SS(0xE) - static_cast<short>(y);
                SetSW(0xA, y);
                const int ey = SS(0xC) - SS(8);
                const int heading = MH_CALL(Math_Ratan2)(static_cast<float>(ey), static_cast<float>(ex));
                unsigned char* p = Gfx_PacketNext;
                SetSW(6, static_cast<unsigned>(heading) & 0xFFF);
                MH_CALL(Gpu_SetPolyG4)(p);
                MH_CALL(Gpu_SetSemiTrans)(p, 1);

                // 1: from each centre out by 8 across the heading + 0x400
                int a = Across54(0x400);
                v = Sin(a);
                PutFloat(p + 0x18, Shl12(v, 3) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0x1C, Shl12(v, 3) + SS(0xE));
                a = Across56(0x400);
                v = Sin(a);
                PutFloat(p + 0x38, Shl12(v, 3) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x3C, Shl12(v, 3) + SS(0xA));
                PutFloat(p + 8, SS(0xC));
                PutFloat(p + 0xC, SS(0xE));
                PutFloat(p + 0x28, SS(8));
                PutFloat(p + 0x2C, SS(0xA));
                for (unsigned k = 0; k < 3; ++k) {
                    p[4 + k] = c15;
                    p[0x24 + k] = c15;
                }
                p[0x14] = c12;
                p[0x15] = c6;
                p[0x16] = c15;
                p[0x34] = c12;
                p[0x35] = c6;
                p[0x36] = c15;
                BandEnds(p, i);
                MH_CALL(Gfx_CommitPrim)(2, 0x44);

                // 2: from 8 out to the half-width, the heading + 0x400 side
                p = NewG4();
                a = Across54(0x400);
                v = Sin(a);
                PutFloat(p + 0x18, Mul12(v, SS(0)) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0x1C, Mul12(v, SS(0)) + SS(0xE));
                v = Sin(a);
                PutFloat(p + 8, Shl12(v, 3) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0xC, Shl12(v, 3) + SS(0xE));
                a = Across56(0x400);
                v = Sin(a);
                PutFloat(p + 0x38, Mul12(v, SS(2)) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x3C, Mul12(v, SS(2)) + SS(0xA));
                v = Sin(a);
                PutFloat(p + 0x28, Shl12(v, 3) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x2C, Shl12(v, 3) + SS(0xA));
                p[4] = c12;
                p[5] = c6;
                p[6] = c15;
                p[0x24] = c12;
                p[0x25] = c6;
                p[0x26] = c15;
                for (unsigned k = 0; k < 3; ++k) {
                    p[0x14 + k] = 1;
                    p[0x34 + k] = 1;
                }
                BandEnds(p, i);
                MH_CALL(Gfx_CommitPrim)(2, 0x44);

                // 3: the centre out by 8 across the heading - 0x400
                p = NewG4();
                a = Across54(-0x400);
                v = Sin(a);
                PutFloat(p + 8, Shl12(v, 3) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0xC, Shl12(v, 3) + SS(0xE));
                a = Across56(-0x400);
                v = Sin(a);
                PutFloat(p + 0x28, Shl12(v, 3) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x2C, Shl12(v, 3) + SS(0xA));
                PutFloat(p + 0x18, SS(0xC));
                PutFloat(p + 0x1C, SS(0xE));
                PutFloat(p + 0x38, SS(8));
                PutFloat(p + 0x3C, SS(0xA));
                for (unsigned k = 0; k < 3; ++k) {
                    p[0x14 + k] = c15;
                    p[0x34 + k] = c15;
                }
                p[4] = c12;
                p[5] = c6;
                p[6] = c15;
                p[0x24] = c12;
                p[0x25] = c6;
                p[0x26] = c15;
                BandEnds(p, i);
                MH_CALL(Gfx_CommitPrim)(2, 0x44);

                // 4: from the half-width in to 8, the heading - 0x400 side
                p = NewG4();
                a = Across54(-0x400);
                v = Sin(a);
                PutFloat(p + 8, Mul12(v, SS(0)) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0xC, Mul12(v, SS(0)) + SS(0xE));
                v = Sin(a);
                PutFloat(p + 0x18, Shl12(v, 3) + SS(0xC));
                v = Cos(a);
                PutFloat(p + 0x1C, Shl12(v, 3) + SS(0xE));
                a = Across56(-0x400);
                v = Sin(a);
                PutFloat(p + 0x28, Mul12(v, SS(2)) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x2C, Mul12(v, SS(2)) + SS(0xA));
                v = Sin(a);
                PutFloat(p + 0x38, Shl12(v, 3) + SS(8));
                v = Cos(a);
                PutFloat(p + 0x3C, Shl12(v, 3) + SS(0xA));
                p[0x14] = c12;
                p[0x15] = c6;
                p[0x16] = c15;
                p[0x34] = c12;
                p[0x35] = c6;
                p[0x36] = c15;
                for (unsigned k = 0; k < 3; ++k) {
                    p[4 + k] = 1;
                    p[0x24 + k] = 1;
                }
                BandEnds(p, i);
                MH_CALL(Gfx_CommitPrim)(2, 0x44);
                ++i;
            } while (static_cast<int>(i) < static_cast<int>(Sc()[0xA]) + 1);
        }
    }
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
}

// original 0x4E4000: a spark (a pool record): a jmp through HolocaustSpark_Types
// by +1, read in place, unchecked.
C2_EXPORT void __cdecl HolocaustSpark_Task(void) { CallCell(::bof3::Img(kSparkTypes.v), Sc()[1]); }

// original 0x4E4020: a call through HolocaustSpark_Phases by +2, read in place,
// unchecked; while +0 and +2 are set, tpage 0x35, the spark's screen point and
// its disc (HolocaustSpark_Draw), tpage 0x15 - each draw mode linked at the
// spark's x, z.
C2_EXPORT void __cdecl HolocaustSpark_Run(void) {
    CallCell(::bof3::Img(kSparkPhases.v), Sc()[2]);
    const unsigned char* s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(rv::As<records::BattleTask>(s)->x), static_cast<unsigned long>(rv::As<records::BattleTask>(s)->z), 2, 0xC);
    MH_CALL(BattleActor_UpdateScreenXY)();
    Call0(addr::HolocaustSpark_Draw);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(rv::As<records::BattleTask>(s)->x), static_cast<unsigned long>(rv::As<records::BattleTask>(s)->z), 2, 0xC);
}

// original 0x4E40B0: +9 (the delay) down; at 0 the spark starts: rising (+0x14
// 0x40000, +0x20 0x20000), at the owner's position moved by its angle (+0xB &
// 0x1F) << 7 times a radius Rand & 0xF + 0x1C (sin and cos unshifted), the
// owner's height; colours +0x5D..+0x5F each Rand & 7 + 5; +9 and +0xA 0x10; +2 on.
C2_EXPORT void __cdecl HolocaustSpark_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x40000);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 0x20000);
    const std::uint32_t roll = static_cast<std::uint32_t>(RandCall());
    const int radius = static_cast<short>((roll & 0xF) + 0x1C);
    const int angle = static_cast<short>((Sc()[0xB] & 0x1F) << 7);
    int v = Sin(angle);
    rv::Store(rv::Current<records::BattleTask>()->x, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(radius)),
                             rv::As<records::SpriteHead>(Owner())->x));
    v = Cos(angle);
    rv::Store(rv::Current<records::BattleTask>()->z, Add(static_cast<std::int32_t>(static_cast<std::uint32_t>(v) * static_cast<std::uint32_t>(radius)),
                             rv::As<records::SpriteHead>(Owner())->z));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Sc()[0x5D] = static_cast<unsigned char>((RandCall() & 7) + 5);
    Sc()[0x5E] = static_cast<unsigned char>((RandCall() & 7) + 5);
    Sc()[0x5F] = static_cast<unsigned char>((RandCall() & 7) + 5);
    Sc()[9] = 0x10;
    Sc()[0xA] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4E41B0: +0x14 up by +0x20, the height +0x3C by +0x14; +9 down, at 0
// +2 on.
C2_EXPORT void __cdecl HolocaustSpark_Rise(void) {
    unsigned char* s = Sc();
    AddLong(s + 0x14, rv::As<records::BattleTask>(s)->work_20);
    s = Sc();
    AddLong(s + 0x3C, rv::As<records::BattleTask>(s)->step_y);
    Dec(Sc()[9]);
    if (Sc()[9] == 0) Inc(Sc()[2]);
}

// original 0x4E41F0: rising as above; the fade +0xA down, at 0 the owner's
// count down and MAGIC219's 0x4F6290 (+0..+4 cleared: the record free), a tail
// jmp.
C2_EXPORT void __cdecl HolocaustSpark_Fade(void) {
    unsigned char* s = Sc();
    AddLong(s + 0x14, rv::As<records::BattleTask>(s)->work_20);
    s = Sc();
    AddLong(s + 0x3C, rv::As<records::BattleTask>(s)->step_y);
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    Call0(addr::MagicFx_FreeCurrentRecord);
}

// original 0x4E4240: a disc of eight semi-transparent gouraud triangles round
// the spark's screen point, radius 32 (sin << 5 >> 12), the rim point kept in
// Prim_VertexScratch's first two words from one triangle to the next; the
// centre colour +0x5D..+0x5F x the fade +0xA, the rim 1; each linked at the
// spark's x, z (0x34 bytes).
C2_EXPORT void __cdecl HolocaustSpark_Draw(void) {
    int v = Sin(0);
    SetVW(0, static_cast<unsigned>(Shl12(v, 5)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
    v = Cos(0);
    const unsigned char* s = Sc();
    SetVW(2, static_cast<unsigned>(Shl12(v, 5)) + static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->screen_y));
    const unsigned char fade = s[0xA];
    const auto red = static_cast<unsigned char>(s[0x5D] * fade), green = static_cast<unsigned char>(s[0x5E] * fade),
               blue = static_cast<unsigned char>(s[0x5F] * fade);
    int angle = 0x200;
    for (unsigned n = 0; n < 8; ++n, angle += 0x200) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG3)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, static_cast<short>(static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x)));
        PutFloat(p + 0xC, static_cast<short>(static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y)));
        PutFloat(p + 0x18, VS(0));
        PutFloat(p + 0x1C, VS(2));
        v = Sin(angle);
        const auto x = static_cast<std::uint16_t>(static_cast<unsigned>(Shl12(v, 5)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_x));
        SetVW(0, x);
        PutFloat(p + 0x28, static_cast<short>(x));
        v = Cos(angle);
        const auto y = static_cast<std::uint16_t>(static_cast<unsigned>(Shl12(v, 5)) + static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->screen_y));
        PutFloat(p + 0x2C, static_cast<short>(y));
        SetVW(2, y);
        p[4] = red;
        p[5] = green;
        p[6] = blue;
        for (unsigned k = 0; k < 3; ++k) {
            p[0x14 + k] = 1;
            p[0x24 + k] = 1;
        }
        s = Sc();
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(rv::As<records::BattleTask>(s)->x), static_cast<unsigned long>(rv::As<records::BattleTask>(s)->z), 2,
                                    0x34);
    }
}

// original 0x4E43C0: the first of the 64 sparks without bit 0 of +0 gets it; its
// index in al, 0xFF when all are taken.
C2_EXPORT unsigned char __cdecl HolocaustSpark_Alloc(void) { return Alloc(::bof3::Img(kSparks.v), 64); }

void MagicC2_Inject() {
    if (bof3::WantsShadow("magic_c2")) magic_c2::SelfTest();
    BOF3_INJECT(Holocaust_Task);
    BOF3_INJECT(Holocaust_Start);
    BOF3_INJECT(HolocaustBeam_Task);
    BOF3_INJECT(HolocaustBeam_Run);
    BOF3_INJECT(HolocaustBeam_Aim);
    BOF3_INJECT(HolocaustBeam_Grow);
    BOF3_INJECT(HolocaustBeam_Emit);
    BOF3_INJECT(HolocaustBeam_Fade);
    BOF3_INJECT(HolocaustBeam_Draw);
    BOF3_INJECT(HolocaustSpark_Task);
    BOF3_INJECT(HolocaustSpark_Run);
    BOF3_INJECT(HolocaustSpark_Start);
    BOF3_INJECT(HolocaustSpark_Rise);
    BOF3_INJECT(HolocaustSpark_Fade);
    BOF3_INJECT(HolocaustSpark_Draw);
    BOF3_INJECT(HolocaustSpark_Alloc);
}
