// BOF3X_SHADOW=scena_sc5: chapter 5's bank through the scenario harness
// (scenario_harness.h), once at start-up. docs/scena_sc5.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC5 --clones, checked
// against a capstone reading of every function), each clone's call shape in
// its comment; the callees the standard set lacks or records otherwise; the
// state and run tables swapped for recorders, and typed stand-ins written
// into the object and cell-hook tables; the regions beyond the standard ones;
// a seed per role; a disturbance of the chapter's cells.
//
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/scena_sc5.h"
#include "game/scena_sc5_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc5 {
namespace {

namespace sh = scenario_harness;

// tools/scenario_rows.py --unit SC5 --clones (at 3e410e7), 2026-09-27: every
// jump internal, nothing REFUSED. Each function's call shape after its name.
// 0x546390 Scena05_Frame: 0xE bytes; +0x7 note: jmp through .data 0x661034, 3 code entries (a data_tables entry); root: chapter 5 slot 0, the frame (Field_ModeDispatch); shape: vtable slot 0 (the frame): no arguments
// 0x5463A0 Scena05_EnterArea: 0x708 bytes; shape: state handler (Scena05_States entry 1): no arguments
constexpr sh::CallSite kCalls5463A0[] = {{0x12, 0x57C140}, {0x27, 0x57C140}, {0x3C, 0x57C0F0}, {0x44, 0x56D6F0}, {0x62, 0x57C140}, {0x7B, 0x57C140}, {0xC1, 0x5341C0}, {0xC8, 0x5341A0}, {0xCF, 0x531F90}, {0xDB, 0x5341C0}, {0xE2, 0x5341A0}, {0xE9, 0x531F90}, {0xF5, 0x531F90}, {0x114, 0x57C140}, {0x13C, 0x57C140}, {0x151, 0x5341C0}, {0x180, 0x57C110}, {0x1A4, 0x57C140}, {0x1C1, 0x57C140}, {0x212, 0x57C140}, {0x24B, 0x57C140}, {0x259, 0x587A20}, {0x261, 0x454810}, {0x26C, 0x5A9949}, {0x274, 0x454810}, {0x2D0, 0x57C140}, {0x2E2, 0x587A20}, {0x2EA, 0x454810}, {0x2F5, 0x5A9949}, {0x2FD, 0x454810}, {0x30A, 0x587AE0}, {0x35F, 0x57C140}, {0x39A, 0x587740}, {0x3B0, 0x57C140}, {0x3C2, 0x587A20}, {0x3CA, 0x454810}, {0x3D5, 0x5A9949}, {0x3DD, 0x454810}, {0x3EA, 0x587AE0}, {0x416, 0x5341A0}, {0x42A, 0x57C140}, {0x43C, 0x587A20}, {0x444, 0x454810}, {0x44F, 0x5A9949}, {0x457, 0x454810}, {0x464, 0x587AE0}, {0x490, 0x5341A0}, {0x4A3, 0x57C140}, {0x4B5, 0x5341C0}, {0x4E5, 0x587740}, {0x4EC, 0x5341A0}, {0x515, 0x5341C0}, {0x545, 0x587740}, {0x54C, 0x5341A0}, {0x572, 0x5341C0}, {0x5A2, 0x587740}, {0x5A9, 0x5341A0}, {0x5CB, 0x531F90}, {0x5F2, 0x57C140}, {0x637, 0x57C140}, {0x645, 0x587A20}, {0x64D, 0x454810}, {0x658, 0x5A9949}, {0x660, 0x454810}, {0x6A4, 0x532ED0}};
constexpr sh::JumpTable kTables5463A0[] = {{0x130, 0x6D8, 5}, {0x2C3, 0x6EC, 7}};
// 0x546AB0 Scena05_Run: 0xE bytes; +0x7 note: jmp through .data 0x661040, 23 code entries (a data_tables entry); shape: state handler (Scena05_States entry 2): no arguments
// 0x546AC0 Scena05_Run1: 0xB8 bytes; shape: run handler (Scena05_Runs entry 1): no arguments
constexpr sh::CallSite kCalls546AC0[] = {{0x31, 0x57C0F0}, {0x39, 0x57C7A0}, {0x7E, 0x57C0F0}, {0x94, 0x594E00}, {0xA0, 0x4976D0}};
// 0x546B80 Scena05_Run4: 0x1DA bytes; shape: run handler (Scena05_Runs entry 4): no arguments
constexpr sh::CallSite kCalls546B80[] = {{0x3E, 0x594E00}, {0x5D, 0x5734F0}, {0x86, 0x532ED0}, {0xA5, 0x4410B0}, {0xC4, 0x531F90}, {0xF8, 0x57C0F0}, {0x10E, 0x594E00}, {0x136, 0x57C0F0}, {0x13E, 0x57C7A0}, {0x15E, 0x4976D0}, {0x17E, 0x57C7A0}};
constexpr sh::JumpTable kTables546B80[] = {{0x1B, 0x19C, 10}};
// 0x546D60 Scena05_Run5: 0x2D7 bytes; shape: run handler (Scena05_Runs entry 5): no arguments
constexpr sh::CallSite kCalls546D60[] = {{0x2A, 0x4976D0}, {0x66, 0x531F90}, {0xA2, 0x594E00}, {0xB2, 0x57C7A0}, {0xD2, 0x4976D0}, {0xFF, 0x5734F0}, {0x111, 0x531F90}, {0x13E, 0x587B40}, {0x158, 0x587AE0}, {0x177, 0x587B40}, {0x17E, 0x495040}, {0x1A6, 0x587AE0}, {0x1AD, 0x4976D0}, {0x1DA, 0x57C0F0}, {0x1E1, 0x5341A0}, {0x220, 0x594E00}, {0x238, 0x57C0F0}, {0x248, 0x57C7A0}};
constexpr sh::JumpTable kTables546D60[] = {{0x1C, 0x270, 17}};
// 0x547040 Scena05_Run6: 0x4A bytes; shape: run handler (Scena05_Runs entry 6): no arguments
constexpr sh::CallSite kCalls547040[] = {{0x1D, 0x57C0F0}, {0x25, 0x57C7A0}};
// 0x547090 Scena05_Run7: 0x45 bytes; shape: run handler (Scena05_Runs entry 7): no arguments
constexpr sh::CallSite kCalls547090[] = {{0x18, 0x57C7A0}, {0x2E, 0x4976D0}};
// 0x5470E0 Scena05_Run8: 0x158 bytes; shape: run handler (Scena05_Runs entry 8): no arguments
constexpr sh::CallSite kCalls5470E0[] = {{0x26, 0x57C0F0}, {0x34, 0x495040}, {0x52, 0x533E50}, {0x60, 0x587B40}, {0x6B, 0x587910}, {0x7B, 0x587A00}, {0xA8, 0x594E00}, {0xB1, 0x587AE0}, {0xE2, 0x57C0F0}, {0xF0, 0x57C0F0}, {0xFD, 0x57C0F0}, {0x10B, 0x57C110}, {0x129, 0x594E00}};
constexpr sh::JumpTable kTables5470E0[] = {{0x13, 0x13C, 7}};
// 0x547240 Scena05_Run9: 0x1F0 bytes; shape: run handler (Scena05_Runs entry 9): no arguments
constexpr sh::CallSite kCalls547240[] = {{0x21, 0x587AE0}, {0x5D, 0x57C0F0}, {0x73, 0x594E00}, {0x85, 0x5734F0}, {0xB9, 0x57C0F0}, {0xD6, 0x594E00}, {0xEE, 0x5734F0}, {0x11C, 0x57C0F0}, {0x15C, 0x57C0F0}, {0x163, 0x5341C0}, {0x176, 0x594E00}, {0x196, 0x57C0F0}, {0x1AC, 0x594E00}};
constexpr sh::JumpTable kTables547240[] = {{0x19, 0x1BC, 13}};
// 0x547430 Scena05_Run10: 0x126 bytes; shape: run handler (Scena05_Runs entry 10): no arguments
constexpr sh::CallSite kCalls547430[] = {{0x33, 0x57C0F0}, {0x41, 0x57C0F0}, {0x4F, 0x57C0F0}, {0x69, 0x57C7A0}, {0x99, 0x57C0F0}, {0xA7, 0x57C140}, {0xC7, 0x594E00}, {0xDF, 0x57C140}, {0x103, 0x594E00}};
// 0x547560 Scena05_Run11: 0x45 bytes; shape: run handler (Scena05_Runs entry 11): no arguments
constexpr sh::CallSite kCalls547560[] = {{0x18, 0x57C7A0}, {0x2E, 0x4976D0}};
// 0x5475B0 Scena05_Run13: 0x26F bytes; shape: run handler (Scena05_Runs entry 13): no arguments
constexpr sh::CallSite kCalls5475B0[] = {{0x67, 0x4976D0}, {0x133, 0x57C0F0}, {0x160, 0x57C7A0}, {0x184, 0x4976D0}, {0x204, 0x57C0F0}, {0x212, 0x57C110}, {0x225, 0x594E00}};
constexpr sh::JumpTable kTables5475B0[] = {{0x1C, 0x230, 11}};
// 0x547820 Scena05_Run14: 0xF4 bytes; shape: run handler (Scena05_Runs entry 14): no arguments
constexpr sh::CallSite kCalls547820[] = {{0x3A, 0x4976D0}, {0x7D, 0x4976D0}, {0xB1, 0x57C0F0}, {0xB9, 0x57C7A0}};
constexpr sh::JumpTable kTables547820[] = {{0x13, 0xE0, 5}};
// 0x547920 Scena05_Run16: 0x1BD0 bytes; shape: run handler (Scena05_Runs entry 16): no arguments
constexpr sh::CallSite kCalls547920[] = {{0x44, 0x4976D0}, {0x5C, 0x4976D0}, {0xA7, 0x57C7A0}, {0x253, 0x4976D0}, {0x2A5, 0x57C0F0}, {0x2AC, 0x587B40}, {0x2BB, 0x54A1A0}, {0x2FC, 0x594E00}, {0x317, 0x594E00}, {0x332, 0x594E00}, {0x371, 0x594E00}, {0x38C, 0x594E00}, {0x3A7, 0x594E00}, {0x3B6, 0x587740}, {0x3EF, 0x4976D0}, {0x481, 0x594E00}, {0x4C1, 0x5341C0}, {0x4C8, 0x5341C0}, {0x4DE, 0x594E00}, {0x4EA, 0x5341C0}, {0x4F1, 0x5341C0}, {0x507, 0x594E00}, {0x513, 0x5341C0}, {0x51A, 0x5341C0}, {0x530, 0x594E00}, {0x53C, 0x5341C0}, {0x543, 0x5341C0}, {0x559, 0x594E00}, {0x569, 0x5341C0}, {0x570, 0x5341C0}, {0x5A5, 0x4976D0}, {0x5F8, 0x587740}, {0x60C, 0x532ED0}, {0x613, 0x4410B0}, {0x626, 0x531F90}, {0x62D, 0x5734F0}, {0x640, 0x531F90}, {0x65C, 0x57C0F0}, {0x672, 0x594E00}, {0x67E, 0x495040}, {0x6BC, 0x57C0F0}, {0x6E5, 0x5341A0}, {0x6EC, 0x5341A0}, {0x702, 0x594E00}, {0x70E, 0x5341A0}, {0x715, 0x5341A0}, {0x72B, 0x594E00}, {0x737, 0x5341A0}, {0x73E, 0x5341A0}, {0x754, 0x594E00}, {0x760, 0x5341A0}, {0x767, 0x5341A0}, {0x77D, 0x594E00}, {0x789, 0x5341A0}, {0x790, 0x5341A0}, {0x7A6, 0x594E00}, {0x7B2, 0x5341A0}, {0x7B9, 0x5341A0}, {0x7CF, 0x594E00}, {0x7DB, 0x5341A0}, {0x7E2, 0x5341A0}, {0x7F8, 0x594E00}, {0x804, 0x5341A0}, {0x80B, 0x5341A0}, {0x821, 0x594E00}, {0x82D, 0x5341A0}, {0x834, 0x5341A0}, {0x84A, 0x594E00}, {0x856, 0x5341A0}, {0x85D, 0x5341A0}, {0x873, 0x594E00}, {0x87F, 0x5341A0}, {0x886, 0x5341A0}, {0x89C, 0x594E00}, {0x8A8, 0x5341A0}, {0x8AF, 0x5341A0}, {0x8C5, 0x594E00}, {0x94E, 0x4976D0}, {0x9AB, 0x532ED0}, {0x9B2, 0x4410B0}, {0x9C5, 0x531F90}, {0x9E1, 0x57C0F0}, {0x9F7, 0x594E00}, {0xA3D, 0x594E00}, {0xA58, 0x594E00}, {0xA73, 0x594E00}, {0xAA3, 0x4976D0}, {0xB00, 0x532ED0}, {0xB07, 0x4410B0}, {0xB21, 0x57C0F0}, {0xB2E, 0x57C0F0}, {0xB68, 0x4976D0}, {0xC0A, 0x495040}, {0xC11, 0x5341C0}, {0xC26, 0x495040}, {0xC32, 0x495040}, {0xC4C, 0x495040}, {0xC61, 0x495040}, {0xC76, 0x495040}, {0xC7D, 0x5341C0}, {0xC84, 0x5341C0}, {0xC95, 0x495040}, {0xCA3, 0x5341C0}, {0xCAA, 0x5341C0}, {0xCC2, 0x495040}, {0xCC9, 0x5341C0}, {0xCD0, 0x5341C0}, {0xCE1, 0x495040}, {0xCEF, 0x5341C0}, {0xCF6, 0x5341C0}, {0xD0E, 0x495040}, {0xD15, 0x5341C0}, {0xD1C, 0x5341C0}, {0xD58, 0x57C140}, {0xD6D, 0x57C140}, {0xDE0, 0x4976D0}, {0xE2A, 0x57C140}, {0xE3F, 0x57C140}, {0xE5A, 0x532ED0}, {0xE61, 0x4410B0}, {0xE82, 0x532ED0}, {0xE89, 0x4410B0}, {0xEA9, 0x532ED0}, {0xEB0, 0x4410B0}, {0xEC3, 0x495040}, {0xF01, 0x57C0F0}, {0xF2A, 0x5341A0}, {0xF31, 0x5341A0}, {0xF47, 0x594E00}, {0xF53, 0x5341A0}, {0xF5A, 0x5341A0}, {0xF70, 0x594E00}, {0xF7C, 0x5341A0}, {0xF83, 0x5341A0}, {0xF99, 0x594E00}, {0xFA5, 0x5341A0}, {0xFAC, 0x5341A0}, {0xFC2, 0x594E00}, {0xFCE, 0x5341A0}, {0xFD5, 0x5341A0}, {0xFEB, 0x594E00}, {0xFF7, 0x5341A0}, {0xFFE, 0x5341A0}, {0x1014, 0x594E00}, {0x1020, 0x5341A0}, {0x1027, 0x5341A0}, {0x103D, 0x594E00}, {0x1049, 0x5341A0}, {0x1050, 0x5341A0}, {0x1066, 0x594E00}, {0x1072, 0x5341A0}, {0x1079, 0x5341A0}, {0x108F, 0x594E00}, {0x109B, 0x5341A0}, {0x10A2, 0x5341A0}, {0x10B8, 0x594E00}, {0x10C4, 0x5341A0}, {0x10CB, 0x5341A0}, {0x10E1, 0x594E00}, {0x10ED, 0x5341A0}, {0x10F4, 0x5341A0}, {0x110A, 0x594E00}, {0x114D, 0x4976D0}, {0x11EF, 0x495040}, {0x11F6, 0x5341C0}, {0x11FD, 0x5341C0}, {0x1213, 0x594E00}, {0x1226, 0x495040}, {0x122D, 0x5341C0}, {0x1234, 0x5341C0}, {0x124A, 0x594E00}, {0x1256, 0x495040}, {0x1264, 0x5341C0}, {0x126B, 0x5341C0}, {0x1281, 0x594E00}, {0x1294, 0x495040}, {0x129B, 0x5341C0}, {0x12A2, 0x5341C0}, {0x12B8, 0x594E00}, {0x12CB, 0x495040}, {0x12D2, 0x5341C0}, {0x12D9, 0x5341C0}, {0x12EF, 0x594E00}, {0x1302, 0x495040}, {0x1309, 0x5341C0}, {0x1310, 0x5341C0}, {0x1326, 0x594E00}, {0x1332, 0x495040}, {0x1340, 0x5341C0}, {0x1347, 0x5341C0}, {0x135D, 0x594E00}, {0x1370, 0x495040}, {0x1377, 0x5341C0}, {0x137E, 0x5341C0}, {0x1394, 0x594E00}, {0x13A0, 0x495040}, {0x13AE, 0x5341C0}, {0x13B5, 0x5341C0}, {0x13CB, 0x594E00}, {0x13DE, 0x495040}, {0x13F3, 0x495040}, {0x13FA, 0x5341C0}, {0x1401, 0x5341C0}, {0x143C, 0x57C140}, {0x1451, 0x57C140}, {0x14C4, 0x4976D0}, {0x150F, 0x57C140}, {0x1523, 0x57C140}, {0x153F, 0x532ED0}, {0x1546, 0x4410B0}, {0x1566, 0x532ED0}, {0x156D, 0x4410B0}, {0x158D, 0x532ED0}, {0x1594, 0x4410B0}, {0x15AE, 0x57C0F0}, {0x15E5, 0x57C0F0}, {0x15EC, 0x495040}, {0x1643, 0x5341A0}, {0x164A, 0x5341A0}, {0x1651, 0x5341A0}, {0x1667, 0x594E00}, {0x1673, 0x5341A0}, {0x167A, 0x5341A0}, {0x1681, 0x5341A0}, {0x1697, 0x594E00}, {0x16BA, 0x57C7A0}, {0x16E3, 0x57C0F0}, {0x16F0, 0x57C0F0}, {0x16FE, 0x57C0F0}, {0x170C, 0x57C0F0}, {0x1719, 0x57C0F0}};
constexpr sh::JumpTable kTables547920[] = {{0x1C, 0x172C, 59}, {0xF24, 0x1AA0, 16}, {0x163D, 0x1BB4, 3}, {0x6DF, 0x191C, 16}, {0x146C, 0x1B60, 7}, {0x148F, 0x1B7C, 7}, {0x14B2, 0x1B98, 7}, {0x11E2, 0x1B20, 16}, {0x1138, 0x1AE0, 16}, {0xD88, 0x1A4C, 7}, {0xDAB, 0x1A68, 7}, {0xDCE, 0x1A84, 7}, {0xBFD, 0x1A0C, 16}, {0xB53, 0x19CC, 16}, {0xA9D, 0x19B0, 7}, {0xA28, 0x1994, 7}, {0x948, 0x1978, 7}, {0x906, 0x195C, 7}, {0x59F, 0x1900, 7}, {0x4BB, 0x18D4, 7}, {0x3DA, 0x18B0, 5}, {0x2E7, 0x1880, 6}, {0x35C, 0x1898, 6}};
// 0x5494F0 Scena05_Run17: 0x58C bytes; shape: run handler (Scena05_Runs entry 17): no arguments
constexpr sh::CallSite kCalls5494F0[] = {{0x3B, 0x4976D0}, {0x63, 0x4976D0}, {0x9D, 0x57C7A0}, {0xE3, 0x57C0F0}, {0x10E, 0x594E00}, {0x129, 0x594E00}, {0x135, 0x587B40}, {0x13F, 0x587740}, {0x1AE, 0x587740}, {0x1C8, 0x4DF820}, {0x1CF, 0x4DF820}, {0x1E9, 0x587740}, {0x212, 0x587AE0}, {0x23F, 0x57C0F0}, {0x286, 0x587740}, {0x2B4, 0x57C0F0}, {0x2CA, 0x594E00}, {0x319, 0x594E00}, {0x343, 0x587740}, {0x34A, 0x4410B0}, {0x371, 0x57C0F0}, {0x394, 0x531F90}, {0x3A0, 0x531F90}, {0x3E1, 0x594E00}, {0x403, 0x594E00}, {0x439, 0x594E00}, {0x459, 0x5734F0}, {0x46D, 0x57C0F0}, {0x494, 0x57C0F0}, {0x4BB, 0x594E00}, {0x4D6, 0x594E00}, {0x4E9, 0x57C7A0}};
constexpr sh::JumpTable kTables5494F0[] = {{0x14, 0x510, 31}};
// 0x549A80 Scena05_Run18: 0x3D0 bytes; shape: run handler (Scena05_Runs entry 18): no arguments
constexpr sh::CallSite kCalls549A80[] = {{0x3D, 0x57C7A0}, {0x83, 0x57C0F0}, {0x99, 0x594E00}, {0xC7, 0x57C0F0}, {0xD8, 0x587740}, {0x103, 0x594E00}, {0x12A, 0x495040}, {0x16C, 0x5341C0}, {0x173, 0x5341C0}, {0x17F, 0x5341C0}, {0x1A5, 0x594E00}, {0x1D5, 0x5734F0}, {0x1F8, 0x587740}, {0x201, 0x587AE0}, {0x230, 0x532ED0}, {0x237, 0x4410B0}, {0x258, 0x531F90}, {0x261, 0x587AE0}, {0x28E, 0x57C0F0}, {0x295, 0x495040}, {0x2D7, 0x5341A0}, {0x2DE, 0x5341A0}, {0x2F4, 0x594E00}, {0x307, 0x5341A0}, {0x30E, 0x5341A0}, {0x324, 0x594E00}, {0x34C, 0x533E50}, {0x360, 0x57C0F0}, {0x368, 0x57C7A0}};
constexpr sh::JumpTable kTables549A80[] = {{0x14, 0x38C, 17}};
// 0x549E50 Scena05_Run19: 0x48 bytes; shape: run handler (Scena05_Runs entry 19): no arguments
constexpr sh::CallSite kCalls549E50[] = {{0x18, 0x57C7A0}, {0x31, 0x4976D0}};
// 0x549EA0 Scena05_Run20: 0x230 bytes; shape: run handler (Scena05_Runs entry 20): no arguments
constexpr sh::CallSite kCalls549EA0[] = {{0x1C, 0x54A1E0}, {0x56, 0x594E00}, {0x70, 0x594E00}, {0x95, 0x57C0F0}, {0xC7, 0x594E00}, {0xE1, 0x594E00}, {0x107, 0x57C0F0}, {0x13A, 0x594E00}, {0x154, 0x594E00}, {0x1AD, 0x5341A0}, {0x1B8, 0x5341A0}, {0x1EB, 0x57C0F0}, {0x1F3, 0x57C7A0}};
constexpr sh::JumpTable kTables549EA0[] = {{0x13, 0x214, 7}};
// 0x54A0D0 Scena05_Run21: 0x78 bytes; shape: run handler (Scena05_Runs entry 21): no arguments
constexpr sh::CallSite kCalls54A0D0[] = {{0x20, 0x57C110}, {0x28, 0x57C7A0}, {0x5A, 0x57C0F0}, {0x61, 0x531F90}, {0x68, 0x5734F0}};
// 0x54A150 Scena05_Run22: 0x45 bytes; shape: run handler (Scena05_Runs entry 22): no arguments
constexpr sh::CallSite kCalls54A150[] = {{0x18, 0x57C7A0}, {0x2E, 0x4976D0}};
// 0x54A1A0 Scena05_PartyCounter: 0x37 bytes; shape: called directly by Scena05_Run16: no arguments
// 0x54A1E0 Scena05_SpawnMember: 0xB3 bytes; shape: called directly by Scena05_Run20: (kind) a byte, random
constexpr sh::CallSite kCalls54A1E0[] = {{0xA, 0x589810}};
// 0x54A2A0 Scena05_ObjectTrigger: 0x1F bytes; +0x14 note: call through .data 0x66109c, 9 code entries (a data_tables entry); root: chapter 5 slot 1, the object trigger (0x56D6D0, the object); shape: vtable slot 1 (the object trigger): the object
// 0x54A2C0 Scena05_Object01: 0x4D bytes; shape: Scena05_Objects entry 1: (object, bits) ignored
constexpr sh::CallSite kCalls54A2C0[] = {{0x8, 0x57C140}, {0x1D, 0x57C0F0}, {0x25, 0x57C7C0}};
// 0x54A310 Scena05_Object02: 0x38 bytes; shape: Scena05_Objects entry 2: (object, bits) ignored
constexpr sh::CallSite kCalls54A310[] = {{0x8, 0x57C0F0}, {0x10, 0x57C7C0}};
// 0x54A350 Scena05_Object03: 0x28 bytes; shape: Scena05_Objects entry 3: (object, bits) ignored
constexpr sh::CallSite kCalls54A350[] = {{0x0, 0x57C7C0}};
// 0x54A380 Scena05_Object04: 0x28 bytes; shape: Scena05_Objects entries 4 and 8: (object, bits) ignored
constexpr sh::CallSite kCalls54A380[] = {{0x0, 0x57C7C0}};
// 0x54A3B0 Scena05_Object05: 0x23 bytes; shape: Scena05_Objects entry 5: (object, bits) ignored
constexpr sh::CallSite kCalls54A3B0[] = {{0x0, 0x57C7C0}};
// 0x54A3E0 Scena05_Object06: 0x23 bytes; shape: Scena05_Objects entry 6: (object, bits) ignored
constexpr sh::CallSite kCalls54A3E0[] = {{0x0, 0x57C7C0}};
// 0x54A410 Scena05_Object07: 0x23 bytes; shape: Scena05_Objects entry 7: (object, bits) ignored
constexpr sh::CallSite kCalls54A410[] = {{0x0, 0x57C7C0}};
// 0x54A440 Scena05_StepHook: 0x3B7 bytes; root: chapter 5 slot 2, the step hook (x, z), al; shape: vtable slot 2, hook (x, z) -> al
constexpr sh::CallSite kCalls54A440[] = {{0x23, 0x57C140}, {0x38, 0x57C140}, {0x64, 0x57C7C0}, {0xAF, 0x57C140}, {0xE7, 0x57C7C0}, {0x121, 0x57C140}, {0x165, 0x57C7C0}, {0x1D0, 0x57C7C0}, {0x212, 0x57C140}, {0x23E, 0x57C7C0}, {0x285, 0x57C140}, {0x29D, 0x57C140}, {0x2E9, 0x57C7C0}, {0x331, 0x57C7C0}, {0x381, 0x57C7C0}};
// 0x54A800 Scena05_CellHook: 0x2A bytes; +0x23 note: jmp through .data 0x6610cc, 2 code entries (a data_tables entry); root: chapter 5 slot 4, the cell hook (x, z), al; shape: vtable slot 4, hook (a, b) -> al (sign-extended by 0x56D7A0)
constexpr sh::CallSite kCalls54A800[] = {{0x11, 0x56D800}};
// 0x54A830 Scena05_Cell0: 0x69 bytes; shape: Scena05_CellHooks entry 0, (a, b) in place -> al
constexpr sh::CallSite kCalls54A830[] = {{0x8, 0x57C140}, {0x1D, 0x57C140}, {0x30, 0x57C0F0}, {0x35, 0x57C7C0}, {0x4D, 0x587740}};
// 0x54A8A0 Scena05_Cell1: 0x6B bytes; shape: Scena05_CellHooks entry 1, (a, b) in place -> al
constexpr sh::CallSite kCalls54A8A0[] = {{0x8, 0x57C140}, {0x1D, 0x57C140}, {0x30, 0x57C0F0}, {0x35, 0x57C7C0}, {0x4D, 0x587740}};

// Scena05_EnterArea (65 calls) and Scena05_Run16 (231) have more call sites
// than the harness re-aims in one clone (64). So this file makes their copies
// itself, as scena_sc12_fuzz.cpp does for its two - bof3::CloneOriginal with
// every site re-aimed at a trampoline that calls the harness's recorder for
// the callee (StandIn: the same log entry, disturbance and answer a re-aimed
// site gets), the jump tables moved into the copy - and hands the harness, as
// each one's "original", a six-byte `jmp [copy]` of its own, which the
// harness clones like any function with no calls. Theirs is still Capcom's
// bytes, only relocated here instead of in the harness.
template <typename F> F Stub(F f) {
    return reinterpret_cast<F>(const_cast<void*>(sh::StandIn(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f)))));
}
template <typename F> F Raw(std::uint32_t address) { return Stub(reinterpret_cast<F>(static_cast<std::uintptr_t>(address))); }
unsigned char __cdecl TFlagsTest(const unsigned char* b, unsigned i) { return Stub(Flags_Test)(b, i); }
void __cdecl TFlagsSet(unsigned char* b, unsigned i) { Stub(Flags_Set)(b, i); }
void __cdecl TFlagsClear(unsigned char* b, unsigned i) { Stub(Flags_Clear)(b, i); }
void __cdecl TClear40() { Stub(ScriptFlags_Clear40)(); }
void __cdecl TMsg(unsigned short id) { Stub(Msg_OpenScript)(id); }
void __cdecl TChangeArea(unsigned area, int x, int z, unsigned flags) { Stub(Field_ChangeArea)(area, x, z, flags); }
void __cdecl TSound(unsigned short id) { Stub(Sound_PlayEffect)(id); }
void __cdecl TCallA(unsigned n) { Stub(Scenario_CallA)(n); }
void __cdecl TCallB(unsigned n) { Stub(Scenario_CallB)(n); }
void __cdecl TPlace(int x, int z, unsigned kind) { Raw<PlaceFn>(at::kPartyPlace)(x, z, kind); }
void __cdecl TBattle(unsigned kind) { Stub(Field_StartEventBattle)(kind); }
unsigned __cdecl TDropIn(unsigned e) { return Stub(Party_DropIn)(e); }
void __cdecl TKind2(unsigned char a) { Stub(Kind2_Place)(a); }
void __cdecl TTransition(unsigned char k) { Stub(Transition_Start)(k); }
void __cdecl TFadeOutStop(int f) { Stub(Music_FadeOutStop)(f); }
void __cdecl TPartyCounter() { Stub(Scena05_PartyCounter)(); }
void __cdecl TSetBit80() { Raw<VoidFn>(at::kSetBit80)(); }
int __cdecl TLoadFile(unsigned t) { return Stub(Music_LoadFile)(t); }
int __cdecl TLoadDone() { return Stub(File_LoadDone)(); }
void __cdecl TSleep(int f) { Stub(Task_Sleep)(f); }
void __cdecl TMusicPlay(unsigned t, int f) { Stub(Music_Play)(t, f); }

struct Tramp { std::uint32_t target; const void* to; };
#define SC5_T(address, fn) {address, reinterpret_cast<const void*>(&fn)}
const Tramp kTramps[] = {
    SC5_T(0x57C140, TFlagsTest),   SC5_T(0x57C0F0, TFlagsSet),    SC5_T(0x57C110, TFlagsClear),
    SC5_T(0x57C7A0, TClear40),     SC5_T(0x4976D0, TMsg),         SC5_T(0x594E00, TChangeArea),
    SC5_T(0x587740, TSound),       SC5_T(0x5341A0, TCallA),       SC5_T(0x5341C0, TCallB),
    SC5_T(0x532ED0, TPlace),       SC5_T(0x4410B0, TBattle),      SC5_T(0x531F90, TDropIn),
    SC5_T(0x5734F0, TKind2),       SC5_T(0x495040, TTransition),  SC5_T(0x587B40, TFadeOutStop),
    SC5_T(0x54A1A0, TPartyCounter), SC5_T(0x56D6F0, TSetBit80),   SC5_T(0x587A20, TLoadFile),
    SC5_T(0x454810, TLoadDone),    SC5_T(0x5A9949, TSleep),       SC5_T(0x587AE0, TMusicPlay),
};
#undef SC5_T

}  // namespace
}  // namespace scena_sc5

extern "C" {
void* g_sc5_enter_copy = nullptr;
void* g_sc5_run16_copy = nullptr;
__attribute__((naked)) void Sc5EnterAreaTheirs() { asm("jmp *_g_sc5_enter_copy"); }
__attribute__((naked)) void Sc5Run16Theirs() { asm("jmp *_g_sc5_run16_copy"); }
}

namespace scena_sc5 {
namespace {

constexpr std::uint32_t kJmpWrapper = 6;   // FF 25 disp32

void* CopyWithTramps(const char* name, std::uint32_t base, std::uint32_t size, const sh::CallSite* sites, int n,
                     const sh::JumpTable* tables, int n_tables) {
    static bof3::CloneCall calls[256];
    if (n > 256) bof3::Fatal("scena_sc5: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) {
        const void* to = nullptr;
        for (const Tramp& t : kTramps)
            if (t.target == sites[i].target) to = t.to;
        if (!to) bof3::Fatal("scena_sc5: %s: no trampoline for 0x%X", name, (unsigned)sites[i].target);
        calls[i] = {sites[i].offset, to, sites[i].target};
    }
    void* copy = bof3::CloneOriginal(name, base, size, calls, n);
    for (int i = 0; i < n_tables; ++i)
        move_script::Relocate(copy, base, size, {tables[i].jmp_disp, tables[i].table, tables[i].entries});
    return copy;
}

std::uint32_t Wrapper(void (*f)()) {
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p[0] != 0xFF || p[1] != 0x25) bof3::Fatal("scena_sc5: the jmp wrapper at %p is not FF 25", static_cast<const void*>(p));
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const sh::Clone kClones[] = {
    {"Scena05_Frame", 0x546390, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Frame), 0, false, sh::Shape::kSlot},
    {"Scena05_EnterArea", Wrapper(&Sc5EnterAreaTheirs), kJmpWrapper, nullptr, 0, nullptr, 0, nullptr, 0,   // this file's copy (above)
     reinterpret_cast<const void*>(&::Scena05_EnterArea), 0, false, sh::Shape::kState},
    {"Scena05_Run", 0x546AB0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run), 0, false, sh::Shape::kState},
    {"Scena05_Run1", 0x546AC0, 0xB8, kCalls546AC0, SH_N(kCalls546AC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run1), 0, false, sh::Shape::kState},
    {"Scena05_Run4", 0x546B80, 0x1DA, kCalls546B80, SH_N(kCalls546B80), nullptr, 0, kTables546B80, SH_N(kTables546B80), reinterpret_cast<const void*>(&::Scena05_Run4), 0, false, sh::Shape::kState},
    {"Scena05_Run5", 0x546D60, 0x2D7, kCalls546D60, SH_N(kCalls546D60), nullptr, 0, kTables546D60, SH_N(kTables546D60), reinterpret_cast<const void*>(&::Scena05_Run5), 0, false, sh::Shape::kState},
    {"Scena05_Run6", 0x547040, 0x4A, kCalls547040, SH_N(kCalls547040), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run6), 0, false, sh::Shape::kState},
    {"Scena05_Run7", 0x547090, 0x45, kCalls547090, SH_N(kCalls547090), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run7), 0, false, sh::Shape::kState},
    {"Scena05_Run8", 0x5470E0, 0x158, kCalls5470E0, SH_N(kCalls5470E0), nullptr, 0, kTables5470E0, SH_N(kTables5470E0), reinterpret_cast<const void*>(&::Scena05_Run8), 0, false, sh::Shape::kState},
    {"Scena05_Run9", 0x547240, 0x1F0, kCalls547240, SH_N(kCalls547240), nullptr, 0, kTables547240, SH_N(kTables547240), reinterpret_cast<const void*>(&::Scena05_Run9), 0, false, sh::Shape::kState},
    {"Scena05_Run10", 0x547430, 0x126, kCalls547430, SH_N(kCalls547430), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run10), 0, false, sh::Shape::kState},
    {"Scena05_Run11", 0x547560, 0x45, kCalls547560, SH_N(kCalls547560), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run11), 0, false, sh::Shape::kState},
    {"Scena05_Run13", 0x5475B0, 0x26F, kCalls5475B0, SH_N(kCalls5475B0), nullptr, 0, kTables5475B0, SH_N(kTables5475B0), reinterpret_cast<const void*>(&::Scena05_Run13), 0, false, sh::Shape::kState},
    {"Scena05_Run14", 0x547820, 0xF4, kCalls547820, SH_N(kCalls547820), nullptr, 0, kTables547820, SH_N(kTables547820), reinterpret_cast<const void*>(&::Scena05_Run14), 0, false, sh::Shape::kState},
    {"Scena05_Run16", Wrapper(&Sc5Run16Theirs), kJmpWrapper, nullptr, 0, nullptr, 0, nullptr, 0,   // this file's copy (above)
     reinterpret_cast<const void*>(&::Scena05_Run16), 0, false, sh::Shape::kState},
    {"Scena05_Run17", 0x5494F0, 0x58C, kCalls5494F0, SH_N(kCalls5494F0), nullptr, 0, kTables5494F0, SH_N(kTables5494F0), reinterpret_cast<const void*>(&::Scena05_Run17), 0, false, sh::Shape::kState},
    {"Scena05_Run18", 0x549A80, 0x3D0, kCalls549A80, SH_N(kCalls549A80), nullptr, 0, kTables549A80, SH_N(kTables549A80), reinterpret_cast<const void*>(&::Scena05_Run18), 0, false, sh::Shape::kState},
    {"Scena05_Run19", 0x549E50, 0x48, kCalls549E50, SH_N(kCalls549E50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run19), 0, false, sh::Shape::kState},
    {"Scena05_Run20", 0x549EA0, 0x230, kCalls549EA0, SH_N(kCalls549EA0), nullptr, 0, kTables549EA0, SH_N(kTables549EA0), reinterpret_cast<const void*>(&::Scena05_Run20), 0, false, sh::Shape::kState},
    {"Scena05_Run21", 0x54A0D0, 0x78, kCalls54A0D0, SH_N(kCalls54A0D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run21), 0, false, sh::Shape::kState},
    {"Scena05_Run22", 0x54A150, 0x45, kCalls54A150, SH_N(kCalls54A150), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Run22), 0, false, sh::Shape::kState},
    {"Scena05_PartyCounter", 0x54A1A0, 0x37, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_PartyCounter), 0, false, sh::Shape::kState},
    {"Scena05_SpawnMember", 0x54A1E0, 0xB3, kCalls54A1E0, SH_N(kCalls54A1E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_SpawnMember), 0, false, sh::Shape::kState},
    {"Scena05_ObjectTrigger", 0x54A2A0, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_ObjectTrigger), 0, false, sh::Shape::kObject},
    {"Scena05_Object01", 0x54A2C0, 0x4D, kCalls54A2C0, SH_N(kCalls54A2C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Object01), 0, false, sh::Shape::kState},
    {"Scena05_Object02", 0x54A310, 0x38, kCalls54A310, SH_N(kCalls54A310), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Object02), 0, false, sh::Shape::kState},
    {"Scena05_Object03", 0x54A350, 0x28, kCalls54A350, SH_N(kCalls54A350), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Object03), 0, false, sh::Shape::kState},
    {"Scena05_Object04", 0x54A380, 0x28, kCalls54A380, SH_N(kCalls54A380), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Object04), 0, false, sh::Shape::kState},
    {"Scena05_Object05", 0x54A3B0, 0x23, kCalls54A3B0, SH_N(kCalls54A3B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Object05), 0, false, sh::Shape::kState},
    {"Scena05_Object06", 0x54A3E0, 0x23, kCalls54A3E0, SH_N(kCalls54A3E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Object06), 0, false, sh::Shape::kState},
    {"Scena05_Object07", 0x54A410, 0x23, kCalls54A410, SH_N(kCalls54A410), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Object07), 0, false, sh::Shape::kState},
    {"Scena05_StepHook", 0x54A440, 0x3B7, kCalls54A440, SH_N(kCalls54A440), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_StepHook), 0xFF, false, sh::Shape::kHook},
    {"Scena05_CellHook", 0x54A800, 0x2A, kCalls54A800, SH_N(kCalls54A800), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_CellHook), 0xFF, false, sh::Shape::kHook},
    {"Scena05_Cell0", 0x54A830, 0x69, kCalls54A830, SH_N(kCalls54A830), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Cell0), 0xFF, false, sh::Shape::kHook},
    {"Scena05_Cell1", 0x54A8A0, 0x6B, kCalls54A8A0, SH_N(kCalls54A8A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena05_Cell1), 0xFF, false, sh::Shape::kHook},
};
enum : unsigned {
    kFrame, kEnterArea, kRun, kRun1, kRun4, kRun5, kRun6, kRun7, kRun8, kRun9, kRun10, kRun11, kRun13, kRun14,
    kRun16, kRun17, kRun18, kRun19, kRun20, kRun21, kRun22, kPartyCounter, kSpawnMember, kObjectTrigger,
    kObject01, kObject02, kObject03, kObject04, kObject05, kObject06, kObject07, kStepHook, kCellHook, kCell0,
    kCell1, kCount
};
static_assert(sizeof kClones / sizeof kClones[0] == kCount, "one role per clone");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

unsigned char& B(std::uint32_t a) { return *sh::Mem(a); }
void SetW(std::uint32_t a, std::uint32_t v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(sh::Mem(a), &w, 2);
}
void SetD(std::uint32_t a, std::uint32_t v) { std::memcpy(sh::Mem(a), &v, 4); }

// Scena05_Objects' and Scena05_CellHooks' entries take arguments (the object
// and the flag row; the cell (a, b)), which a DataTable's handler recorder
// does not log. So the seed writes a stand-in of the exact type into every
// entry, one per index (a wrong index is a different log), and the two
// tables are regions (the harness puts them back). Each logs against the
// dispatcher's own address, which no clone calls.
template <unsigned I> void __cdecl ObjectEntryStub(unsigned char* object, std::uint32_t row) {
    sh::Record(0x54A2A0, I, Key(object), row);
    sh::Stir();
}
template <unsigned I> unsigned char __cdecl CellEntryStub(unsigned a, unsigned b) {
    sh::Record(0x54A800, I, a, b);
    sh::Stir();
    return static_cast<unsigned char>(sh::Noise());
}
using ObjectFn = void (__cdecl*)(unsigned char*, std::uint32_t);
using CellFn = unsigned char (__cdecl*)(unsigned, unsigned);
const ObjectFn kObjectEntries[at::kObjectCount] = {
    &ObjectEntryStub<0>, &ObjectEntryStub<1>, &ObjectEntryStub<2>, &ObjectEntryStub<3>, &ObjectEntryStub<4>,
    &ObjectEntryStub<5>, &ObjectEntryStub<6>, &ObjectEntryStub<7>, &ObjectEntryStub<8>,
};
const CellFn kCellEntries[at::kCellHookCount] = {&CellEntryStub<0>, &CellEntryStub<1>};

// Scena05_PartyCounter's recorder does what the function does to counter 1
// (Scena05_Run16's step 0x11 reads it back): a stand-in quieter than the
// callee would hide the branch (HANDOFF, traps).
std::uint32_t PartyCounterEffect(const std::uint32_t*, std::uint32_t answer) {
    for (unsigned i = 0; i < 3; ++i)
        if (B(at::kParty + i) == 5) B(at::kCounters + 1) = 1;
    for (unsigned i = 0; i < 3; ++i)
        if (B(at::kParty + i) == 6) B(at::kCounters + 1) = 2;
    return answer;
}

// The callees the standard set lacks, or records otherwise than this bank
// needs (docs/scenario_harness.md section 4; the group's listing stands).
#define SC5_RAW(name, address) name, address, address
#define SC5_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    // tested on al alone (test al, al): garbage above a 0 must not matter
    {SC5_OURS(Flags_Test), 2, {kAll, kU8}, sh::Answer::kFlag, 0, 0},
    // group SE's, ours: called by name (the standard set lists it by address)
    {SC5_OURS(Field_StartEventBattle), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    // a pass over the eight records at 0x903A70 and the party
    {SC5_RAW("0x533E50", at::kPartyPass), 0, {}, sh::Answer::kGarbage, 0, 0},
    // the cell-record search: the records, the count, the cell; none or 0..1
    {SC5_RAW("0x56D800", at::kCellFind), 4, {kAll, kU8, kU8, kU8}, sh::Answer::kByte, 0xFF, 1},
    // the bank's own, called directly: counter 1 from the party bytes (the
    // effect above); the effect record at the leader (its kind logged)
    {SC5_OURS(Scena05_PartyCounter), 0, {}, sh::Answer::kGarbage, 0, 0, {}, &PartyCounterEffect},
    {SC5_OURS(Scena05_SpawnMember), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    // the log slots of the table stand-ins above (keyed on the dispatchers'
    // own addresses, which no clone calls)
    {"Scena05_Objects[i]", 0x54A2A0, 0x54A2A0, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&ObjectEntryStub<0>)},
    {"Scena05_CellHooks[i]", 0x54A800, 0x54A800, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&CellEntryStub<0>)},
};
#undef SC5_OURS
#undef SC5_RAW

// The state and run tables take no arguments: swapped for recorders.
const sh::DataTable kTables[] = {{at::kStates, at::kStateCount}, {at::kRuns, at::kRunCount}};

// Beyond the standard regions: the selector, the music byte, the message
// box's flag word, the three bytes of bit 0, the two level bytes, and the two
// tables the seed writes stand-ins into.
const sh::Region kRegions[] = {
    {at::kSelector, 4},
    {at::kMusicCurrent, 1},
    {at::kMsgFlags, 4},
    {at::kBitsB1F, 1},
    {at::kBitsDAF, 1},
    {at::kBitsE53, 1},
    {at::kLevel, 1},
    {at::kLevelB, 1},
    {at::kObjects, 4 * at::kObjectCount},
    {at::kCellHooks, 4 * at::kCellHookCount},
};

// The values the chapter's code compares with.
std::uint32_t AnArea() {
    return SH_PICK(0x2D, 0x31, 0x34, 0x41, 0x43, 0x44, 0x45, 0x4E, 0x4F, 0x50, 0x51, 0x30, 0x4D);
}
std::uint32_t ASelector() { return SH_PICK(1, 2, 6, 1, 2, 0, 3, 5) | (sh::Half() ? 0x80 : 0); }
std::uint32_t ACode() { return SH_PICK(1, 2, 3, 4, 5, 6, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0, 7, 0xA, 0x11); }
std::uint32_t AMember() { return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 5, 6, 0xFF); }

// Each run's steps (the cases its switch holds), and one past.
std::uint32_t AStep(unsigned k) {
    switch (k) {
    case kRun1: return sh::Next() % 4;
    case kRun4: return SH_PICK(0, 1, 2, 3, 4, 7, 8, 9, 0xA, 0x13, 0x14, 0x15, 0x16);
    case kRun5: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24);
    case kRun6: return sh::Next() % 2;
    case kRun7: case kRun11: case kRun19: case kRun22: case kRun21: return sh::Next() % 3;
    case kRun8: return sh::Next() % 8;
    case kRun9: return SH_PICK(0, 1, 2, 3, 4, 9, 0xA, 0xB, 0xC, 0xD);
    case kRun10: return SH_PICK(0, 1, 2, 3, 4, 5);
    case kRun13: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13);
    case kRun14: return sh::Next() % 6;
    case kRun16: return sh::Next() % 0x68;
    case kRun17: return sh::Next() % 0x20;
    case kRun18: return sh::Next() % 0x12;
    case kRun20: return sh::Next() % 8;
    default: return sh::Next();
    }
}

// The counter-0 value each run's step waits on (read off the originals), so
// a step's action is reached, not only its wait.
struct Wait { unsigned char run, step, count; };
const Wait kWaits[] = {
    {1, 2, 0x2D},
    {4, 1, 3}, {4, 2, 0xD}, {4, 3, 0xF}, {4, 7, 0x14}, {4, 8, 0x16}, {4, 9, 8},
    {5, 5, 0x23}, {5, 0x1C, 0x2C}, {5, 0x1D, 0x32}, {5, 0x1F, 0x50},
    {6, 0, 0x18},
    {8, 6, 0x28},
    {9, 1, 5}, {9, 1, 8}, {9, 2, 8}, {9, 0xA, 0xC}, {9, 0xB, 0x1C}, {9, 0xC, 0x1D},
    {10, 0, 0x23}, {10, 1, 0x23}, {10, 4, 3},
    {13, 3, 0x2B}, {13, 3, 0x2C}, {13, 4, 0x2E}, {13, 5, 0x33}, {13, 6, 2}, {13, 0x11, 0x2B}, {13, 0x11, 0x2C},
    {13, 0x12, 0x33},
    {14, 1, 2}, {14, 1, 0xA}, {14, 4, 0x14},
    {16, 0, 0x14}, {16, 2, 0x14}, {16, 0x11, 0xB}, {16, 0x14, 0xA}, {16, 0x15, 0x14}, {16, 0x16, 0x16},
    {16, 0x18, 0x1B}, {16, 0x1E, 0x14}, {16, 0x1F, 0x16}, {16, 0x21, 0x1B}, {16, 0x24, 0x14}, {16, 0x25, 0x16},
    {16, 0x27, 0x1B}, {16, 0x34, 0xA}, {16, 0x36, 0x16}, {16, 0x38, 0x1B}, {16, 0x3E, 0xA}, {16, 0x40, 0x16},
    {16, 0x42, 0x1B}, {16, 0x5A, 0x1C}, {16, 0x5F, 9},
    {17, 0, 1}, {17, 2, 3}, {17, 7, 6}, {17, 9, 1}, {17, 0xA, 5}, {17, 0xC, 6}, {17, 0xF, 0xF}, {17, 0x15, 0x11},
    {17, 0x16, 0x10}, {17, 0x18, 6}, {17, 0x19, 0xA}, {17, 0x1A, 0xE}, {17, 0x1B, 0x15}, {17, 0x1C, 1},
    {17, 0x1D, 0xA}, {17, 0x1E, 1},
    {18, 0, 1}, {18, 2, 3}, {18, 3, 7}, {18, 4, 2}, {18, 8, 1}, {18, 9, 7}, {18, 0xA, 0xB}, {18, 0xB, 0xC},
    {18, 0xC, 0x14}, {18, 0x10, 8},
    {20, 2, 1}, {20, 3, 0xA}, {20, 4, 4}, {20, 6, 5},
    {21, 1, 0x1C},
};
unsigned RunOf(unsigned k) {
    static const unsigned char kRunOf[kCount] = {
        0, 0, 0, 1, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 16, 17, 18, 19, 20, 21, 22,
    };
    return k < kCount ? kRunOf[k] : 0;
}

// The step hook's rectangles: x lo..hi, z lo..hi, the area, and which of x
// and z is a pair of equal values (0 x, 1 z) - both its values and one either
// side, the other dimension at its bounds and one either side.
struct Rect { std::uint32_t x0, x1, z0, z1; unsigned area; };
const Rect kRects[] = {
    {0x4D0000, 0x4D8000, 0x210000, 0x258000, 0x31},
    {0x930000, 0x948000, 0x60000, 0x68000, 0x34},
    {0x840000, 0x868000, 0xC0000, 0xC8000, 0x34},
    {0x840000, 0x868000, 0x180000, 0x188000, 0x34},
    {0x1D0000, 0x1E8000, 0x340000, 0x348000, 0x43},
    {0x190000, 0x1F8000, 0x240000, 0x248000, 0x44},
    {0x288000, 0x290000, 0x7E0000, 0x818000, 0x4E},
    {0x1F0000, 0x1F8000, 0x710000, 0x728000, 0x4E},
};
constexpr unsigned kRectCount = sizeof kRects / sizeof kRects[0];

unsigned g_k;
int g_rect = -1;

void Seed(unsigned k) {
    g_k = k;
    g_rect = -1;
    // The stand-ins into the two tables that take arguments.
    for (unsigned i = 0; i < at::kObjectCount; ++i) SetD(at::kObjects + 4 * i, KeyOf(kObjectEntries[i]));
    for (unsigned i = 0; i < at::kCellHookCount; ++i) SetD(at::kCellHooks + 4 * i, KeyOf(kCellEntries[i]));
    // The cells the chapter compares, each most of the time at a value a
    // branch tests.
    if (sh::Often()) SetW(at::kArea, AnArea());
    for (unsigned c = 0; c < 3; ++c)
        if (sh::Half()) B(at::kCounters + c) = static_cast<unsigned char>(sh::Next() % 8);
    if (sh::Often()) B(at::kCounters + 3) = static_cast<unsigned char>(ACode());
    if (sh::Often()) B(at::kRequest) = static_cast<unsigned char>(SH_PICK(0, 2, 2, 6));
    if (sh::Half()) SetW(at::kWait, 0);
    if (sh::Often()) B(at::kSelector) = static_cast<unsigned char>(ASelector());
    if (sh::Often()) B(at::kMsgFlags) = static_cast<unsigned char>(B(at::kMsgFlags) | 2);
    for (unsigned i = 0; i < 3; ++i)
        if (sh::Often()) B(at::kParty + i) = static_cast<unsigned char>(AMember());
    if (sh::Often()) B(at::kPartyA) = static_cast<unsigned char>(SH_PICK(0, 1, 5, 6, 2));
    if (sh::Often()) B(at::kPartyB) = static_cast<unsigned char>(SH_PICK(0, 1, 5, 6, 2));
    if (sh::Half()) B(at::kLevel) = static_cast<unsigned char>(SH_PICK(7, 8, 9, 0x7F, 0x80, 0xFF));
    if (sh::Half()) SetW(at::kTimer, SH_PICK(0, 0xC6, 0xC7, 0xC8, 0xFFFF));
    switch (k) {
    case kEnterArea:   // the areas and counter 2's cases, together
        if (sh::Often()) SetW(at::kArea, SH_PICK(0x2D, 0x31, 0x34, 0x41, 0x44, 0x45, 0x4E, 0x4F, 0x50, 0x51));
        if (sh::Often()) B(at::kCounters + 2) = static_cast<unsigned char>(sh::Next() % 8);
        break;
    case kFrame: B(at::kState) = static_cast<unsigned char>(sh::Next() % at::kStateCount); break;
    case kRun: B(at::kRun) = static_cast<unsigned char>(sh::Next() % at::kRunCount); break;
    case kObjectTrigger: sh::SpriteRecord(0)[0x86] = static_cast<unsigned char>(sh::Next() % at::kObjectCount); break;
    case kStepHook:
        if (sh::Half()) {
            g_rect = static_cast<int>(sh::Next() % kRectCount);
            if (sh::Often()) SetW(at::kArea, kRects[g_rect].area);
        } else if (sh::Often()) {
            SetW(at::kArea, SH_PICK(0x31, 0x34, 0x43, 0x44, 0x4E));
        }
        break;
    case kRun1: case kRun4: case kRun5: case kRun6: case kRun7: case kRun8: case kRun9: case kRun10: case kRun11:
    case kRun13: case kRun14: case kRun16: case kRun17: case kRun18: case kRun19: case kRun20: case kRun21:
    case kRun22:
        B(at::kStep) = static_cast<unsigned char>(AStep(k));
        if (sh::Often()) {   // one of the values this step waits on (some wait on two)
            unsigned n = 0;
            for (const Wait& w : kWaits)
                if (w.run == RunOf(k) && w.step == B(at::kStep)) ++n;
            if (n != 0) {
                unsigned pick = sh::Next() % n;
                for (const Wait& w : kWaits)
                    if (w.run == RunOf(k) && w.step == B(at::kStep) && pick-- == 0) B(at::kCounters) = w.count;
            }
        }
        if (k == kRun16 && sh::Half()) B(at::kCounters + 1) = 1;
        break;
    default: break;
    }
}

// A bound, or one either side of it.
std::uint32_t Near(std::uint32_t v) { return v + SH_PICK(0, 0, 1, 0xFFFFFFFFu, 0x8000); }

void Args(unsigned k, std::uint32_t* a) {
    switch (k) {
    case kObjectTrigger: a[0] = Key(sh::SpriteRecord(0)); break;
    case kStepHook:
        if (g_rect >= 0) {   // one rectangle, x and z each at one of its bounds, one either side
            const Rect& r = kRects[g_rect];
            a[0] = Near(sh::Half() ? r.x0 : r.x1);
            a[1] = Near(sh::Half() ? r.z0 : r.z1);
            if (sh::Half()) a[0] = r.x0 + (r.x1 - r.x0) / 2;
            else if (sh::Half()) a[1] = r.z0 + (r.z1 - r.z0) / 2;
        } else if (sh::Often()) {
            const Rect& r = kRects[sh::Next() % kRectCount];
            a[0] = Near(sh::Half() ? r.x0 : r.x1);
            const Rect& s = kRects[sh::Next() % kRectCount];
            a[1] = Near(sh::Half() ? s.z0 : s.z1);
        }
        break;
    case kCellHook:   // the cell as bytes, with garbage above half the time
        if (sh::Half()) {
            a[0] &= 0xFF;
            a[1] &= 0xFF;
        }
        break;
    default: break;
    }
}

// After a call, two in three (beyond the harness's own): a counter, the area,
// the selector, the message box's done bit, a party byte, counter 3's code,
// the bit-0 bytes, the level byte. Draws only from h.
void Disturb(std::uint32_t h) {
    const unsigned char v = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 10) {
    case 0: B(at::kCounters + (h >> 12) % 3) = v; break;
    case 1: SetW(at::kArea, (h >> 16) & 1 ? 0x4E : 0x2D + (h >> 17) % 0x25); break;
    case 2: B(at::kSelector) = static_cast<unsigned char>(1 + (h >> 16) % 6); break;
    case 3: B(at::kMsgFlags) = static_cast<unsigned char>(B(at::kMsgFlags) ^ 2); break;
    case 4: B(at::kParty + (h >> 12) % 3) = static_cast<unsigned char>((h >> 16) % 8); break;
    case 5: B(at::kCounters + 3) = static_cast<unsigned char>((h >> 16) % 0x12); break;
    case 6: B(at::kBitsDAF) = static_cast<unsigned char>(B(at::kBitsDAF) ^ 1); break;
    case 7: B(at::kBitsE53) = static_cast<unsigned char>(B(at::kBitsE53) ^ 1); break;
    case 8: B(at::kLevel) = static_cast<unsigned char>(7 + (h >> 16) % 4); break;
    default: B(at::kCounters + 1) = static_cast<unsigned char>((h >> 16) & 1 ? 1 : v); break;
    }
}

// After every disturbance (two calls in three), for the two functions that
// re-read the area after their calls: half the time the area moved to one
// they test, since the harness's disturbance reaches the group's cells only
// one time in sixteen. Noise() is the recorders' stream, the same on both
// passes.
void Settle() {
    if (g_k != kEnterArea && g_k != kStepHook) return;
    const std::uint32_t n = sh::Noise();
    if ((n & 1) == 0) return;
    static const unsigned short kEnter[] = {0x2D, 0x31, 0x34, 0x41, 0x44, 0x45, 0x4E, 0x4F, 0x50, 0x51};
    static const unsigned short kStep[] = {0x31, 0x34, 0x43, 0x44, 0x4E};
    if (g_k == kEnterArea) SetW(at::kArea, kEnter[(n >> 8) % 10]);
    else SetW(at::kArea, kStep[(n >> 8) % 5]);
}

}  // namespace

void SelfTest() {
    g_sc5_enter_copy = CopyWithTramps("Scena05_EnterArea", 0x5463A0, 0x708, kCalls5463A0, SH_N(kCalls5463A0),
                                      kTables5463A0, SH_N(kTables5463A0));
    g_sc5_run16_copy = CopyWithTramps("Scena05_Run16", 0x547920, 0x1BD0, kCalls547920, SH_N(kCalls547920),
                                      kTables547920, SH_N(kTables547920));
    sh::Group group = {"scena_sc5",
                       kClones,
                       kCount,
                       kCallees,
                       sizeof kCallees / sizeof kCallees[0],
                       kTables,
                       sizeof kTables / sizeof kTables[0],
                       kRegions,
                       sizeof kRegions / sizeof kRegions[0],
                       &Seed,
                       &Disturb,
                       8000};
    group.args = &Args;
    group.settle = &Settle;
    group.chapter = 5;
    sh::Run(group);
}

}  // namespace scena_sc5
#undef SH_N
