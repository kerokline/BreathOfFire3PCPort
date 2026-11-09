// Two spell overlays compiled into the exe, round nine group S33
// (docs/magic_s33.md): the PSX's MAGIC151 and MAGIC154.EMI, Magic_Rows rows
// 86 and 35. Read one id down (docs/cut-content.md section 2) the sibling
// labels them Accession and Mighty Chop; the names below use those labels as
// hypotheses, and say what the code does.
//
//   - MAGIC151 0x4EAE70..0x4ED0B1: a banner with the name of the kind in
//     0x904B89, the acting member's script run, a controller child (kind 1,
//     0x44) that raises an orb (a dome of flat quads and a glow of gouraud
//     quads), sparks (a line fan), a bolt (three band rows) and two rings;
//     meanwhile a DAT file chosen by 0x904B89 and the party set is loaded and
//     the acting member's sprite rebuilt - one way (entry 0 of the task's
//     second table) or, when the phase starts at 1, by copying the acting
//     member's record over party record 0, clearing members 1 and 2, and
//     rebuilding the battle windows;
//   - MAGIC154 0x4ED0C0..0x4ED66E: the acting actor's animation 0xC, a copy of
//     its record (kind 1, 0x52) that plays its script, and six blades (the
//     same kind, phase 0) thrown from the source sprite, each a sprite that
//     grows and shrinks.
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s33.h"

#include <cstdint>
#include <cstring>
#include <initializer_list>

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
// move_script::Long is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The scratch the overlays keep their working values in: SharedScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr ::bof3::image::Va kS{SharedScratch_addr};
// kV: now Prim_VertexScratch (symbols.toml).

// The cells the overlays read beyond the harness's names.
// kCentreX: now BossSi_CentreX (symbols.toml). i32: the fight's centre (field_hidden_callees.h)
// kCentreZ: now BossSi_CentreZ (symbols.toml).
// kFormation: now BattlePhases_B89 (symbols.toml). u8: magic_lib's kFormation; indexes the tables below
// kPartySet: now AreaW0b_PartySet (symbols.toml). u8: the loaded party set (field_event_callees.h)
// kPalette: now BattleE6_FormGroup (symbols.toml). u8: the palette index Sprite_LoadPalette is given
// kFrameSet: now BattleE2_AnimSet (symbols.toml). the sprite frame-offset table pointer (sprite_pose.h)
constexpr ::bof3::image::Va kFrameSetBattle{0x8B3580};
constexpr ::bof3::image::Va kFrameSetEffect{0x8E3580};
constexpr ::bof3::image::Va kWindowRows{0x803337};    // the battle windows 0xD.. (Window_Alloc's records +3, stride 0x24)

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

std::uint16_t SW(unsigned k) { return Word(Mem(::bof3::Img(kS.v) + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(::bof3::Img(kS.v) + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(kS.v) + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(::bof3::Img(Prim_VertexScratch_addr) + k), v & 0xFFFF); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(::bof3::Img(Prim_VertexScratch_addr) + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return Low32(p); }
std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }
void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(move_script::Long(at)) + v)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }
// A signed byte widened (movsx).
int SignedByte(unsigned char b) { return static_cast<signed char>(b); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the party's
// by the index, the enemy's by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(::bof3::Img(Battle_Enemies_addr) + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
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

using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S33_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

// The four projected points at +8, +8 + step, +8 + 2 step, +8 + 3 step
// (0xC a flat quad's, 0x10 a gouraud one's).
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S33_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}

// The owner's position +0x34 / +0x38 / +0x3C to the task's.
void AtOwner() {
    rv::Store(rv::Current<records::BattleTask>()->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
}

// A new kind-1 child (0x44) owned by this task with phase `phase`, counted in
// +0xB; the child's slot.
unsigned char* Child44(unsigned char phase) {
    const unsigned slot = NewTask(0x44);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = static_cast<std::int32_t>(Key(s));
    child[1] = phase;
    return child;
}

}  // namespace

#define S33_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC151 (row 86, Accession read one id down)

// ===========================================================================
// MAGIC154 (row 35, Mighty Chop read one id down)

// original 0x4ED0C0: the kind-2 task. A four-entry stack table by +1:
// MightyChop_Start, _Throw, _End, BattleFx_Finish.
S33_EXPORT void __cdecl MightyChop_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::MightyChop_Start, bof3::addr::MightyChop_Throw,
                                                 bof3::addr::MightyChop_End, bof3::addr::BattleFx_Finish};
    const unsigned phase = Sc()[1];
    if (phase >= 4) PastTable("MightyChop_Task", phase, 4);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4ED100: the actor's animation 0xC (argument 2); a copy of the
// acting actor's record (party below 3, else the enemy by index - 3,
// unchecked; its first 0x80 bytes) as a child of kind 1, 0x52, +1 1, +2 0, +6
// 1, +5 0x52; the first 16 words of CLUT row 26 back from their source; the
// owner's +0 bit 0x40; +0xB and +9 1, +1 on.
S33_EXPORT void __cdecl MightyChop_Start(void) {
    MH_CALL(BattleActor_SetAnimation)(0xC, 2);
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x52) & 0xFFu;
    const unsigned actor = Mem(::bof3::Img(at::kActor))[0];
    const unsigned char* const record = actor < 3 ? PartyRecord(actor) : EnemyRecord(actor);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    // rep movsd: 0x20 dwords, forward, one at a time (the two can overlap)
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, move_script::Long(record + k));
    tchild->owner = static_cast<std::int32_t>(Key(s));
    child[1] = 1;
    child[2] = 0;
    child[6] = 1;
    child[5] = 0x52;
    for (unsigned k = 0x1A00; k < 0x1A10; ++k) Gfx_ClutStrip[k] = Gfx_ClutStripSource[k];
    Gfx_ClutStripDirty = 1;
    Owner()[0] |= 0x40;
    Sc()[0xB] = 1;
    Sc()[9] = 1;
    Inc(Sc()[1]);
}

// original 0x4ED1F0: once the copy has ended (+0xB 0): the source sprite's
// direction; the offset (0, -0x18000) turned by it, from the source sprite;
// its height; sound 0x100; six blades (kind 1, 0x52, +1 0, +0xB i, +9 3 i + 1)
// counted in +0xB; +1 on.
S33_EXPORT void __cdecl MightyChop_Throw(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned char* const source = Pointer(::bof3::Img(BattleOdds_ActorObject_addr));
    Sc()[8] = source[8];
    rv::Current<records::BattleTask>()->step_x = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_z, static_cast<std::int32_t>(0xFFFE8000u));
    Turn(Sc());
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(U(rv::Current<records::BattleTask>()->step_x) + U(rv::As<records::SpriteHead>(source)->x)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(U(rv::Current<records::BattleTask>()->step_z) + U(rv::As<records::SpriteHead>(source)->z)));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(source)->height);
    MH_CALL(Sound_PlayById)(0x100);
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x52) & 0xFFu;
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = static_cast<std::int32_t>(Key(s));
        child[1] = 0;
        child[0xB] = static_cast<unsigned char>(i);
        child[9] = static_cast<unsigned char>(i * 3 + 1);
        Inc(s[0xB]);
    }
    Inc(Sc()[1]);
}

// original 0x4ED2D0: once the copy's script has ended (+9 0): the actor's
// animation 4 (argument 0), the owner's +0 bit 0x40 cleared, the target flags
// 0x10, +1 on.
S33_EXPORT void __cdecl MightyChop_End(void) {
    if (Sc()[9] != 0) return;
    MH_CALL(BattleActor_SetAnimation)(4, 0);
    Owner()[0] &= 0xBF;
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    Inc(Sc()[1]);
}

// original 0x4ED310: the children's kind-1 task, a jmp through
// MightyChopChild_Kinds (two entries: the blade, the copy) by +1, unchecked.
S33_EXPORT void __cdecl MightyChopChild_Task(void) {
    static constexpr std::uint32_t kKinds[2] = {bof3::addr::MightyChopBlade_Run, bof3::addr::MightyChopCopy_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 2) PastTable("MightyChopChild_Task", phase, 2);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4ED330: a blade. A draw-mode packet (tpage 0x35) on layer 3; the
// frame-offset table 0x9039D8 the effects' (0x8E3580) while a call through
// MightyChopBlade_Steps (four entries) by +2 runs and, while +0 and +2 are
// set, the sprite is updated; then the battle's (0x8B3580) back.
S33_EXPORT void __cdecl MightyChopBlade_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::MightyChopBlade_Start, bof3::addr::MightyChopBlade_Grow,
                                                bof3::addr::MightyChopBlade_Shrink, bof3::addr::MightyChopBlade_Sink};
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetEffect.v)));
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("MightyChopBlade_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    }
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), static_cast<std::int32_t>(::bof3::Img(kFrameSetBattle.v)));
}

// original 0x4ED390: +9 down; at 0 the blade starts: the owner's direction;
// the offset (0, +0xB << 15) turned by it, from the owner; its height; the
// scale +0x40 / +0x44 0x8000; +0x48 2; the sprite fields +0x24 4, +0x25 0x1D,
// +0x26 0, +0x27 0xA0, +0x28 0, +0x29 3, +0x2A the direction & 1, +0x2B 0,
// +0x2C 0; animation 0, its script ticked; +9 0, +2 on.
S33_EXPORT void __cdecl MightyChopBlade_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[8] = Owner()[8];
    rv::Current<records::BattleTask>()->step_x = 0;
    rv::Store(rv::Current<records::BattleTask>()->step_z, static_cast<std::int32_t>(static_cast<std::uint32_t>(Sc()[0xB]) << 15));
    Turn(Sc());
    rv::Store(rv::Current<records::BattleTask>()->x, static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->x) + U(rv::Current<records::BattleTask>()->step_x)));
    rv::Store(rv::Current<records::BattleTask>()->z, static_cast<std::int32_t>(U(rv::As<records::SpriteHead>(Owner())->z) + U(rv::Current<records::BattleTask>()->step_z)));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    rv::Store(rv::Current<records::BattleTask>()->scale_x, 0x8000);
    rv::Store(rv::Current<records::BattleTask>()->scale_y, 0x8000);
    Sc()[0x48] = 2;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0x2A] = static_cast<unsigned char>(Sc()[8] & 1);
    Sc()[0x29] = 3;
    Sc()[0x24] = 4;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 0;
    MH_CALL(Sprite_SetAnimation)(0);
    MH_CALL(Sprite_ScriptTick)();
    Sc()[9] = 0;
    Inc(Sc()[2]);
}

// original 0x4ED4B0: the scale +0x40 up by 0x4000, +0x44 by 0x8000, +9 up; at 4
// +2 on.
S33_EXPORT void __cdecl MightyChopBlade_Grow(void) {
    AddLong(Sc() + 0x40, 0x4000);
    AddLong(Sc() + 0x44, 0x8000);
    Inc(Sc()[9]);
    if (Sc()[9] == 4) Inc(Sc()[2]);
}

// original 0x4ED4F0: the scale back down by the same, +9 down; at 0 the
// owner's count +0xB down and the task freed.
S33_EXPORT void __cdecl MightyChopBlade_Shrink(void) {
    AddLong(Sc() + 0x40, 0xFFFFC000u);
    AddLong(Sc() + 0x44, 0xFFFF8000u);
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4ED540: +0x44 down by 0x1000; once +0x5F is 0x80 the owner's
// count +0xB down and the task freed. (No step reaches it: _Shrink frees the
// task.)
S33_EXPORT void __cdecl MightyChopBlade_Sink(void) {
    AddLong(Sc() + 0x44, 0xFFFFF000u);
    if (Sc()[0x5F] != 0x80) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4ED570: the copy. A four-entry stack table by +2:
// BattleFx_SetSize, _Play, _Wait, BattleFx_FreeTask; then while +0 is set the
// sprite updated.
S33_EXPORT void __cdecl MightyChopCopy_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::BattleFx_SetSize, bof3::addr::MightyChopCopy_Play,
                                                bof3::addr::MightyChopCopy_Wait, bof3::addr::BattleFx_FreeTask};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("MightyChopCopy_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    if (Sc()[0] != 0) MH_CALL(Sprite_UpdateScreen)();
}

// original 0x4ED5E0: +9 down; at 0 the script ticked and the actor's sound
// (2, 4); else the script ticked and, at its end, the sound and +9 0; either
// way then the owner's +0xB down and +2 on.
S33_EXPORT void __cdecl MightyChopCopy_Play(void) {
    Dec(Sc()[9]);
    if (Sc()[9] == 0) {
        MH_CALL(Sprite_ScriptTickOnce)();
        MH_CALL(BattleActor_PlaySound)(2, 4);
    } else {
        if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
        MH_CALL(BattleActor_PlaySound)(2, 4);
        Sc()[9] = 0;
    }
    Dec(Owner()[0xB]);
    Inc(Sc()[2]);
}

// original 0x4ED650: the script ticked; at its end the owner's +9 down (the
// parent's _End waits for it) and +2 on.
S33_EXPORT void __cdecl MightyChopCopy_Wait(void) {
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    Dec(Owner()[9]);
    Inc(Sc()[2]);
}

void MagicS33_Inject() {
    if (bof3::WantsShadow("magic_s33")) magic_s33::SelfTest();
    BOF3_INJECT(MightyChop_Task);
    BOF3_INJECT(MightyChop_Start);
    BOF3_INJECT(MightyChop_Throw);
    BOF3_INJECT(MightyChop_End);
    BOF3_INJECT(MightyChopChild_Task);
    BOF3_INJECT(MightyChopBlade_Run);
    BOF3_INJECT(MightyChopBlade_Start);
    BOF3_INJECT(MightyChopBlade_Grow);
    BOF3_INJECT(MightyChopBlade_Shrink);
    BOF3_INJECT(MightyChopBlade_Sink);
    BOF3_INJECT(MightyChopCopy_Run);
    BOF3_INJECT(MightyChopCopy_Play);
    BOF3_INJECT(MightyChopCopy_Wait);
}
