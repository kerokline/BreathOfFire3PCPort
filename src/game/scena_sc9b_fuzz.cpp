// BOF3X_SHADOW=scena_sc9b: chapter 9's tail and chapter 10 through the
// scenario harness (scenario_harness.h), once at start-up, as two groups under
// the one shadow name - chapter 9's tail with the chapter byte 9, chapter 10
// with 10 (each block reads the flag row 0x929ED0, which Scenario_Start sets
// from the chapter; neither reads Cond_ByteFA itself). docs/scena_sc9b.md
// section 4.
//
// The clone table (tools/scenario_rows.py --unit SC9b --clones, checked
// against a capstone reading of every function), each clone's call shape in
// its comment; the callees the standard set lacks or records otherwise; the
// state and run tables swapped for recorders, and typed stand-ins written into
// the cell-hook and object tables; the regions beyond the standard ones; a
// seed per role; a disturbance of the chapter's cells.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc9b.h"
#include "game/scena_sc9b_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc9b {
namespace {

namespace sh = scenario_harness;

// tools/scenario_rows.py --unit SC9b --clones, 2026-09-28: every jump
// internal, nothing REFUSED. The tool's names are Fn_<address>; ours below.
//
// --- chapter 9's tail ---
// 0x557170 Scena09_Object07: 0x11 bytes; Scena09_Objects entry 7: (object, bits) unread
constexpr sh::CallSite kCalls557170[] = {{0x8, 0x57C0F0}};
// 0x557190 Scena09_Object08: 0x14 bytes
constexpr sh::CallSite kCalls557190[] = {{0x0, 0x57C7C0}};
// 0x5571B0 Scena09_Object09: 0x11 bytes
constexpr sh::CallSite kCalls5571B0[] = {{0x8, 0x57C0F0}};
// 0x5571D0 Scena09_Object10: 0x11 bytes
constexpr sh::CallSite kCalls5571D0[] = {{0x8, 0x57C0F0}};
// 0x5571F0 Scena09_Object11: 0x14 bytes
constexpr sh::CallSite kCalls5571F0[] = {{0x0, 0x57C7C0}};
// 0x557210 Scena09_Object13: 0x14 bytes
constexpr sh::CallSite kCalls557210[] = {{0x0, 0x57C7C0}};
// 0x557230 Scena09_Object14: 0x14 bytes
constexpr sh::CallSite kCalls557230[] = {{0x0, 0x57C7C0}};
// 0x557250 Scena09_Object15: 0x14 bytes
constexpr sh::CallSite kCalls557250[] = {{0x0, 0x57C7C0}};
// 0x557270 Scena09_StepHook: 0x7A6 bytes; root: chapter 9 slot 2, the step hook (x, z), al
constexpr sh::CallSite kCalls557270[] = {{0x23, 0x57C140}, {0x38, 0x57C140}, {0x64, 0x57C7C0}, {0xCD, 0x57C7C0}, {0x100, 0x57C140}, {0x118, 0x57C140}, {0x188, 0x57C7C0}, {0x1C1, 0x57C140}, {0x1D6, 0x57C140}, {0x202, 0x57C7C0}, {0x24B, 0x57C140}, {0x290, 0x57C0F0}, {0x298, 0x57C7C0}, {0x2D3, 0x57C140}, {0x2FF, 0x57C7C0}, {0x33A, 0x57C140}, {0x34F, 0x57C140}, {0x382, 0x57C7C0}, {0x3BB, 0x57C140}, {0x3D3, 0x57C140}, {0x40F, 0x57C7C0}, {0x44B, 0x57C140}, {0x477, 0x57C7C0}, {0x4C3, 0x57C140}, {0x4D7, 0x57C140}, {0x50C, 0x57C0F0}, {0x514, 0x57C7C0}, {0x550, 0x57C140}, {0x564, 0x57C140}, {0x590, 0x57C7C0}, {0x5CC, 0x57C140}, {0x600, 0x57C7C0}, {0x629, 0x57C140}, {0x655, 0x57C7C0}, {0x69C, 0x57C140}, {0x6B9, 0x57C140}, {0x6E0, 0x57C0F0}, {0x6E8, 0x57C7C0}, {0x743, 0x57C140}, {0x757, 0x57C140}, {0x76A, 0x57C7C0}, {0x77C, 0x57C140}, {0x788, 0x57C7C0}};
// 0x557A20 Scena09_CellHook: 0x2A bytes; +0x23 jmp through .data 0x6614D8, 14 entries (typed stand-ins); root: chapter 9 slot 4, (x, z), al
constexpr sh::CallSite kCalls557A20[] = {{0x11, 0x56D800}};
// 0x557A50..0x5580E0 Scena09_Cell00..Cell13: Scena09_CellHooks entries, reached by a tail jump, (x, z) unread, al
constexpr sh::CallSite kCalls557A50[] = {{0x11, 0x57C140}, {0x1D, 0x57C7C0}, {0x33, 0x57C7C0}, {0x49, 0x57C7C0}};
constexpr sh::CallSite kCalls557AB0[] = {{0x15, 0x57C140}, {0x21, 0x57C7C0}, {0x40, 0x57C140}, {0x4C, 0x57C7C0}, {0x6B, 0x57C140}, {0x77, 0x57C7C0}, {0x8D, 0x57C7C0}, {0xA3, 0x57C7C0}};
constexpr sh::CallSite kCalls557B70[] = {{0x15, 0x57C140}, {0x21, 0x57C7C0}, {0x40, 0x57C140}, {0x4C, 0x57C7C0}, {0x6B, 0x57C140}, {0x77, 0x57C7C0}, {0x8D, 0x57C7C0}, {0xA3, 0x57C7C0}};
constexpr sh::CallSite kCalls557C30[] = {{0x15, 0x57C140}, {0x21, 0x57C7C0}, {0x40, 0x57C140}, {0x4C, 0x57C7C0}, {0x6B, 0x57C140}, {0x77, 0x57C7C0}, {0x9C, 0x57C140}, {0xA8, 0x57C7C0}, {0xBE, 0x57C7C0}, {0xD4, 0x57C7C0}};
constexpr sh::CallSite kCalls557D20[] = {{0x15, 0x57C140}, {0x21, 0x57C7C0}, {0x40, 0x57C140}, {0x4C, 0x57C7C0}, {0x6B, 0x57C140}, {0x77, 0x57C7C0}, {0x95, 0x57C7C0}};
constexpr sh::CallSite kCalls557DD0[] = {{0x15, 0x57C140}, {0x21, 0x57C7C0}, {0x40, 0x57C140}, {0x4C, 0x57C7C0}, {0x6B, 0x57C140}, {0x77, 0x57C7C0}, {0x95, 0x57C7C0}};
constexpr sh::CallSite kCalls557E80[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}, {0x2A, 0x57C7C0}, {0x46, 0x57C0F0}};
constexpr sh::CallSite kCalls557EE0[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}, {0x2A, 0x57C7C0}, {0x46, 0x57C0F0}};
constexpr sh::CallSite kCalls557F40[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}, {0x2A, 0x57C7C0}, {0x46, 0x57C0F0}};
constexpr sh::CallSite kCalls557FA0[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}, {0x2A, 0x57C7C0}, {0x46, 0x57C0F0}};
constexpr sh::CallSite kCalls558000[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}, {0x2A, 0x57C7C0}, {0x46, 0x57C0F0}};
constexpr sh::CallSite kCalls558060[] = {{0x8, 0x57C140}, {0x1D, 0x57C0F0}, {0x27, 0x587740}, {0x32, 0x57C7C0}};
constexpr sh::CallSite kCalls5580B0[] = {{0x8, 0x57C140}, {0x14, 0x57C7C0}};
constexpr sh::CallSite kCalls5580E0[] = {{0x8, 0x57C140}, {0x24, 0x57C7C0}, {0x3A, 0x57C7C0}};
//
// --- chapter 10 ---
// 0x558140 Scena10_Frame: 0xE bytes; jmp through .data 0x661588, 3 entries (a data_tables entry); root: chapter 10 slot 0
// 0x558150 Scena10_EnterArea: 0x3FF bytes; Scena10_States entry 1
constexpr sh::CallSite kCalls558150[] = {{0x1B, 0x57C140}, {0x30, 0x57C110}, {0x4A, 0x57C140}, {0x5E, 0x57C140}, {0x6A, 0x558550}, {0x82, 0x57C140}, {0x97, 0x57C140}, {0xB2, 0x57C0F0}, {0xC3, 0x57C140}, {0xD5, 0x579F00}, {0xE0, 0x579F00}, {0x109, 0x57C140}, {0x11D, 0x57C140}, {0x12B, 0x5341A0}, {0x132, 0x531F90}, {0x156, 0x57C140}, {0x169, 0x57C0F0}, {0x183, 0x57C0F0}, {0x1A6, 0x57C140}, {0x1BF, 0x57C140}, {0x1D4, 0x57C140}, {0x1E1, 0x531F90}, {0x1F3, 0x57C140}, {0x20A, 0x531F90}, {0x22D, 0x57C140}, {0x23B, 0x5341A0}, {0x242, 0x531F90}, {0x266, 0x57C140}, {0x27A, 0x57C140}, {0x2A3, 0x5734F0}, {0x2D3, 0x57C140}, {0x2E8, 0x57C140}, {0x2FC, 0x57C0F0}, {0x318, 0x57C140}, {0x332, 0x587740}, {0x348, 0x587740}, {0x378, 0x57C140}, {0x389, 0x587740}, {0x3A1, 0x589810}};
// 0x558550 Scena10_StartRun1: 0x97 bytes; called by Scena10_EnterArea
constexpr sh::CallSite kCalls558550[] = {{0x0, 0x57C7C0}, {0x7, 0x531F90}, {0x23, 0x56F670}, {0x31, 0x589810}, {0x80, 0x587A20}};
// 0x5585F0 Scena10_Run: 0xE bytes; jmp through .data 0x661594, 14 entries (a data_tables entry); Scena10_States entry 2
// 0x558600 Scena10_Run1: 0x354 bytes; Scena10_Runs entry 1
constexpr sh::CallSite kCalls558600[] = {{0x67, 0x589810}, {0xE2, 0x454810}, {0x108, 0x495040}, {0x140, 0x594E00}, {0x1F7, 0x495040}, {0x255, 0x57C0F0}, {0x26B, 0x594E00}, {0x290, 0x589810}, {0x2CF, 0x5720C0}, {0x2FA, 0x57C7A0}, {0x308, 0x57C0F0}};
constexpr sh::JumpTable kTables558600[] = {{0x15, 0x320, 13}};
// 0x558960 Scena10_Run2: 0x654 bytes; Scena10_Runs entry 2
constexpr sh::CallSite kCalls558960[] = {{0x1B, 0x531F90}, {0x25, 0x454590}, {0x4D, 0x55B5D0}, {0x53, 0x4976D0}, {0x97, 0x454810}, {0xA4, 0x589810}, {0x131, 0x594E00}, {0x162, 0x589810}, {0x1B3, 0x587AE0}, {0x1D9, 0x589810}, {0x246, 0x589810}, {0x297, 0x587BE0}, {0x2B1, 0x587B40}, {0x2DC, 0x589810}, {0x31B, 0x5720C0}, {0x34E, 0x495040}, {0x397, 0x495040}, {0x3B9, 0x4976D0}, {0x3E8, 0x495040}, {0x42B, 0x589810}, {0x47F, 0x5720C0}, {0x4AF, 0x589810}, {0x521, 0x589810}, {0x5A4, 0x594E00}, {0x5C8, 0x57C0F0}, {0x5D0, 0x57C7A0}};
constexpr sh::JumpTable kTables558960[] = {{0x15, 0x5EC, 26}};
// 0x558FC0 Scena10_Run3: 0x50C bytes; Scena10_Runs entry 3
constexpr sh::CallSite kCalls558FC0[] = {{0x18, 0x589810}, {0x6B, 0x531F90}, {0x88, 0x55B650}, {0x94, 0x4976D0}, {0xFF, 0x589810}, {0x17C, 0x57C7A0}, {0x18A, 0x57C0F0}, {0x1A2, 0x531F90}, {0x1F3, 0x594E00}, {0x218, 0x589810}, {0x282, 0x589810}, {0x2EC, 0x589810}, {0x356, 0x589810}, {0x3C7, 0x591900}, {0x3CC, bof3::addr::Sound_StopMusic}, {0x3D2, 0x587910}, {0x3F1, 0x587A00}, {0x3FE, bof3::addr::Sound_ResumeAll}, {0x403, 0x589810}, {0x484, 0x57C7A0}, {0x48A, 0x5341C0}, {0x48F, 0x5594D0}, {0x49D, 0x57C0F0}};
constexpr sh::JumpTable kTables558FC0[] = {{0x14, 0x4B4, 22}};
// 0x5594D0 Scena10_SpriteOp: 0x21 bytes; called by Scena10_Run3
constexpr sh::CallSite kCalls5594D0[] = {{0x0, 0x57CD90}, {0x1A, 0x57A010}};
// 0x559500 Scena10_Run4: 0x3F4 bytes; Scena10_Runs entry 4 (two-level: the byte table rides in the extent)
constexpr sh::CallSite kCalls559500[] = {{0x46, 0x57C140}, {0x5B, 0x559900}, {0x71, 0x4976D0}, {0x8D, 0x4976D0}, {0xBE, 0x57C7A0}, {0xD2, 0x589810}, {0x11C, 0x559930}, {0x133, 0x531F90}, {0x141, 0x57C0F0}, {0x18D, 0x589810}, {0x1F4, 0x531F90}, {0x202, 0x57C0F0}, {0x218, 0x57C7A0}, {0x221, 0x495040}, {0x249, 0x5A9949}, {0x24F, 0x5341A0}, {0x282, 0x594E00}, {0x2B5, 0x495040}, {0x2DD, 0x57C0F0}, {0x2E5, 0x57C7A0}, {0x308, 0x57C140}, {0x314, 0x57C7A0}, {0x329, 0x531F90}, {0x335, 0x531F90}, {0x34F, 0x57C7A0}, {0x35D, 0x57C0F0}, {0x370, 0x594E00}};
constexpr sh::JumpTable kTables559500[] = {{0x1C, 0x390, 17}};
// 0x559900 Scena10_HasItem4Eto55: 0x21 bytes; called by Scena10_Run4, al
constexpr sh::CallSite kCalls559900[] = {{0xC, 0x5919B0}};
// 0x559930 Scena10_TallyMet: 0x34 bytes; called by Scena10_Run4, al
// 0x559970 Scena10_Run5: 0x1AC bytes; Scena10_Runs entry 5
constexpr sh::CallSite kCalls559970[] = {{0x2A, 0x587BE0}, {0x55, 0x587B40}, {0x91, 0x532ED0}, {0xD8, 0x55B550}, {0xDE, 0x4976D0}, {0x103, 0x587740}, {0x10A, 0x4410B0}, {0x122, 0x57C0F0}, {0x138, 0x594E00}, {0x169, 0x57C7A0}};
constexpr sh::JumpTable kTables559970[] = {{0x17, 0x180, 11}};
// 0x559B20 Scena10_Run6: 0xFB0 bytes, 120 calls - this file's own copy (below); Scena10_Runs entry 6
constexpr sh::CallSite kCalls559B20[] = {{0x21, 0x495040}, {0x30, 0x55AAD0}, {0x4E, 0x5341A0}, {0x5D, 0x55AAD0}, {0x78, 0x594E00}, {0x9C, 0x55AAD0}, {0xBA, 0x495040}, {0xD0, 0x55AAD0}, {0xE6, 0x587B40}, {0xF0, 0x587740}, {0x10F, 0x55AAD0}, {0x125, 0x495040}, {0x134, 0x55AAD0}, {0x152, 0x495040}, {0x16E, 0x56F670}, {0x186, 0x55AAD0}, {0x1A2, 0x55AAD0}, {0x1BD, 0x55AAD0}, {0x1D1, 0x589810}, {0x220, 0x5734F0}, {0x22F, 0x55AAD0}, {0x25C, 0x495040}, {0x26B, 0x55AAD0}, {0x289, 0x495040}, {0x2AC, 0x56F670}, {0x2BF, 0x5725F0}, {0x2CE, 0x55AAD0}, {0x2F6, 0x55AAD0}, {0x30C, 0x495040}, {0x318, 0x587AE0}, {0x327, 0x55AAD0}, {0x33E, 0x495040}, {0x364, 0x56F670}, {0x369, 0x589810}, {0x3A3, 0x5725F0}, {0x3B2, 0x55AAD0}, {0x3DA, 0x55AAD0}, {0x3F0, 0x589810}, {0x445, 0x587740}, {0x454, 0x55AAD0}, {0x486, 0x587740}, {0x49C, 0x594E00}, {0x4B2, 0x55AAD0}, {0x4D7, 0x5341C0}, {0x4F0, 0x594E00}, {0x505, 0x55AAD0}, {0x528, 0x57C7A0}, {0x53B, 0x57C0F0}, {0x54A, 0x55AAD0}, {0x561, 0x55AAD0}, {0x57E, 0x5341A0}, {0x597, 0x594E00}, {0x5BD, 0x55AAD0}, {0x5E6, 0x55AAD0}, {0x626, 0x56F670}, {0x638, 0x5720C0}, {0x656, 0x55AAD0}, {0x684, 0x55AAD0}, {0x6C4, 0x56F670}, {0x6D7, 0x5720C0}, {0x6F5, 0x55AAD0}, {0x723, 0x55AAD0}, {0x763, 0x56F670}, {0x775, 0x5720C0}, {0x793, 0x55AAD0}, {0x7C1, 0x55AAD0}, {0x7D5, 0x589810}, {0x830, 0x55AAD0}, {0x861, 0x55AAD0}, {0x8A1, 0x56F670}, {0x8B4, 0x5720C0}, {0x8D2, 0x55AAD0}, {0x900, 0x55AAD0}, {0x940, 0x56F670}, {0x953, 0x5725F0}, {0x97D, 0x55AAD0}, {0x9A9, 0x587740}, {0x9B3, 0x587740}, {0x9BA, 0x587BE0}, {0x9C2, 0x589810}, {0x9FA, 0x55AAD0}, {0xA01, 0x589810}, {0xA5C, 0x55AAD0}, {0xA81, 0x589810}, {0xADC, 0x55AAD0}, {0xB02, 0x589810}, {0xB6E, 0x55AAD0}, {0xB9B, 0x495040}, {0xBAA, 0x55AAD0}, {0xBC1, 0x495040}, {0xBE7, 0x56F670}, {0xBFE, 0x589810}, {0xC58, bof3::addr::Sound_ResumeAll}, {0xC62, 0x587740}, {0xC6C, 0x587740}, {0xC74, 0x55AAD0}, {0xC91, 0x495040}, {0xCA0, 0x55AAD0}, {0xCE1, 0x56F670}, {0xCF6, 0x55AAD0}, {0xD24, 0x55AAD0}, {0xD41, 0x5341A0}, {0xD4B, 0x587740}, {0xD61, 0x594E00}, {0xD77, 0x55AAD0}, {0xD8B, 0x589810}, {0xDE6, 0x55AAD0}, {0xE03, 0x57C0F0}, {0xE22, 0x594E00}, {0xE3F, 0x55AAD0}, {0xE59, 0x4976D0}, {0xE6F, 0x55AAD0}, {0xE88, 0x495040}, {0xEA3, 0x55AAD0}, {0xEB3, 0x57C7A0}, {0xEC8, 0x57C0F0}, {0xEDC, 0x55AAD0}};
constexpr sh::JumpTable kTables559B20[] = {{0x14, 0xEE4, 51}};
// 0x55AAD0 Scena10_Shake: 0x2B bytes; called by runs 6, 7, 12
// 0x55AB00 Scena10_Run7: 0x414 bytes; Scena10_Runs entry 7
constexpr sh::CallSite kCalls55AB00[] = {{0x60, 0x531F90}, {0x81, 0x495040}, {0xD1, 0x5341C0}, {0xEB, 0x57C0F0}, {0x101, 0x594E00}, {0x146, 0x495040}, {0x168, 0x531F90}, {0x16F, 0x5734F0}, {0x198, 0x589810}, {0x20A, 0x594E00}, {0x220, 0x55AAD0}, {0x243, 0x57C7A0}, {0x258, 0x57C0F0}, {0x267, 0x55AAD0}, {0x271, 0x531F90}, {0x280, 0x55AAD0}, {0x29C, 0x55AAD0}, {0x2A4, 0x454810}, {0x2C4, 0x55AAD0}, {0x2DB, 0x495040}, {0x2EA, 0x55AAD0}, {0x309, 0x5341A0}, {0x318, 0x55AAD0}, {0x32F, 0x57C0F0}, {0x342, 0x594E00}, {0x34E, 0x57C7A0}, {0x35C, 0x57C140}, {0x36F, 0x57C0F0}, {0x383, 0x57C7A0}, {0x391, 0x495040}, {0x399, 0x56D6F0}, {0x3B8, 0x55AAD0}};
constexpr sh::JumpTable kTables55AB00[] = {{0x15, 0x3C0, 21}};
// 0x55AF20 Scena10_Run10: 0x37 bytes; Scena10_Runs entry 10 (not walked)
constexpr sh::CallSite kCalls55AF20[] = {{0x15, 0x531F90}, {0x22, 0x57C0F0}};
// 0x55AF60 Scena10_Run11: 0x2C bytes; Scena10_Runs entry 11 (not walked)
constexpr sh::CallSite kCalls55AF60[] = {{0x9, 0x57C7A0}, {0x10, 0x531F90}};
// 0x55AF90 Scena10_Run12: 0x200 bytes; Scena10_Runs entry 12 (not walked)
constexpr sh::CallSite kCalls55AF90[] = {{0x19, 0x4976D0}, {0x4D, 0x531F90}, {0x6D, 0x55AAD0}, {0x86, 0x57C110}, {0xAE, 0x589810}, {0x15A, 0x589810}, {0x1C3, 0x57C7A0}, {0x1D1, 0x57C0F0}};
constexpr sh::JumpTable kTables55AF90[] = {{0x13, 0x1E8, 6}};
// 0x55B190 Scena10_Run13: 0x3BE bytes; Scena10_Runs entry 13 (not walked)
constexpr sh::CallSite kCalls55B190[] = {{0x37, 0x57C0F0}, {0x4D, 0x594E00}, {0x7D, 0x57C0F0}, {0x8C, 0x57C7A0}, {0xAD, 0x57C7A0}, {0xBB, 0x57C0F0}, {0xCB, 0x531F90}, {0xF7, 0x57C7C0}, {0x105, 0x57C0F0}, {0x115, 0x5341C0}, {0x11C, 0x531F90}, {0x137, 0x4976D0}, {0x15D, 0x57C7A0}, {0x17C, 0x531F90}, {0x19B, 0x589810}, {0x255, 0x532ED0}, {0x25C, 0x4410B0}, {0x277, 0x57C0F0}, {0x28A, 0x594E00}, {0x2CF, 0x495040}, {0x2FE, 0x589810}, {0x33A, 0x57C7A0}, {0x353, 0x57C0F0}};
constexpr sh::JumpTable kTables55B190[] = {{0x1D, 0x360, 17}};
// 0x55B550 / 0x55B5D0 / 0x55B650 Scena10_MsgByMemberA / B / C: 0x7E / 0x7E / 0x7F bytes, no calls; ax
// 0x55B6D0 Scena10_ObjectTrigger: 0x1F bytes; call through .data 0x661614 (typed stand-ins); root: chapter 10 slot 1, the object
// 0x55B6F0 .. 0x55BAE0: Scena10_Objects entries (object, bits)
constexpr sh::CallSite kCalls55B6F0[] = {{0x0, 0x57C7C0}, {0x7, 0x531F90}};
constexpr sh::CallSite kCalls55B700[] = {{0x0, 0x57C7C0}, {0x7, 0x531F90}};
constexpr sh::CallSite kCalls55B710[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls55B730[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls55B750[] = {{0x2, 0x531F90}, {0x2E, 0x57C0F0}};
constexpr sh::CallSite kCalls55B790[] = {{0x75, 0x591680}, {0xAC, 0x590BB0}, {0xC8, 0x57C0F0}, {0xCF, 0x497710}, {0xD9, 0x587740}, {0xEC, 0x57C7C0}, {0xFF, 0x497710}};
// 0x55B8B0 / 0x55B960 Scena10_PickupPose / PickupEffect: area 75's and 86's handler tables (not walked)
constexpr sh::CallSite kCalls55B8B0[] = {{0x6E, 0x57C140}, {0x81, 0x5891F0}};
constexpr sh::CallSite kCalls55B960[] = {{0x9, 0x589810}};
constexpr sh::CallSite kCalls55B9E0[] = {{0x2, 0x531F90}, {0x2E, 0x57C0F0}};
constexpr sh::CallSite kCalls55BA20[] = {{0x2, 0x531F90}, {0x2E, 0x57C0F0}};
constexpr sh::CallSite kCalls55BA60[] = {{0x0, 0x57C7C0}, {0x7, 0x531F90}, {0x14, 0x57C0F0}};
constexpr sh::CallSite kCalls55BAA0[] = {{0x8, 0x57C0F0}};
constexpr sh::CallSite kCalls55BAC0[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls55BAE0[] = {{0x8, 0x57C7C0}};
// 0x55BB00 Scena10_StepHook: 0x36C bytes; root: chapter 10 slot 2, (x, z), al
constexpr sh::CallSite kCalls55BB00[] = {{0x2B, 0x57C140}, {0x48, 0x57C7C0}, {0xB1, 0x550F80}, {0xC1, 0x57C7C0}, {0xD6, 0x57C140}, {0xEB, 0x57C140}, {0xFF, 0x57C0F0}, {0x107, 0x57C7C0}, {0x12A, 0x57C140}, {0x14F, 0x57C0F0}, {0x190, 0x57C140}, {0x1EC, 0x57C140}, {0x204, 0x57C140}, {0x221, 0x57C7C0}, {0x23F, 0x57C7C0}, {0x24D, 0x57C0F0}, {0x263, 0x594E00}, {0x2A8, 0x57C140}, {0x2BC, 0x57C140}, {0x2F1, 0x57C7C0}, {0x31D, 0x57C140}, {0x33A, 0x57C7C0}, {0x356, 0x57C110}};
// 0x55BE70 Scena10_ArriveHook: 0x1D0 bytes; root: chapter 10 slot 3, (x, z), al
constexpr sh::CallSite kCalls55BE70[] = {{0x1C, 0x57C140}, {0x31, 0x57C140}, {0x4E, 0x57C7C0}, {0x5B, 0x57C0F0}, {0x67, 0x57C0F0}, {0x9B, 0x57C140}, {0xAF, 0x57C7C0}, {0xE7, 0x57C140}, {0xFB, 0x57C140}, {0x119, 0x57C7C0}, {0x144, 0x57C140}, {0x161, 0x57C7C0}, {0x181, 0x57C140}, {0x196, 0x57C140}, {0x1B3, 0x57C7C0}};

// Scena10_Run6 has 120 call sites, more than the harness re-aims in one clone
// (64). So this file makes its copy itself - bof3::CloneOriginal with every
// site re-aimed at a trampoline that calls the harness's recorder for the
// callee (StandIn: the same log entry, disturbance and answer a re-aimed site
// gets), the jump table moved into the copy - and hands the harness, as its
// "original", a six-byte `jmp [copy]` of its own (scena_sc12_fuzz.cpp's Run4 /
// Run8, scena_sc9a_fuzz.cpp's Run15). Theirs is still Capcom's bytes.
template <typename F> F Stub(F f) {
    return reinterpret_cast<F>(const_cast<void*>(sh::StandIn(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f)))));
}
void __cdecl TTransition(unsigned char k) { Stub(Transition_Start)(k); }
void __cdecl TShake() { Stub(Scena10_Shake)(); }
void __cdecl TCallA(unsigned n) { Stub(Scenario_CallA)(n); }
void __cdecl TCallB(unsigned n) { Stub(Scenario_CallB)(n); }
void __cdecl TChangeArea(unsigned area, int x, int z, unsigned flags) { Stub(Field_ChangeArea)(area, x, z, flags); }
void __cdecl TFadeOutStop(int f) { Stub(Music_FadeOutStop)(f); }
void __cdecl TFadeOut(int f) { Stub(Music_FadeOut)(f); }
void __cdecl TSound(unsigned short id) { Stub(Sound_PlayEffect)(id); }
void __cdecl TViewReset() { Stub(Field_ViewReset)(); }
unsigned char __cdecl TFindFree() { return Stub(Effect_FindFree)(); }
void __cdecl TKind2(unsigned char a) { Stub(Kind2_Place)(a); }
void __cdecl TSetElevation(int v) { Stub(MapView_SetElevation)(v); }
void __cdecl TMusicPlay(unsigned t, int f) { Stub(Music_Play)(t, f); }
void __cdecl TClear40() { Stub(ScriptFlags_Clear40)(); }
void __cdecl TFlagsSet(unsigned char* b, unsigned i) { Stub(Flags_Set)(b, i); }
long __cdecl TElevation(long x, long z) { return Stub(AreaMap_Elevation)(x, z); }
void __cdecl TResumeAll() { Stub(Sound_ResumeAll)(); }
void __cdecl TMsg(unsigned short id) { Stub(Msg_OpenScript)(id); }

struct Tramp { std::uint32_t target; const void* to; };
const Tramp kTramps[] = {
    {0x495040, reinterpret_cast<const void*>(&TTransition)},  {0x55AAD0, reinterpret_cast<const void*>(&TShake)},
    {0x5341A0, reinterpret_cast<const void*>(&TCallA)},       {0x5341C0, reinterpret_cast<const void*>(&TCallB)},
    {0x594E00, reinterpret_cast<const void*>(&TChangeArea)},  {0x587B40, reinterpret_cast<const void*>(&TFadeOutStop)},
    {0x587BE0, reinterpret_cast<const void*>(&TFadeOut)},     {0x587740, reinterpret_cast<const void*>(&TSound)},
    {0x56F670, reinterpret_cast<const void*>(&TViewReset)},   {0x589810, reinterpret_cast<const void*>(&TFindFree)},
    {0x5734F0, reinterpret_cast<const void*>(&TKind2)},       {0x5725F0, reinterpret_cast<const void*>(&TSetElevation)},
    {0x587AE0, reinterpret_cast<const void*>(&TMusicPlay)},   {0x57C7A0, reinterpret_cast<const void*>(&TClear40)},
    {0x57C0F0, reinterpret_cast<const void*>(&TFlagsSet)},    {0x5720C0, reinterpret_cast<const void*>(&TElevation)},
    {bof3::addr::Sound_ResumeAll, reinterpret_cast<const void*>(&TResumeAll)},   {0x4976D0, reinterpret_cast<const void*>(&TMsg)},
};

}  // namespace
}  // namespace scena_sc9b

extern "C" {
void* g_sc9b_run6_copy = nullptr;
__attribute__((naked)) void Sc9bRun6Theirs() { asm("jmp *_g_sc9b_run6_copy"); }
}

namespace scena_sc9b {
namespace {

constexpr std::uint32_t kJmpWrapper = 6;   // FF 25 disp32

void* CopyWithTramps(const char* name, std::uint32_t base, std::uint32_t size, const sh::CallSite* sites, int n,
                     const sh::JumpTable* tables, int n_tables) {
    static bof3::CloneCall calls[128];
    if (n > 128) bof3::Fatal("scena_sc9b: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) {
        const void* to = nullptr;
        for (const Tramp& t : kTramps)
            if (t.target == sites[i].target) to = t.to;
        if (!to) bof3::Fatal("scena_sc9b: %s: no trampoline for 0x%X", name, (unsigned)sites[i].target);
        calls[i] = {sites[i].offset, to, sites[i].target};
    }
    void* copy = bof3::CloneOriginal(name, base, size, calls, n);
    for (int i = 0; i < n_tables; ++i)
        move_script::Relocate(copy, base, size, {tables[i].jmp_disp, tables[i].table, tables[i].entries});
    return copy;
}

std::uint32_t Wrapper(void (*f)()) {
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p[0] != 0xFF || p[1] != 0x25) bof3::Fatal("scena_sc9b: the jmp wrapper at %p is not FF 25", static_cast<const void*>(p));
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define SC9B_CLONE(name, base, size, calls) #name, base, size, calls, SH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)
#define SC9B_CLONE_T(name, base, size, calls, tables) \
    #name, base, size, calls, SH_N(calls), nullptr, 0, tables, SH_N(tables), reinterpret_cast<const void*>(&::name)
#define SC9B_CLONE_0(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name)

// ===========================================================================
// Chapter 9's tail

const sh::Clone kClones9[] = {
    {SC9B_CLONE(Scena09_Object07, 0x557170, 0x11, kCalls557170), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Object08, 0x557190, 0x14, kCalls557190), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Object09, 0x5571B0, 0x11, kCalls5571B0), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Object10, 0x5571D0, 0x11, kCalls5571D0), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Object11, 0x5571F0, 0x14, kCalls5571F0), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Object13, 0x557210, 0x14, kCalls557210), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Object14, 0x557230, 0x14, kCalls557230), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Object15, 0x557250, 0x14, kCalls557250), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_StepHook, 0x557270, 0x7A6, kCalls557270), 0xFF, false, sh::Shape::kHook},
    {SC9B_CLONE(Scena09_CellHook, 0x557A20, 0x2A, kCalls557A20), 0xFF, false, sh::Shape::kHook},
    {SC9B_CLONE(Scena09_Cell00, 0x557A50, 0x5F, kCalls557A50), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell01, 0x557AB0, 0xB9, kCalls557AB0), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell02, 0x557B70, 0xB9, kCalls557B70), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell03, 0x557C30, 0xEA, kCalls557C30), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell04, 0x557D20, 0xAB, kCalls557D20), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell05, 0x557DD0, 0xAB, kCalls557DD0), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell06, 0x557E80, 0x51, kCalls557E80), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell07, 0x557EE0, 0x51, kCalls557EE0), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell08, 0x557F40, 0x51, kCalls557F40), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell09, 0x557FA0, 0x51, kCalls557FA0), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell10, 0x558000, 0x51, kCalls558000), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell11, 0x558060, 0x48, kCalls558060), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell12, 0x5580B0, 0x2D, kCalls5580B0), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE(Scena09_Cell13, 0x5580E0, 0x53, kCalls5580E0), 0xFF, false, sh::Shape::kState},
};
enum : unsigned {
    k9Object07, k9Object08, k9Object09, k9Object10, k9Object11, k9Object13, k9Object14, k9Object15, k9StepHook,
    k9CellHook, k9Cell00, k9Cell01, k9Cell02, k9Cell03, k9Cell04, k9Cell05, k9Cell06, k9Cell07, k9Cell08, k9Cell09,
    k9Cell10, k9Cell11, k9Cell12, k9Cell13, k9Count
};
static_assert(sizeof kClones9 / sizeof kClones9[0] == k9Count, "one role per clone");

// Scena09_CellHooks' entries are reached with (x, z) in place and answer in al,
// which a DataTable's handler recorder does not log. So the seed writes a
// stand-in of the exact type into every entry, one per index (a wrong index is
// a different log), and the table is a region the harness puts back. Each logs
// against the dispatcher's own address, which no clone calls.
template <unsigned I> unsigned char __cdecl CellEntry9(int x, int z) {
    sh::Record(0x557A20, I, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z));
    sh::Stir();
    return static_cast<unsigned char>(sh::Noise());
}
using CellFn = unsigned char (__cdecl*)(int, int);
const CellFn kCellEntries9[14] = {
    &CellEntry9<0>, &CellEntry9<1>, &CellEntry9<2>, &CellEntry9<3>,  &CellEntry9<4>,  &CellEntry9<5>,  &CellEntry9<6>,
    &CellEntry9<7>, &CellEntry9<8>, &CellEntry9<9>, &CellEntry9<10>, &CellEntry9<11>, &CellEntry9<12>, &CellEntry9<13>,
};

#define SC9B_RAW(name, address) name, address, address
#define SC9B_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees9[] = {
    // tested on al alone (test al, al): garbage above a 0 must not matter
    {SC9B_OURS(Flags_Test), 2, {kAll, kU8}, sh::Answer::kFlag, 0, 0},
    // the cell-record search: the records, the count, the cell; none or 0..13
    {SC9B_RAW("0x56D800", at::kCellFind), 4, {kAll, kU8, kAll, kAll}, sh::Answer::kByte, 0xFF, 0x0D},
    // the log slot of the cell stand-ins above
    {"Scena09_CellHooks[i]", 0x557A20, 0x557A20, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&CellEntry9<0>)},
};

const sh::Region kRegions9[] = {
    {at::kCondFE, 1},
    {at::kCellHooks9, 4 * at::kCell9Count},
};

unsigned char& B(std::uint32_t a) { return *sh::Mem(a); }
void SetW(std::uint32_t a, std::uint32_t v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(sh::Mem(a), &w, 2);
}
void SetD(std::uint32_t a, std::uint32_t v) { std::memcpy(sh::Mem(a), &v, 4); }
std::uint32_t GetD(std::uint32_t a) {
    std::uint32_t v;
    std::memcpy(&v, sh::Mem(a), 4);
    return v;
}

// A rectangle of a hook's (x, z) for one area and Cond_ByteFD (0xFF: any):
// the hook's arguments are drawn at its edges, one past them, or inside.
struct Rect { std::uint16_t area; std::uint8_t fd; std::int32_t xlo, xhi, zlo, zhi; };
const Rect kRects9[] = {
    {0x25, 0xFF, 0x200000, 0x238000, 0x410000, 0x418000},
    {0x27, 0xFF, 0x310000, 0x318000, 0x270000, 0x298000},
    {0x29, 0xFF, 0x3F8000, 0x408000, 0x270000, 0x288000},
    {0x29, 0xFF, 0x480000, 0x4B8000, 0x250000, 0x258000},
    {0x29, 0xFF, 0x38000, 0x40000, 0x20000, 0x38000},
    {0x2E, 0xFF, 0xF0000, 0x108000, 0x220000, 0x228000},
    {0x31, 0xFF, 0x268000, 0x2B8000, 0x2B0000, 0x2F8000},
    {0x31, 0xFF, 0x290000, 0x2B8000, 0x5C0000, 0x5C8000},
    {0x31, 0xFF, 0x4E0000, 0x4E8000, 0x220000, 0x258000},
    {0x3B, 0xFF, 0x470000, 0x498000, 0x90000, 0xA8000},
    {0x3B, 0xFF, 0x290000, 0x298000, 0x110000, 0x188000},
    {0x77, 0xFF, 0x420000, 0x448000, 0x3C0000, 0x3E8000},
    {0x77, 0xFF, 0x420000, 0x448000, 0x410000, 0x438000},
    {0x77, 0xFF, 0xA0000, 0xA8000, 0x20000, 0x30000},
    {0x77, 0xFF, 0x60000, 0x68000, 0x20000, 0x30000},
    {0x52, 0, 0x220000, 0x25FFFF, 0x400000, 0x44FFFF},
    {0x52, 0, 0x220000, 0x25FFFF, 0x3C0000, 0x40FFFF},
};
const Rect kRectsStep10[] = {
    {0x4B, 1, 0x30000, 0x40000, 0x6B0000, 0x6EFFFF},
    {0x52, 0, 0x220000, 0x25FFFF, 0x3C0000, 0x40FFFF},
    {0x52, 0, 0x458000, 0x458000, 0x230000, 0x24FFFF},
    {0x78, 0, 0x58000, 0x58000, 0x110000, 0x14FFFF},
    {0x78, 5, 0x2B8000, 0x2B8000, 0x740000, 0x75FFFF},
    {0x80, 0, 0x180000, 0x1CFFFF, 0x118000, 0x118000},
    {0x80, 3, 0x520000, 0x520000, 0x70000, 0x9FFFF},
};
const Rect kRectsArrive10[] = {
    {0x3A, 0xFF, 0x1C0000, 0x20FFFF, 0x300000, 0x310000},
    {0x69, 0, 0x100000, 0x200000, 0x1C0000, 0x1DFFFF},
    {0x78, 5, 0x250000, 0x250000, 0x748000, 0x748000},
    {0x80, 0xFF, 0x200000, 0x24FFFF, 0x2A0000, 0x2B0000},
    {0x80, 0xFF, 0x1F0000, 0x20FFFF, 0x360000, 0x370000},
};

const Rect* g_rect = nullptr;

std::int32_t Around(std::int32_t lo, std::int32_t hi) {
    switch (sh::Next() % 8) {
    case 0: return lo;
    case 1: return hi;
    case 2: return lo - 1;
    case 3: return hi + 1;
    case 4: return lo - 0x10000;
    case 5: return hi + 0x10000;
    default: return lo + static_cast<std::int32_t>(sh::Next() % (static_cast<std::uint32_t>(hi - lo) + 1));
    }
}

// A rectangle picked, its area and Cond_ByteFD seeded two times in three each.
void SeedRect(const Rect* rects, unsigned n) {
    g_rect = &rects[sh::Next() % n];
    if (sh::Often()) SetW(at::kArea, g_rect->area);
    if (g_rect->fd != 0xFF && sh::Often()) B(at::kCondFD) = g_rect->fd;
}
void ArgsRect(std::uint32_t* a) {
    if (!g_rect || !sh::Often()) return;
    a[0] = static_cast<std::uint32_t>(Around(g_rect->xlo, g_rect->xhi));
    a[1] = static_cast<std::uint32_t>(Around(g_rect->zlo, g_rect->zhi));
}

std::uint32_t AnFD() { return SH_PICK(0, 1, 2, 3, 4, 5); }
// ObjTrio +0x8, compared with 4..6 (chapter 9) and 0..2 (chapter 10).
std::uint32_t ALeader8() { return sh::Next() % 8; }
// A member's +0x89, compared with 2, 4, 5.
std::uint32_t AKind() { return SH_PICK(2, 4, 5, 1, 3, 6); }

unsigned g_k9;

void Seed9(unsigned k) {
    g_k9 = k;
    g_rect = nullptr;
    for (unsigned i = 0; i < at::kCell9Count; ++i) SetD(at::kCellHooks9 + 4 * i, KeyOf(kCellEntries9[i]));
    if (sh::Often()) B(at::kCondFD) = static_cast<unsigned char>(AnFD());
    if (sh::Often()) B(at::kLeaderByte8) = static_cast<unsigned char>(ALeader8());
    if (sh::Often()) B(at::kObjTrio + at::kMemberKind) = static_cast<unsigned char>(AKind());
    if (k == k9StepHook) SeedRect(kRects9, sizeof kRects9 / sizeof kRects9[0]);
}

void Args9(unsigned k, std::uint32_t* a) {
    if (k == k9StepHook) ArgsRect(a);
}

// After a call, beyond the harness's own, drawn from `h` alone: the area (one
// of the hook's), Cond_ByteFD, the leader's bytes.
void Disturb9(std::uint32_t h) {
    const unsigned char v = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 4) {
    case 0: {
        static const std::uint16_t kAreas[] = {0x25, 0x27, 0x29, 0x2E, 0x31, 0x3B, 0x77, 0x52};
        SetW(at::kArea, kAreas[(h >> 16) % 8]);
        break;
    }
    case 1: B(at::kCondFD) = static_cast<unsigned char>(v % 6); break;
    case 2: B(at::kLeaderByte8) = static_cast<unsigned char>(v % 8); break;
    default: B(at::kObjTrio + at::kMemberKind) = static_cast<unsigned char>(2 + v % 4); break;
    }
}

// After every disturbance, for the step hook: the area half the time (it is
// read again after each block's calls) and Cond_ByteFD.
void Settle9() {
    if (g_k9 != k9StepHook) return;
    const std::uint32_t n = sh::Noise();
    if (n & 1) {
        static const std::uint16_t kAreas[] = {0x25, 0x27, 0x29, 0x2E, 0x31, 0x3B, 0x77, 0x52};
        SetW(at::kArea, kAreas[(n >> 8) % 8]);
    }
    if (n & 2) B(at::kCondFD) = static_cast<unsigned char>((n >> 16) % 2);
}

// ===========================================================================
// Chapter 10

const sh::Clone kClones10[] = {
    {SC9B_CLONE_0(Scena10_Frame, 0x558140, 0xE), 0, false, sh::Shape::kSlot},
    {SC9B_CLONE(Scena10_EnterArea, 0x558150, 0x3FF, kCalls558150), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena10_StartRun1, 0x558550, 0x97, kCalls558550), 0, false, sh::Shape::kState},
    {SC9B_CLONE_0(Scena10_Run, 0x5585F0, 0xE), 0, false, sh::Shape::kState},
    {SC9B_CLONE_T(Scena10_Run1, 0x558600, 0x354, kCalls558600, kTables558600), 0, false, sh::Shape::kState},
    {SC9B_CLONE_T(Scena10_Run2, 0x558960, 0x654, kCalls558960, kTables558960), 0, false, sh::Shape::kState},
    {SC9B_CLONE_T(Scena10_Run3, 0x558FC0, 0x50C, kCalls558FC0, kTables558FC0), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena10_SpriteOp, 0x5594D0, 0x21, kCalls5594D0), 0, false, sh::Shape::kState},
    {SC9B_CLONE_T(Scena10_Run4, 0x559500, 0x3F4, kCalls559500, kTables559500), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena10_HasItem4Eto55, 0x559900, 0x21, kCalls559900), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE_0(Scena10_TallyMet, 0x559930, 0x34), 0xFF, false, sh::Shape::kState},
    {SC9B_CLONE_T(Scena10_Run5, 0x559970, 0x1AC, kCalls559970, kTables559970), 0, false, sh::Shape::kState},
    {"Scena10_Run6", Wrapper(&Sc9bRun6Theirs), kJmpWrapper, nullptr, 0, nullptr, 0, nullptr, 0,   // this file's copy (above)
     reinterpret_cast<const void*>(&::Scena10_Run6), 0, false, sh::Shape::kState},
    {SC9B_CLONE_0(Scena10_Shake, 0x55AAD0, 0x2B), 0, false, sh::Shape::kState},
    {SC9B_CLONE_T(Scena10_Run7, 0x55AB00, 0x414, kCalls55AB00, kTables55AB00), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena10_Run10, 0x55AF20, 0x37, kCalls55AF20), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena10_Run11, 0x55AF60, 0x2C, kCalls55AF60), 0, false, sh::Shape::kState},
    {SC9B_CLONE_T(Scena10_Run12, 0x55AF90, 0x200, kCalls55AF90, kTables55AF90), 0, false, sh::Shape::kState},
    {SC9B_CLONE_T(Scena10_Run13, 0x55B190, 0x3BE, kCalls55B190, kTables55B190), 0, false, sh::Shape::kState},
    {SC9B_CLONE_0(Scena10_MsgByMemberA, 0x55B550, 0x7E), 0xFFFF, false, sh::Shape::kState},
    {SC9B_CLONE_0(Scena10_MsgByMemberB, 0x55B5D0, 0x7E), 0xFFFF, false, sh::Shape::kState},
    {SC9B_CLONE_0(Scena10_MsgByMemberC, 0x55B650, 0x7F), 0xFFFF, false, sh::Shape::kState},
    {SC9B_CLONE_0(Scena10_ObjectTrigger, 0x55B6D0, 0x1F), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object00, 0x55B6F0, 0xE, kCalls55B6F0), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object01, 0x55B700, 0xE, kCalls55B700), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object02, 0x55B710, 0x14, kCalls55B710), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object03, 0x55B730, 0x19, kCalls55B730), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object04, 0x55B750, 0x37, kCalls55B750), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object05, 0x55B790, 0x113, kCalls55B790), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_PickupPose, 0x55B8B0, 0xA6, kCalls55B8B0), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena10_PickupEffect, 0x55B960, 0x71, kCalls55B960), 0, false, sh::Shape::kState},
    {SC9B_CLONE(Scena10_Object06, 0x55B9E0, 0x37, kCalls55B9E0), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object07, 0x55BA20, 0x37, kCalls55BA20), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object08, 0x55BA60, 0x36, kCalls55BA60), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object09, 0x55BAA0, 0x11, kCalls55BAA0), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object11, 0x55BAC0, 0x14, kCalls55BAC0), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_Object12, 0x55BAE0, 0x1C, kCalls55BAE0), 0, false, sh::Shape::kObject},
    {SC9B_CLONE(Scena10_StepHook, 0x55BB00, 0x36C, kCalls55BB00), 0xFF, false, sh::Shape::kHook},
    {SC9B_CLONE(Scena10_ArriveHook, 0x55BE70, 0x1D0, kCalls55BE70), 0xFF, false, sh::Shape::kHook},
};
enum : unsigned {
    kFrame, kEnterArea, kStartRun1, kRun, kRun1, kRun2, kRun3, kSpriteOp, kRun4, kHasItem, kTally, kRun5, kRun6, kShake,
    kRun7, kRun10, kRun11, kRun12, kRun13, kMsgA, kMsgB, kMsgC, kObjectTrigger, kObject00, kObject01, kObject02,
    kObject03, kObject04, kObject05, kPickupPose, kPickupEffect, kObject06, kObject07, kObject08, kObject09, kObject11,
    kObject12, kStepHook, kArriveHook, kCount
};
static_assert(sizeof kClones10 / sizeof kClones10[0] == kCount, "one role per clone");

// Scena10_Objects' entries take (object, bits): typed stand-ins, one per index,
// the table a region (as Scena09_CellHooks' above).
template <unsigned I> void __cdecl ObjectEntry10(unsigned char* object, std::uint32_t row) {
    sh::Record(0x55B6D0, I, Key(object), row);
    sh::Stir();
}
using ObjectFn = void (__cdecl*)(unsigned char*, std::uint32_t);
const ObjectFn kObjectEntries10[13] = {
    &ObjectEntry10<0>, &ObjectEntry10<1>, &ObjectEntry10<2>, &ObjectEntry10<3>,  &ObjectEntry10<4>,
    &ObjectEntry10<5>, &ObjectEntry10<6>, &ObjectEntry10<7>, &ObjectEntry10<8>,  &ObjectEntry10<9>,
    &ObjectEntry10<10>, &ObjectEntry10<11>, &ObjectEntry10<12>,
};

// The helpers the runs call directly whose answer the caller branches on: a
// stand-in of each one's type that answers 0 a third of the time (al), or
// 0xFFFF (ax) for Scena10_MsgByMemberC. Each logs the step, the run and
// counter 0, as a kPhase recorder logs the chapter bytes.
std::uint32_t ChapterBytes() {
    return static_cast<std::uint32_t>(B(at::kStep)) | static_cast<std::uint32_t>(B(at::kRun)) << 8 |
           static_cast<std::uint32_t>(B(at::kCounters)) << 16;
}
unsigned char __cdecl HasItemStandIn() {
    sh::Record(0x559900, ChapterBytes());
    sh::Stir();
    const std::uint32_t n = sh::Noise();
    return static_cast<unsigned char>(n % 3 == 0 ? 0 : (n | 0x10));
}
unsigned char __cdecl TallyStandIn() {
    sh::Record(0x559930, ChapterBytes());
    sh::Stir();
    const std::uint32_t n = sh::Noise();
    return static_cast<unsigned char>(n % 3 == 0 ? 0 : (n | 0x10));
}
unsigned short __cdecl MsgCStandIn() {
    sh::Record(0x55B650, ChapterBytes());
    sh::Stir();
    const std::uint32_t n = sh::Noise();
    return static_cast<unsigned short>(n % 3 == 0 ? 0xFFFF : n);
}

// Item_NamePtr's answer: a record of 16 bytes Scena10_Object05 copies to
// Text_Records - this file's own buffer, filled from the recorders' stream.
unsigned char g_name[16];
std::uint32_t NameRecord(const std::uint32_t*, std::uint32_t) {
    sh::FillBytes(g_name, sizeof g_name);
    return Key(g_name);
}

const sh::Callee kCallees10[] = {
    // tested on al alone (test al, al): garbage above a 0 must not matter
    {SC9B_OURS(Flags_Test), 2, {kAll, kU8}, sh::Answer::kFlag, 0, 0},
    // group SE's (round ten): the event battle's set-up by the id's byte
    {SC9B_OURS(Field_StartEventBattle), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    // group SE's: the event op 0x handed Scena10_SpriteScript
    {SC9B_OURS(EventOp_0x), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    // group SC7's: a party member with +0x89 8 or 9 in state 2, tested in al
    {SC9B_OURS(Scena07_PartyHas89State2), 0, {}, sh::Answer::kFlag, 0, 0},
    // the item pushed as eax with al set, garbage above: bytes read (char_stats.cpp)
    {SC9B_OURS(Inventory_Count), 3, {kU8, kU8, kU8}, sh::Answer::kFlag, 0, 0},
    {SC9B_OURS(Inventory_Add), 3, {kU8, kU8, kU8}, sh::Answer::kFlag, 0, 0},
    {SC9B_OURS(Item_NamePtr), 2, {kU8, kU8}, sh::Answer::kGarbage, 0, 0, {}, &NameRecord},
    // nobody's (group SX's this wave)
    {SC9B_RAW("0x591900", at::kKeyItemAdd), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC9B_RAW("0x587B80", at::kMusicStop), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC9B_RAW("0x57CD90", at::kSpriteFindFree), 0, {}, sh::Answer::kByte, 0xFF, 0x1D},
    // this group's own, called directly (E8)
    {SC9B_OURS(Scena10_StartRun1), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC9B_OURS(Scena10_SpriteOp), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC9B_OURS(Scena10_Shake), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC9B_OURS(Scena10_MsgByMemberA), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC9B_OURS(Scena10_MsgByMemberB), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC9B_OURS(Scena10_HasItem4Eto55), 0, {}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&HasItemStandIn)},
    {SC9B_OURS(Scena10_TallyMet), 0, {}, sh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&TallyStandIn)},
    {SC9B_OURS(Scena10_MsgByMemberC), 0, {}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&MsgCStandIn)},
    // the log slot of the object stand-ins above
    {"Scena10_Objects[i]", 0x55B6D0, 0x55B6D0, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&ObjectEntry10<0>)},
};
#undef SC9B_OURS
#undef SC9B_RAW

// The state and run tables take no arguments: swapped for recorders.
const sh::DataTable kTables10[] = {{at::kStates10, at::kState10Count}, {at::kRuns10, at::kRun10Count}};

// Beyond the standard regions.
const sh::Region kRegions10[] = {
    {at::kShaking, 2},              // Scena10_Shaking, Scena10_Slot
    {at::kMusicCurrent, 1},
    {at::kCondFE, 1},
    {at::kByte904EE0, 1},
    {at::kShiftY, 2},
    {at::kOtSlot, 1},
    {at::kSortOnX, 1},
    {at::kClut, at::kClutWords * 2},
    {at::kTallyWord, 4},            // 0x903A10..13 are the standard pending-change region's
    {at::kTextRecords, 0x10},
    {at::kMoveObject, 4},
    {at::kHold, 1},
    {at::kMoveSpeed3, 1},
    {at::kObjects10, 4 * at::kObject10Count},
    // the pickup records, random each round (the image's words are all below
    // 0x8000, so only random ones tell the zero-extended record word from the
    // sign-extended sprite word: control B5d)
    {at::kPickups, 6 * at::kPickupCount},
};

unsigned RunOf(unsigned k) {
    switch (k) {
    case kRun1: return 1; case kRun2: return 2; case kRun3: return 3; case kRun4: return 4; case kRun5: return 5;
    case kRun6: return 6; case kRun7: return 7; case kRun10: return 10; case kRun11: return 11; case kRun12: return 12;
    case kRun13: return 13;
    default: return 0;
    }
}

// Each run's steps (the cases its switch holds), and one past.
std::uint32_t AStep(unsigned k) {
    switch (k) {
    case kRun1: return sh::Next() % 0xE;
    case kRun2: return sh::Next() % 0x1B;
    case kRun3: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16);
    case kRun4: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1E, 0x1F, 0x20);
    case kRun5: return sh::Next() % 0xC;
    case kRun6: return sh::Next() % 0x34;
    case kRun7: return sh::Next() % 0x16;
    case kRun10: return SH_PICK(0, 1, 1, 2);
    case kRun11: return SH_PICK(0x22, 0x23, 0x23, 0x24);
    case kRun12: return sh::Next() % 7;
    case kRun13: return sh::Next() % 0x1B;
    default: return sh::Next();
    }
}

// The counter-0 value each run's step waits on (read off ours).
struct Wait { unsigned char run, step, count; };
const Wait kWaits[] = {
    {1, 1, 5}, {1, 2, 6}, {1, 3, 7}, {1, 4, 9}, {1, 5, 0x19}, {1, 8, 1}, {1, 9, 9}, {1, 0xA, 0xB}, {1, 0xB, 9}, {1, 0xC, 0},
    {2, 3, 4}, {2, 5, 8}, {2, 8, 0xA}, {2, 0xA, 0x10}, {2, 0xC, 0x13}, {2, 0x13, 0x16}, {2, 0x14, 0x20}, {2, 0x16, 0x22},
    {2, 0x18, 0x25}, {2, 0x19, 8},
    {3, 1, 0xB}, {3, 4, 0}, {3, 0xB, 0xE}, {3, 0xD, 4}, {3, 0xE, 5}, {3, 0xF, 9}, {3, 0x10, 0xB}, {3, 0x15, 0},
    {4, 1, 0xB}, {4, 6, 0}, {4, 8, 0x15}, {4, 0x1F, 0xE},
    {5, 0, 0x1F}, {5, 2, 0x20}, {5, 3, 0x21}, {5, 5, 0x23}, {5, 6, 0x24}, {5, 0xA, 0},
    {6, 4, 6}, {6, 8, 8}, {6, 9, 9}, {6, 0xA, 0xA}, {6, 0xD, 0xD}, {6, 0x10, 0x13}, {6, 0x12, 1}, {6, 0x13, 5},
    {6, 0x15, 2}, {6, 0x17, 3}, {6, 0x19, 6}, {6, 0x1B, 8}, {6, 0x1D, 0xB}, {6, 0x1F, 0xD}, {6, 0x21, 0xF},
    {6, 0x2A, 0x13}, {6, 0x2D, 0x17}, {6, 0x2E, 1}, {6, 0x2F, 2}, {6, 0x32, 0x15},
    {7, 1, 2}, {7, 5, 2}, {7, 7, 3}, {7, 0xB, 2}, {7, 0xD, 5},
    {13, 0, 4}, {13, 1, 0}, {13, 0x10, 0xE}, {13, 0x13, 0x13}, {13, 0x18, 0xC}, {13, 0x19, 0},
};

// A count paired with the step two times in three: the value kWaits lists for
// this run and step, or one off.
void PairCount(unsigned run, unsigned step) {
    for (const Wait& w : kWaits) {
        if (w.run != run || w.step != step) continue;
        if (!sh::Often()) return;
        B(at::kCounters) = sh::Often() ? w.count : static_cast<unsigned char>(w.count + (sh::Half() ? 1 : 0xFF));
        return;
    }
}

// A member byte from one of the three message lists (read in place), so the
// searches find one.
unsigned char AListedKind() {
    switch (sh::Next() % 3) {
    case 0: return B(at::kMsgKindsA + sh::Next() % 4);
    case 1: return B(at::kMsgKindsB + sh::Next() % 5);
    default: return B(at::kMsgKindsC + sh::Next() % 3);
    }
}

// Sprite_Current placed on a pickup record (read in place) two times in three.
void SeedPickup() {
    if (!sh::Often()) return;
    unsigned char* const sprite = sh::Pointer(at::kSpriteCurrent);
    const std::uint32_t rec = at::kPickups + 6 * (sh::Next() % at::kPickupCount);
    std::memcpy(sprite + 0x36, sh::Mem(rec + 2), 2);
    std::memcpy(sprite + 0x3A, sh::Mem(rec + 4), 2);
    if (sh::Half()) sprite[0x3B] = static_cast<unsigned char>(sprite[0x3B] ^ 0x80);   // the sign-extension edge
}

unsigned g_k10;

void Seed10(unsigned k) {
    g_k10 = k;
    g_rect = nullptr;
    for (unsigned i = 0; i < at::kObject10Count; ++i) SetD(at::kObjects10 + 4 * i, KeyOf(kObjectEntries10[i]));
    // the member count bounded (Run7's copy and the searches read that many
    // ObjTrio records), the members' bytes from the lists and the tests
    B(at::kMemberCount) = static_cast<unsigned char>(sh::Next() % 5);
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const r = sh::Mem(at::kObjTrio + m * at::kObjStride);
        if (sh::Often()) r[at::kMemberKind] = sh::Half() ? AListedKind() : static_cast<unsigned char>(AKind());
        if (sh::Half()) r[at::kMemberState] = static_cast<unsigned char>(SH_PICK(3, 2, 0));
    }
    sh::SetPointer(at::kMoveObject, sh::SpriteRecord(sh::Next()));
    if (sh::Often()) B(at::kCondFD) = static_cast<unsigned char>(AnFD());
    if (sh::Often()) B(at::kLeaderByte8) = static_cast<unsigned char>(ALeader8());
    if (sh::Often()) B(at::kRequest) = static_cast<unsigned char>(SH_PICK(0, 2, 6));
    if (sh::Half()) SetW(at::kWait, 0);
    if (sh::Often()) SetW(at::kTimer, SH_PICK(0, 1, 2));
    if (sh::Half()) B(at::kHold) = 0;
    if (sh::Half()) SetW(at::kInputHeld, 0);
    if (sh::Often()) B(at::kShaking) = static_cast<unsigned char>(SH_PICK(0, 1, 0x80));
    if (sh::Often()) B(at::kSlot10) = static_cast<unsigned char>(sh::Next() % 20);
    if (B(at::kSlot10) < 20 && sh::Half())
        B(at::kEffects + B(at::kSlot10) * at::kEffectStride) = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3));
    // the tally bytes at 0x903A10.. at and one below each bound
    if (sh::Half()) {
        static const unsigned char kAt[] = {0, 2, 3, 5, 7}, kMin[] = {2, 3, 2, 1, 2};
        for (unsigned i = 0; i < 5; ++i)
            B(at::kTally + kAt[i]) = static_cast<unsigned char>(kMin[i] - (sh::Next() % 5 == 0 ? 1 : 0));
    }
    if (sh::Often()) SetW(at::kCamDist, SH_PICK(0xFD85, 0xFD80, 0xFFF6, 0, 0xFD7B));
    switch (k) {
    case kFrame: B(at::kState) = static_cast<unsigned char>(sh::Next() % 3); break;
    case kRun: B(at::kRun) = static_cast<unsigned char>(sh::Next() % at::kRun10Count); break;
    case kObjectTrigger: sh::SpriteRecord(0)[0x86] = static_cast<unsigned char>(sh::Next() % at::kObject10Count); break;
    case kEnterArea:
        if (sh::Often()) SetW(at::kArea, SH_PICK(0x52, 0x5E, 0x62, 0x69, 0x78, 0x79, 0x80, 0x83, 0x84, 0x4B));
        break;
    case kStepHook: SeedRect(kRectsStep10, sizeof kRectsStep10 / sizeof kRectsStep10[0]); break;
    case kArriveHook: SeedRect(kRectsArrive10, sizeof kRectsArrive10 / sizeof kRectsArrive10[0]); break;
    case kObject05: case kPickupPose: SeedPickup(); break;
    case kObject08: B(at::kStep) = static_cast<unsigned char>(sh::Next()); break;
    default:
        if (RunOf(k)) {
            B(at::kStep) = static_cast<unsigned char>(AStep(k));
            PairCount(RunOf(k), B(at::kStep));
        }
        break;
    }
}

void Args10(unsigned k, std::uint32_t* a) {
    if (k == kObjectTrigger) a[0] = Key(sh::SpriteRecord(0));
    if (k == kStepHook || k == kArriveHook) ArgsRect(a);
}

// After a call, beyond the harness's own, drawn from `h` alone: counter 0, the
// area, Cond_ByteFD, the shake flag and the slot, an effect's in-use byte, the
// member count and bytes, the kind-2 position, the sprite's +0xB, the request
// byte, the hold.
void Disturb10(std::uint32_t h) {
    const unsigned char v = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 12) {
    case 0: B(at::kCounters) = static_cast<unsigned char>((h >> 24) & 1 ? v % 0x26 : v); break;
    case 1: {
        static const std::uint16_t kAreas[] = {0x52, 0x5E, 0x62, 0x69, 0x78, 0x79, 0x80, 0x83, 0x84, 0x4B, 0x3A};
        SetW(at::kArea, kAreas[(h >> 16) % 11]);
        break;
    }
    case 2: B(at::kCondFD) = static_cast<unsigned char>(v % 6); break;
    case 3: B(at::kShaking) = static_cast<unsigned char>(v & 1); break;
    case 4: B(at::kSlot10) = static_cast<unsigned char>(v % 20); break;
    case 5: B(at::kEffects + ((h >> 12) % 20) * at::kEffectStride) = static_cast<unsigned char>(v & 3); break;
    case 6: B(at::kMemberCount) = static_cast<unsigned char>(v % 5); break;
    case 7: B(at::kObjTrio + ((h >> 12) % 3) * at::kObjStride + at::kMemberKind) = static_cast<unsigned char>(v % 10); break;
    case 8: SetD((h >> 24) & 1 ? at::kKind2X : at::kKind2Z, GetD(at::kKind2X) ^ (static_cast<std::uint32_t>(v) << 16)); break;
    case 9: sh::Pointer(at::kSpriteCurrent)[0xB] = v; break;
    case 10: B(at::kRequest) = static_cast<unsigned char>((h >> 24) & 1 ? 2 : v % 7); break;
    default: B(at::kHold) = static_cast<unsigned char>(v & 1); break;
    }
}

// After every disturbance (two calls in three), for the functions that read a
// cell again after a call, that cell half the time: the area and Cond_ByteFD
// (the area entry and the hooks), counter 0 (the runs), the kind-2 position
// (run 6), the member count (run 7), the sprite's +0xB (object 5), the step
// (object 8 and run 7). Noise() is the recorders' stream, the same on both
// passes.
void Settle10() {
    const std::uint32_t n = sh::Noise();
    if (!(n & 1)) return;
    switch (g_k10) {
    case kEnterArea:
    case kStepHook:
    case kArriveHook: {
        static const std::uint16_t kAreas[] = {0x52, 0x5E, 0x62, 0x69, 0x78, 0x79, 0x80, 0x83, 0x84, 0x4B, 0x3A};
        if (n & 2) SetW(at::kArea, kAreas[(n >> 8) % 11]);
        if (n & 4) B(at::kCondFD) = static_cast<unsigned char>((n >> 16) % 6);
        break;
    }
    case kRun6:
        if (n & 2) SetD(at::kKind2X, n >> 4);
        if (n & 4) SetD(at::kKind2Z, n >> 6);
        if (n & 8) B(at::kCounters) = static_cast<unsigned char>(n >> 24);
        break;
    case kRun7:
        if (n & 2) B(at::kMemberCount) = static_cast<unsigned char>((n >> 8) % 5);
        if (n & 4) B(at::kStep) = static_cast<unsigned char>((n >> 16) % 0x16);
        break;
    case kObject05: sh::Pointer(at::kSpriteCurrent)[0xB] = static_cast<unsigned char>(n >> 8); break;
    case kObject08: B(at::kStep) = static_cast<unsigned char>(n >> 8); break;
    case kPickupEffect: sh::Pointer(at::kSpriteCurrent)[0xB] = static_cast<unsigned char>(n >> 8); break;
    default:
        if (RunOf(g_k10) && (n & 2)) B(at::kCounters) = static_cast<unsigned char>(n >> 8);
        break;
    }
}

}  // namespace

void SelfTest() {
    sh::Group g9 = {"scena_sc9b",
                    kClones9,
                    k9Count,
                    kCallees9,
                    sizeof kCallees9 / sizeof kCallees9[0],
                    nullptr,
                    0,
                    kRegions9,
                    sizeof kRegions9 / sizeof kRegions9[0],
                    &Seed9,
                    &Disturb9,
                    6000};
    g9.args = &Args9;
    g9.settle = &Settle9;
    g9.chapter = 9;
    sh::Run(g9);

    g_sc9b_run6_copy = CopyWithTramps("Scena10_Run6", 0x559B20, 0xFB0, kCalls559B20, SH_N(kCalls559B20), kTables559B20,
                                      SH_N(kTables559B20));
    sh::Group g10 = {"scena_sc9b",
                     kClones10,
                     kCount,
                     kCallees10,
                     sizeof kCallees10 / sizeof kCallees10[0],
                     kTables10,
                     sizeof kTables10 / sizeof kTables10[0],
                     kRegions10,
                     sizeof kRegions10 / sizeof kRegions10[0],
                     &Seed10,
                     &Disturb10,
                     6000};
    g10.args = &Args10;
    g10.settle = &Settle10;
    g10.chapter = 10;
    sh::Run(g10);
}

}  // namespace scena_sc9b
#undef SH_N
