// BOF3X_SHADOW=scena_sc12: chapter 12's first block through the scenario
// harness (scenario_harness.h), once at start-up. docs/scena_sc12.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC12 --clones, checked
// against a capstone reading of every function), each clone's call shape in
// its comment; the callees (all listed here, so each is recorded as this
// group needs whatever the standard set holds); the four chapter tables the
// dispatchers read in place; the regions; a seed per role; a disturbance of
// the chapter's cells.
//
// STAGE A: written against the harness contract (magic_harness's API under
// scenario_harness / SH_, plus the Clone's `shape` that
// tools/scenario_rows.py prints) before SCH's harness exists; stage B
// builds it against the harness and adjusts what the real one names
// differently.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/scena_sc12.h"
#include "game/scena_sc12_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc12 {
namespace {

namespace sh = scenario_harness;

// tools/scenario_rows.py --unit SC12 --clones (commit 222eb0f's tool),
// 2026-09-27: every jump internal, nothing REFUSED. Each function's call
// shape after its name.
// 0x55E4E0 Scena12_Frame: 0xE bytes; +0x7 note: jmp through .data 0x6616f4, 3 code entries (a data_tables entry); root: chapter 12 slot 0, the frame (Field_ModeDispatch); shape: vtable slot 0 (the frame): no arguments
// 0x55E4F0 Scena12_EnterArea: 0x58C bytes; shape: state handler (Scena12_States entry 1): no arguments
constexpr sh::CallSite kCalls55E4F0[] = {{0x50, 0x57C7A0}, {0xE1, 0x5725F0}, {0x146, 0x57C140}, {0x15B, 0x57C140}, {0x170, 0x57C7C0}, {0x194, 0x589810}, {0x1E8, 0x5341C0}, {0x1F3, 0x5341C0}, {0x1FE, 0x5341C0}, {0x209, 0x5341C0}, {0x210, 0x5341A0}, {0x22C, 0x57C140}, {0x24C, 0x57C0F0}, {0x254, 0x57C7C0}, {0x29B, 0x589810}, {0x2E5, 0x589810}, {0x315, 0x5725F0}, {0x328, 0x5341C0}, {0x360, 0x57C140}, {0x370, 0x589810}, {0x3A2, 0x589810}, {0x3D0, 0x587A20}, {0x3D8, 0x454810}, {0x3E3, 0x5A9949}, {0x3EB, 0x454810}, {0x40C, 0x5725F0}, {0x425, 0x5725F0}, {0x43B, 0x5341C0}, {0x442, 0x5341C0}, {0x449, 0x5341A0}, {0x45D, 0x589810}, {0x48C, 0x589810}, {0x4BC, 0x587A20}, {0x4C4, 0x454810}, {0x4CF, 0x5A9949}, {0x4D7, 0x454810}, {0x4E7, 0x587AE0}};
constexpr sh::JumpTable kTables55E4F0[] = {{0x353, 0x568, 9}, {0x1E2, 0x548, 5}};
// 0x55EA80 Scena12_Run: 0xE bytes; +0x7 note: jmp through .data 0x661700, 10 code entries (a data_tables entry); shape: state handler (Scena12_States entry 2): no arguments
// 0x55EA90 Scena12_Run1: 0x4B bytes; shape: state handler (Scena12_Runs entry 1): no arguments
constexpr sh::CallSite kCalls55EA90[] = {{0x1F, 0x57C7A0}};
// 0x55EAE0 Scena12_Run2: 0x4C bytes; shape: state handler (Scena12_Runs entry 2): no arguments
constexpr sh::CallSite kCalls55EAE0[] = {{0x21, 0x57C7A0}, {0x35, 0x4976D0}};
// 0x55EB30 Scena12_Run4: 0xA90 bytes; shape: state handler (Scena12_Runs entry 4): no arguments
constexpr sh::CallSite kCalls55EB30[] = {{0x49, 0x594E00}, {0x55, 0x5734F0}, {0x92, 0x57C0F0}, {0x9E, 0x57C0F0}, {0xA6, 0x57C7A0}, {0xB2, 0x4976D0}, {0xD7, 0x57C7A0}, {0x104, 0x531F90}, {0x140, 0x57C0F0}, {0x156, 0x594E00}, {0x177, 0x4976D0}, {0x253, 0x57C0F0}, {0x26C, 0x594E00}, {0x278, 0x587AE0}, {0x291, 0x495040}, {0x2C7, 0x4976D0}, {0x319, 0x57C0F0}, {0x321, 0x533E50}, {0x34A, 0x57C0F0}, {0x360, 0x57C0F0}, {0x375, 0x57C0F0}, {0x38B, 0x57C0F0}, {0x3A1, 0x57C0F0}, {0x3B6, 0x57C0F0}, {0x3CC, 0x57C0F0}, {0x3E2, 0x57C0F0}, {0x3F7, 0x57C0F0}, {0x40D, 0x57C0F0}, {0x45F, 0x57C0F0}, {0x46D, 0x57C110}, {0x47B, 0x57C110}, {0x488, 0x57C110}, {0x496, 0x57C110}, {0x4A4, 0x57C110}, {0x4B1, 0x57C110}, {0x4BF, 0x57C110}, {0x4D0, 0x57C110}, {0x4E5, 0x57C0F0}, {0x4F3, 0x57C110}, {0x500, 0x57C110}, {0x50E, 0x57C110}, {0x51C, 0x57C110}, {0x529, 0x57C110}, {0x537, 0x57C110}, {0x54C, 0x57C0F0}, {0x559, 0x57C110}, {0x567, 0x57C110}, {0x575, 0x57C110}, {0x582, 0x57C110}, {0x590, 0x57C110}, {0x59E, 0x57C110}, {0x5AB, 0x57C110}, {0x5BC, 0x57C110}, {0x5D7, 0x57C0F0}, {0x5E5, 0x57C110}, {0x5F3, 0x57C110}, {0x600, 0x57C110}, {0x60E, 0x57C110}, {0x61C, 0x57C110}, {0x629, 0x57C110}, {0x637, 0x57C110}, {0x648, 0x57C110}, {0x65D, 0x57C0F0}, {0x66B, 0x57C110}, {0x678, 0x57C110}, {0x686, 0x57C110}, {0x694, 0x57C110}, {0x6A1, 0x57C110}, {0x6AF, 0x57C110}, {0x6C4, 0x57C0F0}, {0x6D1, 0x57C110}, {0x6DF, 0x57C110}, {0x6ED, 0x57C110}, {0x6FA, 0x57C110}, {0x708, 0x57C110}, {0x716, 0x57C110}, {0x723, 0x57C110}, {0x734, 0x57C110}, {0x74F, 0x57C0F0}, {0x75D, 0x57C110}, {0x76B, 0x57C110}, {0x778, 0x57C110}, {0x786, 0x57C110}, {0x794, 0x57C110}, {0x7A1, 0x57C110}, {0x7AF, 0x57C110}, {0x7C0, 0x57C110}, {0x7D5, 0x57C0F0}, {0x7E3, 0x57C110}, {0x7F0, 0x57C110}, {0x7FE, 0x57C110}, {0x80C, 0x57C110}, {0x819, 0x57C110}, {0x827, 0x57C110}, {0x835, 0x57C110}, {0x845, 0x57C110}, {0x861, 0x57C0F0}, {0x86E, 0x57C110}, {0x87C, 0x57C110}, {0x88A, 0x57C110}, {0x897, 0x57C110}, {0x8A5, 0x57C110}, {0x8B3, 0x57C110}, {0x8C0, 0x57C110}, {0x8D1, 0x57C110}, {0x8EC, 0x57C0F0}, {0x8FA, 0x57C110}, {0x908, 0x57C110}, {0x915, 0x57C110}, {0x923, 0x57C110}, {0x931, 0x57C110}, {0x93E, 0x57C110}, {0x94C, 0x57C110}, {0x95D, 0x57C110}, {0x96A, 0x57C110}, {0x986, 0x594E00}, {0x997, 0x57C7A0}};
constexpr sh::JumpTable kTables55EB30[] = {{0x1C, 0x9C4, 17}, {0x453, 0xA60, 12}, {0x33D, 0xA30, 12}};
// 0x55F5C0 Scena12_Run5: 0x7A8 bytes; shape: state handler (Scena12_Runs entry 5): no arguments
constexpr sh::CallSite kCalls55F5C0[] = {{0x27, 0x4976D0}, {0x73, 0x589810}, {0x9D, 0x4976D0}, {0xCB, 0x57C0F0}, {0xEF, 0x57C7A0}, {0x10C, 0x531F90}, {0x14D, 0x594E00}, {0x15A, 0x5734F0}, {0x196, 0x57C0F0}, {0x1AF, 0x594E00}, {0x1D0, 0x57C0F0}, {0x1ED, 0x57C7A0}, {0x1FE, 0x589810}, {0x259, 0x531F90}, {0x267, 0x57C0F0}, {0x27B, 0x531F90}, {0x2CF, 0x57C7A0}, {0x2E7, 0x57C0F0}, {0x2F4, 0x57C110}, {0x2FC, 0x589810}, {0x353, 0x531F90}, {0x35F, 0x531F90}, {0x36C, 0x531F90}, {0x3B4, 0x594E00}, {0x3C1, 0x4976D0}, {0x3E7, 0x57C7A0}, {0x429, 0x57C0F0}, {0x46F, 0x594E00}, {0x494, 0x594E00}, {0x4BA, 0x594E00}, {0x4E0, 0x594E00}, {0x516, 0x57C0F0}, {0x548, 0x594E00}, {0x567, 0x594E00}, {0x57B, 0x5734F0}, {0x59C, 0x495040}, {0x5F5, 0x5341C0}, {0x5FB, 0x5341C0}, {0x602, 0x5341A0}, {0x60F, 0x5341C0}, {0x616, 0x5341A0}, {0x623, 0x5341C0}, {0x62A, 0x5341A0}, {0x637, 0x5341C0}, {0x63E, 0x5341C0}, {0x645, 0x5341A0}, {0x651, 0x5341C0}, {0x658, 0x5341A0}, {0x664, 0x5341C0}, {0x66B, 0x5341A0}, {0x67B, 0x5341C0}, {0x682, 0x5341A0}, {0x68F, 0x5341C0}, {0x696, 0x5341A0}};
constexpr sh::JumpTable kTables55F5C0[] = {{0x1D, 0x6CC, 23}, {0x5EF, 0x778, 12}, {0x451, 0x758, 5}};
// 0x55FD70 Scena12_Run6: 0x688 bytes; shape: state handler (Scena12_Runs entry 6): no arguments
constexpr sh::CallSite kCalls55FD70[] = {{0x49, 0x594E00}, {0x63, 0x5734F0}, {0x89, 0x57C0F0}, {0xAC, 0x57C0F0}, {0xBF, 0x594E00}, {0xD8, 0x589810}, {0x1A7, 0x594E00}, {0x1FC, 0x594E00}, {0x20F, 0x5734F0}, {0x264, 0x594E00}, {0x277, 0x5734F0}, {0x2D4, 0x594E00}, {0x331, 0x594E00}, {0x354, 0x587740}, {0x397, 0x594E00}, {0x3AA, 0x5734F0}, {0x407, 0x594E00}, {0x45C, 0x594E00}, {0x4B0, 0x594E00}, {0x505, 0x594E00}, {0x523, 0x589810}, {0x56F, 0x587B40}, {0x576, 0x495040}, {0x5CA, 0x57C0F0}, {0x5E3, 0x594E00}, {0x619, 0x57C7A0}};
constexpr sh::JumpTable kTables55FD70[] = {{0x14, 0x62C, 23}};
// 0x560400 Scena12_Run7: 0x4D4 bytes; shape: state handler (Scena12_Runs entry 7): no arguments
constexpr sh::CallSite kCalls560400[] = {{0x3A, 0x594E00}, {0x67, 0x57C0F0}, {0x76, 0x57C7A0}, {0x8A, 0x4976D0}, {0xAE, 0x57C7A0}, {0xC7, 0x57C7C0}, {0xD5, 0x531F90}, {0x122, 0x594E00}, {0x14B, 0x587740}, {0x157, 0x587AE0}, {0x188, 0x57C0F0}, {0x1A8, 0x594E00}, {0x1BA, 0x5734F0}, {0x205, 0x594E00}, {0x224, 0x495040}, {0x252, 0x4976D0}, {0x2A4, 0x587B40}, {0x2B2, 0x57C0F0}, {0x2BA, 0x533E50}, {0x32D, 0x57C0F0}, {0x350, 0x594E00}, {0x385, 0x579F00}, {0x393, 0x579F00}, {0x3A1, 0x579F00}, {0x3AF, 0x579F00}, {0x3BD, 0x579F00}, {0x3CB, 0x579F00}, {0x3DC, 0x579F00}, {0x3EA, 0x579F00}, {0x3F8, 0x579F00}, {0x406, 0x579F00}, {0x416, 0x57C7A0}};
constexpr sh::JumpTable kTables560400[] = {{0x1B, 0x43C, 18}, {0x2D3, 0x4A4, 12}};
// 0x5608E0 Scena12_Run8: 0x884 bytes; shape: state handler (Scena12_Runs entry 8): no arguments
constexpr sh::CallSite kCalls5608E0[] = {{0x60, 0x4976D0}, {0x93, 0x4976D0}, {0xD8, 0x495040}, {0xDD, 0x533E50}, {0xE4, 0x587B40}, {0xF6, 0x57C7A0}, {0x113, 0x587910}, {0x139, 0x587A00}, {0x151, 0x495040}, {0x17F, 0x587AE0}, {0x187, 0x57C7A0}, {0x1A7, 0x57C7A0}, {0x20F, 0x57C0F0}, {0x21D, 0x57C110}, {0x22B, 0x57C110}, {0x238, 0x57C110}, {0x246, 0x57C110}, {0x254, 0x57C110}, {0x261, 0x57C110}, {0x26F, 0x57C110}, {0x280, 0x57C110}, {0x295, 0x57C0F0}, {0x2A3, 0x57C110}, {0x2B0, 0x57C110}, {0x2BE, 0x57C110}, {0x2CC, 0x57C110}, {0x2D9, 0x57C110}, {0x2E7, 0x57C110}, {0x2FC, 0x57C0F0}, {0x309, 0x57C110}, {0x317, 0x57C110}, {0x325, 0x57C110}, {0x332, 0x57C110}, {0x340, 0x57C110}, {0x34E, 0x57C110}, {0x35B, 0x57C110}, {0x36C, 0x57C110}, {0x387, 0x57C0F0}, {0x395, 0x57C110}, {0x3A3, 0x57C110}, {0x3B0, 0x57C110}, {0x3BE, 0x57C110}, {0x3CC, 0x57C110}, {0x3D9, 0x57C110}, {0x3E7, 0x57C110}, {0x3F8, 0x57C110}, {0x40D, 0x57C0F0}, {0x41B, 0x57C110}, {0x428, 0x57C110}, {0x436, 0x57C110}, {0x444, 0x57C110}, {0x451, 0x57C110}, {0x45F, 0x57C110}, {0x474, 0x57C0F0}, {0x481, 0x57C110}, {0x48F, 0x57C110}, {0x49D, 0x57C110}, {0x4AA, 0x57C110}, {0x4B8, 0x57C110}, {0x4C6, 0x57C110}, {0x4D3, 0x57C110}, {0x4E4, 0x57C110}, {0x4FF, 0x57C0F0}, {0x50D, 0x57C110}, {0x51B, 0x57C110}, {0x528, 0x57C110}, {0x536, 0x57C110}, {0x544, 0x57C110}, {0x551, 0x57C110}, {0x55F, 0x57C110}, {0x570, 0x57C110}, {0x585, 0x57C0F0}, {0x593, 0x57C110}, {0x5A0, 0x57C110}, {0x5AE, 0x57C110}, {0x5BC, 0x57C110}, {0x5C9, 0x57C110}, {0x5D7, 0x57C110}, {0x5E5, 0x57C110}, {0x5F5, 0x57C110}, {0x611, 0x57C0F0}, {0x61E, 0x57C110}, {0x62C, 0x57C110}, {0x63A, 0x57C110}, {0x647, 0x57C110}, {0x655, 0x57C110}, {0x663, 0x57C110}, {0x670, 0x57C110}, {0x681, 0x57C110}, {0x69C, 0x57C0F0}, {0x6AA, 0x57C110}, {0x6B8, 0x57C110}, {0x6C5, 0x57C110}, {0x6D3, 0x57C110}, {0x6E1, 0x57C110}, {0x6EE, 0x57C110}, {0x6FC, 0x57C110}, {0x70D, 0x57C110}, {0x71A, 0x57C110}, {0x736, 0x594E00}, {0x75B, 0x4976D0}, {0x795, 0x57C7A0}};
constexpr sh::JumpTable kTables5608E0[] = {{0x1C, 0x7C4, 13}, {0x203, 0x854, 12}, {0x5A, 0x81C, 7}, {0x8D, 0x838, 7}};
// 0x561170 Scena12_Run9: 0x708 bytes; shape: state handler (Scena12_Runs entry 9): no arguments
constexpr sh::CallSite kCalls561170[] = {{0x2F, 0x531F90}, {0x56, 0x57C0F0}, {0x5E, 0x57C7A0}, {0x76, 0x57C7A0}, {0x9F, 0x587B40}, {0xAB, 0x587AE0}, {0xB2, 0x531F90}, {0xB9, 0x5734F0}, {0xD7, 0x589810}, {0x144, 0x531F90}, {0x14C, 0x589810}, {0x207, 0x4976D0}, {0x224, 0x4976D0}, {0x241, 0x4976D0}, {0x25E, 0x4976D0}, {0x2C3, 0x589810}, {0x2ED, 0x589810}, {0x315, 0x589810}, {0x39C, 0x531F90}, {0x3A3, 0x5734F0}, {0x3C3, 0x531F90}, {0x3CA, 0x5734F0}, {0x3F8, 0x57C0F0}, {0x409, 0x532ED0}, {0x429, 0x4410B0}, {0x442, 0x531F90}, {0x472, 0x532ED0}, {0x479, 0x4410B0}, {0x4A0, 0x57C0F0}, {0x4B0, 0x57C7A0}, {0x4DB, 0x5734F0}, {0x4E9, 0x57C0F0}, {0x509, 0x589810}, {0x575, 0x495040}, {0x593, 0x589810}, {0x60F, 0x4976D0}, {0x64B, 0x57C0F0}, {0x650, 0x56D6F0}, {0x663, 0x594E00}};
constexpr sh::JumpTable kTables561170[] = {{0x1C, 0x670, 26}};
// 0x561880 Scena12_ObjectTrigger: 0x1F bytes; +0x14 note: call through .data 0x661728, 15 code entries (a data_tables entry); root: chapter 12 slot 1, the object trigger (0x56D6D0, the object); shape: vtable slot 1 (the object trigger): the object
// 0x5618A0 Scena12_Object06: 0x23 bytes; shape: state handler (Scena12_Objects entry 6): (object, bits) ignored
constexpr sh::CallSite kCalls5618A0[] = {{0x0, 0x57C7C0}};
// 0x5618D0 Scena12_Object09: 0x25 bytes; shape: state handler (Scena12_Objects entry 9): (object, bits) ignored
constexpr sh::CallSite kCalls5618D0[] = {{0x0, 0x57C7C0}};
// 0x561900 Scena12_Object10: 0x25 bytes; shape: state handler (Scena12_Objects entry 10): (object, bits) ignored
constexpr sh::CallSite kCalls561900[] = {{0x0, 0x57C7C0}};
// 0x561930 Scena12_Object11: 0x2A bytes; shape: state handler (Scena12_Objects entry 11): (object, bits) ignored
constexpr sh::CallSite kCalls561930[] = {{0x0, 0x57C7C0}};
// 0x561960 Scena12_Object12: 0x23 bytes; shape: state handler (Scena12_Objects entry 12): (object, bits) ignored
constexpr sh::CallSite kCalls561960[] = {{0x0, 0x57C7C0}};
// 0x561990 Scena12_Object13: 0x25 bytes; shape: state handler (Scena12_Objects entry 13): (object, bits) ignored
constexpr sh::CallSite kCalls561990[] = {{0x0, 0x57C7C0}};
// 0x5619C0 Scena12_Object14: 0x14 bytes; shape: state handler (Scena12_Objects entry 14): (object, bits) ignored
constexpr sh::CallSite kCalls5619C0[] = {{0x0, 0x57C7C0}};
// 0x5619E0 Scena12_ArriveHook: 0x41 bytes; root: chapter 12 slot 3, the arrive hook (x, z), al; shape: vtable slot 3, hook (x, z) -> al
constexpr sh::CallSite kCalls5619E0[] = {{0x12, 0x57C140}, {0x28, 0x57C7C0}};
// 0x561A30 Scena12_StepHook: 0x2B0 bytes; root: chapter 12 slot 2, the step hook (x, z), al; shape: vtable slot 2, hook (x, z) -> al
constexpr sh::CallSite kCalls561A30[] = {{0x21, 0x57C140}, {0x61, 0x57C7C0}, {0x90, 0x579F00}, {0x9B, 0x579F00}, {0xA6, 0x579F00}, {0xB1, 0x579F00}, {0xBF, 0x57C0F0}, {0xF8, 0x57C7C0}, {0x13A, 0x57C140}, {0x166, 0x57C7C0}, {0x198, 0x57C140}, {0x1AD, 0x57C140}, {0x1CE, 0x57C7C0}, {0x1FD, 0x57C140}, {0x24D, 0x57C7C0}, {0x28C, 0x57C7C0}};
// 0x561CE0 Scena12_CellHook: 0x2A bytes; +0x23 note: jmp through .data 0x661778, 8 code entries (a data_tables entry); root: chapter 12 slot 4, the cell hook (x, z), al; shape: vtable slot 4, hook (a, b) -> al (sign-extended by 0x56D7A0)
constexpr sh::CallSite kCalls561CE0[] = {{0x11, 0x56D800}};
// 0x561D10 Scena12_CellTalk: 0x3B bytes; shape: Scena12_CellHooks entries 0..2, (a, b) in place -> al
constexpr sh::CallSite kCalls561D10[] = {{0x0, 0x57C7C0}};
// 0x561D50 Scena12_CellDoor: 0x5D bytes; shape: Scena12_CellHooks entry 3, (a, b) in place -> al
constexpr sh::CallSite kCalls561D50[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}, {0x45, 0x572650}, {0x4F, 0x587740}};
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const sh::Clone kClones[] = {
    {"Scena12_Frame", 0x55E4E0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Frame), 0, false, sh::Shape::kSlot},
    {"Scena12_EnterArea", 0x55E4F0, 0x58C, kCalls55E4F0, SH_N(kCalls55E4F0), nullptr, 0, kTables55E4F0, SH_N(kTables55E4F0), reinterpret_cast<const void*>(&::Scena12_EnterArea)},
    {"Scena12_Run", 0x55EA80, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Run)},
    {"Scena12_Run1", 0x55EA90, 0x4B, kCalls55EA90, SH_N(kCalls55EA90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Run1)},
    {"Scena12_Run2", 0x55EAE0, 0x4C, kCalls55EAE0, SH_N(kCalls55EAE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Run2)},
    {"Scena12_Run4", 0x55EB30, 0xA90, kCalls55EB30, SH_N(kCalls55EB30), nullptr, 0, kTables55EB30, SH_N(kTables55EB30), reinterpret_cast<const void*>(&::Scena12_Run4)},
    {"Scena12_Run5", 0x55F5C0, 0x7A8, kCalls55F5C0, SH_N(kCalls55F5C0), nullptr, 0, kTables55F5C0, SH_N(kTables55F5C0), reinterpret_cast<const void*>(&::Scena12_Run5)},
    {"Scena12_Run6", 0x55FD70, 0x688, kCalls55FD70, SH_N(kCalls55FD70), nullptr, 0, kTables55FD70, SH_N(kTables55FD70), reinterpret_cast<const void*>(&::Scena12_Run6)},
    {"Scena12_Run7", 0x560400, 0x4D4, kCalls560400, SH_N(kCalls560400), nullptr, 0, kTables560400, SH_N(kTables560400), reinterpret_cast<const void*>(&::Scena12_Run7)},
    {"Scena12_Run8", 0x5608E0, 0x884, kCalls5608E0, SH_N(kCalls5608E0), nullptr, 0, kTables5608E0, SH_N(kTables5608E0), reinterpret_cast<const void*>(&::Scena12_Run8)},
    {"Scena12_Run9", 0x561170, 0x708, kCalls561170, SH_N(kCalls561170), nullptr, 0, kTables561170, SH_N(kTables561170), reinterpret_cast<const void*>(&::Scena12_Run9)},
    {"Scena12_ObjectTrigger", 0x561880, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_ObjectTrigger), 0, false, sh::Shape::kObject},
    {"Scena12_Object06", 0x5618A0, 0x23, kCalls5618A0, SH_N(kCalls5618A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Object06)},
    {"Scena12_Object09", 0x5618D0, 0x25, kCalls5618D0, SH_N(kCalls5618D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Object09)},
    {"Scena12_Object10", 0x561900, 0x25, kCalls561900, SH_N(kCalls561900), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Object10)},
    {"Scena12_Object11", 0x561930, 0x2A, kCalls561930, SH_N(kCalls561930), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Object11)},
    {"Scena12_Object12", 0x561960, 0x23, kCalls561960, SH_N(kCalls561960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Object12)},
    {"Scena12_Object13", 0x561990, 0x25, kCalls561990, SH_N(kCalls561990), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Object13)},
    {"Scena12_Object14", 0x5619C0, 0x14, kCalls5619C0, SH_N(kCalls5619C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_Object14)},
    {"Scena12_ArriveHook", 0x5619E0, 0x41, kCalls5619E0, SH_N(kCalls5619E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_ArriveHook), 0xFF, false, sh::Shape::kHook},
    {"Scena12_StepHook", 0x561A30, 0x2B0, kCalls561A30, SH_N(kCalls561A30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_StepHook), 0xFF, false, sh::Shape::kHook},
    {"Scena12_CellHook", 0x561CE0, 0x2A, kCalls561CE0, SH_N(kCalls561CE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_CellHook), 0xFF, false, sh::Shape::kHook},
    {"Scena12_CellTalk", 0x561D10, 0x3B, kCalls561D10, SH_N(kCalls561D10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_CellTalk), 0xFF, false, sh::Shape::kHook},
    {"Scena12_CellDoor", 0x561D50, 0x5D, kCalls561D50, SH_N(kCalls561D50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena12_CellDoor), 0xFF, false, sh::Shape::kHook},
};
#undef SH_N
enum : unsigned {
    kFrame, kEnterArea, kRun, kRun1, kRun2, kRun4, kRun5, kRun6, kRun7, kRun8, kRun9, kObjectTrigger,
    kObject06, kObject09, kObject10, kObject11, kObject12, kObject13, kObject14,
    kArriveHook, kStepHook, kCellHook, kCellTalk, kCellDoor, kCount
};
static_assert(sizeof kClones / sizeof kClones[0] == kCount, "one role per clone");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// A callee ours reaches by its name (Capcom's address it displaced, our
// function), and one ours reaches by a raw address (scena_sc12_callees.h).
#define SC12_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define SC12_RAW(name, address) name, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
const sh::Callee kCallees[] = {
    // the flags: the bits pointer (0x929ED0's value, or 0x904030) and the index
    {SC12_OURS(Flags_Test), 2, {kAll, kAll}, sh::Answer::kFlag, 0, 0},
    {SC12_OURS(Flags_Set), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Flags_Clear), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(ScriptFlags_Set40), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(ScriptFlags_Clear40), 0, {}, sh::Answer::kGarbage, 0, 0},
    // the scene's calls
    {SC12_OURS(Msg_OpenScript), 1, {kU16}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Field_ChangeArea), 4, {kAll, kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Scenario_CallA), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Effect_FindFree), 0, {}, sh::Answer::kByte, 0xFF, 0x13},   // none, or one of the 20
    {SC12_OURS(MapView_SetElevation), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Music_LoadFile), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(File_LoadDone), 0, {}, sh::Answer::kBool, 0, 0},
    {SC12_OURS(Task_Sleep), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Music_Play), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Music_FadeOutStop), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Kind2_Place), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Party_DropIn), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Transition_Start), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Sound_PlayEffect), 1, {kU16}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Sound_LoadStream), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(Sound_StreamDone), 0, {}, sh::Answer::kBool, 0, 0},
    {SC12_OURS(AreaMap_SetByte), 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC12_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, sh::Answer::kGarbage, 0, 0},
    // by raw address (nobody's, or group SE's this wave)
    {SC12_RAW("0x5341C0 Scenario_CallB", at::kCallB), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC12_RAW("0x533E50", at::kPartyPass), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC12_RAW("0x532ED0", at::kPartyPlace), 3, {kAll, kAll, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC12_RAW("0x56D6F0", at::kSetBit80), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC12_RAW("0x56D800", at::kCellFind), 4, {kAll, kU8, kU8, kU8}, sh::Answer::kByte, 0xFF, 3},   // none, or record 0..3
    {SC12_RAW("0x4410B0", at::kBattleBytes), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
};
#undef SC12_OURS
#undef SC12_RAW

// The chapter's tables, read in place by the dispatchers: swapped for recorders.
const sh::DataTable kTables[] = {
    {at::kStates, at::kStateCount}, {at::kRuns, at::kRunCount}, {at::kObjects, at::kObjectCount},
    {at::kCellHooks, at::kCellHookCount},
};

// Cond_Flags (0x903F90..): the story flags 0x904030, the selector 0x90412C,
// the three bytes 0x904062.
constexpr std::uint32_t kCondFlags = 0x903F90;
constexpr std::uint32_t kCondFlagsSize = 0x200;

alignas(16) unsigned char g_sprite[0x100];   // Sprite_Current, for Scena12_CellDoor
alignas(16) unsigned char g_object[0x100];   // the object slot 1 is given

sh::Region g_regions[32];
unsigned g_n_regions;

void AddRegion(std::uint32_t at, std::uint32_t size) {
    if (g_n_regions >= sizeof g_regions / sizeof g_regions[0]) bof3::Fatal("scena_sc12: too many regions");
    g_regions[g_n_regions++] = {at, size};
}

void BuildRegions() {
    g_n_regions = 0;
    AddRegion(0x8034E0, 0x14);              // the scenario bytes, Cond_ByteFD
    AddRegion(at::kCounters, 0xC);          // the counters, 0x90384C.., the effect slot byte
    AddRegion(kCondFlags, kCondFlagsSize);
    AddRegion(at::kFlagBits, 4);
    AddRegion(at::kArea, 2);
    AddRegion(at::kPassFlags, 1);
    AddRegion(at::kScriptFlags, 2);
    AddRegion(at::kRequest, 1);
    AddRegion(at::kWait, 2);
    AddRegion(at::kRedraw, 1);
    AddRegion(at::kMusicCurrent, 1);
    AddRegion(at::kCondFE, 1);
    AddRegion(at::kEffects, 20 * at::kEffectStride);
    AddRegion(at::kAngleX, 4);
    AddRegion(at::kClut, at::kClutWords * 2);
    AddRegion(at::kClutDirty, 1);
    AddRegion(at::kTile, 2);
    AddRegion(at::kHold, 1);
    AddRegion(at::kLoadByte, 1);
    AddRegion(0x802D40, 0x14C);             // ObjTrio record 0: +0x34, +0x3C, +0x89
    AddRegion(at::kMember1, 4);
    AddRegion(at::kMemberState, 1);
    AddRegion(at::kInputHeld, 2);
    AddRegion(Key(g_sprite), sizeof g_sprite);
    AddRegion(Key(g_object), sizeof g_object);
}

unsigned char& B(std::uint32_t a) { return *sh::Mem(a); }
void SetW(std::uint32_t a, std::uint32_t v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(sh::Mem(a), &w, 2);
}
void SetD(std::uint32_t a, std::uint32_t v) { std::memcpy(sh::Mem(a), &v, 4); }

// The values the chapter's code compares with.
std::uint32_t AnArea() { return SH_PICK(0x65, 0x79, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0xBC, 0x10, 0x2D, 0x41, 0x57, 0x73, 0x7E); }
std::uint32_t ACount() { return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 9, 0xA, 0xE, 0x14, 0x18, 0x19, 0x23, 0x28, 0x32, 0x80, 0xFF); }
std::uint32_t ASelector() { return (7 + sh::Next() % 12) | (sh::Half() ? 0x80 : 0); }

// Each run's steps (the cases its switch holds), and one past.
std::uint32_t AStep(unsigned k) {
    switch (k) {
    case kRun1: return SH_PICK(0xA, 0xB, 0xC);
    case kRun2: return SH_PICK(0x32, 0x33, 0x3C, 0x34);
    case kRun4: return SH_PICK(0, 1, 2, 3, 0xA, 0xB, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x27, 0x28);
    case kRun5: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1B, 0x1C, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E);
    case kRun6: return sh::Next() % 0x18;
    case kRun7: return SH_PICK(0, 1, 5, 6, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x1D, 0x1E, 0x1F);
    case kRun8: return SH_PICK(0, 1, 2, 3, 4, 5, 0xA, 0x14, 0x15, 0x1E, 0x1F, 0x20, 0x21);
    case kRun9: return SH_PICK(0, 1, 2, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x1E, 0x1F, 0x20, 0x21,
                               0x22, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30);
    default: return sh::Next();
    }
}

void Seed(unsigned k) {
    // Every round: the flag bits' pointer into Cond_Flags, Sprite_Current at
    // the fuzz's record, the cells the chapter compares, each most of the
    // time at a value a branch tests.
    SetD(at::kFlagBits, kCondFlags + 8 * 12);
    Sprite_Current = g_sprite;
    if (sh::Often()) SetW(at::kArea, AnArea());
    for (unsigned c = 0; c < 3; ++c)
        if (sh::Often()) B(at::kCounters + c) = static_cast<unsigned char>(ACount());
    B(at::kCounters + 3) = static_cast<unsigned char>(sh::Next() % 20);
    if (sh::Often()) B(at::kRequest) = static_cast<unsigned char>(SH_PICK(0, 2, 6));
    if (sh::Half()) SetW(at::kWait, 0);
    if (sh::Often()) B(at::kSelector) = static_cast<unsigned char>(ASelector());
    if (sh::Often()) B(at::kCondFD) = static_cast<unsigned char>(SH_PICK(2, 3));
    if (sh::Often()) B(at::kLeaderName) = static_cast<unsigned char>(2 + sh::Next() % 7);
    if (sh::Half()) B(at::kLoadByte) = 1;
    if (sh::Half()) B(at::kHold) = 0;
    if (sh::Half()) B(at::kMemberState) = 3;
    if (sh::Often()) SetW(at::kInputHeld, SH_PICK(0x2000, 0x3000, 0x6000, 0x1000));
    if (sh::Often()) B(at::kTile) = static_cast<unsigned char>(0x60 + sh::Next() % 9);
    for (unsigned i = 0; i < 3; ++i)
        if (sh::Often()) B(at::kPartyBytes + i) = static_cast<unsigned char>(SH_PICK(2, 8, 5, 4));
    if (sh::Often()) SetD(at::kObjTrioZ, SH_PICK(0x2FFFFF, 0x300000, 0x80000000u));
    if (sh::Often()) SetD(at::kObjTrioY, SH_PICK(0x2000000, 0x2000001, 0x80000000u));
    if (sh::Half()) B(at::kEffects + B(at::kCounters + 3) * at::kEffectStride) = 0;
    switch (k) {
    case kFrame: B(at::kState) = static_cast<unsigned char>(sh::Next() % 3); break;
    case kRun: B(at::kRun) = static_cast<unsigned char>(sh::Next() % 10); break;
    case kObjectTrigger: g_object[0x86] = static_cast<unsigned char>(sh::Next() % 15); break;
    case kRun1: case kRun2: case kRun4: case kRun5: case kRun6: case kRun7: case kRun8: case kRun9:
        B(at::kStep) = static_cast<unsigned char>(AStep(k));
        break;
    case kArriveHook:
        if (sh::Often()) SetW(at::kArea, 0x79);
        break;
    case kStepHook:
        if (sh::Often()) SetW(at::kArea, SH_PICK(0x85, 0x86, 0x88, 0xBC));
        break;
    default: break;
    }
}

// A coordinate at one of the hooks' bounds, one either side, or anything.
std::uint32_t AnX() {
    const std::uint32_t v = SH_PICK(0x160000, 0x1F8000, 0x208000, 0x2E0000, 0x2F8000, 0x330000, 0x348000, 0x1B0000,
                                    0x1C0000, 0x1D8000, 0x390000, 0x3A8000, 0x368000, 0x370000);
    return sh::Often() ? v + SH_PICK(0, 0, 1, 0xFFFFFFFFu) : sh::Next();
}
std::uint32_t AZ() {
    const std::uint32_t v = SH_PICK(0x248000, 0x270000, 0x290000, 0x298000, 0x2A0000, 0x38000, 0x48000, 0x470000,
                                    0x488000, 0x3F0000, 0x3F8000, 0x70000, 0x78000, 0xBFFFF, 0xC0000, 0x60000);
    return sh::Often() ? v + SH_PICK(0, 0, 1, 0xFFFFFFFFu) : sh::Next();
}

void Args(unsigned k, std::uint32_t* a) {
    switch (k) {
    case kObjectTrigger: a[0] = Key(g_object); break;
    case kArriveHook:
    case kStepHook:
        a[0] = AnX();
        a[1] = AZ();
        break;
    default: break;   // the cell hook's (a, b) random; the rest take none
    }
}

// After a call, two in three: move a cell the chapter reads again after its
// calls - a counter, the request byte, the wait word, the area, the selector,
// the step, an effect's in-use byte, the kind-2 hold.
void Disturb(std::uint32_t h) {
    const unsigned char v = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 8) {
    case 0: B(at::kCounters + (h >> 12) % 3) = v; break;
    case 1: B(at::kRequest) = static_cast<unsigned char>((h >> 16) % 3 == 0 ? 0 : v); break;
    case 2: SetW(at::kWait, (h >> 16) & 1 ? 0 : h >> 16); break;
    case 3: SetW(at::kArea, (h >> 16) & 1 ? 0xBC : 0x85 + (h >> 17) % 4); break;
    case 4: B(at::kSelector) = static_cast<unsigned char>(7 + (h >> 16) % 12); break;
    case 5: B(at::kStep) = v; break;
    case 6: B(at::kEffects + ((h >> 12) % 20) * at::kEffectStride) = static_cast<unsigned char>(v & 1); break;
    default: B(at::kHold) = static_cast<unsigned char>(v & 1); break;
    }
}

}  // namespace

void SelfTest() {
    BuildRegions();
    const sh::Group group = {"scena_sc12",
                             kClones,
                             kCount,
                             kCallees,
                             sizeof kCallees / sizeof kCallees[0],
                             kTables,
                             sizeof kTables / sizeof kTables[0],
                             g_regions,
                             g_n_regions,
                             &Seed,
                             &Disturb,
                             0,
                             nullptr,
                             0,
                             &Args};
    sh::Run(group);
}

}  // namespace scena_sc12
