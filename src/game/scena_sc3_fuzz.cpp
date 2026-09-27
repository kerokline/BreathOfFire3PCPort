// BOF3X_SHADOW=scena_sc3: chapters 3 and 4 through the scenario harness
// (scenario_harness.h), once at start-up, as two runs - one per chapter byte
// (a Group takes one chapter). docs/scena_sc3.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC3 --clones, SCH's tool at
// 222eb0f, checked against the reading) with each clone's call shape; the
// callees the harness's standard set lacks (the unnamed ones, ours the
// originals call directly, EventObj_SetFlags' pointer dereferenced); typed
// stand-ins written into the object handler tables (they take arguments);
// the state, run and cell handler tables swapped; the regions beyond the
// standard ones; a seed of each compared constant and a disturbance of the
// chapter cells the harness's own does not move.
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
constexpr sh::Shape kSlot = sh::Shape::kSlot, kState = sh::Shape::kState, kHook = sh::Shape::kHook, kEntry = sh::Shape::kEntry,
                    kObject = sh::Shape::kObject;

// Chapter 3's 36, fuzzed with Cond_ByteFA 3 (its row 0x903FA8). Shapes: the
// frame kSlot; the states, runs, scenes, bobs, the four-bit tail and the
// cell's handler kState (no arguments); the object hook kObject; the object
// handlers kEntry (object, row: the group's args); the hooks and the area
// tests kHook ((x, z), al); the spawn kEntry (the member).
const sh::Clone kClones3[] = {
    {"Scena03_Frame", 0x5428C0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Frame), 0, false, kSlot},
    {"Scena03_Start", 0x5428D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Start), 0, false, kState},
    {"Scena03_EnterArea", 0x5428F0, 0x3AC, kCalls5428F0, SH_N(kCalls5428F0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_EnterArea), 0, false, kState},
    {"Scena03_Run", 0x542CA0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Run), 0, false, kState},
    {"Scena03_Scene1", 0x542CB0, 0x268, kCalls542CB0, SH_N(kCalls542CB0), nullptr, 0, kTables542CB0, SH_N(kTables542CB0), SC3_OURS(Scena03_Scene1), 0, false, kState},
    {"Scena03_Scene2", 0x542F20, 0x40, kCalls542F20, SH_N(kCalls542F20), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Scene2), 0, false, kState},
    {"Scena03_Scene3", 0x542F60, 0x553, kCalls542F60, SH_N(kCalls542F60), nullptr, 0, kTables542F60, SH_N(kTables542F60), SC3_OURS(Scena03_Scene3), 0, false, kState},
    {"Scena03_Scene4", 0x5434C0, 0x1CC, kCalls5434C0, SH_N(kCalls5434C0), nullptr, 0, kTables5434C0, SH_N(kTables5434C0), SC3_OURS(Scena03_Scene4), 0, false, kState},
    {"Scena03_Scene5", 0x543690, 0x454, kCalls543690, SH_N(kCalls543690), nullptr, 0, kTables543690, SH_N(kTables543690), SC3_OURS(Scena03_Scene5), 0, false, kState},
    {"Scena03_Scene6", 0x543AF0, 0x548, kCalls543AF0, SH_N(kCalls543AF0), nullptr, 0, kTables543AF0, SH_N(kTables543AF0), SC3_OURS(Scena03_Scene6), 0, false, kState},
    {"Scena03_Scene7", 0x544040, 0x2EC, kCalls544040, SH_N(kCalls544040), nullptr, 0, kTables544040, SH_N(kTables544040), SC3_OURS(Scena03_Scene7), 0, false, kState},
    {"Scena03_Scene8", 0x544330, 0x4F4, kCalls544330, SH_N(kCalls544330), nullptr, 0, kTables544330, SH_N(kTables544330), SC3_OURS(Scena03_Scene8), 0, false, kState},
    {"Scena03_SpawnAtMember", 0x544830, 0x154, kCalls544830, SH_N(kCalls544830), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_SpawnAtMember), 0xFF, false, kEntry},
    {"Scena03_BobParty4", 0x544990, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_BobParty4), 0, false, kState},
    {"Scena03_BobParty2", 0x5449C0, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_BobParty2), 0, false, kState},
    {"Scena03_ObjectHook", 0x5449F0, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena03_ObjectHook), 0, false, kObject},
    {"Scena03_Object0", 0x544A10, 0x23, kCalls544A10, SH_N(kCalls544A10), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object0), 0, false, kEntry},
    {"Scena03_Object1", 0x544A40, 0x14, kCalls544A40, SH_N(kCalls544A40), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object1), 0xFF, false, kEntry},
    {"Scena03_Object2", 0x544A60, 0x14, kCalls544A60, SH_N(kCalls544A60), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object2), 0xFF, false, kEntry},
    {"Scena03_Object3", 0x544A80, 0x14, kCalls544A80, SH_N(kCalls544A80), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object3), 0xFF, false, kEntry},
    {"Scena03_Object4", 0x544AA0, 0x14, kCalls544AA0, SH_N(kCalls544AA0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object4), 0xFF, false, kEntry},
    {"Scena03_ObjectsAllFour", 0x544AC0, 0x26, kCalls544AC0, SH_N(kCalls544AC0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_ObjectsAllFour), 0xFF, false, kState},
    {"Scena03_Object5", 0x544AF0, 0x4A, kCalls544AF0, SH_N(kCalls544AF0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object5), 0, false, kEntry},
    {"Scena03_Object7", 0x544B40, 0x16, kCalls544B40, SH_N(kCalls544B40), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object7), 0xFF, false, kEntry},
    {"Scena03_Object8", 0x544B60, 0x1F, kCalls544B60, SH_N(kCalls544B60), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Object8), 0xFF, false, kEntry},
    {"Scena03_StepHook", 0x544B80, 0xF3, kCalls544B80, SH_N(kCalls544B80), nullptr, 0, kTables544B80, SH_N(kTables544B80), SC3_OURS(Scena03_StepHook), 0xFF, false, kHook},
    {"Scena03_StepArea33", 0x544C80, 0x7F, kCalls544C80, SH_N(kCalls544C80), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea33), 0xFF, false, kHook},
    {"Scena03_StepArea29", 0x544D00, 0x50, kCalls544D00, SH_N(kCalls544D00), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea29), 0xFF, false, kHook},
    {"Scena03_StepArea25", 0x544D50, 0x47, kCalls544D50, SH_N(kCalls544D50), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea25), 0xFF, false, kHook},
    {"Scena03_StepArea45", 0x544DA0, 0x8C, kCalls544DA0, SH_N(kCalls544DA0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea45), 0xFF, false, kHook},
    {"Scena03_StepArea32", 0x544E30, 0x45, kCalls544E30, SH_N(kCalls544E30), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea32), 0xFF, false, kHook},
    {"Scena03_StepArea63", 0x544E80, 0xBD, kCalls544E80, SH_N(kCalls544E80), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_StepArea63), 0xFF, false, kHook},
    {"Scena03_ArriveHook", 0x544F40, 0x20, kCalls544F40, SH_N(kCalls544F40), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_ArriveHook), 0xFF, false, kHook},
    {"Scena03_ArriveArea47", 0x544F60, 0x4D, kCalls544F60, SH_N(kCalls544F60), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_ArriveArea47), 0xFF, false, kHook},
    {"Scena03_CellHook", 0x544FB0, 0x2D, kCalls544FB0, SH_N(kCalls544FB0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_CellHook), 0xFF, false, kHook},
    {"Scena03_Cell0", 0x544FE0, 0x2E, kCalls544FE0, SH_N(kCalls544FE0), nullptr, 0, nullptr, 0, SC3_OURS(Scena03_Cell0), 0, false, kState},
};
// Chapter 4's 14, with Cond_ByteFA 4 (its row 0x903FB0). Scena04_Message is
// chapter 4's (chapter 3's hooks call it too; it reads no chapter byte).
const sh::Clone kClones4[] = {
    {"Scena04_Frame", 0x545010, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Frame), 0, false, kSlot},
    {"Scena04_Start", 0x545020, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Start), 0, false, kState},
    {"Scena04_EnterArea", 0x545040, 0x2E1, kCalls545040, SH_N(kCalls545040), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_EnterArea), 0, false, kState},
    {"Scena04_Run", 0x545330, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Run), 0, false, kState},
    {"Scena04_Scene1", 0x545340, 0x684, kCalls545340, SH_N(kCalls545340), nullptr, 0, kTables545340, SH_N(kTables545340), SC3_OURS(Scena04_Scene1), 0, false, kState},
    {"Scena04_Scene2", 0x5459D0, 0x754, kCalls5459D0, SH_N(kCalls5459D0), nullptr, 0, kTables5459D0, SH_N(kTables5459D0), SC3_OURS(Scena04_Scene2), 0, false, kState},
    {"Scena04_Tremor", 0x546130, 0x27, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Tremor), 0, false, kState},
    {"Scena04_ObjectHook", 0x546160, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, SC3_OURS(Scena04_ObjectHook), 0, false, kObject},
    {"Scena04_Object0", 0x546180, 0x12, kCalls546180, SH_N(kCalls546180), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Object0), 0xFF, false, kEntry},
    {"Scena04_StepHook", 0x5461A0, 0x20, kCalls5461A0, SH_N(kCalls5461A0), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_StepHook), 0xFF, false, kHook},
    {"Scena04_StepArea28", 0x5461C0, 0x15F, kCalls5461C0, SH_N(kCalls5461C0), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_StepArea28), 0xFF, false, kHook},
    {"Scena04_Message", 0x546320, 0x20, kCalls546320, SH_N(kCalls546320), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Message), 0xFF, false, kEntry},
    {"Scena04_CellHook", 0x546340, 0x2D, kCalls546340, SH_N(kCalls546340), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_CellHook), 0xFF, false, kHook},
    {"Scena04_Cell0", 0x546370, 0x1D, kCalls546370, SH_N(kCalls546370), nullptr, 0, nullptr, 0, SC3_OURS(Scena04_Cell0), 0, false, kState},
};
constexpr unsigned kCount3 = sizeof kClones3 / sizeof kClones3[0];
constexpr unsigned kCount4 = sizeof kClones4 / sizeof kClones4[0];

// --- the object handler tables ---------------------------------------------------
//
// A .data table's handler recorder logs no arguments, and the object handlers
// take (object, row): a typed stand-in of the fuzz's own is written into each
// entry by the seed (the tables are regions, put back after the run), each
// logging against the handler's own address. Scena03_ObjectHandlers' entry 6
// is the bare ret 0x437CC0, which is also Scena03_Runs entry 0 (a handler
// recorder's): its stand-in logs against the object hook 0x5449F0 instead,
// an address no clone calls.
constexpr std::uint32_t kObjAddr3[9] = {0x544A10, 0x544A40, 0x544A60, 0x544A80, 0x544AA0, 0x544AF0, 0x5449F0, 0x544B40, 0x544B60};
constexpr std::uint32_t kObjAddr4 = 0x546180;
template <unsigned I> void __cdecl ObjectEntry3(unsigned char* object, unsigned char* row) {
    sh::Record(kObjAddr3[I], Key(object), Key(row));
    sh::Stir();
}
void __cdecl ObjectEntry4(unsigned char* object, unsigned char* row) {
    sh::Record(kObjAddr4, Key(object), Key(row));
    sh::Stir();
}
const void* const kObjEntries3[9] = {
    reinterpret_cast<const void*>(&ObjectEntry3<0>), reinterpret_cast<const void*>(&ObjectEntry3<1>),
    reinterpret_cast<const void*>(&ObjectEntry3<2>), reinterpret_cast<const void*>(&ObjectEntry3<3>),
    reinterpret_cast<const void*>(&ObjectEntry3<4>), reinterpret_cast<const void*>(&ObjectEntry3<5>),
    reinterpret_cast<const void*>(&ObjectEntry3<6>), reinterpret_cast<const void*>(&ObjectEntry3<7>),
    reinterpret_cast<const void*>(&ObjectEntry3<8>),
};

// --- the callees -------------------------------------------------------------------
//
// Beyond the harness's standard 70 (docs/scenario_harness.md section 4), which
// already record every named engine callee these call and 0x4410B0,
// 0x532ED0, 0x56D6F0, Scenario_CallB and EventObj_Face.
constexpr std::uint32_t kAll = 0xFFFFFFFF;
#define SC3_CAPCOM(name, address, n, ...) {name, address, address, n, {__VA_ARGS__}, sh::Answer::kGarbage, 0, 0}
#define SC3_OURS_CALLEE(fn, n, ...) {#fn, ::bof3::addr::fn, Key(reinterpret_cast<const void*>(&::fn)), n, {__VA_ARGS__}, sh::Answer::kGarbage, 0, 0}

sh::Callee Answering(sh::Callee c, sh::Answer a, std::uint8_t lo = 0, std::uint8_t hi = 0) {
    c.answer = a;
    c.lo = lo;
    c.hi = hi;
    return c;
}
sh::Callee Custom(const char* name, std::uint32_t address, const void* fn) {
    sh::Callee c = {name, address, address, 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0};
    c.custom = fn;
    return c;
}

sh::Callee g_callees[64];
unsigned g_n_callees = 0;
void Add(const sh::Callee& c) {
    if (g_n_callees == sizeof g_callees / sizeof g_callees[0]) bof3::Fatal("scena_sc3: more than 64 callees");
    g_callees[g_n_callees++] = c;
}

void ListCallees(int chapter) {
    g_n_callees = 0;
    // The standard set logs EventObj_SetFlags' pointer; the originals' points
    // into their own frame: log the byte it points at.
    {
        sh::Callee c = SC3_OURS_CALLEE(EventObj_SetFlags, 1, 0);
        c.deref[0] = 1;
        Add(c);
    }
    // Nobody's, by address (scena_sc3_callees.h).
    Add(SC3_CAPCOM("0x533E50", kPartyRestore, 0));
    // an index below n (1) or 0xFF; the callers test al's sign
    Add(Answering(SC3_CAPCOM("0x56D800", kCellFind, 4, kAll, 0xFF, kAll, kAll), sh::Answer::kByte, 0xFF, 0x00));
    // a Sprite_Objects index 0..0x1D, or 0xFF none
    Add(Answering(SC3_CAPCOM("0x57CD90", kSpriteFindFree, 0), sh::Answer::kByte, 0xFF, 0x1D));
    Add(SC3_CAPCOM("0x587B80", kMusicStop, 0));
    Add(SC3_CAPCOM("0x590C90", kItemEvent, 4, 0xFF, kAll, 0xFF, kAll));
    Add(SC3_CAPCOM("0x591900", kKeyItemAdd, 1, 0xFF));
    // Ours the originals' E8 / E9 reach: those whose answer is read answer
    // as their callers read it; the rest log the chapter bytes (kPhase).
    Add(Answering(SC3_OURS_CALLEE(Scena03_SpawnAtMember, 1, 0xFF), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena03_BobParty4, 0), sh::Answer::kPhase));
    Add(Answering(SC3_OURS_CALLEE(Scena03_BobParty2, 0), sh::Answer::kPhase));
    Add(Answering(SC3_OURS_CALLEE(Scena03_ObjectsAllFour, 0), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena03_StepArea33, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena03_StepArea29, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena03_StepArea25, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena03_StepArea45, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena03_StepArea32, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena03_StepArea63, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena03_ArriveArea47, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena04_Tremor, 0), sh::Answer::kPhase));
    Add(Answering(SC3_OURS_CALLEE(Scena04_StepArea28, 2, kAll, kAll), sh::Answer::kFlag));
    Add(Answering(SC3_OURS_CALLEE(Scena04_Message, 1, 0xFF), sh::Answer::kFlag));
    // The object handler tables' stand-ins.
    if (chapter == 3) {
        static const char* const kNames[9] = {"ObjectHandlers3[0]", "ObjectHandlers3[1]", "ObjectHandlers3[2]", "ObjectHandlers3[3]",
                                              "ObjectHandlers3[4]", "ObjectHandlers3[5]", "ObjectHandlers3[6]", "ObjectHandlers3[7]",
                                              "ObjectHandlers3[8]"};
        for (unsigned i = 0; i < 9; ++i) Add(Custom(kNames[i], kObjAddr3[i], kObjEntries3[i]));
    } else {
        Add(Custom("ObjectHandlers4[0]", kObjAddr4, reinterpret_cast<const void*>(&ObjectEntry4)));
    }
}

// --- the tables ------------------------------------------------------------------
//
// Swapped for recorders while the fuzz runs (no arguments). Each chapter's
// state table runs on into its run table (the frame's index 3.. reads runs),
// so one window holds both: the seeds keep the indices inside.
const sh::DataTable kTables3[] = {{at::kStates3, 12}, {at::kCellHandlers3, 1}};
const sh::DataTable kTables4[] = {{at::kStates4, 6}, {at::kCellHandlers4, 1}};

// --- the regions -------------------------------------------------------------------
//
// Beyond the harness's 22 standard ones: the cells of the chapters' own.
const sh::Region kRegions3[] = {
    {at::kAreaTrack, 1}, {at::kAreaTransition, 1}, {at::kMusicTrack, 1}, {at::kByteFE, 1}, {at::kMessage, 2},
    {at::kGameMode, 2}, {0x929F00, 0x14}, {at::kEffectState, 4}, {at::kActiveMember, 4}, {at::kObjects3, 9 * 4},
};
const sh::Region kRegions4[] = {
    {at::kAreaTrack, 1}, {at::kAreaTransition, 1}, {at::kMusicTrack, 1}, {at::kByteFE, 1}, {at::kMessage, 2},
    {at::kGameMode, 2}, {0x929F00, 0x14}, {at::kEffectState, 4}, {at::kActiveMember, 4}, {at::kObjects4, 4},
};

// --- the seed ----------------------------------------------------------------------
//
// Every constant the fifty compare a cell with, picked often; the step
// within each scene's cases (and one past); the state and run inside the
// swapped windows; the object handler stand-ins into their tables.
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
// A 16.16 coordinate at the exact values and cells the hooks test.
std::uint32_t PickCoord() {
    if (sh::Half()) {
        return SH_PICK(0x5C0000, 0x418000, 0x3A8000, 0x28000, 0x4C8000, 0xA8000, 0xB8000, 0x490000, 0x60000, 0x40000, 0x4D8000,
                       0x248000);
    }
    const std::uint32_t hi = SH_PICK(1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xD, 0xE, 0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x17, 0x18, 0x19,
                                     0x1A, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x33, 0x34, 0x35, 0x36, 0x3B, 0x3C,
                                     0x3D, 0x3E, 0x56, 0x57, 0x5A, 0x5B);
    return (hi << 16) | (sh::Half() ? 0 : (sh::Half() ? 0x8000 : (sh::Next() & 0xFFFF)));
}

// The steps each scene's switch holds (0: none).
unsigned StepSpan(const char* name) {
    struct Span { const char* name; unsigned span; };
    static const Span kSpans[] = {
        {"Scena03_Scene1", 8}, {"Scena03_Scene2", 2}, {"Scena03_Scene3", 0x33}, {"Scena03_Scene4", 0xB}, {"Scena03_Scene5", 0x12},
        {"Scena03_Scene6", 0x1D}, {"Scena03_Scene7", 0xE}, {"Scena03_Scene8", 0x15}, {"Scena04_Scene1", 0x1F}, {"Scena04_Scene2", 0x1F},
    };
    for (const Span& s : kSpans)
        if (std::strcmp(s.name, name) == 0) return s.span;
    return 0;
}

// Each scene's steps that wait on a counter, and the value compared: {step,
// counter 0 / 1, value}. Read off the cases (the first compare of 0x903848 /
// 0x903849 in each, capstone, the scratch stepcmp.py), 2026-09-27.
struct StepWait { unsigned char step, counter, value; };
struct SceneWaits { const char* name; StepWait waits[16]; };
const SceneWaits kWaits[] = {
    {"Scena03_Scene1", {{1, 0, 0x28}, {2, 0, 0x29}, {3, 0, 0x2A}, {4, 0, 0x32}, {7, 0, 0x2}}},
    {"Scena03_Scene2", {{1, 0, 0x28}}},
    {"Scena03_Scene3", {{2, 0, 0x22}, {5, 0, 0x2}, {6, 0, 0x18}, {7, 0, 0x25}, {8, 1, 0x15}, {9, 0, 0x2B}, {11, 0, 0xF}, {12, 0, 0x14},
                        {14, 0, 0x1D}, {16, 0, 0x1F}, {24, 0, 0x55}, {26, 0, 0x58}, {50, 0, 0x9}}},
    {"Scena03_Scene4", {{1, 0, 0x4}, {8, 0, 0xF}}},
    {"Scena03_Scene5", {{2, 0, 0x8}, {4, 0, 0x9}, {8, 0, 0x2}, {9, 0, 0x3}, {12, 0, 0x5}, {13, 0, 0x6}, {14, 0, 0x15}, {15, 0, 0x7}}},
    {"Scena03_Scene6", {{3, 0, 0xA}, {7, 0, 0x5}, {12, 0, 0x8}, {16, 0, 0x1}, {22, 0, 0x4}, {25, 0, 0x9}, {28, 0, 0xD}}},
    {"Scena03_Scene7", {{1, 0, 0x2}, {4, 0, 0x7}, {5, 0, 0x12}, {6, 0, 0x16}, {7, 0, 0x18}, {8, 0, 0x27}, {11, 0, 0x2F}, {13, 0, 0x34}}},
    {"Scena03_Scene8", {{4, 0, 0x10}, {5, 0, 0x13}, {14, 0, 0x15}, {15, 0, 0x15}, {16, 0, 0x17}, {19, 0, 0x1C}, {20, 0, 0x27}}},
    {"Scena04_Scene1", {{2, 0, 0x1}, {3, 0, 0x2}, {7, 0, 0x14}, {14, 0, 0x23}, {17, 0, 0x2B}, {19, 0, 0x1}, {21, 0, 0x5}, {22, 0, 0x2},
                        {23, 0, 0x1}, {24, 0, 0x9}}},
    {"Scena04_Scene2", {{1, 0, 0xC}, {2, 0, 0xF}, {6, 0, 0x11}, {14, 0, 0x22}, {15, 0, 0x23}, {17, 0, 0x2}, {20, 0, 0x3}, {22, 0, 0x11}}},
};
const SceneWaits* WaitsOf(const char* name) {
    for (const SceneWaits& w : kWaits)
        if (std::strcmp(w.name, name) == 0) return &w;
    return nullptr;
}

// The area tests' (x, z) where they turn: an exact coordinate or a cell range
// (its high word; the fraction 0, 0x8000 or any), one cell either side too.
struct Hit { std::uint32_t x0, x1, z0, z1; bool x_exact, z_exact; };
struct AreaHits { const char* name; Hit hits[4]; };
const AreaHits kHits[] = {
    {"Scena03_StepArea33", {{2, 4, 0x60000, 0x60000, false, true}, {3, 4, 0x40000, 0x40000, false, true}}},
    {"Scena03_StepArea29", {{0x5C0000, 0x5C0000, 7, 8, true, false}}},
    {"Scena03_StepArea25", {{0x20, 0x23, 0x418000, 0x418000, false, true}}},
    {"Scena03_StepArea45", {{0x3A8000, 0x3A8000, 0x3C, 0x3D, true, false}, {0x1F, 0x20, 0x28000, 0x28000, false, true},
                            {0x1F, 0x20, 0x4C8000, 0x4C8000, false, true}}},
    {"Scena03_StepArea32", {{0x15, 0x15, 0x34, 0x35, false, false}}},
    {"Scena03_StepArea63", {{0xA8000, 0xA8000, 0x24, 0x25, true, false}, {5, 6, 0xB8000, 0xB8000, false, true}}},
    {"Scena03_ArriveArea47", {{0xE, 0x11, 0x490000, 0x490000, false, true}}},
    {"Scena04_StepArea28", {{0x57, 0x5A, 0x13, 0x15, false, false}, {0x57, 0x5A, 0xB, 0x10, false, false},
                            {0x4D8000, 0x4D8000, 0x18, 0x19, true, false}, {0x1F, 0x21, 0x248000, 0x248000, false, true}}},
};
std::uint32_t Coord(std::uint32_t lo, std::uint32_t hi, bool exact) {
    if (exact) return lo;
    std::uint32_t cell = lo + sh::Next() % (hi - lo + 1);
    if (sh::Next() % 4 == 0) cell = sh::Half() ? cell - 1 : hi + 1;
    const std::uint32_t frac = sh::Half() ? 0 : (sh::Half() ? 0x8000 : (sh::Next() & 0xFFFF));
    return (cell & 0xFFFF) << 16 | frac;
}

const sh::Clone* g_clones = kClones3;
int g_chapter = 3;
unsigned g_k = 0;
bool Named(const char* part) { return std::strstr(g_clones[g_k].name, part) != nullptr; }

void Seed(unsigned k) {
    g_k = k;
    const bool four = g_chapter == 4;
    // the tables' indices, inside the swapped windows
    sh::Mem(at::kState)[0] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 3 : sh::Next() % (four ? 6 : 12));
    sh::Mem(at::kRun)[0] = static_cast<unsigned char>(sh::Next() % (four ? 3 : 9));
    // the step: every case, one past, else any byte
    if (const unsigned span = StepSpan(g_clones[k].name); span && sh::Next() % 6)
        sh::Mem(at::kStep)[0] = static_cast<unsigned char>(sh::Next() % (span + 1));
    if (sh::Often()) SetWord(sh::Mem(at::kTimer), PickTimer());
    sh::Mem(at::kEffectByte)[0] = static_cast<unsigned char>(sh::Next() % 6 ? sh::Next() % 0x14 : 0xFF);
    // the counters the steps wait on
    if (sh::Next() % 6) sh::Mem(at::kCounter0)[0] = static_cast<unsigned char>(PickC0());
    if (sh::Often()) sh::Mem(at::kCounter1)[0] = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3, 0x15));
    // the area, Cond_ByteFD, the bytes the entries and hooks test
    if (sh::Often()) SetWord(sh::Mem(at::kArea), PickArea());
    if (sh::Often()) sh::Mem(at::kByteFD)[0] = static_cast<unsigned char>(sh::Next() % 8);
    if (sh::Half()) sh::Mem(at::kKind2Mode)[0] = 1;
    if (sh::Half()) sh::Mem(at::kRow3Byte2)[0] = static_cast<unsigned char>(sh::Mem(at::kRow3Byte2)[0] | 0xF);
    if (sh::Often()) sh::Mem(at::kLeaderKind)[0] = static_cast<unsigned char>(3 + sh::Next() % 5);
    // the waits
    if (sh::Half()) sh::Mem(at::kRequest)[0] = 2;
    if (sh::Half()) SetWord(sh::Mem(at::kWait), 0);
    if (sh::Half()) SetWord(sh::Mem(at::kMessage), SH_PICK(1, 0xC, 0x11, 0x12, 0x1F, 0x33));
    if (sh::Half()) sh::Mem(at::kKind2Hold)[0] = 0;
    if (sh::Half()) SetWord(sh::Mem(at::kGameMode), 2);
    if (sh::Half()) SetWord(sh::Mem(at::kCamDistance), SH_PICK(0, 0x10, 0x22, 0x3F8, 0x3FF, 0x400, 0x7FF0));
    // the object handlers' index in the records the kObject shape passes
    for (unsigned i = 0; i < 4; ++i) sh::SpriteRecord(i)[0x86] = static_cast<unsigned char>(four ? 0 : sh::Next() % 9);
    // the object handler tables' stand-ins
    if (four) {
        SetLong(sh::Mem(at::kObjects4), static_cast<std::int32_t>(Key(reinterpret_cast<const void*>(&ObjectEntry4))));
    } else {
        for (unsigned i = 0; i < 9; ++i) SetLong(sh::Mem(at::kObjects3 + 4 * i), static_cast<std::int32_t>(Key(kObjEntries3[i])));
    }
    // a step that waits on a counter, with the counter at (or next to) the value
    if (const SceneWaits* w = WaitsOf(g_clones[k].name); w && sh::Half()) {
        unsigned n = 0;
        while (n < 16 && (w->waits[n].value || w->waits[n].step)) ++n;
        const StepWait& s = w->waits[sh::Next() % n];
        sh::Mem(at::kStep)[0] = s.step;
        const unsigned jitter = sh::Next() % 5;
        sh::Mem(s.counter ? at::kCounter1 : at::kCounter0)[0] =
            static_cast<unsigned char>(s.value + (jitter == 0 ? 1u : jitter == 1 ? 0xFFu : 0u));
    }
    // a scene's timer at its end (the waits test 0; step 3 of run 3 decrements first)
    if (StepSpan(g_clones[k].name) && sh::Next() % 3 == 0) SetWord(sh::Mem(at::kTimer), SH_PICK(0, 1, 2));
    // run 6's step 0x13 at its first sound, run 3's steps that play the track
    if (Named("Scena03_Scene6") && sh::Next() % 3 == 0) {
        sh::Mem(at::kStep)[0] = 0x13;
        SetWord(sh::Mem(at::kTimer), SH_PICK(0x32, 0x32, 0xA, 1));
    }
    if (Named("Scena04_Scene2") && sh::Next() % 4 == 0) {
        sh::Mem(at::kStep)[0] = 0x15;
        SetWord(sh::Mem(at::kTimer), SH_PICK(0x14, 0x15, 0x20));
    }
    if (Named("Scena03_Scene3") && sh::Next() % 5 == 0) sh::Mem(at::kStep)[0] = static_cast<unsigned char>(SH_PICK(0xF, 0x19));
    // an area the entry knows
    if (Named("EnterArea") && sh::Often())
        SetWord(sh::Mem(at::kArea), four ? SH_PICK(0x28, 0x2A, 0x30, 0x36, 0x41)
                                         : SH_PICK(0x25, 0x26, 0x27, 0x29, 0x2D, 0x32, 0x38, 0x45, 0x47, 0x63));
    // Scena04_Scene2's timed steps near their turns
    if (four && Named("Scene2") && sh::Half()) {
        sh::Mem(at::kStep)[0] = static_cast<unsigned char>(SH_PICK(8, 9, 0xA, 0xB, 0xC, 0xD, 0x10, 0x12, 0x13, 0x15));
        SetWord(sh::Mem(at::kTimer), SH_PICK(0, 1, 2, 0xD, 0x13, 0x14, 0x23, 0x24));
    }
    // Scena03_Scene8's shaking steps at their turns
    if (!four && Named("Scene8") && sh::Half()) {
        sh::Mem(at::kStep)[0] = static_cast<unsigned char>(7 + sh::Next() % 7);
        SetWord(sh::Mem(at::kTimer), SH_PICK(0x1F, 0x20, 0x2F, 0x30, 0x4F, 0x50, 0x8F, 0x90, 0x9F, 0xA0, 0x10F, 0x110, 0x12F, 0x130));
    }
}

// Function k's arguments beyond the shape's: the hooks' and area tests' (x, z)
// at the values tested, the object handlers' object and row, the member, the id.
void Args(unsigned k, std::uint32_t* a) {
    g_k = k;
    const sh::Clone& c = g_clones[k];
    if (c.shape == sh::Shape::kHook && sh::Often()) {
        a[0] = PickCoord();
        a[1] = PickCoord();
    }
    for (const AreaHits& h : kHits) {
        if (std::strcmp(h.name, c.name) != 0 || !sh::Often()) continue;
        unsigned n = 0;
        while (n < 4 && h.hits[n].x0) ++n;
        const Hit& t = h.hits[sh::Next() % n];
        a[0] = Coord(t.x0, t.x1, t.x_exact);
        a[1] = Coord(t.z0, t.z1, t.z_exact);
    }
    if (Named("_Object") && c.shape == sh::Shape::kEntry) a[0] = Key(sh::SpriteRecord(sh::Next()));
    if (Named("SpawnAtMember")) a[0] = (sh::Next() & 0xFFFFFF00u) | (sh::Next() % 3);
}

// After a call, two in three (the harness's own list moves the step, run,
// wait, row, Sprite_Current, Frame_Counter, counter 0, request, timer, script
// flags): a chapter cell of this group's the scenes and hooks read again.
// Only from the hash it is given: the harness's Next would differ between the
// passes.
void Disturb(std::uint32_t h) {
    static const unsigned char kC1[] = {0, 1, 2, 0x15};
    static const unsigned char kAreas[] = {0x25, 0x28, 0x29, 0x2A, 0x2D, 0x30, 0x32, 0x33, 0x36, 0x38, 0x41, 0x45, 0x47, 0x63};
    // the timer, counter 0 and the step to values the cases compare (the
    // harness's own moves them to any value)
    static const unsigned short kTimers[] = {0, 1, 2, 0xA, 0xC, 0xD, 0x13, 0x14, 0x20, 0x32};
    static const unsigned char kC0[] = {0, 1, 2, 5, 9, 0xF, 0x13, 0x14, 0x15, 0x1C, 0x22, 0x28, 0x2B, 0x32};
    // the function being fuzzed reads this one again after a call: half the
    // time, move it (area 0x63's test Cond_ByteFD after Flags_Test; run 6
    // step 0x13 the timer after the first sound; run 3 counter 0 after the
    // music)
    if (h & 0x1000000) {
        if (Named("StepArea63")) {
            sh::Mem(at::kByteFD)[0] = static_cast<unsigned char>(2 + ((h >> 17) & 1));
            return;
        }
        if (Named("Scena03_Scene6")) {
            SetWord(sh::Mem(at::kTimer), 0xA);
            return;
        }
        if (Named("Scena04_Scene2")) {   // step 0x15's exit reads the step again after the tremor
            sh::Mem(at::kStep)[0] = static_cast<unsigned char>(2 + (h >> 17) % 11);
            return;
        }
        if (Named("Scena03_Scene3")) {
            sh::Mem(at::kCounter0)[0] = static_cast<unsigned char>(h & 0x20000 ? 0x1F : 0x1E);
            return;
        }
    }
    switch ((h >> 8) % 10) {
    case 0: sh::Mem(at::kCounter1)[0] = h & 0x10000 ? kC1[(h >> 17) % sizeof kC1] : static_cast<unsigned char>(h >> 24); break;
    case 1: SetWord(sh::Mem(at::kArea), h & 0x10000 ? kAreas[(h >> 17) % sizeof kAreas] : h >> 16); break;
    case 2: sh::Mem(at::kEffects + (h >> 12) % 0xA00)[0] = static_cast<unsigned char>(h >> 24); break;
    case 3: sh::Mem(at::kStatusBits)[0] = static_cast<unsigned char>(h >> 16); break;
    case 4: SetWord(sh::Mem(at::kCamDistance), h >> 16); break;
    case 5: SetWord(sh::Mem(at::kGameMode), (h >> 16) & 3); break;
    case 6: SetWord(sh::Mem(at::kTimer), kTimers[(h >> 16) % (sizeof kTimers / sizeof kTimers[0])]); break;
    case 7: sh::Mem(at::kCounter0)[0] = kC0[(h >> 16) % sizeof kC0]; break;
    case 8: sh::Mem(at::kStep)[0] = static_cast<unsigned char>(2 + (h >> 16) % 0x15); break;
    default: sh::Mem(at::kByteFD)[0] = static_cast<unsigned char>(h & 0x10000 ? 2 + ((h >> 17) & 1) : (h >> 18) & 7); break;
    }
}

void RunChapter(int chapter, const sh::Clone* clones, unsigned n, const sh::DataTable* tables, unsigned n_tables,
                const sh::Region* regions, unsigned n_regions, const char* shadow) {
    g_clones = clones;
    g_chapter = chapter;
    ListCallees(chapter);
    sh::Group group = {shadow, clones, n, g_callees, g_n_callees, tables, n_tables, regions, n_regions, &Seed, &Disturb, 2000};
    group.args = &Args;
    group.chapter = chapter;
    sh::Run(group);
}

}  // namespace

// Two runs of the harness, one per chapter byte (a Group takes one chapter):
// chapter 3's 36 functions with Cond_ByteFA 3, chapter 4's 14 with 4.
void SelfTest() {
    RunChapter(3, kClones3, kCount3, kTables3, sizeof kTables3 / sizeof kTables3[0], kRegions3,
               sizeof kRegions3 / sizeof kRegions3[0], "scena_sc3 (chapter 3)");
    RunChapter(4, kClones4, kCount4, kTables4, sizeof kTables4 / sizeof kTables4[0], kRegions4,
               sizeof kRegions4 / sizeof kRegions4[0], "scena_sc3 (chapter 4)");
}

}  // namespace scena_sc3
