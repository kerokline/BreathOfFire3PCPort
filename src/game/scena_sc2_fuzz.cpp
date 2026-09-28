// BOF3X_SHADOW=scena_sc2: scenario chapter 2's bank through the scenario
// harness (scenario_harness.h), once at start-up. docs/scena_sc2.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC2a / SC2b --clones at
// e4882fb, against the symbols before this group; checked against the
// reading, two rows corrected: Scena02_Scene1B's extent and
// Scena02_Scene08Next added), each clone's call shape in its shape field; the
// callees the standard set lacks, and typed stand-ins for the object handler
// table's entries (which take arguments); the chapter's three handler tables;
// the regions beyond the harness's standard ones; a seed per function that
// puts in the step it switches on and the values its steps wait for; a
// disturbance of the chapter's cells; the arguments of the helpers, the
// object handlers and the step hook.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc2.h"
#include "game/scena_sc2_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc2 {
namespace {

namespace sh = scenario_harness;

// 0x53DDA0 Scena02_Frame: 0xE bytes; +0x7 note: jmp through .data 0x660e64, 3 code entries (a data_tables entry); root: chapter 2 slot 0, the frame (Field_ModeDispatch)
// 0x53DDB0 Scena02_Start: 0x14 bytes
// 0x53DDD0 Scena02_EnterArea: 0x5FC bytes
constexpr sh::CallSite kCalls53DDD0[] = {{0x14, 0x57C140}, {0x30, 0x57C140}, {0x45, 0x57C0F0}, {0x63, 0x57C140}, {0x7C, 0x57C140}, {0x8A, 0x5341C0}, {0x91, 0x542080}, {0x98, 0x542080}, {0xCE, 0x57C140}, {0xDC, 0x587A20}, {0xE4, 0x454810}, {0xEF, 0x5A9949}, {0xF7, 0x454810}, {0x176, 0x57C140}, {0x1AB, 0x57C0F0}, {0x1B9, 0x57C0F0}, {0x1D3, 0x57C140}, {0x232, 0x57C140}, {0x247, 0x57C140}, {0x262, 0x57C7C0}, {0x29E, 0x57C140}, {0x2B2, 0x57C0F0}, {0x2D0, 0x57C7C0}, {0x37A, 0x57C140}, {0x3A9, 0x57C140}, {0x3D0, 0x57C7A0}, {0x42C, 0x57C140}, {0x459, 0x57C0F0}, {0x487, 0x57C0F0}, {0x490, 0x572650}, {0x4A3, 0x57C140}, {0x4FA, 0x57C140}, {0x50E, 0x57C0F0}, {0x515, 0x587B40}, {0x51C, 0x587A20}, {0x524, 0x454810}, {0x52F, 0x5A9949}, {0x537, 0x454810}, {0x544, 0x587AE0}, {0x5A1, 0x57C110}, {0x5A8, 0x587B40}, {0x5AF, 0x587A20}, {0x5B7, 0x454810}, {0x5C2, 0x5A9949}, {0x5CA, 0x454810}, {0x5D7, 0x587AE0}};
constexpr sh::JumpTable kTables53DDD0[] = {{0x13C, 0x5E8, 5}};
// 0x53E3D0 Scena02_Run: 0xE bytes; +0x7 note: jmp through .data 0x660e70, 28 code entries (a data_tables entry)
// 0x53E3E0 Scena02_Scene00: 0x187 bytes
constexpr sh::CallSite kCalls53E3E0[] = {{0x8, 0x57C140}, {0x2F, 0x57C140}, {0x3D, 0x541460}, {0x4E, 0x57C140}, {0x5C, 0x541400}, {0x6C, 0x57C140}, {0x81, 0x57C140}, {0x8F, 0x5414C0}, {0xA0, 0x57C140}, {0xB6, 0x57C140}, {0xCB, 0x57C140}, {0xD9, 0x5414C0}, {0xEA, 0x57C140}, {0xF8, 0x541530}, {0x102, 0x5415A0}, {0x109, 0x541600}, {0x110, 0x541720}, {0x117, 0x541780}, {0x124, 0x57C140}, {0x139, 0x57C140}, {0x147, 0x541660}, {0x14E, 0x5416C0}, {0x155, 0x5417E0}, {0x166, 0x57C140}, {0x174, 0x541840}, {0x17C, 0x5418A0}, {0x181, 0x5419C0}};
// 0x53E570 Scena02_Scene02: 0x314 bytes
constexpr sh::CallSite kCalls53E570[] = {{0x37, 0x594E00}, {0x63, 0x57C7A0}, {0x87, 0x57C0F0}, {0x92, 0x531F90}, {0xAF, 0x57C7A0}, {0xD2, 0x57C0F0}, {0xE4, 0x531F90}, {0x101, 0x57C7A0}, {0x125, 0x57C0F0}, {0x132, 0x57C0F0}, {0x151, 0x594E00}, {0x19B, 0x594E00}, {0x1BE, 0x594E00}, {0x1ED, 0x495040}, {0x1F2, 0x533E50}, {0x1F9, 0x587B40}, {0x20A, 0x57C7A0}, {0x223, 0x587910}, {0x243, 0x587A00}, {0x256, 0x495040}, {0x27B, 0x587AE0}, {0x282, 0x4976D0}, {0x2A2, 0x57C7A0}};
constexpr sh::JumpTable kTables53E570[] = {{0x1B, 0x2B4, 15}};
// 0x53E890 Scena02_Scene04: 0x16C bytes
constexpr sh::CallSite kCalls53E890[] = {{0x2F, 0x594E00}, {0x4C, 0x57C7A0}, {0x6F, 0x57C0F0}, {0x7A, 0x531F90}, {0x97, 0x57C7A0}, {0xBB, 0x57C0F0}, {0xCD, 0x531F90}, {0xE6, 0x57C7A0}, {0x109, 0x57C0F0}, {0x117, 0x57C0F0}, {0x136, 0x594E00}};
constexpr sh::JumpTable kTables53E890[] = {{0x13, 0x148, 9}};
// 0x53EA00 Scena02_Scene05: 0x45 bytes
constexpr sh::CallSite kCalls53EA00[] = {{0x18, 0x57C7A0}, {0x2E, 0x4976D0}};
// 0x53EA50 Scena02_Scene06: 0x194 bytes
constexpr sh::CallSite kCalls53EA50[] = {{0x17, 0x57C7C0}, {0x1E, 0x4976D0}, {0x83, 0x594E00}, {0xA2, 0x57C110}, {0xB0, 0x57C140}, {0xC5, 0x57C110}, {0xD2, 0x57C110}, {0xE0, 0x57C110}, {0xEE, 0x57C0F0}, {0xFB, 0x57C110}, {0x109, 0x57C110}, {0x117, 0x57C110}, {0x124, 0x57C110}, {0x12C, 0x57C7A0}, {0x13E, 0x57C7A0}, {0x167, 0x57C140}};
constexpr sh::JumpTable kTables53EA50[] = {{0x13, 0x17C, 6}};
// 0x53EBF0 Scena02_Scene07: 0x17C bytes
constexpr sh::CallSite kCalls53EBF0[] = {{0x20, 0x531F90}, {0x3F, 0x495040}, {0x65, 0x4976D0}, {0xAD, 0x594E00}, {0xC4, 0x57C7A0}, {0xDD, 0x57C110}, {0xEB, 0x57C140}, {0x100, 0x57C110}, {0x10D, 0x57C110}, {0x11B, 0x57C110}, {0x129, 0x57C0F0}, {0x136, 0x57C110}, {0x144, 0x57C110}, {0x152, 0x57C110}, {0x15F, 0x57C110}};
constexpr sh::JumpTable kTables53EBF0[] = {{0x13, 0x168, 5}};
// 0x53ED70 Scena02_Scene08: 0x3A4 bytes
constexpr sh::CallSite kCalls53ED70[] = {{0x1E, 0x5734F0}, {0x43, 0x532ED0}, {0x5E, 0x4410B0}, {0x80, 0x531F90}, {0x94, 0x57C0F0}, {0xA2, 0x57C0F0}, {0xB8, 0x594E00}, {0xDE, 0x57C0F0}, {0xEB, 0x57C110}, {0xF9, 0x57C110}, {0x101, 0x57C7A0}, {0x118, 0x57C7A0}, {0x139, 0x4976D0}, {0x15D, 0x57C7A0}, {0x18A, 0x57C140}, {0x1C4, 0x57C7C0}, {0x1D8, 0x57C0F0}, {0x1F0, 0x57C140}, {0x22D, 0x57C7C0}, {0x242, 0x57C0F0}, {0x259, 0x57C140}, {0x293, 0x57C7C0}, {0x2A8, 0x57C0F0}, {0x2C4, 0x57C7A0}, {0x2D2, 0x57C0F0}, {0x2DF, 0x57C0F0}, {0x2ED, 0x57C110}, {0x2FB, 0x57C110}, {0x308, 0x57C110}, {0x348, 0x5413D0}, {0x35A, 0x5413D0}, {0x36C, 0x5413D0}};
constexpr sh::JumpTable kTables53ED70[] = {{0xF, 0x374, 12}};
// 0x53F120 Scena02_Scene09: 0x21C bytes
constexpr sh::CallSite kCalls53F120[] = {{0x19, 0x587B40}, {0x20, 0x531F90}, {0x27, 0x5734F0}, {0x37, 0x589810}, {0xA4, 0x589810}, {0x12C, 0x531F90}, {0x154, 0x587AE0}, {0x17A, 0x5734F0}, {0x1A3, 0x5341C0}, {0x1C9, 0x594E00}, {0x1DB, 0x57C0F0}, {0x1E3, 0x57C7A0}};
constexpr sh::JumpTable kTables53F120[] = {{0x13, 0x1FC, 8}};
// 0x53F340 Scena02_Scene0A: 0x18C bytes
constexpr sh::CallSite kCalls53F340[] = {{0x17, 0x57C7C0}, {0x1E, 0x531F90}, {0x61, 0x594E00}, {0x7F, 0x57C0F0}, {0x96, 0x531F90}, {0xB5, 0x5734F0}, {0xDE, 0x532ED0}, {0xF9, 0x4410B0}, {0x114, 0x531F90}, {0x136, 0x57C0F0}, {0x13E, 0x57C7A0}};
constexpr sh::JumpTable kTables53F340[] = {{0x13, 0x160, 11}};
// 0x53F4D0 Scena02_Scene0B: 0x221 bytes
constexpr sh::CallSite kCalls53F4D0[] = {{0x37, 0x495040}, {0x6D, 0x533E50}, {0x7B, 0x587B40}, {0x87, 0x587910}, {0x98, 0x587A00}, {0xD5, 0x594E00}, {0xDE, 0x587AE0}, {0x192, 0x57C7A0}, {0x1C8, 0x57C7A0}};
constexpr sh::JumpTable kTables53F4D0[] = {{0x20, 0x1DC, 11}};
// 0x53F700 Scena02_Scene0C: 0xF4 bytes
constexpr sh::CallSite kCalls53F700[] = {{0x20, 0x531F90}, {0x27, 0x5734F0}, {0x46, 0x531F90}, {0x6B, 0x532ED0}, {0x72, 0x4410B0}, {0x8D, 0x531F90}, {0xAE, 0x57C0F0}, {0xBE, 0x57C7A0}};
constexpr sh::JumpTable kTables53F700[] = {{0x13, 0xE0, 5}};
// 0x53F800 Scena02_Scene0D: 0x1B8 bytes
constexpr sh::CallSite kCalls53F800[] = {{0x19, 0x531F90}, {0x20, 0x5734F0}, {0x49, 0x532ED0}, {0x59, 0x589810}, {0xC5, 0x4410B0}, {0xE4, 0x531F90}, {0xF4, 0x589810}, {0x162, 0x57C0F0}, {0x170, 0x57C0F0}, {0x178, 0x57C7A0}};
constexpr sh::JumpTable kTables53F800[] = {{0x13, 0x19C, 7}};
// 0x53F9C0 Scena02_Scene0E: 0x240 bytes
constexpr sh::CallSite kCalls53F9C0[] = {{0x22, 0x531F90}, {0x34, 0x531F90}, {0x53, 0x5734F0}, {0x75, 0x587740}, {0x9A, 0x57C0F0}, {0xAB, 0x532ED0}, {0xCA, 0x4410B0}, {0xF2, 0x531F90}, {0x104, 0x531F90}, {0x149, 0x594E00}, {0x16A, 0x594E00}, {0x17C, 0x5734F0}, {0x19E, 0x57C0F0}, {0x1CE, 0x594E00}, {0x1E8, 0x594E00}, {0x1F1, 0x57C7A0}};
constexpr sh::JumpTable kTables53F9C0[] = {{0x13, 0x214, 11}};
// 0x53FC00 Scena02_Scene0F: 0x22C bytes
constexpr sh::CallSite kCalls53FC00[] = {{0x32, 0x495040}, {0x74, 0x533E50}, {0x82, 0x587B40}, {0x8E, 0x587910}, {0x9F, 0x587A00}, {0xED, 0x594E00}, {0xF6, 0x587AE0}, {0x11C, 0x57C7A0}, {0x12F, 0x57C7C0}, {0x143, 0x57C0F0}, {0x14A, 0x531F90}, {0x187, 0x594E00}, {0x1AC, 0x587B40}, {0x1B4, 0x533E50}, {0x1DC, 0x57C7A0}};
constexpr sh::JumpTable kTables53FC00[] = {{0x14, 0x1F8, 13}};
// 0x53FE30 Scena02_Scene10: 0x2C3 bytes
constexpr sh::CallSite kCalls53FE30[] = {{0x42, 0x594E00}, {0x5F, 0x495040}, {0x88, 0x5734F0}, {0xC5, 0x594E00}, {0xE5, 0x57C0F0}, {0xED, 0x57C7A0}, {0x111, 0x531F90}, {0x13C, 0x5341A0}, {0x14F, 0x57C0F0}, {0x165, 0x594E00}, {0x187, 0x5734F0}, {0x1A8, 0x57C600}, {0x1FE, 0x57C600}, {0x243, 0x594E00}, {0x25E, 0x57C7A0}};
constexpr sh::JumpTable kTables53FE30[] = {{0x1C, 0x274, 14}};
// 0x540100 Scena02_Scene11: 0x408 bytes
constexpr sh::CallSite kCalls540100[] = {{0x1E, 0x587B40}, {0x27, 0x587AE0}, {0x2E, 0x4976D0}, {0x59, 0x5734F0}, {0x78, 0x531F90}, {0x98, 0x587B40}, {0xBF, 0x587910}, {0xC9, 0x587740}, {0xEA, 0x541AE0}, {0x12E, 0x587740}, {0x14A, 0x587740}, {0x151, 0x495040}, {0x19D, 0x495040}, {0x1B5, 0x532ED0}, {0x1D4, 0x4410B0}, {0x1F4, 0x587B40}, {0x1FB, 0x531F90}, {0x21B, 0x495040}, {0x24C, 0x4976D0}, {0x280, 0x57C0F0}, {0x296, 0x594E00}, {0x2A9, 0x5734F0}, {0x2C9, 0x495040}, {0x2FA, 0x4976D0}, {0x348, 0x594E00}, {0x35B, 0x533E50}, {0x37C, 0x57C7A0}};
constexpr sh::JumpTable kTables540100[] = {{0x18, 0x390, 30}};
// 0x540510 Scena02_Scene12: 0x168 bytes
constexpr sh::CallSite kCalls540510[] = {{0x40, 0x57C0F0}, {0x46, 0x531F90}, {0x71, 0x57C7A0}, {0x94, 0x587B40}, {0xB1, 0x594E00}, {0xCD, 0x57C0F0}, {0xDB, 0x57C0F0}, {0xE8, 0x57C0F0}, {0x113, 0x57C7A0}, {0x129, 0x587AE0}};
constexpr sh::JumpTable kTables540510[] = {{0x14, 0x148, 8}};
// 0x540680 Scena02_Scene13: 0x250 bytes
constexpr sh::CallSite kCalls540680[] = {{0x29, 0x57C0F0}, {0x3F, 0x594E00}, {0x4A, 0x495040}, {0x78, 0x5734F0}, {0x80, 0x589810}, {0xEA, 0x589810}, {0x15D, 0x531F90}, {0x17C, 0x587B40}, {0x185, 0x587AE0}, {0x1A9, 0x594E00}, {0x1CE, 0x532ED0}, {0x1D5, 0x4410B0}, {0x207, 0x57C0F0}, {0x21D, 0x594E00}};
constexpr sh::JumpTable kTables540680[] = {{0x16, 0x230, 8}};
// 0x5408D0 Scena02_Scene15: 0x158 bytes
constexpr sh::CallSite kCalls5408D0[] = {{0x23, 0x587AE0}, {0x2A, 0x5734F0}, {0x57, 0x533E50}, {0x6C, 0x57C0F0}, {0x8D, 0x57C7A0}, {0xAF, 0x495040}, {0xF3, 0x57C0F0}, {0x11E, 0x57C7A0}, {0x12F, 0x56D6F0}};
constexpr sh::JumpTable kTables5408D0[] = {{0x14, 0x138, 8}};
// 0x540A30 Scena02_Scene17: 0x40 bytes
constexpr sh::CallSite kCalls540A30[] = {{0xD, 0x591BE0}, {0x1A, 0x57C0F0}, {0x24, 0x587740}, {0x2C, 0x57C7A0}};
// 0x540A70 Scena02_Scene18: 0x2D1 bytes
constexpr sh::CallSite kCalls540A70[] = {{0x22, 0x4976D0}, {0x64, 0x531F90}, {0x77, 0x531F90}, {0x8A, 0x531F90}, {0xAD, 0x591BC0}, {0xD2, 0x57C7A0}, {0xEC, 0x57C0F0}, {0xF9, 0x57C0F0}, {0x103, 0x587740}, {0x10F, 0x4976D0}, {0x181, 0x4976D0}, {0x19E, 0x591BC0}, {0x1E2, 0x57C0F0}, {0x1EA, 0x57C7A0}, {0x21C, 0x591BC0}, {0x241, 0x57C7A0}, {0x25A, 0x57C0F0}, {0x268, 0x57C0F0}, {0x272, 0x587740}, {0x27E, 0x4976D0}};
constexpr sh::JumpTable kTables540A70[] = {{0x1C, 0x298, 9}};
// 0x540D50 Scena02_Scene19: 0x14E bytes
constexpr sh::CallSite kCalls540D50[] = {{0x30, 0x57C0F0}, {0x3D, 0x591B60}, {0x6F, 0x57C0F0}, {0x7D, 0x590BB0}, {0x85, 0x57C7A0}, {0xAC, 0x4976D0}, {0xD2, 0x541B90}, {0x10D, 0x57C7A0}};
constexpr sh::JumpTable kTables540D50[] = {{0x1C, 0x120, 7}};
// 0x540EA0 Scena02_Scene1A: 0x334 bytes
constexpr sh::CallSite kCalls540EA0[] = {{0x25, 0x587740}, {0x41, 0x5734F0}, {0x48, 0x531F90}, {0xA3, 0x531F90}, {0xB6, 0x531F90}, {0xC9, 0x531F90}, {0xEC, 0x541B90}, {0x121, 0x4976D0}, {0x146, 0x57C7A0}, {0x160, 0x541C50}, {0x167, 0x587B40}, {0x175, 0x531F90}, {0x17C, 0x5734F0}, {0x1A6, 0x532ED0}, {0x1AD, 0x4410B0}, {0x1F4, 0x531F90}, {0x207, 0x531F90}, {0x21A, 0x531F90}, {0x22D, 0x531F90}, {0x240, 0x531F90}, {0x263, 0x57C0F0}, {0x27F, 0x57C7A0}};
constexpr sh::JumpTable kTables540EA0[] = {{0x1C, 0x2B8, 15}, {0x1EE, 0x314, 8}};
// 0x5411E0 Scena02_Scene1B: 0x1EE bytes
constexpr sh::CallSite kCalls5411E0[] = {{0x26, 0x4976D0}, {0x84, 0x495040}, {0xA3, 0x533E50}, {0xAF, 0x587B80}, {0xB8, 0x587910}, {0xC9, 0x587A00}, {0xD6, 0x587B90}, {0x103, 0x594E00}, {0x195, 0x57C7A0}};
constexpr sh::JumpTable kTables5411E0[] = {{0x20, 0x1A8, 11}};
// 0x5413D0 Scena02_Scene08Next: 0x2E bytes; not in the tool's table: it dropped this start as a byte table's (docs/scena_sc2.md section 1.2)
constexpr sh::CallSite kCalls5413D0[] = {{0x9, 0x57C7C0}, {0xE, 0x57C7A0}};
// 0x541400 Scena02_Near01: 0x5C bytes
constexpr sh::CallSite kCalls541400[] = {{0x56, 0x541AB0}};
// 0x541460 Scena02_Near02: 0x5C bytes
constexpr sh::CallSite kCalls541460[] = {{0x56, 0x541AB0}};
// 0x5414C0 Scena02_Near03: 0x62 bytes
constexpr sh::CallSite kCalls5414C0[] = {{0x5C, 0x541AB0}};
// 0x541530 Scena02_Near04: 0x62 bytes
constexpr sh::CallSite kCalls541530[] = {{0x5C, 0x541AB0}};
// 0x5415A0 Scena02_Near05: 0x5C bytes
constexpr sh::CallSite kCalls5415A0[] = {{0x56, 0x541AB0}};
// 0x541600 Scena02_Near06: 0x5A bytes
constexpr sh::CallSite kCalls541600[] = {{0x54, 0x541AB0}};
// 0x541660 Scena02_Near07: 0x5C bytes
constexpr sh::CallSite kCalls541660[] = {{0x56, 0x541AB0}};
// 0x5416C0 Scena02_Near08: 0x5C bytes
constexpr sh::CallSite kCalls5416C0[] = {{0x56, 0x541AB0}};
// 0x541720 Scena02_Near09: 0x5C bytes
constexpr sh::CallSite kCalls541720[] = {{0x56, 0x541AB0}};
// 0x541780 Scena02_Near0A: 0x5C bytes
constexpr sh::CallSite kCalls541780[] = {{0x56, 0x541AB0}};
// 0x5417E0 Scena02_Near0B: 0x5C bytes
constexpr sh::CallSite kCalls5417E0[] = {{0x56, 0x541AB0}};
// 0x541840 Scena02_Near0C: 0x5C bytes
constexpr sh::CallSite kCalls541840[] = {{0x56, 0x541AB0}};
// 0x5418A0 Scena02_DoorStart: 0x11D bytes
constexpr sh::CallSite kCalls5418A0[] = {{0x42, 0x57C7C0}, {0x9D, 0x57C7C0}, {0xF0, 0x57C7C0}};
// 0x5419C0 Scena02_DoorSound: 0xE1 bytes
constexpr sh::CallSite kCalls5419C0[] = {{0x44, 0x587740}, {0x87, 0x587740}, {0xCA, 0x587740}};
// 0x541AB0 Scena02_StartRun07: 0x25 bytes
// 0x541AE0 Scena02_Shake: 0xAE bytes
// 0x541B90 Scena02_PlaceEffect70: 0xB3 bytes
constexpr sh::CallSite kCalls541B90[] = {{0xA, 0x589810}};
// 0x541C50 Scena02_PlaceEffect68: 0xB3 bytes
constexpr sh::CallSite kCalls541C50[] = {{0xA, 0x589810}};
// 0x541D10 Scena02_ObjectHook: 0x1D bytes; +0x12 note: call through .data 0x660ee0, 27 code entries (a data_tables entry); root: chapter 2 slot 1, the object trigger (0x56D6D0, the object)
// 0x541D30 Scena02_Object01: 0x20 bytes
constexpr sh::CallSite kCalls541D30[] = {{0x0, 0x57C7C0}};
// 0x541D50 Scena02_Object02: 0x20 bytes
constexpr sh::CallSite kCalls541D50[] = {{0x0, 0x57C7C0}};
// 0x541D70 Scena02_Object04: 0x3C bytes
constexpr sh::CallSite kCalls541D70[] = {{0x8, 0x57C140}, {0x1C, 0x57C7C0}};
// 0x541DB0 Scena02_Object05: 0x3C bytes
constexpr sh::CallSite kCalls541DB0[] = {{0x8, 0x57C140}, {0x1C, 0x57C7C0}};
// 0x541DF0 Scena02_Object07: 0x8 bytes
// 0x541E00 Scena02_Object06: 0x8 bytes
// 0x541E10 Scena02_Object09: 0x11 bytes
constexpr sh::CallSite kCalls541E10[] = {{0x8, 0x57C0F0}};
// 0x541E30 Scena02_Object0B: 0x25 bytes
constexpr sh::CallSite kCalls541E30[] = {{0x0, 0x57C7C0}};
// 0x541E60 Scena02_Object0C: 0x1E bytes
constexpr sh::CallSite kCalls541E60[] = {{0x0, 0x57C7C0}};
// 0x541E80 Scena02_Object0D: 0x20 bytes
constexpr sh::CallSite kCalls541E80[] = {{0x0, 0x57C7C0}};
// 0x541EA0 Scena02_Object0E: 0x27 bytes
constexpr sh::CallSite kCalls541EA0[] = {{0x0, 0x57C7C0}};
// 0x541ED0 Scena02_Object0F: 0x1E bytes
constexpr sh::CallSite kCalls541ED0[] = {{0x0, 0x57C7C0}};
// 0x541EF0 Scena02_Object10: 0x1F bytes
constexpr sh::CallSite kCalls541EF0[] = {{0x0, 0x57C7C0}, {0x18, 0x587740}};
// 0x541F10 Scena02_Object11: 0x28 bytes
constexpr sh::CallSite kCalls541F10[] = {{0x0, 0x57C7C0}};
// 0x541F40 Scena02_Object13: 0x25 bytes
constexpr sh::CallSite kCalls541F40[] = {{0x0, 0x57C7C0}};
// 0x541F70 Scena02_Object14: 0x14 bytes
constexpr sh::CallSite kCalls541F70[] = {{0x0, 0x57C7C0}};
// 0x541F90 Scena02_Object15: 0x14 bytes
constexpr sh::CallSite kCalls541F90[] = {{0x0, 0x57C7C0}};
// 0x541FB0 Scena02_Object16: 0x28 bytes
constexpr sh::CallSite kCalls541FB0[] = {{0x0, 0x57C7C0}};
// 0x541FE0 Scena02_Object17: 0x35 bytes
constexpr sh::CallSite kCalls541FE0[] = {{0x0, 0x57C7C0}, {0x2E, 0x587740}};
// 0x542020 Scena02_Object18: 0x11 bytes
constexpr sh::CallSite kCalls542020[] = {{0x8, 0x57C0F0}};
// 0x542040 Scena02_Object19: 0x14 bytes
constexpr sh::CallSite kCalls542040[] = {{0x0, 0x57C7C0}};
// 0x542060 Scena02_Object1A: 0x14 bytes
constexpr sh::CallSite kCalls542060[] = {{0x0, 0x57C7C0}};
// 0x542080 Scena02_StripMember: 0x3C bytes
constexpr sh::CallSite kCalls542080[] = {{0x2A, 0x590C90}};
// 0x5420C0 Scena02_StepHook: 0x7F3 bytes; root: chapter 2 slot 2, the step hook (x, z), al
constexpr sh::CallSite kCalls5420C0[] = {{0x21, 0x57C140}, {0x3A, 0x57C140}, {0x66, 0x57C7C0}, {0xAD, 0x594E00}, {0xCE, 0x57C140}, {0xF2, 0x57C7C0}, {0x121, 0x57C140}, {0x13C, 0x57C140}, {0x18D, 0x594E00}, {0x1A0, 0x57C140}, {0x1B4, 0x57C140}, {0x1FB, 0x594E00}, {0x21C, 0x57C140}, {0x231, 0x57C140}, {0x255, 0x57C7C0}, {0x275, 0x57C140}, {0x28A, 0x57C140}, {0x2B6, 0x57C7C0}, {0x2F6, 0x57C7C0}, {0x32F, 0x57C7C0}, {0x36D, 0x57C140}, {0x399, 0x57C7C0}, {0x3BA, 0x57C140}, {0x3E6, 0x57C7C0}, {0x41F, 0x57C7C0}, {0x441, 0x57C140}, {0x45A, 0x57C140}, {0x49E, 0x57C7C0}, {0x50A, 0x57C7C0}, {0x53B, 0x531F90}, {0x549, 0x57C7C0}, {0x5A5, 0x57C7C0}, {0x5CE, 0x57C140}, {0x60D, 0x57C140}, {0x639, 0x57C7C0}, {0x662, 0x57C140}, {0x68E, 0x57C7C0}, {0x6B6, 0x57C140}, {0x6F0, 0x57C7C0}, {0x704, 0x57C140}, {0x730, 0x57C7C0}, {0x759, 0x57C140}, {0x7A8, 0x57C140}, {0x7D4, 0x57C7C0}};
#define SC2_N(a) static_cast<int>(sizeof a / sizeof a[0])
const sh::Clone kClones[] = {
    {"Scena02_Frame", 0x53DDA0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Frame), 0, false, sh::Shape::kSlot},
    {"Scena02_Start", 0x53DDB0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Start), 0, false, sh::Shape::kState},
    {"Scena02_EnterArea", 0x53DDD0, 0x5FC, kCalls53DDD0, SC2_N(kCalls53DDD0), nullptr, 0, kTables53DDD0, SC2_N(kTables53DDD0), reinterpret_cast<const void*>(&::Scena02_EnterArea), 0, false, sh::Shape::kState},
    {"Scena02_Run", 0x53E3D0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Run), 0, false, sh::Shape::kState},
    {"Scena02_Scene00", 0x53E3E0, 0x187, kCalls53E3E0, SC2_N(kCalls53E3E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Scene00), 0, false, sh::Shape::kState},
    {"Scena02_Scene02", 0x53E570, 0x314, kCalls53E570, SC2_N(kCalls53E570), nullptr, 0, kTables53E570, SC2_N(kTables53E570), reinterpret_cast<const void*>(&::Scena02_Scene02), 0, false, sh::Shape::kState},
    {"Scena02_Scene04", 0x53E890, 0x16C, kCalls53E890, SC2_N(kCalls53E890), nullptr, 0, kTables53E890, SC2_N(kTables53E890), reinterpret_cast<const void*>(&::Scena02_Scene04), 0, false, sh::Shape::kState},
    {"Scena02_Scene05", 0x53EA00, 0x45, kCalls53EA00, SC2_N(kCalls53EA00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Scene05), 0, false, sh::Shape::kState},
    {"Scena02_Scene06", 0x53EA50, 0x194, kCalls53EA50, SC2_N(kCalls53EA50), nullptr, 0, kTables53EA50, SC2_N(kTables53EA50), reinterpret_cast<const void*>(&::Scena02_Scene06), 0, false, sh::Shape::kState},
    {"Scena02_Scene07", 0x53EBF0, 0x17C, kCalls53EBF0, SC2_N(kCalls53EBF0), nullptr, 0, kTables53EBF0, SC2_N(kTables53EBF0), reinterpret_cast<const void*>(&::Scena02_Scene07), 0, false, sh::Shape::kState},
    {"Scena02_Scene08", 0x53ED70, 0x3A4, kCalls53ED70, SC2_N(kCalls53ED70), nullptr, 0, kTables53ED70, SC2_N(kTables53ED70), reinterpret_cast<const void*>(&::Scena02_Scene08), 0, false, sh::Shape::kState},
    {"Scena02_Scene09", 0x53F120, 0x21C, kCalls53F120, SC2_N(kCalls53F120), nullptr, 0, kTables53F120, SC2_N(kTables53F120), reinterpret_cast<const void*>(&::Scena02_Scene09), 0, false, sh::Shape::kState},
    {"Scena02_Scene0A", 0x53F340, 0x18C, kCalls53F340, SC2_N(kCalls53F340), nullptr, 0, kTables53F340, SC2_N(kTables53F340), reinterpret_cast<const void*>(&::Scena02_Scene0A), 0, false, sh::Shape::kState},
    {"Scena02_Scene0B", 0x53F4D0, 0x221, kCalls53F4D0, SC2_N(kCalls53F4D0), nullptr, 0, kTables53F4D0, SC2_N(kTables53F4D0), reinterpret_cast<const void*>(&::Scena02_Scene0B), 0, false, sh::Shape::kState},
    {"Scena02_Scene0C", 0x53F700, 0xF4, kCalls53F700, SC2_N(kCalls53F700), nullptr, 0, kTables53F700, SC2_N(kTables53F700), reinterpret_cast<const void*>(&::Scena02_Scene0C), 0, false, sh::Shape::kState},
    {"Scena02_Scene0D", 0x53F800, 0x1B8, kCalls53F800, SC2_N(kCalls53F800), nullptr, 0, kTables53F800, SC2_N(kTables53F800), reinterpret_cast<const void*>(&::Scena02_Scene0D), 0, false, sh::Shape::kState},
    {"Scena02_Scene0E", 0x53F9C0, 0x240, kCalls53F9C0, SC2_N(kCalls53F9C0), nullptr, 0, kTables53F9C0, SC2_N(kTables53F9C0), reinterpret_cast<const void*>(&::Scena02_Scene0E), 0, false, sh::Shape::kState},
    {"Scena02_Scene0F", 0x53FC00, 0x22C, kCalls53FC00, SC2_N(kCalls53FC00), nullptr, 0, kTables53FC00, SC2_N(kTables53FC00), reinterpret_cast<const void*>(&::Scena02_Scene0F), 0, false, sh::Shape::kState},
    {"Scena02_Scene10", 0x53FE30, 0x2C3, kCalls53FE30, SC2_N(kCalls53FE30), nullptr, 0, kTables53FE30, SC2_N(kTables53FE30), reinterpret_cast<const void*>(&::Scena02_Scene10), 0, false, sh::Shape::kState},
    {"Scena02_Scene11", 0x540100, 0x408, kCalls540100, SC2_N(kCalls540100), nullptr, 0, kTables540100, SC2_N(kTables540100), reinterpret_cast<const void*>(&::Scena02_Scene11), 0, false, sh::Shape::kState},
    {"Scena02_Scene12", 0x540510, 0x168, kCalls540510, SC2_N(kCalls540510), nullptr, 0, kTables540510, SC2_N(kTables540510), reinterpret_cast<const void*>(&::Scena02_Scene12), 0, false, sh::Shape::kState},
    {"Scena02_Scene13", 0x540680, 0x250, kCalls540680, SC2_N(kCalls540680), nullptr, 0, kTables540680, SC2_N(kTables540680), reinterpret_cast<const void*>(&::Scena02_Scene13), 0, false, sh::Shape::kState},
    {"Scena02_Scene15", 0x5408D0, 0x158, kCalls5408D0, SC2_N(kCalls5408D0), nullptr, 0, kTables5408D0, SC2_N(kTables5408D0), reinterpret_cast<const void*>(&::Scena02_Scene15), 0, false, sh::Shape::kState},
    {"Scena02_Scene17", 0x540A30, 0x40, kCalls540A30, SC2_N(kCalls540A30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Scene17), 0, false, sh::Shape::kState},
    {"Scena02_Scene18", 0x540A70, 0x2D1, kCalls540A70, SC2_N(kCalls540A70), nullptr, 0, kTables540A70, SC2_N(kTables540A70), reinterpret_cast<const void*>(&::Scena02_Scene18), 0, false, sh::Shape::kState},
    {"Scena02_Scene19", 0x540D50, 0x14E, kCalls540D50, SC2_N(kCalls540D50), nullptr, 0, kTables540D50, SC2_N(kTables540D50), reinterpret_cast<const void*>(&::Scena02_Scene19), 0, false, sh::Shape::kState},
    {"Scena02_Scene1A", 0x540EA0, 0x334, kCalls540EA0, SC2_N(kCalls540EA0), nullptr, 0, kTables540EA0, SC2_N(kTables540EA0), reinterpret_cast<const void*>(&::Scena02_Scene1A), 0, false, sh::Shape::kState},
    {"Scena02_Scene1B", 0x5411E0, 0x1EE, kCalls5411E0, SC2_N(kCalls5411E0), nullptr, 0, kTables5411E0, SC2_N(kTables5411E0), reinterpret_cast<const void*>(&::Scena02_Scene1B), 0, false, sh::Shape::kState},
    {"Scena02_Scene08Next", 0x5413D0, 0x2E, kCalls5413D0, SC2_N(kCalls5413D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Scene08Next), 0, false, sh::Shape::kState},
    {"Scena02_Near01", 0x541400, 0x5C, kCalls541400, SC2_N(kCalls541400), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near01), 0, false, sh::Shape::kEntry},
    {"Scena02_Near02", 0x541460, 0x5C, kCalls541460, SC2_N(kCalls541460), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near02), 0, false, sh::Shape::kEntry},
    {"Scena02_Near03", 0x5414C0, 0x62, kCalls5414C0, SC2_N(kCalls5414C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near03), 0, false, sh::Shape::kEntry},
    {"Scena02_Near04", 0x541530, 0x62, kCalls541530, SC2_N(kCalls541530), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near04), 0, false, sh::Shape::kEntry},
    {"Scena02_Near05", 0x5415A0, 0x5C, kCalls5415A0, SC2_N(kCalls5415A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near05), 0, false, sh::Shape::kEntry},
    {"Scena02_Near06", 0x541600, 0x5A, kCalls541600, SC2_N(kCalls541600), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near06), 0, false, sh::Shape::kEntry},
    {"Scena02_Near07", 0x541660, 0x5C, kCalls541660, SC2_N(kCalls541660), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near07), 0, false, sh::Shape::kEntry},
    {"Scena02_Near08", 0x5416C0, 0x5C, kCalls5416C0, SC2_N(kCalls5416C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near08), 0, false, sh::Shape::kEntry},
    {"Scena02_Near09", 0x541720, 0x5C, kCalls541720, SC2_N(kCalls541720), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near09), 0, false, sh::Shape::kEntry},
    {"Scena02_Near0A", 0x541780, 0x5C, kCalls541780, SC2_N(kCalls541780), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near0A), 0, false, sh::Shape::kEntry},
    {"Scena02_Near0B", 0x5417E0, 0x5C, kCalls5417E0, SC2_N(kCalls5417E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near0B), 0, false, sh::Shape::kEntry},
    {"Scena02_Near0C", 0x541840, 0x5C, kCalls541840, SC2_N(kCalls541840), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Near0C), 0, false, sh::Shape::kEntry},
    {"Scena02_DoorStart", 0x5418A0, 0x11D, kCalls5418A0, SC2_N(kCalls5418A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_DoorStart), 0, false, sh::Shape::kState},
    {"Scena02_DoorSound", 0x5419C0, 0xE1, kCalls5419C0, SC2_N(kCalls5419C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_DoorSound), 0, false, sh::Shape::kState},
    {"Scena02_StartRun07", 0x541AB0, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_StartRun07), 0, false, sh::Shape::kState},
    {"Scena02_Shake", 0x541AE0, 0xAE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Shake), 0, false, sh::Shape::kEntry},
    {"Scena02_PlaceEffect70", 0x541B90, 0xB3, kCalls541B90, SC2_N(kCalls541B90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_PlaceEffect70), 0, false, sh::Shape::kEntry},
    {"Scena02_PlaceEffect68", 0x541C50, 0xB3, kCalls541C50, SC2_N(kCalls541C50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_PlaceEffect68), 0, false, sh::Shape::kEntry},
    {"Scena02_ObjectHook", 0x541D10, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_ObjectHook), 0, false, sh::Shape::kObject},
    {"Scena02_Object01", 0x541D30, 0x20, kCalls541D30, SC2_N(kCalls541D30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object01), 0, false, sh::Shape::kEntry},
    {"Scena02_Object02", 0x541D50, 0x20, kCalls541D50, SC2_N(kCalls541D50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object02), 0, false, sh::Shape::kEntry},
    {"Scena02_Object04", 0x541D70, 0x3C, kCalls541D70, SC2_N(kCalls541D70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object04), 0, false, sh::Shape::kEntry},
    {"Scena02_Object05", 0x541DB0, 0x3C, kCalls541DB0, SC2_N(kCalls541DB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object05), 0, false, sh::Shape::kEntry},
    {"Scena02_Object07", 0x541DF0, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object07), 0, false, sh::Shape::kEntry},
    {"Scena02_Object06", 0x541E00, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object06), 0, false, sh::Shape::kEntry},
    {"Scena02_Object09", 0x541E10, 0x11, kCalls541E10, SC2_N(kCalls541E10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object09), 0, false, sh::Shape::kEntry},
    {"Scena02_Object0B", 0x541E30, 0x25, kCalls541E30, SC2_N(kCalls541E30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object0B), 0, false, sh::Shape::kEntry},
    {"Scena02_Object0C", 0x541E60, 0x1E, kCalls541E60, SC2_N(kCalls541E60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object0C), 0, false, sh::Shape::kEntry},
    {"Scena02_Object0D", 0x541E80, 0x20, kCalls541E80, SC2_N(kCalls541E80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object0D), 0, false, sh::Shape::kEntry},
    {"Scena02_Object0E", 0x541EA0, 0x27, kCalls541EA0, SC2_N(kCalls541EA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object0E), 0, false, sh::Shape::kEntry},
    {"Scena02_Object0F", 0x541ED0, 0x1E, kCalls541ED0, SC2_N(kCalls541ED0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object0F), 0, false, sh::Shape::kEntry},
    {"Scena02_Object10", 0x541EF0, 0x1F, kCalls541EF0, SC2_N(kCalls541EF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object10), 0, false, sh::Shape::kEntry},
    {"Scena02_Object11", 0x541F10, 0x28, kCalls541F10, SC2_N(kCalls541F10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object11), 0, false, sh::Shape::kEntry},
    {"Scena02_Object13", 0x541F40, 0x25, kCalls541F40, SC2_N(kCalls541F40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object13), 0, false, sh::Shape::kEntry},
    {"Scena02_Object14", 0x541F70, 0x14, kCalls541F70, SC2_N(kCalls541F70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object14), 0, false, sh::Shape::kEntry},
    {"Scena02_Object15", 0x541F90, 0x14, kCalls541F90, SC2_N(kCalls541F90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object15), 0, false, sh::Shape::kEntry},
    {"Scena02_Object16", 0x541FB0, 0x28, kCalls541FB0, SC2_N(kCalls541FB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object16), 0, false, sh::Shape::kEntry},
    {"Scena02_Object17", 0x541FE0, 0x35, kCalls541FE0, SC2_N(kCalls541FE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object17), 0, false, sh::Shape::kEntry},
    {"Scena02_Object18", 0x542020, 0x11, kCalls542020, SC2_N(kCalls542020), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object18), 0, false, sh::Shape::kEntry},
    {"Scena02_Object19", 0x542040, 0x14, kCalls542040, SC2_N(kCalls542040), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object19), 0, false, sh::Shape::kEntry},
    {"Scena02_Object1A", 0x542060, 0x14, kCalls542060, SC2_N(kCalls542060), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_Object1A), 0, false, sh::Shape::kEntry},
    {"Scena02_StripMember", 0x542080, 0x3C, kCalls542080, SC2_N(kCalls542080), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_StripMember), 0, false, sh::Shape::kEntry},
    {"Scena02_StepHook", 0x5420C0, 0x7F3, kCalls5420C0, SC2_N(kCalls5420C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena02_StepHook), 0xFF, false, sh::Shape::kHook},
};
#undef SC2_N

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The object handlers take (object, 0x903FA0): a .data table's handler
// recorder logs no arguments, so each distinct entry gets a typed stand-in of
// the fuzz's own, keyed on the handler's address (the harness finds it before
// registering a handler recorder, and puts it in every entry holding that
// address: 0x541E00 is entries 6 and 8). Entries 0, 3 and 0x12 are 0x437CC0,
// the bare ret the runs table holds too: it reads nothing and keeps the
// handler recorder. Entry 0xA, 0x557170, lies in group SC9b's block: a
// stand-in like the rest.
constexpr std::uint32_t kObjectEntries[23] = {0x541D30, 0x541D50, 0x541D70, 0x541DB0, 0x541E00, 0x541DF0, 0x541E10, 0x557170,
                                              0x541E30, 0x541E60, 0x541E80, 0x541EA0, 0x541ED0, 0x541EF0, 0x541F10, 0x541F40,
                                              0x541F70, 0x541F90, 0x541FB0, 0x541FE0, 0x542020, 0x542040, 0x542060};
template <unsigned I> void __cdecl ObjectEntry(unsigned char* object, unsigned char* row) {
    sh::Record(kObjectEntries[I], Key(object), Key(row));
    sh::Stir();
}

void Disturb(std::uint32_t h);
const char* g_name = "";   // the clone being fuzzed, for Disturb
// The effect of the busiest callees: half the time, one of the chapter's own
// cells moved (the group's Disturb), which the harness's own disturbance
// reaches only one call in 24 - the cells stored around a call (the step, the
// counters, the run) and those read again after one.
std::uint32_t Move(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t h = sh::Noise();
    if (h & 1) Disturb(h);
    return answer;
}

// A callee ours reaches by its name (Capcom's address, or our function).
#define SC2_NAMED(name) #name, ::bof3::addr::name, KeyOf(&::name)
// One ours and the originals reach by its address alone.
#define SC2_RAW(address) #address, address, address
#define SC2_OBJECT(i) \
    {"Scena02_ObjectHandlers entry", kObjectEntries[i], kObjectEntries[i], 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr, \
     reinterpret_cast<const void*>(&ObjectEntry<i>)}
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define SC2_MOVED(name, n, ...) {SC2_NAMED(name), n, {__VA_ARGS__}, sh::Answer::kGarbage, 0, 0, {}, &Move}
const sh::Callee kCallees[] = {
    // standard callees with the standard masks, recorded with Move
    SC2_MOVED(Field_ChangeArea, 4, kU16, kAll, kAll, kU8),
    SC2_MOVED(Party_DropIn, 1, kU8),
    SC2_MOVED(Transition_Start, 1, kU8),
    SC2_MOVED(Sound_PlayEffect, 1, kU16),
    SC2_MOVED(Msg_OpenScript, 1, kU16),
    SC2_MOVED(Flags_Set, 2, kAll, kU8),
    SC2_MOVED(Flags_Clear, 2, kAll, kU8),
    SC2_MOVED(ScriptFlags_Set40, 0),
    SC2_MOVED(ScriptFlags_Clear40, 0),
    SC2_MOVED(Kind2_Place, 1, kU8),
    SC2_MOVED(Music_Play, 2, kAll, kAll),
    SC2_MOVED(Music_FadeOutStop, 1, kAll),
    // tested on al alone (test al, al): garbage above a 0 must not matter
    {SC2_NAMED(Flags_Test), 2, {kAll, kU8}, sh::Answer::kFlag, 0, 0, {}, &Move},
    // group SE's, ours: called by name (the standard set lists it by address)
    {SC2_NAMED(Field_StartEventBattle), 1, {kU8}, sh::Answer::kGarbage, 0, 0, {}, &Move},
    // engine callees nobody owns (group SX's this wave; 0x57C600 nobody's)
    {SC2_RAW(0x533E50), 0, {}, sh::Answer::kGarbage, 0, 0, {}, &Move},
    {SC2_RAW(0x587B80), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC2_RAW(0x590C90), 4, {kU8, kAll, kU8, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC2_RAW(0x591B60), 4, {kU8, kU8, kU8, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC2_RAW(0x591BC0), 2, {kAll, kAll}, sh::Answer::kFlag, 0, 0},
    {SC2_RAW(0x591BE0), 2, {kAll, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_RAW(0x57C600), 2, {kU16, kU8}, sh::Answer::kFlag, 0, 0},
    // the bank's own, called directly: by the argument, or (no argument) as a
    // phase that logs the chapter bytes it ran with
    {SC2_NAMED(Scena02_StripMember), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near01), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near02), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near03), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near04), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near05), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near06), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near07), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near08), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near09), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near0A), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near0B), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Near0C), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_Shake), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_PlaceEffect70), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_PlaceEffect68), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC2_NAMED(Scena02_DoorStart), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC2_NAMED(Scena02_DoorSound), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC2_NAMED(Scena02_StartRun07), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC2_NAMED(Scena02_Scene08Next), 0, {}, sh::Answer::kPhase, 0, 0},
    // the object handler table's typed stand-ins
    SC2_OBJECT(0), SC2_OBJECT(1), SC2_OBJECT(2), SC2_OBJECT(3), SC2_OBJECT(4), SC2_OBJECT(5), SC2_OBJECT(6), SC2_OBJECT(7),
    SC2_OBJECT(8), SC2_OBJECT(9), SC2_OBJECT(10), SC2_OBJECT(11), SC2_OBJECT(12), SC2_OBJECT(13), SC2_OBJECT(14),
    SC2_OBJECT(15), SC2_OBJECT(16), SC2_OBJECT(17), SC2_OBJECT(18), SC2_OBJECT(19), SC2_OBJECT(20), SC2_OBJECT(21),
    SC2_OBJECT(22),
};
#undef SC2_OBJECT
#undef SC2_MOVED
#undef SC2_RAW
#undef SC2_NAMED

// The chapter's tables, read in place.
const sh::DataTable kTables[] = {
    {at::kStates, at::kStateCount},     // Scena02_States
    {at::kRuns, at::kRunCount},         // Scena02_Runs
    {at::kObjects, at::kObjectCount},   // Scena02_ObjectHandlers
};

// What the bank reads and writes beyond the harness's 22 standard regions.
const sh::Region kRegions[] = {
    {0x903800, 4},                        // Camera_ShiftX, Camera_ShiftY (Scena02_Shake)
    {0x929F00, 0x14},                     // 0x929F00, 0x929F0C (the shop bytes), Field_Kind2Hold 0x929F12
    {0x66C7E8, 2},                        // Game_Mode
    {at::kChoiceBits, 4},                 // the message box's choice bits
    {at::kAreaByte, 1},                   // 0x904CD0
    {at::kExtraWord, 2},                  // the word just past Sprite_ObjectsExtra (area 0x10)
    {at::kMemberRecords, 8 * at::kMemberStride},   // the eight member records (Scena02_StripMember)
};

// --- the seed ------------------------------------------------------------------

unsigned char* M(std::uint32_t a) { return sh::Mem(a); }
void SetWordAt(std::uint32_t a, unsigned v) { move_script::SetWord(M(a), v); }
bool Is(unsigned k, const char* n);

// The steps each scene switches on (its cases, and one past the last), and
// the pairs (step, counter 0) its waits need.
struct Steps { const char* name; std::uint8_t steps[36]; unsigned n; std::uint8_t pairs[14][2]; unsigned n_pairs; };
const Steps kSteps[] = {
    {"Scena02_Scene02", {0, 1, 2, 3, 5, 6, 7, 8, 0x14, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24}, 16,
     {{2, 8}, {6, 0x22}, {8, 0x22}, {0x14, 0x1E}, {0x1E, 0x64}, {0x1E, 0x65}}, 6},
    {"Scena02_Scene04", {0, 1, 2, 5, 6, 7, 8, 9}, 8, {{1, 8}, {6, 0x22}, {8, 0x22}}, 3},
    {"Scena02_Scene05", {0, 1, 2}, 3, {}, 0},
    {"Scena02_Scene06", {0, 1, 2, 3, 4, 5, 6}, 7, {{1, 0xBE}, {1, 0xBF}}, 2},
    {"Scena02_Scene07", {0, 1, 2, 3, 4, 5}, 6, {{1, 0x21}}, 1},
    {"Scena02_Scene08", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC}, 13, {{0, 3}, {1, 8}, {2, 0xB}, {3, 0xC}, {4, 0x31}, {5, 1}}, 6},
    {"Scena02_Scene09", {0, 1, 2, 3, 4, 5, 6, 7, 8}, 9, {{2, 0xA2}, {4, 0x63}, {5, 0x71}, {6, 0x7E}}, 4},
    {"Scena02_Scene0A", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB}, 12,
     {{1, 1}, {3, 9}, {6, 1}, {7, 2}, {8, 0x10}, {9, 0x11}, {0xA, 0x14}}, 7},
    {"Scena02_Scene0B", {0, 1, 2, 3, 7, 8, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 4}, 13,
     {{0, 0x64}, {0, 0x65}, {1, 1}, {0x18, 1}, {2, 0xC8}, {2, 0xC9}, {7, 0xC8}, {7, 0xC9}}, 8},
    {"Scena02_Scene0C", {0, 1, 2, 3, 4, 5}, 6, {{1, 0x15}, {2, 0x1F}, {3, 0x20}, {4, 0x25}}, 4},
    {"Scena02_Scene0D", {0, 1, 2, 3, 4, 5, 6, 7}, 8, {{1, 0x2B}, {3, 0x2F}, {4, 0x30}, {6, 0x31}}, 4},
    {"Scena02_Scene0E", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB}, 12,
     {{1, 0x3E}, {2, 0x41}, {3, 0x42}, {4, 0x43}, {5, 0x44}, {6, 0x46}, {8, 0x4B}}, 7},
    {"Scena02_Scene0F", {0, 1, 2, 3, 4, 5, 6, 0xA, 0xB, 0xC, 0xD}, 11, {{0, 5}, {0, 6}, {5, 1}, {0xB, 1}, {0xC, 5}, {0xC, 6}}, 6},
    {"Scena02_Scene10", {0, 1, 2, 3, 6, 7, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17}, 15,
     {{1, 1}, {2, 6}, {6, 7}, {7, 9}, {0x10, 2}, {0x11, 0x12}, {0x12, 0x17}, {0x14, 0x18}, {0x15, 0x1A}, {0x16, 7}}, 10},
    {"Scena02_Scene11", {0, 1, 2, 3, 4, 5, 6, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C,
                         0x1D, 0x1E}, 23,
     {{2, 1}, {3, 6}, {4, 0xE}, {5, 0x14}, {0xF, 0x19}, {0x11, 0x1B}, {0x12, 0x1D}, {0x13, 0x23}, {0x14, 0x2B}, {0x19, 1},
      {0x1D, 6}}, 11},
    {"Scena02_Scene12", {0, 1, 2, 3, 5, 6, 7, 8}, 8, {{1, 2}, {2, 4}, {6, 0x11}}, 3},
    {"Scena02_Scene13", {9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12}, 10, {{0xD, 2}, {0xE, 3}, {0xF, 1}, {0x10, 0x27}, {0x11, 0x3C}}, 5},
    {"Scena02_Scene15", {0, 1, 2, 5, 6, 7, 8}, 7, {{1, 3}, {5, 1}, {7, 0xF}}, 3},
    {"Scena02_Scene17", {0, 1}, 2, {}, 0},
    {"Scena02_Scene18", {0, 1, 2, 3, 4, 5, 0xA, 0xC, 0xD, 0x14, 0x15}, 11,
     {{2, 8}, {2, 0xF}, {0xC, 8}, {0xC, 0xF}, {0x14, 8}, {0x14, 0xF}}, 6},
    {"Scena02_Scene19", {0, 1, 2, 0xA, 0xF, 0x10, 0x11, 0x12}, 8, {{1, 0x16}, {0x11, 0x1A}}, 2},
    {"Scena02_Scene1A", {0, 1, 2, 3, 5, 6, 7, 0xB, 0xF, 0x14, 0x15, 0x18, 0x19, 0x1A, 0x1E, 0x1F}, 16,
     {{2, 0x27}, {6, 0x39}, {7, 0x3C}, {0x15, 0x47}, {0x18, 0x4B}, {0x1A, 0x51}}, 6},
    {"Scena02_Scene1B", {0, 1, 2, 5, 0xA, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A}, 12,
     {{5, 0x5A}, {5, 0x5B}, {0xA, 0x5C}, {0xA, 0x5D}, {0x18, 0x5C}, {0x18, 0x5D}}, 6},
};
const Steps* g_steps[sizeof kClones / sizeof kClones[0]];

// Every value a step compares counter 0 with.
std::uint32_t CounterZero() {
    return SH_PICK(1, 2, 3, 4, 5, 6, 7, 8, 9, 0xB, 0xC, 0xE, 0xF, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A,
                   0x1B, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x25, 0x27, 0x2B, 0x2F, 0x30, 0x31, 0x39, 0x3C, 0x3E, 0x41,
                   0x42, 0x43, 0x44, 0x46, 0x47, 0x4B, 0x51, 0x5A, 0x5B, 0x5C, 0x5D, 0x63, 0x64, 0x65, 0x71, 0x7E, 0xA2,
                   0xBE, 0xBF, 0xC8, 0xC9);
}
std::uint32_t Area() {
    return SH_PICK(0, 3, 5, 0xB, 0xD, 0xF, 0x10, 0x12, 0x16, 0x17, 0x1A, 0x1B, 0x1C, 0x1D, 0x2D, 0x5A, 0x1A, 0x1B, 0x1A);
}

// The step hook's rectangles: an area, and the x and z values its bounds
// test (each bound, one past it, and the exact values).
struct Box { std::uint16_t area; std::int32_t xs[6]; unsigned nx; std::int32_t zs[6]; unsigned nz; };
const Box kBoxes[] = {
    {3, {0x557FFF, 0x558000, 0x568000, 0x568001}, 4, {0x1BFFFF, 0x1C0000, 0x1D8000, 0x1D8001}, 4},
    {0xF, {0x35FFFF, 0x360000, 0x378000, 0x378001}, 4, {0x510000, 0x510001, 0x7F0000}, 3},
    {0x1A, {0x33FFFF, 0x340000, 0x358000, 0x358001}, 4, {0x80000, 0x7FFFF}, 2},
    {0x1A, {0x4FFFF, 0x50000, 0x90000, 0x90001}, 4, {0xF0000, 0xF0001}, 2},
    {0x1A, {0x1CFFFF, 0x1D0000, 0x200000, 0x200001}, 4, {0x320000, 0x328000, 0x31FFFF}, 3},
    {0x1A, {0x350000, 0x358000, 0x34FFFF}, 3, {0x4BFFFF, 0x4C0000, 0x4C8000, 0x4D8000, 0x4D8001}, 5},
    {0x1A, {0x24FFFF, 0x250000, 0x278000, 0x278001, 0x350000}, 5, {0x4C0000, 0x4C8000, 0x4C0001}, 3},
    {0x1A, {0x2FFFFF, 0x300000, 0x320000, 0x320001}, 4, {0x2A0000, 0x298000, 0x2A0001}, 3},
    {0x1A, {0x30FFFF, 0x310000, 0x320000, 0x320001}, 4, {0x250000, 0x248000, 0x250001}, 3},
    {0x1A, {0x3B0000, 0x3B8000, 0x3AFFFF}, 3, {0x25FFFF, 0x260000, 0x298000, 0x298001}, 4},
    {0x1A, {0x298000, 0x2A8000, 0x2A0000}, 3, {0x4FFFFF, 0x500000, 0x540000, 0x540001}, 4},
    {0x1B, {0x278000, 0x288000, 0x280000}, 3, {0x24FFFF, 0x250000, 0x278000, 0x278001}, 4},
    {0x1B, {0x1AFFFF, 0x1B0000, 0x1C8000, 0x1C8001}, 4, {0x1C8000, 0x1C8001}, 2},
    {0x1B, {0x177FFF, 0x178000, 0x188000, 0x188001}, 4, {0x24FFFF, 0x250000, 0x278000, 0x278001}, 4},
    {0x1B, {0x8FFFF, 0x90000, 0xB8000, 0xB8001}, 4, {0x3FFFFF, 0x400000, 0x418000, 0x418001}, 4},
    {0x1B, {0xD8000, 0xD8001}, 2, {0x2FFFF, 0x30000, 0x4FFFF, 0x50000, 0x3A123}, 5},
    {0x1B, {0x38FFFF, 0x390000, 0x3B8000, 0x3B8001}, 4, {0x47FFFF, 0x480000, 0x490000, 0x490001}, 4},
    {0x1B, {0x208000, 0x208001}, 2, {0x33FFFF, 0x340000, 0x358000, 0x358001}, 4},
    {0x1C, {0x230000, 0x238000, 0x234000}, 3, {0x5EFFFF, 0x5F0000, 0x600000, 0x600001}, 4},
};
const Box* g_box = nullptr;
unsigned g_near = 0;    // the sprite a near helper is called on

// A near helper's rectangle, relative to its sprite (x lo, x hi, z lo, z hi).
struct Rect { const char* name; std::int32_t x0, x1, z0, z1; };
const Rect kNear[] = {
    {"Scena02_Near01", 0, 0x50000, -0x18000, 0x18000},   {"Scena02_Near02", -0x40000, 0, -0x18000, 0x18000},
    {"Scena02_Near03", -0x18000, 0x18000, 0x10000, 0x40000}, {"Scena02_Near04", -0x18000, 0x18000, -0x40000, 0x10000},
    {"Scena02_Near05", 0, 0x60000, -0x20000, 0x20000},   {"Scena02_Near06", -0x10000, 0x10000, -0x50000, 0},
    {"Scena02_Near07", -0x48000, 0, -0x18000, 0x18000},  {"Scena02_Near08", 0, 0x40000, -0x10000, 0x10000},
    {"Scena02_Near09", 0, 0x40000, -0x20000, 0x28000},   {"Scena02_Near0A", 0, 0x30000, -0x10000, 0x18000},
    {"Scena02_Near0B", -0x10000, 0x10000, 0, 0x50000},   {"Scena02_Near0C", -0x18000, 0x18000, 0, 0x40000},
};
// A bound of [lo, hi] or one past it.
std::int32_t Edge(std::int32_t lo, std::int32_t hi) {
    switch (sh::Next() % 6) {
    case 0: return lo;
    case 1: return lo - 1;
    case 2: return hi;
    case 3: return hi + 1;
    default: return lo + static_cast<std::int32_t>(sh::Next() % static_cast<std::uint32_t>(hi - lo + 1));
    }
}

void Seed(unsigned k) {
    g_name = kClones[k].name;
    g_box = nullptr;
    // Every round: the state and run in their tables, the object hook's index
    // in each object the kObject shape may pass, the area, the chapter's bytes
    // the steps and hooks test.
    M(at::kState)[0] = static_cast<unsigned char>(sh::Next() % at::kStateCount);
    for (unsigned i = 0; i < 4; ++i) sh::SpriteRecord(i)[0x86] = static_cast<unsigned char>(sh::Next() % at::kObjectCount);
    if (sh::Often()) SetWordAt(0x904EFC, Area());
    if (sh::Half()) SetWordAt(0x904EFC, sh::Next() & 0x7F);
    if (sh::Often()) M(at::kCounters)[0] = static_cast<unsigned char>(CounterZero());
    if (sh::Often()) M(at::kCounters + 1)[0] = static_cast<unsigned char>(SH_PICK(0, 1, 2));
    if (sh::Often()) M(at::kCounters + 2)[0] = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3, 4, 5, 6, 7));
    if (sh::Often()) M(at::kCounters + 3)[0] = static_cast<unsigned char>(SH_PICK(0, 1, 2, 5, 6, 0xA, 0xB, 0xE, 0xF, 0x4A, 0x4B));
    if (sh::Often()) M(0x66C7D8)[0] = static_cast<unsigned char>(SH_PICK(0, 2, 1));
    if (sh::Often()) SetWordAt(0x66C810, 0);
    if (sh::Often()) M(0x929F12)[0] = 0;
    if (sh::Often()) M(0x929EC0)[0] = static_cast<unsigned char>(SH_PICK(2, 3, 1));
    if (sh::Often()) M(at::kLeadMember)[0] = static_cast<unsigned char>(SH_PICK(0, 3, 4, 1));
    if (sh::Often()) M(0x8034F1)[0] = static_cast<unsigned char>(SH_PICK(0, 1));   // Cond_ByteFD
    if (sh::Half()) M(at::kChoiceBits)[0] ^= 2;
    if (sh::Half()) M(0x8034E1)[0] ^= 1;                                            // Field_StatusBits bit 0
    if (sh::Half()) SetWordAt(at::kExtraWord, 0x5A);
    if (sh::Half()) M(at::kMemberSeven)[0] = 7;
    // The leader's cell (Scena02_Scene08 step 8, the door helpers, Scena02_Scene1A step 0x19).
    unsigned char* const lead = sh::ObjectOf(0);
    if (sh::Often()) move_script::SetWord(lead + 0x36, SH_PICK(4, 5, 8, 9, 0xC, 0xD, 0x44, 0x45, 0x46, 3, 0x47));
    if (sh::Often()) move_script::SetWord(lead + 0x3A, SH_PICK(3, 4, 5, 8, 9, 0x3E, 0x3F, 0xFFFF, 0x8000));
    if (sh::Often()) lead[0x4B] = static_cast<unsigned char>(SH_PICK(0x42, 0x43, 0x45, 0x41));
    if (sh::Often()) lead[8] = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3, 4, 7, 8, 6));
    // The scene's own.
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
    if (Is(k, "Scena02_Scene08") && sh::Half()) {
        static const unsigned char kWait3[][2] = {{9, 2}, {0xA, 6}, {0xB, 0xB}, {8, 3}};
        const unsigned i = sh::Next() % 4;
        M(at::kStep)[0] = kWait3[i][0];
        if (i == 3) M(at::kCounters + 2)[0] = static_cast<unsigned char>(SH_PICK(3, 2));
        else M(at::kCounters + 3)[0] = kWait3[i][1];
        if (i == 3 && sh::Half()) M(at::kLeadMember)[0] = 0;
    }
    if (Is(k, "Scena02_Scene09") && sh::Half()) {   // step 3: the second effect's record, its first byte 0 or not
        M(at::kStep)[0] = 3;
        const unsigned slot = sh::Next() % 20;
        M(at::kCounters + 3)[0] = static_cast<unsigned char>(slot);
        M(0x7E11E0 + slot * 0x80)[0] = static_cast<unsigned char>(sh::Half() ? 0 : 1);
    }
    if (Is(k, "Scena02_Scene10") && sh::Half()) {
        M(at::kStep)[0] = 0x13;
        M(at::kCounters + 3)[0] = static_cast<unsigned char>(SH_PICK(5, 4, 6, 0xFF));
    }
    if (Is(k, "Scena02_Scene1A") && sh::Half()) {
        M(at::kStep)[0] = static_cast<unsigned char>(SH_PICK(1, 0x1E));
        M(at::kCounters + 3)[0] = static_cast<unsigned char>(SH_PICK(0x4A, 0x4B, 0xE, 0xF, 0x10, 0xFF));
    }
    if (Is(k, "Scena02_Scene08Next")) M(at::kCounters + 2)[0] = static_cast<unsigned char>(SH_PICK(3, 2, 0xFF, 0));
    if (Is(k, "Scena02_Shake")) {
        M(at::kCounters + 1)[0] = static_cast<unsigned char>(SH_PICK(0, 1, 2));
        M(at::kCounters + 3)[0] = static_cast<unsigned char>(SH_PICK(0, 1, 2, 0xFF));
    }
    if (Is(k, "Scena02_Run")) MoveScript_Var7 = static_cast<signed char>(sh::Next() % at::kRunCount);
    if (Is(k, "Scena02_EnterArea")) {
        SetWordAt(0x904EFC, Area());
        M(at::kCounters + 2)[0] = static_cast<unsigned char>(sh::Next() % 8);
    }
    if (Is(k, "Scena02_Scene00") && sh::Often()) SetWordAt(0x904EFC, 0x1A);
    if (Is(k, "Scena02_StepHook") && sh::Often()) {
        g_box = &kBoxes[sh::Next() % (sizeof kBoxes / sizeof kBoxes[0])];
        SetWordAt(0x904EFC, g_box->area);
        if (sh::Half()) M(at::kCounters + 1)[0] = static_cast<unsigned char>(SH_PICK(0, 1));
    }
    // A near helper: its sprite (0..9, as Scena02_Scene00 passes) and the
    // leader at one of its rectangle's bounds.
    g_near = sh::Next() % 10;
    for (const Rect& r : kNear) {
        if (!Is(k, r.name) || !sh::Often()) continue;
        const unsigned char* const s = sh::Mem(0x7DEE80 + 0xA4 * g_near);
        const std::int32_t sx = move_script::Long(s + 0x34), sz = move_script::Long(s + 0x38);
        move_script::SetLong(lead + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(sx) + static_cast<std::uint32_t>(Edge(r.x0, r.x1))));
        move_script::SetLong(lead + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(sz) + static_cast<std::uint32_t>(Edge(r.z0, r.z1))));
        if (sh::Often()) M(0x8034F1)[0] = 0;
    }
}

// The arguments: the object and 0x903FA0 for the object handlers; the sprite
// for a near helper; a member 0..7 for Scena02_StripMember (its index is
// unchecked: section 6); a rectangle's bounds for the step hook.
void Args(unsigned k, std::uint32_t* a) {
    const char* const n = kClones[k].name;
    if (std::strncmp(n, "Scena02_Object", 14) == 0 && std::strcmp(n, "Scena02_ObjectHook") != 0) {
        a[0] = Key(sh::SpriteRecord(a[0]));
        a[1] = at::kRow2;
    } else if (std::strncmp(n, "Scena02_Near", 12) == 0) {
        a[0] = (a[1] & 3) ? g_near : a[0] % 30;
    } else if (std::strcmp(n, "Scena02_StripMember") == 0) {
        a[0] = a[0] % 8;
    } else if (std::strcmp(n, "Scena02_PlaceEffect70") == 0 || std::strcmp(n, "Scena02_PlaceEffect68") == 0) {
        if (a[1] & 1) a[0] = SH_PICK(0x91, 0x92, 0x99);
    } else if (g_box) {
        a[0] = static_cast<std::uint32_t>(g_box->xs[a[2] % g_box->nx]);
        a[1] = static_cast<std::uint32_t>(g_box->zs[a[3] % g_box->nz]);
    }
}

// A cell of the chapter's moved after a call (from the harness's disturbance
// one call in 24, from Move half the time): what the steps store around a
// call and read again after one.
void Disturb(std::uint32_t h) {
    const unsigned b = (h >> 13) & 0xFF;
    switch ((h >> 3) % 10) {
    case 0: M(at::kCounters)[0] = static_cast<unsigned char>(b & 1 ? b : CounterZero()); break;
    case 1: M(at::kCounters + ((h >> 11) & 3))[0] = static_cast<unsigned char>(b); break;
    case 2: M(0x929F12)[0] = static_cast<unsigned char>((h >> 11) & 1); break;
    case 3: M(at::kCounters + 3)[0] = static_cast<unsigned char>(b & 1 ? b : SH_PICK(2, 6, 0xB, 0x4B, 0xF)); break;
    case 4: M(0x929EC0)[0] = static_cast<unsigned char>(2 + ((h >> 11) & 1)); break;
    case 5: M(at::kChoiceBits)[0] ^= 2; break;
    case 6: M(at::kCounters + 2)[0] = static_cast<unsigned char>(b & 1 ? b : SH_PICK(3, 0, 1)); break;
    case 7: M(at::kLeadMember)[0] = static_cast<unsigned char>(b & 1 ? b : 0); break;
    case 8: move_script::SetWord(sh::ObjectOf(0) + 0x36, b & 1 ? 0x44 + (b >> 1) % 3 : b); break;
    default: M(at::kStep)[0] = static_cast<unsigned char>(b); break;
    }
}

bool Is(unsigned k, const char* n) { return std::strcmp(kClones[k].name, n) == 0; }

}  // namespace

void SelfTest() {
    for (unsigned k = 0; k < sizeof kClones / sizeof kClones[0]; ++k) {
        g_steps[k] = nullptr;
        for (const Steps& s : kSteps)
            if (std::strcmp(s.name, kClones[k].name) == 0) g_steps[k] = &s;
    }
    sh::Group g{"scena_sc2", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
                kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                &Seed, &Disturb, 6000};   // 6,000 rounds a function: the steps are many and each wait is narrow
    g.args = &Args;
    g.chapter = 2;
    sh::Run(g);
}

}  // namespace scena_sc2
