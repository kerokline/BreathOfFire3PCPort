// BOF3X_SHADOW=magic_s33: group S33's two overlays (MAGIC151, MAGIC154)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s33.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC151 / MAGIC154 --clones
// (2026-09-27; capstone, every jump internal, no jump table, no REFUSED line),
// names given. Beyond the standard set this group lists the draw callees (the
// GTE and libgpu entry points, Math_Sin / Math_Cos, Gfx_CommitPrim,
// MapView_LinkPrimAt), the sprite and battle calls MAGIC151 rebuilds the
// acting member with, the file load, the banner, AreaMap_Elevation, the
// engine's 0x446770 and 0x4514A0, and the functions of its own its functions
// call directly. Everything the harness lacks is built here, not in the
// harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what the caller reads back is compared;
//   - the calls that act on Sprite_Current (the script ticks, the animations,
//     the palette, the status tint, the CLUT STP bits, the member sprite, the
//     screen update) log which sprite - MAGIC151 swaps it to a party record
//     round them - and the screen update the frame-offset table 0x9039D8 the
//     blades swap.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s33.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s33 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC151 / MAGIC154 --clones, 2026-09-27, names
// given.
// 0x4EAE70: 0x2E bytes  Accession_Task
constexpr mh::Imm kImms4EAE70[] = {{0xF, 0x4EAEA0}, {0x17, 0x4EAF50}, {0x22, 0x4EB600}};
// 0x4EAEA0: 0xA2 bytes  Accession_Start
constexpr mh::CallSite kCalls4EAEA0[] = {{0x0, 0x4514A0}, {0x1E, 0x497740}, {0x2C, 0x5171A0}, {0x3E, 0x44A650}};
// 0x4EAF50: 0x26 bytes  Accession_Run
constexpr mh::Imm kImms4EAF50[] = {{0xF, 0x4EAF80}, {0x17, 0x4EB2A0}};
// 0x4EAF80: 0x3E bytes  Accession_StepsA
constexpr mh::Imm kImms4EAF80[] = {{0xF, 0x4EAFC0}, {0x17, 0x4EB010}, {0x22, 0x4EB0F0}, {0x2A, 0x4EB160}, {0x32, 0x4EB250}};
// 0x4EAFC0: 0x45 bytes  Accession_ActorPose
constexpr mh::CallSite kCalls4EAFC0[] = {{0x2D, 0x589330}};
// 0x4EB010: 0xD8 bytes  Accession_ActorScript
constexpr mh::CallSite kCalls4EB010[] = {{0x44, 0x587900}, {0x73, 0x589410}, {0x9F, 0x435180}};
// 0x4EB0F0: 0x70 bytes  Accession_LoadFormA
constexpr mh::CallSite kCalls4EB0F0[] = {{0x5F, 0x454590}};
// 0x4EB160: 0xE8 bytes  Accession_ApplyA
constexpr mh::CallSite kCalls4EB160[] = {{0x0, 0x454810}, {0x56, 0x454DC0}, {0x98, 0x5366A0}, {0xB9, 0x446BB0}, {0xBE, 0x4551A0}, {0xD0, 0x589330}};
// 0x4EB250: 0x4F bytes  Accession_Finish
// 0x4EB2A0: 0x3E bytes  Accession_StepsB
constexpr mh::Imm kImms4EB2A0[] = {{0xF, 0x4EAFC0}, {0x17, 0x4EB010}, {0x22, 0x4EB2E0}, {0x2A, 0x4EB350}, {0x32, 0x4EB250}};
// 0x4EB2E0: 0x70 bytes  Accession_LoadFormB
constexpr mh::CallSite kCalls4EB2E0[] = {{0x5F, 0x454590}};
// 0x4EB350: 0x2AC bytes  Accession_ApplyB
constexpr mh::CallSite kCalls4EB350[] = {{0x1, 0x454810}, {0x2A, 0x454DC0}, {0x4E, 0x446EA0}, {0x54, 0x446650}, {0x74, 0x446FD0}, {0xBE, 0x5720C0}, {0xEC, 0x5720C0}, {0x152, 0x59E2D0}, {0x215, 0x446FB0}, {0x23A, 0x533BA0}, {0x25A, 0x5366A0}, {0x27B, 0x446BB0}, {0x280, 0x4551A0}, {0x291, 0x589330}};
// 0x4EB600: 0x3F bytes  Accession_End
constexpr mh::CallSite kCalls4EB600[] = {{0x39, 0x4351F0}};
// 0x4EB640: 0x12 bytes; +0xB note: jmp through .data 0x65be74, 5 code entries (a data_tables entry)  AccessionChild_Task
// 0x4EB660: 0x46 bytes  AccessionCtl_Run
constexpr mh::Imm kImms4EB660[] = {{0xF, 0x4EB6B0}, {0x17, 0x4EB760}, {0x22, 0x4EB7D0}, {0x2A, 0x4EB850}, {0x32, 0x4EB880}, {0x3A, 0x4EB8A0}};
// 0x4EB6B0: 0xAC bytes  AccessionCtl_Start
constexpr mh::CallSite kCalls4EB6B0[] = {{0x3A, 0x5720C0}, {0x68, 0x435180}, {0xA3, 0x587900}};
// 0x4EB760: 0x6D bytes  AccessionCtl_SpawnRing
constexpr mh::CallSite kCalls4EB760[] = {{0x1E, 0x435180}};
// 0x4EB7D0: 0x72 bytes  AccessionCtl_SpawnOrb
constexpr mh::CallSite kCalls4EB7D0[] = {{0x1E, 0x435180}, {0x56, 0x587900}};
// 0x4EB850: 0x2C bytes  AccessionCtl_Signal
// 0x4EB880: 0x14 bytes  AccessionCtl_WaitOwner
// 0x4EB8A0: 0x1C bytes  AccessionCtl_End
constexpr mh::CallSite kCalls4EB8A0[] = {{0x16, 0x4351F0}};
// 0x4EB8C0: 0x38 bytes; +0xB note: call through .data 0x65be88, 6 code entries (a data_tables entry)  AccessionOrb_Run
constexpr mh::CallSite kCalls4EB8C0[] = {{0x23, 0x4B7D40}, {0x28, 0x4EBAA0}, {0x2D, 0x4EBDB0}, {0x32, 0x5A7BC0}};
// 0x4EB900: 0x80 bytes  AccessionOrb_Start
// 0x4EB980: 0x1D bytes  AccessionOrb_Grow
// 0x4EB9A0: 0x86 bytes  AccessionOrb_Emit
constexpr mh::CallSite kCalls4EB9A0[] = {{0xE, 0x435180}};
// 0x4EBA30: 0x10 bytes  AccessionOrb_WaitChildren
// 0x4EBA40: 0x1C bytes  AccessionOrb_Dim
// 0x4EBA60: 0x3A bytes  AccessionOrb_Fade
constexpr mh::CallSite kCalls4EBA60[] = {{0x34, 0x4351F0}};
// 0x4EBAA0: 0x308 bytes  AccessionOrb_DrawShell
constexpr mh::CallSite kCalls4EBAA0[] = {{0x32, 0x5A7A00}, {0x4F, 0x5A7A50}, {0x9C, 0x5A7A00}, {0xB5, 0x5A7A50}, {0xCF, 0x5A7A00}, {0xE9, 0x5A7A50}, {0x115, 0x5A7A00}, {0x12F, 0x5A7A50}, {0x18F, 0x5A7A00}, {0x1AF, 0x5A7A50}, {0x1E9, 0x5A7A00}, {0x209, 0x5A7A50}, {0x250, 0x5A77C0}, {0x25B, 0x572FA0}, {0x267, 0x5A75B0}, {0x26F, 0x5A7780}, {0x2A5, 0x5A85F0}, {0x2AB, 0x5A9240}, {0x2D0, 0x572FA0}};
// 0x4EBDB0: 0x337 bytes  AccessionOrb_DrawGlow
constexpr mh::CallSite kCalls4EBDB0[] = {{0x40, 0x5A7A00}, {0x5D, 0x5A7A50}, {0x8B, 0x5B93D2}, {0xAD, 0x5A7A00}, {0xCD, 0x5A7A50}, {0xE7, 0x5A7A00}, {0x101, 0x5A7A50}, {0x12D, 0x5A7A00}, {0x147, 0x5A7A50}, {0x1A9, 0x5A7A00}, {0x1C9, 0x5A7A50}, {0x203, 0x5A7A00}, {0x223, 0x5A7A50}, {0x26A, 0x5A77C0}, {0x275, 0x572FA0}, {0x281, 0x5A7610}, {0x289, 0x5A7780}, {0x2BF, 0x5A85F0}, {0x2C5, 0x5A9350}, {0x31C, 0x572FA0}};
// 0x4EC0F0: 0x70 bytes; +0xB note: call through .data 0x65bec0, 3 code entries (a data_tables entry)  AccessionSpark_Run
constexpr mh::CallSite kCalls4EC0F0[] = {{0x23, 0x4B7D40}, {0x62, 0x4EC240}, {0x6A, 0x5A7BC0}};
// 0x4EC160: 0x74 bytes  AccessionSpark_Start
constexpr mh::CallSite kCalls4EC160[] = {{0x33, 0x5B93D2}};
// 0x4EC1E0: 0x26 bytes  AccessionSpark_Grow
// 0x4EC210: 0x28 bytes  AccessionSpark_Fade
constexpr mh::CallSite kCalls4EC210[] = {{0x22, 0x4351F0}};
// 0x4EC240: 0x2B6 bytes  AccessionSpark_Draw
constexpr mh::CallSite kCalls4EC240[] = {{0x34, 0x5A7A00}, {0x5A, 0x5A7A00}, {0x73, 0x5A7A50}, {0x8C, 0x5A7A50}, {0xF0, 0x5B93D2}, {0x11C, 0x5A7A00}, {0x141, 0x5A7A00}, {0x161, 0x5A7A50}, {0x179, 0x5B93D2}, {0x198, 0x5A7A00}, {0x1B1, 0x5A7A50}, {0x202, 0x5A77C0}, {0x20D, 0x572FA0}, {0x219, 0x5A7650}, {0x22B, 0x5A7780}, {0x246, 0x5A8250}, {0x24F, 0x5A9110}, {0x267, 0x5A8250}, {0x270, 0x5A9110}, {0x295, 0x572FA0}};
// 0x4EC500: 0x72 bytes; +0xB note: call through .data 0x65becc, 4 code entries (a data_tables entry)  AccessionBolt_Run
constexpr mh::CallSite kCalls4EC500[] = {{0x12, 0x4FBD10}, {0x28, 0x4D36C0}, {0x33, 0x4EC640}, {0x47, 0x4EC640}, {0x5B, 0x4EC640}, {0x6C, 0x5A7BC0}};
// 0x4EC580: 0x73 bytes  AccessionBolt_Start
constexpr mh::CallSite kCalls4EC580[] = {{0x47, 0x5B93D2}};
// 0x4EC600: 0x33 bytes  AccessionBolt_Rise
// 0x4EC640: 0x531 bytes  AccessionBolt_DrawBand
constexpr mh::CallSite kCalls4EC640[] = {{0x57, 0x5A7A00}, {0x89, 0x5A77C0}, {0x9F, 0x572FA0}, {0xD1, 0x5B93D2}, {0xDA, 0x5B93D2}, {0xF3, 0x5B93D2}, {0x152, 0x5A7A00}, {0x198, 0x5A7610}, {0x19F, 0x5A7780}, {0x24E, 0x5A85F0}, {0x254, 0x5A9350}, {0x26A, 0x572FA0}, {0x292, 0x5A7610}, {0x29C, 0x5A7780}, {0x31F, 0x5A85F0}, {0x325, 0x5A9350}, {0x33B, 0x572FA0}, {0x363, 0x5A7610}, {0x36D, 0x5A7780}, {0x41B, 0x5A85F0}, {0x421, 0x5A9350}, {0x437, 0x572FA0}, {0x45F, 0x5A7610}, {0x469, 0x5A7780}, {0x4EC, 0x5A85F0}, {0x4F2, 0x5A9350}, {0x508, 0x572FA0}};
// 0x4ECB80: 0x38 bytes; +0xB note: call through .data 0x65bedc, 3 code entries (a data_tables entry)  AccessionRing_Run
constexpr mh::CallSite kCalls4ECB80[] = {{0x23, 0x4B7D40}, {0x28, 0x4ECC50}, {0x2D, 0x4ECE80}, {0x32, 0x5A7BC0}};
// 0x4ECBC0: 0x52 bytes  AccessionRing_Start
// 0x4ECC20: 0x23 bytes  AccessionRing_Widen
// 0x4ECC50: 0x22D bytes  AccessionRing_DrawInner
constexpr mh::CallSite kCalls4ECC50[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x57, 0x5A7A00}, {0x70, 0x5A7A50}, {0x89, 0x5A7A00}, {0xA2, 0x5A7A50}, {0xE5, 0x5A7610}, {0xEC, 0x5A7780}, {0x10C, 0x5A7A00}, {0x125, 0x5A7A50}, {0x158, 0x5A7A00}, {0x171, 0x5A7A50}, {0x1B7, 0x5A85F0}, {0x1C0, 0x5A9350}, {0x20F, 0x461E50}};
// 0x4ECE80: 0x232 bytes  AccessionRing_DrawOuter
constexpr mh::CallSite kCalls4ECE80[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x5C, 0x5A7A00}, {0x75, 0x5A7A50}, {0x8E, 0x5A7A00}, {0xA7, 0x5A7A50}, {0xEA, 0x5A7610}, {0xF1, 0x5A7780}, {0x111, 0x5A7A00}, {0x12A, 0x5A7A50}, {0x15D, 0x5A7A00}, {0x176, 0x5A7A50}, {0x1BC, 0x5A85F0}, {0x1C5, 0x5A9350}, {0x214, 0x461E50}};
// 0x4ED0C0: 0x36 bytes  MightyChop_Task
constexpr mh::Imm kImms4ED0C0[] = {{0xF, 0x4ED100}, {0x17, 0x4ED1F0}, {0x22, 0x4ED2D0}, {0x2A, 0x4F7350}};
// 0x4ED100: 0xE7 bytes  MightyChop_Start
constexpr mh::CallSite kCalls4ED100[] = {{0x7, 0x4FB830}, {0x14, 0x435180}};
// 0x4ED1F0: 0xD4 bytes  MightyChop_Throw
constexpr mh::CallSite kCalls4ED1F0[] = {{0x3C, 0x446770}, {0x71, 0x587900}, {0x7D, 0x435180}};
// 0x4ED2D0: 0x3B bytes  MightyChop_End
constexpr mh::CallSite kCalls4ED2D0[] = {{0x10, 0x4FB830}, {0x2A, 0x452F70}};
// 0x4ED310: 0x12 bytes; +0xB note: jmp through .data 0x65bee8, 2 code entries (a data_tables entry)  MightyChopChild_Task
// 0x4ED330: 0x5C bytes; +0x34 note: call through .data 0x65bef0, 4 code entries (a data_tables entry)  MightyChopBlade_Run
constexpr mh::CallSite kCalls4ED330[] = {{0xE, 0x5A77C0}, {0x17, 0x461E50}, {0x4C, 0x588F20}};
// 0x4ED390: 0x120 bytes  MightyChopBlade_Start
constexpr mh::CallSite kCalls4ED390[] = {{0x49, 0x446770}, {0x101, 0x5891F0}, {0x109, 0x5893A0}};
// 0x4ED4B0: 0x3E bytes  MightyChopBlade_Grow
// 0x4ED4F0: 0x49 bytes  MightyChopBlade_Shrink
constexpr mh::CallSite kCalls4ED4F0[] = {{0x43, 0x4351F0}};
// 0x4ED540: 0x2A bytes  MightyChopBlade_Sink
constexpr mh::CallSite kCalls4ED540[] = {{0x24, 0x4351F0}};
// 0x4ED570: 0x46 bytes  MightyChopCopy_Run
constexpr mh::CallSite kCalls4ED570[] = {{0x3D, 0x588F20}};
constexpr mh::Imm kImms4ED570[] = {{0xF, 0x4ED5C0}, {0x17, 0x4ED5E0}, {0x22, 0x4ED650}, {0x2A, 0x4AEE90}};
// 0x4ED5E0: 0x61 bytes  MightyChopCopy_Play
constexpr mh::CallSite kCalls4ED5E0[] = {{0x19, 0x589410}, {0x22, 0x4FC030}, {0x2C, 0x589410}, {0x39, 0x4FC030}};
// 0x4ED650: 0x1F bytes  MightyChopCopy_Wait
constexpr mh::CallSite kCalls4ED650[] = {{0x0, 0x589410}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Accession_Task", 0x4EAE70, 0x2E, nullptr, 0, kImms4EAE70, MH_N(kImms4EAE70), nullptr, 0, reinterpret_cast<const void*>(&::Accession_Task)},
    {"Accession_Start", 0x4EAEA0, 0xA2, kCalls4EAEA0, MH_N(kCalls4EAEA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_Start)},
    {"Accession_Run", 0x4EAF50, 0x26, nullptr, 0, kImms4EAF50, MH_N(kImms4EAF50), nullptr, 0, reinterpret_cast<const void*>(&::Accession_Run)},
    {"Accession_StepsA", 0x4EAF80, 0x3E, nullptr, 0, kImms4EAF80, MH_N(kImms4EAF80), nullptr, 0, reinterpret_cast<const void*>(&::Accession_StepsA)},
    {"Accession_ActorPose", 0x4EAFC0, 0x45, kCalls4EAFC0, MH_N(kCalls4EAFC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_ActorPose)},
    {"Accession_ActorScript", 0x4EB010, 0xD8, kCalls4EB010, MH_N(kCalls4EB010), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_ActorScript)},
    {"Accession_LoadFormA", 0x4EB0F0, 0x70, kCalls4EB0F0, MH_N(kCalls4EB0F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_LoadFormA)},
    {"Accession_ApplyA", 0x4EB160, 0xE8, kCalls4EB160, MH_N(kCalls4EB160), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_ApplyA)},
    {"Accession_Finish", 0x4EB250, 0x4F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_Finish)},
    {"Accession_StepsB", 0x4EB2A0, 0x3E, nullptr, 0, kImms4EB2A0, MH_N(kImms4EB2A0), nullptr, 0, reinterpret_cast<const void*>(&::Accession_StepsB)},
    {"Accession_LoadFormB", 0x4EB2E0, 0x70, kCalls4EB2E0, MH_N(kCalls4EB2E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_LoadFormB)},
    {"Accession_ApplyB", 0x4EB350, 0x2AC, kCalls4EB350, MH_N(kCalls4EB350), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_ApplyB)},
    {"Accession_End", 0x4EB600, 0x3F, kCalls4EB600, MH_N(kCalls4EB600), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Accession_End)},
    {"AccessionChild_Task", 0x4EB640, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionChild_Task)},
    {"AccessionCtl_Run", 0x4EB660, 0x46, nullptr, 0, kImms4EB660, MH_N(kImms4EB660), nullptr, 0, reinterpret_cast<const void*>(&::AccessionCtl_Run)},
    {"AccessionCtl_Start", 0x4EB6B0, 0xAC, kCalls4EB6B0, MH_N(kCalls4EB6B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionCtl_Start)},
    {"AccessionCtl_SpawnRing", 0x4EB760, 0x6D, kCalls4EB760, MH_N(kCalls4EB760), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionCtl_SpawnRing)},
    {"AccessionCtl_SpawnOrb", 0x4EB7D0, 0x72, kCalls4EB7D0, MH_N(kCalls4EB7D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionCtl_SpawnOrb)},
    {"AccessionCtl_Signal", 0x4EB850, 0x2C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionCtl_Signal)},
    {"AccessionCtl_WaitOwner", 0x4EB880, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionCtl_WaitOwner)},
    {"AccessionCtl_End", 0x4EB8A0, 0x1C, kCalls4EB8A0, MH_N(kCalls4EB8A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionCtl_End)},
    {"AccessionOrb_Run", 0x4EB8C0, 0x38, kCalls4EB8C0, MH_N(kCalls4EB8C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_Run)},
    {"AccessionOrb_Start", 0x4EB900, 0x80, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_Start)},
    {"AccessionOrb_Grow", 0x4EB980, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_Grow)},
    {"AccessionOrb_Emit", 0x4EB9A0, 0x86, kCalls4EB9A0, MH_N(kCalls4EB9A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_Emit)},
    {"AccessionOrb_WaitChildren", 0x4EBA30, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_WaitChildren)},
    {"AccessionOrb_Dim", 0x4EBA40, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_Dim)},
    {"AccessionOrb_Fade", 0x4EBA60, 0x3A, kCalls4EBA60, MH_N(kCalls4EBA60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_Fade)},
    {"AccessionOrb_DrawShell", 0x4EBAA0, 0x308, kCalls4EBAA0, MH_N(kCalls4EBAA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_DrawShell)},
    {"AccessionOrb_DrawGlow", 0x4EBDB0, 0x337, kCalls4EBDB0, MH_N(kCalls4EBDB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionOrb_DrawGlow)},
    {"AccessionSpark_Run", 0x4EC0F0, 0x70, kCalls4EC0F0, MH_N(kCalls4EC0F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionSpark_Run)},
    {"AccessionSpark_Start", 0x4EC160, 0x74, kCalls4EC160, MH_N(kCalls4EC160), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionSpark_Start)},
    {"AccessionSpark_Grow", 0x4EC1E0, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionSpark_Grow)},
    {"AccessionSpark_Fade", 0x4EC210, 0x28, kCalls4EC210, MH_N(kCalls4EC210), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionSpark_Fade)},
    {"AccessionSpark_Draw", 0x4EC240, 0x2B6, kCalls4EC240, MH_N(kCalls4EC240), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionSpark_Draw)},
    {"AccessionBolt_Run", 0x4EC500, 0x72, kCalls4EC500, MH_N(kCalls4EC500), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionBolt_Run)},
    {"AccessionBolt_Start", 0x4EC580, 0x73, kCalls4EC580, MH_N(kCalls4EC580), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionBolt_Start)},
    {"AccessionBolt_Rise", 0x4EC600, 0x33, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionBolt_Rise)},
    {"AccessionBolt_DrawBand", 0x4EC640, 0x531, kCalls4EC640, MH_N(kCalls4EC640), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionBolt_DrawBand)},
    {"AccessionRing_Run", 0x4ECB80, 0x38, kCalls4ECB80, MH_N(kCalls4ECB80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionRing_Run)},
    {"AccessionRing_Start", 0x4ECBC0, 0x52, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionRing_Start)},
    {"AccessionRing_Widen", 0x4ECC20, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionRing_Widen)},
    {"AccessionRing_DrawInner", 0x4ECC50, 0x22D, kCalls4ECC50, MH_N(kCalls4ECC50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionRing_DrawInner)},
    {"AccessionRing_DrawOuter", 0x4ECE80, 0x232, kCalls4ECE80, MH_N(kCalls4ECE80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AccessionRing_DrawOuter)},
    {"MightyChop_Task", 0x4ED0C0, 0x36, nullptr, 0, kImms4ED0C0, MH_N(kImms4ED0C0), nullptr, 0, reinterpret_cast<const void*>(&::MightyChop_Task)},
    {"MightyChop_Start", 0x4ED100, 0xE7, kCalls4ED100, MH_N(kCalls4ED100), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChop_Start)},
    {"MightyChop_Throw", 0x4ED1F0, 0xD4, kCalls4ED1F0, MH_N(kCalls4ED1F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChop_Throw)},
    {"MightyChop_End", 0x4ED2D0, 0x3B, kCalls4ED2D0, MH_N(kCalls4ED2D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChop_End)},
    {"MightyChopChild_Task", 0x4ED310, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChopChild_Task)},
    {"MightyChopBlade_Run", 0x4ED330, 0x5C, kCalls4ED330, MH_N(kCalls4ED330), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChopBlade_Run)},
    {"MightyChopBlade_Start", 0x4ED390, 0x120, kCalls4ED390, MH_N(kCalls4ED390), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChopBlade_Start)},
    {"MightyChopBlade_Grow", 0x4ED4B0, 0x3E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChopBlade_Grow)},
    {"MightyChopBlade_Shrink", 0x4ED4F0, 0x49, kCalls4ED4F0, MH_N(kCalls4ED4F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChopBlade_Shrink)},
    {"MightyChopBlade_Sink", 0x4ED540, 0x2A, kCalls4ED540, MH_N(kCalls4ED540), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChopBlade_Sink)},
    {"MightyChopCopy_Run", 0x4ED570, 0x46, kCalls4ED570, MH_N(kCalls4ED570), kImms4ED570, MH_N(kImms4ED570), nullptr, 0, reinterpret_cast<const void*>(&::MightyChopCopy_Run)},
    {"MightyChopCopy_Play", 0x4ED5E0, 0x61, kCalls4ED5E0, MH_N(kCalls4ED5E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChopCopy_Play)},
    {"MightyChopCopy_Wait", 0x4ED650, 0x1F, kCalls4ED650, MH_N(kCalls4ED650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MightyChopCopy_Wait)},
};
#undef MH_N

enum : unsigned {
    kAccession_Task, kAccession_Start, kAccession_Run, kAccession_StepsA, kAccession_ActorPose,
    kAccession_ActorScript, kAccession_LoadFormA, kAccession_ApplyA, kAccession_Finish, kAccession_StepsB,
    kAccession_LoadFormB, kAccession_ApplyB, kAccession_End, kAccessionChild_Task, kAccessionCtl_Run,
    kAccessionCtl_Start, kAccessionCtl_SpawnRing, kAccessionCtl_SpawnOrb, kAccessionCtl_Signal,
    kAccessionCtl_WaitOwner, kAccessionCtl_End, kAccessionOrb_Run, kAccessionOrb_Start, kAccessionOrb_Grow,
    kAccessionOrb_Emit, kAccessionOrb_WaitChildren, kAccessionOrb_Dim, kAccessionOrb_Fade, kAccessionOrb_DrawShell,
    kAccessionOrb_DrawGlow, kAccessionSpark_Run, kAccessionSpark_Start, kAccessionSpark_Grow, kAccessionSpark_Fade,
    kAccessionSpark_Draw, kAccessionBolt_Run, kAccessionBolt_Start, kAccessionBolt_Rise, kAccessionBolt_DrawBand,
    kAccessionRing_Run, kAccessionRing_Start, kAccessionRing_Widen, kAccessionRing_DrawInner,
    kAccessionRing_DrawOuter,
    kMightyChop_Task, kMightyChop_Start, kMightyChop_Throw, kMightyChop_End, kMightyChopChild_Task,
    kMightyChopBlade_Run, kMightyChopBlade_Start, kMightyChopBlade_Grow, kMightyChopBlade_Shrink,
    kMightyChopBlade_Sink, kMightyChopCopy_Run, kMightyChopCopy_Play, kMightyChopCopy_Wait, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kFrameSet = 0x9039D8, kCentre = 0x903780,
                        kFormation = 0x904B89, kPartySet = 0x90412C, kMembers = 0x939B00;

// The kinds 0x904B89 may hold where MAGIC151 reads a pointer by it: the 26
// pointer pairs of 0x64E9BC but the three that are null (10, 19, 20).
constexpr unsigned char kKinds[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 21, 22, 23, 24, 25};

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log (the real ones link it), then Gfx_PacketNext on by its size, kept in
// the buffer (a draw writes up to 0x48 past it).
void Advance(std::uint32_t size) {
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
}
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    Advance(a[1]);
    return answer;
}
std::uint32_t LinkEffect(const std::uint32_t* a, std::uint32_t answer) {
    Advance(a[3]);
    return answer;
}
// The calls that act on Sprite_Current: which sprite; the screen update, the
// frame-offset table too.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
std::uint32_t NoteSpriteFrames(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
    return answer;
}
// 0x446770 turns the task's +0xC / +0x10 by its +8: the inputs logged, a new
// pair written where the real one writes.
std::uint32_t TurnEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* task = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    mh::Note(task[8], static_cast<std::uint32_t>(Long(task + 0xC)), static_cast<std::uint32_t>(Long(task + 0x10)));
    mh::FillBytes(task + 0xC, 8);
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S33_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S33_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S33_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S33_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    // the sprite and battle calls
    {S33_OURS(Sprite_ScriptTick), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S33_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S33_OURS(Sprite_EnsureAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S33_OURS(Sprite_LoadPalette), 2, {kAll, kU8}, kG, 0, 0, {}, &NoteSprite},
    {S33_OURS(Battle_StatusTint), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S33_OURS(Sprite_SetClutStp), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S33_OURS(Field_MemberSprite), 2, {kU8, kU8}, kG, 0, 0, {}, &NoteSprite},
    {S33_OURS(Battle_SetActorBit), 1, {kU8}, kG, 0, 0},
    {S33_OURS(Battle_ClearActorBit), 1, {kU8}, kG, 0, 0},
    {S33_OURS(Battle_ReturnQueuedItem), 1, {kU8}, kG, 0, 0},
    {S33_OURS(Battle_RemoveFromTurnOrder), 1, {kU8}, kG, 0, 0},
    {S33_OURS(Window_Alloc), 2, {kU8, kU8}, kG, 0, 0},
    {S33_OURS(File_LoadDone), 0, {}, mh::Answer::kFlag, 0, 0},
    {S33_OURS(LoadDatFile), 1, {kAll}, kG, 0, 0},
    {S33_OURS(Str_CopyN), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {S33_OURS(BattleBanner_Add), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S33_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0},
    {S33_OURS(SpellSleep_PushTurnMatrix), 0, {}, kG, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, field_misc:
    // all ours)
    {S33_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S33_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S33_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S33_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S33_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S33_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S33_OURS(Gpu_SetPolyF4), 1, {kAll}, kG, 0, 0},
    {S33_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S33_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    // the projections: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S33_OURS(Gte_RotTransPers), 4, {kAll, kAll, 0, 0}, kG, 0, 0, {6}},
    {S33_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S33_OURS(Gte_StoreDepthF), 1, {kAll}, kG, 0, 0},
    {S33_OURS(Gte_PrimDepths4_0C), 1, {kAll}, kG, 0, 0},
    {S33_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction; the acting member's
    // reset (no arguments)
    {S33_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    {S33_RAW(0x4514A0), 0, {}, kG, 0, 0},
    // this group's own, called directly
    {S33_RAW(0x4EBAA0), 0, {}, kG, 0, 0},
    {S33_RAW(0x4EBDB0), 0, {}, kG, 0, 0},
    {S33_RAW(0x4EC240), 2, {kU8, kU8}, kG, 0, 0},
    {S33_RAW(0x4EC640), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {S33_RAW(0x4ECC50), 0, {}, kG, 0, 0},
    {S33_RAW(0x4ECE80), 0, {}, kG, 0, 0},
};
#undef S33_OURS
#undef S33_RAW

// The seven .data handler tables the dispatchers read in place
// (AccessionChild_Kinds and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65BE74, 5}, {0x65BE88, 6}, {0x65BEC0, 3}, {0x65BECC, 4}, {0x65BEDC, 3}, {0x65BEE8, 2}, {0x65BEF0, 4},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850.., Scratch_Swap at +0xC
    {kFrameSet, 4},                       // the frame-offset table pointer the blades swap
    {0x80E980, 0x20},                     // Gfx_ClutStripSource row 26, its first 16 words
    {0x812980, 0x20},                     // Gfx_ClutStrip row 26, its first 16 words
    {kCentre, 8},                         // the fight's centre
    {0x904B50, 0x50},                     // the battle bytes past the harness's: 0x904B79, 0x904B89, 0x904B8A, 0x904B8F
    {kPartySet, 4},                       // the party set
    {kMembers, 0x400},                    // 0x939B07 / 0x939C05 / 0x939C10 + 0x14C per member
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Party(unsigned i) { return mh::Mem(mh::at::kParty + i * mh::at::kPartyStride); }

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 16); break;
    case 3: SetLong(mh::Mem(kCentre + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 4: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 5: {
        static const unsigned kFields[] = {0x0C, 0x10, 0x14, 0x3E, 0x40, 0x44, 0x5D, 0x5E};
        SetWord(Sc() + kFields[v % 8], h >> 16);
        break;
    }
    case 6: {
        static const unsigned kFields[] = {0, 5, 8, 0x27, 0x2E, 0x30, 0x89, 0x90};
        Party(v % 3)[kFields[(v >> 2) % 8]] = Byte(h >> 24);
        break;
    }
    case 7: {
        // 0x904B89 is read through a pointer only before any call
        static const std::uint32_t kCells[] = {0x904B79, kFormation, kPartySet, 0x904AB0, 0x904AAC, 0x904B8A};
        mh::Mem(kCells[v % 6])[0] = Byte(h >> 24);
        break;
    }
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}
void NearLong(unsigned char* at, std::uint32_t before) {
    if (mh::Half()) SetLong(at, static_cast<std::int32_t>(before + (mh::Half() ? 1u : 0u)));
}

// MightyChopBlade_Run reads its phase after two calls: while it is fuzzed a
// disturbance leaves +2 inside the four-entry table (past it the original
// calls through whatever follows and ours aborts).
bool g_blade_run = false;
void Settle() {
    if (g_blade_run) Sprite_Current[2] &= 3;
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    g_blade_run = k == kMightyChopBlade_Run;
    unsigned char* const owner = mh::Pointer(mh::at::kOwner);
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::Mem(kFormation)[0] = kKinds[mh::Next() % (sizeof kKinds)];
    if (mh::Often()) sc[8] = Byte(mh::Next() % 4);
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kAccession_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kAccession_Run: sc[2] = Byte(mh::Next() % 2); break;
    case kAccession_StepsA: case kAccession_StepsB: sc[3] = Byte(mh::Next() % 5); break;
    case kAccessionChild_Task: sc[1] = Byte(mh::Next() % 5); break;
    case kAccessionCtl_Run: case kAccessionOrb_Run: sc[2] = Byte(mh::Next() % 6); break;
    case kAccessionSpark_Run:
        sc[2] = Byte(mh::Next() % 3);
        if (mh::Often()) sc[4] = Byte(mh::Next() % 16);
        break;
    case kAccessionRing_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kAccessionBolt_Run: case kMightyChop_Task: case kMightyChopBlade_Run: case kMightyChopCopy_Run:
        sc[k == kMightyChop_Task ? 1 : 2] = Byte(mh::Next() % 4);
        break;
    case kMightyChopChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    // MAGIC151's task
    case kAccession_Start:
        if (mh::Often()) mh::Mem(kFormation)[0] = Byte(MH_PICK(3, 0xB, 0xD));
        if (mh::Often()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Next() % 3);
        if (mh::Half()) Party(mh::Mem(mh::at::kTarget)[0] % 5)[0x89] = 0;
        break;
    case kAccession_ActorScript:
        Near(sc[9], 1);
        if (mh::Often()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Next() % 3);
        if (mh::Half()) Party(mh::Mem(mh::at::kTarget)[0] % 5)[0x89] = 0;
        break;
    case kAccession_LoadFormA: case kAccession_LoadFormB:
        if (mh::Often()) sc[0xB] = 1;
        if (mh::Often()) owner[8] = Byte(mh::Next() % 4);
        break;
    case kAccession_ApplyA:
        if (mh::Half()) mh::Mem(mh::at::kFlags)[1] |= 0x40;
        if (mh::Half()) mh::Mem(0x904B8A)[0] = mh::Mem(mh::at::kActor)[0];
        break;
    case kAccession_End: if (mh::Often()) sc[0xB] = Byte(mh::Half() ? 2 : 1); break;
    // the controller
    case kAccessionCtl_SpawnRing: case kAccessionCtl_SpawnOrb: case kAccessionCtl_Signal: Near(sc[9], 1); break;
    case kAccessionCtl_WaitOwner: if (mh::Half()) owner[1] = Byte(mh::Half() ? 2 : 1); break;
    case kAccessionCtl_End: case kAccessionOrb_WaitChildren: if (mh::Half()) sc[0xB] = 0; break;
    // the orb, the sparks, the bolt, the rings
    case kAccessionOrb_Grow: Near(sc[9], 0x1E); break;
    case kAccessionOrb_Emit:
        if (mh::Often()) Frame_Counter &= ~3u;
        if (mh::Often()) owner[2] = 5;
        Near(sc[0xA], 1);
        break;
    case kAccessionOrb_Dim: Near(sc[0x5D], 8); break;
    case kAccessionOrb_Fade:
        if (mh::Half()) sc[0x5D] = 0;
        Near(sc[0x5E], 1);
        break;
    case kAccessionSpark_Grow: Near(sc[0xA], 0x1B); break;
    case kAccessionSpark_Fade: Near(sc[0x5D], 2); break;
    case kAccessionSpark_Draw: sc[0xA] = Byte(mh::Next() % 8); break;
    case kAccessionBolt_Start: Near(sc[9], 1); break;
    case kAccessionBolt_Rise: Near(sc[0xA], 0xB); break;
    case kAccessionRing_Widen: NearLong(sc + 0x14, 0x37F); break;
    // MAGIC154
    case kMightyChop_Throw: if (mh::Half()) sc[0xB] = 0; break;
    case kMightyChop_End: if (mh::Half()) sc[9] = 0; break;
    case kMightyChopBlade_Start: case kMightyChopBlade_Shrink: case kMightyChopCopy_Play: Near(sc[9], 1); break;
    case kMightyChopBlade_Grow: Near(sc[9], 3); break;
    case kMightyChopBlade_Sink: if (mh::Half()) sc[0x5F] = 0x80; break;
    default: break;
    }
}

// AccessionBolt_DrawBand's three words: its caller's rows most of the time.
void Args(unsigned k, std::uint32_t* a) {
    if (k != kAccessionBolt_DrawBand || !mh::Often()) return;
    static const std::uint32_t kRows[3][3] = {{0x20, 0x20, 7}, {0x10, 0x40, 0xF}, {0x10, 0x60, 0x1F}};
    const auto& row = kRows[mh::Next() % 3];
    a[0] = row[0];
    a[1] = row[1];
    a[2] = row[2];
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s33", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    group.settle = &Settle;
    mh::Run(group);
}

}  // namespace magic_s33
