// The battle windows' states (the eighth round's group CL).
// docs/battle_win_states.md.
//
// Nineteen functions the owner's combat route reaches, none of them ever a
// `call rel32` target: record handler 3 of Field_RunTaskRecords (0x596FA0,
// the immediate at 0x59E25C), which runs a battle window by its kind byte +2
// from an eight-entry table built on its own stack, and the six kinds' state
// dispatchers and states under it, each reached through another such table
// (`mov [esp + 4 i], imm32`, then `call [esp + index * 4]`). Each was read to
// its last instruction (2026-09-25); the catalogue's "jumped to" is the
// return address after `call [esp + eax * 4]`, which pe_funcs.py does not
// count as a call.
//
// Every call goes through battle_win_states::g (battle_win_states_callees.h)
// so that the start-up fuzz can stand recorders in for them - for ours and
// for the originals' copies alike. Where the original reads memory after a
// call, ours reads it after the same call; the current window record
// 0x905B84 is re-read where the original re-reads it.
//
// Upper halves. The originals push coordinates as 16-bit registers whose
// upper halves are whatever the register held, and bytes likewise; every
// callee here reads only the low 16 bits of a coordinate and the low byte of
// a slot or an icon (docs/battle_windows.md section 2 for the BC draws;
// Text_DrawAt stores word x and y; Menu_DrawIcon and Battle_ActorIsOut take
// low bytes), so ours passes the value as C++ computes it. The one argument
// whose upper bits are the original's arithmetic and not garbage - the banner
// text's colour, pushed as eax with the record offset's upper bits - ours
// computes the same way.
#include "game/battle_win_states.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/battle_win_states_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_win_states {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

const Callees kOriginals = {
    BattleWin_DrawPartyStatus,
    BattleWin_DrawCommandCross,
    BattleWin_DrawCommandLabel,
    BattleWin_DrawSmallBox,
    BattleWin_DrawMediumBox,
    BattleWin_DrawTargetEnemy,
    BattleWin_DrawEnemyStatus,
    BattleWin_DrawTargetMember,
    Window_FlagAndAdvance,
    Window_RestoreAndBack,
    Window_DispatchKind,
    Text_GlyphCount,
    Text_DrawAt,
    Menu_DrawIcon,
    Battle_ActorIsOut,
    Battle_ReturnTrue,
    {at::kKinds[0], at::kKinds[1], at::kKinds[2], at::kKinds[3], at::kKinds[4], at::kKinds[5], at::kKinds[6],
     at::kKinds[7]},
    {at::kPartyStates[0], at::kPartyStates[1], at::kPartyStates[2]},
    {at::kCrossStates[0], at::kCrossStates[1], at::kCrossStates[2]},
    {at::kLabelStates[0], at::kLabelStates[1], at::kLabelStates[2]},
    {at::kBannerStates[0], at::kBannerStates[1], at::kBannerStates[2]},
    {at::kEnemyStates[0], at::kEnemyStates[1], at::kEnemyStates[2], at::kEnemyStates[3]},
    {at::kMemberStates[0], at::kMemberStates[1], at::kMemberStates[2]},
};
Callees g = kOriginals;

}  // namespace battle_win_states

using namespace battle_win_states;

namespace {

std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Current() { return At(static_cast<std::uint32_t>(Long(At(at::kCurrent)))); }
const unsigned char* Target() { return At(static_cast<std::uint32_t>(Long(At(at::kTarget)))); }
bool Choosing() { return At(at::kChoosing)[0] != 0; }
int S8(std::uint32_t address) { return static_cast<signed char>(At(address)[0]); }

// Enemy slot t's record: 0x93B9E0 + 0x128 * (t - 3), the subtraction at full
// width (0x597320, 0x597400 - `sub eax, 3` on the zero-extended byte).
unsigned char* Enemy(unsigned slot) { return At(at::kEnemies + at::kEnemyStride * (slot - 3u)); }
// A party member's battle object, ObjTrio 0x802D40 + 0x14C * m.
unsigned char* Member(unsigned m) { return At(at::kMembers + at::kMemberStride * m); }

using Handler = void (__cdecl*)();
void Run(std::uint32_t handler) { reinterpret_cast<Handler>(static_cast<std::uintptr_t>(handler))(); }

// A state or kind byte past its stack table: the original calls through the
// words above the table on its own stack - its return address first - which
// cannot be reproduced; ours aborts (CLAUDE.md rule 4, as
// Battle_PhaseDispatch and Field_RunTaskRecords do). docs/battle_win_states.md
// section 4.
[[noreturn]] void PastTable(const char* who, const char* byte, unsigned value, unsigned entries) {
    bof3::Fatal("%s: the current window's %s byte is %u; the original's stack table holds %u", who, byte, value,
                entries);
}

// The gauge's length: 55 * value / top as the original's `idiv` (both words
// zero-extended, so signed and unsigned agree), its low byte kept. A top of 0
// is the original's divide fault; ours aborts loudly instead.
unsigned char Gauge(unsigned value, unsigned top, const char* who) {
    if (top == 0) bof3::Fatal("%s: a gauge of %u over a top of 0 - the original's idiv faults here", who, value);
    return static_cast<unsigned char>((55u * value) / top);
}

// The low-HP flag every gauge state leaves after its step: bit 13 of the
// status word set (as `or byte [+1], 0x20`) while value < top / 4, cleared
// (as `and word, 0xDFFF`) otherwise.
void LowFlag(unsigned char* status, unsigned value, unsigned top) {
    if (value < (top >> 2))
        status[1] = static_cast<unsigned char>(status[1] | 0x20);
    else
        SetWord(status, Word(status) & 0xDFFFu);
}

// The target banner's x offset: 0x64E2BC for side bytes 0 and 3, else 0x64E2BE.
int SideDx(unsigned side) { return (side == 0 || side == 3) ? S8(at::kSideDx) : S8(at::kOtherDx); }

}  // namespace

// ===========================================================================
// The record handler and the kinds' dispatchers

// original 0x596FA0: record handler 3 of Field_RunTaskRecords - the battle
// window by its kind byte +2, from eight handlers on its stack: 0 the party
// row 0x597000, 1 the command cross 0x597090, 2 the command label 0x5971B0,
// 3 the action banner 0x597200, 4 an enemy's gauge 0x597320, 5 a member's
// gauges 0x5975D0, 6 and 7 group CM's 0x597C70 and 0x597D50.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_Run() {
    const unsigned kind = Current()[2];
    if (kind >= 8) PastTable("BattleWin_Run", "kind", kind, 8);
    Run(g.kinds[kind]);
}

// original 0x597000: kind 0 (the party row) by its state +3: 0x597030,
// 0x597070, 0x437CC0 (idle).
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_PartyRowStates() {
    const unsigned state = Current()[3];
    if (state >= 3) PastTable("BattleWin_PartyRowStates", "state", state, 3);
    Run(g.party[state]);
}

// original 0x597090: kind 1 (the command cross): 0x5970C0, 0x597160, idle.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_CrossStates() {
    const unsigned state = Current()[3];
    if (state >= 3) PastTable("BattleWin_CrossStates", "state", state, 3);
    Run(g.cross[state]);
}

// original 0x5971B0: kind 2 (the command label): idle, 0x5971E0, idle.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_LabelStates() {
    const unsigned state = Current()[3];
    if (state >= 3) PastTable("BattleWin_LabelStates", "state", state, 3);
    Run(g.label[state]);
}

// original 0x597200: kind 3 (the action banner): idle, 0x597230, idle.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_BannerStates() {
    const unsigned state = Current()[3];
    if (state >= 3) PastTable("BattleWin_BannerStates", "state", state, 3);
    Run(g.banner[state]);
}

// original 0x597320: kind 4 (an enemy's gauge, slot +0xA = 3..8): its state
// (0x597400, 0x5974C0, 0x597510, idle), then every frame the gauge's step
// through Window_DispatchKind - (+0xB, the enemy's top +0x30, +0x1C, +0x14,
// its HP +0x24, +0xD, +0x18, +8: the kind byte +8 picks the handler); when
// Battle_ActorIsOut(slot) and +0xD is 0 the state becomes 3 (idle); last the
// enemy's low-HP flag. The record is re-read after each call.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_EnemyGaugeStates() {
    const unsigned state = Current()[3];
    if (state >= 4) PastTable("BattleWin_EnemyGaugeStates", "state", state, 4);
    Run(g.enemy[state]);
    unsigned char* w = Current();
    unsigned char* e = Enemy(w[0xA]);
    g.dispatch_kind(Addr(w + 0xB), Addr(e + 0x30), Addr(w + 0x1C), Addr(w + 0x14), Addr(e + 0x24), Addr(w + 0xD),
                    Addr(w + 0x18), Addr(w + 8));
    w = Current();
    if (g.actor_is_out(w[0xA]) != 0) {
        w = Current();
        if (w[0xD] == 0) w[3] = 3;
    }
    w = Current();
    e = Enemy(w[0xA]);
    LowFlag(e + 0x12, Word(e + 0x24), Word(e + 0x30));
}

// original 0x5975D0: kind 5 (a member's gauges, slot +0xA = 0..2): its state
// (0x5976D0, 0x597850, 0x5978B0 - three, no idle entry), then the HP gauge's
// step (+0xB, top +0xA0, +0x1C, +0x14, HP +0x98, +0xD, +0x18, +8) and the AP
// gauge's (+0xC, +0xA2, +0x1E, +0x16, +0x9A, +0xE, +0x1A, +9) through
// Window_DispatchKind, then the member's low-HP flag (+0x90 bit 13). The
// record is re-read after each call.
extern "C" __attribute__((disable_tail_calls)) void __cdecl BattleWin_MemberGaugeStates() {
    const unsigned state = Current()[3];
    if (state >= 3) PastTable("BattleWin_MemberGaugeStates", "state", state, 3);
    Run(g.member[state]);
    unsigned char* w = Current();
    unsigned char* m = Member(w[0xA]);
    g.dispatch_kind(Addr(w + 0xB), Addr(m + 0xA0), Addr(w + 0x1C), Addr(w + 0x14), Addr(m + 0x98), Addr(w + 0xD),
                    Addr(w + 0x18), Addr(w + 8));
    w = Current();
    m = Member(w[0xA]);
    g.dispatch_kind(Addr(w + 0xC), Addr(m + 0xA2), Addr(w + 0x1E), Addr(w + 0x16), Addr(m + 0x9A), Addr(w + 0xE),
                    Addr(w + 0x1A), Addr(w + 9));
    w = Current();
    m = Member(w[0xA]);
    LowFlag(m + 0x90, Word(m + 0x98), Word(m + 0xA0));
}

// ===========================================================================
// Kind 0: the party row

// original 0x597030: state 0 - the row slides up 4 a frame to y 0xC8 (a
// signed word compare); on arriving, the state steps on. Drawn every frame.
extern "C" void __cdecl BattleWin_PartyRowSlideIn() {
    unsigned char* w = Current();
    const auto y = static_cast<std::int16_t>(Word(w + 6));
    if (y <= 0xC8) {
        SetWord(w + 6, 0xC8);
        ++Current()[3];
    } else {
        SetWord(w + 6, static_cast<unsigned>(y - 4));
    }
    w = Current();
    g.party_status(Word(w + 4), Word(w + 6));
}

// original 0x597070: state 1 - the row drawn where it is.
extern "C" void __cdecl BattleWin_PartyRowDraw() {
    const unsigned char* const w = Current();
    g.party_status(Word(w + 4), Word(w + 6));
}

// ===========================================================================
// Kind 1: the command cross

// original 0x5970C0: state 0 - the five arms' icons grow in. Arm i's growth
// g (0x904ABC + i, read before its icon) draws icon i through Menu_DrawIcon at
// the window's +4 / +6 plus the arm's point (0x64E2AC) plus g / 2, 0x10 - g
// square, shade 0x80; the record re-read per arm. Then, when arm 0's growth
// (re-read) is 0, the state steps on; else all seven growths lose 1.
extern "C" void __cdecl BattleWin_CrossGrow() {
    for (unsigned i = 0; i < 5; ++i) {
        const unsigned grow = At(at::kCrossGrow + i)[0];
        const unsigned size = (0x10u - grow) & 0xFFu;
        const unsigned half = grow >> 1;
        const unsigned char* const arm = At(at::kCrossArms + 2 * i);
        const unsigned char* const w = Current();
        const unsigned y = Word(w + 6) + arm[1] + half;
        const unsigned x = Word(w + 4) + arm[0] + half;
        g.icon(i, x, y, size, size, 0x80);
    }
    if (At(at::kCrossGrow)[0] == 0) {
        ++Current()[3];
        return;
    }
    for (unsigned i = 0; i < 7; ++i) --At(at::kCrossGrow + i)[0];
}

// original 0x597160: state 1 - the selected arm's growth (the selection
// 0x904AB4 re-read per arm) up by 2 while below 8, every other arm's down by
// 2 while not 0 (a growth of 1 wraps to 0xFF, as the original's); then the
// cross drawn at the window's +4 / +6.
extern "C" void __cdecl BattleWin_CrossFrame() {
    for (unsigned i = 0; i < 7; ++i) {
        const unsigned sel = At(at::kCrossSel)[0];
        unsigned char* const grow = At(at::kCrossGrow + i);
        if (sel == i) {
            if (*grow < 8) *grow = static_cast<unsigned char>(*grow + 2);
        } else if (*grow != 0) {
            *grow = static_cast<unsigned char>(*grow - 2);
        }
    }
    const unsigned char* const w = Current();
    g.command_cross(Word(w + 4), Word(w + 6));
}

// ===========================================================================
// Kind 2: the command label

// original 0x5971E0: state 1 - the selected command's label banner, unless
// the byte 0x903A5D is set.
extern "C" void __cdecl BattleWin_LabelFrame() {
    if (At(at::kLabelGate)[0] != 0) return;
    g.command_label(At(at::kCrossSel)[0]);
}

// ===========================================================================
// Kind 3: the action banner

// original 0x597230 (PSX 0x801E9470): state 1 - banner record k = +0xA of the
// pool 0x93B8E0 (12 bytes a record): its text (+4) counted by
// Text_GlyphCount; then, record re-read, a wide banner (+2 set) is the medium
// box at (x - 0x10, y) and the text at x + 0x26 - 6 n, 0x12 characters; a
// narrow one the small box at (x, y) and the text at x + 6 (6 - n), 8
// characters; y + 3, colour +0xA, through Text_DrawAt. The record is re-read
// after the count and after the box.
extern "C" void __cdecl BattleWin_BannerFrame() {
    unsigned char* w = Current();
    const auto text = [](std::uint32_t off) {
        return At(static_cast<std::uint32_t>(Long(At(at::kBanners + 4 + off))));
    };
    const unsigned n = g.glyph_count(text(at::kBannerStride * w[0xA]));
    w = Current();
    const std::uint32_t off = at::kBannerStride * w[0xA];
    const bool wide = At(at::kBanners + 2 + off)[0] != 0;
    const unsigned x = Word(w + 4), y = Word(w + 6);
    if (wide) {
        g.medium_box(static_cast<std::uint16_t>(x - 0x10), y);
    } else {
        g.small_box(x, y);
    }
    w = Current();
    const std::uint32_t o = at::kBannerStride * w[0xA];
    // eax: the record offset with its low byte replaced by the colour
    const unsigned colour = (o & ~0xFFu) | At(at::kBanners + 0xA + o)[0];
    const unsigned ty = static_cast<std::uint16_t>(Word(w + 6) + 3);
    if (wide) {
        const unsigned tx = static_cast<std::uint16_t>(Word(w + 4) - 6 * n + 0x26);
        g.text_draw_at(static_cast<int>(tx), static_cast<int>(ty), static_cast<int>(colour), 0x12, text(o));
    } else {
        const unsigned tx = static_cast<std::uint16_t>(Word(w + 4) + 6 * (6 - n));
        g.text_draw_at(static_cast<int>(tx), static_cast<int>(ty), static_cast<int>(colour), 8, text(o));
    }
}

// ===========================================================================
// Kind 4: an enemy's gauge

// original 0x597400: state 0 - the gauge set up from the enemy's HP +0x24 and
// top +0x30: +0xB the length (55 HP / top), at least 1 while HP is not 0;
// +0x14 the HP, +0x1C the top, +0xD 0; the state steps on.
extern "C" void __cdecl BattleWin_EnemyGaugeOpen() {
    unsigned char* w = Current();
    unsigned char* e = Enemy(w[0xA]);
    w[0xB] = Gauge(Word(e + 0x24), Word(e + 0x30), "BattleWin_EnemyGaugeOpen");
    w = Current();
    if (Word(Enemy(w[0xA]) + 0x24) != 0 && w[0xB] == 0) {
        w[0xB] = 1;
        w = Current();
    }
    SetWord(w + 0x14, Word(Enemy(w[0xA]) + 0x24));
    w = Current();
    SetWord(w + 0x1C, Word(Enemy(w[0xA]) + 0x30));
    Current()[0xD] = 0;
    ++Current()[3];
}

// original 0x5974C0: state 1 - the enemy's banner (BattleWin_DrawEnemyStatus
// at +4 / +6 for the slot); while a target is chosen (0x904AAF), a target byte
// of this slot steps the state on (Window_FlagAndAdvance), and one with 0x40
// (all enemies) steps it on too - both are tested, the target re-read.
extern "C" void __cdecl BattleWin_EnemyGaugeFrame() {
    const unsigned char* w = Current();
    g.enemy_status(Word(w + 4), Word(w + 6), w[0xA]);
    if (!Choosing()) return;
    const unsigned char t = Target()[0];
    if (t == Current()[0xA]) g.flag_advance();
    if (Target()[0] & 0x40) g.flag_advance();
}

// original 0x597510: state 2 - the enemy's target banner. Its place: the
// enemy's s8 +0x72 / +0x73 plus the words at -0x52 / -0x50, x plus the side
// offset of the byte at -0x78 (0x64E2BC for 0 and 3, else 0x64E2BE), y plus
// 0xC; the slot taken as a byte, (+0xA - 3) & 0xFF. Drawn by
// BattleWin_DrawTargetEnemy; then, unless a target is being chosen and it is
// this slot (no side bits) or all enemies (0x40), Window_RestoreAndBack.
extern "C" void __cdecl BattleWin_EnemyTargetFrame() {
    unsigned char* w = Current();
    const std::uint32_t off = at::kEnemyStride * ((w[0xA] - 3u) & 0xFFu);
    const std::uint32_t e = at::kEnemies + off;
    const int dx = SideDx(At(e - 0x78)[0]);
    SetWord(w + 4, static_cast<unsigned>(S8(e + 0x72) + Word(At(e - 0x52)) + dx));
    const unsigned y = static_cast<unsigned>(S8(e + 0x73) + Word(At(e - 0x50)) + 0xC);
    SetWord(Current() + 6, y);
    w = Current();
    g.target_enemy(Word(w + 4), Word(w + 6), w[0xA]);
    if (!Choosing()) {
        g.restore_back();
        return;
    }
    const unsigned char t = Target()[0];
    if ((t & 0xC0) == 0) {
        if (t == Current()[0xA]) return;
    } else if (t & 0x40) {
        return;
    }
    g.restore_back();
}

// ===========================================================================
// Kind 5: a member's gauges

// original 0x5976D0: state 0 - both gauges set up: HP (+0x98 over +0xA0)
// into +0xB (at least 1 while HP is not 0), +0x14, +0x1C, +0xD 0; AP (+0x9A
// over +0xA2) into +0xC - 0 when the top is 0, else at least 1 while AP is
// not 0 - +0x16, +0x1E, +0xE 0; the state steps on.
extern "C" void __cdecl BattleWin_MemberGaugeOpen() {
    unsigned char* w = Current();
    unsigned char* m = Member(w[0xA]);
    w[0xB] = Gauge(Word(m + 0x98), Word(m + 0xA0), "BattleWin_MemberGaugeOpen");
    w = Current();
    if (Word(Member(w[0xA]) + 0x98) != 0 && w[0xB] == 0) {
        w[0xB] = 1;
        w = Current();
    }
    SetWord(w + 0x14, Word(Member(w[0xA]) + 0x98));
    w = Current();
    SetWord(w + 0x1C, Word(Member(w[0xA]) + 0xA0));
    Current()[0xD] = 0;
    unsigned char* const v = Current();
    m = Member(v[0xA]);
    const unsigned top = Word(m + 0xA2);
    if (top == 0) {
        v[0xC] = 0;
        w = Current();
    } else {
        v[0xC] = static_cast<unsigned char>((55u * Word(m + 0x9A)) / top);
        w = Current();
        if (Word(Member(w[0xA]) + 0x9A) != 0 && w[0xC] == 0) {
            w[0xC] = 1;
            w = Current();
        }
    }
    SetWord(w + 0x16, Word(Member(w[0xA]) + 0x9A));
    w = Current();
    SetWord(w + 0x1E, Word(Member(w[0xA]) + 0xA2));
    Current()[0xE] = 0;
    ++Current()[3];
}

// original 0x597850: state 1 - nothing drawn. While a target is chosen: when
// the member is out (Battle_ActorIsOut), only if Battle_ReturnTrue answers (a
// second try when the first says 0); then a target byte of this slot steps
// the state on, and one with 0x80 (all members) steps it on too.
extern "C" void __cdecl BattleWin_MemberGaugeWait() {
    if (!Choosing()) return;
    if (g.actor_is_out(Current()[0xA]) != 0) {
        if (g.return_true() == 0 && g.return_true() == 0) return;
    }
    const unsigned char t = Target()[0];
    if (t == Current()[0xA]) g.flag_advance();
    if (Target()[0] & 0x80) g.flag_advance();
}

// original 0x5978B0: state 2 - the member's target banner. Its place: the
// member's words +0x2E / +0x30 plus the pair of BattleWin_MemberTargetOffsets
// 0x64DF70 at row +0x89 (the character), column +8, x plus the side offset of
// +8, y plus 0xC. Then, unless a target is being chosen and it is this slot
// (no side bits) or all members (0x80): +0xF 0 and the state back one, and
// nothing drawn; else BattleWin_DrawTargetMember. (Unlike the enemy's, the
// place is not put back.)
extern "C" void __cdecl BattleWin_MemberTargetFrame() {
    unsigned char* w = Current();
    const unsigned char* m = Member(w[0xA]);
    const unsigned id = m[0x89], side = m[8];
    const int dx = SideDx(side);
    const std::uint32_t pair = at::kMemberOffsets + 2 * (side + 4 * id);
    SetWord(w + 4, static_cast<unsigned>(S8(pair) + Word(m + 0x2E) + dx));
    w = Current();
    m = Member(w[0xA]);
    SetWord(w + 6, static_cast<unsigned>(Word(m + 0x30) + S8(pair + 1) + 0xC));
    if (!Choosing()) {
        Current()[0xF] = 0;
        --Current()[3];
        return;
    }
    const unsigned char t = Target()[0];
    w = Current();
    const bool here = (t & 0xC0) == 0 ? t == w[0xA] : (t & 0x80) != 0;
    if (!here) {
        w[0xF] = 0;
        --Current()[3];
        return;
    }
    g.target_member(Word(w + 4), Word(w + 6), w[0xA]);
}

// ===========================================================================

void BattleWinStates_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_win_states")) SelfTest();

    BOF3_INJECT(BattleWin_Run);
    BOF3_INJECT(BattleWin_PartyRowStates);
    BOF3_INJECT(BattleWin_PartyRowSlideIn);
    BOF3_INJECT(BattleWin_PartyRowDraw);
    BOF3_INJECT(BattleWin_CrossStates);
    BOF3_INJECT(BattleWin_CrossGrow);
    BOF3_INJECT(BattleWin_CrossFrame);
    BOF3_INJECT(BattleWin_LabelStates);
    BOF3_INJECT(BattleWin_LabelFrame);
    BOF3_INJECT(BattleWin_BannerStates);
    BOF3_INJECT(BattleWin_BannerFrame);
    BOF3_INJECT(BattleWin_EnemyGaugeStates);
    BOF3_INJECT(BattleWin_EnemyGaugeOpen);
    BOF3_INJECT(BattleWin_EnemyGaugeFrame);
    BOF3_INJECT(BattleWin_EnemyTargetFrame);
    BOF3_INJECT(BattleWin_MemberGaugeStates);
    BOF3_INJECT(BattleWin_MemberGaugeOpen);
    BOF3_INJECT(BattleWin_MemberGaugeWait);
    BOF3_INJECT(BattleWin_MemberTargetFrame);
}
