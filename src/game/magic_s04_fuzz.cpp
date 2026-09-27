// BOF3X_SHADOW=magic_s04: group S04's two overlays (MAGIC013, MAGIC015 with
// MAGIC016 folded) through the spell round's shared harness (magic_harness.h),
// once at start-up. docs/magic_s04.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC013 / MAGIC015 --clones
// (2026-09-26; capstone, every jump internal but the two four-entry jump
// tables of the wave matrices - cmp 3 before each dispatch, four entries each
// - which the harness moves into the copy; no REFUSED line), names given.
// Beyond the standard set this group lists the draw callees, the sprite
// calls, AreaMap_Elevation, Math_Ratan2, the engine's 0x446770, libgpu's
// unnamed SetPolyF3 and the functions of its own its functions call directly.
// Everything the harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer) and move
//     Gfx_PacketNext on through a packet buffer of the fuzz's own;
//   - the GTE callees of the matrix pushes log what their pointers point at
//     (`deref`) and write a result where the real ones write (group S31's);
//   - 0x446770 logs the task's direction and pair and writes a new pair;
//   - the sprite calls that act on Sprite_Current (the script tick, the steps,
//     the screen update) log which sprite;
//   - the kFlag blind spot (docs/takeover-queue-round9.md section 9): each
//     callee whose "no" is followed by a read of something a recorder moves -
//     Battle_ActorIsOut (the target, re-read), MagicFx_ApplyBuff
//     (Sprite_Current), Sprite_ScriptTickOnce - answers from its own stream
//     and a quarter of the time moves that cell itself; the two proximity
//     tests, whose callers test all of eax, answer a C bool the same way;
//   - Gte_PushMatrix keeps the facing byte inside the wave matrices' jump
//     tables while those two are fuzzed (past 3 the original turns by stack
//     garbage and ours aborts).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s04.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s04 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC013 / MAGIC015 --clones, 2026-09-26, names
// given.
// 0x49E9F0: 0x85 bytes  Snap_Task
constexpr mh::CallSite kCalls49E9F0[] = {{0x63, 0x49F620}};
constexpr mh::Imm kImms49E9F0[] = {{0x16, 0x49EA80}, {0x1E, 0x4EF7C0}, {0x26, 0x49EB40}, {0x2E, 0x4E5200}};
// 0x49EA80: 0xB7 bytes  Snap_Start
constexpr mh::CallSite kCalls49EA80[] = {{0x82, 0x435180}};
// 0x49EB40: 0x7A bytes  Snap_Buff
constexpr mh::CallSite kCalls49EB40[] = {{0x7, 0x4456C0}, {0x1C, 0x4FB6F0}, {0x2C, 0x435180}};
// 0x49EBC0: 0x12 bytes; jmp through .data 0x65A660 (SnapWave_TaskTable)  SnapWave_Task
// 0x49EBE0: 0x3C bytes; call through .data 0x65A664 (SnapWave_Steps)  SnapWave_Run
constexpr mh::CallSite kCalls49EBE0[] = {{0x1D, 0x49EEE0}, {0x22, 0x49F120}, {0x27, 0x5A7BC0}, {0x2C, 0x49F000}, {0x31, 0x49F3A0}, {0x36, 0x5A7BC0}};
// 0x49EC20: 0xC6 bytes  SnapWave_Start
constexpr mh::CallSite kCalls49EC20[] = {{0x30, 0x446770}, {0x8F, 0x446770}};
// 0x49ECF0: 0xF4 bytes  SnapWave_Burst
constexpr mh::CallSite kCalls49ECF0[] = {{0x42, 0x49FA10}, {0x7E, 0x5B93D2}, {0xAF, 0x452F70}, {0xD7, 0x446770}, {0xE1, 0x587900}};
// 0x49EDF0: 0xBC bytes  SnapWave_Swing
constexpr mh::CallSite kCalls49EDF0[] = {{0x68, 0x4530D0}, {0x72, 0x587900}, {0xAB, 0x446770}};
// 0x49EEB0: 0x2E bytes  SnapWave_End
constexpr mh::CallSite kCalls49EEB0[] = {{0x28, 0x4351F0}};
// 0x49EEE0: 0x118 bytes  SnapWave_PushMatrixA
constexpr mh::CallSite kCalls49EEE0[] = {{0x4, 0x5A7B90}, {0xC1, 0x5A8200}, {0xD0, 0x5A8060}, {0xE4, 0x5A7D70}, {0xEE, 0x5A8DE0}, {0xF8, 0x5A8E00}};
constexpr mh::JumpTable kTables49EEE0[] = {{0x1D, 0x108, 4}};
// 0x49F000: 0x114 bytes  SnapWave_PushMatrixB
constexpr mh::CallSite kCalls49F000[] = {{0x4, 0x5A7B90}, {0xBE, 0x5A8200}, {0xCD, 0x5A8060}, {0xE1, 0x5A7D70}, {0xEB, 0x5A8DE0}, {0xF5, 0x5A8E00}};
constexpr mh::JumpTable kTables49F000[] = {{0x1D, 0x104, 4}};
// 0x49F120: 0x27C bytes  SnapWave_DrawRingA
constexpr mh::CallSite kCalls49F120[] = {{0x19, 0x5A77C0}, {0x2F, 0x572FA0}, {0x4D, 0x5A7A00}, {0x7B, 0x5A7A00}, {0x92, 0x5A7A50}, {0xA5, 0x5A7A00}, {0xCB, 0x5A7610}, {0xD2, 0x5A7780}, {0xD8, 0x5A7A00}, {0x156, 0x5A7A00}, {0x16D, 0x5A7A50}, {0x187, 0x5A7A00}, {0x1F4, 0x5A85F0}, {0x1FD, 0x5A9350}, {0x259, 0x572FA0}};
// 0x49F3A0: 0x27C bytes  SnapWave_DrawRingB
constexpr mh::CallSite kCalls49F3A0[] = {{0x19, 0x5A77C0}, {0x2F, 0x572FA0}, {0x4D, 0x5A7A00}, {0x7B, 0x5A7A00}, {0x92, 0x5A7A50}, {0xA5, 0x5A7A00}, {0xCB, 0x5A7610}, {0xD2, 0x5A7780}, {0xD8, 0x5A7A00}, {0x156, 0x5A7A00}, {0x16D, 0x5A7A50}, {0x187, 0x5A7A00}, {0x1F4, 0x5A85F0}, {0x1FD, 0x5A9350}, {0x259, 0x572FA0}};
// 0x49F620: 0x12 bytes; jmp through .data 0x65A67C (SnapSpark_TaskTable)  SnapSpark_Task
// 0x49F640: 0x33 bytes; call through .data 0x65A680 (SnapSpark_Steps)  SnapSpark_Run
constexpr mh::CallSite kCalls49F640[] = {{0x23, 0x49F7F0}, {0x28, 0x49F8A0}, {0x2D, 0x5A7BC0}};
// 0x49F680: 0xB0 bytes  SnapSpark_Launch
constexpr mh::CallSite kCalls49F680[] = {{0x34, 0x5A7A00}, {0x53, 0x5A7A50}, {0x7A, 0x5B93D2}};
// 0x49F730: 0xBB bytes  SnapSpark_Fly
constexpr mh::CallSite kCalls49F730[] = {{0x18, 0x5A7A00}, {0x38, 0x5A7A50}};
// 0x49F7F0: 0xA7 bytes  SnapSpark_PushMatrix
constexpr mh::CallSite kCalls49F7F0[] = {{0x3, 0x5A7B90}, {0x67, 0x5A8200}, {0x76, 0x5A8060}, {0x8A, 0x5A7D70}, {0x94, 0x5A8DE0}, {0x9E, 0x5A8E00}};
// 0x49F8A0: 0x164 bytes  SnapSpark_Draw
constexpr mh::CallSite kCalls49F8A0[] = {{0x12, 0x5A77C0}, {0x28, 0x572FA0}, {0x34, 0x5A7570}, {0x3C, 0x5A7780}, {0x43, 0x5A7A00}, {0x56, 0x5A7A50}, {0x75, 0x5A7A00}, {0x8B, 0x5A7A50}, {0xAD, 0x5A7A00}, {0xC3, 0x5A7A50}, {0x102, 0x5A84A0}, {0x108, 0x5A91C0}, {0x10D, 0x5B93D2}, {0x120, 0x5B93D2}, {0x133, 0x5B93D2}, {0x157, 0x572FA0}};
// 0x49FA10: 0x57 bytes  SnapSpark_Alloc
// 0x49FA70: 0x36 bytes  Charge_Task
constexpr mh::Imm kImms49FA70[] = {{0xF, 0x49FAB0}, {0x17, 0x4A1EC0}, {0x22, 0x43FE80}, {0x2A, 0x43F460}};
// 0x49FAB0: 0x23E bytes  Charge_Start
constexpr mh::CallSite kCalls49FAB0[] = {{0x67, 0x4FB830}, {0x75, 0x435180}, {0x13F, 0x435180}, {0x1F8, 0x4FBF50}, {0x231, 0x4FBE30}};
// 0x49FCF0: 0x36 bytes  AirRaid_Task
constexpr mh::Imm kImms49FCF0[] = {{0xF, 0x49FD30}, {0x17, 0x4A1EC0}, {0x22, 0x43FE80}, {0x2A, 0x43F460}};
// 0x49FD30: 0x1AA bytes  AirRaid_Start
constexpr mh::CallSite kCalls49FD30[] = {{0x50, 0x4FBF50}, {0x89, 0x4FBE30}, {0x94, 0x4FBED0}, {0x9D, 0x4FB830}, {0xBA, 0x435180}, {0x187, 0x587900}};
// 0x49FEE0: 0x36 bytes  FlyingKick_Task
constexpr mh::Imm kImms49FEE0[] = {{0xF, 0x49FF20}, {0x17, 0x4A0090}, {0x22, 0x43FE80}, {0x2A, 0x43F460}};
// 0x49FF20: 0x16E bytes  FlyingKick_Start
constexpr mh::CallSite kCalls49FF20[] = {{0x56, 0x4FB830}, {0x5F, 0x435180}, {0x10A, 0x4FBF50}, {0x143, 0x4FBE30}, {0x14E, 0x4FBED0}};
// 0x4A0090: 0x2E bytes  FlyingKick_End
constexpr mh::CallSite kCalls4A0090[] = {{0xC, 0x4FC000}, {0x1D, 0x4FB830}};
// 0x4A00C0: 0x12 bytes; jmp through .data 0x65A688 (KickImage_Kinds)  KickImage_Task
// 0x4A00E0: 0x6C bytes  ChargeImage_Run
constexpr mh::CallSite kCalls4A00E0[] = {{0x63, 0x588F20}};
constexpr mh::Imm kImms4A00E0[] = {{0xF, 0x4A0150}, {0x17, 0x4A01C0}, {0x22, 0x4A01E0}, {0x2A, 0x4A02B0}, {0x32, 0x4A0330}, {0x3A, 0x4A0370}, {0x42, 0x4A0400}, {0x4A, 0x4AEE90}};
// 0x4A0150: 0x69 bytes  ChargeImage_Start
// 0x4A01C0: 0x12 bytes  KickImage_Tick
constexpr mh::CallSite kCalls4A01C0[] = {{0x0, 0x589410}};
// 0x4A01E0: 0xCA bytes  ChargeImage_Dash
constexpr mh::CallSite kCalls4A01E0[] = {{0x9, 0x4FB9F0}, {0x1A, 0x4FBC30}, {0x37, 0x4FC030}, {0x45, 0x587900}, {0x51, 0x4530D0}, {0x8B, 0x5A7A70}};
// 0x4A02B0: 0x79 bytes  KickImage_Arc
constexpr mh::CallSite kCalls4A02B0[] = {{0xD, 0x5A7A00}, {0x2A, 0x5A7A50}};
// 0x4A0330: 0x3E bytes  ChargeImage_Wait
// 0x4A0370: 0x85 bytes  ChargeImage_Squash
// 0x4A0400: 0x51 bytes  ChargeImage_Stretch
// 0x4A0460: 0x54 bytes  ChargeTrail_Run
constexpr mh::CallSite kCalls4A0460[] = {{0x4B, 0x588F20}};
constexpr mh::Imm kImms4A0460[] = {{0xF, 0x4A04C0}, {0x17, 0x4A01C0}, {0x22, 0x4A05A0}, {0x2A, 0x4A02B0}, {0x32, 0x4AF490}};
// 0x4A04C0: 0xD2 bytes  ChargeTrail_Start
// 0x4A05A0: 0x99 bytes  ChargeTrail_Dash
constexpr mh::CallSite kCalls4A05A0[] = {{0x9, 0x4FB9F0}, {0x1A, 0x4FBC30}, {0x5A, 0x5A7A70}};
// 0x4A0640: 0xA4 bytes  AirRaidImage_Run
constexpr mh::CallSite kCalls4A0640[] = {{0x5B, 0x588F20}, {0x7E, 0x4A0EF0}, {0x83, 0x4A1050}, {0x88, 0x5A7BC0}, {0x91, 0x4A0FA0}, {0x96, 0x4A1050}, {0x9B, 0x5A7BC0}};
constexpr mh::Imm kImms4A0640[] = {{0xF, 0x4A06F0}, {0x17, 0x4A07B0}, {0x22, 0x4A0870}, {0x2A, 0x4A0920}, {0x32, 0x4A09E0}, {0x3A, 0x4A0B10}, {0x42, 0x4A0BA0}};
// 0x4A06F0: 0xBF bytes  AirRaidImage_Start
// 0x4A07B0: 0xB6 bytes  AirRaidImage_Rise
constexpr mh::CallSite kCalls4A07B0[] = {{0xD, 0x4FBB40}, {0xA5, 0x4FBB40}};
// 0x4A0870: 0xA7 bytes  AirRaidImage_Turn
// 0x4A0920: 0xB2 bytes  AirRaidImage_Dive
constexpr mh::CallSite kCalls4A0920[] = {{0xD, 0x4FBB40}, {0x7F, 0x4530D0}, {0x94, 0x4FC030}, {0xA1, 0x587900}};
// 0x4A09E0: 0x129 bytes  AirRaidImage_Bounce
constexpr mh::CallSite kCalls4A09E0[] = {{0x8E, 0x589410}, {0xA0, 0x4FBB40}, {0xF2, 0x4351F0}};
// 0x4A0B10: 0x8A bytes  KickImage_Settle
constexpr mh::CallSite kCalls4A0B10[] = {{0x45, 0x589410}};
// 0x4A0BA0: 0x42 bytes  AirRaidImage_FadeOut
constexpr mh::CallSite kCalls4A0BA0[] = {{0x3C, 0x4351F0}};
// 0x4A0BF0: 0x9D bytes  FlyingKickImage_Run
constexpr mh::CallSite kCalls4A0BF0[] = {{0x5B, 0x588F20}, {0x77, 0x4A0EF0}, {0x7C, 0x4A1050}, {0x81, 0x5A7BC0}, {0x8A, 0x4A0FA0}, {0x8F, 0x4A1050}, {0x94, 0x5A7BC0}};
constexpr mh::Imm kImms4A0BF0[] = {{0xF, 0x4A0C90}, {0x17, 0x4A0CE0}, {0x22, 0x4A0D40}, {0x2A, 0x4A0DD0}, {0x32, 0x4A0B10}, {0x3A, 0x4A0EA0}, {0x42, 0x4AEE90}};
// 0x4A0C90: 0x47 bytes  FlyingKickImage_Start
// 0x4A0CE0: 0x5C bytes  FlyingKickImage_Rise
constexpr mh::CallSite kCalls4A0CE0[] = {{0xD, 0x4FBB40}};
// 0x4A0D40: 0x8D bytes  FlyingKickImage_Dive
constexpr mh::CallSite kCalls4A0D40[] = {{0x8, 0x4FB9F0}, {0x2E, 0x4FBBD0}, {0x5A, 0x4530D0}, {0x6F, 0x4FC030}, {0x7C, 0x587900}};
// 0x4A0DD0: 0xC7 bytes  FlyingKickImage_Bounce
constexpr mh::CallSite kCalls4A0DD0[] = {{0x0, 0x589410}, {0x12, 0x4FBB40}};
// 0x4A0EA0: 0x4A bytes  FlyingKickImage_FadeOut
// 0x4A0EF0: 0xAB bytes  KickImage_PushMatrixGround
constexpr mh::CallSite kCalls4A0EF0[] = {{0x3, 0x5A7B90}, {0x48, 0x5720C0}, {0x6B, 0x5A8200}, {0x7A, 0x5A8060}, {0x8E, 0x5A7D70}, {0x98, 0x5A8DE0}, {0xA2, 0x5A8E00}};
// 0x4A0FA0: 0xA4 bytes  KickImage_PushMatrixSource
constexpr mh::CallSite kCalls4A0FA0[] = {{0x3, 0x5A7B90}, {0x64, 0x5A8200}, {0x73, 0x5A8060}, {0x87, 0x5A7D70}, {0x91, 0x5A8DE0}, {0x9B, 0x5A8E00}};
// 0x4A1050: 0x189 bytes  KickImage_DrawShadow
constexpr mh::CallSite kCalls4A1050[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x3D, 0x5A7A00}, {0x56, 0x5A7A50}, {0x7D, 0x5A75F0}, {0x84, 0x5A7780}, {0xB2, 0x5A7A00}, {0xCB, 0x5A7A50}, {0x11D, 0x5A84A0}, {0x123, 0x5A9310}, {0x14A, 0x461E50}, {0x170, 0x5A77C0}, {0x179, 0x461E50}};

#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Snap_Task", 0x49E9F0, 0x85, kCalls49E9F0, MH_N(kCalls49E9F0), kImms49E9F0, MH_N(kImms49E9F0), nullptr, 0, reinterpret_cast<const void*>(&::Snap_Task)},
    {"Snap_Start", 0x49EA80, 0xB7, kCalls49EA80, MH_N(kCalls49EA80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Snap_Start)},
    {"Snap_Buff", 0x49EB40, 0x7A, kCalls49EB40, MH_N(kCalls49EB40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Snap_Buff)},
    {"SnapWave_Task", 0x49EBC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapWave_Task)},
    {"SnapWave_Run", 0x49EBE0, 0x3C, kCalls49EBE0, MH_N(kCalls49EBE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapWave_Run)},
    {"SnapWave_Start", 0x49EC20, 0xC6, kCalls49EC20, MH_N(kCalls49EC20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapWave_Start)},
    {"SnapWave_Burst", 0x49ECF0, 0xF4, kCalls49ECF0, MH_N(kCalls49ECF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapWave_Burst)},
    {"SnapWave_Swing", 0x49EDF0, 0xBC, kCalls49EDF0, MH_N(kCalls49EDF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapWave_Swing)},
    {"SnapWave_End", 0x49EEB0, 0x2E, kCalls49EEB0, MH_N(kCalls49EEB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapWave_End)},
    {"SnapWave_PushMatrixA", 0x49EEE0, 0x118, kCalls49EEE0, MH_N(kCalls49EEE0), nullptr, 0, kTables49EEE0, MH_N(kTables49EEE0), reinterpret_cast<const void*>(&::SnapWave_PushMatrixA)},
    {"SnapWave_PushMatrixB", 0x49F000, 0x114, kCalls49F000, MH_N(kCalls49F000), nullptr, 0, kTables49F000, MH_N(kTables49F000), reinterpret_cast<const void*>(&::SnapWave_PushMatrixB)},
    {"SnapWave_DrawRingA", 0x49F120, 0x27C, kCalls49F120, MH_N(kCalls49F120), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapWave_DrawRingA)},
    {"SnapWave_DrawRingB", 0x49F3A0, 0x27C, kCalls49F3A0, MH_N(kCalls49F3A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapWave_DrawRingB)},
    {"SnapSpark_Task", 0x49F620, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapSpark_Task)},
    {"SnapSpark_Run", 0x49F640, 0x33, kCalls49F640, MH_N(kCalls49F640), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapSpark_Run)},
    {"SnapSpark_Launch", 0x49F680, 0xB0, kCalls49F680, MH_N(kCalls49F680), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapSpark_Launch)},
    {"SnapSpark_Fly", 0x49F730, 0xBB, kCalls49F730, MH_N(kCalls49F730), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapSpark_Fly)},
    {"SnapSpark_PushMatrix", 0x49F7F0, 0xA7, kCalls49F7F0, MH_N(kCalls49F7F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapSpark_PushMatrix)},
    {"SnapSpark_Draw", 0x49F8A0, 0x164, kCalls49F8A0, MH_N(kCalls49F8A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapSpark_Draw)},
    {"SnapSpark_Alloc", 0x49FA10, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SnapSpark_Alloc), 0xFF},
    {"Charge_Task", 0x49FA70, 0x36, nullptr, 0, kImms49FA70, MH_N(kImms49FA70), nullptr, 0, reinterpret_cast<const void*>(&::Charge_Task)},
    {"Charge_Start", 0x49FAB0, 0x23E, kCalls49FAB0, MH_N(kCalls49FAB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Charge_Start)},
    {"AirRaid_Task", 0x49FCF0, 0x36, nullptr, 0, kImms49FCF0, MH_N(kImms49FCF0), nullptr, 0, reinterpret_cast<const void*>(&::AirRaid_Task)},
    {"AirRaid_Start", 0x49FD30, 0x1AA, kCalls49FD30, MH_N(kCalls49FD30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AirRaid_Start)},
    {"FlyingKick_Task", 0x49FEE0, 0x36, nullptr, 0, kImms49FEE0, MH_N(kImms49FEE0), nullptr, 0, reinterpret_cast<const void*>(&::FlyingKick_Task)},
    {"FlyingKick_Start", 0x49FF20, 0x16E, kCalls49FF20, MH_N(kCalls49FF20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlyingKick_Start)},
    {"FlyingKick_End", 0x4A0090, 0x2E, kCalls4A0090, MH_N(kCalls4A0090), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlyingKick_End)},
    {"KickImage_Task", 0x4A00C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KickImage_Task)},
    {"ChargeImage_Run", 0x4A00E0, 0x6C, kCalls4A00E0, MH_N(kCalls4A00E0), kImms4A00E0, MH_N(kImms4A00E0), nullptr, 0, reinterpret_cast<const void*>(&::ChargeImage_Run)},
    {"ChargeImage_Start", 0x4A0150, 0x69, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChargeImage_Start)},
    {"KickImage_Tick", 0x4A01C0, 0x12, kCalls4A01C0, MH_N(kCalls4A01C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KickImage_Tick)},
    {"ChargeImage_Dash", 0x4A01E0, 0xCA, kCalls4A01E0, MH_N(kCalls4A01E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChargeImage_Dash)},
    {"KickImage_Arc", 0x4A02B0, 0x79, kCalls4A02B0, MH_N(kCalls4A02B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KickImage_Arc)},
    {"ChargeImage_Wait", 0x4A0330, 0x3E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChargeImage_Wait)},
    {"ChargeImage_Squash", 0x4A0370, 0x85, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChargeImage_Squash)},
    {"ChargeImage_Stretch", 0x4A0400, 0x51, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChargeImage_Stretch)},
    {"ChargeTrail_Run", 0x4A0460, 0x54, kCalls4A0460, MH_N(kCalls4A0460), kImms4A0460, MH_N(kImms4A0460), nullptr, 0, reinterpret_cast<const void*>(&::ChargeTrail_Run)},
    {"ChargeTrail_Start", 0x4A04C0, 0xD2, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChargeTrail_Start)},
    {"ChargeTrail_Dash", 0x4A05A0, 0x99, kCalls4A05A0, MH_N(kCalls4A05A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ChargeTrail_Dash)},
    {"AirRaidImage_Run", 0x4A0640, 0xA4, kCalls4A0640, MH_N(kCalls4A0640), kImms4A0640, MH_N(kImms4A0640), nullptr, 0, reinterpret_cast<const void*>(&::AirRaidImage_Run)},
    {"AirRaidImage_Start", 0x4A06F0, 0xBF, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AirRaidImage_Start)},
    {"AirRaidImage_Rise", 0x4A07B0, 0xB6, kCalls4A07B0, MH_N(kCalls4A07B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AirRaidImage_Rise)},
    {"AirRaidImage_Turn", 0x4A0870, 0xA7, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AirRaidImage_Turn)},
    {"AirRaidImage_Dive", 0x4A0920, 0xB2, kCalls4A0920, MH_N(kCalls4A0920), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AirRaidImage_Dive)},
    {"AirRaidImage_Bounce", 0x4A09E0, 0x129, kCalls4A09E0, MH_N(kCalls4A09E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AirRaidImage_Bounce)},
    {"KickImage_Settle", 0x4A0B10, 0x8A, kCalls4A0B10, MH_N(kCalls4A0B10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KickImage_Settle)},
    {"AirRaidImage_FadeOut", 0x4A0BA0, 0x42, kCalls4A0BA0, MH_N(kCalls4A0BA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::AirRaidImage_FadeOut)},
    {"FlyingKickImage_Run", 0x4A0BF0, 0x9D, kCalls4A0BF0, MH_N(kCalls4A0BF0), kImms4A0BF0, MH_N(kImms4A0BF0), nullptr, 0, reinterpret_cast<const void*>(&::FlyingKickImage_Run)},
    {"FlyingKickImage_Start", 0x4A0C90, 0x47, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlyingKickImage_Start)},
    {"FlyingKickImage_Rise", 0x4A0CE0, 0x5C, kCalls4A0CE0, MH_N(kCalls4A0CE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlyingKickImage_Rise)},
    {"FlyingKickImage_Dive", 0x4A0D40, 0x8D, kCalls4A0D40, MH_N(kCalls4A0D40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlyingKickImage_Dive)},
    {"FlyingKickImage_Bounce", 0x4A0DD0, 0xC7, kCalls4A0DD0, MH_N(kCalls4A0DD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlyingKickImage_Bounce)},
    {"FlyingKickImage_FadeOut", 0x4A0EA0, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlyingKickImage_FadeOut)},
    {"KickImage_PushMatrixGround", 0x4A0EF0, 0xAB, kCalls4A0EF0, MH_N(kCalls4A0EF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KickImage_PushMatrixGround)},
    {"KickImage_PushMatrixSource", 0x4A0FA0, 0xA4, kCalls4A0FA0, MH_N(kCalls4A0FA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KickImage_PushMatrixSource)},
    {"KickImage_DrawShadow", 0x4A1050, 0x189, kCalls4A1050, MH_N(kCalls4A1050), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KickImage_DrawShadow)},
};
#undef MH_N

enum : unsigned {
    kSnap_Task, kSnap_Start, kSnap_Buff, kSnapWave_Task, kSnapWave_Run, kSnapWave_Start, kSnapWave_Burst,
    kSnapWave_Swing, kSnapWave_End, kSnapWave_PushMatrixA, kSnapWave_PushMatrixB, kSnapWave_DrawRingA,
    kSnapWave_DrawRingB, kSnapSpark_Task, kSnapSpark_Run, kSnapSpark_Launch, kSnapSpark_Fly, kSnapSpark_PushMatrix,
    kSnapSpark_Draw, kSnapSpark_Alloc,
    kCharge_Task, kCharge_Start, kAirRaid_Task, kAirRaid_Start, kFlyingKick_Task, kFlyingKick_Start, kFlyingKick_End,
    kKickImage_Task, kChargeImage_Run, kChargeImage_Start, kKickImage_Tick, kChargeImage_Dash, kKickImage_Arc,
    kChargeImage_Wait, kChargeImage_Squash, kChargeImage_Stretch, kChargeTrail_Run, kChargeTrail_Start,
    kChargeTrail_Dash, kAirRaidImage_Run, kAirRaidImage_Start, kAirRaidImage_Rise, kAirRaidImage_Turn,
    kAirRaidImage_Dive, kAirRaidImage_Bounce, kKickImage_Settle, kAirRaidImage_FadeOut, kFlyingKickImage_Run,
    kFlyingKickImage_Start, kFlyingKickImage_Rise, kFlyingKickImage_Dive, kFlyingKickImage_Bounce,
    kFlyingKickImage_FadeOut, kKickImage_PushMatrixGround, kKickImage_PushMatrixSource, kKickImage_DrawShadow, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kActorSprite = 0x904B3C, kPool = 0x677A40,
                        kPoolStride = 0x84;

unsigned char* Sc() { return Sprite_Current; }
unsigned char* PoolRecord(unsigned i) { return mh::Mem(kPool + (i & 63) * kPoolStride); }
// A real slot or record for an owner cell the recorders write through.
const void* SomeOwner(std::uint32_t v) {
    return (v & 4) ? static_cast<const void*>(mh::SpriteRecord(v & 1)) : static_cast<const void*>(mh::TaskAt(v & 3));
}

// Set by the seed for the functions each matters to.
bool g_keep_facing = false;   // the facing stays inside 0..3 (the wave matrices)

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
// The sprite calls that act on Sprite_Current: which sprite.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
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
std::uint32_t KeepFacing(const std::uint32_t*, std::uint32_t answer) {
    if (g_keep_facing) Sprite_Current[8] &= 3;
    return answer;
}

// The kFlag blind spot: a kFlag recorder answers 0 exactly when its own
// disturbance did nothing (both from one hash), so a read after a "no" never
// sees a moved cell. These answer from Noise instead, and a quarter of the time
// move the cell their callers read after a "no".
std::uint32_t FlagFrom(std::uint32_t h, std::uint32_t answer) {
    return (h >> 4) % 3 == 0 ? answer & 0xFFFFFF00u : answer | 0x10;
}
// Battle_ActorIsOut: Snap_Buff reads the target again after a "no".
std::uint32_t OutEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t h = mh::Noise();
    if (h % 4 == 0) mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>((h >> 8) % 11);
    return FlagFrom(h, answer);
}
// MagicFx_ApplyBuff, Sprite_ScriptTickOnce: Sprite_Current is read after a
// "no" (Snap_Buff's +1; the ticks' callers).
std::uint32_t TaskFlagEffect(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t h = mh::Noise();
    if (h % 4 == 0) Sprite_Current = mh::TaskAt(h >> 8);
    mh::Note(Key(Sprite_Current));
    return FlagFrom(h, answer);
}
// MagicFx_NearSprite / _NearSprite3D: their callers test all of eax.
std::uint32_t NearEffect(const std::uint32_t*, std::uint32_t) {
    const std::uint32_t h = mh::Noise();
    if (h % 4 == 0) Sprite_Current = mh::TaskAt(h >> 8);
    mh::Note(Key(Sprite_Current));
    return (h >> 4) % 3 == 0 ? 0u : 1u;
}
// The GTE stand-ins of the matrix pushes: a result from the inputs, written
// where the real callee writes (group S22's, as S31 lists them).
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
constexpr mh::Answer kG = mh::Answer::kGarbage;
constexpr mh::Answer kP = mh::Answer::kPhase;
#define S04_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S04_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S04_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S04_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &TaskFlagEffect},
    {S04_OURS(MagicFx_ApplyBuff), 2, {kU8, kU8}, mh::Answer::kFlag, 0, 0, {}, &TaskFlagEffect},
    {S04_OURS(MagicFx_NearSprite), 2, {kAll, kAll}, mh::Answer::kBool, 0, 0, {}, &NearEffect},
    {S04_OURS(MagicFx_NearSprite3D), 2, {kAll, kAll}, mh::Answer::kBool, 0, 0, {}, &NearEffect},
    {S04_OURS(MagicFx_StepToward), 2, {kAll, kU16}, kG, 0, 0, {}, &NoteSprite},
    {S04_OURS(MagicFx_StepAround), 3, {kAll, kAll, kAll}, kG, 0, 0, {}, &NoteSprite},
    {S04_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &OutEffect},
    {S04_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0},
    {S04_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0},
    // the draw library (all ours)
    {S04_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S04_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S04_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S04_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S04_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S04_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S04_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S04_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S04_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0, {}, &KeepFacing},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S04_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S04_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S04_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S04_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S04_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projections: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S04_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S04_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S04_OURS(Gte_PrimDepths3_0C), 1, {kAll}, kG, 0, 0},
    {S04_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S04_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction; libgpu's SetPolyF3
    {S04_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    {S04_RAW(0x5A7570), 1, {kAll}, kG, 0, 0},
    // this group's own, called directly
    {S04_RAW(0x49EEE0), 0, {}, kP, 0, 0},
    {S04_RAW(0x49F000), 0, {}, kP, 0, 0},
    {S04_RAW(0x49F120), 0, {}, kP, 0, 0},
    {S04_RAW(0x49F3A0), 0, {}, kP, 0, 0},
    {S04_RAW(0x49F620), 0, {}, kP, 0, 0},
    {S04_RAW(0x49F7F0), 0, {}, kP, 0, 0},
    {S04_RAW(0x49F8A0), 0, {}, kP, 0, 0},
    {S04_RAW(0x4A0EF0), 0, {}, kP, 0, 0},
    {S04_RAW(0x4A0FA0), 0, {}, kP, 0, 0},
    {S04_RAW(0x4A1050), 0, {}, kP, 0, 0},
    // SnapSpark_Alloc: a pool index 0..0x3F, or 0xFF (none free)
    {S04_RAW(0x49FA10), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},
};
#undef S04_OURS
#undef S04_RAW

// The five .data handler tables the dispatchers read in place (symbols.toml),
// each cell once.
const mh::DataTable kTables[] = {
    {0x65A660, 1}, {0x65A664, 5}, {0x65A67C, 1}, {0x65A680, 2}, {0x65A688, 4},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850..
    {kPool, 64 * kPoolStride},            // SnapSpark_Pool
    {0x80E980, 0x40},                     // Gfx_ClutStripSource 0x1A00..0x1A1F
    {0x812980, 0x40},                     // Gfx_ClutStrip 0x1A00..0x1A1F
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }

// The group's cells a recorder may move (the harness's case 14): the packet
// pointer, a vertex or scratch word, a pool record's live bit or owner, the
// acting actor's sprite, the task's rise / fall / scale / shade fields, the
// owner's count +0xB near 1.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 16); break;
    case 3: PoolRecord(v)[0] ^= 1; break;
    case 4: mh::SetPointer(kPool + (v & 63) * kPoolStride + 0x80, SomeOwner(h >> 24)); break;
    case 5: mh::SetPointer(kActorSprite, mh::SpriteRecord(v)); break;
    case 6: {
        static const unsigned kFields[] = {0x14, 0x20, 0x3E, 0x40, 0x44, 0x5D, 0x5E, 0x5F};
        const unsigned f = kFields[v % 8];
        if (f >= 0x5D) Sc()[f] = Byte(h >> 24);
        else SetWord(Sc() + f, h >> 16);
        break;
    }
    case 7: mh::Pointer(mh::at::kOwner)[0xB] = Byte(v % 3); break;
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}
void Sometimes(unsigned char& b, unsigned value) {
    if (mh::Half()) b = Byte(value);
}

void FillPool() {
    const unsigned mode = mh::Next() % 4;
    const unsigned taken = mh::Next() % 65;
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (mode == 0) rec[0] = Byte(rec[0] | 1);                     // full
        else if (mode == 1) rec[0] = Byte(i < taken ? rec[0] | 1 : rec[0] & ~1u);
        mh::SetPointer(kPool + i * kPoolStride + 0x80, SomeOwner(mh::Next()));
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    unsigned char* const owner = mh::Pointer(mh::at::kOwner);
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kActorSprite, mh::SpriteRecord(mh::Next()));
    FillPool();
    g_keep_facing = k == kSnapWave_PushMatrixA || k == kSnapWave_PushMatrixB;
    if (mh::Often()) sc[8] = Byte(mh::Next() % 5);
    if (mh::Half()) sc[0] = 0;
    if (mh::Half()) owner[0xB] = Byte(mh::Next() % 3);
    switch (k) {
    // the dispatchers: inside their tables
    case kSnap_Task: case kCharge_Task: case kAirRaid_Task: case kFlyingKick_Task: case kKickImage_Task:
        sc[1] = Byte(mh::Next() % 4);
        break;
    case kSnapWave_Task: case kSnapSpark_Task: sc[1] = 0; break;
    case kSnapWave_Run: sc[2] = Byte(mh::Next() % 5); break;
    case kSnapSpark_Run:
        sc[2] = Byte(mh::Next() % 2);
        Sometimes(sc[9], 0);
        break;
    case kChargeImage_Run: sc[2] = Byte(mh::Next() % 8); break;
    case kChargeTrail_Run: sc[2] = Byte(mh::Next() % 5); break;
    case kAirRaidImage_Run: case kFlyingKickImage_Run:
        sc[2] = Byte(mh::Next() % 7);
        Sometimes(sc[9], 0);
        Sometimes(sc[0xB], 0);
        break;
    // the counters: at their thresholds
    case kSnapWave_Burst:
        if (mh::Half()) sc[9] = Byte(mh::Half() ? 4 : 8);
        Sometimes(sc[0xA], 4);
        break;
    case kSnapWave_Swing:
        sc[0xA] = Byte(mh::Next() % 4);
        if (mh::Often()) sc[9] = Byte((sc[0xA] == 1 ? 0xC : 0x10) - 2 - (mh::Half() ? 1 : 0));
        break;
    case kSnapWave_End:
        Sometimes(sc[0xB], 0);
        Near(sc[4], 1);
        break;
    case kSnapWave_PushMatrixA: case kSnapWave_PushMatrixB: sc[8] = Byte(mh::Next() % 4); break;
    case kSnapSpark_Launch: case kSnapSpark_Fly: case kChargeImage_Squash: case kChargeImage_Stretch:
    case kChargeTrail_Start: case kAirRaidImage_Start: case kAirRaidImage_Rise: case kKickImage_Arc:
        Near(sc[9], 1);
        if (k == kAirRaidImage_Start || k == kChargeTrail_Start) Sometimes(sc[0xB], 0);
        break;
    case kChargeImage_Wait: Sometimes(owner[0xB], 1); break;
    case kAirRaidImage_Turn:
        Near(sc[9], 3);
        Sometimes(sc[0xB], 0);
        break;
    case kAirRaidImage_Dive:
        Near(sc[9], 0x13);
        Sometimes(sc[0xB], 0);
        break;
    case kAirRaidImage_Bounce: case kFlyingKickImage_Bounce: {
        sc[0xB] = Byte(mh::Next() % 4);
        if (mh::Half()) sc[0x5D] = Byte(MH_PICK(0x80, 0, 3, 0x90));
        if (mh::Half()) sc[9] = Byte(0x13 + mh::Next() % 3);
        // the fall: +0x14 + +0x20 either side of 0, the height either side of the source's
        const std::int32_t rise = static_cast<std::int32_t>(mh::Next() % 5) - 2;
        if (mh::Often()) {
            SetLong(sc + 0x14, rise - 8);
            SetLong(sc + 0x20, 8 - (mh::Next() % 3));
        }
        // the height either side of the source's after the rise is added to
        // its high word (+0x3E): the compare sees +0x3C once it has moved
        if (mh::Often()) {
            unsigned char* const src = mh::Pointer(mh::at::kSource);
            const std::uint32_t moved = (static_cast<std::uint32_t>(Long(sc + 0x14)) + static_cast<std::uint32_t>(Long(sc + 0x20))) << 16;
            const std::uint32_t at = static_cast<std::uint32_t>(Long(src + 0x3C)) + mh::Next() % 3 - 1;
            SetLong(sc + 0x3C, static_cast<std::int32_t>(at - moved));
        }
        if (mh::Half()) Frame_Counter &= ~1u;
        break;
    }
    case kKickImage_Settle:
        if (mh::Half()) sc[0x5D] = Byte(mh::Half() ? 0x80 : 0x90);
        Sometimes(owner[0xB], 1);
        break;
    case kAirRaidImage_FadeOut: case kFlyingKickImage_FadeOut: Near(sc[0x5D], 0xAF); break;
    case kFlyingKickImage_Rise: Near(sc[0xA], 1); break;
    case kFlyingKickImage_Dive: Near(sc[9], 0x13); break;
    case kFlyingKick_End: Sometimes(sc[0xB], 0); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s04", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    mh::Run(group);
}

}  // namespace magic_s04
