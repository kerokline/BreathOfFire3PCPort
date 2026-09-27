// BOF3X_SHADOW=magic_s35: group S35's three overlays (MAGIC167, 168, 169)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s35.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC167 / 168 / 169 --clones
// (2026-09-27; capstone, every jump internal, no jump table, no REFUSED line),
// names given. Beyond the standard set this group lists the draw callees (the
// GTE and libgpu entry points, Math_Sin / Math_Cos, Gfx_CommitPrim,
// MapView_LinkPrimAt), Sprite_SetTint, Battle_ActorIsOut, MAGIC077's
// ReviveMote_Draw, MAGIC219's 0x4F6290, and the functions of its own its
// functions call directly. Everything the harness lacks is built here, not in
// the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - the projections log their SVECTORs through `deref`;
//   - MAGIC168's motes run with 0x6AFF10 (CureMote_Current) at their record,
//     not Sprite_Current: their steps, the ones called directly and the ones
//     in its tables, are listed as kPhase callees that also log that pointer;
//   - the two pools' owner pointers (+0x28, +0x80) are seeded at the harness's
//     slots and records, because Cure_Task and Benediction_Task put them in
//     the owner cell, which the harness's disturbance writes through.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s35.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s35 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC167 / 168 / 169 --clones, 2026-09-27, names
// given.
// 0x4EF620: 0x46 bytes  LastResort_Task
constexpr mh::Imm kImms4EF620[] = {{0xF, 0x4EF670}, {0x17, 0x4BD180}, {0x22, 0x4B1ED0}, {0x2A, 0x4EF7C0}, {0x32, 0x4C29C0}, {0x3A, 0x4F7350}};
// 0x4EF670: 0x144 bytes  LastResort_Start
constexpr mh::CallSite kCalls4EF670[] = {{0x58, 0x435180}, {0xB9, 0x435180}, {0x138, 0x587900}};
// 0x4EF7C0: 0xF bytes  LastResort_WaitChildren
// 0x4EF7D0: 0x12 bytes; +0xB note: jmp through .data 0x65c05c (a data_tables entry)  LastResortChild_Task
// 0x4EF7F0: 0x46 bytes; +0xB note: call through .data 0x65c064 (a data_tables entry)  LastResortRing_Run
constexpr mh::CallSite kCalls4EF7F0[] = {{0x1D, 0x4B7D40}, {0x36, 0x4EF860}, {0x3B, 0x4EFA00}, {0x40, 0x5A7BC0}};
// 0x4EF840: 0x14 bytes  MagicFx_WaitOwnerChildren
// 0x4EF860: 0x195 bytes  LastResortRing_DrawDisc
constexpr mh::CallSite kCalls4EF860[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x28, 0x5A7A00}, {0x3E, 0x5A7A50}, {0x85, 0x5A7A00}, {0x9A, 0x5A7A50}, {0xCB, 0x5A75F0}, {0xD2, 0x5A7780}, {0xFC, 0x5A84A0}, {0x102, 0x5A9310}, {0x157, 0x461E50}, {0x17C, 0x5A77C0}, {0x185, 0x461E50}};
// 0x4EFA00: 0x1F7 bytes  LastResortRing_DrawBand
constexpr mh::CallSite kCalls4EFA00[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x28, 0x5A7A00}, {0x3E, 0x5A7A50}, {0x54, 0x5A7A00}, {0x66, 0x5A7A50}, {0xB8, 0x5A7A00}, {0xCE, 0x5A7A50}, {0xE3, 0x5A7A00}, {0xF5, 0x5A7A50}, {0x129, 0x5A7610}, {0x130, 0x5A7780}, {0x163, 0x5A85F0}, {0x16C, 0x5A9350}, {0x1B9, 0x461E50}, {0x1DE, 0x5A77C0}, {0x1E7, 0x461E50}};
// 0x4EFC00: 0x50 bytes; +0xB note: call through .data 0x65c070 (a data_tables entry)  LastResortBeam_Run
constexpr mh::CallSite kCalls4EFC00[] = {{0x23, 0x4B7D40}, {0x34, 0x4EFD30}, {0x3B, 0x4F0470}, {0x42, 0x4F0470}, {0x4A, 0x5A7BC0}};
// 0x4EFC50: 0x26 bytes  LastResortBeam_Wait
// 0x4EFC80: 0x3D bytes  LastResortBeam_Rise
// 0x4EFCC0: 0x37 bytes  LastResortBeam_Shrink
// 0x4EFD00: 0x26 bytes  LastResortBeam_End
constexpr mh::CallSite kCalls4EFD00[] = {{0x20, 0x4351F0}};
// 0x4EFD30: 0x739 bytes  LastResortBeam_DrawColumn
constexpr mh::CallSite kCalls4EFD30[] = {{0x51, 0x5A7A00}, {0x69, 0x5A7A50}, {0x144, 0x5A7A00}, {0x15F, 0x5A7A50}, {0x1E6, 0x5A77C0}, {0x1F1, 0x572FA0}, {0x1FD, 0x5A7610}, {0x205, 0x5A7780}, {0x238, 0x5A85F0}, {0x241, 0x5A9350}, {0x307, 0x572FA0}, {0x346, 0x5A7A00}, {0x362, 0x5A7A50}, {0x3AE, 0x5A7610}, {0x3B6, 0x5A7780}, {0x3E9, 0x5A85F0}, {0x3F2, 0x5A9350}, {0x43F, 0x572FA0}, {0x486, 0x5A7A00}, {0x4A2, 0x5A7A50}, {0x4EB, 0x5A7610}, {0x4F3, 0x5A7780}, {0x526, 0x5A85F0}, {0x52F, 0x5A9350}, {0x57D, 0x572FA0}, {0x5C0, 0x5A7A00}, {0x5DC, 0x5A7A50}, {0x640, 0x5A77C0}, {0x64F, 0x572FA0}, {0x65E, 0x5A7610}, {0x666, 0x5A7780}, {0x699, 0x5A85F0}, {0x69F, 0x5A9350}, {0x70D, 0x572FA0}};
// 0x4F0470: 0x1F7 bytes  LastResortBeam_DrawSparks
constexpr mh::CallSite kCalls4F0470[] = {{0x61, 0x5B93D2}, {0x84, 0x5B93D2}, {0xE1, 0x5A7A00}, {0xFD, 0x5A7A50}, {0x162, 0x5A77C0}, {0x16D, 0x572FA0}, {0x179, 0x5A7750}, {0x181, 0x5A7780}, {0x199, 0x5A8250}, {0x1A5, 0x5A9110}, {0x1CA, 0x572FA0}};
// 0x4F0670: 0x81 bytes  Cure_Task
constexpr mh::CallSite kCalls4F0670[] = {{0x69, 0x4F0850}};
constexpr mh::Imm kImms4F0670[] = {{0x15, 0x4F0700}, {0x1D, 0x4B1E70}, {0x25, 0x4B1ED0}, {0x2D, 0x4EE8A0}, {0x35, 0x4B8F50}, {0x3D, 0x4F7350}};
// 0x4F0700: 0x149 bytes  Cure_Start
constexpr mh::CallSite kCalls4F0700[] = {{0x5B, 0x4FBD10}, {0x8A, 0x587900}, {0xAD, 0x4F1120}, {0xE6, 0x5B93D2}};
// 0x4F0850: 0x12 bytes; +0xB note: jmp through .data 0x65c174 (a data_tables entry)  CureMote_Task
// 0x4F0870: 0x8F bytes  CureMote_Run
constexpr mh::CallSite kCalls4F0870[] = {{0x3B, 0x4F0EF0}, {0x59, 0x4F0B70}, {0x83, 0x4F0D00}};
constexpr mh::Imm kImms4F0870[] = {{0xF, 0x4F0900}, {0x17, 0x4F0A70}, {0x22, 0x4F0AE0}};
// 0x4F0900: 0x161 bytes  CureMote_Wait
constexpr mh::CallSite kCalls4F0900[] = {{0x8C, 0x5B93D2}, {0xB0, 0x5B93D2}, {0xD7, 0x5B93D2}, {0x103, 0x5B93D2}, {0x115, 0x5B93D2}, {0x124, 0x5B93D2}};
// 0x4F0A70: 0x6C bytes  CureMote_Rise
constexpr mh::CallSite kCalls4F0A70[] = {{0x21, 0x5A7A00}};
// 0x4F0AE0: 0x85 bytes  CureMote_Fade
constexpr mh::CallSite kCalls4F0AE0[] = {{0x21, 0x5A7A00}, {0x7F, 0x4F1170}};
// 0x4F0B70: 0x185 bytes  CureMote_DrawRays
constexpr mh::CallSite kCalls4F0B70[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xA8, 0x5A76B0}, {0xB0, 0x5A7780}, {0xF0, 0x5A7A50}, {0x117, 0x5A7A00}, {0x16D, 0x572FA0}};
// 0x4F0D00: 0x1ED bytes  CureMote_DrawArcs
constexpr mh::CallSite kCalls4F0D00[] = {{0x2B, 0x5A77C0}, {0x41, 0x572FA0}, {0xB1, 0x5A76D0}, {0xB9, 0x5A7780}, {0xF9, 0x5A7A50}, {0x120, 0x5A7A00}, {0x147, 0x5A7A50}, {0x16E, 0x5A7A00}, {0x1D0, 0x572FA0}};
// 0x4F0EF0: 0x22F bytes  CureMote_DrawStar
constexpr mh::CallSite kCalls4F0EF0[] = {{0x11, 0x5A77C0}, {0x27, 0x572FA0}, {0x2F, 0x5B93D2}, {0xE5, 0x5A75F0}, {0xED, 0x5A7780}, {0x117, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x16B, 0x5A7A00}, {0x192, 0x5A7A50}, {0x217, 0x572FA0}};
// 0x4F1120: 0x4A bytes  CureMote_Alloc
// 0x4F1170: 0x2F bytes  CureMote_Free
// 0x4F11A0: 0xA1 bytes  Benediction_Task
constexpr mh::CallSite kCalls4F11A0[] = {{0x48, 0x4FBD10}, {0x4D, 0x4F13C0}, {0x7F, 0x4F1850}};
constexpr mh::Imm kImms4F11A0[] = {{0x16, 0x4F1250}, {0x1E, 0x4F12E0}, {0x26, 0x4F1390}, {0x2E, 0x43FE80}};
// 0x4F1250: 0x8D bytes  Benediction_Start
constexpr mh::CallSite kCalls4F1250[] = {{0x1D, 0x4FC0E0}, {0x69, 0x587900}};
// 0x4F12E0: 0xAF bytes  Benediction_Spawn
constexpr mh::CallSite kCalls4F12E0[] = {{0x40, 0x435180}};
// 0x4F1390: 0x24 bytes  Benediction_Wait
// 0x4F13C0: 0x133 bytes  Benediction_DrawHalo
constexpr mh::CallSite kCalls4F13C0[] = {{0x17, 0x5A77C0}, {0x2D, 0x572FA0}, {0x55, 0x5A75D0}, {0x5D, 0x5A7780}, {0xBF, 0x5A79A0}, {0xD2, 0x5A79E0}, {0x124, 0x572FA0}};
// 0x4F1500: 0x12 bytes; +0xB note: jmp through .data 0x65c178 (a data_tables entry)  BenedictionChild_Task
// 0x4F1520: 0x46 bytes  BenedictionChild_Run
constexpr mh::Imm kImms4F1520[] = {{0xF, 0x4F1570}, {0x17, 0x4F1660}, {0x22, 0x4F16D0}, {0x2A, 0x4BB370}, {0x32, 0x4F1760}, {0x3A, 0x4F1800}};
// 0x4F1570: 0xED bytes  BenedictionChild_Start
constexpr mh::CallSite kCalls4F1570[] = {{0x93, 0x4F1DE0}};
// 0x4F1660: 0x64 bytes  BenedictionChild_Tint
constexpr mh::CallSite kCalls4F1660[] = {{0x32, 0x454DC0}, {0x40, 0x454CC0}};
// 0x4F16D0: 0x84 bytes  BenedictionChild_Brighten
constexpr mh::CallSite kCalls4F16D0[] = {{0x73, 0x587900}};
// 0x4F1760: 0x99 bytes  BenedictionChild_Fade
constexpr mh::CallSite kCalls4F1760[] = {{0x79, 0x454DC0}, {0x88, 0x4FBDB0}};
// 0x4F1800: 0x46 bytes  BenedictionChild_End
constexpr mh::CallSite kCalls4F1800[] = {{0x23, 0x4456C0}, {0x38, 0x4530D0}, {0x40, 0x4351F0}};
// 0x4F1850: 0x12 bytes; +0xB note: jmp through .data 0x65c17c (a data_tables entry)  BenedictionMote_Task
// 0x4F1870: 0xA4 bytes  BenedictionMote_Run
constexpr mh::CallSite kCalls4F1870[] = {{0x39, 0x5A77C0}, {0x4F, 0x572FA0}, {0x68, 0x4FBD10}, {0x6D, 0x4BD8A0}, {0x72, 0x4F1BD0}, {0x85, 0x5A77C0}, {0x9B, 0x572FA0}};
constexpr mh::Imm kImms4F1870[] = {{0xF, 0x4F1920}, {0x17, 0x4F1A40}, {0x22, 0x4F1B00}};
// 0x4F1920: 0x119 bytes  BenedictionMote_Launch
constexpr mh::CallSite kCalls4F1920[] = {{0x41, 0x5A7A00}, {0x65, 0x5A7A50}, {0x9B, 0x5B93D2}, {0xC7, 0x5B93D2}, {0xD9, 0x5B93D2}, {0xEB, 0x5B93D2}};
// 0x4F1A40: 0xB5 bytes  BenedictionMote_Spiral
constexpr mh::CallSite kCalls4F1A40[] = {{0x2C, 0x5A7A00}, {0x50, 0x5A7A50}};
// 0x4F1B00: 0xC7 bytes  BenedictionMote_Fade
constexpr mh::CallSite kCalls4F1B00[] = {{0x2A, 0x5A7A00}, {0x4E, 0x5A7A50}, {0xC1, 0x4F6290}};
// 0x4F1BD0: 0x207 bytes  BenedictionMote_DrawGlow
constexpr mh::CallSite kCalls4F1BD0[] = {{0x78, 0x5A7610}, {0x80, 0x5A7780}, {0x86, 0x5A7A00}, {0xA6, 0x5A7A50}, {0xC6, 0x5A7A00}, {0xE6, 0x5A7A50}, {0x10C, 0x5A7A00}, {0x12C, 0x5A7A50}, {0x14C, 0x5A7A00}, {0x16C, 0x5A7A50}, {0x1E8, 0x572FA0}};
// 0x4F1DE0: 0x57 bytes  BenedictionMote_Alloc
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"LastResort_Task", 0x4EF620, 0x46, nullptr, 0, kImms4EF620, MH_N(kImms4EF620), nullptr, 0, reinterpret_cast<const void*>(&::LastResort_Task)},
    {"LastResort_Start", 0x4EF670, 0x144, kCalls4EF670, MH_N(kCalls4EF670), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResort_Start)},
    {"LastResort_WaitChildren", 0x4EF7C0, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResort_WaitChildren)},
    {"LastResortChild_Task", 0x4EF7D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortChild_Task)},
    {"LastResortRing_Run", 0x4EF7F0, 0x46, kCalls4EF7F0, MH_N(kCalls4EF7F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortRing_Run)},
    {"MagicFx_WaitOwnerChildren", 0x4EF840, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_WaitOwnerChildren)},
    {"LastResortRing_DrawDisc", 0x4EF860, 0x195, kCalls4EF860, MH_N(kCalls4EF860), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortRing_DrawDisc)},
    {"LastResortRing_DrawBand", 0x4EFA00, 0x1F7, kCalls4EFA00, MH_N(kCalls4EFA00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortRing_DrawBand)},
    {"LastResortBeam_Run", 0x4EFC00, 0x50, kCalls4EFC00, MH_N(kCalls4EFC00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortBeam_Run)},
    {"LastResortBeam_Wait", 0x4EFC50, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortBeam_Wait)},
    {"LastResortBeam_Rise", 0x4EFC80, 0x3D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortBeam_Rise)},
    {"LastResortBeam_Shrink", 0x4EFCC0, 0x37, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortBeam_Shrink)},
    {"LastResortBeam_End", 0x4EFD00, 0x26, kCalls4EFD00, MH_N(kCalls4EFD00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortBeam_End)},
    {"LastResortBeam_DrawColumn", 0x4EFD30, 0x739, kCalls4EFD30, MH_N(kCalls4EFD30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortBeam_DrawColumn)},
    {"LastResortBeam_DrawSparks", 0x4F0470, 0x1F7, kCalls4F0470, MH_N(kCalls4F0470), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LastResortBeam_DrawSparks)},
    {"Cure_Task", 0x4F0670, 0x81, kCalls4F0670, MH_N(kCalls4F0670), kImms4F0670, MH_N(kImms4F0670), nullptr, 0, reinterpret_cast<const void*>(&::Cure_Task)},
    {"Cure_Start", 0x4F0700, 0x149, kCalls4F0700, MH_N(kCalls4F0700), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Cure_Start)},
    {"CureMote_Task", 0x4F0850, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_Task)},
    {"CureMote_Run", 0x4F0870, 0x8F, kCalls4F0870, MH_N(kCalls4F0870), kImms4F0870, MH_N(kImms4F0870), nullptr, 0, reinterpret_cast<const void*>(&::CureMote_Run)},
    {"CureMote_Wait", 0x4F0900, 0x161, kCalls4F0900, MH_N(kCalls4F0900), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_Wait)},
    {"CureMote_Rise", 0x4F0A70, 0x6C, kCalls4F0A70, MH_N(kCalls4F0A70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_Rise)},
    {"CureMote_Fade", 0x4F0AE0, 0x85, kCalls4F0AE0, MH_N(kCalls4F0AE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_Fade)},
    {"CureMote_DrawRays", 0x4F0B70, 0x185, kCalls4F0B70, MH_N(kCalls4F0B70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_DrawRays)},
    {"CureMote_DrawArcs", 0x4F0D00, 0x1ED, kCalls4F0D00, MH_N(kCalls4F0D00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_DrawArcs)},
    {"CureMote_DrawStar", 0x4F0EF0, 0x22F, kCalls4F0EF0, MH_N(kCalls4F0EF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_DrawStar)},
    {"CureMote_Alloc", 0x4F1120, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_Alloc), 0xFF},
    {"CureMote_Free", 0x4F1170, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CureMote_Free)},
    {"Benediction_Task", 0x4F11A0, 0xA1, kCalls4F11A0, MH_N(kCalls4F11A0), kImms4F11A0, MH_N(kImms4F11A0), nullptr, 0, reinterpret_cast<const void*>(&::Benediction_Task)},
    {"Benediction_Start", 0x4F1250, 0x8D, kCalls4F1250, MH_N(kCalls4F1250), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Benediction_Start)},
    {"Benediction_Spawn", 0x4F12E0, 0xAF, kCalls4F12E0, MH_N(kCalls4F12E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Benediction_Spawn)},
    {"Benediction_Wait", 0x4F1390, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Benediction_Wait)},
    {"Benediction_DrawHalo", 0x4F13C0, 0x133, kCalls4F13C0, MH_N(kCalls4F13C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Benediction_DrawHalo)},
    {"BenedictionChild_Task", 0x4F1500, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionChild_Task)},
    {"BenedictionChild_Run", 0x4F1520, 0x46, nullptr, 0, kImms4F1520, MH_N(kImms4F1520), nullptr, 0, reinterpret_cast<const void*>(&::BenedictionChild_Run)},
    {"BenedictionChild_Start", 0x4F1570, 0xED, kCalls4F1570, MH_N(kCalls4F1570), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionChild_Start)},
    {"BenedictionChild_Tint", 0x4F1660, 0x64, kCalls4F1660, MH_N(kCalls4F1660), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionChild_Tint)},
    {"BenedictionChild_Brighten", 0x4F16D0, 0x84, kCalls4F16D0, MH_N(kCalls4F16D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionChild_Brighten)},
    {"BenedictionChild_Fade", 0x4F1760, 0x99, kCalls4F1760, MH_N(kCalls4F1760), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionChild_Fade)},
    {"BenedictionChild_End", 0x4F1800, 0x46, kCalls4F1800, MH_N(kCalls4F1800), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionChild_End)},
    {"BenedictionMote_Task", 0x4F1850, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionMote_Task)},
    {"BenedictionMote_Run", 0x4F1870, 0xA4, kCalls4F1870, MH_N(kCalls4F1870), kImms4F1870, MH_N(kImms4F1870), nullptr, 0, reinterpret_cast<const void*>(&::BenedictionMote_Run)},
    {"BenedictionMote_Launch", 0x4F1920, 0x119, kCalls4F1920, MH_N(kCalls4F1920), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionMote_Launch)},
    {"BenedictionMote_Spiral", 0x4F1A40, 0xB5, kCalls4F1A40, MH_N(kCalls4F1A40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionMote_Spiral)},
    {"BenedictionMote_Fade", 0x4F1B00, 0xC7, kCalls4F1B00, MH_N(kCalls4F1B00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionMote_Fade)},
    {"BenedictionMote_DrawGlow", 0x4F1BD0, 0x207, kCalls4F1BD0, MH_N(kCalls4F1BD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionMote_DrawGlow)},
    {"BenedictionMote_Alloc", 0x4F1DE0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BenedictionMote_Alloc), 0xFF},
};
#undef MH_N

enum : unsigned {
    kLastResort_Task, kLastResort_Start, kLastResort_WaitChildren, kLastResortChild_Task, kLastResortRing_Run,
    kMagicFx_WaitOwnerChildren, kLastResortRing_DrawDisc, kLastResortRing_DrawBand, kLastResortBeam_Run,
    kLastResortBeam_Wait, kLastResortBeam_Rise, kLastResortBeam_Shrink, kLastResortBeam_End, kLastResortBeam_DrawColumn,
    kLastResortBeam_DrawSparks,
    kCure_Task, kCure_Start, kCureMote_Task, kCureMote_Run, kCureMote_Wait, kCureMote_Rise, kCureMote_Fade,
    kCureMote_DrawRays, kCureMote_DrawArcs, kCureMote_DrawStar, kCureMote_Alloc, kCureMote_Free,
    kBenediction_Task, kBenediction_Start, kBenediction_Spawn, kBenediction_Wait, kBenediction_DrawHalo,
    kBenedictionChild_Task, kBenedictionChild_Run, kBenedictionChild_Start, kBenedictionChild_Tint,
    kBenedictionChild_Brighten, kBenedictionChild_Fade, kBenedictionChild_End, kBenedictionMote_Task,
    kBenedictionMote_Run, kBenedictionMote_Launch, kBenedictionMote_Spiral, kBenedictionMote_Fade,
    kBenedictionMote_DrawGlow, kBenedictionMote_Alloc, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kPartyCount = 0x904AB0;
constexpr std::uint32_t kCurePool = 0x6AE910, kCureStride = 0x2C, kCureCurrent = 0x6AFF10;
constexpr unsigned kCureCount = 0x80;
constexpr std::uint32_t kBlessPool = 0x6AFF18, kBlessStride = 0x84;
constexpr unsigned kBlessCount = 0x50;
constexpr std::uint32_t kPoolsEnd = kBlessPool + kBlessCount * kBlessStride;

unsigned char* CureRecord(unsigned n) { return mh::Mem(kCurePool + (n % kCureCount) * kCureStride); }
unsigned char* BlessRecord(unsigned n) { return mh::Mem(kBlessPool + (n % kBlessCount) * kBlessStride); }
unsigned char* Cur() { return mh::Pointer(kCureCurrent); }

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

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage, kP = mh::Answer::kPhase;
#define S35_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S35_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // the sprite and battle calls
    {S35_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    {S35_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    {S35_OURS(ReviveMote_Draw), 0, {}, kP, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, field_misc,
    // battle_items: all ours)
    {S35_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S35_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S35_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S35_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S35_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S35_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S35_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Gpu_SetTile1), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Gpu_SetLineG2), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Gpu_SetLineG3), 1, {kAll}, kG, 0, 0},
    // the projections: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S35_OURS(Gte_RotTransPers), 4, {kAll, kAll, 0, 0}, kG, 0, 0, {6}},
    {S35_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S35_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S35_OURS(Gte_StoreDepthF), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S35_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // MAGIC219's (group S37), a tail jmp: the pool slot's free
    {S35_RAW(0x4F6290), 0, {}, kP, 0, 0},
    // this group's own, called directly
    {S35_RAW(0x4EF860), 0, {}, kP, 0, 0},
    {S35_RAW(0x4EFA00), 0, {}, kP, 0, 0},
    {S35_RAW(0x4EFD30), 0, {}, kP, 0, 0},
    {S35_RAW(0x4F0470), 1, {kAll}, kG, 0, 0},
    {S35_RAW(0x4F13C0), 0, {}, kP, 0, 0},
    {S35_RAW(0x4F1850), 0, {}, kP, 0, 0},
    {S35_RAW(0x4F1BD0), 0, {}, kP, 0, 0},
    {S35_RAW(0x4F1DE0), 0, {}, mh::Answer::kByte, 0xFF, kBlessCount - 1},
    {S35_RAW(0x4F1120), 0, {}, mh::Answer::kByte, 0xFF, kCureCount - 1},
    // MAGIC168's motes, run through CureMote_Current: called directly or held
    // by its tables; each also logs the current mote
    {S35_RAW(0x4F0850), 0, {kCureCurrent}, kP, 0, 0},
    {S35_RAW(0x4F0870), 0, {kCureCurrent}, kP, 0, 0},
    {S35_RAW(0x4F0900), 0, {kCureCurrent}, kP, 0, 0},
    {S35_RAW(0x4F0A70), 0, {kCureCurrent}, kP, 0, 0},
    {S35_RAW(0x4F0AE0), 0, {kCureCurrent}, kP, 0, 0},
    {S35_RAW(0x4F0EF0), 0, {kCureCurrent}, kP, 0, 0},
    {S35_RAW(0x4F1170), 0, {kCureCurrent}, kP, 0, 0},
    {S35_RAW(0x4F0B70), 2, {kU16, kU16}, kG, 0, 0},
    {S35_RAW(0x4F0D00), 2, {kU16, kU16}, kG, 0, 0},
};
#undef S35_OURS
#undef S35_RAW

// The six .data handler tables the dispatchers read in place
// (LastResortChild_Kinds and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65C05C, 2}, {0x65C064, 3}, {0x65C070, 4}, {0x65C174, 1}, {0x65C178, 1}, {0x65C17C, 1},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850.., Scratch_Swap at +0xC
    {0x7E0700, 0xC00},                    // MoveScript_TintRecords
    {0x80E980, 0x200},                    // Gfx_ClutStripSource row 26
    {0x812980, 0x200},                    // Gfx_ClutStrip row 26
    {kCurePool, kPoolsEnd - kCurePool},   // CureMote_Pool, CureMote_Current, BenedictionMote_Pool
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }

// An owner for a pool record: one of the harness's task slots or records.
const void* AnOwner(std::uint32_t v) {
    return (v & 4) != 0 ? static_cast<const void*>(mh::SpriteRecord(v)) : static_cast<const void*>(mh::TaskAt(v));
}

// The group's cells a recorder may move (the harness's case 14). A pool
// record's owner pointer (+0x28 / +0x80) is never moved: the tasks put it in
// the owner cell, which the next disturbance writes through.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 7) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: mh::Mem(kScratch + v % 16)[0] = Byte(h >> 24); break;
    case 3: mh::SetPointer(kCureCurrent, CureRecord(v)); break;
    case 4: CureRecord(v >> 4)[v % 0x28] = Byte(h >> 24); break;
    case 5: BlessRecord(v >> 4)[v % 0x80] = Byte(h >> 24); break;
    case 6: mh::Mem(0x7E0700 + v % 0xC00)[0] = Byte(h >> 24); break;
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

void Seed(unsigned k) {
    // the pools: every owner pointer at a slot or record, the current mote at
    // a record; a full pool one round in eight
    for (unsigned n = 0; n < kCureCount; ++n) mh::SetPointer(kCurePool + n * kCureStride + 0x28, AnOwner(mh::Next()));
    for (unsigned n = 0; n < kBlessCount; ++n) mh::SetPointer(kBlessPool + n * kBlessStride + 0x80, AnOwner(mh::Next()));
    mh::SetPointer(kCureCurrent, CureRecord(mh::Next()));
    if (mh::Next() % 8 == 0) {
        for (unsigned n = 0; n < kCureCount; ++n) CureRecord(n)[0] |= 1;
        for (unsigned n = 0; n < kBlessCount; ++n) BlessRecord(n)[0] |= 1;
    }
    Gfx_PacketNext = PrimAt(mh::Next());
    unsigned char* const sc = Sc();
    unsigned char* const cur = Cur();
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kLastResort_Task: case kCure_Task: sc[1] = Byte(mh::Next() % 6); break;
    case kLastResortChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kLastResortRing_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kLastResortBeam_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kBenediction_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kBenedictionChild_Task: case kBenedictionMote_Task: sc[1] = 0; break;
    case kBenedictionChild_Run: sc[2] = Byte(mh::Next() % 6); break;
    case kBenedictionMote_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kCureMote_Task: cur[1] = 0; break;
    case kCureMote_Run:
        cur[2] = Byte(mh::Next() % 3);
        if (mh::Half()) cur[0xC] &= 0xFC;
        break;
    // the counters: at their thresholds
    case kLastResort_WaitChildren: sc[0xB] = Byte(mh::Next() % 3); break;
    case kMagicFx_WaitOwnerChildren: mh::Pointer(mh::at::kOwner)[0xB] = Byte(mh::Next() % 3); break;
    case kLastResortBeam_Wait: Near(sc[9], 1); break;
    case kLastResortBeam_Rise:
        Near(sc[9], 0x17);
        Near(sc[0xA], 0xF);
        break;
    case kLastResortBeam_Shrink: Near(sc[9], 0x27); break;
    case kLastResortBeam_End: Near(sc[0xA], 0x17); break;
    case kCureMote_Wait: Near(cur[5], 1); break;
    case kCureMote_Rise: if (mh::Half()) cur[6] = Byte(cur[7] - 1u - (mh::Half() ? 1u : 0u)); break;
    case kCureMote_Fade:
        Near(cur[5], 1);
        if (mh::Half()) Frame_Counter &= ~3u;
        break;
    case kBenediction_Spawn:
        Near(sc[9], 0xF);
        if (mh::Half()) mh::Mem(mh::at::kTarget)[0] |= 0x80;
        if (mh::Often()) mh::Mem(kPartyCount)[0] = Byte(mh::Next() % 5);
        break;
    case kBenediction_Wait:
        if (mh::Half()) sc[0xB] = 0;
        Near(sc[9], 1);
        break;
    case kBenedictionChild_Start: case kBenedictionChild_Tint: case kBenedictionChild_Brighten:
    case kBenedictionChild_Fade:
        Near(sc[9], 1);
        if (mh::Often()) sc[4] = Byte(mh::Next() % 3);
        break;
    case kBenedictionChild_End:
        if (mh::Half()) sc[0xB] = 0;
        if (mh::Often()) sc[4] = Byte(mh::Next() % 5);
        break;
    case kBenedictionMote_Launch: Near(sc[9], 1); break;
    case kBenedictionMote_Spiral:
    case kBenedictionMote_Fade:
        if (k == kBenedictionMote_Spiral) {
            if (mh::Half()) sc[9] = Byte(0xE - (mh::Half() ? 1u : 0u));
        } else {
            Near(sc[9], 1);
        }
        // the rise's compare: +0x20 against +0x14, equal a third of the time
        if (mh::Often()) SetLong(sc + 0x20, static_cast<std::int32_t>(mh::Next() % 0x40000));
        if (mh::Half()) SetLong(sc + 0x14, Long(sc + 0x20));
        break;
    // the draws: short loops, both sides of each branch
    case kLastResortBeam_DrawColumn:
        if (mh::Often()) sc[0xA] = Byte(mh::Next() % 0x20);
        if (mh::Half()) sc[2] = 2;
        if (mh::Half()) sc[9] = Byte(0x18 + mh::Next() % 0x20);
        break;
    case kLastResortBeam_DrawSparks:
        if (mh::Half()) sc[2] = 3;
        if (mh::Often()) sc[0xA] = Byte(mh::Next() % 0x1C);
        break;
    default: break;
    }
}

// LastResortBeam_DrawSparks' k: its callers' 2 and 3 most of the time, else
// another positive step (k 0 or below never ends its loop, in the original as
// in ours).
void Args(unsigned k, std::uint32_t* a) {
    if (k == kLastResortBeam_DrawSparks) a[0] = mh::Often() ? 2 + mh::Next() % 2 : 1 + mh::Next() % 6;
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s35", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s35
