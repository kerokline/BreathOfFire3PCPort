// Moved here by tools/move_funcs.py from battle_e6.cpp, battle_sprites.cpp, field_event.cpp, frame_callees.cpp, gfx_flush.cpp, mode_flow.cpp, rest_4a.cpp, save_menu.cpp (docs/source-layout-round.md).
#include "game/field/field_2.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <io.h>
#include <utility>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/area_data_reload.h"
#include "game/battle_e6_callees.h"
#include "game/battle_sprites_callees.h"
#include "game/boss_harness.h"
#include "game/cheats.h"
#include "game/dat_cache.h"
#include "game/dat_load.h"
#include "game/field/field_2_callees.h"
#include "game/field_event.h"
#include "game/field_event_callees.h"
#include "game/frame_callees.h"
#include "game/frame_callees_callees.h"
#include "game/gfx_flush.h"
#include "game/mode_flow.h"
#include "game/mode_flow_callees.h"
#include "game/move_script_bytes.h"
#include "game/music_seq.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_4a_callees.h"
#include "game/save_menu.h"
#include "game/save_menu_callees.h"
#include "game/scenario_harness.h"
#include "game/sound_in.h"
#include "game/sound_sp.h"
#include "game/title_menu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "hook/run_speed.h"
#include "input/sigma.h"

namespace {

namespace at = field_2::at;

}  // namespace

using namespace battle_sprites;

namespace {

using namespace move_script;

}  // namespace

using namespace mode_flow;

using namespace save_menu;

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

U LBattleE6(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

void SetL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }

U Addr(const void* p) { return Low32(p); }

}  // namespace

namespace battle_sprites {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

namespace {

unsigned char* Ptr(const unsigned char* at) { return At(static_cast<std::uint32_t>(Long(at))); }

std::int32_t Address(const void* p) { return static_cast<std::int32_t>(Low32(p)); }

}  // namespace

}  // namespace battle_sprites

namespace {

std::uint32_t Address(const volatile void* p) { return Low32(p); }

constexpr unsigned kSlots = 8, kSlotBytes = 0x10;

constexpr unsigned kTints = 32, kTintBytes = 12;

// `cdq` / `idiv`: the original's signed division, faults included - a zero
// divisor raises the same #DE at the same point of the same computation,
// which a C++ `/` does not promise (as kind2_object.cpp's).
struct QuotRem { std::int32_t quot, rem; };

QuotRem Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quot, rem;
    __asm__ volatile("cltd\n\tidivl %%ecx" : "=a"(quot), "=&d"(rem) : "a"(dividend), "c"(divisor) : "cc");
    return {quot, rem};
}

// A colour component plus a tint byte, in 8 bits as the original adds them:
// a result that is negative as a signed byte is 0, one above 31 is 31 - so a
// large positive tint (0x61 and up on a component of 31) wraps to negative
// and comes out 0, not 31.
unsigned Tinted(unsigned component, unsigned char tint) {
    const auto sum = static_cast<signed char>(static_cast<unsigned char>(component + tint));
    if (sum < 0) return 0;
    if (sum > 0x1F) return 0x1F;
    return static_cast<unsigned>(sum);
}

}  // namespace

namespace mode_flow {

using move_script::At;

using move_script::SetLong;

}  // namespace mode_flow

namespace {

U UL(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U UL(U a) { return UL(move_script::At(a)); }

void SetUL(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }

void SetUL(U a, U v) { SetUL(move_script::At(a), v); }

unsigned char* Ptr(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

unsigned char* AtSaveMenu(save_menu::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

save_menu::U WordSaveMenu(save_menu::U address) {
    std::uint16_t v;
    std::memcpy(&v, AtSaveMenu(address), sizeof v);
    return v;
}

void PutWord(save_menu::U address, save_menu::U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(AtSaveMenu(address), &w, sizeof w);
}

}  // namespace

// field_event's callee seam, moved here from field_event.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace field_event {
namespace {

// A function pointer as another function type, through an integer.
template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }

}  // namespace

// Every callee. Those this module implements are ours (their names are bound
// to our functions by symbols.gen.h); the unnamed ones are Capcom's addresses,
// typed from their call sites and, where read, their first instructions
// (docs/field-event.md, section 3).
const Callees kOriginals = {
    Sprite_RestoreClut, Field_CopyInput, reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_LeaderStates_addr)),
    Sprite_ClearSteps, MapView_GroundAt, Field_LeaderAnimation,
    reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_LeaderControlSteps_addr)), Sprite_ScriptTick,
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Field_Tile89)), Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Field_Tile8A)),
    Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderIdleTest)),
    {Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderCellEvent)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderTalkTest)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderSwapTest)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderMenuTest)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderCheckTest)), Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderRequest4Test)),
     Fn<unsigned char (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderRequest9Test))},
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::Field_LeaderDirection)),
    Area_ZoneAt, Rand, Party_Count, Fn<unsigned char (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::Actor_EquipCount)),
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Sprite_EnsureAnimation)),
    Flags_Test, Sprite_SetAnimationBank, Field_MemberSprite, Field_PartyLoad, Sprite_ReleaseTint, Sprite_LoadPalette,
    Field_PartyPosition, Fn<void (__cdecl*)(long, long, unsigned)>(::bof3::Code(bof3::addr::Field_LeaderPlaceOffset)),
    Party_ClearActive, Field_LeaderFrame, Field_MemberFrame,
    reinterpret_cast<const std::uint32_t*>(::bof3::Img(Field_PendingJumps_addr)),
    Party_ClearAll, Sprite_SetTint, Field_MemberTimers,
    Party_JoinReset, Member_ClearState,
    PartySet_LoadFirst, PartySet_LoadSecond, File_LoadDone, Task_Sleep, PartySet_Find, PartySet_Select,
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::PartySet_ErrorLoop)), LoadDatFile,
    MapView_CheckHeightScale, AreaMap_Elevation,
    Tint_Release,
};
Callees g = kOriginals;

}  // namespace field_event

// frame_callees's callee seam, moved here from frame_callees.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace frame_callees {
namespace {

const frame_callees::CalleesFrameCallees kOriginals = {reinterpret_cast<void (__cdecl*)(unsigned char)>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Field_RunSlot))),   // R4A's (round 14): by address, the value unchanged
                            Sprite_UpdateScreenSlot, Sprite_UpdateScreen, Effect_Release};

}  // namespace

CalleesFrameCallees g = kOriginals;

}  // namespace frame_callees

// original 0x454960. The CLUT strip - VRAM rows 480..511, 256 cells wide - is
// kept by game logic in the DAT arena and marked dirty; here it goes up,
// row by changed row.
extern "C" void __cdecl Gfx_FlushDirtyStrip(void) {
    if (Gfx_ClutStripDirty == 0) return;
    const short rect[4] = {0, 0x1E0, 0x100, 0x20};
    Gfx_LoadImageIfChanged(rect, Gfx_ClutStrip);
    Gfx_ClutStripDirty = 0;
}

// original 0x4549B0 (PSX 0x8014E178, the same loops): the last 0x800 bytes of
// Gfx_ClutStripSource - its rows 28..31 - zeroed, then all 32 rows of 0x200
// bytes copied to Gfx_ClutStrip. So the copy carries the zeros: the strip's
// rows 28..31 come out zero too. The PSX zeroes the same four rows first.
// Gfx_ClutStripDirty is not touched.
extern "C" void __cdecl Gfx_ClutStripRestore(void) {
    std::memset(mode_flow::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3800), 0, 0x800);
    std::memcpy(Gfx_ClutStrip, Gfx_ClutStripSource, 0x4000);
}

#pragma clang attribute push(__attribute__((disable_tail_calls)), apply_to = function)

// 0x4549F0 (PSX 0x8014E294). Row (row & 0xFF) of the CLUT strip - 0x100
// words - copied from the source 0x4000 bytes below it, word by word, and
// Gfx_ClutStripDirty set; the row is not bounded (256 rows reach 0x20 KB past
// the strip). Its sibling 0x4549B0 (Gfx_ClutStripRestore) copies the whole
// strip and does not set the flag.
extern "C" void __cdecl Gfx_ClutStripCopyRow(unsigned row) {
    const U base = ((row & 0xFF) << 9) + ::bof3::Img(Gfx_ClutStrip_addr);
    for (U i = 0; i < 0x100; ++i) PutWord(base + i * 2, WordSaveMenu(base + i * 2 - 0x4000));
    Gfx_ClutStripDirty = 1;
}

#pragma clang attribute pop

// original 0x454A20 (PSX 0x8014E2E8): byte +0 of the 32 MoveScript_TintRecords
// zeroed, then 16 dwords from 0x7E06A0 set to -1.
extern "C" void __cdecl MoveScript_TintReset(void) {
    for (unsigned i = 0; i < 32; ++i) MoveScript_TintRecords[i * 12] = 0;
    for (unsigned i = 0; i < 16; ++i) mode_flow::SetLong(mode_flow::At(::bof3::Img(mode_flow::at::kTintMarks) + i * 4), -1);
}

// original 0x454A50 (PSX 0x8014E378): the Field_Slots record `slot` (its low
// byte) freed - byte +0 zeroed - if its bit 0 is set. As the original has it:
// the index is not checked (callers pass 0..7).
extern "C" void __cdecl Field_SlotRelease(unsigned slot) {
    unsigned char* const r = Field_Slots + (slot & 0xFF) * 16u;
    if (r[0] & 1) r[0] = 0;
}

// original 0x454A80 (PSX 0x8014E3B0): every Field_Slots record (8 of 0x10) whose +0xC is
// `object` released (Field_SlotRelease).
extern "C" void __cdecl Field_SlotsReleaseOwner(const unsigned char* object) {
    for (unsigned i = 0; i < 8; ++i)
        if (LBattleE6(move_script::At(::bof3::Img(Field_Slots_addr) + 0xC + 0x10 * i)) == Addr(object)) BH_CALL(Field_SlotRelease)(i);
}

// original 0x454AB0: Field_SlotRelease for each of the eight slots.
extern "C" void __cdecl Field_SlotsReleaseAll(void) {
    for (unsigned i = 0; i < 8; ++i) mode_flow::g.slot_release(i);
}

// original 0x454AD0: the tint records' frame (PSX FUN_8019725C, term for
// term). For each of the 32 MoveScript_TintRecords with bit 0 of byte +0:
//   - byte +5 is a CLUT depth, indexing Clut_Sixteens, Clut_PerRow and
//     Clut_Stride unbounded - depths 5..7 have 0 per row and divide by zero,
//     as the original does;
//   - CLUT number +1 (the source) and +6 (the target) each become a word
//     index into Gfx_ClutStrip: (n / per_row) << 8 plus the column
//     (n % per_row) * stride, kept to 8 bits;
//   - sixteens * 16 words are copied source to target in order, each
//     component plus a tint byte (+2 red, +3 green, +4 blue) in 8 bits and
//     clamped (Tinted), bit 15 forced by bit 1 of +0 or else kept; the
//     target's first word is 0 instead, whatever the source. Words are read
//     one at a time after the previous store, so overlapping source and
//     target rows copy forward, as the original's;
// then Gfx_ClutStripDirty is 1, and with bit 6 of +0 it is cleared and the
// sprite at dword +8 takes +6 as its CLUT number +0x27 - and with bit 7 its
// +0x24 bit 2. The original captures +0's bit 1, +3 and +4 before the
// copy and reads +2 in it; nothing the copy writes can reach the record, so
// that order is kept but unobservable. The tail re-reads +0 and the pointer
// after each store, in the original's order.
extern "C" void __cdecl MoveScript_TintFrame(void) {
    for (unsigned char* record = MoveScript_TintRecords; record < MoveScript_TintRecords + kTints * kTintBytes;
         record += kTintBytes) {
        if (!(record[0] & 1)) continue;
        const unsigned depth = record[5];
        const std::int32_t per_row = Clut_PerRow[depth];
        const unsigned stride = Clut_Stride[depth];
        const QuotRem from = Idiv(record[1], per_row);
        const QuotRem to = Idiv(record[6], per_row);
        const unsigned count = static_cast<unsigned>(Clut_Sixteens[depth]) << 4;
        if (count != 0) {
            const std::uint32_t source = (static_cast<std::uint32_t>(from.quot & 0xFF) << 8) +
                                         static_cast<unsigned char>(static_cast<unsigned>(from.rem) * stride);
            const std::uint32_t target = (static_cast<std::uint32_t>(to.quot & 0xFF) << 8) +
                                         static_cast<unsigned char>(static_cast<unsigned>(to.rem) * stride);
            const bool force = (record[0] & 2) != 0;
            const unsigned char green = record[3], blue = record[4];
            for (unsigned i = 0; i < count; ++i) {
                const std::uint16_t word = Gfx_ClutStrip[source + i];
                const unsigned top = force ? 1 : word >> 15;
                const unsigned r = Tinted(word & 0x1F, record[2]);
                const unsigned gr = Tinted((word >> 5) & 0x1F, green);
                const unsigned b = Tinted((word >> 10) & 0x1F, blue);
                Gfx_ClutStrip[target + i] = i == 0 ? 0 : static_cast<unsigned short>(top << 15 | b << 10 | gr << 5 | r);
            }
        }
        const unsigned char flags = record[0];
        Gfx_ClutStripDirty = 1;
        if (!(flags & 0x40)) continue;
        const unsigned char clut = record[6];
        record[0] = static_cast<unsigned char>(flags & 0xBF);
        move_script::At(static_cast<std::uint32_t>(move_script::Long(record + 8)))[0x27] = clut;
        if (!(record[0] & 0x80)) continue;
        move_script::At(static_cast<std::uint32_t>(move_script::Long(record + 8)))[0x24] |= 4;
    }
}

// original 0x454CC0 (GAME.EMI 0x8019751C; PSX 0x8019751C): takes the first free of the 32
// tint records (MoveScript_TintRecords, 12 bytes: +0 bit 0 in use), 0xFF when
// none is: +0 = (a << 1) + 0x41 as a byte, with bit 7 when the sprite's +0x28
// is 0 and its +0x24 bit 2 is clear; +6 = ClutMap_Mark(+0x28, index, 1) - the
// clut slot claimed; +1 = the sprite's +0x27 and +5 its +0x28 (read after the
// call), +8 the sprite, +2..+4 = r g b. Answers the index.
extern "C" unsigned char __cdecl Sprite_SetTint(unsigned char* sprite, unsigned char r, unsigned char g_, unsigned char b,
                                                unsigned char a) {
    unsigned i = 0;
    while (battle_sprites::At(::bof3::Img(MoveScript_TintRecords_addr) + i * 12)[0] & 1)
        if (++i == 32) return 0xFF;
    unsigned char* const rec = battle_sprites::At(::bof3::Img(MoveScript_TintRecords_addr) + i * 12);
    const unsigned char head = static_cast<unsigned char>((a << 1) + 0x41);
    rec[0] = head;
    if (sprite[0x28] == 0 && !(sprite[0x24] & 4)) rec[0] = static_cast<unsigned char>(head | 0x80);
    rec[6] = battle_sprites::g.clut_mark(sprite[0x28], i, 1);
    rec[1] = sprite[0x27];
    rec[5] = sprite[0x28];
    battle_sprites::SetLong(rec + 8, Address(sprite));
    rec[2] = r;
    rec[3] = g_;
    rec[4] = b;
    return static_cast<unsigned char>(i);
}

// original 0x454D60 (0x80197670; PSX 0x80197670): releases tint record `index` (unchecked)
// if in use: bit 7 clears bit 2 of the sprite's +0x24, +0 = 0, the sprite's
// +0x27 back from +1, and its clut cells freed (ClutMap_Mark(+5, index, 0)).
extern "C" void __cdecl Tint_Release(unsigned char index) {
    unsigned char* const rec = battle_sprites::At(::bof3::Img(MoveScript_TintRecords_addr) + index * 12u);
    const unsigned char head = rec[0];
    if (!(head & 1)) return;
    if (head & 0x80) Ptr(rec + 8)[0x24] &= 0xFB;
    rec[0] = 0;
    Ptr(rec + 8)[0x27] = rec[1];
    battle_sprites::g.clut_mark(rec[5], index, 0);
}

// original 0x454DC0 (PSX 0x80197718): every tint record of `sprite` released,
// the 32 records of MoveScript_TintRecords in order, compared as whole dwords.
extern "C" void __cdecl Sprite_ReleaseTint(unsigned char* sprite) {
    for (unsigned i = 0; i < 32; ++i)
        if (static_cast<std::uint32_t>(move_script::Long(move_script::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x8 + i * 12))) == Address(sprite))
            field_event::g.tint_release(static_cast<unsigned char>(i));
}

// original 0x454DF0 (0x80197784; PSX 0x80197784): the body Sprite_SetTint and Tint_Release
// share. Claim: ClutMap_FindFree(kind) and the owner written; release:
// ClutMap_FindOwner(owner) and 0xFF written - over the cells the callee left
// in SharedScratch bytes 0 / 1 (row, cell), read after the call: kind 0 one
// cell, 1 sixteen, 2 two, 3 four, 4 eight, anything else none. Answers what
// the callee answered. Neither the row nor a run past the map's 64 cells is
// checked; a release that found nothing still writes over the last cell the
// scratch names.
extern "C" unsigned char __cdecl ClutMap_Mark(unsigned kind, unsigned owner, unsigned claim) {
    unsigned char answer, fill;
    if (claim & 0xFF) {
        answer = battle_sprites::g.clut_find(kind);
        fill = static_cast<unsigned char>(owner);
    } else {
        answer = battle_sprites::g.clut_owner(owner);
        fill = 0xFF;
    }
    unsigned char* const cells = battle_sprites::At(::bof3::Img(battle_sprites::at::kClutMap.v) + battle_sprites::at::kCells.b_00 * 16u + battle_sprites::at::kCells.b_01);
    unsigned n;
    switch (kind & 0xFF) {
    case 0: n = 1; break;
    case 1: n = 16; break;
    case 2: n = 2; break;
    case 3: n = 4; break;
    case 4: n = 8; break;
    default: return answer;
    }
    for (unsigned i = n; i-- > 0;) cells[i] = fill;
    return answer;
}

// original 0x454F30 (0x801978BC; PSX 0x801978BC): the first free clut cell (0xFF in the
// 4 x 16 map) that starts a free run of the kind's size, aligned to it:
// every free cell tried is left in SharedScratch bytes 0 / 1 (row, cell),
// so a failed search leaves the last free cell there. Answers the clut slot
// - kind 0 0xC0 + row * 16 + cell, 1 (cell 0, the whole row) 0x1C + row,
// 2 0xE0 + row * 8 + cell / 2, 3 0x70 + row * 4 + cell / 4, 4 0x38 + row * 2
// + cell / 8 - or 0xFF.
extern "C" unsigned char __cdecl ClutMap_FindFree(unsigned kind) {
    const unsigned char* const map = battle_sprites::At(::bof3::Img(battle_sprites::at::kClutMap.v));
    auto free_run = [map](unsigned row, unsigned cell, unsigned size) {
        for (unsigned k = cell; k < cell + size; ++k)
            if (map[row * 16 + k] != 0xFF) return false;
        return true;
    };
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned cell = 0; cell < 16; ++cell) {
            if (map[row * 16 + cell] != 0xFF) continue;
            battle_sprites::at::kCells.b_01 = static_cast<unsigned char>(cell);
            battle_sprites::at::kCells.b_00 = static_cast<unsigned char>(row);
            switch (kind & 0xFF) {
            case 0: return static_cast<unsigned char>((row + 0xC) * 16 + cell);
            case 1:
                if (cell == 0 && free_run(row, 0, 16)) return static_cast<unsigned char>(row + 0x1C);
                break;
            case 2:
                if (!(cell & 1) && free_run(row, cell, 2)) return static_cast<unsigned char>((cell >> 1) + row * 8 + 0xE0);
                break;
            case 3:
                if (!(cell & 3) && free_run(row, cell, 4)) return static_cast<unsigned char>((cell >> 2) + row * 4 + 0x70);
                break;
            case 4:
                if (!(cell & 7) && free_run(row, cell, 8)) return static_cast<unsigned char>((cell >> 3) + row * 2 + 0x38);
                break;
            default: break;
            }
        }
    return 0xFF;
}

// original 0x455140 (0x80197B20; PSX 0x80197B20): the first cell of the clut map owned by
// `owner`, left in SharedScratch bytes 0 / 1 (row, cell); answers the owner,
// or 0xFF (and the scratch untouched) when it owns none.
extern "C" unsigned char __cdecl ClutMap_FindOwner(unsigned owner) {
    const unsigned char o = static_cast<unsigned char>(owner);
    for (unsigned row = 0; row < 4; ++row)
        for (unsigned cell = 0; cell < 16; ++cell)
            if (battle_sprites::At(::bof3::Img(battle_sprites::at::kClutMap.v))[row * 16 + cell] == o) {
                battle_sprites::at::kCells.b_00 = static_cast<unsigned char>(row);
                battle_sprites::at::kCells.b_01 = static_cast<unsigned char>(cell);
                return o;
            }
    return 0xFF;
}

// original 0x4551A0 (GAME.EMI 0x80197B9C, term for term; PSX 0x80197B9C): sets bit 15 - the
// PlayStation's semi-transparency bit - on entries 1.. of the current sprite's
// clut in Gfx_ClutStrip, and Gfx_ClutStripDirty. The sprite's +0x27 (the
// clut slot) divided by the table 0x6528CC at its +0x28 (the clut kind)
// gives the strip row (+0x10 with +0x24 bit 2), the remainder times
// 0x6528D4's byte the column; 0x6528C4's byte * 16 the entry count, read
// again every turn. A kind whose divisor is 0 (5 and up) divides by zero, as
// it does in the original.
extern "C" void __cdecl Sprite_SetClutStp(void) {
    const unsigned char* const s = Sprite_Current;
    const unsigned kind = s[0x28];
    const int slot = s[0x27];
    volatile int divisor = battle_sprites::At(::bof3::Img(battle_sprites::at::kClutDivisor.v))[kind];
    unsigned char row = static_cast<unsigned char>(slot / divisor);
    const unsigned char column = static_cast<unsigned char>((slot % divisor) * battle_sprites::At(::bof3::Img(battle_sprites::at::kClutStep.v))[kind]);
    if (s[0x24] & 4) row = static_cast<unsigned char>(row + 0x10);
    unsigned count = battle_sprites::At(::bof3::Img(battle_sprites::at::kClutCount.v))[kind] << 4;
    unsigned i = 1;
    if (count > i) {
        unsigned char* entry = reinterpret_cast<unsigned char*>(Gfx_ClutStrip) + 2 + ((row << 8) + column) * 2u;
        do {
            entry[1] |= 0x80;
            count = battle_sprites::At(::bof3::Img(battle_sprites::at::kClutCount.v))[s[0x28]] << 4;
            ++i;
            entry += 2;
        } while (i < count);
    }
    Gfx_ClutStripDirty = 1;
}

// original 0x455250: the slot records (PSX FUN_80197C8C, term for term). Each
// of the 8 whose byte +0 has bit 0 - read afresh each turn - runs
// Field_RunSlot(index). The original pushes the index as a whole stack dword
// of which it stored only the low byte (the upper three are the caller's
// ecx); Field_RunSlot masks it to a byte, so the clean byte here is the same.
extern "C" void __cdecl Field_RunSlots(void) {
    const unsigned char* slot = Field_Slots;
    for (unsigned index = 0; index < kSlots; ++index, slot += kSlotBytes)
        if (slot[0] & 1) frame_callees::g.run_slot(static_cast<unsigned char>(index));
}

// original 0x455290 (PSX 0x80197CF0): the first Field_Slots record without +0 bit 0 taken:
// +0 = 1, +0xC = `object`, +4 = `script`, +2 = 0xFF, +3 = the object's
// +0x27; its index the answer, 0xFF when none is free.
extern "C" unsigned char __cdecl Field_SlotStart(unsigned char* object, unsigned script) {
    for (unsigned i = 0; i < 8; ++i) {
        unsigned char* const slot = move_script::At(::bof3::Img(Field_Slots_addr) + 0x10 * i);
        if (slot[0] & 1) continue;
        slot[0] = 1;
        SetL(slot + 0xC, Addr(object));
        SetL(slot + 4, script);
        slot[2] = 0xFF;
        slot[3] = object[0x27];
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

// original 0x455300 (Field_RunSlots' callee; the PSX FUN_80197D84): record
// Field_Slots + 16 * index. When its object's (+0xC) pose byte +0x4B differs
// from +2, the script +8 is the dword table +4's entry by that pose, its
// frame count +1 the script's byte 3, and +2 the pose (read again). When +1
// is 0 the script steps 4 bytes, back by 4 * byte 1 at an 0xFF, and +1 is the
// new step's byte 3. Then Field_SlotClutCopy(record) and +1 less 1.
extern "C" void __cdecl Field_RunSlot(unsigned char index) {
    unsigned char* const r = Field_Slots + 16u * index;
    unsigned char* const obj = Ptr(UL(r + 0xC));
    const unsigned char pose = obj[0x4B];
    if (pose != r[2]) {
        const U p = UL(Ptr(UL(r + 4)) + 4u * pose);
        SetUL(r + 8, p);
        r[1] = Ptr(p)[3];
        r[2] = obj[0x4B];
    }
    if (r[1] == 0) {
        U p = UL(r + 8) + 4;
        SetUL(r + 8, p);
        if (Ptr(p)[0] == 0xFF) {
            p -= 4u * Ptr(p)[1];
            SetUL(r + 8, p);
        }
        r[1] = Ptr(UL(r + 8))[3];
    }
    SH_CALL(Field_SlotClutCopy)(r);
    r[1] = static_cast<unsigned char>(r[1] - 1);
}

// original 0x455380 (no PSX twin paired): the slot's frame +3 split by the
// object's (+0xC) kind +0x28 through the divisors 0x6528E4: the quotient is a
// CLUT row (+0x10 with the object's +0x24 bit 2), the remainder times
// 0x6528EC[kind] an offset added to the script's (+8) bytes 0 and 1; then
// script byte 2 colours (its count read again each turn) are copied along the
// row from column byte 1 + offset to byte 0 + offset, and Gfx_ClutStripDirty
// set. Columns and the row are bytes (they wrap); a divisor of 0 faults in the
// original (kinds 5..7 of the table): ours aborts.
extern "C" void __cdecl Field_SlotClutCopy(unsigned char* slot) {
    unsigned char* const obj = Ptr(UL(slot + 0xC));
    const int frame = slot[3];
    const unsigned char kind = obj[0x28];
    const int divisor = move_script::At(::bof3::Img(rest_4a::at::kSlotDivisors.v) + kind)[0];
    if (divisor == 0) bof3::Fatal("Field_SlotClutCopy: object kind %u has no frames - the original divides by 0", (unsigned)kind);
    auto row = static_cast<unsigned char>(frame / divisor);
    const auto offset = static_cast<unsigned char>((frame % divisor) * move_script::At(::bof3::Img(rest_4a::at::kSlotSteps.v) + kind)[0]);
    const unsigned char* const script = Ptr(UL(slot + 8));
    const auto to = static_cast<unsigned char>(script[0] + offset);
    const auto from = static_cast<unsigned char>(script[1] + offset);
    if (obj[0x24] & 4) row = static_cast<unsigned char>(row + 0x10);
    if (script[2] != 0) {
        unsigned char i = 0;
        do {
            const U base = static_cast<U>(row) << 8 | i;
            const unsigned short c = Gfx_ClutStrip[base + from];
            ++i;
            Gfx_ClutStrip[base + to] = c;
        } while (i < Ptr(UL(slot + 8))[2]);
    }
    Gfx_ClutStripDirty = 1;
}

void Field2_Inject() {
    BOF3_INJECT(Gfx_FlushDirtyStrip);
    BOF3_INJECT(Gfx_ClutStripRestore);
    BOF3_INJECT(Gfx_ClutStripCopyRow);
    BOF3_INJECT(MoveScript_TintReset);
    BOF3_INJECT(Field_SlotRelease);
    BOF3_INJECT(Field_SlotsReleaseOwner);
    BOF3_INJECT(Field_SlotsReleaseAll);
    BOF3_INJECT(MoveScript_TintFrame);
    BOF3_INJECT(Sprite_SetTint);
    BOF3_INJECT(Tint_Release);
    BOF3_INJECT(Sprite_ReleaseTint);
    BOF3_INJECT(ClutMap_Mark);
    BOF3_INJECT(ClutMap_FindFree);
    BOF3_INJECT(ClutMap_FindOwner);
    BOF3_INJECT(Sprite_SetClutStp);
    BOF3_INJECT(Field_RunSlots);
    BOF3_INJECT(Field_SlotStart);
    BOF3_INJECT(Field_RunSlot);
    BOF3_INJECT(Field_SlotClutCopy);
}
