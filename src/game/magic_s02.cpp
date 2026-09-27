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
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kActorRecord = 0x904B3C;   // unsigned char *: the acting actor's sprite record
constexpr std::uint32_t kAbility = 0x904B80;       // u16: the ability being used
constexpr std::uint32_t kComboHits = 0x904B96;     // u8: MAGIC003's hit count, for the damage
constexpr std::uint32_t kFxBits = 0x904AA9;        // u8: bit 0x20 set by ElemStrike_Start
constexpr std::uint32_t kFrameSet = 0x9039D8;      // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kFrameSetHit = 0x8C5D80;

// SuperComboHit_Pool: 32 task-like entries of 0x84 bytes (.bss).
constexpr std::uint32_t kPool = 0x6769C0;
constexpr unsigned kPoolCount = 32;
constexpr std::uint32_t kPoolStride = 0x84;

// The overlays' .data, read in place (as the originals read it, unchecked).
constexpr std::uint32_t kImageTints = 0x65A4F8;    // MAGIC003: 3 signed bytes an after-image
constexpr std::uint32_t kButtonTimes = 0x65A504;   // bytes: the time allowed, by the count
constexpr std::uint32_t kButtons = 0x65A518;       // bytes: the Input_Pressed bit asked for
constexpr std::uint32_t kTextLengths = 0x65A51C;   // bytes: a text's length, to centre it
constexpr std::uint32_t kButtonCluts = 0x65A520;   // bytes: a button glyph's clut x
constexpr std::uint32_t kHitAnims = 0x65A550;      // bytes: a hit sprite's +0x27 + 0x50
constexpr std::uint32_t kBoxes = 0x65A55C;         // words x, y, w, h: a prompt box
constexpr std::uint32_t kTexts = 0x66A0D8;         // pointers to the prompt texts (0-ended, 0xFF a space)
constexpr std::uint32_t kStrikeTints = 0x65A584;   // MAGIC004: 3 bytes a kind
constexpr std::uint32_t kFxDelays = 0x65A5A8;      // bytes: the effect's sound delay, by kind
constexpr std::uint32_t kStrikeSounds = 0x65A5B4;  // words: the hit sound, by kind (0 none)
constexpr std::uint32_t kKindIndex = 0x49C0CC;     // .text: ElemStrike_Kind's byte table, 0x99 entries

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned ActorByte() { return static_cast<unsigned>(Long(Mem(at::kActor))) & 0xFF; }

short S16(const unsigned char* at) { return static_cast<short>(Word(at)); }
void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
// `mov ecx, [a]; add [b], ecx` on dwords: wraps.
void AddLong(unsigned char* to, const unsigned char* by) {
    SetLong(to, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(to)) + static_cast<std::uint32_t>(Long(by))));
}

// `fild dword` then `fstp dword`: an integer vertex as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
unsigned char* PoolEntry(unsigned i) { return Mem(kPool + i * kPoolStride); }
// The originals index the records by the battle index, unchecked: a party
// member below 3 (the target) or at most 2 (the actor), else the enemy by
// index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}
unsigned char* TargetRecord(unsigned t) { return t < 3 ? PartyRecord(t) : EnemyRecord(t); }
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
using Fn1 = void (__cdecl*)(unsigned);
using Alloc = unsigned char (__cdecl*)();
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call1(std::uint32_t address, unsigned a) { MH_AT(Fn1, address)(a); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }

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

// The four corners of a 12 x 12 screen quad at (x, y), as floats at +8, +0x18,
// +0x28, +0x38 (x) and +0xC, +0x1C, +0x2C, +0x3C (y).
void Quad12(unsigned char* p, int x, int y) {
    PutFloat(p + 8, x);
    PutFloat(p + 0xC, y);
    PutFloat(p + 0x18, x + 0xC);
    PutFloat(p + 0x1C, y);
    PutFloat(p + 0x28, x);
    PutFloat(p + 0x2C, y + 0xC);
    PutFloat(p + 0x38, x + 0xC);
    PutFloat(p + 0x3C, y + 0xC);
}
// A glyph quad's texture: tpage (0, 0, 0x3C0, 0), clut (clut_x, 0x1E0).
void GlyphPage(unsigned char* p, unsigned clut_x) {
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 0, 0x3C0, 0) & 0xFFFF);
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(static_cast<int>(clut_x), 0x1E0) & 0xFFFF);
}
// u / v of the four corners (u0 u1 u0 u1, v0 v0 v1 v1) and a mid-grey shade.
void GlyphUv(unsigned char* p, unsigned u0, unsigned u1, unsigned v0, unsigned v1) {
    p[0x14] = static_cast<unsigned char>(u0);
    p[0x15] = static_cast<unsigned char>(v0);
    p[0x24] = static_cast<unsigned char>(u1);
    p[0x25] = static_cast<unsigned char>(v0);
    p[0x34] = static_cast<unsigned char>(u0);
    p[0x35] = static_cast<unsigned char>(v1);
    p[0x44] = static_cast<unsigned char>(u1);
    p[0x45] = static_cast<unsigned char>(v1);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
}

// A counter byte down by one; true when it reached 0.
bool CountDown(unsigned k) {
    Dec(Sc()[k]);
    return Sc()[k] == 0;
}

// The start of a dash or after-image's leap: its landing point kept (+0x18,
// +0x1C, +0x44 from x, z, height), a step 0x2000 along x, a rise 0x300000 and
// a fall 0xFFF80000, the step turned by the direction (0x446770).
void LeapSetUp() {
    unsigned char* const s = Sc();
    SetLong(s + 0x18, Long(s + 0x34));
    SetLong(s + 0x1C, Long(s + 0x38));
    SetLong(s + 0x44, Long(s + 0x3C));
    SetLong(s + 0xC, 0x2000);
    SetLong(s + 0x10, 0);
    SetLong(s + 0x14, 0x300000);
    SetLong(s + 0x20, static_cast<std::int32_t>(0xFFF80000u));
    Turn(Sc());
}
// x, z and height by the step and the rise, the rise by the fall.
void LeapMove(unsigned char* s) {
    AddLong(s + 0x34, s + 0xC);
    AddLong(s + 0x38, s + 0x10);
    AddLong(s + 0x3C, s + 0x14);
    AddLong(s + 0x14, s + 0x20);
}
// x and z by the step only.
void GroundMove(unsigned char* s) {
    AddLong(s + 0x34, s + 0xC);
    AddLong(s + 0x38, s + 0x10);
}
bool BackAtStart(const unsigned char* s) { return Long(s + 0x34) == Long(s + 0x18) && Long(s + 0x38) == Long(s + 0x1C); }
unsigned char Tick() { return MH_CALL(Sprite_ScriptTickOnce)(); }

// A dash's count-down to its hit (+9 0xFF once hit): at 0 two sounds, the hit
// sprites (SuperCombo_SpawnHits) and target flags 0x10.
void DashCountDown() {
    unsigned char* const s = Sc();
    const unsigned char c = s[9];
    if (c == 0) {
        MH_CALL(Sound_PlayById)(0x101);
        MH_CALL(BattleActor_PlaySound)(2, 0);
        Call0(bof3::addr::SuperCombo_SpawnHits);
        MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
        Sc()[9] = 0xFF;
    } else if (c != 0xFF) {
        s[9] = static_cast<unsigned char>(c - 1);
    }
}

}  // namespace

#define S02_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC003 (row 3, Super Combo read one id down)

// original 0x49A7B0: the kind-2 task. A call through SuperCombo_Phases (ten
// entries) by +1; then Sprite_Current and the owner as they are after it,
// and every entry of the pool with bit 0 of +0 run as a task of its own
// (Sprite_Current the entry, the owner its +0x80), both put back after each.
S02_EXPORT void __cdecl SuperCombo_Task(void) {
    static constexpr std::uint32_t kPhases[10] = {
        bof3::addr::SuperCombo_Start,        bof3::addr::SuperCombo_Prompt1,    bof3::addr::SuperCombo_Prompt2,
        bof3::addr::SuperCombo_PickButton,   bof3::addr::SuperCombo_ReadButton, bof3::addr::SuperCombo_Pause,
        bof3::addr::SuperCombo_ShowCount,    bof3::addr::SuperCombo_Strike,     bof3::addr::SuperCombo_WaitChildren,
        bof3::addr::SuperCombo_End};
    const unsigned phase = Sc()[1];
    if (phase >= 10) PastTable("SuperCombo_Task", phase, 10);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const current = Sc();
    const std::int32_t owner = Long(Mem(at::kOwner));
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const e = PoolEntry(i);
        if ((e[0] & 1) == 0) continue;
        const std::int32_t entry_owner = Long(e + 0x80);
        Sprite_Current = e;
        SetLong(Mem(at::kOwner), entry_owner);
        Call0(bof3::addr::SuperComboHit_Task);
        SetLong(Mem(at::kOwner), owner);
        Sprite_Current = current;
    }
}

// original 0x49A820: entry 0. Bytes +0..+2 of the pool's 32 entries cleared;
// the prompt's screen point (+0x2E 0xA0, +0x30 0x70), +0xB 0, +9 0x1E, the
// count +0xA 0; +1 on.
S02_EXPORT void __cdecl SuperCombo_Start(void) {
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const e = PoolEntry(i);
        e[0] = 0;
        e[1] = 0;
        e[2] = 0;
    }
    unsigned char* const s = Sc();
    SetWord(s + 0x2E, 0xA0);
    SetWord(s + 0x30, 0x70);
    s[0xB] = 0;
    s[9] = 0x1E;
    s[0xA] = 0;
    Inc(s[1]);
}

// original 0x49A880: entry 1. Box 1 and text 1; +9 down, at 0 +9 0x1E, +1 on.
S02_EXPORT void __cdecl SuperCombo_Prompt1(void) {
    Call1(bof3::addr::SuperCombo_DrawBox, 1);
    Call1(bof3::addr::SuperCombo_DrawText, 1);
    if (!CountDown(9)) return;
    Sc()[9] = 0x1E;
    Inc(Sc()[1]);
}

// original 0x49A8C0: entry 2. Box 2 and text 2; +9 down, at 0 +9 0xA, +1 on.
S02_EXPORT void __cdecl SuperCombo_Prompt2(void) {
    Call1(bof3::addr::SuperCombo_DrawBox, 2);
    Call1(bof3::addr::SuperCombo_DrawText, 2);
    if (!CountDown(9)) return;
    Sc()[9] = 0xA;
    Inc(Sc()[1]);
}

// original 0x49A900: entry 3. +9 down; at 0 the button asked for (+0xB =
// Rand & 3) and the time allowed (+9: the count's byte of 0x65A504 below
// 0x10, else 3); +1 on.
S02_EXPORT void __cdecl SuperCombo_PickButton(void) {
    if (!CountDown(9)) return;
    const unsigned r = static_cast<unsigned>(MH_CALL(Rand)());
    Sc()[0xB] = static_cast<unsigned char>(r & 3);
    unsigned char* const s = Sc();
    const unsigned char count = s[0xA];
    if (count < 0x10) {
        s[9] = Mem(kButtonTimes)[count];
        Inc(Sc()[1]);
        return;
    }
    s[9] = 3;
    Inc(Sc()[1]);
}

// original 0x49A960: entry 4. Box 4 and the button +0xB; then the pad
// (Input_Pressed, read after both): a press of any of bits 0xF0 sets +9 6 and,
// when the whole word is the button asked for (0x65A518), counts a hit (+0xA)
// and goes back to entry 3 while the count is below 0x20, else to entry 5; no
// press counts +9 down, at 0 +9 6 and entry 5.
S02_EXPORT void __cdecl SuperCombo_ReadButton(void) {
    Call1(bof3::addr::SuperCombo_DrawBox, 4);
    Call1(bof3::addr::SuperCombo_DrawButton, Sc()[0xB]);
    const std::uint16_t pressed = Input_Pressed;
    if ((pressed & 0xF0) != 0) {
        unsigned char* const s = Sc();
        const std::uint16_t wanted = Mem(kButtons)[s[0xB]];
        s[9] = 6;
        if (pressed == wanted) {
            Inc(s[0xA]);
            s[1] = s[0xA] < 0x20 ? 3 : 5;
            return;
        }
        s[1] = 5;
        return;
    }
    if (!CountDown(9)) return;
    Sc()[9] = 6;
    Sc()[1] = 5;
}

// original 0x49A9F0: entry 5. +9 down, at 0 +9 0x1E and +1 on.
S02_EXPORT void __cdecl SuperCombo_Pause(void) {
    if (!CountDown(9)) return;
    Sc()[9] = 0x1E;
    Inc(Sc()[1]);
}

// original 0x49AA20: entry 6. Box 0, the count, text 0; +9 down, at 0 +1 on.
S02_EXPORT void __cdecl SuperCombo_ShowCount(void) {
    Call1(bof3::addr::SuperCombo_DrawBox, 0);
    Call1(bof3::addr::SuperCombo_DrawCount, Sc()[0xA]);
    Call1(bof3::addr::SuperCombo_DrawText, 0);
    if (!CountDown(9)) return;
    Inc(Sc()[1]);
}

// original 0x49AA60: entry 7. The owner's direction and position; +0xB 0,
// +1 on; the caster's animation 0xC; the dash and the four after-images (each
// counts +0xB up); +4 their number; the owner's +0 bit 0x40; sound 0x100.
S02_EXPORT void __cdecl SuperCombo_Strike(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    Call0(bof3::addr::SuperCombo_SpawnDash);
    Call0(bof3::addr::SuperCombo_SpawnImages);
    unsigned char* const s = Sc();
    s[4] = s[0xB];
    Owner()[0] |= 0x40;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x49AAF0: entry 8. Once no child is left (+0xB 0): the actor
// sprite's +0 bit 0x40 cleared, the caster's animation 4, +1 on.
S02_EXPORT void __cdecl SuperCombo_WaitChildren(void) {
    if (Sc()[0xB] != 0) return;
    Pointer(kActorRecord)[0] &= 0xBF;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Inc(Sc()[1]);
}

// original 0x49AB20: entry 9. Sound 0x102; the count to 0x904B96 (both it
// and the target read after the sound); target flag 0x40, the done bit, free.
S02_EXPORT void __cdecl SuperCombo_End(void) {
    MH_CALL(Sound_PlayById)(0x102);
    const unsigned char count = Sc()[0xA];
    const unsigned char target = TargetByte();
    Mem(kComboHits)[0] = count;
    MH_CALL(Battle_SetTargetFlag40)(target);
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49AB60: the kind-1 task of parameter 1, a two-entry stack table by
// +1: the dash, the after-images.
S02_EXPORT void __cdecl SuperComboChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::SuperComboDash_Run, bof3::addr::SuperComboImage_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("SuperComboChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x49AB90: the dash, a five-entry stack table by +2; then while +2
// and +0 are set the sprite's screen slot updated.
S02_EXPORT void __cdecl SuperComboDash_Run(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::SuperComboDash_Start, bof3::addr::SuperComboDash_Leap,
                                                bof3::addr::SuperComboDash_Hit, bof3::addr::SuperComboDash_Return,
                                                bof3::addr::SuperComboDash_End};
    const unsigned phase = Sc()[2];
    if (phase >= 5) PastTable("SuperComboDash_Run", phase, 5);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    MH_CALL(Sprite_UpdateScreenSlot)();
}

// original 0x49ABF0: +9 down; at 0 the leap set up, +9 the actor's effect
// size (BattleActor_FxSize), +2 on.
S02_EXPORT void __cdecl SuperComboDash_Start(void) {
    if (!CountDown(9)) return;
    LeapSetUp();
    const unsigned char size = MH_CALL(BattleActor_FxSize)();
    Sc()[9] = size;
    Inc(Sc()[2]);
}

// original 0x49AC80: the count-down to the hit; the leap; the script ticked;
// +2 on when the height is back where it started.
S02_EXPORT void __cdecl SuperComboDash_Leap(void) {
    DashCountDown();
    LeapMove(Sc());
    Tick();
    const unsigned char* const s = Sc();
    if (Long(s + 0x3C) == Long(s + 0x44)) Inc(Sc()[2]);
}

// original 0x49AD30: the count-down to the hit without moving; the script
// ticked; once hit (+9 0xFF) the step 0xFFFFE000 turned back, +2 on.
S02_EXPORT void __cdecl SuperComboDash_Hit(void) {
    DashCountDown();
    Tick();
    unsigned char* const s = Sc();
    if (s[9] != 0xFF) return;
    SetLong(s + 0xC, static_cast<std::int32_t>(0xFFFFE000u));
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    Inc(Sc()[2]);
}

// original 0x49ADC0: back along the ground; the script ticked; +2 on at the
// start point.
S02_EXPORT void __cdecl SuperComboDash_Return(void) {
    GroundMove(Sc());
    Tick();
    if (BackAtStart(Sc())) Inc(Sc()[2]);
}

// original 0x49AE10: waits until the owner's +0xB is at most 1; then counts it
// down and frees the task.
S02_EXPORT void __cdecl SuperComboDash_End(void) {
    unsigned char* const owner = Owner();
    const unsigned char left = owner[0xB];
    if (left > 1) return;
    owner[0xB] = static_cast<unsigned char>(left - 1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49AE30: an after-image, a four-entry stack table by +2; then
// while +2 and +0 are set the sprite's screen slot updated.
S02_EXPORT void __cdecl SuperComboImage_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::SuperComboImage_Start, bof3::addr::SuperComboImage_Leap,
                                                bof3::addr::SuperComboImage_Turn, bof3::addr::SuperComboImage_Return};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("SuperComboImage_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[2] == 0 || s[0] == 0) return;
    MH_CALL(Sprite_UpdateScreenSlot)();
}

// original 0x49AE80: +9 down; at 0 the leap set up, the sprite made
// semi-transparent (+0 bit 0x20, +0x5C 3, +0x5D..+0x5F = +0xB x 0xF0) and
// tinted by the after-image's three signed bytes (0x65A4F8 + 3 x +0xB); +2 on.
S02_EXPORT void __cdecl SuperComboImage_Start(void) {
    if (!CountDown(9)) return;
    LeapSetUp();
    Sc()[0] |= 0x20;
    Sc()[0x5C] = 3;
    const auto shade = [] { return static_cast<unsigned char>(Sc()[0xB] * 0xF0u); };
    Sc()[0x5D] = shade();
    Sc()[0x5F] = shade();
    Sc()[0x5E] = shade();
    unsigned char* const s = Sc();
    const unsigned char* const tint = Mem(kImageTints + 3u * s[0xB]);
    MH_CALL(Sprite_SetTint)(s, tint[0], tint[1], tint[2], 1);
    Inc(Sc()[2]);
}

// original 0x49AF80: the leap; the script ticked; +2 on when the height is back.
S02_EXPORT void __cdecl SuperComboImage_Leap(void) {
    LeapMove(Sc());
    Tick();
    const unsigned char* const s = Sc();
    if (Long(s + 0x3C) == Long(s + 0x44)) Inc(Sc()[2]);
}

// original 0x49AFE0: the step 0xFFFFE000 turned back; the script ticked; +2 on.
S02_EXPORT void __cdecl SuperComboImage_Turn(void) {
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(0xFFFFE000u));
    SetLong(Sc() + 0x10, 0);
    Turn(Sc());
    Tick();
    Inc(Sc()[2]);
}

// original 0x49B020: back along the ground; the script ticked; at the start
// point the tint released, the owner's +0xB down, the task freed.
S02_EXPORT void __cdecl SuperComboImage_Return(void) {
    GroundMove(Sc());
    Tick();
    unsigned char* const s = Sc();
    if (!BackAtStart(s)) return;
    MH_CALL(Sprite_ReleaseTint)(s);
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49B080: a pool entry's task, a jmp through
// SuperComboHit_TaskTable (one entry) by +1, unchecked.
S02_EXPORT void __cdecl SuperComboHit_Task(void) {
    const unsigned phase = Sc()[1];
    if (phase >= 1) PastTable("SuperComboHit_Task", phase, 1);
    magic_harness::Phase(bof3::addr::SuperComboHit_Run)();
}

// original 0x49B0A0: the hit sprite's frame-offset table (0x8C5D80) while a
// two-entry stack table by +2 runs; then while +0 and +2 are set the sprite
// queued; the battle's table (0x8B3580) back.
S02_EXPORT void __cdecl SuperComboHit_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::SuperComboHit_Start, bof3::addr::SuperComboHit_Play};
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetHit));
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("SuperComboHit_Run", phase, 2);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_QueueOverlay)();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x49B0F0: +9 down; at 0 the entry placed at the source sprite
// (direction and position), its screen point, its sprite bytes (+0x27 from
// 0x65A550 by +0xB, +0x2A bit 0 of +4), animation +0xB; +2 on.
S02_EXPORT void __cdecl SuperComboHit_Start(void) {
    if (!CountDown(9)) return;
    const unsigned char* const src = Pointer(at::kSource);
    Sc()[8] = src[8];
    SetLong(Sc() + 0x34, Long(src + 0x34));
    SetLong(Sc() + 0x38, Long(src + 0x38));
    SetLong(Sc() + 0x3C, Long(src + 0x3C));
    MH_CALL(BattleActor_UpdateScreenXY)();
    unsigned char* const s = Sc();
    s[0x29] = 0;
    s[0x25] = 0x1D;
    s[0x26] = 0;
    s[0x24] = 0x84;
    SetWord(s + 0x2C, 0);
    s[0x2B] = 0;
    s[0x28] = 0;
    s[0x27] = static_cast<unsigned char>(Mem(kHitAnims)[s[0xB]] - 0x50);
    s[0x2A] = (s[4] & 1) != 0 ? 1 : 0;
    MH_CALL(Sprite_SetAnimation)(Sc()[0xB]);
    Inc(Sc()[2]);
}

// original 0x49B1D0: the script ticked twice; when the second reports the
// end, the owner's +0xB down and MAGIC219's 0x4F6290 (a tail jmp).
S02_EXPORT void __cdecl SuperComboHit_Play(void) {
    Tick();
    if (Tick() == 0) return;
    Dec(Owner()[0xB]);
    Call0(bof3::addr::MagicFx_FreeCurrentRecord);
}

// original 0x49B1F0: one kind-1 task of parameter 1 (the slot unchecked), a
// copy of the acting actor's record (0x80 bytes) with +0x80 this task, +6 1,
// +5 1, +1 0 (the dash), +2 0, +9 1; +0xB up.
S02_EXPORT void __cdecl SuperCombo_SpawnDash(void) {
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 1) & 0xFFu;
    unsigned char* const child = TaskSlot(slot);
    CopyRecord(child, ActorRecord(ActorByte()));
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
    child[6] = 1;
    child[5] = 1;
    child[1] = 0;
    child[2] = 0;
    child[9] = 1;
    Inc(Sc()[0xB]);
}

// original 0x49B2B0: four such copies with +1 1 (an after-image), +0xB 0..3 and
// +9 5, 9, 13, 17; +0xB up for each.
S02_EXPORT void __cdecl SuperCombo_SpawnImages(void) {
    unsigned char n = 0;
    unsigned char delay = 5;
    do {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 1) & 0xFFu;
        unsigned char* const child = TaskSlot(slot);
        CopyRecord(child, ActorRecord(ActorByte()));
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(Sc())));
        child[6] = 1;
        child[5] = 1;
        child[1] = 1;
        child[2] = 0;
        child[0xB] = n;
        child[9] = delay;
        delay = static_cast<unsigned char>(delay + 4);
        Inc(Sc()[0xB]);
        n = static_cast<unsigned char>(n + 1);
    } while (delay < 0x15);
}

// original 0x49B390: one pool entry for each hit counted (the owner's +0xA,
// read again after each): its +0x80 the owner, +1 0, +2 0, +4 i, +0xB
// (i >> 1) & 7, +9 1 + 4 i; the owner's +0xB up. The allocator's "none free"
// (0xFF) is not tested: its entry lies past the pool.
S02_EXPORT void __cdecl SuperCombo_SpawnHits(void) {
    if (Owner()[0xA] == 0) return;
    unsigned char i = 0;
    unsigned char delay = 1;
    do {
        const unsigned slot = MH_AT(Alloc, bof3::addr::SuperComboHit_Alloc)() & 0xFFu;
        unsigned char* const owner = Owner();
        unsigned char* const e = PoolEntry(slot);
        SetLong(e + 0x80, static_cast<std::int32_t>(Key(owner)));
        e[1] = 0;
        e[2] = 0;
        e[4] = i;
        e[0xB] = static_cast<unsigned char>((i >> 1) & 7);
        e[9] = delay;
        Inc(owner[0xB]);
        i = static_cast<unsigned char>(i + 1);
        delay = static_cast<unsigned char>(delay + 4);
    } while (i < Owner()[0xA]);
}

// original 0x49B420: text `which` (u8) of the prompt table 0x66A0D8, centred on
// the task's +0x2E by its length (0x65A51C) and drawn from the glyph page at
// v 0x24.. as 12 x 12 quads, a draw-mode packet first; 0xFF is a half-width
// space, 0 ends it. The x is a 16-bit register, the y the task's +0x30.
S02_EXPORT void __cdecl SuperCombo_DrawText(unsigned which) {
    DrawModeCommit(0x15, 2);
    const unsigned k = which & 0xFF;
    const unsigned length = Mem(kTextLengths)[k];
    const unsigned char* const s = Sc();
    std::uint16_t x = static_cast<std::uint16_t>(Word(s + 0x2E) - 6 * length);
    const short y = S16(s + 0x30);
    if (Pointer(kTexts + 4 * k)[0] == 0) return;
    unsigned char i = 0;
    do {
        const unsigned char c = Pointer(kTexts + 4 * k)[i];
        if (c == 0xFF) {
            x = static_cast<std::uint16_t>(x - 6);
        } else {
            const unsigned code = static_cast<unsigned char>(c - 0x3F);
            unsigned char* const p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyFT4)(p);
            Quad12(p, static_cast<short>(x), y);
            GlyphPage(p, 0);
            const unsigned q = code / 21, r = code % 21;
            GlyphUv(p, r * 12, (r + 1) * 12, (q + 3) * 12, (q + 4) * 12);
            MH_CALL(Gfx_CommitPrim)(2, 0x48);
        }
        i = static_cast<unsigned char>(i + 1);
        x = static_cast<std::uint16_t>(x + 0xC);
    } while (Pointer(kTexts + 4 * k)[i] != 0);
}

// original 0x49B5F0: a draw-mode packet, then button `button` (u8) as a 12 x 12
// quad at the task's screen point: u 12 (button + 15), v 0x30..0x3C, clut x
// from 0x65A520.
S02_EXPORT void __cdecl SuperCombo_DrawButton(unsigned button) {
    DrawModeCommit(0x15, 2);
    const unsigned char* const s = Sc();
    const short x = S16(s + 0x2E);
    const short y = S16(s + 0x30);
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    Quad12(p, x, y);
    const unsigned b = button & 0xFF;
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 0, 0x3C0, 0) & 0xFFFF);
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(Mem(kButtonCluts)[b], 0x1E0) & 0xFFFF);
    GlyphUv(p, (b + 0xF) * 12, (b + 0x10) * 12, 0x30, 0x3C);
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// original 0x49B700: a draw-mode packet, then `count` (u8) in decimal as 12 x 12
// digits (u 12 (d + 6), v 0x18..0x24): the units at the task's +0x2E - 0x42,
// the tens (when not 0) 12 to their left.
S02_EXPORT void __cdecl SuperCombo_DrawCount(unsigned count) {
    DrawModeCommit(0x15, 2);
    const unsigned char* const s = Sc();
    std::uint16_t x = Word(s + 0x2E);
    const short y = S16(s + 0x30);
    unsigned char* p = Gfx_PacketNext;
    const unsigned n = count & 0xFF;
    const unsigned tens = n / 10, units = n % 10;
    x = static_cast<std::uint16_t>(x - 0x42);
    MH_CALL(Gpu_SetPolyFT4)(p);
    Quad12(p, static_cast<short>(x), y);
    GlyphPage(p, 0);
    GlyphUv(p, (units + 6) * 12, (units + 7) * 12, 0x18, 0x24);
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
    x = static_cast<std::uint16_t>(x - 0xC);
    if ((tens & 0xFF) == 0) return;
    p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    Quad12(p, static_cast<short>(x), y);
    GlyphPage(p, 0);
    GlyphUv(p, (tens + 6) * 12, (tens + 7) * 12, 0x18, 0x24);
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// original 0x49B900: box `which` (u8) of 0x65A55C (x, y, w, h words, from the
// task's screen point) as two semi-transparent tiles grown by 4 and by 6,
// shade 0x20, in draw mode tpage 0x55; the draw mode back to tpage 0x15.
S02_EXPORT void __cdecl SuperCombo_DrawBox(unsigned which) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x55, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    const unsigned char* const box = Mem(kBoxes + 8 * (which & 0xFF));
    for (int grow : {4, 6}) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetTile)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        PutFloat(p + 8, S16(Sc() + 0x2E) - static_cast<int>(Word(box)) - grow);
        PutFloat(p + 0xC, S16(Sc() + 0x30) - static_cast<int>(Word(box + 2)) - grow);
        PutFloat(p + 0x14, static_cast<int>(Word(box + 4)) + 2 * grow);
        PutFloat(p + 0x18, static_cast<int>(Word(box + 6)) + 2 * grow);
        p[4] = 0x20;
        p[5] = 0x20;
        p[6] = 0x20;
        MH_CALL(Gfx_CommitPrim)(2, 0x1C);
    }
    DrawModeCommit(0x15, 2);
}

// original 0x49BA90: the first free pool entry (bit 0 of +0 clear) taken, its
// index in al; 0xFF when all 32 are in use.
S02_EXPORT unsigned char __cdecl SuperComboHit_Alloc(void) {
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const e = PoolEntry(i);
        if ((e[0] & 1) != 0) continue;
        e[0] |= 1;
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// ===========================================================================
// MAGIC004 (rows 88, 92, 93, 98..100, 129..132: the Strikes and Claws read one
// id down)

// original 0x49BAF0: the kind-2 task, a five-entry stack table by +1.
S02_EXPORT void __cdecl ElemStrike_Task(void) {
    static constexpr std::uint32_t kPhases[5] = {bof3::addr::ElemStrike_Start, bof3::addr::ElemStrike_Tint,
                                                 bof3::addr::ElemStrike_Hit, bof3::addr::ElemStrike_Fade,
                                                 bof3::addr::ElemStrike_End};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("ElemStrike_Task", phase, 5);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x49BB30: the owner's direction and position; +0xB and +9 0, +1 on;
// the caster's animation 0xC; the kind (+3 / +4, ElemStrike_Kind); a child of
// kind 1, parameter 0x46, made a copy of the acting actor's record (+1 0: it
// plays), and a second (+1 1: the effect, with this task's +3 / +4), both
// owned by this task and counted in +0xB (slots unchecked); the owner's +0
// bit 0x40, 0x904AA9 bit 0x20; CLUT row 26 back from its source.
S02_EXPORT void __cdecl ElemStrike_Start(void) {
    Sc()[8] = Owner()[8];
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Inc(Sc()[1]);
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    Call0(bof3::addr::ElemStrike_Kind);
    {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x46) & 0xFFu;
        unsigned char* const child = TaskSlot(slot);
        CopyRecord(child, ActorRecord(ActorByte()));
        unsigned char* const s = Sc();
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 0;
        child[2] = 0;
        child[6] = 1;
        child[5] = 0x46;
        s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
    }
    {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x46) & 0xFFu;
        unsigned char* const child = TaskSlot(slot);
        unsigned char* const s = Sc();
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
        child[1] = 1;
        child[3] = s[3];
        child[4] = s[4];
        Inc(s[0xB]);
    }
    Owner()[0] |= 0x40;
    Mem(kFxBits)[0] |= 0x20;
    for (unsigned k = 0x1A00; k < 0x1B00; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
}

// original 0x49BCD0: once +0xB is at most 1 (the copy's child gone): the
// kind's three tint bytes (0x65A584 + 3 x +4) to +0x5D..+0x5F; the target's
// record (taken first) untinted and tinted by them, the slot to +0xA; the
// caster's animation 4; the owner's bit 0x40 cleared; target flags 0x20;
// sound 0x100, and 0x101 for an ability 0x85..0x88; +1 on.
S02_EXPORT void __cdecl ElemStrike_Tint(void) {
    if (Sc()[0xB] > 1) return;
    unsigned char* const record = TargetRecord(static_cast<unsigned>(Long(Mem(at::kTarget))) & 0xFF);
    for (unsigned c = 0; c < 3; ++c) {
        unsigned char* const s = Sc();
        s[0x5D + c] = Mem(kStrikeTints + 3u * s[4] + c)[0];
    }
    MH_CALL(Sprite_ReleaseTint)(record);
    const unsigned char* const s = Sc();
    const unsigned char slot = MH_CALL(Sprite_SetTint)(record, s[0x5D], s[0x5E], s[0x5F], 0);
    Sc()[0xA] = slot;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x20);
    MH_CALL(Sound_PlayById)(0x100);
    const std::uint16_t ability = Word(Mem(kAbility));
    if (ability > 0x84 && ability <= 0x88) MH_CALL(Sound_PlayById)(0x101);
    Inc(Sc()[1]);
}

// original 0x49BDE0: once +0xB is 0: the kind's hit sound (0x65A5B4 + 2 x +4)
// unless 0; target flag 0x40; +9 0, +1 on.
S02_EXPORT void __cdecl ElemStrike_Hit(void) {
    const unsigned char* const s = Sc();
    if (s[0xB] != 0) return;
    const std::uint16_t sound = Word(Mem(kStrikeSounds + 2u * s[4]));
    if (sound != 0) MH_CALL(Sound_PlayById)(sound);
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    Sc()[9] = 0;
    Inc(Sc()[1]);
}

// original 0x49BE30: while +9 is below 0xC, MAGIC008's 0x4A29C0 and +9 up by
// 4; each of +0x5D..+0x5F not 0 down by 2, written to tint record +0xA; all
// three 0: the target's record untinted, the target flashed, +1 on.
S02_EXPORT void __cdecl ElemStrike_Fade(void) {
    if (Sc()[9] < 0xC) {
        Call0(bof3::addr::Magic008_DrawFlash);
        Sc()[9] = static_cast<unsigned char>(Sc()[9] + 4);
    }
    unsigned char* const s = Sc();
    for (unsigned c = 0x5D; c < 0x60; ++c)
        if (s[c] != 0) s[c] = static_cast<unsigned char>(s[c] - 2);
    for (unsigned c = 0; c < 3; ++c) MoveScript_TintRecords[s[0xA] * 12u + 2 + c] = s[0x5D + c];
    if (s[0x5D] != 0 || s[0x5E] != 0 || s[0x5F] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(TargetRecord(static_cast<unsigned>(Long(Mem(at::kTarget))) & 0xFF));
    MH_CALL(BattleActor_Flash)(TargetByte());
    Inc(Sc()[1]);
}

// original 0x49BF30: the target's record taken first; the effect ends (the done
// bit, the task freed) unless the target is not out and its state +1 is 6.
S02_EXPORT void __cdecl ElemStrike_End(void) {
    const unsigned char target = TargetByte();
    const unsigned char* const record = TargetRecord(static_cast<unsigned>(Long(Mem(at::kTarget))) & 0xFF);
    if (MH_CALL(Battle_ActorIsOut)(target) == 0 && record[1] == 6) return;
    Mem(at::kFlags)[0] |= 4;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x49BF90: the ability word 0x904B80 less 4, through the byte table
// 0x49C0CC (0x99 entries, past it the last case), picks one of twelve cases:
// +3 / +4 as the jump table's targets write them.
S02_EXPORT void __cdecl ElemStrike_Kind(void) {
    static constexpr unsigned char kCases[12][2] = {{0, 0}, {1, 1}, {0, 2},   {1, 3},   {0, 4}, {1, 8},
                                                    {1, 9}, {1, 0xA}, {1, 0xB}, {1, 5}, {0, 6}, {0, 7}};
    const std::uint32_t index = (static_cast<std::uint32_t>(Long(Mem(kAbility))) & 0xFFFF) - 4u;
    const unsigned c = index > 0x98 ? 11u : Mem(kKindIndex)[index];
    if (c >= 12) PastTable("ElemStrike_Kind", c, 12);
    Sc()[3] = kCases[c][0];
    Sc()[4] = kCases[c][1];
}

// original 0x49C170: the kind-1 task of parameter 0x46, a jmp through
// ElemStrikeChild_Kinds (two entries) by +1, unchecked.
S02_EXPORT void __cdecl ElemStrikeChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::ElemStrikeCopy_Run, bof3::addr::ElemStrikeFx_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("ElemStrikeChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x49C190: the caster's copy, a four-entry stack table by +2
// (BattleFx_SetSize, ElemStrikeCopy_Play, MAGIC161's 0x4EE560,
// BattleFx_FreeTask); the sprite's screen point updated while +0 is set.
S02_EXPORT void __cdecl ElemStrikeCopy_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BattleFx_SetSize, bof3::addr::ElemStrikeCopy_Play,
                                                bof3::addr::BattleFx_ScriptToEnd, bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("ElemStrikeCopy_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x49C1E0: the script ticked; +9 down, at 0 the actor's sound (2 for
// an ability of 0x85 or more, else 0; 4) and +2 on.
S02_EXPORT void __cdecl ElemStrikeCopy_Play(void) {
    Tick();
    if (!CountDown(9)) return;
    MH_CALL(BattleActor_PlaySound)(Word(Mem(kAbility)) >= 0x85 ? 2 : 0, 4);
    Inc(Sc()[2]);
}

// original 0x49C230: a draw-mode packet on layer 3; the effects' frame-offset
// table (0x8E3580) while a call through ElemStrikeFx_Steps (three entries) by
// +2 runs; the battle's back.
S02_EXPORT void __cdecl ElemStrikeFx_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::MagicFx_WaitOwnerChildren, bof3::addr::ElemStrikeFx_Start,
                                                bof3::addr::ElemStrikeFx_Play};
    DrawModeCommit(0x15, 3);
    const unsigned phase = Sc()[2];
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    if (phase >= 3) PastTable("ElemStrikeFx_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
}

// original 0x49C280: at the source sprite, 0x800000 higher; the sprite bytes
// (+0x27 0x1A, +0x28 1, +0x24 0 for a kind with +3, else 0xA0, 0, 4; +0x25
// 0x1D, +0x26 0, +0x5C..+0x5F 0, +0x2A 0, +0x29 4, +0x2C 0, +0x2B 1);
// animation 0; +9 the kind's delay (0x65A5A8 + +4); +2 on.
S02_EXPORT void __cdecl ElemStrikeFx_Start(void) {
    SetLong(Sc() + 0x34, Long(Pointer(at::kSource) + 0x34));
    SetLong(Sc() + 0x38, Long(Pointer(at::kSource) + 0x38));
    SetLong(Sc() + 0x3C,
            static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Pointer(at::kSource) + 0x3C)) + 0x800000u));
    unsigned char* const s = Sc();
    s[0x25] = 0x1D;
    s[0x26] = 0;
    if (s[3] != 0) {
        s[0x27] = 0x1A;
        s[0x28] = 1;
        s[0x24] = 0;
    } else {
        s[0x27] = 0xA0;
        s[0x28] = 0;
        s[0x24] = 4;
    }
    s[0x5D] = 0;
    s[0x5E] = 0;
    s[0x5F] = 0;
    s[0x5C] = 0;
    s[0x2A] = 0;
    s[0x29] = 4;
    SetWord(s + 0x2C, 0);
    s[0x2B] = 1;
    MH_CALL(Sprite_SetAnimation)(0);
    Sc()[9] = Mem(kFxDelays)[Sc()[4]];
    Inc(Sc()[2]);
}

// original 0x49C380: +9 counts down to 0, at 0 sound 0x101; the script ticked:
// at its end the owner's +0xB down and the task freed, else the sprite's
// screen point updated (both tail jmps).
S02_EXPORT void __cdecl ElemStrikeFx_Play(void) {
    unsigned char* const s = Sc();
    const unsigned char delay = s[9];
    if (delay != 0) {
        s[9] = static_cast<unsigned char>(delay - 1);
        if (Sc()[9] == 0) MH_CALL(Sound_PlayById)(0x101);
    }
    if (Tick() != 0) {
        Dec(Owner()[0xB]);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    MH_CALL(Sprite_UpdateScreen)();
}

void MagicS02_Inject() {
    if (bof3::WantsShadow("magic_s02")) magic_s02::SelfTest();
    BOF3_INJECT(SuperCombo_Task);
    BOF3_INJECT(SuperCombo_Start);
    BOF3_INJECT(SuperCombo_Prompt1);
    BOF3_INJECT(SuperCombo_Prompt2);
    BOF3_INJECT(SuperCombo_PickButton);
    BOF3_INJECT(SuperCombo_ReadButton);
    BOF3_INJECT(SuperCombo_Pause);
    BOF3_INJECT(SuperCombo_ShowCount);
    BOF3_INJECT(SuperCombo_Strike);
    BOF3_INJECT(SuperCombo_WaitChildren);
    BOF3_INJECT(SuperCombo_End);
    BOF3_INJECT(SuperComboChild_Task);
    BOF3_INJECT(SuperComboDash_Run);
    BOF3_INJECT(SuperComboDash_Start);
    BOF3_INJECT(SuperComboDash_Leap);
    BOF3_INJECT(SuperComboDash_Hit);
    BOF3_INJECT(SuperComboDash_Return);
    BOF3_INJECT(SuperComboDash_End);
    BOF3_INJECT(SuperComboImage_Run);
    BOF3_INJECT(SuperComboImage_Start);
    BOF3_INJECT(SuperComboImage_Leap);
    BOF3_INJECT(SuperComboImage_Turn);
    BOF3_INJECT(SuperComboImage_Return);
    BOF3_INJECT(SuperComboHit_Task);
    BOF3_INJECT(SuperComboHit_Run);
    BOF3_INJECT(SuperComboHit_Start);
    BOF3_INJECT(SuperComboHit_Play);
    BOF3_INJECT(SuperCombo_SpawnDash);
    BOF3_INJECT(SuperCombo_SpawnImages);
    BOF3_INJECT(SuperCombo_SpawnHits);
    BOF3_INJECT(SuperCombo_DrawText);
    BOF3_INJECT(SuperCombo_DrawButton);
    BOF3_INJECT(SuperCombo_DrawCount);
    BOF3_INJECT(SuperCombo_DrawBox);
    BOF3_INJECT(SuperComboHit_Alloc);
    BOF3_INJECT(ElemStrike_Task);
    BOF3_INJECT(ElemStrike_Start);
    BOF3_INJECT(ElemStrike_Tint);
    BOF3_INJECT(ElemStrike_Hit);
    BOF3_INJECT(ElemStrike_Fade);
    BOF3_INJECT(ElemStrike_End);
    BOF3_INJECT(ElemStrike_Kind);
    BOF3_INJECT(ElemStrikeChild_Task);
    BOF3_INJECT(ElemStrikeCopy_Run);
    BOF3_INJECT(ElemStrikeCopy_Play);
    BOF3_INJECT(ElemStrikeFx_Run);
    BOF3_INJECT(ElemStrikeFx_Start);
    BOF3_INJECT(ElemStrikeFx_Play);
}
