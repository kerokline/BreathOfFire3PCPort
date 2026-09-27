// BOF3X_SHADOW=scena_sc7: chapters 7 and 8 through the scenario harness
// (scenario_harness.h), once at start-up, as two runs - one per chapter byte
// (a Group takes one chapter). docs/scena_sc7.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC7 --clones, checked
// against the reading) with each clone's call shape; the callees the
// harness's standard set lacks (the unnamed ones, ours the originals call
// directly, Inventory_Add / Item_NamePtr / AreaMap_SetByte with the widths
// they read, Item_NamePtr and Scena07_TakeEffect49 answering pointers into
// state the fuzz compares); typed stand-ins written into the object handler
// tables (they take arguments); the state, run and cell handler tables
// swapped; the regions beyond the standard ones; a seed of each compared
// constant and a disturbance of the chapter cells the harness's own does not
// move.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc7.h"
#include "game/scena_sc7_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc7 {
namespace {

namespace sh = scenario_harness;
using move_script::SetLong;
using move_script::SetWord;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the clone table ---------------------------------------------------------
//
// tools/scenario_rows.py --unit SC7 --clones, 2026-09-27: every jump internal
// or a listed E8 / E9, nothing REFUSED. It lists 52 functions: the 66 starts
// of the band less the 14 jump-table cases pc_funcs / pc_hidden took for
// starts (0x54F380 of 0x54F340's table; 0x54F540, 0x54F560, 0x54F8A0 of
// 0x54F420's; 0x54FA40 of 0x54F9E0's; 0x550950 of 0x550930's; 0x551BA0 of
// 0x551980's; 0x551E40 of 0x551DE0's; 0x551F60 of 0x551F40's; 0x552400 of
// 0x5523E0's; 0x552770 of 0x5525B0's; 0x552DD0 of 0x552D50's; 0x553070 and
// 0x5534E0 of 0x553050's). Five two-level switches keep their byte tables in
// the original, read-only, as the copies read them.
constexpr sh::CallSite kCalls54F0B0[] = {{0x12, 0x57C140}, {0x20, 0x5341A0}, {0x27, 0x531F90}, {0x3A, 0x5725F0}, {0x65, 0x57C140}, {0x7A, 0x57C0F0}, {0x93, 0x56F670}, {0x99, 0x531F90}, {0xA0, 0x587BE0}, {0xB9, 0x57C140}, {0xCE, 0x57C110}, {0xDC, 0x57C110}, {0xFA, 0x57C140}, {0x106, 0x57C7C0}, {0x112, 0x531F90}, {0x130, 0x57C140}, {0x142, 0x579F00}, {0x14C, 0x454590}, {0x154, 0x454810}, {0x15F, 0x5A9949}, {0x167, 0x454810}, {0x176, 0x579F00}, {0x181, 0x579F00}, {0x19A, 0x57C0F0}, {0x1C6, 0x57C140}, {0x1D2, 0x57C7C0}, {0x1DE, 0x5341A0}, {0x1E4, 0x531F90}, {0x210, 0x57C140}, {0x225, 0x57C140}, {0x231, 0x57C7C0}, {0x24A, 0x56F670}, {0x251, 0x531F90}};
constexpr sh::CallSite kCalls54F340[] = {{0x28, 0x4976D0}, {0x4D, 0x495040}, {0x69, 0x56F670}, {0xA0, 0x57C7A0}, {0xAE, 0x57C0F0}};
constexpr sh::JumpTable kTables54F340[] = {{0x14, 0xCC, 4}};
constexpr sh::CallSite kCalls54F420[] = {{0x1A, 0x4976D0}, {0x3F, 0x57C7A0}, {0x63, 0x495040}, {0x6A, 0x587B40}, {0xC0, 0x5341A0}, {0xD9, 0x594E00}, {0x10F, 0x495040}, {0x16A, 0x589810}, {0x1E9, 0x587AE0}, {0x207, 0x589810}, {0x277, 0x587B40}, {0x280, 0x587AE0}, {0x29E, 0x589810}, {0x314, 0x589810}, {0x391, 0x495040}, {0x3B3, 0x587B40}, {0x3D9, 0x495040}, {0x44A, 0x589810}, {0x482, 0x495040}, {0x4B1, 0x495040}, {0x4D0, 0x587740}, {0x4E1, 0x532ED0}, {0x4FD, 0x4410B0}, {0x50A, 0x57C0F0}, {0x520, 0x587740}, {0x528, 0x57C7A0}};
constexpr sh::JumpTable kTables54F420[] = {{0x14, 0x54C, 29}};
constexpr sh::CallSite kCalls54F9E0[] = {{0x50, 0x495040}, {0x74, 0x533E50}, {0x79, 0x587B80}, {0x80, 0x587910}, {0x97, 0x587A00}, {0xA4, 0x587B90}, {0xBD, 0x57C0F0}, {0xD6, 0x594E00}, {0x105, 0x495040}, {0x125, 0x57C7A0}};
constexpr sh::JumpTable kTables54F9E0[] = {{0x13, 0x144, 7}};
constexpr sh::CallSite kCalls54FB40[] = {{0x17, 0x589810}, {0xAC, 0x5734F0}, {0xD5, 0x532ED0}, {0xDC, 0x531F90}, {0xFB, 0x587B40}, {0x118, 0x589810}, {0x18A, 0x589810}, {0x1F5, 0x587740}, {0x249, 0x589810}, {0x2A4, 0x54FE90}, {0x2BE, 0x587AE0}, {0x2E2, 0x4410B0}, {0x2FB, 0x57C0F0}, {0x303, 0x57C7A0}};
constexpr sh::JumpTable kTables54FB40[] = {{0x13, 0x31C, 13}};
constexpr sh::CallSite kCalls54FE90[] = {{0x30, 0x589810}, {0x89, 0x587740}};
constexpr sh::CallSite kCalls54FF40[] = {{0x23, 0x57C0F0}, {0x36, 0x594E00}, {0x47, 0x57C7A0}, {0x7B, 0x594E00}};
constexpr sh::CallSite kCalls54FFE0[] = {{0x2D, 0x5341C0}, {0x55, 0x57C0F0}, {0x5D, 0x57C7A0}, {0xCF, 0x4976D0}, {0xF8, 0x5341C0}, {0xFF, 0x531F90}, {0x121, 0x550CE0}, {0x158, 0x550CE0}, {0x19C, 0x550CE0}, {0x1DE, 0x550CE0}, {0x205, 0x5720C0}, {0x2A7, 0x532ED0}, {0x2AE, 0x4410B0}, {0x2C8, 0x57C0F0}, {0x2DE, 0x594E00}, {0x30C, 0x589810}, {0x353, 0x550CE0}, {0x39A, 0x587740}, {0x3A4, 0x587740}, {0x3D1, 0x550CE0}, {0x49F, 0x587B80}, {0x4A6, 0x4976D0}, {0x4E1, 0x56F670}, {0x51A, 0x4976D0}, {0x571, 0x587740}, {0x594, 0x550CE0}, {0x5AB, 0x550CE0}, {0x5DE, 0x550CE0}, {0x5F5, 0x550CE0}, {0x620, 0x587B40}, {0x66C, 0x550CE0}, {0x683, 0x550CE0}, {0x6B5, 0x589810}, {0x722, 0x589810}, {0x788, 0x5508C0}, {0x79E, 0x57C7A0}, {0x7B7, 0x56D6F0}, {0x7CA, 0x594E00}, {0x7DB, 0x587740}, {0x7F5, 0x550890}, {0x803, 0x550890}};
constexpr sh::JumpTable kTables54FFE0[] = {{0x1A, 0x80C, 40}};
constexpr sh::CallSite kCalls5508C0[] = {{0x12, 0x57CD90}, {0x28, 0x57A010}};
constexpr sh::CallSite kCalls550930[] = {{0x5A, 0x590BB0}, {0x7C, 0x591680}, {0xA9, 0x4976D0}, {0xF6, 0x590BB0}, {0x119, 0x591680}, {0x146, 0x4976D0}, {0x15A, 0x57C0F0}, {0x164, 0x587740}, {0x177, 0x497710}, {0x1A6, 0x57C7A0}};
constexpr sh::JumpTable kTables550930[] = {{0x1C, 0x1BC, 7}};
constexpr sh::CallSite kCalls550B20[] = {{0x12, 0x57C7A0}};
constexpr sh::CallSite kCalls550B70[] = {{0x8, 0x57C0F0}, {0xD, 0x57C7C0}, {0x14, 0x531F90}};
constexpr sh::CallSite kCalls550BB0[] = {{0x8, 0x57C0F0}, {0xF, 0x591900}, {0x14, 0x57C7C0}, {0x1B, 0x531F90}};
constexpr sh::CallSite kCalls550C00[] = {{0x8, 0x57C0F0}, {0xD, 0x57C7C0}, {0x14, 0x531F90}};
constexpr sh::CallSite kCalls550C40[] = {{0x1, 0x57C7C0}, {0x27, 0x57C140}};
constexpr sh::CallSite kCalls550C90[] = {{0x0, 0x57C7C0}, {0x22, 0x57C140}};
constexpr sh::CallSite kCalls550CE0[] = {{0x1, 0x589810}};
constexpr sh::CallSite kCalls550D20[] = {{0x18, 0x57C140}, {0x2D, 0x57C140}, {0x4C, 0x57C7C0}, {0x95, 0x57C140}, {0xA1, 0x550F80}, {0xC7, 0x57C7C0}, {0x104, 0x57C140}, {0x112, 0x4976D0}, {0x174, 0x57C7C0}, {0x17B, 0x4976D0}, {0x1AD, 0x57C140}, {0x1C1, 0x57C140}, {0x1DF, 0x57C7C0}};
constexpr sh::CallSite kCalls550F20[] = {{0x12, 0x57C140}, {0x35, 0x57C7C0}, {0x3C, 0x587BE0}};
constexpr sh::CallSite kCalls550FD0[] = {{0x11, 0x56D800}};
constexpr sh::CallSite kCalls551000[] = {{0x0, 0x57C7C0}};
constexpr sh::CallSite kCalls551030[] = {{0x13, 0x551060}, {0x18, 0x553730}, {0x24, 0x57C0F0}};
constexpr sh::CallSite kCalls551060[] = {{0x1D, 0x533E50}};
constexpr sh::CallSite kCalls5510A0[] = {{0x20, 0x57C140}, {0x35, 0x57C0F0}, {0x3C, 0x5341A0}, {0x43, 0x531F90}, {0x70, 0x57C140}, {0x7C, 0x57C7C0}, {0x83, 0x531F90}, {0x9F, 0x5720C0}, {0xA5, 0x5725F0}, {0xAD, 0x56F670}, {0xEF, 0x57C140}, {0x104, 0x57C140}, {0x110, 0x589810}, {0x180, 0x57C140}, {0x18E, 0x5341A0}, {0x1B4, 0x57C140}, {0x1C9, 0x57C140}, {0x1D5, 0x57C7C0}, {0x224, 0x57C140}, {0x238, 0x57C140}, {0x244, 0x57C7C0}, {0x25F, 0x5341A0}, {0x266, 0x531F90}, {0x281, 0x57C140}, {0x29C, 0x579F00}, {0x2D5, 0x57C140}, {0x2EE, 0x57C140}, {0x2FC, 0x587A20}, {0x304, 0x454810}, {0x30F, 0x5A9949}, {0x317, 0x454810}, {0x322, 0x531F90}, {0x33E, 0x5720C0}, {0x344, 0x5725F0}, {0x349, 0x56F670}, {0x357, 0x57C0F0}, {0x367, 0x57C140}, {0x37C, 0x57C140}, {0x388, 0x57C7C0}, {0x38F, 0x531F90}, {0x3AB, 0x5720C0}, {0x3B1, 0x5725F0}, {0x3B6, 0x56F670}, {0x3C6, 0x57C0F0}, {0x3FB, 0x57C140}, {0x407, 0x57C7C0}, {0x415, 0x531F90}, {0x446, 0x57C140}, {0x454, 0x531F90}, {0x46C, 0x57C140}, {0x47A, 0x5341A0}, {0x481, 0x531F90}, {0x4A1, 0x57C140}, {0x4AF, 0x5341A0}, {0x4B6, 0x531F90}, {0x4C4, 0x57C0F0}};
constexpr sh::CallSite kCalls551590[] = {{0x2B, 0x587AE0}, {0x7B, 0x589810}, {0xC0, 0x5720C0}, {0xF1, 0x589810}, {0x134, 0x495040}, {0x165, 0x587BE0}, {0x185, 0x587B40}, {0x1D7, 0x5341A0}, {0x1EA, 0x594E00}, {0x21C, 0x594E00}, {0x230, 0x57C0F0}, {0x273, 0x587740}, {0x281, 0x495040}, {0x2C3, 0x594E00}, {0x2D1, 0x57C0F0}, {0x2F4, 0x57C7A0}};
constexpr sh::JumpTable kTables551590[] = {{0x16, 0x310, 13}};
constexpr sh::CallSite kCalls5518E0[] = {{0x31, 0x57C0F0}};
constexpr sh::CallSite kCalls551980[] = {{0x23, 0x531F90}, {0x80, 0x5720C0}, {0x98, 0x5720C0}, {0x9E, 0x5725F0}, {0xC2, 0x589810}, {0x107, 0x5720C0}, {0x140, 0x587BE0}, {0x17D, 0x532ED0}, {0x184, 0x587B40}, {0x210, 0x587740}, {0x256, 0x587B80}, {0x25D, 0x4976D0}, {0x288, 0x587B90}, {0x292, 0x587740}, {0x2BD, 0x4410B0}, {0x2CF, 0x454590}, {0x2D7, 0x454810}, {0x2E0, 0x517290}, {0x2E7, 0x5A9949}, {0x2EF, 0x454810}, {0x2FA, 0x531F90}, {0x323, 0x589810}, {0x379, 0x587AE0}, {0x3CC, 0x57C0F0}, {0x3D8, 0x57C0F0}, {0x3E5, 0x57C0F0}, {0x3F6, 0x57C7A0}};
constexpr sh::JumpTable kTables551980[] = {{0x13, 0x40C, 19}};
constexpr sh::CallSite kCalls551DE0[] = {{0x24, 0x531F90}, {0x42, 0x57C7A0}, {0x4F, 0x57C0F0}, {0x7D, 0x531F90}, {0x97, 0x4976D0}, {0xB8, 0x57C7A0}, {0xE5, 0x57C0F0}, {0xF0, 0x591BE0}, {0x101, 0x57C7A0}, {0x114, 0x594E00}};
constexpr sh::JumpTable kTables551DE0[] = {{0x18, 0x128, 11}};
constexpr sh::CallSite kCalls551F40[] = {{0x31, 0x594E00}, {0x57, 0x589810}, {0x9F, 0x594E00}, {0xDA, 0x594E00}, {0x10D, 0x533E50}, {0x11B, 0x57C0F0}, {0x12E, 0x594E00}, {0x13F, 0x57C7A0}};
constexpr sh::JumpTable kTables551F40[] = {{0x1C, 0x158, 7}};
constexpr sh::CallSite kCalls552120[] = {{0x1A, 0x587BE0}, {0x50, 0x587B40}, {0x5E, 0x531F90}, {0x81, 0x589810}, {0xF4, 0x589810}, {0x11E, 0x589810}, {0x19E, 0x589810}, {0x1FF, 0x532ED0}, {0x205, 0x531F90}, {0x24A, 0x4410B0}, {0x263, 0x57C7A0}, {0x270, 0x57C0F0}};
constexpr sh::JumpTable kTables552120[] = {{0x14, 0x290, 9}};
constexpr sh::CallSite kCalls5523E0[] = {{0x3C, 0x531F90}, {0x64, 0x57C0F0}, {0x6C, 0x57C7A0}, {0x90, 0x594E00}, {0xB2, 0x495040}, {0xD7, 0x533E50}, {0xDC, 0x587B80}, {0xE2, 0x587910}, {0xF3, 0x587A00}, {0x100, 0x587B90}, {0x10E, 0x495040}, {0x132, 0x57C0F0}, {0x17D, 0x57C7A0}};
constexpr sh::JumpTable kTables5523E0[] = {{0x1C, 0x190, 9}};
constexpr sh::CallSite kCalls5525B0[] = {{0x37, 0x594E00}, {0x69, 0x4976D0}, {0x9C, 0x587AE0}, {0xA3, 0x495040}, {0xDF, 0x589810}, {0x144, 0x495040}, {0x188, 0x57C0F0}, {0x1A3, 0x594E00}, {0x1DE, 0x495040}, {0x1F3, 0x57C7A0}, {0x220, 0x57C7A0}, {0x227, 0x531F90}, {0x245, 0x589810}, {0x2AC, 0x587BE0}, {0x2D2, 0x587B40}, {0x2DC, 0x587740}, {0x306, 0x589810}, {0x3A9, 0x5341C0}, {0x3B1, 0x552CA0}, {0x3C1, 0x531F90}, {0x3DF, 0x589810}, {0x449, 0x589810}, {0x49C, 0x5734F0}, {0x4C1, 0x589810}, {0x528, 0x57C7A0}, {0x52F, 0x587B40}, {0x53D, 0x57C0F0}, {0x57C, 0x587AE0}, {0x5E3, 0x531F90}, {0x608, 0x57C0F0}, {0x62F, 0x5341A0}, {0x664, 0x5720C0}};
constexpr sh::JumpTable kTables5525B0[] = {{0x14, 0x680, 28}};
constexpr sh::CallSite kCalls552CA0[] = {{0x0, 0x57CD90}, {0x28, 0x57CD90}, {0x71, 0x57AD10}, {0x7A, 0x589200}, {0x92, 0x57AD10}, {0x9B, 0x589200}};
constexpr sh::CallSite kCalls552D50[] = {{0x2C, 0x5734F0}, {0x8F, 0x57C7A0}, {0xA5, 0x594E00}, {0xB2, 0x57C0F0}, {0xCA, 0x531F90}, {0xE8, 0x589810}, {0x136, 0x57C7A0}, {0x140, 0x587740}, {0x14E, 0x57C0F0}, {0x161, 0x594E00}};
constexpr sh::JumpTable kTables552D50[] = {{0x14, 0x178, 8}};
constexpr sh::CallSite kCalls552EF0[] = {{0x1F, 0x57C7A0}, {0x26, 0x531F90}, {0x3B, 0x589810}, {0xA5, 0x589810}, {0x10D, 0x57C7A0}, {0x11B, 0x57C0F0}};
constexpr sh::JumpTable kTables552EF0[] = {{0x1B, 0x130, 5}};
constexpr sh::CallSite kCalls553050[] = {{0x22, 0x531F90}, {0x2D, 0x536850}, {0x84, 0x454810}, {0x9E, 0x589810}, {0xF0, 0x536890}, {0x11A, 0x454810}, {0x129, 0x5341A0}, {0x130, 0x531F90}, {0x15A, 0x589810}, {0x18A, 0x587740}, {0x1DC, 0x587B40}, {0x1E3, 0x4976D0}, {0x20A, 0x553630}, {0x29E, 0x531F90}, {0x2AC, 0x495040}, {0x2BD, 0x532ED0}, {0x2DD, 0x4410B0}, {0x2FE, 0x531F90}, {0x312, 0x57C0F0}, {0x32B, 0x495040}, {0x353, 0x5341A0}, {0x369, 0x594E00}, {0x3A2, 0x587AE0}, {0x3A9, 0x495040}, {0x3D5, 0x587B40}, {0x3DC, 0x587910}, {0x3ED, 0x587A00}, {0x3FC, 0x495040}, {0x424, 0x4976D0}, {0x49F, 0x587AE0}, {0x4AD, 0x495040}, {0x4D0, 0x5720C0}, {0x4D6, 0x5725F0}, {0x4DE, 0x56F670}, {0x4F7, 0x57C7A0}, {0x4FC, 0x56D6F0}};
constexpr sh::JumpTable kTables553050[] = {{0x1C, 0x510, 23}};
constexpr sh::CallSite kCalls553630[] = {{0x11, 0x498DE0}, {0x37, 0x590660}, {0x42, 0x590CE0}, {0x4D, 0x590D70}};
constexpr sh::CallSite kCalls5536B0[] = {{0x7, 0x57C7C0}};
constexpr sh::CallSite kCalls5536E0[] = {{0x7, 0x57C7C0}};
constexpr sh::CallSite kCalls553700[] = {{0x7, 0x57C7C0}, {0x22, 0x57C0F0}};
constexpr sh::CallSite kCalls553750[] = {{0x36, 0x57C140}, {0x42, 0x57C7C0}, {0x63, 0x57C140}, {0x6F, 0x57C7C0}, {0xAF, 0x57C0F0}};
constexpr sh::CallSite kCalls553810[] = {{0x1D, 0x57C140}, {0x47, 0x57C7C0}, {0x6C, 0x57C140}, {0x85, 0x57C140}, {0xB1, 0x57C7C0}, {0xEF, 0x57C140}, {0x103, 0x57C0F0}, {0x10D, 0x587740}, {0x12C, 0x57C140}, {0x144, 0x57C140}, {0x16D, 0x57C7C0}, {0x19E, 0x57C140}, {0x1B7, 0x57C0F0}, {0x1E1, 0x57C140}, {0x1FA, 0x57C0F0}, {0x225, 0x587740}, {0x22D, 0x57C7C0}, {0x259, 0x57C140}, {0x29B, 0x57C7C0}, {0x2B9, 0x57C140}, {0x2CE, 0x57C140}, {0x2EB, 0x57C7C0}};
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define SC7_OURS(f) reinterpret_cast<const void*>(&::f)
constexpr sh::Shape kSlot = sh::Shape::kSlot, kState = sh::Shape::kState, kHook = sh::Shape::kHook, kEntry = sh::Shape::kEntry,
                    kObject = sh::Shape::kObject;

// Chapter 7's 27, fuzzed with Cond_ByteFA 7 (its row 0x903FC8). Shapes: the
// frame kSlot; the states, runs, helpers and the cell's handler kState; the
// object hook kObject; the object handlers and Scena07_TakeEffect49 kEntry;
// the hooks kHook ((x, z), al); Scena07_PartyHas89State2 answers in al.
const sh::Clone kClones7[] = {
    {"Scena07_Frame", 0x54F080, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Frame), 0, false, kSlot},
    {"Scena07_Start", 0x54F090, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Start), 0, false, kState},
    {"Scena07_EnterArea", 0x54F0B0, 0x276, kCalls54F0B0, SH_N(kCalls54F0B0), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_EnterArea), 0, false, kState},
    {"Scena07_Run", 0x54F330, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Run), 0, false, kState},
    {"Scena07_Scene1", 0x54F340, 0xDC, kCalls54F340, SH_N(kCalls54F340), nullptr, 0, kTables54F340, SH_N(kTables54F340), SC7_OURS(Scena07_Scene1), 0, false, kState},
    {"Scena07_Scene2", 0x54F420, 0x5C0, kCalls54F420, SH_N(kCalls54F420), nullptr, 0, kTables54F420, SH_N(kTables54F420), SC7_OURS(Scena07_Scene2), 0, false, kState},
    {"Scena07_Scene3", 0x54F9E0, 0x160, kCalls54F9E0, SH_N(kCalls54F9E0), nullptr, 0, kTables54F9E0, SH_N(kTables54F9E0), SC7_OURS(Scena07_Scene3), 0, false, kState},
    {"Scena07_Scene4", 0x54FB40, 0x350, kCalls54FB40, SH_N(kCalls54FB40), nullptr, 0, kTables54FB40, SH_N(kTables54FB40), SC7_OURS(Scena07_Scene4), 0, false, kState},
    {"Scena07_TimedEffects", 0x54FE90, 0xAA, kCalls54FE90, SH_N(kCalls54FE90), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_TimedEffects), 0, false, kState},
    {"Scena07_Scene5", 0x54FF40, 0x92, kCalls54FF40, SH_N(kCalls54FF40), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Scene5), 0, false, kState},
    {"Scena07_Scene6", 0x54FFE0, 0x8AC, kCalls54FFE0, SH_N(kCalls54FFE0), nullptr, 0, kTables54FFE0, SH_N(kTables54FFE0), SC7_OURS(Scena07_Scene6), 0, false, kState},
    {"Scena07_ShakeCamera", 0x550890, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena07_ShakeCamera), 0, false, kState},
    {"Scena07_PlaceObjects", 0x5508C0, 0x6C, kCalls5508C0, SH_N(kCalls5508C0), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_PlaceObjects), 0, false, kState},
    {"Scena07_Scene7", 0x550930, 0x1EE, kCalls550930, SH_N(kCalls550930), nullptr, 0, kTables550930, SH_N(kTables550930), SC7_OURS(Scena07_Scene7), 0, false, kState},
    {"Scena07_Scene8", 0x550B20, 0x26, kCalls550B20, SH_N(kCalls550B20), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Scene8), 0, false, kState},
    {"Scena07_ObjectHook", 0x550B50, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena07_ObjectHook), 0, false, kObject},
    {"Scena07_Object0", 0x550B70, 0x3C, kCalls550B70, SH_N(kCalls550B70), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Object0), 0, false, kEntry},
    {"Scena07_Object1", 0x550BB0, 0x43, kCalls550BB0, SH_N(kCalls550BB0), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Object1), 0, false, kEntry},
    {"Scena07_Object2", 0x550C00, 0x3C, kCalls550C00, SH_N(kCalls550C00), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Object2), 0, false, kEntry},
    {"Scena07_Object3", 0x550C40, 0x49, kCalls550C40, SH_N(kCalls550C40), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Object3), 0, false, kEntry},
    {"Scena07_Object4", 0x550C90, 0x44, kCalls550C90, SH_N(kCalls550C90), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Object4), 0, false, kEntry},
    {"Scena07_TakeEffect49", 0x550CE0, 0x32, kCalls550CE0, SH_N(kCalls550CE0), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_TakeEffect49), 0xFFFFFFFF, false, kEntry},
    {"Scena07_StepHook", 0x550D20, 0x1FC, kCalls550D20, SH_N(kCalls550D20), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_StepHook), 0xFF, false, kHook},
    {"Scena07_ArriveHook", 0x550F20, 0x58, kCalls550F20, SH_N(kCalls550F20), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_ArriveHook), 0xFF, false, kHook},
    {"Scena07_PartyHas89State2", 0x550F80, 0x42, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena07_PartyHas89State2), 0xFF, false, kState},
    {"Scena07_CellHook", 0x550FD0, 0x2D, kCalls550FD0, SH_N(kCalls550FD0), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_CellHook), 0xFF, false, kHook},
    {"Scena07_Cell0", 0x551000, 0x14, kCalls551000, SH_N(kCalls551000), nullptr, 0, nullptr, 0, SC7_OURS(Scena07_Cell0), 0, false, kState},
};
// Chapter 8's 25, with Cond_ByteFA 8 (its row 0x903FD0).
const sh::Clone kClones8[] = {
    {"Scena08_Frame", 0x551020, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena08_Frame), 0, false, kSlot},
    {"Scena08_Start", 0x551030, 0x2D, kCalls551030, SH_N(kCalls551030), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_Start), 0, false, kState},
    {"Scena08_PartyBitsToRecord0", 0x551060, 0x40, kCalls551060, SH_N(kCalls551060), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_PartyBitsToRecord0), 0, false, kState},
    {"Scena08_EnterArea", 0x5510A0, 0x4DE, kCalls5510A0, SH_N(kCalls5510A0), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_EnterArea), 0, false, kState},
    {"Scena08_Run", 0x551580, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena08_Run), 0, false, kState},
    {"Scena08_Scene1", 0x551590, 0x344, kCalls551590, SH_N(kCalls551590), nullptr, 0, kTables551590, SH_N(kTables551590), SC7_OURS(Scena08_Scene1), 0, false, kState},
    {"Scena08_Scene2", 0x5518E0, 0x91, kCalls5518E0, SH_N(kCalls5518E0), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_Scene2), 0, false, kState},
    {"Scena08_Scene3", 0x551980, 0x458, kCalls551980, SH_N(kCalls551980), nullptr, 0, kTables551980, SH_N(kTables551980), SC7_OURS(Scena08_Scene3), 0, false, kState},
    {"Scena08_Scene4", 0x551DE0, 0x154, kCalls551DE0, SH_N(kCalls551DE0), nullptr, 0, kTables551DE0, SH_N(kTables551DE0), SC7_OURS(Scena08_Scene4), 0, false, kState},
    {"Scena08_Scene5", 0x551F40, 0x1D9, kCalls551F40, SH_N(kCalls551F40), nullptr, 0, kTables551F40, SH_N(kTables551F40), SC7_OURS(Scena08_Scene5), 0, false, kState},
    {"Scena08_Scene6", 0x552120, 0x2B4, kCalls552120, SH_N(kCalls552120), nullptr, 0, kTables552120, SH_N(kTables552120), SC7_OURS(Scena08_Scene6), 0, false, kState},
    {"Scena08_Scene7", 0x5523E0, 0x1C4, kCalls5523E0, SH_N(kCalls5523E0), nullptr, 0, kTables5523E0, SH_N(kTables5523E0), SC7_OURS(Scena08_Scene7), 0, false, kState},
    {"Scena08_Scene8", 0x5525B0, 0x6F0, kCalls5525B0, SH_N(kCalls5525B0), nullptr, 0, kTables5525B0, SH_N(kTables5525B0), SC7_OURS(Scena08_Scene8), 0, false, kState},
    {"Scena08_SpawnPair", 0x552CA0, 0xA4, kCalls552CA0, SH_N(kCalls552CA0), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_SpawnPair), 0, false, kState},
    {"Scena08_Scene9", 0x552D50, 0x198, kCalls552D50, SH_N(kCalls552D50), nullptr, 0, kTables552D50, SH_N(kTables552D50), SC7_OURS(Scena08_Scene9), 0, false, kState},
    {"Scena08_Scene10", 0x552EF0, 0x151, kCalls552EF0, SH_N(kCalls552EF0), nullptr, 0, kTables552EF0, SH_N(kTables552EF0), SC7_OURS(Scena08_Scene10), 0, false, kState},
    {"Scena08_Scene11", 0x553050, 0x5D1, kCalls553050, SH_N(kCalls553050), nullptr, 0, kTables553050, SH_N(kTables553050), SC7_OURS(Scena08_Scene11), 0, false, kState},
    {"Scena08_SetUpRecord4", 0x553630, 0x56, kCalls553630, SH_N(kCalls553630), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_SetUpRecord4), 0, false, kState},
    {"Scena08_ObjectHook", 0x553690, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena08_ObjectHook), 0, false, kObject},
    {"Scena08_Object0", 0x5536B0, 0x24, kCalls5536B0, SH_N(kCalls5536B0), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_Object0), 0, false, kEntry},
    {"Scena08_Object1", 0x5536E0, 0x1B, kCalls5536E0, SH_N(kCalls5536E0), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_Object1), 0, false, kEntry},
    {"Scena08_Object2", 0x553700, 0x2B, kCalls553700, SH_N(kCalls553700), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_Object2), 0, false, kEntry},
    {"Scena08_SwapKeyItem4", 0x553730, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, SC7_OURS(Scena08_SwapKeyItem4), 0, false, kState},
    {"Scena08_StepHook", 0x553750, 0xBC, kCalls553750, SH_N(kCalls553750), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_StepHook), 0xFF, false, kHook},
    {"Scena08_ArriveHook", 0x553810, 0x311, kCalls553810, SH_N(kCalls553810), nullptr, 0, nullptr, 0, SC7_OURS(Scena08_ArriveHook), 0xFF, false, kHook},
};
constexpr unsigned kCount7 = sizeof kClones7 / sizeof kClones7[0];
constexpr unsigned kCount8 = sizeof kClones8 / sizeof kClones8[0];

// --- the object handler tables ---------------------------------------------------
//
// A .data table's handler recorder logs no arguments, and the object handlers
// take (object, row): a typed stand-in of the fuzz's own is written into each
// entry by the seed (the tables are regions, put back after the run), each
// logging against the handler's own address.
constexpr std::uint32_t kObjAddr7[5] = {0x550B70, 0x550BB0, 0x550C00, 0x550C40, 0x550C90};
constexpr std::uint32_t kObjAddr8[3] = {0x5536B0, 0x5536E0, 0x553700};
template <unsigned I> void __cdecl ObjectEntry7(unsigned char* object, unsigned char* row) {
    sh::Record(kObjAddr7[I], Key(object), Key(row));
    sh::Stir();
}
template <unsigned I> void __cdecl ObjectEntry8(unsigned char* object, unsigned char* row) {
    sh::Record(kObjAddr8[I], Key(object), Key(row));
    sh::Stir();
}
const void* const kObjEntries7[5] = {
    reinterpret_cast<const void*>(&ObjectEntry7<0>), reinterpret_cast<const void*>(&ObjectEntry7<1>),
    reinterpret_cast<const void*>(&ObjectEntry7<2>), reinterpret_cast<const void*>(&ObjectEntry7<3>),
    reinterpret_cast<const void*>(&ObjectEntry7<4>),
};
const void* const kObjEntries8[3] = {
    reinterpret_cast<const void*>(&ObjectEntry8<0>), reinterpret_cast<const void*>(&ObjectEntry8<1>),
    reinterpret_cast<const void*>(&ObjectEntry8<2>),
};

// --- the callees -------------------------------------------------------------------
//
// Beyond the harness's standard 70 (docs/scenario_harness.md section 4), which
// already record every named engine callee these call and 0x532ED0, 0x56D6F0,
// Scenario_CallB, EventOp_6x and Sound_ResumeAll.
constexpr std::uint32_t kAll = 0xFFFFFFFF;
#define SC7_CAPCOM(name, address, n, ...) {name, address, address, n, {__VA_ARGS__}, sh::Answer::kGarbage, 0, 0}
#define SC7_OURS_CALLEE(fn, n, ...) {#fn, ::bof3::addr::fn, Key(reinterpret_cast<const void*>(&::fn)), n, {__VA_ARGS__}, sh::Answer::kGarbage, 0, 0}

sh::Callee Answering(sh::Callee c, sh::Answer a, std::uint8_t lo = 0, std::uint8_t hi = 0) {
    c.answer = a;
    c.lo = lo;
    c.hi = hi;
    return c;
}
sh::Callee WithEffect(sh::Callee c, sh::Effect e) {
    c.effect = e;
    return c;
}
sh::Callee Custom(const char* name, std::uint32_t address, const void* fn) {
    sh::Callee c = {name, address, address, 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0};
    c.custom = fn;
    return c;
}

// Item_NamePtr's answer: a name of 16 bytes of the harness's noise (the same
// on both passes), in a buffer the fuzz compares.
alignas(16) unsigned char g_name[16];
std::uint32_t NameAnswer(const std::uint32_t*, std::uint32_t) {
    sh::FillBytes(g_name, sizeof g_name);
    return Key(g_name);
}
// Scena07_TakeEffect49's answer: one of the 20 Effect_Objects records (its
// callers write through it).
std::uint32_t EffectAnswer(const std::uint32_t*, std::uint32_t answer) {
    return Key(sh::Mem(at::kEffects + (answer % 20) * 0x80));
}

sh::Callee g_callees[64];
unsigned g_n_callees = 0;
void Add(const sh::Callee& c) {
    if (g_n_callees == sizeof g_callees / sizeof g_callees[0]) bof3::Fatal("scena_sc7: more than 64 callees");
    g_callees[g_n_callees++] = c;
}

void ListCallees(int chapter) {
    g_n_callees = 0;
    // Ours, named, that the standard set lists by address or not at all.
    Add(SC7_OURS_CALLEE(Field_StartEventBattle, 1, 0xFF));
    Add(SC7_OURS_CALLEE(EventOp_0x, 1, kAll));
    Add(SC7_OURS_CALLEE(Char_RecalcStats, 1, kAll));
    // The widths the callees read (the originals leave garbage above them).
    Add(Answering(SC7_OURS_CALLEE(Inventory_Add, 3, 0xFF, 0xFF, 0xFF), sh::Answer::kFlag));
    Add(WithEffect(SC7_OURS_CALLEE(Item_NamePtr, 2, 0xFF, 0xFF), &NameAnswer));
    Add(SC7_OURS_CALLEE(AreaMap_SetByte, 3, 0xFFFF, 0xFFFF, 0xFF));
    // Nobody's, by address (scena_sc7_callees.h).
    Add(SC7_CAPCOM("0x533E50", kPartyRestore, 0));
    // an index below n (1) or a negative byte; the caller tests al's sign
    Add(Answering(SC7_CAPCOM("0x56D800", kCellFind, 4, kAll, 0xFF, kAll, kAll), sh::Answer::kByte, 0xFF, 0x00));
    // a Sprite_Objects index 0..0x1D, or 0xFF none
    Add(Answering(SC7_CAPCOM("0x57CD90", kSpriteFindFree, 0), sh::Answer::kByte, 0xFF, 0x1D));
    Add(SC7_CAPCOM("0x587B80", kMusicStop, 0));
    Add(SC7_CAPCOM("0x591900", kKeyItemAdd, 1, 0xFF));
    Add(SC7_CAPCOM("0x591BE0", kCall591BE0, 2, kAll, kAll));
    Add(SC7_CAPCOM("0x498DE0", kCall498DE0, 1, 0xFF));
    // Ours the originals' E8 reach: those whose answer is read answer as their
    // callers read it; the rest log the chapter bytes (kPhase).
    if (chapter == 7) {
        Add(Answering(SC7_OURS_CALLEE(Scena07_TimedEffects, 0), sh::Answer::kPhase));
        Add(Answering(SC7_OURS_CALLEE(Scena07_ShakeCamera, 0), sh::Answer::kPhase));
        Add(Answering(SC7_OURS_CALLEE(Scena07_PlaceObjects, 0), sh::Answer::kPhase));
        Add(WithEffect(SC7_OURS_CALLEE(Scena07_TakeEffect49, 1, 0xFF), &EffectAnswer));
        Add(Answering(SC7_OURS_CALLEE(Scena07_PartyHas89State2, 0), sh::Answer::kFlag));
        static const char* const kNames[5] = {"ObjectHandlers7[0]", "ObjectHandlers7[1]", "ObjectHandlers7[2]",
                                              "ObjectHandlers7[3]", "ObjectHandlers7[4]"};
        for (unsigned i = 0; i < 5; ++i) Add(Custom(kNames[i], kObjAddr7[i], kObjEntries7[i]));
    } else {
        Add(Answering(SC7_OURS_CALLEE(Scena08_PartyBitsToRecord0, 0), sh::Answer::kPhase));
        Add(Answering(SC7_OURS_CALLEE(Scena08_SwapKeyItem4, 0), sh::Answer::kPhase));
        Add(Answering(SC7_OURS_CALLEE(Scena08_SpawnPair, 0), sh::Answer::kPhase));
        Add(Answering(SC7_OURS_CALLEE(Scena08_SetUpRecord4, 0), sh::Answer::kPhase));
        static const char* const kNames[3] = {"ObjectHandlers8[0]", "ObjectHandlers8[1]", "ObjectHandlers8[2]"};
        for (unsigned i = 0; i < 3; ++i) Add(Custom(kNames[i], kObjAddr8[i], kObjEntries8[i]));
    }
}

// --- the tables ------------------------------------------------------------------
//
// Swapped for recorders while the fuzz runs (no arguments). Each chapter's
// state table runs on into its run table (the frame's index 3.. reads runs),
// so one window holds both: the seeds keep the indices inside.
const sh::DataTable kTables7[] = {{at::kStates7, 12}, {at::kCellHandlers7, 1}};
const sh::DataTable kTables8[] = {{at::kStates8, 15}};

// --- the regions -------------------------------------------------------------------
//
// Beyond the harness's 22 standard ones: the cells of the chapters' own and
// the engine cells they touch that no standard region covers.
sh::Region g_regions[24];
unsigned g_n_regions = 0;
void ListRegions(int chapter) {
    const sh::Region common[] = {
        {0x903800, 8},                     // Camera_ShiftY and the sprite pointer 0x903804
        {at::kCharRecords, 8 * at::kRecordStride},   // CharacterRecords 0..7
        {at::kTextRecords, 0x30},          // Text_Records +0..+0x2F
        {at::kAreaTrack, 1},
        {at::kAreaTransition, 1},
        {at::kMusicTrack, 1},
        {at::kByteFE, 1},
        {at::kMessage, 2},
        {at::kActiveMember, 4},
        {at::kKept, 0x10},                 // chapter 8's cells 0x6BC720..
        {0x929F10, 4},                     // 0x929F10, Field_Kind2Hold
        {at::kDrawPool, 0x14},             // 0x9039F0..0x903A03
        {at::kEffectState, 0x10},          // MoveScript_EffectState [0..15]
        {at::kKeyItems, 0x20},
        {Key(g_name), sizeof g_name},
    };
    g_n_regions = 0;
    for (const sh::Region& r : common) g_regions[g_n_regions++] = r;
    if (chapter == 7) g_regions[g_n_regions++] = {at::kObjects7, 5 * 4};
    else g_regions[g_n_regions++] = {at::kObjects8, 3 * 4};
}

// --- the seed ----------------------------------------------------------------------
//
// Each scene's cases (from the reading) and, per case that waits on a
// counter, the counter and the value: {step, counter (0 = 0x903848, 3 =
// 0x90384B), value}. Read off ours (the scratch stepcmp.py, each case's first
// compare), which the fuzz then holds to the originals.
struct StepWait { unsigned char step, counter, value; };
struct Scene { const char* name; unsigned char steps[40]; unsigned n_steps; StepWait waits[16]; unsigned n_waits; };
const Scene kScenes[] = {
    {"Scena07_Scene1", {0x0, 0x1, 0x2, 0x3}, 4, {{0x3, 0, 0x15}}, 1},
    {"Scena07_Scene2", {0x0, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x15, 0x16, 0x19, 0x1A, 0x1C}, 24, {{0x9, 0, 0x3}, {0xA, 0, 0x4}, {0xB, 0, 0x5}, {0xC, 0, 0xA}, {0xD, 0, 0xB}, {0xE, 0, 0xC}, {0xF, 0, 0xD}, {0x11, 0, 0x12}, {0x13, 0, 0x22}, {0x19, 0, 0x45}, {0x1A, 0, 0x46}}, 11},
    {"Scena07_Scene3", {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6}, 7, {{0x0, 0, 0x3}, {0x1, 0, 0xD}, {0x2, 0, 0x10}}, 3},
    {"Scena07_Scene4", {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xC}, 12, {{0x2, 0, 0x2}, {0x3, 0, 0x9}, {0x4, 0, 0xA}, {0x6, 0, 0xC}, {0x7, 0, 0xD}, {0x8, 0, 0xE}, {0x9, 0, 0xF}, {0xA, 0, 0x12}}, 8},
    {"Scena07_Scene5", {0x0, 0x1, 0x2}, 3, {{0x2, 0, 0x9}}, 1},
    {"Scena07_Scene6", {0x0, 0x1, 0x5, 0x6, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27}, 33, {{0x0, 0, 0x5}, {0x1, 0, 0x7}, {0xC, 0, 0x3}, {0xE, 0, 0x7}, {0x12, 0, 0x13}, {0x15, 0, 0x14}, {0x16, 0, 0x15}, {0x1B, 0, 0x1A}, {0x1D, 0, 0x1C}, {0x1F, 0, 0x1E}, {0x20, 0, 0x1F}, {0x24, 0, 0x25}, {0x25, 0, 0x26}, {0x26, 0, 0x27}, {0x27, 0, 0x29}}, 15},
    {"Scena07_Scene7", {0x0, 0x1, 0xA, 0xC, 0x14, 0x15}, 6, {{0x1, 0, 0x1F}, {0xC, 0, 0x24}}, 2},
    {"Scena07_Scene8", {}, 0, {}, 0},
    {"Scena08_Scene1", {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC}, 13, {{0x2, 0, 0x4}, {0x3, 0, 0x7}, {0x4, 0, 0x10}, {0x7, 0, 0x16}, {0xB, 0, 0x1D}}, 5},
    {"Scena08_Scene2", {0x0, 0x1, 0x2}, 3, {{0x1, 3, 0x12}}, 1},
    {"Scena08_Scene3", {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xE, 0xF, 0x10, 0x11, 0x12}, 18, {{0x2, 0, 0x5}, {0x3, 0, 0x6}, {0x5, 0, 0xB}, {0xA, 0, 0x17}, {0xC, 0, 0x19}, {0xF, 0, 0x23}, {0x12, 0, 0x0}}, 7},
    {"Scena08_Scene4", {0x0, 0x1, 0x2, 0x5, 0x6, 0xA}, 6, {{0x1, 0, 0x0}, {0xA, 0, 0xA}}, 2},
    {"Scena08_Scene5", {0x0, 0x64, 0x1, 0x2, 0x3, 0x4}, 6, {{0x1, 0, 0x1}, {0x2, 0, 0x2}, {0x3, 0, 0x6}, {0x4, 0, 0x8}}, 4},
    {"Scena08_Scene6", {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x8}, 8, {{0x2, 0, 0x1}, {0x6, 0, 0x5}}, 2},
    {"Scena08_Scene7", {0x0, 0x1, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}, 8, {{0x1, 0, 0x0}, {0xB, 0, 0xC}, {0xF, 0, 0x0}}, 3},
    {"Scena08_Scene8", {0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0xA, 0xB, 0xC, 0xD, 0xE, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B}, 22, {{0x4, 0, 0x2}, {0x5, 0, 0x5}, {0xB, 0, 0x2}, {0xD, 0, 0x6}, {0x15, 0, 0xA}, {0x16, 0, 0x12}, {0x17, 0, 0x16}, {0x18, 0, 0x1A}, {0x1B, 0, 0x0}}, 9},
    {"Scena08_Scene9", {0x0, 0x1, 0x2, 0x3, 0x5, 0x6, 0x7}, 7, {{0x0, 0, 0x8}, {0x1, 0, 0x14}, {0x3, 0, 0x0}, {0x6, 0, 0x5}}, 4},
    {"Scena08_Scene10", {0x0, 0xA, 0xB, 0xC}, 4, {{0xB, 0, 0x6}}, 1},
    {"Scena08_Scene11", {0x0, 0x1, 0x2, 0x64, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15}, 22, {{0x1, 0, 0x1}, {0x3, 0, 0x3}, {0x4, 0, 0x8}, {0x6, 0, 0x17}, {0xA, 0, 0x1A}, {0xD, 0, 0x1E}, {0x10, 0, 0x3A}}, 7},
};
const Scene* SceneOf(const char* name) {
    for (const Scene& s : kScenes)
        if (std::strcmp(s.name, name) == 0) return &s;
    return nullptr;
}

std::uint32_t PickTimer() {
    return SH_PICK(0, 0, 1, 1, 2, 3, 8, 0xA, 0xC, 0x10, 0x1E, 0x20, 0x30, 0x3C, 0x61, 0x78, 0x87, 0x88, 0x89, 0x96, 0xFFFF);
}
std::uint32_t PickArea7() { return SH_PICK(0x35, 0x4A, 0x52, 0x55, 0x67, 0x69, 0xAF, 0x34, 0x54, 0xAE); }
std::uint32_t PickArea8() { return SH_PICK(0, 1, 2, 3, 0xC, 0xD, 0xF, 0x12, 0x15, 0x20, 0x23, 0x2B, 0x46, 0x14, 0x21); }
// The counter 0 values chapter 7's / 8's code compares (step-paired below).
std::uint32_t PickC0(int chapter) {
    if (chapter == 7)
        return SH_PICK(0, 1, 3, 4, 5, 7, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1C, 0x1E, 0x1F,
                       0x22, 0x24, 0x25, 0x26, 0x27, 0x29, 0x45, 0x46);
    return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 0xA, 0xB, 0xC, 0x10, 0x12, 0x14, 0x16, 0x17, 0x19, 0x1A, 0x1D, 0x1E, 0x23, 0x3A);
}

// A 16.16 coordinate: an exact value, or a cell in lo..hi (one cell either
// side a quarter of the time) with the fraction 0, 0x8000 or any.
std::uint32_t Coord(std::uint32_t lo, std::uint32_t hi, bool exact) {
    if (exact) return sh::Next() % 8 ? lo : lo + (sh::Half() ? 0x8000u : 0xFFFF8000u);
    std::uint32_t cell = lo + sh::Next() % (hi - lo + 1);
    if (sh::Next() % 4 == 0) cell = sh::Half() ? lo - 1 : hi + 1;
    const std::uint32_t frac = sh::Half() ? 0 : (sh::Half() ? 0x8000 : (sh::Next() & 0xFFFF));
    return (cell & 0xFFFF) << 16 | frac;
}
// The hooks' (x, z) where they turn: an exact coordinate or a cell range.
struct Hit { std::uint32_t x0, x1, z0, z1; bool x_exact, z_exact; };
struct HookHits { const char* name; Hit hits[10]; unsigned n; };
const HookHits kHits[] = {
    {"Scena07_StepHook", {{0x348000, 0, 0x13, 0x16, true, false}, {0x22, 0x25, 0x3C, 0x40, false, false},
                          {0x25, 0x26, 0x1C8000, 0, false, true}, {0x10, 0x30, 0x1C, 0x1F, false, false},
                          {0x18, 0x2D, 0x20, 0x24, false, false}}, 5},
    {"Scena07_ArriveHook", {{0x4A, 0x4B, 0x1F, 0x20, false, false}, {0x4A, 0x4B, 0x200000, 0, false, true}}, 2},
    {"Scena08_StepHook", {{0x338000, 0, 0x27, 0x28, true, false}, {0x388000, 0, 0x2E, 0x31, true, false}}, 2},
    {"Scena08_ArriveHook", {{0x1C, 0x1D, 0x2B, 0x2C, false, false}, {0x2F0000, 0, 0x1E, 0x1F, true, false},
                            {0x2F0000, 0, 0x1F0000, 0, true, true}, {0x1B0000, 0, 0x110000, 0, true, true},
                            {0x160000, 0, 0xD0000, 0, true, true}, {0x110000, 0, 0x130000, 0, true, true},
                            {0x70000, 0, 0x130000, 0, true, true}, {0x1C, 0x1D, 2, 3, false, false},
                            {0x45, 0x47, 0x37, 0x39, false, false}, {0x20, 0x25, 0xE, 0x10, false, false}}, 10},
};

const sh::Clone* g_clones = kClones7;
int g_chapter = 7;
unsigned g_k = 0;
bool Named(const char* part) { return std::strstr(g_clones[g_k].name, part) != nullptr; }

void Seed(unsigned k) {
    g_k = k;
    const bool eight = g_chapter == 8;
    // the tables' indices, inside the swapped windows
    sh::Mem(at::kState)[0] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 3 : sh::Next() % (eight ? 15 : 12));
    sh::Mem(at::kRun)[0] = static_cast<unsigned char>(sh::Next() % (eight ? 12 : 9));
    // the step: a case of the scene, else any byte below 0x70
    const Scene* const scene = SceneOf(g_clones[k].name);
    if (scene && scene->n_steps && sh::Next() % 6) {
        const unsigned step = scene->steps[sh::Next() % scene->n_steps];
        sh::Mem(at::kStep)[0] = static_cast<unsigned char>(sh::Next() % 8 ? step : step + 1);
    } else if (sh::Half()) {
        sh::Mem(at::kStep)[0] = static_cast<unsigned char>(sh::Next() % 0x70);
    }
    if (sh::Often()) SetWord(sh::Mem(at::kTimer), PickTimer());
    // the counters the steps wait on
    if (sh::Next() % 6) sh::Mem(at::kCounter0)[0] = static_cast<unsigned char>(PickC0(g_chapter));
    if (scene && scene->n_waits && sh::Often()) {
        const StepWait& w = scene->waits[sh::Next() % scene->n_waits];
        sh::Mem(at::kStep)[0] = w.step;
        const unsigned jitter = sh::Next() % 5;
        sh::Mem(w.counter ? at::kCounter3 : at::kCounter0)[0] =
            static_cast<unsigned char>(w.value + (jitter == 0 ? 1u : jitter == 1 ? 0xFFu : 0u));
    }
    // counter 3 names a sprite (chapter 8 run 4) or waits for 0x12 (run 2)
    if (sh::Often()) sh::Mem(at::kCounter3)[0] = static_cast<unsigned char>(sh::Half() ? sh::Next() % 4 : SH_PICK(0x11, 0x12, 0x13));
    // the area, Cond_ByteFD, the bytes the entries and hooks test
    if (sh::Next() % 6) SetWord(sh::Mem(at::kArea), eight ? PickArea8() : PickArea7());
    if (sh::Next() % 6) sh::Mem(at::kByteFD)[0] = static_cast<unsigned char>(sh::Next() % 6);
    // the waits
    if (sh::Half()) sh::Mem(at::kRequest)[0] = static_cast<unsigned char>(sh::Half() ? 2 : SH_PICK(0, 0, 1, 5, 6));
    if (sh::Half()) SetWord(sh::Mem(at::kWait), 0);
    if (sh::Half()) SetWord(sh::Mem(at::kMessage), SH_PICK(2, 2, 0xE, 0xE, 3, 0xD));
    if (sh::Half()) sh::Mem(at::kKind2Hold)[0] = 0;
    // the party: its count, member 0's kind and kept effect slot, the +0x89
    // Scena07_PartyHas89State2 looks for
    if (sh::Often()) sh::Mem(at::kMemberCount)[0] = static_cast<unsigned char>(sh::Next() % 4);
    for (unsigned i = 0; i < 3; ++i)
        if (sh::Half()) sh::ObjectOf(i)[0x89] = static_cast<unsigned char>(sh::Half() ? 2 : sh::Next() % 4);
    if (sh::Often()) sh::Mem(at::kLeaderKind)[0] = static_cast<unsigned char>(sh::Next() % 9);
    if (sh::Often()) sh::Mem(at::kLeaderSlot)[0] = static_cast<unsigned char>(sh::Next() % 20);
    if (sh::Often()) sh::Mem(at::kKeptEffect)[0] = static_cast<unsigned char>(sh::Next() % 20);
    // the pointers and indices the code writes through, inside the regions
    sh::SetPointer(at::kFocusObject, sh::SpriteRecord(sh::Next()));
    sh::SetPointer(at::kActiveMember, sh::Half() ? sh::SpriteRecord(sh::Next()) : sh::Mem(at::kSpritesExtra + (sh::Next() % 4) * at::kSpriteStride));
    sh::Mem(at::kKeptFlags)[0] = static_cast<unsigned char>(sh::Next() % 30);
    for (unsigned i = 0; i < 16; ++i) sh::Mem(at::kEffectState + i)[0] = static_cast<unsigned char>(sh::Next() % 8);
    for (unsigned i = 0; i < 0x20; ++i)
        if (sh::Next() % 4 == 0) sh::Mem(at::kKeyItems + i)[0] = 4;
    // the object handlers' index in the records the kObject shape passes, and
    // the handler tables' stand-ins
    for (unsigned i = 0; i < 4; ++i) sh::SpriteRecord(i)[0x86] = static_cast<unsigned char>(sh::Next() % (eight ? 3 : 5));
    if (eight) {
        for (unsigned i = 0; i < 3; ++i) SetLong(sh::Mem(at::kObjects8 + 4 * i), static_cast<std::int32_t>(Key(kObjEntries8[i])));
    } else {
        for (unsigned i = 0; i < 5; ++i) SetLong(sh::Mem(at::kObjects7 + 4 * i), static_cast<std::int32_t>(Key(kObjEntries7[i])));
    }
    // run 4's timed effects: the timer at a record's frame
    if ((Named("TimedEffects") || Named("Scena07_Scene4")) && sh::Half()) {
        const unsigned i = sh::Next() % 12;
        SetWord(sh::Mem(at::kTimer), move_script::Word(sh::Mem(at::kTimed7 + 6 * i + 4)) + (sh::Next() % 4 == 0 ? 1u : 0u));
        if (Named("Scena07_Scene4")) sh::Mem(at::kStep)[0] = static_cast<unsigned char>(8 + sh::Next() % 2);
    }
    // a scene's timer at its end (the waits test 0 before or after a decrement)
    if (scene && sh::Next() % 3 == 0) SetWord(sh::Mem(at::kTimer), SH_PICK(0, 1, 2));
    // chapter 7 run 6's step 0xD makes an effect at timer 0x88
    if (Named("Scena07_Scene6") && sh::Next() % 5 == 0) {
        sh::Mem(at::kStep)[0] = 0xD;
        SetWord(sh::Mem(at::kTimer), SH_PICK(0x88, 0x89, 0x87, 0, 1));
    }
    // an area the entry knows
    if (Named("EnterArea") && sh::Often()) SetWord(sh::Mem(at::kArea), eight ? PickArea8() : PickArea7());
    if (Named("Scena08_EnterArea") && sh::Half()) sh::Mem(at::kByteFD)[0] = static_cast<unsigned char>(SH_PICK(0, 1, 2, 5));
}

// Function k's arguments beyond the shape's: the hooks' (x, z) at the values
// tested, the object handlers' object and row, the member of the effect.
void Args(unsigned k, std::uint32_t* a) {
    g_k = k;
    const sh::Clone& c = g_clones[k];
    for (const HookHits& h : kHits) {
        if (std::strcmp(h.name, c.name) != 0 || !sh::Often()) continue;
        const Hit& t = h.hits[sh::Next() % h.n];
        a[0] = Coord(t.x0, t.x1, t.x_exact);
        a[1] = Coord(t.z0, t.z1, t.z_exact);
    }
    if (Named("_Object") && c.shape == sh::Shape::kEntry) {
        a[0] = Key(sh::SpriteRecord(sh::Next()));
        a[1] = Key(sh::FlagRow());
    }
    if (Named("TakeEffect49") && sh::Often()) a[0] = (sh::Next() & 0xFFFFFF00u) | (sh::Next() % 10);
}

// After a call, two in three (the harness's own list moves the step, run,
// wait, row, Sprite_Current, Frame_Counter, counter 0, request, timer, script
// flags): a chapter cell of this group's the scenes and hooks read again.
// Only from the hash it is given: the harness's Next would differ between the
// passes.
void Disturb(std::uint32_t h) {
    static const unsigned char kC0[] = {0, 1, 3, 5, 7, 0xA, 0xC, 0x13, 0x15, 0x17, 0x1A, 0x1E, 0x1F, 0x25, 0x26, 0x27, 0x29};
    static const unsigned short kTimers[] = {0, 1, 2, 0x88, 0xFFFF};
    static const unsigned char kAreas7[] = {0x35, 0x4A, 0x52, 0x55, 0x67, 0x69, 0xAF};
    static const unsigned char kAreas8[] = {0, 1, 2, 3, 0xC, 0xD, 0xF, 0x12, 0x15, 0x20, 0x23, 0x2B, 0x46};
    switch ((h >> 8) % 10) {
    case 0: sh::Mem(at::kCounter0)[0] = h & 0x10000 ? kC0[(h >> 17) % sizeof kC0] : static_cast<unsigned char>(h >> 24); break;
    case 1:
        SetWord(sh::Mem(at::kArea), g_chapter == 8 ? kAreas8[(h >> 16) % sizeof kAreas8] : kAreas7[(h >> 16) % sizeof kAreas7]);
        break;
    case 2: sh::Mem(at::kEffects + (h >> 12) % 0xA00)[0] = static_cast<unsigned char>(h >> 24); break;
    case 3: sh::Mem(at::kByteFD)[0] = static_cast<unsigned char>((h >> 16) % 6); break;
    case 4: SetWord(sh::Mem(at::kTimer), kTimers[(h >> 16) % (sizeof kTimers / sizeof kTimers[0])]); break;
    case 5: sh::Mem(at::kRequest)[0] = static_cast<unsigned char>(h & 0x10000 ? 2 : (h >> 17) % 7); break;
    case 6: sh::Mem(at::kCounter3)[0] = static_cast<unsigned char>(h & 0x10000 ? 0x12 : (h >> 17) % 4); break;
    case 7: sh::Mem(at::kStep)[0] = static_cast<unsigned char>((h >> 16) % 0x28); break;
    case 8: sh::Mem(at::kKind2Hold)[0] = static_cast<unsigned char>((h >> 16) & 1); break;
    default: SetWord(sh::Mem(at::kMessage), h & 0x10000 ? 2u : 0xEu); break;
    }
}

void RunChapter(int chapter, const sh::Clone* clones, unsigned n, const sh::DataTable* tables, unsigned n_tables,
                const char* shadow) {
    g_clones = clones;
    g_chapter = chapter;
    ListCallees(chapter);
    ListRegions(chapter);
    sh::Group group = {shadow, clones, n, g_callees, g_n_callees, tables, n_tables, g_regions, g_n_regions, &Seed, &Disturb, 6000};
    group.args = &Args;
    group.chapter = chapter;
    sh::Run(group);
}

}  // namespace

// Two runs of the harness, one per chapter byte (a Group takes one chapter):
// chapter 7's 27 functions with Cond_ByteFA 7, chapter 8's 25 with 8.
void SelfTest() {
    RunChapter(7, kClones7, kCount7, kTables7, sizeof kTables7 / sizeof kTables7[0], "scena_sc7 (chapter 7)");
    RunChapter(8, kClones8, kCount8, kTables8, sizeof kTables8 / sizeof kTables8[0], "scena_sc7 (chapter 8)");
}

}  // namespace scena_sc7
