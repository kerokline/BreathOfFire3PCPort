// BOF3X_SHADOW=scena_sc1: scenario chapter 1's bank through the scenario
// harness (scenario_harness.h), once at start-up. docs/scena_sc1.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC1 --clones at 218eeec,
// against the symbols before this group; checked against the reading), each
// clone's call shape in its comment and its shape field; the callees the
// standard set lacks, and typed stand-ins for the object and cell handler
// tables' entries (which take arguments); the chapter's three handler tables;
// the regions beyond the harness's standard ones; a seed per function that
// puts in the step it switches on and the values its steps wait for; a
// disturbance of the chapter's cells; the object handlers' arguments.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc1.h"
#include "game/scena_sc1_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc1 {
namespace {

namespace sh = scenario_harness;

// 0x539AE0 Scena01_Start: 0x39 bytes; state handler: Scena01_States entry 0, (void)
constexpr sh::CallSite kCalls539AE0[] = {{0x2, 0x5341A0}, {0x15, 0x594E00}};
// 0x53A2B0 Scena01_Run: 0xE bytes; state handler: Scena01_States entry 2, (void); a jmp through Scena01_Runs;  - +0x7 note: jmp through .data 0x660d88, 24 code entries (a data_tables entry)
// 0x53A2C0 Scena01_Scene01: 0xB3 bytes; state handler: Scena01_Runs entry 01, (void)
constexpr sh::CallSite kCalls53A2C0[] = {{0x2C, 0x57C0F0}, {0x5D, 0x594E00}, {0x6F, 0x5734F0}, {0xA9, 0x594E00}};
// 0x53A380 Scena01_Scene02: 0x260 bytes; state handler: Scena01_Runs entry 02, (void)
constexpr sh::CallSite kCalls53A380[] = {{0x42, 0x594E00}, {0x67, 0x587B40}, {0x70, 0x587AE0}, {0x8A, 0x594E00}, {0xBB, 0x587B40}, {0xCD, 0x594E00}, {0xE6, 0x495040}, {0xF0, 0x587740}, {0x112, 0x495040}, {0x188, 0x594E00}, {0x1C7, 0x594E00}, {0x1F4, 0x57C0F0}, {0x1FC, 0x57C7A0}};
constexpr sh::JumpTable kTables53A380[] = {{0x14, 0x230, 12}};
// 0x53A5E0 Scena01_Scene03: 0x4AC bytes; state handler: Scena01_Runs entry 03, (void)
constexpr sh::CallSite kCalls53A5E0[] = {{0x1B, 0x5734F0}, {0x21, 0x531F90}, {0xDA, 0x5905D0}, {0x123, 0x57C0F0}, {0x12B, 0x57C7A0}, {0x140, 0x531F90}, {0x147, 0x5734F0}, {0x1D6, 0x589810}, {0x262, 0x587AE0}, {0x280, 0x589810}, {0x2EE, 0x589810}, {0x35B, 0x587B40}, {0x368, 0x587AE0}, {0x370, 0x589810}, {0x417, 0x57C0F0}, {0x427, 0x57C7A0}};
constexpr sh::JumpTable kTables53A5E0[] = {{0x14, 0x450, 23}};
// 0x53AA90 Scena01_Scene05: 0x191 bytes; state handler: Scena01_Runs entry 05, (void)
constexpr sh::CallSite kCalls53AA90[] = {{0x2B, 0x4976D0}, {0x45, 0x531F90}, {0x4C, 0x4976D0}, {0x89, 0x57C7A0}, {0xAE, 0x587740}, {0xC6, 0x57C7C0}, {0xE4, 0x594E00}, {0xFD, 0x5734F0}, {0x120, 0x57C0F0}, {0x141, 0x57C7A0}, {0x15E, 0x590BB0}};
constexpr sh::JumpTable kTables53AA90[] = {{0x1C, 0x168, 7}};
// 0x53AC30 Scena01_Scene06: 0x500 bytes; state handler: Scena01_Runs entry 06, (void)
constexpr sh::CallSite kCalls53AC30[] = {{0x4D, 0x57C140}, {0x5D, 0x57C7C0}, {0x81, 0x57C0F0}, {0x91, 0x531F90}, {0xB1, 0x587B40}, {0xEF, 0x57C7A0}, {0x107, 0x587AE0}, {0x115, 0x5734F0}, {0x158, 0x495040}, {0x19B, 0x5341C0}, {0x1A9, 0x57C0F0}, {0x1BF, 0x594E00}, {0x208, 0x587AE0}, {0x210, 0x589810}, {0x23A, 0x589810}, {0x2D3, 0x587740}, {0x2DA, 0x5891F0}, {0x31E, 0x5891F0}, {0x390, 0x587AE0}, {0x398, 0x589810}, {0x3C2, 0x589810}, {0x41C, 0x531F90}, {0x42F, 0x531F90}, {0x436, 0x5734F0}, {0x46B, 0x57C0F0}, {0x473, 0x57C7A0}};
constexpr sh::JumpTable kTables53AC30[] = {{0x20, 0x4A0, 17}};
// 0x53B130 Scena01_Scene08: 0x4B9 bytes; state handler: Scena01_Runs entry 08, (void)
constexpr sh::CallSite kCalls53B130[] = {{0x3A, 0x57C7C0}, {0x69, 0x531F90}, {0x6F, 0x5734F0}, {0x9A, 0x57C7C0}, {0xC9, 0x531F90}, {0xCF, 0x5734F0}, {0xF1, 0x57C550}, {0x13E, 0x587740}, {0x16C, 0x587740}, {0x191, 0x57C550}, {0x20E, 0x57C0F0}, {0x224, 0x594E00}, {0x25A, 0x594E00}, {0x26F, 0x587B40}, {0x276, 0x587910}, {0x2A4, 0x587740}, {0x2AB, 0x4976D0}, {0x2D2, 0x495040}, {0x306, 0x533E50}, {0x30F, 0x587AE0}, {0x32D, 0x57C7A0}, {0x35F, 0x5734F0}, {0x389, 0x532ED0}, {0x390, 0x5734F0}, {0x3B3, 0x4410B0}, {0x412, 0x57C7A0}, {0x420, 0x57C0F0}};
constexpr sh::JumpTable kTables53B130[] = {{0x1C, 0x42C, 20}};
// 0x53B5F0 Scena01_Scene09: 0x78C bytes; state handler: Scena01_Runs entry 09, (void)
constexpr sh::CallSite kCalls53B5F0[] = {{0x2E, 0x4976D0}, {0xEB, 0x4976D0}, {0x116, 0x579F00}, {0x120, 0x579F00}, {0x12A, 0x579F00}, {0x134, 0x579F00}, {0x151, 0x57C0F0}, {0x171, 0x579F00}, {0x17F, 0x579F00}, {0x18D, 0x579F00}, {0x19B, 0x579F00}, {0x1A3, 0x57C7A0}, {0x1E1, 0x531F90}, {0x1F6, 0x57C0F0}, {0x20D, 0x531F90}, {0x222, 0x57C0F0}, {0x23B, 0x531F90}, {0x242, 0x5734F0}, {0x293, 0x579F00}, {0x2A1, 0x579F00}, {0x2AF, 0x579F00}, {0x2BD, 0x579F00}, {0x2C5, 0x57C7A0}, {0x2D4, 0x57C140}, {0x2F6, 0x4976D0}, {0x336, 0x57C0F0}, {0x33D, 0x531F90}, {0x359, 0x57C7A0}, {0x39E, 0x594E00}, {0x3BE, 0x57C140}, {0x3D9, 0x57C0F0}, {0x3EF, 0x594E00}, {0x40A, 0x594E00}, {0x421, 0x57C7A0}, {0x45C, 0x587900}, {0x464, 0x57C7C0}, {0x482, 0x587900}, {0x48A, 0x57C7C0}, {0x4A8, 0x587900}, {0x4B0, 0x57C7C0}, {0x50E, 0x587740}, {0x51C, 0x531F90}, {0x523, 0x5734F0}, {0x543, 0x587B40}, {0x54C, 0x587AE0}, {0x591, 0x532ED0}, {0x5B1, 0x4410B0}, {0x5D8, 0x57C0F0}, {0x5E6, 0x57C110}, {0x5ED, 0x531F90}, {0x603, 0x587740}, {0x62F, 0x57C7A0}, {0x662, 0x594E00}, {0x66C, 0x57C7A0}, {0x6B4, 0x57C7C0}, {0x6CA, 0x594E00}};
constexpr sh::JumpTable kTables53B5F0[] = {{0x18, 0x6D4, 46}};
// 0x53BD80 Scena01_Scene0A: 0x294 bytes; state handler: Scena01_Runs entry 0A, (void)
constexpr sh::CallSite kCalls53BD80[] = {{0x28, 0x4976D0}, {0x5E, 0x57C7A0}, {0x8E, 0x57C110}, {0xA0, 0x531F90}, {0xD1, 0x587740}, {0xDB, 0x587740}, {0xEF, 0x57C140}, {0x114, 0x57C140}, {0x13F, 0x594E00}, {0x158, 0x5734F0}, {0x175, 0x57C7A0}, {0x193, 0x5734F0}, {0x1B2, 0x5734F0}, {0x1EE, 0x5734F0}, {0x22A, 0x594E00}};
constexpr sh::JumpTable kTables53BD80[] = {{0x1B, 0x23C, 14}};
// 0x53C020 Scena01_Scene0B: 0x30C bytes; state handler: Scena01_Runs entry 0B, (void)
constexpr sh::CallSite kCalls53C020[] = {{0x27, 0x57C0F0}, {0x3D, 0x594E00}, {0x66, 0x589810}, {0xD3, 0x589810}, {0x149, 0x57C0F0}, {0x151, 0x57C7A0}, {0x15F, 0x4976D0}, {0x184, 0x57C7A0}, {0x1A8, 0x531F90}, {0x1C8, 0x495040}, {0x1F0, 0x57C0F0}, {0x206, 0x594E00}, {0x284, 0x57C0F0}, {0x294, 0x57C7A0}};
constexpr sh::JumpTable kTables53C020[] = {{0x14, 0x2BC, 20}};
// 0x53C330 Scena01_Scene0C: 0x26C bytes; state handler: Scena01_Runs entry 0C, (void)
constexpr sh::CallSite kCalls53C330[] = {{0x39, 0x594E00}, {0x52, 0x5734F0}, {0x71, 0x495040}, {0x9A, 0x587AE0}, {0xB7, 0x589810}, {0x128, 0x57C550}, {0x14F, 0x495040}, {0x17C, 0x4976D0}, {0x1B7, 0x57C0F0}, {0x1CD, 0x594E00}, {0x1F6, 0x57C0F0}, {0x20C, 0x594E00}, {0x215, 0x57C7A0}, {0x21A, 0x533E50}};
constexpr sh::JumpTable kTables53C330[] = {{0x13, 0x240, 11}};
// 0x53C5A0 Scena01_Scene0D: 0x480 bytes; state handler: Scena01_Runs entry 0D, (void)
constexpr sh::CallSite kCalls53C5A0[] = {{0x20, 0x57C7C0}, {0x3C, 0x594E00}, {0xAA, 0x5734F0}, {0xCA, 0x587B40}, {0xEC, 0x587AE0}, {0x10A, 0x57C7C0}, {0x12D, 0x57C0F0}, {0x142, 0x594E00}, {0x16D, 0x57C7A0}, {0x187, 0x57C0F0}, {0x1A5, 0x53D3B0}, {0x1B9, 0x531F90}, {0x1D5, 0x53D3B0}, {0x20A, 0x57C7A0}, {0x224, 0x57C0F0}, {0x24A, 0x57C7A0}, {0x263, 0x57C0F0}, {0x27C, 0x4976D0}, {0x2D9, 0x57C7A0}, {0x2F3, 0x57C0F0}, {0x329, 0x57C7A0}, {0x343, 0x57C0F0}, {0x35C, 0x4976D0}, {0x3BF, 0x57C7A0}, {0x3D8, 0x57C0F0}, {0x40F, 0x57C7A0}, {0x429, 0x57C0F0}};
constexpr sh::JumpTable kTables53C5A0[] = {{0x14, 0x434, 19}};
// 0x53CA20 Scena01_Scene0E: 0x18C bytes; state handler: Scena01_Runs entry 0E, (void)
constexpr sh::CallSite kCalls53CA20[] = {{0x20, 0x587B40}, {0x29, 0x587AE0}, {0x3E, 0x4976D0}, {0x6D, 0x57C0F0}, {0x83, 0x594E00}, {0x97, 0x495040}, {0xCD, 0x56F670}, {0xDB, 0x4976D0}, {0xFA, 0x495040}, {0x157, 0x594E00}, {0x15F, 0x56D6F0}};
constexpr sh::JumpTable kTables53CA20[] = {{0x13, 0x164, 10}};
// 0x53CBB0 Scena01_Scene0F: 0x128 bytes; state handler: Scena01_Runs entry 0F, (void)
constexpr sh::CallSite kCalls53CBB0[] = {{0x3D, 0x495040}, {0x42, 0x533E50}, {0x49, 0x587B40}, {0x5A, 0x57C7A0}, {0x7D, 0x587910}, {0x9D, 0x587A00}, {0xB0, 0x495040}, {0xD5, 0x587AE0}, {0xDC, 0x4976D0}, {0xFC, 0x57C7A0}};
constexpr sh::JumpTable kTables53CBB0[] = {{0x13, 0x110, 6}};
// 0x53CCE0 Scena01_Scene11: 0x2B1 bytes; state handler: Scena01_Runs entry 11, (void)
constexpr sh::CallSite kCalls53CCE0[] = {{0x3E, 0x495040}, {0x67, 0x533E50}, {0x75, 0x587B40}, {0x81, 0x587910}, {0x92, 0x587A00}, {0xCD, 0x594E00}, {0xD6, 0x587AE0}, {0xFB, 0x57C7A0}, {0x191, 0x533E50}, {0x197, 0x587910}, {0x1C0, 0x594E00}, {0x1E7, 0x57C7A0}, {0x255, 0x57C7A0}};
constexpr sh::JumpTable kTables53CCE0[] = {{0x1C, 0x268, 12}};
// 0x53CFA0 Scena01_Scene12: 0x63 bytes; state handler: Scena01_Runs entry 12, (void)
constexpr sh::CallSite kCalls53CFA0[] = {{0x18, 0x57C7A0}, {0x3C, 0x587B40}, {0x45, 0x587AE0}, {0x53, 0x531F90}};
// 0x53D010 Scena01_Scene14: 0x53 bytes; state handler: Scena01_Runs entry 14, (void)
constexpr sh::CallSite kCalls53D010[] = {{0x1A, 0x57C7A0}, {0x3B, 0x4976D0}};
// 0x53D070 Scena01_Scene15: 0x45 bytes; state handler: Scena01_Runs entry 15, (void)
constexpr sh::CallSite kCalls53D070[] = {{0x18, 0x57C7A0}, {0x2E, 0x4976D0}};
// 0x53D0C0 Scena01_Scene16: 0x45 bytes; state handler: Scena01_Runs entry 16, (void)
constexpr sh::CallSite kCalls53D0C0[] = {{0x18, 0x57C7A0}, {0x2E, 0x4976D0}};
// 0x53D110 Scena01_Scene17: 0x2A0 bytes; state handler: Scena01_Runs entry 17, (void)
constexpr sh::CallSite kCalls53D110[] = {{0x3B, 0x495040}, {0x64, 0x533E50}, {0x72, 0x587B40}, {0x7E, 0x587910}, {0x8F, 0x587A00}, {0x153, 0x495040}, {0x181, 0x57C7A0}, {0x1A9, 0x533E50}, {0x1B7, 0x587B40}, {0x1C3, 0x587910}, {0x1D4, 0x587A00}, {0x231, 0x594E00}, {0x23A, 0x587AE0}};
constexpr sh::JumpTable kTables53D110[] = {{0x20, 0x244, 13}};
// 0x53D3B0 Scena01_PlaceEffect: 0xB3 bytes; called directly by Scena01_Scene0D with one word (n)
constexpr sh::CallSite kCalls53D3B0[] = {{0xA, 0x589810}};
// 0x53D470 Scena01_ObjectHook: 0x1D bytes; vtable slot 1 (the object trigger, 0x56D6D0): (object), no answer read;  - +0x12 note: call through .data 0x660de8, 18 code entries (a data_tables entry)
// 0x53D490 Scena01_Object01: 0x3E bytes; object handler: Scena01_ObjectHandlers entry 01, (object, 0x903F98)
constexpr sh::CallSite kCalls53D490[] = {{0x8, 0x57C140}, {0x19, 0x57C7C0}, {0x35, 0x57C0F0}};
// 0x53D4D0 Scena01_Object02: 0x1E bytes; object handler: Scena01_ObjectHandlers entry 02, (object, 0x903F98)
constexpr sh::CallSite kCalls53D4D0[] = {{0x0, 0x57C7C0}};
// 0x53D4F0 Scena01_Object03: 0x20 bytes; object handler: Scena01_ObjectHandlers entry 03, (object, 0x903F98)
constexpr sh::CallSite kCalls53D4F0[] = {{0x0, 0x57C7C0}};
// 0x53D510 Scena01_Object04: 0x20 bytes; object handler: Scena01_ObjectHandlers entry 04, (object, 0x903F98)
constexpr sh::CallSite kCalls53D510[] = {{0x0, 0x57C7C0}};
// 0x53D530 Scena01_Object05: 0x20 bytes; object handler: Scena01_ObjectHandlers entry 05, (object, 0x903F98)
constexpr sh::CallSite kCalls53D530[] = {{0x0, 0x57C7C0}};
// 0x53D550 Scena01_Object06: 0x7D bytes; object handler: Scena01_ObjectHandlers entry 06, (object, 0x903F98)
constexpr sh::CallSite kCalls53D550[] = {{0x1, 0x57C7C0}, {0x5D, 0x57C4C0}};
// 0x53D5D0 Scena01_Object07: 0x7A bytes; object handler: Scena01_ObjectHandlers entry 07, (object, 0x903F98)
constexpr sh::CallSite kCalls53D5D0[] = {{0x1, 0x57C7C0}, {0x43, 0x57C4C0}};
// 0x53D650 Scena01_Object08: 0x83 bytes; object handler: Scena01_ObjectHandlers entry 08, (object, 0x903F98)
constexpr sh::CallSite kCalls53D650[] = {{0x1, 0x57C7C0}, {0x4A, 0x57C4C0}};
// 0x53D6E0 Scena01_Object09: 0x71 bytes; object handler: Scena01_ObjectHandlers entry 09, (object, 0x903F98)
constexpr sh::CallSite kCalls53D6E0[] = {{0x0, 0x57C7C0}, {0x4A, 0x57C4C0}};
// 0x53D760 Scena01_Object0A: 0x8 bytes; object handler: Scena01_ObjectHandlers entry 0A, (object, 0x903F98)
// 0x53D770 Scena01_Object0B: 0x8 bytes; object handler: Scena01_ObjectHandlers entry 0B, (object, 0x903F98)
// 0x53D780 Scena01_Object0C: 0x8 bytes; object handler: Scena01_ObjectHandlers entry 0C, (object, 0x903F98)
// 0x53D790 Scena01_Object0D: 0x8 bytes; object handler: Scena01_ObjectHandlers entry 0D, (object, 0x903F98)
// 0x53D7A0 Scena01_Object0E: 0x2A bytes; object handler: Scena01_ObjectHandlers entry 0E, (object, 0x903F98)
constexpr sh::CallSite kCalls53D7A0[] = {{0x0, 0x57C7C0}};
// 0x53D7D0 Scena01_Object0F: 0x1E bytes; object handler: Scena01_ObjectHandlers entry 0F, (object, 0x903F98)
constexpr sh::CallSite kCalls53D7D0[] = {{0x0, 0x57C7C0}};
// 0x53D7F0 Scena01_Object10: 0x20 bytes; object handler: Scena01_ObjectHandlers entry 10, (object, 0x903F98)
constexpr sh::CallSite kCalls53D7F0[] = {{0x0, 0x57C7C0}};
// 0x53D810 Scena01_Object11: 0x14 bytes; object handler: Scena01_ObjectHandlers entry 11, (object, 0x903F98)
constexpr sh::CallSite kCalls53D810[] = {{0x0, 0x57C7C0}};
// 0x53DD20 Scena01_CellHook: 0x2A bytes; vtable slot 4, the cell hook: (x, z) -> al;  - +0x23 note: jmp through .data 0x660e3c, 2 code entries (a data_tables entry)
constexpr sh::CallSite kCalls53DD20[] = {{0x11, 0x56D800}};
// 0x53DD50 Scena01_Cell: 0x42 bytes; Scena01_CellHandlers entry, tail-jumped with the hook's (x, z) -> al
constexpr sh::CallSite kCalls53DD50[] = {{0x8, 0x57C140}, {0x1D, 0x57C140}, {0x29, 0x57C7C0}};
#define SC1_N(a) static_cast<int>(sizeof a / sizeof a[0])
const sh::Clone kClones[] = {
    {"Scena01_Start", 0x539AE0, 0x39, kCalls539AE0, SC1_N(kCalls539AE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Start), 0, false, sh::Shape::kState},
    {"Scena01_Run", 0x53A2B0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Run), 0, false, sh::Shape::kState},
    {"Scena01_Scene01", 0x53A2C0, 0xB3, kCalls53A2C0, SC1_N(kCalls53A2C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Scene01), 0, false, sh::Shape::kState},
    {"Scena01_Scene02", 0x53A380, 0x260, kCalls53A380, SC1_N(kCalls53A380), nullptr, 0, kTables53A380, SC1_N(kTables53A380), reinterpret_cast<const void*>(&::Scena01_Scene02), 0, false, sh::Shape::kState},
    {"Scena01_Scene03", 0x53A5E0, 0x4AC, kCalls53A5E0, SC1_N(kCalls53A5E0), nullptr, 0, kTables53A5E0, SC1_N(kTables53A5E0), reinterpret_cast<const void*>(&::Scena01_Scene03), 0, false, sh::Shape::kState},
    {"Scena01_Scene05", 0x53AA90, 0x191, kCalls53AA90, SC1_N(kCalls53AA90), nullptr, 0, kTables53AA90, SC1_N(kTables53AA90), reinterpret_cast<const void*>(&::Scena01_Scene05), 0, false, sh::Shape::kState},
    {"Scena01_Scene06", 0x53AC30, 0x500, kCalls53AC30, SC1_N(kCalls53AC30), nullptr, 0, kTables53AC30, SC1_N(kTables53AC30), reinterpret_cast<const void*>(&::Scena01_Scene06), 0, false, sh::Shape::kState},
    {"Scena01_Scene08", 0x53B130, 0x4B9, kCalls53B130, SC1_N(kCalls53B130), nullptr, 0, kTables53B130, SC1_N(kTables53B130), reinterpret_cast<const void*>(&::Scena01_Scene08), 0, false, sh::Shape::kState},
    {"Scena01_Scene09", 0x53B5F0, 0x78C, kCalls53B5F0, SC1_N(kCalls53B5F0), nullptr, 0, kTables53B5F0, SC1_N(kTables53B5F0), reinterpret_cast<const void*>(&::Scena01_Scene09), 0, false, sh::Shape::kState},
    {"Scena01_Scene0A", 0x53BD80, 0x294, kCalls53BD80, SC1_N(kCalls53BD80), nullptr, 0, kTables53BD80, SC1_N(kTables53BD80), reinterpret_cast<const void*>(&::Scena01_Scene0A), 0, false, sh::Shape::kState},
    {"Scena01_Scene0B", 0x53C020, 0x30C, kCalls53C020, SC1_N(kCalls53C020), nullptr, 0, kTables53C020, SC1_N(kTables53C020), reinterpret_cast<const void*>(&::Scena01_Scene0B), 0, false, sh::Shape::kState},
    {"Scena01_Scene0C", 0x53C330, 0x26C, kCalls53C330, SC1_N(kCalls53C330), nullptr, 0, kTables53C330, SC1_N(kTables53C330), reinterpret_cast<const void*>(&::Scena01_Scene0C), 0, false, sh::Shape::kState},
    {"Scena01_Scene0D", 0x53C5A0, 0x480, kCalls53C5A0, SC1_N(kCalls53C5A0), nullptr, 0, kTables53C5A0, SC1_N(kTables53C5A0), reinterpret_cast<const void*>(&::Scena01_Scene0D), 0, false, sh::Shape::kState},
    {"Scena01_Scene0E", 0x53CA20, 0x18C, kCalls53CA20, SC1_N(kCalls53CA20), nullptr, 0, kTables53CA20, SC1_N(kTables53CA20), reinterpret_cast<const void*>(&::Scena01_Scene0E), 0, false, sh::Shape::kState},
    {"Scena01_Scene0F", 0x53CBB0, 0x128, kCalls53CBB0, SC1_N(kCalls53CBB0), nullptr, 0, kTables53CBB0, SC1_N(kTables53CBB0), reinterpret_cast<const void*>(&::Scena01_Scene0F), 0, false, sh::Shape::kState},
    {"Scena01_Scene11", 0x53CCE0, 0x2B1, kCalls53CCE0, SC1_N(kCalls53CCE0), nullptr, 0, kTables53CCE0, SC1_N(kTables53CCE0), reinterpret_cast<const void*>(&::Scena01_Scene11), 0, false, sh::Shape::kState},
    {"Scena01_Scene12", 0x53CFA0, 0x63, kCalls53CFA0, SC1_N(kCalls53CFA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Scene12), 0, false, sh::Shape::kState},
    {"Scena01_Scene14", 0x53D010, 0x53, kCalls53D010, SC1_N(kCalls53D010), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Scene14), 0, false, sh::Shape::kState},
    {"Scena01_Scene15", 0x53D070, 0x45, kCalls53D070, SC1_N(kCalls53D070), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Scene15), 0, false, sh::Shape::kState},
    {"Scena01_Scene16", 0x53D0C0, 0x45, kCalls53D0C0, SC1_N(kCalls53D0C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Scene16), 0, false, sh::Shape::kState},
    {"Scena01_Scene17", 0x53D110, 0x2A0, kCalls53D110, SC1_N(kCalls53D110), nullptr, 0, kTables53D110, SC1_N(kTables53D110), reinterpret_cast<const void*>(&::Scena01_Scene17), 0, false, sh::Shape::kState},
    {"Scena01_PlaceEffect", 0x53D3B0, 0xB3, kCalls53D3B0, SC1_N(kCalls53D3B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_PlaceEffect), 0, false, sh::Shape::kEntry},
    {"Scena01_ObjectHook", 0x53D470, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_ObjectHook), 0, false, sh::Shape::kObject},
    {"Scena01_Object01", 0x53D490, 0x3E, kCalls53D490, SC1_N(kCalls53D490), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object01), 0, false, sh::Shape::kEntry},
    {"Scena01_Object02", 0x53D4D0, 0x1E, kCalls53D4D0, SC1_N(kCalls53D4D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object02), 0, false, sh::Shape::kEntry},
    {"Scena01_Object03", 0x53D4F0, 0x20, kCalls53D4F0, SC1_N(kCalls53D4F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object03), 0, false, sh::Shape::kEntry},
    {"Scena01_Object04", 0x53D510, 0x20, kCalls53D510, SC1_N(kCalls53D510), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object04), 0, false, sh::Shape::kEntry},
    {"Scena01_Object05", 0x53D530, 0x20, kCalls53D530, SC1_N(kCalls53D530), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object05), 0, false, sh::Shape::kEntry},
    {"Scena01_Object06", 0x53D550, 0x7D, kCalls53D550, SC1_N(kCalls53D550), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object06), 0, false, sh::Shape::kEntry},
    {"Scena01_Object07", 0x53D5D0, 0x7A, kCalls53D5D0, SC1_N(kCalls53D5D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object07), 0, false, sh::Shape::kEntry},
    {"Scena01_Object08", 0x53D650, 0x83, kCalls53D650, SC1_N(kCalls53D650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object08), 0, false, sh::Shape::kEntry},
    {"Scena01_Object09", 0x53D6E0, 0x71, kCalls53D6E0, SC1_N(kCalls53D6E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object09), 0, false, sh::Shape::kEntry},
    {"Scena01_Object0A", 0x53D760, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object0A), 0, false, sh::Shape::kEntry},
    {"Scena01_Object0B", 0x53D770, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object0B), 0, false, sh::Shape::kEntry},
    {"Scena01_Object0C", 0x53D780, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object0C), 0, false, sh::Shape::kEntry},
    {"Scena01_Object0D", 0x53D790, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object0D), 0, false, sh::Shape::kEntry},
    {"Scena01_Object0E", 0x53D7A0, 0x2A, kCalls53D7A0, SC1_N(kCalls53D7A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object0E), 0, false, sh::Shape::kEntry},
    {"Scena01_Object0F", 0x53D7D0, 0x1E, kCalls53D7D0, SC1_N(kCalls53D7D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object0F), 0, false, sh::Shape::kEntry},
    {"Scena01_Object10", 0x53D7F0, 0x20, kCalls53D7F0, SC1_N(kCalls53D7F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object10), 0, false, sh::Shape::kEntry},
    {"Scena01_Object11", 0x53D810, 0x14, kCalls53D810, SC1_N(kCalls53D810), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Object11), 0, false, sh::Shape::kEntry},
    {"Scena01_CellHook", 0x53DD20, 0x2A, kCalls53DD20, SC1_N(kCalls53DD20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_CellHook), 0xFF, false, sh::Shape::kHook},
    {"Scena01_Cell", 0x53DD50, 0x42, kCalls53DD50, SC1_N(kCalls53DD50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena01_Cell), 0xFF, false, sh::Shape::kHook},
};
#undef SC1_N

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The object handlers take (object, 0x903F98) and the cell handler (x, z): a
// .data table's handler recorder logs no arguments, so each of their table
// entries gets a typed stand-in of the fuzz's own, keyed on the handler's
// address (the harness finds it before registering a handler recorder).
// Entry 0 of Scena01_ObjectHandlers is 0x437CC0, the bare ret the runs
// table holds too: it reads nothing, and keeps the handler recorder.
constexpr std::uint32_t kObjectEntries[17] = {0x53D490, 0x53D4D0, 0x53D4F0, 0x53D510, 0x53D530, 0x53D550, 0x53D5D0, 0x53D650,
                                              0x53D6E0, 0x53D760, 0x53D770, 0x53D780, 0x53D790, 0x53D7A0, 0x53D7D0, 0x53D7F0,
                                              0x53D810};
template <unsigned I> void __cdecl ObjectEntry(unsigned char* object, unsigned char* row) {
    sh::Record(kObjectEntries[I], Key(object), Key(row));
    sh::Stir();
}
unsigned char __cdecl CellEntry(int x, int z) {
    sh::Record(0x53DD50, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    sh::Stir();
    return static_cast<unsigned char>(sh::Noise());
}

void Disturb(std::uint32_t h);
void Settle();
const char* g_name = "";   // the clone being fuzzed, for Disturb
// The effect of the busiest callees: half the time, one of the chapter's own
// cells moved (the group's Disturb), which the harness's own disturbance
// reaches only one call in 24 - the cells stored around a call (the step,
// Cond_ByteFE, 0x904CD0, the effect slot) and those read again after one.
std::uint32_t Move(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t h = sh::Noise();
    if (h & 1) {
        Disturb(h);
        Settle();
    }
    return answer;
}

// A callee ours reaches by its name (Capcom's address, or our function).
#define SC1_NAMED(name) #name, ::bof3::addr::name, KeyOf(&::name)
// One ours and the originals reach by its address alone.
#define SC1_RAW(address) #address, address, address
#define SC1_OBJECT(i) \
    {"Scena01_ObjectHandlers[" #i "]", kObjectEntries[i - 1], kObjectEntries[i - 1], 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr, \
     reinterpret_cast<const void*>(&ObjectEntry<i - 1>)}
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define SC1_MOVED(name, n, ...) {SC1_NAMED(name), n, {__VA_ARGS__}, sh::Answer::kGarbage, 0, 0, {}, &Move}
const sh::Callee kCallees[] = {
    // standard callees with the standard masks, recorded with Move
    SC1_MOVED(Field_ChangeArea, 4, kU16, kAll, kAll, kU8),
    SC1_MOVED(Party_DropIn, 1, kU8),
    SC1_MOVED(Transition_Start, 1, kU8),
    SC1_MOVED(Sound_PlayEffect, 1, kU16),
    SC1_MOVED(Msg_OpenScript, 1, kU16),
    SC1_MOVED(Flags_Set, 2, kAll, kU8),
    SC1_MOVED(ScriptFlags_Set40, 0),
    SC1_MOVED(ScriptFlags_Clear40, 0),
    SC1_MOVED(Kind2_Place, 1, kU8),
    SC1_MOVED(Music_Play, 2, kAll, kAll),
    SC1_MOVED(Music_FadeOutStop, 1, kAll),
    // what the standard set lacks
    {SC1_NAMED(Menu_DrawHand), 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC1_NAMED(Scena01_PlaceEffect), 1, {kU8}, sh::Answer::kGarbage, 0, 0},   // ours, called directly by Scena01_Scene0D
    {SC1_RAW(0x533E50), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC1_RAW(0x57C550), 2, {kU16, kU8}, sh::Answer::kFlag, 0, 0},
    // the record's index 0 or 1, or 0xFF (0xFF..0x01 wraps)
    {SC1_RAW(0x56D800), 4, {kAll, kU8, kU8, kU8}, sh::Answer::kByte, 0xFF, 0x01},
    // the handler tables' typed stand-ins
    SC1_OBJECT(1), SC1_OBJECT(2), SC1_OBJECT(3), SC1_OBJECT(4), SC1_OBJECT(5), SC1_OBJECT(6), SC1_OBJECT(7), SC1_OBJECT(8),
    SC1_OBJECT(9), SC1_OBJECT(10), SC1_OBJECT(11), SC1_OBJECT(12), SC1_OBJECT(13), SC1_OBJECT(14), SC1_OBJECT(15),
    SC1_OBJECT(16), SC1_OBJECT(17),
    {"Scena01_CellHandlers", 0x53DD50, 0x53DD50, 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&CellEntry)},
};
#undef SC1_OBJECT
#undef SC1_MOVED
#undef SC1_RAW
#undef SC1_NAMED

// The chapter's tables, read in place.
const sh::DataTable kTables[] = {
    {at::kRuns, at::kRunCount},             // Scena01_Runs
    {at::kObjects, at::kObjectCount},       // Scena01_ObjectHandlers
    {at::kCellHandlers, at::kCellCount},    // Scena01_CellHandlers
};

// What the bank reads and writes beyond the harness's 22 standard regions.
const sh::Region kRegions[] = {
    {0x929F00, 0x14},                     // 0x929F00, 0x929F0C (the shop bytes), Field_Kind2Hold 0x929F12
    {0x7E11E0 + 0xFF * 0x80, 4},          // record 0xFF's first byte: Scena01_Scene02 step 4 clears it (section 6)
    {0x803150, 8},                        // 0x803157
    {0x905E20, 4},                        // Cond_ByteFE
    {0x66C7E8, 2},                        // Game_Mode
    {0x7DEE44, 8},                        // the choice bits and word
    {0x904CD0, 1},
    {0x92BF17, 1},
    {0x669730, 1},
    {0x903A70, 8 * 0xA4},                 // the eight member records (Scena01_Scene06 step 0xF)
};

// --- the seed ------------------------------------------------------------------

unsigned char* M(std::uint32_t a) { return sh::Mem(a); }
void SetWordAt(std::uint32_t a, unsigned v) { move_script::SetWord(M(a), v); }

// The steps each function switches on (its cases, and one past the last),
// and the pairs (step, counter 0) its waits need.
struct Steps { const char* name; std::uint8_t steps[48]; unsigned n; std::uint8_t pairs[20][2]; unsigned n_pairs; };
const Steps kSteps[] = {
    {"Scena01_Scene01", {0, 1, 2, 3}, 4, {{0, 1}, {2, 0x1E}}, 2},
    {"Scena01_Scene02", {0, 1, 2, 3, 4, 5, 8, 9, 0xA, 0xB, 0xC}, 11, {{0, 2}, {1, 4}, {2, 0xA}, {3, 1}, {9, 3}, {0xA, 1}, {0xB, 1}, {4, 0}, {8, 0}}, 9},
    {"Scena01_Scene03", {0, 1, 2, 3, 4, 5, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}, 19,
     {{2, 0x1F}, {3, 0x28}, {4, 0x2A}, {0xB, 1}, {0xD, 3}, {0x11, 0xA}, {0x12, 0xB}, {0x13, 0x10}, {0x14, 0x18}, {0x16, 0x22}}, 10},
    {"Scena01_Scene05", {0, 1, 2, 3, 0xA, 0xB, 0xC, 0xD}, 8, {{0xC, 0x16}}, 1},
    {"Scena01_Scene06", {0, 1, 2, 3, 4, 0xA, 0xB, 0xC, 0xD, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1B}, 20,
     {{2, 0xC}, {3, 0x23}, {0xA, 2}, {0xB, 3}, {0xC, 0x1A}, {0x10, 1}, {0x11, 5}, {0x15, 0x1E}, {0x1A, 0xF}, {0xF, 0}, {0x12, 0}, {0x13, 0}}, 12},
    {"Scena01_Scene08", {0, 1, 2, 3, 4, 5, 6, 7, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D}, 22,
     {{1, 3}, {6, 0x12}, {0xE, 0x14}, {0x13, 0xB}, {0x37, 0x1F}, {0x38, 0x20}, {0x39, 0x23}, {0x3A, 0x24}, {0x3C, 0x28}}, 9},
    {"Scena01_Scene09", {0, 1, 2, 3, 4, 5, 6, 8, 9, 0xB, 0xC, 0xD, 0xE, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19,
                         0x1A, 0x1B, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2D, 0x2E}, 39,
     {{8, 7}, {0xB, 0x1E}, {0xB, 0x32}, {0xC, 0x21}, {0xD, 0x22}, {0xE, 0x35}, {0x14, 0x3C}, {0x14, 0x46}, {0x17, 0x45}, {0x18, 4},
      {0x19, 9}, {0x1F, 0x66}, {0x23, 0x68}, {0x24, 0x6C}, {0x25, 0x6D}, {0x27, 0x78}, {0x1A, 0}, {0x2D, 0}, {3, 0}}, 19},
    {"Scena01_Scene0A", {0, 1, 2, 3, 4, 5, 0xB, 0xC, 0xD, 0x14, 0x15, 0x19, 0x1B, 0x1E, 0x1F, 0x20}, 16,
     {{2, 0x14}, {0xB, 0x14}, {0xD, 0xA}, {0x15, 8}, {0x19, 0xD}, {0x1F, 0xA}}, 6},
    {"Scena01_Scene0B", {0, 1, 2, 3, 4, 5, 0xA, 0xB, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14}, 14,
     {{2, 1}, {3, 3}, {4, 0x12}, {0x10, 0x19}, {0x13, 0x19}}, 5},
    {"Scena01_Scene0C", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB}, 12, {{2, 2}, {4, 5}, {5, 6}, {6, 8}, {9, 0x14}}, 5},
    {"Scena01_Scene0D", {0, 1, 2, 3, 4, 5, 6, 7, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13}, 18,
     {{2, 6}, {3, 0x13}, {4, 0x1E}, {5, 0x28}, {6, 4}, {0xA, 0xB}, {0xB, 0xD}}, 7},
    {"Scena01_Scene0E", {0, 1, 3, 4, 5, 6, 7, 8, 9, 0xA}, 10, {{4, 7}, {9, 0x14}}, 2},
    {"Scena01_Scene0F", {0, 1, 2, 3, 4, 5, 6}, 7, {{0, 0x64}, {0, 0x65}}, 2},
    {"Scena01_Scene11", {0, 1, 2, 3, 4, 5, 6, 7, 8, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19}, 15,
     {{0, 0x64}, {0, 0x65}, {2, 0xC8}, {2, 0xC9}, {5, 0x64}, {5, 0x65}, {6, 1}, {0x18, 1}}, 8},
    {"Scena01_Scene12", {0, 1, 2}, 3, {{1, 2}}, 1},
    {"Scena01_Scene14", {0, 1, 2}, 3, {}, 0},
    {"Scena01_Scene15", {0, 1, 2}, 3, {}, 0},
    {"Scena01_Scene16", {0, 1, 2}, 3, {}, 0},
    {"Scena01_Scene17", {0, 1, 2, 3, 4, 5, 6, 0xA, 0xB, 0xC, 0xD, 0xE, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19}, 18,
     {{0, 0x64}, {0, 0x65}, {2, 0xC8}, {2, 0xC9}, {5, 0x64}, {5, 0x65}, {0x18, 1}}, 7},
};
const Steps* g_steps[sizeof kClones / sizeof kClones[0]];

// Every value a step compares counter 0 with.
std::uint32_t CounterZero() {
    return SH_PICK(1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xF, 0x10, 0x12, 0x13, 0x14, 0x16, 0x18, 0x19, 0x1A, 0x1E,
                   0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x28, 0x2A, 0x32, 0x35, 0x3C, 0x45, 0x46, 0x64, 0x65, 0x66, 0x68,
                   0x6C, 0x6D, 0x78, 0xC8, 0xC9);
}
std::uint32_t EffectSlot(std::uint32_t h) { return h % 5 == 0 ? 0xFF : h % 20; }

void Seed(unsigned k) {
    g_name = kClones[k].name;
    // Every round: the effect slot a real one, the member index 0..7, the
    // object hook's index in each object the kObject shape may pass.
    M(at::kEffectSlot)[0] = static_cast<unsigned char>(EffectSlot(sh::Next()));
    M(at::kScene06Member)[0] = static_cast<unsigned char>(sh::Next() % 8);
    for (unsigned i = 0; i < 4; ++i) sh::SpriteRecord(i)[0x86] = static_cast<unsigned char>(sh::Next() % at::kObjectCount);
    if (sh::Half()) sh::SetPointer(0x937F88, sh::ObjectOf(sh::Next()));   // Sprite_Current at a party object
    // The waits, each on its boundary two in three.
    if (sh::Often()) M(at::kCounters)[0] = static_cast<unsigned char>(CounterZero());
    if (sh::Half()) M(at::kCounters + 1)[0] = static_cast<unsigned char>(SH_PICK(1, 2, 3));
    if (sh::Half()) M(at::kCounters + 2)[0] = static_cast<unsigned char>(SH_PICK(0, 0x80, 0x7F, 0xFF));
    if (sh::Often()) M(at::kCounters + 3)[0] = static_cast<unsigned char>(SH_PICK(0x12, 0x13, 0x14, 0x19, 0x2C, 0x2D, 0x4E, 0x4F, 0x80, 0x7F, 0));
    if (sh::Often()) M(0x66C7D8)[0] = static_cast<unsigned char>(SH_PICK(0, 2, 1));
    if (sh::Often()) SetWordAt(0x66C810, 0);
    if (sh::Often()) M(0x929F12)[0] = 0;
    if (sh::Half()) SetWordAt(0x7E1BEC, SH_PICK(0, 0x40, 0x41));
    if (sh::Often()) M(0x929EC0)[0] = static_cast<unsigned char>(SH_PICK(2, 3, 1, 4));
    if (sh::Half()) SetWordAt(at::kTimer, SH_PICK(0, 1, 0x6C, 0x6D, 0xB2, 0xB3, 0xFFFF));
    if (sh::Half()) M(0x8034F1)[0] = 4;    // Cond_ByteFD
    if (sh::Half()) SetWordAt(at::kChoiceWord, 2);
    if (sh::Half()) M(at::kChoiceBits)[0] |= 2;
    // The leader's cell words and position (Scena01_Scene08 / 09's places).
    unsigned char* const lead = sh::ObjectOf(0);
    if (sh::Often()) move_script::SetWord(lead + 0x36, SH_PICK(0x1B, 0x1C, 0x3D, 0x3E, 0x3F, 0x41, 0x5B, 0x5C, 0x5D, 0x5E));
    if (sh::Often()) move_script::SetWord(lead + 0x3A, SH_PICK(9, 0xD, 0x23, 0x24, 0x2A, 0x2B, 0xFFFF));
    if (sh::Half()) move_script::SetLong(lead + 0x38, static_cast<std::int32_t>(SH_PICK(0x480000, 0x488000)));
    if (sh::Half())
        move_script::SetLong(lead + 0x34, static_cast<std::int32_t>(SH_PICK(0x3DFFFF, 0x3E0000, 0x3F8000, 0x3F8001, 0x80000000u)));
    if (sh::Half()) move_script::SetWord(lead + 0x58, 0xC);
    if (sh::Half()) lead[0x4A] = 1;
    if (sh::Half()) lead[0x89] = 4;
    // The function's own.
    if (g_steps[k]) {
        const Steps& s = *g_steps[k];
        const unsigned r = sh::Next() % 3;
        if (r == 0 && s.n_pairs) {
            const unsigned i = sh::Next() % s.n_pairs;
            M(at::kStep)[0] = s.pairs[i][0];
            M(at::kCounters)[0] = s.pairs[i][1];
        } else {
            M(at::kStep)[0] = r != 2 ? s.steps[sh::Next() % s.n] : static_cast<unsigned char>(sh::Next());
        }
    }
    // The places Scena01_Scene08 step 0 and Scena01_Scene09 steps 3, 0x1A, 0x2D test.
    if (std::strcmp(kClones[k].name, "Scena01_Scene08") == 0 && sh::Half()) {
        M(at::kStep)[0] = 0;
        move_script::SetWord(lead + 0x36, SH_PICK(0x1A, 0x1B, 0x1C, 0x1D));
        move_script::SetWord(lead + 0x3A, SH_PICK(0x22, 0x23, 0x24, 0x8000));
    }
    if (std::strcmp(kClones[k].name, "Scena01_Scene09") == 0 && sh::Half()) {
        M(at::kStep)[0] = static_cast<unsigned char>(SH_PICK(3, 0x1A, 0x2D));
        move_script::SetWord(lead + 0x36, SH_PICK(0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F));
        move_script::SetWord(lead + 0x3A, SH_PICK(0xC, 0xD, 0xE, 0x2A, 0x2B));
    }
    if (std::strcmp(kClones[k].name, "Scena01_Scene0D") == 0 && sh::Half()) {
        M(at::kStep)[0] = static_cast<unsigned char>(SH_PICK(0xC, 0xF, 0x12));
        M(at::kCounters + 3)[0] = static_cast<unsigned char>(SH_PICK(0x14, 0x19, 0x13));
        if (sh::Often()) M(at::kChoiceBits)[0] |= 2;
    }
    if (std::strcmp(kClones[k].name, "Scena01_Run") == 0) MoveScript_Var7 = static_cast<signed char>(sh::Next() % at::kRunCount);
}

// The arguments: an object and 0x903F98 for the object handlers, n for
// Scena01_PlaceEffect; the hooks' (x, z) and the object hook's object are
// the shapes' own.
void Args(unsigned k, std::uint32_t* a) {
    const char* const n = kClones[k].name;
    if (std::strncmp(n, "Scena01_Object", 14) == 0 && std::strcmp(n, "Scena01_ObjectHook") != 0) {
        a[0] = Key(sh::SpriteRecord(a[0]));
        a[1] = at::kStartWord;
    } else if (std::strcmp(n, "Scena01_PlaceEffect") == 0 && (a[1] & 1)) {
        a[0] = (a[1] & 2) ? 0x93 : 0x92;
    }
}

// A cell of the chapter's moved after a call (from the harness's disturbance
// one call in 24, from Move half the time): what the steps store around a
// call and read again after one.
void Disturb(std::uint32_t h) {
    const unsigned b = (h >> 13) & 0xFF;
    unsigned b2 = 0;
    switch ((h >> 3) % 10) {
    case 0: {
        static const unsigned char kWaits[] = {0x14, 0x19, 0x32, 0x46, 0x65, 0x1E, 0xA, 0x22};
        // Scena01_Scene09 steps 0xB and 0x14 test counter 0 again after the first test's calls
        if (std::strcmp(g_name, "Scena01_Scene09") == 0 && (b & 1)) b2 = (b & 2) ? 0x32 : 0x46;
        M(at::kCounters)[0] = static_cast<unsigned char>(b2 ? b2 : b & 1 ? b : kWaits[(b >> 1) % 8]);
        break;
    }
    case 1: M(at::kCounters + ((h >> 11) & 3))[0] = static_cast<unsigned char>(b); break;
    case 2: M(0x929F12)[0] = static_cast<unsigned char>((h >> 11) & 1); break;
    case 3: M(at::kEffectSlot)[0] = static_cast<unsigned char>(EffectSlot(h >> 11)); break;
    case 4: M(0x929EC0)[0] = static_cast<unsigned char>(2 + ((h >> 11) & 1)); break;
    case 5: SetWordAt(0x7E1BEC, (h >> 11) & 1 ? 0x40 : 0); break;
    case 6:   // Scena01_Scene0D steps 0xF and 0x12 read counter 3 again after the first end's calls
        M(at::kCounters + 3)[0] = static_cast<unsigned char>(std::strcmp(g_name, "Scena01_Scene0D") == 0 ? 0x19
                                                             : (h >> 11) % 3 == 0 ? 0x80 : (h >> 11) % 3 == 1 ? 0x14 : 0x19);
        break;
    case 7: M(0x905E20)[0] = static_cast<unsigned char>(b); break;          // Cond_ByteFE
    case 8: M(0x904CD0)[0] = static_cast<unsigned char>(b); break;
    default: M(at::kStep)[0] = static_cast<unsigned char>(b); break;
    }
}

// After any disturbance: the cells a function indexes with after a call.
void Settle() {
    const unsigned char s = M(at::kEffectSlot)[0];
    if (s != 0xFF && s >= 20) M(at::kEffectSlot)[0] = static_cast<unsigned char>(s % 20);
    M(at::kScene06Member)[0] &= 7;
}

}  // namespace

void SelfTest() {
    for (unsigned k = 0; k < sizeof kClones / sizeof kClones[0]; ++k) {
        g_steps[k] = nullptr;
        for (const Steps& s : kSteps)
            if (std::strcmp(s.name, kClones[k].name) == 0) g_steps[k] = &s;
    }
    sh::Group g{"scena_sc1", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
                kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                &Seed, &Disturb, 6000};   // 6,000 rounds a function: the steps are many and each wait is narrow
    g.settle = &Settle;
    g.args = &Args;
    g.chapter = 1;
    sh::Run(g);
}

}  // namespace scena_sc1
