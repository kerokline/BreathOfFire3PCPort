// Group BJ of the seventh takeover round (docs/battle_draw.md): six functions,
// each read to its last instruction with capstone against bof3/BOF3.exe
// (2026-09-23). Faithful: no divergence. The start-up fuzz is
// battle_draw_fuzz.cpp.
//
//   D3d_DrawPolyG3            0x5A0E80..0x5A1047 (0x1C8)  code 0x30   -
//   D3d_DrawLineG2            0x5A18B0..0x5A19FC (0x14D)  code 0x50   -
//   D3d_DrawLineG3            0x5A1B50..0x5A1D0D (0x1BE)  code 0x58   -
//   BattleMenu_DrawItemList   0x59CD00..0x59D1F1 (0x4F2)              - (no pair)
//   BattleMenu_DrawSkillList  0x59D200..0x59D635 (0x436)              - (no pair)
//   AreaMap_TintClut          0x573050..0x573077 (0x28)               PSX 0x801565B4
//
// Every call goes through battle_draw::g (battle_draw_callees.h), so that the
// fuzz can stand recorders in for them - for ours and for the originals'
// copies alike. Where the original reads memory after a call, ours reads it
// after the same call.
//
// Upper halves. The two list draws push many values built with 8- and 16-bit
// operations, whose upper bits are whatever the register or stack slot held
// (a local byte in a dword slot, a 16-bit add into ax). Every callee reads only
// the low 16 bits of a coordinate and the low byte of a colour, flag, count or
// id - the menus' own (menu_windows.cpp's rule), and group BD's by their first
// instructions (docs/battle_draw.md section 3) - so ours passes each value with
// its upper bits as C++ computes them.
#include "game/battle_draw.h"

#include "game/d3d_lines.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/text_advance.h"
#include "game/battle_draw_callees.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_draw {

namespace {
template <typename T> T Fn(U address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    D3d_PrimColor,
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kRetOnly.v)),
    D3d_SetBlend,
    D3d_SetShadeMode,
    Menu_DrawBox,
    Menu_ListScroll,
    Item_CanUse,
    Menu_DrawItemRow,
    Text_CharCount,
    Text_DrawAt,
    Inventory_CountUsed,
    Crt_sprintf,
    Text_DrawFont8,
    Menu_DrawPieces,
    Menu_DrawPiece,
    Menu_DrawScrollBar,
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kSkillUsable.v)),
    Fn<void (__cdecl*)(int, int, int, unsigned, const unsigned char*, unsigned, int)>(::bof3::Code(kSkillRow.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kSkillIcon.v)),
    Fn<unsigned (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kSkillCost.v)),
    Fn<const unsigned char* (__cdecl*)(unsigned, unsigned, unsigned)>(::bof3::Code(kSkillList.v)),
    Gfx_ClutAdjust,
    AreaMap_ClutCycleStart,
};
Callees g = kOriginals;

}  // namespace battle_draw

namespace {

using namespace battle_draw;

unsigned char* AtBattleDraw(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
battle_draw::U LongBattleDraw(const unsigned char* p) {
    battle_draw::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

}  // namespace

// --- the Direct3D handlers -------------------------------------------------------
// Each is reached from one site of Gfx_DrawOTag's Direct3D table (d3d-draw.md
// section 2) and returns what DrawPrimitive returned (the caller does not read
// it). Untextured and Gouraud (shade mode 2): SetTextureBattleDraw(0, NULL); the blend
// mode Gfx_DrawTpage's low word, read for each colour and again for the blend;
// the colour helper gets a null specular pointer, so the vertices' specular,
// tu and tv stay as the last draw left them. The colours go into locals, as
// D3d_DrawPolyG4's do. The PC port's own: no PSX twin (the PlayStation drew
// primitives on its GPU).

// --- the battle menu's lists -------------------------------------------------------
// Both are the drawing half of a window-task handler (0x59CB40 and 0x59CBC0,
// entries 1 and 2 of the handler table 0x66B534, each of which first runs a
// step from its own table by the record's state byte +3, then tail-calls this
// with the current record 0x905B84). Their shape is Menu_DrawItemList's
// (0x5759C0), with seven rows and a smaller frame. Neither returns anything its
// caller reads.

// original 0x59CD00 (no PSX pair): the battle's item list. The window record:
// +4 / +6 x and y, +8 the Item_CanUse mode, +9 greyed (and the boxes' flags),
// +0xA the category, +0xB the top index, +0xC a marked index, +0xD the cursor,
// word +0x10 two arrow flags and a countdown in its high nibble, +0x12 the
// scroll state.
//
// The box (x + 3, y + 3, 0x99, 0x82); the scroll step, whose offset (s8)
// shifts the rows and whose moving flag draws an eighth; each row 13 apart
// whose item is not 0: dim when greyed or when Item_CanUse says no for the
// member of the battle party list 0x904065 the menu is on (0x929F06), colour
// 7 when dim, 2 on the cursor (top + row == +0xD), 0 else; the count 1 for
// category 4, else the counts array's; the cursor's and the marked row drawn 2
// higher over a colour-7 dim shadow (none when the colour is 7). Then the
// title and footer boxes, the category's title (0x66B58C) centred by its
// character count, "%3d/%3d" of Inventory_CountUsed and the room (0x20 for
// category 4, else 0x80), the arrows by +0x10's bits 1 and 0, the countdown
// stepped down, the frame's pieces and the scroll bar over 0x80 entries.
// DIVERGENCE DIV-0059: the item and skill lists' title (the field menu's
// and the battle's - "Examine" while a skill targets, the owner 2026-09-27)
// is placed by the originals at 6 * (13 - n) into the 0x99-wide box: centred
// on x + 78 for 12-unit glyphs, left of centre for DIV-0006's 8-unit ones.
// Under a Latin overlay ours centres on the width the pen covers. 0 until
// BattleDraw_Inject patches it to 1 under the name BattleListTitleCentre,
// so BOF3X_ORIGINAL=BattleListTitleCentre and the fuzz (before the patch)
// keep the originals' arithmetic.
unsigned char g_list_title_centre = 0;

namespace {
// The x of the title within its window: the original's 6 * (13 - n), or
// under DIV-0059 the same centre, x + 78, less half the real width.
int ListTitleX(const unsigned char* label, int n) {
    if (!g_list_title_centre) return 6 * (13 - n);
    return 78 - static_cast<int>(TextAdvance_Width(label) / 2);
}

constexpr ::bof3::image::CodeVa kTextDrawAt{0x516B30};    // Text_DrawAt: the sites' target in Capcom's bytes, a patch fact (raw)
// The title draws of the two list windows that are still Capcom's: the
// Text_DrawAt call after `6 * (13 - Text_CharCount) + x` in each.
constexpr ::bof3::image::CodeVa kTitleCallA{0x596D13};    // titles 0x66AF10
constexpr ::bof3::image::CodeVa kTitleCallB{0x59DEFA};    // titles 0x66B5C4

// What those two calls reach under DIV-0059: the x they reckoned, 6 * (13 -
// n) + the window's, moved to the centre less half the real width.
extern "C" const unsigned char* __cdecl ListTitle_DrawAt(int x, int y, int colour, int count,
                                                         const unsigned char* text) {
    const int n = static_cast<int>(Text_CharCount(text) & 0xFF);
    return Text_DrawAt(x - 6 * (13 - n) + ListTitle_X(text, n), y, colour, count, text);
}
}  // namespace

int ListTitle_X(const unsigned char* label, int n) { return ListTitleX(label, n); }

// What kTitleCallA and kTitleCallB call now (ListTitle_SiteDraw): a place, the
// game calls through it once OperandCall has re-aimed their calls.
U g_title_site_draw = ::bof3::Code(kTextDrawAt.v);
std::uint32_t ListTitle_SiteDraw() { return g_title_site_draw; }
bool ListTitle_Centring() { return g_list_title_centre != 0; }

// --- the area palette ---------------------------------------------------------------

void BattleDraw_Inject() {
    bof3::OperandCall("DIV-0059 list title draw", kTitleCallA.v, &g_title_site_draw);
    bof3::OperandCall("DIV-0059 list title draw", kTitleCallB.v, &g_title_site_draw);
    if (bof3::WantsShadow("battle_draw")) battle_draw::SelfTest();
    // DIVERGENCE DIV-0059: a Latin language overlay only, as MenuLists_Inject
    // tests it for DIV-0058.
    {
        if (Lang_Latin()) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("BattleListTitleCentre",
                             Low32(&g_list_title_centre),
                             &was, &is, 1);
            bof3::Log("DIV-0059    list titles centred on their width: %s", g_list_title_centre ? "on" : "off");
            const bool a = bof3::RetargetCall("BattleListTitleCentre", kTitleCallA.v, kTextDrawAt.v,
                                              reinterpret_cast<void*>(&ListTitle_DrawAt));
            const bool b = bof3::RetargetCall("BattleListTitleCentre", kTitleCallB.v, kTextDrawAt.v,
                                              reinterpret_cast<void*>(&ListTitle_DrawAt));
            if (a != b) bof3::Fatal("DIV-0059: one title call re-aimed and not the other");   // one name: both or neither
            if (a) g_title_site_draw = Low32(&ListTitle_DrawAt);
        }
    }
}
