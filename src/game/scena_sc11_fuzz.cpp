// BOF3X_SHADOW=scena_sc11: scenario chapter 11 through the scenario round's
// harness (scenario_harness.h), once at start-up. docs/scena_sc11.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC11 --clones, 222eb0f, held
// against the group's own recursive descent: the same thirty functions,
// extents, calls and jump tables), every callee the functions call, the three
// .data dispatch tables, the regions the chapter writes beyond the harness's
// standard ones, a seed per function that puts its switch's steps and each
// comparison's constants in, the arguments of the three that take any, and a
// disturbance of the chapter's cells.
//
// Chapter 11 on the group (Cond_ByteFA and its flag row every round); 0
// mismatches in 240,000 rounds, 54 controls refused (docs/scena_sc11.md
// sections 4 and 7).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc11.h"
#include "game/scena_sc11_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc11 {
namespace {

namespace sh = scenario_harness;
using move_script::SetLong;

template <typename T> std::uint32_t KeyOf(T p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
#define SC11_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define SC11_THEIRS(name) #name, KeyOf(name), KeyOf(name)
#define SC11_RAW(label, address) label, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// --- the clone table ------------------------------------------------------------
//
// tools/scenario_rows.py --unit SC11 --clones (222eb0f), 2026-09-27: every
// jump internal, nothing REFUSED; the switches' tables bounded at their cmp.
// Each clone's call shape: a vtable slot (void, no arguments), the object
// slot (the object), a hook (x, z) answering in al, or a state handler
// (Scena11_States / Scena11_Runs / Scena11_Triggers entry: void, no
// arguments; the triggers are pushed (object, flags) and read neither).

// 0x55C040: 0xE bytes; +0x7 jmp through .data 0x66166C (Scena11_States); vtable slot 0
// 0x55C050: 0xF bytes; state handler (Scena11_States 0)
// 0x55C060: 0x2F4 bytes; state handler (Scena11_States 1)
constexpr sh::CallSite kCalls55C060[] = {{0x43, 0x57C140}, {0x5C, 0x57C140}, {0x6C, 0x57C7C0}, {0xB6, 0x5341C0}, {0xBC, 0x5341C0}, {0xC2, 0x5341A0}, {0xCE, 0x5341C0}, {0xE0, 0x5341C0}, {0xEA, 0x5341C0}, {0xF1, 0x5341C0}, {0xF7, 0x5341A0}, {0x106, 0x5341C0}, {0x111, 0x5341C0}, {0x118, 0x5341A0}, {0x1B7, 0x5725F0}, {0x227, 0x589810}, {0x257, 0x5725F0}, {0x26A, 0x5341C0}};
constexpr sh::JumpTable kTables55C060[] = {{0xB0, 0x2C4, 12}};
// 0x55C360: 0xE bytes; +0x7 jmp through .data 0x661678 (Scena11_Runs); state handler (Scena11_States 2)
// 0x55C370: 0x3DD bytes; state handler (Scena11_Runs 1)
constexpr sh::CallSite kCalls55C370[] = {{0x4B, 0x531F90}, {0x57, 0x531F90}, {0x63, 0x531F90}, {0x7C, 0x495040}, {0xE2, 0x594E00}, {0x137, 0x594E00}, {0x187, 0x57C0F0}, {0x1A0, 0x594E00}, {0x1E0, 0x594E00}, {0x1EC, 0x5734F0}, {0x1F6, 0x5725F0}, {0x236, 0x495040}, {0x290, 0x57C0F0}, {0x2A9, 0x594E00}, {0x2CA, 0x57C0F0}, {0x2D8, 0x57C0F0}, {0x2E0, 0x57C7A0}, {0x320, 0x57C7A0}, {0x337, 0x4976D0}};
constexpr sh::JumpTable kTables55C370[] = {{0x1C, 0x350, 20}};
// 0x55C750: 0x73 bytes; state handler (Scena11_Runs 2)
constexpr sh::CallSite kCalls55C750[] = {{0x20, 0x57C0F0}, {0x28, 0x57C7A0}, {0x63, 0x531F90}};
// 0x55C7D0: 0x1A5 bytes; state handler (Scena11_Runs 3)
constexpr sh::CallSite kCalls55C7D0[] = {{0x37, 0x531F90}, {0x5D, 0x57C0F0}, {0x65, 0x57C7A0}, {0x82, 0x5919B0}, {0x93, 0x5919B0}, {0xA4, 0x5919B0}, {0xB5, 0x5919B0}, {0xD6, 0x4976D0}, {0x100, 0x57C7A0}, {0x118, 0x531F90}, {0x13B, 0x57C7A0}, {0x148, 0x57C0F0}};
constexpr sh::JumpTable kTables55C7D0[] = {{0x1C, 0x174, 8}};
// 0x55C980: 0x2C8 bytes; state handler (Scena11_Runs 4)
constexpr sh::CallSite kCalls55C980[] = {{0x2E, 0x589810}, {0x7A, 0x5B93D2}, {0xD9, 0x55CC50}, {0xFC, 0x534DB0}, {0x105, 0x537480}, {0x10F, 0x497710}, {0x145, 0x590BB0}, {0x154, 0x591680}, {0x17E, 0x497710}, {0x197, 0x4976D0}, {0x1C2, 0x587740}, {0x1E6, 0x587740}, {0x1ED, 0x4976D0}, {0x217, 0x590BB0}, {0x229, 0x57C7A0}};
constexpr sh::JumpTable kTables55C980[] = {{0x1D, 0x258, 9}, {0xBB, 0x2B0, 6}};
// 0x55CC50: 0x97 bytes; called by Scena11_Scene4 (E8), one argument (the slot, u8)
constexpr sh::CallSite kCalls55CC50[] = {{0x18, 0x589590}, {0x7B, 0x5891F0}};
// 0x55CCF0: 0x583 bytes; state handler (Scena11_Runs 5)
constexpr sh::CallSite kCalls55CCF0[] = {{0x39, 0x531F90}, {0x5F, 0x587B40}, {0x6D, 0x57C0F0}, {0x8A, 0x594E00}, {0xD2, 0x531F90}, {0x10A, 0x589810}, {0x137, 0x589810}, {0x19E, 0x587740}, {0x1A5, 0x587B40}, {0x1C0, 0x589810}, {0x262, 0x587910}, {0x273, 0x587A00}, {0x282, 0x4976D0}, {0x2BF, 0x594E00}, {0x2F4, 0x587B40}, {0x2FB, 0x531F90}, {0x30E, 0x587910}, {0x31F, 0x587A00}, {0x360, 0x57C0F0}, {0x376, 0x594E00}, {0x39C, 0x590BB0}, {0x3AB, 0x591680}, {0x3D4, 0x4976D0}, {0x3EE, 0x4976D0}, {0x3FF, 0x587740}, {0x43B, 0x57C7A0}, {0x466, 0x57C7A0}, {0x48A, 0x531F90}, {0x4B4, 0x587B40}, {0x4CA, 0x594E00}};
constexpr sh::JumpTable kTables55CCF0[] = {{0x1C, 0x4F8, 24}};
// 0x55D280: 0x475 bytes; state handler (Scena11_Runs 6)
constexpr sh::CallSite kCalls55D280[] = {{0x35, 0x531F90}, {0x93, 0x57C0F0}, {0xAC, 0x594E00}, {0xC4, 0x57C7C0}, {0xD8, 0x495040}, {0xDF, 0x587B40}, {0x10C, 0x4DF820}, {0x145, 0x594E00}, {0x18C, 0x57C0F0}, {0x1B8, 0x594E00}, {0x1E3, 0x594E00}, {0x208, 0x587910}, {0x218, 0x587A00}, {0x23C, 0x587910}, {0x24C, 0x587A00}, {0x27C, 0x537480}, {0x2A4, 0x4DF820}, {0x2C3, 0x587B40}, {0x2D8, 0x495040}, {0x2FF, 0x4976D0}, {0x33A, 0x57C0F0}, {0x353, 0x594E00}, {0x35F, 0x587AE0}, {0x371, 0x587B80}, {0x378, 0x587910}, {0x388, 0x587A00}, {0x391, 0x587B90}, {0x3B0, 0x57C0F0}, {0x3B8, 0x57C7A0}};
constexpr sh::JumpTable kTables55D280[] = {{0x1B, 0x3DC, 26}};
// 0x55D700: 0x8F4 bytes; state handler (Scena11_Runs 8)
constexpr sh::CallSite kCalls55D700[] = {{0x27, 0x57C0F0}, {0x3D, 0x594E00}, {0x49, 0x5734F0}, {0x62, 0x589810}, {0xEF, 0x589810}, {0x180, 0x589810}, {0x21E, 0x589810}, {0x28A, 0x495040}, {0x2BB, 0x4976D0}, {0x306, 0x594E00}, {0x346, 0x594E00}, {0x355, 0x587740}, {0x360, 0x5734F0}, {0x378, 0x589810}, {0x3DA, 0x572650}, {0x3E2, 0x589810}, {0x43A, 0x4976D0}, {0x461, 0x495040}, {0x4A4, 0x589810}, {0x54E, 0x589810}, {0x5DE, 0x589810}, {0x6AF, 0x57C0F0}, {0x6B9, 0x587740}, {0x6D2, 0x594E00}, {0x73A, 0x57C0F0}, {0x753, 0x594E00}, {0x75F, 0x5734F0}, {0x77F, 0x495040}, {0x7D7, 0x57C0F0}, {0x7F0, 0x594E00}, {0x7FC, 0x5734F0}, {0x81F, 0x57C0F0}, {0x827, 0x57C7A0}};
constexpr sh::JumpTable kTables55D700[] = {{0x14, 0x854, 40}};
// 0x55E000: 0x168 bytes; state handler (Scena11_Runs 9)
constexpr sh::CallSite kCalls55E000[] = {{0x2D, 0x531F90}, {0x52, 0x57C0F0}, {0x5A, 0x57C7A0}, {0x96, 0x531F90}, {0xC6, 0x495040}, {0xE8, 0x4976D0}, {0x114, 0x56D6F0}, {0x136, 0x594E00}};
constexpr sh::JumpTable kTables55E000[] = {{0x13, 0x140, 10}};
// 0x55E170: 0x1F bytes; +0x14 call through .data 0x6616A0 (Scena11_Triggers); vtable slot 1, the object
// 0x55E190..0x55E420: state handlers (Scena11_Triggers 1..14)
constexpr sh::CallSite kCalls55E190[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}, {0x3F, 0x57C0F0}};
constexpr sh::CallSite kCallsSet40[] = {{0x0, 0x57C7C0}};   // each of 0x55E1E0..0x55E420
// 0x55E450: 0x80 bytes; vtable slot 3, the arrive hook (x, z), al
constexpr sh::CallSite kCalls55E450[] = {{0x12, 0x57C140}, {0x3A, 0x57C7C0}, {0x4E, 0x57C7C0}, {0x69, 0x57C7C0}};
// 0x55E4D0: 0x3 bytes; vtable slot 4 (x, z), al

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define SC11_CLONE(name, base, size, calls, tables) \
    {#name, base, size, calls, SH_N(calls), nullptr, 0, tables, SH_N(tables), reinterpret_cast<const void*>(&::name), 0, false, sh::Shape::kState}
#define SC11_CALLS(name, base, size, calls) \
    {#name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, sh::Shape::kState}
#define SC11_PLAIN(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, sh::Shape::shape}

const sh::Clone kClones[] = {
    SC11_PLAIN(Scena11_Frame, 0x55C040, 0xE, kSlot),                                // 0  vtable slot 0
    SC11_PLAIN(Scena11_Start, 0x55C050, 0xF, kState),                               // 1  state 0
    SC11_CLONE(Scena11_EnterArea, 0x55C060, 0x2F4, kCalls55C060, kTables55C060),   // 2  state 1
    SC11_PLAIN(Scena11_Run, 0x55C360, 0xE, kState),                                       // 3  state 2
    SC11_CLONE(Scena11_Scene1, 0x55C370, 0x3DD, kCalls55C370, kTables55C370),      // 4  run 1
    SC11_CALLS(Scena11_Scene2, 0x55C750, 0x73, kCalls55C750),                       // 5  run 2
    SC11_CLONE(Scena11_Scene3, 0x55C7D0, 0x1A5, kCalls55C7D0, kTables55C7D0),      // 6  run 3
    SC11_CLONE(Scena11_Scene4, 0x55C980, 0x2C8, kCalls55C980, kTables55C980),      // 7  run 4
    {"Scena11_EffectAnimate", 0x55CC50, 0x97, kCalls55CC50, SH_N(kCalls55CC50), nullptr, 0, nullptr, 0,   // 8  scene 4's E8, one word (slot)
     reinterpret_cast<const void*>(&::Scena11_EffectAnimate), 0, false, sh::Shape::kEntry},
    SC11_CLONE(Scena11_Scene5, 0x55CCF0, 0x583, kCalls55CCF0, kTables55CCF0),      // 9  run 5
    SC11_CLONE(Scena11_Scene6, 0x55D280, 0x475, kCalls55D280, kTables55D280),      // 10 run 6
    SC11_CLONE(Scena11_Scene8, 0x55D700, 0x8F4, kCalls55D700, kTables55D700),      // 11 run 8
    SC11_CLONE(Scena11_Scene9, 0x55E000, 0x168, kCalls55E000, kTables55E000),      // 12 run 9
    {"Scena11_ObjectTrigger", 0x55E170, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0,  // 13 vtable slot 1
     reinterpret_cast<const void*>(&::Scena11_ObjectTrigger), 0, false, sh::Shape::kObject},
    SC11_CALLS(Scena11_Trigger01, 0x55E190, 0x48, kCalls55E190),                    // 14 trigger 1
    SC11_CALLS(Scena11_Trigger02, 0x55E1E0, 0x23, kCallsSet40),                     // 15..27 triggers 2..14
    SC11_CALLS(Scena11_Trigger03, 0x55E210, 0x23, kCallsSet40),
    SC11_CALLS(Scena11_Trigger04, 0x55E240, 0x25, kCallsSet40),
    SC11_CALLS(Scena11_Trigger05, 0x55E270, 0x23, kCallsSet40),
    SC11_CALLS(Scena11_Trigger06, 0x55E2A0, 0x23, kCallsSet40),
    SC11_CALLS(Scena11_Trigger07, 0x55E2D0, 0x23, kCallsSet40),
    SC11_CALLS(Scena11_Trigger08, 0x55E300, 0x25, kCallsSet40),
    SC11_CALLS(Scena11_Trigger09, 0x55E330, 0x25, kCallsSet40),
    SC11_CALLS(Scena11_Trigger10, 0x55E360, 0x25, kCallsSet40),
    SC11_CALLS(Scena11_Trigger11, 0x55E390, 0x2A, kCallsSet40),
    SC11_CALLS(Scena11_Trigger12, 0x55E3C0, 0x23, kCallsSet40),
    SC11_CALLS(Scena11_Trigger13, 0x55E3F0, 0x25, kCallsSet40),
    SC11_CALLS(Scena11_Trigger14, 0x55E420, 0x25, kCallsSet40),
    {"Scena11_ArriveHook", 0x55E450, 0x80, kCalls55E450, SH_N(kCalls55E450), nullptr, 0, nullptr, 0,   // 28 slot 3
     reinterpret_cast<const void*>(&::Scena11_ArriveHook), 0xFF, false, sh::Shape::kHook},
    {"Scena11_CellHook", 0x55E4D0, 0x3, nullptr, 0, nullptr, 0, nullptr, 0,         // 29 slot 4
     reinterpret_cast<const void*>(&::Scena11_CellHook), 0xFF, false, sh::Shape::kHook},
};
constexpr unsigned kClonesN = sizeof kClones / sizeof kClones[0];
enum : unsigned {
    kFrame, kStart, kEnterArea, kRun, kScene1, kScene2, kScene3, kScene4, kEffectAnimate, kScene5, kScene6, kScene8,
    kScene9, kObjectTrigger, kTrigger01, kTrigger14 = kTrigger01 + 13, kArriveHook, kCellHook,
};
static_assert(kCellHook + 1 == sizeof kClones / sizeof kClones[0], "the clone indices");

// --- the callees --------------------------------------------------------------------
//
// Every callee the thirty call, listed here (registered before the standard
// set, so this listing stands whatever the harness's standard set holds).

std::uint8_t g_name[16];   // what Item_NamePtr's stand-in answers: a name record of the fuzz's own

// Item_NamePtr: the callers copy 16 bytes from the answer - a buffer of the
// fuzz's own, filled from the recorders' stream.
std::uint32_t NameEffect(const std::uint32_t*, std::uint32_t) {
    sh::FillBytes(g_name, sizeof g_name);
    return KeyOf(g_name);
}

// A .data table's handler recorder logs no arguments, and Scena11_ObjectTrigger
// passes two (the object, the flag row): while it is fuzzed, every entry of
// Scena11_Triggers is this stand-in of the entry's own type (the seed writes
// it, and the recorders back for every other function, which reach the table
// through Scena11_Runs with no arguments). scena_sc0_fuzz.cpp's ObjectEntry
// is the precedent.
// One per entry, so the log says which entry the trigger called.
template <unsigned I> void __cdecl TriggerEntry(unsigned char* object, unsigned char* row) {
    sh::Record(0x55E170, I, KeyOf(object), KeyOf(row));
    sh::Stir();
}
using TriggerFn = void (__cdecl*)(unsigned char*, unsigned char*);
const TriggerFn kTriggerEntries[] = {
    &TriggerEntry<0>, &TriggerEntry<1>, &TriggerEntry<2>, &TriggerEntry<3>, &TriggerEntry<4>,
    &TriggerEntry<5>, &TriggerEntry<6>, &TriggerEntry<7>, &TriggerEntry<8>, &TriggerEntry<9>,
    &TriggerEntry<10>, &TriggerEntry<11>, &TriggerEntry<12>, &TriggerEntry<13>, &TriggerEntry<14>,
};

const sh::Callee kCallees[] = {
    // the flag bits (the pointer read from 0x929ED0 is the same on both sides)
    {SC11_OURS(Flags_Test), 2, {kAll, kAll}, sh::Answer::kFlag, 0, 0},
    {SC11_OURS(Flags_Set), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(ScriptFlags_Set40), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(ScriptFlags_Clear40), 0, {}, sh::Answer::kGarbage, 0, 0},
    // the chapter's call tables (entry n & 0xFF, no other argument read: chapter 11's entries take none)
    {SC11_OURS(Scenario_CallA), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Scenario_CallB), 1, {kU8}, sh::Answer::kGarbage, 0, 0},   // ours since round twelve group FE2
    // the field
    {SC11_OURS(Field_ChangeArea), 4, {kAll, kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Party_DropIn), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Transition_Start), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(MapView_SetElevation), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Kind2_Place), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, sh::Answer::kGarbage, 0, 0},   // answer unread
    {SC11_OURS(Port_DroppedCall), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC11_RAW("0x534DB0", kSpriteSetUp), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC11_RAW("0x537480", kCountDown), 2, {kU16, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC11_RAW("0x56D6F0", kStatusBit80), 0, {}, sh::Answer::kGarbage, 0, 0},
    // effects and sprites: a slot of the 20 records or none (0xFF)
    {SC11_OURS(Effect_FindFree), 0, {}, sh::Answer::kByte, 0xFF, 0x13},
    {SC11_OURS(Sprite_SetAnimationBank), 1, {kU16}, sh::Answer::kGarbage, 0, 0},   // answer unread
    {SC11_OURS(Sprite_SetAnimation), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Scena11_EffectAnimate), 1, {kU8}, sh::Answer::kGarbage, 0, 0},     // ours, called by its E8
    {SC11_THEIRS(Rand), 0, {}, sh::Answer::kRand, 0, 0},
    // messages
    {SC11_OURS(Msg_OpenScript), 1, {kU16}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Msg_OpenSystem), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    // the inventory: Inventory_Count's u16 is tested whole (test ax, ax), Inventory_Add's al
    {SC11_OURS(Inventory_Count), 3, {kAll, kAll, kAll}, sh::Answer::kBool, 0, 0},
    {SC11_OURS(Inventory_Add), 3, {kAll, kAll, kAll}, sh::Answer::kFlag, 0, 0},   // a fourth word pushed, unread
    {SC11_OURS(Item_NamePtr), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, &NameEffect},
    // sound and music: Sound_StreamDone's eax is tested whole
    {SC11_OURS(Sound_PlayEffect), 1, {kU16}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Music_FadeOutStop), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Music_Play), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Sound_LoadStream), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC11_OURS(Sound_StreamDone), 0, {}, sh::Answer::kBool, 0, 0},
    {SC11_THEIRS(Sound_ResumeAll), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC11_RAW("0x587B80", kSoundJmp), 0, {}, sh::Answer::kGarbage, 0, 0},
    // Scena11_Triggers' entries while Scena11_ObjectTrigger is fuzzed: the
    // entry's index, the object and the row logged (below)
    {"Scena11_Triggers[] (keyed on 0x55E170)", 0x55E170, 0x55E170, 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&TriggerEntry<0>)},
};

// --- the .data tables: swapped for recorders while the fuzz runs --------------------
//
// Scena11_Runs and Scena11_Triggers are back to back, so a MoveScript_Var7 of
// 10..24 reaches a trigger's recorder through the run table, as in the game.
const sh::DataTable kTables[] = {{0x66166C, 3}, {0x661678, 10}, {0x6616A0, 15}};

// --- the state ---------------------------------------------------------------------------

constexpr std::uint32_t kState = 0x8034E2, kVar7 = 0x8034E4, kStep = 0x8034E5;
constexpr std::uint32_t kCounters = 0x903848;
constexpr std::uint32_t kEffects = 0x7E11E0;      // Effect_Objects: 20 records of 0x80
constexpr unsigned kEffectCount = 20;

// What the chapter writes or reads beyond the harness's 22 standard regions
// (docs/scenario_harness.md section 4: the chapter bytes, the camera, the
// counters and slot, the flag rows and row pointer, Field_ScriptFlags,
// Sprite_Current and MoveScript_F3Divisor, Draw_PassFlags, ObjTrio,
// Field_State, the effect records, Field_Kind2X / Z and MapView_Redraw, the
// request, the wait word and the area are all standard).
const sh::Region kRegions[] = {
    {0x904CD0, 0x20},           // the byte 0x904CD0 and Text_Records (16 bytes at 0x904CE0)
    {0x904EE0, 1},              // set 0xFF with one area change
    {0x90412C, 1},              // the party byte of area 0x65
    {0x905E20, 1},              // Cond_ByteFE
    {0x903802, 2},              // Camera_ShiftY
    {0x929F12, 2},              // Field_Kind2Hold (the standard view focus starts at 0x929F14)
};
constexpr std::uint32_t kTriggers = 0x6616A0;
constexpr unsigned kTriggerCount = 15;
std::uint32_t g_trigger_recorders[kTriggerCount];   // the harness's handler recorders in Scena11_Triggers
bool g_have_recorders = false;

unsigned char* Mem(std::uint32_t a) { return sh::Mem(a); }
unsigned char& B(std::uint32_t a) { return sh::Mem(a)[0]; }

// The steps each scene's switch holds (the next one after a case too, and one
// past the switch's bound, which does nothing).
constexpr std::uint8_t kSteps1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 0x31, 0x32, 0x33, 0x34, 0x3C, 0x3D};
constexpr std::uint8_t kSteps2[] = {0, 1, 2};
constexpr std::uint8_t kSteps3[] = {0, 1, 2, 9, 10, 11, 12, 13, 15, 16, 17};
constexpr std::uint8_t kSteps4[] = {0, 1, 2, 3, 5, 6, 7, 0x13, 0x14, 0x15, 0x16, 0x32, 0x33};
constexpr std::uint8_t kSteps5[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 0x28, 0x29, 0x2A, 0x2B};
constexpr std::uint8_t kSteps6[] = {0, 1, 2, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 20, 21, 30, 31, 32, 33, 34, 35, 36, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49};
constexpr std::uint8_t kSteps8[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 27, 28, 29, 31, 33, 34, 35, 36, 37, 38, 39, 40};
constexpr std::uint8_t kSteps9[] = {0, 1, 2, 5, 6, 7, 8, 9, 10};
// The counter 0 values the scenes wait on, and the others'.
constexpr std::uint8_t kCounter0s[] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 0xA, 0xF, 0x14, 0x15, 0x16, 0xFF};
constexpr std::uint8_t kCounter2s[] = {0, 1, 2, 3, 4, 10};
// MoveScript_Var7: the scenes, the two rets, and the three values past the run
// table the triggers set (10, 11, 14 reach Scena11_Triggers' entries 0, 1, 4).
constexpr std::uint8_t kVar7s[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14};
constexpr std::uint16_t kAreas[] = {0x65, 0x79, 0x82, 0x83, 0x84, 0x88, 0x2D, 0x41, 0x57, 0x10, 0x73, 0x44, 0x9C, 0};

template <typename T, unsigned N> T PickOf(const T (&v)[N]) { return v[sh::Next() % N]; }

// The arrive hook's x: its boundary 0x160000, either side, or anything.
std::uint32_t HookX() {
    switch (sh::Next() % 4) {
    case 0: return 0x160000;
    case 1: return 0x160001;
    case 2: return 0x160000 - (sh::Next() & 0xFFFF);
    default: return sh::Next();
    }
}

unsigned g_k;   // the clone being seeded, for Disturb

void Seed(unsigned k) {
    g_k = k;
    // Scena11_Triggers: the typed stand-in while slot 1 is fuzzed, the
    // harness's recorders otherwise (kept from the first round, when the
    // harness has just put them there)
    if (!g_have_recorders) {
        for (unsigned i = 0; i < kTriggerCount; ++i)
            g_trigger_recorders[i] = static_cast<std::uint32_t>(move_script::Long(Mem(kTriggers + 4 * i)));
        g_have_recorders = true;
    }
    for (unsigned i = 0; i < kTriggerCount; ++i)
        SetLong(Mem(kTriggers + 4 * i), static_cast<std::int32_t>(k == kObjectTrigger ? KeyOf(kTriggerEntries[i]) : g_trigger_recorders[i]));
    // the chapter's bytes, by the values each comparison names
    B(kState) = static_cast<unsigned char>(sh::Next() % 3);
    B(kVar7) = static_cast<unsigned char>(sh::Often() ? PickOf(kVar7s) : sh::Next() % 25);
    B(kCounters) = PickOf(kCounter0s);
    if (sh::Half()) B(kCounters + 2) = PickOf(kCounter2s);
    B(kCounters + 3) = static_cast<unsigned char>(sh::Next() % kEffectCount);   // the slot a scene waits on
    Mem(kEffects + 0x80u * B(kCounters + 3))[0] = static_cast<unsigned char>(sh::Next() % 3 == 0 ? 0 : 1);
    if (sh::Half()) Field_Request = PickOf<std::uint8_t, 3>({0, 2, 8});
    if (sh::Half()) MoveScript_WaitWordDA = 0;
    if (sh::Half()) Field_Kind2Hold = 0;
    if (sh::Often()) Game_AreaNumber = PickOf(kAreas);
    // the party byte of area 0x65 (7..18, with its top bit sometimes)
    if (sh::Often()) B(0x90412C) = static_cast<unsigned char>(6 + sh::Next() % 14 + (sh::Half() ? 0x80 : 0));
    // member 0's bytes: +8 (0, 6, 7 and others), +0x89 (6, 7 and others)
    if (sh::Often()) B(0x802D48) = PickOf<std::uint8_t, 5>({0, 6, 7, 1, 8});
    if (sh::Often()) B(0x802DC9) = PickOf<std::uint8_t, 4>({6, 7, 5, 8});
    // the step, by the function's switch
    switch (k) {
    case kScene1:
        B(kStep) = PickOf(kSteps1);
        if (sh::Half()) B(kCounters) = PickOf<std::uint8_t, 4>({1, 2, 3, 0x14});   // the values its steps wait on
        break;
    case kScene2: B(kStep) = PickOf(kSteps2); break;
    case kScene3: B(kStep) = PickOf(kSteps3); break;
    case kScene4:
        B(kStep) = PickOf(kSteps4);
        sh::SetRandHint(sh::Next() & 0xF);
        if (sh::Half()) {   // the roll's step and the item step, with member 0's +0x89 they test
            B(kStep) = sh::Half() ? 0 : 0x14;
            B(0x802DC9) = sh::Often() ? (B(kStep) ? 7 : 6) : 5;
        }
        break;
    case kScene5: B(kStep) = PickOf(kSteps5); break;
    case kScene6:
        B(kStep) = PickOf(kSteps6);
        if (sh::Half()) B(kCounters + 3) = static_cast<unsigned char>(1 + sh::Next() % 2);   // step 20 tests it for 1
        break;
    case kScene8: B(kStep) = PickOf(kSteps8); break;
    case kScene9: B(kStep) = PickOf(kSteps9); break;
    case kEnterArea:
        if (sh::Often()) Game_AreaNumber = PickOf(kAreas);
        B(kCounters + 2) = PickOf(kCounter2s);
        if (sh::Half()) {   // area 0x65's start: counter 2 at 0, the party byte 7..18 (its top bit too)
            Game_AreaNumber = 0x65;
            B(kCounters + 2) = 0;
            B(0x90412C) = static_cast<unsigned char>(6 + sh::Next() % 14 + (sh::Half() ? 0x80 : 0));
        } else if (sh::Half()) {   // area 0x83's counter 1: the elevation, then the area read afresh
            Game_AreaNumber = 0x83;
            B(kCounters + 2) = 1;
        }
        break;
    case kObjectTrigger: sh::SpriteRecord(0)[0x86] = static_cast<std::uint8_t>(sh::Next() % kTriggerCount); break;
    case kArriveHook:
        if (sh::Often()) Game_AreaNumber = 0x79;
        break;
    default: break;
    }
    if (sh::Next() % 8 == 0) B(kStep) = static_cast<unsigned char>(sh::Next());   // any step, now and then
}

// The arguments: the object for slot 1, (x, z) for the hooks, the slot for
// Scena11_EffectAnimate; nothing else reads its arguments.
void Args(unsigned k, std::uint32_t* a) {
    if (k == kObjectTrigger) a[0] = KeyOf(sh::SpriteRecord(0));
    if (k == kArriveHook || k == kCellHook) a[0] = HookX();
    if (k == kEffectAnimate) a[0] = (sh::Next() & ~0xFFu) | (sh::Next() % kEffectCount);
}

// After a call, two in three: a counter byte (0 or one of the values waited
// on), the request, the wait word, Field_Kind2Hold, the slot byte 0x903850
// (a record 0..19), the area, or the live byte of the
// effect counter 3 names. Drawn from the hash given, never the harness's Next.
void Disturb(std::uint32_t h) {
    // Scena11_EnterArea: the area moved half the time (what its fresh reads see)
    switch (g_k == kEnterArea && (h & 0x800) ? 6u : (h >> 8) % 8) {
    case 5: B(0x903850) = static_cast<unsigned char>((h >> 12) % kEffectCount); break;   // the slot byte, read back after Rand
    case 6: {   // the area, read afresh by Scena11_EnterArea after its calls
        static const std::uint16_t kMoved[] = {0x83, 0x84, 0x88, 0x73, 0x10, 0x79};
        Game_AreaNumber = kMoved[(h >> 12) % 6];
        break;
    }
    case 0: B(kCounters + (h >> 12) % 3) = kCounter0s[(h >> 16) % (sizeof kCounter0s)]; break;
    case 1: Field_Request = static_cast<unsigned char>((h >> 12) % 3 == 0 ? 2 : 0); break;
    case 2: MoveScript_WaitWordDA = static_cast<unsigned short>((h >> 12) & 1 ? 0 : h >> 16); break;
    case 3: Field_Kind2Hold = static_cast<unsigned char>((h >> 12) & 1); break;
    case 4: Mem(kEffects + 0x80u * (B(kCounters + 3) % kEffectCount))[0] = static_cast<unsigned char>((h >> 12) & 1); break;
    default: break;
    }
}

// Counter 3 indexes the effect records again after a call (scene 8): kept
// inside the 20.
void Settle() { B(kCounters + 3) = static_cast<unsigned char>(B(kCounters + 3) % kEffectCount); }

}  // namespace

void SelfTest() {
    sh::Group group = {
        "scena_sc11", kClones, kClonesN, kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
        &Seed, &Disturb, 8000, &Settle, 0, &Args,
    };
    group.chapter = 11;
    sh::Run(group);
}

}  // namespace scena_sc11
