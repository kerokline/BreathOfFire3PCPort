// BOF3X_SHADOW=area_w1e: world 1's areas 65 and 67 through the area round's
// shared harness (area_harness.h), once at start-up: one area_harness::Run
// per area (Group::area its number, the real descriptors and tables in
// place, area 65's own .data state tables swapped through DataTable).
// docs/area_w1e.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA065 / AREA067 --clones,
// 2026-09-28), each read against the disassembly. Area 65's group is area
// 45's (area_w1b_fuzz.cpp) with area 65's tables and the name sets' shape.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1e.h"
#include "game/area_w1e_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w1e {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using ah::Mem;
using S = ah::Shape;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define AH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define W1E_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W1E_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// One answering in al.
#define W1E_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}

constexpr U kActiveMember = 0x9035A4;     // Field_ActiveMember
constexpr U kEffects = 0x7E11E0;          // Effect_Objects: the first eight records

const ah::Callee kFindFree = {"Effect_FindFree", bof3::addr::Effect_FindFree, KeyOf(&::Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x07};
const ah::Callee kSet40 = {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kClear40 = {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0};

// ===========================================================================
// Area 65 (area 45's code over area 65's tables: area_w1b_fuzz.cpp's group)
// ===========================================================================

// --- the fuzz's own memory: a packet buffer, four map items, four names ------

constexpr unsigned kPacketBytes = 0x200, kItemBytes = 0x48;
alignas(16) unsigned char g_packets[kPacketBytes];
alignas(16) unsigned char g_items[4][kItemBytes];
alignas(16) unsigned char g_names[4][16];

// Area 65's tables the fuzz randomises, put back to the exe's bytes two
// rounds in three (so the seeds see the shipped values most of the time).
// The plate and cell tables stop at area 65's choice table and descriptor
// (0x6046CC..0x604713), which stay the image's.
constexpr U kA65Tables1 = at::kA65PlateAnims, kA65Tables1Bytes = at::kA65CellsEnd - at::kA65PlateAnims;
constexpr U kA65Tables2 = at::kA65PlaceMessages, kA65Tables2Bytes = at::kA65NameSetsEnd - at::kA65PlaceMessages;
unsigned char g_a65_tables1[kA65Tables1Bytes];   // 0x6043C8..0x6046CB
unsigned char g_a65_tables2[kA65Tables2Bytes];   // 0x604714..0x60487F
unsigned char g_a65_dirs[0x18];                  // 0x604938..0x60494F
unsigned char g_a65_drift[0xC];                  // 0x604958..0x604963
constexpr unsigned kCells65 = (at::kA65CellsEnd - at::kA65Cells) / 4;   // 182
constexpr unsigned kNameSets65 = (at::kA65NameSetsEnd - at::kA65NameSets) / at::kA65NameSetStride;   // 2

bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }

// The packet cursor moved on by `size`, kept inside the fuzz's buffer.
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
// AreaMap_ByteAt: the cell kinds the world map tests, mostly.
U ByteAtEffect(const U*, U answer) {
    static const U kCells[] = {0xA1, 0xA1, 0xA0, 0xAE, 0xA2, 0x9F, 0, 0x21};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 5 == 0 ? (n >> 8) & 0xFF : kCells[(n >> 4) % 8]);
}
// Item_NamePtr: one of the fuzz's four 16-byte names.
U NameEffect(const U*, U answer) { return Key(g_names[(answer >> 4) % 4]); }
// MapView_ItemHalfAt: a third of the time none, else one of the fuzz's items.
U ItemEffect(const U*, U answer) { return answer % 3 == 0 ? 0u : Key(g_items[(answer >> 4) % 4]); }
// The primitive setters write the primitive's bytes, so a store the caller
// makes before the call (where the original makes it after) shows.
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
// Gte_RotTransPers4 (ten words pushed): the four corners written; the two
// stack locals are the caller's and never read after.
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

// Gte_RotTransPers4's listing: the vectors' eight bytes each, the corners'
// addresses, the two locals not logged (stack addresses differ).
const ah::Callee kPers4 = {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, KeyOf(&::Gte_RotTransPers4), 10,
                           {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, ah::Answer::kGarbage, 0, 0,
                           {8, 8, 8, 8}, &PersEffect, nullptr};

// clones: tools/area_rows.py --unit AREA065 --clones (area 45's rows at area
// 65's addresses, the place hook's last two four bytes later)
constexpr ah::CallSite kCalls40B8C0[] = {{0x22, 0x57C7A0}, {0x3B, 0x57C7C0}, {0x4F, 0x536700}, {0x8E, 0x4976D0}, {0x117, 0x591680}, {0x158, 0x4976D0}};
constexpr ah::CallSite kCalls40BA40[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls40BB20[] = {{0xB, 0x589590}};
constexpr ah::CallSite kCalls40BB70[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls40BCC0[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls40BD10[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls40BD70[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls40BDE0[] = {{0x0, 0x40BDF0}, {0x5, 0x40BEC0}};
constexpr ah::CallSite kCalls40BE10[] = {{0x1C, 0x40BE40}};
constexpr ah::CallSite kCalls40BE40[] = {{0x1F, 0x40C020}};
constexpr ah::CallSite kCalls40BE70[] = {{0x38, 0x40C020}};
constexpr ah::CallSite kCalls40BEE0[] = {{0x59, 0x40C2B0}};
constexpr ah::CallSite kCalls40BF50[] = {{0x65, 0x40C2B0}};
constexpr ah::CallSite kCalls40BFC0[] = {{0x4E, 0x40C2B0}};
constexpr ah::CallSite kCalls40C020[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x40C1F0}, {0x51, 0x536700}, {0x7B, 0x40C1F0}, {0xDC, 0x40C1F0}, {0xF8, 0x40C1F0}, {0x15B, 0x40C1F0}, {0x173, 0x531920}, {0x1A8, 0x40C1F0}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls40C1F0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls40C2B0[] = {{0x17, 0x40C1F0}, {0x26, 0x40C1F0}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls40C330[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls40C490[] = {{0x37, 0x5893A0}, {0x3C, 0x588F20}, {0x46, 0x589840}};
constexpr ah::CallSite kCalls40C500[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls40C5C0[] = {{0xB7, 0x5A75D0}, {0xBF, 0x5A77A0}, {0x199, 0x5A85F0}, {0x19F, 0x5A9290}, {0x1AF, 0x572A00}, {0x1BB, 0x461E50}, {0x21D, 0x572F70}, {0x236, 0x5A75D0}, {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};

const ah::Clone kClones65[] = {
    W1E_C(Area65_PlaceMessage, 0x40B8C0, 0x178, kCalls40B8C0, S::kState),
    W1E_C(Area65_PlateRun, 0x40BA40, 0xD6, kCalls40BA40, S::kState),
    W1E_C(Area65_PlateStart, 0x40BB20, 0x4E, kCalls40BB20, S::kState),
    W1E_C(Area65_PlateShow, 0x40BB70, 0x142, kCalls40BB70, S::kState),
    W1E_C(Area65_PlateGrow, 0x40BCC0, 0x41, kCalls40BCC0, S::kState),
    W1E_C(Area65_PlateHold, 0x40BD10, 0x58, kCalls40BD10, S::kState),
    W1E_C(Area65_PlateShrink, 0x40BD70, 0x50, kCalls40BD70, S::kState),
    W1E_P(Area65_HudRun, 0x40BDC0, 0x12, S::kState),
    W1E_C(Area65_HudFrame, 0x40BDE0, 0xA, kCalls40BDE0, S::kState),
    W1E_P(Area65_FrameStep, 0x40BDF0, 0x12, S::kCallee),
    W1E_C(Area65_FrameSlideIn, 0x40BE10, 0x21, kCalls40BE10, S::kState),
    W1E_C(Area65_FrameHold, 0x40BE40, 0x28, kCalls40BE40, S::kState),
    W1E_C(Area65_FrameSlideOut, 0x40BE70, 0x41, kCalls40BE70, S::kState),
    W1E_P(Area65_BoxStep, 0x40BEC0, 0x12, S::kCallee),
    W1E_C(Area65_BoxSlideIn, 0x40BEE0, 0x62, kCalls40BEE0, S::kState),
    W1E_C(Area65_BoxHold, 0x40BF50, 0x6E, kCalls40BF50, S::kState),
    W1E_C(Area65_BoxSlideOut, 0x40BFC0, 0x57, kCalls40BFC0, S::kState),
    W1E_C(Area65_DrawFrame, 0x40C020, 0x1C5, kCalls40C020, S::kCallee),
    W1E_C(Area65_DrawSprite, 0x40C1F0, 0xBC, kCalls40C1F0, S::kCallee),
    W1E_C(Area65_DrawHud, 0x40C2B0, 0x58, kCalls40C2B0, S::kCallee),
    W1E_P(Area65_Record8Run, 0x40C310, 0x12, S::kState),
    W1E_C(Area65_Record8Place, 0x40C330, 0x154, kCalls40C330, S::kState),
    W1E_C(Area65_Record8Move, 0x40C490, 0x4B, kCalls40C490, S::kState),
    W1E_P(Area65_Record4Run, 0x40C4E0, 0x12, S::kState),
    W1E_C(Area65_Record4MarkCell, 0x40C500, 0xB5, kCalls40C500, S::kState),
    W1E_C(Area65_DrawDrift, 0x40C5C0, 0x462, kCalls40C5C0, S::kState),
};
enum : unsigned {
    kPlaceMessage, kPlateRun, kPlateStart, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame,
    kFrameStep, kFrameSlideIn, kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame,
    kDrawSprite, kDrawHud, kRecord8Run, kRecord8Place, kRecord8Move, kRecord4Run, kRecord4MarkCell, kDrawDrift,
};
static_assert(kDrawDrift + 1 == sizeof kClones65 / sizeof kClones65[0], "area 65's seeding indices");

const ah::Callee kCallees65[] = {
    // area 65's own, called directly
    {"Area65_FrameStep", 0x40BDF0, 0x40BDF0, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area65_BoxStep", 0x40BEC0, 0x40BEC0, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area65_DrawFrame", 0x40C020, 0x40C020, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},   // y pushed with a stale high half
    {"Area65_DrawSprite", 0x40C1F0, 0x40C1F0, 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},   // a lit key's index carries the button word above its byte
    {"Area65_DrawHud", 0x40C2B0, 0x40C2B0, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},   // y pushed with the object pointer's high half
    // named, ours, beyond the standard set
    kSet40, kClear40,
    {"Item_NamePtr", bof3::addr::Item_NamePtr, KeyOf(&::Item_NamePtr), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &NameEffect, nullptr},
    {"WorldMap_PinSprite", bof3::addr::WorldMap_PinSprite, KeyOf(&::WorldMap_PinSprite), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Effect_Release", bof3::addr::Effect_Release, KeyOf(&::Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, KeyOf(&::Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect, nullptr},
    {"WorldMap_DrawNeedle", bof3::addr::WorldMap_DrawNeedle, KeyOf(&::WorldMap_DrawNeedle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"WorldMap_RecordIndex", bof3::addr::WorldMap_RecordIndex, KeyOf(&::WorldMap_RecordIndex), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Prim_SetTexture", bof3::addr::Prim_SetTexture, KeyOf(&::Prim_SetTexture), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &TextureEffect, nullptr},
    // standard ones listed again with what the caller reads after
    {"AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, KeyOf(&::AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect, nullptr},
    {"Field_CellHasEvent", bof3::addr::Field_CellHasEvent, KeyOf(&::Field_CellHasEvent), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},   // words pushed with stale high halves
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

const ah::DataTable kTables65[] = {
    {at::kA65PlateStates, 5}, {at::kA65HudStates, 2}, {at::kA65FrameStates, 4},
    {at::kA65BoxStates, 4},   {at::kA65Record8States, 3}, {at::kA65Record4States, 2},
};

ah::Region g_regions65[] = {
    {at::kMapMode, 1},
    {0x7E0918, 1},                // Draw_PassFlags
    {0x7E0670, 4},                // Gfx_PacketNext
    {0, kPacketBytes},            // g_packets (set at start-up)
    {0x9037A0, 0x20},             // Prim_VertexScratch
    {0x66C7E8, 2},                // Game_Mode
    {at::kButtonMap0, 0x10},      // the button map's words 0..7
    {0, 4 * kItemBytes},          // g_items
    {0, sizeof g_names},          // g_names
    {at::kTextRecords, 0xA0},     // Text_Records' first five rows
    {at::kAreaTextOffset, 4},
    {at::kFlag3A79, 1},
    {kA65Tables1, kA65Tables1Bytes},   // area 65's plate and cell tables
    {kA65Tables2, kA65Tables2Bytes},   // its place messages and name sets
    {at::kA65Directions, 0x18},   // Area65_Directions, Area65_Record8Anims
    {at::kA65DriftUBase, 0xC},    // Area65_DriftUV
};

unsigned char* Obj() { return Sprite_Current; }

// The leader's cell words as bytes, and one Area65_Cells record planted with
// them (the search has no bound), a sentinel in the last record.
void PlantCell(bool random_place) {
    unsigned char* const cx = Mem(at::kLeaderCellWordX);
    unsigned char* const cz = Mem(at::kLeaderCellWordZ);
    cx[1] = 0;
    cz[1] = 0;
    unsigned char* const last = Mem(at::kA65CellsEnd - 4);
    last[0] = cx[0];
    last[1] = cz[0];
    // the sentinel's id follows the words, so a search with stale words (or
    // none again) lands on a record of another set
    last[3] = Mem(at::kA65NameSets + ((cx[0] ^ cz[0]) % kNameSets65) * at::kA65NameSetStride)[0];
    if (random_place) {
        unsigned char* const r = Mem(at::kA65Cells + (ah::Next() % kCells65) * 4);
        r[0] = cx[0];
        r[1] = cz[0];
    }
}

// Area 65's disturbance: the cells its functions read again after a call.
void Disturb65(U h) {
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
// After every disturbance: the dispatch bytes inside their tables (the plate
// run reads +1 after its calls), the leader's cell words bytes with their
// sentinel record (the place hook reads them again after its call).
void Settle65() {
    unsigned char* const o = Obj();
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(false);
}

void Seed65(unsigned k) {
    unsigned char* const o = Obj();
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(kA65Tables1), g_a65_tables1, sizeof g_a65_tables1);
    if (ah::Often()) std::memcpy(Mem(kA65Tables2), g_a65_tables2, sizeof g_a65_tables2);
    if (ah::Often()) std::memcpy(Mem(at::kA65Directions), g_a65_dirs, sizeof g_a65_dirs);
    if (ah::Often()) std::memcpy(Mem(at::kA65DriftUBase), g_a65_drift, sizeof g_a65_drift);
    o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(true);
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
        // the place in one of the eleven rows, or in none
        if (ah::Often()) SetWord(Mem(at::kA65PlaceMessages + (ah::Next() % 11) * 0x20), Word(Mem(at::kPlace)));
        // the found cell's id one of the two sets', or none
        unsigned char* cell = Mem(at::kA65Cells);
        while (!(cell[0] == Mem(at::kLeaderCellWordX)[0] && cell[1] == Mem(at::kLeaderCellWordZ)[0])) cell += 4;
        if (ah::Often()) cell[3] = Mem(at::kA65NameSets + (ah::Next() % kNameSets65) * at::kA65NameSetStride)[0];
        // items held or not, now and then a list ended early (any of its five)
        for (unsigned i = 0; i < 0x74; ++i)
            if (ah::Half()) Mem(at::kItemsHeld + i)[0] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Half())
            Mem(at::kA65NameSets + 1 + (ah::Next() % kNameSets65) * at::kA65NameSetStride + ah::Next() % 5)[0] =
                static_cast<unsigned char>(AH_PICK(0xFF, 0x16, 0x5E));
        break;
    }
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) Mem(at::kLeaderSteps)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
        SetWord(Mem(at::kA65PlateAnims + (ah::Next() % 11) * 4), Word(Mem(at::kPlace)));
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
        if (ah::Often()) SetLong(Mem(at::kButtonMap0), static_cast<std::int32_t>(Word(Mem(at::kA65Buttons + (ah::Next() % 6) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) {
            // a word only the eighth entry answers, when it has such bits
            U others = 0;
            for (U e = 0; e < 7; ++e) others |= Word(Mem(at::kA65Buttons + e * 4));
            const U only = Word(Mem(at::kA65Buttons + 7 * 4)) & ~others;
            if (only != 0) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(only | (ah::Next() & 0xFFFF0000u)));
        } else if (ah::Often()) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(Word(Mem(at::kA65Buttons + (ah::Half() ? 6 + ah::Next() % 2 : ah::Next() % 8) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) SetLong(Mem(at::kButtonMap0), 0);
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1000u);
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x4000u);
        if (ah::Half()) Mem(at::kPartySet)[0] = static_cast<unsigned char>(AH_PICK(0xC, 0x8C, 0xB, 0xFF));
        break;
    case kRecord8Place:
        if (ah::Often()) o[8] = static_cast<unsigned char>(ah::Next() % 4);
        if (ah::Often()) o[6] = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0xFF, 1));
        break;
    case kRecord8Move:
        // +0xB held or not, +0 bit 7 set or clear: the four ways
        o[0xB] = static_cast<unsigned char>(ah::Often() ? (ah::Half() ? 0 : AH_PICK(1, 2, 0x80, 0xFF)) : ah::Next());
        o[0] = static_cast<unsigned char>(ah::Half() ? o[0] | 0x80 : o[0] & 0x7F);
        break;
    case kRecord4MarkCell:
        Mem(at::kFlag3A79)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 9, 8, 0x89) : ah::Next());
        if (ah::Often()) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & ~1u);
        // the map's width so that any cell record lands in the block
        AreaMap_Header[0] = static_cast<unsigned char>(ah::Next() % 0x18);
        // a cell record of the 182 (the byte's other values read on into the
        // descriptor and the message rows, as the original does)
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Next() % kCells65);
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
void Args65(unsigned k, U* a) {
    if (k == kDrawFrame || k == kDrawHud || k == kDrawSprite) {
        a[0] = ah::Often() ? AH_PICK(0x10, 0x5C, 0, 0x7FF0) : ah::Next();
        a[1] = ah::Often() ? AH_PICK(0xFFD0, 0xFFFFFFD0u, 0xC8, 0xF0, 0x10) | (ah::Half() ? ah::Next() & 0xFFFF0000u : 0) : ah::Next();
        if (k == kDrawSprite) a[2] = ah::Often() ? AH_PICK(0, 1, 2, 3, 4, 5, 0x15, 0x100, 0x1FF) : ah::Next();
    }
}

// ===========================================================================
// Area 67
// ===========================================================================

// clones: tools/area_rows.py --unit AREA067 --clones
constexpr ah::CallSite kCalls40CAD0[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls40CB10[] = {{0x30, 0x57CE80}};
constexpr ah::CallSite kCalls40CB60[] = {{0x30, 0x57CE80}};
constexpr ah::CallSite kCalls40CBB0[] = {{0x33, 0x57CE80}};
constexpr ah::CallSite kCalls40CC00[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40CC50[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40CCA0[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls40CCF0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40CD40[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40CD90[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls40CE10[] = {{0x2, 0x587B40}};
constexpr ah::CallSite kCalls40CE20[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls40CE70[] = {{0x1D, 0x57C7C0}};
constexpr ah::CallSite kCalls40CEC0[] = {{0x6, 0x579F00}};
constexpr ah::CallSite kCalls40CED0[] = {{0x0, 0x57C7C0}};
const ah::Clone kClones67[] = {
    W1E_P(Area67_ChoiceMark6, 0x40CA30, 0x23, S::kChoice),
    W1E_P(Area67_ChoiceMessage, 0x40CA60, 0x17, S::kChoice),
    W1E_P(Area67_ChoiceMark5, 0x40CA80, 0x27, S::kChoice),
    W1E_P(Area67_SetBit24, 0x40CAB0, 0xA, S::kHandler),
    W1E_P(Area67_Object0XUp, 0x40CAC0, 0xB, S::kHandler),
    W1E_C(Area67_SpawnEffect2D, 0x40CAD0, 0x40, kCalls40CAD0, S::kHandler),
    W1E_C(Area67_Member0SpawnAt, 0x40CB10, 0x46, kCalls40CB10, S::kHandler),
    W1E_C(Area67_Member1SpawnAt, 0x40CB60, 0x46, kCalls40CB60, S::kHandler),
    W1E_C(Area67_Member2SpawnAt, 0x40CBB0, 0x49, kCalls40CBB0, S::kHandler),
    W1E_C(Area67_Member0Spawn3, 0x40CC00, 0x42, kCalls40CC00, S::kHandler),
    W1E_C(Area67_Member1Spawn3, 0x40CC50, 0x42, kCalls40CC50, S::kHandler),
    W1E_C(Area67_Member2Spawn3, 0x40CCA0, 0x46, kCalls40CCA0, S::kHandler),
    W1E_C(Area67_Member0Spawn1, 0x40CCF0, 0x42, kCalls40CCF0, S::kHandler),
    W1E_C(Area67_Member1Spawn1, 0x40CD40, 0x42, kCalls40CD40, S::kHandler),
    W1E_C(Area67_Member2Spawn1, 0x40CD90, 0x46, kCalls40CD90, S::kHandler),
    W1E_P(Area67_Object0XDown, 0x40CDE0, 0xB, S::kHandler),
    W1E_P(Area67_Object1XUp, 0x40CDF0, 0xB, S::kHandler),
    W1E_P(Area67_Object1XDown, 0x40CE00, 0xB, S::kHandler),
    W1E_C(Area67_MusicFade10, 0x40CE10, 0x9, kCalls40CE10, S::kHandler),
    W1E_C(Area67_LeaderSpawn4, 0x40CE20, 0x42, kCalls40CE20, S::kHandler),
    W1E_C(Area67_ResetOn6, 0x40CE70, 0x4F, kCalls40CE70, S::kHandler),
    W1E_C(Area67_ClearCell, 0x40CEC0, 0xF, kCalls40CEC0, S::kHandler),
    W1E_A(Area67_Trigger31, 0x40CED0, 0x1D, kCalls40CED0, S::kCallee),
};
enum : unsigned {
    k67Mark6, k67Message, k67Mark5, k67SetBit24, k67Obj0Up, k67Spawn2D, k67At0, k67At1, k67At2, k67S30, k67S31, k67S32,
    k67S10, k67S11, k67S12, k67Obj0Down, k67Obj1Up, k67Obj1Down, k67Fade, k67Leader4, k67Reset, k67Cell, k67Trigger,
};
static_assert(k67Trigger + 1 == sizeof kClones67 / sizeof kClones67[0], "area 67's seeding indices");

// Effect_Spawn / Effect_SpawnAt answer an Effect_Objects slot or 0xFF for
// none: a range through 0xFF (the handlers test it). Ours both since round
// fourteen (R2B): keyed by name. Effect_SpawnAt's third byte is pushed with a stale high
// half (the table byte loaded into cl).
const ah::Callee kCallees67[] = {
    {"Effect_Spawn", bof3::addr::Effect_Spawn, KeyOf(&::Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02},
    {"Effect_SpawnAt", bof3::addr::Effect_SpawnAt, KeyOf(&::Effect_SpawnAt), 6, {kU8, kU8, kU8, kAll, kAll, kAll}, ah::Answer::kByte, 0xFE, 0x02},
    kFindFree, kSet40,
};
const ah::Region kRegions67[] = {{kActiveMember, 4}, {kEffects, 8 * 0x80}};

// Field_ActiveMember: one of the party records or of the first four field
// objects (the reset handler dereferences it).
unsigned char* SomeRecord(U h) { return (h & 1) ? ah::PartyOf(static_cast<unsigned char>(h >> 1)) : ah::TaskAt(h >> 1); }

void Seed67(unsigned k) {
    ah::SetPointer(kActiveMember, SomeRecord(ah::Next()));
    // the members' character ids: the first eight most of the time
    for (unsigned m = 0; m < 3; ++m)
        if (ah::Often()) Mem(at::kPartyList + m)[0] = static_cast<unsigned char>(ah::Next() % 8);
    switch (k) {
    case k67Mark6:
    case k67Message:
    case k67Mark5:
        Mem(at::kChoice)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 3, 4, 5, 6, 0xFF, 0x80, 0x7F) : ah::Next());
        break;
    case k67Reset:
        Mem(at::kLeaderEffect)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(6, 6, 6, 5, 7, 0x86, 0) : ah::Next());
        break;
    default:
        break;
    }
}
// The trigger's (object, flags): a field object and the bank, never read.
void Args67(unsigned k, U* a) {
    if (k != k67Trigger) return;
    a[0] = Key(ah::Object(ah::Next()));
    a[1] = 0x904030;
}

// ===========================================================================

// BOF3X_AR1E_AREA=n runs area n's group alone (the controls script's
// shortcut); unset, both areas run.
bool Wants(int area) {
    const char* const only = std::getenv("BOF3X_AR1E_AREA");
    return only == nullptr || *only == 0 || std::atoi(only) == area;
}

}  // namespace

void SelfTest() {
    g_regions65[3].at = Key(g_packets);
    g_regions65[7].at = Key(g_items);
    g_regions65[8].at = Key(g_names);
    std::memcpy(g_a65_tables1, Mem(kA65Tables1), sizeof g_a65_tables1);
    std::memcpy(g_a65_tables2, Mem(kA65Tables2), sizeof g_a65_tables2);
    std::memcpy(g_a65_dirs, Mem(at::kA65Directions), sizeof g_a65_dirs);
    std::memcpy(g_a65_drift, Mem(at::kA65DriftUBase), sizeof g_a65_drift);

    if (Wants(65)) {
        ah::Group g{"area_w1e", kClones65, AH_COUNT(kClones65), kCallees65, AH_COUNT(kCallees65), kTables65, AH_COUNT(kTables65),
                    g_regions65, AH_COUNT(g_regions65), &Seed65, &Disturb65, 4000};
        g.settle = &Settle65;
        g.phase_span = 5;
        g.args = &Args65;
        g.area = 65;
        ah::Run(g);
    }
    if (Wants(67)) {
        ah::Group g{"area_w1e", kClones67, AH_COUNT(kClones67), kCallees67, AH_COUNT(kCallees67), nullptr, 0,
                    kRegions67, AH_COUNT(kRegions67), &Seed67, nullptr, 4000};
        g.args = &Args67;
        g.area = 67;
        ah::Run(g);
    }
}

}  // namespace area_w1e
