// BOF3X_SHADOW=field_e2: group FE2's 51 functions through the scenario
// harness's field mode (scenario_harness.h, docs/scenario_harness.md section
// 7), once at start-up. docs/field_e2.md section 4.
//
// The clone table is tools/band_rows.py --group FE2 --clones (2026-09-29,
// every extent read to its last instruction; the tool's, not the cut's),
// names given; 0x56D240 dropped (a case of Scena17_DrawLine, ours). Beyond the
// standard and field-standard stand-ins the group lists:
//   - its own functions another of its own calls (Records_CountUnpaired /
//     OfKind, Field_ObjectTriggerByKind, Mode11_ObjectHalt / Control);
//   - standard callees re-listed with the masks of what the callee reads,
//     where the originals push a register with stale upper bytes
//     (Sprite_EnsureAnimation, Char_LoseHp, Area_TestCondition,
//     MapView_SetElevation, Gfx_CommitPrim, Msg_SystemPtr, Menu_DrawHand,
//     Inventory_Count / Add, Item_HelpMessage, 0x594700);
//   - the GTE callees re-listed so that a pointer into the caller's stack
//     frame is not compared by value (the two sides' frames differ) and the
//     SVECTORs are hashed without their fourth word (DIV-0023), with
//     Gte_RotTransPers' screen point filled as the two floats it is;
//   - louder stand-ins where the caller branches on the answer's value:
//     AreaMap_ByteAt from a seeded grid, Input_AutoRepeat from the four
//     direction bits, WorldMap_RecordIndex below 12;
//   - Task_Sleep escaping PartySet_ErrorLoop's endless loop after one frame;
//   - one typed stand-in per entry of the tables the functions call through
//     with arguments (Scenario_CallB's call tables, Scenario_Hooks slots 1
//     and 4, Field_ObjectTriggers, WorldMap_FieldHooks), built from the
//     image at start-up.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_e2.h"
#include "game/field_e2_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

// --- PartySet_ErrorLoop's way out, outside any namespace for the asm names ------------------
//
// The original loops for ever, as ours does: the fuzz runs one frame of each.
// Task_Sleep's stand-in (SleepEscape) records its word and leaves the loop -
// on the original's side through the copy's frame (0x3C bytes above the
// stand-in's return address: fourteen pushed words and its own return
// address), on ours through OursEscapable's saved frame.
extern "C" {
std::uint32_t fe2_escape_esp asm("fe2_escape_esp") = 0;
std::uint32_t fe2_escape_pop asm("fe2_escape_pop") = 0;
void (__cdecl* fe2_escape_ours)(void) asm("fe2_escape_ours") = nullptr;
std::uint32_t __cdecl Fe2SleepTarget(std::uint32_t frames, std::uint32_t entry_esp) asm("fe2_sleep_target");
}

std::uint32_t __cdecl Fe2SleepTarget(std::uint32_t frames, std::uint32_t entry_esp) {
    scenario_harness::Record(bof3::addr::Task_Sleep, frames & 0xFFFF);
    if (scenario_harness::g_active) {
        fe2_escape_pop = 1;
        return fe2_escape_esp;
    }
    fe2_escape_pop = 0;
    return entry_esp + 0x3C;
}

namespace field_e2 {
namespace {

__attribute__((naked)) void SleepEscape() {
    asm("movl %esp, %eax\n\t"
        "pushl %eax\n\t"
        "pushl 8(%esp)\n\t"
        "call fe2_sleep_target\n\t"
        "movl %eax, %esp\n\t"
        "cmpl $0, fe2_escape_pop\n\t"
        "je 1f\n\t"
        "popl %edi\n\t"
        "popl %esi\n\t"
        "popl %ebx\n\t"
        "popl %ebp\n\t"
        "1:\n\t"
        "ret");
}
__attribute__((naked)) void OursEscapable() {
    asm("pushl %ebp\n\t"
        "pushl %ebx\n\t"
        "pushl %esi\n\t"
        "pushl %edi\n\t"
        "movl %esp, fe2_escape_esp\n\t"
        "call *fe2_escape_ours\n\t"
        "int3");
}

namespace sh = scenario_harness;
using sh::Arg;
using sh::ArgAt;
using sh::Shape;
using U = std::uint32_t;

unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char& B(U a) { return *sh::Mem(a); }
U L(U a) { return static_cast<U>(move_script::Long(sh::Mem(a))); }
void SetL(U a, U v) { move_script::SetLong(sh::Mem(a), static_cast<std::int32_t>(v)); }
std::uint16_t W(U a) { return move_script::Word(sh::Mem(a)); }
void SetW(U a, unsigned v) { move_script::SetWord(sh::Mem(a), v); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the clone table ------------------------------------------------------------------------

constexpr sh::CallSite kCalls5341E0[] = {{0x36, 0x57C140}, {0x49, 0x57C140}, {0x8B, 0x57C140}, {0x154, 0x57C0F0}, {0x177, 0x5343C0},
                                         {0x18F, 0x5343C0}, {0x199, 0x534420}, {0x1A0, 0x534420}, {0x1B2, 0x57C140}, {0x1D1, 0x57C0F0}};
constexpr sh::CallSite kCalls534480[] = {{0x46, 0x589330}, {0x63, 0x589330}, {0x80, 0x589330}, {0x9D, 0x589330},
                                         {0xB4, 0x589330}, {0xD1, 0x589330}, {0xEE, 0x589330}, {0x105, 0x589330}};
constexpr sh::CallSite kCalls534C20[] = {{0x9C, 0x537480}, {0xB4, 0x537500}, {0x14C, 0x535310}};
constexpr sh::CallSite kCalls535490[] = {{0xF, 0x536700},  {0x1D, 0x536700}, {0x2B, 0x536700},  {0x36, 0x536700}, {0x98, 0x536700},
                                         {0xA3, 0x536700}, {0xF2, 0x536700}, {0x100, 0x536700}, {0x14A, 0x536700}};
constexpr sh::CallSite kCalls535830[] = {{0x13, 0x535730},  {0x56, 0x5725C0},  {0x8F, 0x5725C0},  {0xC8, 0x5725C0},  {0x11A, 0x5725C0},
                                         {0x14F, 0x5725C0}, {0x186, 0x5725C0}, {0x1CB, 0x5725C0}, {0x1FC, 0x5725C0}, {0x254, 0x5725C0},
                                         {0x289, 0x5725C0}, {0x2C0, 0x5725C0}, {0x305, 0x5725C0}, {0x336, 0x5725C0}, {0x38F, 0x5725C0},
                                         {0x3B6, 0x572570}, {0x3F6, 0x531CF0}};
constexpr sh::CallSite kCalls535D60[] = {{0xF, 0x536700},  {0x1D, 0x536700},  {0x2B, 0x536700},  {0x36, 0x536700}, {0xB2, 0x536700},
                                         {0xBF, 0x536700}, {0x13A, 0x536700}, {0x14A, 0x536700}, {0x1C0, 0x536700}};
constexpr sh::CallSite kCalls535FC0[] = {{0xD, 0x589330}, {0x18, 0x589330}};
constexpr sh::CallSite kCalls535FE0[] = {{0x17, 0x534610}, {0x54, 0x589330}, {0x5C, 0x536670}};
constexpr sh::CallSite kCalls536050[] = {{0x15, 0x536670}, {0x3F, 0x5893A0}, {0x4C, 0x589330}, {0x60, 0x5893A0}};
constexpr sh::CallSite kCalls5360C0[] = {{0x26, 0x589330}, {0x4F, 0x589330}};
constexpr sh::CallSite kCalls536130[] = {{0x0, 0x589410}, {0x16, 0x589330}, {0x23, 0x589330}};
constexpr sh::CallSite kCalls536170[] = {{0x58, 0x572570}, {0x8D, 0x572570}, {0xC7, 0x589330}, {0xF5, 0x5725F0}, {0x106, 0x589410}};
constexpr sh::CallSite kCalls536290[] = {{0x27, 0x589330}, {0x32, 0x589330}};
constexpr sh::CallSite kCalls5362D0[] = {{0x44, 0x572570}, {0x75, 0x572570}, {0x99, 0x589330}, {0xC7, 0x5725F0}, {0xD8, 0x589410}};
constexpr sh::CallSite kCalls5363C0[] = {{0x0, 0x589410}, {0x20, 0x534610}, {0x5D, 0x589330}, {0x65, 0x536670}};
constexpr sh::CallSite kCalls536440[] = {{0x15, 0x536670}, {0x39, 0x5366A0}, {0x41, 0x5893A0}, {0x4E, 0x589330}, {0x56, 0x536650}};
constexpr sh::CallSite kCalls5364D0[] = {{0x40, 0x572570}, {0x76, 0x5893A0}};
constexpr sh::CallSite kCalls536550[] = {{0x17, 0x572570}, {0x38, 0x5891F0}, {0x53, 0x5366A0}, {0x7B, 0x5893A0}};
constexpr sh::CallSite kCalls5365D0[] = {{0x1F, 0x589330}, {0x3E, 0x5366A0}, {0x46, 0x536650}};
constexpr sh::CallSite kCalls536A60[] = {{0xE, 0x517090}, {0x35, 0x5B9380}, {0x48, 0x517090}, {0x4F, 0x5A9949}};
constexpr sh::CallSite kCalls536BF0[] = {{0xDE, 0x530480}, {0x134, 0x52E160}, {0x200, 0x5345E0}, {0x205, 0x535F50}, {0x212, 0x52E140}};
constexpr sh::CallSite kCalls536E70[] = {{0xD, 0x536EC0}, {0x17, 0x52E140}};
constexpr sh::CallSite kCalls536E90[] = {{0x13, 0x5725F0}};
constexpr sh::CallSite kCalls536EC0[] = {{0x40, 0x536BF0}};
constexpr sh::CallSite kCalls536F10[] = {{0x70, 0x5A8250},  {0x82, 0x5B9550},  {0x95, 0x5B9550},  {0xAB, 0x5A75D0},  {0xB1, 0x5A92E0},
                                         {0x19B, 0x572FA0}, {0x1A7, 0x461E50}, {0x1AF, 0x5A7B90}, {0x209, 0x5A8200}, {0x218, 0x5A8060},
                                         {0x22C, 0x5A7D70}, {0x236, 0x5A8DE0}, {0x240, 0x5A8E00}, {0x250, 0x5A77C0}, {0x271, 0x572FA0},
                                         {0x27D, 0x461E50}, {0x2C2, 0x5A7A50}, {0x2D4, 0x5A7A00}, {0x2F3, 0x5A7A50}, {0x305, 0x5A7A00},
                                         {0x324, 0x5A75F0}, {0x32C, 0x5A7780}, {0x356, 0x5A84A0}, {0x35C, 0x5A9310}, {0x399, 0x572FA0},
                                         {0x3A5, 0x461E50}, {0x3BB, 0x5A7BC0}};
constexpr sh::CallSite kCalls56D6B0[] = {{0xF, 0x56E020}};
constexpr sh::CallSite kCalls56D7A0[] = {{0x36, 0x56E670}};
constexpr sh::CallSite kCalls56D930[] = {{0x23, 0x454590}, {0x38, 0x454810}, {0x43, 0x4549F0}, {0x4B, 0x585A00},
                                         {0x57, 0x586670}, {0x71, 0x454770}, {0x86, 0x454810}, {0x98, 0x57C7A0}};
constexpr sh::JumpTable kTables56D930[] = {{0x13, 0xC0, 6}};
constexpr sh::CallSite kCalls56DA10[] = {{0x24, 0x57C7C0}, {0x3F, 0x531F90}, {0x9E, 0x594E00},  {0xBF, 0x594E00},
                                         {0xE0, 0x594E00}, {0x102, 0x57C7A0}, {0x10E, 0x57C110}, {0x11A, 0x57C110}};
constexpr sh::JumpTable kTables56DA10[] = {{0x13, 0x138, 5}, {0x89, 0x14C, 6}};
constexpr sh::CallSite kCalls56DB80[] = {{0x20, 0x57C7C0},  {0x27, 0x495040},  {0x4D, 0x533E50},  {0x52, bof3::addr::Sound_StopMusic},  {0x58, 0x587910},
                                         {0x69, 0x587A00},  {0x76, bof3::addr::Sound_ResumeAll},  {0x7D, 0x495040},  {0xB9, 0x57C7C0},  {0x11B, 0x4976D0},
                                         {0x140, 0x57C7A0}, {0x15B, 0x57C7C0}, {0x162, 0x495040}, {0x184, 0x533E50}, {0x189, bof3::addr::Sound_StopMusic},
                                         {0x18F, 0x587910}, {0x1A0, 0x587A00}, {0x1A9, bof3::addr::Sound_ResumeAll}, {0x1B0, 0x495040}};
constexpr sh::JumpTable kTables56DB80[] = {{0x1C, 0x200, 14}};
constexpr sh::CallSite kCalls56DDD0[] = {{0x18, 0x57C7A0}, {0x33, 0x57C7C0}, {0x3D, 0x4976D0}};
constexpr sh::CallSite kCalls56DE30[] = {{0x0, 0x462A90}};
constexpr sh::CallSite kCalls56DE50[] = {{0x15, 0x587A00}, {0x2F, bof3::addr::Sound_ResumeAll}, {0x3B, 0x57C7A0}, {0x6B, 0x57C0F0}, {0x84, 0x497740},
                                         {0x92, 0x5B9450}, {0x9C, 0x497710}, {0xA7, bof3::addr::Sound_StopMusic}, {0xAE, 0x587910}};
constexpr sh::CallSite kCalls56DF10[] = {{0x3F, 0x57C140}, {0x8B, 0x594E00}, {0x9A, 0x57C7A0}, {0xB8, 0x57C7A0},
                                         {0xCB, 0x57C7C0}, {0xD4, 0x5919B0}, {0xE7, 0x4976D0}};
constexpr sh::CallSite kCalls570870[] = {{0xE, 0x56FF00}, {0x5B, 0x5A75D0}, {0x63, 0x5A77A0}, {0xEE, 0x5A85F0},
                                         {0x103, 0x5A9170}, {0x15C, 0x572A00}, {0x173, 0x461E50}};
constexpr sh::CallSite kCalls570BC0[] = {{0xD, 0x56FF00},   {0x77, 0x5A77C0},  {0x84, 0x461E50},  {0x90, 0x5A7610},  {0x123, 0x5A85F0},
                                         {0x138, 0x5A9170}, {0x144, 0x5A7780}, {0x1CC, 0x461E50}, {0x1F8, 0x5A77C0}, {0x206, 0x461E50}};
constexpr sh::CallSite kCalls570DE0[] = {{0xC, 0x56FF00},   {0xCC, 0x5A7B90},  {0x135, 0x5A8200}, {0x144, 0x5A8060}, {0x158, 0x5A7D70},
                                         {0x162, 0x5A8DE0}, {0x16C, 0x5A8E00}, {0x178, 0x5A75D0}, {0x180, 0x5A77A0}, {0x230, 0x5A85F0},
                                         {0x245, 0x5A9170}, {0x251, 0x572A00}, {0x270, 0x461E50}, {0x278, 0x5A7BC0}};
constexpr sh::CallSite kCalls593960[] = {{0x1B, 0x469750}, {0x22, 0x594410}, {0x2A, 0x5947D0}, {0x2F, 0x5942C0}};
constexpr sh::CallSite kCalls5939A0[] = {{0x2, 0x495040}, {0x18, 0x594790}};
constexpr sh::CallSite kCalls5939F0[] = {{0xE, 0x5942C0}, {0x13, 0x5947D0}, {0x31, 0x5905D0}};
constexpr sh::CallSite kCalls593A30[] = {{0xD, 0x461EB0},   {0x64, 0x587740},  {0x88, 0x594700},  {0xE8, 0x5919B0},  {0x105, 0x5919B0},
                                         {0x12F, 0x587740}, {0x158, 0x587740}, {0x173, 0x587740}, {0x19E, 0x587740}, {0x1BF, 0x469750},
                                         {0x1FB, 0x591C20}, {0x201, 0x497740}, {0x212, 0x516B30}, {0x21C, 0x594410}};
constexpr sh::CallSite kCalls593C60[] = {{0x12, 0x461EB0},  {0x7E, 0x594700},  {0xE1, 0x5919B0},  {0xF9, 0x5919B0},
                                         {0x138, 0x587740}, {0x15B, 0x587740}, {0x190, 0x587740}, {0x1B1, 0x469750},
                                         {0x1D1, 0x516B30}, {0x1D8, 0x594410}, {0x1DD, 0x594AD0}, {0x1E8, 0x5905D0}};
constexpr sh::CallSite kCalls593E60[] = {{0xC, 0x461EB0},   {0x2D, 0x587740},  {0x4E, 0x587740},  {0x82, 0x587740},  {0x9D, 0x587740},
                                         {0xE3, 0x590BB0},  {0xEB, 0x594D90},  {0x10B, 0x469750}, {0x14F, 0x591680}, {0x192, 0x516B30},
                                         {0x1B8, 0x516B30}, {0x1D4, 0x5905D0}, {0x1DE, 0x594410}, {0x1EF, 0x594AD0}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define FE2_FN(name) reinterpret_cast<const void*>(&::name)
#define FE2_C(name, base, size, calls) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, FE2_FN(name)
#define FE2_L(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, FE2_FN(name)
#define FE2_T(name, base, size, calls, tables) #name, base, size, calls, SH_N(calls), nullptr, 0, tables, SH_N(tables), FE2_FN(name)
constexpr Shape kSp = Shape::kSprite, kSt = Shape::kState, kCa = Shape::kCall;
const sh::Clone kClones[] = {
    {FE2_L(Scenario_CallB, 0x5341C0, 0x1B), 0, false, Shape::kEntry},
    {FE2_C(Field_AfterBattleTally, 0x5341E0, 0x1DD, kCalls5341E0), 0, false, kSt},
    {FE2_L(Records_CountUnpaired, 0x5343C0, 0x55), 0, false, kCa, ArgAt(1, Arg::kScratch)},
    {FE2_L(Records_CountUnpairedOfKind, 0x534420, 0x57), 0, false, kCa},
    {FE2_C(Field_FaceMode11Object, 0x534480, 0x10C, kCalls534480), 0, false, kSp},
    {FE2_C(Field_FloorHurt, 0x534C20, 0x182, kCalls534C20), 0, false, kCa},
    {FE2_C(AreaMap_CellsAllWide, 0x535490, 0x172, kCalls535490), 0xFF, false, kCa},
    {FE2_C(Field_WayBlockedWide, 0x535830, 0x415, kCalls535830), 0xFF, false, kCa},
    {FE2_C(AreaMap_CellsNoneWide, 0x535D60, 0x1E6, kCalls535D60), 0xFF, false, kCa},
    {FE2_C(Leader_Pose3C, 0x535FC0, 0x1F, kCalls535FC0), 0, false, kSp},
    {FE2_C(Leader_HopStart, 0x535FE0, 0x6A, kCalls535FE0), 0, false, kSp},
    {FE2_C(Leader_HopFlight, 0x536050, 0x69, kCalls536050), 0xFF, false, kSp},
    {FE2_C(Leader_HopPose, 0x5360C0, 0x64, kCalls5360C0), 0xFF, false, kSp},
    {FE2_C(Leader_Pose3E, 0x536130, 0x31, kCalls536130), 0xFF, false, kSp},
    {FE2_C(Leader_StepUp, 0x536170, 0x115, kCalls536170), 0xFF, false, kSp},
    {FE2_C(Leader_Pose34, 0x536290, 0x39, kCalls536290), 0, false, kSp},
    {FE2_C(Leader_StepDown, 0x5362D0, 0xE5, kCalls5362D0), 0xFF, false, kSp},
    {FE2_C(Leader_HopStartAfterTick, 0x5363C0, 0x7D, kCalls5363C0), 0xFF, false, kSp},
    {FE2_C(Leader_HopFall, 0x536440, 0x8F, kCalls536440), 0xFF, false, kSp},
    {FE2_C(Leader_Rise, 0x5364D0, 0x7B, kCalls5364D0), 0xFF, false, kSp},
    {FE2_C(Leader_Sink, 0x536550, 0x80, kCalls536550), 0xFF, false, kSp},
    {FE2_C(Leader_TurnBack, 0x5365D0, 0x75, kCalls5365D0), 0, false, kSp},
    {"PartySet_ErrorLoop", 0x536A60, 0x59, kCalls536A60, SH_N(kCalls536A60), nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&OursEscapable), 0, false, kSt},
    {FE2_L(Mode11_ObjectFrame, 0x536B60, 0x2F), 0, false, kSt},
    {FE2_L(Mode11_ObjectStart, 0x536B90, 0x58), 0, false, kSp},
    {FE2_C(Mode11_ObjectControl, 0x536BF0, 0x27C, kCalls536BF0), 0, false, kSp},
    {FE2_C(Mode11_ObjectMove, 0x536E70, 0x1C, kCalls536E70), 0, false, kSp},
    {FE2_C(Mode11_ObjectEnd, 0x536E90, 0x23, kCalls536E90), 0, false, kSt},
    {FE2_C(Mode11_ObjectHalt, 0x536EC0, 0x4F, kCalls536EC0), 0, false, kSp},
    {FE2_C(Mode11_ObjectDraw, 0x536F10, 0x3C8, kCalls536F10), 0, false, kSt},
    {FE2_C(Field_ObjectTrigger, 0x56D6B0, 0x3E, kCalls56D6B0), 0, false, kCa, ArgAt(0, Arg::kSprite)},
    {FE2_C(Scenario_CellHook, 0x56D7A0, 0x57, kCalls56D7A0), 0xFFFFFFFFu, false, kCa},
    {FE2_T(FieldTail_LoadBank, 0x56D930, 0xD8, kCalls56D930, kTables56D930), 0, false, kSt},
    {FE2_T(FieldTail_DropInMove, 0x56DA10, 0x164, kCalls56DA10, kTables56DA10), 0, false, kSt},
    {FE2_T(FieldTail_HealAndMenu, 0x56DB80, 0x250, kCalls56DB80, kTables56DB80), 0, false, kSt},
    {FE2_C(FieldTail_Message, 0x56DDD0, 0x59, kCalls56DDD0), 0, false, kSt},
    {FE2_C(FieldTail_WorldMapHook, 0x56DE30, 0x11, kCalls56DE30), 0, false, kSt},
    {FE2_C(FieldTail_FlagMessage, 0x56DE50, 0xBF, kCalls56DE50), 0, false, kSt},
    {FE2_C(FieldTail_StoryWarp, 0x56DF10, 0x104, kCalls56DF10), 0, false, kSt},
    {FE2_L(Field_ObjectTriggerByKind, 0x56E020, 0x1D), 0, false, kCa, ArgAt(0, Arg::kSprite)},
    {FE2_C(MapCell_DrawFrames, 0x570870, 0x190, kCalls570870), 0, false, kCa},
    {FE2_C(MapCell_DrawShaded, 0x570BC0, 0x214, kCalls570BC0), 0, false, kCa},
    {FE2_C(MapCell_DrawSpinning, 0x570DE0, 0x2A4, kCalls570DE0), 0, false, kCa},
    {FE2_L(AreaMap_ClearCell, 0x5728D0, 0x128), 0, false, kCa},
    {FE2_C(ItemTrade_Open, 0x593960, 0x34, kCalls593960), 0, false, kSt},
    {FE2_C(ItemTrade_OpenStart, 0x5939A0, 0x2F, kCalls5939A0), 0, false, kSt},
    {FE2_L(ItemTrade_OpenWait, 0x5939D0, 0x19), 0, false, kSt},
    {FE2_C(ItemTrade_Run, 0x5939F0, 0x3A, kCalls5939F0), 0, false, kSt},
    {FE2_C(ItemTrade_PickItem, 0x593A30, 0x225, kCalls593A30), 0, false, kSt},
    {FE2_C(ItemTrade_PickCount, 0x593C60, 0x1F5, kCalls593C60), 0, false, kSt},
    {FE2_C(ItemTrade_Confirm, 0x593E60, 0x1F5, kCalls593E60), 0, false, kSt},
};
#undef FE2_T
#undef FE2_L
#undef FE2_C
#undef FE2_FN

enum : unsigned {
    kCallB, kTally, kUnpaired, kUnpairedKind, kFace, kFloor, kCellsAll, kWayBlocked, kCellsNone, kPose3C, kHopStart,
    kHopFlight, kHopPose, kPose3E, kStepUp, kPose34, kStepDown, kHopAfterTick, kHopFall, kRise, kSink, kTurnBack,
    kErrorLoop, kObjFrame, kObjStart, kObjControl, kObjMove, kObjEnd, kObjHalt, kObjDraw, kTrigger, kCellHook, kTailLoad,
    kTailDropIn, kTailHeal, kTailMessage, kTailWorld, kTailFlag, kTailWarp, kTriggerByKind, kDrawFrames, kDrawShaded,
    kDrawSpinning, kClearCell, kTradeOpen, kTradeOpenStart, kTradeOpenWait, kTradeRun, kTradePick, kTradeCount, kTradeConfirm,
    kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ----------------------------------------------------------------------

alignas(16) unsigned char g_record[0x100];   // the map-cell record the draws are handed
alignas(16) unsigned char g_grid[0x40];      // AreaMap_ByteAt's cells, 8 x 8 by the low bits of x and z
alignas(16) unsigned char g_flagcell[0x10];  // what 0x903804 points at
alignas(16) unsigned char g_ground[4];       // MapView_GroundAt's answers are near this s16

// --- the stand-ins' effects ---------------------------------------------------------------------

// What a stand-in may write: the regions or the calling thread's stack
// (scenario_harness.cpp's Writable, which is not exported).
bool OnStack(const void* p, unsigned n) {
    std::uint32_t base;
    __asm__("movl %%fs:4, %0" : "=r"(base));
    const auto here = Key(&base), at = Key(p);
    return at > here && at + n > at && at + n <= base;
}
bool Writable(U at, unsigned n) {
    const void* const p = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(at));
    return sh::InRegions(p, n) || OnStack(p, n);
}
void Fill(U at, unsigned n) {
    if (Writable(at, n)) sh::FillBytes(reinterpret_cast<void*>(static_cast<std::uintptr_t>(at)), n);
}
void Floats(U at, unsigned n) {
    // small whole numbers, as the harness's FillFloats: _ftol and x87 adds read them back
    if (!Writable(at, 4 * n)) return;
    for (unsigned i = 0; i < n; ++i) {
        const float f = static_cast<float>(static_cast<std::int16_t>(sh::Noise() >> 9));
        std::memcpy(reinterpret_cast<void*>(static_cast<std::uintptr_t>(at + 4 * i)), &f, 4);
    }
}
U FxRtp(const U* a, U answer) {
    Floats(a[1], 2);   // the screen point: two floats (Gte_RotTransPers writes both dwords)
    Fill(a[2], 4);
    return answer;
}
U FxRt(const U* a, U answer) {
    Fill(a[1], 12);
    return answer;
}
U FxRotMatrix(const U* a, U) {
    Fill(a[1], 18);
    return a[1];
}
U FxMul(const U* a, U) {
    Fill(a[2], 18);
    return a[2];
}
U FxSetTrans(const U* a, U answer) {
    // the translation Gte_SetTransMatrix reads is the matrix's +0x14, three longs
    const U t = a[0] + 0x14;
    if (Writable(t, 12)) sh::NoteBytes(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(t)), 12);
    return answer;
}
U FxRtp3(const U* a, U answer) {
    for (unsigned i = 3; i < 6; ++i) Floats(a[i], 2);
    Fill(a[6], 4);
    return answer;
}
U FxRtp4(const U* a, U answer) {
    for (unsigned i = 4; i < 8; ++i) Floats(a[i], 2);
    Fill(a[8], 4);
    return answer;
}
U FxCommit(const U* a, U answer) {
    // Gfx_PacketNext += size (a byte) while the packet stays well inside the buffer
    unsigned char* const next = sh::Pointer(scenario_harness::at::kPacketNext);
    unsigned char* const base = sh::Packets();
    const unsigned size = a[1] & 0xFF;
    if (next >= base && next + size + 0x80 <= base + 0x800) sh::SetPointer(scenario_harness::at::kPacketNext, next + size);
    return answer;
}
U FxText(const U*, U answer) { return Key(sh::Text() + (answer & 0xF0)); }
U FxGrid(const U* a, U answer) { return (answer & 0xFFFFFF00u) | g_grid[(a[0] & 7) + 8 * (a[1] & 7)]; }
U FxSlope(const U*, U answer) {
    // at the 0x40 Field_WayBlockedWide compares with (s16, > 0x40) half the time
    const U n = sh::Noise();
    if (n & 1) return answer;
    return (answer & 0xFFFF0000u) | (0x3Fu + (n >> 4) % 3u);
}
U FxGround(const U*, U answer) {
    // a height within 0xC0 of the seeded one two times in three (the callers
    // compare it with a sprite's height, and Field_WayBlockedWide with 0xC0);
    // a quarter of those at an edge: the seeded one or one either side, so a
    // ground equal to the height the seed put it at is met (control D1,
    // docs/field_e2.md section 10), or 0xC0 / 0xC1 either way, Field_WayBlockedWide's
    // bound (W2). Louder, a quarter of the time: Field_State's actor +0x89
    // moved below 12 - the step helpers read it again after this call, which
    // the group's case 0 alone reached once in 6,000 rounds (DS5)
    const U m = sh::Noise();
    if (m % 4 == 0 && sh::InRegions(Field_State + 0x89, 1)) Field_State[0x89] = static_cast<unsigned char>((m >> 8) % 12);
    const U n = sh::Noise();
    if (n % 3 == 0) return answer;
    static const U kEdges[] = {0xFFFF, 0, 1, 0xFFFF, 0, 1, 0xC0, 0xC1, 0xFF40, 0xFF3F};
    if (((n >> 2) & 3) == 0) return (answer & 0xFFFF0000u) | ((move_script::Word(g_ground) + kEdges[(n >> 8) % 10]) & 0xFFFFu);
    return (answer & 0xFFFF0000u) | ((move_script::Word(g_ground) + (n >> 8) % 0x182u - 0xC1u) & 0xFFFFu);
}
// 0x594700 (an ingredient short), louder a quarter of the time: the trade's
// pick moved inside the rows - ItemTrade_PickItem reads it again after this
// call, which the group's case 3 alone reached once in 6,000 rounds (DS8)
U FxLacks(const U*, U answer) {
    const U n = sh::Noise();
    unsigned char* const pick = Mem(field_e2::at::kTradePick);
    const unsigned rows = Mem(field_e2::at::kTradeRowCount)[0];
    if (n % 4 == 0) pick[0] = static_cast<unsigned char>((n >> 8) % (rows != 0 ? rows : 1u));
    return answer;
}
U FxRepeat(const U*, U answer) {
    static const U kMoves[] = {0, 0x1000, 0x2000, 0x4000, 0x8000, 0xA000, 0x5000, 0xF000};
    const U n = sh::Noise();
    return n % 4 == 0 ? answer : (answer & 0xFFFF0000u) | kMoves[(n >> 4) % 8];
}

// --- the callees -------------------------------------------------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag;
#define FE2_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define FE2_AT(address) "0x" #address, 0x##address, 0x##address
const sh::Callee kFixed[] = {
    // the group's own, called by another of its own
    {FE2_OURS(Records_CountUnpaired), 2, {kU8, kAll}, kG, 0, 0},
    {FE2_OURS(Records_CountUnpairedOfKind), 1, {kU8}, kG, 0, 0},
    {FE2_OURS(Field_ObjectTriggerByKind), 1, {kAll}, kG, 0, 0},
    {FE2_OURS(Mode11_ObjectHalt), 0, {}, sh::Answer::kPhase, 0, 0},
    {FE2_OURS(Mode11_ObjectControl), 0, {}, sh::Answer::kPhase, 0, 0},
    // re-listed: the callee reads a byte or a word of what the original pushes whole
    // 0x58933A cmp [+0x4B], al; 0x589200 mov dl, [esp + 4] (Sprite_SetAnimationAt)
    {FE2_OURS(Sprite_EnsureAnimation), 1, {kU8}, kF, 0, 0},
    // scena_sx.cpp: the amount's word compared and stored (the answer, the whole
    // amount, no caller of the group reads), the member's byte
    {FE2_OURS(Char_LoseHp), 2, {kU16, kU8}, kG, 0, 0},
    // map_cells.cpp: only the low 16 bits are read
    {FE2_OURS(Area_TestCondition), 1, {kU16}, kF, 0, 0},
    // map_view.cpp: the value's low word into MapView_Elevation and the u16 offset
    {FE2_OURS(MapView_SetElevation), 1, {kU16}, kG, 0, 0},
    // symbols.toml: both arguments masked to a byte
    {FE2_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, kG, 0, 0, {}, &FxCommit},
    // the (u16 id) block select and offset
    {FE2_OURS(Msg_SystemPtr), 1, {kU16}, kG, 0, 0, {}, &FxText},
    // a SPRT at (u16 x - 0x16, u16 y + 2); the third word unread
    {FE2_OURS(Menu_DrawHand), 3, {kU16, kU16, 0}, kG, 0, 0},
    // char_stats.cpp: the category's, the item's and the flag's low bytes; the
    // answer a count byte of 99 at most (a stack's count, or how many of the
    // eight records wear it) - ItemTrade_PickCount's loop never ends past 227
    {FE2_OURS(Inventory_Count), 3, {kU8, kU8, kU8}, sh::Answer::kByte, 0, 99},
    {FE2_OURS(Inventory_Add), 3, {kU8, kU8, kU8}, kF, 0, 0},
    {FE2_OURS(Item_HelpMessage), 2, {kU8, kU8}, kG, 0, 0},
    // 0x594711 and ebp, 0xFF; 0x594767 and ecx, 0xFF
    {FE2_AT(594700), 2, {kU8, kU8}, kF, 0, 0, {}, &FxLacks},
    // louder: the cells from a seeded grid, the pad's moves, a record index below 12
    {FE2_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kG, 0, 0, {}, &FxGrid},
    {FE2_OURS(Input_AutoRepeat), 1, {kAll}, kG, 0, 0, {}, &FxRepeat},
    {FE2_OURS(MapView_GroundAt), 2, {kAll, kAll}, kG, 0, 0, {}, &FxGround},
    {FE2_OURS(MapView_SlopeAt), 3, {kAll, kAll, kAll}, kG, 0, 0, {}, &FxSlope},
    {FE2_OURS(WorldMap_RecordIndex), 0, {}, sh::Answer::kByte, 0, 11},
    // not in either standard set
    {FE2_AT(537500), 2, {kU16, kU8}, kG, 0, 0},
    {FE2_OURS(Party_HealJoined), 0, {}, kG, 0, 0},
    {FE2_OURS(Sound_StopMusic), 0, {}, kG, 0, 0},
    {FE2_OURS(Field_LeaderDirection), 0, {}, kF, 0, 0},
    {FE2_OURS(Field_LeaderStepTarget), 0, {}, kF, 0, 0},
    {FE2_OURS(Field_JumpCheckHeight), 0, {}, kG, 0, 0},
    {FE2_OURS(Crt_strncpy), 3, {kAll, kAll, kAll}, sh::Answer::kThrough, 0, 0},
    {FE2_OURS(Task_Sleep), 1, {kU16}, kG, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&SleepEscape)},
    // the GTE: a pointer into the caller's frame not compared by value, an
    // SVECTOR hashed without its fourth word, the outputs written
    {FE2_OURS(Gte_RotTransPers), 3, {0, 0, 0}, kG, 0, 0, {6, 0, 0}, &FxRtp, nullptr, true},
    {FE2_OURS(Gte_RotTrans), 2, {0, 0}, kG, 0, 0, {6, 0}, &FxRt, nullptr, true},
    {FE2_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6, 0}, &FxRotMatrix, nullptr, true},
    {FE2_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {18, 18, 0}, &FxMul, nullptr, true},
    {FE2_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}, nullptr, nullptr, true},
    {FE2_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &FxSetTrans, nullptr, true},
    {FE2_OURS(Gte_RotTransPers3), 7, {kAll, kAll, kAll, kAll, kAll, kAll, 0}, kG, 0, 0, {8, 8, 8, 0, 0, 0, 0}, &FxRtp3, nullptr, true},
    {FE2_OURS(Gte_RotTransPers4), 9, {0, 0, 0, 0, kAll, kAll, kAll, kAll, 0}, kG, 0, 0, {6, 6, 6, 6, 0, 0, 0, 0, 0}, &FxRtp4, nullptr, true},
};
#undef FE2_AT
#undef FE2_OURS

// The tables the functions call through with arguments, a typed stand-in per
// entry (the handler recorder logs no arguments): Scenario_CallB's tables of
// chapters 6 and 9, Scenario_Hooks slots 1 and 4 of chapters 1, 2, 6 and 9
// (chapter 2's slot 4 is 0: not swapped), WorldMap_FieldHooks and
// Field_ObjectTriggers. The chapters the fuzz sets are these four.
constexpr U kChapters[] = {1, 2, 6, 9};
const sh::DataTable kTables[] = {
    {0x65F704, 8},  {0x65F78C, 10},                                   // Scena06_CallB, Scena09_CallB
    {0x660D6C, 1},  {0x660E54, 1},  {0x6610E4, 1},  {0x6613EC, 1},    // slot 1 of chapters 1, 2, 6, 9
    {0x660D78, 1},  {0x6610F0, 1},  {0x6613F8, 1},                    // slot 4 of chapters 1, 6, 9
    {field_e2::at::kWorldMapHooks, 12},                            // 0x662DF0..: index 11 is also 0x56E020's entry 0
    {0x662E20, 32}, {0x662EA0, 32}, {0x662F20, 1},                    // Field_ObjectTriggers' 65
    {field_e2::at::kObjectStates, 4},                                 // Mode11_ObjectStates
    {field_e2::at::kTradeOpenSteps, 2}, {field_e2::at::kTradeRunSteps, 4},
};
constexpr unsigned kCallBRows = 2, kSlot1Rows = 4, kSlot4Rows = 3, kWorldRows = 1, kTriggerRows = 3;

// kFixed and the typed entries, built at start-up from the tables as they are.
sh::Callee g_callees[400];
unsigned g_callee_n;
void AddEntry(U address, unsigned nargs) {
    for (unsigned i = 0; i < g_callee_n; ++i)
        if (g_callees[i].address == address) return;
    if (g_callee_n == sizeof g_callees / sizeof g_callees[0]) bof3::Fatal("field_e2: more than 400 stand-ins");
    sh::Callee& c = g_callees[g_callee_n++];
    c = sh::Callee{"table entry", address, address, nargs, {kAll, kAll, kAll}, kG, 0, 0};
}
void BuildCallees() {
    g_callee_n = 0;
    for (const sh::Callee& c : kFixed) g_callees[g_callee_n++] = c;
    unsigned t = 0;
    for (unsigned r = 0; r < kCallBRows; ++r, ++t)
        for (unsigned i = 0; i < kTables[t].entries; ++i) AddEntry(L(kTables[t].at + 4 * i), 3);
    for (unsigned r = 0; r < kSlot1Rows; ++r, ++t) AddEntry(L(kTables[t].at), 1);
    for (unsigned r = 0; r < kSlot4Rows; ++r, ++t) AddEntry(L(kTables[t].at), 2);
    for (unsigned r = 0; r < kWorldRows + kTriggerRows; ++r, ++t)
        for (unsigned i = 0; i < kTables[t].entries; ++i) AddEntry(L(kTables[t].at + 4 * i), 2);
}

// --- the state ------------------------------------------------------------------------------------

// Beyond the harness's standard and field regions (which hold the chapter
// bytes, Cond_Flags with the story flags, the save block 0x904098..0x904160
// and 0x904560..0x904700, ObjTrio and Field_State, the sprite records, the
// packet cursor and buffers, the area block, 0x905B70.., the menu block,
// 0x903A14.., the text scratch, Sprite_Current / Frame_Counter).
const sh::Region kRegions[] = {
    {0x9039A8, 0x50},                        // the field tail 0x9039F3..0x9039F7 and its neighbours
    {0x904700, 0x1F0},                       // the pairs' end and the eight records 0x9048B0..0x9048F0
    {0x903A94, 0x903F90 - 0x903A94},         // CharacterRecords past the standard 0x903A14 region
    {field_e2::at::kObject, 0xA4},           // the mode-11 object
    {field_e2::at::kClutWords, 0x100},       // the CLUT words of member slots 0..3
    {field_e2::at::kVertexScratch, 0x18},    // Prim_VertexScratch's three SVECTORs
    {field_e2::at::kMapCells, 0x38 * 0x1C * 2},
    {field_e2::at::kMapOrigin, 4},
    {field_e2::at::kTextRecords, 0x40},
    {0x6BE080, 0x20},                        // the trade screen's bytes 0x6BE08C..0x6BE08F
    {0x939850, 0x20},                        // its states 0x93985C / 0x93985E
    {0x903580, 0x14},                        // the button words (save data) 0x903582..0x903590
    {0x92BF18, 4},                           // Draw_OtSlot
    {0x66C7E8, 4},                           // Game_Mode, Game_Step
    {0x903800, 8},                           // 0x903804, the pointer FieldTail_FlagMessage writes through
    {0x905E20, 4},                           // Cond_ByteFE
    {Key(g_record), sizeof g_record},
    {Key(g_grid), sizeof g_grid},
    {Key(g_flagcell), sizeof g_flagcell},
    {Key(g_ground), sizeof g_ground},
};

// --- the moves -----------------------------------------------------------------------------------
//
// What the functions read again after a call that a caller could have moved:
// Field_State (to another ObjTrio record), the slope flag 0x903850, the tail
// state, the trade screen's bytes, the object's state, a CLUT slot byte.
void Move(U h) {
    const unsigned v = (h >> 8) & 0xFF, w = (h >> 16) & 0xFF;
    switch (sh::DisturbCase(h, 9)) {
    case 0: Field_State = sh::ObjectOf(v); break;
    case 1: B(field_e2::at::kEffectSlot) = static_cast<unsigned char>(v & 1 ? 0 : w); break;
    case 2: B(field_e2::at::kTailState) = static_cast<unsigned char>(v % 24); break;
    case 3: B(field_e2::at::kTradePick) = static_cast<unsigned char>(w); break;
    case 4: B(field_e2::at::kTradeQuantity) = static_cast<unsigned char>(v % 100); break;
    case 5: B(field_e2::at::kTradeStep) = static_cast<unsigned char>(v % 3); break;
    case 6: B(field_e2::at::kObject + 1) = static_cast<unsigned char>(v % 4); break;
    case 7: Sprite_Current[9] = static_cast<unsigned char>(v % 6); break;
    case 8: B(field_e2::at::kTailArg) = static_cast<unsigned char>(v % 7); break;
    default: break;
    }
}
void Disturb(U h) { Move(h); }

// --- the seed ---------------------------------------------------------------------------------------

namespace at = field_e2::at;

void SeedRecords() {
    // the eight records (kinds 4, 5, 0xB and others, a key, +3) and pairs naming them
    for (U r = 0; r < 8; ++r) {
        unsigned char* const rec = Mem(at::kRecords + 8 * r);
        rec[0] = static_cast<unsigned char>(PickOf(4, 4, 5, 0xB, sh::Next() % 12));
        rec[1] = static_cast<unsigned char>(sh::Next() % 4);
        rec[3] = static_cast<unsigned char>(sh::Half() ? 0 : 1 + sh::Next() % 0xFF);
    }
    for (U p = 0; p < 60; ++p) {
        unsigned char* const pair = Mem(at::kPairs + 8 * p);
        pair[0] = static_cast<unsigned char>(sh::Next() % 4 == 0 ? 1 : 0);
        pair[1] = static_cast<unsigned char>(PickOf(0xA, 0xB, 1 + sh::Next() % 8, sh::Next() % 12));
    }
    if (sh::Half())
        for (U p = 0; p < 60; ++p) Mem(at::kPairs + 8 * p)[0] = 0;   // no pair in use at all
}

void SeedParty() {
    // member slots 0..3 (the CLUT words' region), actor ids below 12 (the pace
    // tables) and 8 (CharacterRecords), facings 0..7
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const s = sh::SpriteRecord(i);
        s[5] = static_cast<unsigned char>(s[5] & 3);
        s[8] = static_cast<unsigned char>(PickOf(3, 7, sh::Next() % 8, sh::Next() % 8));
        s[0x70] = static_cast<unsigned char>(sh::Next() % 4);
    }
    for (unsigned i = 0; i < 3; ++i) {
        unsigned char* const o = sh::ObjectOf(i);
        o[0x89] = static_cast<unsigned char>(sh::Next() % 12);
        o[0x148] = static_cast<unsigned char>(sh::Next() % 8);
    }
    B(field_e2::at::kObject + 5) = static_cast<unsigned char>(B(field_e2::at::kObject + 5) & 3);
}

// A map-cell record in g_record for the draw k. Each is well formed: its
// byte +2 is a subrecord boundary, a frame period is not 0 and its last
// threshold is the period.
void BuildRecord(unsigned k) {
    unsigned char* const r = g_record;
    const unsigned subs = 1 + sh::Next() % 3;
    U at = 0;
    if (k == kDrawFrames) {
        at = 1;
        for (unsigned s = 0; s < subs; ++s) {
            at += 4;
            const unsigned frames = 1 + sh::Next() % 4, period = 1 + sh::Next() % 0x40;
            const U skip = (frames + 5) >> 2;
            unsigned char* const f = r + at * 4;
            f[0] = static_cast<unsigned char>(period);
            f[1] = static_cast<unsigned char>(frames);
            unsigned t = 0;
            for (unsigned i = 0; i < frames; ++i) {
                t = i + 1 == frames ? period : t + sh::Next() % 8;
                if (t > period) t = period;
                f[2 + i] = static_cast<unsigned char>(t);
            }
            at += frames + skip;
        }
    } else if (k == kDrawShaded) {
        at = 1 + 8 * subs;
    } else {
        at = 3 + 6 * subs;
    }
    r[2] = static_cast<unsigned char>(sh::Next() % 6 == 0 ? (k == kDrawSpinning ? 3 : 1) : at);
}

// The run of records AreaMap_ClearCell walks: the view cell (row, column)
// names item n, the dword before record n + base holds the count, the steps
// sum to count - 1.
void BuildRun(int row, int column) {
    const U item = 1 + sh::Next() % 0x40, base = sh::Next() % 0x100;
    SetW(at::kMapCells + 2u * static_cast<U>(column + row * 0x1C), item);
    SetW(at::kCellBase, base);
    const U n = item + base;
    const U count = 1 + sh::Next() % 6;
    SetL(at::kCellRecords - 4 + n * 4, (L(at::kCellRecords - 4 + n * 4) & 0xFFFFu) | (count << 16));
    U left = count - 1, p = at::kCellRecords + n * 4;
    while (left != 0) {
        const U step = left >= 2 && sh::Half() ? 2 : 1;
        U v = L(p) & 0x0000FFFFu;
        v |= static_cast<U>(PickOf(3, 9, 0x27, 0x30, sh::Next() % 0x40)) << 24;
        v |= step << 16;
        SetL(p, v);
        p += step * 4;
        left -= step;
    }
}

void Seed(unsigned k) {
    SeedParty();
    sh::SetPointer(field_e2::at::kStreamFlag, g_flagcell + (sh::Next() & 7));
    {
        // the ground the heights are compared with: about the sprite's height
        // plus its actor's offset (the step and hop helpers), or anywhere
        const unsigned char* const s = Sprite_Current;
        const U offset = W(field_e2::at::kPaceHeights + Field_State[0x89] * 2u);
        const U height = move_script::Word(s + 0x3E);
        move_script::SetWord(g_ground, PickOf(height + offset, height, height + 0x10 + offset, height - 0x10, sh::Next()));
    }
    B(at::kEffectSlot) = static_cast<unsigned char>(sh::Often() ? 0 : 1 + sh::Next() % 0xFF);
    switch (k) {
    case kCallB:
        Cond_ByteFA = static_cast<signed char>(PickOf(6, 9));
        break;
    case kTally: {
        Cond_ByteFA = static_cast<signed char>(PickOf(7, 8, 9, 12, sh::Next() % 20));
        B(at::kLevelA) = static_cast<unsigned char>(PickOf(0, 1, 9, 0xA, 0xB, sh::Next() % 0x20));
        B(at::kLevelB) = static_cast<unsigned char>(PickOf(3, 4, 6, 7, 8, sh::Next() % 0x10));
        B(at::kBiasA) = static_cast<unsigned char>(sh::Next() % 0x40);
        B(at::kBiasB) = static_cast<unsigned char>(sh::Next() % 0x40);
        B(at::kTallyByte) = static_cast<unsigned char>(PickOf(0x1D, 0x1E, 0x1F, 0, sh::Next() % 0x20));
        SetL(at::kTallyDword, PickOf(0, 4, 5, 0x13, 0x14, 0x27, 0x3B, sh::Next() % 0x40, sh::Next()));
        // story flags 0xF5..0xFF: set below a first clear one (or all set)
        const U first = 0xF6 + sh::Next() % 11;
        for (U f = 0xF5; f <= 0xFF; ++f) {
            unsigned char& byte = B(at::kStoryFlags + (f >> 3));
            const auto bit = static_cast<unsigned char>(1u << (f & 7));
            byte = f < first || (f > first && sh::Half()) ? static_cast<unsigned char>(byte | bit) : static_cast<unsigned char>(byte & ~bit);
        }
        if (sh::Half()) B(at::kStoryFlags + (0xF5 >> 3)) = static_cast<unsigned char>(B(at::kStoryFlags + (0xF5 >> 3)) & ~0x20);
        SeedRecords();
        break;
    }
    case kUnpaired:
    case kUnpairedKind: SeedRecords(); break;
    case kFace: {
        Field_Request = static_cast<unsigned char>(sh::Often() ? 9 : sh::Next() % 10);
        const unsigned char* const s = Sprite_Current;
        SetL(at::kObject + 0x34, static_cast<U>(move_script::Long(s + 0x34)) + PickOf(0, 0, 1, 0xFFFFFFFFu, sh::Next()));
        SetL(at::kObject + 0x38, static_cast<U>(move_script::Long(s + 0x38)) + PickOf(0, 0, 1, 0xFFFFFFFFu, sh::Next()));
        break;
    }
    case kFloor:
        for (U r = 0; r < 8; ++r) {
            const U rec = at::kCharRecords + r * at::kRecordStride;
            const U max = sh::Next() % 1000;
            SetW(rec + 0x20, max);
            SetW(rec + 0x18, PickOf(0, 1, max / 4, max / 4 + 1, max, sh::Next() % 1000));
        }
        break;
    case kCellsAll:
    case kCellsNone: {
        const auto code = static_cast<unsigned char>(PickOf(0x20, 0x21, 0xD0, 0x10, sh::Next()));
        const bool same = k == kCellsAll ? sh::Often() : !sh::Often();
        for (unsigned i = 0; i < sizeof g_grid; ++i) {
            if (same && sh::Next() % 8 != 0) g_grid[i] = code;
            else g_grid[i] = static_cast<unsigned char>(PickOf(code ^ 0x0F, code ^ 0x01, 0x20 + sh::Next() % 0x10, sh::Next()));
        }
        B(at::kEffectSlot) = code;   // Args reads the code from here (the state is taken before the arguments)
        break;
    }
    case kHopFlight:
    case kHopFall:
    case kHopPose:
        for (unsigned i = 0; i < 4; ++i) {
            unsigned char* const s = sh::SpriteRecord(i);
            s[9] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, sh::Next() % 16));
            s[0xA] = static_cast<unsigned char>(sh::Often() ? (s[9] ? s[9] - 1 : 0) : PickOf(1, 2, sh::Next() % 16));
        }
        break;
    case kStepUp:
    case kStepDown:
    case kRise:
    case kSink:
        Frame_Counter = PickOf(0, 4, sh::Next());
        B(at::kScriptFlagsLow) = static_cast<unsigned char>(sh::Half() ? B(at::kScriptFlagsLow) & ~8 : B(at::kScriptFlagsLow));
        B(at::kScriptFlags2) = static_cast<unsigned char>(sh::Half() ? B(at::kScriptFlags2) & ~8 : B(at::kScriptFlags2));
        if (sh::Half()) Sprite_Current[5] = 0;
        break;
    case kObjFrame: B(at::kObject + 1) = static_cast<unsigned char>(sh::Next() % 4); break;
    case kObjControl: {
        const U pressed = sh::Next();
        SetW(0x7E1BEC, sh::Next() % 4 == 0 ? pressed | 0x800u : pressed & ~0x800u);
        SetW(at::kButtonsLeave, sh::Often() ? 0 : sh::Next());
        SetW(at::kButtonsRun, sh::Next());
        B(at::kRunDefault) = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
        Field_Request = static_cast<unsigned char>(PickOf(5, 9, sh::Next() % 10));
        const unsigned char* const s = Sprite_Current;
        const U dir = s[8];
        SetL(at::kLeaderX, L(at::kDirectionSteps + dir * 8) + static_cast<U>(move_script::Long(s + 0x34)) +
                               PickOf(0, 0x60000, 0x60001, 0xFFFA0000u, 0xFFF9FFFFu, sh::Next() % 0x80000, sh::Next()));
        SetL(at::kLeaderZ, L(at::kDirectionSteps + 4 + dir * 8) + static_cast<U>(move_script::Long(s + 0x38)) +
                               PickOf(0, 0x60000, 0x60001, 0xFFFA0000u, sh::Next() % 0x80000, sh::Next()));
        break;
    }
    case kObjHalt: SetW(at::kInputHeld, sh::Half() ? W(at::kInputHeld) & 0x0FFFu : W(at::kInputHeld)); break;
    case kObjMove: Sprite_Current[9] = static_cast<unsigned char>(sh::Half() ? 0 : Sprite_Current[9]); break;
    case kObjEnd: B(at::kKind2Hold) = static_cast<unsigned char>(sh::Half() ? 0 : B(at::kKind2Hold)); break;
    case kObjDraw:
        B(at::kObject) = static_cast<unsigned char>(sh::Next() % 4 == 0 ? B(at::kObject) | 0x40 : B(at::kObject) & ~0x40);
        break;
    case kTrigger:
    case kCellHook:
    case kTriggerByKind:
        Cond_ByteFA = static_cast<signed char>(kChapters[sh::Next() % 4]);
        for (unsigned i = 0; i < 4; ++i) sh::SpriteRecord(i)[0x86] = static_cast<unsigned char>(sh::Next() % 66);
        break;
    case kTailLoad:
    case kTailDropIn:
    case kTailHeal:
    case kTailMessage:
    case kTailWorld:
    case kTailFlag:
    case kTailWarp: {
        static const unsigned kStates[] = {6, 5, 24, 2, 1, 2, 3};
        const unsigned states = kStates[k - kTailLoad];
        B(at::kTailState) = static_cast<unsigned char>(sh::Next() % 8 == 0 ? sh::Next() : sh::Next() % states);
        B(at::kTailArg) = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 4, 5, 6, 0xFF, sh::Next()));
        Field_Request = static_cast<unsigned char>(sh::Half() ? 2 : sh::Next() % 8);
        SetW(field_e2::at::kWaitWord, sh::Half() ? 0 : sh::Next());
        B(at::kCounterB) = static_cast<unsigned char>(PickOf(0x20, 0x31, sh::Next()));
        SetW(at::kTailTimer, PickOf(1, 2, sh::Next()));
        SetW(at::kGameMode, sh::Half() ? 2 : sh::Next());
        B(at::kCondByteFE) = static_cast<unsigned char>(sh::Half() ? 2 : sh::Next());
        break;
    }
    case kDrawFrames:
    case kDrawShaded:
    case kDrawSpinning:
        for (unsigned i = 0; i < sizeof g_record; ++i) g_record[i] = static_cast<unsigned char>(sh::Next());
        BuildRecord(k);
        break;
    case kClearCell: {
        for (U i = 0; i < 0x38 * 0x1C * 2; ++i) Mem(at::kMapCells)[i] = 0;
        // the cell's view position (c, r) in range half the time: origin words from the arguments Args will draw
        const U x = sh::Next() & 0x1F, z = sh::Next() & 0x1F;
        SetL(Key(g_flagcell), x | z << 8);   // for Args (the state is taken before the arguments)
        if (sh::Often()) {
            const int c = static_cast<int>(sh::Next() % 0x38), r = static_cast<int>(sh::Next() % 0x38);
            SetW(at::kMapOrigin, static_cast<U>(static_cast<int>(x) + 1 - c));
            SetW(at::kMapOrigin + 2, static_cast<U>(static_cast<int>(z) - r));
            if (c + r < 0x38 && c - r >= 0 && c - r < 0x38) {
                int row = c + r + static_cast<short>(W(field_e2::at::kMapRow)) + 1;
                if (row >= 0x38) row -= 0x38;
                int column = (c - r) / 2 + static_cast<short>(W(field_e2::at::kMapColumn)) + 1;
                if (column >= 0x1C) column -= 0x1C;
                if (sh::Often()) BuildRun(row, column);
            }
        }
        break;
    }
    case kTradeOpen:
    case kTradeOpenStart:
    case kTradeOpenWait:
    case kTradeRun:
    case kTradePick:
    case kTradeCount:
    case kTradeConfirm: {
        B(at::kTradeStep) = static_cast<unsigned char>(k == kTradeOpen ? sh::Next() % 2 : k == kTradeRun ? sh::Next() % 4 : sh::Next() % 4);
        const unsigned rows = 1 + sh::Next() % 10;
        B(at::kTradeRowCount) = static_cast<unsigned char>(rows);
        B(at::kTradeRow) = static_cast<unsigned char>(sh::Next() % 4);
        B(at::kTradePick) = static_cast<unsigned char>((sh::Often() ? 0 : PickOf(0x40, 0x80, 0xC0)) | (sh::Next() % rows));
        B(at::kTradeQuantity) = static_cast<unsigned char>(PickOf(0, 1, 2, 0x62, 0x63, 0x64, sh::Next() % 100));
        B(at::kTradeAnswer) = static_cast<unsigned char>(sh::Next() % 2);
        const U pressed = sh::Next() & 0xFFFF;
        SetW(0x7E1BEC, pressed);
        SetW(at::kConfirm, sh::Half() ? pressed & (1u << (sh::Next() % 16)) : 0);
        SetW(at::kCancel, sh::Half() ? pressed & (1u << (sh::Next() % 16)) : 0);
        SetW(field_e2::at::kWaitWord, sh::Half() ? 0 : sh::Next());
        break;
    }
    default: break;
    }
}

// The arguments each kCall and kEntry function reads (garbage above the bytes it masks).
void Args(unsigned k, U* a) {
    const U hi = a[9] & 0xFFFFFF00u;
    switch (k) {
    case kCallB: a[0] = hi | (Cond_ByteFA == 6 ? sh::Next() % 8 : sh::Next() % 10); break;
    case kUnpaired: a[0] = hi | Mem(at::kRecords + 1 + 8 * (sh::Next() % 8))[0]; break;
    case kUnpairedKind: a[0] = hi | PickOf(4, 5, 0xB, Mem(at::kRecords + 8 * (sh::Next() % 8))[0]); break;
    case kFloor: a[0] = hi | (sh::Next() % 9); break;
    case kCellsAll:
    case kCellsNone:
        a[0] = (sh::Next() % 0x40) << 16 | (sh::Half() ? 0 : sh::Next() & 0xFFFF);
        a[1] = (sh::Next() % 0x40) << 16 | (sh::Half() ? 0 : sh::Next() & 0xFFFF);
        a[2] = hi | B(at::kEffectSlot);
        a[3] = (a[3] & 0xFFFFFF00u) | (sh::Half() ? 0 : 1 + sh::Next() % 0xFF);
        break;
    case kWayBlocked:
        a[0] = (sh::Next() % 0x40) << 16 | (sh::Half() ? 0 : sh::Next() & 0xFFFF);
        a[1] = (sh::Next() % 0x40) << 16 | (sh::Half() ? 0 : sh::Next() & 0xFFFF);
        // the seeded ground itself half the time (FxGround's edges are about it)
        a[2] = (a[2] & 0xFFFF0000u) | ((move_script::Word(g_ground) + (sh::Half() ? 0u : sh::Next() % 0x40u - 0x20u)) & 0xFFFFu);
        break;
    case kCellHook: break;   // x, z any words
    case kDrawFrames:
    case kDrawShaded:
    case kDrawSpinning:
        a[0] = Key(g_record);
        a[1] = PickOf(0x80, 0x7F, 0x81, sh::Next() & 0xFF, sh::Next());
        a[2] = PickOf(0x80, 0x7F, 0x81, sh::Next() & 0xFF, sh::Next());
        break;
    case kClearCell: {
        const U xz = L(Key(g_flagcell));
        a[0] = (a[0] & 0xFFFF0000u) | (xz & 0xFF);
        a[1] = (a[1] & 0xFFFF0000u) | ((xz >> 8) & 0xFF);
        break;
    }
    default: break;
    }
}

// A subset for a control or a hunt (BOF3X_FE2_ONLY=first,count; the enum's
// numbering) and the rounds (BOF3X_FE2_ROUNDS); the committed run is every
// function at 6,000.
unsigned g_first;
void SeedFrom(unsigned k) { Seed(k + g_first); }
void ArgsFrom(unsigned k, U* a) { Args(k + g_first, a); }

}  // namespace

void SelfTest() {
    fe2_escape_ours = &::PartySet_ErrorLoop;
    BuildCallees();
    unsigned first = 0, count = kCount, rounds = 6000;
    if (const char* only = std::getenv("BOF3X_FE2_ONLY")) {
        char* end = nullptr;
        first = static_cast<unsigned>(std::strtoul(only, &end, 10));
        if (end && *end == ',') count = static_cast<unsigned>(std::strtoul(end + 1, nullptr, 10));
        if (first >= kCount) first = kCount - 1;
        if (count == 0 || first + count > kCount) count = kCount - first;
    }
    if (const char* r = std::getenv("BOF3X_FE2_ROUNDS")) rounds = static_cast<unsigned>(std::strtoul(r, nullptr, 10));
    g_first = first;
    sh::Group group = {"field_e2", kClones + first, count, g_callees, g_callee_n,
                       kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                       &SeedFrom, &Disturb, rounds};
    group.args = &ArgsFrom;
    group.field = true;
    group.chapter = 6;
    sh::Run(group);
}

}  // namespace field_e2
