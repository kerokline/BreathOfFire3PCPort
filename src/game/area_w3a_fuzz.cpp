// BOF3X_SHADOW=area_w3a: world 3's areas 115..119 through the area round's
// shared harness (area_harness.h), once at start-up: one area_harness::Run
// per area (Group::area its number, the real descriptor and tables in place,
// the .data state tables swapped through DataTable).
// docs/area_w3a.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA115..119 --clones,
// 2026-09-28), each read against the disassembly. Area 115's group is area
// 88's (area_w2b_fuzz.cpp, itself area 45's) over area 115's tables, copied
// with the name-set count made a field (area 115 has two sets where 87 and 88
// have three).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3a.h"
#include "game/area_w3a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w3a {
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
#define W3A_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W3A_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone answering in al.
#define W3A_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}
#define W3A_AP(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}

const ah::Callee kSet40 = {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kClear40 = {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0};
// Effect_Spawn answers an Effect_Objects slot or 0xFF for none (area 11's
// listing): the byte 0xFE..0x02 through 0xFF, garbage above.
const ah::Callee kSpawn = {"Effect_Spawn", ::bof3::addr::Effect_Spawn, KeyOf(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02};

// The fuzz's own literal addresses of the tables it plants (read off the exe,
// docs/area_w3a.md): never the constants ours reads, so a wrong one in ours
// shows.
constexpr U kFzCursor = 0x7DEE67;
constexpr U kFzLeaderDir = 0x802D48;
constexpr U kFzPartyList = 0x904062;
constexpr U kFzZenny = 0x904058;
constexpr U kFzByte9398CF = 0x9398CF;
constexpr U kFzFlagRow = 0x929ED0;

// BOF3X_AR3A_AREA=n runs area n's group alone (the controls script's
// shortcut); unset, every area runs.
bool Wants(int area) {
    const char* const only = std::getenv("BOF3X_AR3A_AREA");
    return only == nullptr || *only == 0 || std::atoi(only) == area;
}

// A cursor (the choices' answer, s8): its cases, their edges, the sign's.
unsigned char SomeCursor() {
    return static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 1, 2, 3, 4, 5, 0xFF, 0x80, 0x7F, 6) : ah::Next());
}
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

// ===========================================================================
// Area 115: the world-map copy (area 88's group, area_w2b_fuzz.cpp; area 45's, area_w1b_fuzz.cpp)
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

const ah::Callee kPers4 = {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, KeyOf(&::Gte_RotTransPers4), 10,
                           {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, ah::Answer::kGarbage, 0, 0,
                           {8, 8, 8, 8}, &PersEffect, nullptr};

// --- the clones --------------------------------------------------------------

// clones: tools/area_rows.py --unit AREA115 --clones (area 88's rows at area
// 115's addresses; WorldMapHud_Start 0x419110 between HudRun and HudFrame is
// ours already and not cloned)
constexpr ah::CallSite kCalls418BE0[] = {{0x22, 0x57C7A0}, {0x3B, 0x57C7C0}, {0x4F, 0x536700}, {0x8E, 0x4976D0}, {0x117, 0x591680}, {0x158, 0x4976D0}};
constexpr ah::CallSite kCalls418D60[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls418E40[] = {{0xE, 0x589590}};
constexpr ah::CallSite kCalls418EA0[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls418FF0[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls419040[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls4190A0[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls419130[] = {{0x0, 0x419140}, {0x5, 0x419210}};
constexpr ah::CallSite kCalls419160[] = {{0x1C, 0x419190}};
constexpr ah::CallSite kCalls419190[] = {{0x1F, 0x419370}};
constexpr ah::CallSite kCalls4191C0[] = {{0x38, 0x419370}};
constexpr ah::CallSite kCalls419230[] = {{0x59, 0x419600}};
constexpr ah::CallSite kCalls4192A0[] = {{0x65, 0x419600}};
constexpr ah::CallSite kCalls419310[] = {{0x4E, 0x419600}};
constexpr ah::CallSite kCalls419370[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x419540}, {0x51, 0x536700}, {0x7B, 0x419540}, {0xDC, 0x419540}, {0xF8, 0x419540}, {0x15B, 0x419540}, {0x173, 0x531920}, {0x1A8, 0x419540}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls419540[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls419600[] = {{0x17, 0x419540}, {0x26, 0x419540}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls419680[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls419800[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls4198C0[] = {{0xB7, 0x5A75D0}, {0xBF, 0x5A77A0}, {0x199, 0x5A85F0}, {0x19F, 0x5A9290}, {0x1AF, 0x572A00}, {0x1BB, 0x461E50}, {0x21D, 0x572F70}, {0x236, 0x5A75D0}, {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};

const ah::Clone kClones115[] = {
    W3A_C(Area115_PlaceMessage, 0x418BE0, 0x178, kCalls418BE0, S::kState),
    W3A_C(Area115_PlateRun, 0x418D60, 0xD6, kCalls418D60, S::kState),
    W3A_C(Area115_PlateStart, 0x418E40, 0x51, kCalls418E40, S::kState),
    W3A_C(Area115_PlateShow, 0x418EA0, 0x142, kCalls418EA0, S::kState),
    W3A_C(Area115_PlateGrow, 0x418FF0, 0x41, kCalls418FF0, S::kState),
    W3A_C(Area115_PlateHold, 0x419040, 0x58, kCalls419040, S::kState),
    W3A_C(Area115_PlateShrink, 0x4190A0, 0x50, kCalls4190A0, S::kState),
    W3A_P(Area115_HudRun, 0x4190F0, 0x12, S::kState),
    W3A_C(Area115_HudFrame, 0x419130, 0xA, kCalls419130, S::kState),
    W3A_P(Area115_FrameStep, 0x419140, 0x12, S::kCallee),
    W3A_C(Area115_FrameSlideIn, 0x419160, 0x21, kCalls419160, S::kState),
    W3A_C(Area115_FrameHold, 0x419190, 0x28, kCalls419190, S::kState),
    W3A_C(Area115_FrameSlideOut, 0x4191C0, 0x41, kCalls4191C0, S::kState),
    W3A_P(Area115_BoxStep, 0x419210, 0x12, S::kCallee),
    W3A_C(Area115_BoxSlideIn, 0x419230, 0x62, kCalls419230, S::kState),
    W3A_C(Area115_BoxHold, 0x4192A0, 0x6E, kCalls4192A0, S::kState),
    W3A_C(Area115_BoxSlideOut, 0x419310, 0x57, kCalls419310, S::kState),
    W3A_C(Area115_DrawFrame, 0x419370, 0x1C5, kCalls419370, S::kCallee),
    W3A_C(Area115_DrawSprite, 0x419540, 0xBC, kCalls419540, S::kCallee),
    W3A_C(Area115_DrawHud, 0x419600, 0x58, kCalls419600, S::kCallee),
    W3A_P(Area115_Record8Run, 0x419660, 0x12, S::kState),
    W3A_C(Area115_Record8Place, 0x419680, 0x154, kCalls419680, S::kState),
    W3A_P(Area115_Record4Run, 0x4197E0, 0x12, S::kState),
    W3A_C(Area115_Record4MarkCell, 0x419800, 0xB5, kCalls419800, S::kState),
    W3A_C(Area115_DrawDrift, 0x4198C0, 0x462, kCalls4198C0, S::kState),
};
enum : unsigned {
    kPlaceMessage, kPlateRun, kPlateStart, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame,
    kFrameStep, kFrameSlideIn, kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame,
    kDrawSprite, kDrawHud, kRecord8Run, kRecord8Place, kRecord4Run, kRecord4MarkCell, kDrawDrift,
};
static_assert(kDrawDrift + 1 == AH_COUNT(kClones115), "the copy's seeding indices");

// --- the callees: the standard set's, with what the callers read after -------

#define W3A_WM_CALLEES(nn)                                                                                                        \
    {"Area" #nn "_FrameStep", at::kWm##nn.fn_frame_step, at::kWm##nn.fn_frame_step, 0, {}, ah::Answer::kPhase, 0, 0},          \
    {"Area" #nn "_BoxStep", at::kWm##nn.fn_box_step, at::kWm##nn.fn_box_step, 0, {}, ah::Answer::kPhase, 0, 0},                \
    {"Area" #nn "_DrawFrame", at::kWm##nn.fn_draw_frame, at::kWm##nn.fn_draw_frame, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0}, \
    {"Area" #nn "_DrawSprite", at::kWm##nn.fn_draw_sprite, at::kWm##nn.fn_draw_sprite, 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0}, \
    {"Area" #nn "_DrawHud", at::kWm##nn.fn_draw_hud, at::kWm##nn.fn_draw_hud, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},     \
    kSet40, kClear40,                                                                                                             \
    {"Item_NamePtr", bof3::addr::Item_NamePtr, KeyOf(&::Item_NamePtr), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &NameEffect, nullptr}, \
    {"WorldMap_PinSprite", bof3::addr::WorldMap_PinSprite, KeyOf(&::WorldMap_PinSprite), 0, {}, ah::Answer::kGarbage, 0, 0},  \
    {"Effect_Release", bof3::addr::Effect_Release, KeyOf(&::Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},              \
    {"Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, KeyOf(&::Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect, nullptr}, \
    {"WorldMap_DrawNeedle", bof3::addr::WorldMap_DrawNeedle, KeyOf(&::WorldMap_DrawNeedle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0}, \
    {"WorldMap_RecordIndex", bof3::addr::WorldMap_RecordIndex, KeyOf(&::WorldMap_RecordIndex), 0, {}, ah::Answer::kGarbage, 0, 0}, \
    {"Prim_SetTexture", bof3::addr::Prim_SetTexture, KeyOf(&::Prim_SetTexture), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &TextureEffect, nullptr}, \
    {"AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, KeyOf(&::AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect, nullptr}, \
    {"Field_CellHasEvent", bof3::addr::Field_CellHasEvent, KeyOf(&::Field_CellHasEvent), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0}, \
    {"MapView_ItemHalfAt", bof3::addr::MapView_ItemHalfAt, KeyOf(&::MapView_ItemHalfAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ItemEffect, nullptr}, \
    {"MapView_LinkPrimAt", bof3::addr::MapView_LinkPrimAt, KeyOf(&::MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect, nullptr}, \
    {"Gpu_SetPolyFT4", bof3::addr::Gpu_SetPolyFT4, KeyOf(&::Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr}, \
    {"Gpu_SetShadeTex", bof3::addr::Gpu_SetShadeTex, KeyOf(&::Gpu_SetShadeTex), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ShadeEffect, nullptr}, \
    {"Gpu_SetSemiTrans", bof3::addr::Gpu_SetSemiTrans, KeyOf(&::Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect, nullptr}, \
    {"Gpu_SetSprt", bof3::addr::Gpu_SetSprt, KeyOf(&::Gpu_SetSprt), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SprtEffect, nullptr}, \
    {"Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, KeyOf(&::Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect, nullptr}, \
    {"Gte_PrimDepths4_10", bof3::addr::Gte_PrimDepths4_10, KeyOf(&::Gte_PrimDepths4_10), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DepthEffect, nullptr}, \
    kPers4

const ah::Callee kCallees115[] = {W3A_WM_CALLEES(115)};
#undef W3A_WM_CALLEES

const ah::DataTable kTables115[] = {
    {at::kWm115.plate_states, 5}, {at::kWm115.hud_states, 2}, {at::kWm115.frame_states, 4},
    {at::kWm115.box_states, 4},   {at::kWm115.record8_states, 3}, {at::kWm115.record4_states, 2},
};

// --- the fuzz's own literal table addresses (read off the exe, docs/area_w3a.md
// section 2): the seed plants and the regions are built from these, never from
// the kWm115 ours reads, so a wrong constant in ours shows. The state
// tables' DataTables and the callees stay on kWm115 (a wrong one there
// moves the swapped entries away from what ours reads, which shows too).
struct Fz {
    U plate_anims, plate_anims_end, cells, place_messages, place_messages_end, name_sets;
    unsigned set_stride;
    U directions, drift_u, buttons;
    unsigned cell_records;
    unsigned sets;   // name sets: 3 in areas 87 and 88, 2 in area 115
};
constexpr Fz kFz115 = {0x620148, 0x62015C, 0x62015C, 0x620264, 0x620304, 0x620304, 6, 0x6203C8, 0x6203E8, 0x6203A4, 3, 2};

// --- the regions: the harness's plus the copy's tables (only the tables: the
// descriptor data between the cell records and the place rows is left alone)

enum : unsigned { kRegPackets = 3, kRegItems = 7, kRegNames = 8 };
#define W3A_WM_REGIONS(nn, text_bytes, cell_records)                                                     \
    {at::kMapMode, 1}, {0x7E0918, 1}, {0x7E0670, 4}, {0, kPacketBytes}, {0x9037A0, 0x20}, {0x66C7E8, 2}, \
    {at::kButtonMap0, 0x10}, {0, 4 * kItemBytes}, {0, sizeof g_names}, {at::kTextRecords, text_bytes},  \
    {at::kAreaText, 4}, {at::kFlag3A79, 1},                                                              \
    {kFz##nn.plate_anims, kFz##nn.plate_anims_end - kFz##nn.plate_anims},                   \
    {kFz##nn.cells, 4 * (cell_records)},                                                             \
    {kFz##nn.place_messages, kFz##nn.place_messages_end - kFz##nn.place_messages},          \
    {kFz##nn.name_sets, kFz##nn.sets * kFz##nn.set_stride},                                                 \
    {kFz##nn.directions, 0x18}, {kFz##nn.drift_u, 0xC}
// Area 115's cell region takes its zero record (the third, as area 87's
// fourth); five text rows.
ah::Region g_regions115[] = {W3A_WM_REGIONS(115, 0xA0, 3)};
#undef W3A_WM_REGIONS

// The exe's own bytes of the tables the fuzz randomises, put back two rounds
// in three (so the seeds see the shipped values most of the time).
struct Saved {
    unsigned char anims[0x40], cells[0x10], messages[0x160], names[0x28], dirs[0x18], drift[0xC];
};
Saved g_saved115;

// The copy the running group is (set before each Run; the harness calls
// the seed and the disturbance synchronously).
const Fz* g_t = &kFz115;
Saved* g_saved = &g_saved115;

unsigned CellRecords() { return g_t->cell_records; }

void SaveTables(const Fz& t, Saved& s) {
    std::memcpy(s.anims, Mem(t.plate_anims), t.plate_anims_end - t.plate_anims);
    std::memcpy(s.cells, Mem(t.cells), 0x10);
    std::memcpy(s.messages, Mem(t.place_messages), t.place_messages_end - t.place_messages);
    std::memcpy(s.names, Mem(t.name_sets), t.sets * t.set_stride);
    std::memcpy(s.dirs, Mem(t.directions), sizeof s.dirs);
    std::memcpy(s.drift, Mem(t.drift_u), sizeof s.drift);
}

unsigned char* Obj() { return Sprite_Current; }

// The leader's cell words as bytes, and one cell record planted with them
// (the search has no bound): the last record (the sentinel) always, a
// random one when asked.
void PlantCell(bool random_place) {
    const Fz& t = *g_t;
    unsigned char* const cx = Mem(at::kLeaderCellWordX);
    unsigned char* const cz = Mem(at::kLeaderCellWordZ);
    cx[1] = 0;
    cz[1] = 0;
    const unsigned n = CellRecords();
    unsigned char* const last = Mem(t.cells + (n - 1) * 4);
    last[0] = cx[0];
    last[1] = cz[0];
    // the sentinel's id one of the sets', so a search with stale words (or
    // none again) lands on a record of another set
    last[3] = Mem(t.name_sets + ((cx[0] ^ cz[0]) % t.sets) * t.set_stride)[0];
    if (random_place) {
        unsigned char* const r = Mem(t.cells + (ah::Next() % n) * 4);
        r[0] = cx[0];
        r[1] = cz[0];
    }
}

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
    case 7: SetWord(Mem(at::kPlace), v); break;
    case 8: Mem(at::kLeaderCellX + (v & 1) * 2)[0] = static_cast<unsigned char>(v); break;
    case 9: Mem(at::kPartySet)[0] = static_cast<unsigned char>(v & 1 ? 0xC : v); break;
    case 10: case 11: Mem(v & 1 ? at::kLeaderCellWordX : at::kLeaderCellWordZ)[0] = static_cast<unsigned char>(h >> 24); break;   // the settle re-plants its record
    default: break;
    }
}
// The leader's cell words as the seed left them: after a disturbance moves
// one, the settle plants (old x, new z) in record 0 and (new x, old z) in
// record 1, each with another set's id than the sentinel's, so a read of a
// stale word finds a record (and another name set) instead of running off.
unsigned char g_cx0, g_cz0;
void SettleWm() {
    unsigned char* const o = Obj();
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(false);
    const Fz& t = *g_t;
    const unsigned char cx = Mem(at::kLeaderCellWordX)[0], cz = Mem(at::kLeaderCellWordZ)[0];
    if (cx == g_cx0 && cz == g_cz0) return;
    const unsigned char sentinel = Mem(t.cells + (CellRecords() - 1) * 4)[3];
    const auto other = [&t, sentinel](unsigned k) {
        const unsigned char id = Mem(t.name_sets + (k % t.sets) * t.set_stride)[0];
        return id != sentinel ? id : Mem(t.name_sets + ((k + 1) % t.sets) * t.set_stride)[0];
    };
    // the current words' record must stay the first match: plant a stale
    // pair only where it differs from the current one
    if (g_cx0 != cx) {
        unsigned char* const r0 = Mem(t.cells);
        r0[0] = g_cx0;
        r0[1] = cz;
        r0[3] = other(cx);
    }
    if (g_cz0 != cz) {
        unsigned char* const r1 = Mem(t.cells + 4);
        r1[0] = cx;
        r1[1] = g_cz0;
        r1[3] = other(cz + 1u);
    }
}

void SeedWm(unsigned k) {
    const Fz& t = *g_t;
    const Saved& s = *g_saved;
    unsigned char* const o = Obj();
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(t.plate_anims), s.anims, t.plate_anims_end - t.plate_anims);
    if (ah::Often()) std::memcpy(Mem(t.cells), s.cells, CellRecords() * 4);
    if (ah::Often()) std::memcpy(Mem(t.place_messages), s.messages, t.place_messages_end - t.place_messages);
    if (ah::Often()) std::memcpy(Mem(t.name_sets), s.names, t.sets * t.set_stride);
    if (ah::Often()) std::memcpy(Mem(t.directions), s.dirs, sizeof s.dirs);
    if (ah::Often()) std::memcpy(Mem(t.drift_u), s.drift, sizeof s.drift);
    o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(true);
    g_cx0 = Mem(at::kLeaderCellWordX)[0];
    g_cz0 = Mem(at::kLeaderCellWordZ)[0];
    const unsigned rows = (t.place_messages_end - t.place_messages) / 0x20;
    const unsigned anims = (t.plate_anims_end - t.plate_anims) / 4;
    const auto leave = [o] {
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2) : ah::Next());
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 2, 5, 1, 3));
        Field_ScriptFlags = static_cast<unsigned short>(ah::Often() ? Field_ScriptFlags & ~0x100u : Field_ScriptFlags | 0x100u);
    };
    switch (k) {
    case kPlaceMessage: {
        Mem(at::kMsgState)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        Cond_ByteFA = static_cast<signed char>(ah::Often() ? AH_PICK(1, 2, 15, 0, 0xFF, 0x80, 0x7F, 8) : ah::Next());
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 5, 3));
        // the place in one of the rows, or in none
        if (ah::Often()) SetWord(Mem(t.place_messages + (ah::Next() % rows) * 0x20), Word(Mem(at::kPlace)));
        // the found cell's id one of the sets', or none
        unsigned char* cell = Mem(t.cells);
        while (!(cell[0] == Mem(at::kLeaderCellWordX)[0] && cell[1] == Mem(at::kLeaderCellWordZ)[0])) cell += 4;
        if (ah::Often()) cell[3] = Mem(t.name_sets + (ah::Next() % t.sets) * t.set_stride)[0];
        // items held or not, now and then a list ended early
        for (unsigned i = 0; i < 0x74; ++i)
            if (ah::Half()) Mem(at::kItemsHeld + i)[0] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Half())
            Mem(t.name_sets + 1 + (ah::Next() % t.sets) * t.set_stride + ah::Next() % (t.set_stride - 1))[0] = static_cast<unsigned char>(AH_PICK(0xFF, 0x16, 0x5E));
        break;
    }
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) Mem(at::kLeaderSteps)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
        SetWord(Mem(t.plate_anims + (ah::Next() % anims) * 4), Word(Mem(at::kPlace)));
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
        if (ah::Often()) SetLong(Mem(at::kButtonMap0), static_cast<std::int32_t>(Word(Mem(t.buttons + (ah::Next() % 6) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) {
            // a word only the eighth entry answers, when it has such bits
            U others = 0;
            for (U e = 0; e < 7; ++e) others |= Word(Mem(t.buttons + e * 4));
            const U only = Word(Mem(t.buttons + 7 * 4)) & ~others;
            if (only != 0) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(only | (ah::Next() & 0xFFFF0000u)));
        } else if (ah::Often()) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(Word(Mem(t.buttons + (ah::Half() ? 6 + ah::Next() % 2 : ah::Next() % 8) * 4)) | (ah::Next() & 0xFFFF0000u)));
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
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Next() % CellRecords());
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

void RunWorldMap(int area, const Fz& t, Saved& saved, const ah::Clone* clones, unsigned n, const ah::Callee* callees,
                 unsigned n_callees, const ah::DataTable* tables, ah::Region* regions, unsigned n_regions) {
    regions[kRegPackets].at = Key(g_packets);
    regions[kRegItems].at = Key(g_items);
    regions[kRegNames].at = Key(g_names);
    g_t = &t;
    g_saved = &saved;
    ah::Group g{"area_w3a", clones, n, callees, n_callees, tables, 6, regions, n_regions, &SeedWm, &DisturbWm, 4000};
    g.settle = &SettleWm;
    g.phase_span = 5;
    g.args = &ArgsWm;
    g.area = area;
    ah::Run(g);
}

// ===========================================================================
// Area 116
// ===========================================================================

// clones: tools/area_rows.py --unit AREA116 --clones
constexpr ah::CallSite kCalls419D80[] = {{0x11, 0x594E00}};
constexpr ah::CallSite kCalls419DA0[] = {{0x10, 0x57C140}, {0x50, 0x531F90}};
constexpr ah::CallSite kCalls419E00[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls419E40[] = {{0x22, 0x4220D0}};
const ah::Clone kClones116[] = {
    W3A_P(Area116_ChoicePay10000, 0x419D30, 0x4C, S::kChoice),
    W3A_C(Area116_ToArea100, 0x419D80, 0x1A, kCalls419D80, S::kHandler),
    W3A_A(Area116_StepHook, 0x419DA0, 0x5E, kCalls419DA0, S::kHook),
    W3A_A(Area116_Trigger26, 0x419E00, 0x16, kCalls419E00, S::kCallee),
    W3A_P(Area116_EffectB8Run, 0x419E20, 0x12, S::kCallee),
    W3A_C(Area116_EffectB8Ring, 0x419E40, 0x2B, kCalls419E40, S::kState),
};
enum : unsigned { k116Pay, k116ToArea, k116Step, k116Trigger, k116Run, k116Ring };
static_assert(k116Ring + 1 == AH_COUNT(kClones116), "area 116's seeding indices");

const ah::Callee kCallees116[] = {
    kSet40,
    // the point's three dwords read through the pointer (docs/area_w2d.md)
    {"RingAt_4220D0", kRingAt, kRingAt, 1, {0}, ah::Answer::kGarbage, 0, 0, {12}},
};
const ah::DataTable kTables116[] = {{0x620664, 2}};   // Area116_EffectStates
const ah::Region kRegions116[] = {{kFzByte9398CF, 1}};

void Seed116(unsigned k) {
    switch (k) {
    case k116Pay: {
        Mem(kFzCursor)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 0xFF, 0x80, 0x7F) : ah::Next());
        // Party_Zenny at the price's edge, and past the sign (an unsigned compare)
        const U zenny = ah::Often() ? 10000u + AH_PICK(0, 0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u) : AH_PICK(0, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFF, 0x98967F);
        SetLong(Mem(kFzZenny), static_cast<std::int32_t>(ah::Half() ? zenny : ah::Next()));
        break;
    }
    case k116Step:
        Cond_ByteFD = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 2, 2, 1, 3, 0x82, 0) : ah::Next());
        Mem(kFzLeaderDir)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 7, 6, 0, 7, 6, 1, 5, 8, 0x80, 0x87) : ah::Next());
        break;
    case k116Run: Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    default: break;
    }
}
// The step hook's (x, z): 16.16 positions whose high words are at and about
// the box (0x46..0x48, 0x33..0x35), with any low word; a trigger's (object,
// flags).
U BoxWord(U lo) {
    const U high = ah::Often() ? lo + AH_PICK(0, 1, 2, 0xFFFFFFFFu, 3, 0x10000, 0x8000) : ah::Next();
    return (high << 16) | (ah::Next() & 0xFFFF);
}
void Args116(unsigned k, U* a) {
    if (k == k116Step) {
        a[0] = BoxWord(0x46);
        a[1] = BoxWord(0x33);
    } else if (k == k116Trigger) {
        ArgsTrigger(a);
    }
}

// ===========================================================================
// Areas 117 and 118 (one body over a table set each) and their spawns
// ===========================================================================

constexpr ah::CallSite kCallsSpawn30[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCallsSpawn2C[] = {{0x2C, 0x57CE10}};

// clones: tools/area_rows.py --unit AREA117 --clones
constexpr ah::CallSite kCalls419FB0[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls41A000[] = {{0x2B, 0x587740}};
constexpr ah::CallSite kCalls41A040[] = {{0x2E, 0x587740}};
constexpr ah::JumpTable kTables41A040[] = {{0x1E, 0x40, 5}};
constexpr ah::CallSite kCalls41A0A0[] = {{0x4F, 0x41A2D0}, {0x11C, 0x57C140}, {0x13B, 0x589330}, {0x154, 0x534610}, {0x15D, 0x534710}, {0x184, 0x57C140}, {0x1D4, 0x52E140}, {0x1E1, 0x5893A0}};
constexpr ah::CallSite kCalls41A340[] = {{0x41, 0x57C140}, {0x5F, 0x57C160}, {0x66, 0x469FE0}, {0x70, 0x587740}};
const ah::Clone kClones117[] = {
    W3A_C(Area117_Spawn3AtMember2, 0x419E70, 0x46, kCallsSpawn30, S::kHandler),
    W3A_C(Area117_Spawn4AtMember2, 0x419EC0, 0x46, kCallsSpawn30, S::kHandler),
    W3A_C(Area117_Spawn5AtMember2, 0x419F10, 0x46, kCallsSpawn30, S::kHandler),
    W3A_C(Area117_Spawn2AtMember2, 0x419F60, 0x46, kCallsSpawn30, S::kHandler),
    W3A_A(Area117_Trigger33, 0x419FB0, 0x1D, kCalls419FB0, S::kCallee),
    W3A_P(Area117_ChoiceArmTailA, 0x419FD0, 0x28, S::kChoice),
    W3A_C(Area117_ChoiceMessage1, 0x41A000, 0x32, kCalls41A000, S::kChoice),
    {"Area117_ChoiceMessage2", 0x41A040, 0x54, kCalls41A040, AH_N(kCalls41A040), nullptr, 0, kTables41A040, AH_N(kTables41A040),
     reinterpret_cast<const void*>(&::Area117_ChoiceMessage2), 0, false, S::kChoice},
    W3A_C(Area117_MembersFrame, 0x41A0A0, 0x223, kCalls41A0A0, S::kCallee),
    W3A_AP(Area117_MemberRect, 0x41A2D0, 0x6A, S::kCallee),
    W3A_A(Area117_SwitchHook, 0x41A340, 0x7D, kCalls41A340, S::kHook),
};
enum : unsigned { k117Spawn3, k117Spawn4, k117Spawn5, k117Spawn2, k117Trigger, k117ArmTail, k117Msg1, k117Msg2, k117Frame, k117Rect, k117Switch };
static_assert(k117Switch + 1 == AH_COUNT(kClones117), "area 117's seeding indices");

// clones: tools/area_rows.py --unit AREA118 --clones
constexpr ah::CallSite kCalls41A410[] = {{0x4F, 0x41A640}, {0x11C, 0x57C140}, {0x13B, 0x589330}, {0x154, 0x534610}, {0x15D, 0x534710}, {0x184, 0x57C140}, {0x1D4, 0x52E140}, {0x1E1, 0x5893A0}};
constexpr ah::CallSite kCalls41A6B0[] = {{0x41, 0x57C140}, {0x5F, 0x57C160}, {0x66, 0x469FE0}, {0x70, 0x587740}};
const ah::Clone kClones118[] = {
    W3A_C(Area118_Spawn1AtLeader, 0x41A3C0, 0x42, kCallsSpawn2C, S::kHandler),
    W3A_C(Area118_MembersFrame, 0x41A410, 0x223, kCalls41A410, S::kCallee),
    W3A_AP(Area118_MemberRect, 0x41A640, 0x6A, S::kCallee),
    W3A_A(Area118_SwitchHook, 0x41A6B0, 0x7D, kCalls41A6B0, S::kHook),
};
enum : unsigned { k118Spawn1, k118Frame, k118Rect, k118Switch };

const ah::Callee kCalleesTwin[] = {
    kSpawn,
    kSet40,
    // the rectangle searches as the member frames call them: a record (0) or
    // none (0xFF), the member pushed as a dword with a stale high part
    {"Area117_MemberRect", 0x41A2D0, 0x41A2D0, 1, {kU8}, ah::Answer::kByte, 0xFF, 0x00},
    {"Area118_MemberRect", 0x41A640, 0x41A640, 1, {kU8}, ah::Answer::kByte, 0xFF, 0x00},
    {"Field_JumpSetUp", bof3::addr::Field_JumpSetUp, KeyOf(&::Field_JumpSetUp), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Field_JumpCamera", bof3::addr::Field_JumpCamera, KeyOf(&::Field_JumpCamera), 0, {}, ah::Answer::kGarbage, 0, 0},
    // the flag pushed with Flags_Test's answer above its byte
    {"Flags_Toggle", bof3::addr::Flags_Toggle, KeyOf(&::Flags_Toggle), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"Effect_HoldFlag1C", bof3::addr::Effect_HoldFlag1C, KeyOf(&::Effect_HoldFlag1C), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
};

// A twin's literal addresses (read off the exe; docs/area_w3a.md section 4).
struct FzTwin {
    U rect;     // (x0, z0, x1, z1, facing, flag)
    U sw;       // (x, z, facing, flag)
};
constexpr FzTwin kFzTw117 = {0x6215A8, 0x62160C};
constexpr FzTwin kFzTw118 = {0x621DF0, 0x621E3C};
const FzTwin* g_tw = &kFzTw117;
int g_twin_area = 117;

// The member MemberRect is asked about: drawn by the seed (which plants that
// record about the rectangle; an args hook's writes are lost) and handed over
// by the args.
U g_member;

// A party list's id: the tables' twelve, mostly, else any byte (the tables
// are read unchecked; every byte stays in .data).
void SeedIds() {
    for (U m = 0; m < 3; ++m)
        if (ah::Often()) Mem(kFzPartyList + m)[0] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 12 : ah::Next());
}

// A coordinate at and about a rectangle edge (a byte << 16).
std::int32_t Edge(unsigned char cell) {
    const U base = static_cast<U>(cell) << 16;
    return static_cast<std::int32_t>(base + AH_PICK(0, 0, 1, 0xFFFFFFFFu, 0x8000, 0xFFFF8000u, 0x10000, 0xFFFF0000u));
}

void SeedTwinFrame() {
    const unsigned char* const r = Mem(g_tw->rect);
    unsigned char* const caller = Sprite_Current;
    // the caller's marks: none, one member's, several, every bit
    caller[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2, 4, 3, 7, 5, 0xFF, 0xF8) : ah::Next());
    // the members' bits in the two flag words: mostly clear
    if (ah::Often()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x2007u);
    if (ah::Often()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x7u);
    if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | (1u << (ah::Next() % 3)));
    if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 | (1u << (ah::Next() % 3)));
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const p = ah::PartyOf(static_cast<unsigned char>(m));
        if (ah::Often()) p[9] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 8, 7, 0xFF));
        if (ah::Often()) p[8] = static_cast<unsigned char>(ah::Half() ? r[4] : ah::Half() ? r[4] ^ 4 : ah::Next() % 8);
    }
    Field_Request = static_cast<unsigned char>(ah::Often() ? AH_PICK(5, 0, 2, 4) : ah::Next());
    if (ah::Half()) Field_MemberCount = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3));
}

void SeedTwinRect() {
    const unsigned char* const r = Mem(g_tw->rect);
    g_member = ah::Often() ? ah::Next() % 3 : ah::Next() % 4 | (ah::Next() & 0xFFFFFF00u);
    unsigned char* const p = ah::PartyOf(static_cast<unsigned char>(g_member & 0xFF));
    if ((g_member & 0xFF) >= 3) return;
    // x and z at each edge, one axis sometimes far off so each test decides
    SetLong(p + 0x34, ah::Often() ? Edge(r[ah::Half() ? 0 : 2]) : static_cast<std::int32_t>(ah::Next()));
    SetLong(p + 0x38, ah::Often() ? Edge(r[ah::Half() ? 1 : 3]) : static_cast<std::int32_t>(ah::Next()));
    if (ah::Half()) SetLong(p + 0x34, static_cast<std::int32_t>(((static_cast<U>(r[0]) + static_cast<U>(r[2])) << 15) + (ah::Next() & 0xFFFF)));
    if (ah::Half()) SetLong(p + 0x38, static_cast<std::int32_t>(((static_cast<U>(r[1]) + static_cast<U>(r[3])) << 15) + (ah::Next() & 0xFFFF)));
}

void SeedTwinSwitch() {
    const unsigned char* const s = Mem(g_tw->sw);
    Mem(kFzLeaderDir)[0] = static_cast<unsigned char>(ah::Often() ? s[2] : ah::Half() ? s[2] ^ 1 : ah::Next());
}

void SeedTwin(unsigned k) {
    if (g_twin_area == 117) {
        switch (k) {
        case k117Spawn3: case k117Spawn4: case k117Spawn5: case k117Spawn2: SeedIds(); break;
        case k117ArmTail: case k117Msg1: case k117Msg2: Mem(kFzCursor)[0] = SomeCursor(); break;
        case k117Frame: SeedTwinFrame(); break;
        case k117Rect: SeedTwinRect(); break;
        case k117Switch: SeedTwinSwitch(); break;
        default: break;
        }
    } else {
        switch (k) {
        case k118Spawn1: SeedIds(); break;
        case k118Frame: SeedTwinFrame(); break;
        case k118Rect: SeedTwinRect(); break;
        case k118Switch: SeedTwinSwitch(); break;
        default: break;
        }
    }
}

// The switch's (x, z): the record's bytes (two rounds in three) with bits
// above, one off, bit 7 flipped, or anything.
void ArgsSwitch(U* a) {
    const unsigned char* const s = Mem(g_tw->sw);
    if (ah::Often()) {
        a[0] = s[0] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
        a[1] = s[1] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
        if (ah::Half()) a[ah::Next() & 1] += ah::Half() ? 1 : 0xFFFFFFFFu;
        else if (ah::Half()) a[ah::Next() & 1] ^= 0x80;
    } else {
        a[0] = ah::Next();
        a[1] = ah::Next();
    }
}
void ArgsTwin(unsigned k, U* a) {
    const bool a117 = g_twin_area == 117;
    if ((a117 && k == k117Rect) || (!a117 && k == k118Rect)) a[0] = g_member;
    else if ((a117 && k == k117Switch) || (!a117 && k == k118Switch)) ArgsSwitch(a);
    else if (a117 && k == k117Trigger) ArgsTrigger(a);
}

// The twins' disturbance: a member's bit in the flag words, a party record's
// +8 / +9 (the frame reads them again after its calls), Field_Request.
void DisturbTwin(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 6) {
    case 0: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ (1u << (v % 3))); break;
    case 1: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (v & 1 ? 0x2000u : 1u << (v % 3))); break;
    case 2: ah::PartyOf(static_cast<unsigned char>(v % 3))[9] = static_cast<unsigned char>(h >> 24); break;
    case 3: ah::PartyOf(static_cast<unsigned char>(v % 3))[8] = static_cast<unsigned char>((h >> 24) % 8); break;
    case 4: Field_Request = static_cast<unsigned char>(v & 1 ? 5 : v); break;
    default: break;
    }
}

void RunTwin(int area, const FzTwin& t, const ah::Clone* clones, unsigned n) {
    g_tw = &t;
    g_twin_area = area;
    ah::Group g{"area_w3a", clones, n, kCalleesTwin, AH_COUNT(kCalleesTwin), nullptr, 0, nullptr, 0, &SeedTwin, &DisturbTwin, 6000};
    g.args = &ArgsTwin;
    g.area = area;
    ah::Run(g);
}

// ===========================================================================
// Area 119
// ===========================================================================

// clones: tools/area_rows.py --unit AREA119 --clones
constexpr ah::CallSite kCalls41A960[] = {{0x8, 0x57C0F0}, {0x12, 0x587740}};
constexpr ah::CallSite kCalls41A980[] = {{0x8, 0x57C110}, {0x12, 0x587740}};
const ah::Clone kClones119[] = {
    W3A_C(Area119_Spawn1AtLeader, 0x41A730, 0x42, kCallsSpawn2C, S::kHandler),
    W3A_C(Area119_Spawn4AtLeader, 0x41A780, 0x42, kCallsSpawn2C, S::kHandler),
    W3A_C(Area119_Spawn1AtLeaderB, 0x41A7D0, 0x42, kCallsSpawn2C, S::kHandler),
    W3A_C(Area119_Spawn1AtMember1, 0x41A820, 0x42, kCallsSpawn2C, S::kHandler),
    W3A_C(Area119_Spawn1AtMember2, 0x41A870, 0x46, kCallsSpawn30, S::kHandler),
    W3A_C(Area119_Spawn3AtMember2, 0x41A8C0, 0x46, kCallsSpawn30, S::kHandler),
    W3A_C(Area119_Spawn4AtMember2, 0x41A910, 0x46, kCallsSpawn30, S::kHandler),
    W3A_C(Area119_SetRowFlag3C, 0x41A960, 0x1B, kCalls41A960, S::kHandler),
    W3A_C(Area119_ClearRowFlag3C, 0x41A980, 0x1B, kCalls41A980, S::kHandler),
    W3A_P(Area119_ChoiceCounter0, 0x41A9A0, 0x28, S::kChoice),
};
enum : unsigned { k119Choice = 9 };
const ah::Callee kCallees119[] = {kSpawn};
const ah::Region kRegions119[] = {{kFzFlagRow, 4}};

void Seed119(unsigned k) {
    if (k == k119Choice) Mem(kFzCursor)[0] = SomeCursor();
    else if (k < 7) SeedIds();
}

}  // namespace

void SelfTest() {
    SaveTables(kFz115, g_saved115);

    if (Wants(115))
        RunWorldMap(115, kFz115, g_saved115, kClones115, AH_COUNT(kClones115), kCallees115, AH_COUNT(kCallees115), kTables115,
                    g_regions115, AH_COUNT(g_regions115));
    if (Wants(116)) {
        ah::Group g{"area_w3a", kClones116, AH_COUNT(kClones116), kCallees116, AH_COUNT(kCallees116), kTables116, AH_COUNT(kTables116),
                    kRegions116, AH_COUNT(kRegions116), &Seed116, nullptr, 6000};
        g.args = &Args116;
        g.area = 116;
        ah::Run(g);
    }
    if (Wants(117)) RunTwin(117, kFzTw117, kClones117, AH_COUNT(kClones117));
    if (Wants(118)) RunTwin(118, kFzTw118, kClones118, AH_COUNT(kClones118));
    if (Wants(119)) {
        ah::Group g{"area_w3a", kClones119, AH_COUNT(kClones119), kCallees119, AH_COUNT(kCallees119), nullptr, 0,
                    kRegions119, AH_COUNT(kRegions119), &Seed119, nullptr, 4000};
        g.area = 119;
        ah::Run(g);
    }
}

}  // namespace area_w3a
