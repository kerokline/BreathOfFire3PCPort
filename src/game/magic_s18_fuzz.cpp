// BOF3X_SHADOW=magic_s18: MAGIC079 and MAGIC082 through the spell round's
// shared harness (magic_harness.h), once at start-up. docs/magic_s18.md
// section 3.
//
// The clone tables are tools/magic_rows.py --unit MAGIC079 / MAGIC082
// --clones (2026-09-25), the placeholders renamed, the .data tables at their
// read sizes (the tool counts the code pointers running on to the next
// table). The regions the harness does not hold already: the packet cursor
// and a primitive buffer of the fuzz's own, the scratch words and the vertex
// scratch the draws build in, the action id, the tint records, the CLUT strip.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s18.h"
#include "game/move_script_bytes.h"

namespace magic_s18 {
namespace {

namespace mh = magic_harness;

// --- the clones ----------------------------------------------------------------

constexpr mh::Imm kImms4BEB50[] = {{0xF, 0x4BEB80}, {0x17, 0x4F7350}};
constexpr mh::CallSite kCalls4BEB80[] = {{0x11, 0x435180}, {0x65, 0x587900}};
constexpr mh::CallSite kCalls4BEC20[] = {{0x23, 0x4B7D40}, {0x28, 0x4BEF70}, {0x2D, 0x5A7BC0}};
constexpr mh::CallSite kCalls4BED90[] = {{0x26, 0x4351F0}};
constexpr mh::CallSite kCalls4BEDC0[] = {{0x23, 0x4B7D40}, {0x28, 0x4BEF70}, {0x2D, 0x5A7BC0}};
constexpr mh::CallSite kCalls4BEF40[] = {{0x26, 0x4351F0}};
constexpr mh::CallSite kCalls4BEF70[] = {
    {0x19, 0x5A77C0},  {0x2F, 0x572FA0},  {0x5D, 0x5A7A00},  {0x9E, 0x5A7A00},  {0xF7, 0x5A7A00},  {0x161, 0x5A7A00},
    {0x1CE, 0x5A7A00}, {0x20A, 0x5A7A00}, {0x21B, 0x5A7A00}, {0x27F, 0x5A7A00}, {0x290, 0x5A7A00}, {0x2EC, 0x5A7A50},
    {0x30C, 0x5A7A00}, {0x332, 0x5A7A50}, {0x352, 0x5A7A00}, {0x3D5, 0x5A7A50}, {0x3F0, 0x5A7A00}, {0x415, 0x5A7A50},
    {0x430, 0x5A7A00}, {0x45B, 0x5A7610}, {0x54E, 0x5A85F0}, {0x554, 0x5A9350}, {0x56D, 0x572FA0}};
constexpr mh::CallSite kCalls4BF520[] = {{0x23, 0x4B7D40}, {0x28, 0x4BF720}, {0x2D, 0x5A7BC0}};
constexpr mh::CallSite kCalls4BF5E0[] = {{0x2D, 0x4351F0}};
constexpr mh::CallSite kCalls4BF620[] = {{0x23, 0x4B7D40}, {0x28, 0x4BF720}, {0x2D, 0x5A7BC0}};
constexpr mh::CallSite kCalls4BF6E0[] = {{0x2D, 0x4351F0}};
constexpr mh::CallSite kCalls4BF720[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x3D, 0x5A7A00}, {0x56, 0x5A7A50},
                                         {0x7D, 0x5A75F0}, {0x84, 0x5A7780}, {0xB2, 0x5A7A00}, {0xCB, 0x5A7A50},
                                         {0x11D, 0x5A84A0}, {0x123, 0x5A9310}, {0x165, 0x461E50}, {0x18B, 0x5A77C0},
                                         {0x194, 0x461E50}};
constexpr mh::Imm kImms4C01F0[] = {{0xF, 0x4C0240},  {0x17, 0x4B1E70}, {0x22, 0x4B1ED0},
                                   {0x2A, 0x4C03B0}, {0x32, 0x4C03D0}, {0x3A, 0x4E5200}};
constexpr mh::CallSite kCalls4C0240[] = {{0x37, 0x4C04F0}, {0x65, 0x435180}, {0xCD, 0x435180}, {0x164, 0x587900}};
constexpr mh::CallSite kCalls4C03D0[] = {{0x6C, 0x454DC0}, {0x78, 0x4FBDB0}, {0x95, 0x4FB6F0}, {0xA5, 0x435180}, {0xD3, 0x435180}};
constexpr mh::JumpTable kTables4C04F0[] = {{0x28, 0x78, 4}};
constexpr mh::CallSite kCalls4C0640[] = {{0x1D, 0x4B7D40}, {0x22, 0x4C08A0}, {0x27, 0x4C06A0}, {0x2C, 0x5A7BC0}};
constexpr mh::CallSite kCalls4C06A0[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x8E, 0x5A7A00}, {0xA0, 0x5A7A50},
                                         {0xE4, 0x5A7A00}, {0xF6, 0x5A7A50}, {0x123, 0x5A75F0}, {0x12B, 0x5A7780},
                                         {0x155, 0x5A84A0}, {0x15B, 0x5A9310}, {0x1B2, 0x461E50}, {0x1D9, 0x5A77C0},
                                         {0x1E2, 0x461E50}};
constexpr mh::CallSite kCalls4C08A0[] = {
    {0x19, 0x5A77C0},  {0x22, 0x461E50},  {0x84, 0x5A7A00},  {0x96, 0x5A7A50},  {0xA8, 0x5A7A00},  {0xBA, 0x5A7A50},
    {0x10C, 0x5A7A00}, {0x11E, 0x5A7A50}, {0x130, 0x5A7A00}, {0x142, 0x5A7A50}, {0x176, 0x5A7610}, {0x17D, 0x5A7780},
    {0x1B0, 0x5A85F0}, {0x1B9, 0x5A9350}, {0x208, 0x461E50}, {0x22E, 0x5A77C0}, {0x237, 0x461E50}};
constexpr mh::CallSite kCalls4C0B10[] = {{0x0, 0x4FBD10},  {0x5, 0x5B93D2},  {0x11, 0x4C12F0},
                                         {0x19, 0x4B7D40}, {0x1E, 0x4C0E30}, {0x23, 0x5A7BC0}};
constexpr mh::CallSite kCalls4C0BB0[] = {{0x0, 0x4FBD10},  {0x5, 0x5B93D2},  {0x11, 0x4C12F0},
                                         {0x19, 0x4B7D40}, {0x1E, 0x4C0E30}, {0x23, 0x5A7BC0}};
constexpr mh::CallSite kCalls4C0C40[] = {{0x0, 0x4FBD10},  {0x16, 0x5B93D2}, {0x23, 0x5B93D2}, {0x2F, 0x4C12F0},
                                         {0x37, 0x4B7D40}, {0x3C, 0x4C0E30}, {0x41, 0x5A7BC0}, {0xA5, 0x587900}};
constexpr mh::CallSite kCalls4C0D20[] = {{0x0, 0x4FBD10},  {0x5, 0x5B93D2},  {0x11, 0x4C12F0}, {0x16, 0x4B7D40}, {0x1B, 0x4C0E30},
                                         {0x20, 0x5A7BC0}, {0x86, 0x5A7A00}, {0xB4, 0x5A7A50}, {0xFE, 0x4351F0}};
constexpr mh::CallSite kCalls4C0E30[] = {
    {0x15, 0x5A77C0},  {0x2B, 0x572FA0},  {0xF6, 0x5A7A00},  {0x116, 0x5A7A50}, {0x14D, 0x5A7A00}, {0x16D, 0x5A7A50},
    {0x193, 0x5A75F0}, {0x19A, 0x5A7780}, {0x1C4, 0x5A87A0}, {0x277, 0x4FB880}, {0x32C, 0x5A7A00}, {0x34C, 0x5A7A50},
    {0x383, 0x5A7A00}, {0x3A3, 0x5A7A50}, {0x3C9, 0x5A75F0}, {0x3D0, 0x5A7780}, {0x3FA, 0x5A87A0}, {0x4A9, 0x4FB880}};
constexpr mh::CallSite kCalls4C12F0[] = {{0x23, 0x5A77C0}, {0x39, 0x572FA0}, {0x45, 0x5A75F0}, {0x4C, 0x5A7780}, {0x7B, 0x5A7A00},
                                         {0xA5, 0x5A7A50}, {0xD5, 0x5A7A00}, {0xFF, 0x5A7A50}, {0x15A, 0x572FA0}};

#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S18_FN(name) reinterpret_cast<const void*>(&::name)
#define S18_PLAIN(name, base, size) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, S18_FN(name)}
#define S18_CALLS(name, base, size, calls) {#name, base, size, calls, MH_N(calls), nullptr, 0, nullptr, 0, S18_FN(name)}
const mh::Clone kClones[] = {
    {"Drain_Task", 0x4BEB50, 0x26, nullptr, 0, kImms4BEB50, MH_N(kImms4BEB50), nullptr, 0, S18_FN(Drain_Task)},
    S18_CALLS(Drain_Start, 0x4BEB80, 0x7D, kCalls4BEB80),
    S18_PLAIN(DrainOrb_Dispatch, 0x4BEC00, 0x12),
    S18_CALLS(DrainOrbA_Task, 0x4BEC20, 0x33, kCalls4BEC20),
    S18_PLAIN(DrainOrbA_Place, 0x4BEC60, 0x61),
    S18_PLAIN(DrainOrbA_Drop, 0x4BECD0, 0x27),
    S18_PLAIN(DrainOrbA_Run, 0x4BED00, 0x38),
    S18_PLAIN(DrainOrbA_Signal, 0x4BED40, 0x41),
    S18_CALLS(DrainOrbA_Shrink, 0x4BED90, 0x2C, kCalls4BED90),
    S18_CALLS(DrainOrbB_Task, 0x4BEDC0, 0x33, kCalls4BEDC0),
    S18_PLAIN(DrainOrbB_Place, 0x4BEE00, 0x75),
    S18_PLAIN(DrainOrbB_Drop, 0x4BEE80, 0x30),
    S18_PLAIN(DrainOrbB_Run, 0x4BEEB0, 0x38),
    S18_PLAIN(DrainOrbB_Signal, 0x4BEEF0, 0x41),
    S18_CALLS(DrainOrbB_Shrink, 0x4BEF40, 0x2C, kCalls4BEF40),
    S18_CALLS(DrainOrb_Draw, 0x4BEF70, 0x5AC, kCalls4BEF70),
    S18_CALLS(DrainOrbC_Task, 0x4BF520, 0x33, kCalls4BF520),
    S18_PLAIN(DrainOrbC_Place, 0x4BF560, 0x45),
    S18_PLAIN(DrainOrbC_Wait, 0x4BF5B0, 0x23),
    S18_CALLS(DrainOrbC_Shrink, 0x4BF5E0, 0x33, kCalls4BF5E0),
    S18_CALLS(DrainOrbD_Task, 0x4BF620, 0x33, kCalls4BF620),
    S18_PLAIN(DrainOrbD_Place, 0x4BF660, 0x50),
    S18_PLAIN(DrainOrbD_Wait, 0x4BF6B0, 0x23),
    S18_CALLS(DrainOrbD_Shrink, 0x4BF6E0, 0x33, kCalls4BF6E0),
    S18_CALLS(DrainOrb_DrawDisc, 0x4BF720, 0x1A4, kCalls4BF720),
    {"Buff_Task", 0x4C01F0, 0x46, nullptr, 0, kImms4C01F0, MH_N(kImms4C01F0), nullptr, 0, S18_FN(Buff_Task)},
    S18_CALLS(Buff_Start, 0x4C0240, 0x16D, kCalls4C0240),
    S18_PLAIN(Buff_WaitChildren, 0x4C03B0, 0x18),
    S18_CALLS(Buff_Fade, 0x4C03D0, 0x11C, kCalls4C03D0),
    {"Buff_Kind", 0x4C04F0, 0x122, nullptr, 0, nullptr, 0, kTables4C04F0, MH_N(kTables4C04F0), S18_FN(Buff_Kind), 0xFF},
    S18_PLAIN(BuffFx_Dispatch, 0x4C0620, 0x12),
    S18_CALLS(BuffRing_Task, 0x4C0640, 0x32, kCalls4C0640),
    S18_PLAIN(BuffRing_Wait, 0x4C0680, 0x14),
    S18_CALLS(BuffRing_DrawDisc, 0x4C06A0, 0x1F1, kCalls4C06A0),
    S18_CALLS(BuffRing_DrawBand, 0x4C08A0, 0x247, kCalls4C08A0),
    S18_PLAIN(BuffSpike_Dispatch, 0x4C0AF0, 0x12),
    S18_CALLS(BuffSpike_Arc, 0x4C0B10, 0x95, kCalls4C0B10),
    S18_CALLS(BuffSpike_Arc2, 0x4C0BB0, 0x82, kCalls4C0BB0),
    S18_CALLS(BuffSpike_Spin, 0x4C0C40, 0xD4, kCalls4C0C40),
    S18_CALLS(BuffSpike_Orbit, 0x4C0D20, 0x104, kCalls4C0D20),
    // calm: the original stores each depth by (a - +9) / 8 with +9 read again
    // (mh::Clone::calm)
    {"BuffSpike_Draw", 0x4C0E30, 0x4B9, kCalls4C0E30, MH_N(kCalls4C0E30), nullptr, 0, nullptr, 0, S18_FN(BuffSpike_Draw), 0, true},
    S18_CALLS(MagicFx_DrawDiscRadius, 0x4C12F0, 0x172, kCalls4C12F0),
};
#undef S18_PLAIN
#undef S18_CALLS
#undef S18_FN
#undef MH_N
enum : unsigned {
    kDrainTask, kDrainStart, kOrbDispatch, kATask, kAPlace, kADrop, kARun, kASignal, kAShrink, kBTask, kBPlace, kBDrop,
    kBRun, kBSignal, kBShrink, kOrbDraw, kCTask, kCPlace, kCWait, kCShrink, kDTask, kDPlace, kDWait, kDShrink, kOrbDisc,
    kBuffTask, kBuffStart, kBuffWait, kBuffFade, kBuffKind, kFxDispatch, kRingTask, kRingWait, kRingDisc, kRingBand,
    kSpikeDispatch, kArc, kArc2, kSpin, kOrbit, kSpikeDraw, kDiscRadius, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enumerator a clone");

// --- the callees beyond the standard set ------------------------------------------

template <typename F> std::uint32_t KeyOf(F f) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(reinterpret_cast<const void*>(f)));
}
#define S18_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
const mh::Callee kCallees[] = {
    {S18_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(Math_Sin), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(Math_Cos), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(Gpu_SetPolyG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(Gpu_SetPolyG4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    // the vertices (the vertex scratch, logged by their eight bytes each) and
    // the screen points (the packet);
    // the last two pointers are the caller's stack
    {S18_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, mh::Answer::kGarbage, 0, 0, {8, 8, 8}},
    {S18_OURS(Gte_RotTransPers4), 8, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0, {8, 8, 8, 8}},
    {S18_OURS(Gte_RotAverage3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, mh::Answer::kGarbage, 0, 0, {8, 8, 8}},
    {S18_OURS(Gte_PrimDepths3_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(Gte_PrimDepths4_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    // the effect library's (queue group L): the depths array by its 16 bytes
    {"0x4FB880", 0x4FB880, 0x4FB880, 7, {kAll, kAll, kAll, kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0, {0, 0, 16}},
    {"0x4FB6F0", 0x4FB6F0, 0x4FB6F0, 2, {kU8, kU8}, mh::Answer::kFlag, 0, 0},
    // this group's own, called by its others
    {S18_OURS(DrainOrb_Draw), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(DrainOrb_DrawDisc), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(Buff_Kind), 0, {}, mh::Answer::kByte, 0, 3},
    {S18_OURS(BuffRing_DrawDisc), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(BuffRing_DrawBand), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(BuffSpike_Draw), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S18_OURS(MagicFx_DrawDiscRadius), 1, {kU16}, mh::Answer::kGarbage, 0, 0},
};
#undef S18_OURS

// The eight .data tables the dispatchers read in place, at their read sizes.
const mh::DataTable kTables[] = {{0x65B240, 4}, {0x65B250, 5}, {0x65B264, 5}, {0x65B2F8, 4},
                                 {0x65B308, 4}, {0x65B3D8, 2}, {0x65B3E0, 3}, {0x65B3EC, 6}};

// --- the state ---------------------------------------------------------------------

constexpr std::uint32_t kPacketCursor = 0x7E0670;   // Gfx_PacketNext
constexpr std::uint32_t kActorObject = 0x904B3C, kCommandKind = 0x904B35, kActionId = 0x904B80;
alignas(16) unsigned char g_prims[0x400];
unsigned char* PrimAt(std::uint32_t k) { return g_prims + 4 * (k % 32); }

const mh::Region kRegions[] = {
    {kPacketCursor, 4},
    {static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims)), sizeof g_prims},
    {bof3::addr::DamageScratch, 0x10},          // the scratch words 0x903850..0x90385F
    {0x9037A0, 0x20},                           // Prim_VertexScratch: four SVECTORs
    {kActionId, 4},
    {0x7E0700, 0xC00},   // MoveScript_TintRecords: 256 records of 12: Buff_Fade's index is a byte
    {0x80E980, 0x40},                           // Buff_Start's CLUT strip, from
    {0x812980, 0x40},                           // and to
};

// A byte at v or one either side three times in four, else left random.
void Near(unsigned char& b, unsigned v) {
    switch (mh::Next() % 4) {
    case 0: b = static_cast<unsigned char>(v); break;
    case 1: b = static_cast<unsigned char>(v - 1); break;
    case 2: b = static_cast<unsigned char>(v + 1); break;
    default: break;
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sprite_Current;
    unsigned char* const owner = mh::Pointer(mh::at::kOwner);
    // every function: the acting block's object a sprite record, the packet
    // cursor inside the fuzz's buffer
    mh::SetPointer(kActorObject, mh::SpriteRecord(mh::Next()));
    mh::SetPointer(kPacketCursor, PrimAt(mh::Next()));
    if (mh::Often()) sc[4] = static_cast<unsigned char>(mh::Next() % 4);   // a kind Buff_Kind gives
    switch (k) {
    case kDrainTask: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kOrbDispatch: sc[1] = static_cast<unsigned char>(mh::Next() % 4); break;
    case kATask: case kBTask:
        sc[2] = static_cast<unsigned char>(mh::Next() % 5);
        if (mh::Half()) sc[0] = 0;
        break;
    case kCTask: case kDTask:
        sc[2] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Half()) sc[0] = 0;
        break;
    case kADrop: case kBDrop: case kBPlace: case kDPlace: Near(sc[9], 1); break;
    case kARun: Near(sc[9], 0x20); Near(sc[0xA], 0x31); break;
    case kASignal: Near(sc[9], 0x20); Near(sc[0xA], 0x11); break;
    case kAShrink: case kBShrink: Near(sc[0xB], 1); break;
    case kBRun: Near(sc[9], 0); Near(sc[0xA], 0x7F); break;
    case kBSignal: Near(sc[9], 0); Near(sc[0xA], 0x9F); break;
    case kCWait: case kDWait:
        sc[1] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Often()) owner[0xB] = static_cast<unsigned char>(sc[1] + (mh::Half() ? 0 : 1));
        break;
    case kCShrink: case kDShrink: Near(sc[9], 5); break;
    case kOrbDraw:
        // every branch: the phase 0..5, +1 zero or not, +9 across the 24 rings
        sc[2] = static_cast<unsigned char>(mh::Next() % 6);
        if (mh::Half()) sc[1] = 0;
        if (mh::Half()) sc[9] = static_cast<unsigned char>(mh::Next() % 27);
        if (mh::Half()) sc[9] = 0;
        break;
    case kBuffTask: sc[1] = static_cast<unsigned char>(mh::Next() % 6); break;
    case kBuffWait: if (mh::Often()) sc[0xB] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kBuffFade: Near(sc[9], 1); break;
    case kBuffKind: {
        if (mh::Often()) mh::Mem(kCommandKind)[0] = 4;
        const std::uint32_t id = MH_PICK(0x22, 0x23, 0x52, 0x54, 0x55, 0xB8, 0xBA, 0xBB, 0x21, 0x24, 0x51, 0x53, 0x56, 0xB7,
                                         0xB9, 0xBC, 0x14, 0x117, 0x10B, 0x211, 0x13, 0x15, 0x116, 0x118, 0x10A, 0x10C,
                                         0x210, 0x212, 0x122, 0xAB, 0xBB22);
        if (mh::Often()) move_script::SetWord(mh::Mem(kActionId), id);
        break;
    }
    case kFxDispatch:   // +2 too, so a dispatch read from the wrong byte stays inside the table
        sc[1] = static_cast<unsigned char>(mh::Next() % 2);
        sc[2] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kRingTask:
        sc[2] = static_cast<unsigned char>(mh::Next() % 3);
        if (mh::Half()) sc[0] = 0;
        break;
    case kRingWait: if (mh::Often()) owner[0xB] = static_cast<unsigned char>(mh::Next() % 5); break;
    case kSpikeDispatch: sc[2] = static_cast<unsigned char>(mh::Next() % 6); break;
    case kArc: case kArc2: Near(sc[0xA], 1); break;
    case kSpin: {
        const std::uint32_t spin = MH_PICK(3, 4, 4, 5, 0xFFFFFFFFu, 0x80000000u);
        if (mh::Often()) move_script::SetLong(sc + 0xC, static_cast<std::int32_t>(spin));
        sc[0xA] = static_cast<unsigned char>(MH_PICK(1, 1, 4, 5, 6, 0x80));
        if (mh::Often()) sc[0xB] = static_cast<unsigned char>(mh::Next() % 2);   // the first spike, or the second
        if (mh::Half()) sc[0x10] = static_cast<unsigned char>(MH_PICK(7, 0xF, 0));
        break;
    }
    case kOrbit: Near(sc[0xA], 0xF); break;
    default: break;
    }
}

// A cell of this group's the recorders may move: the packet cursor, the
// acting block's object, the action id and the command kind.
void Disturb(std::uint32_t h) {
    switch ((h >> 8) % 4) {
    case 0: mh::SetPointer(kPacketCursor, PrimAt(h >> 12)); break;
    case 1: mh::SetPointer(kActorObject, mh::SpriteRecord(h >> 12)); break;
    case 2: move_script::SetWord(mh::Mem(kActionId), h >> 16); break;
    default: mh::Mem(kCommandKind)[0] = static_cast<unsigned char>((h >> 16) & 1 ? 4 : h >> 20); break;
    }
}

}  // namespace

void SelfTest() {
    const mh::Group group = {
        "magic_s18",
        kClones,
        sizeof kClones / sizeof kClones[0],
        kCallees,
        sizeof kCallees / sizeof kCallees[0],
        kTables,
        sizeof kTables / sizeof kTables[0],
        kRegions,
        sizeof kRegions / sizeof kRegions[0],
        &Seed,
        &Disturb,
        2000,
    };
    mh::Run(group);
}

}  // namespace magic_s18
