// The battle windows' draw helpers (the seventh round's group BD).
// docs/battle_window_draw.md.
//
// Nineteen functions the owner's combat route reaches: the four openers of
// battle windows 1..4, and the primitives the battle window handlers (group
// BC's 0x442FA0, 0x443870, 0x443B10, 0x443D90, 0x443F60) draw with - the digit
// strip, a flat quad of a shape table, two tiles, a two-part bar, a 16 x 8
// cell, three lines - the test that keeps one enemy name per kind, and, from
// the skill list, the ability helpers: can it be used, its row, its AP cost,
// the lowest of its flag bits, a member's list of a type. Each was read to its
// last instruction (2026-09-23) against its PSX twin - BATTLE.EMI's for the
// first fourteen, GAME.EMI's for the skill test and row, the boot EXE's for the
// three ability helpers (docs/battle_window_draw.md, section 1).
//
// Every call goes through battle_window_draw::g (battle_window_draw_callees.h),
// so that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Where the original reads memory after a call,
// ours reads it after the same call.
//
// Upper halves. Some originals push a register whose upper bits are the
// caller's (the icon byte of Menu_DrawSkillRow) or a full argument dword
// (Skill_CanUse's member and id to Skill_ApCost); every callee here reads only
// the low 16 bits of a coordinate and the low byte of a colour, flag, count or
// id, so ours passes values with the upper bits C++ gives them - except where
// the original's own dword is cheap to rebuild exactly, which ours does.
//
// Floats. Every coordinate is an int that the original loads with `fild` and
// stores with `fstp dword`: a 16-bit value plus at most two bytes, exact in a
// float, so a plain conversion is the same bits.
#include "game/battle_window_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_window_draw_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_window_draw {

const Callees kOriginals = {
    Window_Alloc,
    Crt_sprintf,
    Gpu_GetTPage,
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_GetClut,
    Gpu_SetSprt,
    Gpu_SetPolyF4,
    Gpu_SetSemiTrans,
    Gpu_SetTile,
    Gpu_SetPolyFT4,
    Gpu_SetLineF2,
    Menu_DrawIcon8,
    Text_CharCount,
    Text_DrawAt,
    Text_DrawFont8,
    Skill_ApCost,
};
Callees g = kOriginals;

}  // namespace battle_window_draw

using namespace battle_window_draw;

namespace {

using move_script::At;
using move_script::SetWord;
using move_script::Word;

char* PrintBuf() { return reinterpret_cast<char*>(move_script::At(::bof3::Img(battle_window_draw::at::kPrintBuf.v))); }
const char* Format(std::uint32_t at) { return reinterpret_cast<const char*>(move_script::At(at)); }

}  // namespace

// ===========================================================================
// The four window openers: battle window records 1..4, handler 3 (the battle
// windows), sub-kind 0..3 in +2 and the caller's state in +3. Window_Alloc's
// answer is not looked at - the fields are written whether or not the record
// was free, as the original has it (PSX 0x801D92D4 .. 0x801D93F8 alike).

// ===========================================================================

// original 0x444C40 (PSX 0x801DA048): cell `cell` of the battle's 16 x 8 strip -
// draw mode page (0x3C0, 0) under abr 1; an SPRT of 16 x 8 at (u16 x, u16 y),
// u = cell << 4 as a byte, v 0xD8, CLUT word 0x7800, shade 0x80.
//
// As the original has it: x and y are taken unsigned (`and 0xFFFF`, then
// `fild`), where the other helpers here sign-extend - a coordinate below 0
// lands at 65,536 less it. The PlayStation stores the halfwords, where the two
// readings are the same (US 0x801DA078; on the JP disc the tail at 0x801DA048
// stores x and y with `sh` into the SPRT's signed vertex halfwords +8, +0xA).
//
// DIVERGENCE DIV-0161: 1 through PatchBytes("BattleDrawSigned") in
// BattleWindowDraw_Inject, after the self-test. x and y are then read as the
// PlayStation stores them, signed halfwords, so a coordinate below 0 clips at
// the screen's left or top edge (D336).
namespace battle_window_draw { std::uint8_t g_draw_signed = 0; }

// ===========================================================================
// The skill list's helpers. The ability table 0x65C4C8 is 0x18 bytes a
// record: the name in 16, then the PSX record's eight bytes (b0 at +0x10, the
// cost b2 at +0x12, u16_4 at +0x14 - the sibling's names/abilities.toml).

// original 0x57DA70 (GAME.EMI 0x801B05B0): whether member `member` may use
// ability `id` in list mode `mode`. Id 0: no. Mode 1 (the field's lists):
// the member's record through the party list 0x904062 and 0x66972C; no when
// its AP (+0x1A) is below Skill_ApCost or the ability's b0 lacks bit 0. Mode
// 2 (battle): the working record 0x802DC0 + member * 0x14C; no when its AP
// is below the cost, b0 lacks bit 1, or the member's +0x10 has bit 4 while
// the ability's u16_4 has 0x400; then five abilities have tests of their own
// - 0x14 only while +0x1E is below 5, 0x15 only while the four bytes at
// 0x904660 are all 0, 0x3E the same for 0x90465C, 0x8C only at 0 AP, 0x97
// not while 0x904AAA is 0x25. Any other mode: yes.
//
// As the original has it: in battle the AP is read before the cost call and
// the rest after it; in mode 1 the record is pushed as the member dword with
// its low byte replaced (the argument's own stack slot), and the AP is read
// after the call. The PSX tests the four bytes as one word each.
extern "C" unsigned char __cdecl Skill_CanUse(unsigned mode, unsigned member, unsigned id) {
    const unsigned ability = id & 0xFF;
    if (ability == 0) return 0;
    const unsigned char* const rec = At(::bof3::Img(at::kAbilities.v) + ability * at::kAbilityStride);
    const unsigned m = mode & 0xFF;
    if (m == 1) {
        const unsigned character = At(::bof3::Img(Cond_Flags_addr) + 0xd2 + (member & 0xFF))[0];
        const unsigned record = At(::bof3::Img(Character_IndexById_addr) + character)[0];
        const unsigned cost = battle_window_draw::g.ap_cost((member & 0xFFFFFF00u) | record, id, 0) & 0xFF;
        if (Word(At(::bof3::Img(CharacterRecords_addr) + record * at::kCharStride + 0x1A)) < cost) return 0;
        return (rec[0x10] & 1) ? 1 : 0;
    }
    if (m != 2) return 1;
    const unsigned char* const w = At(::bof3::Img(at::kWorking.v) + (member & 0xFF) * at::kWorkingStride);
    const unsigned ap = Word(w + 0x1A);
    const unsigned cost = battle_window_draw::g.ap_cost(member, id, 1) & 0xFF;
    if (ap < cost) return 0;
    if ((rec[0x10] & 2) == 0) return 0;
    if ((w[0x10] & 0x10) != 0 && (rec[0x15] & 4) != 0) return 0;
    switch (ability) {
    case 0x14: return w[0x1E] < 5 ? 1 : 0;
    case 0x15: {
        const unsigned char* const f = At(::bof3::Img(Rest3c_Byte660_addr));
        return (f[0] | f[1] | f[2] | f[3]) == 0 ? 1 : 0;
    }
    case 0x3E: {
        const unsigned char* const f = At(::bof3::Img(ModeFlow_CountdownA_addr));
        return (f[0] | f[1] | f[2] | f[3]) == 0 ? 1 : 0;
    }
    case 0x8C: return ap == 0 ? 1 : 0;
    case 0x97: return EventBattle_Id == 0x25 ? 0 : 1;
    default: return 1;
    }
}

// original 0x57DC90 (GAME.EMI 0x801B0928): one row of the skill list - the
// kind's 8 x 8 icon (table 0x663D70) at (x, y + 2), the name at (x + 10, y)
// for Text_CharCount characters, "%2d" of the cost's low byte in the 8 px font
// at (x + 0x6D, y + 2). The battle's skill list 0x59D200 hands it the ability
// record itself as the name (its 16-byte GBK name field), Skill_FlagIndex as
// the kind and Skill_ApCost as the cost.
extern "C" void __cdecl Menu_DrawSkillRow(int x, int y, int colour, unsigned kind, const unsigned char* name,
                                          unsigned cost, int dim) {
    const unsigned char icon = At(::bof3::Img(at::kSkillIcons.v) + (kind & 0xFF))[0];
    battle_window_draw::g.icon8(x, y + 2, icon, dim);
    const unsigned char n = battle_window_draw::g.char_count(name);
    battle_window_draw::g.text_draw_at(x + 10, y, colour, n, name);
    battle_window_draw::g.sprintf_(PrintBuf(), Format(::bof3::Img(Boss26Fx_CountFormat_addr)), cost & 0xFF);
    battle_window_draw::g.font8(x + 0x6D, y + 2, colour, reinterpret_cast<const unsigned char*>(PrintBuf()));
}

// original 0x5918A0 (boot 0x80166A74): the lowest set bit of ability `id`'s
// u16_4 & 0x1FF, counted from 1; 0 when none is.
//
// As the original has it: the scan runs to 10 and would answer 10, which the
// mask makes unreachable.
extern "C" unsigned char __cdecl Skill_FlagIndex(unsigned id) {
    const unsigned flags = move_script::Word(move_script::At(::bof3::Img(at::kAbilities.v) + (id & 0xFF) * at::kAbilityStride + 0x14));
    if ((flags & 0x1FF) == 0) return 0;
    unsigned bit = 1;
    unsigned char n = 1;
    for (;;) {
        if ((bit & flags & 0xFFFF) != 0) return n;
        bit <<= 1;
        ++n;
        if (n > 9) return n;
    }
}

// original 0x591DB0 (boot 0x80167398): ability `id`'s AP cost (b2, +0x12) for
// member `member` - of CharacterRecords when `battle`'s low byte is 0, else of
// the working record 0x802DC0 + member * 0x14C: halved, rounding up, when the
// record's +0x16 or +0x17 is 0x1A; else three quarters, rounding up, when
// either is 0x19.
extern "C" unsigned char __cdecl Skill_ApCost(unsigned member, unsigned id, unsigned battle) {
    const unsigned m = member & 0xFF;
    const unsigned char* const r =
        (battle & 0xFF) == 0 ? move_script::At(::bof3::Img(CharacterRecords_addr) + m * at::kCharStride) : move_script::At(::bof3::Img(at::kWorking.v) + m * at::kWorkingStride);
    const unsigned cost = move_script::At(::bof3::Img(at::kAbilities.v) + (id & 0xFF) * at::kAbilityStride + 0x12)[0];
    const unsigned char a = r[0x16];
    if (a == 0x1A) return static_cast<unsigned char>((cost + 1) >> 1);
    const unsigned char b = r[0x17];
    if (b == 0x1A) return static_cast<unsigned char>((cost + 1) >> 1);
    if (a != 0x19 && b != 0x19) return static_cast<unsigned char>(cost);
    const unsigned three = cost * 3;
    if ((three & 3) != 0) return static_cast<unsigned char>((three >> 2) + 1);
    return static_cast<unsigned char>(three >> 2);
}

// original 0x591E50 (boot 0x80167464): member `member`'s ability list of type
// `type` - +0x6A for 1, +0x74 for 2, +0x7E for 3, +0x60 for anything else - of
// its CharacterRecords record through the party list 0x904062 and 0x66972C
// when `battle`'s low byte is 0, else of the working record 0x802DC0 +
// member * 0x14C. The PSX lists are 4 lower (+0x5C .. +0x7A,
// AbilityList_ForType 0x80167514, which takes an ability id for the type).
extern "C" unsigned char* __cdecl Char_AbilityList(unsigned member, unsigned type, unsigned battle) {
    unsigned char* r;
    if ((battle & 0xFF) == 0) {
        const unsigned character = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2 + (member & 0xFF))[0];
        r = move_script::At(::bof3::Img(CharacterRecords_addr) + move_script::At(::bof3::Img(Character_IndexById_addr) + character)[0] * at::kCharStride);
    } else {
        r = move_script::At(::bof3::Img(at::kWorking.v) + (member & 0xFF) * at::kWorkingStride);
    }
    switch (type & 0xFF) {
    case 1: return r + 0x6A;
    case 2: return r + 0x74;
    case 3: return r + 0x7E;
    default: return r + 0x60;
    }
}

// ===========================================================================

void BattleWindowDraw_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_window_draw")) SelfTest();
    {
        // DIV-0161: after the self-test, which compared Capcom's unsigned reading.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("BattleDrawSigned", Low32(&battle_window_draw::g_draw_signed), &was, &is, 1);
        bof3::Log("DIV-0161    the battle strip cell takes its x and y signed, as the PlayStation (on unless the line above says OFF)");
    }

    BOF3_INJECT(Skill_CanUse);
    BOF3_INJECT(Menu_DrawSkillRow);
    BOF3_INJECT(Skill_FlagIndex);
    BOF3_INJECT(Skill_ApCost);
    BOF3_INJECT(Char_AbilityList);
}
