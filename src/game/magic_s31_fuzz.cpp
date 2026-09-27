// BOF3X_SHADOW=magic_s31: group S31's four overlays (MAGIC132, 137, 138, 143)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s31.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC132 / 137 / 138 / 143
// --clones (2026-09-26; capstone, every jump internal but the jump table of
// CoronaRay_PushMatrix, which the harness moves into the copy; no REFUSED
// line), names given. Beyond the standard set this group lists the draw
// callees (the GTE and libgpu entry points, Math_Sin / Math_Cos,
// Gfx_CommitPrim, MapView_LinkPrimAt), the sprite calls, AreaMap_Elevation,
// the engine's 0x446770, and the functions of its own (and S22's
// LightningBolt_PushMatrix) that its functions call directly. Everything the
// harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - the GTE callees of the two matrix pushes log what their pointers point
//     at (`deref`) and write a result where the real ones write;
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what the caller reads back is compared;
//   - the sprite calls that act on Sprite_Current (the script tick, the
//     animation, the queue, the screen update) log which sprite, and the two
//     that draw log the frame-offset table 0x9039D8 the shells swap;
//   - BattleTask_Create answers 0xFF (none free) sometimes, but only for
//     MainCannon_Fire, the one caller that tests it (the others write through
//     the slot unchecked, a fault on both sides);
//   - Gte_PushMatrix keeps the facing byte inside CoronaRay_PushMatrix's
//     four-entry jump table while that function is fuzzed (past it the
//     original turns by stack garbage and ours aborts).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s31.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s31 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC132 / 137 / 138 / 143 --clones, 2026-09-26,
// names given.
// 0x4E6950: 0x3E bytes  DoomBreath_Task
constexpr mh::Imm kImms4E6950[] = {{0xF, 0x4E6990}, {0x17, 0x4E6A50}, {0x22, 0x4E6AD0}, {0x2A, 0x4F9F70}, {0x32, 0x4E6B60}};
// 0x4E6990: 0xC0 bytes  DoomBreath_Start
constexpr mh::CallSite kCalls4E6990[] = {{0x11, 0x4FC0E0}, {0x1A, 0x435180}, {0x75, 0x454DC0}, {0x89, 0x454CC0}, {0x9C, 0x587900}};
// 0x4E6A50: 0x73 bytes  DoomBreath_Brighten
// 0x4E6AD0: 0x8E bytes  DoomBreath_Fade
constexpr mh::CallSite kCalls4E6AD0[] = {{0x68, 0x454DC0}, {0x74, 0x4FBDB0}};
// 0x4E6B60: 0x31 bytes  DoomBreath_End
constexpr mh::CallSite kCalls4E6B60[] = {{0x1C, 0x4530D0}, {0x2B, 0x4351F0}};
// 0x4E6BA0: 0x12 bytes; +0xB note: jmp through .data 0x65bd64, 43 code entries (a data_tables entry)  DoomBreathOrb_Task
// 0x4E6BC0: 0x40 bytes; +0xB note: call through .data 0x65bd68, 42 code entries (a data_tables entry)  DoomBreathOrb_Run
constexpr mh::CallSite kCalls4E6BC0[] = {{0x2B, 0x4E6DF0}, {0x30, 0x4E6EC0}, {0x35, 0x4E7130}, {0x3A, 0x5A7BC0}};
// 0x4E6C00: 0xF2 bytes  DoomBreathOrb_Launch
constexpr mh::CallSite kCalls4E6C00[] = {{0x49, 0x446770}, {0xAB, 0x446770}};
// 0x4E6D00: 0x3D bytes  DoomBreathOrb_Move
// 0x4E6D40: 0x30 bytes  DoomBreathOrb_Grow
// 0x4E6D70: 0x3E bytes  DoomBreathOrb_Shrink
// 0x4E6DB0: 0x36 bytes  DoomBreathOrb_End
constexpr mh::CallSite kCalls4E6DB0[] = {{0x30, 0x4351F0}};
// 0x4E6DF0: 0xC9 bytes  DoomBreathOrb_PushMatrix
constexpr mh::CallSite kCalls4E6DF0[] = {{0x3, 0x5A7B90}, {0x89, 0x5A8200}, {0x98, 0x5A8060}, {0xAC, 0x5A7D70}, {0xB6, 0x5A8DE0}, {0xC0, 0x5A8E00}};
// 0x4E6EC0: 0x268 bytes  DoomBreathOrb_DrawStream
constexpr mh::CallSite kCalls4E6EC0[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x84, 0x5A7A00}, {0xB0, 0x5A75D0}, {0xB8, 0x5A7780}, {0x120, 0x5A7A00}, {0x17B, 0x5A79A0}, {0x18A, 0x5A79E0}, {0x21C, 0x5A85F0}, {0x225, 0x5A9290}, {0x22E, 0x461E50}, {0x24F, 0x5A77C0}, {0x258, 0x461E50}};
// 0x4E7130: 0x2E4 bytes  DoomBreathOrb_DrawGlow
constexpr mh::CallSite kCalls4E7130[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x82, 0x5A7A00}, {0xAE, 0x5A7610}, {0xB6, 0x5A7780}, {0x120, 0x5A7A00}, {0x29A, 0x5A85F0}, {0x2A0, 0x5A9350}, {0x2A9, 0x461E50}, {0x2CB, 0x5A77C0}, {0x2D4, 0x461E50}};
// 0x4E7420: 0x2E bytes  Corona_Task
constexpr mh::Imm kImms4E7420[] = {{0xF, 0x4E7450}, {0x17, 0x4E75B0}, {0x22, 0x4E75F0}};
// 0x4E7450: 0x154 bytes  Corona_Start
constexpr mh::CallSite kCalls4E7450[] = {{0x21, 0x435180}, {0x62, 0x435180}, {0xE8, 0x435180}};
// 0x4E75B0: 0x33 bytes  Corona_Wait
constexpr mh::CallSite kCalls4E75B0[] = {{0x22, 0x452F70}};
// 0x4E75F0: 0x32 bytes  Corona_End
constexpr mh::CallSite kCalls4E75F0[] = {{0x13, 0x4530D0}, {0x2C, 0x4351F0}};
// 0x4E7630: 0x12 bytes; +0xB note: jmp through .data 0x65bd7c, 37 code entries (a data_tables entry)  CoronaChild_Task
// 0x4E7650: 0x2D bytes; +0xB note: call through .data 0x65bd88, 34 code entries (a data_tables entry)  CoronaRay_Run
constexpr mh::CallSite kCalls4E7650[] = {{0x1D, 0x4E77D0}, {0x22, 0x4E78B0}, {0x27, 0x5A7BC0}};
// 0x4E7680: 0xAB bytes  CoronaRay_Start
constexpr mh::CallSite kCalls4E7680[] = {{0x20, 0x446770}, {0x58, 0x5720C0}, {0x71, 0x587740}};
// 0x4E7730: 0x96 bytes  CoronaRay_Advance
constexpr mh::CallSite kCalls4E7730[] = {{0x46, 0x446770}, {0x90, 0x4351F0}};
// 0x4E77D0: 0xDC bytes  CoronaRay_PushMatrix
constexpr mh::CallSite kCalls4E77D0[] = {{0x3, 0x5A7B90}, {0x8B, 0x5A8200}, {0x9A, 0x5A8060}, {0xAE, 0x5A7D70}, {0xB8, 0x5A8DE0}, {0xC2, 0x5A8E00}};
constexpr mh::JumpTable kTables4E77D0[] = {{0x26, 0xCC, 4}};
// 0x4E78B0: 0x578 bytes  CoronaRay_Draw
constexpr mh::CallSite kCalls4E78B0[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0x6C, 0x5A7A00}, {0x89, 0x5A7A50}, {0xA9, 0x5A7A00}, {0xC6, 0x5A7A00}, {0xE3, 0x5A7A50}, {0x103, 0x5A7A00}, {0x138, 0x5A7630}, {0x140, 0x5A7780}, {0x1BE, 0x5A7A00}, {0x1DB, 0x5A7A50}, {0x1FB, 0x5A7A00}, {0x218, 0x5A7A00}, {0x235, 0x5A7A50}, {0x255, 0x5A7A00}, {0x477, 0x5A79A0}, {0x487, 0x5A79E0}, {0x52B, 0x5A85F0}, {0x531, 0x5A93A0}, {0x53A, 0x461E50}, {0x562, 0x5A77C0}, {0x56B, 0x461E50}};
// 0x4E7E30: 0x23 bytes; +0xB note: call through .data 0x65bd98, 30 code entries (a data_tables entry)  CoronaFlash_Run
constexpr mh::CallSite kCalls4E7E30[] = {{0x1D, 0x4E7E80}};
// 0x4E7E60: 0x1C bytes  CoronaFlash_Start
// 0x4E7E80: 0xF2 bytes  CoronaFlash_Draw
constexpr mh::CallSite kCalls4E7E80[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x25, 0x5A7610}, {0x2D, 0x5A7780}, {0xCD, 0x461E50}, {0xDE, 0x5A77C0}, {0xE7, 0x461E50}};
// 0x4E7F80: 0x2E bytes  CoronaEnemy_Task
constexpr mh::Imm kImms4E7F80[] = {{0xF, 0x4E7FB0}, {0x17, 0x43EC10}, {0x22, 0x4AEE90}};
// 0x4E7FB0: 0x31 bytes  CoronaEnemy_Start
constexpr mh::CallSite kCalls4E7FB0[] = {{0x16, 0x5891F0}, {0x1E, 0x5893A0}, {0x23, 0x5890E0}};
// 0x4E7FF0: 0x2E bytes  MainCannon_Task
constexpr mh::Imm kImms4E7FF0[] = {{0xF, 0x4E8020}, {0x17, 0x4E80A0}, {0x22, 0x4E8160}};
// 0x4E8020: 0x72 bytes  MainCannon_Start
constexpr mh::CallSite kCalls4E8020[] = {{0x5D, 0x5893A0}};
// 0x4E80A0: 0xBC bytes  MainCannon_Fire
constexpr mh::CallSite kCalls4E80A0[] = {{0x12, 0x435180}, {0xAE, 0x5893A0}};
// 0x4E8160: 0x40 bytes  MainCannon_End
constexpr mh::CallSite kCalls4E8160[] = {{0x11, 0x5893A0}, {0x2B, 0x4530D0}, {0x3A, 0x4351F0}};
// 0x4E81A0: 0x12 bytes; +0xB note: jmp through .data 0x65bda8, 26 code entries (a data_tables entry)  MainCannonChild_Task
// 0x4E81C0: 0x52 bytes  MainCannonShell_Run
constexpr mh::CallSite kCalls4E81C0[] = {{0x3F, 0x5890E0}};
constexpr mh::Imm kImms4E81C0[] = {{0x19, 0x4E8220}, {0x24, 0x4E8440}, {0x2C, 0x4AEE90}};
// 0x4E8220: 0x21F bytes  MainCannonShell_Aim
constexpr mh::CallSite kCalls4E8220[] = {{0x205, 0x5891F0}};
// 0x4E8440: 0x10E bytes  MainCannonShell_Fly
constexpr mh::CallSite kCalls4E8440[] = {{0xA0, 0x435180}, {0x107, 0x5893A0}};
// 0x4E8550: 0x52 bytes  MainCannonBlast_Run
constexpr mh::CallSite kCalls4E8550[] = {{0x3F, 0x588F20}};
constexpr mh::Imm kImms4E8550[] = {{0x19, 0x4E85B0}, {0x24, 0x4E8640}, {0x2C, 0x4AF490}};
// 0x4E85B0: 0x86 bytes  MainCannonBlast_Start
constexpr mh::CallSite kCalls4E85B0[] = {{0x59, 0x5891F0}, {0x75, 0x587740}};
// 0x4E8640: 0x17 bytes  MainCannonBlast_Play
constexpr mh::CallSite kCalls4E8640[] = {{0x0, 0x5893A0}, {0x12, 0x5890E0}};
// 0x4E8660: 0x26 bytes  ThunderClap_Task
constexpr mh::Imm kImms4E8660[] = {{0xF, 0x4E8690}, {0x17, 0x4E5200}};
// 0x4E8690: 0x6D bytes  ThunderClap_Start
constexpr mh::CallSite kCalls4E8690[] = {{0xB, 0x435180}, {0x56, 0x587900}};
// 0x4E8700: 0x12 bytes; +0xB note: jmp through .data 0x65bdb0, 24 code entries (a data_tables entry)  ThunderClapBolt_Task
// 0x4E8720: 0xAA bytes; +0xB note: call through .data 0x65bdb4, 23 code entries (a data_tables entry)  ThunderClapBolt_Run
constexpr mh::CallSite kCalls4E8720[] = {{0x12, 0x4FBD10}, {0x2C, 0x4CAE30}, {0x37, 0x4E88D0}, {0x4B, 0x4E88D0}, {0x59, 0x4E8DA0}, {0x70, 0x4E88D0}, {0x90, 0x4E8FA0}, {0x9F, 0x4E8FA0}, {0xA4, 0x5A7BC0}};
// 0x4E87D0: 0x2D bytes  ThunderClapBolt_Start
constexpr mh::CallSite kCalls4E87D0[] = {{0x0, 0x5B93D2}};
// 0x4E8800: 0x69 bytes  ThunderClapBolt_Rise
constexpr mh::CallSite kCalls4E8800[] = {{0x2D, 0x454DC0}, {0x41, 0x454CC0}, {0x4E, 0x452F70}};
// 0x4E8870: 0x55 bytes  ThunderClapBolt_Fade
constexpr mh::CallSite kCalls4E8870[] = {{0x2D, 0x4530D0}, {0x39, 0x454DC0}, {0x44, 0x4FBDB0}};
// 0x4E88D0: 0x4D0 bytes  ThunderClapBolt_DrawBand
constexpr mh::CallSite kCalls4E88D0[] = {{0x4B, 0x5A7A00}, {0x7A, 0x5A77C0}, {0x90, 0x572FA0}, {0xB9, 0x5B93D2}, {0xC2, 0x5B93D2}, {0xDF, 0x5B93D2}, {0x13E, 0x5A7A00}, {0x186, 0x5A7610}, {0x18D, 0x5A7780}, {0x22D, 0x5A85F0}, {0x233, 0x5A9350}, {0x249, 0x572FA0}, {0x271, 0x5A7610}, {0x27B, 0x5A7780}, {0x2E9, 0x5A85F0}, {0x2EF, 0x5A9350}, {0x305, 0x572FA0}, {0x330, 0x5A7610}, {0x33A, 0x5A7780}, {0x3D4, 0x5A85F0}, {0x3DA, 0x5A9350}, {0x3F0, 0x572FA0}, {0x418, 0x5A7610}, {0x422, 0x5A7780}, {0x490, 0x5A85F0}, {0x496, 0x5A9350}, {0x4AC, 0x572FA0}};
// 0x4E8DA0: 0x1F4 bytes  ThunderClapBolt_DrawArcs
constexpr mh::CallSite kCalls4E8DA0[] = {{0x31, 0x5B93D2}, {0x4D, 0x5A7A00}, {0xBB, 0x5A77C0}, {0xC6, 0x572FA0}, {0xD2, 0x5A76B0}, {0xDA, 0x5A7780}, {0xF2, 0x5A8250}, {0xFE, 0x5A9110}, {0x106, 0x5B93D2}, {0x10F, 0x5B93D2}, {0x123, 0x5B93D2}, {0x15A, 0x5A7A00}, {0x190, 0x5A8250}, {0x199, 0x5A9110}, {0x1B3, 0x5B93D2}, {0x1D6, 0x572FA0}};
// 0x4E8FA0: 0x199 bytes  ThunderClapBolt_DrawFlash
constexpr mh::CallSite kCalls4E8FA0[] = {{0x4, 0x5B93D2}, {0x32, 0x5A77C0}, {0x48, 0x572FA0}, {0x59, 0x5A75F0}, {0x60, 0x5A7780}, {0x8F, 0x5A7A00}, {0xB6, 0x5A7A50}, {0xE3, 0x5A7A00}, {0x10A, 0x5A7A50}, {0x180, 0x572FA0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"DoomBreath_Task", 0x4E6950, 0x3E, nullptr, 0, kImms4E6950, MH_N(kImms4E6950), nullptr, 0, reinterpret_cast<const void*>(&::DoomBreath_Task)},
    {"DoomBreath_Start", 0x4E6990, 0xC0, kCalls4E6990, MH_N(kCalls4E6990), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreath_Start)},
    {"DoomBreath_Brighten", 0x4E6A50, 0x73, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreath_Brighten)},
    {"DoomBreath_Fade", 0x4E6AD0, 0x8E, kCalls4E6AD0, MH_N(kCalls4E6AD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreath_Fade)},
    {"DoomBreath_End", 0x4E6B60, 0x31, kCalls4E6B60, MH_N(kCalls4E6B60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreath_End)},
    {"DoomBreathOrb_Task", 0x4E6BA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_Task)},
    {"DoomBreathOrb_Run", 0x4E6BC0, 0x40, kCalls4E6BC0, MH_N(kCalls4E6BC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_Run)},
    {"DoomBreathOrb_Launch", 0x4E6C00, 0xF2, kCalls4E6C00, MH_N(kCalls4E6C00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_Launch)},
    {"DoomBreathOrb_Move", 0x4E6D00, 0x3D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_Move)},
    {"DoomBreathOrb_Grow", 0x4E6D40, 0x30, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_Grow)},
    {"DoomBreathOrb_Shrink", 0x4E6D70, 0x3E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_Shrink)},
    {"DoomBreathOrb_End", 0x4E6DB0, 0x36, kCalls4E6DB0, MH_N(kCalls4E6DB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_End)},
    {"DoomBreathOrb_PushMatrix", 0x4E6DF0, 0xC9, kCalls4E6DF0, MH_N(kCalls4E6DF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_PushMatrix)},
    {"DoomBreathOrb_DrawStream", 0x4E6EC0, 0x268, kCalls4E6EC0, MH_N(kCalls4E6EC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_DrawStream)},
    {"DoomBreathOrb_DrawGlow", 0x4E7130, 0x2E4, kCalls4E7130, MH_N(kCalls4E7130), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DoomBreathOrb_DrawGlow)},
    {"Corona_Task", 0x4E7420, 0x2E, nullptr, 0, kImms4E7420, MH_N(kImms4E7420), nullptr, 0, reinterpret_cast<const void*>(&::Corona_Task)},
    {"Corona_Start", 0x4E7450, 0x154, kCalls4E7450, MH_N(kCalls4E7450), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Corona_Start)},
    {"Corona_Wait", 0x4E75B0, 0x33, kCalls4E75B0, MH_N(kCalls4E75B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Corona_Wait)},
    {"Corona_End", 0x4E75F0, 0x32, kCalls4E75F0, MH_N(kCalls4E75F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Corona_End)},
    {"CoronaChild_Task", 0x4E7630, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaChild_Task)},
    {"CoronaRay_Run", 0x4E7650, 0x2D, kCalls4E7650, MH_N(kCalls4E7650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaRay_Run)},
    {"CoronaRay_Start", 0x4E7680, 0xAB, kCalls4E7680, MH_N(kCalls4E7680), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaRay_Start)},
    {"CoronaRay_Advance", 0x4E7730, 0x96, kCalls4E7730, MH_N(kCalls4E7730), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaRay_Advance)},
    {"CoronaRay_PushMatrix", 0x4E77D0, 0xDC, kCalls4E77D0, MH_N(kCalls4E77D0), nullptr, 0, kTables4E77D0, MH_N(kTables4E77D0), reinterpret_cast<const void*>(&::CoronaRay_PushMatrix)},
    {"CoronaRay_Draw", 0x4E78B0, 0x578, kCalls4E78B0, MH_N(kCalls4E78B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaRay_Draw)},
    {"CoronaFlash_Run", 0x4E7E30, 0x23, kCalls4E7E30, MH_N(kCalls4E7E30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaFlash_Run)},
    {"CoronaFlash_Start", 0x4E7E60, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaFlash_Start)},
    {"CoronaFlash_Draw", 0x4E7E80, 0xF2, kCalls4E7E80, MH_N(kCalls4E7E80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaFlash_Draw)},
    {"CoronaEnemy_Task", 0x4E7F80, 0x2E, nullptr, 0, kImms4E7F80, MH_N(kImms4E7F80), nullptr, 0, reinterpret_cast<const void*>(&::CoronaEnemy_Task)},
    {"CoronaEnemy_Start", 0x4E7FB0, 0x31, kCalls4E7FB0, MH_N(kCalls4E7FB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CoronaEnemy_Start)},
    {"MainCannon_Task", 0x4E7FF0, 0x2E, nullptr, 0, kImms4E7FF0, MH_N(kImms4E7FF0), nullptr, 0, reinterpret_cast<const void*>(&::MainCannon_Task)},
    {"MainCannon_Start", 0x4E8020, 0x72, kCalls4E8020, MH_N(kCalls4E8020), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MainCannon_Start)},
    {"MainCannon_Fire", 0x4E80A0, 0xBC, kCalls4E80A0, MH_N(kCalls4E80A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MainCannon_Fire)},
    {"MainCannon_End", 0x4E8160, 0x40, kCalls4E8160, MH_N(kCalls4E8160), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MainCannon_End)},
    {"MainCannonChild_Task", 0x4E81A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MainCannonChild_Task)},
    {"MainCannonShell_Run", 0x4E81C0, 0x52, kCalls4E81C0, MH_N(kCalls4E81C0), kImms4E81C0, MH_N(kImms4E81C0), nullptr, 0, reinterpret_cast<const void*>(&::MainCannonShell_Run)},
    {"MainCannonShell_Aim", 0x4E8220, 0x21F, kCalls4E8220, MH_N(kCalls4E8220), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MainCannonShell_Aim)},
    {"MainCannonShell_Fly", 0x4E8440, 0x10E, kCalls4E8440, MH_N(kCalls4E8440), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MainCannonShell_Fly)},
    {"MainCannonBlast_Run", 0x4E8550, 0x52, kCalls4E8550, MH_N(kCalls4E8550), kImms4E8550, MH_N(kImms4E8550), nullptr, 0, reinterpret_cast<const void*>(&::MainCannonBlast_Run)},
    {"MainCannonBlast_Start", 0x4E85B0, 0x86, kCalls4E85B0, MH_N(kCalls4E85B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MainCannonBlast_Start)},
    {"MainCannonBlast_Play", 0x4E8640, 0x17, kCalls4E8640, MH_N(kCalls4E8640), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MainCannonBlast_Play)},
    {"ThunderClap_Task", 0x4E8660, 0x26, nullptr, 0, kImms4E8660, MH_N(kImms4E8660), nullptr, 0, reinterpret_cast<const void*>(&::ThunderClap_Task)},
    {"ThunderClap_Start", 0x4E8690, 0x6D, kCalls4E8690, MH_N(kCalls4E8690), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClap_Start)},
    {"ThunderClapBolt_Task", 0x4E8700, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClapBolt_Task)},
    {"ThunderClapBolt_Run", 0x4E8720, 0xAA, kCalls4E8720, MH_N(kCalls4E8720), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClapBolt_Run)},
    {"ThunderClapBolt_Start", 0x4E87D0, 0x2D, kCalls4E87D0, MH_N(kCalls4E87D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClapBolt_Start)},
    {"ThunderClapBolt_Rise", 0x4E8800, 0x69, kCalls4E8800, MH_N(kCalls4E8800), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClapBolt_Rise)},
    {"ThunderClapBolt_Fade", 0x4E8870, 0x55, kCalls4E8870, MH_N(kCalls4E8870), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClapBolt_Fade)},
    {"ThunderClapBolt_DrawBand", 0x4E88D0, 0x4D0, kCalls4E88D0, MH_N(kCalls4E88D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClapBolt_DrawBand)},
    {"ThunderClapBolt_DrawArcs", 0x4E8DA0, 0x1F4, kCalls4E8DA0, MH_N(kCalls4E8DA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClapBolt_DrawArcs)},
    {"ThunderClapBolt_DrawFlash", 0x4E8FA0, 0x199, kCalls4E8FA0, MH_N(kCalls4E8FA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderClapBolt_DrawFlash)},
};
#undef MH_N

enum : unsigned {
    kDoomBreath_Task, kDoomBreath_Start, kDoomBreath_Brighten, kDoomBreath_Fade, kDoomBreath_End, kDoomBreathOrb_Task,
    kDoomBreathOrb_Run, kDoomBreathOrb_Launch, kDoomBreathOrb_Move, kDoomBreathOrb_Grow, kDoomBreathOrb_Shrink,
    kDoomBreathOrb_End, kDoomBreathOrb_PushMatrix, kDoomBreathOrb_DrawStream, kDoomBreathOrb_DrawGlow,
    kCorona_Task, kCorona_Start, kCorona_Wait, kCorona_End, kCoronaChild_Task, kCoronaRay_Run, kCoronaRay_Start,
    kCoronaRay_Advance, kCoronaRay_PushMatrix, kCoronaRay_Draw, kCoronaFlash_Run, kCoronaFlash_Start, kCoronaFlash_Draw,
    kCoronaEnemy_Task, kCoronaEnemy_Start,
    kMainCannon_Task, kMainCannon_Start, kMainCannon_Fire, kMainCannon_End, kMainCannonChild_Task, kMainCannonShell_Run,
    kMainCannonShell_Aim, kMainCannonShell_Fly, kMainCannonBlast_Run, kMainCannonBlast_Start, kMainCannonBlast_Play,
    kThunderClap_Task, kThunderClap_Start, kThunderClapBolt_Task, kThunderClapBolt_Run, kThunderClapBolt_Start,
    kThunderClapBolt_Rise, kThunderClapBolt_Fade, kThunderClapBolt_DrawBand, kThunderClapBolt_DrawArcs,
    kThunderClapBolt_DrawFlash, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kActorRecord = 0x904B3C, kEventBattle = 0x904AAA,
                        kFrameSet = 0x9039D8, kKind2 = 0x905E60;

// Set by the seed for the one function each matters to.
bool g_allow_none = false;    // BattleTask_Create may answer 0xFF (MainCannon_Fire)
bool g_keep_facing = false;   // the facing stays inside 0..3 (CoronaRay_PushMatrix)

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log (the real ones link it), then Gfx_PacketNext on by its size, kept in
// the buffer (a draw writes up to 0x58 past it).
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
// The sprite calls that act on Sprite_Current: which sprite; the two that
// draw, the frame-offset table too.
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
// None free (0xFF) a quarter of the time, for MainCannon_Fire only.
std::uint32_t CreateEffect(const std::uint32_t*, std::uint32_t answer) {
    if (g_allow_none && mh::Noise() % 4 == 0) return answer | 0xFF;
    return answer;
}
std::uint32_t KeepFacing(const std::uint32_t*, std::uint32_t answer) {
    if (g_keep_facing) Sprite_Current[8] &= 3;
    return answer;
}
// The GTE stand-ins of the matrix pushes: a result from the inputs, written
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

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S31_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S31_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S31_OURS(BattleTask_Create), 2, {kU8, kU8}, mh::Answer::kByte, 0, mh::at::kTaskCount - 1, {}, &CreateEffect},
    {S31_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    // the sprite calls
    {S31_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    {S31_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S31_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S31_OURS(Sprite_QueueOverlay), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S31_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0},
    // the draw library (psx_gpu, psx_gte*, draw_emit, world_map, field_misc,
    // battle_items: all ours)
    {S31_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S31_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S31_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S31_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S31_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S31_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S31_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gpu_SetPolyGT4), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gpu_SetLineG2), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0, {}, &KeepFacing},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S31_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S31_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S31_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S31_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S31_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projections: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S31_OURS(Gte_RotTransPers), 4, {kAll, kAll, 0, 0}, kG, 0, 0, {6}},
    {S31_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S31_OURS(Gte_StoreDepthF), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    {S31_OURS(Gte_PrimDepths4_14), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction
    {S31_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // group S22's, called directly
    {S31_RAW(0x4CAE30), 0, {}, kG, 0, 0},
    // this group's own, called directly
    {S31_RAW(0x4E6DF0), 0, {}, kG, 0, 0},
    {S31_RAW(0x4E6EC0), 0, {}, kG, 0, 0},
    {S31_RAW(0x4E7130), 0, {}, kG, 0, 0},
    {S31_RAW(0x4E77D0), 0, {}, kG, 0, 0},
    {S31_RAW(0x4E78B0), 0, {}, kG, 0, 0},
    {S31_RAW(0x4E7E80), 0, {}, kG, 0, 0},
    {S31_RAW(0x4E88D0), 3, {kAll, kAll, kAll}, kG, 0, 0},
    {S31_RAW(0x4E8DA0), 0, {}, kG, 0, 0},
    {S31_RAW(0x4E8FA0), 0, {}, kG, 0, 0},
};
#undef S31_OURS
#undef S31_RAW

// The eight .data handler tables the dispatchers read in place
// (DoomBreathOrb_TaskTable and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65BD64, 1}, {0x65BD68, 5}, {0x65BD7C, 3}, {0x65BD88, 4},
    {0x65BD98, 4}, {0x65BDA8, 2}, {0x65BDB0, 1}, {0x65BDB4, 4},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850.., Scratch_Swap at +0xC
    {kKind2, 8},                          // Field_Kind2Z, Field_Kind2X
    {kFrameSet, 4},                       // the frame-offset table pointer the shells swap
    {0x7E0700, 0xC00},                    // MoveScript_TintRecords
    {0x80E980, 0x200},                    // Gfx_ClutStripSource row 26
    {0x812980, 0x200},                    // Gfx_ClutStrip row 26
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }

// The group's cells a recorder may move (the harness's case 14). The loop
// bounds the draws read back (word 0x903850 in CoronaRay_Draw, dword
// 0x90385C in ThunderClapBolt_DrawArcs) stay small, or a loop runs past the
// log.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: {
        const unsigned k = v % 16;
        if (k < 2) SetWord(mh::Mem(kScratch), (h >> 16) % 0x30);
        else if (k >= 0xC) SetLong(mh::Mem(kScratch + 0xC), static_cast<std::int32_t>((h >> 16) % 40));
        else mh::Mem(kScratch + k)[0] = Byte(h >> 24);
        break;
    }
    case 3: SetLong(mh::Mem(kKind2 + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 4: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 5: {
        static const unsigned kFields[] = {0x18, 0x1C, 0x20, 0x2E, 0x30};
        SetWord(Sc() + kFields[v % 5], h >> 16);
        break;
    }
    case 6: mh::SetPointer(kActorRecord, mh::SpriteRecord(v)); break;
    case 7: mh::Mem(0x7E0700 + v % 0xC00)[0] = Byte(h >> 24); break;
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
// The acting actor's record, as the originals index it (a party member at
// 0..2, else the enemy by index - 3).
unsigned char* ActorRecord() {
    const unsigned a = mh::Mem(mh::at::kActor)[0];
    return a <= 2 ? mh::PartyOf(static_cast<unsigned char>(a)) : mh::EnemyOf(static_cast<unsigned char>(a));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kActorRecord, mh::SpriteRecord(mh::Next()));
    g_allow_none = k == kMainCannon_Fire;
    g_keep_facing = k == kCoronaRay_PushMatrix;
    if (mh::Often()) sc[8] = Byte(mh::Next() % 5);
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kDoomBreath_Task: sc[1] = Byte(mh::Next() % 5); break;
    case kCorona_Task: case kMainCannon_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kThunderClap_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kDoomBreathOrb_Task: case kThunderClapBolt_Task: sc[1] = 0; break;
    case kCoronaChild_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kMainCannonChild_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kDoomBreathOrb_Run: sc[2] = Byte(mh::Next() % 5); break;
    case kCoronaRay_Run: case kCoronaFlash_Run: case kThunderClapBolt_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kCoronaEnemy_Task: case kMainCannonShell_Run: case kMainCannonBlast_Run: sc[2] = Byte(mh::Next() % 3); break;
    // the counters: at their thresholds
    case kDoomBreath_Brighten: Near(sc[9], 0xF); break;
    case kDoomBreath_Fade: case kCorona_Wait: case kDoomBreathOrb_Launch: case kDoomBreathOrb_Move:
    case kThunderClapBolt_Fade:
        Near(sc[9], 1);
        break;
    case kDoomBreath_End: if (mh::Half()) sc[0xB] = Byte(mh::Half() ? 0xFF : 0xFE); break;
    case kCorona_End: case kMainCannon_End: if (mh::Half()) sc[0xB] = Byte(mh::Half() ? 0 : 1); break;
    case kDoomBreathOrb_Grow: NearLong(sc + 0x1C, 0x7E - 1); break;
    case kDoomBreathOrb_Shrink: NearLong(sc + 0x1C, 0x22 - 1); break;
    case kDoomBreathOrb_End: NearLong(sc + 0x1C, 0); break;
    case kCoronaRay_Advance: Near(sc[9], 0x31); break;
    case kCoronaRay_PushMatrix: sc[8] = Byte(mh::Next() % 4); break;
    case kThunderClapBolt_Rise: Near(sc[0xA], 0xB); break;
    case kMainCannon_Fire:
        if (mh::Often()) Frame_Counter &= ~7u;
        Near(sc[9], 5);
        break;
    // Corona's third child: an event battle, the acting enemy's +0x100 0x29
    case kCorona_Start:
        if (mh::Often()) mh::Mem(kEventBattle)[0] = Byte(1 + mh::Next() % 0xFF);
        if (mh::Often()) {
            const unsigned a = mh::Mem(mh::at::kActor)[0];
            mh::Mem(mh::at::kEnemies + static_cast<std::uint32_t>((static_cast<int>(a) - 3) * 0x128) + 0x100)[0] = 0x29;
        }
        break;
    // the shell: its y one step either side of the actor's limit
    case kMainCannonShell_Fly:
        if (mh::Often()) {
            const unsigned a = mh::Mem(mh::at::kActor)[0];
            const int limit = static_cast<short>(Word(ActorRecord() + 0x30)) - (a <= 2 ? 0x14 : 0x28);
            SetLong(sc + 0x1C, static_cast<std::int32_t>(static_cast<std::uint32_t>(limit - 1 + static_cast<int>(mh::Next() % 3)) << 4));
        }
        break;
    // the draws: both sides of each branch, short loops
    case kDoomBreathOrb_DrawStream: case kDoomBreathOrb_DrawGlow:
        if (mh::Half()) sc[2] = 4;
        break;
    case kCoronaRay_Draw:
        if (mh::Half()) sc[2] = 3;
        sc[9] = Byte(mh::Often() ? mh::Next() % 0x30 : mh::Next());
        break;
    case kThunderClapBolt_DrawArcs: sc[0xA] = Byte(mh::Next() % 40); break;
    default: break;
    }
}

// ThunderClapBolt_DrawBand's three words: its callers' rows most of the time.
void Args(unsigned k, std::uint32_t* a) {
    if (k != kThunderClapBolt_DrawBand || !mh::Often()) return;
    static const std::uint32_t kRows[3][3] = {{0x30, 0x10, 3}, {0x20, 0x50, 0xF}, {0x18, 0x80, 0x1F}};
    const auto& row = kRows[mh::Next() % 3];
    a[0] = row[0];
    a[1] = row[1];
    a[2] = row[2];
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s31", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s31
