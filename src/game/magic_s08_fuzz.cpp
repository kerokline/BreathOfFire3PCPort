// BOF3X_SHADOW=magic_s08: group S08's four overlays (MAGIC041, 042, 043, 044)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s08.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC041 / 042 / 043 / 044
// --clones (2026-09-26; capstone, every jump internal, no jump table, no
// REFUSED line), names given, and the tool's ".data" entry counts replaced by
// where the next table starts. Beyond the standard set this group lists the
// draw callees (the GTE and libgpu entry points, Math_Sin / _Cos / _Ratan2,
// Gfx_CommitPrim, MapView_LinkPrimAt), the tint calls, the engine's 0x446770,
// and the functions of its own that its functions call directly. Everything
// the harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes and move Gfx_PacketNext on through a packet buffer of the fuzz's
//     own, as the real ones do (every primitive is built at the same pointer);
//   - the GTE callees of WardMote_PushMatrix log what their pointers point at
//     (`deref`) and write a result where the real ones write (group S22's);
//   - 0x446770 logs the task's direction and pair and writes a new pair;
//     BattleActor_UpdateScreenXY logs the sprite and writes its screen point
//     (+0x2E / +0x30), which the callers read back;
//   - MagicFx_NearSprite answers a whole eax of 0 or 1 (EvilEyeBeam_Seek tests
//     all 32 bits) from its own stream, and a quarter of the time moves
//     Sprite_Current itself: the harness's kFlag answers 0 exactly when its
//     own disturbance did nothing (docs/takeover-queue-round9.md section 9),
//     which would leave the "not near" path's re-reads unexercised;
//   - Math_Ratan2 answers, for EvilEyeBeam_Seek, a heading 0x5FF..0x601 or
//     0x9FF..0xA01 from the old one half the time (the snap's two bounds);
//   - BattleTask_Create answers 0xFF (none free) sometimes, but only for
//     EvilEyeBeam_Trail, the one caller that tests it;
//   - `settle` keeps a beam's +4 at 0 or 1 while MAGIC044's functions run:
//     the trail index (+4 x 32 + point) is unchecked in the original and a
//     disturbed +4 would write far outside any compared region.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s08.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s08 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

#define S08_P(name) reinterpret_cast<const void*>(&::name)
// tools/magic_rows.py --unit MAGIC041 / 042 / 043 / 044 --clones, 2026-09-26,
// names given.
// 0x4A6440: 0x46 bytes  Berserk_Task
constexpr mh::Imm kImms4A6440[] = {{0xF, 0x4A6490}, {0x17, 0x4A6580}, {0x22, 0x4A65D0}, {0x2A, 0x4A6640}, {0x32, 0x4A6650}, {0x3A, 0x4F7350}};
// 0x4A6490: 0xF0 bytes  Berserk_Start
constexpr mh::CallSite kCalls4A6490[] = {{0x60, 0x435180}, {0xAD, 0x435180}, {0xE5, 0x587900}};
// 0x4A6580: 0x4E bytes  Berserk_Tint
constexpr mh::CallSite kCalls4A6580[] = {{0x20, 0x454DC0}, {0x34, 0x454CC0}};
// 0x4A65D0: 0x65 bytes  Berserk_Brighten
// 0x4A6640: 0xF bytes  Berserk_WaitChildren
// 0x4A6650: 0x86 bytes  Berserk_Fade
constexpr mh::CallSite kCalls4A6650[] = {{0x69, 0x454DC0}, {0x75, 0x4FBDB0}};
// 0x4A66E0: 0x12 bytes; +0xB note: jmp through .data 0x65a82c, 2 entries (BerserkChild_Kinds)  BerserkChild_Task
// 0x4A6700: 0x4E bytes  BerserkRing_Run
constexpr mh::CallSite kCalls4A6700[] = {{0x3B, 0x4B7D40}, {0x40, 0x4A68D0}, {0x45, 0x5A7BC0}};
constexpr mh::Imm kImms4A6700[] = {{0xF, 0x4A6750}, {0x17, 0x4A67C0}, {0x22, 0x4A67E0}};
// 0x4A6750: 0x61 bytes  BerserkRing_Start
// 0x4A67C0: 0x1D bytes  BerserkRing_Grow
// 0x4A67E0: 0x35 bytes  BerserkRing_Fade
constexpr mh::CallSite kCalls4A67E0[] = {{0x2F, 0x4351F0}};
// 0x4A6820: 0x56 bytes  BerserkGlow_Run
constexpr mh::CallSite kCalls4A6820[] = {{0x43, 0x4B7D40}, {0x48, 0x4A6C10}, {0x4D, 0x5A7BC0}};
constexpr mh::Imm kImms4A6820[] = {{0xF, 0x4A6880}, {0x17, 0x4E5950}, {0x22, 0x4C32D0}, {0x2A, 0x4B1740}};
// 0x4A6880: 0x45 bytes  BerserkGlow_Start
// 0x4A68D0: 0x334 bytes  BerserkRing_Draw
constexpr mh::CallSite kCalls4A68D0[] = {{0x7, 0x5B93D2}, {0x37, 0x5A7A00}, {0x64, 0x5A7A00}, {0x8D, 0x5A7A50}, {0xA6, 0x5A7A00}, {0xCE, 0x5A7A50}, {0xE7, 0x5A7A00}, {0x11B, 0x5B93D2}, {0x14E, 0x5A7A00}, {0x17B, 0x5A7A00}, {0x1CF, 0x5A7A50}, {0x1E8, 0x5A7A00}, {0x22A, 0x5A7A50}, {0x243, 0x5A7A00}, {0x284, 0x5A77C0}, {0x28F, 0x572FA0}, {0x29B, 0x5A7610}, {0x2A6, 0x5A7780}, {0x309, 0x5A85F0}, {0x30F, 0x5A9350}, {0x31A, 0x572FA0}};
// 0x4A6C10: 0x1DA bytes  BerserkGlow_Draw
constexpr mh::CallSite kCalls4A6C10[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x46, 0x5A7A00}, {0x5F, 0x5A7A50}, {0x9B, 0x5A75F0}, {0xA2, 0x5A7780}, {0xD0, 0x5A7A00}, {0xE9, 0x5A7A50}, {0x126, 0x5A84A0}, {0x12C, 0x5A9310}, {0x19C, 0x461E50}, {0x1C1, 0x5A77C0}, {0x1CA, 0x461E50}};
// 0x4A6DF0: 0x26 bytes  Counter_Task
constexpr mh::Imm kImms4A6DF0[] = {{0xF, 0x4A6E20}, {0x17, 0x4F7350}};
// 0x4A6E20: 0x9B bytes  Counter_Start
constexpr mh::CallSite kCalls4A6E20[] = {{0x5A, 0x435180}, {0x92, 0x587900}};
// 0x4A6EC0: 0x12 bytes; +0xB note: jmp through .data 0x65a834, 1 entry (CounterChild_Kinds)  CounterChild_Task
// 0x4A6EE0: 0xAF bytes  CounterMark_Run
constexpr mh::CallSite kCalls4A6EE0[] = {{0x4B, 0x4A7690}, {0x5E, 0x4A7220}, {0x75, 0x4A7530}, {0x8C, 0x4A7530}, {0xA3, 0x4A7530}};
constexpr mh::Imm kImms4A6EE0[] = {{0xF, 0x4A6F90}, {0x17, 0x4A7000}, {0x22, 0x4A7030}, {0x2A, 0x4A7060}, {0x32, 0x4A70A0}};
// 0x4A6F90: 0x6F bytes  CounterMark_Start
constexpr mh::CallSite kCalls4A6F90[] = {{0x44, 0x4A70F0}};
// 0x4A7000: 0x29 bytes  CounterMark_Open
// 0x4A7030: 0x2A bytes  CounterMark_Grow
// 0x4A7060: 0x38 bytes  CounterMark_Swell
// 0x4A70A0: 0x48 bytes  CounterMark_Fade
constexpr mh::CallSite kCalls4A70A0[] = {{0x42, 0x4351F0}};
// 0x4A70F0: 0x12E bytes  CounterMark_Place
constexpr mh::CallSite kCalls4A70F0[] = {{0x1, 0x4FBD10}};
// 0x4A7220: 0x306 bytes  CounterMark_DrawSpokes
constexpr mh::CallSite kCalls4A7220[] = {{0x31, 0x5A77C0}, {0x3A, 0x461E50}, {0x86, 0x5A76B0}, {0x8D, 0x5A7780}, {0xD3, 0x5A7A50}, {0xFA, 0x5A7A00}, {0x132, 0x461E50}, {0x14A, 0x5A76B0}, {0x151, 0x5A7780}, {0x18A, 0x5A7A50}, {0x1B8, 0x5A7A00}, {0x1F9, 0x461E50}, {0x205, 0x5A76B0}, {0x20C, 0x5A7780}, {0x248, 0x5A7A50}, {0x276, 0x5A7A00}, {0x2B7, 0x461E50}, {0x2F0, 0x5A77C0}, {0x2F9, 0x461E50}};
// 0x4A7530: 0x152 bytes  CounterMark_DrawArc
constexpr mh::CallSite kCalls4A7530[] = {{0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x6E, 0x5A76B0}, {0x76, 0x5A7780}, {0xB6, 0x5A7A50}, {0xDD, 0x5A7A00}, {0x118, 0x461E50}, {0x13E, 0x5A77C0}, {0x147, 0x461E50}};
// 0x4A7690: 0x193 bytes  CounterMark_DrawDisc
constexpr mh::CallSite kCalls4A7690[] = {{0x16, 0x5A77C0}, {0x1F, 0x461E50}, {0x27, 0x5B93D2}, {0x67, 0x5A75F0}, {0x6E, 0x5A7780}, {0x98, 0x5A7A00}, {0xBF, 0x5A7A50}, {0xEC, 0x5A7A00}, {0x113, 0x5A7A50}, {0x15B, 0x461E50}, {0x17D, 0x5A77C0}, {0x186, 0x461E50}};
// 0x4A7830: 0x8D bytes  Ward_Task
constexpr mh::CallSite kCalls4A7830[] = {{0x6B, 0x4A7BE0}};
constexpr mh::Imm kImms4A7830[] = {{0x16, 0x4A78C0}, {0x1E, 0x4A79D0}, {0x26, 0x4A7A30}, {0x2E, 0x4A7AB0}, {0x36, 0x4F7350}};
// 0x4A78C0: 0x109 bytes  Ward_Start
constexpr mh::CallSite kCalls4A78C0[] = {{0x76, 0x4A81F0}, {0x100, 0x587900}};
// 0x4A79D0: 0x54 bytes  Ward_Tint
constexpr mh::CallSite kCalls4A79D0[] = {{0x21, 0x454DC0}, {0x2F, 0x454CC0}};
// 0x4A7A30: 0x72 bytes  Ward_Brighten
// 0x4A7AB0: 0x127 bytes  Ward_Fade
constexpr mh::CallSite kCalls4A7AB0[] = {{0x5B, 0x454D60}, {0x67, 0x4FBDB0}, {0x86, 0x4FB6F0}, {0x96, 0x435180}, {0xCB, 0x435180}};
// 0x4A7BE0: 0x12 bytes; +0xB note: jmp through .data 0x65a904, 1 entry (WardMote_TaskTable)  WardMote_Task
// 0x4A7C00: 0x33 bytes; +0xB note: call through .data 0x65a908, 4 entries (WardMote_Steps)  WardMote_Run
constexpr mh::CallSite kCalls4A7C00[] = {{0x23, 0x4A7E60}, {0x28, 0x4A7F10}, {0x2D, 0x5A7BC0}};
// 0x4A7C40: 0x10D bytes  WardMote_Launch
constexpr mh::CallSite kCalls4A7C40[] = {{0x47, 0x446770}, {0xA6, 0x446770}, {0xE0, 0x5A7A70}};
// 0x4A7D50: 0x4A bytes  WardMote_Open
// 0x4A7DA0: 0x3C bytes  WardMote_Fly
// 0x4A7DE0: 0x7F bytes  WardMote_End
// 0x4A7E60: 0xAE bytes  WardMote_PushMatrix
constexpr mh::CallSite kCalls4A7E60[] = {{0x3, 0x5A7B90}, {0x6E, 0x5A8200}, {0x7D, 0x5A8060}, {0x91, 0x5A7D70}, {0x9B, 0x5A8DE0}, {0xA5, 0x5A8E00}};
// 0x4A7F10: 0x2D1 bytes  WardMote_Draw
constexpr mh::CallSite kCalls4A7F10[] = {{0x19, 0x5A77C0}, {0x2E, 0x572FA0}, {0x66, 0x5B93D2}, {0x85, 0x5B93D2}, {0xA4, 0x5B93D2}, {0xEB, 0x5A7A00}, {0x104, 0x5A7A50}, {0x12B, 0x5A7A00}, {0x144, 0x5A7A50}, {0x179, 0x5A7610}, {0x180, 0x5A7780}, {0x1A0, 0x5A7A00}, {0x1B9, 0x5A7A50}, {0x1EC, 0x5A7A00}, {0x205, 0x5A7A50}, {0x24B, 0x5A85F0}, {0x254, 0x5A9350}, {0x2AF, 0x572FA0}};
// 0x4A81F0: 0x57 bytes  WardMote_Alloc
// 0x4A8250: 0x26 bytes  EvilEye_Task
constexpr mh::Imm kImms4A8250[] = {{0xF, 0x4A8280}, {0x17, 0x4F7350}};
// 0x4A8280: 0xD9 bytes  EvilEye_Start
constexpr mh::CallSite kCalls4A8280[] = {{0x61, 0x435180}, {0xD2, 0x587900}};
// 0x4A8360: 0x12 bytes; +0xB note: jmp through .data 0x65a924, 2 entries (EvilEyeChild_Kinds)  EvilEyeChild_Task
// 0x4A8380: 0x41 bytes; +0xB note: call through .data 0x65a92c, 4 entries (EvilEyeBeam_Steps)  EvilEyeBeam_Run
constexpr mh::CallSite kCalls4A8380[] = {{0x2B, 0x4A8820}, {0x3B, 0x4A8C70}};
// 0x4A83D0: 0x1EA bytes  EvilEyeBeam_Start
constexpr mh::CallSite kCalls4A83D0[] = {{0xE7, 0x446770}, {0x155, 0x5A7A70}, {0x172, 0x4FBD10}};
// 0x4A85C0: 0x163 bytes  EvilEyeBeam_Seek
constexpr mh::CallSite kCalls4A85C0[] = {{0x59, 0x4FBA90}, {0x98, 0x5A7A70}, {0xAC, 0x4FBC30}, {0x131, 0x4FBD10}, {0x136, 0x4A9350}, {0x155, 0x452F70}};
// 0x4A8730: 0xBA bytes  EvilEyeBeam_Trail
constexpr mh::CallSite kCalls4A8730[] = {{0x1C, 0x4FBD10}, {0x21, 0x4A9350}, {0x38, 0x435180}};
// 0x4A87F0: 0x2C bytes  EvilEyeBeam_End
constexpr mh::CallSite kCalls4A87F0[] = {{0x0, 0x4A93E0}, {0x26, 0x4351F0}};
// 0x4A8820: 0x44D bytes  EvilEyeBeam_DrawThin
constexpr mh::CallSite kCalls4A8820[] = {{0x16, 0x5A77C0}, {0x1F, 0x461E50}, {0xEE, 0x5A7A70}, {0x197, 0x5A7610}, {0x19E, 0x5A7780}, {0x1BC, 0x5A7A00}, {0x1EA, 0x5A7A50}, {0x229, 0x5A7A00}, {0x257, 0x5A7A50}, {0x2B7, 0x5A7A70}, {0x2D5, 0x5A7A00}, {0x303, 0x5A7A50}, {0x342, 0x5A7A00}, {0x370, 0x5A7A50}, {0x40E, 0x461E50}, {0x437, 0x5A77C0}, {0x440, 0x461E50}};
// 0x4A8C70: 0x6D9 bytes  EvilEyeBeam_DrawWide
constexpr mh::CallSite kCalls4A8C70[] = {{0x16, 0x5A77C0}, {0x1F, 0x461E50}, {0xE7, 0x5A7A70}, {0x18F, 0x5A7610}, {0x196, 0x5A7780}, {0x1B4, 0x5A7A00}, {0x1E2, 0x5A7A50}, {0x266, 0x5A7A70}, {0x284, 0x5A7A00}, {0x2B2, 0x5A7A50}, {0x35B, 0x461E50}, {0x427, 0x5A7A70}, {0x4D0, 0x5A7610}, {0x4D7, 0x5A7780}, {0x4F5, 0x5A7A00}, {0x523, 0x5A7A50}, {0x5A7, 0x5A7A70}, {0x5C5, 0x5A7A00}, {0x5F3, 0x5A7A50}, {0x69B, 0x461E50}, {0x6C3, 0x5A77C0}, {0x6CC, 0x461E50}};
// 0x4A9350: 0x8E bytes  EvilEyeBeam_Record
// 0x4A93E0: 0x8B bytes  EvilEyeBeam_Erase
// 0x4A9470: 0x29 bytes; +0xB note: call through .data 0x65a93c, 3 entries (EvilEyeSpark_Steps)  EvilEyeSpark_Run
constexpr mh::CallSite kCalls4A9470[] = {{0x23, 0x4A95A0}};
// 0x4A94A0: 0x61 bytes  EvilEyeSpark_Start
constexpr mh::CallSite kCalls4A94A0[] = {{0x0, 0x4FBD10}, {0xF, 0x5B93D2}, {0x21, 0x5B93D2}, {0x33, 0x5B93D2}};
// 0x4A9510: 0x36 bytes  EvilEyeSpark_Grow
constexpr mh::CallSite kCalls4A9510[] = {{0x0, 0x5B93D2}};
// 0x4A9550: 0x49 bytes  EvilEyeSpark_Fade
constexpr mh::CallSite kCalls4A9550[] = {{0x12, 0x5B93D2}, {0x43, 0x4351F0}};
// 0x4A95A0: 0x283 bytes  EvilEyeSpark_Draw
constexpr mh::CallSite kCalls4A95A0[] = {{0x10, 0x5A77C0}, {0x26, 0x572FA0}, {0x45, 0x5A75D0}, {0x4D, 0x5A7780}, {0x57, 0x5A7A50}, {0x85, 0x5A7A00}, {0xB3, 0x5A7A50}, {0xE1, 0x5A7A00}, {0x112, 0x5A7A50}, {0x140, 0x5A7A00}, {0x16E, 0x5A7A50}, {0x19C, 0x5A7A00}, {0x1D3, 0x5A79A0}, {0x1E3, 0x5A79E0}, {0x278, 0x572FA0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Berserk_Task", 0x4A6440, 0x46, nullptr, 0, kImms4A6440, MH_N(kImms4A6440), nullptr, 0, S08_P(Berserk_Task)},
    {"Berserk_Start", 0x4A6490, 0xF0, kCalls4A6490, MH_N(kCalls4A6490), nullptr, 0, nullptr, 0, S08_P(Berserk_Start)},
    {"Berserk_Tint", 0x4A6580, 0x4E, kCalls4A6580, MH_N(kCalls4A6580), nullptr, 0, nullptr, 0, S08_P(Berserk_Tint)},
    {"Berserk_Brighten", 0x4A65D0, 0x65, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(Berserk_Brighten)},
    {"Berserk_WaitChildren", 0x4A6640, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(Berserk_WaitChildren)},
    {"Berserk_Fade", 0x4A6650, 0x86, kCalls4A6650, MH_N(kCalls4A6650), nullptr, 0, nullptr, 0, S08_P(Berserk_Fade)},
    {"BerserkChild_Task", 0x4A66E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(BerserkChild_Task)},
    {"BerserkRing_Run", 0x4A6700, 0x4E, kCalls4A6700, MH_N(kCalls4A6700), kImms4A6700, MH_N(kImms4A6700), nullptr, 0, S08_P(BerserkRing_Run)},
    {"BerserkRing_Start", 0x4A6750, 0x61, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(BerserkRing_Start)},
    {"BerserkRing_Grow", 0x4A67C0, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(BerserkRing_Grow)},
    {"BerserkRing_Fade", 0x4A67E0, 0x35, kCalls4A67E0, MH_N(kCalls4A67E0), nullptr, 0, nullptr, 0, S08_P(BerserkRing_Fade)},
    {"BerserkGlow_Run", 0x4A6820, 0x56, kCalls4A6820, MH_N(kCalls4A6820), kImms4A6820, MH_N(kImms4A6820), nullptr, 0, S08_P(BerserkGlow_Run)},
    {"BerserkGlow_Start", 0x4A6880, 0x45, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(BerserkGlow_Start)},
    {"BerserkRing_Draw", 0x4A68D0, 0x334, kCalls4A68D0, MH_N(kCalls4A68D0), nullptr, 0, nullptr, 0, S08_P(BerserkRing_Draw)},
    {"BerserkGlow_Draw", 0x4A6C10, 0x1DA, kCalls4A6C10, MH_N(kCalls4A6C10), nullptr, 0, nullptr, 0, S08_P(BerserkGlow_Draw)},
    {"Counter_Task", 0x4A6DF0, 0x26, nullptr, 0, kImms4A6DF0, MH_N(kImms4A6DF0), nullptr, 0, S08_P(Counter_Task)},
    {"Counter_Start", 0x4A6E20, 0x9B, kCalls4A6E20, MH_N(kCalls4A6E20), nullptr, 0, nullptr, 0, S08_P(Counter_Start)},
    {"CounterChild_Task", 0x4A6EC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(CounterChild_Task)},
    {"CounterMark_Run", 0x4A6EE0, 0xAF, kCalls4A6EE0, MH_N(kCalls4A6EE0), kImms4A6EE0, MH_N(kImms4A6EE0), nullptr, 0, S08_P(CounterMark_Run)},
    {"CounterMark_Start", 0x4A6F90, 0x6F, kCalls4A6F90, MH_N(kCalls4A6F90), nullptr, 0, nullptr, 0, S08_P(CounterMark_Start)},
    {"CounterMark_Open", 0x4A7000, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(CounterMark_Open)},
    {"CounterMark_Grow", 0x4A7030, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(CounterMark_Grow)},
    {"CounterMark_Swell", 0x4A7060, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(CounterMark_Swell)},
    {"CounterMark_Fade", 0x4A70A0, 0x48, kCalls4A70A0, MH_N(kCalls4A70A0), nullptr, 0, nullptr, 0, S08_P(CounterMark_Fade)},
    {"CounterMark_Place", 0x4A70F0, 0x12E, kCalls4A70F0, MH_N(kCalls4A70F0), nullptr, 0, nullptr, 0, S08_P(CounterMark_Place)},
    {"CounterMark_DrawSpokes", 0x4A7220, 0x306, kCalls4A7220, MH_N(kCalls4A7220), nullptr, 0, nullptr, 0, S08_P(CounterMark_DrawSpokes)},
    {"CounterMark_DrawArc", 0x4A7530, 0x152, kCalls4A7530, MH_N(kCalls4A7530), nullptr, 0, nullptr, 0, S08_P(CounterMark_DrawArc)},
    {"CounterMark_DrawDisc", 0x4A7690, 0x193, kCalls4A7690, MH_N(kCalls4A7690), nullptr, 0, nullptr, 0, S08_P(CounterMark_DrawDisc)},
    {"Ward_Task", 0x4A7830, 0x8D, kCalls4A7830, MH_N(kCalls4A7830), kImms4A7830, MH_N(kImms4A7830), nullptr, 0, S08_P(Ward_Task)},
    {"Ward_Start", 0x4A78C0, 0x109, kCalls4A78C0, MH_N(kCalls4A78C0), nullptr, 0, nullptr, 0, S08_P(Ward_Start)},
    {"Ward_Tint", 0x4A79D0, 0x54, kCalls4A79D0, MH_N(kCalls4A79D0), nullptr, 0, nullptr, 0, S08_P(Ward_Tint)},
    {"Ward_Brighten", 0x4A7A30, 0x72, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(Ward_Brighten)},
    {"Ward_Fade", 0x4A7AB0, 0x127, kCalls4A7AB0, MH_N(kCalls4A7AB0), nullptr, 0, nullptr, 0, S08_P(Ward_Fade)},
    {"WardMote_Task", 0x4A7BE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(WardMote_Task)},
    {"WardMote_Run", 0x4A7C00, 0x33, kCalls4A7C00, MH_N(kCalls4A7C00), nullptr, 0, nullptr, 0, S08_P(WardMote_Run)},
    {"WardMote_Launch", 0x4A7C40, 0x10D, kCalls4A7C40, MH_N(kCalls4A7C40), nullptr, 0, nullptr, 0, S08_P(WardMote_Launch)},
    {"WardMote_Open", 0x4A7D50, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(WardMote_Open)},
    {"WardMote_Fly", 0x4A7DA0, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(WardMote_Fly)},
    {"WardMote_End", 0x4A7DE0, 0x7F, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(WardMote_End)},
    {"WardMote_PushMatrix", 0x4A7E60, 0xAE, kCalls4A7E60, MH_N(kCalls4A7E60), nullptr, 0, nullptr, 0, S08_P(WardMote_PushMatrix)},
    {"WardMote_Draw", 0x4A7F10, 0x2D1, kCalls4A7F10, MH_N(kCalls4A7F10), nullptr, 0, nullptr, 0, S08_P(WardMote_Draw)},
    {"WardMote_Alloc", 0x4A81F0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(WardMote_Alloc), 0xFF},
    {"EvilEye_Task", 0x4A8250, 0x26, nullptr, 0, kImms4A8250, MH_N(kImms4A8250), nullptr, 0, S08_P(EvilEye_Task)},
    {"EvilEye_Start", 0x4A8280, 0xD9, kCalls4A8280, MH_N(kCalls4A8280), nullptr, 0, nullptr, 0, S08_P(EvilEye_Start)},
    {"EvilEyeChild_Task", 0x4A8360, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(EvilEyeChild_Task)},
    {"EvilEyeBeam_Run", 0x4A8380, 0x41, kCalls4A8380, MH_N(kCalls4A8380), nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_Run)},
    {"EvilEyeBeam_Start", 0x4A83D0, 0x1EA, kCalls4A83D0, MH_N(kCalls4A83D0), nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_Start)},
    {"EvilEyeBeam_Seek", 0x4A85C0, 0x163, kCalls4A85C0, MH_N(kCalls4A85C0), nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_Seek)},
    {"EvilEyeBeam_Trail", 0x4A8730, 0xBA, kCalls4A8730, MH_N(kCalls4A8730), nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_Trail)},
    {"EvilEyeBeam_End", 0x4A87F0, 0x2C, kCalls4A87F0, MH_N(kCalls4A87F0), nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_End)},
    {"EvilEyeBeam_DrawThin", 0x4A8820, 0x44D, kCalls4A8820, MH_N(kCalls4A8820), nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_DrawThin)},
    {"EvilEyeBeam_DrawWide", 0x4A8C70, 0x6D9, kCalls4A8C70, MH_N(kCalls4A8C70), nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_DrawWide)},
    {"EvilEyeBeam_Record", 0x4A9350, 0x8E, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_Record)},
    {"EvilEyeBeam_Erase", 0x4A93E0, 0x8B, nullptr, 0, nullptr, 0, nullptr, 0, S08_P(EvilEyeBeam_Erase)},
    {"EvilEyeSpark_Run", 0x4A9470, 0x29, kCalls4A9470, MH_N(kCalls4A9470), nullptr, 0, nullptr, 0, S08_P(EvilEyeSpark_Run)},
    {"EvilEyeSpark_Start", 0x4A94A0, 0x61, kCalls4A94A0, MH_N(kCalls4A94A0), nullptr, 0, nullptr, 0, S08_P(EvilEyeSpark_Start)},
    {"EvilEyeSpark_Grow", 0x4A9510, 0x36, kCalls4A9510, MH_N(kCalls4A9510), nullptr, 0, nullptr, 0, S08_P(EvilEyeSpark_Grow)},
    {"EvilEyeSpark_Fade", 0x4A9550, 0x49, kCalls4A9550, MH_N(kCalls4A9550), nullptr, 0, nullptr, 0, S08_P(EvilEyeSpark_Fade)},
    {"EvilEyeSpark_Draw", 0x4A95A0, 0x283, kCalls4A95A0, MH_N(kCalls4A95A0), nullptr, 0, nullptr, 0, S08_P(EvilEyeSpark_Draw)},
};
#undef MH_N

enum : unsigned {
    kBerserk_Task, kBerserk_Start, kBerserk_Tint, kBerserk_Brighten, kBerserk_WaitChildren, kBerserk_Fade,
    kBerserkChild_Task, kBerserkRing_Run, kBerserkRing_Start, kBerserkRing_Grow, kBerserkRing_Fade, kBerserkGlow_Run,
    kBerserkGlow_Start, kBerserkRing_Draw, kBerserkGlow_Draw,
    kCounter_Task, kCounter_Start, kCounterChild_Task, kCounterMark_Run, kCounterMark_Start, kCounterMark_Open,
    kCounterMark_Grow, kCounterMark_Swell, kCounterMark_Fade, kCounterMark_Place, kCounterMark_DrawSpokes,
    kCounterMark_DrawArc, kCounterMark_DrawDisc,
    kWard_Task, kWard_Start, kWard_Tint, kWard_Brighten, kWard_Fade, kWardMote_Task, kWardMote_Run, kWardMote_Launch,
    kWardMote_Open, kWardMote_Fly, kWardMote_End, kWardMote_PushMatrix, kWardMote_Draw, kWardMote_Alloc,
    kEvilEye_Task, kEvilEye_Start, kEvilEyeChild_Task, kEvilEyeBeam_Run, kEvilEyeBeam_Start, kEvilEyeBeam_Seek,
    kEvilEyeBeam_Trail, kEvilEyeBeam_End, kEvilEyeBeam_DrawThin, kEvilEyeBeam_DrawWide, kEvilEyeBeam_Record,
    kEvilEyeBeam_Erase, kEvilEyeSpark_Run, kEvilEyeSpark_Start, kEvilEyeSpark_Grow, kEvilEyeSpark_Fade,
    kEvilEyeSpark_Draw, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kKind2 = 0x905E60, kTints = 0x7E0700,
                        kBattleMore = 0x904B50, kAbility = 0x904B80, kFormIndex = 0x904B89, kWardPool = 0x67BC40,
                        kTrails = 0x67DD40;
constexpr unsigned kTrailBytes = 0x600;

// Set by the seed for the functions each matters to.
bool g_allow_none = false;   // BattleTask_Create may answer 0xFF (EvilEyeBeam_Trail)
bool g_beams = false;        // a beam's +4 stays 0 / 1 (MAGIC044)
bool g_seek = false;         // Math_Ratan2 answers near the snap's bounds (EvilEyeBeam_Seek)

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log, then Gfx_PacketNext on by its size, kept in the buffer.
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
// BattleActor_UpdateScreenXY: which sprite; its screen point written.
std::uint32_t ScreenEffect(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    mh::FillBytes(Sprite_Current + 0x2E, 4);
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
// None free (0xFF) a quarter of the time, for EvilEyeBeam_Trail only.
std::uint32_t CreateEffect(const std::uint32_t*, std::uint32_t answer) {
    if (g_allow_none && mh::Noise() % 4 == 0) return answer | 0xFF;
    return answer;
}
// MagicFx_NearSprite: a C int of 0 (a third of the time) or 1 from its own
// stream, and Sprite_Current moved a quarter of the time, whatever it answers.
std::uint32_t NearEffect(const std::uint32_t*, std::uint32_t) {
    const std::uint32_t h = mh::Noise();
    if (h % 4 == 0) Sprite_Current = mh::TaskAt(h >> 8);
    return (h >> 4) % 3 == 0 ? 0u : 1u;
}
// Math_Ratan2 for EvilEyeBeam_Seek: half the time a heading whose turn from
// the old one (+0x10 by then) is one of the snap's bounds or one either side.
std::uint32_t RatanEffect(const std::uint32_t*, std::uint32_t answer) {
    if (!g_seek) return answer;
    const std::uint32_t h = mh::Noise();
    if (h & 1) return answer;
    static const std::uint32_t kTurns[] = {0x5FF, 0x600, 0x601, 0x9FF, 0xA00, 0xA01};
    const int old = static_cast<int>(static_cast<std::uint32_t>(Long(Sprite_Current + 0x10)) & 0xFFF);
    const int turn = static_cast<int>(kTurns[(h >> 1) % 6]);
    const int to = h & 0x100 ? old - turn : old + turn;
    if (to < 0 || to > 0xFFF) return answer;
    return (answer & ~0xFFFu) | static_cast<std::uint32_t>(to);
}
// The GTE stand-ins of the matrix push: a result from the inputs, written
// where the real callee writes (group S22's).
std::uint32_t RotTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* v = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* t = reinterpret_cast<long*>(static_cast<std::uintptr_t>(a[1]));
    t[0] = v[0] * 3 + 1;
    t[1] = v[1] * 5 - 2;
    t[2] = v[2] * 7 + 3;
    return answer;
}
std::uint32_t RotMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* r = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[0]));
    auto* m = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[1]));
    for (unsigned i = 0; i < 9; ++i) m[i] = static_cast<short>(r[i % 3] * static_cast<int>(i + 1) + static_cast<int>(i));
    return answer;
}
std::uint32_t MulMatrixEffect(const std::uint32_t* a, std::uint32_t answer) {
    const auto* b = reinterpret_cast<const short*>(static_cast<std::uintptr_t>(a[1]));
    auto* out = reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[2]));
    mh::Note(a[1] == a[2]);
    for (unsigned i = 0; i < 9; ++i) out[i] = static_cast<short>(b[i] * 3 + 7);
    return answer;
}
std::uint32_t SetTransEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0])) + 0x14, 12);
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage, kP = mh::Answer::kPhase;
#define S08_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S08_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S08_OURS(BattleTask_Create), 2, {kU8, kU8}, mh::Answer::kByte, 0, mh::at::kTaskCount - 1, {}, &CreateEffect},
    {S08_OURS(BattleActor_UpdateScreenXY), 0, {}, kG, 0, 0, {}, &ScreenEffect},
    {S08_OURS(MagicFx_NearSprite), 2, {kAll, kAll}, kG, 0, 0, {}, &NearEffect},
    // the tint calls
    {S08_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    {S08_OURS(Tint_Release), 1, {kU8}, kG, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, field_misc,
    // battle_items: all ours)
    {S08_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S08_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S08_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0, {}, &RatanEffect},
    {S08_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S08_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S08_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S08_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S08_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S08_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S08_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S08_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S08_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S08_OURS(Gpu_SetLineG2), 1, {kAll}, kG, 0, 0},
    {S08_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S08_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S08_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S08_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S08_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S08_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projections: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S08_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S08_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S08_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S08_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction
    {S08_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // this group's own, called directly: the steps and draws log the task
    // they ran for; the two argument draws their arguments; the pool's alloc
    // answers "none" or a record
    {S08_RAW(0x4A68D0), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A6C10), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A70F0), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A7220), 2, {kU8, kU16}, kG, 0, 0},
    {S08_RAW(0x4A7530), 2, {kU8, kU16}, kG, 0, 0},
    {S08_RAW(0x4A7690), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A7BE0), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A7E60), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A7F10), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A81F0), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},
    {S08_RAW(0x4A8820), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A8C70), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A9350), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A93E0), 0, {}, kP, 0, 0},
    {S08_RAW(0x4A95A0), 0, {}, kP, 0, 0},
};
#undef S08_OURS
#undef S08_RAW

// The seven .data handler tables the dispatchers read in place
// (BerserkChild_Kinds and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65A82C, 2}, {0x65A834, 1}, {0x65A904, 1}, {0x65A908, 4}, {0x65A924, 2}, {0x65A92C, 4}, {0x65A93C, 3},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},              // Gfx_PacketNext
    {0, kPrimBytes},            // g_prims (filled in at start-up)
    {kVertex, 0x20},            // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},           // 0x903850.., Scratch_Swap at +0xC
    {kKind2, 8},                // Field_Kind2Z, Field_Kind2X
    {kTints, 0xC00},            // MoveScript_TintRecords
    {0x80E980, 0x200},          // Gfx_ClutStripSource row 26
    {0x812980, 0x200},          // Gfx_ClutStrip row 26
    {kBattleMore, 0x50},        // 0x904B50..: the ability word 0x904B80, the form byte 0x904B89
    {kWardPool, 64 * 0x84},     // WardMote_Pool
    {kTrails, kTrailBytes},     // EvilEyeBeam_Trails, and past it what an index of up to 0x120 reaches
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* PoolRecord(unsigned i) { return mh::Mem(kWardPool + (i & 63) * 0x84u); }
unsigned char* SomeOwner(std::uint32_t v) { return v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v); }

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 16); break;
    case 3: SetLong(mh::Mem(kKind2 + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 4: mh::Mem(kTints + v % 0xC00)[0] = Byte(h >> 24); break;
    case 5: PoolRecord(v)[0] = Byte(PoolRecord(v)[0] ^ 1); break;
    case 6: SetWord(mh::Mem(kTrails + 2 * (v % (kTrailBytes / 2))), h & 0x10000 ? 0xFFFF : h >> 17); break;
    case 7:
        if (v & 1) SetWord(mh::Mem(kAbility), h & 0x20000 ? 0x2B : h >> 18);
        else mh::Mem(kFormIndex)[0] = Byte(h >> 24);
        break;
    default: break;
    }
}

// A beam's +4 (the trail index's high part) back to 0 / 1 after any
// disturbance, while MAGIC044's functions run.
void Settle() {
    if (g_beams) Sprite_Current[4] &= 1;
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

// The pool: full, the first n taken, or as the fill left it; every record's
// owner a task or a sprite record (the walk makes it 0x93B940).
void FillPool() {
    const unsigned mode = mh::Next() % 4;
    const unsigned taken = mh::Next() % 65;
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (mode == 0) rec[0] = Byte(rec[0] | 1);
        else if (mode == 1) rec[0] = Byte(i < taken ? rec[0] | 1 : rec[0] & ~1u);
        mh::SetPointer(kWardPool + i * 0x84u + 0x80, SomeOwner(mh::Next()));
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    FillPool();
    g_allow_none = k == kEvilEyeBeam_Trail;
    g_seek = k == kEvilEyeBeam_Seek;
    g_beams = k >= kEvilEye_Task;
    if (g_beams)
        for (unsigned t = 0; t < 4; ++t) mh::TaskAt(t)[4] &= 1;
    if (mh::Often()) sc[8] = Byte(mh::Next() % 5);
    if (mh::Half()) sc[0] = Byte(mh::Half() ? 0 : 1);
    if (mh::Often()) SetWord(mh::Mem(kAbility), mh::Half() ? 0x2B : MH_PICK(0x2A, 0x2C, 0x99, 0x2B));
    switch (k) {
    // the dispatchers: inside their tables
    case kBerserk_Task: sc[1] = Byte(mh::Next() % 6); break;
    case kBerserkChild_Task: case kCounter_Task: case kEvilEye_Task: case kEvilEyeChild_Task:
        sc[1] = Byte(mh::Next() % 2);
        break;
    case kCounterChild_Task: case kWardMote_Task: sc[1] = 0; break;
    case kWard_Task: sc[1] = Byte(mh::Next() % 5); break;
    case kBerserkRing_Run: case kEvilEyeSpark_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kBerserkGlow_Run: case kWardMote_Run: case kEvilEyeBeam_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kCounterMark_Run: sc[2] = Byte(mh::Next() % 5); break;
    // the counters: at their thresholds
    case kBerserk_Tint: case kBerserkRing_Start: case kWard_Tint: case kWard_Brighten: case kWardMote_Launch:
    case kEvilEyeBeam_End:
        Near(sc[9], 1);
        break;
    case kBerserk_Brighten: Near(sc[0xA], 7); break;
    case kBerserk_WaitChildren: Near(sc[0xB], 0); break;
    case kBerserk_Fade: case kBerserkRing_Fade: case kEvilEyeSpark_Fade: Near(sc[0xA], 1); break;
    case kBerserkRing_Grow: case kCounterMark_Swell: Near(sc[0xA], 0x1C); break;
    case kCounterMark_Open: Near(sc[0xA], 3); break;
    case kCounterMark_Grow: Near(sc[9], 0x48); break;
    case kCounterMark_Fade:
        Near(sc[0xA], 4);
        if (mh::Half()) sc[9] = 0;
        break;
    case kCounterMark_Place:
        if (mh::Often()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Next() % 3);
        if (mh::Half()) mh::PartyOf(Byte(mh::Mem(mh::at::kTarget)[0] % 3))[0x134] ^= 2;
        break;
    case kWard_Fade:
        if (mh::Often()) sc[9] = Byte(mh::Often() ? 1 : 2);
        if (mh::Often()) sc[0xB] = Byte(MH_PICK(0, 1, 1, 2));
        break;
    case kWardMote_Open: Near(sc[0xA], 0xE); break;
    case kWardMote_Fly: Near(sc[9], 0x17); break;
    case kWardMote_End: Near(sc[0xA], 2); break;
    case kEvilEyeBeam_Start: {
        const unsigned actor = mh::Mem(mh::at::kActor)[0];
        if (actor >= 3 && mh::Often()) mh::EnemyOf(Byte(actor))[0x8C] = Byte(MH_PICK(0x9A, 0xB5, 0x9B, 0xB4));
        break;
    }
    case kEvilEyeBeam_Seek: Near(sc[0xA], 0xF); break;
    case kEvilEyeBeam_Trail:
        Near(sc[0xA], 0xF);
        if (mh::Often()) Frame_Counter &= ~3u;
        break;
    // the trail: short, sometimes erased at its head
    case kEvilEyeBeam_DrawThin: case kEvilEyeBeam_DrawWide: case kEvilEyeBeam_Record: case kEvilEyeBeam_Erase:
        if (mh::Often()) sc[0xA] = Byte(mh::Next() % 0x14);
        if (mh::Often()) sc[0xB] = Byte(mh::Next() % 0x12);
        if (mh::Half()) SetWord(mh::Mem(kTrails + (sc[4] * 32u + sc[0xB]) * 4u), 0xFFFF);
        if (mh::Half() && mh::Half()) SetWord(mh::Mem(kTrails + (sc[4] * 32u + sc[0xB] + 1) * 4u), 0xFFFF);
        break;
    case kEvilEyeSpark_Grow: Near(sc[9], 0xC); break;
    default: break;
    }
}

// CounterMark_DrawSpokes' first angle stays below 0xE0 (from 0xE0 the
// original never ends); its end and CounterMark_DrawArc's bounds seeded.
void Args(unsigned k, std::uint32_t* a) {
    if (k == kCounterMark_DrawSpokes) {
        const std::uint32_t first = mh::Often() ? mh::Next() % 0xE0 : 0xD0 + mh::Next() % 0x10;
        a[0] = (a[0] & 0xFFFFFF00u) | first;
    } else if (k == kCounterMark_DrawArc && mh::Half()) {
        a[0] = (a[0] & 0xFFFFFF00u) | MH_PICK(0, 0x7F, 0x80, 0xFF);
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s08", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.settle = &Settle;
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s08
