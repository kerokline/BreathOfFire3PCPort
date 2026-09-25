// The battle windows (the seventh round's group BC). docs/battle_windows.md.
//
// Twenty-two functions the owner's combat route reaches: the party row's
// status bars, the command cross and its icons, the command label banner,
// the three plain boxes, the enemy and member target banners - the handlers
// the window task 0x596A90 calls - with the window task's small helpers and
// the battle party's per-frame state step (0x441100 .. 0x4412B0). Each was
// read to its last instruction (2026-09-23) against its BATTLE.EMI twin where
// analysis/pairs_propagated.json has one (docs/battle_windows.md, section 1).
//
// Every call goes through battle_windows::g (battle_windows_callees.h), so
// that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Where the original reads memory after a call,
// ours reads it after the same call.
//
// Upper halves. The originals push many values built with 8- and 16-bit
// operations, whose upper bits are whatever the register or the stack slot
// held; every callee here reads only the low 16 bits of a coordinate and the
// low byte of a colour, size or flag (each callee's read is in the fuzz's
// stand-ins), so ours passes the value with its upper bits as C++ computes
// them. The originals also park values in their own argument slots (the
// status bars' x in the first, a colour byte in the second); unobservable to
// a caller, ours uses locals.
#include "game/battle_windows.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/text_pairs.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_windows {

using move_script::At;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Gpu_GetTPage,
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetPolyFT4,
    Text_DrawAt,
    Text_DrawFont8,
    Crt_sprintf,
    Menu_DrawIcon,
    Sprite_UpdateScreen,
    reinterpret_cast<unsigned (__cdecl*)()>(reinterpret_cast<void*>(&Sprite_ScriptTick)),
    reinterpret_cast<unsigned (__cdecl*)()>(reinterpret_cast<void*>(&Sprite_ScriptTickOnce)),
    reinterpret_cast<unsigned (__cdecl*)(unsigned)>(reinterpret_cast<void*>(&Sprite_EnsureAnimation)),
    Fn<unsigned (__cdecl*)(unsigned, std::uint32_t, unsigned)>(kPoseFrom),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(kDrawValue),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(kDrawEdge),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(kDrawTile),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, unsigned)>(kDrawTileRgb),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, unsigned)>(kDrawBar),
    Fn<void (__cdecl*)(int, int, unsigned)>(kDrawDigit),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(kLinePlain),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(kLineSemi0),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(kLineSemi1),
    Fn<unsigned (__cdecl*)(unsigned)>(kEnemyNameShown),
    Fn<const unsigned char* (__cdecl*)(int, int, unsigned, unsigned, const unsigned char*)>(kTinyFont),
    reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(at::kStateTable)),
    {at::kKindHandlers[0], at::kKindHandlers[1], at::kKindHandlers[2]},
    BattleObj_RunState,
    BattleWin_DrawCommandIcon,
    BattleWin_DrawMessageBox,
};
Callees g = kOriginals;

}  // namespace battle_windows

using namespace battle_windows;

namespace {

using move_script::Long;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char* Ptr(std::uint32_t at) { return At(static_cast<std::uint32_t>(Long(At(at)))); }
unsigned char* Current() { return Ptr(at::kCurrent); }
unsigned char* FieldState() { return Field_State; }
unsigned char* Sprite() { return Sprite_Current; }

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
    const int row = static_cast<signed char>(At(at::kColour)[0]);
    const unsigned w = Word(At(at::kClutShadow + static_cast<std::uint32_t>(row * 64)));
    return {(w & 0x1F) << 3, ((w >> 5) & 0x1F) << 3, ((w >> 10) & 0x1F) << 3};
}

// The draw mode every window opens with: page (0x3C0, 0) under `abr`.
void OpenDrawMode(unsigned abr) {
    const unsigned tpage = g.get_tpage(0, abr, 0x3C0, 0) & 0xFFFF;
    g.draw_mode(Gfx_PacketNext, 0, 0, tpage, 0);
    g.commit(1, 0xC);
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
unsigned char* Member(unsigned i) { return At(at::kPartyObjects + at::kObjectStride * i); }

// A status colour from the member's +0x90 flags: 2 when bit 14 (grey),
// else what the caller derives.
unsigned StatusColourName(unsigned flags) { return (flags & 0x4000) ? 2u : ((flags & 0xBFC) != 0 ? 1u : 0u); }
unsigned StatusColourHp(unsigned flags) { return (flags & 0x4000) ? 2u : ((flags >> 11) & 4u); }

}  // namespace

// ===========================================================================
// The battle party's per-frame state step (the battle frame 0x42E2F0 calls
// the first two; the state handlers of 0x64DFE0 the rest).

// original 0x4411E0 (PSX 0x801DEE70): the current object's state handler -
// when bit 0 of Sprite_Current +0 is set, tail-jump through the 27-entry
// table 0x64DFE0 by the state byte +1 (unbounded, as the PlayStation's), else
// return with eax still Sprite_Current.
extern "C" unsigned __cdecl BattleObj_RunState() {
    unsigned char* const s = Sprite();
    if ((s[0] & 1) == 0) return Addr(s);
    using Handler = unsigned (__cdecl*)();
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(g.state_table[s[1]]))();
}

// original 0x441100: each of the three party objects 0x802D40 + 0x14C i made
// Field_State and Sprite_Current, then its state step (the third a tail jump,
// so the answer is its eax).
extern "C" unsigned __cdecl BattleParty_RunStates() {
    for (unsigned i = 0; i < 2; ++i) {
        Field_State = Member(i);
        Sprite_Current = Member(i);
        g.run_state();
    }
    Field_State = Member(2);
    Sprite_Current = Member(2);
    return g.run_state();
}

// original 0x441140: each party object made Field_State and Sprite_Current,
// and its screen slot updated when +0 has bit 0 and not bit 6.
extern "C" void __cdecl BattleParty_UpdateScreens() {
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const o = Member(i);
        Field_State = o;
        Sprite_Current = o;
        const unsigned char b = o[0];
        if ((b & 1) && !(b & 0x40)) g.update_screen();
    }
}

// original 0x441180 (PSX 0x801DEDA0): Sprite_ScriptTick for the current
// object unless Field_State +0x90 has bit 2, or the gate 0x904B8E is set and
// +0x134 lacks bit 4; 1 when it does not tick.
extern "C" unsigned char __cdecl BattleObj_ScriptTick() {
    unsigned char* const f = FieldState();
    if (f[0x90] & 4) return 1;
    if (At(at::kTickGate)[0] != 0 && !(f[0x134] & 0x10)) return 1;
    return static_cast<unsigned char>(g.script_tick());
}

// original 0x4411B0 (PSX 0x801DEE08): the same gate before
// Sprite_ScriptTickOnce.
extern "C" unsigned char __cdecl BattleObj_ScriptTickOnce() {
    unsigned char* const f = FieldState();
    if (f[0x90] & 4) return 1;
    if (At(at::kTickGate)[0] != 0 && !(f[0x134] & 0x10)) return 1;
    return static_cast<unsigned char>(g.script_tick_once());
}

// original 0x4412B0 (PSX 0x801DEFA0): the pose a party object stands in.
// Under battle kind 5 with 0x904AE8 bit 1, a pose from the set 0x8C5D80
// (0x589110): +8 + 0x1C when Field_State +0x91 has bit 6, else +0x4B's pose
// (0x904AE5 bit 5 set) or +8 + 0x20. Otherwise Sprite_EnsureAnimation of +8
// plus: 0x1C (+0x91 bit 6), 0x30 (+0x91 bit 3); under 0x904AA8 bit 4, 4
// (+0x90 bit 2), 0x18 (any of +0x90's 0x20C0) or 8; else 0x14 (+0x130 bit 1)
// or by the action +0x125: 0 -> 0x18 / 4 as before, 1 -> 8, 2 -> 0x14,
// 4 -> 0x38 (+0x134 bit 2) or, by bit 3 of the skill +0x126's flags
// (0x65C4DD + 24 skill), 8 or 0x28, 5 -> 4; 3 and above 5 change nothing and
// answer the action. The answer is the callee's eax.
extern "C" unsigned __cdecl BattleObj_PickPose() {
    const bool own_poses = At(at::kBattleKind)[0] == 5 && (At(at::kKindFlags)[0] & 2) != 0;
    if (own_poses) {
        const bool high = (At(at::kPoseFlags)[0] & 0x20) != 0;
        if (FieldState()[0x91] & 0x40)
            return g.pose_from(static_cast<unsigned char>(Sprite()[8] + 0x1C), at::kPoseSet, 0x1800);
        if (high) return g.pose_from(Sprite()[0x4B], at::kPoseSet, 0x1800);
        return g.pose_from(static_cast<unsigned char>(Sprite()[8] + 0x20), at::kPoseSet, 0x1800);
    }
    unsigned char* const f = FieldState();
    const unsigned flags = Word(f + 0x90);
    const auto pose = [](unsigned add) { return g.ensure_pose(static_cast<unsigned char>(Sprite()[8] + add)); };
    if (flags & 0x4000) return pose(0x1C);
    if (flags & 0x0800) return pose(0x30);
    if (At(at::kPoseMode)[0] & 0x10) {
        if (flags & 4) return pose(4);
        if (flags & 0x20C0) return pose(0x18);
        return pose(8);
    }
    if (f[0x130] & 2) return pose(0x14);
    const unsigned action = f[0x125];
    switch (action) {
    case 0: return pose((flags & 0x20C0) ? 0x18 : 4);
    case 1: return pose(8);
    case 2: return pose(0x14);
    case 4: {
        if (f[0x134] & 4) return pose(0x38);
        const unsigned skill = Word(f + 0x126);
        return pose((At(at::kSkillFlags + skill * 24)[0] & 8) ? 8 : 0x28);
    }
    case 5: return pose(4);
    default: return action;   // 3, and anything past the table's six
    }
}

// ===========================================================================
// The window task's handlers

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
    const unsigned count = At(at::kPartyCount)[0];
    g.draw_edge(x, y, static_cast<unsigned char>((count + 5) << 1), 1);
    g.draw_edge(x, y + 0x18, static_cast<unsigned char>((At(at::kPartyCount)[0] << 1) + 0xB), 1);
    g.draw_tile(x, y + 2, static_cast<unsigned char>(At(at::kPartyCount)[0] + 0xD), 1);
    const Rgb c = WindowRgb();
    if (At(at::kPartyCount)[0] == 0) return;
    int slot = x;           // the original keeps x + 0x5E i in its first argument's slot
    int xs = x + 0x14;
    unsigned i = 0;
    do {
        g.draw_tile_rgb(xs, y + 0xE, 9, 0, 0);
        const unsigned char* const bars = At(at::kPartyBars + 36 * i);
        const unsigned char b1 = bars[2], b0 = bars[0];
        g.draw_bar(xs, y + 0xE, b0, b1, 0);
        unsigned char* const m = Member(i);
        if (Word(m + 0xA2) != 0) {
            g.line_semi1(xs, y + 0xE, bars[1] + slot + 0x14, y + 0xE, 0x64, 0, 0xC8);
            const unsigned char extra = bars[3];
            if (extra != 0) {
                const int start = bars[1] + slot + 0x14;
                g.line_semi1(start, y + 0xE, extra + start, y + 0xE, 0xC8, 0, 0);
            }
        }
        g.tiny_font(xs - 0xE, y + 5, StatusColourName(Word(m + 0x90)), 5, m + 0x80);
        g.draw_digit(xs + 0x26, y + 6, 1);
        g.draw_digit(xs - 0x10, y + 0xE, 0);
        const unsigned hp_colour = StatusColourHp(Word(m + 0x90));
        g.draw_value(xs + 0x34, y + 0xF, hp_colour, Word(m + 0x98));
        const unsigned value = Word(m + 0x9A);
        unsigned colour = 2;
        if (value != 0 && !(m[0x91] & 0x40)) colour = value < static_cast<unsigned>(Word(m + 0xA2) >> 2) ? 4u : 0u;
        g.draw_value(xs + 0x34, y + 8, colour, value);
        g.line_semi1(xs - 0x12, y + 2, xs + 0x49, y + 2, c.r, c.g, c.b);
        g.line_semi1(xs - 0x12, y + 3, xs - 0x12, y + 0x17, c.r, c.g, c.b);
        g.line_semi0(xs + 0x49, y + 2, xs + 0x49, y + 0x17, c.r, c.g, c.b);
        g.line_semi0(xs - 0x12, y + 0x17, xs + 0x49, y + 0x17, c.r, c.g, c.b);
        slot += 0x5E;
        xs += 0x5E;
        i = static_cast<unsigned char>(i + 1);
    } while (i < At(at::kPartyCount)[0]);
}

namespace {
// The command cross's shade for arm j: 0x40 for arms 2 and 3 while the
// acting member's +0x134 has bit 1, else 0x80.
unsigned ArmShade(unsigned j) {
    if (j == 2 || j == 3) {
        if (Ptr(at::kActor)[0x134] & 2) return 0x40;
    }
    return 0x80;
}
}  // namespace

// original 0x4432F0 (PSX 0x801D7A10): the command cross at (x, y) - arms
// 0..4 but the selected one 0x904AB4 as icons (Menu_DrawIcon, grown by
// 0x904ABC + j about the arm's point 0x64E2AC + 2 j), then the selected arm
// on top: a plain 16 x 16 icon when its growth is 0, else
// BattleWin_DrawCommandIcon; then the two extra arms 5 and 6 when
// 0x904AC1 / 0x904AC2 are set. The selection is re-read after each icon;
// the shade stays what the last arm set when the selection is 5 or more.
extern "C" void __cdecl BattleWin_DrawCommandCross(int x, int y) {
    std::uint32_t sel = static_cast<std::uint32_t>(Long(At(at::kCrossSel)));
    unsigned shade = 0;
    for (unsigned j = 0; j < 5; ++j) {
        if (j == (sel & 0xFF)) continue;
        shade = ArmShade(j);
        const unsigned char grow = At(at::kCrossGrow + j)[0];
        const unsigned half = grow >> 1;
        const unsigned char* const arm = At(at::kCrossArms + 2 * j);
        const unsigned size = static_cast<unsigned char>(grow + 0x10);
        g.icon(j, static_cast<unsigned>(x + arm[0] - static_cast<int>(half)),
               static_cast<unsigned>(y + arm[1] - static_cast<int>(half)), size, size, shade);
        sel = static_cast<std::uint32_t>(Long(At(at::kCrossSel)));
    }
    const unsigned k = sel & 0xFF;
    if (k < 5) shade = ArmShade(k);
    const unsigned char grow = At(at::kCrossGrow + k)[0];
    const unsigned char* const arm = At(at::kCrossArms + 2 * k);
    if (grow == 0) {
        g.icon(sel, static_cast<unsigned>(x + arm[0]), static_cast<unsigned>(y + arm[1]), 0x10, 0x10, shade);
    } else {
        const unsigned half = grow >> 1;
        const unsigned size = static_cast<unsigned char>(grow + 0x10);
        g.command_icon(sel, static_cast<unsigned>(x + arm[0] - static_cast<int>(half)),
                       static_cast<unsigned>(y + arm[1] - static_cast<int>(half)), size, size, shade);
    }
    for (unsigned e = 5; e < 7; ++e) {
        const unsigned char grow2 = At(e == 5 ? at::kCrossExtra5 : at::kCrossExtra6)[0];
        if (grow2 == 0) continue;
        const unsigned half = grow2 >> 1;
        const unsigned char* const arm2 = At(at::kCrossArms + 2 * e);
        const unsigned size = static_cast<unsigned char>(grow2 + 0x10);
        g.command_icon(e, static_cast<unsigned>(x + arm2[0] - static_cast<int>(half)),
                       static_cast<unsigned>(y + arm2[1] - static_cast<int>(half)), size, size, 0x80);
    }
}

namespace {
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
}  // namespace

// original 0x4434C0 (PSX 0x801D7CE0): a grown command icon k, a POLY_FT4 at
// (x, y) sized w x h (low words, low bytes) from page (0x100, 0x100), u = k
// << 5 .. + 0x18, v 0xE0 .. 0xF8, shade r = g = b, CLUT row 0x7800 | the
// table byte's low 6 bits (above).
extern "C" __attribute__((disable_tail_calls, noinline)) void __cdecl BattleWin_DrawCommandIcon(
    unsigned k, unsigned x, unsigned y, unsigned w, unsigned h, unsigned shade) {
    const auto* const frame = static_cast<const unsigned char*>(__builtin_frame_address(0));
    const unsigned tpage0 = g.get_tpage(0, 0, 0x100, 0x100) & 0xFFFF;
    g.draw_mode(Gfx_PacketNext, 0, 0, tpage0, 0);
    g.commit(1, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    g.set_poly_ft4(p);
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
    const unsigned tpage = g.get_tpage(0, 0, 0x100, 0x100);
    SetWord(p + 0x26, tpage);
    const unsigned char clut = i < sizeof kCommandClut ? kCommandClut[i] : CommandClutPastTable(i, frame, x1, y1);
    SetWord(p + 0x16, 0x7800u | (clut & 0x3Fu));
    g.commit(1, 0x48);
}

namespace {
// The three plain boxes: a tile (size index `tile`) at (x, y + 2), the edge
// pieces `top` at (x, y) and `bottom` at (x, y + 0x11), then four lines in
// the window colour from x + 2 to x + `right`, y + 2 to y + 0x10.
void PlainBox(int x, int y, unsigned tile, unsigned top, unsigned bottom, int right) {
    OpenDrawMode(0);
    const Rgb c = WindowRgb();
    g.draw_tile(x, y + 2, tile, 1);
    g.draw_edge(x, y, top, 1);
    g.draw_edge(x, y + 0x11, bottom, 1);
    const int x0 = x + 2, x1 = x + right;
    g.line_semi1(x0, y + 2, x1, y + 2, c.r, c.g, c.b);
    g.line_semi1(x0, y + 3, x0, y + 0x10, c.r, c.g, c.b);
    g.line_semi0(x1, y + 2, x1, y + 0x10, c.r, c.g, c.b);
    g.line_semi0(x0, y + 0x10, x1, y + 0x10, c.r, c.g, c.b);
}
}  // namespace

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
    const int y = Word(At(at::kCommandBoxes + 8 * i + 2));
    const int x = Word(At(at::kCommandBoxes + 8 * i));
    const Rgb c = WindowRgb();
    g.draw_tile(x, y + 2, 5, 1);
    g.draw_edge(x, y, 0xA, 1);
    g.draw_edge(x, y + 0x11, 0xB, 1);
    g.line_semi1(x + 2, y + 2, x + 0x25, y + 2, c.r, c.g, c.b);
    g.line_semi1(x + 2, y + 3, x + 2, y + 0x10, c.r, c.g, c.b);
    g.line_semi0(x + 0x25, y + 2, x + 0x25, y + 0x10, c.r, c.g, c.b);
    g.line_semi0(x + 2, y + 0x10, x + 0x25, y + 0x10, c.r, c.g, c.b);
    const auto* const label = At(static_cast<std::uint32_t>(Long(At(at::kCommandLabels + 4 * i))));
    g.text_draw_at(x + 8, y + 3, 0, 8, label);
}

namespace {
// The target banners' frame: the outer box under abr 0 at (x + 4, y + 6),
// then the inner under abr 1 at (x, y).
void TargetFrame(int x, int y) {
    g.draw_tile(x + 4, y + 8, 8, 1);
    g.draw_edge(x + 4, y + 6, 2, 1);
    g.draw_edge(x + 4, y + 0x19, 3, 1);
    OpenDrawMode(1);
    g.draw_tile(x, y + 2, 8, 0);
    g.draw_tile_rgb(x + 6, y + 0xB, 9, 0, 0);
    g.draw_edge(x, y, 2, 0);
    g.draw_edge(x, y + 0x13, 3, 0);
}
// An enemy's gauge, or 0x444340 of 0xFFFF (its ":" glyph, no number) when +0xF is not 1.
void EnemyGauge(int x, int y, std::uint32_t enemy) {
    if (At(enemy + 0xF)[0] == 1) {
        const unsigned char* const w = Current();
        g.draw_bar(x + 6, y + 0xA, w[0xB], w[0xD], 1);
    } else {
        g.draw_value(x + 0x24, y + 0xC, 0, 0xFFFF);
    }
}
}  // namespace

// original 0x443B10 (PSX 0x801D8660): the enemy target banner - slot t (an
// enemy is t - 3, record 0x93B9E0 + 0x128 e): the two-layer frame, its gauge
// (the current window's +0xB / +0xD) or the 0xFFFF value, its name in the 8 px font,
// the two outer lines in the window colour and the two inner ones 0x10
// darker (clamped at 0).
extern "C" void __cdecl BattleWin_DrawTargetEnemy(int x, int y, unsigned t) {
    OpenDrawMode(0);
    const unsigned w = Word(At(at::kClutShadow + static_cast<std::uint32_t>(
                                  static_cast<int>(static_cast<signed char>(At(at::kColour)[0])) * 64)));
    const unsigned r = (w & 0x1F) << 3, gr = ((w >> 5) & 0x1F) << 3, b = ((w >> 10) & 0x1F) << 3;
    const unsigned e = static_cast<unsigned char>(t - 3);
    TargetFrame(x, y);
    const std::uint32_t enemy = at::kEnemies + at::kEnemyStride * e;
    EnemyGauge(x, y, enemy);
    std::uint8_t expanded[33];   // DIV-0057: the 8-unit draw is Capcom's, so pairs go in as their glyphs
    g.tiny_font(x + 4, y + 3, 0, 8, TextPairs_Expand(At(enemy), expanded, sizeof expanded));
    g.line_semi1(x + 2, y + 2, x + 0x49, y + 2, r, gr, b);
    g.line_semi1(x + 2, y + 3, x + 2, y + 0x12, r, gr, b);
    const unsigned r2 = Darker(r), g2 = Darker(gr), b2 = Darker(b);
    g.line_plain(x + 0x49, y + 2, x + 0x49, y + 0x12, r2, g2, b2);
    g.line_plain(x + 2, y + 0x12, x + 0x49, y + 0x12, r2, g2, b2);
}

// original 0x443D90 (PSX 0x801D8AB4): the enemy status banner - slot t as
// above in one layer: its gauge or the 0xFFFF value, its name only when 0x444EB0 says
// so, and four lines in the window colour.
extern "C" void __cdecl BattleWin_DrawEnemyStatus(int x, int y, unsigned t) {
    OpenDrawMode(0);
    const int row = static_cast<signed char>(At(at::kColour)[0]);
    const unsigned w = Word(At(at::kClutShadow + static_cast<std::uint32_t>(row * 64)));
    const unsigned r = (w & 0x1F) << 3, gr = ((w >> 5) & 0x1F) << 3, b = ((w >> 10) & 0x1F) << 3;
    const unsigned e = static_cast<unsigned char>(t - 3);
    g.draw_tile(x, y + 2, 8, 1);
    g.draw_tile_rgb(x + 6, y + 0xB, 9, 0, 0);
    g.draw_edge(x, y, 2, 1);
    g.draw_edge(x, y + 0x13, 3, 1);
    const std::uint32_t enemy = at::kEnemies + at::kEnemyStride * e;
    EnemyGauge(x, y, enemy);
    std::uint8_t expanded[33];   // DIV-0057, as in BattleWin_DrawTargetEnemy
    if (static_cast<unsigned char>(g.enemy_name_shown(e)) != 0)
        g.tiny_font(x + 4, y + 3, 0, 8, TextPairs_Expand(At(enemy), expanded, sizeof expanded));
    g.line_semi1(x + 2, y + 2, x + 0x49, y + 2, r, gr, b);
    g.line_semi1(x + 2, y + 3, x + 2, y + 0x12, r, gr, b);
    g.line_semi0(x + 0x49, y + 2, x + 0x49, y + 0x12, r, gr, b);
    g.line_semi0(x + 2, y + 0x12, x + 0x49, y + 0x12, r, gr, b);
}

// original 0x443F60 (PSX 0x801D8DC8): the member target banner - member m's
// (ObjTrio + 0x14C m) two-layer frame, name in the 8 px font, a digit, the
// value +0x98 (coloured 2 grey / 4 by +0x90 bit 13), "/" through sprintf and
// the 8 px numeral font and the value +0xA0, both coloured 2 by bit 14 else
// 0, the two outer lines and the two inner ones 0x10 darker.
extern "C" void __cdecl BattleWin_DrawTargetMember(int x, int y, unsigned m) {
    OpenDrawMode(0);
    const unsigned w = Word(At(at::kClutShadow + static_cast<std::uint32_t>(
                                  static_cast<int>(static_cast<signed char>(At(at::kColour)[0])) * 64)));
    const unsigned r = (w & 0x1F) << 3, gr = ((w >> 5) & 0x1F) << 3, b = ((w >> 10) & 0x1F) << 3;
    g.draw_tile(x + 4, y + 8, 8, 1);
    g.draw_edge(x + 4, y + 6, 2, 1);
    g.draw_edge(x + 4, y + 0x19, 3, 1);
    OpenDrawMode(1);
    g.draw_tile(x, y + 2, 8, 0);
    g.draw_edge(x, y, 2, 0);
    g.draw_edge(x, y + 0x13, 3, 0);
    unsigned char* const o = Member(m & 0xFF);
    g.tiny_font(x + 4, y + 2, StatusColourName(Word(o + 0x90)), 5, o + 0x80);
    const unsigned colour = StatusColourHp(Word(o + 0x90));
    g.draw_digit(x + 2, y + 0xA, 0);
    g.draw_value(x + 0x14, y + 0xC, colour, Word(o + 0x98));
    const unsigned slash_colour = (Word(o + 0x90) >> 13) & 2;
    char* const buf = reinterpret_cast<char*>(At(at::kPrintBuf));
    g.sprintf_(buf, reinterpret_cast<const char*>(At(at::kSlash)));
    g.font8(x + 0x28, y + 0xA, static_cast<int>(slash_colour), reinterpret_cast<const unsigned char*>(buf));
    // the slash's colour, not the first value's: the original keeps both in
    // its first argument's slot, the second over the first
    g.draw_value(x + 0x30, y + 0xC, slash_colour, Word(o + 0xA0));
    g.line_semi1(x + 2, y + 2, x + 0x49, y + 2, r, gr, b);
    g.line_semi1(x + 2, y + 3, x + 2, y + 0x12, r, gr, b);
    const unsigned r2 = Darker(r), g2 = Darker(gr), b2 = Darker(b);
    g.line_plain(x + 0x49, y + 2, x + 0x49, y + 0x12, r2, g2, b2);
    g.line_plain(x + 2, y + 0x12, x + 0x49, y + 0x12, r2, g2, b2);
}

// ===========================================================================
// The window task's small helpers

// original 0x5979E0: the current window record's +0xF set and its state +3
// stepped on.
extern "C" void __cdecl Window_FlagAndAdvance() {
    Current()[0xF] = 1;
    ++Current()[3];
}

// original 0x597A00: the current window record's position +4 / +6 put back
// from +0x10 / +0x12, +0xF cleared, its state +3 stepped back.
extern "C" void __cdecl Window_RestoreAndBack() {
    unsigned char* w = Current();
    SetWord(w + 4, Word(w + 0x10));
    w = Current();
    SetWord(w + 6, Word(w + 0x12));
    Current()[0xF] = 0;
    --Current()[3];
}

// original 0x597A30: a window kind's handler by the byte its eighth argument
// points at, from three on its stack (0x597A80, 0x597BD0, 0x597C10), handed
// all eight arguments; the answer is the handler's. Past 2 the original
// indexes its frame without a bound: 3 calls its own return address, 4..11
// its arguments as pointers, 12 and up the caller's frame - ours reads the
// same slots (`frame` is our EBP = entry ESP - 4; the table's dword i sits at
// entry ESP - 0xC + 4 i). docs/battle_windows.md, section 4.
extern "C" __attribute__((disable_tail_calls, noinline)) unsigned __cdecl Window_DispatchKind(
    std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5,
    std::uint32_t a6, std::uint32_t a7) {
    const auto* const frame = static_cast<const unsigned char*>(__builtin_frame_address(0));
    const unsigned i = At(a7)[0];
    std::uint32_t handler;
    if (i < 3) {
        handler = g.kind_handlers[i];
    } else {
        std::memcpy(&handler, frame - 8 + 4 * i, sizeof handler);
    }
    using Handler = unsigned (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(handler))(a0, a1, a2, a3, a4, a5, a6, a7);
}

// original 0x597ED0: the battle message - the current window's box
// (BattleWin_DrawMessageBox at +4, +6), then the ring's entry 0x93C2A0
// through Text_DrawAt at (+4 + 4, +6 + 3), to its end. The record and the
// index are read after the box.
extern "C" void __cdecl BattleWin_DrawMessage() {
    const unsigned char* w = Current();
    g.message_box(Word(w + 4), Word(w + 6));
    const unsigned index = At(at::kMsgHead)[0];
    const auto* const text = At(static_cast<std::uint32_t>(Long(At(at::kMsgRing + 8 * index))));
    w = Current();
    g.text_draw_at(static_cast<std::uint16_t>(Word(w + 4) + 4), static_cast<std::uint16_t>(Word(w + 6) + 3), 0, 0xFF,
                   text);
}

// original 0x597F20: the battle message ring's read index 0x93C2A0 stepped
// on (mod 16); 1 when it has reached the write index 0x93C2A1.
extern "C" unsigned char __cdecl BattleMsg_Advance() {
    const auto head = static_cast<unsigned char>((At(at::kMsgHead)[0] + 1) & 0xF);
    const unsigned char tail = At(at::kMsgTail)[0];
    At(at::kMsgHead)[0] = head;
    return tail == head ? 1 : 0;
}

// original 0x597F40: the characters of a string, a byte of 0x80 or more
// taking the next with it; a byte count.
extern "C" unsigned char __cdecl Text_GlyphCount(const unsigned char* text) {
    unsigned char n = 0;
    const unsigned char* p = text;
    unsigned char c = *p;
    while (c != 0) {
        if (c & 0x80) {
            if (TextPair_At(p)) ++n;      // DIV-0057: a pair is two characters wide
            ++p;
        }
        c = p[1];
        ++p;
        ++n;
    }
    return n;
}

// ===========================================================================

void BattleWindows_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_windows")) SelfTest();

    BOF3_INJECT(BattleParty_RunStates);
    BOF3_INJECT(BattleParty_UpdateScreens);
    BOF3_INJECT(BattleObj_ScriptTick);
    BOF3_INJECT(BattleObj_ScriptTickOnce);
    BOF3_INJECT(BattleObj_RunState);
    BOF3_INJECT(BattleObj_PickPose);
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
    BOF3_INJECT(Window_FlagAndAdvance);
    BOF3_INJECT(Window_RestoreAndBack);
    BOF3_INJECT(Window_DispatchKind);
    BOF3_INJECT(BattleWin_DrawMessage);
    BOF3_INJECT(BattleMsg_Advance);
    BOF3_INJECT(Text_GlyphCount);
}
