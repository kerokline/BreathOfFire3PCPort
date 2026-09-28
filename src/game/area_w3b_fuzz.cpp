// BOF3X_SHADOW=area_w3b: world 3's areas 120 and 121 through the area round's
// shared harness (area_harness.h), once at start-up: five area_harness::Run
// calls under the one shadow name - area 120; area 121's handlers; its
// world-map copy; its leader state 12; its effect kind 0x5C with the
// effect-kind-0x18 band and the object trigger - each with Group::area its
// number, the real descriptors and tables in place, the .data state tables
// swapped through DataTable. docs/area_w3b.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA120 / AREA121 --clones,
// 2026-09-28), each read against the disassembly; the world-map copy's group
// is area 87's (area_w2b_fuzz.cpp) over area 121's tables.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3b.h"
#include "game/area_w3b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w3b {
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
#define AH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define W3B_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W3B_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone answering in al.
#define W3B_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}
#define W3B_AP(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}

constexpr U kActiveMember = 0x9035A4;     // Field_ActiveMember
constexpr U kScriptObject = 0x929E80;     // MoveScript_Object
constexpr U kEffects = 0x7E11E0;          // Effect_Objects: twenty records of 0x80
constexpr U kEffect255 = kEffects + 0xFF * 0x80;   // "record 255" (0x7E9160): Effect_FindFree's none unchecked
constexpr U kExtraE1 = 0x802000 + 0xE1 * 0xA4;     // "extra record 0xE1" (0x80B0A4): Sprite_ObjectAt's none unchecked
constexpr U kActorStates = 0x903A80;      // Field_ActorStates
constexpr U kInput = 0x7E1BE8;            // Input_Held, Input_Pressed
constexpr U kDrawPass = 0x7E0918;         // Draw_PassFlags
constexpr U kPacketCell = 0x7E0670;       // Gfx_PacketNext
constexpr U kVertices = 0x9037A0;         // Prim_VertexScratch
constexpr U kGameMode = 0x66C7E8;
constexpr U kCondFE = 0x905E20;           // Cond_ByteFE
constexpr U kGteMatrix = 0x7DE4A0;        // Gte_Matrix: the GTE's current rotation and translation
constexpr U kCameraMatrix = 0x905E40;     // Camera_Matrix

// A MATRIX of identity rotation (0x1000 on the diagonal) and translation t.
void SetIdentity(U at, std::int32_t tx, std::int32_t ty, std::int32_t tz) {
    unsigned char* const m = Mem(at);
    std::memset(m, 0, 0x20);
    SetWord(m + 0, 0x1000);
    SetWord(m + 8, 0x1000);
    SetWord(m + 0x10, 0x1000);
    SetLong(m + 0x14, tx);
    SetLong(m + 0x18, ty);
    SetLong(m + 0x1C, tz);
}

const ah::Callee kSet40 = {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kClear40 = {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kFindFree = {"Effect_FindFree", bof3::addr::Effect_FindFree, KeyOf(&::Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x13};

// A party record or one of the first four field objects.
unsigned char* SomeRecord(U h) { return (h & 1) ? ah::PartyOf(static_cast<unsigned char>(h >> 1)) : ah::TaskAt(h >> 1); }

// ===========================================================================
// The fuzz's own memory: a packet buffer (as area_w2b_fuzz.cpp's)
// ===========================================================================

constexpr unsigned kPacketBytes = 0x200;
alignas(16) unsigned char g_packets[kPacketBytes];

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

U CommitEffect(const U* a, U answer) { Advance(a[1] & 0xFF); return answer; }
U LinkEffect(const U* a, U answer) {
    if (ah::Noise() % 5) Advance(a[3] & 0xFF);
    return answer;
}
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

#define W3B_DRAW_CALLEES                                                                                                           \
    {"Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, KeyOf(&::Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect, nullptr}, \
    {"Gpu_SetPolyFT4", bof3::addr::Gpu_SetPolyFT4, KeyOf(&::Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr}, \
    {"Gpu_SetPolyG4", bof3::addr::Gpu_SetPolyG4, KeyOf(&::Gpu_SetPolyG4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr}, \
    {"Gpu_SetShadeTex", bof3::addr::Gpu_SetShadeTex, KeyOf(&::Gpu_SetShadeTex), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ShadeEffect, nullptr}, \
    {"Gpu_SetSemiTrans", bof3::addr::Gpu_SetSemiTrans, KeyOf(&::Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect, nullptr}, \
    {"Gpu_SetSprt", bof3::addr::Gpu_SetSprt, KeyOf(&::Gpu_SetSprt), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SprtEffect, nullptr}, \
    {"Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, KeyOf(&::Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect, nullptr}, \
    {"MapView_LinkPrimAt", bof3::addr::MapView_LinkPrimAt, KeyOf(&::MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect, nullptr}

// ===========================================================================
// Area 120: three choices
// ===========================================================================

const ah::Clone kClones120[] = {
    W3B_P(Area120_ChoiceVar3A, 0x41A9D0, 0x1C, S::kChoice),
    W3B_P(Area120_ChoiceVar3B, 0x41A9F0, 0x1C, S::kChoice),
    W3B_P(Area120_ChoiceStartRun13, 0x41AA10, 0x37, S::kChoice),
};
const ah::Region kRegions120[] = {{at::kRowPointer, 4}};

void Seed120(unsigned) {
    ah::SetPointer(at::kRowPointer, SomeRecord(ah::Next()));
    Mem(at::kChoice)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2, 0x80, 0xFF) : ah::Next());
    if (ah::Half()) SetWord(ah::Pointer(at::kRowPointer) + 0x8A, AH_PICK(0, 0xFFFF, 0x7FFF, 1));
}
void Disturb120(U h) {
    if ((h >> 16) % 2 == 0) ah::SetPointer(at::kRowPointer, SomeRecord(h >> 20));
}

// ===========================================================================
// Area 121: its three handlers and handler 0's two states
// ===========================================================================

constexpr ah::CallSite kCalls41AAD0[] = {{0x34, 0x587740}, {0x3C, 0x57C7C0}};
constexpr ah::CallSite kCalls41AB40[] = {{0x1, 0x589810}};
const ah::Clone kClones121h[] = {
    W3B_P(Area121_CameraRun, 0x41AA50, 0x12, S::kHandler),
    W3B_P(Area121_CameraFar, 0x41AA70, 0x1E, S::kState),
    W3B_P(Area121_CameraApproach, 0x41AA90, 0x40, S::kState),
    W3B_C(Area121_StartRun4, 0x41AAD0, 0x68, kCalls41AAD0, S::kHandler),
    W3B_C(Area121_SpawnEffectBA, 0x41AB40, 0x28, kCalls41AB40, S::kHandler),
};
enum : unsigned { kHCameraRun, kHCameraFar, kHCameraApproach, kHStartRun4, kHSpawn };
const ah::Callee kCallees121h[] = {kSet40, kFindFree};
const ah::DataTable kTables121h[] = {{at::kA121CameraStates, 2}};
const ah::Region kRegions121h[] = {{kScriptObject, 4}, {kActiveMember, 4}, {kEffects, 20 * 0x80}};

void Seed121h(unsigned k) {
    ah::SetPointer(kScriptObject, SomeRecord(ah::Next()));
    ah::SetPointer(kActiveMember, SomeRecord(ah::Next()));
    unsigned char* const o = Sprite_Current;
    switch (k) {
    case kHCameraRun: o[4] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kHCameraApproach:
        // the ramp's end 0x5DC (s16) and one step short of it, the sign's edges
        Camera_Distance = static_cast<short>(ah::Often() ? AH_PICK(0x5DC, 0x5DB, 0x59C, 0x59B, 0x5DD, 0xFBDC, 0x7FFF, 0x8000, 0) : ah::Next());
        break;
    case kHStartRun4:
        if (ah::Half()) Field_ActiveMember[0x80] = static_cast<unsigned char>(ah::Half() ? 0xFF : 1);
        break;
    default: break;
    }
}
void Disturb121h(U h) {
    switch ((h >> 16) % 3) {
    case 0: ah::SetPointer(kScriptObject, SomeRecord(h >> 20)); break;
    case 1: ah::SetPointer(kActiveMember, SomeRecord(h >> 20)); break;
    default: break;
    }
}

// ===========================================================================
// Area 121: the world-map copy (area 87's group, area_w2b_fuzz.cpp)
// ===========================================================================

constexpr ah::CallSite kCalls41AB70[] = {{0x18, 0x57C7A0}, {0x33, 0x57C7C0}, {0x58, 0x4976D0}};
constexpr ah::CallSite kCalls41ABF0[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls41ACD0[] = {{0xE, 0x589590}};
constexpr ah::CallSite kCalls41AD30[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls41AE80[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls41AED0[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls41AF30[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls41AFA0[] = {{0x0, 0x41AFB0}, {0x5, 0x41B080}};
constexpr ah::CallSite kCalls41AFD0[] = {{0x1C, 0x41B000}};
constexpr ah::CallSite kCalls41B000[] = {{0x1F, 0x41B1E0}};
constexpr ah::CallSite kCalls41B030[] = {{0x38, 0x41B1E0}};
constexpr ah::CallSite kCalls41B0A0[] = {{0x59, 0x41B470}};
constexpr ah::CallSite kCalls41B110[] = {{0x65, 0x41B470}};
constexpr ah::CallSite kCalls41B180[] = {{0x4E, 0x41B470}};
constexpr ah::CallSite kCalls41B1E0[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x41B3B0}, {0x51, 0x536700}, {0x7B, 0x41B3B0}, {0xDC, 0x41B3B0}, {0xF8, 0x41B3B0}, {0x15B, 0x41B3B0}, {0x173, 0x531920}, {0x1A8, 0x41B3B0}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls41B3B0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls41B470[] = {{0x17, 0x41B3B0}, {0x26, 0x41B3B0}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls41B4F0[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls41B670[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls41B730[] = {{0xD1, 0x5A75D0}, {0xD9, 0x5A77A0}, {0x17C, 0x5A85F0}, {0x182, 0x5A9290}, {0x194, 0x572A00}, {0x1A0, 0x461E50}, {0x1AC, 0x5A75D0}, {0x1B4, 0x5A77A0}, {0x261, 0x5A85F0}, {0x267, 0x5A9290}, {0x27C, 0x572A00}, {0x285, 0x461E50}};

const ah::Clone kClonesWm[] = {
    W3B_C(Area121_PlaceMessage, 0x41AB70, 0x74, kCalls41AB70, S::kState),
    W3B_C(Area121_PlateRun, 0x41ABF0, 0xD6, kCalls41ABF0, S::kState),
    W3B_C(Area121_PlateStart, 0x41ACD0, 0x51, kCalls41ACD0, S::kState),
    W3B_C(Area121_PlateShow, 0x41AD30, 0x142, kCalls41AD30, S::kState),
    W3B_C(Area121_PlateGrow, 0x41AE80, 0x41, kCalls41AE80, S::kState),
    W3B_C(Area121_PlateHold, 0x41AED0, 0x58, kCalls41AED0, S::kState),
    W3B_C(Area121_PlateShrink, 0x41AF30, 0x50, kCalls41AF30, S::kState),
    W3B_P(Area121_HudRun, 0x41AF80, 0x12, S::kState),
    W3B_C(Area121_HudFrame, 0x41AFA0, 0xA, kCalls41AFA0, S::kState),
    W3B_P(Area121_FrameStep, 0x41AFB0, 0x12, S::kCallee),
    W3B_C(Area121_FrameSlideIn, 0x41AFD0, 0x21, kCalls41AFD0, S::kState),
    W3B_C(Area121_FrameHold, 0x41B000, 0x28, kCalls41B000, S::kState),
    W3B_C(Area121_FrameSlideOut, 0x41B030, 0x41, kCalls41B030, S::kState),
    W3B_P(Area121_BoxStep, 0x41B080, 0x12, S::kCallee),
    W3B_C(Area121_BoxSlideIn, 0x41B0A0, 0x62, kCalls41B0A0, S::kState),
    W3B_C(Area121_BoxHold, 0x41B110, 0x6E, kCalls41B110, S::kState),
    W3B_C(Area121_BoxSlideOut, 0x41B180, 0x57, kCalls41B180, S::kState),
    W3B_C(Area121_DrawFrame, 0x41B1E0, 0x1C5, kCalls41B1E0, S::kCallee),
    W3B_C(Area121_DrawSprite, 0x41B3B0, 0xBC, kCalls41B3B0, S::kCallee),
    W3B_C(Area121_DrawHud, 0x41B470, 0x58, kCalls41B470, S::kCallee),
    W3B_P(Area121_Record8Run, 0x41B4D0, 0x12, S::kState),
    W3B_C(Area121_Record8Place, 0x41B4F0, 0x154, kCalls41B4F0, S::kState),
    W3B_P(Area121_Record4Run, 0x41B650, 0x12, S::kState),
    W3B_C(Area121_Record4MarkCell, 0x41B670, 0xB5, kCalls41B670, S::kState),
    W3B_C(Area121_DrawDrift, 0x41B730, 0x295, kCalls41B730, S::kState),
};
enum : unsigned {
    kPlaceMessage, kPlateRun, kPlateStart, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame,
    kFrameStep, kFrameSlideIn, kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame,
    kDrawSprite, kDrawHud, kRecord8Run, kRecord8Place, kRecord4Run, kRecord4MarkCell, kDrawDrift,
};
static_assert(kDrawDrift + 1 == AH_COUNT(kClonesWm), "the copy's seeding indices");

U ByteAtEffect(const U*, U answer) {
    static const U kCells[] = {0xA1, 0xA1, 0xA0, 0xAE, 0xA2, 0x9F, 0, 0x21};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 5 == 0 ? (n >> 8) & 0xFF : kCells[(n >> 4) % 8]);
}

const ah::Callee kPers4 = {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, KeyOf(&::Gte_RotTransPers4), 10,
                           {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, ah::Answer::kGarbage, 0, 0,
                           {8, 8, 8, 8}, &PersEffect, nullptr};

const ah::Callee kCalleesWm[] = {
    {"Area121_FrameStep", at::kWm121.fn_frame_step, at::kWm121.fn_frame_step, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area121_BoxStep", at::kWm121.fn_box_step, at::kWm121.fn_box_step, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area121_DrawFrame", at::kWm121.fn_draw_frame, at::kWm121.fn_draw_frame, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},
    {"Area121_DrawSprite", at::kWm121.fn_draw_sprite, at::kWm121.fn_draw_sprite, 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"Area121_DrawHud", at::kWm121.fn_draw_hud, at::kWm121.fn_draw_hud, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},
    kSet40, kClear40,
    {"WorldMap_PinSprite", bof3::addr::WorldMap_PinSprite, KeyOf(&::WorldMap_PinSprite), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Effect_Release", bof3::addr::Effect_Release, KeyOf(&::Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"WorldMap_DrawNeedle", bof3::addr::WorldMap_DrawNeedle, KeyOf(&::WorldMap_DrawNeedle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"WorldMap_RecordIndex", bof3::addr::WorldMap_RecordIndex, KeyOf(&::WorldMap_RecordIndex), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Prim_SetTexture", bof3::addr::Prim_SetTexture, KeyOf(&::Prim_SetTexture), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &TextureEffect, nullptr},
    {"AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, KeyOf(&::AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect, nullptr},
    {"Field_CellHasEvent", bof3::addr::Field_CellHasEvent, KeyOf(&::Field_CellHasEvent), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    {"Gte_PrimDepths4_10", bof3::addr::Gte_PrimDepths4_10, KeyOf(&::Gte_PrimDepths4_10), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DepthEffect, nullptr},
    kPers4,
    W3B_DRAW_CALLEES,
};

const ah::DataTable kTablesWm[] = {
    {at::kWm121.plate_states, 5}, {at::kWm121.hud_states, 2}, {at::kWm121.frame_states, 4},
    {at::kWm121.box_states, 4},   {at::kWm121.record8_states, 3}, {at::kWm121.record4_states, 2},
};

// The fuzz's own literal table addresses (read off the exe, docs/area_w3b.md
// section 4): the seed plants and the regions are built from these, never
// from the kWm121 ours reads, so a wrong constant in ours shows.
constexpr U kFzPlaces = 0x624624, kFzPlacesEnd = 0x62462C;
constexpr U kFzPlateAnims = 0x623690, kFzPlateAnimsEnd = 0x6236A0;
constexpr U kFzCells = 0x6236A0;
constexpr U kFzDirections = 0x6246E4;     // with the record-8 animations after them: 0x18 bytes
constexpr U kFzButtons = 0x6246C0;

enum : unsigned { kRegWmPackets = 3 };
ah::Region g_regionsWm[] = {
    {at::kMapMode, 1}, {kDrawPass, 1}, {kPacketCell, 4}, {0, kPacketBytes}, {kVertices, 0x20}, {kGameMode, 2},
    {at::kButtonMap0, 0x10}, {at::kAreaText, 4}, {at::kFlag3A79, 1},
    {kFzPlaces, kFzPlacesEnd - kFzPlaces}, {kFzPlateAnims, kFzPlateAnimsEnd - kFzPlateAnims}, {kFzCells, 8},
    {kFzDirections, 0x18},
};

// The exe's own bytes of the tables the fuzz randomises, put back two rounds
// in three (so the seeds see the shipped values most of the time).
struct Saved {
    unsigned char places[8], anims[0x10], cells[8], dirs[0x18];
};
Saved g_saved;

void SaveTables() {
    std::memcpy(g_saved.places, Mem(kFzPlaces), sizeof g_saved.places);
    std::memcpy(g_saved.anims, Mem(kFzPlateAnims), sizeof g_saved.anims);
    std::memcpy(g_saved.cells, Mem(kFzCells), sizeof g_saved.cells);
    std::memcpy(g_saved.dirs, Mem(kFzDirections), sizeof g_saved.dirs);
}

void DisturbWm(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 10) {
    case 0: Mem(at::kMapMode)[0] = static_cast<unsigned char>(v % 4 == 0 ? v : v % 3); break;
    case 1: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ (v & 1 ? 4 : 0x1B)); break;
    case 2: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (v & 1 ? 0x100 : 0x4000)); break;
    case 3: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000); break;
    case 4: Mem(at::kMsgState)[0] = static_cast<unsigned char>(v % 3); break;
    case 5: Game_Mode = static_cast<unsigned short>(Game_Mode ^ 1); break;
    case 6: SetWord(Mem(at::kPlace), Word(Mem(kFzPlaces + (v % 4) * 2))); break;
    case 7: Mem(at::kLeaderCellX + (v & 1) * 2)[0] = static_cast<unsigned char>(v); break;
    case 8: Mem(at::kPartySet)[0] = static_cast<unsigned char>(v & 1 ? 0xC : v); break;
    default: break;
    }
}
void SettleWm() {
    unsigned char* const o = Sprite_Current;
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    // the plate's unbounded search: the place in the last animation entry
    SetWord(Mem(kFzPlateAnimsEnd - 4), Word(Mem(at::kPlace)));
}

void SeedWm(unsigned k) {
    unsigned char* const o = Sprite_Current;
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(kFzPlaces), g_saved.places, sizeof g_saved.places);
    if (ah::Often()) std::memcpy(Mem(kFzPlateAnims), g_saved.anims, sizeof g_saved.anims);
    if (ah::Often()) std::memcpy(Mem(kFzCells), g_saved.cells, sizeof g_saved.cells);
    if (ah::Often()) std::memcpy(Mem(kFzDirections), g_saved.dirs, sizeof g_saved.dirs);
    o[1] = static_cast<unsigned char>(o[1] % 5);
    // the place one of the four (or of the plate animations'), or none
    if (ah::Often()) SetWord(Mem(at::kPlace), Word(Mem((ah::Half() ? kFzPlaces : kFzPlateAnims) + (ah::Next() % 4) * (ah::Half() ? 2 : 4))));
    SetWord(Mem(kFzPlateAnimsEnd - 4), Word(Mem(at::kPlace)));
    const auto leave = [o] {
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2) : ah::Next());
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 2, 5, 1, 3));
        Field_ScriptFlags = static_cast<unsigned short>(ah::Often() ? Field_ScriptFlags & ~0x100u : Field_ScriptFlags | 0x100u);
    };
    switch (k) {
    case kPlaceMessage:
        Mem(at::kMsgState)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 5, 3));
        // the place one of the four, one of them with a high byte, or none
        if (ah::Often()) SetWord(Mem(at::kPlace), Word(Mem(kFzPlaces + (ah::Next() % 4) * 2)) ^ (ah::Half() ? 0 : AH_PICK(0x100, 1, 0x8000)));
        break;
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) Mem(at::kLeaderSteps)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
        SetWord(Mem(kFzPlateAnims + (ah::Next() % 4) * 4), Word(Mem(at::kPlace)));
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
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Next() % 2);
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

// ===========================================================================
// Area 121: leader state 12
// ===========================================================================

constexpr ah::CallSite kCalls41B9D0[] = {{0x12, 0x4156C0}};
constexpr ah::CallSite kCalls41B9F0[] = {{0x3, 0x41BEC0}, {0x77, 0x531950}, {0x84, 0x41C0A0}, {0x91, 0x530920}, {0x9E, 0x41C0E0}, {0xB4, 0x41C110}, {0xEE, 0x415640}, {0x17F, 0x52E160}, {0x1D5, 0x531DF0}, {0x1F5, 0x41BC60}, {0x1FE, 0x415680}, {0x239, 0x5345E0}, {0x23E, 0x535F50}, {0x24B, 0x52E140}};
constexpr ah::CallSite kCalls41BC60[] = {{0x41, 0x531CF0}, {0xA4, 0x41BDA0}, {0xFB, 0x41BDA0}, {0x107, 0x5345E0}, {0x114, 0x52E140}};
constexpr ah::CallSite kCalls41BDA0[] = {{0x16, 0x41BE10}, {0x29, 0x52E160}, {0x36, 0x531CF0}};
constexpr ah::CallSite kCalls41BEC0[] = {{0x1A, 0x531CF0}, {0x35, 0x41BDA0}, {0x41, 0x5345E0}, {0x4E, 0x52E140}};
constexpr ah::CallSite kCalls41BF40[] = {{0x23, 0x52E140}, {0x30, 0x5350C0}, {0x35, 0x534A00}, {0x52, 0x536700}, {0x79, 0x594E00}, {0x9F, 0x536700}, {0xBA, 0x5951D0}, {0xE0, 0x415640}, {0xF2, 0x56D750}, {0x108, 0x534F10}, {0x13A, 0x41B9F0}};
constexpr ah::CallSite kCalls41C0A0[] = {{0x1D, 0x587740}};

const ah::Clone kClonesLeader[] = {
    W3B_C(Area121_LeaderRun, 0x41B9D0, 0x17, kCalls41B9D0, S::kState),
    W3B_C(Area121_LeaderControl, 0x41B9F0, 0x26B, kCalls41B9F0, S::kState),
    W3B_C(Area121_PushObject, 0x41BC60, 0x133, kCalls41BC60, S::kCallee),
    W3B_A(Area121_StepAround, 0x41BDA0, 0x69, kCalls41BDA0, S::kCallee),
    W3B_AP(Area121_DirectionTo, 0x41BE10, 0xA2, S::kCallee),
    W3B_A(Area121_StepOffObject, 0x41BEC0, 0x71, kCalls41BEC0, S::kCallee),
    W3B_C(Area121_LeaderStep, 0x41BF40, 0x157, kCalls41BF40, S::kState),
    W3B_A(Area121_MenuButton, 0x41C0A0, 0x3A, kCalls41C0A0, S::kCallee),
    W3B_AP(Area121_Request4Button, 0x41C0E0, 0x30, S::kCallee),
    W3B_AP(Area121_TurnInput, 0x41C110, 0x72, S::kCallee),
};
enum : unsigned { kLRun, kLControl, kLPush, kLStepAround, kLDirection, kLStepOff, kLStep, kLMenu, kLRequest4, kLTurn };
static_assert(kLTurn + 1 == AH_COUNT(kClonesLeader), "the leader's seeding indices");

// The leader's early-out tests: the seed picks which one answers "yes" this
// round (g_exit, one of five, or none about half the time), so that about
// half the rounds reach the walk; "no" is a low byte of 0 with garbage above
// it now and then (the tests read al).
enum : unsigned { kExitStepOff, kExitCellEvent, kExitMenu, kExitTalk, kExitRequest4, kExitNone };
unsigned g_exit = kExitNone;
U Answer(unsigned which, U answer) {
    const U n = ah::Noise();
    if (g_exit == which) return (answer & 0xFFFFFF00u) | ((n >> 8) % 0xFF + 1);
    return n % 4 == 0 ? (answer & 0xFFFFFF00u) : 0u;
}
U StepOffAnswer(const U*, U answer) { return Answer(kExitStepOff, answer); }
U CellEventAnswer(const U*, U answer) { return Answer(kExitCellEvent, answer); }
U MenuAnswer(const U*, U answer) { return Answer(kExitMenu, answer); }
U TalkAnswer(const U*, U answer) { return Answer(kExitTalk, answer); }
U Request4Answer(const U*, U answer) { return Answer(kExitRequest4, answer); }
// Field_LeaderStepTarget: 0 (the walk), 1 or 0xFF (blocked), or another code;
// in the control state's rounds (g_walk) 0 two times in three, so that the
// push and the step behind it are reached.
bool g_walk = false;
U StepTargetAnswer(const U*, U answer) {
    static const U kT[] = {0, 0, 0, 1, 0xFF, 2, 3, 5, 0xFE};
    const U n = ah::Noise();
    if (g_walk && n % 3 != 0) return (n >> 8) % 4 == 0 ? (answer & 0xFFFFFF00u) : 0u;
    return (answer & 0xFFFFFF00u) | kT[(n >> 4) % 9];
}
// Field_LeaderPushObjects: an object to push half the time.
U PushAnswer(const U*, U answer) {
    const U n = ah::Noise();
    return n % 2 == 0 ? (n % 4 == 0 ? (answer & 0xFFFFFF00u) : 0u) : ((answer & 0xFFFFFF00u) | ((n >> 8) % 0xFF + 1));
}
// Sprite_ObjectAt: none (0xFF) often, else an object 0..0x21 (the extra four
// from 0x1E), garbage above.
U ObjectAtAnswer(const U*, U answer) {
    static const U kK[] = {0xFF, 0xFF, 0, 1, 3, 0x1D, 0x1E, 0x1F, 0x21};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | kK[n % 9];
}
// Scenario_ArriveHook: all of eax - 0, a low byte of 0 above a set bit, or any.
U ArriveAnswer(const U*, U answer) {
    const U n = ah::Noise() % 4;
    return n < 2 ? 0u : n == 2 ? 0x100u : (answer | 1u);
}
// AreaMap_ByteAt under the leader: the change-of-area cell 0xAF, the link cell
// 0xC0, their neighbours, or any.
U LeaderCellAnswer(const U*, U answer) {
    static const U kC[] = {0xAF, 0xAF, 0xC0, 0xC0, 0xAE, 0xB0, 0xBF, 0xC1, 0};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 6 == 0 ? (n >> 8) & 0xFF : kC[(n >> 4) % 9]);
}
// Area121_TurnInput turns Sprite_Current +8, which the leader reads after the
// call: the stand-in turns it by 0, +-1, +-2, +-3, 4 or to anything, and
// answers "turned" or not.
U TurnEffect(const U*, U answer) {
    static const U kD[] = {0, 1, 0xFF, 2, 0xFE, 3, 0xFD, 4, 5, 0xFA, 0xF6, 6, 10};
    const U n = ah::Noise();
    unsigned char* const o = Sprite_Current;
    static const U kSigned[] = {0x7F, 0x80, 0x81, 0xFE, 0x02, 0x7C, 0x83};
    if (n % 3 != 0) o[8] = static_cast<unsigned char>(n % 11 == 0 ? (n >> 8) : n % 7 == 0 ? kSigned[(n >> 8) % 7] : o[8] + kD[(n >> 4) % 13]);
    return (answer & 0xFFFFFF00u) | ((n >> 12) % 3 == 0 ? 0u : 1u);
}
// 0x415680 leaves the pace in Field_State +0x128 (3 or 4), which the leader
// reads after the call.
U PaceEffect(const U*, U answer) {
    Field_State[0x128] = static_cast<unsigned char>(ah::Noise() % 2 == 0 ? 3 : 4);
    return answer;
}

const ah::Callee kCalleesLeader[] = {
    {"0x4156C0", kLeaderCharge104, kLeaderCharge104, 0, {}, ah::Answer::kGarbage, 0, 0},
    {"0x415640", kLeaderHalt104, kLeaderHalt104, 0, {}, ah::Answer::kGarbage, 0, 0},
    {"0x415680", kLeaderPace104, kLeaderPace104, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &PaceEffect, nullptr},
    {"Area121_LeaderControl", kLeaderControl, kLeaderControl, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area121_PushObject", kPushObject, kPushObject, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area121_StepAround", kStepAround, kStepAround, 3, {kAll, kAll, kU8}, ah::Answer::kFlag, 0, 0},
    {"Area121_DirectionTo", kDirectionTo, kDirectionTo, 3, {kAll, kAll, kU8}, ah::Answer::kByte, 0, 7},
    {"Area121_StepOffObject", kStepOffObject, kStepOffObject, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &StepOffAnswer, nullptr},
    {"Area121_MenuButton", kMenuButton, kMenuButton, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MenuAnswer, nullptr},
    {"Area121_Request4Button", kRequest4Button, kRequest4Button, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Request4Answer, nullptr},
    {"Area121_TurnInput", kTurnInput, kTurnInput, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &TurnEffect, nullptr},
    {"Field_LeaderCellEvent", bof3::addr::Field_LeaderCellEvent, KeyOf(&::Field_LeaderCellEvent), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &CellEventAnswer, nullptr},
    {"Field_LeaderTalkTest", bof3::addr::Field_LeaderTalkTest, KeyOf(&::Field_LeaderTalkTest), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &TalkAnswer, nullptr},
    {"Field_LeaderStepTarget", bof3::addr::Field_LeaderStepTarget, KeyOf(&::Field_LeaderStepTarget), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &StepTargetAnswer, nullptr},
    {"Sprite_ObjectAt", bof3::addr::Sprite_ObjectAt, KeyOf(&::Sprite_ObjectAt), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ObjectAtAnswer, nullptr},
    {"Scenario_ArriveHook", bof3::addr::Scenario_ArriveHook, KeyOf(&::Scenario_ArriveHook), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ArriveAnswer, nullptr},
    {"AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, KeyOf(&::AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &LeaderCellAnswer, nullptr},
    // the area a word, the flags a byte (the pushes carry stale bits above them)
    {"Field_ChangeArea", bof3::addr::Field_ChangeArea, KeyOf(&::Field_ChangeArea), 4, {kU16, kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"Field_LeaderPushObjects", bof3::addr::Field_LeaderPushObjects, KeyOf(&::Field_LeaderPushObjects), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &PushAnswer, nullptr},
    {"Area_LinkAt", bof3::addr::Area_LinkAt, KeyOf(&::Area_LinkAt), 2, {kU8, kU8}, ah::Answer::kFlag, 0, 0},
};
const ah::DataTable kTablesLeader[] = {{at::kLeaderStates, 2}};
const ah::Region kRegionsLeader[] = {
    {kActorStates, 4 * 0xA4}, {kExtraE1, 0xA4}, {at::kButtonMap0, 0x10}, {kInput, 8}, {at::k904EE0, 1}, {at::k937F98, 1},
};

// The object planted near the point a round asks about, and the point.
U g_x, g_z, g_k;

// An object record 0..0x21 (0xFF: the extra record 0xE1).
unsigned char* Record(U k) {
    k &= 0xFF;
    return k < 0x1E ? Sprite_Objects + k * 0xA4 : Sprite_ObjectsExtra + (k - 0x1E) * 0xA4;
}

void SeedLeader(unsigned k) {
    unsigned char* const o = Sprite_Current;
    Game_AreaNumber = static_cast<unsigned short>(ah::Often() ? 0x79 : AH_PICK(0x78, 0x7A, 0x179, 0x68, 0x79));
    g_exit = ah::Half() ? kExitNone : ah::Next() % (kExitNone + 1);
    g_walk = k == kLControl;
    // the facing a direction (the table has eight), now and then a whole byte
    o[8] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Half() ? AH_PICK(0x7F, 0x80, 0x81, 0xF9, 0xFE, 0xFF, 0x7D) : ah::Next());
    // the push button and the held keys, sharing a bit half the time
    const U button = ah::Often() ? (1u << (ah::Next() % 16)) : ah::Next() & 0xFFFF;
    SetWord(Mem(at::kButtonMap1), button);
    SetWord(Mem(kInput), ah::Half() ? (button | (ah::Next() & ah::Next())) : (ah::Next() & ~button & 0xFFFF));
    if (ah::Half()) SetWord(Mem(kInput), 0);
    switch (k) {
    case kLRun: o[2] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kLControl: {
        // the four state tests: one of them stops the round now and then
        const unsigned stop = ah::Next() % 8;
        Field_ScriptFlags = static_cast<unsigned short>(stop == 0 ? Field_ScriptFlags | 0x100u : Field_ScriptFlags & ~0x100u);
        Field_ScriptFlags2 = static_cast<unsigned short>(stop == 1 ? Field_ScriptFlags2 | 0x40u : Field_ScriptFlags2 & ~0x40u);
        Field_Request = static_cast<unsigned char>(stop == 2 ? AH_PICK(1, 2, 0x80, 0xFF) : 0);
        o[0xA] = static_cast<unsigned char>(stop == 3 ? AH_PICK(1, 0x80, 0xFF) : 0);
        Field_InputHeld = static_cast<unsigned short>(ah::Half() ? 0 : ah::Often() ? AH_PICK(1, 1, 2, 0x8000, 0xFFFF) : ah::Next() | 1);
        Field_State[0x148] = static_cast<unsigned char>(ah::Next() % 4);
        for (unsigned r = 0; r < 4; ++r)
            Mem(kActorStates + r * 0xA4)[0] = static_cast<unsigned char>(ah::Half() ? 0x20 | ah::Next() : ah::Next() & ~0x20u);
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0x40, 0x3F, 0x41, 0, 0xC0) : ah::Next());
        break;
    }
    case kLPush:
        // every object's facing the leader's half the time, as the push wants
        if (ah::Half()) {
            for (unsigned r = 0; r < 0x22; ++r) Record(r)[8] = static_cast<unsigned char>((o[8] & 7) | (ah::Next() & 0xF8));
            Mem(kExtraE1)[8] = static_cast<unsigned char>((o[8] & 7) | (ah::Next() & 0xF8));
        }
        break;
    case kLStepAround:
    case kLDirection: {
        // (x, z) about object k: each axis below, at or above its position
        g_k = ah::Often() ? AH_PICK(0, 1, 5, 0x1D, 0x1E, 0x21, 0xFF) : ah::Next() % 0x22;
        if (ah::Half()) g_k |= ah::Next() & 0xFFFFFF00u;
        const unsigned char* const r = Record(g_k == 0xFF ? 0 : g_k);
        const auto around = [](U v) { return v + AH_PICK(0, 0, 1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u, 0x80000000u); };
        g_x = ah::Often() ? around(static_cast<U>(Long(r + 0x34))) : ah::Next();
        g_z = ah::Often() ? around(static_cast<U>(Long(r + 0x38))) : ah::Next();
        break;
    }
    case kLStep:
        o[9] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2, 0xFF) : ah::Next());
        Field_InputFlags = static_cast<unsigned char>(ah::Next() | (ah::Often() ? 1 : 0));
        if (ah::Often()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x100u);
        if (ah::Often()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x40u);
        SetWord(Mem(at::kButtonMap0), ah::Half() ? 1u << (ah::Next() % 16) : 0u);
        Field_InputHeld = static_cast<unsigned short>(ah::Often() ? (1u << (ah::Next() % 16)) : 0u);
        if (ah::Half()) o[8] = static_cast<unsigned char>(AH_PICK(2, 6, 1, 3, 5, 7, 0x82));
        break;
    case kLMenu:
        Field_MenuButton = static_cast<unsigned short>(1u << (ah::Next() % 16));
        Input_Pressed = static_cast<unsigned short>(ah::Half() ? Field_MenuButton | (ah::Next() & 0xFFFF) : ah::Next() & ~Field_MenuButton & 0xFFFF);
        Field_ScriptFlags = static_cast<unsigned short>(ah::Half() ? Field_ScriptFlags & ~0x40u : Field_ScriptFlags | 0x40u);
        break;
    case kLRequest4:
        SetWord(Mem(at::kButtonMap3), 1u << (ah::Next() % 16));
        Field_InputHeld = static_cast<unsigned short>(ah::Half() ? Word(Mem(at::kButtonMap3)) | (ah::Next() & 0xFFFF) : ah::Next() & ~Word(Mem(at::kButtonMap3)) & 0xFFFF);
        Field_ScriptFlags = static_cast<unsigned short>(ah::Half() ? Field_ScriptFlags & ~0x2000u : Field_ScriptFlags | 0x2000u);
        break;
    case kLTurn: {
        // the high byte's four bits, alone, in pairs, or none
        static const U kH[] = {0x4000, 0x2000, 0x8000, 0x1000, 0x6000, 0xA000, 0x3000, 0x9000, 0, 0xF000, 0x0800, 0x00FF};
        Input_Held = static_cast<unsigned short>(ah::Often() ? kH[ah::Next() % 12] | (ah::Next() & 0xFF) : ah::Next());
        break;
    }
    default: break;
    }
}
void ArgsLeader(unsigned k, U* a) {
    if (k == kLStepAround || k == kLDirection) {
        a[0] = g_x;
        a[1] = g_z;
        a[2] = g_k;
    }
}
// The leader's cells the machine reads again after its calls: the facing,
// the push button's word, the pace, Game_AreaNumber.
void DisturbLeader(U h) {
    switch ((h >> 16) % 5) {
    case 0: Sprite_Current[8] = static_cast<unsigned char>(h >> 8); break;
    case 1: SetWord(Mem(at::kButtonMap1), (h >> 8) & 0xFFFF); break;
    case 2: Field_State[0x128] = static_cast<unsigned char>((h >> 8) & 1 ? 4 : 3); break;
    case 3: Field_InputHeld = static_cast<unsigned short>(h >> 8); break;
    default: break;
    }
}

// ===========================================================================
// Area 121: effect kind 0x5C, the top band, the object trigger
// ===========================================================================

constexpr ah::CallSite kCalls41C1B0[] = {{0x26, 0x57BA60}, {0x53, 0x41C270}, {0x64, 0x5918E0}, {0x73, 0x589810}};
constexpr ah::CallSite kCalls41C270[] = {{0x2D, 0x57B830}, {0x68, 0x41C350}, {0x71, 0x415940}, {0x83, 0x415940}, {0xA2, 0x41C350}, {0xAE, 0x415940}, {0xC0, 0x41C350}, {0xC9, 0x415940}, {0xD2, 0x415940}};
constexpr ah::CallSite kCalls41C350[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x27, 0x5A7710}, {0x7D, 0x461E50}};
constexpr ah::CallSite kCalls41C3E0[] = {{0x14, 0x41C440}, {0x33, 0x41C440}, {0x4E, 0x41C270}};
constexpr ah::CallSite kCalls41C440[] = {{0xB5, 0x41C510}};
constexpr ah::CallSite kCalls41C510[] = {{0x3E, 0x41C3E0}, {0x43, 0x41C270}};
constexpr ah::CallSite kCalls41C560[] = {{0x37, 0x41C5B0}};
constexpr ah::CallSite kCalls41C5B0[] = {{0x2C, 0x5A7B90}, {0x88, 0x5A8200}, {0x97, 0x5A8060}, {0xAB, 0x5A7D70}, {0xB5, 0x5A8DE0}, {0xBF, 0x5A8E00}, {0x138, 0x5A75D0}, {0x140, 0x5A7780}, {0x19F, 0x5A85F0}, {0x1A8, 0x5A9290}, {0x1C9, 0x572FA0}, {0x1D1, 0x5A7BC0}};
constexpr ah::CallSite kCalls41C790[] = {{0x36, 0x5A77C0}, {0x3F, 0x461E50}, {0x4B, 0x5A7610}, {0x52, 0x5A7780}, {0xD1, 0x461E50}};
constexpr ah::CallSite kCalls41C870[] = {{0x0, 0x57C7C0}};

const ah::Clone kClonesFx[] = {
    W3B_P(Area121_Kind5CRun, 0x41C190, 0x12, S::kState),
    W3B_C(Area121_Kind5CStart, 0x41C1B0, 0xB1, kCalls41C1B0, S::kState),
    W3B_C(Area121_Kind5CFollow, 0x41C270, 0xDB, kCalls41C270, S::kCallee),
    W3B_C(Area121_GaugeSprite, 0x41C350, 0x87, kCalls41C350, S::kCallee),
    W3B_C(Area121_Kind5CFace, 0x41C3E0, 0x54, kCalls41C3E0, S::kState),
    W3B_A(Area121_Kind5CTurnStep, 0x41C440, 0xCB, kCalls41C440, S::kCallee),
    W3B_C(Area121_Kind5CTurn, 0x41C510, 0x48, kCalls41C510, S::kState),
    W3B_C(Area121_RingWait, 0x41C560, 0x4B, kCalls41C560, S::kState),
    W3B_C(Area121_RingRise, 0x41C5B0, 0x1DB, kCalls41C5B0, S::kState),
    W3B_C(Area121_DrawTopBand, 0x41C790, 0xDD, kCalls41C790, S::kState),
    W3B_A(Area121_Trigger38, 0x41C870, 0x1D, kCalls41C870, S::kCallee),
};
enum : unsigned { kFRun, kFStart, kFFollow, kFGauge, kFFace, kFTurnStep, kFTurn, kFRingWait, kFRingRise, kFBand, kFTrigger };
static_assert(kFTrigger + 1 == AH_COUNT(kClonesFx), "the effects' seeding indices");

// Area121_Kind5CTurnStep answers "turned" or not; mostly not, so that the
// face state reaches its second ask and its own store.
U TurnStepAnswer(const U*, U answer) {
    const U n = ah::Noise() % 3;
    return (answer & 0xFFFFFF00u) | (n == 0 ? 1u : 0u);
}
// The matrix chain through the GTE for real (the stack matrix's pointers
// differ between the passes; what it computes shows in the primitive's
// corners, which Gte_RotTransPers4 - for real too - writes into the packet).
#define W3B_THROUGH(name, n) {#name, bof3::addr::name, KeyOf(&::name), n, {}, ah::Answer::kThrough, 0, 0}

const ah::Callee kCalleesFx[] = {
    {"Area121_Kind5CFollow", kKind5CFollow, kKind5CFollow, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area121_Kind5CFace", kKind5CFace, kKind5CFace, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area121_Kind5CTurn", kKind5CTurn, kKind5CTurn, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area121_RingRise", kRingRise, kRingRise, 0, {}, ah::Answer::kPhase, 0, 0},
    {"Area121_GaugeSprite", kGaugeSprite, kGaugeSprite, 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {"Area121_Kind5CTurnStep", kKind5CTurnStep, kKind5CTurnStep, 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &TurnStepAnswer, nullptr},
    // the gauge row: both bytes (one pushed with a stale register above it)
    {"0x415940", kGaugeRow104, kGaugeRow104, 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0},
    {"KeyItem_Has", bof3::addr::KeyItem_Has, KeyOf(&::KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    kFindFree, kSet40,
    W3B_THROUGH(Gte_PushMatrix, 0), W3B_THROUGH(Gte_PopMatrix, 0), W3B_THROUGH(Gte_RotTrans, 2), W3B_THROUGH(Gte_RotMatrix, 2),
    W3B_THROUGH(Gte_MulMatrix0, 3), W3B_THROUGH(Gte_SetRotMatrix, 1), W3B_THROUGH(Gte_SetTransMatrix, 1),
    W3B_THROUGH(Gte_RotTransPers4, 10), W3B_THROUGH(Gte_PrimDepths4_10, 1),
    W3B_DRAW_CALLEES,
};
const ah::DataTable kTablesFx[] = {{at::kKind5CStates, 5}};

enum : unsigned { kRegFxPackets = 3 };
ah::Region g_regionsFx[] = {
    {kDrawPass, 1}, {kPacketCell, 4}, {kVertices, 0x20}, {0, kPacketBytes}, {kEffects, 20 * 0x80}, {kEffect255, 0x10},
    {kCondFE, 1}, {at::kBandHeights, 4}, {kGteMatrix, 0x20}, {kCameraMatrix, 0x20},
};
unsigned char g_heights[4];

void SeedFx(unsigned k) {
    unsigned char* const o = Sprite_Current;
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(at::kBandHeights), g_heights, sizeof g_heights);
    // the leader's facing and the object's within the eight, now and then not
    Mem(at::kLeaderDir)[0] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Next());
    o[8] = static_cast<unsigned char>(ah::Often() ? (ah::Half() ? Mem(at::kLeaderDir)[0] : ah::Next() % 8) : ah::Next());
    Draw_PassFlags = static_cast<unsigned char>(ah::Often() ? AH_PICK(0x1B, 4, 1, 0x10, 0xE4, 0x1F, 0) : ah::Next());
    switch (k) {
    case kFRun: o[1] = static_cast<unsigned char>(ah::Next() % 5); break;
    case kFStart:
        Game_AreaNumber = static_cast<unsigned short>(ah::Often() ? AH_PICK(0x79, 0x79, 0x68, 0x68, 0x78, 0x7A) : ah::Next() % 200);
        break;
    case kFFollow:
        Field_StatusBits = static_cast<unsigned char>(ah::Half() ? Field_StatusBits & ~0x40u : Field_StatusBits | 0x40u);
        Mem(at::kLeaderA)[0] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Half()) Mem(at::kLeaderB)[0] = static_cast<unsigned char>(AH_PICK(0, 0x40, 0x41, 0xFF, 0x20));
        break;
    case kFTurnStep:
    case kFTurn:
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(1, 1, 2, 0, 8, 0xFF));
        if (ah::Half()) SetLong(o + 0x14, AH_PICK(0x40, 0xFFFFFFC0u, 0, 0x1000));
        if (ah::Half()) SetLong(o + 0x6C, static_cast<std::int32_t>(AH_PICK(0, 0xFC0, 0xFFF, 0x1000, 0xFFFFFFFFu)));
        break;
    case kFRingWait:
        o[0xA] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 8, 0xFF) : ah::Next());
        break;
    case kFRingRise:
        SetWord(o + 0x3E, ah::Often() ? AH_PICK(0x270, 0x271, 0x26F, 0x280, 0, 0xFFF0, 0x7FF0, 0x8000, 0x100, 0xFFE1, 0x8001, 0xFFFF, 0xFF01) : ah::Next());
        // the GTE and the camera at identity half the time (a translation
        // off the ring so the depth is sane), else the random fill
        if (ah::Half()) SetIdentity(kGteMatrix, 0, 0, 0);
        if (ah::Half()) SetIdentity(kCameraMatrix, static_cast<std::int32_t>(ah::Next() % 0x100), static_cast<std::int32_t>(ah::Next() % 0x100), 0x400 + static_cast<std::int32_t>(ah::Next() % 0x400));
        if (ah::Half()) {
            // positions the GTE maps near the screen, and anything
            SetLong(o + 0x34, static_cast<std::int32_t>(0x800000 + (ah::Next() & 0xFFFFF)));
            SetLong(o + 0x38, static_cast<std::int32_t>(0x800000 + (ah::Next() & 0xFFFFF)));
        }
        break;
    case kFBand:
        Cond_ByteFE = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 2, 0, 3, 0xFF, 1, 2) : ah::Next());
        break;
    default: break;
    }
}
// The draws' and turns' arguments: the gauge's (x, y, n) whole words; the
// turn's facings bytes with bits above them.
void ArgsFx(unsigned k, U* a) {
    if (k == kFGauge) {
        a[0] = ah::Often() ? AH_PICK(0xDC, 0, 0x7FF0, 0xFFFF8000u) : ah::Next();
        a[1] = ah::Often() ? AH_PICK(0x10, 0xFFF0, 0x7FFF) | (ah::Half() ? ah::Next() & 0xFFFF0000u : 0) : ah::Next();
        a[2] = ah::Often() ? AH_PICK(0, 1, 2, 3, 7, 8, 0x100) : ah::Next();
    } else if (k == kFTurnStep) {
        const U b = ah::Next() % 8;
        const U d = AH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 0xFF, 0xFE);
        a[0] = ((b + d) & (ah::Often() ? 7u : 0xFFu)) | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0);
        a[1] = b | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0);
    }
}
void DisturbFx(U h) {
    switch ((h >> 16) % 4) {
    case 0: Mem(at::kLeaderDir)[0] = static_cast<unsigned char>((h >> 8) % 8); break;
    case 1: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ 0x1B); break;
    case 2: Mem(at::kLeaderB)[0] = static_cast<unsigned char>(h >> 8); break;
    default: break;
    }
}
void SettleFx() {
    unsigned char* const o = Sprite_Current;
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
}

// BOF3X_AR3B_GROUP=n runs group n alone (120, 121 the handlers, 1210 the world
// map, 1211 the leader, 1212 the effects; the controls script's shortcut);
// unset, every group runs.
bool Wants(int group) {
    const char* const only = std::getenv("BOF3X_AR3B_GROUP");
    return only == nullptr || *only == 0 || std::atoi(only) == group;
}

}  // namespace

void SelfTest() {
    SaveTables();
    std::memcpy(g_heights, Mem(at::kBandHeights), sizeof g_heights);

    if (Wants(120)) {
        ah::Group g{"area_w3b", kClones120, AH_COUNT(kClones120), nullptr, 0, nullptr, 0,
                    kRegions120, AH_COUNT(kRegions120), &Seed120, &Disturb120, 6000};
        g.area = 120;
        ah::Run(g);
    }
    if (Wants(121)) {
        ah::Group g{"area_w3b", kClones121h, AH_COUNT(kClones121h), kCallees121h, AH_COUNT(kCallees121h), kTables121h,
                    AH_COUNT(kTables121h), kRegions121h, AH_COUNT(kRegions121h), &Seed121h, &Disturb121h, 6000};
        g.phase_span = 2;
        g.area = 121;
        ah::Run(g);
    }
    if (Wants(1210)) {
        g_regionsWm[kRegWmPackets].at = Key(g_packets);
        ah::Group g{"area_w3b", kClonesWm, AH_COUNT(kClonesWm), kCalleesWm, AH_COUNT(kCalleesWm), kTablesWm,
                    AH_COUNT(kTablesWm), g_regionsWm, AH_COUNT(g_regionsWm), &SeedWm, &DisturbWm, 4000};
        g.settle = &SettleWm;
        g.phase_span = 5;
        g.args = &ArgsWm;
        g.area = 121;
        ah::Run(g);
    }
    if (Wants(1211)) {
        ah::Group g{"area_w3b", kClonesLeader, AH_COUNT(kClonesLeader), kCalleesLeader, AH_COUNT(kCalleesLeader),
                    kTablesLeader, AH_COUNT(kTablesLeader), kRegionsLeader, AH_COUNT(kRegionsLeader), &SeedLeader,
                    &DisturbLeader, 8000};
        g.phase_span = 2;
        g.args = &ArgsLeader;
        g.area = 121;
        ah::Run(g);
    }
    if (Wants(1212)) {
        g_regionsFx[kRegFxPackets].at = Key(g_packets);
        ah::Group g{"area_w3b", kClonesFx, AH_COUNT(kClonesFx), kCalleesFx, AH_COUNT(kCalleesFx), kTablesFx,
                    AH_COUNT(kTablesFx), g_regionsFx, AH_COUNT(g_regionsFx), &SeedFx, &DisturbFx, 6000};
        g.settle = &SettleFx;
        g.phase_span = 5;
        g.args = &ArgsFx;
        g.area = 121;
        ah::Run(g);
    }
}

}  // namespace area_w3b
