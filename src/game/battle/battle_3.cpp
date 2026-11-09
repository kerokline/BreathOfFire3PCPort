// Moved here by tools/move_funcs.py from battle_actor_copies.cpp, battle_damage.cpp, battle_e4.cpp, battle_flow.cpp, battle_misc.cpp, battle_setup.cpp, battle_window_draw.cpp, battle_windows.cpp, rest_3b.cpp (docs/source-layout-round.md).
#include "game/battle/battle_3.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle/battle_3_callees.h"
#include "game/battle_actor_copies_callees.h"
#include "game/battle_damage_callees.h"
#include "game/battle_e4_callees.h"
#include "game/battle_flow_callees.h"
#include "game/battle_misc.h"
#include "game/battle_misc_callees.h"
#include "game/battle_setup_callees.h"
#include "game/battle_text.h"
#include "game/battle_window_draw.h"
#include "game/battle_window_draw_callees.h"
#include "game/battle_windows.h"
#include "game/battle_windows_callees.h"
#include "game/boss_harness.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3b_callees.h"
#include "game/text_pairs.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_3::at;

}  // namespace

using namespace battle_actor_copies;

using namespace battle_damage;

using namespace battle_flow;

namespace {

using namespace battle_misc;

}  // namespace

using namespace battle_setup;

using namespace battle_window_draw;

using namespace battle_windows;

namespace {

// `mov al, [step]; jmp [eax*4 + table]`: entry `index` of a .data table,
// read afresh. As the original has it, the index is not checked - one past
// the end reads the next table's first entry (the tables sit back to back),
// so ours reads exactly what the original's jmp would.
using Handler = void (__cdecl*)();

void Dispatch(const unsigned long* table, unsigned index) { reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))(); }

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// An actor byte: 0..2 the party, 3 and up the enemies. Both address the
// working record by the byte alone, as the original does (`and eax, 0xFF`).
unsigned char* Party(unsigned actor) { return move_script::At(::bof3::Img(ObjTrio_addr) + 0x10 + (actor & 0xFF) * battle_damage::at::kPartyStride); }

unsigned char* Enemy(unsigned actor) { return move_script::At(::bof3::Img(Battle_Enemies_addr) + 0x80 + ((actor & 0xFF) - 3) * battle_damage::at::kEnemyStride); }

// Party(a) is +0x10 into the member record (0x802D50), Enemy(a) +0x80 into the
// enemy object (0x93B9E0): their typed views start at the records'
// starts, rv::As<records::PartyMember>(Party(a) - 0x10) and
// rv::As<records::BattleEnemy>(Enemy(a) + at::kEObject).
unsigned char Byte(std::uint32_t address) { return move_script::At(address)[0]; }

std::int32_t S32(std::uint32_t v) { return static_cast<std::int32_t>(v); }

std::uint32_t U32(std::int32_t v) { return static_cast<std::uint32_t>(v); }

short S16(unsigned v) { return static_cast<short>(static_cast<unsigned short>(v)); }

// 32-bit products wrap, as imul's low half does.
std::int32_t Mul(std::int32_t a, std::int32_t b) { return S32(U32(a) * U32(b)); }

// The two sorts: entries of (s16 value, u16 actor), for i, for j after i, the
// pair swapped whenever entry i's value is below entry j's (signed) - highest
// first, equal values in their first order only as far as the swaps leave
// them. `cmp ax, [ecx]; jge` in both (0x445048, 0x445332).
void SortDescending(unsigned char* entries, unsigned n) {
    if (n <= 1) return;
    for (unsigned i = 0; i + 1 < n; ++i)
        for (unsigned j = i + 1; j < n; ++j) {
            unsigned char* const a = entries + 4 * i;
            unsigned char* const b = entries + 4 * j;
            if (S16(move_script::Word(a)) < S16(move_script::Word(b))) {
                const std::int32_t t = move_script::Long(b);
                move_script::SetLong(b, move_script::Long(a));
                move_script::SetLong(a, t);
            }
        }
}

// The gate both order builders apply: with the byte 0x904B7A set, an actor
// takes part only with bit 15 of its second flag dword.
bool Gated(const unsigned char* flags2) { return BattlePhases_B7A != 0 && (move_script::Long(flags2) & 0x8000) == 0; }

// Battle_CalcDamage's roll for flag bit 1: eight percentages on its stack.
constexpr unsigned char kFlagRoll[8] = {50, 50, 50, 50, 60, 60, 60, 70};

using U = std::uint32_t;

unsigned char& B(U address) { return move_script::At(address)[0]; }

unsigned char* Ptr(U cell) { return move_script::At(static_cast<U>(move_script::Long(move_script::At(cell)))); }

// The party's records by a byte (0..2 in play), the enemies' objects by an
// actor byte (3..10: object actor - 3), as every original indexes them - the
// byte alone, the product wrapping as 32-bit arithmetic does.
unsigned char* PartyBattleE4(unsigned member) { return move_script::At(::bof3::Img(ObjTrio_addr) + (member & 0xFF) * battle_e4::at::kPartyStride); }

unsigned char* EnemyBattleE4(unsigned actor) { return move_script::At(::bof3::Img(Battle_Enemies_addr) + ((actor & 0xFF) - 3u) * battle_e4::at::kEnemyStride); }

unsigned char* CharRecord(unsigned index) { return move_script::At(::bof3::Img(CharacterRecords_addr) + (index & 0xFF) * battle_e4::at::kCharStride); }

std::int32_t S32BattleE4(U v) { return static_cast<std::int32_t>(v); }

unsigned Low(U v) { return v & 0xFF; }

// The screen tiles' shared head: Gpu_GetTPage(0, 2, 0x3C0, 0); Gpu_SetDrawMode
// at Gfx_PacketNext with the page (the fifth word is the 0 the original left
// on the stack from the first call's pushes); Gfx_CommitPrim(1, 0xC); then
// Gpu_SetTile at the pointer read again. Answers the tile.
unsigned char* TileHead() {
    const unsigned tpage = BH_CALL(Gpu_GetTPage)(0, 2, 0x3C0, 0) & 0xFFFF;
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, tpage, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    BH_CALL(Gpu_SetTile)(p);
    return p;
}

// The pair of dwords at +x / +x + 4 turned by the direction byte +8: 1 (x, z)
// to (-z, x), 2 to (-x, -z), 3 to (z, -x); other values leave them.
void TurnPair(unsigned char* r, unsigned x) {
    const U a = static_cast<U>(move_script::Long(r + x));
    const U b = static_cast<U>(move_script::Long(r + x + 4));
    switch (r[8]) {
    case 1:
        move_script::SetLong(r + x, S32BattleE4(0u - b));
        move_script::SetLong(r + x + 4, S32BattleE4(a));
        break;
    case 2:
        move_script::SetLong(r + x, S32BattleE4(0u - a));
        move_script::SetLong(r + x + 4, S32BattleE4(0u - b));
        break;
    case 3:
        move_script::SetLong(r + x, S32BattleE4(b));
        move_script::SetLong(r + x + 4, S32BattleE4(0u - a));
        break;
    default:
        break;
    }
}

}  // namespace

namespace battle_flow {

using move_script::At;

}  // namespace battle_flow

namespace {

unsigned char* EnemyObject(unsigned index) { return battle_flow::At(::bof3::Img(Battle_Enemies_addr) + index * battle_flow::at::kEnemySize); }

// A float's bits for a primitive's corner: the original's fild of a sign-
// extended 16-bit value, then fstp.
std::int32_t FloatOfShort(int v) { return std::bit_cast<std::int32_t>(static_cast<float>(static_cast<short>(v))); }

unsigned char* AtBattleMisc(battle_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

battle_misc::U WordBattleMisc(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

battle_misc::U LongBattleMisc(const unsigned char* p) {
    battle_misc::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutLong(unsigned char* p, battle_misc::U v) { std::memcpy(p, &v, sizeof v); }

battle_misc::U SpriteCurrent() { return LongBattleMisc(AtBattleMisc(battle_misc::at::Sprite_CurrentAt())); }

void SetSpriteCurrent(battle_misc::U v) { PutLong(AtBattleMisc(battle_misc::at::Sprite_CurrentAt()), v); }

// The context a slot byte names: 0x93A000 + slot * 0x84, no bound.
battle_misc::U Context(battle_misc::U slot) { return ::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * kContextStride; }

unsigned char* P(unsigned actor) { return move_script::At(battle_setup::at::Party(actor)); }

unsigned char* E(unsigned actor) { return move_script::At(battle_setup::at::Enemy(actor)); }

bool Yes(unsigned al) { return (al & 0xFFu) != 0; }

unsigned char* Next() { return Gfx_PacketNext; }

unsigned char* Window(unsigned slot) { return move_script::At(::bof3::Img(WindowRecords_addr) + slot * battle_window_draw::at::kWindowStride); }

char* PrintBuf() { return reinterpret_cast<char*>(move_script::At(::bof3::Img(battle_window_draw::at::kPrintBuf.v))); }

const char* Format(std::uint32_t at) { return reinterpret_cast<const char*>(move_script::At(at)); }

std::int32_t S16BattleWindowDraw(std::uint32_t v) { return static_cast<std::int16_t>(v); }

float F(std::int32_t v) { return static_cast<float>(v); }

void Put(unsigned char* at, float f) { std::memcpy(at, &f, sizeof f); }

// Entry 0 of CLUT row (s8 window colour * 2 + row) in the shadow at 0x80B788,
// its 5-bit channels widened to 8 into the primitive's r g b - the colour byte
// read again for each channel, as the originals do.
void ShadeColour(unsigned char* p, unsigned row) {
    const auto entry = [row]() {
        const std::int32_t colour = Config_WindowColour;
        return move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x208 + static_cast<std::uint32_t>((static_cast<std::int32_t>(row) + colour * 2) << 5));
    };
    p[4] = static_cast<unsigned char>((entry()[0] & 0x1F) << 3);
    p[5] = static_cast<unsigned char>(((move_script::Word(entry()) >> 5) & 0x1F) << 3);
    p[6] = static_cast<unsigned char>(((move_script::Word(entry()) >> 10) & 0x1F) << 3);
}

// The LINE_F2 the three line helpers share: (s16 x0, s16 y0) to (s16 x1,
// s16 y1) in (r, g, b), the colour bytes the arguments' low bytes.
void LineBody(unsigned char* p, int x0, int y0, int x1, int y1, int r, int gr, int b) {
    p[4] = static_cast<unsigned char>(r);
    p[5] = static_cast<unsigned char>(gr);
    p[6] = static_cast<unsigned char>(b);
    Put(p + 0x08, F(S16BattleWindowDraw(static_cast<std::uint32_t>(x0))));
    Put(p + 0x0C, F(S16BattleWindowDraw(static_cast<std::uint32_t>(y0))));
    Put(p + 0x14, F(S16BattleWindowDraw(static_cast<std::uint32_t>(x1))));
    Put(p + 0x18, F(S16BattleWindowDraw(static_cast<std::uint32_t>(y1))));
}

// The two semi-transparent ones: draw mode page (0x3C0, 0) under `abr`, then
// the line, semi-transparent.
void BlendLine(unsigned abr, int x0, int y0, int x1, int y1, int r, int gr, int b) {
    const unsigned page = battle_window_draw::g.get_tpage(0, abr, 0x3C0, 0);
    battle_window_draw::g.draw_mode(Next(), 0, 0, page & 0xFFFF, 0);
    battle_window_draw::g.commit(1, 0xC);
    unsigned char* const p = Next();
    battle_window_draw::g.set_line_f2(p);
    LineBody(p, x0, y0, x1, y1, r, gr, b);
    battle_window_draw::g.set_semi(p, 1);
    battle_window_draw::g.commit(1, 0x20);
}

}  // namespace

namespace battle_windows {

using move_script::At;

}  // namespace battle_windows

namespace {

unsigned char* PtrBattleWindows(std::uint32_t at) { return battle_windows::At(static_cast<std::uint32_t>(move_script::Long(battle_windows::At(at)))); }

unsigned char* CurrentBattleWindows() { return PtrBattleWindows(::bof3::Img(battle_windows::at::kCurrent.v)); }

// `fild dword` then `fstp dword`: an int rounded once to a float (exact for
// every value here, all below 2^24).
void PutFloat(unsigned char* at, std::int32_t v) {
    const auto f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// The window colour's line colour: the first word of CLUT shadow row s8
// 0x903A5A, as three bytes 0..0xF8. Read once, where the original reads it.
struct Rgb { unsigned r, g, b; };

Rgb WindowRgb() {
    const int row = Config_WindowColour;
    const unsigned w = move_script::Word(battle_windows::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x228 + static_cast<std::uint32_t>(row * 64)));
    return {(w & 0x1F) << 3, ((w >> 5) & 0x1F) << 3, ((w >> 10) & 0x1F) << 3};
}

// The draw mode every window opens with: page (0x3C0, 0) under `abr`.
void OpenDrawMode(unsigned abr) {
    const unsigned tpage = battle_windows::g.get_tpage(0, abr, 0x3C0, 0) & 0xFFFF;
    battle_windows::g.draw_mode(Gfx_PacketNext, 0, 0, tpage, 0);
    battle_windows::g.commit(1, 0xC);
}

// A 16-bit signed clamp to 0..0xFF of a line colour less 0x10, as the target
// banners compute their inner lines' colour (cmp on the low word, the whole
// register kept).
unsigned Darker(unsigned c) {
    const int v = static_cast<int>(c) - 0x10;
    const auto s = static_cast<std::int16_t>(v);
    if (s < 0) return 0;
    if (s > 0xFF) return 0xFF;
    return static_cast<unsigned>(v);
}

// A party member's battle object (ObjTrio, stride 0x14C) and its words.
unsigned char* MemberBattleWindows(unsigned i) { return battle_windows::At(::bof3::Img(ObjTrio_addr) + battle_windows::at::kObjectStride * i); }

// A status colour from the member's +0x90 flags: 2 when bit 14 (grey),
// else what the caller derives.
unsigned StatusColourName(unsigned flags) { return (flags & 0x4000) ? 2u : ((flags & 0xBFC) != 0 ? 1u : 0u); }

unsigned StatusColourHp(unsigned flags) { return (flags & 0x4000) ? 2u : ((flags >> 11) & 4u); }

// The command cross's shade for arm j: 0x40 for arms 2 and 3 while the
// acting member's +0x134 has bit 1, else 0x80.
unsigned ArmShade(unsigned j) {
    if (j == 2 || j == 3) {
        if (reinterpret_cast<unsigned char*>(BattleE4_MenuActor.Native())[0x134] & 2) return 0x40;
    }
    return 0x80;
}

// The CLUT row of command icon k: the original builds 7 bytes on its stack
// (8 9 8 9 8 8 8, as the PlayStation copies from 0x801D0C64) and indexes them
// with k's low byte without a bound (`mov dl, [esp + ebx + 0x40]`), so a k
// past 6 reads its own frame: the byte at entry ESP - 8 + k. Ours is entered
// at the same ESP (the detour is a jmp) and `frame` is our EBP = entry ESP -
// 4. What the original holds there at that moment: 7 a byte it never wrote
// (ours reads a byte of its saved EBP - the original's is undefined too); 8..11
// the return address; 12..15 the first argument; 16..19 the second
// argument's slot, into which it has stored the float x + w; 20..31 the next
// three arguments; 32..35 the last argument's slot, now y + h; 36 and up the
// caller's frame. docs/battle_windows.md, section 4.
constexpr unsigned char kCommandClut[7] = {8, 9, 8, 9, 8, 8, 8};

unsigned char CommandClutPastTable(unsigned k, const unsigned char* frame, std::int32_t xw, std::int32_t yh) {
    if (k >= 16 && k < 20) {
        const auto f = static_cast<float>(xw);
        unsigned char b[4];
        std::memcpy(b, &f, 4);
        return b[k - 16];
    }
    if (k >= 32 && k < 36) {
        unsigned char b[4];
        std::memcpy(b, &yh, 4);
        return b[k - 32];
    }
    return frame[static_cast<int>(k) - 4];
}

// The three plain boxes: a tile (size index `tile`) at (x, y + 2), the edge
// pieces `top` at (x, y) and `bottom` at (x, y + 0x11), then four lines in
// the window colour from x + 2 to x + `right`, y + 2 to y + 0x10.
void PlainBox(int x, int y, unsigned tile, unsigned top, unsigned bottom, int right) {
    OpenDrawMode(0);
    const Rgb c = WindowRgb();
    battle_windows::g.draw_tile(x, y + 2, tile, 1);
    battle_windows::g.draw_edge(x, y, top, 1);
    battle_windows::g.draw_edge(x, y + 0x11, bottom, 1);
    const int x0 = x + 2, x1 = x + right;
    battle_windows::g.line_semi1(x0, y + 2, x1, y + 2, c.r, c.g, c.b);
    battle_windows::g.line_semi1(x0, y + 3, x0, y + 0x10, c.r, c.g, c.b);
    battle_windows::g.line_semi0(x1, y + 2, x1, y + 0x10, c.r, c.g, c.b);
    battle_windows::g.line_semi0(x0, y + 0x10, x1, y + 0x10, c.r, c.g, c.b);
}

// The target banners' frame: the outer box under abr 0 at (x + 4, y + 6),
// then the inner under abr 1 at (x, y).
void TargetFrame(int x, int y) {
    battle_windows::g.draw_tile(x + 4, y + 8, 8, 1);
    battle_windows::g.draw_edge(x + 4, y + 6, 2, 1);
    battle_windows::g.draw_edge(x + 4, y + 0x19, 3, 1);
    OpenDrawMode(1);
    battle_windows::g.draw_tile(x, y + 2, 8, 0);
    battle_windows::g.draw_tile_rgb(x + 6, y + 0xB, 9, 0, 0);
    battle_windows::g.draw_edge(x, y, 2, 0);
    battle_windows::g.draw_edge(x, y + 0x13, 3, 0);
}

// An enemy's gauge, or 0x444340 of 0xFFFF (its ":" glyph, no number) when +0xF is not 1.
void EnemyGauge(int x, int y, std::uint32_t enemy) {
    if (battle_windows::At(enemy + 0xF)[0] == 1) {
        const unsigned char* const w = CurrentBattleWindows();
        battle_windows::g.draw_bar(x + 6, y + 0xA, w[0xB], w[0xD], 1);
    } else {
        battle_windows::g.draw_value(x + 0x24, y + 0xC, 0, 0xFFFF);
    }
}

using S32Rest3b = std::int32_t;

// The records as the originals index them: a party record by the byte
// itself (0x802D40 + 0x14C n, any n), an enemy object by the byte less 3
// (0x93B960 + 0x128 (n - 3), the difference in 32 bits - a byte below 3
// lands before the enemies). Neither is checked, as in the original.
unsigned char* PartyRest3b(unsigned n) { return move_script::At(::bof3::Img(ObjTrio_addr) + (n & 0xFF) * rest_3b::at::kPartyStride); }

unsigned char* CharRecordRest3b(unsigned roster) { return move_script::At(::bof3::Img(CharacterRecords_addr) + (roster & 0xFF) * rest_3b::at::kCharStride); }

// (value * percent) / 100: the product in 32 bits (wrapping), the division
// signed and truncated (imul 0x51EB851F, sar 5, + the sign bit); above `cap`
// the cap, below 0 zero.
int Percent(int value, int percent, int cap) {
    const S32Rest3b q = static_cast<S32Rest3b>(static_cast<U>(value) * static_cast<U>(percent)) / 100;
    if (q > cap) return cap;
    return q < 0 ? 0 : q;
}

}  // namespace

// battle_actor_copies's callee seam, moved here from battle_actor_copies.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace battle_actor_copies {
namespace {

template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
using F0 = U (__cdecl*)();
using F1 = U (__cdecl*)(U);
using F2 = U (__cdecl*)(U, U);
using F3 = U (__cdecl*)(U, U, U);

}  // namespace

const Callees kOriginals = {
    As<F2>(BattleBanner_SetMessage),
    As<F1>(Battle_DefaultTarget),
    Fn<F1>(::bof3::Code(kPrevTarget.v)),
    As<F3>(Battle_WrapIndex),
    As<F1>(Input_AutoRepeat),
    As<F1>(Sound_PlayEffect),
    As<F0>(ItemMenu_SetupForActor),
    As<F3>(Char_AbilityList),
    As<F1>(Msg_SystemPtr),
    As<F0>(ItemMenu_CanUseSelected),
    As<F0>(Battle_ReturnTrue),
    As<F0>(ItemMenu_FreeWindows),
};
Callees g = kOriginals;

}  // namespace battle_actor_copies

// battle_damage's callee seam, moved here from battle_damage.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace battle_damage {
namespace {

template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Battle_ActorCanAct,
    Battle_ActorCanCommand,
    Battle_LevelClass,
    Battle_CalcDamage,
    Battle_BaseDamage,
    Battle_ScaleDamage,
    Battle_ElementAffinity,
    EnemyAI_RowDone,
    Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(bof3::addr::Battle_ActorIsOut)),
    Fn<void (__cdecl*)(unsigned, unsigned, unsigned, unsigned, unsigned)>(::bof3::Code(bof3::addr::BattleBanner_Add)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_StatusResisted80)),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_InflictStatus)),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_ClearStatus)),
    Fn<int (__cdecl*)(int, unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_HitOrMissParty)),
    Fn<int (__cdecl*)(int, unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_HitOrMissEnemy)),
    Fn<int (__cdecl*)()>(::bof3::Code(bof3::addr::Battle_PartyDefenceMean)),
    Fn<int (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::Battle_PsiStatusDeathAffinity)),
    Rand,
    Fn<void (__cdecl*)(unsigned char*, const unsigned char*)>(::bof3::Code(bof3::addr::EnemyAI_ApplyAction)),
    Fn<void (__cdecl*)(unsigned char*, unsigned, unsigned)>(::bof3::Code(bof3::addr::EnemyAI_SetRowDone)),
    Fn<unsigned char (__cdecl*)(unsigned, unsigned)>(::bof3::Code(bof3::addr::EnemyAI_OtherRowsDone)),
    Fn<void (__cdecl*)()>(::bof3::Code(bof3::addr::EnemyAI_DedupMessages)),
};
Callees g = kOriginals;

}  // namespace battle_damage

// original 0x442FA0 (PSX 0x801D74DC): the party row - the frame of edge
// pieces and tiles sized by the member count 0x904AB0 at (x, y), then per
// member i (x' = x + 0x14 + 0x5E i): a tile, the gauge of bytes +0 / +2 of
// 0x80333F + 36 i, the charge marks when the object's +0xA2 is not zero (two
// lines, the second only when +3 is), the name +0x80 in the 8 px font, two
// digits, the two values +0x98 and +0x9A with their colours, and the four
// lines of the member's box in the window colour (read once, after the
// frame's tile). The count is read again after each member.
extern "C" void __cdecl BattleWin_DrawPartyStatus(int x, int y) {
    OpenDrawMode(0);
    const unsigned count = battle_windows::At(::bof3::Img(battle_windows::at::kPartyCount.v))[0];
    battle_windows::g.draw_edge(x, y, static_cast<unsigned char>((count + 5) << 1), 1);
    battle_windows::g.draw_edge(x, y + 0x18, static_cast<unsigned char>((battle_windows::At(::bof3::Img(battle_windows::at::kPartyCount.v))[0] << 1) + 0xB), 1);
    battle_windows::g.draw_tile(x, y + 2, static_cast<unsigned char>(battle_windows::At(::bof3::Img(battle_windows::at::kPartyCount.v))[0] + 0xD), 1);
    const Rgb c = WindowRgb();
    if (battle_windows::At(::bof3::Img(battle_windows::at::kPartyCount.v))[0] == 0) return;
    int slot = x;           // the original keeps x + 0x5E i in its first argument's slot
    int xs = x + 0x14;
    unsigned i = 0;
    do {
        battle_windows::g.draw_tile_rgb(xs, y + 0xE, 9, 0, 0);
        const unsigned char* const bars = battle_windows::At(::bof3::Img(WindowRecords_addr) + 0x1df + 36 * i);
        const unsigned char b1 = bars[2], b0 = bars[0];
        battle_windows::g.draw_bar(xs, y + 0xE, b0, b1, 0);
        unsigned char* const m = MemberBattleWindows(i);
        auto* const member_rec = rv::As<records::PartyMember>(m);
        if (member_rec->max_ap != 0) {
            battle_windows::g.line_semi1(xs, y + 0xE, bars[1] + slot + 0x14, y + 0xE, 0x64, 0, 0xC8);
            const unsigned char extra = bars[3];
            if (extra != 0) {
                const int start = bars[1] + slot + 0x14;
                battle_windows::g.line_semi1(start, y + 0xE, extra + start, y + 0xE, 0xC8, 0, 0);
            }
        }
        battle_windows::g.tiny_font(xs - 0xE, y + 5, StatusColourName(member_rec->status), 5, m + 0x80);
        battle_windows::g.draw_digit(xs + 0x26, y + 6, 1);
        battle_windows::g.draw_digit(xs - 0x10, y + 0xE, 0);
        const unsigned hp_colour = StatusColourHp(member_rec->status);
        battle_windows::g.draw_value(xs + 0x34, y + 0xF, hp_colour, member_rec->hp);
        const unsigned value = member_rec->ap;
        unsigned colour = 2;
        if (value != 0 && !(m[0x91] & 0x40)) colour = value < static_cast<unsigned>(member_rec->max_ap >> 2) ? 4u : 0u;
        battle_windows::g.draw_value(xs + 0x34, y + 8, colour, value);
        battle_windows::g.line_semi1(xs - 0x12, y + 2, xs + 0x49, y + 2, c.r, c.g, c.b);
        battle_windows::g.line_semi1(xs - 0x12, y + 3, xs - 0x12, y + 0x17, c.r, c.g, c.b);
        battle_windows::g.line_semi0(xs + 0x49, y + 2, xs + 0x49, y + 0x17, c.r, c.g, c.b);
        battle_windows::g.line_semi0(xs - 0x12, y + 0x17, xs + 0x49, y + 0x17, c.r, c.g, c.b);
        slot += 0x5E;
        xs += 0x5E;
        i = static_cast<unsigned char>(i + 1);
    } while (i < battle_windows::At(::bof3::Img(battle_windows::at::kPartyCount.v))[0]);
}

// original 0x4432F0 (PSX 0x801D7A10): the command cross at (x, y) - arms
// 0..4 but the selected one 0x904AB4 as icons (Menu_DrawIcon, grown by
// 0x904ABC + j about the arm's point 0x64E2AC + 2 j), then the selected arm
// on top: a plain 16 x 16 icon when its growth is 0, else
// BattleWin_DrawCommandIcon; then the two extra arms 5 and 6 when
// 0x904AC1 / 0x904AC2 are set. The selection is re-read after each icon;
// the shade stays what the last arm set when the selection is 5 or more.
extern "C" void __cdecl BattleWin_DrawCommandCross(int x, int y) {
    std::uint32_t sel = static_cast<std::uint32_t>(move_script::Long(battle_windows::At(::bof3::Img(battle_windows::at::kCrossSel.v))));
    unsigned shade = 0;
    for (unsigned j = 0; j < 5; ++j) {
        if (j == (sel & 0xFF)) continue;
        shade = ArmShade(j);
        const unsigned char grow = battle_windows::At(::bof3::Img(battle_windows::at::kCrossGrow.v) + j)[0];
        const unsigned half = grow >> 1;
        const unsigned char* const arm = battle_windows::At(::bof3::Img(battle_windows::at::kCrossArms.v) + 2 * j);
        const unsigned size = static_cast<unsigned char>(grow + 0x10);
        battle_windows::g.icon(j, static_cast<unsigned>(x + arm[0] - static_cast<int>(half)),
               static_cast<unsigned>(y + arm[1] - static_cast<int>(half)), size, size, shade);
        sel = static_cast<std::uint32_t>(move_script::Long(battle_windows::At(::bof3::Img(battle_windows::at::kCrossSel.v))));
    }
    const unsigned k = sel & 0xFF;
    if (k < 5) shade = ArmShade(k);
    const unsigned char grow = battle_windows::At(::bof3::Img(battle_windows::at::kCrossGrow.v) + k)[0];
    const unsigned char* const arm = battle_windows::At(::bof3::Img(battle_windows::at::kCrossArms.v) + 2 * k);
    if (grow == 0) {
        battle_windows::g.icon(sel, static_cast<unsigned>(x + arm[0]), static_cast<unsigned>(y + arm[1]), 0x10, 0x10, shade);
    } else {
        const unsigned half = grow >> 1;
        const unsigned size = static_cast<unsigned char>(grow + 0x10);
        battle_windows::g.command_icon(sel, static_cast<unsigned>(x + arm[0] - static_cast<int>(half)),
                       static_cast<unsigned>(y + arm[1] - static_cast<int>(half)), size, size, shade);
    }
    for (unsigned e = 5; e < 7; ++e) {
        const unsigned char grow2 = battle_windows::At(e == 5 ? ::bof3::Img(battle_windows::at::kCrossExtra5.v) : ::bof3::Img(battle_windows::at::kCrossExtra6.v))[0];
        if (grow2 == 0) continue;
        const unsigned half = grow2 >> 1;
        const unsigned char* const arm2 = battle_windows::At(::bof3::Img(battle_windows::at::kCrossArms.v) + 2 * e);
        const unsigned size = static_cast<unsigned char>(grow2 + 0x10);
        battle_windows::g.command_icon(e, static_cast<unsigned>(x + arm2[0] - static_cast<int>(half)),
                       static_cast<unsigned>(y + arm2[1] - static_cast<int>(half)), size, size, 0x80);
    }
}

// original 0x4434C0 (PSX 0x801D7CE0): a grown command icon k, a POLY_FT4 at
// (x, y) sized w x h (low words, low bytes) from page (0x100, 0x100), u = k
// << 5 .. + 0x18, v 0xE0 .. 0xF8, shade r = g = b, CLUT row 0x7800 | the
// table byte's low 6 bits (above).
extern "C" __attribute__((disable_tail_calls, noinline)) void __cdecl BattleWin_DrawCommandIcon(
    unsigned k, unsigned x, unsigned y, unsigned w, unsigned h, unsigned shade) {
    const auto* const frame = static_cast<const unsigned char*>(__builtin_frame_address(0));
    const unsigned tpage0 = battle_windows::g.get_tpage(0, 0, 0x100, 0x100) & 0xFFFF;
    battle_windows::g.draw_mode(Gfx_PacketNext, 0, 0, tpage0, 0);
    battle_windows::g.commit(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    battle_windows::g.set_poly_ft4(p);
    auto* const ft4 = rv::As<records::GpuPolyFT4>(p);  // a POLY_FT4: g.set_poly_ft4 just started it
    const auto s = static_cast<unsigned char>(shade);
    p[4] = s;
    p[5] = s;
    p[6] = s;
    const std::int32_t x0 = static_cast<std::int32_t>(x & 0xFFFF), y0 = static_cast<std::int32_t>(y & 0xFFFF);
    const std::int32_t x1 = x0 + static_cast<std::int32_t>(w & 0xFF), y1 = y0 + static_cast<std::int32_t>(h & 0xFF);
    PutFloat(p + 0x08, x0);
    PutFloat(p + 0x0C, y0);
    PutFloat(p + 0x18, x1);
    PutFloat(p + 0x1C, y0);
    PutFloat(p + 0x28, x0);
    PutFloat(p + 0x38, x1);
    PutFloat(p + 0x2C, y1);
    PutFloat(p + 0x3C, y1);
    const unsigned i = k & 0xFF;
    const auto u = static_cast<unsigned char>(i << 5);
    const auto u1 = static_cast<unsigned char>(u + 0x18);
    p[0x14] = u;
    p[0x34] = u;
    p[0x15] = 0xE0;
    p[0x24] = u1;
    p[0x25] = 0xE0;
    p[0x35] = 0xF8;
    p[0x44] = u1;
    p[0x45] = 0xF8;
    const unsigned tpage = battle_windows::g.get_tpage(0, 0, 0x100, 0x100);
    rv::Store<unsigned>(ft4->tpage, tpage);
    const unsigned char clut = i < sizeof kCommandClut ? kCommandClut[i] : CommandClutPastTable(i, frame, x1, y1);
    rv::Store<unsigned>(ft4->clut, 0x7800u | (clut & 0x3Fu));
    battle_windows::g.commit(1, 0x48);
}

// original 0x443610 (PSX 0x801D7E80): the battle message's box, 0x113 wide.
extern "C" void __cdecl BattleWin_DrawMessageBox(int x, int y) { PlainBox(x, y, 3, 8, 9, 0x115); }

// original 0x443740 (PSX 0x801D8060): a box 0x43 wide.
extern "C" void __cdecl BattleWin_DrawSmallBox(int x, int y) { PlainBox(x, y, 6, 4, 5, 0x45); }

// original 0x443870 (PSX 0x801D8240): a box 0x63 wide.
extern "C" void __cdecl BattleWin_DrawMediumBox(int x, int y) { PlainBox(x, y, 0xA, 6, 7, 0x65); }

// original 0x4439A0 (PSX 0x801D8420): the command label banner - command k's
// box at Battle_CommandBoxes 0x64E2C8 + 8 k (s16 x, y), 0x23 wide, and its
// label from Battle_CommandLabelPointers 0x669D60 + 4 k (0x669D28 + 8 k,
// "攻 击" for Attack) through Text_DrawAt at (x + 8, y + 3), 8 bytes. k's low
// byte, unbounded as the PlayStation's.
extern "C" void __cdecl BattleWin_DrawCommandLabel(unsigned k) {
    OpenDrawMode(0);
    const unsigned i = k & 0xFF;
    const int y = move_script::Word(battle_windows::At(::bof3::Img(Battle_CommandBoxes_addr) + 8 * i + 2));
    const int x = move_script::Word(battle_windows::At(::bof3::Img(Battle_CommandBoxes_addr) + 8 * i));
    const Rgb c = WindowRgb();
    battle_windows::g.draw_tile(x, y + 2, 5, 1);
    battle_windows::g.draw_edge(x, y, 0xA, 1);
    battle_windows::g.draw_edge(x, y + 0x11, 0xB, 1);
    battle_windows::g.line_semi1(x + 2, y + 2, x + 0x25, y + 2, c.r, c.g, c.b);
    battle_windows::g.line_semi1(x + 2, y + 3, x + 2, y + 0x10, c.r, c.g, c.b);
    battle_windows::g.line_semi0(x + 0x25, y + 2, x + 0x25, y + 0x10, c.r, c.g, c.b);
    battle_windows::g.line_semi0(x + 2, y + 0x10, x + 0x25, y + 0x10, c.r, c.g, c.b);
    const auto* const label = battle_windows::At(static_cast<std::uint32_t>(move_script::Long(battle_windows::At(::bof3::Img(Battle_CommandLabelPointers_addr) + 4 * i))));
    battle_windows::g.text_draw_at(x + 8, y + 3, 0, 8, label);
}

// original 0x443B10 (PSX 0x801D8660): the enemy target banner - slot t (an
// enemy is t - 3, record 0x93B9E0 + 0x128 e): the two-layer frame, its gauge
// (the current window's +0xB / +0xD) or the 0xFFFF value, its name in the 8 px font,
// the two outer lines in the window colour and the two inner ones 0x10
// darker (clamped at 0).
extern "C" void __cdecl BattleWin_DrawTargetEnemy(int x, int y, unsigned t) {
    OpenDrawMode(0);
    const unsigned w = move_script::Word(battle_windows::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x228 + static_cast<std::uint32_t>(
                                  Config_WindowColour * 64)));
    const unsigned r = (w & 0x1F) << 3, gr = ((w >> 5) & 0x1F) << 3, b = ((w >> 10) & 0x1F) << 3;
    const unsigned e = static_cast<unsigned char>(t - 3);
    TargetFrame(x, y);
    const std::uint32_t enemy = ::bof3::Img(Battle_Enemies_addr) + 0x80 + battle_windows::at::kEnemyStride * e;
    EnemyGauge(x, y, enemy);
    std::uint8_t expanded[33];   // DIV-0057: the 8-unit draw is Capcom's, so pairs go in as their glyphs
    battle_windows::g.tiny_font(x + 4, y + 3, 0, 8, TextPairs_Expand(battle_windows::At(enemy), expanded, sizeof expanded));
    battle_windows::g.line_semi1(x + 2, y + 2, x + 0x49, y + 2, r, gr, b);
    battle_windows::g.line_semi1(x + 2, y + 3, x + 2, y + 0x12, r, gr, b);
    const unsigned r2 = Darker(r), g2 = Darker(gr), b2 = Darker(b);
    battle_windows::g.line_plain(x + 0x49, y + 2, x + 0x49, y + 0x12, r2, g2, b2);
    battle_windows::g.line_plain(x + 2, y + 0x12, x + 0x49, y + 0x12, r2, g2, b2);
}

// original 0x443D90 (PSX 0x801D8AB4): the enemy status banner - slot t as
// above in one layer: its gauge or the 0xFFFF value, its name only when 0x444EB0 says
// so, and four lines in the window colour.
extern "C" void __cdecl BattleWin_DrawEnemyStatus(int x, int y, unsigned t) {
    OpenDrawMode(0);
    const int row = Config_WindowColour;
    const unsigned w = move_script::Word(battle_windows::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x228 + static_cast<std::uint32_t>(row * 64)));
    const unsigned r = (w & 0x1F) << 3, gr = ((w >> 5) & 0x1F) << 3, b = ((w >> 10) & 0x1F) << 3;
    const unsigned e = static_cast<unsigned char>(t - 3);
    battle_windows::g.draw_tile(x, y + 2, 8, 1);
    battle_windows::g.draw_tile_rgb(x + 6, y + 0xB, 9, 0, 0);
    battle_windows::g.draw_edge(x, y, 2, 1);
    battle_windows::g.draw_edge(x, y + 0x13, 3, 1);
    const std::uint32_t enemy = ::bof3::Img(Battle_Enemies_addr) + 0x80 + battle_windows::at::kEnemyStride * e;
    EnemyGauge(x, y, enemy);
    std::uint8_t expanded[33];   // DIV-0057, as in BattleWin_DrawTargetEnemy
    if (static_cast<unsigned char>(battle_windows::g.enemy_name_shown(e)) != 0)
        battle_windows::g.tiny_font(x + 4, y + 3, 0, 8, TextPairs_Expand(battle_windows::At(enemy), expanded, sizeof expanded));
    battle_windows::g.line_semi1(x + 2, y + 2, x + 0x49, y + 2, r, gr, b);
    battle_windows::g.line_semi1(x + 2, y + 3, x + 2, y + 0x12, r, gr, b);
    battle_windows::g.line_semi0(x + 0x49, y + 2, x + 0x49, y + 0x12, r, gr, b);
    battle_windows::g.line_semi0(x + 2, y + 0x12, x + 0x49, y + 0x12, r, gr, b);
}

// original 0x443F60 (PSX 0x801D8DC8): the member target banner - member m's
// (ObjTrio + 0x14C m) two-layer frame, name in the 8 px font, a digit, the
// value +0x98 (coloured 2 grey / 4 by +0x90 bit 13), "/" through sprintf and
// the 8 px numeral font and the value +0xA0, both coloured 2 by bit 14 else
// 0, the two outer lines and the two inner ones 0x10 darker.
extern "C" void __cdecl BattleWin_DrawTargetMember(int x, int y, unsigned m) {
    OpenDrawMode(0);
    const unsigned w = move_script::Word(battle_windows::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x228 + static_cast<std::uint32_t>(
                                  Config_WindowColour * 64)));
    const unsigned r = (w & 0x1F) << 3, gr = ((w >> 5) & 0x1F) << 3, b = ((w >> 10) & 0x1F) << 3;
    battle_windows::g.draw_tile(x + 4, y + 8, 8, 1);
    battle_windows::g.draw_edge(x + 4, y + 6, 2, 1);
    battle_windows::g.draw_edge(x + 4, y + 0x19, 3, 1);
    OpenDrawMode(1);
    battle_windows::g.draw_tile(x, y + 2, 8, 0);
    battle_windows::g.draw_edge(x, y, 2, 0);
    battle_windows::g.draw_edge(x, y + 0x13, 3, 0);
    unsigned char* const o = MemberBattleWindows(m & 0xFF);
    auto* const member_rec = rv::As<records::PartyMember>(o);
    battle_windows::g.tiny_font(x + 4, y + 2, StatusColourName(member_rec->status), 5, o + 0x80);
    const unsigned colour = StatusColourHp(member_rec->status);
    battle_windows::g.draw_digit(x + 2, y + 0xA, 0);
    battle_windows::g.draw_value(x + 0x14, y + 0xC, colour, member_rec->hp);
    const unsigned slash_colour = (member_rec->status >> 13) & 2;
    char* const buf = reinterpret_cast<char*>(battle_windows::At(::bof3::Img(battle_windows::at::kPrintBuf.v)));
    battle_windows::g.sprintf_(buf, reinterpret_cast<const char*>(battle_windows::At(::bof3::Img(battle_windows::at::kSlash.v))));
    battle_windows::g.font8(x + 0x28, y + 0xA, static_cast<int>(slash_colour), reinterpret_cast<const unsigned char*>(buf));
    // the slash's colour, not the first value's: the original keeps both in
    // its first argument's slot, the second over the first
    battle_windows::g.draw_value(x + 0x30, y + 0xC, slash_colour, member_rec->max_hp);
    battle_windows::g.line_semi1(x + 2, y + 2, x + 0x49, y + 2, r, gr, b);
    battle_windows::g.line_semi1(x + 2, y + 3, x + 2, y + 0x12, r, gr, b);
    const unsigned r2 = Darker(r), g2 = Darker(gr), b2 = Darker(b);
    battle_windows::g.line_plain(x + 0x49, y + 2, x + 0x49, y + 0x12, r2, g2, b2);
    battle_windows::g.line_plain(x + 2, y + 0x12, x + 0x49, y + 0x12, r2, g2, b2);
}

// original 0x444230 (PSX 0x801D92D4): window 1, the party's status panel: x
// from the byte table 0x64E307 by the party size (the low byte of 0x904AB0,
// also kept in +0xA), y 0xF0 (its handler slides it up to 0xC8), +8 and +9
// cleared.
extern "C" void __cdecl BattleWin_OpenStatus(unsigned state) {
    battle_window_draw::g.window_alloc(1, 3);
    unsigned char* const w = Window(1);
    auto* const win = rv::As<records::WindowRecord>(w);
    win->state = static_cast<unsigned char>(state);
    const unsigned size = move_script::At(::bof3::Img(battle_window_draw::at::kPartyCount.v))[0];
    w[0xA] = static_cast<unsigned char>(size);
    win->kind = 0;
    win->x = move_script::At(::bof3::Img(battle_window_draw::at::kStatusX.v) + size)[0];
    win->y = 0xF0;
    w[8] = 0;
    w[9] = 0;
}

// original 0x444290 (PSX 0x801D9358): window 2, sub-kind 1, at (0x88, 0x58).
extern "C" void __cdecl BattleWin_OpenSub1(unsigned state) {
    battle_window_draw::g.window_alloc(2, 3);
    auto* const win = rv::As<records::WindowRecord>(Window(2));
    win->kind = 1;
    win->state = static_cast<unsigned char>(state);
    win->x = 0x88;
    win->y = 0x58;
}

// original 0x4442C0 (PSX 0x801D93B4): window 3, sub-kind 2; its place is
// left as it was.
extern "C" void __cdecl BattleWin_OpenSub2(unsigned state) {
    battle_window_draw::g.window_alloc(3, 3);
    auto* const win = rv::As<records::WindowRecord>(Window(3));
    win->kind = 2;
    win->state = static_cast<unsigned char>(state);
}

// original 0x4442E0 (PSX 0x801D93F8): window 4, sub-kind 3, at (0x7C, 0x2A).
extern "C" void __cdecl BattleWin_OpenSub3(unsigned state) {
    battle_window_draw::g.window_alloc(4, 3);
    auto* const win = rv::As<records::WindowRecord>(Window(4));
    win->kind = 3;
    win->state = static_cast<unsigned char>(state);
    win->x = 0x7C;
    win->y = 0x2A;
}

// original 0x444310 (PSX 0x801D9454, the same): Window_Alloc(0, 3), then the
// four fields at 0x803162: bytes 6 and 0, words 0x14 and 0xFFEA (-22). The
// eax it leaves (Window_Alloc's) is read by none of its 18 callers.
extern "C" void __cdecl Battle_OpenMsgWindow() {
    battle_setup::g.window_alloc(0, 3);
    auto* const window_rec = &WindowRecords[0];
    window_rec->kind = 6;
    window_rec->state = 0;
    window_rec->x = 0x14;
    window_rec->y = 0xFFEA;
}

// original 0x444340 (PSX 0x801D94A4): a number in the battle's 6 x 5 digit
// strip - "%4d" of the low word of `value`, or ":" (the strip's cell 10) for
// 0xFFFF; draw mode page (0x3C0, 0); then per character, 5 to the right each,
// spaces skipped: the character less '0' written back into the print buffer,
// an SPRT of 6 x 5 at (s16 x, s16 y), u = c * 6 - 0x50 as a byte, v 0xD8, the
// CLUT at (colour << 4, 0x1E0), shade 0x80.
//
// As the original has it: the character is changed in the print buffer before
// the CLUT call and read back after it; the index is a byte and the end test
// is on the next character, so the first is drawn whatever it is; x steps as
// a dword and only its low 16 bits are placed.
extern "C" void __cdecl BattleWin_DrawNumber(int x, int y, int colour, int value) {
    if ((static_cast<std::uint32_t>(value) & 0xFFFF) == 0xFFFF)
        battle_window_draw::g.sprintf_(PrintBuf(), Format(::bof3::Img(battle_window_draw::at::kFmtNone.v)));
    else
        battle_window_draw::g.sprintf_(PrintBuf(), Format(::bof3::Img(battle_window_draw::at::kFmtNumber.v)), static_cast<std::uint32_t>(value) & 0xFFFF);
    const unsigned page = battle_window_draw::g.get_tpage(0, 0, 0x3C0, 0);
    battle_window_draw::g.draw_mode(Next(), 0, 0, page & 0xFFFF, 0);
    battle_window_draw::g.commit(1, 0xC);
    std::int32_t px = x;
    unsigned char i = 0;
    unsigned char* c = reinterpret_cast<unsigned char*>(PrintBuf());
    do {
        if (*c != 0x20) {
            unsigned char* const p = Next();  // a SPRT: g.set_sprt below
            *c = static_cast<unsigned char>(*c - 0x30);
            rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, battle_window_draw::g.get_clut(static_cast<int>((static_cast<unsigned>(colour) & 0xFF) << 4), 0x1E0));
            Put(p + 8, F(S16BattleWindowDraw(static_cast<std::uint32_t>(px))));
            Put(p + 0xC, F(S16BattleWindowDraw(static_cast<std::uint32_t>(y))));
            p[4] = p[5] = p[6] = 0x80;
            p[0x15] = 0xD8;
            p[0x14] = static_cast<unsigned char>(*c * 6 - 0x50);
            rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, 6);
            rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, 5);
            battle_window_draw::g.set_sprt(p);
            battle_window_draw::g.commit(1, 0x1C);
        }
        ++i;
        px += 5;
        c = reinterpret_cast<unsigned char*>(PrintBuf()) + i;
    } while (*c != 0);
}

// original 0x444480 (PSX 0x801D9654): a number at (x, y). The value's u16
// through sprintf("%3d") into 0x904BA0; a draw mode under tpage (0, 0, 0x3C0,
// 0) committed to slot 1; then from the first character to the next NUL,
// each but a space an 8 x 8 SPRT at the pen: the character less '0' stored
// back into the text, CLUT (clut << 4, 0x1E0), shade 0x80, u = (digit +
// 0x16) * 8 as a byte, v 0xD0; committed to slot 1. The pen moves 8 for
// every character, spaces too.
//
// As the original has it: the first character is drawn before any NUL test
// (the loop is a do-while); the packet cursor is read before Gpu_GetClut and
// the digit re-read from the text after it; x and y are the arguments' low
// 16 bits, the pen as 32 bits; the tpage call's texture window 0 is a push
// left under it.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_DrawNumber(int x, int y, unsigned clut, unsigned value) {
    battle_flow::g.crt_sprintf(reinterpret_cast<char*>(battle_flow::At(::bof3::Img(battle_flow::at::kNumberText.v))), reinterpret_cast<const char*>(battle_flow::At(::bof3::Img(battle_flow::at::kNumberFormat.v))),
              value & 0xFFFF);
    const unsigned tpage = battle_flow::g.get_tpage(0, 0, 0x3C0, 0) & 0xFFFF;
    battle_flow::g.draw_mode(Gfx_PacketNext, 0, 0, tpage, 0);
    battle_flow::g.commit(1, 0xC);
    unsigned i = 0;
    int pen = x;
    unsigned char* c = battle_flow::At(::bof3::Img(battle_flow::at::kNumberText.v));
    do {
        if (c[0] != ' ') {
            unsigned char* const p = Gfx_PacketNext;
            auto* const sprt = rv::As<records::GpuSprt>(p);  // a SPRT: g.set_sprt (Gpu_SetSprt) below starts it
            c[0] = static_cast<unsigned char>(c[0] - 0x30);
            const unsigned clut_word = battle_flow::g.get_clut(static_cast<int>((clut & 0xFF) << 4), 0x1E0);
            rv::Store<unsigned>(sprt->clut, clut_word);
            p[4] = 0x80;
            p[5] = 0x80;
            p[6] = 0x80;
            rv::Store<float>(sprt->x, std::bit_cast<float>(FloatOfShort(pen)));
            rv::Store<float>(sprt->y, std::bit_cast<float>(FloatOfShort(y)));
            p[0x15] = 0xD0;
            rv::Store<unsigned>(sprt->w, 8);
            p[0x14] = static_cast<unsigned char>((c[0] + 0x16) << 3);
            rv::Store<unsigned>(sprt->h, 8);
            battle_flow::g.set_sprt(p);
            battle_flow::g.commit(1, 0x1C);
        }
        pen += 8;
        i = (i + 1) & 0xFF;
        c = battle_flow::At(::bof3::Img(battle_flow::at::kNumberText.v) + i);
    } while (c[0] != 0);
}

// original 0x4445A0 (PSX 0x801D97D4): a 24 x 8 label at (x, y) - cell `cell`
// of the strip at u 0x68, v 0xD0 (u = cell * 24 + 0x68 as a byte), CLUT
// (clut << 4, 0x1E0), shade 0x80 - under the same draw mode as
// Battle_DrawNumber.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Battle_DrawLabel(int x, int y, unsigned clut, unsigned cell) {
    const unsigned tpage = battle_flow::g.get_tpage(0, 0, 0x3C0, 0) & 0xFFFF;
    battle_flow::g.draw_mode(Gfx_PacketNext, 0, 0, tpage, 0);
    battle_flow::g.commit(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    auto* const sprt = rv::As<records::GpuSprt>(p);  // a SPRT: g.set_sprt (Gpu_SetSprt) below starts it
    const unsigned clut_word = battle_flow::g.get_clut(static_cast<int>((clut & 0xFF) << 4), 0x1E0);
    rv::Store<unsigned>(sprt->clut, clut_word);
    p[4] = 0x80;
    p[5] = 0x80;
    p[6] = 0x80;
    rv::Store<float>(sprt->x, std::bit_cast<float>(FloatOfShort(x)));
    rv::Store<float>(sprt->y, std::bit_cast<float>(FloatOfShort(y)));
    p[0x15] = 0xD0;
    p[0x14] = static_cast<unsigned char>(cell * 0x18 + 0x68);
    rv::Store<unsigned>(sprt->w, 0x18);
    rv::Store<unsigned>(sprt->h, 8);
    battle_flow::g.set_sprt(p);
    battle_flow::g.commit(1, 0x1C);
}

// original 0x444660 (PSX 0x801D98D0, gap44): the whole screen dimmed - a TILE
// at (0, 0) of 960.0 x 240.0 in grey 0x28, semi-transparent (mode 1), after
// the draw mode for the page (0x3C0, 0). BE1's 0x42EE00 calls it. DIV-0041:
// its x from Widescreen_FillX(), as the PSP's -32 (the 960 already reaches
// past the right edge); 0 while the fills are unarmed, as the fuzz compares.
extern "C" void __cdecl BattleWin_DimScreen(void) {
    unsigned char* const p = TileHead();
    auto* const tile = rv::As<records::GpuTile>(p);  // TileHead started it with Gpu_SetTile
    rv::Store<float>(tile->x, Widescreen_FillX());
    rv::Store<float>(tile->y, std::bit_cast<float>(0));
    move_script::SetLong(p + 0x14, 0x44700000);   // 960.0f
    move_script::SetLong(p + 0x18, 0x43700000);   // 240.0f
    p[4] = 0x28;
    p[5] = 0x28;
    p[6] = 0x28;
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x4446E0 (PSX 0x801D997C, gap44): BattleWin_DrawTile's tile (s16 x,
// s16 y; w, h the u16 pair of 0x64E268 by the size byte) in the colour of the
// three bytes 0x903850..0x903852, semi-transparent (mode 1). DIV-0041: size
// 11 as BattleWin_DrawTile (Widescreen_BattleTile).
extern "C" void __cdecl BattleWin_DrawTileTint(int x, int y, int size) {
    unsigned char* const p = TileHead();
    const unsigned char* const t = move_script::At(::bof3::Img(battle_e4::at::kTileSizes.v) + (Low(static_cast<U>(size)) << 2));
    int sx = S16(static_cast<U>(x)), w = static_cast<int>(move_script::Word(t));
    Widescreen_BattleTile(Low(static_cast<U>(size)), sx, w);
    auto* const tile = rv::As<records::GpuTile>(p);  // TileHead started it with Gpu_SetTile
    rv::Store<std::int32_t>(tile->x, sx);
    rv::Store<std::int32_t>(tile->y, S16(static_cast<U>(y)));
    rv::Store<std::int32_t>(tile->w, w);
    rv::Store<std::int32_t>(tile->h, move_script::Word(t + 2));
    p[4] = battle_e4::at::kTint.b_00;
    p[5] = battle_e4::at::kTint.b_01;
    p[6] = battle_e4::at::kTint.b_02;
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
}

// original 0x4447B0 (PSX 0x801D9A84): a flat POLY_F4 of shape `shape` - four
// corners (u16 dx, u16 dy) of the 16-byte records at 0x64E148 added to
// (s16 x, s16 y) - in entry 0 of CLUT row (window colour * 2 + shade), made
// semi-transparent by the shade's bit 0.
extern "C" void __cdecl BattleWin_DrawQuadF4(int x, int y, int shape, int shade) {
    unsigned char* const p = Next();
    battle_window_draw::g.set_poly_f4(p);
    const unsigned char* const t = move_script::At(::bof3::Img(battle_window_draw::at::kQuadShapes.v) + ((static_cast<unsigned>(shape) & 0xFF) << 4));
    const std::int32_t sx = S16BattleWindowDraw(static_cast<std::uint32_t>(x)), sy = S16BattleWindowDraw(static_cast<std::uint32_t>(y));
    Put(p + 0x08, F(move_script::Word(t + 0x0) + sx));
    Put(p + 0x0C, F(move_script::Word(t + 0x2) + sy));
    Put(p + 0x14, F(move_script::Word(t + 0x4) + sx));
    Put(p + 0x18, F(move_script::Word(t + 0x6) + sy));
    Put(p + 0x20, F(move_script::Word(t + 0x8) + sx));
    Put(p + 0x24, F(move_script::Word(t + 0xA) + sy));
    Put(p + 0x2C, F(move_script::Word(t + 0xC) + sx));
    Put(p + 0x30, F(move_script::Word(t + 0xE) + sy));
    const unsigned row = static_cast<unsigned>(shade) & 0xFF;
    ShadeColour(p, row);
    battle_window_draw::g.set_semi(p, row);
    battle_window_draw::g.commit(1, 0x38);
}

// original 0x444900 (PSX 0x801D9C50): a TILE at (s16 x, s16 y) of size
// `size` - u16 w, u16 h of the 4-byte records at 0x64E268 - coloured and
// made semi-transparent as BattleWin_DrawQuadF4. DIV-0041: size 11, the
// full-frame tile, over the wide frame (Widescreen_BattleTile).
extern "C" void __cdecl BattleWin_DrawTile(int x, int y, int size, int shade) {
    unsigned char* const p = Next();
    battle_window_draw::g.set_tile(p);
    const unsigned char* const t = move_script::At(::bof3::Img(battle_window_draw::at::kTileSizes.v) + ((static_cast<unsigned>(size) & 0xFF) << 2));
    int sx = S16BattleWindowDraw(static_cast<std::uint32_t>(x)), w = static_cast<int>(move_script::Word(t));
    Widescreen_BattleTile(static_cast<unsigned>(size) & 0xFF, sx, w);
    Put(p + 0x08, F(sx));
    Put(p + 0x0C, F(S16BattleWindowDraw(static_cast<std::uint32_t>(y))));
    Put(p + 0x14, F(w));
    Put(p + 0x18, F(move_script::Word(t + 2)));
    const unsigned row = static_cast<unsigned>(shade) & 0xFF;
    ShadeColour(p, row);
    battle_window_draw::g.set_semi(p, row);
    battle_window_draw::g.commit(1, 0x1C);
}

// original 0x4449E0 (PSX 0x801D9D8C): the same tile in a given 15-bit colour -
// as the PlayStation has it, r from bits 10..14 and b from bits 0..4 - made
// semi-transparent by `abe`'s bit 0. DIV-0041: size 11 as BattleWin_DrawTile.
extern "C" void __cdecl BattleWin_DrawTileRgb(int x, int y, int size, int colour, int abe) {
    unsigned char* const p = Next();
    battle_window_draw::g.set_tile(p);
    const unsigned char* const t = move_script::At(::bof3::Img(battle_window_draw::at::kTileSizes.v) + ((static_cast<unsigned>(size) & 0xFF) << 2));
    int sx = S16BattleWindowDraw(static_cast<std::uint32_t>(x)), w = static_cast<int>(move_script::Word(t));
    Widescreen_BattleTile(static_cast<unsigned>(size) & 0xFF, sx, w);
    Put(p + 0x08, F(sx));
    Put(p + 0x0C, F(S16BattleWindowDraw(static_cast<std::uint32_t>(y))));
    Put(p + 0x14, F(w));
    Put(p + 0x18, F(move_script::Word(t + 2)));
    const auto c = static_cast<std::uint32_t>(colour);
    p[4] = static_cast<unsigned char>(((c >> 10) & 0x1F) << 3);
    p[5] = static_cast<unsigned char>(((c >> 5) & 0x1F) << 3);
    p[6] = static_cast<unsigned char>((c & 0x1F) << 3);
    battle_window_draw::g.set_semi(p, static_cast<unsigned>(abe) & 0xFF);
    battle_window_draw::g.commit(1, 0x1C);
}

// original 0x444A90 (PSX 0x801D9E6C): a bar 7 high of two POLY_FT4s from the
// page (0x3C0, 0): the body from s16 x to x + w (a byte) stretched over u
// 0x90..0x97 with the CLUT at (0xE0, 0x1E0) - or u 0x80..0x87 and CLUT
// (0xB0, 0x1E0) when `lit`'s low byte is set - then its end cap from x + w to
// x + w + cap (a byte) over u 0x98..0x9F, CLUT (0xB0, 0x1E0); v 0xD8..0xDF,
// shade 0x80.
//
// As the original has it: the cap's corners are the body's floats, kept in
// the argument slots and copied as bits - the same values ours converts again.
extern "C" void __cdecl BattleWin_DrawBar(int x, int y, int w, int cap, int lit) {
    unsigned char* p = Next();
    battle_window_draw::g.set_poly_ft4(p);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, battle_window_draw::g.get_tpage(0, 0, 0x3C0, 0));
    const bool on = (static_cast<unsigned>(lit) & 0xFF) != 0;
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, battle_window_draw::g.get_clut(on ? 0xB0 : 0xE0, 0x1E0));
    const std::int32_t left = S16BattleWindowDraw(static_cast<std::uint32_t>(x)), top = S16BattleWindowDraw(static_cast<std::uint32_t>(y));
    const std::int32_t mid = left + static_cast<std::int32_t>(static_cast<unsigned>(w) & 0xFF);
    const std::int32_t bottom = top + 7;
    Put(p + 0x08, F(left));
    Put(p + 0x0C, F(top));
    Put(p + 0x18, F(mid));
    Put(p + 0x1C, F(top));
    Put(p + 0x28, F(left));
    Put(p + 0x2C, F(bottom));
    Put(p + 0x38, F(mid));
    Put(p + 0x3C, F(bottom));
    p[4] = p[5] = p[6] = 0x80;
    const unsigned char u0 = on ? 0x80 : 0x90, u1 = on ? 0x87 : 0x97;
    p[0x14] = u0;
    p[0x15] = 0xD8;
    p[0x24] = u1;
    p[0x25] = 0xD8;
    p[0x34] = u0;
    p[0x35] = 0xDF;
    p[0x44] = u1;
    p[0x45] = 0xDF;
    battle_window_draw::g.commit(1, 0x48);

    p = Next();
    battle_window_draw::g.set_poly_ft4(p);
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->tpage, battle_window_draw::g.get_tpage(0, 0, 0x3C0, 0));
    rv::Store<unsigned>(rv::As<records::GpuPolyFT4>(p)->clut, battle_window_draw::g.get_clut(0xB0, 0x1E0));
    const std::int32_t right = mid + static_cast<std::int32_t>(static_cast<unsigned>(cap) & 0xFF);
    Put(p + 0x08, F(mid));
    Put(p + 0x0C, F(top));
    Put(p + 0x18, F(right));
    Put(p + 0x1C, F(top));
    Put(p + 0x28, F(mid));
    Put(p + 0x2C, F(bottom));
    Put(p + 0x38, F(right));
    Put(p + 0x3C, F(bottom));
    p[4] = p[5] = p[6] = 0x80;
    p[0x14] = 0x98;
    p[0x15] = 0xD8;
    p[0x24] = 0x9F;
    p[0x25] = 0xD8;
    p[0x34] = 0x98;
    p[0x35] = 0xDF;
    p[0x44] = 0x9F;
    p[0x45] = 0xDF;
    battle_window_draw::g.commit(1, 0x48);
}

extern "C" void __cdecl BattleWin_DrawCell16(int x, int y, int cell) {
    const unsigned page = battle_window_draw::g.get_tpage(0, 1, 0x3C0, 0);
    battle_window_draw::g.draw_mode(Next(), 0, 0, page & 0xFFFF, 0);
    battle_window_draw::g.commit(1, 0xC);
    unsigned char* const p = Next();  // a SPRT: g.set_sprt below
    p[4] = p[5] = p[6] = 0x80;
    p[0x14] = static_cast<unsigned char>(static_cast<unsigned>(cell) << 4);
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->w, 0x10);
    const std::int32_t xs = battle_window_draw::g_draw_signed ? S16BattleWindowDraw(static_cast<std::uint32_t>(x)) : static_cast<std::int32_t>(static_cast<std::uint32_t>(x) & 0xFFFF);
    const std::int32_t ys = battle_window_draw::g_draw_signed ? S16BattleWindowDraw(static_cast<std::uint32_t>(y)) : static_cast<std::int32_t>(static_cast<std::uint32_t>(y) & 0xFFFF);
    Put(p + 0x08, F(xs));
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->h, 8);
    Put(p + 0x0C, F(ys));
    rv::Store<unsigned>(rv::As<records::GpuSprt>(p)->clut, 0x7800);
    p[0x15] = 0xD8;
    battle_window_draw::g.set_sprt(p);
    battle_window_draw::g.commit(1, 0x1C);
}

// original 0x444CE0 (PSX 0x801DA3D8): an opaque LINE_F2, no draw mode of its own.
extern "C" void __cdecl BattleWin_DrawLine(int x0, int y0, int x1, int y1, int r, int gr, int b) {
    unsigned char* const p = Next();
    battle_window_draw::g.set_line_f2(p);
    LineBody(p, x0, y0, x1, y1, r, gr, b);
    battle_window_draw::g.commit(1, 0x20);
}

// original 0x444D50 (PSX 0x801DA484): the line under abr 0 (half and half).
extern "C" void __cdecl BattleWin_DrawLineHalf(int x0, int y0, int x1, int y1, int r, int gr, int b) {
    BlendLine(0, x0, y0, x1, y1, r, gr, b);
}

// original 0x444E00 (PSX 0x801DA578): the line under abr 1 (added).
extern "C" void __cdecl BattleWin_DrawLineAdd(int x0, int y0, int x1, int y1, int r, int gr, int b) {
    BlendLine(1, x0, y0, x1, y1, r, gr, b);
}

// original 0x444EB0 (PSX 0x801DA66C): 1 unless another battle window already
// shows an enemy of this one's kind. The kind is byte +0xC of enemy `enemy`'s
// record (0x93B9E0, stride 0x128); windows 5..12 carry an actor in +0xA
// (enemies are actors 3 and up). The highest of them showing this enemy is
// found (none: -1); every window above it whose state (+3) is not 2 or 3 and
// whose actor's kind byte is the same gives 0.
//
// As the original has it: a window above with an actor below 3 is looked up
// at a negative enemy index, before the records (0x93B674 for actor 0).
extern "C" unsigned char __cdecl BattleWin_FirstOfKind(unsigned enemy) {
    const unsigned e = enemy & 0xFF;
    const unsigned char kind = Battle_Enemies[e].enemy_kind;
    const unsigned actor = e + 3;
    signed char found = 7;
    while (Window(5 + found)[0xA] != actor) {
        --found;
        if (found < 0) break;
    }
    if (found >= 7) return 1;
    signed char k = 7;
    do {
        const unsigned char* const w = Window(static_cast<unsigned>(5 + k));
        auto* const win = rv::AsConst<records::WindowRecord>(w);
        if (win->state != 2 && win->state != 3) {
            const std::int32_t index = static_cast<std::int32_t>(w[0xA]) - 3;
            const std::uint32_t at = ::bof3::Img(Battle_Enemies_addr) + 0x80 + battle_window_draw::at::kEnemyKind + static_cast<std::uint32_t>(index * static_cast<std::int32_t>(battle_window_draw::at::kEnemyStride));
            if (move_script::At(at)[0] == kind) return 0;
        }
        --k;
    } while (k > found);
    return 1;
}

// original 0x444F40 (PSX 0x801DA7A4): the order the party enters its
// commands in. Each member who passes the gate and Battle_ActorCanCommand is
// listed with its agility (+0x98), the list sorted highest first, then
// written to 0x904AB6 (three bytes, 0xFF-ended) - each member asked
// Battle_ActorCanCommand again - with the count at 0x904AC3 (read back as a
// signed byte for the index, as the original does).
// As the original has it: the enemies are walked first through
// Battle_ActorCanAct for a sum and a highest agility that nothing reads (the
// PSX computes and drops the same); the calls are kept, the arithmetic is not.
extern "C" void __cdecl Battle_BuildEntryOrder() {
    for (unsigned a = 3; a <= 10; ++a) {
        if (Gated(Enemy(a) + battle_damage::at::kEFlags2)) continue;
        battle_damage::g.can_act(a);
    }
    unsigned char list[3 * 4];
    unsigned n = 0;
    for (unsigned a = 0; a <= 2; ++a) {
        const unsigned char* const p = Party(a);
        auto* const pr = rv::AsConst<records::PartyMember>(p - 0x10);
        if (Gated(p + battle_damage::at::kPFlags2)) continue;
        if (!battle_damage::g.can_command(a)) continue;
        move_script::SetWord(list + 4 * n, pr->agi);
        move_script::SetWord(list + 4 * n + 2, a);
        ++n;
    }
    SortDescending(list, n);
    BattleActorCopies_EntryCount = 0;
    move_script::SetWord(move_script::At(::bof3::Img(battle_damage::at::kEntryOrder.v)), 0xFFFF);
    move_script::At(::bof3::Img(battle_damage::at::kEntryOrder.v) + 2)[0] = 0xFF;
    for (unsigned k = 0; k < n; ++k) {
        const unsigned char actor = list[4 * k + 2];
        if (!battle_damage::g.can_command(actor)) continue;
        const int index = BattleActorCopies_EntryCount;
        move_script::At(::bof3::Img(battle_damage::at::kEntryOrder.v) + index)[0] = actor;
        BattleActorCopies_EntryCount = static_cast<unsigned char>(static_cast<unsigned char>(BattleActorCopies_EntryCount) + 1);
    }
}

// original 0x4450E0 (PSX Battle_BuildTurnOrder 0x801DAAB4): who acts in
// which order this round. Each actor who passes the gate and
// Battle_ActorCanAct gets a value in 0x939A80 (s16 value, u16 actor):
//   - a party member its agility (+0x98) plus a command bonus times the
//     percentage of its level class (0x64E328, row 1) / 100: the bonus 4 for
//     an ability of skill class 0 (NameTable_Abilities +1), 2 for class 1 or 3,
//     1 for an item, else 0 - 16-bit arithmetic, the product truncated to a
//     word and divided signed;
//   - an enemy its agility (+0x38) plus a signed byte of 0x64E334, sixteen
//     per level class (row 0), picked by Rand & 15.
// The count to 0x904AE3, the pairs sorted highest first, the actors copied
// to 0x904ACC (eleven bytes, 0xFF where unused), the cursor 0x904AE2 to 0.
// As the original has it: the party is first walked through
// Battle_ActorCanAct for a sum and a highest agility nothing reads; and a
// level class of 6 reads the percentage past the five real ones (the first
// word of 0x64E334, 516 %) and the enemies' bytes past their four rows (the
// variance table's bytes) - the PSX has the same (docs/battle_damage.md,
// defects).
extern "C" void __cdecl Battle_BuildTurnOrder() {
    for (unsigned a = 0; a <= 2; ++a) {
        if (Gated(Party(a) + battle_damage::at::kPFlags2)) continue;
        battle_damage::g.can_act(a);
    }
    unsigned n = 0;
    for (unsigned a = 0; a <= 2; ++a) {
        unsigned char* const p = Party(a);
        auto* const pr = rv::As<records::PartyMember>(p - 0x10);
        if (Gated(p + battle_damage::at::kPFlags2)) continue;
        if (!battle_damage::g.can_act(a)) continue;
        unsigned char* const slot = move_script::At(::bof3::Img(battle_damage::at::kTurnWork.v) + 4 * n);
        const unsigned char command = p[battle_damage::at::kPCommand];
        move_script::SetWord(slot + 2, a);
        move_script::SetWord(slot, 0);
        if (command == 4) {
            const unsigned kind = move_script::At(::bof3::Img(bof3::addr::NameTable_Abilities) + pr->queued_id * battle_damage::at::kAbilityStride)[1];
            if (kind == 0) move_script::SetWord(slot, 4);
            else if (kind == 1 || kind == 3) move_script::SetWord(slot, 2);
        }
        if (command == 5) move_script::SetWord(slot, 1);
        const unsigned level_class = battle_damage::g.level_class(pr->level, 1);
        const unsigned percent = move_script::Word(move_script::At(::bof3::Img(battle_damage::at::kTurnPercent.v) + 2 * level_class));
        const int product = S16(percent * move_script::Word(slot));
        move_script::SetWord(slot, static_cast<unsigned>(product / 100) + pr->agi);
        ++n;
    }
    for (unsigned a = 3; a <= 10; ++a) {
        const unsigned char* const e = Enemy(a);
        auto* const er = rv::AsConst<records::BattleEnemy>(e - 0x80);
        if (Gated(e + battle_damage::at::kEFlags2)) continue;
        if (!battle_damage::g.can_act(a)) continue;
        unsigned char* const slot = move_script::At(::bof3::Img(battle_damage::at::kTurnWork.v) + 4 * n);
        const unsigned level_class = battle_damage::g.level_class(e[battle_damage::at::kELevel], 0);
        const unsigned roll = static_cast<unsigned>(battle_damage::g.rand()) & 0xF;
        const int jitter = static_cast<signed char>(Byte(::bof3::Img(battle_damage::at::kTurnJitter.v) + 16 * level_class + roll));
        move_script::SetWord(slot, static_cast<unsigned>(jitter) + er->agi);
        move_script::SetWord(slot + 2, a);
        ++n;
    }
    BattleDamage_TurnCount = static_cast<unsigned char>(n);
    SortDescending(move_script::At(::bof3::Img(battle_damage::at::kTurnWork.v)), n);
    move_script::SetLong(move_script::At(::bof3::Img(battle_damage::at::kTurnOrder.v)), -1);
    move_script::SetLong(move_script::At(::bof3::Img(battle_damage::at::kTurnOrder.v) + 4), -1);
    move_script::SetWord(move_script::At(::bof3::Img(battle_damage::at::kTurnOrder.v) + 8), 0xFFFF);
    move_script::At(::bof3::Img(battle_damage::at::kTurnOrder.v) + 10)[0] = 0xFF;
    for (unsigned k = 0; k < BattleDamage_TurnCount; ++k) move_script::At(::bof3::Img(battle_damage::at::kTurnOrder.v) + k)[0] = Byte(::bof3::Img(battle_damage::at::kTurnWork.v) + 4 * k + 2);
    BossSe_RoundSlot = 0;
}

// original 0x4453C0 (PSX 0x801DB020, the same): the enemies standing
// (Battle_ActorStanding) give an average and a highest agility (+0xB8); every
// member 0x445980 lets act and Battle_PartyOutpaces passes gets flag 0x8000
// (+0x134), and the answer is 1 when one did. Then the same the other way:
// the members' agility (+0xA8), and each enemy 0x445980 lets act and 0x445600
// passes gets flag 0x8000 (+0x114).
// As the original has it: the sums and the highest are built from the whole
// eax Battle_ActorStanding leaves (its upper half over the agility word), the
// sum's low word divided by the count only when it is not 0 - otherwise the
// whole sum is what the tests are given - and the highest replaced when its
// low word is below. Both tests read the low words alone.
extern "C" unsigned __cdecl Battle_MarkFasterSide() {
    const auto measure = [](unsigned from, unsigned to, bool party, std::uint32_t& average, std::uint32_t& highest) {
        std::uint32_t sum = 0, top = 0;
        unsigned count = 0;
        for (unsigned i = from; i <= to; ++i) {
            const unsigned r = battle_setup::g.standing(i);
            if (!Yes(r)) continue;
            const std::uint32_t v = (r & 0xFFFF0000u) | move_script::Word((party ? P(i) + battle_setup::at::kPAgi : E(i) + battle_setup::at::kEAgi));
            sum += v;
            if ((top & 0xFFFF) < (v & 0xFFFF)) top = v;
            count = (count + 1) & 0xFF;
        }
        if (sum & 0xFFFF) sum = static_cast<std::uint32_t>(static_cast<int>(sum & 0xFFFF) / static_cast<int>(count));
        average = sum;
        highest = top;
    };
    std::uint32_t average, highest;
    measure(3, 10, false, average, highest);
    unsigned marked = 0;
    for (unsigned i = 0; i <= 2; ++i) {
        if (!Yes(battle_setup::g.can_act(i))) continue;
        if (!Yes(battle_setup::g.outpaces(i, average, highest))) continue;
        unsigned char* const flags = P(i) + battle_setup::at::kPFlags;
        move_script::SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(flags)) | 0x8000u));
        marked = (marked + 1) & 0xFF;
    }
    const unsigned answer = marked != 0 ? 1 : 0;
    measure(0, 2, true, average, highest);
    for (unsigned i = 3; i <= 10; ++i) {
        if (!Yes(battle_setup::g.can_act(i))) continue;
        if (!Yes(battle_setup::g.enemy_outpaces(i, average, highest))) continue;
        unsigned char* const flags = E(i) + battle_setup::at::kEFlags;
        move_script::SetLong(flags, static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(flags)) | 0x8000u));
    }
    return answer;
}

// original 0x445550 (PSX 0x801DB2C0, the same): 1 when the actor is present
// (+0 bit 0) with none of status 0x4944 (party +0x90) or 0x4144 (enemy
// +0x92). The whole eax as the original leaves it: the answer in al over the
// record's offset (actor * 0x14C, or (actor - 3) * 0x128 as a byte index),
// which Battle_MarkFasterSide's sums carry in their upper half.
extern "C" unsigned __cdecl Battle_ActorStanding(unsigned actor) {
    const unsigned a = actor & 0xFF;
    std::uint32_t offset;
    unsigned r;
    if (a < 3) {
        offset = a * battle_setup::at::kPartyStride;
        auto* const member_rec = rv::AsConst<records::PartyMember>(move_script::At(::bof3::Img(ObjTrio_addr) + offset));
        r = (member_rec->flags & 1) && (member_rec->status & 0x4944) == 0 ? 1 : 0;
    } else {
        offset = ((a - 3) & 0xFF) * battle_setup::at::kEnemyStride;
        auto* const enemy_rec = rv::AsConst<records::BattleEnemy>(move_script::At(::bof3::Img(Battle_Enemies_addr) + offset));
        r = (enemy_rec->flags & 1) && (enemy_rec->status & 0x4144) == 0 ? 1 : 0;
    }
    return (offset & ~0xFFu) | r;
}

// original 0x4455C0 (PSX 0x801DB368, the same): 1 when the member's agility
// (+0xA8) is at least twice the average's low word and at least the
// highest's low word. The whole eax: the agility's high byte over the answer.
extern "C" unsigned __cdecl Battle_PartyOutpaces(unsigned actor, unsigned average, unsigned highest) {
    const unsigned agility = rv::As<records::PartyMember>(P(actor))->agi;
    unsigned r = 1;
    if ((average & 0xFFFF) * 2 > agility) r = 0;
    else if ((highest & 0xFFFF) > agility) r = 0;
    return (agility & 0xFF00) | r;
}

// original 0x445600 (PSX 0x801DB3AC, gap74): Battle_PartyOutpaces' enemy twin -
// 1 when the enemy's u16 +0xB8 is at least twice the average's low word
// (signed compare of the two ints) and at least the highest's low word.
extern "C" unsigned char __cdecl Battle_EnemyOutpaces(unsigned actor, unsigned average, unsigned highest) {
    const unsigned pace = rv::As<records::BattleEnemy>(EnemyBattleE4(actor))->agi;
    if (S32BattleE4((average & 0xFFFF) << 1) > S32BattleE4(pace)) return 0;
    if ((highest & 0xFFFF) > pace) return 0;
    return 1;
}

// original 0x445640 (PSX Battle_LevelClass 0x801DB3FC): the first of six
// thresholds in row `row` of 0x64E374 that the level is below, else 6. Row 0
// the enemies' (16, 36, 64, 99, 99, 99), row 1 the party's (8, 16, 32, 48,
// 99, 99), so 4 and 5 never come back and a level of 99 is class 6. Both
// arguments' low bytes; the row is not bounded.
extern "C" unsigned char __cdecl Battle_LevelClass(unsigned level, unsigned row) {
    const unsigned char* const thresholds = move_script::At(::bof3::Img(battle_damage::at::kLevelRows.v) + (row & 0xFF) * 6);
    for (unsigned i = 0; i < 6; ++i)
        if ((level & 0xFF) < thresholds[i]) return static_cast<unsigned char>(i);
    return 6;
}

// original 0x445680 (PSX Battle_ClearCommands 0x801DB45C): the command byte
// and the first flag dword of the three party records and the eight enemy
// records cleared.
extern "C" void __cdecl Battle_ClearCommands() {
    for (unsigned a = 0; a < 3; ++a) {
        unsigned char* const p = Party(a);
        auto* const pr = rv::As<records::PartyMember>(p - 0x10);
        p[battle_damage::at::kPCommand] = 0;
        pr->flags_130 = 0;
    }
    for (unsigned a = 3; a < 11; ++a) {
        auto* const er = rv::As<records::BattleEnemy>(Enemy(a) + battle_damage::at::kEObject);
        er->command_kind = 0;
        er->flags_110 = 0;
    }
}

// original 0x4456C0 (PSX 0x801DB4EC, the sibling's Battle_ActorFrameUpdate
// hypothesis - it is this test): 1 when the actor is out of the fight -
// its object's bit 0 clear, or its status 0x4000 set (party: ObjTrio + 0x14C
// * actor, byte +0x91; enemy: 0x93B960 + 0x128 * (actor - 3), byte +0x93).
//
// As the original has it: the actor is the argument's low byte, and the
// enemy index is not checked.
extern "C" unsigned char __cdecl Battle_ActorIsOut(unsigned actor) {
    const unsigned a = actor & 0xFF;
    if (a < 3) {
        const unsigned char* const m = battle_flow::At(::bof3::Img(ObjTrio_addr) + a * battle_flow::at::kMemberSize);
        auto* const member = rv::AsConst<records::PartyMember>(m);
        if ((member->flags & 1) == 0) return 1;
        return (m[0x91] & 0x40) != 0 ? 1 : 0;
    }
    const unsigned char* const e = EnemyObject((a - 3) & 0xFF);
    auto* const enemy_rec = rv::AsConst<records::BattleEnemy>(e);
    if ((enemy_rec->flags & 1) == 0) return 1;
    return (e[0x93] & 0x40) != 0 ? 1 : 0;
}

// original 0x445730 (PSX 0x801DB594): the first actor on the same side as
// `actor` (its low byte) that 0x4456C0 does not call out, looking from it
// upwards and then from the side's first: the party 0..2, the enemies 3..10;
// 0xFF if none. A byte above 10 skips the upward look and asks 3 up to it.
// Each probe is pushed as the argument slot with its low byte replaced - the
// callee reads the byte.
extern "C" unsigned char __cdecl Battle_DefaultTarget(unsigned actor) {
    const unsigned b = actor & 0xFF;
    const unsigned high = actor & ~0xFFu;
    if (b < 3) {
        for (unsigned i = b; i <= 2; ++i)
            if (!battle_damage::g.is_out(high | i)) return static_cast<unsigned char>(i);
        for (unsigned i = 0; i < b; ++i)
            if (!battle_damage::g.is_out(high | i)) return static_cast<unsigned char>(i);
        return 0xFF;
    }
    if (b <= 10)
        for (unsigned i = b; i <= 10; ++i)
            if (!battle_damage::g.is_out(high | i)) return static_cast<unsigned char>(i);
    for (unsigned i = 3; i < b; ++i)
        if (!battle_damage::g.is_out(high | i)) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x4457F0 (PSX 0x801DB6BC, gap74): Battle_DefaultTarget's downward
// twin. From the actor byte down within its side (0..2, or 3..10), the first
// actor Battle_ActorIsOut answers 0 for; then from the side's top (2, or 10)
// down to just above the byte; 0xFF if none. The comparisons are signed bytes,
// as the original's: a byte of 0x80..0xFF counts as below 3 and searches 10
// down through the bytes that follow 0 (0xFF, 0xFE ...) to just above it.
extern "C" unsigned char __cdecl Battle_PrevTarget(unsigned actor) {
    const auto start = static_cast<unsigned char>(actor);
    const auto s = static_cast<signed char>(start);
    if (start < 3) {
        for (signed char i = s;; --i) {
            if (BH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i)) == 0) return static_cast<unsigned char>(i);
            if (static_cast<signed char>(i - 1) < 0) break;
        }
        if (s >= 2) return 0xFF;
        for (signed char i = 2;; --i) {
            if (BH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i)) == 0) return static_cast<unsigned char>(i);
            if (!(static_cast<signed char>(i - 1) > s)) return 0xFF;
        }
    }
    if (s >= 3) {
        for (signed char i = s;; --i) {
            if (BH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i)) == 0) return static_cast<unsigned char>(i);
            if (!(static_cast<signed char>(i - 1) >= 3)) break;
        }
    }
    if (s >= 10) return 0xFF;
    for (signed char i = 10;; --i) {
        if (BH_CALL(Battle_ActorIsOut)(static_cast<unsigned char>(i)) == 0) return static_cast<unsigned char>(i);
        if (!(static_cast<signed char>(i - 1) > s)) return 0xFF;
    }
}

// original 0x4458B0 (PSX 0x801DB80C): Battle_ActorCanAct with more refused -
// the status mask 0x4964 for the party, and the second flags' bits 0x14001
// (party) or 0x4000 (enemy). What the entry order asks before it lists a
// member.
extern "C" unsigned char __cdecl Battle_ActorCanCommand(unsigned actor) {
    if ((actor & 0xFF) < 3) {
        const unsigned char* const p = Party(actor);
        auto* const pr = rv::AsConst<records::PartyMember>(p - 0x10);
        if ((pr->flags & 1) == 0) return 0;
        if (pr->status & 0x4964) return 0;
        if (BattleFlow_AnimGate != 0 && (p[battle_damage::at::kPFlags2] & 0x10) == 0) return 0;
        if (static_cast<std::int32_t>(pr->flags_134) & 0x14001) return 0;
        const unsigned char side = BattlePhases_Initiative;
        return side == 2 || side == 3 ? 0 : 1;
    }
    const unsigned char* const e = Enemy(actor);
    auto* const er = rv::AsConst<records::BattleEnemy>(e - 0x80);
    if ((er->flags & 1) == 0) return 0;
    if (er->status & 0x4164) return 0;
    if (BattleFlow_AnimGate != 0 && (e[battle_damage::at::kEFlags2] & 0x10) == 0) return 0;
    if (static_cast<std::int32_t>(er->flags_114) & 0x4000) return 0;
    const unsigned char side = BattlePhases_Initiative;
    return side == 1 || side == 3 ? 0 : 1;
}

// original 0x445980 (PSX Battle_ActorCanAct 0x801DB9AC): 1 if the actor can
// take a turn. A party member: bit 0 of its object's first byte, none of the
// status bits 0x4944, and with 0x904B8E set bit 4 of its second flags; an
// enemy the same on its own fields with 0x4144. Then 0 when 0x904AE4 names the
// actor's side (2 the party, 1 the enemies) or is 3.
extern "C" unsigned char __cdecl Battle_ActorCanAct(unsigned actor) {
    if ((actor & 0xFF) < 3) {
        const unsigned char* const p = Party(actor);
        auto* const pr = rv::AsConst<records::PartyMember>(p - 0x10);
        if ((pr->flags & 1) == 0) return 0;
        if (pr->status & 0x4944) return 0;
        if (BattleFlow_AnimGate != 0 && (p[battle_damage::at::kPFlags2] & 0x10) == 0) return 0;
        const unsigned char side = BattlePhases_Initiative;
        return side == 2 || side == 3 ? 0 : 1;
    }
    const unsigned char* const e = Enemy(actor);
    auto* const er = rv::AsConst<records::BattleEnemy>(e - 0x80);
    if ((er->flags & 1) == 0) return 0;
    if (er->status & 0x4144) return 0;
    if (BattleFlow_AnimGate != 0 && (e[battle_damage::at::kEFlags2] & 0x10) == 0) return 0;
    const unsigned char side = BattlePhases_Initiative;
    return side == 1 || side == 3 ? 0 : 1;
}

// original 0x445A30 (PSX Battle_ApplyDamage 0x801DBB40): an attack landed.
// The target's shown byte (+0x11C / +0x8C) = 0x11; the amount
// Battle_CalcDamage(attacker, target, 0xFFFF). The flag 0x100 in the target's
// flags (an enemy's only while its HP is at most 0x7FFF) makes the amount the
// whole HP, shows the message 0x44A650(1, 0, 0, 0x1E, *0x669E04), clears the
// flag and sets 0x8031F3. Then, a party member: damage below its HP is taken
// off; otherwise HP 0, and the flag bit 2 (a kill) set when the overkill is
// below its byte +0x8C and neither of its second flags' bits 0..1 is set; a
// heal (0 or less) is added up to max HP, reaching it sets shown bit 2. An
// enemy the same without the overkill test, and an HP of 0xFFFF takes
// nothing. Last, a party attacker whose character index (+0x148) is 5, for a
// non-zero amount: weapon 'J' / 'L' / 'O' inflicts 0x40 / 8 / 0x20 through
// 0x44F1D0 unless 0x44FA70 says it was resisted. Returns the amount.
extern "C" short __cdecl Battle_ApplyDamage(unsigned attacker, unsigned target) {
    const unsigned t = target & 0xFF;
    if (t <= 2) Party(t)[battle_damage::at::kPShown] = 0x11;
    else rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->shown = 0x11;
    std::int32_t v = battle_damage::g.calc_damage(attacker, target, 0xFFFF);
    if (t <= 2) {
        unsigned char* const p = Party(t);
        auto* const pr = rv::As<records::PartyMember>(p - 0x10);
        if (static_cast<std::int32_t>(pr->flags_130) & 0x100) {
            v = pr->hp;
            battle_damage::g.show_message(1, 0, 0, 0x1E, BattleDamage_KillMessage);
            const std::uint32_t f = U32(static_cast<std::int32_t>(pr->flags_130)) & ~0x100u;
            WindowRecords[4].state = 1;
            pr->flags_130 = S32(f);
        }
        const std::int32_t hp = pr->hp;
        if (v > 0) {
            if (hp > v) {
                pr->hp = U32(hp - v);
            } else {
                if (v - hp < pr->survive && (p[battle_damage::at::kPFlags2] & 3) == 0)
                    pr->flags_130 = S32(U32(static_cast<std::int32_t>(pr->flags_130)) | 4);
                pr->hp = 0;
            }
        } else {
            const std::int32_t max = pr->max_hp;
            if (hp - v < max) {
                pr->hp = U32(hp - v);
            } else {
                pr->hp = U32(max);
                p[battle_damage::at::kPShown] = static_cast<unsigned char>(p[battle_damage::at::kPShown] | 4);
            }
        }
    } else {
        unsigned char* const e = Enemy(t);
        auto* const er = rv::As<records::BattleEnemy>(e - 0x80);
        if (er->hp != 0xFFFF) {
            if ((static_cast<std::int32_t>(er->flags_110) & 0x100) && er->hp <= 0x7FFF) {
                v = move_script::Word(e + battle_damage::at::kEHp);
                battle_damage::g.show_message(1, 0, 0, 0x1E, BattleDamage_KillMessage);
                const std::uint32_t f = U32(static_cast<std::int32_t>(er->flags_110)) & ~0x100u;
                WindowRecords[4].state = 1;
                er->flags_110 = S32(f);
            }
            const std::int32_t hp = er->hp;
            if (v > 0) {
                rv::Store<unsigned>(er->hp, hp > v ? U32(hp - v) : 0);
            } else {
                const std::int32_t max = er->max_hp;
                if (hp - v < max) {
                    er->hp = U32(hp - v);
                } else {
                    er->hp = U32(max);
                    er->shown = static_cast<unsigned char>(er->shown | 4);
                }
            }
        }
    }
    if ((attacker & 0xFF) <= 2) {
        auto* const pr = rv::As<records::PartyMember>(Party(attacker) - 0x10);
        if (pr->char_index == 5 && v != 0) {
            if (pr->weapon == 'J' && !battle_damage::g.element_resisted(attacker, target)) battle_damage::g.inflict(target, 0x40);
            if (pr->weapon == 'L' && !battle_damage::g.element_resisted(attacker, target)) battle_damage::g.inflict(target, 8);
            if (pr->weapon == 'O' && !battle_damage::g.element_resisted(attacker, target)) battle_damage::g.inflict(target, 0x20);
        }
    }
    return static_cast<short>(v);
}

// original 0x445CF0 (PSX Battle_CalcDamage 0x801DC00C): an attack's damage,
// before it is applied.
//   1. SharedScratch = `element`'s low word; 0xFFFF means the weapon's: a
//      party attacker's weapon element byte (NameTable_Weapons +0x13), an
//      enemy 0.
//   2. A charged attacker (second flags bit 6): the attack 0x939FE4 plus
//      (charge x attack) >> 1 for the party - but set to that for an enemy,
//      as on the PSX; the charge byte cleared, the bit cleared.
//   3. Battle_BaseDamage(attacker, target, 0). A party attacker on an enemy:
//      doubled for weapons 0x13 / 0x17 / 0x33 on family 4, and again for
//      0x16 / 0x35 / 0x40 on family 1 (the enemy record's +0x0D).
//   4. The target's status byte: without any of 0x64, the to-hit roll
//      (0x446110 on the party, 0x4461B0 on an enemy) decides the amount;
//      with 0x40 or 0x20 the status is cured through 0x44F4B0 (its bit
//      or'ed into 0x904B9A) and the amount is at least 1; with 4, at least 1.
//   5. Flag bit 1: the amount x {50,50,50,50,60,60,60,70}[Rand % 8] / 100.
//   6. 0x904AA8 bit 7: amount x 2 + Battle_BaseDamage(.., 1) / 4.
//   7. A quarter off: on a party target when 0x904060 is 2; on a status 0x80.
//   8. At least 1 with status 0x60, or with 0x904AA8 bit 7.
//   9. 0 if the target's flags have 0x10000; else clamped to +-9999.
// From step 7 on the amount is a word (the original's `sar ax, 2` with the
// rest of eax whatever it held); before it, 32 bits, as the to-hit roll and
// step 6 see it.
// As the original has it: step 5 reads the table at Rand % 8 signed - the
// CRT's rand is never negative, and ours refuses loudly if it were.
extern "C" short __cdecl Battle_CalcDamage(unsigned attacker, unsigned target, unsigned element) {
    const unsigned a = attacker & 0xFF;
    const unsigned t = target & 0xFF;
    unsigned char* const scratch = move_script::At(::bof3::Img(SharedScratch_addr));
    if ((element & 0xFFFF) == 0xFFFF) {
        if (a <= 2) {
            const unsigned weapon = rv::As<records::PartyMember>(Party(a) - 0x10)->weapon;
            move_script::SetWord(scratch, Byte(::bof3::Img(bof3::addr::NameTable_Weapons) + weapon * battle_damage::at::kWeaponStride + battle_damage::at::kWeaponElement));
        } else {
            move_script::SetWord(scratch, 0);
        }
    } else {
        move_script::SetWord(scratch, element);
    }
    unsigned char* const atk = move_script::At(::bof3::Img(battle_damage::at::kAttackerAtk.v));
    if (a <= 2) {
        unsigned char* const p = Party(a);
        auto* const pr = rv::As<records::PartyMember>(p - 0x10);
        if (p[battle_damage::at::kPFlags2] & 0x40) {
            const std::int32_t add = static_cast<std::int32_t>(pr->count_40 * (U32(move_script::Long(atk)) & 0xFFFF)) >> 1;
            move_script::SetWord(atk, move_script::Word(atk) + U32(add));
            pr->count_40 = 0;
            pr->flags_134 = S32(U32(static_cast<std::int32_t>(pr->flags_134)) & ~0x40u);
        }
    } else {
        unsigned char* const e = Enemy(a);
        auto* const er = rv::As<records::BattleEnemy>(e - 0x80);
        if (e[battle_damage::at::kEFlags2] & 0x40) {
            const std::int32_t set = static_cast<std::int32_t>(er->count_40 * (U32(move_script::Long(atk)) & 0xFFFF)) >> 1;
            move_script::SetWord(atk, U32(set));
            er->count_40 = 0;
            er->flags_114 = S32(U32(static_cast<std::int32_t>(er->flags_114)) & ~0x40u);
        }
    }
    std::int32_t v = battle_damage::g.base_damage(attacker, target, 0);
    if (a <= 2 && t >= 3) {
        const unsigned char weapon = rv::As<records::PartyMember>(Party(a) - 0x10)->weapon;
        if ((weapon == 0x13 || weapon == 0x17 || weapon == 0x33) && rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->family == 4) v = S32(U32(v) * 2);
        if ((weapon == 0x16 || weapon == 0x35 || weapon == 0x40) && rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->family == 1) v = S32(U32(v) * 2);
    }
    unsigned char status, flags;
    if (t <= 2) {
        status = Party(t)[battle_damage::at::kPStatus];
        flags = Party(t)[battle_damage::at::kPFlags];
    } else {
        status = Enemy(t)[battle_damage::at::kEStatus];
        flags = Enemy(t)[battle_damage::at::kEFlags];
    }
    if ((status & 0x64) == 0) {
        v = t <= 2 ? battle_damage::g.hit_party(v, attacker, target) : battle_damage::g.hit_enemy(v, attacker, target);
    } else {
        if (status & 0x40) {
            move_script::At(::bof3::Img(battle_damage::at::kCuredBits.v))[0] = static_cast<unsigned char>(Byte(::bof3::Img(battle_damage::at::kCuredBits.v)) | 0x40);
            if (t <= 2) {
                battle_damage::g.cure(rv::As<records::PartyMember>(Party(t) - 0x10)->actor, 0x40);
                Party(t)[battle_damage::at::kPShown2] = 0;
            } else {
                battle_damage::g.cure(rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->actor, 0x40);
                rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->counter = 0;
            }
            if ((U32(v) & 0xFFFF) == 0) v = 1;
        }
        if (status & 0x20) {
            move_script::SetWord(move_script::At(::bof3::Img(battle_damage::at::kCuredBits.v)), move_script::Word(move_script::At(::bof3::Img(battle_damage::at::kCuredBits.v))) | 0x20);
            if (t <= 2) {
                battle_damage::g.cure(rv::As<records::PartyMember>(Party(t) - 0x10)->actor, 0x20);
                Party(t)[battle_damage::at::kPShown2] = 0;
            } else {
                battle_damage::g.cure(rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->actor, 0x20);
                rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->counter = 0;
            }
            if ((U32(v) & 0xFFFF) == 0) v = 1;
        }
        if ((status & 4) && (U32(v) & 0xFFFF) == 0) v = 1;
    }
    if (flags & 2) {
        const int roll = battle_damage::g.rand();
        if (roll < 0) bof3::Fatal("Battle_CalcDamage: Rand %d, where the original reads its saved registers", roll);
        v = Mul(kFlagRoll[roll % 8], S16(U32(v))) / 100;
    }
    if (Byte(::bof3::Img(battle_damage::at::kBattleFlags.v)) & 0x80) {
        const std::int32_t plain = S16(U32(battle_damage::g.base_damage(attacker, target, 1)));
        v = S32(U32((plain + (plain < 0 ? 3 : 0)) >> 2) + U32(v) * 2);
    }
    unsigned short d = static_cast<unsigned short>(v);
    if (t <= 2 && Cond_Flags[0xd0] == 2) d = static_cast<unsigned short>(d - static_cast<unsigned short>(S16(d) >> 2));
    if (status & 0x80) d = static_cast<unsigned short>(d - static_cast<unsigned short>(S16(d) >> 2));
    if ((status & 0x60) && d == 0) d = 1;
    if ((Byte(::bof3::Img(battle_damage::at::kBattleFlags.v)) & 0x80) && d == 0) d = 1;
    if (t <= 2 ? (static_cast<std::int32_t>(rv::As<records::PartyMember>(Party(t) - 0x10)->flags_130) & 0x10000) : (static_cast<std::int32_t>(rv::As<records::BattleEnemy>(Enemy(t) + battle_damage::at::kEObject)->flags_110) & 0x10000)) return 0;
    if (S16(d) > 9999) return 9999;
    if (S16(d) < -9999) return -9999;
    return S16(d);
}

// original 0x446110 (no PSX twin in the pairs): an attack on a member, hit or
// missed. With the round flag 0x80 (0x904AA8) the amount stands. Else a miss
// when the attacker's status bit 8 (a member's +0x90, an enemy's +0x92) is set
// and Rand's bit 1 is set, or when 0x939F9B is not below Rand() % 100; a
// miss clears the target member's +0x12C bit 0x10 and answers 0. ax out.
extern "C" int __cdecl Battle_HitOrMissParty(int amount, unsigned attacker, unsigned target) {
    const auto hit = static_cast<int>(static_cast<U>(amount) & 0xFFFF);
    if (B(::bof3::Img(battle_e4::at::kFlags.v)) & 0x80) return hit;
    const unsigned a = Low(attacker);
    const bool dazed = a <= 2 ? (PartyBattleE4(a)[0x90] & 8) != 0 : (EnemyBattleE4(a)[0x92] & 8) != 0;
    if (!dazed || (BH_CALL(Rand)() & 2) == 0) {
        const int roll = BH_CALL(Rand)() % 100;
        if (BattleE3_EvadeBoost < roll) return hit;
    }
    PartyBattleE4(target)[0x12C] &= 0xEF;
    return 0;
}

// original 0x4461B0 (no PSX twin in the pairs): the same on an enemy. The
// round flag 0x80: the amount. The attacker's status bit 8 and Rand's bit 1: a
// miss. A member attacking with +0x134 bit 0x100: a hit. The target's +0x90
// bit 4 and Rand's bit 1: a miss. An enemy attacking: a hit. Else a hit when
// the low byte of 0x939FFC is above Rand() % 100. A miss clears the target
// enemy's +0x10C bit 0x10 and answers 0. ax out.
extern "C" int __cdecl Battle_HitOrMissEnemy(int amount, unsigned attacker, unsigned target) {
    const auto hit = static_cast<int>(static_cast<U>(amount) & 0xFFFF);
    if (B(::bof3::Img(battle_e4::at::kFlags.v)) & 0x80) return hit;
    const unsigned a = Low(attacker);
    const bool dazed = a <= 2 ? (PartyBattleE4(a)[0x90] & 8) != 0 : (EnemyBattleE4(a)[0x92] & 8) != 0;
    if (dazed && (BH_CALL(Rand)() & 2)) {
        rv::As<records::BattleEnemy>(EnemyBattleE4(target))->shown &= 0xEF;
        return 0;
    }
    if (a <= 2 && (static_cast<U>(static_cast<std::int32_t>(rv::As<records::PartyMember>(PartyBattleE4(a))->flags_134)) & 0x100)) return hit;
    unsigned char* const e = EnemyBattleE4(target);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
    if ((e[0x90] & 4) == 0 || (BH_CALL(Rand)() & 2) == 0) {
        if (a > 2) return hit;
        const int roll = BH_CALL(Rand)() % 100;
        if (static_cast<int>(static_cast<U>(move_script::Long(move_script::At(::bof3::Img(BattleE4_HitEnemy_addr)))) & 0xFF) > roll) return hit;
    }
    enemy_rec->shown &= 0xEF;
    return 0;
}

// original 0x4462B0 (PSX Battle_BaseDamage 0x801DCAA0): the attack less the
// defence, then Battle_ScaleDamage. The attack is the scratch word 0x939FE4.
// A party attacker, or an enemy on an enemy: less the target's defence
// 0x939F86 (mode 0) or nothing (mode 1), not below 0. An enemy on the party:
// less 0x4463E0's defence (mode 0), not below 0, then in 8.8 plus the enemy's
// word +0x18 x (2 + Rand % 2) << 16 / (0x64E380[i] << 8), i.e. x 0.2 or 0.3;
// i is (x / -5), 0 above -5, 8 below -0x33 - and x is 0 or more by then, so
// i is always 0 and the divisor always 10 (the PSX has the same dead
// arithmetic). Then + (Rand & 1) and Battle_ScaleDamage(attacker, target,
// that), whose answer is returned whole. Only the arguments' low bytes are
// read; both are passed on whole.
extern "C" int __cdecl Battle_BaseDamage(unsigned attacker, unsigned target, unsigned mode) {
    const unsigned a = attacker & 0xFF;
    const bool plain = (mode & 0xFF) != 0;
    std::int32_t v;
    if (a <= 2 || (target & 0xFF) > 2) {
        const std::int32_t def = plain ? 0 : BattleDamage_TargetDef;
        v = static_cast<std::int32_t>(U32(move_script::Long(move_script::At(::bof3::Img(battle_damage::at::kAttackerAtk.v)))) & 0xFFFF) - def;
        if (v < 0) v = 0;
    } else {
        const std::int32_t def = plain ? 0 : static_cast<std::int32_t>(U32(battle_damage::g.party_def()) & 0xFFFF);
        std::int32_t x = static_cast<std::int32_t>(U32(move_script::Long(move_script::At(::bof3::Img(battle_damage::at::kAttackerAtk.v)))) & 0xFFFF) - def;
        if (x < 0) x = 0;
        unsigned char index = static_cast<unsigned char>(x / -5);
        if (x > -5) index = 0;
        if (x < -0x33) index = 8;
        const std::int32_t factor = battle_damage::g.rand() % 2 + 2;
        const std::int32_t numerator = S32(U32(Mul(factor, rv::As<records::BattleEnemy>(Enemy(attacker) + battle_damage::at::kEObject)->level)) << 16);
        const std::int32_t divisor = static_cast<std::int32_t>(Byte(::bof3::Img(battle_damage::at::kBonusDivisor.v) + index)) << 8;
        if (divisor == 0) bof3::Fatal("Battle_BaseDamage: a zero divisor, where the original divides by zero");
        v = S32(U32(numerator / divisor) + (U32(x) << 8)) >> 8;
    }
    v = S32(U32(v) + (U32(battle_damage::g.rand()) & 1));
    return battle_damage::g.scale_damage(attacker, target, v);
}

// original 0x4463E0 (no PSX twin in the pairs): the party's mean u16 +0xA6
// over the count (the dword 0x904AB0's low byte; the sum kept to 16 bits, as
// the original's `and eax, 0xFFFF`), plus the u16 0x939F86, halved toward 0.
// The original divides by the count unchecked: ours aborts at 0.
extern "C" int __cdecl Battle_PartyDefenceMean(void) {
    const unsigned n = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(battle_e4::at::kPartyCount.v)))) & 0xFF;
    U sum = 0;
    for (unsigned i = 0; i < n; ++i) sum += rv::As<records::PartyMember>(PartyBattleE4(i))->def;
    if (n == 0)
        bof3::Fatal("Battle_PartyDefenceMean: the party count 0x904AB0 is 0 - the original divides by it (docs/battle_e4.md "
                    "section 7)");
    const int mean = static_cast<int>(sum & 0xFFFF) / static_cast<int>(n);
    const int v = mean + BattleDamage_TargetDef;
    return v / 2;
}

// original 0x446430 (PSX Battle_ScaleDamage 0x801DCD18): the formula's tail.
// In 8.8 fixed point: s = base << 8; the multiplier 0x100 - (s / 256000) <<
// 17, >> 8, at least 0xCD (205/256 - for any base under 256,000 it is 0xCD);
// times s >> 8; times the variance Battle_DamageVarianceTable[Rand & 7]
// (218..307 / 256) >> 8. Then, when SharedScratch's low byte has any of bits
// 0..4 (the weapon's elements), times Battle_ElementAffinity(target, the
// scratch word) / 100; when it has bit 5 (holy), times
// Battle_HolyAffinityTable[the target's holy class] / 100 (the scratch read
// again after the affinity call, as the original does). Rounded half up out
// of 8.8. Every product 32-bit and wrapping, every division signed and
// truncating. `attacker` is not read.
extern "C" int __cdecl Battle_ScaleDamage(unsigned attacker, unsigned target, int base) {
    (void)attacker;
    const std::int32_t s = S32(U32(base) << 8);
    std::int32_t m = S32(0x100u - (U32(s / 256000) << 17)) >> 8;
    if (m < 0xCD) m = 0xCD;
    const int roll = battle_damage::g.rand();
    m = Mul(m, s) >> 8;
    std::int32_t v = Mul(move_script::Long(move_script::At(::bof3::Img(bof3::addr::Battle_DamageVarianceTable) + 4 * (U32(roll) & 7))), m) >> 8;
    unsigned scratch = SharedScratch.w_00;
    if (scratch & 0x1F) {
        const short affinity = static_cast<short>(battle_damage::g.element_affinity(target, scratch));
        v = Mul(affinity, v) / 100;
        scratch = SharedScratch.w_00;
    }
    if (scratch & 0x20) {
        const unsigned holy = (target & 0xFF) <= 2 ? rv::As<records::PartyMember>(Party(target) - 0x10)->holy : rv::As<records::BattleEnemy>(Enemy(target) + battle_damage::at::kEObject)->holy;
        v = Mul(S16(move_script::Word(move_script::At(::bof3::Img(bof3::addr::Battle_HolyAffinityTable) + 2 * holy))), v) / 100;
    }
    if ((v & 0xFF) >= 0x80) v = S32(U32(v) + 0x100);
    return v >> 8;
}

// original 0x446540 (PSX 0x801DCEC0): with status 0x80 (a member's +0x90, an
// enemy's +0x92) the HP change word (a member's +0x128, an enemy's +0x108) is
// a tenth of the HP (+0x98 / +0xA4), rounded: ((hp * 10) / 10 + 5) / 10.
extern "C" void __cdecl Battle_SetHpChange(unsigned actor) {
    const unsigned a = Low(actor);
    auto tenth = [](unsigned hp) {
        const int x = static_cast<int>(hp) * 10;
        return static_cast<unsigned>((x / 10 + 5) / 10);
    };
    if (a <= 2) {
        unsigned char* const p = PartyBattleE4(a);
        auto* const member_rec = rv::As<records::PartyMember>(p);
        if (p[0x90] & 0x80) rv::Store<unsigned>(member_rec->hp_change, tenth(member_rec->hp));
        return;
    }
    unsigned char* const e = EnemyBattleE4(a);
    auto* const enemy_rec = rv::As<records::BattleEnemy>(e);
    if (e[0x92] & 0x80) enemy_rec->hp_change = tenth(enemy_rec->hp);
}

// original 0x446600 (PSX Battle_ReloadPartyRecords 0x801DD054, the sibling's
// name): for each of the count's members (the dword 0x904AB0's low byte),
// 0x29 dwords of its character record (by its +0x148) over its +0x80.
extern "C" void __cdecl Battle_ReloadPartyRecords(void) {
    const unsigned n = static_cast<U>(move_script::Long(move_script::At(::bof3::Img(battle_e4::at::kPartyCount.v)))) & 0xFF;
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const p = PartyBattleE4(i) + 0x80;
        std::memcpy(p, CharRecord(p[0xC8]), 0x29 * 4);
    }
}

// original 0x446650 (PSX Battle_RemoveFromTurnOrder 0x801DD114): the actor's
// command byte cleared and bit 1 of its flags (a party member also bit 2 of
// its second flags), then every place in the turn order (0x904ACC, up to the
// count 0x904AE3) holding the actor's byte set to 0xFF.
extern "C" void __cdecl Battle_RemoveFromTurnOrder(unsigned actor) {
    const unsigned char b = static_cast<unsigned char>(actor);
    if (b <= 2) {
        unsigned char* const p = Party(actor);
        auto* const pr = rv::As<records::PartyMember>(p - 0x10);
        p[battle_damage::at::kPCommand] = 0;
        pr->flags_130 = S32(U32(static_cast<std::int32_t>(pr->flags_130)) & ~2u);
        pr->flags_134 = S32(U32(static_cast<std::int32_t>(pr->flags_134)) & ~4u);
    } else {
        auto* const er = rv::As<records::BattleEnemy>(Enemy(actor) + battle_damage::at::kEObject);
        er->command_kind = 0;
        er->flags_110 = S32(U32(static_cast<std::int32_t>(er->flags_110)) & ~2u);
    }
    for (unsigned k = 0; k < BattleDamage_TurnCount; ++k)
        if (Byte(::bof3::Img(battle_damage::at::kTurnOrder.v) + k) == b) move_script::At(::bof3::Img(battle_damage::at::kTurnOrder.v) + k)[0] = 0xFF;
}

// original 0x446700 (no PSX twin in the pairs): the turn order's front moved
// down one (0x904AE2 - 1, stored first) and the actor put there (0x904ACC +
// the new front).
extern "C" void __cdecl Battle_OrderPushFront(unsigned actor) {
    const auto front = static_cast<unsigned char>(BossSe_RoundSlot - 1);
    BossSe_RoundSlot = front;
    B(::bof3::Img(battle_e4::at::kOrder.v) + front) = static_cast<unsigned char>(actor);
}

// original 0x446720 (PSX AutoBattle_FillCommands 0x801DD264, the sibling's
// name): from the count of commands chosen 0x904AC3 while it is below 3
// (signed), the member at that place of the entry order 0x904AB6 (a signed
// index) - to the first 0xFF - is made 0x904AAE and given command 3 (+0x124)
// with +0x125 = 1; the count is stored after each.
extern "C" void __cdecl AutoBattle_FillCommands(void) {
    auto c = BattleActorCopies_EntryCount;
    if (c >= 3) return;
    for (;;) {
        const unsigned char m = B(::bof3::Img(battle_e4::at::kEntryOrder.v) + static_cast<U>(static_cast<std::int32_t>(c)));
        if (m == 0xFF) return;
        BattleE4_AutoMember = m;
        unsigned char* const p = PartyBattleE4(m);
        c = static_cast<signed char>(c + 1);
        p[0x125] = 1;
        p[0x124] = 3;
        BattleActorCopies_EntryCount = static_cast<unsigned char>(c);
        if (c >= 3) return;
    }
}

// original 0x446770 (PSX 0x801DD318): an effect task's (dx, dz) +0xC / +0x10
// turned by its direction +8 (TurnPair). 70-odd callers: the spells' and the
// action tasks'.
extern "C" void __cdecl Battle_TurnVectorC(unsigned char* record) { TurnPair(record, 0xC); }

// original 0x4467C0 (PSX 0x801DD394, gap31): the same on +0x18 / +0x1C.
extern "C" void __cdecl Battle_TurnVector18(unsigned char* record) { TurnPair(record, 0x18); }

// original 0x446810 (PSX 0x801DD410): whether the member Field_State reacts,
// its callers BE3's 0x441D80 (which set the round flag 0x40 on a 1). The flag
// already set: cleared (a 16-bit and), 0. Else 0 when Field_State's +0x130
// bit 0, an acting actor below 3, +0x90 & 0x4864, kind 4 with ability 0xA1,
// or 0x904B8E set without +0x134 bit 0x10; 1 with +0x130 bit 15; else 1 when
// +0xB9 is above Rand() % 100 (Field_State read again after Rand).
extern "C" unsigned char __cdecl Battle_MemberReactRoll(void) {
    if (B(::bof3::Img(battle_e4::at::kFlags.v)) & 0x40) {
        move_script::SetWord(move_script::At(::bof3::Img(battle_e4::at::kFlags.v)), move_script::Word(move_script::At(::bof3::Img(battle_e4::at::kFlags.v))) & 0xFFBF);
        return 0;
    }
    unsigned char* const fs = Field_State;
    auto* const member_rec = rv::As<records::PartyMember>(fs);
    const U word = static_cast<U>(static_cast<std::int32_t>(member_rec->flags_130));
    if (word & 1) return 0;
    if (B(::bof3::Img(battle_e4::at::kActor.v)) < 3) return 0;
    if (member_rec->status & 0x4864) return 0;
    if (B(::bof3::Img(battle_e4::at::kActionKind.v)) == 4 && move_script::Word(move_script::At(::bof3::Img(battle_e4::at::kAbility.v))) == 0xA1) return 0;
    if (BattleFlow_AnimGate != 0 && (fs[0x134] & 0x10) == 0) return 0;
    if (word & 0x8000) return 1;
    const int roll = BH_CALL(Rand)() % 100;
    return static_cast<int>(Field_State[0xB9]) > roll ? 1 : 0;
}

// original 0x4468B0 (PSX BattleResult_AddExp 0x801DD564, the sibling's
// name; read): for each party slot below the count 0x904AB0 (read again each
// pass) that BattleResult_MemberTakesExp answers for: the slot's character
// (+0x89) to its roster index (CharId_ToRosterIndex, read again for each of
// its calls), that character record's EXP dword (+0xC) + exp below 9999999
// (unsigned): exp added to the record the second call names; else that
// record's EXP set to 9999999. The slot counter is a byte stored over the
// caller's ecx slot: the word handed to the member test carries those upper
// bytes (masked by the callee). Answers exp (eax, on both paths).
extern "C" unsigned __cdecl BattleResult_AddExp(unsigned exp) {
    for (unsigned slot = 0; (slot & 0xFF) < B(::bof3::Img(rest_3b::at::kPartyCount.v)); slot = (slot + 1) & 0xFF) {
        const unsigned char takes = BH_CALL(BattleResult_MemberTakesExp)(slot);
        if (takes == 0) continue;
        const unsigned r1 = BH_CALL(CharId_ToRosterIndex)(PartyRest3b(slot)[0x89]) & 0xFF;
        const U sum = static_cast<U>(rv::As<records::CharRecord>(CharRecordRest3b(r1))->exp) + exp;
        if (sum < 9999999u) {
            const unsigned r2 = BH_CALL(CharId_ToRosterIndex)(PartyRest3b(slot)[0x89]) & 0xFF;
            unsigned char* const e = CharRecordRest3b(r2) + 0xC;
            move_script::SetLong(e, static_cast<S32Rest3b>(static_cast<U>(move_script::Long(e)) + exp));
        } else {
            const unsigned r3 = BH_CALL(CharId_ToRosterIndex)(PartyRest3b(slot)[0x89]) & 0xFF;
            rv::As<records::CharRecord>(CharRecordRest3b(r3))->exp = 9999999;
        }
    }
    return exp;
}

// original 0x446990: 0 when Battle_ActorIsOut(slot) (the word pushed whole,
// the callee reads its byte); else 1 when party record (slot & 0xFF)'s
// dword +0x134 has bit 10 clear. al only (the caller tests it).
extern "C" unsigned char __cdecl BattleResult_MemberTakesExp(unsigned slot) {
    if (BH_CALL(Battle_ActorIsOut)(slot) & 0xFF) return 0;
    return static_cast<unsigned char>(((rv::As<records::PartyMember>(PartyRest3b(slot))->flags_134 >> 10) & 1) ^ 1);
}

// original 0x4469D0 (PSX CharId_ToRosterIndex 0x801DD774, the sibling's
// name; read): the byte at 0x66972C + the id's low byte, 7 answered as 0.
// eax is the byte zero-extended (the callee's `and eax, 0xFF` before the
// load); BattleResult_FindLevelUp passes the whole of it on.
extern "C" unsigned __cdecl CharId_ToRosterIndex(unsigned id) {
    const unsigned r = B(::bof3::Img(rest_3b::at::kRosterOf.v) + (id & 0xFF));
    return r == 7 ? 0u : r;
}

// original 0x4469F0 (no PSX twin paired): a wrap-around index. value below
// `low` gives `high`, above `high` gives `low`, else itself - signed compares,
// whole dwords. 24 call sites, all in the item and skill menus' handlers after
// 0x447840, which push the result on to 0x445730 / 0x4457F0 or store its al.
extern "C" long __cdecl Battle_WrapIndex(long high, long low, long value) {
    if (value < low) return high;
    if (value <= high) return value;
    return low;
}

// original 0x446A10 (PSX 0x801DD7C8): the menu cursor's pulse. The byte counter
// kPulse steps by 2, down while kPulseDown is 1, up otherwise; reaching 0x1F
// or more sets the flag, 0 clears it (tested in that order, so 0 goes up).
// Byte arithmetic. The one caller (0x42EED0, before the menu's dispatch)
// reloads eax.
extern "C" void __cdecl Battle_PulseStep(void) {
    const unsigned char c = AtBattleMisc(::bof3::Img(kPulse.v))[0];
    if (c >= 0x1F) BattleMisc_PulseDown = 1;
    if (c == 0) {
        BattleMisc_PulseDown = 0;
        AtBattleMisc(::bof3::Img(kPulse.v))[0] = static_cast<unsigned char>(c + 2);
        return;
    }
    AtBattleMisc(::bof3::Img(kPulse.v))[0] = static_cast<unsigned char>(BattleMisc_PulseDown == 1 ? c - 2 : c + 2);
}

// original 0x446A50 (PSX 0x801DD820 Battle_PlayActorCue): Sound_PlayEffect of the
// u16 at kCueTable[kind + (cue & 0xFF) * 3], kind the word +0x2C of the record
// Field_State points at (the current party actor in battle; PSX 0x8014624C).
// The id is pushed in ecx, whose upper half is the record pointer's (mov cx,
// [..]); Sound_PlayEffect reads the low word. Returns its eax, which one of the
// thirteen callers (0x4FC079) returns on.
extern "C" unsigned long __cdecl Battle_PlayActorCue(unsigned cue) {
    const battle_misc::U record = LongBattleMisc(AtBattleMisc(battle_misc::at::Field_StateAt()));
    const battle_misc::U index = WordBattleMisc(AtBattleMisc(record + 0x2C)) + (cue & 0xFF) * 3;
    const battle_misc::U id = (record & 0xFFFF0000u) | WordBattleMisc(AtBattleMisc(::bof3::Img(kCueTable.v) + index * 2));
    return battle_misc::g.play_effect(id);
}

// original 0x446A80 (PSX Battle_WriteBackMember 0x801DD874, the sibling's
// name): a present member's (+0 bit 0) HP +0x98, +0x9A and +0x9C into its
// character record's +0x18, +0x1A, +0x1C; its +0x90 kept to bits 0x60A0 and
// copied to the record's +0x10. The member byte alone indexes (0..2 by its
// only caller, Battle_WriteBackParty).
extern "C" void __cdecl Battle_WriteBackMember(unsigned member) {
    auto* const member_rec = rv::As<records::PartyMember>(PartyBattleE4(member));
    if ((member_rec->flags & 1) == 0) return;
    unsigned char* const r = CharRecord(member_rec->char_index);
    auto* const char_rec = rv::As<records::CharRecord>(r);  // 0x903A70 + 0xA4 n
    static_assert(battle_e4::at::kCharStride == sizeof(records::CharRecord));
    rv::Store<unsigned>(char_rec->hp, member_rec->hp);
    rv::Store<unsigned>(char_rec->ap, member_rec->ap);
    const unsigned char c = member_rec->survive;
    member_rec->status = member_rec->status & 0x60A0;
    const unsigned s = member_rec->status;
    r[0x1C] = c;
    rv::Store<unsigned>(char_rec->status, s);
}

// original 0x446B00 (PSX 0x801DD940): the target 0x904B44 among the enemies not
// out (actors 3..10): first the highest u16 +0x98 - kept as a byte, compared
// with each word zero-extended, as the original has it - then, among those
// whose +0x98 equals that byte, the one with the lowest HP +0xA4 (the last of
// equals); 0 when none. battle_sprites' fallback when an action has no target.
extern "C" void __cdecl Battle_PickEnemyTarget(void) {
    unsigned char top = 0;
    for (unsigned a = 3; a <= 10; ++a) {
        if (BH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        const unsigned char* const e = EnemyBattleE4(a);
        auto* const enemy_rec = rv::AsConst<records::BattleEnemy>(e);
        if (top < enemy_rec->level) top = e[0x98];
    }
    unsigned lowest = 0xFFFF;
    unsigned char pick = 0;
    for (unsigned a = 3; a <= 10; ++a) {
        if (BH_CALL(Battle_ActorIsOut)(a) != 0) continue;
        auto* const enemy_rec = rv::As<records::BattleEnemy>(EnemyBattleE4(a));
        if (top != enemy_rec->level) continue;
        const unsigned hp = enemy_rec->hp;
        if (lowest < hp) continue;
        lowest = hp;
        pick = static_cast<unsigned char>(a);
    }
    B(::bof3::Img(battle_e4::at::kTarget.v)) = pick;
}

extern "C" __attribute__((naked)) unsigned long __cdecl Battle_StatusTint(unsigned) {
#if defined(__i386__)
    asm("testb $0x80, 4(%esp)\n\t"
        "jne " BOF3_SYM(BattleMisc_StatusTintBody) "\n\t"
        "ret");
#elif defined(__x86_64__)
    asm("testb $0x80, %cl\n\t"
        "jne " BOF3_SYM(BattleMisc_StatusTintBody) "\n\t"
        "ret");
#else
#error "Battle_StatusTint: no entry for this architecture"
#endif
}

// original 0x446BD0 (PSX 0x801DDAB8): a context for Sprite_Current's actor.
// slot = 0x435180(0, 6), al; the context's +0x80 = Sprite_Current (read after
// the call), +9 = 0, +0x34 / +0x38 / +0x3C the object's position dwords, each
// read just before it is stored. Its one caller (0x446C30) does not read eax.
extern "C" void __cdecl Battle_InitActorContext(void) {
    const battle_misc::U slot = battle_misc::g.alloc_context(0, 6);
    unsigned char* const context = AtBattleMisc(Context(slot));
    const battle_misc::U object = SpriteCurrent();
    PutLong(context + 0x80, object);
    context[9] = 0;
    PutLong(context + 0x34, LongBattleMisc(AtBattleMisc(object + 0x34)));
    PutLong(context + 0x38, LongBattleMisc(AtBattleMisc(object + 0x38)));
    PutLong(context + 0x3C, LongBattleMisc(AtBattleMisc(object + 0x3C)));
}

// original 0x446C30 (PSX 0x801DDB44): every actor's context, in actor order. The
// three party objects (ObjTrio, stride 0x14C) and the eight enemy objects
// (0x93B960, stride 0x128): one whose byte 0 has bit 0 becomes Sprite_Current and
// Field_State (party) or kCurrentEnemy (enemies) and gets Battle_InitActorContext;
// any other still takes a slot, 0x435180(0, 0), so slots stay in actor order.
extern "C" void __cdecl Battle_InitActorContexts(void) {
    for (battle_misc::U i = 0; i < 3; ++i) {
        const battle_misc::U object = battle_misc::at::ObjTrioAt() + i * kPartyStride;
        if (AtBattleMisc(object)[0] & 1) {
            SetSpriteCurrent(object);
            PutLong(AtBattleMisc(battle_misc::at::Field_StateAt()), object);
            battle_misc::g.init_context();
        } else {
            battle_misc::g.alloc_context(0, 0);
        }
    }
    for (battle_misc::U i = 0; i < 8; ++i) {
        const battle_misc::U object = ::bof3::Img(Battle_Enemies_addr) + i * kEnemyStride;
        if (AtBattleMisc(object)[0] & 1) {
            SetSpriteCurrent(object);
            BattleE3_CurrentEnemy = reinterpret_cast<records::BattleEnemy*>(static_cast<std::uintptr_t>(object));
            battle_misc::g.init_context();
        } else {
            battle_misc::g.alloc_context(0, 0);
        }
    }
}

// original 0x446CB0 (PSX 0x801DDC7C): the status counter's roll (a member's
// +0x12D, an enemy's +0x10D). 0 at 0. The percentage 75 from 3 on, else
// 0x64E3E8's by the counter: 1 when it is not below Rand() % 100. Else 1 when
// (200 - 0x64E3E0's by the actor's +0xB6 / +0xC6) * 50 is not below Rand() %
// 10000.
extern "C" unsigned char __cdecl Battle_WakeRoll(unsigned actor) {
    const unsigned a = Low(actor);
    const unsigned char count = a <= 2 ? PartyBattleE4(a)[0x12D] : rv::As<records::BattleEnemy>(EnemyBattleE4(a))->counter;
    if (count == 0) return 0;
    const int chance = count >= 3 ? 0x4B : B(::bof3::Img(battle_e4::at::kWakeChance.v) + count);
    if (chance >= BH_CALL(Rand)() % 100) return 1;
    const unsigned level = a <= 2 ? PartyBattleE4(a)[0xB6] : EnemyBattleE4(a)[0xC6];
    const int roll = BH_CALL(Rand)() % 10000;
    return (200 - static_cast<int>(B(::bof3::Img(battle_e4::at::kWakeResist.v) + level))) * 50 >= roll ? 1 : 0;
}

// original 0x446D90 (PSX 0x801DDE44): an item word (category << 8 | item)
// given back to its inventory slot: the category's count at the slot, 1 at 99
// (0x63) and nothing else; otherwise one more and the id put at the slot, 0.
// The category indexes the list pointers unchecked (4's count pointer is 0).
extern "C" unsigned char __cdecl Battle_ReturnItem(unsigned slot, unsigned item) {
    const unsigned s = Low(slot);
    const unsigned category = (item & 0xFFFF) >> 8;
    unsigned char* const counts = Ptr(::bof3::Img(battle_e4::at::kInventoryCounts.v) + 4 * category);
    const unsigned char c = counts[s];
    if (c == 0x63) return 1;
    counts[s] = static_cast<unsigned char>(c + 1);
    Ptr(::bof3::Img(battle_e4::at::kInventoryIds.v) + 4 * category)[s] = static_cast<unsigned char>(item);
    return 0;
}

// original 0x446DE0 (no PSX twin in the pairs): the end phase (0x904AA0 = 5)
// at BattleEnd_Steps' step 1 (0x904AA1), 0x904AA2 = 0. The boss hooks'
// way out on a win (round eleven's 0x446DE0).
extern "C" void __cdecl BattleEnd_EnterStep1(void) {
    B(::bof3::Img(battle_e4::at::kPhase.v)) = 5;
    BattleActions_Step = 1;
    BattleE4_Step2 = 0;
}

// original 0x446E00: the same at step 2.
extern "C" void __cdecl BattleEnd_EnterStep2(void) {
    B(::bof3::Img(battle_e4::at::kPhase.v)) = 5;
    BattleActions_Step = 2;
    BattleE4_Step2 = 0;
}

// original 0x446E20: the same at step 3.
extern "C" void __cdecl BattleEnd_EnterStep3(void) {
    B(::bof3::Img(battle_e4::at::kPhase.v)) = 5;
    BattleActions_Step = 3;
    BattleE4_Step2 = 0;
}

// original 0x446E40 (no PSX twin paired): the list kSoundLists[set & 0xFF] -
// none: al 1. Walk its dwords from the first until one whose high half is
// key & 0xFF or one that is -1 (the -1 is tested first, the key on the high half
// shifted down, logically); a -1 there: al 1; else Sound_LoadStream(its low half
// + 0x1000), al 0. The four callers test al.
extern "C" unsigned char __cdecl Battle_LoadSoundByKey(unsigned key, unsigned set) {
    battle_misc::U p = LongBattleMisc(AtBattleMisc(::bof3::Img(kSoundLists.v) + (set & 0xFF) * 4));
    if (p == 0) return 1;
    battle_misc::U e = LongBattleMisc(AtBattleMisc(p));
    if (e != 0xFFFFFFFFu) {
        const battle_misc::U want = key & 0xFF;
        while ((e >> 16) != want) {
            e = LongBattleMisc(AtBattleMisc(p + 4));
            p += 4;
            if (e == 0xFFFFFFFFu) break;
        }
    }
    const battle_misc::U entry = LongBattleMisc(AtBattleMisc(p));
    if (entry == 0xFFFFFFFFu) return 1;
    battle_misc::g.load_stream((entry & 0xFFFF) + 0x1000);
    return 0;
}

// original 0x446EA0 (PSX 0x801DDFB4, the same): for a member (above 2,
// nothing) found among the queue bytes 0x904ACC + (0x904AE2 - 1) up to
// 0x904AE3 (bytes, the first index 0xFF when 0x904AE2 is 0 - then nothing),
// whose queued action is an item (+0x125 = 5) with a category byte of 0
// (+0x127): 0x446D90(slot +0x12E, item word +0x126) gives it back. Not kept:
// the original leaves the loop index in its own argument slot, and eax as
// it falls; its callers pop the slot and put al over eax (the one callee they
// pass it to, 0x446650, reads its low byte - checked at all 12 call sites).
extern "C" void __cdecl Battle_ReturnQueuedItem(unsigned actor) {
    const unsigned a = actor & 0xFF;
    if (a > 2) return;
    const unsigned to = BattleDamage_TurnCount;
    unsigned i = static_cast<unsigned char>(BossSe_RoundSlot - 1);
    for (;;) {
        if (i >= to) return;
        if (move_script::At(::bof3::Img(battle_setup::at::kItemQueue.v) + i)[0] == a) break;
        ++i;
    }
    unsigned char* const o = P(a);
    if (o[battle_setup::at::kPQueuedKind] != 5) return;
    if (o[battle_setup::at::kPQueuedItem + 1] != 0) return;
    battle_setup::g.return_item(o[battle_setup::at::kPItemSlot], rv::As<records::PartyMember>(o)->queued_id);
}

// original 0x446F20 (PSX 0x801DE074, paired; unnamed there): clamped to 0..999.
extern "C" int __cdecl Stat_PercentCap999(int value, int percent) { return Percent(value, percent, 999); }

// original 0x446F50 (PSX 0x801DE0C0): clamped to 0..9999.
extern "C" int __cdecl Stat_PercentCap9999(int value, int percent) { return Percent(value, percent, 9999); }

// original 0x446F80 (PSX 0x801DE10C): clamped to 0..100.
extern "C" int __cdecl Stat_PercentCap100(int value, int percent) { return Percent(value, percent, 100); }

// originals 0x446FB0 (PSX 0x801DE158) and 0x446FD0 (PSX 0x801DE178
// Battle_ClearActorBit): bit (actor & 31) of the u16 kActorBits set or cleared -
// the x86 shift count, as the PSX sllv's. eax is 1 << n, or its complement,
// which callers such as 0x42FBE0 push on with a word written over the low half.
extern "C" unsigned long __cdecl Battle_SetActorBit(unsigned actor) {
    const battle_misc::U bit = 1u << (actor & 31);
    BattleActions_Pending = static_cast<std::uint32_t>(BattleActions_Pending) | bit;
    return bit;
}

extern "C" unsigned long __cdecl Battle_ClearActorBit(unsigned actor) {
    const battle_misc::U mask = ~(1u << (actor & 31));
    BattleActions_Pending = static_cast<std::uint32_t>(BattleActions_Pending) & mask;
    return mask;
}

// original 0x446FF0 (PSX 0x801DE19C): a copy of every actor able to act, for 11
// actors: the party's while index <= 2 (object present, and its +0x134 without
// bit 3), the enemies' after (0x4456C0(index) al zero, and the object's +0x114
// without bit 3). Each gets slot = 0x435180(0, 7); Sprite_Current = the
// object (stored after the call); the context's first 0x80 bytes the object's,
// copied a dword at a time forward (rep movsd); +0x80 the object; then +6 = 0,
// +5 = 7, +1 = 0, +2 = 0, +0x29 = 3, +0x5C..+0x5F = 0. The party walk steps
// on past index 2 unused. Returns the original's eax, which its one caller
// (0x42E9C7, a state of the battle task) returns on: the last actor's (index 10)
// context pointer, or 0x4456C0's eax when that actor was passed over, with
// al = 11.
extern "C" unsigned long __cdecl Battle_SpawnActorCopies(void) {
    battle_misc::U eax = 0;
    for (battle_misc::U index = 0; index <= 10; ++index) {
        battle_misc::U object;
        if (index <= 2) {
            object = battle_misc::at::ObjTrioAt() + index * kPartyStride;
            if (!(AtBattleMisc(object)[0] & 1) || (AtBattleMisc(object + 0x134)[0] & 8)) continue;
        } else {
            object = ::bof3::Img(Battle_Enemies_addr) + (index - 3) * kEnemyStride;
            eax = battle_misc::g.actor_out(index);
            if ((eax & 0xFF) != 0 || (AtBattleMisc(object + 0x114)[0] & 8)) continue;
        }
        const battle_misc::U slot = battle_misc::g.alloc_context(0, 7);
        SetSpriteCurrent(object);
        const battle_misc::U context = Context(slot);
        for (battle_misc::U k = 0; k < 0x80; k += 4) PutLong(AtBattleMisc(context + k), LongBattleMisc(AtBattleMisc(object + k)));
        PutLong(AtBattleMisc(context + 0x80), object);
        unsigned char* const c = AtBattleMisc(context);
        c[6] = 0;
        c[5] = 7;
        c[1] = 0;
        c[2] = 0;
        c[0x29] = 3;
        c[0x5C] = 0;
        c[0x5D] = 0;
        c[0x5F] = 0;
        c[0x5E] = 0;
        eax = context;
    }
    return (eax & 0xFFFFFF00u) | 11;
}

// original 0x447110 (the catalogue pairs it with PSX 0x80093A74, not read):
// entry 0x904AA3 of BattleTarget_Steps 0x64E3EC - 0x447120, 0x447140.
extern "C" void __cdecl BattleTarget_Dispatch(void) { Dispatch(BattleTarget_Steps, BattleE4_Step3); }

// original 0x447120: BattleBanner_SetMessage(0, 0) - message 0, the prompt -
// then the step counted on (read after the call).
extern "C" void __cdecl BattleTarget_ShowPrompt(void) {
    battle_actor_copies::g.set_message(0, 0);
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

void Battle3_Inject() {
    BOF3_INJECT(BattleWin_DrawPartyStatus);
    BOF3_INJECT(BattleWin_DrawCommandCross);
    BOF3_INJECT(BattleWin_DrawCommandIcon);
    BOF3_INJECT(BattleWin_DrawMessageBox);
    BOF3_INJECT(BattleWin_DrawSmallBox);
    BOF3_INJECT(BattleWin_DrawMediumBox);
    BOF3_INJECT(BattleWin_DrawCommandLabel);
    BOF3_INJECT(BattleWin_DrawTargetEnemy);
    BOF3_INJECT(BattleWin_DrawEnemyStatus);
    BOF3_INJECT(BattleWin_DrawTargetMember);
    BOF3_INJECT(BattleWin_OpenStatus);
    BOF3_INJECT(BattleWin_OpenSub1);
    BOF3_INJECT(BattleWin_OpenSub2);
    BOF3_INJECT(BattleWin_OpenSub3);
    BOF3_INJECT(Battle_OpenMsgWindow);
    BOF3_INJECT(BattleWin_DrawNumber);
    BOF3_INJECT(Battle_DrawNumber);
    BOF3_INJECT(Battle_DrawLabel);
    BOF3_INJECT(BattleWin_DimScreen);
    BOF3_INJECT(BattleWin_DrawTileTint);
    BOF3_INJECT(BattleWin_DrawQuadF4);
    BOF3_INJECT(BattleWin_DrawTile);
    BOF3_INJECT(BattleWin_DrawTileRgb);
    BOF3_INJECT(BattleWin_DrawBar);
    BOF3_INJECT(BattleWin_DrawCell16);
    BOF3_INJECT(BattleWin_DrawLine);
    BOF3_INJECT(BattleWin_DrawLineHalf);
    BOF3_INJECT(BattleWin_DrawLineAdd);
    BOF3_INJECT(BattleWin_FirstOfKind);
    BOF3_INJECT(Battle_BuildEntryOrder);
    BOF3_INJECT(Battle_BuildTurnOrder);
    BOF3_INJECT(Battle_MarkFasterSide);
    BOF3_INJECT(Battle_ActorStanding);
    BOF3_INJECT(Battle_PartyOutpaces);
    BOF3_INJECT(Battle_EnemyOutpaces);
    BOF3_INJECT(Battle_LevelClass);
    BOF3_INJECT(Battle_ClearCommands);
    BOF3_INJECT(Battle_ActorIsOut);
    BOF3_INJECT(Battle_DefaultTarget);
    BOF3_INJECT(Battle_PrevTarget);
    BOF3_INJECT(Battle_ActorCanCommand);
    BOF3_INJECT(Battle_ActorCanAct);
    BOF3_INJECT(Battle_ApplyDamage);
    BOF3_INJECT(Battle_CalcDamage);
    BOF3_INJECT(Battle_HitOrMissParty);
    BOF3_INJECT(Battle_HitOrMissEnemy);
    BOF3_INJECT(Battle_BaseDamage);
    BOF3_INJECT(Battle_PartyDefenceMean);
    BOF3_INJECT(Battle_ScaleDamage);
    BOF3_INJECT(Battle_SetHpChange);
    BOF3_INJECT(Battle_ReloadPartyRecords);
    BOF3_INJECT(Battle_RemoveFromTurnOrder);
    BOF3_INJECT(Battle_OrderPushFront);
    BOF3_INJECT(AutoBattle_FillCommands);
    BOF3_INJECT(Battle_TurnVectorC);
    BOF3_INJECT(Battle_TurnVector18);
    BOF3_INJECT(Battle_MemberReactRoll);
    BOF3_INJECT(BattleResult_AddExp);
    BOF3_INJECT(BattleResult_MemberTakesExp);
    BOF3_INJECT(CharId_ToRosterIndex);
    BOF3_INJECT(Battle_WrapIndex);
    BOF3_INJECT(Battle_PulseStep);
    BOF3_INJECT(Battle_PlayActorCue);
    BOF3_INJECT(Battle_WriteBackMember);
    BOF3_INJECT(Battle_PickEnemyTarget);
    BOF3_INJECT(Battle_StatusTint);
    BOF3_INJECT(Battle_InitActorContext);
    BOF3_INJECT(Battle_InitActorContexts);
    BOF3_INJECT(Battle_WakeRoll);
    BOF3_INJECT(Battle_ReturnItem);
    BOF3_INJECT(BattleEnd_EnterStep1);
    BOF3_INJECT(BattleEnd_EnterStep2);
    BOF3_INJECT(BattleEnd_EnterStep3);
    BOF3_INJECT(Battle_LoadSoundByKey);
    BOF3_INJECT(Battle_ReturnQueuedItem);
    BOF3_INJECT(Stat_PercentCap999);
    BOF3_INJECT(Stat_PercentCap9999);
    BOF3_INJECT(Stat_PercentCap100);
    BOF3_INJECT(Battle_SetActorBit);
    BOF3_INJECT(Battle_ClearActorBit);
    BOF3_INJECT(Battle_SpawnActorCopies);
    BOF3_INJECT(BattleTarget_Dispatch);
    BOF3_INJECT(BattleTarget_ShowPrompt);
}
