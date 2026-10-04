// BOF3X_SHADOW=battle_e7: group BE7's 31 battle-window functions through the
// boss harness as an engine group (boss_harness.h, docs/boss_harness.md
// section 10), once at start-up: one boss_harness::Run. docs/battle_e7.md
// section 5.
//
// The clone rows are tools/band_rows.py's (--group BE7 --clones, 2026-09-29),
// each read against the disassembly. The window handlers (the stack tables'
// entries: Window_Handler4Kinds' kinds 0 and 1, 0x598890's kinds 1..4 and
// their states) are kWindow, the four stack dispatchers with their state byte
// (+3) drawn below their tables; the draws their callers call directly are
// kHelper with this file's words.
//
// The listings below narrow the standard set's masks for the draws these
// windows call where the caller pushes a register whose upper half is another
// call's leftover (ecx / edx after a call, the handler's own ecx at entry):
// each narrowed mask is what our implementation of that callee reads (its
// source cited beside it), so the comparison keeps everything the callee
// uses.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/battle_e7.h"
#include "game/battle_e7_callees.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"

namespace battle_e7 {
namespace {

namespace bh = boss_harness;
using U = std::uint32_t;
using bh::Mem;
using move_script::SetWord;
using S = bh::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define BH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define BH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define BH_FN(name) reinterpret_cast<const void*>(&::name)

bh::Clone C(const char* name, U base, U size, const bh::CallSite* calls, int n, const bh::Imm* imms, int n_imms,
            const void* ours, S shape, std::uint8_t states = 0) {
    bh::Clone c{name, base, size, calls, n, imms, n_imms, nullptr, 0, ours, 0, false, shape};
    if (states) {
        c.state_at = 3;
        c.states = states;
    }
    return c;
}

// ===========================================================================
// The clone rows (tools/band_rows.py --group BE7 --clones)
// ===========================================================================

constexpr bh::CallSite kCalls597FC0[] = {
    {0x15, 0x5982D0}, {0x2B, 0x432170}, {0x44, 0x432170}, {0x59, 0x5B9380}, {0x60, 0x497740}, {0x70, 0x516B30},
    {0x88, 0x432170}, {0xA1, 0x432170}, {0xB6, 0x5B9380}, {0xBD, 0x497740}, {0xCD, 0x516B30}, {0xE4, 0x432170},
    {0xFC, 0x432170}, {0x111, 0x5B9380}, {0x118, 0x497740}, {0x128, 0x516B30}, {0x13F, 0x432170}, {0x158, 0x432170},
    {0x16D, 0x5B9380}, {0x174, 0x497740}, {0x184, 0x516B30}, {0x19A, 0x432170}, {0x1B3, 0x432170}, {0x1C8, 0x5B9380},
    {0x1CF, 0x497740}, {0x1DF, 0x516B30}, {0x1F6, 0x432170}, {0x20E, 0x432170}, {0x223, 0x5B9380}, {0x22A, 0x497740},
    {0x23A, 0x516B30}, {0x286, 0x5B9380}, {0x28D, 0x497740}, {0x29D, 0x516B30}, {0x2E9, 0x5B9380}, {0x2F0, 0x497740},
    {0x300, 0x516B30}};
constexpr bh::CallSite kCalls5982D0[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50}, {0x3F, 0x5A7740},
                                         {0xEB, 0x5A7780}, {0xF4, 0x461E50}, {0x13D, 0x4447B0}, {0x14F, 0x4447B0},
                                         {0x17A, 0x444E00}, {0x197, 0x444E00}, {0x1B6, 0x444D50}, {0x1CA, 0x444D50}};
constexpr bh::CallSite kCalls5984B0[] = {{0x1C, 0x5982D0}, {0x98, 0x598750}};
constexpr bh::CallSite kCalls598750[] = {{0x2E, 0x591720}, {0x48, 0x57D360}, {0x57, 0x591680}, {0x61, 0x57D800},
                                         {0x74, 0x516B30}, {0x93, 0x5B9380}, {0xA7, 0x517090}};
constexpr bh::CallSite kCalls598A30[] = {{0x19, 0x598BE0}, {0x57, 0x57CF60}, {0x7F, 0x57CF60}, {0xAB, 0x57CF60}, {0xF0, 0x516F60},
                                         {0x10A, 0x5B9380}, {0x12D, 0x516F60}, {0x161, 0x5905D0}, {0x198, 0x5905D0}};
constexpr bh::CallSite kCalls598BE0[] = {{0x17, 0x5A79A0},  {0x2F, 0x5A77C0},  {0x38, 0x461E50},  {0x94, 0x57D860},
                                         {0x107, 0x57D860}, {0x116, 0x57D860}, {0x164, 0x57D860}, {0x173, 0x57D860},
                                         {0x199, 0x57D860}, {0x1AE, 0x57D860}, {0x1C3, 0x57D860}, {0x1CE, 0x57D860}};
constexpr bh::Imm kImms598DC0[] = {{0xF, 0x598DF0}, {0x17, 0x598E10}, {0x22, 0x598E50}};
constexpr bh::CallSite kCalls598E10[] = {{0x2E, 0x598E90}};
constexpr bh::CallSite kCalls598E50[] = {{0x2E, 0x598E90}};
constexpr bh::CallSite kCalls598E90[] = {{0x30, 0x57CF60}, {0x57, 0x516B30}, {0x79, 0x516B30}, {0x87, 0x57D910}, {0xBF, 0x5905D0}};
constexpr bh::Imm kImms598F60[] = {{0xF, 0x598FA0}, {0x17, 0x599020}, {0x22, 0x599050}, {0x2A, 0x599080}, {0x32, 0x5990B0}};
constexpr bh::CallSite kCalls598FA0[] = {{0xF, 0x435180}};
constexpr bh::CallSite kCalls599020[] = {{0x0, 0x5990E0}};
constexpr bh::CallSite kCalls599050[] = {{0x0, 0x5990E0}, {0x15, 0x59E310}};
constexpr bh::CallSite kCalls599080[] = {{0x0, 0x5990E0}};
constexpr bh::CallSite kCalls5990B0[] = {{0x0, 0x5990E0}};
constexpr bh::CallSite kCalls5990E0[] = {{0x20, 0x599780},  {0x6D, 0x57DD10},  {0x102, 0x5B9380}, {0x12E, 0x516F60},
                                         {0x16B, 0x599910}, {0x18F, 0x497740}, {0x1B4, 0x516E70}, {0x1CE, 0x5B9380},
                                         {0x1F2, 0x517090}, {0x243, 0x599A00}, {0x268, 0x5905D0}};
constexpr bh::Imm kImms599360[] = {{0xF, 0x599390}, {0x17, 0x599410}, {0x22, 0x599440}};
constexpr bh::CallSite kCalls599390[] = {{0xF, 0x435180}};
constexpr bh::CallSite kCalls599410[] = {{0x0, 0x599570}};
constexpr bh::CallSite kCalls599440[] = {{0x0, 0x599570}, {0x15, 0x59E310}};
constexpr bh::Imm kImms599470[] = {{0xF, 0x5994A0}, {0x17, 0x599510}, {0x22, 0x599540}};
constexpr bh::CallSite kCalls5994A0[] = {{0xF, 0x435180}};
constexpr bh::CallSite kCalls599510[] = {{0x0, 0x599570}};
constexpr bh::CallSite kCalls599540[] = {{0x0, 0x599570}, {0x15, 0x59E310}};
constexpr bh::CallSite kCalls599570[] = {{0x20, 0x599780},  {0x6D, 0x57DD10},  {0x123, 0x599910}, {0x146, 0x497740},
                                         {0x16F, 0x516E70}, {0x188, 0x5B9380}, {0x1AC, 0x517090}, {0x1F1, 0x5905D0}};
constexpr bh::CallSite kCalls599780[] = {{0x26, 0x57CF60},  {0x3D, 0x57CF60},  {0x57, 0x57CF60},  {0x79, 0x57D800},
                                         {0x92, 0x516B30},  {0xA0, 0x57D910},  {0xAE, 0x57D910},  {0xC7, 0x57D860},
                                         {0xE7, 0x57D860},  {0x105, 0x57D860}, {0x11E, 0x57D860}, {0x136, 0x57D860},
                                         {0x144, 0x57D860}, {0x160, 0x57D860}, {0x178, 0x57D860}, {0x183, 0x57D860}};
constexpr bh::CallSite kCalls599910[] = {{0x12, 0x5A79A0}, {0x2A, 0x5A77C0}, {0x33, 0x461E50},
                                         {0x5F, 0x5A79E0}, {0xD0, 0x5A7710}, {0xD9, 0x461E50}};
constexpr bh::CallSite kCalls599A00[] = {{0x61, 0x5A7670}, {0xD8, 0x461E50}, {0xE4, 0x5A7690}, {0x13C, 0x461E50}};
constexpr bh::CallSite kCalls59D640[] = {
    {0x26, 0x57CF60},  {0x42, 0x57D800},  {0x6A, 0x516B30},  {0x80, 0x516B30},  {0x93, 0x516B30},  {0xA6, 0x516B30},
    {0xBC, 0x516B30},  {0xC2, 0x590660},  {0xD8, 0x5B9380},  {0xEC, 0x517090},  {0x102, 0x5B9380}, {0x116, 0x517090},
    {0x12C, 0x5B9380}, {0x13D, 0x517090}, {0x153, 0x5B9380}, {0x164, 0x517090}, {0x18F, 0x590AB0}, {0x1D4, 0x5A77C0},
    {0x1DD, 0x461E50}, {0x200, 0x5A79E0}, {0x219, 0x59DB70}, {0x232, 0x5B9380}, {0x24C, 0x517090}, {0x2CA, 0x591680},
    {0x315, 0x57D9A0}, {0x365, 0x57D360}, {0x36F, 0x57D800}, {0x383, 0x516B30}, {0x3D0, 0x57D360}, {0x3DA, 0x57D800},
    {0x3F1, 0x516B30}, {0x443, 0x57D910}, {0x45C, 0x57D860}, {0x46A, 0x57D860}, {0x48A, 0x57D860}, {0x498, 0x57D860},
    {0x4C5, 0x57D860}, {0x4D4, 0x57D860}, {0x4E9, 0x57D860}, {0x4F7, 0x57D860}, {0x506, 0x57D860}, {0x511, 0x57D860}};

const bh::Clone kClones[] = {
    C("BattleResultWin_DrawLevelUp", 0x597FC0, 0x30A, kCalls597FC0, BH_N(kCalls597FC0), nullptr, 0, BH_FN(BattleResultWin_DrawLevelUp), S::kWindow),
    C("BattleResultWin_DrawFrame", 0x5982D0, 0x1D8, kCalls5982D0, BH_N(kCalls5982D0), nullptr, 0, BH_FN(BattleResultWin_DrawFrame), S::kHelper),
    C("BattleResultWin_DrawDrops", 0x5984B0, 0xB2, kCalls5984B0, BH_N(kCalls5984B0), nullptr, 0, BH_FN(BattleResultWin_DrawDrops), S::kWindow),
    C("BattleResultWin_DrawItem", 0x598750, 0xB3, kCalls598750, BH_N(kCalls598750), nullptr, 0, BH_FN(BattleResultWin_DrawItem), S::kHelper),
    C("GeneWin_DrawGrid", 0x598A30, 0x1A2, kCalls598A30, BH_N(kCalls598A30), nullptr, 0, BH_FN(GeneWin_DrawGrid), S::kHelper),
    C("GeneWin_DrawFrame", 0x598BE0, 0x1DE, kCalls598BE0, BH_N(kCalls598BE0), nullptr, 0, BH_FN(GeneWin_DrawFrame), S::kHelper),
    C("GeneWin_ChoiceStates", 0x598DC0, 0x2E, nullptr, 0, kImms598DC0, BH_N(kImms598DC0), BH_FN(GeneWin_ChoiceStates), S::kWindow, 3),
    C("GeneWin_ChoiceOpen", 0x598DF0, 0x20, nullptr, 0, nullptr, 0, BH_FN(GeneWin_ChoiceOpen), S::kWindow),
    C("GeneWin_ChoiceSlideIn", 0x598E10, 0x37, kCalls598E10, BH_N(kCalls598E10), nullptr, 0, BH_FN(GeneWin_ChoiceSlideIn), S::kWindow),
    C("GeneWin_ChoiceSlideOut", 0x598E50, 0x37, kCalls598E50, BH_N(kCalls598E50), nullptr, 0, BH_FN(GeneWin_ChoiceSlideOut), S::kWindow),
    C("GeneWin_DrawChoices", 0x598E90, 0xCC, kCalls598E90, BH_N(kCalls598E90), nullptr, 0, BH_FN(GeneWin_DrawChoices), S::kHelper),
    C("GeneWin_ListStates", 0x598F60, 0x3E, nullptr, 0, kImms598F60, BH_N(kImms598F60), BH_FN(GeneWin_ListStates), S::kWindow, 5),
    C("GeneWin_ListOpen", 0x598FA0, 0x7E, kCalls598FA0, BH_N(kCalls598FA0), nullptr, 0, BH_FN(GeneWin_ListOpen), S::kWindow),
    C("GeneWin_ListSlideIn", 0x599020, 0x24, kCalls599020, BH_N(kCalls599020), nullptr, 0, BH_FN(GeneWin_ListSlideIn), S::kWindow),
    C("GeneWin_ListSlideOut", 0x599050, 0x22, kCalls599050, BH_N(kCalls599050), nullptr, 0, BH_FN(GeneWin_ListSlideOut), S::kWindow),
    C("GeneWin_ListShiftLeft", 0x599080, 0x2F, kCalls599080, BH_N(kCalls599080), nullptr, 0, BH_FN(GeneWin_ListShiftLeft), S::kWindow),
    C("GeneWin_ListShiftRight", 0x5990B0, 0x2F, kCalls5990B0, BH_N(kCalls5990B0), nullptr, 0, BH_FN(GeneWin_ListShiftRight), S::kWindow),
    C("GeneWin_DrawList", 0x5990E0, 0x274, kCalls5990E0, BH_N(kCalls5990E0), nullptr, 0, BH_FN(GeneWin_DrawList), S::kHelper),
    C("GeneWin_List2States", 0x599360, 0x2E, nullptr, 0, kImms599360, BH_N(kImms599360), BH_FN(GeneWin_List2States), S::kWindow, 3),
    C("GeneWin_List2Open", 0x599390, 0x7E, kCalls599390, BH_N(kCalls599390), nullptr, 0, BH_FN(GeneWin_List2Open), S::kWindow),
    C("GeneWin_List2SlideIn", 0x599410, 0x24, kCalls599410, BH_N(kCalls599410), nullptr, 0, BH_FN(GeneWin_List2SlideIn), S::kWindow),
    C("GeneWin_List2SlideOut", 0x599440, 0x22, kCalls599440, BH_N(kCalls599440), nullptr, 0, BH_FN(GeneWin_List2SlideOut), S::kWindow),
    C("GeneWin_List3States", 0x599470, 0x2E, nullptr, 0, kImms599470, BH_N(kImms599470), BH_FN(GeneWin_List3States), S::kWindow, 3),
    C("GeneWin_List3Open", 0x5994A0, 0x6B, kCalls5994A0, BH_N(kCalls5994A0), nullptr, 0, BH_FN(GeneWin_List3Open), S::kWindow),
    C("GeneWin_List3SlideIn", 0x599510, 0x2F, kCalls599510, BH_N(kCalls599510), nullptr, 0, BH_FN(GeneWin_List3SlideIn), S::kWindow),
    C("GeneWin_List3SlideOut", 0x599540, 0x22, kCalls599540, BH_N(kCalls599540), nullptr, 0, BH_FN(GeneWin_List3SlideOut), S::kWindow),
    C("GeneWin_DrawList2", 0x599570, 0x201, kCalls599570, BH_N(kCalls599570), nullptr, 0, BH_FN(GeneWin_DrawList2), S::kHelper),
    C("GeneWin_DrawListFrame", 0x599780, 0x190, kCalls599780, BH_N(kCalls599780), nullptr, 0, BH_FN(GeneWin_DrawListFrame), S::kHelper),
    C("GeneWin_DrawFormIcon", 0x599910, 0xE4, kCalls599910, BH_N(kCalls599910), nullptr, 0, BH_FN(GeneWin_DrawFormIcon), S::kHelper),
    C("GeneWin_DrawCursorBox", 0x599A00, 0x148, kCalls599A00, BH_N(kCalls599A00), nullptr, 0, BH_FN(GeneWin_DrawCursorBox), S::kHelper),
    C("BattleEquipWin_Draw", 0x59D640, 0x521, kCalls59D640, BH_N(kCalls59D640), nullptr, 0, BH_FN(BattleEquipWin_Draw), S::kHelper),
};

// ===========================================================================
// The callees
// ===========================================================================

// Equip_PreviewSet(member, set, marks, values): the caller's two stack
// buffers filled as the real one fills them - a mark 0..4 a stat (the caller
// compares it with 4 and 1) and a value a stat - and noted, so the draws
// after read the same bytes on both passes.
U PreviewEffect(const U* a, U answer) {
    auto* const marks = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[2]));
    auto* const values = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[3]));
    for (unsigned i = 0; i < 4; ++i) marks[i] = static_cast<unsigned char>(bh::Noise() % 6);
    bh::FillBytes(values, 8);
    bh::NoteBytes(marks, 4);
    bh::NoteBytes(values, 8);
    return answer;
}

const bh::Callee kCallees[] = {
    // --- the group's own, called directly
    {"BattleResultWin_DrawFrame", 0x5982D0, KeyOf(&::BattleResultWin_DrawFrame), 4, {kU16, kU16, kU16, kU16}, bh::Answer::kGarbage, 0, 0},
    {"BattleResultWin_DrawItem", 0x598750, KeyOf(&::BattleResultWin_DrawItem), 7, {kAll, kAll, kAll, kU8, kAll, kU8, kAll}, bh::Answer::kGarbage, 0, 0},
    {"GeneWin_DrawFrame", 0x598BE0, KeyOf(&::GeneWin_DrawFrame), 4, {kU16, kU16, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"GeneWin_DrawChoices", 0x598E90, KeyOf(&::GeneWin_DrawChoices), 2, {kU16, kU16}, bh::Answer::kGarbage, 0, 0},
    {"GeneWin_DrawList", 0x5990E0, KeyOf(&::GeneWin_DrawList), 0, {at::kWindowCurrent}, bh::Answer::kPhase, 0, 0},
    {"GeneWin_DrawList2", 0x599570, KeyOf(&::GeneWin_DrawList2), 0, {at::kWindowCurrent}, bh::Answer::kPhase, 0, 0},
    {"GeneWin_DrawListFrame", 0x599780, KeyOf(&::GeneWin_DrawListFrame), 5, {kU16, kU16, 0, 0, kU8}, bh::Answer::kGarbage, 0, 0},
    {"GeneWin_DrawFormIcon", 0x599910, KeyOf(&::GeneWin_DrawFormIcon), 3, {kU16, kU16, kU8}, bh::Answer::kGarbage, 0, 0},
    {"GeneWin_DrawCursorBox", 0x599A00, KeyOf(&::GeneWin_DrawCursorBox), 6, {kU16, kU16, kU16, kU16, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    // --- another group's (BE1's 0x432170) and nobody's (0x59DB70), by address
    {"0x432170", at::kStatGain, at::kStatGain, 2, {kU8, kU8}, bh::Answer::kFlag, 0, 0},
    {"0x59DB70", at::kDrawBar, at::kDrawBar, 6, {kU16, kU16, kU8, kU8, kU16, kU8}, bh::Answer::kGarbage, 0, 0},   // reads x, y and the clut as words (0x59DB70's and / cx)
    // --- the draws, narrowed to what ours reads (see the header)
    {"Menu_DrawBox", bof3::addr::Menu_DrawBox, KeyOf(&::Menu_DrawBox), 6, {kU16, kU16, kU16, kU16, kU8, kU8}, bh::Answer::kGarbage, 0, 0},   // menu_windows.cpp: U16 x/y/w/h, bytes
    {"Menu_DrawPiece", bof3::addr::Menu_DrawPiece, KeyOf(&::Menu_DrawPiece), 4, {kU16, kU16, kU8, kU8}, bh::Answer::kGarbage, 0, 0},   // S16 x/y, Menu_PieceRect's id & 0xFF, the flag byte
    {"Menu_DrawPieces", bof3::addr::Menu_DrawPieces, KeyOf(&::Menu_DrawPieces), 4, {kU16, kU16, kAll, kU8}, bh::Answer::kGarbage, 0, 0},
    {"Menu_DrawHand", bof3::addr::Menu_DrawHand, KeyOf(&::Menu_DrawHand), 3, {kU16, kU16, 0}, bh::Answer::kGarbage, 0, 0},   // char_stats.cpp: x & 0xFFFF, y & 0xFFFF
    {"Menu_DrawIcon8", bof3::addr::Menu_DrawIcon8, KeyOf(&::Menu_DrawIcon8), 4, {kU16, kU16, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"Menu_DrawScrollBar", bof3::addr::Menu_DrawScrollBar, KeyOf(&::Menu_DrawScrollBar), 7, {kAll, kU8, kU16, kU16, kU8, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"Text_DrawAt", bof3::addr::Text_DrawAt, KeyOf(&::Text_DrawAt), 5, {kU16, kU16, kU8, kU8, 0}, bh::Answer::kGarbage, 0, 0, {}, &bh::TextArg4Effect},   // msgbox.cpp: shorts; Text_DrawString bytes
    {"Text_DrawSmall", bof3::addr::Text_DrawSmall, KeyOf(&::Text_DrawSmall), 5, {kU16, kU16, kU8, kU8, 0}, bh::Answer::kGarbage, 0, 0, {}, &bh::TextArg4Effect},   // the pen stored as words
    {"Text_DrawFont12", bof3::addr::Text_DrawFont12, KeyOf(&::Text_DrawFont12), 4, {kU16, kU16, kU8, 0}, bh::Answer::kGarbage, 0, 0, {}, &bh::TextArg3Effect},   // S16 pen, colour & 0x3F
    {"Text_DrawFont8", bof3::addr::Text_DrawFont8, KeyOf(&::Text_DrawFont8), 4, {kU16, kU16, kU8, 0}, bh::Answer::kGarbage, 0, 0, {}, &bh::TextArg3Effect},
    {"BattleWin_DrawQuadF4", bof3::addr::BattleWin_DrawQuadF4, KeyOf(&::BattleWin_DrawQuadF4), 4, {kU16, kU16, kU8, kU8}, bh::Answer::kGarbage, 0, 0},   // battle_window_draw.cpp: S16, bytes
    {"BattleWin_DrawLineAdd", bof3::addr::BattleWin_DrawLineAdd, KeyOf(&::BattleWin_DrawLineAdd), 7, {kU16, kU16, kU16, kU16, kU8, kU8, kU8}, bh::Answer::kGarbage, 0, 0},   // the colours' low bytes
    {"BattleWin_DrawLineHalf", bof3::addr::BattleWin_DrawLineHalf, KeyOf(&::BattleWin_DrawLineHalf), 7, {kU16, kU16, kU16, kU16, kU8, kU8, kU8}, bh::Answer::kGarbage, 0, 0},
    {"Item_CanUse", bof3::addr::Item_CanUse, KeyOf(&::Item_CanUse), 4, {kU8, kU8, kU8, kU8}, bh::Answer::kFlag, 0, 0},   // menu_windows.cpp: mode 2 reads the category and the item as bytes
    {"Item_IconKind", bof3::addr::Item_IconKind, KeyOf(&::Item_IconKind), 2, {kU8, kU8}, bh::Answer::kGarbage, 0, 0},   // char_stats.cpp: bytes
    {"Equip_PreviewSet", bof3::addr::Equip_PreviewSet, KeyOf(&::Equip_PreviewSet), 4, {kAll, kAll, 0, 0}, bh::Answer::kGarbage, 0, 0, {}, &PreviewEffect},
};

// The group's cells beyond the engine frame: the sprintf buffers the windows
// draw (0x904BA0; the level-up window's eight at 0x904D20) and the two lists'
// scroll marks 0x939848 / 0x939850.
const bh::Region kRegions[] = {
    {at::kPrintBuf, 0x20},
    {at::kLevelBufs, 0xF0},
    {0x939840, 0x20},
};

// ===========================================================================
// The seeds, the words, the disturbance
// ===========================================================================

unsigned char Byte(std::initializer_list<unsigned> v) {
    const unsigned n = static_cast<unsigned>(v.size());
    return static_cast<unsigned char>(v.begin()[bh::Next() % n]);
}
unsigned Word(std::initializer_list<unsigned> v) {
    const unsigned n = static_cast<unsigned>(v.size());
    return v.begin()[bh::Next() % n] & 0xFFFF;
}
unsigned char* Rec() { return bh::CurrentWindow(); }
unsigned Word16(U address) { return move_script::Word(Mem(address)); }

// A slide's +4 / +6 at and around its bound, or anything.
void Slide(unsigned at, std::initializer_list<unsigned> bounds) {
    if (!bh::Often()) return;
    const int delta = static_cast<int>(Byte({0, 0, 1, 2, 0x10, 0x20})) * (bh::Half() ? 1 : -1);
    SetWord(Rec() + at, (Word(bounds) + static_cast<unsigned>(delta)) & 0xFFFF);
}

// The menu actor's member byte inside the party, and its AP about a cost.
void ActorAp(unsigned cost) {
    unsigned char* const actor = bh::Pointer(at::kMenuActor);
    if (bh::Often()) actor[5] = static_cast<unsigned char>(bh::Next() % 3);
    unsigned char* const member = Mem(at::kPartyAp + (actor[5] % 3) * at::kPartyStride);
    if (bh::Often()) SetWord(member, (cost + Byte({0, 0, 1, 0xFF, 2, 0x80})) & 0xFFFF);
}

// A gene list's rows: cost indices (0xFF after the first sometimes), form
// bytes (0xFF sometimes, else 0..31 and beyond), the record's top row inside
// the list.
void Rows(U list, unsigned rows, unsigned top_max) {
    for (unsigned r = 0; r < rows; ++r) {
        unsigned char* const row = Mem(list + 4 * r);
        if (bh::Often()) row[0] = static_cast<unsigned char>(bh::Next() % 20);
        row[1] = bh::Half() ? 0xFF : static_cast<unsigned char>(bh::Often() ? bh::Next() % 20 : bh::Next());
        row[2] = bh::Half() ? 0xFF : static_cast<unsigned char>(bh::Often() ? bh::Next() % 20 : bh::Next());
        row[3] = bh::Often() ? (bh::Half() ? 0xFF : static_cast<unsigned char>(bh::Next() % 32)) : static_cast<unsigned char>(bh::Next());
    }
    unsigned char* const rec = Rec();
    if (bh::Often()) SetWord(rec + 0x12, bh::Next() % (top_max + 1));
    if (bh::Often()) SetWord(rec + 0x10, bh::Next() % 3);
    Mem(at::kStep2)[0] = Byte({6, 6, 4, 0});
    Mem(at::kStep3)[0] = Byte({5, 5, 0, 1});
    ActorAp(Byte({0, 3, 10, 20, 0x40}));
}

const bh::Clone* g_cur = kClones;
// 0x59D640's record (its sixth word): a window record, drawn by the seed.
unsigned char* g_record;

void Seed(unsigned k) {
    unsigned char* const rec = Rec();
    switch (g_cur[k].base) {
    case 0x597FC0:   // the member and the level inside the table; the gains through 0x432170's recorder
        if (bh::Often()) rec[0xA] = static_cast<unsigned char>(bh::Next() % 8);
        if (bh::Often()) rec[0xB] = static_cast<unsigned char>(bh::Next() % 99);
        break;
    case 0x5984B0: {  // the drops: a count 0..8, words zero sometimes
        Mem(at::kDropCount)[0] = Byte({0, 1, 2, 3, 5, 8, 8});
        for (unsigned i = 0; i < 8; ++i) {
            if (bh::Half()) SetWord(Mem(at::kDropItems + 2 * i), 0);
            if (bh::Half()) Mem(at::kDropCounts + i)[0] = Byte({0, 1, 2, 9});
        }
        break;
    }
    case 0x598A30:   // the grid: step 4, the row 0xFF or a cell, the cost about the AP
        Mem(at::kStep2)[0] = Byte({4, 4, 3, 5});
        Mem(at::kGeneRow)[0] = Byte({0xFF, 0xFF, 0, 1, 2, 3});
        Mem(at::kGeneColumn)[0] = Byte({0, 1, 2, 3, 4});
        Mem(at::kApCost)[0] = Byte({0, 1, 4, 8, 0x10, 0xFF});
        ActorAp(Mem(at::kApCost)[0]);
        break;
    case 0x598E10: Slide(6, {0x29, 0x19, 0xFFE9}); break;
    case 0x598E50: Slide(6, {0xFFE9, 0xFFF9, 0x29}); break;
    case 0x598E90:   // the choice: fixed or not, the box 0..2 mostly
        Mem(at::kGeneChoice)[0] = static_cast<unsigned char>((bh::Half() ? 0x80 : 0) | (bh::Often() ? bh::Next() % 3 : bh::Next() & 0x7F));
        break;
    // the slide-outs' bounds as the code holds them: DIV-0041 widens them
    // before this runs when the view is wide (battle_e7.cpp reads them there)
    case 0x599020: case 0x599050: case 0x599080: case 0x5990B0: {
        const unsigned out = Word16(at::kListOutBound);
        Slide(4, {0x5B, 0x3B, 0x11, out, out + 0x20, 0x31});
        Rows(at::kGeneList, 6, 3);
        break;
    }
    case 0x5990E0: Rows(at::kGeneList, 6, 3); break;
    case 0x599410: case 0x599440: case 0x599510: case 0x599540: {
        const unsigned out2 = Word16(at::kList2OutBound), out3 = Word16(at::kList3OutBound);
        Slide(4, {0x5B, 0x7B, out2, out2 - 0x20, 0xA3, 0xC3, out3, out3 - 0x20});
        Rows(at::kGeneList2, 12, 9);
        break;
    }
    case 0x599570: Rows(at::kGeneList2, 12, 9); break;
    case 0x59D640: {  // the record's rows +0xA / +0xB, +0xD bit 1, BATE's member
        g_record = bh::WindowAt(bh::Next());
        unsigned char* const r = g_record;
        if (bh::Often()) r[0xA] = static_cast<unsigned char>(bh::Next() % 7);
        if (bh::Often()) r[0xB] = static_cast<unsigned char>(bh::Next() % 7);
        Mem(at::kFieldMember)[0] = Byte({0, 1, 2, 0xFF});
        break;
    }
    default:
        break;
    }
}

// The words of each kHelper: coordinates (a word, garbage above it half the
// time), the bytes inside the tables they index.
U Coord() { return (bh::Half() ? bh::Next() & 0xFFFF0000u : 0) | (bh::Often() ? bh::Next() % 0x140 : bh::Next() & 0xFFFF); }
U Small(unsigned below) { return (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Often() ? bh::Next() % below : bh::Next() & 0xFF); }

void Args(unsigned k, U* a) {
    switch (g_cur[k].base) {
    case 0x5982D0: a[0] = Coord(); a[1] = Coord(); a[2] = Coord(); a[3] = Small(0x80); break;
    case 0x598750:
        a[0] = Coord(); a[1] = Coord(); a[2] = Small(8);
        a[3] = Small(5); a[4] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | Byte({0, 1, 2, 0x40, 0xFF});
        a[5] = Small(4); a[6] = Small(2);
        break;
    case 0x598BE0:   // cells: small grids, never so many that the pieces overrun the log (64 x 64 at most)
        a[0] = Coord(); a[1] = Coord();
        a[2] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Often() ? bh::Next() % 12 : bh::Next() % 64);
        a[3] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Often() ? bh::Next() % 12 : bh::Next() % 64);
        break;
    case 0x598E90: a[0] = Coord(); a[1] = Coord(); break;
    case 0x599780: a[0] = Coord(); a[1] = Coord(); a[2] = bh::Next(); a[3] = bh::Next(); a[4] = Small(2); break;
    case 0x599910: a[0] = Coord(); a[1] = Coord(); a[2] = Small(32); break;
    case 0x599A00:
        a[0] = Coord(); a[1] = Coord(); a[2] = Coord(); a[3] = Coord();
        a[4] = Small(2); a[5] = Small(8);
        break;
    case 0x59D640:
        a[0] = Small(3); a[1] = Coord(); a[2] = Coord(); a[3] = bh::Next();
        a[4] = Small(4); a[5] = Key(g_record);
        break;
    default:
        break;
    }
}

// What the windows read again after a call: the drops' count, the window
// colour, the choice, the cost, the hand's row and column, a row of the gene
// lists, BATE's member byte.
void Disturb(U h) {
    const auto b = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 8) {
    case 0: Mem(at::kDropCount)[0] = static_cast<unsigned char>(b % 9); break;
    case 1: Mem(at::kColour)[0] = b; break;
    case 2: Mem(at::kGeneChoice)[0] = static_cast<unsigned char>((b & 0x80) | (b % 3)); break;
    case 3: Mem(at::kApCost)[0] = b; break;
    case 4: Mem(at::kGeneRow)[0] = h & 0x1000000 ? 0xFF : static_cast<unsigned char>(b % 4); break;
    case 5: Mem(at::kGeneList + (h >> 24) % 0x48)[0] = b; break;
    case 6: Mem(at::kFieldMember)[0] = static_cast<unsigned char>(b % 3); break;
    default: Mem(at::kGeneColumn)[0] = static_cast<unsigned char>(b % 5); break;
    }
}

}  // namespace

void SelfTest() {
    bh::Group g{"battle_e7", kClones, BH_COUNT(kClones), kCallees, BH_COUNT(kCallees), nullptr, 0, kRegions, BH_COUNT(kRegions),
                &Seed, &Disturb, 6000};
    g.args = &Args;
    g.engine = true;
    bh::Run(g);
}

}  // namespace battle_e7
