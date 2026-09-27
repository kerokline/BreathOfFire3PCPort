// BOF3X_SHADOW=area_w0b: world 0's areas 16 and 18..26 through the area
// round's shared harness (area_harness.h), once at start-up: one
// area_harness::Run per area (Group::area its number, the real descriptor and
// tables in place, the area's own .data state tables swapped through
// DataTable). docs/area_w0b.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA<nnn> --clones,
// 2026-09-27), each read against the disassembly; the one change is area 20's
// init, cloned from its body 0x402DC0 (its entry is a five-byte jmp there,
// which CloneOriginal takes for a detour), the call offsets 0x10 nearer.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0b.h"
#include "game/area_w0b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w0b {
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

// --- the fuzz's own memory: a packet buffer, four map items, four names ------

constexpr unsigned kPacketBytes = 0x200, kItemBytes = 0x48;
alignas(16) unsigned char g_packets[kPacketBytes];
alignas(16) unsigned char g_items[4][kItemBytes];
alignas(16) unsigned char g_names[4][16];

// The exe's own bytes of the tables the fuzz randomises, put back two rounds
// in three (so the seeds see the shipped values most of the time).
unsigned char g_a16_tables[0x37C];     // 0x5E5BE0..0x5E5F5B
unsigned char g_a16_dirs[0x18];        // 0x5E6014..0x5E602B
unsigned char g_a16_drift[0xC];        // 0x5E6034..0x5E603F
unsigned char g_a20_tables[0x18];      // 0x5E6C3C..0x5E6C53

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
// Inventory_Count: its word 0 a quarter of the time (garbage above).
U CountEffect(const U*, U answer) { return (answer & 3) == 0 ? answer & 0xFFFF0000u : answer; }

// Gte_RotTransPers4's listing: the vectors' eight bytes each, the corners'
// addresses, the two locals not logged (stack addresses differ).
const ah::Callee kPers4 = {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, KeyOf(&::Gte_RotTransPers4), 10,
                               {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, ah::Answer::kGarbage, 0, 0,
                               {8, 8, 8, 8}, &PersEffect, nullptr};

// ===========================================================================
// Area 16
// ===========================================================================

// clones: tools/area_rows.py --unit AREA016 --clones
constexpr ah::CallSite kCalls401B80[] = {{0x22, 0x57C7A0}, {0x3B, 0x57C7C0}, {0x4F, 0x536700}, {0x8E, 0x4976D0}, {0x113, 0x591680}, {0x154, 0x4976D0}};
constexpr ah::CallSite kCalls401D00[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls401DE0[] = {{0xB, 0x589590}};
constexpr ah::CallSite kCalls401E30[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls401F80[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls401FD0[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls402030[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls4020A0[] = {{0x0, 0x4020B0}, {0x5, 0x402180}};
constexpr ah::CallSite kCalls4020D0[] = {{0x1C, 0x402100}};
constexpr ah::CallSite kCalls402100[] = {{0x1F, 0x4022E0}};
constexpr ah::CallSite kCalls402130[] = {{0x38, 0x4022E0}};
constexpr ah::CallSite kCalls4021A0[] = {{0x59, 0x402570}};
constexpr ah::CallSite kCalls402210[] = {{0x65, 0x402570}};
constexpr ah::CallSite kCalls402280[] = {{0x4E, 0x402570}};
constexpr ah::CallSite kCalls4022E0[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x4024B0}, {0x51, 0x536700}, {0x7B, 0x4024B0}, {0xDC, 0x4024B0}, {0xF8, 0x4024B0}, {0x15B, 0x4024B0}, {0x173, 0x531920}, {0x1A8, 0x4024B0}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls4024B0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls402570[] = {{0x17, 0x4024B0}, {0x26, 0x4024B0}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls4025F0[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls402770[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls402830[] = {{0xB7, 0x5A75D0}, {0xBF, 0x5A77A0}, {0x199, 0x5A85F0}, {0x19F, 0x5A9290}, {0x1AF, 0x572A00}, {0x1BB, 0x461E50}, {0x21D, 0x572F70}, {0x236, 0x5A75D0}, {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};

#define W0B_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W0B_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
const ah::Clone kClones16[] = {
    W0B_C(Area16_PlaceMessage, 0x401B80, 0x174, kCalls401B80, S::kState),
    W0B_C(Area16_PlateRun, 0x401D00, 0xD6, kCalls401D00, S::kState),
    W0B_C(Area16_PlateStart, 0x401DE0, 0x4E, kCalls401DE0, S::kState),
    W0B_C(Area16_PlateShow, 0x401E30, 0x142, kCalls401E30, S::kState),
    W0B_C(Area16_PlateGrow, 0x401F80, 0x41, kCalls401F80, S::kState),
    W0B_C(Area16_PlateHold, 0x401FD0, 0x58, kCalls401FD0, S::kState),
    W0B_C(Area16_PlateShrink, 0x402030, 0x50, kCalls402030, S::kState),
    W0B_P(Area16_HudRun, 0x402080, 0x12, S::kState),
    W0B_C(Area16_HudFrame, 0x4020A0, 0xA, kCalls4020A0, S::kState),
    W0B_P(Area16_FrameStep, 0x4020B0, 0x12, S::kCallee),
    W0B_C(Area16_FrameSlideIn, 0x4020D0, 0x21, kCalls4020D0, S::kState),
    W0B_C(Area16_FrameHold, 0x402100, 0x28, kCalls402100, S::kState),
    W0B_C(Area16_FrameSlideOut, 0x402130, 0x41, kCalls402130, S::kState),
    W0B_P(Area16_BoxStep, 0x402180, 0x12, S::kCallee),
    W0B_C(Area16_BoxSlideIn, 0x4021A0, 0x62, kCalls4021A0, S::kState),
    W0B_C(Area16_BoxHold, 0x402210, 0x6E, kCalls402210, S::kState),
    W0B_C(Area16_BoxSlideOut, 0x402280, 0x57, kCalls402280, S::kState),
    W0B_C(Area16_DrawFrame, 0x4022E0, 0x1C5, kCalls4022E0, S::kCallee),
    W0B_C(Area16_DrawSprite, 0x4024B0, 0xBC, kCalls4024B0, S::kCallee),
    W0B_C(Area16_DrawHud, 0x402570, 0x58, kCalls402570, S::kCallee),
    W0B_P(Area16_Record8Run, 0x4025D0, 0x12, S::kState),
    W0B_C(Area16_Record8Place, 0x4025F0, 0x154, kCalls4025F0, S::kState),
    W0B_P(Area16_Record4Run, 0x402750, 0x12, S::kState),
    W0B_C(Area16_Record4MarkCell, 0x402770, 0xB5, kCalls402770, S::kState),
    W0B_C(Area16_DrawDrift, 0x402830, 0x462, kCalls402830, S::kState),
};
enum : unsigned {
    kPlaceMessage, kPlateRun, kPlateStart, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame,
    kFrameStep, kFrameSlideIn, kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame,
    kDrawSprite, kDrawHud, kRecord8Run, kRecord8Place, kRecord4Run, kRecord4MarkCell, kDrawDrift,
};
static_assert(kDrawDrift + 1 == sizeof kClones16 / sizeof kClones16[0], "area 16's seeding indices");

const ah::Callee kCallees16[] = {
    // area 16's own, called directly
    {"Area16_FrameStep", 0x4020B0, 0x4020B0, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area16_BoxStep", 0x402180, 0x402180, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area16_DrawFrame", 0x4022E0, 0x4022E0, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},   // y pushed with a stale high half
    {"Area16_DrawSprite", 0x4024B0, 0x4024B0, 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},   // a lit key's index carries the button word above its byte
    {"Area16_DrawHud", 0x402570, 0x402570, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},   // y pushed with the object pointer's high half
    // named, ours, beyond the standard set
    {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
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

const ah::DataTable kTables16[] = {
    {at::kA16PlateStates, 5}, {at::kA16HudStates, 2}, {at::kA16FrameStates, 4},
    {at::kA16BoxStates, 4},   {at::kA16Record8States, 3}, {at::kA16Record4States, 2},
};

ah::Region g_regions16[] = {
    {at::kMapMode, 1},
    {0x7E0918, 1},                // Draw_PassFlags
    {0x7E0670, 4},                // Gfx_PacketNext
    {0, kPacketBytes},            // g_packets (set at start-up)
    {0x9037A0, 0x20},             // Prim_VertexScratch
    {0x66C7E8, 2},                // Game_Mode
    {at::kButtonMap0, 0x10},      // the button map's words 0..7
    {0, 4 * kItemBytes},          // g_items
    {0, sizeof g_names},          // g_names
    {at::kTextRecords, 0x80},     // Text_Records' first four rows
    {at::kAreaTextOffset, 4},
    {at::kFlag3A79, 1},
    {at::kA16PlateAnims, 0x37C},  // area 16's plate, cell, message and name tables
    {at::kA16Directions, 0x18},   // Area16_Directions, Area16_Record8Anims
    {at::kA16DriftUBase, 0xC},    // Area16_DriftUV
};

unsigned char* Obj() { return Sprite_Current; }

// The leader's cell words as bytes, and one Area16_Cells record planted with
// them (the search has no bound), a sentinel in the last record.
void PlantCell(bool random_place) {
    unsigned char* const cx = Mem(at::kLeaderCellWordX);
    unsigned char* const cz = Mem(at::kLeaderCellWordZ);
    cx[1] = 0;
    cz[1] = 0;
    const U n = (at::kA16CellsEnd - at::kA16Cells) / 4;
    unsigned char* const last = Mem(at::kA16CellsEnd - 4);
    last[0] = cx[0];
    last[1] = cz[0];
    // the sentinel's id follows the words, so a search with stale words (or
    // none again) lands on a record of another set
    last[3] = Mem(at::kA16NameSets + ((cx[0] ^ cz[0]) % 3) * 5)[0];
    if (random_place) {
        unsigned char* const r = Mem(at::kA16Cells + (ah::Next() % n) * 4);
        r[0] = cx[0];
        r[1] = cz[0];
    }
}

// Area 16's disturbance: the cells its functions read again after a call.
void Disturb16(U h) {
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
void Settle16() {
    unsigned char* const o = Obj();
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(false);
}

void Seed16(unsigned k) {
    unsigned char* const o = Obj();
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(at::kA16PlateAnims), g_a16_tables, sizeof g_a16_tables);
    if (ah::Often()) std::memcpy(Mem(at::kA16Directions), g_a16_dirs, sizeof g_a16_dirs);
    if (ah::Often()) std::memcpy(Mem(at::kA16DriftUBase), g_a16_drift, sizeof g_a16_drift);
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
        // the place in one of the nine rows, or in none
        if (ah::Often()) SetWord(Mem(at::kA16PlaceMessages + (ah::Next() % 9) * 0x20), Word(Mem(at::kPlace)));
        // the found cell's id one of the three sets', or none
        unsigned char* cell = Mem(at::kA16Cells);
        while (!(cell[0] == Mem(at::kLeaderCellWordX)[0] && cell[1] == Mem(at::kLeaderCellWordZ)[0])) cell += 4;
        if (ah::Often()) cell[3] = Mem(at::kA16NameSets + (ah::Next() % 3) * 5)[0];
        // items held or not, now and then a list ended early
        for (unsigned i = 0; i < 0x74; ++i)
            if (ah::Half()) Mem(at::kItemsHeld + i)[0] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Half()) Mem(at::kA16NameSets + 1 + (ah::Next() % 3) * 5 + ah::Next() % 4)[0] = static_cast<unsigned char>(AH_PICK(0xFF, 0x16, 0x5E));
        break;
    }
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) Mem(at::kLeaderSteps)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
        SetWord(Mem(at::kA16PlateAnims + (ah::Next() % 9) * 4), Word(Mem(at::kPlace)));
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
        if (ah::Often()) SetLong(Mem(at::kButtonMap0), static_cast<std::int32_t>(Word(Mem(at::kA16Buttons + (ah::Next() % 6) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) {
            // a word only the eighth entry answers, when it has such bits
            U others = 0;
            for (U e = 0; e < 7; ++e) others |= Word(Mem(at::kA16Buttons + e * 4));
            const U only = Word(Mem(at::kA16Buttons + 7 * 4)) & ~others;
            if (only != 0) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(only | (ah::Next() & 0xFFFF0000u)));
        } else if (ah::Often()) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(Word(Mem(at::kA16Buttons + (ah::Half() ? 6 + ah::Next() % 2 : ah::Next() % 8) * 4)) | (ah::Next() & 0xFFFF0000u)));
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
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Next() % 138);
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
void Args16(unsigned k, U* a) {
    if (k == kDrawFrame || k == kDrawHud || k == kDrawSprite) {
        a[0] = ah::Often() ? AH_PICK(0x10, 0x5C, 0, 0x7FF0) : ah::Next();
        a[1] = ah::Often() ? AH_PICK(0xFFD0, 0xFFFFFFD0u, 0xC8, 0xF0, 0x10) | (ah::Half() ? ah::Next() & 0xFFFF0000u : 0) : ah::Next();
        if (k == kDrawSprite) a[2] = ah::Often() ? AH_PICK(0, 1, 2, 3, 4, 5, 0x15, 0x100, 0x1FF) : ah::Next();
    }
}

// ===========================================================================
// Areas 18..26
// ===========================================================================

// --- area 18 ---
constexpr ah::CallSite kCalls402D00[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}, {0x32, 0x579F00}, {0x3D, 0x579F00}, {0x4B, 0x579F00}, {0x56, 0x579F00}};
const ah::Clone kClones18[] = {
    W0B_P(Area18_SetRecordByte, 0x402CA0, 0x32, S::kInit),
    W0B_P(Area18_SkipScript, 0x402CE0, 0x19, S::kHandler),
    W0B_C(Area18_ClearCells, 0x402D00, 0x5F, kCalls402D00, S::kHandler),
};
constexpr U kArea18Record = 0x667ABC;   // Area_StateRecords[18] (0x667AB8) + 4
const ah::Region kRegions18[] = {{kArea18Record, 1}, {0x929E80, 4}};
void Seed18(unsigned k) {
    if (k == 0) Cond_ByteFA = static_cast<signed char>(ah::Often() ? AH_PICK(8, 8, 7, 9, 0x88, 0) : ah::Next());
    if (k == 1) {
        if (ah::Often()) Field_State[0x89] = static_cast<unsigned char>(AH_PICK(2, 2, 0, 3, 0x82));
        MoveScript_Object = ah::Half() ? ah::Object(ah::Next() % 4) : ah::PartyOf(static_cast<unsigned char>(ah::Next()));
    }
}

// --- area 19 ---
constexpr ah::CallSite kCalls402DA0[] = {{0x2, 0x5734F0}};
const ah::Clone kClones19[] = {
    W0B_P(Area19_CameraOut, 0x402D60, 0x11, S::kHandler),
    W0B_P(Area19_CameraIn, 0x402D80, 0x11, S::kHandler),
    W0B_C(Area19_PlaceKind2, 0x402DA0, 0x9, kCalls402DA0, S::kHandler),
};
const ah::Callee kCallees19[] = {
    {"Kind2_Place", bof3::addr::Kind2_Place, KeyOf(&::Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
};
void Seed19(unsigned k) {
    if (k < 2 && ah::Often()) Camera_Distance = static_cast<short>(AH_PICK(0, 0x7FFF, 0x8000, 0xF600, 0x0A00, 0x75FF));
}

// --- area 20 ---
// The entry is a five-byte jmp 0x402DC0: cloned from the body, the tool's
// call offsets (0x12, 0x4D, 0x98 from 0x402DB0) 0x10 nearer.
constexpr ah::CallSite kCalls402DC0[] = {{0x2, 0x5B93D2}, {0x3D, 0x5B93D2}, {0x88, 0x5720C0}};
const ah::Clone kClones20[] = {
    W0B_C(Area20_PickFieldObject, 0x402DC0, 0xBB, kCalls402DC0, S::kInit),
};
const ah::Region kRegions20[] = {{at::kA20Cells, 0x18}};
void Seed20(unsigned) {
    if (ah::Often()) std::memcpy(Mem(at::kA20Cells), g_a20_tables, sizeof g_a20_tables);
    // the first roll at a chance's edge (the running sum, or one below), bits
    // above the 0x3F mask now and then
    if (ah::Half()) {
        const unsigned upto = ah::Next() % 9;
        unsigned sum = 0;
        for (unsigned i = 0; i < upto; ++i) sum += Mem(at::kA20Weights + i)[0];
        ah::SetRandFirst(static_cast<int>(((sum - (ah::Half() ? 1u : 0u)) & 0x3F) | (ah::Next() & 0xC0)));
    }
    if (ah::Half()) std::memset(Mem(at::kA20Weights), ah::Half() ? 0 : 7, 8);   // none kept (8)
}

// --- area 21 ---
constexpr ah::CallSite kCalls402E80[] = {{0x20, 0x591BC0}};
constexpr ah::CallSite kCalls402EE0[] = {{0xF, 0x5919B0}, {0x44, 0x5B9380}, {0x55, 0x591BE0}, {0x62, 0x591B60}, {0x70, 0x57C0F0}};
const ah::Clone kClones21[] = {
    W0B_C(Area21_ChoicePay, 0x402E80, 0x3C, kCalls402E80, S::kChoice),
    W0B_P(Area21_ChoiceMessage, 0x402EC0, 0x17, S::kChoice),
    W0B_C(Area21_ChoiceTrade, 0x402EE0, 0xAF, kCalls402EE0, S::kChoice),
};
const ah::Callee kCallees21[] = {
    {"money take 0x591BC0", kMoneyTake, kMoneyTake, 1, {kAll}, ah::Answer::kFlag, 0, 0},
    {"money give 0x591BE0", kMoneyGive, kMoneyGive, 2, {kAll, kU8}, ah::Answer::kFlag, 0, 0},
    {"inventory take 0x591B60", kInventoryTake, kInventoryTake, 3, {kU8, kU8, kU8}, ah::Answer::kFlag, 0, 0},
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"Inventory_Count", bof3::addr::Inventory_Count, KeyOf(&::Inventory_Count), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CountEffect, nullptr},
};
const ah::Region kRegions21[] = {{at::kTextRecords, 0x80}, {at::kFlagBase, 4}};
// Areas 21 and 26: the flag row's pointer moved (never followed: the
// recorders log it), so a pointer not read again shows.
void DisturbFlagRow(U h) {
    if ((h >> 16) % 3 == 0) SetLong(Mem(at::kFlagBase), static_cast<std::int32_t>(0x904000u + ((h >> 8) & 0xF0)));
}
void Seed21(unsigned k) {
    Mem(at::kChoice)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2, 0xFF, 0x80) : ah::Next());
    if (k == 0 && ah::Often()) SetLong(Mem(at::kMoney), static_cast<std::int32_t>(AH_PICK(0x13, 0x14, 0x15, 0, 0x98967F, 0x80000000u)));
    if (k == 2 && ah::Often()) Mem(at::kCounter)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 6, 7, 8, 0xFF, 0x19));
}

// --- area 22 ---
constexpr ah::CallSite kCalls403020[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}};
const ah::Clone kClones22[] = {
    W0B_P(Area22_ChoiceA, 0x402F90, 0x28, S::kChoice),
    W0B_P(Area22_ChoiceB, 0x402FC0, 0x28, S::kChoice),
    W0B_P(Area22_ChoiceC, 0x402FF0, 0x28, S::kChoice),
    W0B_C(Area22_ClearCells, 0x403020, 0x30, kCalls403020, S::kHandler),
};
void SeedChoice(unsigned) {
    Mem(at::kChoice)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 0xFF, 0x80, 0x7F, 0x81) : ah::Next());
}

// --- area 23 (and the nine areas' shared choice 0x403050, which area 23 reaches) ---
constexpr ah::CallSite kCalls403080[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}};
constexpr ah::CallSite kCalls4030B0[] = {{0x9, 0x579F00}, {0x17, 0x579F00}, {0x25, 0x579F00}, {0x33, 0x579F00}};
const ah::Clone kClones23[] = {
    W0B_P(Area22_ArmTailOnYes, 0x403050, 0x28, S::kChoice),
    W0B_C(Area23_ClearCells, 0x403080, 0x30, kCalls403080, S::kChoice),
    W0B_C(Area23_SetCells, 0x4030B0, 0x3C, kCalls4030B0, S::kChoice),
    W0B_P(Area23_CameraUp, 0x4030F0, 0x10, S::kChoice),
    W0B_P(Area23_CameraDown, 0x403100, 0x10, S::kChoice),
};
const ah::Region kRegions23[] = {{0x903802, 2}};   // Camera_ShiftY
void Seed23(unsigned k) {
    SeedChoice(k);
    if (ah::Half()) Camera_ShiftY = static_cast<short>(AH_PICK(0, 0x7FFF, 0x8000, 0x12, 0xFFEE, 0x7FF0));
}

// --- area 24 ---
const ah::Clone kClones24[] = {
    W0B_P(Area24_NoMessage, 0x403110, 0xA, S::kChoice),
};

// --- area 25 ---
constexpr ah::CallSite kCalls403120[] = {{0x9, 0x589810}};
const ah::Clone kClones25[] = {
    W0B_C(Area25_SpawnEffect, 0x403120, 0x57, kCalls403120, S::kHandler),
};
// Effect_FindFree: a slot of the first eight or none (0xFF), garbage above.
const ah::Callee kFindFree = {"Effect_FindFree", bof3::addr::Effect_FindFree, KeyOf(&::Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x07};
const ah::Callee kCallees25[] = {kFindFree};
const ah::Region kRegionsEffects[] = {{0x7E11E0, 8 * 0x80}, {0x903802, 2}, {at::kFlagBase, 4}};   // Effect_Objects' first eight
void Seed25(unsigned) {
    if (ah::Often()) Frame_Counter &= ~3u;
    if (ah::Often()) Mem(at::kEffectCount)[0] = static_cast<unsigned char>(AH_PICK(0x13, 0x14, 0, 0xFF, 0x15));
}

// --- area 26 ---
constexpr ah::CallSite kCalls4031E0[] = {{0x8, 0x57C0F0}, {0x16, 0x57C110}};
constexpr ah::CallSite kCalls403200[] = {{0x8, 0x57C110}};
constexpr ah::CallSite kCalls403220[] = {{0x8, 0x57C0F0}};
constexpr ah::CallSite kCalls403240[] = {{0x8, 0x57C110}};
constexpr ah::CallSite kCalls403260[] = {{0x8, 0x57C0F0}};
constexpr ah::CallSite kCalls403280[] = {{0x8, 0x57C110}};
constexpr ah::CallSite kCalls4032A0[] = {{0x8, 0x57C0F0}};
constexpr ah::CallSite kCalls4032C0[] = {{0x8, 0x57C110}};
constexpr ah::CallSite kCalls4032E0[] = {{0x8, 0x590BB0}, {0x12, 0x587740}, {0x1F, 0x57C0F0}};
constexpr ah::CallSite kCalls403310[] = {{0x12, 0x587B40}};
constexpr ah::CallSite kCalls403330[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls403380[] = {{0x8, 0x57C0F0}};
constexpr ah::CallSite kCalls4033A0[] = {{0x1, 0x589810}, {0x3F, 0x5720C0}};
const ah::Clone kClones26[] = {
    W0B_P(Area26_ChoiceMessage, 0x403180, 0x2E, S::kChoice),
    W0B_P(Area26_ChoiceSetVar, 0x4031B0, 0x28, S::kChoice),
    W0B_C(Area26_Flag29Set28Clear, 0x4031E0, 0x1F, kCalls4031E0, S::kChoice),
    W0B_C(Area26_Flag29Clear, 0x403200, 0x11, kCalls403200, S::kChoice),
    W0B_C(Area26_Flag28Set, 0x403220, 0x11, kCalls403220, S::kChoice),
    W0B_C(Area26_Flag28Clear, 0x403240, 0x11, kCalls403240, S::kChoice),
    W0B_C(Area26_Flag2BSet, 0x403260, 0x11, kCalls403260, S::kChoice),
    W0B_C(Area26_Flag2BClear, 0x403280, 0x11, kCalls403280, S::kChoice),
    W0B_C(Area26_Flag2CSet, 0x4032A0, 0x11, kCalls4032A0, S::kChoice),
    W0B_C(Area26_Flag2CClear, 0x4032C0, 0x11, kCalls4032C0, S::kChoice),
    W0B_C(Area26_GiveItem, 0x4032E0, 0x28, kCalls4032E0, S::kChoice),
    W0B_C(Area26_ResetCamera, 0x403310, 0x19, kCalls403310, S::kChoice),
    W0B_C(Area26_SpawnEffect, 0x403330, 0x42, kCalls403330, S::kHandler),
    W0B_C(Area26_Flag2ESet, 0x403380, 0x11, kCalls403380, S::kChoice),
    W0B_C(Area26_PlaceEffect, 0x4033A0, 0x53, kCalls4033A0, S::kChoice),
};
const ah::Callee kCallees26[] = {
    kFindFree,
    // Effect_Spawn answers a slot or 0xFF for none: 0xFE..0x02, as area 11 lists it
    {"Effect_Spawn", 0x57CE10, 0x57CE10, 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02},
};
void Seed26(unsigned k) {
    SeedChoice(k);
    if (ah::Often()) Mem(at::kPartyFirst)[0] = static_cast<unsigned char>(ah::Next() % 8);
    if (ah::Half()) Camera_ShiftY = static_cast<short>(ah::Next());
}

#undef W0B_C
#undef W0B_P

template <std::size_t N> constexpr unsigned Count(const ah::Clone (&)[N]) { return N; }

void RunArea(const char* shadow, int area, const ah::Clone* clones, unsigned n, const ah::Callee* callees, unsigned n_callees,
             const ah::DataTable* tables, unsigned n_tables, const ah::Region* regions, unsigned n_regions,
             void (*seed)(unsigned), void (*disturb)(U), unsigned rounds) {
    ah::Group g{shadow, clones, n, callees, n_callees, tables, n_tables, regions, n_regions, seed, disturb, rounds};
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    g_regions16[3].at = Key(g_packets);
    g_regions16[7].at = Key(g_items);
    g_regions16[8].at = Key(g_names);
    std::memcpy(g_a16_tables, Mem(at::kA16PlateAnims), sizeof g_a16_tables);
    std::memcpy(g_a16_dirs, Mem(at::kA16Directions), sizeof g_a16_dirs);
    std::memcpy(g_a16_drift, Mem(at::kA16DriftUBase), sizeof g_a16_drift);
    std::memcpy(g_a20_tables, Mem(at::kA20Cells), sizeof g_a20_tables);

    {
        ah::Group g{"area_w0b", kClones16, Count(kClones16), kCallees16, AH_COUNT(kCallees16), kTables16, AH_COUNT(kTables16),
                    g_regions16, AH_COUNT(g_regions16), &Seed16, &Disturb16, 4000};
        g.settle = &Settle16;
        g.phase_span = 5;
        g.args = &Args16;
        g.area = 16;
        ah::Run(g);
    }
    RunArea("area_w0b", 18, kClones18, Count(kClones18), nullptr, 0, nullptr, 0, kRegions18, AH_COUNT(kRegions18), &Seed18, nullptr, 4000);
    RunArea("area_w0b", 19, kClones19, Count(kClones19), kCallees19, AH_COUNT(kCallees19), nullptr, 0, nullptr, 0, &Seed19, nullptr, 4000);
    RunArea("area_w0b", 20, kClones20, Count(kClones20), nullptr, 0, nullptr, 0, kRegions20, AH_COUNT(kRegions20), &Seed20, nullptr, 8000);
    RunArea("area_w0b", 21, kClones21, Count(kClones21), kCallees21, AH_COUNT(kCallees21), nullptr, 0, kRegions21, AH_COUNT(kRegions21), &Seed21, &DisturbFlagRow, 4000);
    RunArea("area_w0b", 22, kClones22, Count(kClones22), nullptr, 0, nullptr, 0, nullptr, 0, &SeedChoice, nullptr, 4000);
    RunArea("area_w0b", 23, kClones23, Count(kClones23), nullptr, 0, nullptr, 0, kRegions23, AH_COUNT(kRegions23), &Seed23, nullptr, 4000);
    RunArea("area_w0b", 24, kClones24, Count(kClones24), nullptr, 0, nullptr, 0, nullptr, 0, &SeedChoice, nullptr, 4000);
    RunArea("area_w0b", 25, kClones25, Count(kClones25), kCallees25, AH_COUNT(kCallees25), nullptr, 0, kRegionsEffects, AH_COUNT(kRegionsEffects), &Seed25, nullptr, 4000);
    RunArea("area_w0b", 26, kClones26, Count(kClones26), kCallees26, AH_COUNT(kCallees26), nullptr, 0, kRegionsEffects, AH_COUNT(kRegionsEffects), &Seed26, &DisturbFlagRow, 4000);
}

}  // namespace area_w0b
