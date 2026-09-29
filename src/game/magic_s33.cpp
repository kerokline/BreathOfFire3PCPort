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

// The scratch the overlays keep their working values in: DamageScratch's
// sixteen bytes (0x903850..0x90385F, Scratch_Swap at +0xC) and the four
// SVECTORs of Prim_VertexScratch (0x9037A0..0x9037BF). Both are read again
// after every call, as the originals read them.
constexpr std::uint32_t kS = 0x903850;
constexpr std::uint32_t kV = 0x9037A0;

// The cells the overlays read beyond the harness's names.
constexpr std::uint32_t kCentreX = 0x903780;       // i32: the fight's centre (field_hidden_callees.h)
constexpr std::uint32_t kCentreZ = 0x903784;
constexpr std::uint32_t kFormation = 0x904B89;     // u8: magic_lib's kFormation; indexes the tables below
constexpr std::uint32_t kPartySet = 0x90412C;      // u8: the loaded party set (field_event_callees.h)
constexpr std::uint32_t kPalette = 0x904B79;       // u8: the palette index Sprite_LoadPalette is given
constexpr std::uint32_t kFrameSet = 0x9039D8;      // the sprite frame-offset table pointer (sprite_pose.h)
constexpr std::uint32_t kFrameSetBattle = 0x8B3580;
constexpr std::uint32_t kFrameSetEffect = 0x8E3580;
constexpr std::uint32_t kBannerText = 0x904EA0;    // char[]: the banner's text
constexpr std::uint32_t kBannerIds = 0x64ECB0;     // u8 by 0x904B89: the system message id
constexpr std::uint32_t kFormFiles = 0x64E9BC;     // two u16 * by 0x904B89 (stride 8): the DAT ids by party set
constexpr std::uint32_t kPalettes = 0x80D380;      // Gfx_ClutStripSource row 0x1B4: 64 bytes a sprite slot
constexpr std::uint32_t kSetMembers = 0x669750;    // u8[3] by party set: the members
constexpr std::uint32_t kMemberOffsets = 0x64DF70; // BattleWin_MemberTargetOffsets: s8 pairs by (column + 4 row)
constexpr std::uint32_t kSideOffsets = 0x64E2BC;   // s8 by (0x904AAC ^ 2) >> 1 (stride 2)
constexpr std::uint32_t kSparkAngles = 0x65BEA0;   // u8 pairs by the spark's +4: its two angles
// Per member, stride 0x14C, beside the party records.
constexpr std::uint32_t kMemberClut = 0x939B07;    // u8: the member's +0x27 kept
constexpr std::uint32_t kMemberB05 = 0x939C05;     // u8
constexpr std::uint32_t kMemberC10 = 0x939C10;     // u32
constexpr std::uint32_t kWindowRows = 0x803337;    // the battle windows 0xD.. (Window_Alloc's records +3, stride 0x24)

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(at::kOwner); }
unsigned char TargetByte() { return Mem(at::kTarget)[0]; }
unsigned ActorIndex() { return static_cast<unsigned>(Long(Mem(at::kActor))) & 0xFF; }

std::uint16_t SW(unsigned k) { return Word(Mem(kS + k)); }
short SS(unsigned k) { return static_cast<short>(SW(k)); }
unsigned char SB(unsigned k) { return Mem(kS + k)[0]; }
void SetSW(unsigned k, unsigned v) { SetWord(Mem(kS + k), v & 0xFFFF); }

std::uint16_t VW(unsigned k) { return Word(Mem(kV + k)); }
short VS(unsigned k) { return static_cast<short>(VW(k)); }
void SetVW(unsigned k, unsigned v) { SetWord(Mem(kV + k), v & 0xFFFF); }
void AddVW(unsigned k, unsigned v) { SetVW(k, VW(k) + v); }
const short* VP(unsigned k) { return reinterpret_cast<const short*>(Mem(kV + k)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }
void AddLong(unsigned char* at, std::uint32_t v) { SetLong(at, static_cast<std::int32_t>(U(Long(at)) + v)); }

// `imul` then `sar 0xC`: the 32-bit product wraps, the shift is arithmetic.
int Mul12(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> 12; }
// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }
// A signed byte widened (movsx).
int SignedByte(unsigned char b) { return static_cast<signed char>(b); }

unsigned char* TaskSlot(unsigned slot) { return Mem(at::kTasks + slot * at::kTaskStride); }
// The originals index the records by the battle index, unchecked: the party's
// by the index, the enemy's by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned battle_index) {
    return Mem(at::kEnemies + static_cast<std::uint32_t>(static_cast<int>(battle_index) - 3) * at::kEnemyStride);
}

std::uint32_t RandCall() { return static_cast<std::uint32_t>(MH_CALL(Rand)()); }
unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();
using Fn2 = void (__cdecl*)(unsigned, unsigned);
using Fn3 = void (__cdecl*)(int, int, int);
void Call0(std::uint32_t address) { MH_AT(Fn0, address)(); }
void Call2(std::uint32_t address, unsigned a, unsigned b) { MH_AT(Fn2, address)(a, b); }
void Call3(std::uint32_t address, int a, int b, int c) { MH_AT(Fn3, address)(a, b, c); }

// Capcom's, unnamed, in no group: turns the dx / dz pair +0xC / +0x10 of the
// task it is given by its direction byte +8 (docs/magic_s22.md).
constexpr std::uint32_t kTurnOffset = 0x446770;
using TaskFn = void (__cdecl*)(unsigned char*);
void Turn(unsigned char* task) { MH_AT(TaskFn, kTurnOffset)(task); }
// Capcom's, unnamed, in no group: resets the acting member's battle state
// (its +0x138 / +0x13C, flag bits of +0x130 / +0x134, then more; not read to
// its end here). Takes nothing.
constexpr std::uint32_t kResetActor = bof3::addr::DragonForm_Transform;   // rebound 2026-09-29 (round twelve, BE6): the value is unchanged, the fuzz keys on it

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The callees with the arguments the originals push. Gte_RotTransPers gets the
// flag pointer the originals pass; the four-point projection the depth and
// flag pointers.
using Rtp1Fn = long (__cdecl*)(const short*, unsigned char*, long*, long*);
using Rtp4Fn = long (__cdecl*)(const short*, const short*, const short*, const short*, unsigned char*, unsigned char*,
                               unsigned char*, unsigned char*, long*, long*);
#define S33_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&::name)))

void Rtp1(unsigned v, unsigned char* sxy) {
    long p, flag;
    S33_AS(Rtp1Fn, Gte_RotTransPers)(VP(v), sxy, &p, &flag);
}
// The four projected points at +8, +8 + step, +8 + 2 step, +8 + 3 step
// (0xC a flat quad's, 0x10 a gouraud one's).
void Rtp4(unsigned char* prim, unsigned step) {
    long p, flag;
    S33_AS(Rtp4Fn, Gte_RotTransPers4)(VP(0), VP(8), VP(0x10), VP(0x18), prim + 8, prim + 8 + step, prim + 8 + 2 * step,
                                     prim + 8 + 3 * step, &p, &flag);
}
void LinkAtSprite(unsigned size) {
    const unsigned char* const s = Sc();
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(Long(s + 0x34)), static_cast<unsigned long>(Long(s + 0x38)), 2,
                                size);
}
void LoadPaletteOfCurrent() {
    const unsigned char* const c = Sprite_Current;
    const unsigned char index = Mem(kPalette)[0];
    MH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(Mem(kPalettes + (static_cast<unsigned>(c[5]) << 6))),
                                index);
}

// The owner's position +0x34 / +0x38 / +0x3C to the task's.
void AtOwner() {
    SetLong(Sc() + 0x34, Long(Owner() + 0x34));
    SetLong(Sc() + 0x38, Long(Owner() + 0x38));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
}

// A new kind-1 child (0x44) owned by this task with phase `phase`, counted in
// +0xB; the child's slot.
unsigned char* Child44(unsigned char phase) {
    const unsigned slot = NewTask(0x44);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
    child[1] = phase;
    return child;
}

// The DAT file for the acting kind: the u16 table of 0x64E9BC + 8 x 0x904B89,
// its first pointer for an owner facing 0 or 1, else its second.
const unsigned char* FormTable() {
    const unsigned facing = Owner()[8];
    const unsigned kind = Mem(kFormation)[0];
    const std::uint32_t cell = kFormFiles + kind * 8u + (facing == 0 || facing == 1 ? 0u : 4u);
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(U(Long(Mem(cell)))));
}

}  // namespace

#define S33_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC151 (row 86, Accession read one id down)

// original 0x4EAE70: the kind-2 task. A three-entry stack table by +1:
// Accession_Start, Accession_Run, Accession_End.
S33_EXPORT void __cdecl Accession_Task(void) {
    static constexpr std::uint32_t kPhases[3] = {bof3::addr::Accession_Start, bof3::addr::Accession_Run,
                                                 bof3::addr::Accession_End};
    const unsigned phase = Sc()[1];
    if (phase >= 3) PastTable("Accession_Task", phase, 3);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4EAEA0: the engine's 0x4514A0; 0x8031F3 1; a banner (kind 1, 0x3C
// frames) of eight characters of the system message 0x64ECB0[0x904B89],
// copied to 0x904EA0; +2 on when 0x904B89 is 3, 0xB or 0xD; +9 3 when the
// target's party +0x89 is 0, else 0x1E; +1 on.
S33_EXPORT void __cdecl Accession_Start(void) {
    MH_AT(Fn0, kResetActor)();
    const unsigned kind = Mem(kFormation)[0];
    Mem(0x8031F3)[0] = 1;
    const unsigned id = Mem(kBannerIds + kind)[0];
    const unsigned char* const text = MH_CALL(Msg_SystemPtr)(id);
    MH_CALL(Str_CopyN)(reinterpret_cast<char*>(Mem(kBannerText)), reinterpret_cast<const char*>(text), 8);
    MH_CALL(BattleBanner_Add)(1, 1, 0, 0x3C, reinterpret_cast<const char*>(Mem(kBannerText)));
    const unsigned again = Mem(kFormation)[0];
    if (again == 3 || again == 0xB || again == 0xD) Inc(Sc()[2]);
    Sc()[9] = PartyRecord(TargetByte())[0x89] == 0 ? 3 : 0x1E;
    Inc(Sc()[1]);
}

// original 0x4EAF50: a two-entry stack table by +2: Accession_StepsA,
// Accession_StepsB.
S33_EXPORT void __cdecl Accession_Run(void) {
    static constexpr std::uint32_t kSteps[2] = {bof3::addr::Accession_StepsA, bof3::addr::Accession_StepsB};
    const unsigned phase = Sc()[2];
    if (phase >= 2) PastTable("Accession_Run", phase, 2);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4EAF80: a five-entry stack table by +3: _ActorPose,
// _ActorScript, _LoadFormA, _ApplyA, _Finish.
S33_EXPORT void __cdecl Accession_StepsA(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::Accession_ActorPose, bof3::addr::Accession_ActorScript,
                                                bof3::addr::Accession_LoadFormA, bof3::addr::Accession_ApplyA,
                                                bof3::addr::Accession_Finish};
    const unsigned phase = Sc()[3];
    if (phase >= 5) PastTable("Accession_StepsA", phase, 5);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4EAFC0: on the acting member's record (party, by 0x904B34
// unchecked) as Sprite_Current, animation +8 + 0x3C ensured; Sprite_Current
// back, +3 on.
S33_EXPORT void __cdecl Accession_ActorPose(void) {
    unsigned char* const saved = Sc();
    unsigned char* const rec = PartyRecord(ActorIndex());
    Sprite_Current = rec;
    MH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(rec[8] + 0x3C));
    Sprite_Current = saved;
    Inc(saved[3]);
}

// original 0x4EB010: +9 down; at 0 sound 0x100 (0x101 when the target's party
// +0x89 is not 0). Then the acting member's script ticked once as
// Sprite_Current; at its end the member's +0 bit 0x40, and back on this task
// +0xB 0, a controller child (kind 1, 0x44, +1 4), +3 on.
S33_EXPORT void __cdecl Accession_ActorScript(void) {
    Dec(Sc()[9]);
    if (Sc()[9] == 0) MH_CALL(Sound_PlayById)(PartyRecord(TargetByte())[0x89] == 0 ? 0x100 : 0x101);
    unsigned char* const saved = Sc();
    Sprite_Current = PartyRecord(ActorIndex());
    if (MH_CALL(Sprite_ScriptTickOnce)() == 0) {
        Sprite_Current = saved;
        return;
    }
    Sprite_Current[0] |= 0x40;
    Sprite_Current = saved;
    saved[0xB] = 0;
    Child44(4);
    Inc(Sc()[3]);
}

// original 0x4EB0F0: once the controller signals (+0xB 1): the DAT file
// FormTable()[party set] loaded, +3 on.
S33_EXPORT void __cdecl Accession_LoadFormA(void) {
    if (Sc()[0xB] != 1) return;
    const unsigned char* const table = FormTable();
    const unsigned set = static_cast<unsigned>(Long(Mem(kPartySet))) & 0xFF;
    MH_CALL(LoadDatFile)(static_cast<int>(Word(table + set * 2u)));
    Inc(Sc()[3]);
}

// original 0x4EB160: once the file is loaded: 0x904AA8 bit 14 cleared when set
// and 0x904B8A is the actor; on the acting member's record as Sprite_Current,
// its tint released, its +0x27 kept at 0x939B07, its palette reloaded
// (0x80D380 + its +5 x 64, index 0x904B79), its status tint (+0x90), its CLUT
// STP bits, animation +8 + 4 ensured; Sprite_Current back, +3 on.
S33_EXPORT void __cdecl Accession_ApplyA(void) {
    if (MH_CALL(File_LoadDone)() == 0) return;
    if ((U(Long(Mem(at::kFlags))) & 0x4000) != 0 && Mem(0x904B8A)[0] == Mem(at::kActor)[0])
        SetWord(Mem(at::kFlags), Word(Mem(at::kFlags)) & 0xBFFFu);
    unsigned char* const saved = Sc();
    unsigned char* const rec = PartyRecord(ActorIndex());
    Sprite_Current = rec;
    MH_CALL(Sprite_ReleaseTint)(rec);
    {
        const unsigned a = ActorIndex();
        const unsigned char index = Mem(kPalette)[0];
        Mem(kMemberClut + a * at::kPartyStride)[0] = PartyRecord(a)[0x27];
        const unsigned char* const c = Sprite_Current;
        MH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(Mem(kPalettes + (static_cast<unsigned>(c[5]) << 6))),
                                    index);
    }
    MH_CALL(Battle_StatusTint)(Word(PartyRecord(ActorIndex()) + 0x90));
    MH_CALL(Sprite_SetClutStp)();
    MH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 4));
    Sprite_Current = saved;
    Inc(saved[3]);
}

// original 0x4EB250: the acting member's +0 bit 0x40 cleared; +1 on, +2 and +3
// 0.
S33_EXPORT void __cdecl Accession_Finish(void) {
    unsigned char* const saved = Sc();
    unsigned char* const rec = PartyRecord(ActorIndex());
    Sprite_Current = rec;
    rec[0] &= 0xBF;
    Sprite_Current = saved;
    Inc(saved[1]);
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// original 0x4EB2A0: a five-entry stack table by +3: _ActorPose,
// _ActorScript, _LoadFormB, _ApplyB, _Finish.
S33_EXPORT void __cdecl Accession_StepsB(void) {
    static constexpr std::uint32_t kSteps[5] = {bof3::addr::Accession_ActorPose, bof3::addr::Accession_ActorScript,
                                                bof3::addr::Accession_LoadFormB, bof3::addr::Accession_ApplyB,
                                                bof3::addr::Accession_Finish};
    const unsigned phase = Sc()[3];
    if (phase >= 5) PastTable("Accession_StepsB", phase, 5);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4EB2E0: once the controller signals (+0xB 1): party records 0..2
// +0 bit 0x40; the DAT file FormTable()[0] loaded, +3 on.
S33_EXPORT void __cdecl Accession_LoadFormB(void) {
    if (Sc()[0xB] != 1) return;
    for (unsigned i = 0; i < 3; ++i) PartyRecord(i)[0] |= 0x40;
    const unsigned char* const table = FormTable();
    MH_CALL(LoadDatFile)(static_cast<int>(Word(table)));
    Inc(Sc()[3]);
}

// original 0x4EB350: once the file is loaded: each present member (+0 bit 0)
// of 0..2 has its tint released, its +0x27 kept, 0x939C05 / 0x939C10 cleared,
// its queued item returned and its turn removed; the actor bit cleared; the
// acting member's record (0x14C bytes) copied over party record 0, placed at
// the fight's centre on the ground, task slot 0 there too; members 1 and 2
// cleared (+0 0), the actor 0; 0x904B8F the old 0x904AB0, 0x904AB0 and
// 0x904AB1 1; each present member's window (0xD + i) allocated and placed by
// 0x64DF70 / 0x64E2BC; then on the (new) acting member's record as
// Sprite_Current: its actor bit, +0 bit 0x40 cleared, its member sprite for
// the party set's first member, its palette, status tint, CLUT STP bits,
// animation +8 + 4; Sprite_Current back, +3 on.
S33_EXPORT void __cdecl Accession_ApplyB(void) {
    if (MH_CALL(File_LoadDone)() == 0) return;
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const m = PartyRecord(i);
        if ((m[0] & 1) == 0) continue;
        MH_CALL(Sprite_ReleaseTint)(m);
        Mem(kMemberClut + i * at::kPartyStride)[0] = m[0x27];
        Mem(kMemberB05 + i * at::kPartyStride)[0] = 0;
        SetLong(Mem(kMemberC10 + i * at::kPartyStride), 0);
        MH_CALL(Battle_ReturnQueuedItem)(i);
        MH_CALL(Battle_RemoveFromTurnOrder)(i);
    }
    MH_CALL(Battle_ClearActorBit)(Mem(at::kActor)[0]);
    {
        unsigned char* const first = PartyRecord(0);
        const unsigned char* const from = PartyRecord(ActorIndex());
        const std::int32_t x = Long(Mem(kCentreX));
        // rep movsd: 0x53 dwords, forward, one at a time
        for (unsigned k = 0; k < 0x14C; k += 4) SetLong(first + k, Long(from + k));
        const std::int32_t z = Long(Mem(kCentreZ));
        first[5] = 0;
        SetLong(first + 0x34, x);
        SetLong(first + 0x38, z);
        const long ground = MH_CALL(AreaMap_Elevation)(x, z);
        const std::int32_t z0 = Long(first + 0x38);
        const std::int32_t cz = Long(Mem(kCentreZ));
        SetWord(first + 0x3E, U(ground) & 0xFFFF);
        SetLong(Mem(at::kTasks + 0x34), Long(Mem(kCentreX)));
        const std::int32_t x0 = Long(first + 0x34);
        SetLong(Mem(at::kTasks + 0x38), cz);
        const long ground0 = MH_CALL(AreaMap_Elevation)(x0, z0);
        const unsigned char was = Mem(0x904AB0)[0];
        Mem(0x904AA9)[0] |= 0x80;
        SetLong(Mem(at::kTasks + 0x3C), static_cast<std::int32_t>(Shl(static_cast<short>(ground0), 16)));
        PartyRecord(1)[0] = 0;
        PartyRecord(2)[0] = 0;
        Mem(at::kActor)[0] = 0;
        Mem(0x904B8F)[0] = was;
        Mem(0x904AB0)[0] = 1;
        Mem(0x904AB1)[0] = 1;
    }
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char* const m = PartyRecord(i);
        unsigned char* const w = Mem(kWindowRows + i * 0x24u);
        if ((m[0] & 1) == 0) continue;
        MH_CALL(Window_Alloc)(i + 0xD, 3);
        const unsigned column = m[8], row = m[0x89];
        w[-1] = 5;
        w[0] = 0;
        const unsigned k = (column + row * 4u) * 2u;
        const unsigned side = ((U(Long(Mem(0x904AAC))) & 0xFF) ^ 2u) >> 1;
        const unsigned dx = static_cast<unsigned>(SignedByte(Mem(kSideOffsets + side * 2u)[0]) +
                                                  SignedByte(Mem(kMemberOffsets + k)[0])) +
                            Word(m + 0x2E);
        SetWord(w + 1, dx & 0xFFFF);
        const unsigned dy =
            static_cast<unsigned>(SignedByte(Mem(kMemberOffsets + k + 1)[0])) + Word(m + 0x30) +
            0xCu;
        SetWord(w + 3, dy & 0xFFFF);
        w[7] = static_cast<unsigned char>(i);
        w[5] = 0;
        w[6] = 0;
    }
    const unsigned char was = Mem(0x904AB0)[0];
    unsigned char* const saved = Sc();
    Mem(0x80318E)[0] = was;
    const unsigned a = ActorIndex();
    SetWord(Mem(at::kFlags), Word(Mem(at::kFlags)) & 0xBFFFu);
    SetWord(Mem(0x803188), 0x70);
    unsigned char* const rec = PartyRecord(a);
    Sprite_Current = rec;
    MH_CALL(Battle_SetActorBit)(rec[5]);
    Sprite_Current[0] &= 0xBF;
    {
        const unsigned set = static_cast<unsigned>(Long(Mem(kPartySet))) & 0xFF;
        MH_CALL(Field_MemberSprite)(Mem(kSetMembers + set * 3u)[0], 0);
    }
    LoadPaletteOfCurrent();
    MH_CALL(Battle_StatusTint)(Word(PartyRecord(ActorIndex()) + 0x90));
    MH_CALL(Sprite_SetClutStp)();
    MH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(Sprite_Current[8] + 4));
    Sprite_Current = saved;
    Inc(saved[3]);
}

// original 0x4EB600: once the controller has ended (+0xB 2): the effect's
// done flag; the acting member's +0x134 bit 2 cleared and bit 0x20 set; the
// task freed.
S33_EXPORT void __cdecl Accession_End(void) {
    if (Sc()[0xB] != 2) return;
    const unsigned a = ActorIndex();
    Mem(at::kFlags)[0] |= 4;
    unsigned char* const flags = PartyRecord(a) + 0x134;
    SetLong(flags, static_cast<std::int32_t>((U(Long(flags)) & ~4u) | 0x20u));
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EB640: the children's kind-1 task, a jmp through
// AccessionChild_Kinds (five entries: the orb, the spark, the bolt, the ring,
// the controller) by +1, unchecked.
S33_EXPORT void __cdecl AccessionChild_Task(void) {
    static constexpr std::uint32_t kKinds[5] = {bof3::addr::AccessionOrb_Run, bof3::addr::AccessionSpark_Run,
                                                bof3::addr::AccessionBolt_Run, bof3::addr::AccessionRing_Run,
                                                bof3::addr::AccessionCtl_Run};
    const unsigned phase = Sc()[1];
    if (phase >= 5) PastTable("AccessionChild_Task", phase, 5);
    magic_harness::Phase(kKinds[phase])();
}

// original 0x4EB660: the controller, a six-entry stack table by +2: _Start,
// _SpawnRing, _SpawnOrb, _Signal, _WaitOwner, _End.
S33_EXPORT void __cdecl AccessionCtl_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::AccessionCtl_Start, bof3::addr::AccessionCtl_SpawnRing,
                                                bof3::addr::AccessionCtl_SpawnOrb, bof3::addr::AccessionCtl_Signal,
                                                bof3::addr::AccessionCtl_WaitOwner, bof3::addr::AccessionCtl_End};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("AccessionCtl_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
}

// original 0x4EB6B0: the source sprite's direction; the fight's centre on the
// ground; +0xB 0, +9 4, +2 on; a bolt child (+1 2, +9 1) counted in +0xB;
// sound 0x102.
S33_EXPORT void __cdecl AccessionCtl_Start(void) {
    Sc()[8] = Pointer(at::kSource)[8];
    SetLong(Sc() + 0x34, Long(Mem(kCentreX)));
    SetLong(Sc() + 0x38, Long(Mem(kCentreZ)));
    {
        const unsigned char* const s = Sc();
        const long ground = MH_CALL(AreaMap_Elevation)(Long(s + 0x34), Long(s + 0x38));
        SetWord(Sc() + 0x3E, U(ground) & 0xFFFF);
    }
    Sc()[0xB] = 0;
    Sc()[9] = 4;
    Inc(Sc()[2]);
    unsigned char* const child = Child44(2);
    child[9] = 1;
    Inc(Sc()[0xB]);
    MH_CALL(Sound_PlayById)(0x102);
}

// original 0x4EB760: +9 down; at 0 a ring child (+1 3) counted in +0xB, +9 4,
// +2 on.
S33_EXPORT void __cdecl AccessionCtl_SpawnRing(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Child44(3);
    Inc(Sc()[0xB]);
    Sc()[9] = 4;
    Inc(Sc()[2]);
}

// original 0x4EB7D0: +9 down; at 0 an orb child (+1 0) counted in +0xB, sound
// 0x103, +9 0x14, +2 on.
S33_EXPORT void __cdecl AccessionCtl_SpawnOrb(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Child44(0);
    Inc(Sc()[0xB]);
    MH_CALL(Sound_PlayById)(0x103);
    Sc()[9] = 0x14;
    Inc(Sc()[2]);
}

// original 0x4EB850: +9 down; at 0 the owner's +0xB 1 (its load may start), +2
// on.
S33_EXPORT void __cdecl AccessionCtl_Signal(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Owner()[0xB] = 1;
    Inc(Sc()[2]);
}

// original 0x4EB880: once the owner's +1 is 2 (its steps are done), +2 on.
S33_EXPORT void __cdecl AccessionCtl_WaitOwner(void) {
    if (Owner()[1] == 2) Inc(Sc()[2]);
}

// original 0x4EB8A0: once every child has ended (+0xB 0), the owner's +0xB 2
// and the task freed.
S33_EXPORT void __cdecl AccessionCtl_End(void) {
    if (Sc()[0xB] != 0) return;
    Owner()[0xB] = 2;
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EB8C0: the orb. A call through AccessionOrb_Steps (six entries)
// by +2; then while +0 and +2 are set, under the actor matrix, its shell and
// its glow.
S33_EXPORT void __cdecl AccessionOrb_Run(void) {
    static constexpr std::uint32_t kSteps[6] = {bof3::addr::AccessionOrb_Start, bof3::addr::AccessionOrb_Grow,
                                                bof3::addr::AccessionOrb_Emit, bof3::addr::AccessionOrb_WaitChildren,
                                                bof3::addr::AccessionOrb_Dim, bof3::addr::AccessionOrb_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 6) PastTable("AccessionOrb_Run", phase, 6);
    magic_harness::Phase(kSteps[phase])();
    const unsigned char* const s = Sc();
    if (s[0] == 0 || s[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::AccessionOrb_DrawShell);
    Call0(bof3::addr::AccessionOrb_DrawGlow);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EB900: at the owner; +0x5D and +0x5E 0x10, +0x5F 0x24, +4, +0xB
// and +9 0, +0xA 0x1E, +2 on.
S33_EXPORT void __cdecl AccessionOrb_Start(void) {
    AtOwner();
    Sc()[0x5D] = 0x10;
    Sc()[0x5E] = 0x10;
    Sc()[0x5F] = 0x24;
    Sc()[4] = 0;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Sc()[0xA] = 0x1E;
    Inc(Sc()[2]);
}

// original 0x4EB980: the radius +9 up by 2; at 0x20 +2 on.
S33_EXPORT void __cdecl AccessionOrb_Grow(void) {
    AddB(Sc()[9], 2);
    if (Sc()[9] == 0x20) Inc(Sc()[2]);
}

// original 0x4EB9A0: every fourth frame a spark child (+1 1, +4 this task's +4
// & 0xF) counted in +0xB, +4 up; while the owner is at its step 5, +0xA down,
// at 0 +2 on.
S33_EXPORT void __cdecl AccessionOrb_Emit(void) {
    if ((static_cast<unsigned char>(Frame_Counter) & 3) == 0) {
        unsigned char* const child = Child44(1);
        unsigned char* const s = Sc();
        child[4] = static_cast<unsigned char>(s[4] & 0xF);
        Inc(s[0xB]);
        Inc(Sc()[4]);
    }
    if (Owner()[2] != 5) return;
    Dec(Sc()[0xA]);
    if (Sc()[0xA] == 0) Inc(Sc()[2]);
}

// original 0x4EBA30: once every spark has ended (+0xB 0), +2 on.
S33_EXPORT void __cdecl AccessionOrb_WaitChildren(void) {
    if (Sc()[0xB] == 0) Inc(Sc()[2]);
}

// original 0x4EBA40: the shell's shade +0x5D down; at 8 +2 on.
S33_EXPORT void __cdecl AccessionOrb_Dim(void) {
    Dec(Sc()[0x5D]);
    if (Sc()[0x5D] == 8) Inc(Sc()[2]);
}

// original 0x4EBA60: the shell's shade down to 0, the glow's +0x5E down; at 0
// the owner's count +0xB down and the task freed.
S33_EXPORT void __cdecl AccessionOrb_Fade(void) {
    if (Sc()[0x5D] != 0) Dec(Sc()[0x5D]);
    Dec(Sc()[0x5E]);
    if (Sc()[0x5E] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EBAA0: the orb's shell - a dome of radius +9 x 16 from latitude
// 0x400 to 0xB80 in eight bands of sixteen semi-transparent flat quads (tpage
// 0x55), each shaded +0x5D x 15 and linked at its corner's world position.
S33_EXPORT void __cdecl AccessionOrb_DrawShell(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0xA, static_cast<unsigned>(SignedByte(s[0x5D]) * 15));
        SetSW(0, static_cast<unsigned>(s[9]) << 4);
    }
    int v = MH_CALL(Math_Sin)(0x400);
    SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0x400);
    SetSW(6, static_cast<unsigned>(Mul12(v, SS(0))));
    int angle = 0x480;
    for (int bands = 8; bands != 0; --bands, angle += 0x80) {
        {
            const std::uint16_t h = SW(6), r = SW(2);
            SetSW(4, r);
            SetSW(8, h);
        }
        v = MH_CALL(Math_Sin)(angle);
        SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(angle);
        SetSW(6, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Sin)(0);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(0);
        SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
        {
            const std::uint16_t h = SW(6);
            SetVW(4, h);
            SetVW(0xC, h);
        }
        v = MH_CALL(Math_Sin)(0);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = MH_CALL(Math_Cos)(0);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
        {
            const std::uint16_t h = SW(8);
            SetVW(0x14, h);
            SetVW(0x1C, h);
        }
        for (unsigned i = 1; i < 0x11; ++i) {
            const unsigned turn = (i & 0xF) << 8;
            {
                const std::uint16_t x = VW(8), y = VW(0xA);
                SetSW(0xE, turn);
                SetVW(0, x);
                SetVW(2, y);
            }
            v = MH_CALL(Math_Sin)(static_cast<int>(turn));
            SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
            v = MH_CALL(Math_Cos)(SS(0xE));
            {
                const int w = Mul12(v, SS(2));
                const short e = SS(0xE);
                const std::uint16_t x = VW(0x18);
                SetVW(0xA, static_cast<unsigned>(w));
                const std::uint16_t y = VW(0x1A);
                SetVW(0x10, x);
                SetVW(0x12, y);
                v = MH_CALL(Math_Sin)(e);
            }
            SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
            v = MH_CALL(Math_Cos)(SS(0xE));
            const auto z = static_cast<short>(Mul12(v, SS(4)));
            const short vx = VS(0x18);
            SetVW(0x1A, static_cast<unsigned short>(z));
            unsigned char* p = Gfx_PacketNext;
            const unsigned char* const s = Sc();
            const std::uint32_t wx = Shl(vx, 9) + U(Long(s + 0x34));
            const std::uint32_t wz = Shl(z, 9) + U(Long(s + 0x38));
            MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x55, 0);
            MH_CALL(MapView_LinkPrimAt)(wx, wz, 1, 0xC);
            p = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(p);
            MH_CALL(Gpu_SetSemiTrans)(p, 1);
            Rtp4(p, 0xC);
            MH_CALL(Gte_PrimDepths4_0C)(p);
            p[4] = SB(0xA);
            p[5] = SB(0xA);
            p[6] = SB(0xA);
            MH_CALL(MapView_LinkPrimAt)(wx, wz, 1, 0x38);
        }
    }
}

// original 0x4EBDB0: the orb's glow - one band of sixteen semi-transparent
// gouraud quads (tpage 0x35) between latitude 0x400 at radius +9 x 16 and a
// latitude (Rand & 3 + +0x5F) x 32 at radius +9 x 18, dark at the inner edge,
// (+0x5E x 12, +0x5E x 8, +0x5E x 12) at the outer.
S33_EXPORT void __cdecl AccessionOrb_DrawGlow(void) {
    {
        const unsigned char* const s = Sc();
        SetSW(0xA, static_cast<unsigned>(SignedByte(s[0x5E]) * 12));
        SetSW(0xC, Shl(SignedByte(s[0x5E]), 3));
        SetSW(0, static_cast<unsigned>(s[9]) << 4);
    }
    int v = MH_CALL(Math_Sin)(0x400);
    SetSW(4, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(0x400);
    SetSW(8, static_cast<unsigned>(Mul12(v, SS(0))));
    SetSW(0, static_cast<unsigned>(Sc()[9]) * 18u);
    {
        const std::uint32_t r = RandCall();
        const auto lat = static_cast<short>(Shl(static_cast<int>(r & 3) + SignedByte(Sc()[0x5F]), 5));
        SetSW(0xE, static_cast<unsigned short>(lat));
        v = MH_CALL(Math_Sin)(lat);
    }
    SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Cos)(SS(0xE));
    SetSW(6, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    {
        const std::uint16_t h = SW(6);
        SetVW(4, h);
        SetVW(0xC, h);
    }
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(4))));
    {
        const std::uint16_t h = SW(8);
        SetVW(0x14, h);
        SetVW(0x1C, h);
    }
    for (unsigned i = 1; i < 0x11; ++i) {
        const unsigned turn = (i & 0xF) << 8;
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetSW(0xE, turn);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = MH_CALL(Math_Sin)(static_cast<int>(turn));
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(SS(0xE));
        {
            const int w = Mul12(v, SS(2));
            const short e = SS(0xE);
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(w));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
            v = MH_CALL(Math_Sin)(e);
        }
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(4))));
        v = MH_CALL(Math_Cos)(SS(0xE));
        const auto z = static_cast<short>(Mul12(v, SS(4)));
        const short vx = VS(0x18);
        SetVW(0x1A, static_cast<unsigned short>(z));
        unsigned char* p = Gfx_PacketNext;
        const unsigned char* const s = Sc();
        const std::uint32_t wx = Shl(vx, 9) + U(Long(s + 0x34));
        const std::uint32_t wz = Shl(z, 9) + U(Long(s + 0x38));
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x35, 0);
        MH_CALL(MapView_LinkPrimAt)(wx, wz, 1, 0xC);
        p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
        p[0x24] = SB(0xA);
        p[0x25] = SB(0xC);
        p[0x26] = SB(0xA);
        p[0x34] = SB(0xA);
        p[0x35] = SB(0xC);
        p[0x36] = SB(0xA);
        MH_CALL(MapView_LinkPrimAt)(wx, wz, 1, 0x44);
    }
}

// original 0x4EC0F0: a spark. A call through AccessionSpark_Steps (three
// entries) by +2; then while +0 and +2 are set, under the actor matrix: +0xC
// up, +9 up by 2, the lines at the angle pair 0x65BEA0[+4] (the first plus
// +9), unchecked.
S33_EXPORT void __cdecl AccessionSpark_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::AccessionSpark_Start, bof3::addr::AccessionSpark_Grow,
                                                bof3::addr::AccessionSpark_Fade};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("AccessionSpark_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] == 0 || s[2] == 0) return;
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    AddLong(Sc() + 0xC, 1);
    AddB(Sc()[9], 2);
    {
        const unsigned char* const s = Sc();
        const unsigned char* const pair = Mem(kSparkAngles + static_cast<unsigned>(s[4]) * 2u);
        Call2(bof3::addr::AccessionSpark_Draw, static_cast<unsigned char>(pair[0] + s[9]), pair[1]);
    }
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EC160: at the owner; +0xC Rand & 0x1F, +0x5D 0x10, +0xB and +9
// 0, +0xA 4, +2 on.
S33_EXPORT void __cdecl AccessionSpark_Start(void) {
    AtOwner();
    const std::uint32_t r = RandCall();
    SetLong(Sc() + 0xC, static_cast<std::int32_t>(r & 0x1F));
    Sc()[0x5D] = 0x10;
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    Sc()[0xA] = 4;
    Inc(Sc()[2]);
}

// original 0x4EC1E0: the line count +0xA up by 4; from 0x20 +0xB 1 and +2 on.
S33_EXPORT void __cdecl AccessionSpark_Grow(void) {
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] < 0x20) return;
    Sc()[0xB] = 1;
    Inc(Sc()[2]);
}

// original 0x4EC210: the shade +0x5D down by 2; at 0 the owner's count +0xB
// down and the task freed.
S33_EXPORT void __cdecl AccessionSpark_Fade(void) {
    AddB(Sc()[0x5D], 0xFE);
    if (Sc()[0x5D] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4EC240: the spark's +0xA lines (the count read back each line),
// each a semi-transparent flat line (tpage (+0xB & 3) << 5 | 0x15, mode +0xB)
// from the previous end to a point at radius 0x200 whose latitude wobbles
// about a2 (Rand & 3 + 4 by a sine of ((+0xC + i) & 0x1F) << 7) and whose
// longitude steps by Rand & 7 from a1 (the argument slot keeps the sum);
// shaded +0x5D x 12, linked at its end's world position.
S33_EXPORT void __cdecl AccessionSpark_Draw(unsigned a1, unsigned a2) {
    auto along = static_cast<unsigned char>(a1);
    const unsigned lat = a2 & 0xFF;
    SetSW(0xA, static_cast<unsigned>(SignedByte(Sc()[0x5D]) * 12));
    SetSW(0, 0x200);
    int v = MH_CALL(Math_Sin)(static_cast<int>(lat << 4));
    SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
    v = MH_CALL(Math_Sin)(static_cast<int>(static_cast<unsigned>(along) << 4));
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(static_cast<int>(static_cast<unsigned>(along) << 4));
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(static_cast<int>(lat << 4));
    SetVW(0xC, static_cast<unsigned>(Mul12(v, SS(0))));
    if (Sc()[0xA] == 0) return;
    for (unsigned i = 0;;) {
        {
            const std::uint16_t h = VW(0xC), x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
            SetVW(4, h);
        }
        std::uint32_t r = RandCall();
        SetSW(4, (r & 3) + 4);
        {
            const auto turn = static_cast<unsigned short>(((Sc()[0xC] + i) & 0x1F) << 7);
            SetSW(0xE, turn);
            v = MH_CALL(Math_Sin)(static_cast<short>(turn));
        }
        {
            const auto wobble = static_cast<short>(Shl(Mul12(v, SS(4)) + static_cast<int>(lat), 4));
            SetSW(0xE, static_cast<unsigned short>(wobble));
            v = MH_CALL(Math_Sin)(wobble);
        }
        SetSW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        v = MH_CALL(Math_Cos)(SS(0xE));
        SetVW(0xC, static_cast<unsigned>(Mul12(v, SS(0))));
        r = RandCall();
        along = static_cast<unsigned char>(along + (r & 7));
        const int lon = static_cast<int>(static_cast<unsigned>(along) << 4);
        v = MH_CALL(Math_Sin)(lon);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(lon);
        const auto z = static_cast<short>(Mul12(v, SS(2)));
        SetVW(0xA, static_cast<unsigned short>(z));
        const short vx = VS(8);
        std::uint32_t wx, wz;
        unsigned tpage;
        {
            const unsigned char* const s = Sc();
            wx = Shl(vx, 9) + U(Long(s + 0x34));
            tpage = ((s[0xB] & 3u) << 5) | 0x15u;
            wz = Shl(z, 9) + U(Long(s + 0x38));
        }
        MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
        MH_CALL(MapView_LinkPrimAt)(wx, wz, 2, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetLineF2)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, Sc()[0xB]);
        Rtp1(0, p + 8);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x10));
        Rtp1(8, p + 0x14);
        MH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(p + 0x1C));
        p[4] = SB(0xA);
        p[5] = SB(0xA);
        p[6] = SB(0xA);
        MH_CALL(MapView_LinkPrimAt)(wx, wz, 2, 0x20);
        ++i;
        if (static_cast<unsigned char>(i) >= Sc()[0xA]) break;
    }
}

// original 0x4EC500: the bolt. A call through AccessionBolt_Steps (four
// entries: _Start, _Rise, group S22's MyollnirBolt_Hold and JoltBolt_End) by
// +2, the screen point; then while +2 and +0 are set, under group S25's turn
// matrix, three band rows ((0x20, 0x20, 7), (0x10, 0x40, 0xF), (0x10, 0x60,
// 0x1F), the angle stepping by 4 and 4, then back).
S33_EXPORT void __cdecl AccessionBolt_Run(void) {
    static constexpr std::uint32_t kSteps[4] = {bof3::addr::AccessionBolt_Start, bof3::addr::AccessionBolt_Rise,
                                                bof3::addr::MyollnirBolt_Hold, bof3::addr::JoltBolt_End};
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("AccessionBolt_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    MH_CALL(BattleActor_UpdateScreenXY)();
    {
        const unsigned char* const s = Sc();
        if (s[2] == 0 || s[0] == 0) return;
    }
    MH_CALL(SpellSleep_PushTurnMatrix)();
    Call3(bof3::addr::AccessionBolt_DrawBand, 0x20, 0x20, 7);
    AddB(Sc()[0xB], 4);
    Call3(bof3::addr::AccessionBolt_DrawBand, 0x10, 0x40, 0xF);
    AddB(Sc()[0xB], 4);
    Call3(bof3::addr::AccessionBolt_DrawBand, 0x10, 0x60, 0x1F);
    AddB(Sc()[0xB], 0xF8);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4EC580: +9 down; at 0: at the owner, +0xB Rand & 0xF, +9 and +0xA
// 0, +2 on.
S33_EXPORT void __cdecl AccessionBolt_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    AtOwner();
    const std::uint32_t r = RandCall();
    Sc()[0xB] = static_cast<unsigned char>(r & 0xF);
    Sc()[9] = 0;
    Sc()[0xA] = 0;
    Inc(Sc()[2]);
}

// original 0x4EC600: +0xB up, +0xA up by 4; at 0x10 +9 8 and +2 on.
S33_EXPORT void __cdecl AccessionBolt_Rise(void) {
    Inc(Sc()[0xB]);
    AddB(Sc()[0xA], 4);
    if (Sc()[0xA] != 0x10) return;
    Sc()[9] = 8;
    Inc(Sc()[2]);
}

// original 0x4EC640: one row of the bolt - 17 steps of four semi-transparent
// gouraud quads (tpage 0x35): the radius s16 a2 plus or minus Rand & a3 (the
// first s16 a2 / 2), the corners a1 in, each step 0x40 higher, the angle
// ((+0xB + i) & 0xF) << 8; the shades +0xA x 15 and +0xA x 13 (Scratch_Swap),
// read back at every use; the first step's far edges dark.
S33_EXPORT void __cdecl AccessionBolt_DrawBand(int a1, int a2, int a3) {
    const auto in = static_cast<std::uint16_t>(a1);
    const auto in2 = static_cast<std::uint16_t>(static_cast<std::uint32_t>(a1) * 2);
    {
        const int r = static_cast<short>(a2);
        SetSW(0, static_cast<unsigned>(r / 2));
    }
    {
        const unsigned char* const s = Sc();
        SetSW(0xE, (s[0xB] & 0xFu) << 8);
        SetSW(0xA, static_cast<unsigned>(s[0xA]) * 15u);
        SetSW(0xC, static_cast<unsigned>(s[0xA]) * 13u);
    }
    int v = MH_CALL(Math_Sin)(SS(0xE));
    {
        unsigned char* const p = Gfx_PacketNext;
        SetVW(2, static_cast<unsigned>(Mul12(v, SS(0))));
        SetVW(4, 0);
        MH_CALL(Gpu_SetDrawMode)(p, 0, 1, 0x35, 0);
    }
    LinkAtSprite(0xC);
    for (int i = 1; i < 0x12; ++i) {
        SetSW(0xE, ((Sc()[0xB] + static_cast<unsigned>(i)) & 0xFu) << 8);
        std::uint32_t r = RandCall();
        if (r & 1) {
            r = RandCall();
            SetSW(0, (r & static_cast<std::uint32_t>(a3)) + static_cast<std::uint32_t>(a2));
        } else {
            r = RandCall();
            SetSW(0, static_cast<std::uint32_t>(a2) - (r & static_cast<std::uint32_t>(a3)));
        }
        {
            const std::uint16_t top = VW(2), h = VW(4);
            SetVW(0x12, top);
            SetVW(0x1A, static_cast<unsigned>(top) - in);
            SetVW(0x10, 0);
            SetVW(0x14, h);
            SetVW(0x18, 0);
            SetVW(0x1C, h);
            SetVW(0, 0);
        }
        v = MH_CALL(Math_Sin)(SS(0xE));
        SetVW(8, 0);
        unsigned char* p = Gfx_PacketNext;
        {
            const int y = Mul12(v, SS(0));
            const unsigned up = (0u - static_cast<unsigned>(i)) << 6;
            SetVW(2, static_cast<unsigned>(y));
            SetVW(4, up);
            SetVW(0xA, static_cast<unsigned>(y) - in);
            SetVW(0xC, up);
        }
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = SB(0xA);
        p[5] = SB(0xA);
        p[6] = SB(0xA);
        p[0x14] = SB(0xC);
        p[0x15] = 1;
        p[0x16] = SB(0xC);
        if (i == 1) {
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x24] = SB(0xA);
            p[0x25] = SB(0xA);
            p[0x26] = SB(0xA);
            p[0x34] = SB(0xC);
            p[0x35] = 1;
            p[0x36] = SB(0xC);
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, 0u - in);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[5] = 1;
        p[4] = SB(0xC);
        p[6] = SB(0xC);
        p[0x14] = 1;
        p[0x15] = 1;
        p[0x16] = 1;
        if (i == 1) {
            for (unsigned k : {0x24u, 0x25u, 0x26u}) p[k] = 1;
        } else {
            p[0x25] = 1;
            p[0x24] = SB(0xC);
            p[0x26] = SB(0xC);
        }
        p[0x34] = 1;
        p[0x35] = 1;
        p[0x36] = 1;
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, in2);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[5] = 1;
        p[4] = SB(0xC);
        p[6] = SB(0xC);
        p[0x14] = SB(0xA);
        p[0x15] = SB(0xA);
        p[0x16] = SB(0xA);
        if (i == 1) {
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x25] = 1;
            p[0x24] = SB(0xC);
            p[0x26] = SB(0xC);
            p[0x34] = SB(0xA);
            p[0x35] = SB(0xA);
            p[0x36] = SB(0xA);
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        p = Gfx_PacketNext;
        for (unsigned k : {2u, 0xAu, 0x12u, 0x1Au}) AddVW(k, in);
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        p[4] = 1;
        p[5] = 1;
        p[6] = 1;
        p[0x14] = SB(0xC);
        p[0x15] = 1;
        p[0x16] = SB(0xC);
        p[0x24] = 1;
        p[0x25] = 1;
        p[0x26] = 1;
        if (i == 1) {
            for (unsigned k : {0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            p[0x35] = 1;
            p[0x34] = SB(0xC);
            p[0x36] = SB(0xC);
        }
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        LinkAtSprite(0x44);
        AddVW(2, 0u - in2);
    }
}

// original 0x4ECB80: the rings. A call through AccessionRing_Steps (three
// entries: _Start, _Widen, group S29's DivineBeam_Widen) by +2; then while +0
// and +2 are set, under the actor matrix, the inner and the outer ring.
S33_EXPORT void __cdecl AccessionRing_Run(void) {
    static constexpr std::uint32_t kSteps[3] = {bof3::addr::AccessionRing_Start, bof3::addr::AccessionRing_Widen,
                                                bof3::addr::DivineBeam_Widen};
    const unsigned phase = Sc()[2];
    if (phase >= 3) PastTable("AccessionRing_Run", phase, 3);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] == 0 || s[2] == 0) return;
    }
    MH_CALL(MagicFx_PushActorMatrix)();
    Call0(bof3::addr::AccessionRing_DrawInner);
    Call0(bof3::addr::AccessionRing_DrawOuter);
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4ECBC0: at the owner; +0x14 0x100, +9 0x10, +2 on.
S33_EXPORT void __cdecl AccessionRing_Start(void) {
    AtOwner();
    SetLong(Sc() + 0x14, 0x100);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4ECC20: the radius +0x14 up by 0x80; at 0x400 +2 on.
S33_EXPORT void __cdecl AccessionRing_Widen(void) {
    AddLong(Sc() + 0x14, 0x80);
    if (Long(Sc() + 0x14) == 0x400) Inc(Sc()[2]);
}

namespace {

// A flat ring of 64 semi-transparent gouraud quads (tpage 0x35, layer 4)
// between the radii SS(2) = +0x14 + inner_add and SS(4) = +0x14 + outer_add;
// the shade +9 x 15 on the outer edge (vertices 0 and 1) when `bright_outer`,
// else on the inner (vertices 2 and 3), 1 on the other.
void DrawRing(unsigned inner_add, unsigned outer_add, bool bright_outer) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x35, 0);
    MH_CALL(Gfx_CommitPrim)(4, 0xC);
    {
        const unsigned char* const s = Sc();
        SetSW(2, Word(s + 0x14) + inner_add);
        SetSW(4, Word(s + 0x14) + outer_add);
        SetSW(0xA, static_cast<unsigned>(s[9]) * 15u);
    }
    int v = MH_CALL(Math_Sin)(0);
    SetVW(8, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0xA, static_cast<unsigned>(Mul12(v, SS(4))));
    v = MH_CALL(Math_Sin)(0);
    SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(2))));
    v = MH_CALL(Math_Cos)(0);
    SetVW(0x1C, 0);
    SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(2))));
    SetVW(0x14, 0);
    SetVW(0xC, 0);
    SetVW(4, 0);
    for (int a = 0x40; a < 0x1040; a += 0x40) {
        unsigned char* const p = Gfx_PacketNext;
        MH_CALL(Gpu_SetPolyG4)(p);
        MH_CALL(Gpu_SetSemiTrans)(p, 1);
        {
            const std::uint16_t x = VW(8), y = VW(0xA);
            SetVW(0, x);
            SetVW(2, y);
        }
        v = MH_CALL(Math_Sin)(a);
        SetVW(8, static_cast<unsigned>(Mul12(v, SS(4))));
        v = MH_CALL(Math_Cos)(a);
        {
            const int w = Mul12(v, SS(4));
            const std::uint16_t x = VW(0x18);
            SetVW(0xA, static_cast<unsigned>(w));
            const std::uint16_t y = VW(0x1A);
            SetVW(0x10, x);
            SetVW(0x12, y);
        }
        v = MH_CALL(Math_Sin)(a);
        SetVW(0x18, static_cast<unsigned>(Mul12(v, SS(2))));
        v = MH_CALL(Math_Cos)(a);
        SetVW(0x1A, static_cast<unsigned>(Mul12(v, SS(2))));
        Rtp4(p, 0x10);
        MH_CALL(Gte_PrimDepths4_10B)(p);
        if (bright_outer) {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = SB(0xA);
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = 1;
        } else {
            for (unsigned k : {4u, 5u, 6u, 0x14u, 0x15u, 0x16u}) p[k] = 1;
            for (unsigned k : {0x24u, 0x25u, 0x26u, 0x34u, 0x35u, 0x36u}) p[k] = SB(0xA);
        }
        MH_CALL(Gfx_CommitPrim)(4, 0x44);
    }
}

}  // namespace

// original 0x4ECC50: the inner ring, from the radius +0x14 to +0x14 + 0x40,
// bright at its outer edge.
S33_EXPORT void __cdecl AccessionRing_DrawInner(void) { DrawRing(0, 0x40, true); }

// original 0x4ECE80: the outer ring, from +0x14 + 0x40 to +0x14 + 0x80, bright
// at its inner edge.
S33_EXPORT void __cdecl AccessionRing_DrawOuter(void) { DrawRing(0x40, 0x80, false); }

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
    const unsigned actor = Mem(at::kActor)[0];
    const unsigned char* const record = actor < 3 ? PartyRecord(actor) : EnemyRecord(actor);
    unsigned char* const s = Sc();
    unsigned char* const child = TaskSlot(slot);
    // rep movsd: 0x20 dwords, forward, one at a time (the two can overlap)
    for (unsigned k = 0; k < 0x80; k += 4) SetLong(child + k, Long(record + k));
    SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
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
    const unsigned char* const source = Pointer(at::kSource);
    Sc()[8] = source[8];
    SetLong(Sc() + 0xC, 0);
    SetLong(Sc() + 0x10, static_cast<std::int32_t>(0xFFFE8000u));
    Turn(Sc());
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(U(Long(Sc() + 0xC)) + U(Long(source + 0x34))));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(U(Long(Sc() + 0x10)) + U(Long(source + 0x38))));
    SetLong(Sc() + 0x3C, Long(source + 0x3C));
    MH_CALL(Sound_PlayById)(0x100);
    for (unsigned i = 0; i < 6; ++i) {
        const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x52) & 0xFFu;
        unsigned char* const s = Sc();
        unsigned char* const child = TaskSlot(slot);
        SetLong(child + 0x80, static_cast<std::int32_t>(Key(s)));
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
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetEffect));
    const unsigned phase = Sc()[2];
    if (phase >= 4) PastTable("MightyChopBlade_Run", phase, 4);
    magic_harness::Phase(kSteps[phase])();
    {
        const unsigned char* const s = Sc();
        if (s[0] != 0 && s[2] != 0) MH_CALL(Sprite_UpdateScreen)();
    }
    SetLong(Mem(kFrameSet), static_cast<std::int32_t>(kFrameSetBattle));
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
    SetLong(Sc() + 0xC, 0);
    SetLong(Sc() + 0x10, static_cast<std::int32_t>(static_cast<std::uint32_t>(Sc()[0xB]) << 15));
    Turn(Sc());
    SetLong(Sc() + 0x34, static_cast<std::int32_t>(U(Long(Owner() + 0x34)) + U(Long(Sc() + 0xC))));
    SetLong(Sc() + 0x38, static_cast<std::int32_t>(U(Long(Owner() + 0x38)) + U(Long(Sc() + 0x10))));
    SetLong(Sc() + 0x3C, Long(Owner() + 0x3C));
    SetLong(Sc() + 0x40, 0x8000);
    SetLong(Sc() + 0x44, 0x8000);
    Sc()[0x48] = 2;
    Sc()[0x25] = 0x1D;
    Sc()[0x26] = 0;
    Sc()[0x27] = 0xA0;
    Sc()[0x28] = 0;
    Sc()[0x2A] = static_cast<unsigned char>(Sc()[8] & 1);
    Sc()[0x29] = 3;
    Sc()[0x24] = 4;
    SetWord(Sc() + 0x2C, 0);
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
    BOF3_INJECT(Accession_Task);
    BOF3_INJECT(Accession_Start);
    BOF3_INJECT(Accession_Run);
    BOF3_INJECT(Accession_StepsA);
    BOF3_INJECT(Accession_ActorPose);
    BOF3_INJECT(Accession_ActorScript);
    BOF3_INJECT(Accession_LoadFormA);
    BOF3_INJECT(Accession_ApplyA);
    BOF3_INJECT(Accession_Finish);
    BOF3_INJECT(Accession_StepsB);
    BOF3_INJECT(Accession_LoadFormB);
    BOF3_INJECT(Accession_ApplyB);
    BOF3_INJECT(Accession_End);
    BOF3_INJECT(AccessionChild_Task);
    BOF3_INJECT(AccessionCtl_Run);
    BOF3_INJECT(AccessionCtl_Start);
    BOF3_INJECT(AccessionCtl_SpawnRing);
    BOF3_INJECT(AccessionCtl_SpawnOrb);
    BOF3_INJECT(AccessionCtl_Signal);
    BOF3_INJECT(AccessionCtl_WaitOwner);
    BOF3_INJECT(AccessionCtl_End);
    BOF3_INJECT(AccessionOrb_Run);
    BOF3_INJECT(AccessionOrb_Start);
    BOF3_INJECT(AccessionOrb_Grow);
    BOF3_INJECT(AccessionOrb_Emit);
    BOF3_INJECT(AccessionOrb_WaitChildren);
    BOF3_INJECT(AccessionOrb_Dim);
    BOF3_INJECT(AccessionOrb_Fade);
    BOF3_INJECT(AccessionOrb_DrawShell);
    BOF3_INJECT(AccessionOrb_DrawGlow);
    BOF3_INJECT(AccessionSpark_Run);
    BOF3_INJECT(AccessionSpark_Start);
    BOF3_INJECT(AccessionSpark_Grow);
    BOF3_INJECT(AccessionSpark_Fade);
    BOF3_INJECT(AccessionSpark_Draw);
    BOF3_INJECT(AccessionBolt_Run);
    BOF3_INJECT(AccessionBolt_Start);
    BOF3_INJECT(AccessionBolt_Rise);
    BOF3_INJECT(AccessionBolt_DrawBand);
    BOF3_INJECT(AccessionRing_Run);
    BOF3_INJECT(AccessionRing_Start);
    BOF3_INJECT(AccessionRing_Widen);
    BOF3_INJECT(AccessionRing_DrawInner);
    BOF3_INJECT(AccessionRing_DrawOuter);
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
