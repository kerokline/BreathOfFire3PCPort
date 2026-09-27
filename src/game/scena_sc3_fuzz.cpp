// BOF3X_SHADOW=scena_sc3: chapters 3 and 4 through the scenario harness
// (scenario_harness.h), once at start-up. docs/scena_sc3.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC3 --clones, SCH's tool at
// 222eb0f, checked against the reading), every callee the fifty call - the
// engine's by name, the ones nobody owns by address, and our own called
// directly (the step hooks' area tests, the stand-in spawn, the bobs, the
// tremor, the message) -, the chapters' .data dispatch tables, the regions,
// a seed of each compared constant and a disturbance of the chapter cells.
//
// Each clone's call shape (the scenario harness's Shape, marked here in a
// comment until the harness has the field): kSlot a vtable slot 0 (no
// arguments), kObject slot 1 (the object), kHook slots 2..4 ((x, z), al),
// kState a state or run handler reached through the chapter's tables, and
// kCallee a function the others call directly (with its own arguments).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc3.h"
#include "game/scena_sc3_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc3 {
namespace {

namespace sh = scenario_harness;
using move_script::SetLong;
using move_script::SetWord;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the clone table ---------------------------------------------------------
//
// tools/scenario_rows.py --unit SC3 --clones (222eb0f), 2026-09-27: every
// jump internal or a listed E8 / E9, nothing REFUSED. It lists 50 functions:
// the 54 starts of the band less the five jump-table cases pc_hidden took for
// starts (0x5453D0, 0x5455A0, 0x545910 of 0x545340's table; 0x545AA0,
// 0x545F70 of 0x5459D0's), plus 0x544AC0, the shared tail of the object
// handlers 1..4, which their E9 reach and which it makes a start.
constexpr sh::CallSite kCalls5428F0[] = {{0x12, 0x57C140}, {0x38, 0x579F00}, {0x42, 0x579F00}, {0x61, 0x57C140}, {0x8B, 0x57C140}, {0xC0, 0x531F90}, {0xE3, 0x57C140}, {0xF7, 0x57C140}, {0x103, 0x57C7C0}, {0x109, 0x5341A0}, {0x119, 0x5725F0}, {0x129, 0x5734F0}, {0x13A, 0x57C140}, {0x14E, 0x57C140}, {0x15A, 0x57C7C0}, {0x172, 0x5734F0}, {0x1B8, 0x57C140}, {0x1CD, 0x57C0F0}, {0x1D9, 0x531F90}, {0x204, 0x57C140}, {0x212, 0x531F90}, {0x220, 0x57C0F0}, {0x23C, 0x57C140}, {0x248, 0x57C7C0}, {0x24F, 0x587B40}, {0x258, 0x587AE0}, {0x266, 0x531F90}, {0x273, 0x57C0F0}, {0x29A, 0x57C140}, {0x2A7, 0x5341C0}, {0x2AE, 0x531F90}, {0x2B8, 0x587740}, {0x2C6, 0x57C0F0}, {0x2E0, 0x57C140}, {0x2F2, 0x579F00}, {0x2FD, 0x579F00}, {0x308, 0x579F00}, {0x327, 0x57C140}, {0x350, 0x57C7C0}, {0x371, 0x531F90}, {0x386, 0x57C0F0}};
constexpr sh::CallSite kCalls542CB0[] = {{0x19, 0x531F90}, {0x3B, 0x589810}, {0x69, 0x587740}, {0x86, 0x589810}, {0xB4, 0x587740}, {0xD1, 0x589810}, {0xFF, 0x587740}, {0x12B, 0x590C90}, {0x137, 0x57C0F0}, {0x145, 0x57C0F0}, {0x15B, 0x594E00}, {0x179, 0x589810}, {0x206, 0x57C0F0}, {0x220, 0x594E00}};
constexpr sh::JumpTable kTables542CB0[] = {{0x13, 0x248, 8}};
constexpr sh::CallSite kCalls542F20[] = {{0x29, 0x531F90}};
constexpr sh::CallSite kCalls542F60[] = {{0x2F, 0x4976D0}, {0x55, 0x495040}, {0x63, 0x531F90}, {0x6A, 0x5734F0}, {0x93, 0x587740}, {0xD0, 0x594E00}, {0x109, 0x495040}, {0x134, 0x589810}, {0x19C, 0x587B40}, {0x1A5, 0x587AE0}, {0x1CA, 0x57C0F0}, {0x1F0, 0x57C0F0}, {0x1FF, 0x533E50}, {0x204, 0x57C7A0}, {0x227, 0x57C0F0}, {0x253, 0x57C0F0}, {0x26A, 0x57C7C0}, {0x271, 0x531F90}, {0x29E, 0x532ED0}, {0x2A5, 0x4410B0}, {0x2EF, 0x495040}, {0x328, 0x587B80}, {0x32F, 0x587910}, {0x33F, 0x587A00}, {0x357, 0x587AE0}, {0x3E6, 0x4976D0}, {0x40C, 0x495040}, {0x41B, 0x533E50}, {0x44A, 0x587B80}, {0x451, 0x587910}, {0x461, 0x587A00}, {0x474, 0x587AE0}, {0x499, 0x57C7A0}};
// (its byte table 0x543480 stays in the original, read-only, as the copy reads it)
constexpr sh::JumpTable kTables542F60[] = {{0x1B, 0x4B8, 26}};
constexpr sh::CallSite kCalls5434C0[] = {{0x19, 0x531F90}, {0x38, 0x495040}, {0x61, 0x4976D0}, {0x9D, 0x594E00}, {0xC8, 0x57C7A0}, {0x10A, 0x57C7C0}, {0x117, 0x57C0F0}, {0x134, 0x594E00}, {0x172, 0x57C7A0}};
constexpr sh::JumpTable kTables5434C0[] = {{0x13, 0x1A0, 11}};
constexpr sh::CallSite kCalls543690[] = {{0xA7, 0x594E00}, {0xE9, 0x594E00}, {0x115, 0x57C7C0}, {0x11A, 0x57C810}, {0x121, 0x4976D0}, {0x147, 0x495040}, {0x17B, 0x5734F0}, {0x198, 0x589810}, {0x227, 0x594E00}, {0x24C, 0x57C7E0}, {0x253, 0x4976D0}, {0x280, 0x495040}, {0x287, 0x5341A0}, {0x28E, 0x531F90}, {0x2B2, 0x589810}, {0x318, 0x589810}, {0x387, 0x57C0F0}, {0x39D, 0x594E00}, {0x3BE, 0x57C0F0}, {0x3CD, 0x57C7A0}};
constexpr sh::JumpTable kTables543690[] = {{0x13, 0x40C, 18}};
constexpr sh::CallSite kCalls543AF0[] = {{0x2E, 0x495040}, {0x8A, 0x594E00}, {0xB4, 0x57C7A0}, {0x13C, 0x495040}, {0x15C, 0x495040}, {0x1AA, 0x587B40}, {0x1B0, 0x587910}, {0x1B8, 0x533E50}, {0x1C6, 0x587A00}, {0x1E9, 0x571720}, {0x1F0, 0x495040}, {0x233, 0x57C7A0}, {0x29A, 0x531F90}, {0x2D9, 0x594E00}, {0x327, 0x587740}, {0x33E, 0x587740}, {0x372, 0x56F670}, {0x3E9, 0x56F670}, {0x460, 0x56F670}, {0x4A6, 0x57C7A0}, {0x4BA, 0x57C0F0}};
constexpr sh::JumpTable kTables543AF0[] = {{0x14, 0x4D4, 29}};
constexpr sh::CallSite kCalls544040[] = {{0x3F, 0x531F90}, {0x7A, 0x594E00}, {0xC5, 0x594E00}, {0xE2, 0x589810}, {0x14B, 0x589810}, {0x1A0, 0x589810}, {0x27E, 0x57C7A0}, {0x292, 0x57C0F0}};
constexpr sh::JumpTable kTables544040[] = {{0x13, 0x2B4, 14}};
constexpr sh::CallSite kCalls544330[] = {{0x75, 0x587BE0}, {0x7C, 0x531F90}, {0x99, 0x589810}, {0x15F, 0x587740}, {0x17F, 0x544830}, {0x186, 0x544830}, {0x18E, 0x544990}, {0x1D0, 0x587740}, {0x207, 0x587740}, {0x216, 0x589810}, {0x276, 0x587740}, {0x280, 0x587740}, {0x299, 0x587740}, {0x2A3, 0x587740}, {0x2E7, 0x544830}, {0x2EE, 0x544830}, {0x2F6, 0x5449C0}, {0x30C, 0x589810}, {0x36D, 0x544830}, {0x374, 0x544830}, {0x393, 0x587B40}, {0x3B7, 0x56F670}, {0x3E4, 0x495040}, {0x3F9, 0x587AE0}, {0x416, 0x589810}, {0x452, 0x587B40}, {0x45B, 0x587AE0}, {0x469, 0x57C0F0}, {0x475, 0x57C0F0}, {0x47D, 0x57C7A0}, {0x493, 0x56D6F0}};
constexpr sh::JumpTable kTables544330[] = {{0x13, 0x4A0, 21}};
constexpr sh::CallSite kCalls544830[] = {{0x3, 0x57CD90}, {0x3A, 0x579E30}, {0x41, 0x589590}, {0xAF, 0x5720C0}, {0x10F, 0x579DB0}, {0x147, 0x579D70}};
constexpr sh::CallSite kCalls544A10[] = {{0x0, 0x57C7C0}, {0xC, 0x57C0F0}};
constexpr sh::CallSite kCalls544A40[] = {{0x7, 0x57C0F0}, {0xF, 0x544AC0}};
constexpr sh::CallSite kCalls544A60[] = {{0x7, 0x57C0F0}, {0xF, 0x544AC0}};
constexpr sh::CallSite kCalls544A80[] = {{0x7, 0x57C0F0}, {0xF, 0x544AC0}};
constexpr sh::CallSite kCalls544AA0[] = {{0x7, 0x57C0F0}, {0xF, 0x544AC0}};
constexpr sh::CallSite kCalls544AC0[] = {{0xB, 0x57C7C0}};
constexpr sh::CallSite kCalls544AF0[] = {{0x2, 0x591900}, {0x9, 0x4976D0}, {0x15, 0x587B80}, {0x1C, 0x587910}, {0x24, 0x587A00}, {0x2D, 0x517290}, {0x34, 0x5A9949}, {0x3C, 0x587A00}, {0x45, 0x587B90}};
constexpr sh::CallSite kCalls544B40[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls544B60[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls544B80[] = {{0x2D, 0x544D50}, {0x40, 0x544D00}, {0x53, 0x544E30}, {0x66, 0x544C80}, {0x79, 0x544DA0}, {0x8C, 0x544E80}};
// (its byte table 0x544C34 stays in the original, read-only)
constexpr sh::JumpTable kTables544B80[] = {{0x1F, 0x98, 7}};
constexpr sh::CallSite kCalls544C80[] = {{0x8, 0x57C140}, {0x1D, 0x57C140}, {0x52, 0x57C140}};
constexpr sh::CallSite kCalls544D00[] = {{0x8, 0x57C140}, {0x34, 0x57C0F0}};
constexpr sh::CallSite kCalls544D50[] = {{0x8, 0x57C140}, {0x3B, 0x546320}};
constexpr sh::CallSite kCalls544DA0[] = {{0x8, 0x57C140}, {0x53, 0x546320}, {0x80, 0x546320}};
constexpr sh::CallSite kCalls544E30[] = {{0x8, 0x57C140}, {0x39, 0x546320}};
constexpr sh::CallSite kCalls544E80[] = {{0x8, 0x57C140}, {0x53, 0x546320}, {0x7F, 0x57C140}, {0x8B, 0x57C7C0}, {0xB1, 0x546320}};
constexpr sh::CallSite kCalls544F40[] = {{0x17, 0x544F60}};
constexpr sh::CallSite kCalls544F60[] = {{0x8, 0x57C140}, {0x2B, 0x57C7C0}};
constexpr sh::CallSite kCalls544FB0[] = {{0x11, 0x56D800}};
constexpr sh::CallSite kCalls544FE0[] = {{0x2, 0x4976D0}, {0x11, 0x57C7C0}};
constexpr sh::CallSite kCalls545040[] = {{0x22, 0x57C140}, {0x37, 0x57C0F0}, {0x44, 0x587B40}, {0x4D, 0x587AE0}, {0x52, 0x57C7C0}, {0x67, 0x5341A0}, {0x6E, 0x531F90}, {0x7F, 0x57C140}, {0x93, 0x57C140}, {0xB0, 0x57C810}, {0xB9, 0x5734F0}, {0xC1, 0x57C810}, {0xE2, 0x57C140}, {0xF7, 0x57C0F0}, {0x103, 0x5341A0}, {0x109, 0x531F90}, {0x119, 0x57C140}, {0x12E, 0x57C140}, {0x13A, 0x57C810}, {0x16C, 0x57C140}, {0x180, 0x57C0F0}, {0x18E, 0x531F90}, {0x19F, 0x57C140}, {0x1B4, 0x579F00}, {0x1C2, 0x579F00}, {0x1F8, 0x57C140}, {0x210, 0x57C140}, {0x227, 0x5734F0}, {0x25B, 0x589810}, {0x2AF, 0x589810}};
constexpr sh::CallSite kCalls545340[] = {{0x53, 0x531F90}, {0x75, 0x572650}, {0x7F, 0x587740}, {0x9D, 0x589810}, {0x109, 0x587740}, {0x16B, 0x57C7A0}, {0x199, 0x587BE0}, {0x1A0, 0x587910}, {0x1DE, 0x587B40}, {0x20B, 0x4976D0}, {0x275, 0x4976D0}, {0x2AC, 0x587AE0}, {0x2CA, 0x495040}, {0x2EA, 0x495040}, {0x30A, 0x533E50}, {0x311, 0x587BE0}, {0x317, 0x587910}, {0x339, 0x56F670}, {0x347, 0x587A00}, {0x356, 0x587B40}, {0x35F, 0x587AE0}, {0x37B, 0x495040}, {0x3DA, 0x594E00}, {0x44F, 0x57C0F0}, {0x465, 0x594E00}, {0x4A6, 0x594E00}, {0x4E0, 0x594E00}, {0x51D, 0x594E00}, {0x53D, 0x587BE0}, {0x544, 0x587910}, {0x555, 0x587A00}, {0x577, 0x454810}, {0x5E4, 0x587AE0}, {0x5F4, 0x57C7A0}};
constexpr sh::JumpTable kTables545340[] = {{0x14, 0x608, 31}};
constexpr sh::CallSite kCalls5459D0[] = {{0x3D, 0x531F90}, {0x60, 0x587B40}, {0x6A, 0x587740}, {0xE4, 0x56F670}, {0x167, 0x56F670}, {0x16E, 0x587A20}, {0x182, 0x454810}, {0x1E1, 0x5725F0}, {0x1F8, 0x587AE0}, {0x1FF, 0x531F90}, {0x207, 0x589810}, {0x2C8, 0x589810}, {0x3B5, 0x5725F0}, {0x3D6, 0x589810}, {0x41C, 0x589810}, {0x4D1, 0x594E00}, {0x4E6, 0x57C0F0}, {0x509, 0x587BE0}, {0x540, 0x589810}, {0x5AD, 0x589810}, {0x643, 0x546130}, {0x672, 0x594E00}, {0x692, 0x57C7A0}, {0x6A7, 0x57C0F0}, {0x6AF, 0x56D6F0}, {0x6CF, 0x546130}};
constexpr sh::JumpTable kTables5459D0[] = {{0x1A, 0x6D8, 31}};
constexpr sh::CallSite kCalls546180[] = {{0x7, 0x57C0F0}};
constexpr sh::CallSite kCalls5461A0[] = {{0x17, 0x5461C0}};
constexpr sh::CallSite kCalls5461C0[] = {{0x8, 0x57C140}, {0x58, 0x57C7C0}, {0x65, 0x57C0F0}, {0x6F, 0x587740}, {0x96, 0x57C140}, {0xE2, 0x546320}, {0x10E, 0x546320}, {0x120, 0x57C140}, {0x153, 0x546320}};
constexpr sh::CallSite kCalls546320[] = {{0x7, 0x4976D0}};
constexpr sh::CallSite kCalls546340[] = {{0x11, 0x56D800}};
constexpr sh::CallSite kCalls546370[] = {{0x0, 0x57C7C0}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define SC3_OURS(f) reinterpret_cast<const void*>(&::f)
// Name, base, size, calls, imms (none), tables, ours, ret_mask. Shape in the comment.
const sh::Clone kClones[] = {
    {"Scena03_Frame", 0x5428C0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Frame)},                     // kSlot 0 (Scena03_States)
    {"Scena03_Start", 0x5428D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Start)},                    // kState 0
    {"Scena03_EnterArea", 0x5428F0, 0x3AC, kCalls5428F0, SH_N(kCalls5428F0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_EnterArea)},   // kState 1
    {"Scena03_Run", 0x542CA0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Run)},                         // kState 2 (Scena03_Runs)
    {"Scena03_Scene1", 0x542CB0, 0x268, kCalls542CB0, SH_N(kCalls542CB0), nullptr, 0, kTables542CB0, SH_N(kTables542CB0), SC3_OURS(Scena03_Scene1)},   // kState, run 1
    {"Scena03_Scene2", 0x542F20, 0x40, kCalls542F20, SH_N(kCalls542F20), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Scene2)},           // kState, run 2
    {"Scena03_Scene3", 0x542F60, 0x553, kCalls542F60, SH_N(kCalls542F60), nullptr, 0, kTables542F60, SH_N(kTables542F60), SC3_OURS(Scena03_Scene3)},   // kState, run 3
    {"Scena03_Scene4", 0x5434C0, 0x1CC, kCalls5434C0, SH_N(kCalls5434C0), nullptr, 0, kTables5434C0, SH_N(kTables5434C0), SC3_OURS(Scena03_Scene4)},   // kState, run 4
    {"Scena03_Scene5", 0x543690, 0x454, kCalls543690, SH_N(kCalls543690), nullptr, 0, kTables543690, SH_N(kTables543690), SC3_OURS(Scena03_Scene5)},   // kState, run 5
    {"Scena03_Scene6", 0x543AF0, 0x548, kCalls543AF0, SH_N(kCalls543AF0), nullptr, 0, kTables543AF0, SH_N(kTables543AF0), SC3_OURS(Scena03_Scene6)},   // kState, run 6
    {"Scena03_Scene7", 0x544040, 0x2EC, kCalls544040, SH_N(kCalls544040), nullptr, 0, kTables544040, SH_N(kTables544040), SC3_OURS(Scena03_Scene7)},   // kState, run 7
    {"Scena03_Scene8", 0x544330, 0x4F4, kCalls544330, SH_N(kCalls544330), nullptr, 0, kTables544330, SH_N(kTables544330), SC3_OURS(Scena03_Scene8)},   // kState, run 8
    {"Scena03_SpawnAtMember", 0x544830, 0x154, kCalls544830, SH_N(kCalls544830), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_SpawnAtMember), 0xFF},   // kCallee (member), al
    {"Scena03_BobParty4", 0x544990, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_BobParty4)},            // kCallee
    {"Scena03_BobParty2", 0x5449C0, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_BobParty2)},            // kCallee
    {"Scena03_ObjectHook", 0x5449F0, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_ObjectHook)},          // kObject (Scena03_ObjectHandlers)
    {"Scena03_Object0", 0x544A10, 0x23, kCalls544A10, SH_N(kCalls544A10), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object0)},        // kState (object, bank)
    {"Scena03_Object1", 0x544A40, 0x14, kCalls544A40, SH_N(kCalls544A40), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object1), 0xFF},  // kState (object, bank), al
    {"Scena03_Object2", 0x544A60, 0x14, kCalls544A60, SH_N(kCalls544A60), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object2), 0xFF},
    {"Scena03_Object3", 0x544A80, 0x14, kCalls544A80, SH_N(kCalls544A80), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object3), 0xFF},
    {"Scena03_Object4", 0x544AA0, 0x14, kCalls544AA0, SH_N(kCalls544AA0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object4), 0xFF},
    {"Scena03_ObjectsAllFour", 0x544AC0, 0x26, kCalls544AC0, SH_N(kCalls544AC0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_ObjectsAllFour), 0xFF},   // kCallee (a tail), al
    {"Scena03_Object5", 0x544AF0, 0x4A, kCalls544AF0, SH_N(kCalls544AF0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object5)},        // kState (object, bank)
    {"Scena03_Object7", 0x544B40, 0x16, kCalls544B40, SH_N(kCalls544B40), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object7), 0xFF},
    {"Scena03_Object8", 0x544B60, 0x1F, kCalls544B60, SH_N(kCalls544B60), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object8), 0xFF},
    {"Scena03_StepHook", 0x544B80, 0xF3, kCalls544B80, SH_N(kCalls544B80), nullptr, 0, kTables544B80, SH_N(kTables544B80), SC3_OURS(Scena03_StepHook), 0xFF},   // kHook slot 2 (x, z), al
    {"Scena03_StepArea33", 0x544C80, 0x7F, kCalls544C80, SH_N(kCalls544C80), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea33), 0xFF},   // kCallee (x, z), al
    {"Scena03_StepArea29", 0x544D00, 0x50, kCalls544D00, SH_N(kCalls544D00), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea29), 0xFF},
    {"Scena03_StepArea25", 0x544D50, 0x47, kCalls544D50, SH_N(kCalls544D50), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea25), 0xFF},
    {"Scena03_StepArea45", 0x544DA0, 0x8C, kCalls544DA0, SH_N(kCalls544DA0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea45), 0xFF},
    {"Scena03_StepArea32", 0x544E30, 0x45, kCalls544E30, SH_N(kCalls544E30), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea32), 0xFF},
    {"Scena03_StepArea63", 0x544E80, 0xBD, kCalls544E80, SH_N(kCalls544E80), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea63), 0xFF},
    {"Scena03_ArriveHook", 0x544F40, 0x20, kCalls544F40, SH_N(kCalls544F40), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_ArriveHook), 0xFF},   // kHook slot 3 (x, z), al
    {"Scena03_ArriveArea47", 0x544F60, 0x4D, kCalls544F60, SH_N(kCalls544F60), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_ArriveArea47), 0xFF},   // kCallee (x, z), al
    {"Scena03_CellHook", 0x544FB0, 0x2D, kCalls544FB0, SH_N(kCalls544FB0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_CellHook), 0xFF},   // kHook slot 4 (x, z), al (Scena03_CellHandlers)
    {"Scena03_Cell0", 0x544FE0, 0x2E, kCalls544FE0, SH_N(kCalls544FE0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Cell0)},            // kState (the cell's handler)
    {"Scena04_Frame", 0x545010, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Frame)},                     // kSlot 0 (Scena04_States)
    {"Scena04_Start", 0x545020, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Start)},                    // kState 0
    {"Scena04_EnterArea", 0x545040, 0x2E1, kCalls545040, SH_N(kCalls545040), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_EnterArea)},   // kState 1
    {"Scena04_Run", 0x545330, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Run)},                         // kState 2 (Scena04_Runs)
    {"Scena04_Scene1", 0x545340, 0x684, kCalls545340, SH_N(kCalls545340), nullptr, 0, kTables545340, SH_N(kTables545340), SC3_OURS(Scena04_Scene1)},   // kState, run 1
    {"Scena04_Scene2", 0x5459D0, 0x754, kCalls5459D0, SH_N(kCalls5459D0), nullptr, 0, kTables5459D0, SH_N(kTables5459D0), SC3_OURS(Scena04_Scene2)},   // kState, run 2
    {"Scena04_Tremor", 0x546130, 0x27, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Tremor)},                  // kCallee (Scena04_Scene2's tail)
    {"Scena04_ObjectHook", 0x546160, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_ObjectHook)},          // kObject (Scena04_ObjectHandlers)
    {"Scena04_Object0", 0x546180, 0x12, kCalls546180, SH_N(kCalls546180), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Object0), 0xFF},  // kState (object, bank), al
    {"Scena04_StepHook", 0x5461A0, 0x20, kCalls5461A0, SH_N(kCalls5461A0), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_StepHook), 0xFF},   // kHook slot 2 (x, z), al
    {"Scena04_StepArea28", 0x5461C0, 0x15F, kCalls5461C0, SH_N(kCalls5461C0), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_StepArea28), 0xFF},   // kCallee (x, z), al
    {"Scena04_Message", 0x546320, 0x20, kCalls546320, SH_N(kCalls546320), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Message), 0xFF},  // kCallee (id), al
    {"Scena04_CellHook", 0x546340, 0x2D, kCalls546340, SH_N(kCalls546340), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_CellHook), 0xFF},   // kHook slot 4 (x, z), al (Scena04_CellHandlers)
    {"Scena04_Cell0", 0x546370, 0x1D, kCalls546370, SH_N(kCalls546370), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Cell0)},            // kState (the cell's handler)
};
constexpr unsigned kClonesN = sizeof kClones / sizeof kClones[0];

// --- the callees ---------------------------------------------------------------
//
// Listed whole (registered before the harness's standard set, so a listing
// here stands): stage B drops those the scenario harness already records the
// same way. The key is the pointer ours passes: our function for a callee
// that is ours, the address for Capcom's.
constexpr std::uint32_t kAll = 0xFFFFFFFF;
#define SC3_CAPCOM(name, address, n, ...) {name, address, address, n, {__VA_ARGS__}, sh::Answer::kGarbage, 0, 0}
#define SC3_OURS_CALLEE(name, fn, address, n, ...) {name, address, Key(reinterpret_cast<const void*>(&::fn)), n, {__VA_ARGS__}, sh::Answer::kGarbage, 0, 0}

sh::Callee Answering(sh::Callee c, sh::Answer a, std::uint8_t lo = 0, std::uint8_t hi = 0) {
    c.answer = a;
    c.lo = lo;
    c.hi = hi;
    return c;
}

// Filled at SelfTest (Key() of our functions is not a constant expression).
sh::Callee g_callees[64];
unsigned g_n_callees = 0;
void Add(const sh::Callee& c) { g_callees[g_n_callees++] = c; }

void ListCallees() {
    g_n_callees = 0;
    // The engine's, ours by name.
    Add(Answering(SC3_OURS_CALLEE("Flags_Test", Flags_Test, 0x57C140, 2, kAll, kAll), sh::Answer::kFlag));
    Add(SC3_OURS_CALLEE("Flags_Set", Flags_Set, 0x57C0F0, 2, kAll, kAll));
    Add(SC3_OURS_CALLEE("AreaMap_SetByte", AreaMap_SetByte, 0x579F00, 3, kAll, kAll, kAll));
    Add(SC3_OURS_CALLEE("Party_DropIn", Party_DropIn, 0x531F90, 1, kAll));
    Add(SC3_OURS_CALLEE("ScriptFlags_Set40", ScriptFlags_Set40, 0x57C7C0, 0));
    Add(SC3_OURS_CALLEE("ScriptFlags_Clear40", ScriptFlags_Clear40, 0x57C7A0, 0));
    Add(SC3_OURS_CALLEE("ObjTrio_SetBit40", ObjTrio_SetBit40, 0x57C810, 0));
    Add(SC3_OURS_CALLEE("Scenario_CallA", Scenario_CallA, 0x5341A0, 1, 0xFF));
    Add(SC3_OURS_CALLEE("MapView_SetElevation", MapView_SetElevation, 0x5725F0, 1, kAll));
    Add(SC3_OURS_CALLEE("Kind2_Place", Kind2_Place, 0x5734F0, 1, 0xFF));
    Add(SC3_OURS_CALLEE("Music_FadeOutStop", Music_FadeOutStop, 0x587B40, 1, kAll));
    Add(SC3_OURS_CALLEE("Music_Play", Music_Play, 0x587AE0, 2, kAll, kAll));
    Add(SC3_OURS_CALLEE("Music_FadeOut", Music_FadeOut, 0x587BE0, 1, kAll));
    Add(SC3_OURS_CALLEE("Music_LoadFile", Music_LoadFile, 0x587A20, 1, kAll));
    Add(SC3_OURS_CALLEE("Sound_PlayEffect", Sound_PlayEffect, 0x587740, 1, 0xFFFF));
    Add(SC3_OURS_CALLEE("Sound_LoadStream", Sound_LoadStream, 0x587910, 1, kAll));
    Add(Answering(SC3_OURS_CALLEE("Sound_StreamDone", Sound_StreamDone, 0x587A00, 0), sh::Answer::kBool));
    Add(Answering(SC3_OURS_CALLEE("File_LoadDone", File_LoadDone, 0x454810, 0), sh::Answer::kBool));
    // Effect_Objects holds 20 records: a slot 0..0x13, or 0xFF none.
    Add(Answering(SC3_OURS_CALLEE("Effect_FindFree", Effect_FindFree, 0x589810, 0), sh::Answer::kByte, 0xFF, 0x13));
    Add(SC3_OURS_CALLEE("Field_ChangeArea", Field_ChangeArea, 0x594E00, 4, kAll, kAll, kAll, kAll));
    Add(SC3_OURS_CALLEE("Msg_OpenScript", Msg_OpenScript, 0x4976D0, 1, 0xFFFF));
    Add(SC3_OURS_CALLEE("Transition_Start", Transition_Start, 0x495040, 1, 0xFF));
    Add(SC3_OURS_CALLEE("Field_LoadingFrame", Field_LoadingFrame, 0x517290, 0));
    Add(SC3_OURS_CALLEE("Task_Sleep", Task_Sleep, 0x5A9949, 1, kAll));
    Add(SC3_OURS_CALLEE("Field_ViewReset", Field_ViewReset, 0x56F670, 0));
    Add(SC3_OURS_CALLEE("AreaMap_SetupEntries", AreaMap_SetupEntries, 0x571720, 0));
    Add(SC3_OURS_CALLEE("MoveCmd_TestFB", MoveCmd_TestFB, 0x572650, 2, 0xFFFF, 0xFFFF));
    Add(SC3_OURS_CALLEE("EventObj_Reset", EventObj_Reset, 0x579E30, 0));
    Add(SC3_OURS_CALLEE("Sprite_SetAnimationBank", Sprite_SetAnimationBank, 0x589590, 1, 0xFFFF));
    Add(SC3_OURS_CALLEE("AreaMap_Elevation", AreaMap_Elevation, 0x5720C0, 2, kAll, kAll));
    {
        // a pointer to a byte of the caller's frame (the original's: its
        // argument's top byte): logged as the byte it points at
        sh::Callee c = SC3_OURS_CALLEE("EventObj_SetFlags", EventObj_SetFlags, 0x579DB0, 1, 0);
        c.deref[0] = 1;
        Add(c);
    }
    // Capcom's, named but not ours.
    Add(SC3_CAPCOM("ObjTrio_ClearBit40", 0x57C7E0, 0));
    Add(SC3_CAPCOM("Sound_ResumeAll", 0x587B90, 0));
    // Nobody's, by address (scena_sc3_callees.h).
    Add(SC3_CAPCOM("0x4410B0 (SE)", kLeaderState5, 1, 0xFF));
    Add(SC3_CAPCOM("EventObj_Face 0x579D70 (SE)", kEventObjFace, 0));
    Add(SC3_CAPCOM("Scenario_CallB 0x5341C0", kCallB, 1, 0xFF));
    Add(SC3_CAPCOM("0x532ED0", kPlaceParty, 3, kAll, kAll, 0xFF));
    Add(SC3_CAPCOM("0x533E50", kPartyRestore, 0));
    Add(SC3_CAPCOM("0x56D6F0", kStatusBit80, 0));
    // 0x56D800: an index below n (1) or 0xFF; the originals test al's sign
    Add(Answering(SC3_CAPCOM("0x56D800", kCellFind, 4, kAll, 0xFF, kAll, kAll), sh::Answer::kByte, 0xFF, 0x00));
    // 0x57CD90: a Sprite_Objects index 0..0x1D, or 0xFF none
    Add(Answering(SC3_CAPCOM("0x57CD90", kSpriteFindFree, 0), sh::Answer::kByte, 0xFF, 0x1D));
    Add(SC3_CAPCOM("0x587B80", kMusicStop, 0));
    Add(SC3_CAPCOM("0x590C90", kItemEvent, 4, 0xFF, kAll, 0xFF, kAll));
    Add(SC3_CAPCOM("0x591900", kKeyItemAdd, 1, 0xFF));
    // Ours called directly by the originals' E8 / E9 (each clone is fuzzed
    // alone; these answer as the callers read them).
    Add(SC3_OURS_CALLEE("Scena03_SpawnAtMember", Scena03_SpawnAtMember, 0x544830, 1, 0xFF));
    Add(SC3_OURS_CALLEE("Scena03_BobParty4", Scena03_BobParty4, 0x544990, 0));
    Add(SC3_OURS_CALLEE("Scena03_BobParty2", Scena03_BobParty2, 0x5449C0, 0));
    Add(Answering(SC3_OURS_CALLEE("Scena03_ObjectsAllFour", Scena03_ObjectsAllFour, 0x544AC0, 0), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE("Scena03_StepArea33", Scena03_StepArea33, 0x544C80, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE("Scena03_StepArea29", Scena03_StepArea29, 0x544D00, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE("Scena03_StepArea25", Scena03_StepArea25, 0x544D50, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE("Scena03_StepArea45", Scena03_StepArea45, 0x544DA0, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE("Scena03_StepArea32", Scena03_StepArea32, 0x544E30, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE("Scena03_StepArea63", Scena03_StepArea63, 0x544E80, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE("Scena03_ArriveArea47", Scena03_ArriveArea47, 0x544F60, 2, kAll, kAll), sh::Answer::kFlag));
    Add(SC3_OURS_CALLEE("Scena04_Tremor", Scena04_Tremor, 0x546130, 0));
    Add(Answering(SC3_OURS_CALLEE("Scena04_StepArea28", Scena04_StepArea28, 0x5461C0, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE("Scena04_Message", Scena04_Message, 0x546320, 1, 0xFF), sh::Answer::kFlag));
}

// --- the tables ------------------------------------------------------------------
//
// Swapped for recorders while the fuzz runs. Each chapter's state table runs
// on into its run table (the frame's index 3.. reads runs), so one window
// holds both: the seeds keep the indices inside.
const sh::DataTable kTables[] = {
    {at::kStates3, 12},       // Scena03_States (3) and Scena03_Runs (9)
    {at::kObjects3, 9},       // Scena03_ObjectHandlers
    {at::kCellHandlers3, 1},  // Scena03_CellHandlers
    {at::kStates4, 6},        // Scena04_States (3) and Scena04_Runs (3)
    {at::kObjects4, 1},       // Scena04_ObjectHandlers
    {at::kCellHandlers4, 1},  // Scena04_CellHandlers
};

// --- the regions -------------------------------------------------------------------
//
// Every cell the fifty read or write; stage B drops those the harness's
// standard regions already hold.
unsigned char g_object[0x100];   // the object slot 1 is given
sh::Region g_regions[32];
unsigned g_n_regions = 0;
void Region(std::uint32_t at, std::uint32_t size) { g_regions[g_n_regions++] = {at, size}; }
void ListRegions() {
    g_n_regions = 0;
    Region(0x8034E0, 0x14);             // Cond_ByteFA .. Cond_ByteFD: state, effect byte, run, step, timer
    Region(0x903840, 0x20);             // Camera_Distance, the counters, the slot byte
    Region(0x903FA0, 0x18);             // the chapters' flag rows 0x903FA8 / 0x903FB0
    Region(at::kPassFlags, 1);
    Region(at::kArea, 2);
    Region(at::kByteFE, 1);
    Region(0x905E60, 0xA);              // Field_Kind2Z / X, the byte after, MapView_Redraw
    Region(at::kScriptFlags, 2);
    Region(at::kRequest, 1);
    Region(at::kGameMode, 2);
    Region(at::kWait, 2);
    Region(at::kMessage, 2);
    Region(0x937F88, 0x14);             // Sprite_Current, MoveScript_F3Divisor, Frame_Counter, the pending kind
    Region(at::kAreaTrack, 1);
    Region(at::kAreaTransition, 1);
    Region(at::kMusicTrack, 1);
    Region(0x929EC0, 0x60);             // member counts, camera angles, the flag row pointer, the load bytes, Kind2Hold, elevation
    Region(at::kEffectState, 1);
    Region(at::kMembers, 3 * at::kMemberStride);
    Region(at::kActiveMember, 4);
    Region(at::kEffects, 0xA00);
    Region(at::kSprites, 0x1338);
    Region(Key(g_object), sizeof g_object);
}

// --- the seed ----------------------------------------------------------------------
//
// Every constant the fifty compare a cell with, picked half the time; the
// step within each scene's cases (and one past); the state and run inside
// the swapped windows.
std::uint32_t PickC0() {
    return SH_PICK(1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xC, 0xD, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x1C,
                   0x1D, 0x1F, 0x22, 0x23, 0x25, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2F, 0x32, 0x34, 0x55, 0x58);
}
std::uint32_t PickTimer() {
    return SH_PICK(0, 0, 1, 2, 3, 0xA, 0xB, 0xC, 0xD, 0x12, 0x13, 0x14, 0x1F, 0x20, 0x21, 0x2F, 0x30, 0x32, 0x4F, 0x50, 0x7F,
                   0x80, 0x8F, 0x90, 0x9F, 0xA0, 0x10F, 0x110, 0x12F, 0x130, 0xFFFF);
}
std::uint32_t PickArea() {
    return SH_PICK(0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2D, 0x30, 0x32, 0x33, 0x36, 0x38, 0x41, 0x45, 0x47, 0x63, 0x64);
}
// A 16.16 coordinate near the cells and exact values the hooks test.
std::int32_t PickCoord() {
    if (sh::Half()) {
        return static_cast<std::int32_t>(SH_PICK(0x5C0000, 0x418000, 0x3A8000, 0x28000, 0x4C8000, 0xA8000, 0xB8000, 0x490000, 0x60000,
                                                 0x40000, 0x4D8000, 0x248000));
    }
    const std::uint32_t hi = SH_PICK(1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xD, 0xE, 0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17, 0x18, 0x19,
                                     0x1A, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x33, 0x34, 0x35, 0x36, 0x3B, 0x3C,
                                     0x3D, 0x3E, 0x56, 0x57, 0x5A, 0x5B);
    return static_cast<std::int32_t>((hi << 16) | (sh::Half() ? 0 : (sh::Half() ? 0x8000 : (sh::Next() & 0xFFFF))));
}

// Steps each clone's switch holds (0: any byte, weighted low).
unsigned StepSpan(const char* name) {
    struct Span { const char* name; unsigned span; };
    static const Span kSpans[] = {
        {"Scena03_Scene1", 9}, {"Scena03_Scene2", 3}, {"Scena03_Scene3", 0x34}, {"Scena03_Scene4", 0xC}, {"Scena03_Scene5", 0x13},
        {"Scena03_Scene6", 0x1E}, {"Scena03_Scene7", 0xF}, {"Scena03_Scene8", 0x16}, {"Scena04_Scene1", 0x20}, {"Scena04_Scene2", 0x20},
    };
    for (const Span& s : kSpans)
        if (std::strcmp(s.name, name) == 0) return s.span;
    return 0x20;
}

bool IsChapter4(unsigned k) { return kClones[k].base >= 0x545010; }

void Seed(unsigned k) {
    // the scenario bytes
    sh::Mem(at::kState)[0] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 3 : sh::Next() % (IsChapter4(k) ? 6 : 12));
    sh::Mem(at::kRun)[0] = static_cast<unsigned char>(sh::Next() % (IsChapter4(k) ? 3 : 9));
    const unsigned span = StepSpan(kClones[k].name);
    sh::Mem(at::kStep)[0] = static_cast<unsigned char>(sh::Often() ? sh::Next() % (span + 1) : sh::Next());
    if (sh::Often()) SetWord(sh::Mem(at::kTimer), PickTimer());
    sh::Mem(at::kEffectByte)[0] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 0x14 : 0xFF);
    // the counters
    if (sh::Often()) sh::Mem(at::kCounter0)[0] = static_cast<unsigned char>(PickC0());
    if (sh::Half()) sh::Mem(at::kCounter1)[0] = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3, 0x15));
    // the area, the bytes the entries test
    if (sh::Often()) SetWord(sh::Mem(at::kArea), PickArea());
    if (sh::Often()) sh::Mem(at::kByteFD)[0] = static_cast<unsigned char>(sh::Next() % 8);
    if (sh::Half()) sh::Mem(at::kKind2Mode)[0] = 1;
    if (sh::Half()) sh::Mem(at::kRow3Byte2)[0] = static_cast<unsigned char>(sh::Mem(at::kRow3Byte2)[0] | 0xF);
    sh::Mem(at::kLeaderKind)[0] = static_cast<unsigned char>(sh::Often() ? 3 + sh::Next() % 5 : sh::Next());
    // the waits
    if (sh::Half()) sh::Mem(at::kRequest)[0] = 2;
    if (sh::Half()) SetWord(sh::Mem(at::kWait), 0);
    if (sh::Half()) SetWord(sh::Mem(at::kMessage), SH_PICK(1, 0xC, 0x11, 0x12, 0x1F, 0x33));
    if (sh::Half()) sh::Mem(at::kKind2Hold)[0] = 0;
    if (sh::Half()) SetWord(sh::Mem(at::kGameMode), 2);
    if (sh::Half()) SetWord(sh::Mem(at::kCamDistance), SH_PICK(0, 0x10, 0x3F8, 0x3FF, 0x400, 0x7FF0));
    // the pointers read through: the current sprite a record of the pool,
    // the object slot 1 is given
    sh::SetPointer(at::kSpriteCurrent, sh::Mem(at::kSprites + (sh::Next() % 30) * at::kSpriteStride));
    g_object[0x86] = static_cast<unsigned char>(IsChapter4(k) ? 0 : sh::Next() % 9);
}

// Function k's arguments: (x, z) for the hooks and area tests, the object and
// a row for slot 1 and its handlers, a member, a message id.
void Args(unsigned k, std::uint32_t* a) {
    const char* const name = kClones[k].name;
    if (std::strstr(name, "Hook") || std::strstr(name, "StepArea") || std::strstr(name, "ArriveArea")) {
        a[0] = static_cast<std::uint32_t>(PickCoord());
        a[1] = static_cast<std::uint32_t>(PickCoord());
    }
    if (std::strstr(name, "ObjectHook") || std::strstr(name, "Object0") || std::strstr(name, "Object1") ||
        std::strstr(name, "Object2") || std::strstr(name, "Object3") || std::strstr(name, "Object4") ||
        std::strstr(name, "Object5") || std::strstr(name, "Object7") || std::strstr(name, "Object8")) {
        a[0] = Key(g_object);
        a[1] = sh::Next();   // the row, only passed on to Flags_Set's recorder
    }
    if (std::strcmp(name, "Scena03_SpawnAtMember") == 0) a[0] = sh::Often() ? sh::Next() % 3 : (sh::Next() & 0xFFFFFF00) | (sh::Next() % 3);
    if (std::strcmp(name, "Scena04_Message") == 0) a[0] = sh::Next();
}

// After a call, two in three: a chapter cell the scenes read again - the
// step, the timer, a counter, the request, the wait word, the area, a byte of
// the effect records, the current sprite (another record of the pool).
void Disturb(std::uint32_t h) {
    switch ((h >> 8) % 10) {
    case 0: sh::Mem(at::kStep)[0] = static_cast<unsigned char>(h >> 16); break;
    case 1: SetWord(sh::Mem(at::kTimer), h >> 16); break;
    case 2: sh::Mem(at::kCounters + ((h >> 12) & 3))[0] = static_cast<unsigned char>(h >> 16); break;
    case 3: sh::Mem(at::kRequest)[0] = static_cast<unsigned char>((h >> 16) & 3); break;
    case 4: SetWord(sh::Mem(at::kWait), (h >> 16) & 1); break;
    case 5: SetWord(sh::Mem(at::kArea), h >> 16); break;
    case 6: sh::Mem(at::kEffects + (h >> 12) % 0xA00)[0] = static_cast<unsigned char>(h >> 24); break;
    case 7: sh::SetPointer(at::kSpriteCurrent, sh::Mem(at::kSprites + ((h >> 16) % 30) * at::kSpriteStride)); break;
    case 8: sh::Mem(at::kStatusBits)[0] = static_cast<unsigned char>(h >> 16); break;
    default: SetWord(sh::Mem(at::kCamDistance), h >> 16); break;
    }
}

}  // namespace

void SelfTest() {
    ListCallees();
    ListRegions();
    const sh::Group group = {
        "scena_sc3", kClones, kClonesN, g_callees, g_n_callees, kTables, sizeof kTables / sizeof kTables[0],
        g_regions, g_n_regions, &Seed, &Disturb, 2000, nullptr, 0, &Args,
    };
    sh::Run(group);
}

}  // namespace scena_sc3
