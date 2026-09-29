// BOF3X_SHADOW=scena_sc6: chapter 6's bank through the scenario harness
// (scenario_harness.h), once at start-up. docs/scena_sc6.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC6 --clones at 3e410e7,
// checked against a capstone reading of every function: the same 48 starts,
// extents, calls and jump tables), each clone's call shape in its comment;
// the callees the standard set lacks or records otherwise; the state and run
// tables swapped for recorders, typed stand-ins written into the object,
// cell-handler and leap-phase tables; the regions beyond the standard ones; a
// seed per role; a disturbance of the chapter's cells.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc6.h"
#include "game/scena_sc6_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc6 {
namespace {

namespace sh = scenario_harness;

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])

// tools/scenario_rows.py --unit SC6 --clones (3e410e7), 2026-09-27: every
// jump internal, nothing REFUSED; the switches' tables bounded at their cmp,
// their byte tables counted in. The shapes: the vtable slots 0 (no
// arguments), 1 (the object), 2 and 4 (hooks (x, z) answering in al); the
// states, runs and object handlers (no arguments read); the cell handlers
// ((x, z) in place, al); the leap and its phases (seven words, al); the
// helpers called by E8 (none, or the one word of Scena06_LeaderEffect).
constexpr sh::CallSite kCalls54A920[] = {{0x0, 0x54E700}};
constexpr sh::CallSite kCalls54A930[] = {{0x19, 0x57C140}, {0xED, 0x5341C0}, {0x10B, 0x57C140}, {0x124, 0x57C140}, {0x13A, 0x57C7C0}, {0x16D, 0x57C0F0}, {0x18C, 0x5341C0}, {0x197, 0x5341C0}, {0x1A2, 0x5341C0}, {0x1AD, 0x5341C0}, {0x1B8, 0x5341C0}, {0x1BF, 0x5341A0}, {0x1DA, 0x57C140}, {0x1E7, 0x5341A0}, {0x210, 0x57C140}, {0x224, 0x57C140}, {0x237, 0x54E3B0}, {0x24F, 0x57C140}, {0x264, 0x57C140}, {0x290, 0x57C0F0}, {0x298, 0x56D6F0}, {0x2C1, 0x57C140}, {0x2D6, 0x57C0F0}, {0x2E8, 0x57C140}, {0x2FB, 0x57C7A0}, {0x350, 0x532ED0}, {0x373, 0x57C140}, {0x3A9, 0x57C140}, {0x3D1, 0x54E3B0}, {0x3E0, 0x57C140}, {0x3FC, 0x57C140}, {0x411, 0x57C140}, {0x425, 0x57C0F0}, {0x433, 0x54E3B0}};
constexpr sh::JumpTable kTables54A930[] = {{0x186, 0x488, 6}};
constexpr sh::CallSite kCalls54ADE0[] = {{0x2F, 0x531F90}, {0x5B, 0x57C0F0}, {0x71, 0x594E00}, {0x7D, 0x5734F0}, {0x8B, 0x57C0F0}, {0xD8, 0x594E00}, {0xFA, 0x495040}, {0x148, 0x57C110}, {0x156, 0x57C0F0}, {0x15E, 0x57C7A0}};
constexpr sh::JumpTable kTables54ADE0[] = {{0x14, 0x178, 11}};
constexpr sh::CallSite kCalls54AF90[] = {{0x3B, 0x531F90}, {0x61, 0x4976D0}, {0xBA, 0x531F90}, {0xCD, 0x531F90}, {0xE0, 0x531F90}, {0x121, 0x531F90}, {0x13F, 0x589810}, {0x1E2, 0x589810}, {0x24F, 0x57C0F0}, {0x257, 0x57C7A0}};
constexpr sh::JumpTable kTables54AF90[] = {{0x20, 0x270, 12}};
constexpr sh::CallSite kCalls54B250[] = {{0x30, 0x594E00}, {0x58, 0x5734F0}, {0x76, 0x589810}, {0xF2, 0x587AE0}, {0x117, 0x589810}, {0x17A, 0x587B40}, {0x184, 0x587740}, {0x1A6, 0x587AE0}, {0x1B0, 0x587740}, {0x1D6, 0x589810}, {0x25A, 0x57C0F0}, {0x270, 0x594E00}, {0x27D, 0x5734F0}, {0x285, 0x589810}, {0x2F1, 0x589810}, {0x36D, 0x57C0F0}, {0x37B, 0x57C0F0}, {0x382, 0x591900}, {0x3E9, 0x594E00}, {0x3F3, 0x533E50}, {0x3F8, 0x57C7A0}};
constexpr sh::JumpTable kTables54B250[] = {{0x14, 0x428, 16}};
constexpr sh::CallSite kCalls54B6C0[] = {{0x21, 0x4976D0}, {0x47, 0x531F90}, {0xB6, 0x495040}, {0xDC, 0x4976D0}, {0x116, 0x57C0F0}, {0x12C, 0x594E00}, {0x14B, 0x57C0F0}, {0x153, 0x57C7A0}, {0x176, 0x4976D0}, {0x19C, 0x531F90}, {0x1C2, 0x57C0F0}, {0x1CA, 0x57C7A0}, {0x1ED, 0x4976D0}, {0x20F, 0x531F90}, {0x230, 0x57C0F0}, {0x238, 0x57C7A0}, {0x25B, 0x4976D0}, {0x27B, 0x57C7A0}};
constexpr sh::JumpTable kTables54B6C0[] = {{0x1B, 0x290, 17}};
constexpr sh::CallSite kCalls54B9C0[] = {{0x19, 0x495040}, {0x55, 0x594E00}, {0x6D, 0x495040}, {0x72, 0x533E50}, {0x86, 0x57C0F0}, {0x8D, 0x587B40}, {0xA7, 0x587910}, {0xB7, 0x587A00}, {0xD5, 0x594E00}, {0xE5, 0x57C7A0}};
constexpr sh::JumpTable kTables54B9C0[] = {{0x13, 0x108, 6}};
constexpr sh::CallSite kCalls54BAE0[] = {{0x25, 0x531F90}, {0x58, 0x57C0F0}, {0x6E, 0x594E00}, {0x79, 0x4976D0}, {0x9F, 0x495040}, {0xE0, 0x589810}, {0x192, 0x5734F0}, {0x19A, 0x589810}, {0x242, 0x57C0F0}, {0x258, 0x594E00}, {0x27E, 0x57C0F0}, {0x286, 0x57C7A0}, {0x2A9, 0x531F90}, {0x2B0, 0x5734F0}, {0x322, 0x591BE0}, {0x34A, 0x57C0F0}, {0x352, 0x57C7A0}, {0x38B, 0x594E00}, {0x3AB, 0x57C7A0}};
constexpr sh::JumpTable kTables54BAE0[] = {{0x1F, 0x3C0, 20}};
constexpr sh::CallSite kCalls54BF20[] = {{0x70, 0x594E00}, {0x82, 0x4976D0}, {0xAF, 0x5734F0}, {0x14A, 0x57C0F0}, {0x160, 0x594E00}, {0x186, 0x57C0F0}, {0x1AA, 0x591BE0}, {0x1DA, 0x57C7A0}, {0x23B, 0x594E00}, {0x246, 0x4976D0}, {0x273, 0x5734F0}, {0x2FB, 0x57C0F0}, {0x311, 0x594E00}, {0x337, 0x531F90}, {0x354, 0x533E50}, {0x36D, 0x57C110}, {0x37B, 0x57C110}, {0x38E, 0x594E00}, {0x3AE, 0x57C0F0}, {0x3BB, 0x57C0F0}, {0x3D1, 0x594E00}, {0x41C, 0x594E00}, {0x42E, 0x57C7A0}};
constexpr sh::JumpTable kTables54BF20[] = {{0x1B, 0x4A8, 30}};
constexpr sh::CallSite kCalls54C480[] = {{0x29, 0x533E50}, {0x30, 0x5734F0}, {0x121, 0x532ED0}, {0x13A, 0x4410B0}, {0x151, 0x533E50}, {0x175, 0x594E00}, {0x192, 0x57C7A0}, {0x1AD, 0x531F90}, {0x1B4, 0x5734F0}, {0x1E4, 0x532ED0}, {0x20A, 0x4410B0}, {0x24D, 0x594E00}, {0x2A5, 0x4976D0}, {0x2D7, 0x533E50}, {0x2F3, 0x57C0F0}, {0x306, 0x594E00}, {0x3A7, 0x533E50}, {0x3C9, 0x594E00}, {0x3D9, 0x57C7A0}};
constexpr sh::JumpTable kTables54C480[] = {{0x22, 0x408, 20}};
constexpr sh::CallSite kCalls54C910[] = {{0x51, 0x57C0F0}, {0x7D, 0x594E00}, {0xC8, 0x57C0F0}, {0xF4, 0x594E00}, {0x114, 0x4410B0}, {0x127, 0x531F90}, {0x13C, 0x57C0F0}, {0x152, 0x594E00}, {0x170, 0x533E50}, {0x1A0, 0x594E00}, {0x1AC, 0x531F90}, {0x1CC, 0x495040}, {0x1D9, 0x57C0F0}, {0x201, 0x4976D0}, {0x226, 0x533E50}, {0x24A, 0x531F90}, {0x26A, 0x495040}, {0x278, 0x57C0F0}, {0x29C, 0x4976D0}, {0x2BD, 0x533E50}, {0x2D9, 0x57C110}, {0x2FA, 0x594E00}};
constexpr sh::JumpTable kTables54C910[] = {{0x14, 0x304, 24}};
constexpr sh::CallSite kCalls54CC80[] = {{0x2D, 0x531F90}, {0x4A, 0x587B80}, {0x51, 0x587910}, {0x61, 0x587A00}, {0x6A, 0x587B90}, {0x87, 0x57C7A0}, {0x9B, 0x531F90}, {0xA8, 0x57C0F0}, {0xC1, 0x57C7A0}};
constexpr sh::JumpTable kTables54CC80[] = {{0x13, 0xE4, 7}};
constexpr sh::CallSite kCalls54CD80[] = {{0x26, 0x57C140}, {0x34, 0x4976D0}, {0x46, 0x4976D0}, {0x63, 0x57C7A0}, {0x7D, 0x57C140}, {0x92, 0x57C140}, {0xA0, 0x4976D0}, {0xB9, 0x4976D0}, {0xF4, 0x531F90}, {0x126, 0x57C0F0}, {0x12E, 0x57C7A0}};
constexpr sh::JumpTable kTables54CD80[] = {{0x13, 0x150, 10}};
constexpr sh::CallSite kCalls54CF00[] = {{0x1F, 0x54E2F0}, {0x33, 0x531F90}, {0x39, 0x5734F0}, {0x59, 0x587B40}, {0x62, 0x587AE0}, {0x8C, 0x532ED0}, {0xAE, 0x4410B0}, {0xCD, 0x531F90}, {0xED, 0x495040}, {0x11C, 0x587B40}, {0x125, 0x587AE0}, {0x12C, 0x4976D0}, {0x153, 0x591CC0}, {0x1D6, 0x532ED0}, {0x1DF, 0x587AE0}, {0x219, 0x4410B0}, {0x239, 0x531F90}, {0x247, 0x57C0F0}, {0x255, 0x495040}, {0x28C, 0x57C0F0}, {0x2C0, 0x5341C0}, {0x2C7, 0x5341A0}, {0x2DD, 0x594E00}, {0x2FE, 0x594E00}, {0x325, 0x587AE0}, {0x341, 0x57C7A0}};
constexpr sh::JumpTable kTables54CF00[] = {{0x16, 0x354, 20}};
constexpr sh::CallSite kCalls54D2B0[] = {{0x1B, 0x4976D0}, {0x49, 0x579F00}, {0x51, 0x589810}, {0x7E, 0x57C110}, {0x8C, 0x57C0F0}, {0xA6, 0x57C140}, {0xC8, 0x57C140}, {0xE6, 0x579F00}, {0xF2, 0x57C0F0}, {0xFC, 0x587740}, {0x103, 0x531F90}, {0x12B, 0x57C0F0}, {0x135, 0x587740}, {0x13E, 0x587AE0}, {0x151, 0x579F00}, {0x15F, 0x57C140}, {0x17C, 0x57C0F0}, {0x186, 0x587740}, {0x190, 0x587740}, {0x199, 0x587AE0}, {0x1A0, 0x531F90}, {0x1AD, 0x57C110}, {0x1C6, 0x57C110}, {0x1E8, 0x57C110}, {0x204, 0x57C7A0}};
constexpr sh::JumpTable kTables54D2B0[] = {{0x15, 0x218, 9}};
constexpr sh::CallSite kCalls54D4F0[] = {{0x24, 0x531F90}, {0x2B, 0x5734F0}, {0x5D, 0x4976D0}, {0x7A, 0x4976D0}, {0x97, 0x4976D0}, {0xD9, 0x589810}, {0x189, 0x589810}, {0x239, 0x589810}, {0x31A, 0x54E2F0}, {0x32D, 0x54DA20}, {0x345, 0x531F90}, {0x37C, 0x587B40}, {0x383, 0x531F90}, {0x3A5, 0x5734F0}, {0x3D1, 0x532ED0}, {0x402, 0x4976D0}, {0x41F, 0x4976D0}, {0x43D, 0x4976D0}, {0x465, 0x4410B0}, {0x491, 0x57C0F0}, {0x4AD, 0x57C7A0}};
constexpr sh::JumpTable kTables54D4F0[] = {{0x1E, 0x4C4, 15}};
constexpr sh::CallSite kCalls54DA20[] = {{0x13, 0x4976D0}, {0x30, 0x4976D0}, {0x4D, 0x4976D0}};
constexpr sh::CallSite kCalls54DA90[] = {{0x44, 0x4976D0}, {0x6A, 0x589810}, {0x91, 0x531F90}, {0xA4, 0x57C0F0}, {0xAE, 0x587740}, {0xB8, 0x587740}, {0xD9, 0x495040}, {0xFB, 0x495040}, {0x102, 0x531F90}, {0x109, 0x5734F0}, {0x147, 0x589810}, {0x1C8, 0x589810}, {0x26F, 0x587B40}, {0x278, 0x587AE0}, {0x299, 0x587B40}, {0x2A0, 0x587910}, {0x2B2, 0x587A00}, {0x2C3, 0x587AE0}, {0x2FC, 0x4976D0}, {0x319, 0x4976D0}, {0x336, 0x4976D0}, {0x381, 0x57C0F0}, {0x38E, 0x590BB0}, {0x3A8, 0x57C7A0}, {0x3BE, 0x4976D0}};
constexpr sh::JumpTable kTables54DA90[] = {{0x1D, 0x3D8, 18}};
constexpr sh::CallSite kCalls54DED0[] = {{0x5A, 0x590BB0}, {0x7C, 0x591680}, {0xA9, 0x4976D0}, {0xF6, 0x590BB0}, {0x119, 0x591680}, {0x146, 0x4976D0}, {0x15A, 0x57C0F0}, {0x164, 0x587740}, {0x177, 0x497710}, {0x1A6, 0x57C7A0}};
constexpr sh::JumpTable kTables54DED0[] = {{0x1C, 0x1BC, 7}};
constexpr sh::CallSite kCalls54E0C0[] = {{0x2F, 0x594E00}, {0x3A, 0x495040}, {0x95, 0x589810}, {0xFF, 0x589810}, {0x181, 0x594E00}, {0x1B4, 0x57C0F0}, {0x1C7, 0x594E00}, {0x1D0, 0x57C7A0}};
constexpr sh::JumpTable kTables54E0C0[] = {{0x13, 0x1F8, 11}};
constexpr sh::CallSite kCalls54E2F0[] = {{0xA, 0x589810}};
constexpr sh::CallSite kCalls54E3B0[] = {{0x16, 0x5341C0}, {0x1D, 0x5341A0}, {0x28, 0x5341C0}, {0x2F, 0x5341A0}, {0x3A, 0x5341C0}, {0x41, 0x5341A0}, {0x4C, 0x5341C0}, {0x53, 0x5341A0}, {0x5E, 0x5341C0}, {0x65, 0x5341A0}};
constexpr sh::JumpTable kTables54E3B0[] = {{0x10, 0x70, 7}};
constexpr sh::CallSite kCalls54E460[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls54E490[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls54E4C0[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}};
constexpr sh::CallSite kCalls54E500[] = {{0x0, 0x57C7C0}, {0x31, 0x57C0F0}};
constexpr sh::CallSite kCalls54E540[] = {{0x8, 0x57C0F0}};
constexpr sh::CallSite kCalls54E560[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls54E590[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls54E5C0[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls54E5F0[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls54E620[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls54E650[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls54E680[] = {{0x0, 0x57C7C0}, {0x1B, 0x57C140}};
constexpr sh::CallSite kCalls54E6C0[] = {{0x0, 0x57C7C0}, {0x19, 0x57C140}};
constexpr sh::CallSite kCalls54E8E0[] = {{0x5D, 0x5720C0}, {0xB2, 0x589330}, {0xFC, 0x57CD40}};
constexpr sh::CallSite kCalls54E9F0[] = {{0x16, 0x5720C0}, {0x63, 0x5720C0}};
constexpr sh::CallSite kCalls54EA80[] = {{0x3B, 0x57C7C0}, {0x77, 0x57C140}, {0x8C, 0x57C140}, {0xC1, 0x57C0F0}, {0xC9, 0x57C7C0}, {0x105, 0x57C140}, {0x11A, 0x57C140}, {0x146, 0x57C7C0}, {0x188, 0x57C140}, {0x1B4, 0x57C7C0}, {0x1FA, 0x57C140}, {0x20F, 0x57C140}, {0x23B, 0x57C7C0}, {0x274, 0x57C140}, {0x288, 0x57C140}, {0x2B4, 0x57C7C0}, {0x2FB, 0x57C140}, {0x310, 0x57C140}, {0x33C, 0x57C7C0}, {0x375, 0x57C140}, {0x3AD, 0x57C7C0}, {0x3E7, 0x57C140}, {0x413, 0x57C7C0}};
constexpr sh::CallSite kCalls54EED0[] = {{0x11, 0x56D800}};
constexpr sh::CallSite kCalls54EF00[] = {{0x8, 0x5919B0}, {0x15, 0x57C7C0}, {0x43, 0x57C7C0}};
constexpr sh::CallSite kCalls54EF80[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}};
constexpr sh::CallSite kCalls54EFC0[] = {{0x8, 0x57C140}, {0x1B, 0x57C140}, {0x2E, 0x57C140}, {0x43, 0x57C140}, {0x4F, 0x57C7C0}, {0x7B, 0x57C7C0}, {0x91, 0x57C7C0}};

const sh::Clone kClones[] = {
    {"Scena06_Frame", 0x54A910, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Frame), 0, false, sh::Shape::kSlot},   // vtable slot 0 (the frame): no arguments
    {"Scena06_Start", 0x54A920, 0xD, kCalls54A920, SH_N(kCalls54A920), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Start), 0, false, sh::Shape::kState},   // state handler (Scena06_States 0)
    {"Scena06_EnterArea", 0x54A930, 0x4A0, kCalls54A930, SH_N(kCalls54A930), nullptr, 0, kTables54A930, SH_N(kTables54A930), reinterpret_cast<const void*>(&::Scena06_EnterArea), 0, false, sh::Shape::kState},   // state handler (Scena06_States 1)
    {"Scena06_Run", 0x54ADD0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Run), 0, false, sh::Shape::kState},   // state handler (Scena06_States 2)
    {"Scena06_Run01", 0x54ADE0, 0x1A4, kCalls54ADE0, SH_N(kCalls54ADE0), nullptr, 0, kTables54ADE0, SH_N(kTables54ADE0), reinterpret_cast<const void*>(&::Scena06_Run01), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 1)
    {"Scena06_Run02", 0x54AF90, 0x2C0, kCalls54AF90, SH_N(kCalls54AF90), nullptr, 0, kTables54AF90, SH_N(kTables54AF90), reinterpret_cast<const void*>(&::Scena06_Run02), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 2)
    {"Scena06_Run03", 0x54B250, 0x468, kCalls54B250, SH_N(kCalls54B250), nullptr, 0, kTables54B250, SH_N(kTables54B250), reinterpret_cast<const void*>(&::Scena06_Run03), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 3)
    {"Scena06_Run04", 0x54B6C0, 0x2F4, kCalls54B6C0, SH_N(kCalls54B6C0), nullptr, 0, kTables54B6C0, SH_N(kTables54B6C0), reinterpret_cast<const void*>(&::Scena06_Run04), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 4)
    {"Scena06_Run05", 0x54B9C0, 0x120, kCalls54B9C0, SH_N(kCalls54B9C0), nullptr, 0, kTables54B9C0, SH_N(kTables54B9C0), reinterpret_cast<const void*>(&::Scena06_Run05), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 5)
    {"Scena06_Run06", 0x54BAE0, 0x440, kCalls54BAE0, SH_N(kCalls54BAE0), nullptr, 0, kTables54BAE0, SH_N(kTables54BAE0), reinterpret_cast<const void*>(&::Scena06_Run06), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 6)
    {"Scena06_Run07", 0x54BF20, 0x55F, kCalls54BF20, SH_N(kCalls54BF20), nullptr, 0, kTables54BF20, SH_N(kTables54BF20), reinterpret_cast<const void*>(&::Scena06_Run07), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 7)
    {"Scena06_Run08", 0x54C480, 0x482, kCalls54C480, SH_N(kCalls54C480), nullptr, 0, kTables54C480, SH_N(kTables54C480), reinterpret_cast<const void*>(&::Scena06_Run08), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 8)
    {"Scena06_Run09", 0x54C910, 0x364, kCalls54C910, SH_N(kCalls54C910), nullptr, 0, kTables54C910, SH_N(kTables54C910), reinterpret_cast<const void*>(&::Scena06_Run09), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 9)
    {"Scena06_Run10", 0x54CC80, 0x100, kCalls54CC80, SH_N(kCalls54CC80), nullptr, 0, kTables54CC80, SH_N(kTables54CC80), reinterpret_cast<const void*>(&::Scena06_Run10), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 10)
    {"Scena06_Run11", 0x54CD80, 0x178, kCalls54CD80, SH_N(kCalls54CD80), nullptr, 0, kTables54CD80, SH_N(kTables54CD80), reinterpret_cast<const void*>(&::Scena06_Run11), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 11)
    {"Scena06_Run12", 0x54CF00, 0x3A4, kCalls54CF00, SH_N(kCalls54CF00), nullptr, 0, kTables54CF00, SH_N(kTables54CF00), reinterpret_cast<const void*>(&::Scena06_Run12), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 12)
    {"Scena06_Run13", 0x54D2B0, 0x23C, kCalls54D2B0, SH_N(kCalls54D2B0), nullptr, 0, kTables54D2B0, SH_N(kTables54D2B0), reinterpret_cast<const void*>(&::Scena06_Run13), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 13)
    {"Scena06_Run14", 0x54D4F0, 0x524, kCalls54D4F0, SH_N(kCalls54D4F0), nullptr, 0, kTables54D4F0, SH_N(kTables54D4F0), reinterpret_cast<const void*>(&::Scena06_Run14), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 14)
    {"Scena06_MemberLines", 0x54DA20, 0x6A, kCalls54DA20, SH_N(kCalls54DA20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_MemberLines), 0, false, sh::Shape::kState},   // Scena06_Run14's E8, no arguments
    {"Scena06_Run15", 0x54DA90, 0x43F, kCalls54DA90, SH_N(kCalls54DA90), nullptr, 0, kTables54DA90, SH_N(kTables54DA90), reinterpret_cast<const void*>(&::Scena06_Run15), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 15)
    {"Scena06_Run16", 0x54DED0, 0x1EE, kCalls54DED0, SH_N(kCalls54DED0), nullptr, 0, kTables54DED0, SH_N(kTables54DED0), reinterpret_cast<const void*>(&::Scena06_Run16), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 16)
    {"Scena06_Run17", 0x54E0C0, 0x224, kCalls54E0C0, SH_N(kCalls54E0C0), nullptr, 0, kTables54E0C0, SH_N(kTables54E0C0), reinterpret_cast<const void*>(&::Scena06_Run17), 0, false, sh::Shape::kState},   // state handler (Scena06_Runs 17)
    {"Scena06_LeaderEffect", 0x54E2F0, 0xB3, kCalls54E2F0, SH_N(kCalls54E2F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_LeaderEffect), 0, false, sh::Shape::kEntry},   // Scena06_Run12's and Run14's E8, one word (the arg's byte)
    {"Scena06_PartyCalls", 0x54E3B0, 0x8C, kCalls54E3B0, SH_N(kCalls54E3B0), nullptr, 0, kTables54E3B0, SH_N(kTables54E3B0), reinterpret_cast<const void*>(&::Scena06_PartyCalls), 0, false, sh::Shape::kState},   // Scena06_EnterArea's E8, no arguments
    {"Scena06_ObjectTrigger", 0x54E440, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_ObjectTrigger), 0, false, sh::Shape::kObject},   // vtable slot 1: the object
    {"Scena06_Object01", 0x54E460, 0x28, kCalls54E460, SH_N(kCalls54E460), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object01), 0, false, sh::Shape::kState},   // Scena06_Objects 1: (object, bits) unread
    {"Scena06_Object02", 0x54E490, 0x28, kCalls54E490, SH_N(kCalls54E490), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object02), 0, false, sh::Shape::kState},   // Scena06_Objects 2: (object, bits) unread
    {"Scena06_Object03", 0x54E4C0, 0x3E, kCalls54E4C0, SH_N(kCalls54E4C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object03), 0, false, sh::Shape::kState},   // Scena06_Objects 3: (object, bits) unread
    {"Scena06_Object04", 0x54E500, 0x3A, kCalls54E500, SH_N(kCalls54E500), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object04), 0, false, sh::Shape::kState},   // Scena06_Objects 4: (object, bits) unread
    {"Scena06_Object05", 0x54E540, 0x11, kCalls54E540, SH_N(kCalls54E540), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object05), 0, false, sh::Shape::kState},   // Scena06_Objects 5: (object, bits) unread
    {"Scena06_Object06", 0x54E560, 0x28, kCalls54E560, SH_N(kCalls54E560), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object06), 0, false, sh::Shape::kState},   // Scena06_Objects 6: (object, bits) unread
    {"Scena06_Object07", 0x54E590, 0x2A, kCalls54E590, SH_N(kCalls54E590), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object07), 0, false, sh::Shape::kState},   // Scena06_Objects 7: (object, bits) unread
    {"Scena06_Object08", 0x54E5C0, 0x2A, kCalls54E5C0, SH_N(kCalls54E5C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object08), 0, false, sh::Shape::kState},   // Scena06_Objects 8: (object, bits) unread
    {"Scena06_Object09", 0x54E5F0, 0x2A, kCalls54E5F0, SH_N(kCalls54E5F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object09), 0, false, sh::Shape::kState},   // Scena06_Objects 9: (object, bits) unread
    {"Scena06_Object10", 0x54E620, 0x28, kCalls54E620, SH_N(kCalls54E620), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object10), 0, false, sh::Shape::kState},   // Scena06_Objects 10: (object, bits) unread
    {"Scena06_Object11", 0x54E650, 0x2A, kCalls54E650, SH_N(kCalls54E650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object11), 0, false, sh::Shape::kState},   // Scena06_Objects 11: (object, bits) unread
    {"Scena06_Object12", 0x54E680, 0x3D, kCalls54E680, SH_N(kCalls54E680), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object12), 0, false, sh::Shape::kState},   // Scena06_Objects 12: (object, bits) unread
    {"Scena06_Object13", 0x54E6C0, 0x3B, kCalls54E6C0, SH_N(kCalls54E6C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Object13), 0, false, sh::Shape::kState},   // Scena06_Objects 13: (object, bits) unread
    {"Scena06_GuestRecord", 0x54E700, 0x84, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_GuestRecord), 0, false, sh::Shape::kState},   // Scena06_Start's E8, no arguments
    {"Scena06_Leap", 0x54E790, 0x39, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Leap), 0xFF, false, sh::Shape::kEntry},   // area 77's E8 (0x40F0E6): seven words, al
    {"Scena06_LeapStart", 0x54E7D0, 0x10A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_LeapStart), 0xFF, false, sh::Shape::kEntry},   // Scena06_LeapPhases 0: seven words, al
    {"Scena06_LeapAir", 0x54E8E0, 0x110, kCalls54E8E0, SH_N(kCalls54E8E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_LeapAir), 0xFF, false, sh::Shape::kEntry},   // Scena06_LeapPhases 1: seven words, al
    {"Scena06_LeapLand", 0x54E9F0, 0x89, kCalls54E9F0, SH_N(kCalls54E9F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_LeapLand), 0xFF, false, sh::Shape::kEntry},   // Scena06_LeapPhases 2: seven words, al
    {"Scena06_StepHook", 0x54EA80, 0x44B, kCalls54EA80, SH_N(kCalls54EA80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_StepHook), 0xFF, false, sh::Shape::kHook},   // vtable slot 2, hook (x, z) -> al
    {"Scena06_CellHook", 0x54EED0, 0x2A, kCalls54EED0, SH_N(kCalls54EED0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_CellHook), 0xFF, false, sh::Shape::kHook},   // vtable slot 4, hook (x, z) -> al
    {"Scena06_Cell0", 0x54EF00, 0x72, kCalls54EF00, SH_N(kCalls54EF00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Cell0), 0xFF, false, sh::Shape::kHook},   // Scena06_CellHandlers 0, (x, z) in place -> al
    {"Scena06_Cell2", 0x54EF80, 0x3E, kCalls54EF80, SH_N(kCalls54EF80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Cell2), 0xFF, false, sh::Shape::kHook},   // Scena06_CellHandlers 2, (x, z) in place -> al
    {"Scena06_Cell3", 0x54EFC0, 0xBB, kCalls54EFC0, SH_N(kCalls54EFC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena06_Cell3), 0xFF, false, sh::Shape::kHook},   // Scena06_CellHandlers 3, (x, z) in place -> al
};
enum : unsigned {
    kFrame, kStart, kEnterArea, kRun,
    kRun01, kRun02, kRun03, kRun04, kRun05, kRun06, kRun07, kRun08, kRun09, kRun10, kRun11, kRun12, kRun13, kRun14,
    kMemberLines, kRun15, kRun16, kRun17, kLeaderEffect, kPartyCalls, kObjectTrigger,
    kObject01, kObject13 = kObject01 + 12,
    kGuestRecord, kLeap, kLeapStart, kLeapAir, kLeapLand, kStepHook, kCellHook, kCell0, kCell2, kCell3, kCount
};
static_assert(sizeof kClones / sizeof kClones[0] == kCount, "one role per clone");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The run a clone is (1..17), 0 for any other.
unsigned RunOf(unsigned k) {
    if (k >= kRun01 && k <= kRun14) return k - kRun01 + 1;
    if (k == kRun15) return 15;
    if (k == kRun16) return 16;
    if (k == kRun17) return 17;
    return 0;
}

// --- the typed stand-ins -------------------------------------------------------------
//
// Scena06_Objects' entries take (object, bits), Scena06_CellHandlers' (x, z)
// answering al, Scena06_LeapPhases' seven words answering al - arguments a
// DataTable's handler recorder does not log. So the seed writes a stand-in of
// the exact type into every entry, one per index (a wrong index is a
// different log), and the three tables are regions (the harness puts them
// back). Each logs against its dispatcher's own address, which no clone
// calls. Scena06_CellHandlers' entry 1 is Capcom's bare ret 0x437CC0 half the
// time instead, so that its answer (0x56D800's al) is compared too.
template <unsigned I> void __cdecl ObjectEntry(unsigned char* object, std::uint32_t bits) {
    sh::Record(0x54E440, I, Key(object), bits);
    sh::Stir();
}
template <unsigned I> unsigned char __cdecl CellEntry(int x, int z) {
    sh::Record(0x54EED0, I, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    sh::Stir();
    return static_cast<unsigned char>(sh::Noise());
}
template <unsigned I> unsigned char __cdecl LeapEntry(unsigned char* object, unsigned dx, unsigned dz, unsigned lift,
                                                      unsigned gravity, unsigned animation, unsigned flag) {
    sh::Record(0x54E790, I, Key(object), dx, dz);
    sh::Record(0x54E790, lift, gravity, animation, flag);
    sh::Stir();
    return static_cast<unsigned char>(sh::Noise());
}
using ObjectFn = void (__cdecl*)(unsigned char*, std::uint32_t);
using CellFn = unsigned char (__cdecl*)(int, int);
using LeapFn = unsigned char (__cdecl*)(unsigned char*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned);
const ObjectFn kObjectEntries[at::kObjectCount] = {
    &ObjectEntry<0>, &ObjectEntry<1>, &ObjectEntry<2>, &ObjectEntry<3>, &ObjectEntry<4>, &ObjectEntry<5>, &ObjectEntry<6>,
    &ObjectEntry<7>, &ObjectEntry<8>, &ObjectEntry<9>, &ObjectEntry<10>, &ObjectEntry<11>, &ObjectEntry<12>, &ObjectEntry<13>,
};
const CellFn kCellEntries[at::kCellHandlerCount] = {&CellEntry<0>, &CellEntry<1>, &CellEntry<2>, &CellEntry<3>};
const LeapFn kLeapEntries[at::kLeapPhaseCount] = {&LeapEntry<0>, &LeapEntry<1>, &LeapEntry<2>};

// Item_NamePtr: the callers copy 16 bytes from the answer - a buffer of the
// fuzz's own, filled from the recorders' stream.
std::uint8_t g_name[16];
std::uint32_t NameEffect(const std::uint32_t*, std::uint32_t) {
    sh::FillBytes(g_name, sizeof g_name);
    return Key(g_name);
}

// --- the callees -----------------------------------------------------------------
//
// Beyond the standard set (docs/scenario_harness.md section 4), or recorded
// otherwise than it records them (the group's listing stands).
#define SC6_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define SC6_THEIRS(name) #name, KeyOf(name), KeyOf(name)
#define SC6_RAW(label, address) label, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    // tested on al alone (test al, al): garbage above a 0 must not matter
    {SC6_OURS(Flags_Test), 2, {kAll, kU8}, sh::Answer::kFlag, 0, 0},
    // the group's own, called by E8: the chapter bytes they run with, or the argument
    {SC6_OURS(Scena06_GuestRecord), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC6_OURS(Scena06_PartyCalls), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC6_OURS(Scena06_MemberLines), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC6_OURS(Scena06_LeaderEffect), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    // group SE's, ours: by name (the standard set lists 0x4410B0 by address)
    {SC6_OURS(Field_StartEventBattle), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC6_OURS(Party_AddToLists), 1, {kU8}, sh::Answer::kGarbage, 0, 0},   // al unread
    // the items: each reads its arguments' low bytes (char_stats.cpp); Inventory_Add's
    // al tested, a fourth word pushed and unread; Inventory_Count's u16 tested whole
    {SC6_OURS(Inventory_Add), 3, {kU8, kU8, kU8}, sh::Answer::kFlag, 0, 0},
    {SC6_OURS(Inventory_Count), 3, {kAll, kAll, kAll}, sh::Answer::kBool, 0, 0},
    {SC6_OURS(Item_NamePtr), 2, {kU8, kU8}, sh::Answer::kGarbage, 0, 0, {}, &NameEffect},
    // Capcom's, named
    {SC6_OURS(MoveCmd_OpDB), 0, {}, sh::Answer::kGarbage, 0, 0},
    // nobody's, by address
    {SC6_RAW("0x533E50", at::kPartyPass), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC6_RAW("0x56D800", at::kCellFind), 4, {kAll, kU8, kU8, kU8}, sh::Answer::kByte, 0xFF, 3},
    {SC6_RAW("0x591900", at::kKeyItemPut), 1, {kU8}, sh::Answer::kGarbage, 0, 0},    // al unread
    {SC6_RAW("0x591BE0", at::kZennyAdd), 2, {kAll, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC6_RAW("0x587B80", at::kSoundJmp), 0, {}, sh::Answer::kGarbage, 0, 0},
    // the log slots of the table stand-ins above
    {"Scena06_Objects[i]", 0x54E440, 0x54E440, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&ObjectEntry<0>)},
    {"Scena06_CellHandlers[i]", 0x54EED0, 0x54EED0, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&CellEntry<0>)},
    {"Scena06_LeapPhases[i]", 0x54E790, 0x54E790, 4, {kAll, kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&LeapEntry<0>)},
};
#undef SC6_OURS
#undef SC6_THEIRS
#undef SC6_RAW

// The state and run tables take no arguments: swapped for recorders.
const sh::DataTable kTables[] = {{at::kStates, at::kStateCount}, {at::kRuns, at::kRunCount}};

// The leap's movement-script object: its +4 the speed index.
unsigned char g_leap_object[8];

// Beyond the standard regions: the character records (record 7 and the
// members' +0xB), MoveScript_EffectState, the selector, the byte 0x904C9F,
// the music byte and the two Text_Records names, Cond_ByteFE, the load bytes
// and Field_Kind2Hold, the byte 0x7DEE44, run 8's copy of the party list, the
// three tables the seed writes stand-ins into, and the leap's object.
const sh::Region kRegions[] = {
    {at::kCharRecords, 8 * at::kCharStride},
    {at::kEffectState, 0x18},
    {at::kSelector, 4},
    {at::kByte904C9F, 1},
    {at::kMusicByte, 0x40},
    {at::kCondFE, 1},
    {0x929F0C, 8},
    {at::kBank7DEE44, 4},
    {0x939A00, 0x110},
    {at::kObjects, 4 * at::kObjectCount},
    {at::kLeapPhases, 4 * at::kLeapPhaseCount},
    {at::kCellHandlers, 4 * at::kCellHandlerCount},
    {at::kGuestStats, 8},        // Capcom's constant bytes, random here so that each byte's use is seen
    {at::kMemberBytes, 8},       // likewise
    {0, sizeof g_leap_object},   // the leap's object (its address set at start-up)
};
sh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

unsigned char& B(std::uint32_t a) { return *sh::Mem(a); }
void SetW(std::uint32_t a, std::uint32_t v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(sh::Mem(a), &w, 2);
}
void SetD(std::uint32_t a, std::uint32_t v) { std::memcpy(sh::Mem(a), &v, 4); }

// Each run's steps (the cases its switch holds, and one or two past).
std::uint32_t AStep(unsigned run) {
    switch (run) {
    case 1: return SH_PICK(0, 1, 2, 3, 4, 7, 10, 5, 11);
    case 2: return SH_PICK(0, 1, 5, 6, 10, 11, 15, 16, 17, 18, 19, 2, 20);
    case 3: return SH_PICK(0, 1, 3, 4, 5, 7, 8, 9, 10, 11, 12, 13, 14, 15, 2, 6, 16);
    case 4: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 20, 21, 22, 30, 31, 8, 32);
    case 5: return SH_PICK(0, 1, 2, 3, 4, 5, 6);
    case 6: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 20, 21, 22, 23, 24, 30, 31, 32, 11, 33);
    case 7: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 0xA, 0xB, 0xE, 0xF, 0x12, 0x13, 0x14, 0x15, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E,
                           0x1F, 0x20, 0x21, 0x23, 0x24, 0x25, 0x28, 0x2D, 0x32, 0x33, 0x3C, 0x3D, 0x3E, 7, 0x3F);
    case 8: return SH_PICK(0, 1, 2, 4, 5, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x1E, 0x1F, 0x20,
                           0x28, 0x29, 3, 0x2A);
    case 9: return SH_PICK(0, 1, 2, 3, 5, 7, 0xA, 0xF, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x17, 4, 0x18);
    case 10: return SH_PICK(0, 1, 2, 3, 5, 6, 4, 7);
    case 11: return SH_PICK(0, 1, 5, 6, 7, 8, 9, 2, 10);
    case 12: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 9, 0x14);
    case 13: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 9);
    case 14: return SH_PICK(0, 1, 2, 5, 0xA, 0xB, 0xC, 0xD, 0x14, 0x15, 0x16, 0x17, 0x1E, 0x23, 3, 0x24);
    case 15: return SH_PICK(0, 1, 5, 6, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x1E, 2, 0x1F);
    case 16: return SH_PICK(0, 1, 0xA, 0xC, 0x14, 0x15, 2, 0x16);
    case 17: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 9, 0xB);
    default: return sh::Next();
    }
}

// The counter-0 value each run's step waits on (read off ours), so a step's
// action is reached, not only its wait.
struct Wait { unsigned char run, step, count; };
const Wait kWaits[] = {
    {1, 1, 8}, {1, 3, 0x11}, {1, 10, 2},
    {2, 1, 4}, {2, 11, 0x14}, {2, 16, 0x1A}, {2, 18, 0x1C}, {2, 19, 0x1E},
    {3, 3, 3}, {3, 4, 7}, {3, 5, 0xA}, {3, 8, 0xC}, {3, 9, 0xD}, {3, 10, 0xE}, {3, 12, 1}, {3, 13, 2}, {3, 14, 0x32},
    {4, 2, 1}, {4, 4, 0xA}, {4, 7, 0xD}, {4, 12, 0x15}, {4, 22, 1},
    {5, 2, 0xC},
    {6, 1, 9}, {6, 5, 2}, {6, 7, 0x13}, {6, 9, 0x16}, {6, 10, 0x22}, {6, 21, 4}, {6, 23, 7}, {6, 24, 9}, {6, 32, 0x36},
    {7, 4, 2}, {7, 0x1D, 2}, {7, 0xB, 3}, {7, 0xF, 3}, {7, 0x21, 3}, {7, 0x2D, 3}, {7, 0x33, 3}, {7, 0x13, 2}, {7, 0x14, 4},
    {7, 0x24, 3},
    {8, 5, 4}, {8, 0xB, 2}, {8, 0xC, 3}, {8, 0xD, 4}, {8, 0xE, 0xA},
    {9, 1, 0xA}, {9, 3, 0xA}, {9, 5, 0xF}, {9, 0xA, 0x14}, {9, 0x10, 0x14}, {9, 0x15, 2},
    {10, 1, 8}, {10, 3, 0xA}, {10, 6, 0x17},
    {11, 6, 1}, {11, 8, 2}, {11, 9, 3},
    {12, 2, 1}, {12, 3, 3}, {12, 4, 5}, {12, 5, 0xA}, {12, 6, 0x1D}, {12, 0xE, 0x3C}, {12, 0xF, 5}, {12, 0x13, 0x14},
    {13, 5, 1}, {13, 8, 1},
    {14, 1, 4}, {14, 5, 0xA}, {14, 0xD, 0xF}, {14, 0x15, 0x15}, {14, 0x16, 0x1A}, {14, 0x17, 0x1C}, {14, 0x23, 0x1E},
    {15, 9, 0x29}, {15, 0xC, 0x2D}, {15, 0x11, 0x35}, {15, 0x12, 0x42}, {15, 0x14, 0x47}, {15, 0x16, 0x49},
    {16, 1, 0x1F}, {16, 0xC, 0x24},
    {17, 3, 0xF}, {17, 4, 0x14}, {17, 5, 0x18}, {17, 6, 0x1B}, {17, 8, 9},
};
std::uint32_t ACount() {
    const Wait& w = kWaits[sh::Next() % (sizeof kWaits / sizeof kWaits[0])];
    return sh::Often() ? w.count : sh::Next() & 0xFF;
}

std::uint32_t AnArea() {
    return SH_PICK(0x27, 0x2E, 0x35, 0x4C, 0x57, 0x5C, 0x5E, 0x2D, 0x41, 0x10, 0x39, 0x43, 0x4D, 0x2F, 0x60);
}
std::uint32_t APartyByte() { return SH_PICK(0, 1, 2, 5, 3, 7); }

unsigned g_k;

void Seed(unsigned k) {
    g_k = k;
    // the typed stand-ins (the three tables are regions: put back afterwards)
    for (unsigned i = 0; i < at::kObjectCount; ++i) SetD(at::kObjects + 4 * i, KeyOf(kObjectEntries[i]));
    for (unsigned i = 0; i < at::kLeapPhaseCount; ++i) SetD(at::kLeapPhases + 4 * i, KeyOf(kLeapEntries[i]));
    for (unsigned i = 0; i < at::kCellHandlerCount; ++i) SetD(at::kCellHandlers + 4 * i, KeyOf(kCellEntries[i]));
    if (sh::Half()) SetD(at::kCellHandlers + 4, at::kBareRet);
    // every member's record byte inside the eight records
    for (unsigned i = 0; i < 0x18; ++i) B(at::kEffectState + i) = static_cast<unsigned char>(sh::Next() & 7);
    // the cells the chapter compares, each most of the time at a value a
    // branch tests
    if (sh::Often()) SetW(at::kArea, AnArea());
    if (sh::Often()) B(at::kCounters) = static_cast<unsigned char>(ACount());
    if (sh::Half()) B(at::kCounters + 1) = static_cast<unsigned char>(sh::Next() % 20);
    if (sh::Often()) B(at::kCounters + 2) = static_cast<unsigned char>(sh::Next() % 6);
    B(at::kCounters + 3) = static_cast<unsigned char>(sh::Often() ? sh::Next() % 20 : SH_PICK(0x10, 0x11, 0x12));
    if (sh::Half()) B(at::kEffects + B(at::kCounters + 3) * at::kEffectStride) = 0;
    if (sh::Half()) B(at::kEffects + (B(at::kCounters + 1) % 20) * at::kEffectStride) = 0;
    if (sh::Often()) B(at::kRequest) = static_cast<unsigned char>(SH_PICK(0, 2, 7, 6));
    if (sh::Half()) SetW(at::kWait, 0);
    if (sh::Half()) B(at::kHold) = 0;
    if (sh::Often()) B(at::kSelector) = static_cast<unsigned char>((sh::Next() % 8) | (sh::Half() ? 0x80 : 0));
    for (unsigned i = 0; i < 3; ++i)
        if (sh::Often()) B(at::kPartyBytes + i) = static_cast<unsigned char>(APartyByte());
    if (sh::Half()) B(at::kRecord7 + 0x12) = static_cast<unsigned char>(SH_PICK(0, 0xFF, 1));
    if (sh::Half()) B(at::kRecord7 + 0x15) = static_cast<unsigned char>(SH_PICK(0, 0xFF, 1));
    if (sh::Half()) B(at::kObjTrio + 8) = static_cast<unsigned char>(SH_PICK(2, 3, 4, 5));
    if (sh::Half()) B(at::kLoad0F) = 0;
    if (sh::Half()) B(at::kLoad10) = 0;
    if (sh::Half()) SetW(at::kTimer, SH_PICK(0, 1, 2));
    switch (k) {
    case kFrame: B(at::kState) = static_cast<unsigned char>(sh::Next() % at::kStateCount); break;
    case kRun: B(at::kRun) = static_cast<unsigned char>(sh::Next() % at::kRunCount); break;
    case kEnterArea:   // the areas and counter 2's cases, together
        if (sh::Often()) SetW(at::kArea, SH_PICK(0x27, 0x2E, 0x35, 0x4C, 0x57, 0x5C, 0x5E));
        if (sh::Often()) B(at::kCounters + 2) = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3, 4, 5));
        break;
    case kPartyCalls: B(at::kSelector) = static_cast<unsigned char>((sh::Next() % 8) | (sh::Half() ? 0x80 : 0)); break;
    case kObjectTrigger: sh::SpriteRecord(0)[0x86] = static_cast<unsigned char>(sh::Next() % at::kObjectCount); break;
    case kStepHook:
    case kCellHook:
        if (sh::Often()) SetW(at::kArea, SH_PICK(0x27, 0x39, 0x43, 0x4D, 0x5E));
        break;
    case kLeap:
        g_leap_object[4] = static_cast<unsigned char>(sh::Next() % 8);
        for (unsigned s = 0; s < 4; ++s) sh::SpriteRecord(s)[4] = static_cast<unsigned char>(sh::Next() % at::kLeapPhaseCount);
        break;
    case kLeapStart:
        // speed indexes 0..7: 0, 6, 7 read a speed of 0; 8.. would divide by 0 (both sides fault)
        g_leap_object[4] = static_cast<unsigned char>(sh::Next() % 8);
        break;
    case kLeapAir: {
        // indexes 1..5 only (a 0 speed divides by 0 on both sides)
        g_leap_object[4] = static_cast<unsigned char>(1 + sh::Next() % 5);
        for (unsigned s = 0; s < 4; ++s) {
            unsigned char* const r = sh::SpriteRecord(s);
            if (sh::Often()) r[0xA] = static_cast<unsigned char>(sh::Next() % 3);
            if (sh::Often()) r[0xB] = static_cast<unsigned char>(sh::Next() % 2);
            if (sh::Often()) SetD(Key(r + 0x14), SH_PICK(0, 1, 0xFFFFFFFFu, 0x100, 0xFFFFFF00u, 0x400, 0xFFFFFC00u));
        }
        break;
    }
    case kLeapLand:
        for (unsigned s = 0; s < 4; ++s)
            if (sh::Often()) SetW(Key(sh::SpriteRecord(s) + 0x3E), SH_PICK(0, 0x240, 0x241, 0x23F, 0x1000, 0xFFFF));
        break;
    default:
        if (const unsigned run = RunOf(k)) {
            B(at::kStep) = static_cast<unsigned char>(AStep(run));
            if (sh::Often())
                for (const Wait& w : kWaits)
                    if (w.run == run && w.step == B(at::kStep)) B(at::kCounters) = w.count;
            if (run == 7 && sh::Half()) {   // record 7's two equipment bytes, both set or not
                B(at::kRecord7 + 0x12) = static_cast<unsigned char>(sh::Half() ? 0 : 1 + sh::Next() % 0xFE);
                B(at::kRecord7 + 0x15) = static_cast<unsigned char>(sh::Half() ? 0 : 1 + sh::Next() % 0xFE);
            }
            if (run == 12 && sh::Half()) B(at::kBank7DEE44) = static_cast<unsigned char>(sh::Next() & 2);
            if (run == 8 && sh::Often())   // step 0's sort: the three +0x89 among a few values (ties too)
                for (unsigned m = 0; m < 3; ++m) B(at::kObjTrio + at::kObjStride * m + 0x89) = static_cast<unsigned char>(sh::Next() % 4);
        }
        break;
    }
    if (sh::Next() % 8 == 0 && RunOf(k)) B(at::kStep) = static_cast<unsigned char>(sh::Next());   // any step, now and then
}

// A coordinate at one of the step hook's bounds, one either side, or anything.
void HookXZ(std::uint32_t* a) {
    static const std::uint32_t kRects[][4] = {
        // x lo, x hi, z lo, z hi (an "either" pair is the two x or z values)
        {0x310000, 0x318000, 0x270000, 0x298000}, {0x430000, 0x448000, 0x370000, 0x378000},
        {0x270000, 0x328000, 0x520000, 0x528000}, {0x1C0000, 0x1F8000, 0x1F0000, 0x1F8000},
        {0x300000, 0x318000, 0x40000, 0x98000},   {0x1B8000, 0x1C0000, 0x40000, 0x98000},
        {0x430000, 0x438000, 0x320000, 0x348000}, {0x1B8000, 0x1C0000, 0x3D0000, 0x3F8000},
    };
    const std::uint32_t* r = kRects[sh::Next() % 8];
    a[0] = r[sh::Next() % 2] + (sh::Often() ? 0 : SH_PICK(1, 0xFFFFFFFFu, 0x8000));
    a[1] = r[2 + sh::Next() % 2] + (sh::Often() ? 0 : SH_PICK(1, 0xFFFFFFFFu, 0x8000));
}

void Args(unsigned k, std::uint32_t* a) {
    switch (k) {
    case kObjectTrigger: a[0] = Key(sh::SpriteRecord(0)); break;
    case kStepHook:
        if (sh::Often()) HookXZ(a);
        break;
    case kLeaderEffect: a[0] = sh::Often() ? SH_PICK(0x91, 0x93) : sh::Next(); break;
    case kLeap:
    case kLeapStart:
    case kLeapAir:
    case kLeapLand:
        a[0] = Key(g_leap_object);
        if (sh::Half()) a[1] = SH_PICK(0, 1, 0xFF, 0x80, 0x7F, 0x103);
        if (sh::Half()) a[2] = SH_PICK(0, 1, 0xFF, 0x80, 0x7F, 0x203);
        if (sh::Half()) a[4] = SH_PICK(0xFFFFFC00u, 0, 0x400, 0x12345C00u);
        if (sh::Half()) a[5] = (a[5] & ~0xFFu) | 0xFF;
        if (sh::Half()) a[6] &= 0xFFFFFF00u;
        break;
    default: break;
    }
}

// After a call, two in three (beyond the harness's own): counter 0 at a value
// waited on, the request, the area, the selector, a party byte, record 7's
// equipment bytes, the hold and load bytes, an effect's in-use byte, the byte
// 0x7DEE44. Drawn from the hash given, never the harness's Next.
void Disturb(std::uint32_t h) {
    const unsigned char v = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 10) {
    case 0: B(at::kCounters) = kWaits[(h >> 12) % (sizeof kWaits / sizeof kWaits[0])].count; break;
    case 1: B(at::kRequest) = static_cast<unsigned char>((h >> 12) % 3 == 0 ? 2 : (h >> 14) & 1 ? 0 : 7); break;
    case 2: {
        static const std::uint16_t kMoved[] = {0x27, 0x2E, 0x35, 0x4C, 0x57, 0x5C, 0x5E, 0x39, 0x43, 0x4D};
        SetW(at::kArea, kMoved[(h >> 12) % 10]);
        break;
    }
    case 3: B(at::kSelector) = static_cast<unsigned char>((h >> 12) % 8); break;
    case 4: B(at::kPartyBytes + (h >> 12) % 3) = static_cast<unsigned char>((h >> 14) % 6); break;
    case 5: B(at::kRecord7 + ((h >> 12) & 1 ? 0x12 : 0x15)) = static_cast<unsigned char>(v & 1 ? 0 : v); break;
    case 6: B(((h >> 12) & 1) ? at::kHold : ((h >> 13) & 1) ? at::kLoad0F : at::kLoad10) = static_cast<unsigned char>(v & 1); break;
    case 7: B(at::kEffects + ((h >> 12) % 20) * at::kEffectStride) = static_cast<unsigned char>(v & 1); break;
    case 8: B(at::kBank7DEE44) = static_cast<unsigned char>(v & 2); break;
    default: B(at::kCounters + 3) = static_cast<unsigned char>((h >> 12) % 20); break;
    }
}

// After every disturbance: for run 16, half the time one of record 7's two
// equipment bytes moved (it reads them again after Inventory_Add); for the two
// that read the area again after their calls (EnterArea, StepHook): half the
// time the area moved to one they test,
// since the harness's disturbance reaches the group's cells one time in
// sixteen. Noise() is the recorders' stream, the same on both passes.
void Settle() {
    if (g_k == kRun16) {   // record 7's two bytes, read again after Inventory_Add
        const std::uint32_t n = sh::Noise();
        if (n & 1) B(at::kRecord7 + ((n >> 1) & 1 ? 0x12 : 0x15)) = static_cast<unsigned char>(n >> 8);
        return;
    }
    if (g_k != kEnterArea && g_k != kStepHook) return;
    const std::uint32_t n = sh::Noise();
    if (!(n & 1)) return;
    static const std::uint16_t kEnter[] = {0x27, 0x2E, 0x35, 0x4C, 0x57, 0x5C, 0x5E, 0x2D};
    static const std::uint16_t kStep[] = {0x27, 0x39, 0x43, 0x4D, 0x5E};
    SetW(at::kArea, g_k == kEnterArea ? kEnter[(n >> 8) % 8] : kStep[(n >> 8) % 5]);
}

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[sizeof kRegions / sizeof kRegions[0] - 1].at = Key(g_leap_object);
    sh::Group group = {"scena_sc6",
                       kClones,
                       kCount,
                       kCallees,
                       sizeof kCallees / sizeof kCallees[0],
                       kTables,
                       sizeof kTables / sizeof kTables[0],
                       g_regions,
                       sizeof g_regions / sizeof g_regions[0],
                       &Seed,
                       &Disturb,
                       6000};
    group.args = &Args;
    group.settle = &Settle;
    group.chapter = 6;
    sh::Run(group);
}

}  // namespace scena_sc6
#undef SH_N
