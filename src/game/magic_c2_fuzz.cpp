// BOF3X_SHADOW=magic_c2: group C2's four overlays (MAGIC057, 081, 116, 129)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_c2.md section 7.
//
// The clone table is tools/magic_rows.py --unit MAGIC057 / 081 / 116 / 129
// --clones (2026-09-26; capstone, every jump internal, no jump table, nothing
// REFUSED), names given. What the group adds, all through the harness's
// fields (no harness edit):
//
//   - the draws: the packet pointer aimed into a buffer of the fuzz's own, and
//     every commit (Gfx_CommitPrim, MapView_LinkPrimAt) logs the packet as it
//     stands - the stand-ins never advance the pointer, so every primitive of
//     a draw is built in the same bytes; 0x494110's vector (a local of the
//     caller's) logged by its twelve bytes (`deref`);
//   - the callees that act on the current task (the caster swaps, the screen
//     updates, the steps) note Sprite_Current, so a swap left out shows;
//   - the four functions that answer (two allocators, the any-live flag, the
//     free-streak pointer) carry `ret_mask`;
//   - a `settle` that keeps +0xA of the four task slots below 0x14 while
//     HolocaustBeam_Draw runs (it loops to +0xA, read each time, and never
//     ends at 0xFF), and `args` handing a streak record to the two streak
//     functions that take one.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_c2.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_c2 {
namespace {

namespace mh = magic_harness;
namespace at = magic_harness::at;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

// tools/magic_rows.py --unit MAGIC057 / 081 / 116 / 129 --clones, 2026-09-26, names given.
// 0x4AE7C0: 0x2E bytes
constexpr mh::Imm kImms4AE7C0[] = {{0xF, 0x4AE7F0}, {0x17, 0x4AE850}, {0x22, 0x4AEA20}};
// 0x4AE7F0: 0x59 bytes
constexpr mh::CallSite kCalls4AE7F0[] = {{0x35, 0x4549F0}};
// 0x4AE850: 0x36 bytes
constexpr mh::Imm kImms4AE850[] = {{0xF, 0x4AE890}, {0x17, 0x4AE920}, {0x22, 0x4AE960}, {0x2A, 0x4AE9F0}};
// 0x4AE890: 0x83 bytes
constexpr mh::CallSite kCalls4AE890[] = {{0x7, 0x587900}, {0x11, 0x587900}, {0x2F, 0x5891F0}, {0x3E, 0x435180}};
// 0x4AE920: 0x33 bytes
constexpr mh::CallSite kCalls4AE920[] = {{0x18, 0x589410}};
// 0x4AE960: 0x82 bytes
constexpr mh::CallSite kCalls4AE960[] = {{0x19, 0x435180}};
// 0x4AE9F0: 0x22 bytes
// 0x4AEA20: 0x1A bytes
constexpr mh::CallSite kCalls4AEA20[] = {{0xD, 0x4530D0}, {0x15, 0x4351F0}};
// 0x4AEA40: 0x2E bytes
constexpr mh::Imm kImms4AEA40[] = {{0xF, 0x4AEA70}, {0x17, 0x4AED00}, {0x22, 0x4AEEA0}};
// 0x4AEA70: 0x52 bytes
constexpr mh::CallSite kCalls4AEA70[] = {{0x3F, 0x588F20}};
constexpr mh::Imm kImms4AEA70[] = {{0x19, 0x4AEAD0}, {0x24, 0x4AEC20}, {0x2C, 0x4AECF0}};
// 0x4AEAD0: 0x144 bytes
constexpr mh::CallSite kCalls4AEAD0[] = {{0xB, 0x5B93D2}, {0x1D, 0x5B93D2}, {0x4F, 0x5B93D2}, {0x61, 0x5B93D2}, {0x129, 0x5891F0}};
// 0x4AEC20: 0xC8 bytes
constexpr mh::CallSite kCalls4AEC20[] = {{0x1D, 0x5893A0}, {0x2F, 0x5720C0}, {0x47, 0x5B93D2}, {0x6D, 0x5B93D2}};
// 0x4AECF0: 0xD bytes
constexpr mh::CallSite kCalls4AECF0[] = {{0x8, 0x4351F0}};
// 0x4AED00: 0x2E bytes
constexpr mh::Imm kImms4AED00[] = {{0xF, 0x4AED30}, {0x17, 0x4AED70}, {0x22, 0x4AEE90}};
// 0x4AED30: 0x3E bytes
constexpr mh::CallSite kCalls4AED30[] = {{0x21, 0x587900}};
// 0x4AED70: 0xDB bytes
constexpr mh::CallSite kCalls4AED70[] = {{0x60, 0x587900}};
constexpr mh::Imm kImms4AED70[] = {{0xF, 0x4AEE50}, {0x17, 0x4AEE70}};
// 0x4AEE50: 0x16 bytes
// 0x4AEE70: 0x15 bytes
// 0x4AEEA0: 0x5E bytes
constexpr mh::CallSite kCalls4AEEA0[] = {{0x4B, 0x588F20}};
constexpr mh::Imm kImms4AEEA0[] = {{0x19, 0x4AEF00}, {0x24, 0x4AEFE0}, {0x2C, 0x4AEE90}};
// 0x4AEF00: 0xD7 bytes
constexpr mh::CallSite kCalls4AEF00[] = {{0x31, 0x5720C0}, {0xC0, 0x5891F0}};
// 0x4AEFE0: 0x56 bytes
constexpr mh::CallSite kCalls4AEFE0[] = {{0x30, 0x5720C0}};
// 0x4BF8D0: 0x85 bytes
constexpr mh::CallSite kCalls4BF8D0[] = {{0x63, 0x4BFC80}};
constexpr mh::Imm kImms4BF8D0[] = {{0x16, 0x4BF960}, {0x1E, 0x43F430}, {0x26, 0x4BFA20}, {0x2E, 0x4BFA90}};
// 0x4BF960: 0xB5 bytes
constexpr mh::CallSite kCalls4BF960[] = {{0x9D, 0x5891F0}};
// 0x4BFA20: 0x64 bytes
constexpr mh::CallSite kCalls4BFA20[] = {{0x6, 0x435180}, {0x4A, 0x589410}};
// 0x4BFA90: 0x47 bytes
constexpr mh::CallSite kCalls4BFA90[] = {{0x18, 0x589410}, {0x32, 0x4530D0}, {0x41, 0x4351F0}};
// 0x4BFAE0: 0x12 bytes; +0xB note: jmp through .data 0x65b358, 2 code entries (a data_tables entry)
// 0x4BFB00: 0x2E bytes
constexpr mh::Imm kImms4BFB00[] = {{0xF, 0x4BFB30}, {0x17, 0x4BFBD0}, {0x22, 0x4D1AA0}};
// 0x4BFB30: 0x9D bytes
constexpr mh::CallSite kCalls4BFB30[] = {{0x40, 0x435180}, {0x78, 0x587900}};
// 0x4BFBD0: 0x90 bytes
constexpr mh::CallSite kCalls4BFBD0[] = {{0xE, 0x4C0190}};
// 0x4BFC60: 0x13 bytes
constexpr mh::CallSite kCalls4BFC60[] = {{0x9, 0x4351F0}, {0xE, 0x4FC0E0}};
// 0x4BFC80: 0x36 bytes
constexpr mh::Imm kImms4BFC80[] = {{0xF, 0x4BFCC0}, {0x17, 0x4BFD70}, {0x22, 0x4BFE40}, {0x2A, 0x4BFE90}};
// 0x4BFCC0: 0xB0 bytes
constexpr mh::CallSite kCalls4BFCC0[] = {{0x8A, 0x5A7A70}};
// 0x4BFD70: 0xC1 bytes
constexpr mh::CallSite kCalls4BFD70[] = {{0x21, 0x5A7A00}, {0x4E, 0x5A7A00}, {0x74, 0x5A7A50}, {0x91, 0x4FBD10}, {0x96, 0x4BFEE0}};
// 0x4BFE40: 0x4B bytes
constexpr mh::CallSite kCalls4BFE40[] = {{0x1C, 0x4FB9F0}, {0x24, 0x4FBD10}, {0x29, 0x4BFEE0}};
// 0x4BFE90: 0x46 bytes
constexpr mh::CallSite kCalls4BFE90[] = {{0x0, 0x4FBD10}, {0x5, 0x4BFEE0}};
// 0x4BFEE0: 0x2AA bytes
constexpr mh::CallSite kCalls4BFEE0[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0x37, 0x5A7630}, {0x8D, 0x5A7A00}, {0xB8, 0x5A7A50}, {0xE3, 0x5A7A00}, {0x10E, 0x5A7A50}, {0x136, 0x5A7A00}, {0x15E, 0x5A7A50}, {0x189, 0x5A7A00}, {0x1B4, 0x5A7A50}, {0x1E8, 0x5A79A0}, {0x1F8, 0x5A79E0}, {0x285, 0x5A7780}, {0x29E, 0x572FA0}};
// 0x4C0190: 0x57 bytes
// 0x4D9AE0: 0x36 bytes
constexpr mh::Imm kImms4D9AE0[] = {{0xF, 0x4D9B20}, {0x17, 0x4D9BB0}, {0x22, 0x4D9C00}, {0x2A, 0x4D9C50}};
// 0x4D9B20: 0x83 bytes
constexpr mh::CallSite kCalls4D9B20[] = {{0x4D, 0x4D9C70}, {0x6B, 0x5891F0}};
// 0x4D9BB0: 0x48 bytes
constexpr mh::CallSite kCalls4D9BB0[] = {{0x18, 0x589410}, {0x38, 0x587740}};
// 0x4D9C00: 0x44 bytes
constexpr mh::CallSite kCalls4D9C00[] = {{0x0, 0x4D9E40}, {0x5, 0x4D9C90}, {0x31, 0x4530D0}, {0x3B, 0x587740}};
// 0x4D9C50: 0x16 bytes
constexpr mh::CallSite kCalls4D9C50[] = {{0x0, 0x4D9C90}, {0x10, 0x4351F0}};
// 0x4D9C70: 0x14 bytes
// 0x4D9C90: 0xB4 bytes
constexpr mh::CallSite kCalls4D9C90[] = {{0x16, 0x5A79A0}, {0x2E, 0x5A77C0}, {0x37, 0x461E50}, {0x3F, 0x494060}, {0x59, 0x4D9D50}};
// 0x4D9D50: 0xEC bytes
constexpr mh::CallSite kCalls4D9D50[] = {{0xC, 0x5A7610}, {0x14, 0x5A7780}, {0x40, 0x494110}, {0x5B, 0x494110}, {0x84, 0x494110}, {0x9F, 0x494110}, {0xDE, 0x461E50}};
// 0x4D9E40: 0x1D bytes
constexpr mh::CallSite kCalls4D9E40[] = {{0x6, 0x4D9E60}, {0x10, 0x4D9E80}};
// 0x4D9E60: 0x19 bytes
// 0x4D9E80: 0xB6 bytes
constexpr mh::CallSite kCalls4D9E80[] = {{0x13, 0x5B93D2}, {0x79, 0x5B93D2}, {0x84, 0x5B93D2}, {0x8F, 0x5B93D2}};
// 0x4E3260: 0x7D bytes
constexpr mh::CallSite kCalls4E3260[] = {{0x5B, 0x4E4000}};
constexpr mh::Imm kImms4E3260[] = {{0x16, 0x4E32E0}, {0x1E, 0x4F9F70}, {0x26, 0x4F7350}};
// 0x4E32E0: 0xC6 bytes
constexpr mh::CallSite kCalls4E32E0[] = {{0x1D, 0x4FC0E0}, {0x68, 0x435180}};
// 0x4E33B0: 0x12 bytes; +0xB note: jmp through .data 0x65bc30, 19 code entries (a data_tables entry)
// 0x4E33D0: 0x36 bytes; +0xB note: call through .data 0x65bc34, 18 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E33D0[] = {{0x30, 0x4E3740}};
// 0x4E3410: 0x24B bytes
constexpr mh::CallSite kCalls4E3410[] = {{0xD2, 0x4FBD10}, {0x172, 0x4FBD10}, {0x204, 0x5A7A70}, {0x241, 0x587900}};
// 0x4E3660: 0x1D bytes
// 0x4E3680: 0x84 bytes
constexpr mh::CallSite kCalls4E3680[] = {{0x16, 0x4E43C0}, {0x44, 0x5B93D2}};
// 0x4E3710: 0x27 bytes
constexpr mh::CallSite kCalls4E3710[] = {{0x21, 0x4351F0}};
// 0x4E3740: 0x8B4 bytes
constexpr mh::CallSite kCalls4E3740[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0xAA, 0x5A7A00}, {0x11B, 0x5A7A00}, {0x13A, 0x5A7A00}, {0x14F, 0x5A7A00}, {0x162, 0x5A7A50}, {0x1C2, 0x5A7A70}, {0x1D9, 0x5A7610}, {0x1E1, 0x5A7780}, {0x1FB, 0x5A7A00}, {0x21B, 0x5A7A50}, {0x24D, 0x5A7A00}, {0x26D, 0x5A7A50}, {0x344, 0x461E50}, {0x350, 0x5A7610}, {0x358, 0x5A7780}, {0x372, 0x5A7A00}, {0x399, 0x5A7A50}, {0x3C0, 0x5A7A00}, {0x3E0, 0x5A7A50}, {0x412, 0x5A7A00}, {0x439, 0x5A7A50}, {0x460, 0x5A7A00}, {0x480, 0x5A7A50}, {0x515, 0x461E50}, {0x521, 0x5A7610}, {0x529, 0x5A7780}, {0x543, 0x5A7A00}, {0x563, 0x5A7A50}, {0x595, 0x5A7A00}, {0x5B5, 0x5A7A50}, {0x68C, 0x461E50}, {0x698, 0x5A7610}, {0x6A0, 0x5A7780}, {0x6BA, 0x5A7A00}, {0x6E1, 0x5A7A50}, {0x708, 0x5A7A00}, {0x728, 0x5A7A50}, {0x75A, 0x5A7A00}, {0x781, 0x5A7A50}, {0x7A8, 0x5A7A00}, {0x7C8, 0x5A7A50}, {0x85D, 0x461E50}, {0x89D, 0x5A77C0}, {0x8A6, 0x461E50}};
// 0x4E4000: 0x12 bytes; +0xB note: jmp through .data 0x65bc44, 14 code entries (a data_tables entry)
// 0x4E4020: 0x84 bytes; +0xB note: call through .data 0x65bc48, 13 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E4020[] = {{0x32, 0x5A77C0}, {0x48, 0x572FA0}, {0x4D, 0x4FBD10}, {0x52, 0x4E4240}, {0x65, 0x5A77C0}, {0x7B, 0x572FA0}};
// 0x4E40B0: 0xF8 bytes
constexpr mh::CallSite kCalls4E40B0[] = {{0x32, 0x5B93D2}, {0x58, 0x5A7A00}, {0x75, 0x5A7A50}, {0xA3, 0x5B93D2}, {0xB5, 0x5B93D2}, {0xC7, 0x5B93D2}};
// 0x4E41B0: 0x3D bytes
// 0x4E41F0: 0x47 bytes
constexpr mh::CallSite kCalls4E41F0[] = {{0x41, 0x4F6290}};
// 0x4E4240: 0x17B bytes
constexpr mh::CallSite kCalls4E4240[] = {{0x9, 0x5A7A00}, {0x26, 0x5A7A50}, {0x78, 0x5A75F0}, {0x7F, 0x5A7780}, {0xD3, 0x5A7A00}, {0xFD, 0x5A7A50}, {0x15E, 0x572FA0}};
// 0x4E43C0: 0x57 bytes
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"BoneDance_Task", 0x4AE7C0, 0x2E, nullptr, 0, kImms4AE7C0, MH_N(kImms4AE7C0), nullptr, 0, reinterpret_cast<const void*>(&::BoneDance_Task)},
    {"BoneDance_Start", 0x4AE7F0, 0x59, kCalls4AE7F0, MH_N(kCalls4AE7F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDance_Start)},
    {"BoneDance_Run", 0x4AE850, 0x36, nullptr, 0, kImms4AE850, MH_N(kImms4AE850), nullptr, 0, reinterpret_cast<const void*>(&::BoneDance_Run)},
    {"BoneDance_Cast", 0x4AE890, 0x83, kCalls4AE890, MH_N(kCalls4AE890), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDance_Cast)},
    {"BoneDance_Hold", 0x4AE920, 0x33, kCalls4AE920, MH_N(kCalls4AE920), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDance_Hold)},
    {"BoneDance_Spawn", 0x4AE960, 0x82, kCalls4AE960, MH_N(kCalls4AE960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDance_Spawn)},
    {"BoneDance_WaitBones", 0x4AE9F0, 0x22, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDance_WaitBones)},
    {"BoneDance_End", 0x4AEA20, 0x1A, kCalls4AEA20, MH_N(kCalls4AEA20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDance_End)},
    {"BoneDanceChild_Task", 0x4AEA40, 0x2E, nullptr, 0, kImms4AEA40, MH_N(kImms4AEA40), nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceChild_Task)},
    {"BoneDanceBone_Run", 0x4AEA70, 0x52, kCalls4AEA70, MH_N(kCalls4AEA70), kImms4AEA70, MH_N(kImms4AEA70), nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceBone_Run)},
    {"BoneDanceBone_Start", 0x4AEAD0, 0x144, kCalls4AEAD0, MH_N(kCalls4AEAD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceBone_Start)},
    {"BoneDanceBone_Fall", 0x4AEC20, 0xC8, kCalls4AEC20, MH_N(kCalls4AEC20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceBone_Fall)},
    {"BoneDanceBone_End", 0x4AECF0, 0xD, kCalls4AECF0, MH_N(kCalls4AECF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceBone_End)},
    {"BoneDanceShake_Run", 0x4AED00, 0x2E, nullptr, 0, kImms4AED00, MH_N(kImms4AED00), nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceShake_Run)},
    {"BoneDanceShake_Start", 0x4AED30, 0x3E, kCalls4AED30, MH_N(kCalls4AED30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceShake_Start)},
    {"BoneDanceShake_Step", 0x4AED70, 0xDB, kCalls4AED70, MH_N(kCalls4AED70), kImms4AED70, MH_N(kImms4AED70), nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceShake_Step)},
    {"BoneDanceShake_Down", 0x4AEE50, 0x16, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceShake_Down)},
    {"BoneDanceShake_Up", 0x4AEE70, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceShake_Up)},
    {"BoneDanceFollow_Run", 0x4AEEA0, 0x5E, kCalls4AEEA0, MH_N(kCalls4AEEA0), kImms4AEEA0, MH_N(kImms4AEEA0), nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceFollow_Run)},
    {"BoneDanceFollow_Start", 0x4AEF00, 0xD7, kCalls4AEF00, MH_N(kCalls4AEF00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceFollow_Start)},
    {"BoneDanceFollow_Step", 0x4AEFE0, 0x56, kCalls4AEFE0, MH_N(kCalls4AEFE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BoneDanceFollow_Step)},
    {"RottenBreath_Task", 0x4BF8D0, 0x85, kCalls4BF8D0, MH_N(kCalls4BF8D0), kImms4BF8D0, MH_N(kImms4BF8D0), nullptr, 0, reinterpret_cast<const void*>(&::RottenBreath_Task)},
    {"RottenBreath_Start", 0x4BF960, 0xB5, kCalls4BF960, MH_N(kCalls4BF960), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreath_Start)},
    {"RottenBreath_Emit", 0x4BFA20, 0x64, kCalls4BFA20, MH_N(kCalls4BFA20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreath_Emit)},
    {"RottenBreath_End", 0x4BFA90, 0x47, kCalls4BFA90, MH_N(kCalls4BFA90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreath_End)},
    {"RottenBreathChild_Task", 0x4BFAE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathChild_Task)},
    {"RottenBreathCloud_Run", 0x4BFB00, 0x2E, nullptr, 0, kImms4BFB00, MH_N(kImms4BFB00), nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathCloud_Run)},
    {"RottenBreathCloud_Start", 0x4BFB30, 0x9D, kCalls4BFB30, MH_N(kCalls4BFB30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathCloud_Start)},
    {"RottenBreathCloud_Emit", 0x4BFBD0, 0x90, kCalls4BFBD0, MH_N(kCalls4BFBD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathCloud_Emit)},
    {"RottenBreathAim_Run", 0x4BFC60, 0x13, kCalls4BFC60, MH_N(kCalls4BFC60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathAim_Run)},
    {"RottenBreathMote_Run", 0x4BFC80, 0x36, nullptr, 0, kImms4BFC80, MH_N(kImms4BFC80), nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathMote_Run)},
    {"RottenBreathMote_Start", 0x4BFCC0, 0xB0, kCalls4BFCC0, MH_N(kCalls4BFCC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathMote_Start)},
    {"RottenBreathMote_Swirl", 0x4BFD70, 0xC1, kCalls4BFD70, MH_N(kCalls4BFD70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathMote_Swirl)},
    {"RottenBreathMote_Fly", 0x4BFE40, 0x4B, kCalls4BFE40, MH_N(kCalls4BFE40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathMote_Fly)},
    {"RottenBreathMote_End", 0x4BFE90, 0x46, kCalls4BFE90, MH_N(kCalls4BFE90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathMote_End)},
    {"RottenBreathMote_Draw", 0x4BFEE0, 0x2AA, kCalls4BFEE0, MH_N(kCalls4BFEE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathMote_Draw)},
    {"RottenBreathMote_Alloc", 0x4C0190, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::RottenBreathMote_Alloc), 0xFF},
    {"UtmostAttack_Task", 0x4D9AE0, 0x36, nullptr, 0, kImms4D9AE0, MH_N(kImms4D9AE0), nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_Task)},
    {"UtmostAttack_Start", 0x4D9B20, 0x83, kCalls4D9B20, MH_N(kCalls4D9B20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_Start)},
    {"UtmostAttack_WaitCaster", 0x4D9BB0, 0x48, kCalls4D9BB0, MH_N(kCalls4D9BB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_WaitCaster)},
    {"UtmostAttack_Stream", 0x4D9C00, 0x44, kCalls4D9C00, MH_N(kCalls4D9C00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_Stream)},
    {"UtmostAttack_End", 0x4D9C50, 0x16, kCalls4D9C50, MH_N(kCalls4D9C50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_End)},
    {"UtmostAttack_ClearStreaks", 0x4D9C70, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_ClearStreaks)},
    {"UtmostAttack_DrawStreaks", 0x4D9C90, 0xB4, kCalls4D9C90, MH_N(kCalls4D9C90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_DrawStreaks), 0xFF},
    {"UtmostAttack_DrawStreak", 0x4D9D50, 0xEC, kCalls4D9D50, MH_N(kCalls4D9D50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_DrawStreak)},
    {"UtmostAttack_SpawnStreaks", 0x4D9E40, 0x1D, kCalls4D9E40, MH_N(kCalls4D9E40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_SpawnStreaks)},
    {"UtmostAttack_FreeStreak", 0x4D9E60, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_FreeStreak), 0xFFFFFFFFu},
    {"UtmostAttack_InitStreak", 0x4D9E80, 0xB6, kCalls4D9E80, MH_N(kCalls4D9E80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::UtmostAttack_InitStreak)},
    {"Holocaust_Task", 0x4E3260, 0x7D, kCalls4E3260, MH_N(kCalls4E3260), kImms4E3260, MH_N(kImms4E3260), nullptr, 0, reinterpret_cast<const void*>(&::Holocaust_Task)},
    {"Holocaust_Start", 0x4E32E0, 0xC6, kCalls4E32E0, MH_N(kCalls4E32E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Holocaust_Start)},
    {"HolocaustBeam_Task", 0x4E33B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustBeam_Task)},
    {"HolocaustBeam_Run", 0x4E33D0, 0x36, kCalls4E33D0, MH_N(kCalls4E33D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustBeam_Run)},
    {"HolocaustBeam_Aim", 0x4E3410, 0x24B, kCalls4E3410, MH_N(kCalls4E3410), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustBeam_Aim)},
    {"HolocaustBeam_Grow", 0x4E3660, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustBeam_Grow)},
    {"HolocaustBeam_Emit", 0x4E3680, 0x84, kCalls4E3680, MH_N(kCalls4E3680), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustBeam_Emit)},
    {"HolocaustBeam_Fade", 0x4E3710, 0x27, kCalls4E3710, MH_N(kCalls4E3710), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustBeam_Fade)},
    {"HolocaustBeam_Draw", 0x4E3740, 0x8B4, kCalls4E3740, MH_N(kCalls4E3740), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustBeam_Draw)},
    {"HolocaustSpark_Task", 0x4E4000, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustSpark_Task)},
    {"HolocaustSpark_Run", 0x4E4020, 0x84, kCalls4E4020, MH_N(kCalls4E4020), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustSpark_Run)},
    {"HolocaustSpark_Start", 0x4E40B0, 0xF8, kCalls4E40B0, MH_N(kCalls4E40B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustSpark_Start)},
    {"HolocaustSpark_Rise", 0x4E41B0, 0x3D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustSpark_Rise)},
    {"HolocaustSpark_Fade", 0x4E41F0, 0x47, kCalls4E41F0, MH_N(kCalls4E41F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustSpark_Fade)},
    {"HolocaustSpark_Draw", 0x4E4240, 0x17B, kCalls4E4240, MH_N(kCalls4E4240), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustSpark_Draw)},
    {"HolocaustSpark_Alloc", 0x4E43C0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HolocaustSpark_Alloc), 0xFF},
};
#undef MH_N

enum : unsigned {
    kBoneDance_Task, kBoneDance_Start, kBoneDance_Run, kBoneDance_Cast, kBoneDance_Hold, kBoneDance_Spawn,
    kBoneDance_WaitBones, kBoneDance_End, kBoneDanceChild_Task, kBoneDanceBone_Run, kBoneDanceBone_Start,
    kBoneDanceBone_Fall, kBoneDanceBone_End, kBoneDanceShake_Run, kBoneDanceShake_Start, kBoneDanceShake_Step,
    kBoneDanceShake_Down, kBoneDanceShake_Up, kBoneDanceFollow_Run, kBoneDanceFollow_Start, kBoneDanceFollow_Step,
    kRottenBreath_Task, kRottenBreath_Start, kRottenBreath_Emit, kRottenBreath_End, kRottenBreathChild_Task,
    kRottenBreathCloud_Run, kRottenBreathCloud_Start, kRottenBreathCloud_Emit, kRottenBreathAim_Run,
    kRottenBreathMote_Run, kRottenBreathMote_Start, kRottenBreathMote_Swirl, kRottenBreathMote_Fly,
    kRottenBreathMote_End, kRottenBreathMote_Draw, kRottenBreathMote_Alloc,
    kUtmostAttack_Task, kUtmostAttack_Start, kUtmostAttack_WaitCaster, kUtmostAttack_Stream, kUtmostAttack_End,
    kUtmostAttack_ClearStreaks, kUtmostAttack_DrawStreaks, kUtmostAttack_DrawStreak, kUtmostAttack_SpawnStreaks,
    kUtmostAttack_FreeStreak, kUtmostAttack_InitStreak,
    kHolocaust_Task, kHolocaust_Start, kHolocaustBeam_Task, kHolocaustBeam_Run, kHolocaustBeam_Aim,
    kHolocaustBeam_Grow, kHolocaustBeam_Emit, kHolocaustBeam_Fade, kHolocaustBeam_Draw, kHolocaustSpark_Task,
    kHolocaustSpark_Run, kHolocaustSpark_Start, kHolocaustSpark_Rise, kHolocaustSpark_Fade, kHolocaustSpark_Draw,
    kHolocaustSpark_Alloc, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The group's cells (magic_c2.cpp).
constexpr std::uint32_t kPacketNext = 0x7E0670;
constexpr std::uint32_t kS = 0x903850, kV = 0x9037A0, kFrameSet = 0x9039D8;
constexpr std::uint32_t kMotes = 0x68E1B8, kStreaks = 0x698EC0, kSparks = 0x6A2D38;
constexpr std::uint32_t kRecord = 0x84, kStreak = 0x3C;

// The packets the draws fill: Gfx_PacketNext aimed at one of four places in
// this buffer (and moved between them by the disturbance). A primitive is at
// most 0x54 bytes (the GT4); a commit logs 0x58.
alignas(16) unsigned char g_packets[0x100];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 3) * 0x20; }

// --- the callees' effects -------------------------------------------------------

std::uint32_t Committed(const std::uint32_t*, std::uint32_t answer) {
    mh::NoteBytes(Gfx_PacketNext, 0x58);
    return answer;
}
std::uint32_t NoteCurrent(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
std::uint32_t NoteDrawn(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
    return answer;
}

#define C2_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define C2_RAW(address) #address, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;

const mh::Callee kCallees[] = {
    // the sprite and the task, noting the current task (listed before the
    // standard set, so these listings stand)
    {C2_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteCurrent},
    {C2_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteCurrent},
    {C2_OURS(Sprite_ScriptTick), 0, {}, kG, 0, 0, {}, &NoteCurrent},
    {C2_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteDrawn},
    {C2_OURS(BattleActor_UpdateScreenXY), 0, {}, kG, 0, 0, {}, &NoteCurrent},
    {C2_OURS(BattleTask_FreeCurrent), 0, {}, kG, 0, 0, {}, &NoteCurrent},
    {C2_OURS(MagicFx_StepToward), 2, {kAll, kU16}, kG, 0, 0, {}, &NoteCurrent},
    {C2_OURS(MagicFx_CenterOnSide), 0, {}, kG, 0, 0, {}, &NoteCurrent},
    {C2_OURS(Gfx_ClutStripCopyRow), 1, {kU8}, kG, 0, 0},
    {C2_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0},
    // the draws'
    {C2_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, kG, 0, 0, {}, &Committed},
    {C2_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kU8, kU8}, kG, 0, 0, {}, &Committed},
    {C2_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {C2_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {C2_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {C2_OURS(Gpu_SetPolyGT4), 1, {kAll}, kG, 0, 0},
    {C2_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {C2_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {C2_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {C2_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {C2_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {C2_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0},
    // Capcom's, unnamed: the map camera, the point projection (the vector a
    // local of the caller's: its twelve bytes), MAGIC219's task clear
    {C2_RAW(0x494060), 0, {}, kG, 0, 0},
    {C2_RAW(0x494110), 2, {0, kAll}, kG, 0, 0, {12}},
    {C2_RAW(0x4F6290), 0, {}, kG, 0, 0, {}, &NoteCurrent},
    // this group's own, called by address
    {C2_RAW(0x4BFC80), 0, {}, mh::Answer::kPhase, 0, 0},
    {C2_RAW(0x4BFEE0), 0, {}, mh::Answer::kPhase, 0, 0},
    {C2_RAW(0x4C0190), 0, {}, mh::Answer::kByte, 0xFF, 0x2F},   // 0xFF, or a mote 0..47
    {C2_RAW(0x4D9C70), 0, {}, mh::Answer::kPhase, 0, 0},
    {C2_RAW(0x4D9C90), 0, {}, mh::Answer::kFlag, 0, 0},
    {C2_RAW(0x4D9D50), 1, {kAll}, kG, 0, 0},
    {C2_RAW(0x4D9E40), 0, {}, mh::Answer::kPhase, 0, 0},
    {C2_RAW(0x4D9E60), 0, {}, mh::Answer::kBool, 0, 0},         // null, or a streak (1 here: only handed on)
    {C2_RAW(0x4D9E80), 1, {kAll}, kG, 0, 0},
    {C2_RAW(0x4E4000), 0, {}, mh::Answer::kPhase, 0, 0},
    {C2_RAW(0x4E3740), 0, {}, mh::Answer::kPhase, 0, 0},
    {C2_RAW(0x4E4240), 0, {}, mh::Answer::kPhase, 0, 0},
    {C2_RAW(0x4E43C0), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},   // 0xFF, or a spark 0..63
};

// The .data handler tables the dispatchers read in place: RottenBreathChild_Types,
// HolocaustBeam_Types with HolocaustBeam_Phases (the second begins one cell in),
// HolocaustSpark_Types with HolocaustSpark_Phases.
const mh::DataTable kTables[] = {{0x65B358, 2}, {0x65BC30, 5}, {0x65BC44, 4}};

const mh::Region kRegions[] = {
    {kPacketNext, 4},
    {Key(g_packets), sizeof g_packets},
    {kS, 0x10},
    {kV, 4},
    {kFrameSet, 4},
    {0x903802, 2},                          // Camera_ShiftY
    {0x905E69, 1},                          // MapView_Redraw
    {0x80E980, 0x20},                       // Gfx_ClutStrip row 26's source, the first sixteen
    {0x812980, 0x20},                       // Gfx_ClutStrip row 26, the first sixteen
    {kMotes, 48 * kRecord},
    {kMotes + 0xFF * kRecord, kRecord},     // the record an unchecked 0xFF writes
    {kStreaks, 128 * kStreak},
    {kSparks, 64 * kRecord},
    {kSparks + 0xFF * kRecord, kRecord},
};

// --- the seed -------------------------------------------------------------------

unsigned g_k;   // the function being fuzzed, for settle

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return mh::Pointer(at::kOwner); }
unsigned char* ValidOwner(std::uint32_t v) { return v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v); }
unsigned char* Mote(unsigned i) { return mh::Mem(kMotes + i * kRecord); }
unsigned char* Spark(unsigned i) { return mh::Mem(kSparks + i * kRecord); }
unsigned char* Streak(unsigned i) { return mh::Mem(kStreaks + i * kStreak); }
unsigned char B(std::uint32_t v) { return static_cast<unsigned char>(v); }

// HolocaustBeam_Draw loops to +0xA of the current task, read each time: the
// four task slots' kept below 0x14 while it runs, whatever a disturbance wrote.
void Settle() {
    if (g_k != kHolocaustBeam_Draw) return;
    for (unsigned k = 0; k < 4; ++k) mh::TaskAt(k)[0xA] = B(mh::TaskAt(k)[0xA] % 0x14);
}

// A pool's records with bit 0 set in the first `m`, the rest as the fill left them.
void Occupy(std::uint32_t pool, unsigned count, unsigned m) {
    for (unsigned i = 0; i < m && i < count; ++i) mh::Mem(pool + i * kRecord)[0] |= 1;
}

void Seed(unsigned k) {
    g_k = k;
    unsigned char* const sc = Sc();
    mh::SetPointer(kPacketNext, PacketAt(mh::Next()));
    // The walks make each live record the owner a recorder writes through:
    // every record's +0x80 a real slot or record.
    for (unsigned i = 0; i < 48; ++i) mh::SetPointer(kMotes + i * kRecord + 0x80, ValidOwner(mh::Next()));
    for (unsigned i = 0; i < 64; ++i) mh::SetPointer(kSparks + i * kRecord + 0x80, ValidOwner(mh::Next()));
    // Half the streaks dead, some at their last frame.
    for (unsigned i = 0; i < 128; ++i) {
        if (mh::Half()) Streak(i)[0] = 0;
        if (mh::Next() % 4 == 0) Streak(i)[2] = 1;
    }
    if (mh::Half()) mh::Mem(at::kTarget)[0] = B(mh::Mem(at::kTarget)[0] | 0x40);
    switch (k) {
    // the stack and .data dispatchers: an index inside the table
    case kBoneDance_Task: case kBoneDanceChild_Task: case kHolocaust_Task: sc[1] = B(mh::Next() % 3); break;
    case kRottenBreath_Task: case kUtmostAttack_Task: sc[1] = B(mh::Next() % 4); break;
    case kRottenBreathChild_Task: sc[1] = B(mh::Next() % 2); break;
    case kHolocaustBeam_Task: sc[1] = B(mh::Next() % 5); break;
    case kHolocaustSpark_Task: sc[1] = B(mh::Next() % 4); break;
    case kBoneDance_Run: case kRottenBreathMote_Run: case kHolocaustBeam_Run: sc[2] = B(mh::Next() % 4); break;
    case kBoneDanceBone_Run: case kBoneDanceShake_Run: case kBoneDanceFollow_Run: case kRottenBreathCloud_Run:
    case kHolocaustSpark_Run:
        sc[2] = B(mh::Next() % 3);
        break;
    case kBoneDanceShake_Step:
        sc[3] = B(mh::Next() % 2);
        sc[0xA] = B(MH_PICK(0xFF, 1, 2, 0, 0x55));
        if (mh::Half()) sc[9] = 2;
        break;
    // the count-downs and thresholds, at and either side of their ends
    case kBoneDance_Spawn: sc[9] = B(MH_PICK(0x1F, 0x20, 0x21, 0, 0x90)); break;
    case kBoneDance_WaitBones: if (mh::Half()) sc[0xA] = sc[9]; break;
    case kBoneDanceBone_Fall: if (mh::Half()) sc[9] = 1; break;
    case kBoneDanceShake_Down: case kBoneDanceShake_Up:
        SetLong(sc + 0x10, static_cast<std::int32_t>(MH_PICK(0xFFFFFFFCu, 0xFFFFFFFDu, 0xFFFFFFFEu, 0xFFFFFFFFu, 0, 1, 0x80000000u)));
        break;
    case kBoneDanceFollow_Step: case kBoneDanceShake_Start: break;
    case kRottenBreath_End: if (mh::Half()) sc[0xB] = 0; break;
    case kRottenBreathCloud_Emit:
        sc[9] = B(MH_PICK(0x63, 0x64, 0x65, 0x80, 0x81, 0x7F, 0xFF, 3));
        if (mh::Half()) Frame_Counter = (Frame_Counter & ~3u) | 3u;
        if (mh::Often()) mh::SetRandHint(0);
        break;
    case kRottenBreathMote_Start:
        // the aim slot the cloud's +0xA names is read directly: one of the 48
        Owner()[0xA] = B(Owner()[0xA] % at::kTaskCount);
        break;
    case kRottenBreathMote_Swirl: if (mh::Half()) sc[9] = 0xF; break;
    case kRottenBreathMote_Fly: if (mh::Half()) sc[0xA] = 1; break;
    case kRottenBreathMote_Draw: sc[9] = B(MH_PICK(7, 8, 0, 0xFF, 3)); break;
    case kRottenBreathMote_Alloc:
        if (mh::Half()) Occupy(kMotes, 48, mh::Half() ? 48 : mh::Next() % 48);
        break;
    case kUtmostAttack_Stream: if (mh::Half()) sc[9] = 1; break;
    case kUtmostAttack_FreeStreak:
        if (mh::Half()) {
            for (unsigned i = 0; i < 128; ++i) Streak(i)[0] = B(Streak(i)[0] | 1);
            if (mh::Half()) Streak(mh::Next() % 128)[0] = 0;
        }
        break;
    case kHolocaustBeam_Aim:
        if (mh::Half()) sc[9] = 1;
        // the member +4 names is read directly: a record the regions hold
        for (unsigned t = 0; t < 4; ++t) mh::TaskAt(t)[4] = B(mh::TaskAt(t)[4] % 5);
        break;
    case kHolocaustBeam_Grow: sc[0xA] = B(MH_PICK(0xE, 0xF, 0x10, 0)); break;
    case kHolocaustBeam_Emit:
        sc[9] = B(MH_PICK(0x10, 0x11, 1, 0x12, 0xFF));
        if (mh::Half()) Frame_Counter &= ~3u;
        break;
    case kHolocaustBeam_Fade: if (mh::Half()) sc[0x5D] = 1; break;
    case kHolocaustBeam_Draw:
        for (unsigned t = 0; t < 4; ++t) mh::TaskAt(t)[0xA] = B(mh::Next() % 0x14);
        if (mh::Next() % 4 == 0) sc[0xA] = B(MH_PICK(0, 1));
        break;
    case kHolocaustSpark_Start: case kHolocaustSpark_Rise: if (mh::Half()) sc[9] = 1; break;
    case kHolocaustSpark_Fade: if (mh::Half()) sc[0xA] = 1; break;
    case kHolocaustSpark_Alloc:
        if (mh::Half()) Occupy(kSparks, 64, mh::Half() ? 64 : mh::Next() % 64);
        break;
    default: break;
    }
    if (k == kHolocaustBeam_Run || k == kHolocaustSpark_Run) {
        if (mh::Next() % 4 == 0) sc[0] = 0;
    }
}

// The two streak functions take a streak: one of the 128.
void Args(unsigned k, std::uint32_t* a) {
    if (k == kUtmostAttack_DrawStreak || k == kUtmostAttack_InitStreak) a[0] = kStreaks + (mh::Next() % 128) * kStreak;
}

// After a call, two in three (the harness's case 14): the packet pointer, a
// scratch word, a vertex word, a byte of a streak, of a mote or a spark (not
// its owner, which the walk hands on), the frame-offset table, bit 0 of a
// party record.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    const auto b = static_cast<unsigned char>(h >> 24);
    switch ((h >> 8) % 7) {
    case 0: mh::SetPointer(kPacketNext, PacketAt(v)); break;
    case 1: SetWord(mh::Mem(kS + 2 * (v % 8)), h >> 16); break;
    case 2: SetWord(mh::Mem(kV + 2 * (v % 2)), h >> 16); break;
    case 3: Streak(v % 128)[(h >> 20) % kStreak] = b; break;
    case 4: (v & 1 ? Mote(v % 48) : Spark(v % 64))[(h >> 20) % 0x80] = b; break;
    case 5: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    default: {
        unsigned char* const member = mh::PartyOf(static_cast<unsigned char>(v % 3));
        member[0] = B(member[0] ^ 1);
        break;
    }
    }
}

}  // namespace

void SelfTest() {
    mh::Group group = {
        "magic_c2", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000,
    };
    group.settle = &Settle;
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_c2
