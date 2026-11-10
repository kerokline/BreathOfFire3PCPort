// Moved here by tools/move_funcs.py from magic_s26.cpp (docs/source-layout-round.md).
#include "game/magic/magic_115.h"

#include <cstdint>

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

namespace addr = bof3::addr;

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

// SharedScratch's first dword (0x903850; a word at +0 and +2 in the beams)
// and Prim_VertexScratch's four SVECTORs (0x9037A0..0x9037BF): read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* TintRecord(unsigned i) { return MoveScript_TintRecords + 12 * i; }

std::uint32_t Key(const void* p) { return Low32(p); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

std::int32_t AddL(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

std::int32_t MulL(std::int32_t a, unsigned b) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * b); }

std::int32_t Shl(std::int32_t a, unsigned n) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << n); }

void SetL(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(v)); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

}  // namespace

#define S26_EXPORT extern "C" __attribute__((disable_tail_calls))

// The sprite set-up both MAGIC115 children share (texture slot 0x1D, CLUT row
// 0xA0, scale 0x10000 both ways, colour 0), all but the animation.
static void Magic115_SpriteSetUp() {
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    SetL(Sc() + 0x40, 0x10000);
    SetL(Sc() + 0x44, 0x10000);
    Sc()[0x48] = 1;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0x24] = 4;
    Sc()[0x5D] = 0;
    Sc()[0x5E] = 0;
    Sc()[0x5F] = 0;
    Sc()[0x5C] = 0;
    Sc()[0x2A] = 0;
    Sc()[0x29] = 4;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 0;
}

// The orbit step both circling phases share: +9 up, the angle (+9 & 0x3F) <<
// 6 kept in SharedScratch's dword, the position the owner's plus 3 x (sin,
// cos) << 2.
static void Magic115_Circle() {
    Inc(Sc()[9]);
    const std::uint32_t angle = (Sc()[9] & 0x3Fu) << 6;
    SetL(Mem(::bof3::Img(kS.v)), angle);
    {
        const int sn = MH_CALL(Math_Sin)(static_cast<int>(angle));
        const unsigned char* const owner = Owner();
        rv::Store(rv::Current<records::BattleTask>()->x, AddL(Shl(MulL(sn, 3), 0xE) >> 0xC, rv::As<records::SpriteHead>(owner)->x));
    }
    {
        const int c = MH_CALL(Math_Cos)(Long(Mem(::bof3::Img(kS.v))));
        unsigned char* const s = Sc();
        records::BattleTask* const ts = rv::As<records::BattleTask>(s);
        const unsigned char* const owner = Owner();
        ts->z = AddL(Shl(MulL(c, 3), 0xE) >> 0xC, rv::As<records::SpriteHead>(owner)->z);
    }
}

// original 0x4D9440: the kind-2 task. A four-entry stack table by +1:
// Magic115_Start, Magic115_TintUp, Magic115_TintDown, BattleFx_Finish.
S26_EXPORT void __cdecl Magic115_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic115_Start, addr::Magic115_TintUp, addr::Magic115_TintDown,
                                                 addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("Magic115_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D9480: at the source sprite (read once); a child (kind 1, task
// 0x30, +1 0) owned by this task; the source's tints released and a tint (0,
// 0, 0, 1) set, kept in +0xA; CLUT row 26's first sixteen from its source with
// the STP bit, its first again without; Gfx_ClutStripDirty; sound 0x100; +0xB
// 1, +9 0, +1 on.
S26_EXPORT void __cdecl Magic115_Start(void) {
    unsigned char* const source = Source();
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(source)->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(source)->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(source)->height);
    const unsigned slot = NewTask(0x30);
    unsigned char* const child = TaskSlot(slot);
    SetL(child + 0x80, Key(Sc()));
    child[1] = 0;
    MH_CALL(Sprite_ReleaseTint)(source);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(source, 0, 0, 0, 1);
    Sc()[0xA] = tint;
    for (unsigned k = 0; k < 16; ++k)
        Gfx_ClutStrip[0x1A00 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[0x1A00 + k] | 0x8000);
    Gfx_ClutStrip[0x1A00] = Gfx_ClutStripSource[0x1A00];
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
    Sc()[0xB] = 1;
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x4D9560: the tint record +0xA's colour bytes +2..+4 = +9 x 2 (the
// child counts +9 up); from 6 on, +1 on.
S26_EXPORT void __cdecl Magic115_TintUp(void) {
    unsigned char* const s = Sc();
    TintRecord(s[0xA])[2] = static_cast<unsigned char>(s[9] << 1);
    TintRecord(s[0xA])[3] = static_cast<unsigned char>(s[9] << 1);
    TintRecord(s[0xA])[4] = static_cast<unsigned char>(s[9] << 1);
    if (s[9] >= 6) Inc(s[1]);
}

// original 0x4D95B0: on odd frames the tint record's colour down one; at red 0
// the record released (Tint_Release) and +1 on.
S26_EXPORT void __cdecl Magic115_TintDown(void) {
    unsigned char* const s = Sc();
    if ((Frame_Counter & 1) != 0) {
        Dec(TintRecord(s[0xA])[2]);
        Dec(TintRecord(s[0xA])[3]);
        Dec(TintRecord(s[0xA])[4]);
    }
    const unsigned char index = s[0xA];
    if (TintRecord(index)[2] != 0) return;
    MH_CALL(Tint_Release)(index);
    Inc(Sc()[1]);
}

// original 0x4D9630: the child's kind-1 task (parameter 0x30), a jmp through
// Magic115_ChildKinds (two entries) by +1, unchecked.
S26_EXPORT void __cdecl Magic115_ChildTask(void) {
    static constexpr std::uint32_t kKinds[2] = {addr::Magic115_OrbitRun, addr::Magic115_MoteRun};
    const unsigned kind = Sc()[1];
    if (kind >= 2) PastTable("Magic115_ChildTask", kind, 2);
    magic_harness::Phase(kKinds[kind])();
}

// original 0x4D9650: with the effects' frame-offset table, a call through
// Magic115_OrbitPhases (four entries, the last MAGIC058's 0x4AF490) by +2,
// unchecked; while +0 and +2 are set, the sprite's screen point.
S26_EXPORT void __cdecl Magic115_OrbitRun(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic115_OrbitStart, addr::Magic115_Orbit,
                                                 addr::Magic115_OrbitFade, addr::MagicFx_UncountAndFree};
    SetL(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("Magic115_OrbitRun", phase, 4);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetL(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4D9690: at the owner (height + 0x2C0), the sprite set up,
// animation 0, +9 0xFF, +2 on.
S26_EXPORT void __cdecl Magic115_OrbitStart(void) {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->height_whole, (rv::As<records::SpriteHead>(Owner())->height_whole + 0x2C0) & 0xFFFF);
    Magic115_SpriteSetUp();
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = 0xFF;
    Inc(Sc()[2]);
}

// original 0x4D9790: the sprite's script ticked; the orbit step; every eighth
// step a mote (kind 1, task 0x30, +1 1, owned by the owner, its delay +9 from
// the step) at the sprite, the owner's +9 and +0xB counting it; at step 0x40
// the sprite's bit 0x20, its colour (1, 0xC0, 0xC0, 0xC0), +2 on.
S26_EXPORT void __cdecl Magic115_Orbit(void) {
    MH_CALL(Sprite_ScriptTick)();
    Magic115_Circle();
    unsigned char* s = Sc();
    if ((s[9] & 7) == 0) {
        const unsigned slot = NewTask(0x30);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        s = Sc();
        SetL(child + 0x80, Key(owner));
        child[1] = 1;
        child[9] = static_cast<unsigned char>(((s[9] >> 2) + 2) << 2);
        tchild->x = rv::As<records::BattleTask>(s)->x;
        tchild->z = rv::As<records::BattleTask>(s)->z;
        rv::Store(tchild->height, rv::As<records::BattleTask>(s)->height);
        Inc(owner[9]);
        Inc(Owner()[0xB]);
        s = Sc();
    }
    if (s[9] != 0x40) return;
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    Sc()[0x5C] = 1;
    Sc()[0x5D] = 0xC0;
    Sc()[0x5E] = 0xC0;
    Sc()[0x5F] = 0xC0;
    Inc(Sc()[2]);
}

// original 0x4D98E0: the colour down 4; the script ticked; the orbit step;
// red 0x80, +2 on.
S26_EXPORT void __cdecl Magic115_OrbitFade(void) {
    AddB(Sc()[0x5D], 0xFC);
    AddB(Sc()[0x5E], 0xFC);
    AddB(Sc()[0x5F], 0xFC);
    MH_CALL(Sprite_ScriptTick)();
    Magic115_Circle();
    Sc()[0x5D] = 0x80;
    Inc(Sc()[2]);
}

// original 0x4D9990: the mote, with the effects' frame-offset table: a call
// through Magic115_MotePhases (four entries, the last MAGIC058's 0x4AF490) by
// +2, unchecked; while +0 and +2 are set, its screen point.
S26_EXPORT void __cdecl Magic115_MoteRun(void) {
    static constexpr std::uint32_t kPhases[4] = {addr::Magic115_MoteStart, addr::Magic115_MoteTick,
                                                 addr::Magic115_MoteFade, addr::MagicFx_UncountAndFree};
    SetL(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8E3580));
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("Magic115_MoteRun", phase, 4);
    magic_harness::Phase(kPhases[phase])();
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    SetL(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4D99D0: the sprite set up, animation 1, +2 on.
S26_EXPORT void __cdecl Magic115_MoteStart(void) {
    Magic115_SpriteSetUp();
    MH_CALL(Sprite_SetAnimation)(1);
    Inc(Sc()[2]);
}

// original 0x4D9A80: the script ticked twice; at its end (the second) +2 on by
// two (past the fade).
S26_EXPORT void __cdecl Magic115_MoteTick(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    if (MH_CALL(Sprite_ScriptTickOnce)() != 0) AddB(Sc()[2], 2);
}

// original 0x4D9AA0: the colour down 4, the script ticked, red 0x80, +2 on.
S26_EXPORT void __cdecl Magic115_MoteFade(void) {
    AddB(Sc()[0x5D], 0xFC);
    AddB(Sc()[0x5E], 0xFC);
    AddB(Sc()[0x5F], 0xFC);
    MH_CALL(Sprite_ScriptTickOnce)();
    Sc()[0x5D] = 0x80;
    Inc(Sc()[2]);
}

void Magic115_Inject() {
    BOF3_INJECT(Magic115_Task);
    BOF3_INJECT(Magic115_Start);
    BOF3_INJECT(Magic115_TintUp);
    BOF3_INJECT(Magic115_TintDown);
    BOF3_INJECT(Magic115_ChildTask);
    BOF3_INJECT(Magic115_OrbitRun);
    BOF3_INJECT(Magic115_OrbitStart);
    BOF3_INJECT(Magic115_Orbit);
    BOF3_INJECT(Magic115_OrbitFade);
    BOF3_INJECT(Magic115_MoteRun);
    BOF3_INJECT(Magic115_MoteStart);
    BOF3_INJECT(Magic115_MoteTick);
    BOF3_INJECT(Magic115_MoteFade);
}
