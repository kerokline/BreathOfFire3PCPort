// BOF3X_SHADOW=area_w4a: world 4's areas 152..155, 166 and 167 through the
// area round's shared harness (area_harness.h), once at start-up: one
// area_harness::Run per area (Group::area its number, the real descriptor and
// tables in place, area 152's .data state tables swapped through DataTable).
// docs/area_w4a.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA152..155, 166, 167
// --clones, 2026-09-28), each read against the disassembly (a scratch E8 / E9
// scan agrees site for site); area 152's world map is area 87's rows
// (area_w2b_fuzz.cpp) at its addresses, its field hook its own, and its group
// area 87's with area 152's tables.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4a.h"
#include "game/area_w4a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w4a {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using ah::Mem;
using S = ah::Shape;
using at::WorldMapTables;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define AH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define W4A_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W4A_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone answering in al.
#define W4A_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}

constexpr U kScriptObject = 0x929E80;     // MoveScript_Object
constexpr U kWaitWordDA = 0x66C810;       // MoveScript_WaitWordDA (u16)
constexpr U kByteFE = 0x905E20;           // Cond_ByteFE
constexpr U kByteFD = 0x8034F1;           // Cond_ByteFD

const ah::Callee kSet40 = {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kClear40 = {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0};
// the index pushed with bits above its byte (the cell hook's: the switch
// search's pointer)
const ah::Callee kToggle = {"Flags_Toggle", bof3::addr::Flags_Toggle, KeyOf(&::Flags_Toggle), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0};

// A party record or one of the first four field objects.
unsigned char* SomeRecord(U h) { return (h & 1) ? ah::PartyOf(static_cast<unsigned char>(h >> 1)) : ah::TaskAt(h >> 1); }

// ===========================================================================
// Area 152: the world map's eleventh copy (area 87's group, area_w2b_fuzz.cpp),
// its init and the record-8 state 0 every world map shares
// ===========================================================================

// --- the fuzz's own memory: a packet buffer, four map items, four names ------

constexpr unsigned kPacketBytes = 0x200, kItemBytes = 0x48;
alignas(16) unsigned char g_packets[kPacketBytes];
alignas(16) unsigned char g_items[4][kItemBytes];
alignas(16) unsigned char g_names[4][16];

bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }

void Advance(U size) {
    U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x50)) next = Key(g_packets) + (ah::Noise() % 4) * 4;
    Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
void Scribble(U at, unsigned n) {
    if (!InPackets(at, n)) return;
    unsigned char* const p = Mem(at);
    for (unsigned i = 0; i < n; i += 4) SetLong(p + i, static_cast<std::int32_t>(ah::Noise()));
}

// --- the effects: the callees that write what the caller reads after ---------

U CommitEffect(const U* a, U answer) { Advance(a[1] & 0xFF); return answer; }
U LinkEffect(const U* a, U answer) {
    if (ah::Noise() % 5) Advance(a[3] & 0xFF);
    return answer;
}
U ByteAtEffect(const U*, U answer) {
    static const U kCells[] = {0xA1, 0xA1, 0xA0, 0xAE, 0xA2, 0x9F, 0, 0x21};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 5 == 0 ? (n >> 8) & 0xFF : kCells[(n >> 4) % 8]);
}
U NameEffect(const U*, U answer) { return Key(g_names[(answer >> 4) % 4]); }
U ItemEffect(const U*, U answer) { return answer % 3 == 0 ? 0u : Key(g_items[(answer >> 4) % 4]); }
U PolyEffect(const U* a, U answer) {
    Scribble(a[0], 0x48);
    if (InPackets(a[0], 8)) Mem(a[0])[7] = 0x2C;
    return answer;
}
U SprtEffect(const U* a, U answer) { Scribble(a[0], 0x20); return answer; }
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U ShadeEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 1 : Mem(a[0])[7] & 0xFE);
    return answer;
}
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 2 : Mem(a[0])[7] & 0xFD);
    return answer;
}
U PersEffect(const U* a, U answer) {
    for (unsigned c = 4; c < 8; ++c) Scribble(a[c], 8);
    return answer;
}
U DepthEffect(const U* a, U answer) {
    for (U z = 0x10; z <= 0x40; z += 0x10) Scribble(a[0] + z, 4);
    return answer;
}
U TextureEffect(const U* a, U answer) {
    if (InPackets(a[1], 0x18)) Mem(a[1])[0x16] = static_cast<unsigned char>(a[0]);
    return answer;
}
// Effect_FindFree: a slot of the twenty mostly, none (0xFF) one call in five
// (the spawn loop ends there), garbage above the byte.
U FindFreeEffect(const U*, U answer) {
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 5 == 0 ? 0xFFu : (n >> 8) % at::kEffectCount);
}

const ah::Callee kPers4 = {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, KeyOf(&::Gte_RotTransPers4), 10,
                           {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, ah::Answer::kGarbage, 0, 0,
                           {8, 8, 8, 8}, &PersEffect, nullptr};

// --- the clones --------------------------------------------------------------

// clones: tools/area_rows.py --unit AREA152 --clones
constexpr ah::CallSite kCalls4249D0[] = {{0x7, 0x57C110}, {0x13, 0x57C110}, {0x22, 0x57C110}, {0x31, 0x57C110}, {0x40, 0x57C110}, {0x4F, 0x57C110}};
constexpr ah::CallSite kCalls424A30[] = {{0x18, 0x57C7A0}, {0x33, 0x57C7C0}, {0x6B, 0x4976D0}};
constexpr ah::CallSite kCalls424AC0[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls424BA0[] = {{0xE, 0x589590}};
constexpr ah::CallSite kCalls424C00[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls424D50[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls424DA0[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls424E00[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls424E70[] = {{0x0, 0x424E80}, {0x5, 0x424F50}};
constexpr ah::CallSite kCalls424EA0[] = {{0x1C, 0x424ED0}};
constexpr ah::CallSite kCalls424ED0[] = {{0x1F, 0x4250B0}};
constexpr ah::CallSite kCalls424F00[] = {{0x38, 0x4250B0}};
constexpr ah::CallSite kCalls424F70[] = {{0x59, 0x425340}};
constexpr ah::CallSite kCalls424FE0[] = {{0x65, 0x425340}};
constexpr ah::CallSite kCalls425050[] = {{0x4E, 0x425340}};
constexpr ah::CallSite kCalls4250B0[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x425280}, {0x51, 0x536700}, {0x7B, 0x425280}, {0xDC, 0x425280}, {0xF8, 0x425280}, {0x15B, 0x425280}, {0x173, 0x531920}, {0x1A8, 0x425280}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls425280[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls425340[] = {{0x17, 0x425280}, {0x26, 0x425280}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls4253C0[] = {{0xF, 0x589840}, {0x27, 0x5B93D2}, {0x3D, 0x589810}, {0x8D, 0x5720C0}};
constexpr ah::CallSite kCalls425470[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls4255F0[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls4256B0[] = {{0xB7, 0x5A75D0}, {0xBF, 0x5A77A0}, {0x199, 0x5A85F0}, {0x19F, 0x5A9290}, {0x1AF, 0x572A00}, {0x1BB, 0x461E50}, {0x21D, 0x572F70}, {0x236, 0x5A75D0}, {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};

const ah::Clone kClones152[] = {
    W4A_C(Area152_PlaceMessage, 0x424A30, 0x87, kCalls424A30, S::kState),
    W4A_C(Area152_PlateRun, 0x424AC0, 0xD6, kCalls424AC0, S::kState),
    W4A_C(Area152_PlateStart, 0x424BA0, 0x51, kCalls424BA0, S::kState),
    W4A_C(Area152_PlateShow, 0x424C00, 0x142, kCalls424C00, S::kState),
    W4A_C(Area152_PlateGrow, 0x424D50, 0x41, kCalls424D50, S::kState),
    W4A_C(Area152_PlateHold, 0x424DA0, 0x58, kCalls424DA0, S::kState),
    W4A_C(Area152_PlateShrink, 0x424E00, 0x50, kCalls424E00, S::kState),
    W4A_P(Area152_HudRun, 0x424E50, 0x12, S::kState),
    W4A_C(Area152_HudFrame, 0x424E70, 0xA, kCalls424E70, S::kState),
    W4A_P(Area152_FrameStep, 0x424E80, 0x12, S::kCallee),
    W4A_C(Area152_FrameSlideIn, 0x424EA0, 0x21, kCalls424EA0, S::kState),
    W4A_C(Area152_FrameHold, 0x424ED0, 0x28, kCalls424ED0, S::kState),
    W4A_C(Area152_FrameSlideOut, 0x424F00, 0x41, kCalls424F00, S::kState),
    W4A_P(Area152_BoxStep, 0x424F50, 0x12, S::kCallee),
    W4A_C(Area152_BoxSlideIn, 0x424F70, 0x62, kCalls424F70, S::kState),
    W4A_C(Area152_BoxHold, 0x424FE0, 0x6E, kCalls424FE0, S::kState),
    W4A_C(Area152_BoxSlideOut, 0x425050, 0x57, kCalls425050, S::kState),
    W4A_C(Area152_DrawFrame, 0x4250B0, 0x1C5, kCalls4250B0, S::kCallee),
    W4A_C(Area152_DrawSprite, 0x425280, 0xBC, kCalls425280, S::kCallee),
    W4A_C(Area152_DrawHud, 0x425340, 0x58, kCalls425340, S::kCallee),
    W4A_P(Area152_Record8Run, 0x4253A0, 0x12, S::kState),
    W4A_C(Area152_Record8Place, 0x425470, 0x154, kCalls425470, S::kState),
    W4A_P(Area152_Record4Run, 0x4255D0, 0x12, S::kState),
    W4A_C(Area152_Record4MarkCell, 0x4255F0, 0xB5, kCalls4255F0, S::kState),
    W4A_C(Area152_DrawDrift, 0x4256B0, 0x462, kCalls4256B0, S::kState),
    W4A_C(Area152_Init, 0x4249D0, 0x58, kCalls4249D0, S::kInit),
    W4A_C(Area152_Record8Spawn, 0x4253C0, 0xAD, kCalls4253C0, S::kState),
};
enum : unsigned {
    kPlaceMessage, kPlateRun, kPlateStart, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame,
    kFrameStep, kFrameSlideIn, kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame,
    kDrawSprite, kDrawHud, kRecord8Run, kRecord8Place, kRecord4Run, kRecord4MarkCell, kDrawDrift, kInit152, kSpawn,
};
static_assert(kSpawn + 1 == AH_COUNT(kClones152), "area 152's seeding indices");

// --- the callees: the standard set's, with what the callers read after -------

const ah::Callee kCallees152[] = {
    {"Area152_FrameStep", at::kWm152.fn_frame_step, at::kWm152.fn_frame_step, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area152_BoxStep", at::kWm152.fn_box_step, at::kWm152.fn_box_step, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area152_DrawFrame", at::kWm152.fn_draw_frame, at::kWm152.fn_draw_frame, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},
    {"Area152_DrawSprite", at::kWm152.fn_draw_sprite, at::kWm152.fn_draw_sprite, 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"Area152_DrawHud", at::kWm152.fn_draw_hud, at::kWm152.fn_draw_hud, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},
    kSet40, kClear40,
    {"WorldMap_PinSprite", bof3::addr::WorldMap_PinSprite, KeyOf(&::WorldMap_PinSprite), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Effect_Release", bof3::addr::Effect_Release, KeyOf(&::Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Effect_FindFree", bof3::addr::Effect_FindFree, KeyOf(&::Effect_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &FindFreeEffect, nullptr},
    {"Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, KeyOf(&::Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect, nullptr},
    {"WorldMap_DrawNeedle", bof3::addr::WorldMap_DrawNeedle, KeyOf(&::WorldMap_DrawNeedle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"WorldMap_RecordIndex", bof3::addr::WorldMap_RecordIndex, KeyOf(&::WorldMap_RecordIndex), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Prim_SetTexture", bof3::addr::Prim_SetTexture, KeyOf(&::Prim_SetTexture), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &TextureEffect, nullptr},
    {"AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, KeyOf(&::AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect, nullptr},
    {"Field_CellHasEvent", bof3::addr::Field_CellHasEvent, KeyOf(&::Field_CellHasEvent), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    {"MapView_ItemHalfAt", bof3::addr::MapView_ItemHalfAt, KeyOf(&::MapView_ItemHalfAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ItemEffect, nullptr},
    {"MapView_LinkPrimAt", bof3::addr::MapView_LinkPrimAt, KeyOf(&::MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect, nullptr},
    {"Gpu_SetPolyFT4", bof3::addr::Gpu_SetPolyFT4, KeyOf(&::Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr},
    {"Gpu_SetShadeTex", bof3::addr::Gpu_SetShadeTex, KeyOf(&::Gpu_SetShadeTex), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ShadeEffect, nullptr},
    {"Gpu_SetSemiTrans", bof3::addr::Gpu_SetSemiTrans, KeyOf(&::Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect, nullptr},
    {"Gpu_SetSprt", bof3::addr::Gpu_SetSprt, KeyOf(&::Gpu_SetSprt), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SprtEffect, nullptr},
    {"Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, KeyOf(&::Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect, nullptr},
    {"Gte_PrimDepths4_10", bof3::addr::Gte_PrimDepths4_10, KeyOf(&::Gte_PrimDepths4_10), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DepthEffect, nullptr},
    kPers4,
};

const ah::DataTable kTables152[] = {
    {at::kWm152.plate_states, 5}, {at::kWm152.hud_states, 2}, {at::kWm152.frame_states, 4},
    {at::kWm152.box_states, 4},   {at::kWm152.record8_states, 3}, {at::kWm152.record4_states, 2},
};

// --- the fuzz's own literal table addresses (read off the exe, docs/area_w4a.md
// section 2): the seed plants and the regions are built from these, never from
// the kWm152 ours reads, so a wrong constant in ours shows. The state tables'
// DataTables and the callees stay on kWm152 (a wrong one there moves the
// swapped entries away from what ours reads, which shows too).
constexpr U kFzRows = 0x637344, kFzRowsEnd = 0x6373A4;    // 3 place rows
constexpr U kFzAnims = 0x637288, kFzAnimsEnd = 0x637294;  // 3 plate animations
constexpr U kFzCells = 0x637294;                          // 2 cell records
constexpr unsigned kFzCellRecords = 2;
constexpr U kFzDirections = 0x63745C, kFzDriftU = 0x63747C, kFzButtons = 0x637438;
constexpr unsigned kRows = (kFzRowsEnd - kFzRows) / 0x20, kAnims = (kFzAnimsEnd - kFzAnims) / 4;

// --- the regions: the harness's plus the copy's tables and the effects -------

enum : unsigned { kRegPackets = 3, kRegItems = 7, kRegNames = 8 };
ah::Region g_regions152[] = {
    {at::kMapMode, 1}, {0x7E0918, 1}, {0x7E0670, 4}, {0, kPacketBytes}, {0x9037A0, 0x20}, {0x66C7E8, 2},
    {at::kButtonMap0, 0x10}, {0, 4 * kItemBytes}, {0, sizeof g_names},
    {at::kAreaText, 4}, {at::kFlag3A79, 1},
    {kFzAnims, kFzAnimsEnd - kFzAnims},
    {kFzCells, 4 * kFzCellRecords},
    {kFzRows, kFzRowsEnd - kFzRows},
    {kFzDirections, 0x18}, {kFzDriftU, 0xC},
    {at::kEffects, at::kEffectCount * at::kEffectStride},
};

// The exe's own bytes of the tables the fuzz randomises, put back two rounds
// in three (so the seeds see the shipped values most of the time).
struct Saved {
    unsigned char anims[0xC], cells[8], rows[0x60], dirs[0x18], drift[0xC];
};
Saved g_saved;

void SaveTables() {
    std::memcpy(g_saved.anims, Mem(kFzAnims), sizeof g_saved.anims);
    std::memcpy(g_saved.cells, Mem(kFzCells), sizeof g_saved.cells);
    std::memcpy(g_saved.rows, Mem(kFzRows), sizeof g_saved.rows);
    std::memcpy(g_saved.dirs, Mem(kFzDirections), sizeof g_saved.dirs);
    std::memcpy(g_saved.drift, Mem(kFzDriftU), sizeof g_saved.drift);
}

unsigned char* Obj() { return Sprite_Current; }

void DisturbWm(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 12) {
    case 0: Mem(at::kMapMode)[0] = static_cast<unsigned char>(v % 4 == 0 ? v : v % 3); break;
    case 1: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ (v & 1 ? 4 : 0x1B)); break;
    case 2: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (v & 1 ? 0x100 : 0x4000)); break;
    case 3: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000); break;
    case 4: Mem(at::kMsgState)[0] = static_cast<unsigned char>(v % 3); break;
    case 5: Game_Mode = static_cast<unsigned short>(Game_Mode ^ 1); break;
    case 6: Cond_ByteFA = static_cast<signed char>(v % 16); break;
    case 7: SetWord(Mem(at::kPlace), Word(Mem(kFzRows + (v % kRows) * 0x20))); break;   // a row's place (the settle re-plants the plate entry)
    case 8: Mem(at::kLeaderCellX + (v & 1) * 2)[0] = static_cast<unsigned char>(v); break;
    case 9: Mem(at::kPartySet)[0] = static_cast<unsigned char>(v & 1 ? 0xC : v); break;
    case 10: Field_StatusBits = static_cast<unsigned char>(Field_StatusBits ^ 1); break;
    case 11: Frame_Counter = (Frame_Counter & ~0x3FFu) | (v & 1 ? 0 : v << 2); break;
    default: break;
    }
}
// The plate search has no bound: keep the place in the last animation entry
// (after a disturbance moves it) and +1 inside the plate table.
void SettleWm() {
    unsigned char* const o = Obj();
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    SetWord(Mem(kFzAnims + (kAnims - 1) * 4), Word(Mem(at::kPlace)));
}

void SeedWm(unsigned k) {
    unsigned char* const o = Obj();
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(kFzAnims), g_saved.anims, sizeof g_saved.anims);
    if (ah::Often()) std::memcpy(Mem(kFzCells), g_saved.cells, sizeof g_saved.cells);
    if (ah::Often()) std::memcpy(Mem(kFzRows), g_saved.rows, sizeof g_saved.rows);
    if (ah::Often()) std::memcpy(Mem(kFzDirections), g_saved.dirs, sizeof g_saved.dirs);
    if (ah::Often()) std::memcpy(Mem(kFzDriftU), g_saved.drift, sizeof g_saved.drift);
    o[1] = static_cast<unsigned char>(o[1] % 5);
    // the place one of the rows' (or anything), and always in the last plate entry
    if (ah::Often()) SetWord(Mem(at::kPlace), Word(Mem(kFzRows + (ah::Next() % kRows) * 0x20)));
    SetWord(Mem(kFzAnims + (kAnims - 1) * 4), Word(Mem(at::kPlace)));
    const auto leave = [o] {
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2) : ah::Next());
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 2, 5, 1, 3));
        Field_ScriptFlags = static_cast<unsigned short>(ah::Often() ? Field_ScriptFlags & ~0x100u : Field_ScriptFlags | 0x100u);
    };
    switch (k) {
    case kPlaceMessage:
        Mem(at::kMsgState)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        // the chapter inside a row mostly; its edges and a few negatives
        Cond_ByteFA = static_cast<signed char>(ah::Often() ? ah::Next() % 16 : AH_PICK(0, 15, 16, 0xFF, 0xF0, 0x80, 0x7F, 0x31));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 5, 3));
        // each row's message words distinct (the shipped rows repeat one word)
        if (ah::Half())
            for (U w = 2; w < kFzRowsEnd - kFzRows; w += 2)
                if ((w & 0x1F) != 0) SetWord(Mem(kFzRows + w), ah::Next());
        // the place in a row (a row's place moved onto the chosen one), or none
        if (ah::Often()) SetWord(Mem(kFzRows + (ah::Next() % kRows) * 0x20), Word(Mem(at::kPlace)));
        else if (ah::Half()) SetWord(Mem(at::kPlace), 0xFFFF);
        SetWord(Mem(kFzAnims + (kAnims - 1) * 4), Word(Mem(at::kPlace)));
        break;
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) Mem(at::kLeaderSteps)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
        SetWord(Mem(kFzAnims + (ah::Next() % kAnims) * 4), Word(Mem(at::kPlace)));
        break;
    case kPlateGrow:
    case kPlateShrink:
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(1, 1, 2, 0, 0xFF, 0x80));
        if (ah::Half()) SetLong(o + 0x40, static_cast<std::int32_t>(AH_PICK(0, 0xE000, 0x10000, 0xFFFFE000u, 0x7FFFF000)));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 2, 0));
        break;
    case kPlateHold:
        Game_Mode = static_cast<unsigned short>(ah::Often() ? AH_PICK(0, 2, 1, 0x101) : ah::Next());
        if (ah::Often()) o[7] = static_cast<unsigned char>(ah::Half() ? 1 : AH_PICK(0, 2, 3, 4));
        if (ah::Often()) o[0xB] = o[7];
        if (ah::Often()) SetLong(o + 0x18, static_cast<std::int32_t>(Word(Mem(at::kPlace)) | (ah::Half() ? 0 : 0x10000u)));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 5, 2));
        break;
    case kHudRun: o[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFrameStep: o[2] = static_cast<unsigned char>(ah::Next() % 4); break;
    case kBoxStep: o[3] = static_cast<unsigned char>(ah::Next() % 4); break;
    case kRecord8Run: o[1] = static_cast<unsigned char>(ah::Next() % 3); break;
    case kRecord4Run: o[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFrameSlideIn:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0, 0xFFFF, 1, 0xFFF0, 0x7FF0, 0x7FFF, 0xFFD0));
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1) : ah::Next());
        break;
    case kFrameHold:
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
        break;
    case kFrameSlideOut:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0xFFE0, 0xFFE1, 0xFFDF, 0x8000, 0x800F, 0x8010, 0x10));
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
        break;
    case kBoxSlideIn:
        if (ah::Often()) SetWord(o + 0x30, AH_PICK(0xD1, 0xD2, 0xD3, 0xF0, 0x800A, 0x8009, 0));
        leave();
        break;
    case kBoxHold:
        if (ah::Often()) o[0xB] = 0;
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(0x58, 0x59, 0x5A, 0xFF, 0, 0x7F));
        leave();
        break;
    case kBoxSlideOut:
        if (ah::Often()) SetWord(o + 0x30, AH_PICK(0xE5, 0xE6, 0xE7, 0xC8, 0x7FF6, 0x7FF5));
        leave();
        break;
    case kDrawFrame:
    case kDrawHud:
    case kDrawSprite:
        Draw_PassFlags = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 2, 8, 0x10, 0x1B, 4, 0x20, 0xE4) : ah::Next());
        // the button words with a bit of one entry's mask, or none
        if (ah::Often()) SetLong(Mem(at::kButtonMap0), static_cast<std::int32_t>(Word(Mem(kFzButtons + (ah::Next() % 6) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) {
            // a word only the eighth entry answers, when it has such bits
            U others = 0;
            for (U e = 0; e < 7; ++e) others |= Word(Mem(kFzButtons + e * 4));
            const U only = Word(Mem(kFzButtons + 7 * 4)) & ~others;
            if (only != 0) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(only | (ah::Next() & 0xFFFF0000u)));
        } else if (ah::Often()) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(Word(Mem(kFzButtons + (ah::Half() ? 6 + ah::Next() % 2 : ah::Next() % 8) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) SetLong(Mem(at::kButtonMap0), 0);
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1000u);
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x4000u);
        if (ah::Half()) Mem(at::kPartySet)[0] = static_cast<unsigned char>(AH_PICK(0xC, 0x8C, 0xB, 0xFF));
        break;
    case kRecord8Place:
        if (ah::Often()) o[8] = static_cast<unsigned char>(ah::Next() % 4);
        if (ah::Often()) o[6] = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0xFF, 1));
        break;
    case kRecord4MarkCell:
        Mem(at::kFlag3A79)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 9, 8, 0x89) : ah::Next());
        if (ah::Often()) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & ~1u);
        // the map's width so that any cell record lands in the block
        AreaMap_Header[0] = static_cast<unsigned char>(ah::Next() % 0x18);
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Next() % kFzCellRecords);
        break;
    case kDrawDrift: {
        if (ah::Half()) o[2] = 0;
        if (ah::Often()) Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags | 4);
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 3, 2, 3, 0, 1, 4, 5) : ah::Next());
        const unsigned height = Mem(at::kMapHeight)[0];
        if (ah::Half()) SetWord(o + 0x3A, height + AH_PICK(8, 9, 7, 0, 0x7FFF));
        const auto near = [](unsigned char* cell, unsigned centre) {
            SetWord(cell, centre + AH_PICK(0, 25, 26, static_cast<U>(-25), static_cast<U>(-26), 1, 100));
        };
        if (ah::Often()) near(Mem(at::kLeaderCellX), Word(o + 0x36));
        if (ah::Often()) near(Mem(at::kLeaderCellZ), Word(o + 0x3A));
        if (ah::Half()) Frame_Counter = AH_PICK(0, 15, 16, 31, 0x2F, 0xFFFFFFFFu);
        break;
    }
    case kSpawn:
        // the status bit clear mostly, the frame at a multiple of 1024 mostly
        // (or one of its low ten bits set)
        if (ah::Often()) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & ~1u);
        Frame_Counter = ah::Often() ? (ah::Next() << 10) : (ah::Next() << 10) | AH_PICK(1, 0x200, 0x3FF, 0x100);
        break;
    default: break;
    }
}

// The draws' arguments: x and y whole words (the dial's and the box's
// positions, and anything), the sprite's index a byte with bits above it.
void ArgsWm(unsigned k, U* a) {
    if (k == kDrawFrame || k == kDrawHud || k == kDrawSprite) {
        a[0] = ah::Often() ? AH_PICK(0x10, 0x5C, 0, 0x7FF0) : ah::Next();
        a[1] = ah::Often() ? AH_PICK(0xFFD0, 0xFFFFFFD0u, 0xC8, 0xF0, 0x10) | (ah::Half() ? ah::Next() & 0xFFFF0000u : 0) : ah::Next();
        if (k == kDrawSprite) a[2] = ah::Often() ? AH_PICK(0, 1, 2, 3, 4, 5, 0x15, 0x100, 0x1FF) : ah::Next();
    }
}

void Run152() {
    g_regions152[kRegPackets].at = Key(g_packets);
    g_regions152[kRegItems].at = Key(g_items);
    g_regions152[kRegNames].at = Key(g_names);
    ah::Group g{"area_w4a", kClones152, AH_COUNT(kClones152), kCallees152, AH_COUNT(kCallees152), kTables152,
                AH_COUNT(kTables152), g_regions152, AH_COUNT(g_regions152), &SeedWm, &DisturbWm, 4000};
    g.settle = &SettleWm;
    g.phase_span = 5;
    g.args = &ArgsWm;
    g.area = 152;
    ah::Run(g);
}

// ===========================================================================
// Areas 153, 154, 155 and 166: choices
// ===========================================================================

// clones: tools/area_rows.py --unit AREA153 / 154 / 155 / 166 --clones
constexpr ah::CallSite kCalls425BA0[] = {{0xD, 0x591680}, {0x3D, 0x590BB0}, {0x4E, 0x587740}, {0x66, 0x57C0F0}, {0x7F, 0x57C0F0}};
constexpr ah::CallSite kCalls425C60[] = {{0x12, 0x57C7C0}};
const ah::Clone kClones153[] = {W4A_P(Area153_ChoiceFocusPair, 0x425B20, 0x3C, S::kChoice)};
const ah::Clone kClones154[] = {W4A_P(Area154_ChoiceFocusPair, 0x425B60, 0x3C, S::kChoice)};
const ah::Clone kClones155[] = {
    W4A_C(Area155_ChoiceGiveItem4D, 0x425BA0, 0x88, kCalls425BA0, S::kChoice),
    W4A_P(Area155_ChoiceArmTail10, 0x425C30, 0x28, S::kChoice),
};
const ah::Clone kClones166[] = {W4A_C(Area166_ChoiceFocusPair, 0x425C60, 0x5F, kCalls425C60, S::kChoice)};

const ah::Callee kCalleesChoice[] = {
    kSet40,
    {"Item_NamePtr", bof3::addr::Item_NamePtr, KeyOf(&::Item_NamePtr), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &NameEffect, nullptr},
};

// The fuzz's own literal addresses of the pair tables.
constexpr U kFzPairs153 = 0x63841C, kFzPairs154 = 0x638994, kFzPairs166 = 0x63A79C;
ah::Region g_regionsChoice[] = {
    {at::kFocusObject, 4}, {at::kTextRecords, 0x10}, {0, sizeof g_names},
    {kFzPairs153, 8}, {kFzPairs154, 8}, {kFzPairs166, 8},
};

// the answer 0..3 mostly (each pair of the tables, and each branch's value),
// the sign's edges; the focus pointer a party record or a field object.
void SeedChoice(unsigned) {
    Mem(at::kChoice)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 3, 0, 2, 3) : AH_PICK(4, 0x80, 0xFF, 0x7F, 0xFE));
    ah::SetPointer(at::kFocusObject, SomeRecord(ah::Next()));
}
// The choices' disturbance: the focus pointer (read again for the second
// store), the answer byte (read again by area 166's pair after its call).
void DisturbChoice(U h) {
    switch ((h >> 16) % 3) {
    case 0: ah::SetPointer(at::kFocusObject, SomeRecord(h >> 20)); break;
    case 1: Mem(at::kChoice)[0] = static_cast<unsigned char>((h >> 8) % 4); break;
    default: break;
    }
}

void RunChoices(int area, const ah::Clone* clones, unsigned n) {
    g_regionsChoice[2].at = Key(g_names);
    ah::Group g{"area_w4a", clones, n, kCalleesChoice, AH_COUNT(kCalleesChoice), nullptr, 0, g_regionsChoice,
                AH_COUNT(g_regionsChoice), &SeedChoice, &DisturbChoice, 6000};
    g.area = area;
    ah::Run(g);
}

// ===========================================================================
// Area 167
// ===========================================================================

// clones: tools/area_rows.py --unit AREA167 --clones
constexpr ah::CallSite kCalls425CC0[] = {{0x1D, 0x57C7C0}};
constexpr ah::CallSite kCalls425D00[] = {{0x1E, 0x57C7C0}};
constexpr ah::CallSite kCalls425D40[] = {{0x1C, 0x57C7C0}};
constexpr ah::CallSite kCalls425D80[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls425DC0[] = {{0x10, 0x57C140}, {0x23, 0x57C110}, {0x48, 0x57C140}, {0x79, 0x57C140}, {0x8C, 0x57C110}, {0xB1, 0x57C140}};
constexpr ah::CallSite kCalls425EA0[] = {{0x1B, 0x57C160}};
constexpr ah::CallSite kCalls425ED0[] = {{0x7, 0x57C140}};
constexpr ah::CallSite kCalls425EF0[] = {{0x14, 0x57C140}, {0x27, 0x57C110}, {0x60, 0x57C140}, {0x73, 0x57C140}, {0xB8, 0x57C140}, {0xCB, 0x57C110}, {0xF0, 0x57C140}};
constexpr ah::CallSite kCalls426010[] = {{0x24, 0x57C0F0}, {0x30, 0x57C110}, {0x44, 0x57C110}, {0x50, 0x57C0F0}, {0x64, 0x57C110}, {0x70, 0x57C110}};
constexpr ah::CallSite kCalls4260F0[] = {{0x26, 0x531F90}, {0x59, 0x57C0F0}, {0x72, 0x594E00}, {0x81, 0x587740}, {0xA8, 0x57C7A0}, {0xCC, 0x531F90}, {0x118, 0x594E00}, {0x127, 0x57C160}, {0x133, 0x57C0F0}, {0x160, 0x587740}, {0x18B, 0x531F90}, {0x19E, 0x531F90}, {0x1CD, 0x57C7A0}, {0x1E3, 0x594E00}, {0x1EF, 0x57C110}, {0x1FB, 0x57C110}, {0x21A, 0x594E00}, {0x226, 0x57C0F0}, {0x232, 0x57C110}, {0x251, 0x594E00}, {0x25D, 0x57C110}, {0x269, 0x57C0F0}, {0x27E, 0x587740}, {0x28D, 0x57C0F0}, {0x2D1, 0x57C7A0}, {0x2E0, 0x57C0F0}, {0x2F6, 0x594E00}};
constexpr ah::JumpTable kTables4260F0[] = {{0x20, 0x318, 14}};
constexpr ah::CallSite kCalls426470[] = {{0x15, 0x57C7C0}};
constexpr ah::CallSite kCalls4264A0[] = {{0x4C, 0x57C160}, {0x54, 0x57C7C0}};
constexpr ah::CallSite kCalls426510[] = {{0x13, 0x57C140}, {0x29, 0x57C140}};

const ah::Clone kClones167[] = {
    W4A_C(Area167_ChoiceTail32At15, 0x425CC0, 0x38, kCalls425CC0, S::kChoice),
    W4A_C(Area167_ChoiceTail32At14Even, 0x425D00, 0x39, kCalls425D00, S::kChoice),
    W4A_C(Area167_ChoiceTail32At14, 0x425D40, 0x37, kCalls425D40, S::kChoice),
    W4A_C(Area167_ChoiceTail32AtA, 0x425D80, 0x2D, kCalls425D80, S::kChoice),
    W4A_P(Area167_SetByteFE2, 0x425DB0, 0x8, S::kHandler),
    W4A_C(Area167_HeightBy4B4C, 0x425DC0, 0xDE, kCalls425DC0, S::kHandler),
    W4A_C(Area167_Toggle4BUnlessLow, 0x425EA0, 0x2F, kCalls425EA0, S::kHandler),
    W4A_C(Area167_Skip3Unless4B, 0x425ED0, 0x1E, kCalls425ED0, S::kHandler),
    W4A_C(Area167_HeightBy48To4A, 0x425EF0, 0x11E, kCalls425EF0, S::kHandler),
    W4A_C(Area167_Flags48By49, 0x426010, 0x84, kCalls426010, S::kHandler),
    W4A_P(Area167_Skip3ByChapter, 0x4260A0, 0x33, S::kHandler),
    W4A_P(Area167_SetByteFE10, 0x4260E0, 0x8, S::kHandler),
    {"Area167_Tail32", 0x4260F0, 0x350, kCalls4260F0, AH_N(kCalls4260F0), nullptr, 0, kTables4260F0, AH_N(kTables4260F0),
     reinterpret_cast<const void*>(&::Area167_Tail32), 0, false, S::kTail},
    W4A_A(Area167_ArriveHook, 0x426470, 0x2E, kCalls426470, S::kHook),
    W4A_A(Area167_CellHook, 0x4264A0, 0x6C, kCalls4264A0, S::kHook),
    W4A_C(Area167_Init, 0x426510, 0x4D, kCalls426510, S::kInit),
};
enum : unsigned {
    kAt15, kAt14Even, kAt14, kAtA, kFE2, kHeight4B, kToggle4B, kSkip4B, kHeight48, kFlags48, kSkipChapter, kFE10,
    kTail, kArrive, kCell, kInit167,
};
static_assert(kInit167 + 1 == AH_COUNT(kClones167), "area 167's seeding indices");

const ah::Callee kCallees167[] = {kSet40, kClear40, kToggle};

constexpr U kFzSwitch = 0x63C594;     // the fuzz's own literal: the cell hook's one record
ah::Region g_regions167[] = {
    {kScriptObject, 4}, {kByteFE, 1}, {kWaitWordDA, 2}, {kFzSwitch, 4},
};
unsigned char g_switch[4];

void Seed167(unsigned k) {
    ah::SetPointer(kScriptObject, SomeRecord(ah::Next()));
    if (ah::Often()) std::memcpy(Mem(kFzSwitch), g_switch, 4);
    Cond_ByteFD = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 3, 1, 2) : AH_PICK(4, 0x80, 0xFF, 0x81, 0x82));
    unsigned char* const o = Sprite_Current;
    switch (k) {
    case kAt15:
    case kAt14Even:
    case kAt14:
    case kAtA:
        Mem(at::kChoice)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 3, 0, 2) : ah::Next());
        break;
    case kHeight4B:
    case kHeight48:
    case kToggle4B:
    case kFlags48:
        if (ah::Often()) SetWord(o + 0x3E, ah::Half() ? 0x3C0 : AH_PICK(0x3C1, 0x3BF, 0x13C0, 0x9C0, 0xFDC0));
        if (ah::Half()) o[0] = static_cast<unsigned char>(o[0] | 0x40);
        Mem(at::kByte905E68)[0] = static_cast<unsigned char>(ah::Half() ? 0 : AH_PICK(1, 0x80, 0xFF));
        break;
    case kSkipChapter:
        Mem(at::kScriptVar5)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 0x80) : ah::Next());
        break;
    case kTail: {
        // every state, the range's edges and the sign
        Mem(at::kTailState)[0] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 0x21 : AH_PICK(0x20, 0x21, 0x22, 0x7F, 0x80, 0xFF, 0xE0));
        static const U kWaits[] = {0x18, 0x64, 0x74, 0, 0x44, 0x54};
        Mem(at::kScriptVar6)[0] = static_cast<unsigned char>(ah::Often() ? kWaits[ah::Next() % 6] + AH_PICK(0, 0, 0, 1, 0xFF) : ah::Next());
        Mem(at::kScriptVar5)[0] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 3 : AH_PICK(3, 0x80, 0xFF));
        SetWord(Mem(at::kTailTimer), ah::Often() ? AH_PICK(1, 1, 2, 0, 0xFFFF, 0x1E) : ah::Next());
        SetWord(Mem(kWaitWordDA), ah::Half() ? 0 : AH_PICK(1, 0x100, 0xFFFF));
        break;
    }
    case kInit167:
        SetWord(Mem(at::kCameraDistance), ah::Next());
        break;
    default: break;
    }
}
// The hooks' arguments: the arrive hook 16.16 positions with the high words
// at and about (0x14..0x16, 0x66), the cell hook the switch's (x, z) bytes
// with bits above them (and the leader's facing its nibble, or the nibble with
// bits above it, or anything).
void Args167(unsigned k, U* a) {
    if (k == kArrive) {
        const U lo = ah::Half() ? ah::Next() & 0xFFFF : AH_PICK(0, 0xFFFF, 0x8000);
        const U hx = ah::Often() ? AH_PICK(0x14, 0x15, 0x16, 0x13, 0x17, 0x10014, 0xFFFF) : ah::Next();
        const U hz = ah::Often() ? AH_PICK(0x66, 0x66, 0x65, 0x67, 0x166, 0x10066) : ah::Next();
        a[0] = (hx << 16) | lo;
        a[1] = (hz << 16) | (ah::Next() & 0xFFFF);
    } else if (k == kCell) {
        const unsigned char* const r = Mem(kFzSwitch);
        if (ah::Often()) {
            a[0] = r[0] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
            a[1] = r[1] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
            if (ah::Half()) a[ah::Next() & 1] += ah::Half() ? 1 : 0xFFFFFFFFu;
        } else {
            a[0] = ah::Next();
            a[1] = ah::Next();
        }
        const unsigned nib = r[2] & 0xFu;
        const unsigned near[] = {nib | 0x10u, nib ^ 1u, r[2], nib | 0x80u};
        Mem(at::kLeaderDir)[0] = static_cast<unsigned char>(ah::Often() ? nib : near[ah::Next() % 4]);
    }
}
// Area 167's disturbance: the chapter byte (read again after the flag tests),
// MoveScript_Object (read again for each store), the height word the handlers
// test, variables 5 and 6, the tail's state and countdown.
void Disturb167(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 7) {
    case 0: Cond_ByteFD = static_cast<unsigned char>(v % 4); break;
    case 1: ah::SetPointer(kScriptObject, SomeRecord(h >> 20)); break;
    case 2: SetWord(Sprite_Current + 0x3E, v & 1 ? 0x3C0 : 0x9C0); break;
    case 3: Mem(at::kScriptVar5)[0] = static_cast<unsigned char>(v % 3); break;
    case 4: Mem(at::kScriptVar6)[0] = static_cast<unsigned char>(v); break;
    case 5: SetWord(Mem(at::kTailTimer), v % 3); break;
    default: break;
    }
}

void Run167() {
    std::memcpy(g_switch, Mem(kFzSwitch), 4);
    ah::Group g{"area_w4a", kClones167, AH_COUNT(kClones167), kCallees167, AH_COUNT(kCallees167), nullptr, 0,
                g_regions167, AH_COUNT(g_regions167), &Seed167, &Disturb167, 6000};
    g.args = &Args167;
    g.area = 167;
    ah::Run(g);
}

// BOF3X_AR4A_AREA=n runs area n's group alone (the controls script's
// shortcut); unset, every area runs.
bool Wants(int area) {
    const char* const only = std::getenv("BOF3X_AR4A_AREA");
    return only == nullptr || *only == 0 || std::atoi(only) == area;
}

}  // namespace

void SelfTest() {
    SaveTables();
    if (Wants(152)) Run152();
    if (Wants(153)) RunChoices(153, kClones153, AH_COUNT(kClones153));
    if (Wants(154)) RunChoices(154, kClones154, AH_COUNT(kClones154));
    if (Wants(155)) RunChoices(155, kClones155, AH_COUNT(kClones155));
    if (Wants(166)) RunChoices(166, kClones166, AH_COUNT(kClones166));
    if (Wants(167)) Run167();
}

}  // namespace area_w4a
