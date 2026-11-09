// Moved here by tools/move_funcs.py from magic_s30.cpp (docs/source-layout-round.md).
#include "game/magic/kaiser.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s30.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
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

using move_script::SetLong;

using move_script::Word;

// kV: now Prim_VertexScratch (symbols.toml).
// The dword the sprite children point at a sprite bank while their phase
// runs (0x8E3580 / 0x813580), and put back to 0x8B3580 after.
// kSpriteBank: now BattleE2_AnimSet (symbols.toml).
// MAGIC131's mote pool: 48 records of 0x2C bytes, and the cell holding the
// mote being run (KaiserMote_Pool, KaiserMote_Current).
// kMotePool: now KaiserMote_Pool (symbols.toml).
constexpr std::uint32_t kMoteStride = 0x2C;

constexpr unsigned kMotes = 48;

// kMoteCurrent: now KaiserMote_Current (symbols.toml).
// The battle bytes MAGIC131 reads beyond the harness's: 0x904AAC (the
// battle's facing, battle region), 0x904B89 (read by the engine as the
// form index, docs/magic_lib.md), 0x90412C (the loaded party set,
// docs/field-event.md), and the table of file-index tables by 0x904B89.
constexpr ::bof3::image::Va kFacing{0x904AAC};

// kForm: now BattlePhases_B89 (symbols.toml).
// kPartySet: now AreaW0b_PartySet (symbols.toml).
constexpr ::bof3::image::Va kSetFiles{0x64E9BC};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

std::int32_t PtrValue(const void* p) {
    return static_cast<std::int32_t>(Low32(p));
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* PartyRecord(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* Mote(unsigned i) { return Mem(::bof3::Img(KaiserMote_Pool_addr) + i * kMoteStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

void DrawMode(unsigned char* prim, unsigned tpage) { MH_CALL(Gpu_SetDrawMode)(prim, 0, 1, tpage, 0); }

void Commit(unsigned slot, unsigned size) { MH_CALL(Gfx_CommitPrim)(slot, size); }

// This group's functions called directly, as the originals call them: by
// address, which in the game is the jmp Inject put there (or Capcom's code
// under BOF3X_ORIGINAL) and in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// Gfx_ClutStrip / Gfx_ClutStripSource word indices: row 26 (0x812980 /
// 0x80E980), row 1 (0x80F980 / 0x80B980) and row 1 + 0x21 (0x80F9C2).
constexpr unsigned kRow26 = 0x1A00, kRow1 = 0x200;

}  // namespace

#define S30_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4E4C50: the kind-2 task. An eight-entry stack table by +1 (Start,
// LoadFormFile, HideParty, WaitChildren, ReloadParty, LoadSetFile, ShowParty,
// MagicFx_EndWhenChildrenDone); a draw mode (tpage 0x55) committed; every
// live mote run (KaiserMote_Current the record, the owner its +0x28, the
// owner read before the walk put back after each); a draw mode (tpage 0x15)
// committed.
S30_EXPORT void __cdecl Kaiser_Task(void) {
    static constexpr std::uint32_t kPhases[8] = {
        bof3::addr::Kaiser_Start,       bof3::addr::Kaiser_LoadFormFile, bof3::addr::Kaiser_HideParty,
        bof3::addr::Kaiser_WaitChildren, bof3::addr::Kaiser_ReloadParty, bof3::addr::Kaiser_LoadSetFile,
        bof3::addr::Kaiser_ShowParty,   bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sc()[1];
    if (phase >= 8) PastTable("Kaiser_Task", phase, 8);
    magic_harness::Phase(kPhases[phase])();
    DrawMode(Gfx_PacketNext, 0x55);
    Commit(2, 0xC);
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kMotes; ++i) {
        unsigned char* const m = Mote(i);
        if ((m[0] & 1) == 0) continue;
        SetLong(Mem(::bof3::Img(KaiserMote_Current_addr)), PtrValue(m));
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), Long(m + 0x28));
        Call0(bof3::addr::KaiserMote_Task);
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
    }
    DrawMode(Gfx_PacketNext, 0x15);
    Commit(2, 0xC);
}

// original 0x4E4D20: every mote's +0..+2 cleared; the owner's direction
// byte; the task at the side's centre; +0xB 0, +9 0xA, +0xA 0, +1 on; one
// kind-1 child 0x69 of kind 1 (the flash); CLUT row 26 from its source with
// the stp bit; sound 0x102.
S30_EXPORT void __cdecl Kaiser_Start(void) {
    for (unsigned i = 0; i < kMotes; ++i) {
        Mote(i)[0] = 0;
        Mote(i)[1] = 0;
        Mote(i)[2] = 0;
    }
    Sc()[8] = Owner()[8];
    MH_CALL(MagicFx_CenterOnSide)();
    Sc()[0xB] = 0;
    Sc()[9] = 0xA;
    Sc()[0xA] = 0;
    Inc(Sc()[1]);
    const unsigned slot = NewTask(0x69);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = PtrValue(self);
    child[1] = 1;
    Inc(self[0xB]);
    for (unsigned k = 0; k < 0x100; ++k)
        Gfx_ClutStrip[kRow26 + k] = static_cast<unsigned short>(Gfx_ClutStripSource[kRow26 + k] | 0x8000);
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x102);
}

// original 0x4E4DF0: +9 down; at 0 LoadDatFile(0x228 when 0x904B89 is 7,
// 0x227 when 8, else 0x229) and +1 on.
S30_EXPORT void __cdecl Kaiser_LoadFormFile(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned form = Mem(::bof3::Img(BattlePhases_B89_addr))[0];
    MH_CALL(LoadDatFile)(form == 7 ? 0x228 : form == 8 ? 0x227 : 0x229);
    Inc(Sc()[1]);
}

// original 0x4E4E40: once File_LoadDone: the three party members' +0 cleared
// (their sprites off); a kind-1 child 0x69 of kind 0 (the sprite), +4 0x21,
// +9 0x18; CLUT row 1 copied from its source; +1 on.
S30_EXPORT void __cdecl Kaiser_HideParty(void) {
    if (MH_CALL(File_LoadDone)() == 0) return;
    PartyRecord(0)[0] = 0;
    PartyRecord(1)[0] = 0;
    PartyRecord(2)[0] = 0;
    const unsigned slot = NewTask(0x69);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = PtrValue(self);
    child[1] = 0;
    child[4] = 0x21;
    child[9] = 0x18;
    Inc(self[0xB]);
    for (unsigned k = 0; k < 0x100; ++k) Gfx_ClutStrip[kRow1 + k] = Gfx_ClutStripSource[kRow1 + k];
    Gfx_ClutStripDirty = 1;
    Inc(Sc()[1]);
}

// original 0x4E4EE0: once +0xB (the children) is 0: a kind-1 child 0x69 of
// kind 1 (the flash); +9 0xA; +1 on.
S30_EXPORT void __cdecl Kaiser_WaitChildren(void) {
    if (Sc()[0xB] != 0) return;
    const unsigned slot = NewTask(0x69);
    unsigned char* const self = Sc();
    unsigned char* const child = TaskSlot(slot);
    records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
    tchild->owner = PtrValue(self);
    child[1] = 1;
    Inc(self[0xB]);
    Sc()[9] = 0xA;
    Inc(Sc()[1]);
}

// original 0x4E4F40: +9 down; at 0: if a member below Field_MemberCount has
// +0x89 4 and +0x134 bit 0, one file by the party set 0x90412C (7, 0xD, 0xE,
// 0xF: 0x122..0x125, or 0x126..0x129 with 0x904AAC bit 1); else
// PartySet_Select(the set & 0x7F, 2 with 0x904AAC bit 1, else 1). +1 on.
S30_EXPORT void __cdecl Kaiser_ReloadParty(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    const unsigned count = Field_MemberCount;
    bool found = false;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const m = PartyRecord(i);
        if (m[0x89] == 4 && (m[0x134] & 1) != 0) found = true;
    }
    if (found) {
        const auto bit = [] { return (Mem(::bof3::Img(kFacing.v))[0] & 2) != 0; };
        if (Mem(::bof3::Img(AreaW0b_PartySet_addr))[0] == 7) MH_CALL(LoadDatFile)(bit() ? 0x126 : 0x122);
        if (Mem(::bof3::Img(AreaW0b_PartySet_addr))[0] == 0xD) MH_CALL(LoadDatFile)(bit() ? 0x127 : 0x123);
        if (Mem(::bof3::Img(AreaW0b_PartySet_addr))[0] == 0xE) MH_CALL(LoadDatFile)(bit() ? 0x128 : 0x124);
        if (Mem(::bof3::Img(AreaW0b_PartySet_addr))[0] == 0xF) MH_CALL(LoadDatFile)(bit() ? 0x129 : 0x125);
    } else {
        const unsigned mode = (Mem(::bof3::Img(kFacing.v))[0] & 2) != 0 ? 2 : 1;
        MH_CALL(PartySet_Select)(Mem(::bof3::Img(AreaW0b_PartySet_addr))[0] & 0x7Fu, mode);
    }
    Inc(Sc()[1]);
}

// original 0x4E5090: once File_LoadDone: LoadDatFile(the word at
// [0x64E9BC + 0x904B89 x 8 (+4 with 0x904AAC bit 1)] + the party set x 2),
// both indices unchecked; +1 on.
S30_EXPORT void __cdecl Kaiser_LoadSetFile(void) {
    if (MH_CALL(File_LoadDone)() == 0) return;
    const unsigned half = (Mem(::bof3::Img(kFacing.v))[0] & 2) != 0 ? 4 : 0;
    const unsigned set = Mem(::bof3::Img(AreaW0b_PartySet_addr))[0];
    const unsigned form = Mem(::bof3::Img(BattlePhases_B89_addr))[0];
    const std::uint32_t table = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(kSetFiles.v) + form * 8 + half)));
    // The original reads through the entry unchecked; a null one faults.
    if (table == 0)
        bof3::Fatal("Kaiser_LoadSetFile: 0x904B89 is %u, whose file table (0x%X) is null", form,
                    (unsigned)(kSetFiles.v + form * 8 + half));
    MH_CALL(LoadDatFile)(Word(Mem(table + set * 2)));
    Inc(Sc()[1]);
}

// original 0x4E5100: once File_LoadDone: for each member below
// Field_MemberCount (re-read each time round), Sprite_Current the member,
// +0 1 and animation +8 + 4, then + 0x18 with +0x90 & 0x2080, + 0x30 with
// +0x91 bit 3, + 0x1C when Battle_ActorIsOut; Sprite_Current put back; +1 on.
S30_EXPORT void __cdecl Kaiser_ShowParty(void) {
    if (MH_CALL(File_LoadDone)() == 0) return;
    if (Field_MemberCount == 0) {
        Inc(Sc()[1]);
        return;
    }
    unsigned char* const saved = Sc();
    unsigned i = 0;
    do {
        unsigned char* const m = PartyRecord(i);
        records::PartyMember* const tm = rv::As<records::PartyMember>(m);
        Sprite_Current = m;
        m[0] = 1;
        MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(m[8] + 4));
        if ((tm->status & 0x2080) != 0) MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[8] + 0x18));
        if ((m[0x91] & 8) != 0) MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[8] + 0x30));
        if (MH_CALL(Battle_ActorIsOut)(i) != 0) MH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(Sc()[8] + 0x1C));
        i = (i + 1) & 0xFF;
        Sprite_Current = saved;
    } while (i < Field_MemberCount);
    Inc(saved[1]);
}

void Kaiser_Inject() {
    BOF3_INJECT(Kaiser_Task);
    BOF3_INJECT(Kaiser_Start);
    BOF3_INJECT(Kaiser_LoadFormFile);
    BOF3_INJECT(Kaiser_HideParty);
    BOF3_INJECT(Kaiser_WaitChildren);
    BOF3_INJECT(Kaiser_ReloadParty);
    BOF3_INJECT(Kaiser_LoadSetFile);
    BOF3_INJECT(Kaiser_ShowParty);
}
