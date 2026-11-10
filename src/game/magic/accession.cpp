// Moved here by tools/move_funcs.py from magic_s33.cpp (docs/source-layout-round.md).
#include "game/magic/accession.h"

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

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

// move_script::Long is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

constexpr ::bof3::image::Va kBannerText{0x904EA0};    // char[]: the banner's text

constexpr ::bof3::image::Va kBannerIds{0x64ECB0};     // u8 by 0x904B89: the system message id

constexpr ::bof3::image::Va kFormFiles{0x64E9BC};     // two u16 * by 0x904B89 (stride 8): the DAT ids by party set

// kPalettes: now Gfx_ClutStripSource[0xf00] (symbols.toml). Gfx_ClutStripSource row 0x1B4: 64 bytes a sprite slot
// kSetMembers: now MoveScript_EffectArg[0xc] (symbols.toml). u8[3] by party set: the members
// kMemberOffsets: now BattleWin_MemberTargetOffsets (symbols.toml). BattleWin_MemberTargetOffsets: s8 pairs by (column + 4 row)
constexpr ::bof3::image::Va kSideOffsets{0x64E2BC};   // s8 by (0x904AAC ^ 2) >> 1 (stride 2)

// kSparkAngles: now AccessionSpark_Angles (symbols.toml). u8 pairs by the spark's +4: its two angles
// Per member, stride 0x14C, beside the party records.
constexpr ::bof3::image::Va kMemberClut{0x939B07};    // u8: the member's +0x27 kept

constexpr ::bof3::image::Va kMemberB05{0x939C05};     // u8

constexpr ::bof3::image::Va kMemberC10{0x939C10};     // u32

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

unsigned ActorIndex() { return static_cast<unsigned>(move_script::Long(Mem(::bof3::Img(at::kActor)))) & 0xFF; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::uint32_t Key(const void* p) { return Low32(p); }

std::uint32_t U(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// `shl n` on a dword.
std::uint32_t Shl(int v, unsigned n) { return static_cast<std::uint32_t>(v) << n; }

// A signed byte widened (movsx).
int SignedByte(unsigned char b) { return static_cast<signed char>(b); }

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

// The originals index the records by the battle index, unchecked: the party's
// by the index, the enemy's by index - 3.
unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called by address, as the originals call them: in
// the game the jmp Inject put there (or Capcom's code under
// BOF3X_ORIGINAL), in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

// Capcom's, unnamed, in no group: resets the acting member's battle state
// (its +0x138 / +0x13C, flag bits of +0x130 / +0x134, then more; not read to
// its end here). Takes nothing.
constexpr ::bof3::image::CodeVa kResetActor{bof3::addr::DragonForm_Transform};   // rebound 2026-09-29 (round twelve, BE6): the value is unchanged, the fuzz keys on it

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

void LoadPaletteOfCurrent() {
    const unsigned char* const c = Sprite_Current;
    const unsigned char index = Mem(::bof3::Img(BattleE6_FormGroup_addr))[0];
    MH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + (static_cast<unsigned>(c[5]) << 6))),
                                index);
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

// The DAT file for the acting kind: the u16 table of 0x64E9BC + 8 x 0x904B89,
// its first pointer for an owner facing 0 or 1, else its second.
const unsigned char* FormTable() {
    const unsigned facing = Owner()[8];
    const unsigned kind = Mem(::bof3::Img(BattlePhases_B89_addr))[0];
    const std::uint32_t cell = ::bof3::Img(kFormFiles.v) + kind * 8u + (facing == 0 || facing == 1 ? 0u : 4u);
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(U(move_script::Long(Mem(cell)))));
}

}  // namespace

#define S33_EXPORT extern "C" __attribute__((disable_tail_calls))

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
    MH_AT(Fn0, kResetActor.v)();
    const unsigned kind = Mem(::bof3::Img(BattlePhases_B89_addr))[0];
    Mem(::bof3::Img(0x8031F3))[0] = 1;
    const unsigned id = Mem(::bof3::Img(kBannerIds.v) + kind)[0];
    const unsigned char* const text = MH_CALL(Msg_SystemPtr)(id);
    MH_CALL(Str_CopyN)(reinterpret_cast<char*>(Mem(::bof3::Img(kBannerText.v))), reinterpret_cast<const char*>(text), 8);
    MH_CALL(BattleBanner_Add)(1, 1, 0, 0x3C, reinterpret_cast<const char*>(Mem(::bof3::Img(kBannerText.v))));
    const unsigned again = Mem(::bof3::Img(BattlePhases_B89_addr))[0];
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
    const unsigned set = static_cast<unsigned>(move_script::Long(Mem(::bof3::Img(AreaW0b_PartySet_addr)))) & 0xFF;
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
    if ((U(move_script::Long(Mem(::bof3::Img(at::kFlags)))) & 0x4000) != 0 && Mem(::bof3::Img(BattleFlow_ActorAt_addr))[0] == Mem(::bof3::Img(at::kActor))[0])
        SetWord(Mem(::bof3::Img(at::kFlags)), Word(Mem(::bof3::Img(at::kFlags))) & 0xBFFFu);
    unsigned char* const saved = Sc();
    unsigned char* const rec = PartyRecord(ActorIndex());
    Sprite_Current = rec;
    MH_CALL(Sprite_ReleaseTint)(rec);
    {
        const unsigned a = ActorIndex();
        const unsigned char index = Mem(::bof3::Img(BattleE6_FormGroup_addr))[0];
        Mem(::bof3::Img(kMemberClut.v) + a * at::kPartyStride)[0] = PartyRecord(a)[0x27];
        const unsigned char* const c = Sprite_Current;
        MH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(Mem(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + (static_cast<unsigned>(c[5]) << 6))),
                                    index);
    }
    MH_CALL(Battle_StatusTint)(rv::As<records::PartyMember>(PartyRecord(ActorIndex()))->status);
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
        Mem(::bof3::Img(kMemberClut.v) + i * at::kPartyStride)[0] = m[0x27];
        Mem(::bof3::Img(kMemberB05.v) + i * at::kPartyStride)[0] = 0;
        SetLong(Mem(::bof3::Img(kMemberC10.v) + i * at::kPartyStride), 0);
        MH_CALL(Battle_ReturnQueuedItem)(i);
        MH_CALL(Battle_RemoveFromTurnOrder)(i);
    }
    MH_CALL(Battle_ClearActorBit)(Mem(::bof3::Img(at::kActor))[0]);
    {
        unsigned char* const first = PartyRecord(0);
        records::PartyMember* const tfirst = rv::As<records::PartyMember>(first);
        const unsigned char* const from = PartyRecord(ActorIndex());
        const std::int32_t x = move_script::Long(Mem(::bof3::Img(BossSi_CentreX_addr)));
        // rep movsd: 0x53 dwords, forward, one at a time
        for (unsigned k = 0; k < 0x14C; k += 4) SetLong(first + k, move_script::Long(from + k));
        const std::int32_t z = move_script::Long(Mem(::bof3::Img(BossSi_CentreZ_addr)));
        first[5] = 0;
        tfirst->x = x;
        tfirst->z = z;
        const long ground = MH_CALL(AreaMap_Elevation)(x, z);
        const std::int32_t z0 = tfirst->z;
        const std::int32_t cz = move_script::Long(Mem(::bof3::Img(BossSi_CentreZ_addr)));
        rv::Store<unsigned>(tfirst->height_whole, U(ground) & 0xFFFF);
        SetLong(Mem(::bof3::Img(BattleTask_Slots_addr) + 0x34), move_script::Long(Mem(::bof3::Img(BossSi_CentreX_addr))));
        const std::int32_t x0 = tfirst->x;
        SetLong(Mem(::bof3::Img(BattleTask_Slots_addr) + 0x38), cz);
        const long ground0 = MH_CALL(AreaMap_Elevation)(x0, z0);
        const unsigned char was = Mem(::bof3::Img(0x904AB0))[0];
        Mem(::bof3::Img(0x904AA9))[0] |= 0x80;
        SetLong(Mem(::bof3::Img(BattleTask_Slots_addr) + 0x3C), static_cast<std::int32_t>(Shl(static_cast<short>(ground0), 16)));
        PartyRecord(1)[0] = 0;
        PartyRecord(2)[0] = 0;
        Mem(::bof3::Img(at::kActor))[0] = 0;
        Mem(::bof3::Img(BattleE2_SavedCount_addr))[0] = was;
        Mem(::bof3::Img(0x904AB0))[0] = 1;
        Mem(::bof3::Img(Battle_PartyStanding_addr))[0] = 1;
    }
    for (unsigned i = 0; i < 3; ++i) {
        const unsigned char* const m = PartyRecord(i);
        const records::PartyMember* const tm = rv::AsConst<records::PartyMember>(m);
        unsigned char* const w = Mem(::bof3::Img(WindowRecords_addr) + 0x1d7 + i * 0x24u);
        if ((m[0] & 1) == 0) continue;
        MH_CALL(Window_Alloc)(i + 0xD, 3);
        const unsigned column = m[8], row = m[0x89];
        w[-1] = 5;
        w[0] = 0;
        const unsigned k = (column + row * 4u) * 2u;
        const unsigned side = ((U(move_script::Long(Mem(::bof3::Img(0x904AAC)))) & 0xFF) ^ 2u) >> 1;
        const unsigned dx = static_cast<unsigned>(SignedByte(Mem(::bof3::Img(kSideOffsets.v) + side * 2u)[0]) +
                                                  SignedByte(Mem(::bof3::Img(BattleWin_MemberTargetOffsets_addr) + k)[0])) +
                            static_cast<std::uint16_t>(tm->screen_x);
        SetWord(w + 1, dx & 0xFFFF);
        const unsigned dy =
            static_cast<unsigned>(SignedByte(Mem(::bof3::Img(BattleWin_MemberTargetOffsets_addr) + k + 1)[0])) + static_cast<std::uint16_t>(tm->screen_y) +
            0xCu;
        SetWord(w + 3, dy & 0xFFFF);
        w[7] = static_cast<unsigned char>(i);
        w[5] = 0;
        w[6] = 0;
    }
    const unsigned char was = Mem(::bof3::Img(0x904AB0))[0];
    unsigned char* const saved = Sc();
    Mem(::bof3::Img(Window1_ParamA_addr))[0] = was;
    const unsigned a = ActorIndex();
    SetWord(Mem(::bof3::Img(at::kFlags)), Word(Mem(::bof3::Img(at::kFlags))) & 0xBFFFu);
    SetWord(Mem(::bof3::Img(0x803188)), 0x70);
    unsigned char* const rec = PartyRecord(a);
    Sprite_Current = rec;
    MH_CALL(Battle_SetActorBit)(rec[5]);
    Sprite_Current[0] &= 0xBF;
    {
        const unsigned set = static_cast<unsigned>(move_script::Long(Mem(::bof3::Img(AreaW0b_PartySet_addr)))) & 0xFF;
        MH_CALL(Field_MemberSprite)(Mem(::bof3::Img(MoveScript_EffectArg_addr) + 0xc + set * 3u)[0], 0);
    }
    LoadPaletteOfCurrent();
    MH_CALL(Battle_StatusTint)(rv::As<records::PartyMember>(PartyRecord(ActorIndex()))->status);
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
    Mem(::bof3::Img(at::kFlags))[0] |= 4;
    unsigned char* const flags = PartyRecord(a) + 0x134;
    SetLong(flags, static_cast<std::int32_t>((U(move_script::Long(flags)) & ~4u) | 0x20u));
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

void Accession_Inject() {
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
}
