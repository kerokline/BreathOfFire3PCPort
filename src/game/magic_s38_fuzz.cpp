// BOF3X_SHADOW=magic_s38: group S38's three overlays (MAGIC223, MAGIC225,
// MAGIC226/227) through the spell round's shared harness (magic_harness.h),
// once at start-up. docs/magic_s38.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC223 / MAGIC225 /
// MAGIC226/MAGIC227 --clones (2026-09-27; capstone, every jump internal, no
// jump table, no REFUSED line), names given. Beyond the standard set this
// group lists the draw callees (the GTE and libgpu entry points, Math_Sin /
// Math_Cos / Math_Ratan2, Gfx_CommitPrim, MapView_LinkPrimAt), the sprite
// calls, Battle_ActorIsOut, the engine's 0x446770, MAGIC219's 0x4F6290 and
// 0x4F6020, MAGIC053's LavaburstChild_DrawGlow and the functions of its own
// its functions call directly. Everything the harness lacks is built here, not
// in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - Math_Sin, Math_Cos and Rand move a scratch or vertex word a third of the
//     time, since the callers read their radii, angles and colours back from
//     them after each call;
//   - the calls that act on Sprite_Current (the script tick, the animation,
//     the screen point, the side's centre, the matrix pushes, MAGIC219's free)
//     log which sprite; the screen update the frame-offset table 0x9039D8 the
//     veil, shards and rock swap round their steps too;
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what the caller reads back is compared;
//   - BattleTask_Create answers 0xFF (none free) a quarter of the time, but
//     only while Magic225_Apply is fuzzed, the one caller that tests it (the
//     others write through the slot unchecked, a fault on both sides);
//   - the three pools are regions, their records' owners real slots or
//     records (the walks make them the owner cell, which recorders write
//     through).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s38.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s38 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC223 / MAGIC225 / MAGIC226/MAGIC227 --clones,
// 2026-09-27, names given; ret_mask 0xFF on the three pool allocators (an index
// in al).
// 0x4F8640: 0x7D bytes  Tempest_Task
constexpr mh::CallSite kCalls4F8640[] = {{0x5B, 0x4F89E0}};
constexpr mh::Imm kImms4F8640[] = {{0x16, 0x4F86C0}, {0x1E, 0x4F9F70}, {0x26, 0x4F8830}};
// 0x4F86C0: 0x162 bytes  Tempest_Start
constexpr mh::CallSite kCalls4F86C0[] = {{0x81, 0x435180}, {0xB5, 0x4F8EE0}, {0xE9, 0x5B93D2}, {0x15B, 0x587900}};
// 0x4F8830: 0x2A bytes  Tempest_End
constexpr mh::CallSite kCalls4F8830[] = {{0x1C, 0x4530D0}, {0x24, 0x4351F0}};
// 0x4F8860: 0x12 bytes; +0xB note: jmp through .data 0x65c2e4, 1 code entries (a data_tables entry)  TempestFlash_Task
// 0x4F8880: 0x23 bytes; +0xB note: call through .data 0x65c2e8, 4 code entries (a data_tables entry)  TempestFlash_Run
constexpr mh::CallSite kCalls4F8880[] = {{0x1D, 0x4F88B0}};
// 0x4F88B0: 0x12F bytes  TempestFlash_Draw
constexpr mh::CallSite kCalls4F88B0[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x25, 0x5A7610}, {0x2D, 0x5A7780}, {0x109, 0x461E50}, {0x11B, 0x5A77C0}, {0x124, 0x461E50}};
// 0x4F89E0: 0x12 bytes; +0xB note: jmp through .data 0x65c2f8, 1 code entries (a data_tables entry)  TempestGust_Task
// 0x4F8A00: 0x29 bytes; +0xB note: call through .data 0x65c2fc, 3 code entries (a data_tables entry)  TempestGust_Run
constexpr mh::CallSite kCalls4F8A00[] = {{0x23, 0x4F8C30}};
// 0x4F8A30: 0x119 bytes  TempestGust_Launch
constexpr mh::CallSite kCalls4F8A30[] = {{0x5D, 0x4FBD10}, {0x93, 0x5A7A00}, {0xB4, 0x5A7A50}};
// 0x4F8B50: 0x63 bytes  TempestGust_Drift
constexpr mh::CallSite kCalls4F8B50[] = {{0xD, 0x5A7A00}, {0x27, 0x5A7A50}};
// 0x4F8BC0: 0x66 bytes  TempestGust_Fly
constexpr mh::CallSite kCalls4F8BC0[] = {{0xD, 0x5A7A00}, {0x2A, 0x5A7A50}, {0x60, 0x4F6290}};
// 0x4F8C30: 0x2A1 bytes  TempestGust_Draw
constexpr mh::CallSite kCalls4F8C30[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x43, 0x5A75D0}, {0x4B, 0x5A7780}, {0x63, 0x5A7A00}, {0x8F, 0x5A7A50}, {0xC8, 0x5A7A00}, {0xF4, 0x5A7A50}, {0x12D, 0x5A7A00}, {0x159, 0x5A7A50}, {0x195, 0x5A7A00}, {0x1C1, 0x5A7A50}, {0x1F5, 0x5A79A0}, {0x20F, 0x5A79E0}, {0x296, 0x461E50}};
// 0x4F8EE0: 0x57 bytes  Tempest_GustAlloc
// 0x4F8F40: 0x85 bytes  Magic225_Task
constexpr mh::CallSite kCalls4F8F40[] = {{0x63, 0x4F9A40}};
constexpr mh::Imm kImms4F8F40[] = {{0x16, 0x4F8FD0}, {0x1E, 0x4F9090}, {0x26, 0x4F9130}, {0x2E, 0x4F7350}};
// 0x4F8FD0: 0xB1 bytes  Magic225_Start
constexpr mh::CallSite kCalls4F8FD0[] = {{0x1D, 0x4FC0E0}, {0x40, 0x435180}, {0xAA, 0x587900}};
// 0x4F9090: 0x97 bytes  Magic225_Spawn
constexpr mh::CallSite kCalls4F9090[] = {{0x29, 0x587900}, {0x31, 0x4F9D80}};
// 0x4F9130: 0x269 bytes  Magic225_Apply
constexpr mh::CallSite kCalls4F9130[] = {{0x2A, 0x4456C0}, {0x51, 0x4FB6F0}, {0x61, 0x435180}, {0xA4, 0x435180}, {0x151, 0x4456C0}, {0x174, 0x4FB6F0}, {0x184, 0x435180}, {0x1C7, 0x435180}};
// 0x4F93A0: 0x12 bytes; +0xB note: jmp through .data 0x65c318, 1 code entries (a data_tables entry)  Magic225Veil_Task
// 0x4F93C0: 0x83 bytes; +0x15 note: call through .data 0x65c328, 5 code entries (a data_tables entry)  Magic225Veil_Run
constexpr mh::CallSite kCalls4F93C0[] = {{0x2D, 0x4F9980}, {0x3A, 0x4F9610}, {0x4D, 0x4F9610}, {0x5D, 0x4F9610}, {0x70, 0x4F9610}};
// 0x4F9450: 0xB7 bytes  Magic225Veil_Start
constexpr mh::CallSite kCalls4F9450[] = {{0x22, 0x4456C0}, {0x50, 0x4456C0}};
// 0x4F9510: 0x26 bytes  Magic225Veil_FadeIn
// 0x4F9540: 0x1F bytes  Magic225Veil_WaitBuffs
// 0x4F9560: 0xAE bytes  Magic225Veil_End
constexpr mh::CallSite kCalls4F9560[] = {{0x4F, 0x4456C0}, {0x7D, 0x4456C0}, {0xA5, 0x4351F0}};
// 0x4F9610: 0x368 bytes  Magic225Veil_DrawBurst
constexpr mh::CallSite kCalls4F9610[] = {{0x12, 0x5A77C0}, {0x23, 0x461E50}, {0xD8, 0x5A75F0}, {0xE0, 0x5A7780}, {0xF4, 0x5A7A00}, {0x111, 0x5A7A50}, {0x134, 0x5A7A00}, {0x151, 0x5A7A50}, {0x1C6, 0x461E50}, {0x1EE, 0x5A7610}, {0x1F6, 0x5A7780}, {0x1FC, 0x5A7A00}, {0x219, 0x5A7A50}, {0x240, 0x5A7A00}, {0x261, 0x5A7A50}, {0x27E, 0x5A7A00}, {0x29B, 0x5A7A50}, {0x2BC, 0x5A7A00}, {0x2D9, 0x5A7A50}, {0x34C, 0x461E50}};
// 0x4F9980: 0xBD bytes  Magic225Veil_DrawShade
constexpr mh::CallSite kCalls4F9980[] = {{0x21, 0x5A77C0}, {0x31, 0x461E50}, {0x3D, 0x5A75B0}, {0x4E, 0x5A7780}, {0xB2, 0x461E50}};
// 0x4F9A40: 0x12 bytes; +0xB note: jmp through .data 0x65c33c, 1 code entries (a data_tables entry)  Magic225Shard_Task
// 0x4F9A60: 0x3D bytes; +0x15 note: call through .data 0x65c340, 3 code entries (a data_tables entry)  Magic225Shard_Run
constexpr mh::CallSite kCalls4F9A60[] = {{0x2D, 0x588F20}};
// 0x4F9AA0: 0x1B8 bytes  Magic225Shard_Launch
constexpr mh::CallSite kCalls4F9AA0[] = {{0x48, 0x446770}, {0xAE, 0x5A7A70}, {0xCF, 0x5B93D2}, {0xDF, 0x5B93D2}, {0x18E, 0x5891F0}};
// 0x4F9C60: 0x8A bytes  Magic225Shard_Fly
constexpr mh::CallSite kCalls4F9C60[] = {{0x1, 0x5893A0}, {0x31, 0x5A7A00}, {0x52, 0x5A7A50}};
// 0x4F9CF0: 0x85 bytes  Magic225Shard_Fall
constexpr mh::CallSite kCalls4F9CF0[] = {{0x1, 0x5893A0}, {0x31, 0x5A7A00}, {0x4B, 0x5A7A50}, {0x7F, 0x4F6290}};
// 0x4F9D80: 0x57 bytes  Magic225_ShardAlloc
// 0x4F9DE0: 0x7D bytes  MeteorStrike_Task
constexpr mh::CallSite kCalls4F9DE0[] = {{0x5B, 0x4FA670}};
constexpr mh::Imm kImms4F9DE0[] = {{0x16, 0x4F9E60}, {0x1E, 0x4F9F70}, {0x26, 0x4F7350}};
// 0x4F9E60: 0x10B bytes  MeteorStrike_Start
constexpr mh::CallSite kCalls4F9E60[] = {{0x1C, 0x4FC0E0}, {0x21, 0x4FBD10}, {0x78, 0x435180}, {0xA6, 0x587900}};
// 0x4F9F70: 0x33 bytes  MagicFx_CountDownFlag10
constexpr mh::CallSite kCalls4F9F70[] = {{0x22, 0x452F70}};
// 0x4F9FB0: 0x12 bytes; +0xB note: jmp through .data 0x65c34c, 1 code entries (a data_tables entry)  MeteorStrikeRock_Task
// 0x4F9FD0: 0x56 bytes; +0x15 note: call through .data 0x65c350, 5 code entries (a data_tables entry)  MeteorStrikeRock_Run
constexpr mh::CallSite kCalls4F9FD0[] = {{0x2D, 0x4FBD10}, {0x32, 0x588F20}, {0x37, 0x4F6020}, {0x3C, 0x4AC510}, {0x41, 0x4FA440}, {0x46, 0x5A7BC0}};
// 0x4FA030: 0x190 bytes  MeteorStrikeRock_Launch
constexpr mh::CallSite kCalls4FA030[] = {{0x5E, 0x446770}, {0xBF, 0x446770}, {0x159, 0x5891F0}};
// 0x4FA1C0: 0x17C bytes  MeteorStrikeRock_Fall
constexpr mh::CallSite kCalls4FA1C0[] = {{0x5F, 0x4FAF90}, {0xC5, 0x4FAF90}, {0x15C, 0x587900}};
// 0x4FA340: 0x47 bytes  MeteorStrikeRock_Shake
// 0x4FA390: 0x4C bytes  MeteorStrikeRock_Hide
// 0x4FA3E0: 0x54 bytes  MeteorStrikeRock_End
constexpr mh::CallSite kCalls4FA3E0[] = {{0x4E, 0x4351F0}};
// 0x4FA440: 0x22A bytes  MeteorStrikeRock_DrawRing
constexpr mh::CallSite kCalls4FA440[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x54, 0x5A7A00}, {0x6D, 0x5A7A50}, {0x86, 0x5A7A00}, {0x9F, 0x5A7A50}, {0xE2, 0x5A7610}, {0xE9, 0x5A7780}, {0x109, 0x5A7A00}, {0x122, 0x5A7A50}, {0x155, 0x5A7A00}, {0x16E, 0x5A7A50}, {0x1B4, 0x5A85F0}, {0x1BD, 0x5A9350}, {0x20C, 0x461E50}};
// 0x4FA670: 0x12 bytes; +0xB note: jmp through .data 0x65c36c, 2 code entries (a data_tables entry)  MeteorStrikeRecord_Task
// 0x4FA690: 0x2E bytes; +0xB note: call through .data 0x65c384, 3 code entries (a data_tables entry)  MeteorStrikeChip_Run
constexpr mh::CallSite kCalls4FA690[] = {{0x23, 0x4FBD10}, {0x28, 0x4FA910}};
// 0x4FA6C0: 0xD0 bytes  MeteorStrikeChip_Launch
constexpr mh::CallSite kCalls4FA6C0[] = {{0x32, 0x5A7A00}, {0x57, 0x5A7A50}};
// 0x4FA790: 0xC1 bytes  MeteorStrikeChip_Rise
constexpr mh::CallSite kCalls4FA790[] = {{0x20, 0x5A7A00}, {0x3E, 0x5A7A50}};
// 0x4FA860: 0xA5 bytes  MeteorStrikeChip_Fall
constexpr mh::CallSite kCalls4FA860[] = {{0x20, 0x5A7A00}, {0x3C, 0x5A7A50}, {0x9F, 0x4F6290}};
// 0x4FA910: 0x25D bytes  MeteorStrikeChip_Draw
constexpr mh::CallSite kCalls4FA910[] = {{0x10, 0x5A77C0}, {0x26, 0x572FA0}, {0x54, 0x5A75D0}, {0x5C, 0x5A7780}, {0x66, 0x5A7A50}, {0x94, 0x5A7A00}, {0xC2, 0x5A7A50}, {0xF0, 0x5A7A00}, {0x121, 0x5A7A50}, {0x14F, 0x5A7A00}, {0x17D, 0x5A7A50}, {0x1AB, 0x5A7A00}, {0x1E2, 0x5A79A0}, {0x1F2, 0x5A79E0}, {0x252, 0x572FA0}};
// 0x4FAB70: 0x33 bytes; +0xB note: call through .data 0x65c390, 3 code entries (a data_tables entry)  MeteorStrikeTrail_Run
constexpr mh::CallSite kCalls4FAB70[] = {{0x23, 0x4B7D40}, {0x28, 0x4FAD00}, {0x2D, 0x5A7BC0}};
// 0x4FABB0: 0x87 bytes  MeteorStrikeTrail_Start
// 0x4FAC40: 0x4E bytes  MeteorStrikeTrail_Follow
// 0x4FAC90: 0x68 bytes  MeteorStrikeTrail_Fade
constexpr mh::CallSite kCalls4FAC90[] = {{0x62, 0x4F6290}};
// 0x4FAD00: 0x287 bytes  MeteorStrikeTrail_Draw
constexpr mh::CallSite kCalls4FAD00[] = {{0x63, 0x5A7A00}, {0x7C, 0x5A7A50}, {0xA6, 0x5A7A00}, {0xBF, 0x5A7A50}, {0x108, 0x5A7A00}, {0x121, 0x5A7A50}, {0x154, 0x5A7A00}, {0x16D, 0x5A7A50}, {0x1B3, 0x5A77C0}, {0x1BE, 0x572FA0}, {0x1CA, 0x5A7610}, {0x1D2, 0x5A7780}, {0x208, 0x5A85F0}, {0x20E, 0x5A9350}, {0x265, 0x572FA0}};
// 0x4FAF90: 0x57 bytes  MeteorStrike_RecordAlloc
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Tempest_Task", 0x4F8640, 0x7D, kCalls4F8640, MH_N(kCalls4F8640), kImms4F8640, MH_N(kImms4F8640), nullptr, 0, reinterpret_cast<const void*>(&::Tempest_Task)},
    {"Tempest_Start", 0x4F86C0, 0x162, kCalls4F86C0, MH_N(kCalls4F86C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Tempest_Start)},
    {"Tempest_End", 0x4F8830, 0x2A, kCalls4F8830, MH_N(kCalls4F8830), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Tempest_End)},
    {"TempestFlash_Task", 0x4F8860, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestFlash_Task)},
    {"TempestFlash_Run", 0x4F8880, 0x23, kCalls4F8880, MH_N(kCalls4F8880), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestFlash_Run)},
    {"TempestFlash_Draw", 0x4F88B0, 0x12F, kCalls4F88B0, MH_N(kCalls4F88B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestFlash_Draw)},
    {"TempestGust_Task", 0x4F89E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestGust_Task)},
    {"TempestGust_Run", 0x4F8A00, 0x29, kCalls4F8A00, MH_N(kCalls4F8A00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestGust_Run)},
    {"TempestGust_Launch", 0x4F8A30, 0x119, kCalls4F8A30, MH_N(kCalls4F8A30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestGust_Launch)},
    {"TempestGust_Drift", 0x4F8B50, 0x63, kCalls4F8B50, MH_N(kCalls4F8B50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestGust_Drift)},
    {"TempestGust_Fly", 0x4F8BC0, 0x66, kCalls4F8BC0, MH_N(kCalls4F8BC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestGust_Fly)},
    {"TempestGust_Draw", 0x4F8C30, 0x2A1, kCalls4F8C30, MH_N(kCalls4F8C30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::TempestGust_Draw)},
    {"Tempest_GustAlloc", 0x4F8EE0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Tempest_GustAlloc), 0xFF},
    {"Magic225_Task", 0x4F8F40, 0x85, kCalls4F8F40, MH_N(kCalls4F8F40), kImms4F8F40, MH_N(kImms4F8F40), nullptr, 0, reinterpret_cast<const void*>(&::Magic225_Task)},
    {"Magic225_Start", 0x4F8FD0, 0xB1, kCalls4F8FD0, MH_N(kCalls4F8FD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225_Start)},
    {"Magic225_Spawn", 0x4F9090, 0x97, kCalls4F9090, MH_N(kCalls4F9090), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225_Spawn)},
    {"Magic225_Apply", 0x4F9130, 0x269, kCalls4F9130, MH_N(kCalls4F9130), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225_Apply)},
    {"Magic225Veil_Task", 0x4F93A0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Veil_Task)},
    {"Magic225Veil_Run", 0x4F93C0, 0x83, kCalls4F93C0, MH_N(kCalls4F93C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Veil_Run)},
    {"Magic225Veil_Start", 0x4F9450, 0xB7, kCalls4F9450, MH_N(kCalls4F9450), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Veil_Start)},
    {"Magic225Veil_FadeIn", 0x4F9510, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Veil_FadeIn)},
    {"Magic225Veil_WaitBuffs", 0x4F9540, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Veil_WaitBuffs)},
    {"Magic225Veil_End", 0x4F9560, 0xAE, kCalls4F9560, MH_N(kCalls4F9560), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Veil_End)},
    {"Magic225Veil_DrawBurst", 0x4F9610, 0x368, kCalls4F9610, MH_N(kCalls4F9610), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Veil_DrawBurst)},
    {"Magic225Veil_DrawShade", 0x4F9980, 0xBD, kCalls4F9980, MH_N(kCalls4F9980), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Veil_DrawShade)},
    {"Magic225Shard_Task", 0x4F9A40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Shard_Task)},
    {"Magic225Shard_Run", 0x4F9A60, 0x3D, kCalls4F9A60, MH_N(kCalls4F9A60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Shard_Run)},
    {"Magic225Shard_Launch", 0x4F9AA0, 0x1B8, kCalls4F9AA0, MH_N(kCalls4F9AA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Shard_Launch)},
    {"Magic225Shard_Fly", 0x4F9C60, 0x8A, kCalls4F9C60, MH_N(kCalls4F9C60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Shard_Fly)},
    {"Magic225Shard_Fall", 0x4F9CF0, 0x85, kCalls4F9CF0, MH_N(kCalls4F9CF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225Shard_Fall)},
    {"Magic225_ShardAlloc", 0x4F9D80, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic225_ShardAlloc), 0xFF},
    {"MeteorStrike_Task", 0x4F9DE0, 0x7D, kCalls4F9DE0, MH_N(kCalls4F9DE0), kImms4F9DE0, MH_N(kImms4F9DE0), nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrike_Task)},
    {"MeteorStrike_Start", 0x4F9E60, 0x10B, kCalls4F9E60, MH_N(kCalls4F9E60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrike_Start)},
    {"MagicFx_CountDownFlag10", 0x4F9F70, 0x33, kCalls4F9F70, MH_N(kCalls4F9F70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_CountDownFlag10)},
    {"MeteorStrikeRock_Task", 0x4F9FB0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRock_Task)},
    {"MeteorStrikeRock_Run", 0x4F9FD0, 0x56, kCalls4F9FD0, MH_N(kCalls4F9FD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRock_Run)},
    {"MeteorStrikeRock_Launch", 0x4FA030, 0x190, kCalls4FA030, MH_N(kCalls4FA030), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRock_Launch)},
    {"MeteorStrikeRock_Fall", 0x4FA1C0, 0x17C, kCalls4FA1C0, MH_N(kCalls4FA1C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRock_Fall)},
    {"MeteorStrikeRock_Shake", 0x4FA340, 0x47, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRock_Shake)},
    {"MeteorStrikeRock_Hide", 0x4FA390, 0x4C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRock_Hide)},
    {"MeteorStrikeRock_End", 0x4FA3E0, 0x54, kCalls4FA3E0, MH_N(kCalls4FA3E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRock_End)},
    {"MeteorStrikeRock_DrawRing", 0x4FA440, 0x22A, kCalls4FA440, MH_N(kCalls4FA440), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRock_DrawRing)},
    {"MeteorStrikeRecord_Task", 0x4FA670, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeRecord_Task)},
    {"MeteorStrikeChip_Run", 0x4FA690, 0x2E, kCalls4FA690, MH_N(kCalls4FA690), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeChip_Run)},
    {"MeteorStrikeChip_Launch", 0x4FA6C0, 0xD0, kCalls4FA6C0, MH_N(kCalls4FA6C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeChip_Launch)},
    {"MeteorStrikeChip_Rise", 0x4FA790, 0xC1, kCalls4FA790, MH_N(kCalls4FA790), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeChip_Rise)},
    {"MeteorStrikeChip_Fall", 0x4FA860, 0xA5, kCalls4FA860, MH_N(kCalls4FA860), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeChip_Fall)},
    {"MeteorStrikeChip_Draw", 0x4FA910, 0x25D, kCalls4FA910, MH_N(kCalls4FA910), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeChip_Draw)},
    {"MeteorStrikeTrail_Run", 0x4FAB70, 0x33, kCalls4FAB70, MH_N(kCalls4FAB70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeTrail_Run)},
    {"MeteorStrikeTrail_Start", 0x4FABB0, 0x87, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeTrail_Start)},
    {"MeteorStrikeTrail_Follow", 0x4FAC40, 0x4E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeTrail_Follow)},
    {"MeteorStrikeTrail_Fade", 0x4FAC90, 0x68, kCalls4FAC90, MH_N(kCalls4FAC90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeTrail_Fade)},
    {"MeteorStrikeTrail_Draw", 0x4FAD00, 0x287, kCalls4FAD00, MH_N(kCalls4FAD00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrikeTrail_Draw)},
    {"MeteorStrike_RecordAlloc", 0x4FAF90, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MeteorStrike_RecordAlloc), 0xFF},
};
#undef MH_N

enum : unsigned {
    kTempest_Task, kTempest_Start, kTempest_End, kTempestFlash_Task, kTempestFlash_Run, kTempestFlash_Draw,
    kTempestGust_Task, kTempestGust_Run, kTempestGust_Launch, kTempestGust_Drift, kTempestGust_Fly, kTempestGust_Draw,
    kTempest_GustAlloc,
    kMagic225_Task, kMagic225_Start, kMagic225_Spawn, kMagic225_Apply, kMagic225Veil_Task, kMagic225Veil_Run,
    kMagic225Veil_Start, kMagic225Veil_FadeIn, kMagic225Veil_WaitBuffs, kMagic225Veil_End, kMagic225Veil_DrawBurst,
    kMagic225Veil_DrawShade, kMagic225Shard_Task, kMagic225Shard_Run, kMagic225Shard_Launch, kMagic225Shard_Fly,
    kMagic225Shard_Fall, kMagic225_ShardAlloc,
    kMeteorStrike_Task, kMeteorStrike_Start, kMagicFx_CountDownFlag10, kMeteorStrikeRock_Task, kMeteorStrikeRock_Run,
    kMeteorStrikeRock_Launch, kMeteorStrikeRock_Fall, kMeteorStrikeRock_Shake, kMeteorStrikeRock_Hide,
    kMeteorStrikeRock_End, kMeteorStrikeRock_DrawRing, kMeteorStrikeRecord_Task, kMeteorStrikeChip_Run,
    kMeteorStrikeChip_Launch, kMeteorStrikeChip_Rise, kMeteorStrikeChip_Fall, kMeteorStrikeChip_Draw,
    kMeteorStrikeTrail_Run, kMeteorStrikeTrail_Start, kMeteorStrikeTrail_Follow, kMeteorStrikeTrail_Fade,
    kMeteorStrikeTrail_Draw, kMeteorStrike_RecordAlloc, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kScratchBytes = 0x1C, kActorRecord = 0x904B3C,
                        kEventBattle = 0x904AAA, kAbility = 0x904B80, kFrameSet = 0x9039D8, kCamera = 0x929EC8;

struct Pool {
    std::uint32_t at;
    unsigned records;
};
constexpr Pool kGustPool = {0x6B7BE0, 64}, kShardPool = {0x6B9CE0, 32}, kRecordPool = {0x6BAD60, 48};

// Set by the seed for the one function it matters to.
bool g_allow_none = false;   // BattleTask_Create may answer 0xFF (Magic225_Apply)

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
// Rand, Math_Sin and Math_Cos: the callers keep their angles, radii, colours
// and vertices in the scratch words and Prim_VertexScratch and read them again
// after each of these calls, so a third of the time one of those words is
// moved here (group S10's).
std::uint32_t StirScratch(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t r = mh::Noise();
    if (r % 3 != 0) return answer;
    const auto v = static_cast<std::uint16_t>(mh::Noise() >> 16);
    if (r & 0x100) SetWord(mh::Mem(kScratch + 2 * ((r >> 12) % (kScratchBytes / 2))), v);
    else SetWord(mh::Mem(kVertex + 2 * ((r >> 12) % 16)), v);
    return answer;
}
// 0x446770 turns the task's +0xC / +0x10 by its +8: the inputs logged, a new
// pair written where the real one writes.
std::uint32_t TurnEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* task = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    mh::Note(task[8], static_cast<std::uint32_t>(Long(task + 0xC)), static_cast<std::uint32_t>(Long(task + 0x10)));
    mh::FillBytes(task + 0xC, 8);
    // Magic225Shard_Launch reads the actor's sprite 0x904B3C once, before the
    // turn: a quarter of the time the pointer moves here, so a re-read shows
    // (control V97 stood on the harness's own disturbance alone).
    const std::uint32_t r = mh::Noise();
    if (r % 4 == 0) mh::SetPointer(kActorRecord, mh::SpriteRecord(r >> 8));
    return answer;
}
// None free (0xFF) a quarter of the time, for Magic225_Apply only.
std::uint32_t CreateEffect(const std::uint32_t*, std::uint32_t answer) {
    if (g_allow_none && mh::Noise() % 4 == 0) return answer | 0xFF;
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S38_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S38_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S38_OURS(BattleTask_Create), 2, {kU8, kU8}, mh::Answer::kByte, 0, mh::at::kTaskCount - 1, {}, &CreateEffect},
    {S38_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S38_OURS(BattleActor_UpdateScreenXY), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_OURS(MagicFx_CenterOnSide), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_OURS(MagicFx_PushActorMatrix), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {"Rand", 0x5B93D2, KeyOf(&::Rand), 0, {}, mh::Answer::kRand, 0, 0, {}, &StirScratch},
    // the sprite and battle calls
    {S38_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S38_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S38_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    // the draw library (all ours)
    {S38_OURS(Math_Sin), 1, {kAll}, kG, 0, 0, {}, &StirScratch},
    {S38_OURS(Math_Cos), 1, {kAll}, kG, 0, 0, {}, &StirScratch},
    {S38_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0},
    {S38_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, kG, 0, 0, {}, &CommitEffect},
    {S38_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kU8, kU8}, kG, 0, 0, {}, &LinkEffect},
    {S38_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S38_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S38_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S38_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S38_OURS(Gpu_SetPolyF4), 1, {kAll}, kG, 0, 0},
    {S38_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S38_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S38_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S38_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S38_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // MAGIC053's (group S10), called directly by the rock
    {S38_OURS(LavaburstChild_DrawGlow), 0, {}, kG, 0, 0, {}, &NoteSprite},
    // Capcom's, unnamed, in no group: the dx / dz turn by direction
    {S38_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // other units', not yet ours: MAGIC219's record free (a tail jmp) and
    // matrix push
    {S38_RAW(0x4F6290), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_RAW(0x4F6020), 0, {}, kG, 0, 0, {}, &NoteSprite},
    // this group's own, called directly
    {S38_RAW(0x4F88B0), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_RAW(0x4F89E0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S38_RAW(0x4F8C30), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_RAW(0x4F8EE0), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},
    {S38_RAW(0x4F9610), 4, {kU16, kU16, kU16, kU8}, kG, 0, 0, {}, &NoteSprite},
    {S38_RAW(0x4F9980), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_RAW(0x4F9A40), 0, {}, mh::Answer::kPhase, 0, 0},
    {S38_RAW(0x4F9D80), 0, {}, mh::Answer::kByte, 0xFF, 0x1F},
    {S38_RAW(0x4FA440), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_RAW(0x4FA670), 0, {}, mh::Answer::kPhase, 0, 0},
    {S38_RAW(0x4FA910), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_RAW(0x4FAD00), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S38_RAW(0x4FAF90), 0, {}, mh::Answer::kByte, 0xFF, 0x2F},
};
#undef S38_OURS
#undef S38_RAW

// The .data handler tables the dispatchers read in place (symbols.toml).
const mh::DataTable kTables[] = {
    {0x65C2E4, 1},   // TempestFlash_TaskTable
    {0x65C2E8, 4},   // TempestFlash_Steps
    {0x65C2F8, 1},   // TempestGust_TaskTable
    {0x65C2FC, 3},   // TempestGust_Steps
    {0x65C318, 1},   // Magic225Veil_TaskTable
    {0x65C328, 5},   // Magic225Veil_Steps
    {0x65C33C, 1},   // Magic225Shard_TaskTable
    {0x65C340, 3},   // Magic225Shard_Steps
    {0x65C34C, 1},   // MeteorStrikeRock_TaskTable
    {0x65C350, 5},   // MeteorStrikeRock_Steps
    {0x65C36C, 2},   // MeteorStrikeRecord_Kinds
    {0x65C384, 3},   // MeteorStrikeChip_Steps
    {0x65C390, 3},   // MeteorStrikeTrail_Steps
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                                  // Gfx_PacketNext
    {0, kPrimBytes},                                // g_prims (filled in at start-up)
    {kVertex, 0x20},                                // Prim_VertexScratch, four SVECTORs
    {kScratch, kScratchBytes},                      // 0x903850..0x90386B
    {kFrameSet, 4},                                 // the frame-offset table pointer the veil, shards and rock swap
    {kCamera, 2},                                   // Camera_Angles[0]
    {kAbility, 2},                                  // the acting ability
    {kGustPool.at, kGustPool.records * 0x84u},      // Tempest_GustPool
    {kShardPool.at, kShardPool.records * 0x84u},    // Magic225_ShardPool
    {kRecordPool.at, kRecordPool.records * 0x84u},  // MeteorStrike_RecordPool
    {0x812980, 0x200},                              // Gfx_ClutStrip row 26
    {0x80E980, 0x200},                              // Gfx_ClutStripSource row 26
    {0x80F980, 0x200},                              // Gfx_ClutStrip row 2
    {0x80B980, 0x200},                              // Gfx_ClutStripSource row 2
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* PoolRecord(const Pool& pool, unsigned i) { return mh::Mem(pool.at + (i % pool.records) * 0x84u); }
const Pool& SomePool(std::uint32_t v) {
    static const Pool* const kPools[3] = {&kGustPool, &kShardPool, &kRecordPool};
    return *kPools[v % 3];
}

// A real slot or record for a pool record's owner (a walk makes it the owner
// cell, which the recorders write through).
const void* SomeOwner(std::uint32_t v) {
    return (v & 4) ? static_cast<const void*>(mh::SpriteRecord(v & 1)) : static_cast<const void*>(mh::TaskAt(v & 3));
}

// The group's cells a recorder may move (the harness's case 14):
// Gfx_PacketNext, a vertex word, a scratch word, the frame-offset table, the
// camera angle, a pool record's live bit, the actor's sprite pointer, the
// event-battle byte, the ability word.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 9) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % (kScratchBytes / 2))), h >> 16); break;
    case 3: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 4: SetWord(mh::Mem(kCamera), h >> 16); break;
    case 5: PoolRecord(SomePool(v), v >> 2)[0] ^= 1; break;
    case 6: mh::SetPointer(kActorRecord, mh::SpriteRecord(v)); break;
    case 7: mh::Mem(kEventBattle)[0] = Byte(h >> 24); break;
    case 8: SetWord(mh::Mem(kAbility), (h & 0x1000000) ? 0xDF : h >> 16); break;
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

// A pool full, taken up to a point, or as the fill left it; every owner real.
void FillPool(const Pool& pool) {
    const unsigned mode = mh::Next() % 4;
    const unsigned taken = mh::Next() % (pool.records + 1);
    for (unsigned i = 0; i < pool.records; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        if (mode == 0) rec[0] = Byte(rec[0] | 1);
        else if (mode == 1) rec[0] = Byte(i < taken ? rec[0] | 1 : rec[0] & ~1u);
        mh::SetPointer(pool.at + i * 0x84u + 0x80, SomeOwner(mh::Next()));
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    unsigned char* const owner = mh::Pointer(mh::at::kOwner);
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kActorRecord, mh::SpriteRecord(mh::Next()));
    FillPool(kGustPool);
    FillPool(kShardPool);
    FillPool(kRecordPool);
    g_allow_none = k == kMagic225_Apply;
    if (mh::Half()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Mem(mh::at::kTarget)[0] | 0x40);
    if (mh::Often()) mh::Mem(mh::at::kActor)[0] = Byte(mh::Next() % 11);
    if (mh::Often()) owner[8] = Byte(mh::Next() % 4);
    if (mh::Often()) sc[8] = Byte(mh::Next() % 4);
    if (mh::Half()) SetWord(mh::Mem(kAbility), mh::Half() ? 0xDF : 0xE0);
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kTempest_Task: case kMeteorStrike_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kMagic225_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kTempestFlash_Task: case kTempestGust_Task: case kMagic225Veil_Task: case kMagic225Shard_Task:
    case kMeteorStrikeRock_Task:
        sc[1] = 0;
        break;
    case kMeteorStrikeRecord_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kTempestFlash_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kTempestGust_Run: case kMagic225Shard_Run: case kMeteorStrikeChip_Run: case kMeteorStrikeTrail_Run:
        sc[2] = Byte(mh::Next() % 3);
        break;
    case kMagic225Veil_Run: sc[2] = Byte(mh::Next() % 5); break;
    case kMeteorStrikeRock_Run:
        sc[2] = Byte(mh::Next() % 5);
        if (mh::Half()) sc[0] = Byte(sc[0] ^ 1);
        break;
    // the counters: at their thresholds
    case kTempest_End: case kMagic225_Apply: if (mh::Often()) sc[0xB] = 0; break;
    case kMagic225_Spawn: case kMagic225Shard_Launch: case kMagic225Shard_Fall:
    case kMagicFx_CountDownFlag10: case kMeteorStrikeRock_Shake: case kMeteorStrikeRock_Hide:
    case kMeteorStrikeChip_Launch: case kMeteorStrikeChip_Rise: case kMeteorStrikeTrail_Start:
        Near(sc[9], 1);
        break;
    case kTempestGust_Drift: Near(sc[0xA], 0xB); break;
    // TempestGust_Launch compares the whole word: 0xDF with a high byte a
    // third of the time (control T50 stood without it)
    case kTempestGust_Launch:
        Near(sc[9], 1);
        if (mh::Often()) SetWord(mh::Mem(kAbility), 0xDF + 0x100 * (1 + mh::Next() % 0xFF) * (mh::Next() % 3 == 0));
        break;
    case kTempestGust_Fly: Near(sc[9], 0x12); break;
    case kMagic225Veil_FadeIn: Near(sc[0xA], 0xD); break;
    case kMagic225Veil_WaitBuffs: if (mh::Often()) owner[0xB] = Byte(mh::Next() % 3); break;
    case kMagic225Veil_End:
        if (mh::Half()) sc[9] = 0;
        Near(sc[0xA], 0);
        break;
    case kMagic225Veil_Start: if (mh::Half()) mh::Mem(kEventBattle)[0] = mh::Half() ? 0x37 : 0x36; break;
    case kMagic225Shard_Fly:
        if (mh::Often()) SetLong(sc + 0xC, static_cast<std::int32_t>(4 + mh::Next() % 2));
        break;
    case kMeteorStrike_Start: if (mh::Often()) mh::Mem(mh::at::kActor)[0] = Byte(mh::Half() ? 2 + mh::Next() % 2 : mh::Next() % 11); break;
    case kMeteorStrikeRock_Fall:
        if (mh::Half()) Frame_Counter &= ~3u;
        Near(sc[9], 1);
        break;
    case kMeteorStrikeRock_End:
        if (mh::Half()) sc[0xA] = 0;
        if (mh::Often()) sc[0x5D] = Byte(0x87 + mh::Next() % 3);
        break;
    case kMeteorStrikeChip_Fall:
        if (mh::Half()) sc[0x5E] = 0;
        Near(sc[9], 1);
        break;
    case kMeteorStrikeTrail_Follow: Near(sc[9], 0x16); break;
    case kMeteorStrikeTrail_Fade: if (mh::Often()) sc[0xA] = Byte(1 + mh::Next() % 3); break;
    default: break;
    }
}

// Magic225Veil_DrawBurst's four words: its caller's rows most of the time.
void Args(unsigned k, std::uint32_t* a) {
    if (k != kMagic225Veil_DrawBurst || !mh::Often()) return;
    static const std::uint32_t kRows[4][4] = {{0x30, 0x10, 0x52, 0}, {0x12C, 0, 0x80, 1}, {0x40, 0xF0, 0x6C, 2},
                                              {0x140, 0xF0, 0x5C, 3}};
    const auto& row = kRows[mh::Next() % 4];
    for (unsigned i = 0; i < 4; ++i) a[i] = row[i];
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s38", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s38
