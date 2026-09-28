// BOF3X_SHADOW=area_w1c: world 1's areas 48..52 through the area round's
// shared harness (area_harness.h), once at start-up - one area_harness::Run
// per area, each Group setting its own area number, all under the one shadow
// name. docs/area_w1c.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA048..052
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and the one jump table agree); the shapes are the root table each
// function hangs from (docs/area_w1c.md section 1). The group's own callees
// (area 49's and 52's helpers, area 52's jump thunk's target) are recorders
// here like any other callee, so each function is fuzzed alone.
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1c.h"
#include "game/area_w1c_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w1c {
namespace {

namespace ah = area_harness;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])

// ---- area 48 ----
constexpr ah::CallSite kCalls408FF0[] = {{0x21, 0x587740}, {0x2F, 0x57C7A0}};
constexpr ah::CallSite kCalls409040[] = {{0x2B, 0x57C160}};
constexpr ah::CallSite kCalls409080[] = {{0x39, 0x587740}};
constexpr ah::CallSite kCalls4090C0[] = {{0x28, 0x57C840}, {0x42, 0x578C10}, {0x72, 0x573400}};
constexpr ah::CallSite kCalls409170[] = {{0x28, 0x57C840}, {0x42, 0x578C10}, {0x72, 0x573400}};
constexpr ah::CallSite kCalls4092D0[] = {{0x35, 0x531F90}, {0x48, 0x531F90}, {0x50, 0x57C7A0}};
constexpr ah::CallSite kCalls409340[] = {{0x28, 0x57C7C0}, {0x56, 0x57C7C0}, {0x80, 0x57C7C0}};
constexpr ah::CallSite kCalls4093D0[] = {{0xE, 0x587740}};
// ---- area 49 ----
constexpr ah::CallSite kCalls4093F0[] = {{0x23, 0x57C0F0}};
constexpr ah::CallSite kCalls409440[] = {{0x13, 0x409460}};
constexpr ah::CallSite kCalls409460[] = {{0x7, 0x57C110}, {0x13, 0x57C0F0}};
constexpr ah::CallSite kCalls409480[] = {{0x1B, 0x409460}, {0x75, 0x57C140}, {0xBC, 0x57C140}, {0xFD, 0x409760}, {0x1CA, 0x57C140}, {0x1E9, 0x589330}, {0x202, 0x534610}, {0x20F, 0x534710}, {0x236, 0x57C140}, {0x286, 0x52E140}, {0x293, 0x5893A0}};
constexpr ah::CallSite kCalls4097D0[] = {{0x41, 0x57C140}, {0x5F, 0x57C160}, {0x66, 0x469FE0}, {0x70, 0x587740}};
constexpr ah::CallSite kCalls409850[] = {{0x8, 0x57C0F0}};
constexpr ah::CallSite kCalls409870[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}, {0x32, 0x579F00}, {0x3D, 0x579F00}};
constexpr ah::CallSite kCalls4098C0[] = {{0x9, 0x579F00}, {0x17, 0x579F00}, {0x25, 0x579F00}, {0x33, 0x579F00}, {0x41, 0x579F00}, {0x4F, 0x579F00}};
constexpr ah::CallSite kCalls409980[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls4099D0[] = {{0x2C, 0x57CE10}};
constexpr ah::CallSite kCalls409A20[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls409A70[] = {{0x6, 0x579F00}, {0x11, 0x579F00}};
constexpr ah::CallSite kCalls409A90[] = {{0xA, 0x589810}, {0x34, 0x587740}, {0x4E, 0x579F00}, {0x5C, 0x579F00}};
constexpr ah::CallSite kCalls409B20[] = {{0x30, 0x57CE10}};
constexpr ah::CallSite kCalls409B70[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}, {0x32, 0x579F00}, {0x3D, 0x579F00}, {0x4B, 0x579F00}, {0x56, 0x579F00}};
constexpr ah::CallSite kCalls409BD0[] = {{0x1D, 0x57C7C0}};
constexpr ah::CallSite kCalls409C10[] = {{0x1D, 0x57C7C0}};
// ---- area 50 ----
constexpr ah::CallSite kCalls409C90[] = {{0x6, 0x579F00}, {0x11, 0x579F00}, {0x1C, 0x579F00}, {0x27, 0x579F00}};
constexpr ah::CallSite kCalls409CC0[] = {{0x0, 0x57C7C0}};
// ---- area 51 ----
constexpr ah::CallSite kCalls409CE0[] = {{0x12, 0x531F10}, {0x3C, 0x57C8A0}};
constexpr ah::CallSite kCalls409D30[] = {{0x2F, 0x5720C0}};
constexpr ah::CallSite kCalls409DA0[] = {{0x8, 0x57C140}, {0x1B, 0x57C140}};
// ---- area 52 ----
constexpr ah::CallSite kCalls409E20[] = {{0x61, 0x572620}, {0x74, 0x536700}, {0x93, 0x579F00}, {0x9F, 0x572620}, {0xB5, 0x536700}, {0xD7, 0x579F00}};
constexpr ah::CallSite kCalls409F20[] = {{0xD, 0x40A2F0}, {0x2A, 0x572620}, {0x43, 0x579F00}, {0x55, 0x572620}, {0x72, 0x579F00}, {0xA0, 0x572620}, {0xB7, 0x536700}, {0xD6, 0x579F00}, {0xEC, 0x572620}, {0x104, 0x536700}, {0x11F, 0x579F00}, {0x137, 0x578C10}};
constexpr ah::CallSite kCalls40A070[] = {{0x38, 0x572620}, {0x4E, 0x579F00}, {0x57, 0x572620}, {0x6C, 0x579F00}, {0xC0, 0x572620}, {0xD3, 0x536700}, {0xF2, 0x579F00}, {0xFE, 0x572620}, {0x114, 0x536700}, {0x136, 0x579F00}};
constexpr ah::CallSite kCalls40A1D0[] = {{0xD, 0x40A2F0}, {0x3B, 0x40A450}, {0x5F, 0x40A350}};
constexpr ah::CallSite kCalls40A260[] = {{0xD, 0x40A2F0}, {0x3B, 0x40A450}, {0x5F, 0x40A350}};
constexpr ah::CallSite kCalls40A2F0[] = {{0x36, 0x40A4C0}};
constexpr ah::CallSite kCalls40A350[] = {{0x10, 0x572620}, {0x20, 0x579F00}, {0x2C, 0x572620}, {0x3D, 0x579F00}, {0x6C, 0x572620}, {0x7C, 0x536700}, {0x94, 0x579F00}, {0xA6, 0x572620}, {0xB5, 0x536700}, {0xC8, 0x579F00}, {0xDF, 0x578C10}};
constexpr ah::CallSite kCalls40A450[] = {{0x11, 0x572620}, {0x25, 0x579F00}, {0x2F, 0x572620}, {0x42, 0x579F00}};
constexpr ah::CallSite kCalls40A4C0[] = {{0x1C, 0x572620}, {0x31, 0x579F00}, {0x3D, 0x572620}, {0x55, 0x579F00}};
constexpr ah::CallSite kCalls40A530[] = {{0x3F, 0x57C7C0}, {0x57, 0x40A5C0}, {0x79, 0x57C160}, {0x83, 0x587740}};
constexpr ah::CallSite kCalls40A670[] = {{0x7C, 0x4976D0}, {0x102, 0x57C110}, {0x111, 0x57C7A0}, {0x182, 0x57C110}, {0x191, 0x57C7A0}, {0x1C3, 0x40A940}, {0x1E8, 0x572620}, {0x1F5, 0x579F00}, {0x202, 0x572620}, {0x20F, 0x579F00}, {0x21C, 0x572620}, {0x22A, 0x579F00}, {0x23A, 0x572620}, {0x248, 0x579F00}};
constexpr ah::JumpTable kTables40A670[] = {{0x1C, 0x264, 15}};
constexpr ah::CallSite kCalls40A940[] = {{0x0, 0x40A9C0}};
constexpr ah::CallSite kCalls40A950[] = {{0x7, 0x57C110}};
constexpr ah::CallSite kCalls40A960[] = {{0x1, 0x589810}, {0x3F, 0x5720C0}};
constexpr ah::CallSite kCalls40A9C0[] = {{0x1, 0x589810}, {0x3F, 0x5720C0}};
constexpr ah::CallSite kCalls40AA20[] = {{0xE, 0x40A2F0}, {0x24, 0x572620}, {0x3C, 0x536700}, {0x58, 0x579F00}, {0x6E, 0x572620}, {0x86, 0x536700}, {0xA2, 0x579F00}, {0xBD, 0x578C10}};

const ah::Clone kClones48[] = {
    {"Area48_ChoiceCounterBOr1", 0x408FF0, 0x41, kCalls408FF0, AH_N(kCalls408FF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_ChoiceCounterBOr1), 0x0, false, ah::Shape::kChoice},
    {"Area48_ToggleFlagD", 0x409040, 0x34, kCalls409040, AH_N(kCalls409040), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_ToggleFlagD), 0x0, false, ah::Shape::kHandler},
    {"Area48_BumpCount", 0x409080, 0x40, kCalls409080, AH_N(kCalls409080), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_BumpCount), 0x0, false, ah::Shape::kHandler},
    {"Area48_MoveScriptObject5", 0x4090C0, 0xA3, kCalls4090C0, AH_N(kCalls4090C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_MoveScriptObject5), 0x0, false, ah::Shape::kHandler},
    {"Area48_MoveScriptObject1", 0x409170, 0xA3, kCalls409170, AH_N(kCalls409170), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_MoveScriptObject1), 0x0, false, ah::Shape::kHandler},
    {"Area48_PoseByCount", 0x409220, 0x87, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_PoseByCount), 0x0, false, ah::Shape::kHandler},
    {"Area48_ResetCount", 0x4092B0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_ResetCount), 0x0, false, ah::Shape::kHandler},
    {"Area48_TailDropIn", 0x4092D0, 0x6B, kCalls4092D0, AH_N(kCalls4092D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_TailDropIn), 0x0, false, ah::Shape::kTail},
    {"Area48_ArriveHook", 0x409340, 0x8B, kCalls409340, AH_N(kCalls409340), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_ArriveHook), 0xFF, false, ah::Shape::kHook},
    {"Area48_InitSound", 0x4093D0, 0x15, kCalls4093D0, AH_N(kCalls4093D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area48_InitSound), 0x0, false, ah::Shape::kInit},
};
enum : unsigned { k48Choice, k48ToggleD, k48Bump, k48Move5, k48Move1, k48Pose, k48Reset, k48Tail, k48Arrive, k48Init };
const ah::Clone kClones49[] = {
    {"Area49_ChoiceMessageFlag3B", 0x4093F0, 0x2C, kCalls4093F0, AH_N(kCalls4093F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_ChoiceMessageFlag3B), 0x0, false, ah::Shape::kChoice},
    {"Area49_ChoiceMessage", 0x409420, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_ChoiceMessage), 0x0, false, ah::Shape::kChoice},
    {"Area49_StepHook", 0x409440, 0x1B, kCalls409440, AH_N(kCalls409440), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_StepHook), 0xFF, false, ah::Shape::kHook},
    {"Area49_SwapFlags2To3", 0x409460, 0x1C, kCalls409460, AH_N(kCalls409460), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_SwapFlags2To3), 0x0, false, ah::Shape::kCallee},
    {"Area49_EffectFrame", 0x409480, 0x2D6, kCalls409480, AH_N(kCalls409480), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_EffectFrame), 0x0, false, ah::Shape::kCallee},
    {"Area49_MemberZone", 0x409760, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_MemberZone), 0xFF, false, ah::Shape::kCallee},
    {"Area49_CellHook", 0x4097D0, 0x7D, kCalls4097D0, AH_N(kCalls4097D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_CellHook), 0xFF, false, ah::Shape::kHook},
    {"Area49_SetRowFlag3F", 0x409850, 0x11, kCalls409850, AH_N(kCalls409850), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_SetRowFlag3F), 0x0, false, ah::Shape::kHandler},
    {"Area49_ClearCells", 0x409870, 0x46, kCalls409870, AH_N(kCalls409870), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_ClearCells), 0x0, false, ah::Shape::kHandler},
    {"Area49_SetCells", 0x4098C0, 0x58, kCalls4098C0, AH_N(kCalls4098C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_SetCells), 0x0, false, ah::Shape::kHandler},
    {"Area49_Counter1If4", 0x409920, 0x11, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_Counter1If4), 0x0, false, ah::Shape::kHandler},
    {"Area49_Counter2If4", 0x409940, 0x11, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_Counter2If4), 0x0, false, ah::Shape::kHandler},
    {"Area49_ShiftCameraUp", 0x409960, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_ShiftCameraUp), 0x0, false, ah::Shape::kHandler},
    {"Area49_ShiftCameraDown", 0x409970, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_ShiftCameraDown), 0x0, false, ah::Shape::kHandler},
    {"Area49_SpawnAtMember0", 0x409980, 0x42, kCalls409980, AH_N(kCalls409980), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_SpawnAtMember0), 0x0, false, ah::Shape::kHandler},
    {"Area49_SpawnAtMember1", 0x4099D0, 0x42, kCalls4099D0, AH_N(kCalls4099D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_SpawnAtMember1), 0x0, false, ah::Shape::kHandler},
    {"Area49_SpawnAtMember2", 0x409A20, 0x46, kCalls409A20, AH_N(kCalls409A20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_SpawnAtMember2), 0x0, false, ah::Shape::kHandler},
    {"Area49_ClearCellsB", 0x409A70, 0x1A, kCalls409A70, AH_N(kCalls409A70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_ClearCellsB), 0x0, false, ah::Shape::kHandler},
    {"Area49_SpawnEffect6C", 0x409A90, 0x89, kCalls409A90, AH_N(kCalls409A90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_SpawnEffect6C), 0x0, false, ah::Shape::kHandler},
    {"Area49_SpawnKind3AtMember2", 0x409B20, 0x46, kCalls409B20, AH_N(kCalls409B20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_SpawnKind3AtMember2), 0x0, false, ah::Shape::kHandler},
    {"Area49_ClearCellsC", 0x409B70, 0x5F, kCalls409B70, AH_N(kCalls409B70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_ClearCellsC), 0x0, false, ah::Shape::kHandler},
    {"Area49_RunStep5", 0x409BD0, 0x3B, kCalls409BD0, AH_N(kCalls409BD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_RunStep5), 0x0, false, ah::Shape::kHandler},
    {"Area49_RunStep6", 0x409C10, 0x3B, kCalls409C10, AH_N(kCalls409C10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area49_RunStep6), 0x0, false, ah::Shape::kHandler},
};
enum : unsigned {
    k49Choice0, k49Choice1, k49Step, k49Swap, k49Frame, k49Zone, k49Cell, k49RowFlag, k49Clear, k49Set, k49Counter1, k49Counter2,
    k49CameraUp, k49CameraDown, k49Spawn0, k49Spawn1, k49Spawn2, k49ClearB, k49Spawn6C, k49Spawn3, k49ClearC, k49Run5, k49Run6
};
const ah::Clone kClones50[] = {
    {"Area50_ChoiceAsk82", 0x409C50, 0x3E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area50_ChoiceAsk82), 0x0, false, ah::Shape::kChoice},
    {"Area50_SetCells", 0x409C90, 0x30, kCalls409C90, AH_N(kCalls409C90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area50_SetCells), 0x0, false, ah::Shape::kHandler},
    {"Area50_Trigger43", 0x409CC0, 0x14, kCalls409CC0, AH_N(kCalls409CC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area50_Trigger43), 0xFF, false, ah::Shape::kCallee},
};
enum : unsigned { k50Choice, k50Cells, k50Trigger };
const ah::Clone kClones51[] = {
    {"Area51_MemberAtObject", 0x409CE0, 0x46, kCalls409CE0, AH_N(kCalls409CE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area51_MemberAtObject), 0x0, false, ah::Shape::kHandler},
    {"Area51_PlaceAtMember", 0x409D30, 0x45, kCalls409D30, AH_N(kCalls409D30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area51_PlaceAtMember), 0x0, false, ah::Shape::kHandler},
    {"Area51_ChoiceMessage", 0x409D80, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area51_ChoiceMessage), 0x0, false, ah::Shape::kChoice},
    {"Area51_TintBackdrop", 0x409DA0, 0x79, kCalls409DA0, AH_N(kCalls409DA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area51_TintBackdrop), 0x0, false, ah::Shape::kInit},
};
enum : unsigned { k51MemberAt, k51Place, k51Choice, k51Tint };
const ah::Clone kClones52[] = {
    {"Area52_StashBlock", 0x409E20, 0xF5, kCalls409E20, AH_N(kCalls409E20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_StashBlock), 0x0, false, ah::Shape::kHandler},
    {"Area52_ShiftBlockZ", 0x409F20, 0x14D, kCalls409F20, AH_N(kCalls409F20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_ShiftBlockZ), 0x0, false, ah::Shape::kHandler},
    {"Area52_ResetBlock", 0x40A070, 0x154, kCalls40A070, AH_N(kCalls40A070), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_ResetBlock), 0x0, false, ah::Shape::kHandler},
    {"Area52_MoveBlockA", 0x40A1D0, 0x8B, kCalls40A1D0, AH_N(kCalls40A1D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_MoveBlockA), 0x0, false, ah::Shape::kHandler},
    {"Area52_MoveBlockB", 0x40A260, 0x8B, kCalls40A260, AH_N(kCalls40A260), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_MoveBlockB), 0x0, false, ah::Shape::kHandler},
    {"Area52_BlockCell", 0x40A2F0, 0x58, kCalls40A2F0, AH_N(kCalls40A2F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_BlockCell), 0xFF, false, ah::Shape::kCallee},
    {"Area52_ShiftBlockX", 0x40A350, 0xF6, kCalls40A350, AH_N(kCalls40A350), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_ShiftBlockX), 0x0, false, ah::Shape::kCallee},
    {"Area52_DropColumn", 0x40A450, 0x64, kCalls40A450, AH_N(kCalls40A450), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_DropColumn), 0x0, false, ah::Shape::kCallee},
    {"Area52_RestoreBlock", 0x40A4C0, 0x6C, kCalls40A4C0, AH_N(kCalls40A4C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_RestoreBlock), 0x0, false, ah::Shape::kCallee},
    {"Area52_CellHook", 0x40A530, 0x90, kCalls40A530, AH_N(kCalls40A530), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_CellHook), 0xFF, false, ah::Shape::kHook},
    {"Area52_PartyInRect", 0x40A5C0, 0xA4, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_PartyInRect), 0xFF, false, ah::Shape::kCallee},
    {"Area52_TailBlock", 0x40A670, 0x2D0, kCalls40A670, AH_N(kCalls40A670), nullptr, 0, kTables40A670, AH_N(kTables40A670), reinterpret_cast<const void*>(&::Area52_TailBlock), 0x0, false, ah::Shape::kTail},
    {"Area52_SpawnEffect48BJump", 0x40A940, 0x5, kCalls40A940, AH_N(kCalls40A940), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_SpawnEffect48BJump), 0x0, false, ah::Shape::kCallee},
    {"Area52_ClearFlag25", 0x40A950, 0x10, kCalls40A950, AH_N(kCalls40A950), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_ClearFlag25), 0x0, false, ah::Shape::kHandler},
    {"Area52_SpawnEffect48A", 0x40A960, 0x57, kCalls40A960, AH_N(kCalls40A960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_SpawnEffect48A), 0x0, false, ah::Shape::kHandler},
    {"Area52_SpawnEffect48B", 0x40A9C0, 0x57, kCalls40A9C0, AH_N(kCalls40A9C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_SpawnEffect48B), 0x0, false, ah::Shape::kHandler},
    {"Area52_ShiftBlockXBack", 0x40AA20, 0xD3, kCalls40AA20, AH_N(kCalls40AA20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area52_ShiftBlockXBack), 0x0, false, ah::Shape::kHandler},
};
enum : unsigned {
    k52Stash, k52ShiftZ, k52Reset, k52MoveA, k52MoveB, k52Cell, k52ShiftX, k52Drop, k52Restore, k52Hook, k52InRect, k52Tail,
    k52Jump, k52Flag25, k52Spawn48A, k52Spawn48B, k52ShiftXBack
};
#undef AH_N

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(std::uint32_t address) { return *ah::Mem(address); }

// Two dwords the clone of Area52_BlockCell writes through (its arguments
// point here; the region compares them).
std::int32_t g_cells[2];

// A record the active member pointer may name: one of the four party objects
// (Sprite_ObjectsExtra), a field object, a party record, or the running
// object itself (the two alias in the game).
unsigned char* MemberRecord(std::uint32_t v) {
    switch (v % 4) {
    case 0: return ah::Mem(at::kSpriteObjectsExtra + (v >> 2) % 4 * 0xA4);
    case 1: return ah::Object(v >> 2);
    case 2: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return Sprite_Current;
    }
}
unsigned char* ScriptRecord(std::uint32_t v) {
    return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1));
}

// ---- the stand-ins the group lists ----

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu, kHigh = 0xFFFF0000u;

// Louder than the real callees, on purpose (each only half the time, from
// Noise): after these calls the callers read a cell again - the tail's
// sub-kind after Party_DropIn (Area48_TailDropIn), the script object after
// MoveCmd_Move / MoveCmd_MoveKind2, the active member after
// Sound_PlayEffect (Area49_SpawnEffect6C), the running object after the map
// reads and the elevation (area 52's column helpers, Area51_PlaceAtMember),
// counter 3 after Area52_BlockCell and the object's x after
// Area52_ShiftBlockX (Area52_MoveBlockA / B). The harness's own disturbance
// reaches a group cell about one call in 24.
std::uint32_t MovesSub(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) B(at::kTailSub) = static_cast<unsigned char>((n >> 8) % 4);
    return answer;
}
std::uint32_t MovesScriptObject(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 8));
    return answer;
}
std::uint32_t MovesMember(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kActiveMember, MemberRecord(n >> 8));
    return answer;
}
std::uint32_t MovesCurrent(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3);
    return answer;
}
// MoveScript_ObjectKind answers an int its callers compare whole with 2.
std::uint32_t KindAnswer(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    return n % 3 == 0 ? answer : n % 4;
}
// Area52_BlockCell writes the object's x and z less 0x8000 through its two
// pointers (half the time; else noise) and moves counter 3.
std::uint32_t BlockCellEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* const x = reinterpret_cast<std::int32_t*>(static_cast<std::uintptr_t>(a[0]));
    auto* const z = reinterpret_cast<std::int32_t*>(static_cast<std::uintptr_t>(a[1]));
    const std::uint32_t n = ah::Noise();
    *x = n & 1 ? static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(Sprite_Current + 0x34)) - 0x8000u)
               : static_cast<std::int32_t>(ah::Noise());
    *z = n & 2 ? static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(Sprite_Current + 0x38)) - 0x8000u)
               : static_cast<std::int32_t>(ah::Noise());
    if (n & 4) {
        static const unsigned char kCounters[] = {0xA, 0x14, 0, 0xB};
        B(at::kCounter3) = kCounters[(n >> 8) % 4];
    }
    return answer;
}
std::uint32_t ShiftXEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = ah::Noise();
    if (n & 1) move_script::SetLong(Sprite_Current + 0x34, n & 2 ? 0x858000 : static_cast<std::int32_t>(n));
    return answer;
}

#define W1C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W1C_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    // the map: words x, z and a value byte, as the originals pass them
    {W1C_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    {W1C_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W1C_OURS(AreaMap_Elevation), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {"SetLayerByte_572620", at::kSetLayerByte, at::kSetLayerByte, 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    {"FlagsToggle_57C160", at::kFlagsToggle, at::kFlagsToggle, 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"MemberSetState_57C8A0", at::kMemberSetState, at::kMemberSetState, 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0},
    {"SpawnKind4_469FE0", at::kSpawnKind4, at::kSpawnKind4, 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    // slots inside the group's four effect records, or none; members 0..2 or none
    {W1C_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03},
    {W1C_THEIRS(Effect_Spawn), 5, {kU8, kU8, kU8, kU16, kU16}, ah::Answer::kByte, 0xFE, 0x02},
    {W1C_OURS(Party_MemberAt), 3, {kAll, kAll, kAll}, ah::Answer::kByte, 0xFE, 0x02},
    {W1C_OURS(Party_DropIn), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesSub},
    {W1C_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &MovesMember},
    {W1C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1C_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1C_OURS(MoveScript_ObjectKind), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &KindAnswer},
    {W1C_THEIRS(MoveCmd_Move), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W1C_OURS(MoveCmd_MoveKind2), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W1C_OURS(Field_JumpSetUp), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1C_OURS(Field_JumpCamera), 0, {}, ah::Answer::kGarbage, 0, 0},
    // the group's own, called directly
    {W1C_OURS(Area49_SwapFlags2To3), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1C_OURS(Area49_MemberZone), 1, {kU8}, ah::Answer::kByte, 0xFF, 0x06},
    {W1C_OURS(Area52_BlockCell), 2, {0, 0}, ah::Answer::kFlag, 0, 0, {}, &BlockCellEffect},
    {W1C_OURS(Area52_ShiftBlockX), 2, {kHigh, kHigh}, ah::Answer::kGarbage, 0, 0, {}, &ShiftXEffect},
    {W1C_OURS(Area52_DropColumn), 2, {kHigh, kHigh}, ah::Answer::kGarbage, 0, 0},
    {W1C_OURS(Area52_RestoreBlock), 2, {kHigh, kHigh}, ah::Answer::kGarbage, 0, 0},
    {W1C_OURS(Area52_PartyInRect), 0, {}, ah::Answer::kFlag, 0, 0},
    {W1C_OURS(Area52_SpawnEffect48BJump), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W1C_OURS(Area52_SpawnEffect48B), 0, {}, ah::Answer::kGarbage, 0, 0},
};
#undef W1C_OURS
#undef W1C_THEIRS

// Beyond the field frame: the effect records (slots 0..3), the active member
// and script object pointers, the chapter row pointer, the mark, area 48's
// two bytes, 0x9045FB, Camera_ShiftY, MoveScript_EffectState, area 52's
// extra objects (.data), and the two dwords above.
const ah::Region kRegions[] = {
    {at::kEffectObjects, 4 * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFlagRow, 4},
    {at::kAnswerMark, 1},
    {at::kCount48, 1},
    {at::kByte803490, 1},
    {at::kByte9045FB, 1},
    {at::kCameraShiftY, 2},
    {at::kEffectState, 24},
    {at::kArea52ExtraObjects, 16},
    {0, sizeof g_cells},   // g_cells, filled in at Run
};

// Every round: the pointers the areas follow put back inside the regions.
void Common() {
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(std::uint32_t h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 8) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 0x31 : v); break;
    case 1: B(at::kTailSub) = static_cast<unsigned char>(v % 4); break;
    case 2: {
        static const unsigned char kCounters[] = {0xA, 0x14, 0x1E, 0x28, 0};
        B(at::kCounter3) = h & 0x100 ? kCounters[v % 5] : v;
        break;
    }
    case 3: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 4: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 5: B(at::kCount48) = static_cast<unsigned char>(v % 10); break;
    case 6: move_script::SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? v % 6 : v); break;
    default: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 4); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (tested signed by some), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 0xFF, 0x80, 0x81, 0x7F, 4));
}
// Area 48's count: each value a switch tests, the byte edges, anything.
void SeedCount() {
    if (ah::Often()) B(at::kCount48) = static_cast<unsigned char>(AH_PICK(2, 6, 8, 0, 1, 3, 5, 7, 9, 0xFF, 0x40, 0x3F, 0x41, 0xC0));
}
// A 16.16 word with the high word `high` and any low word.
std::uint32_t At16(std::uint32_t high, std::uint32_t low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }

// ---- area 48 ----
void Seed48(unsigned k) {
    Common();
    switch (k) {
    case k48Choice: SeedAnswer(); break;
    case k48ToggleD: case k48Bump: {
        // the leader's index inside MoveScript_EffectState, its byte 1 half the time
        if (ah::Often()) B(at::kLeaderByte89) = static_cast<unsigned char>(ah::Next() % 24);
        if (ah::Half()) B(at::kEffectState + B(at::kLeaderByte89) % 24) = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 0x81));
        SeedCount();
        break;
    }
    case k48Move5: case k48Move1: case k48Pose: SeedCount(); break;
    case k48Tail:
        SeedCount();
        if (ah::Often()) B(at::kTailSub) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 0xFF, 0x81));
        break;
    case k48Init:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(5, 4, 6, 0x85));
        break;
    default: break;
    }
}
// The arrive hook's (x, z): one doorway at a time, on it or one step off an
// edge, z exactly 0x58000 / 0xA8000 for the second and third, or anything.
void Args48(unsigned k, std::uint32_t* a) {
    if (k != k48Arrive || !ah::Often()) return;
    const std::uint32_t lx = a[0], lz = a[1];
    switch (ah::Next() % 5) {
    case 0:
        a[0] = At16(AH_PICK(0x17, 0x17, 0x16, 0x18, 0x117), lx);
        a[1] = At16(AH_PICK(0xD, 0xE, 0xC, 0xF, 0x10D), lz);
        break;
    case 1:
        a[0] = At16(AH_PICK(0xC, 0xD, 0xB, 0xE, 0x10C), lx);
        a[1] = At16(AH_PICK(6, 6, 5, 7, 0x106), lz);
        break;
    case 2:
        a[0] = At16(AH_PICK(0x13, 0x14, 0x12, 0x15, 0x113), lx);
        a[1] = At16(AH_PICK(0xB, 0xB, 0xA, 0xC, 0x10B), lz);
        break;
    case 3:
        a[0] = At16(AH_PICK(0xC, 0xD, 0x13, 0x14, 0x17), lx);
        a[1] = AH_PICK(0x58000, 0xA8000, 0x58001, 0xA7FFF, 0xD0000, 0x68000);
        break;
    default:
        a[0] = At16(0x17, lx);
        a[1] = At16(0xD + ah::Next() % 2, lz);
        break;
    }
}

// ---- area 49 ----

// A party record's position on or beside one edge of a rectangle whose
// bounds are bytes << 16 (Area49_Zones), each axis drawn alone.
std::int32_t Around(unsigned lo, unsigned hi) {
    switch (ah::Next() % 6) {
    case 0: return static_cast<std::int32_t>(lo << 16);
    case 1: return static_cast<std::int32_t>(lo << 16) - 1;
    case 2: return static_cast<std::int32_t>(hi << 16);
    case 3: return static_cast<std::int32_t>(hi << 16) + 1;
    case 4: return static_cast<std::int32_t>((lo + (hi > lo ? ah::Next() % (hi - lo) : 0)) << 16 | (ah::Next() & 0xFFFF));
    default: return static_cast<std::int32_t>(ah::Next());
    }
}
void PlaceInZone(unsigned member) {
    unsigned char* const record = ah::PartyOf(static_cast<unsigned char>(member));
    const unsigned char* const zone = ah::Mem(at::kArea49Zones + (ah::Next() % at::kArea49ZoneCount) * 6u);
    move_script::SetLong(record + 0x34, Around(zone[0], zone[2]));
    move_script::SetLong(record + 0x38, Around(zone[1], zone[3]));
}

// A cell switch's (x, z) and the leader's pose: record i's exactly, or one
// field off by one, a high byte above the cell byte half the time. The seed
// draws the record and sets the pose (the arguments are drawn after the
// round's state is captured, so a write there would be lost); the arguments
// then follow the seed's draw.
const unsigned char* g_switch = nullptr;   // the record drawn, or none
unsigned g_switch_off = 0;                 // 0 none off, 1 x off, 2 z off, 3 the pose off
void SeedSwitch(std::uint32_t table, unsigned stride, unsigned count, unsigned char pose_mask) {
    g_switch = nullptr;
    if (!ah::Often()) return;
    g_switch = ah::Mem(table + (ah::Next() % count) * stride);
    g_switch_off = ah::Half() ? 1 + ah::Next() % 3 : 0;
    const auto pose = static_cast<unsigned char>(g_switch[2] & pose_mask);
    B(at::kLeaderPose) = g_switch_off == 3 ? static_cast<unsigned char>(pose + (ah::Half() ? 1 : 0x10)) : pose;
}
void CellArgs(std::uint32_t* a) {
    if (!g_switch) return;
    const unsigned char* const r = g_switch;
    a[0] = (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0) | r[0];
    a[1] = (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0) | r[1];
    if (g_switch_off == 1) a[0] = (a[0] & ~0xFFu) | static_cast<unsigned char>(r[0] + (ah::Half() ? 1 : 0xFF));
    if (g_switch_off == 2) a[1] = (a[1] & ~0xFFu) | static_cast<unsigned char>(r[1] + (ah::Half() ? 1 : 0xFF));
}

void Seed49(unsigned k) {
    Common();
    if (k == k49Cell) SeedSwitch(at::kArea49CellSwitches, 4, at::kArea49CellSwitchCount, 0xFF);
    switch (k) {
    case k49Choice0: case k49Choice1: SeedAnswer(); break;
    case k49Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0x80));
        break;
    case k49Frame: {
        // the running object an effect record, its member mask any; the
        // area the pending one or not; the members' +9 at 0 half the time
        Sprite_Current = ah::Mem(at::kEffectObjects + (ah::Next() % 4) * at::kEffectStride);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 0, 2, 4, 6));
        if (ah::Half()) move_script::SetWord(ah::Mem(at::kPendingArea), ah::Half() ? 0x31 : AH_PICK(0x30, 0x32, 0x131));
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~7u);
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~7u);
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char* const record = ah::PartyOf(static_cast<unsigned char>(m));
            if (ah::Half()) record[9] = 0;
            if (ah::Half()) record[8] = static_cast<unsigned char>(ah::Next() % 8);
        }
        if (ah::Next() % 10 == 0) Field_MemberCount = 0;
        break;
    }
    case k49Zone:
        for (unsigned m = 0; m < 3; ++m)
            if (ah::Often()) PlaceInZone(m);
        break;
    case k49Counter1: if (ah::Half()) B(at::kPartyList0 + 1) = 4; break;
    case k49Counter2: if (ah::Half()) B(at::kPartyList0 + 2) = 4; break;
    case k49Spawn0: case k49Spawn1: case k49Spawn2: case k49Spawn3:
        for (unsigned m = 0; m < 3; ++m)
            if (ah::Often()) B(at::kPartyList0 + m) = static_cast<unsigned char>(ah::Next() % 8);
        break;
    case k49Spawn6C: if (ah::Half()) B(at::kLeaderByte89) = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0x85)); break;
    case k49Run5: case k49Run6: if (ah::Half()) B(at::kLeaderByte89) = static_cast<unsigned char>(AH_PICK(6, 6, 5, 7, 0x86)); break;
    default: break;
    }
}
void Args49(unsigned k, std::uint32_t* a) {
    if (k == k49Step) {
        if (ah::Often()) a[0] = AH_PICK(0x4D8000, 0x4D8000, 0x4D8001, 0x4D7FFF, 0x14D8000, 0x4D0000);
    } else if (k == k49Zone) {
        a[0] = ah::Often() ? ah::Next() % 3 | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0) : ah::Next() % 8;
    } else if (k == k49Cell) {
        CellArgs(a);
    }
}

// ---- area 50 ----
void Seed50(unsigned k) {
    Common();
    if (k == k50Choice) {
        SeedAnswer();
        if (ah::Often()) B(at::kByte9045FB) = static_cast<unsigned char>(AH_PICK(0x1E, 0x1D, 0x1F, 0, 0xFF, 0x80, 0x7F));
    }
}
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(unsigned, std::uint32_t* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}

// ---- area 51 ----

// The chain AreaMap_HeaderPass walks, built in the area block: 0..10 entries
// of kinds around 0x81 with steps of 1..3 dwords, a zero dword the end
// (area_011_fuzz.cpp's).
void Chain() {
    const unsigned base = 0x10 + ah::Next() % 0x300;
    AreaMap_EntryBase = static_cast<unsigned short>(base);
    unsigned char* p = AreaMap_Header + base * 4u;
    const unsigned count = ah::Next() % 11;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned step = 1 + ah::Next() % 3;
        const std::uint32_t kind = AH_PICK(0x81, 0x81, 0x80, 0x82, 0x01, 0x00, 0xC1, 0x41) << 24;
        const std::uint32_t low = ah::Half() ? ah::Next() & 0xFFFF : 0;
        move_script::SetLong(p, static_cast<std::int32_t>(kind | step << 16 | low));
        p += step * 4u;
    }
    move_script::SetLong(p, 0);
}
void Seed51(unsigned k) {
    Common();
    switch (k) {
    case k51MemberAt:
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 5));
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1000u);
        break;
    case k51Place:
        // ten records after a field object, so the one it reads is one too
        if (ah::Often()) ah::SetPointer(at::kActiveMember, ah::Object(10 + ah::Next() % 20));
        break;
    case k51Choice: SeedAnswer(); break;
    case k51Tint: {
        // the two flags in all four ways (the bits set for the record: Flags_Test is a recorder), and the chain
        unsigned char* const flags = ah::Mem(at::kCondRow14 + 2);
        flags[0] = static_cast<unsigned char>((flags[0] & ~0x18u) | (ah::Next() & 0x18u));
        Chain();
        break;
    }
    default: break;
    }
}

// ---- area 52 ----

// A word on or beside an edge of [lo, lo + span): lo, lo - 1, the last inside,
// the first past, inside, anything.
unsigned Edge(unsigned lo, unsigned span) {
    switch (ah::Next() % 6) {
    case 0: return lo;
    case 1: return lo - 1;
    case 2: return lo + span - 1;
    case 3: return lo + span;
    case 4: return lo + (span ? ah::Next() % span : 0);
    default: return ah::Next();
    }
}

void Seed52(unsigned k) {
    Common();
    switch (k) {
    case k52Stash:
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 0, 2, 4, 6));
        break;
    case k52Reset:
        // the start exactly, or one word off
        if (ah::Often()) {
            move_script::SetLong(Sprite_Current + 0x34, 0x118000);
            move_script::SetLong(Sprite_Current + 0x38, 0x298000);
            if (ah::Half()) {
                if (ah::Half()) move_script::SetLong(Sprite_Current + 0x34, AH_PICK(0x118001, 0x117FFF, 0x10118000));
                else move_script::SetLong(Sprite_Current + 0x38, AH_PICK(0x298001, 0x297FFF, 0x10298000));
            }
        }
        break;
    case k52MoveA: case k52MoveB:
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(0xA, 0x14, 0xA, 0x14, 0, 9, 0xB, 0x13, 0x15));
        if (ah::Half())
            move_script::SetLong(Sprite_Current + 0x34, k == k52MoveA ? AH_PICK(0x688000, 0x688001, 0x858000) : AH_PICK(0x668000, 0x668001, 0x858000));
        break;
    case k52Cell:
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 0, 2, 4));
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(0, 1, 0xA, 0x80));
        break;
    case k52InRect: {
        // the tail state 0 or not (the rectangle), each member on or beside an edge
        if (ah::Half()) B(at::kTailState) = 0;
        const unsigned char* const rect = ah::Mem(at::kArea52Rects + (B(at::kTailState) != 0 ? 4u : 0u));
        for (unsigned m = 0; m < 3; ++m) {
            if (!ah::Often()) continue;
            unsigned char* const record = ah::PartyOf(static_cast<unsigned char>(m));
            move_script::SetWord(record + 0x36, Edge(rect[0], rect[2]));
            move_script::SetWord(record + 0x3A, Edge(rect[1], rect[3]));
        }
        if (ah::Next() % 10 == 0) Field_MemberCount = 0;
        break;
    }
    case k52Hook: SeedSwitch(at::kArea52CellSwitches, 5, at::kArea52CellSwitchCount, 0x0F); break;
    case k52Tail: {
        // each state the tables reach, its neighbours and the out-of-range
        // sides; then the cell that state waits on, at its value two times
        // in three and beside it otherwise (step-paired)
        static const std::uint32_t kStates[] = {0, 1, 2, 0xA, 0xB, 0x14, 0x15, 0x16, 0x28, 0x29, 0x2A, 0x2D, 0x2E, 0x2F,
                                                0, 1, 2, 0xA, 0xB, 0x14, 0x15, 0x16, 0x28, 0x29, 0x2A, 0x2D, 0x2E, 0x2F,
                                                3, 9, 0xC, 0x13, 0x17, 0x27, 0x2B, 0x2C, 0x30, 0x31, 0xFF, 0x80, 0x7F};
        const auto state = static_cast<unsigned char>(ah::Pick(kStates, sizeof kStates / sizeof kStates[0]));
        B(at::kTailState) = state;
        const bool on = ah::Often();
        switch (state) {
        case 1: case 0x15: case 0x29:   // the timer less 1 below 4
            move_script::SetWord(ah::Mem(at::kTailTimer), on ? AH_PICK(1, 2, 3, 4, 0) : AH_PICK(5, 6, 0x10, 0xFFFF));
            break;
        case 2: case 0x16: case 0x2A:   // the timer less 1 at 0
            move_script::SetWord(ah::Mem(at::kTailTimer), on ? 1 : AH_PICK(0, 2, 0x101, 0xFFFF));
            break;
        case 0xB: Field_Request = static_cast<unsigned char>(on ? AH_PICK(0, 1, 5, 3) : 2); break;
        case 0x2E: B(at::kCounter3) = static_cast<unsigned char>(on ? 0x1E : AH_PICK(0x1D, 0x1F, 0x9E, 0)); break;
        case 0x2F: B(at::kCounter3) = static_cast<unsigned char>(on ? 0x28 : AH_PICK(0x27, 0x29, 0xA8, 0)); break;
        default: break;
        }
        break;
    }
    default: break;
    }
}
void Args52(unsigned k, std::uint32_t* a) {
    if (k == k52Cell) {
        a[0] = Key(&g_cells[0]);
        a[1] = Key(&g_cells[1]);
    } else if (k == k52Hook) {
        CellArgs(a);
    }
}

void RunArea(int area, const ah::Clone* clones, unsigned n, void (*seed)(unsigned), void (*args)(unsigned, std::uint32_t*),
             unsigned rounds) {
    static ah::Region regions[sizeof kRegions / sizeof kRegions[0]];
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) regions[i] = kRegions[i];
    regions[sizeof kRegions / sizeof kRegions[0] - 1].at = Key(g_cells);
    ah::Group g{"area_w1c", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], nullptr, 0,
                regions, sizeof regions / sizeof regions[0], seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    RunArea(48, kClones48, sizeof kClones48 / sizeof kClones48[0], &Seed48, &Args48, kRounds);
    RunArea(49, kClones49, sizeof kClones49 / sizeof kClones49[0], &Seed49, &Args49, kRounds);
    RunArea(50, kClones50, sizeof kClones50 / sizeof kClones50[0], &Seed50, &ArgsTrigger, kRounds);
    RunArea(51, kClones51, sizeof kClones51 / sizeof kClones51[0], &Seed51, nullptr, kRounds);
    RunArea(52, kClones52, sizeof kClones52 / sizeof kClones52[0], &Seed52, &Args52, kRounds);
}

}  // namespace area_w1c
