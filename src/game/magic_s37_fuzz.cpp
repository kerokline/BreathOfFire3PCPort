// BOF3X_SHADOW=magic_s37: group S37's three overlays (MAGIC219, MAGIC220/221,
// MAGIC222) through the spell round's shared harness (magic_harness.h), once
// at start-up. docs/magic_s37.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC219 / MAGIC220/MAGIC221 /
// MAGIC222 --clones (2026-09-27; capstone, every jump internal, no jump table,
// no REFUSED line), names given. Beyond the standard set this group lists the
// draw callees (the GTE and libgpu entry points, Math_Sin / Math_Cos /
// Math_Ratan2, Gfx_CommitPrim, MapView_LinkPrimAt), the sprite calls, the
// engine's 0x446770, and the functions of its own that its functions call
// directly. Everything the harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do;
//   - the GTE callees of MagicFx_PushRecordMatrix log what their pointers
//     point at (`deref`) and write a result where the real ones write;
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what the caller reads back is compared;
//   - Math_Sin and Math_Cos move a scratch or vertex word a third of the time
//     (the draws read their radius, colour and vertex words again after every
//     call);
//   - Math_Ratan2 answers near MagmaBreathRecord_Fly's two thresholds (a turn
//     of 0x600 and 0xA00 from the last angle) most of the time while that
//     function is fuzzed;
//   - the sprite calls that act on Sprite_Current log which sprite, the screen
//     update the frame-offset table 0x9039D8 the Run steps swap;
//   - the two pools are regions, their owners real slots or records (the
//     walks make +0x80 the owner cell, which the recorders write through).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s37.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s37 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

// tools/magic_rows.py --unit MAGIC219 / MAGIC220/MAGIC221 / MAGIC222 --clones,
// 2026-09-27, names given.
// 0x4F59D0: 0x85 bytes  MagmaBreath_Task
constexpr mh::CallSite kCalls4F59D0[] = {{0x63, 0x4F5BC0}};
constexpr mh::Imm kImms4F59D0[] = {{0x16, 0x4F5A60}, {0x1E, 0x4F5B00}, {0x26, 0x4F5B80}, {0x2E, 0x4F7350}};
// 0x4F5A60: 0x94 bytes  MagmaBreath_Start
constexpr mh::CallSite kCalls4F5A60[] = {{0x1D, 0x4FC0E0}, {0x8D, 0x587900}};
// 0x4F5B00: 0x7D bytes  MagmaBreath_Spawn
constexpr mh::CallSite kCalls4F5B00[] = {{0xB, 0x4F6230}, {0x43, 0x5B93D2}};
// 0x4F5B80: 0x3D bytes  MagmaBreath_Strike
constexpr mh::CallSite kCalls4F5B80[] = {{0x1E, 0x587900}, {0x2C, 0x452F70}};
// 0x4F5BC0: 0x12 bytes; +0xB note: jmp through .data 0x65c244, 1 code entries (a data_tables entry)  MagmaBreathRecord_Task
// 0x4F5BE0: 0x4C bytes; +0x15 note: call through .data 0x65c248, 4 code entries (a data_tables entry)  MagmaBreathRecord_Run
constexpr mh::CallSite kCalls4F5BE0[] = {{0x2D, 0x588F20}, {0x32, 0x4F6020}, {0x37, 0x4F60D0}, {0x3C, 0x5A7BC0}};
// 0x4F5C30: 0x208 bytes  MagmaBreathRecord_Launch
constexpr mh::CallSite kCalls4F5C30[] = {{0x48, 0x446770}, {0xB5, 0x5A7A00}, {0xDC, 0x5A7A50}, {0x1A4, 0x5891F0}, {0x1D8, 0x5A7A70}};
// 0x4F5E40: 0x124 bytes  MagmaBreathRecord_Fly
constexpr mh::CallSite kCalls4F5E40[] = {{0x18, 0x5893A0}, {0x85, 0x5A7A70}, {0xAC, 0x4FBA90}, {0xBC, 0x4FBCD0}};
// 0x4F5F70: 0x6E bytes  MagmaBreathRecord_Land
constexpr mh::CallSite kCalls4F5F70[] = {{0x4A, 0x5891F0}};
// 0x4F5FE0: 0x3F bytes  MagmaBreathRecord_Burn
constexpr mh::CallSite kCalls4F5FE0[] = {{0x0, 0x589410}, {0x39, 0x4F6290}};
// 0x4F6020: 0xA4 bytes  MagicFx_PushRecordMatrix
constexpr mh::CallSite kCalls4F6020[] = {{0x3, 0x5A7B90}, {0x64, 0x5A8200}, {0x73, 0x5A8060}, {0x87, 0x5A7D70}, {0x91, 0x5A8DE0}, {0x9B, 0x5A8E00}};
// 0x4F60D0: 0x15F bytes  MagmaBreathRecord_DrawGlow
constexpr mh::CallSite kCalls4F60D0[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x3C, 0x5A7A00}, {0x52, 0x5A7A50}, {0x8B, 0x5A75F0}, {0x92, 0x5A7780}, {0xC0, 0x5A7A00}, {0xD6, 0x5A7A50}, {0x110, 0x5A84A0}, {0x116, 0x5A9310}, {0x13D, 0x461E50}};
// 0x4F6230: 0x57 bytes  MagmaBreath_PoolAlloc
// 0x4F6290: 0x2F bytes  MagicFx_FreeCurrentRecord
// 0x4F62C0: 0x7D bytes  GeoBreath_Task
constexpr mh::CallSite kCalls4F62C0[] = {{0x5B, 0x4F6D90}};
constexpr mh::Imm kImms4F62C0[] = {{0x16, 0x4F6340}, {0x1E, 0x4F9F70}, {0x26, 0x4F7350}};
// 0x4F6340: 0xF4 bytes  GeoBreath_Start
constexpr mh::CallSite kCalls4F6340[] = {{0x1C, 0x4FC0E0}, {0x5D, 0x587900}, {0x66, 0x435180}};
// 0x4F6440: 0x12 bytes; +0xB note: jmp through .data 0x65c258, 1 code entries (a data_tables entry)  GeoBreathChild_Task
// 0x4F6460: 0x51 bytes; +0x15 note: call through .data 0x65c25c, 8 code entries (a data_tables entry)  GeoBreathChild_Run
constexpr mh::CallSite kCalls4F6460[] = {{0x2D, 0x588F20}, {0x32, 0x4F6020}, {0x37, 0x4F69C0}, {0x3C, 0x4F6B60}, {0x41, 0x5A7BC0}};
// 0x4F64C0: 0x108 bytes  GeoBreathChild_Start
constexpr mh::CallSite kCalls4F64C0[] = {{0xD1, 0x5891F0}};
// 0x4F65D0: 0x42 bytes  GeoBreathChild_Swirl
constexpr mh::CallSite kCalls4F65D0[] = {{0x0, 0x589410}};
// 0x4F6620: 0xDF bytes  GeoBreathChild_Burst
constexpr mh::CallSite kCalls4F6620[] = {{0x39, 0x4F7190}, {0x93, 0x587900}, {0xA0, 0x587900}};
// 0x4F6700: 0x7F bytes  GeoBreathChild_Shake
// 0x4F6780: 0xFA bytes  GeoBreathChild_Quake
constexpr mh::CallSite kCalls4F6780[] = {{0x69, 0x587900}, {0x73, 0x4F7190}};
// 0x4F6880: 0x10E bytes  GeoBreathChild_Settle
constexpr mh::CallSite kCalls4F6880[] = {{0x69, 0x587900}, {0x73, 0x4F7190}};
// 0x4F6990: 0x29 bytes  GeoBreathChild_End
constexpr mh::CallSite kCalls4F6990[] = {{0x12, 0x589410}, {0x23, 0x4351F0}};
// 0x4F69C0: 0x19F bytes  GeoBreathChild_DrawGlow
constexpr mh::CallSite kCalls4F69C0[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x43, 0x5A7A00}, {0x5C, 0x5A7A50}, {0x98, 0x5A75F0}, {0xA0, 0x5A7780}, {0xCE, 0x5A7A00}, {0xE7, 0x5A7A50}, {0x124, 0x5A84A0}, {0x12A, 0x5A9310}, {0x181, 0x461E50}};
// 0x4F6B60: 0x22A bytes  GeoBreathChild_DrawRing
constexpr mh::CallSite kCalls4F6B60[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x54, 0x5A7A00}, {0x6D, 0x5A7A50}, {0x86, 0x5A7A00}, {0x9F, 0x5A7A50}, {0xE2, 0x5A7610}, {0xE9, 0x5A7780}, {0x109, 0x5A7A00}, {0x122, 0x5A7A50}, {0x155, 0x5A7A00}, {0x16E, 0x5A7A50}, {0x1B4, 0x5A85F0}, {0x1BD, 0x5A9350}, {0x20C, 0x461E50}};
// 0x4F6D90: 0x12 bytes; +0xB note: jmp through .data 0x65c27c, 1 code entries (a data_tables entry)  GeoBreathRecord_Task
// 0x4F6DB0: 0x33 bytes; +0xB note: call through .data 0x65c280, 3 code entries (a data_tables entry)  GeoBreathRecord_Run
constexpr mh::CallSite kCalls4F6DB0[] = {{0x23, 0x4B7D40}, {0x28, 0x4F6F70}, {0x2D, 0x5A7BC0}};
// 0x4F6DF0: 0x86 bytes  GeoBreathRecord_Start
constexpr mh::CallSite kCalls4F6DF0[] = {{0x17, 0x5A7A00}, {0x3A, 0x5A7A50}};
// 0x4F6E80: 0x6A bytes  GeoBreathRecord_Fly
constexpr mh::CallSite kCalls4F6E80[] = {{0x1C, 0x5A7A00}, {0x3B, 0x5A7A50}};
// 0x4F6EF0: 0x73 bytes  GeoBreathRecord_Fade
constexpr mh::CallSite kCalls4F6EF0[] = {{0x1C, 0x5A7A00}, {0x3A, 0x5A7A50}, {0x6D, 0x4F6290}};
// 0x4F6F70: 0x220 bytes  GeoBreathRecord_Draw
constexpr mh::CallSite kCalls4F6F70[] = {{0x13, 0x5A77C0}, {0x29, 0x572FA0}, {0x35, 0x5A75D0}, {0x3D, 0x5A7780}, {0x66, 0x5A7A50}, {0x83, 0x5A7A00}, {0xA7, 0x5A7A50}, {0xC4, 0x5A7A00}, {0xEB, 0x5A7A50}, {0x108, 0x5A7A00}, {0x12C, 0x5A7A50}, {0x149, 0x5A7A00}, {0x175, 0x5A79A0}, {0x184, 0x5A79E0}, {0x1F3, 0x5A85F0}, {0x1FC, 0x5A9290}, {0x212, 0x572FA0}};
// 0x4F7190: 0x57 bytes  GeoBreath_PoolAlloc
// 0x4F71F0: 0x2E bytes  Combustion_Task
constexpr mh::Imm kImms4F71F0[] = {{0xF, 0x4F7220}, {0x17, 0x4F7320}, {0x22, 0x4F7350}};
// 0x4F7220: 0xF8 bytes  Combustion_Start
constexpr mh::CallSite kCalls4F7220[] = {{0x0, 0x4FC0E0}, {0x5, 0x4FBD10}, {0x29, 0x435180}, {0x63, 0x435180}};
// 0x4F7320: 0x2F bytes  Combustion_Wait
constexpr mh::CallSite kCalls4F7320[] = {{0x1E, 0x587900}};
// 0x4F7380: 0x12 bytes; +0xB note: jmp through .data 0x65c28c, 4 code entries (a data_tables entry)  CombustionChild_Task
// 0x4F73A0: 0x42 bytes; +0x15 note: call through .data 0x65c29c, 6 code entries (a data_tables entry)  CombustionSprite_Run
constexpr mh::CallSite kCalls4F73A0[] = {{0x2D, 0x4FBD10}, {0x32, 0x588F20}};
// 0x4F73F0: 0x124 bytes  CombustionSprite_Start
constexpr mh::CallSite kCalls4F73F0[] = {{0x109, 0x5891F0}};
// 0x4F7520: 0xB7 bytes  CombustionSprite_Fall
constexpr mh::CallSite kCalls4F7520[] = {{0x45, 0x587900}, {0x53, 0x435180}};
// 0x4F75E0: 0x47 bytes  CombustionSprite_Shake
// 0x4F7630: 0xAD bytes  CombustionSprite_Flash
constexpr mh::CallSite kCalls4F7630[] = {{0x23, 0x587900}, {0x2C, 0x435180}};
// 0x4F76E0: 0x64 bytes  CombustionSprite_Fade
constexpr mh::CallSite kCalls4F76E0[] = {{0x5E, 0x4351F0}};
// 0x4F7750: 0x38 bytes; +0xB note: call through .data 0x65c2b4, 4 code entries (a data_tables entry)  CombustionGlow_Run
constexpr mh::CallSite kCalls4F7750[] = {{0x23, 0x4B7D40}, {0x28, 0x4F7830}, {0x2D, 0x4F79D0}, {0x32, 0x5A7BC0}};
// 0x4F7790: 0x6A bytes  CombustionGlow_Start
// 0x4F7800: 0x2A bytes  CombustionGlow_Grow
// 0x4F7830: 0x19E bytes  CombustionGlow_DrawGlow
constexpr mh::CallSite kCalls4F7830[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x42, 0x5A7A00}, {0x5B, 0x5A7A50}, {0x97, 0x5A75F0}, {0x9F, 0x5A7780}, {0xCD, 0x5A7A00}, {0xE6, 0x5A7A50}, {0x123, 0x5A84A0}, {0x129, 0x5A9310}, {0x180, 0x461E50}};
// 0x4F79D0: 0x229 bytes  CombustionGlow_DrawRing
constexpr mh::CallSite kCalls4F79D0[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x53, 0x5A7A00}, {0x6C, 0x5A7A50}, {0x85, 0x5A7A00}, {0x9E, 0x5A7A50}, {0xE1, 0x5A7610}, {0xE8, 0x5A7780}, {0x108, 0x5A7A00}, {0x121, 0x5A7A50}, {0x154, 0x5A7A00}, {0x16D, 0x5A7A50}, {0x1B3, 0x5A85F0}, {0x1BC, 0x5A9350}, {0x20B, 0x461E50}};
// 0x4F7C00: 0x33 bytes; +0xB note: call through .data 0x65c2c4, 3 code entries (a data_tables entry)  CombustionMote_Run
constexpr mh::CallSite kCalls4F7C00[] = {{0x23, 0x4B7D40}, {0x28, 0x4F7E10}, {0x2D, 0x5A7BC0}};
// 0x4F7C40: 0xA0 bytes  CombustionMote_Start
constexpr mh::CallSite kCalls4F7C40[] = {{0x1, 0x5B93D2}, {0x2D, 0x5A7A00}, {0x51, 0x5A7A50}};
// 0x4F7CE0: 0x87 bytes  CombustionMote_Grow
constexpr mh::CallSite kCalls4F7CE0[] = {{0x24, 0x5A7A00}, {0x48, 0x5A7A50}};
// 0x4F7D70: 0x91 bytes  CombustionMote_Shrink
constexpr mh::CallSite kCalls4F7D70[] = {{0x24, 0x5A7A00}, {0x48, 0x5A7A50}, {0x8B, 0x4351F0}};
// 0x4F7E10: 0x21E bytes  CombustionMote_Draw
constexpr mh::CallSite kCalls4F7E10[] = {{0x13, 0x5A77C0}, {0x29, 0x572FA0}, {0x35, 0x5A75D0}, {0x3D, 0x5A7780}, {0x64, 0x5A7A50}, {0x81, 0x5A7A00}, {0xA5, 0x5A7A50}, {0xC2, 0x5A7A00}, {0xE9, 0x5A7A50}, {0x106, 0x5A7A00}, {0x12A, 0x5A7A50}, {0x147, 0x5A7A00}, {0x173, 0x5A79A0}, {0x182, 0x5A79E0}, {0x1F1, 0x5A85F0}, {0x1FA, 0x5A9290}, {0x210, 0x572FA0}};
// 0x4F8030: 0x2E bytes; +0xB note: call through .data 0x65c2d0, 5 code entries (a data_tables entry)  CombustionFlash_Run
constexpr mh::CallSite kCalls4F8030[] = {{0x23, 0x4F8160}, {0x28, 0x4F8160}};
// 0x4F8060: 0x5C bytes  CombustionFlash_Start
constexpr mh::CallSite kCalls4F8060[] = {{0x2E, 0x452F70}};
// 0x4F80C0: 0x26 bytes  CombustionFlash_Hold
// 0x4F80F0: 0x2B bytes  CombustionFlash_Spin
// 0x4F8120: 0x35 bytes  CombustionFlash_End
constexpr mh::CallSite kCalls4F8120[] = {{0x2F, 0x4351F0}};
// 0x4F8160: 0x4D9 bytes  CombustionFlash_Draw
constexpr mh::CallSite kCalls4F8160[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0xA9, 0x5A7610}, {0xB1, 0x5A7780}, {0xC9, 0x5A7A00}, {0x10C, 0x5A7A00}, {0x1A5, 0x461E50}, {0x1B1, 0x5A7610}, {0x1B9, 0x5A7780}, {0x1C6, 0x5A7A00}, {0x1ED, 0x5A7A00}, {0x214, 0x5A7A00}, {0x23B, 0x5A7A00}, {0x2AA, 0x461E50}, {0x2B9, 0x5A7610}, {0x2C1, 0x5A7780}, {0x2CE, 0x5A7A00}, {0x305, 0x5A7A00}, {0x3A0, 0x461E50}, {0x3AC, 0x5A7610}, {0x3B4, 0x5A7780}, {0x3C1, 0x5A7A00}, {0x3EA, 0x5A7A00}, {0x413, 0x5A7A00}, {0x43C, 0x5A7A00}, {0x4AD, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"MagmaBreath_Task", 0x4F59D0, 0x85, kCalls4F59D0, MH_N(kCalls4F59D0), kImms4F59D0, MH_N(kImms4F59D0), nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreath_Task)},
    {"MagmaBreath_Start", 0x4F5A60, 0x94, kCalls4F5A60, MH_N(kCalls4F5A60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreath_Start)},
    {"MagmaBreath_Spawn", 0x4F5B00, 0x7D, kCalls4F5B00, MH_N(kCalls4F5B00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreath_Spawn)},
    {"MagmaBreath_Strike", 0x4F5B80, 0x3D, kCalls4F5B80, MH_N(kCalls4F5B80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreath_Strike)},
    {"MagmaBreathRecord_Task", 0x4F5BC0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreathRecord_Task)},
    {"MagmaBreathRecord_Run", 0x4F5BE0, 0x4C, kCalls4F5BE0, MH_N(kCalls4F5BE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreathRecord_Run)},
    {"MagmaBreathRecord_Launch", 0x4F5C30, 0x208, kCalls4F5C30, MH_N(kCalls4F5C30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreathRecord_Launch)},
    {"MagmaBreathRecord_Fly", 0x4F5E40, 0x124, kCalls4F5E40, MH_N(kCalls4F5E40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreathRecord_Fly)},
    {"MagmaBreathRecord_Land", 0x4F5F70, 0x6E, kCalls4F5F70, MH_N(kCalls4F5F70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreathRecord_Land)},
    {"MagmaBreathRecord_Burn", 0x4F5FE0, 0x3F, kCalls4F5FE0, MH_N(kCalls4F5FE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreathRecord_Burn)},
    {"MagicFx_PushRecordMatrix", 0x4F6020, 0xA4, kCalls4F6020, MH_N(kCalls4F6020), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_PushRecordMatrix)},
    {"MagmaBreathRecord_DrawGlow", 0x4F60D0, 0x15F, kCalls4F60D0, MH_N(kCalls4F60D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreathRecord_DrawGlow)},
    {"MagmaBreath_PoolAlloc", 0x4F6230, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagmaBreath_PoolAlloc), 0xFF},
    {"MagicFx_FreeCurrentRecord", 0x4F6290, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_FreeCurrentRecord)},
    {"GeoBreath_Task", 0x4F62C0, 0x7D, kCalls4F62C0, MH_N(kCalls4F62C0), kImms4F62C0, MH_N(kImms4F62C0), nullptr, 0, reinterpret_cast<const void*>(&::GeoBreath_Task)},
    {"GeoBreath_Start", 0x4F6340, 0xF4, kCalls4F6340, MH_N(kCalls4F6340), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreath_Start)},
    {"GeoBreathChild_Task", 0x4F6440, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_Task)},
    {"GeoBreathChild_Run", 0x4F6460, 0x51, kCalls4F6460, MH_N(kCalls4F6460), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_Run)},
    {"GeoBreathChild_Start", 0x4F64C0, 0x108, kCalls4F64C0, MH_N(kCalls4F64C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_Start)},
    {"GeoBreathChild_Swirl", 0x4F65D0, 0x42, kCalls4F65D0, MH_N(kCalls4F65D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_Swirl)},
    {"GeoBreathChild_Burst", 0x4F6620, 0xDF, kCalls4F6620, MH_N(kCalls4F6620), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_Burst)},
    {"GeoBreathChild_Shake", 0x4F6700, 0x7F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_Shake)},
    {"GeoBreathChild_Quake", 0x4F6780, 0xFA, kCalls4F6780, MH_N(kCalls4F6780), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_Quake)},
    {"GeoBreathChild_Settle", 0x4F6880, 0x10E, kCalls4F6880, MH_N(kCalls4F6880), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_Settle)},
    {"GeoBreathChild_End", 0x4F6990, 0x29, kCalls4F6990, MH_N(kCalls4F6990), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_End)},
    {"GeoBreathChild_DrawGlow", 0x4F69C0, 0x19F, kCalls4F69C0, MH_N(kCalls4F69C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_DrawGlow)},
    {"GeoBreathChild_DrawRing", 0x4F6B60, 0x22A, kCalls4F6B60, MH_N(kCalls4F6B60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathChild_DrawRing)},
    {"GeoBreathRecord_Task", 0x4F6D90, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathRecord_Task)},
    {"GeoBreathRecord_Run", 0x4F6DB0, 0x33, kCalls4F6DB0, MH_N(kCalls4F6DB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathRecord_Run)},
    {"GeoBreathRecord_Start", 0x4F6DF0, 0x86, kCalls4F6DF0, MH_N(kCalls4F6DF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathRecord_Start)},
    {"GeoBreathRecord_Fly", 0x4F6E80, 0x6A, kCalls4F6E80, MH_N(kCalls4F6E80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathRecord_Fly)},
    {"GeoBreathRecord_Fade", 0x4F6EF0, 0x73, kCalls4F6EF0, MH_N(kCalls4F6EF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathRecord_Fade)},
    {"GeoBreathRecord_Draw", 0x4F6F70, 0x220, kCalls4F6F70, MH_N(kCalls4F6F70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreathRecord_Draw)},
    {"GeoBreath_PoolAlloc", 0x4F7190, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::GeoBreath_PoolAlloc), 0xFF},
    {"Combustion_Task", 0x4F71F0, 0x2E, nullptr, 0, kImms4F71F0, MH_N(kImms4F71F0), nullptr, 0, reinterpret_cast<const void*>(&::Combustion_Task)},
    {"Combustion_Start", 0x4F7220, 0xF8, kCalls4F7220, MH_N(kCalls4F7220), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Combustion_Start)},
    {"Combustion_Wait", 0x4F7320, 0x2F, kCalls4F7320, MH_N(kCalls4F7320), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Combustion_Wait)},
    {"CombustionChild_Task", 0x4F7380, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionChild_Task)},
    {"CombustionSprite_Run", 0x4F73A0, 0x42, kCalls4F73A0, MH_N(kCalls4F73A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionSprite_Run)},
    {"CombustionSprite_Start", 0x4F73F0, 0x124, kCalls4F73F0, MH_N(kCalls4F73F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionSprite_Start)},
    {"CombustionSprite_Fall", 0x4F7520, 0xB7, kCalls4F7520, MH_N(kCalls4F7520), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionSprite_Fall)},
    {"CombustionSprite_Shake", 0x4F75E0, 0x47, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionSprite_Shake)},
    {"CombustionSprite_Flash", 0x4F7630, 0xAD, kCalls4F7630, MH_N(kCalls4F7630), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionSprite_Flash)},
    {"CombustionSprite_Fade", 0x4F76E0, 0x64, kCalls4F76E0, MH_N(kCalls4F76E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionSprite_Fade)},
    {"CombustionGlow_Run", 0x4F7750, 0x38, kCalls4F7750, MH_N(kCalls4F7750), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionGlow_Run)},
    {"CombustionGlow_Start", 0x4F7790, 0x6A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionGlow_Start)},
    {"CombustionGlow_Grow", 0x4F7800, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionGlow_Grow)},
    {"CombustionGlow_DrawGlow", 0x4F7830, 0x19E, kCalls4F7830, MH_N(kCalls4F7830), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionGlow_DrawGlow)},
    {"CombustionGlow_DrawRing", 0x4F79D0, 0x229, kCalls4F79D0, MH_N(kCalls4F79D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionGlow_DrawRing)},
    {"CombustionMote_Run", 0x4F7C00, 0x33, kCalls4F7C00, MH_N(kCalls4F7C00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionMote_Run)},
    {"CombustionMote_Start", 0x4F7C40, 0xA0, kCalls4F7C40, MH_N(kCalls4F7C40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionMote_Start)},
    {"CombustionMote_Grow", 0x4F7CE0, 0x87, kCalls4F7CE0, MH_N(kCalls4F7CE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionMote_Grow)},
    {"CombustionMote_Shrink", 0x4F7D70, 0x91, kCalls4F7D70, MH_N(kCalls4F7D70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionMote_Shrink)},
    {"CombustionMote_Draw", 0x4F7E10, 0x21E, kCalls4F7E10, MH_N(kCalls4F7E10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionMote_Draw)},
    {"CombustionFlash_Run", 0x4F8030, 0x2E, kCalls4F8030, MH_N(kCalls4F8030), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionFlash_Run)},
    {"CombustionFlash_Start", 0x4F8060, 0x5C, kCalls4F8060, MH_N(kCalls4F8060), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionFlash_Start)},
    {"CombustionFlash_Hold", 0x4F80C0, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionFlash_Hold)},
    {"CombustionFlash_Spin", 0x4F80F0, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionFlash_Spin)},
    {"CombustionFlash_End", 0x4F8120, 0x35, kCalls4F8120, MH_N(kCalls4F8120), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionFlash_End)},
    {"CombustionFlash_Draw", 0x4F8160, 0x4D9, kCalls4F8160, MH_N(kCalls4F8160), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::CombustionFlash_Draw)},
};
#undef MH_N

enum : unsigned {
    kMagmaBreath_Task, kMagmaBreath_Start, kMagmaBreath_Spawn, kMagmaBreath_Strike, kMagmaBreathRecord_Task, kMagmaBreathRecord_Run, kMagmaBreathRecord_Launch, kMagmaBreathRecord_Fly, kMagmaBreathRecord_Land, kMagmaBreathRecord_Burn, kMagicFx_PushRecordMatrix, kMagmaBreathRecord_DrawGlow, kMagmaBreath_PoolAlloc, kMagicFx_FreeCurrentRecord, kGeoBreath_Task, kGeoBreath_Start, kGeoBreathChild_Task, kGeoBreathChild_Run, kGeoBreathChild_Start, kGeoBreathChild_Swirl, kGeoBreathChild_Burst, kGeoBreathChild_Shake, kGeoBreathChild_Quake, kGeoBreathChild_Settle, kGeoBreathChild_End, kGeoBreathChild_DrawGlow, kGeoBreathChild_DrawRing, kGeoBreathRecord_Task, kGeoBreathRecord_Run, kGeoBreathRecord_Start, kGeoBreathRecord_Fly, kGeoBreathRecord_Fade, kGeoBreathRecord_Draw, kGeoBreath_PoolAlloc, kCombustion_Task, kCombustion_Start, kCombustion_Wait, kCombustionChild_Task, kCombustionSprite_Run, kCombustionSprite_Start, kCombustionSprite_Fall, kCombustionSprite_Shake, kCombustionSprite_Flash, kCombustionSprite_Fade, kCombustionGlow_Run, kCombustionGlow_Start, kCombustionGlow_Grow, kCombustionGlow_DrawGlow, kCombustionGlow_DrawRing, kCombustionMote_Run, kCombustionMote_Start, kCombustionMote_Grow, kCombustionMote_Shrink, kCombustionMote_Draw, kCombustionFlash_Run, kCombustionFlash_Start, kCombustionFlash_Hold, kCombustionFlash_Spin, kCombustionFlash_End, kCombustionFlash_Draw, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x2000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kVertex = 0x9037A0, kScratch = 0x903850, kActorRecord = 0x904B3C, kFrameSet = 0x9039D8,
                        kMagmaPool = 0x6B4A60, kMagmaBytes = 32 * 0x84, kGeoPool = 0x6B5AE0, kGeoBytes = 64 * 0x84,
                        kField = 0x905E60, kShiftX = 0x903800, kCamera = 0x929EC8;

// Math_Ratan2's answer near the fuzzed function's threshold (MagmaBreathRecord_Fly).
bool g_ratan_on = false;
std::uint32_t g_ratan = 0;

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
// The sprite calls that act on Sprite_Current: which sprite; the screen
// update, the frame-offset table too.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
// Sprite_ScriptTickOnce answers 0 or 1 in al (sprite_anim.cpp); a kFlag
// "yes" always has bit 4, so a quarter of the time al is exactly 1 here
// (garbage above), or a test of other bits than al's whole would stand.
std::uint32_t NoteSpriteOnce(const std::uint32_t* a, std::uint32_t answer) {
    NoteSprite(a, answer);
    if ((answer & 0xFF) != 0 && mh::Noise() % 4 == 0) answer = (answer & 0xFFFFFF00u) | 1;
    return answer;
}
std::uint32_t NoteSpriteFrames(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
    return answer;
}
// Math_Sin and Math_Cos: a third of the time a scratch or vertex word moved.
std::uint32_t StirScratch(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t r = mh::Noise();
    if (r % 3 != 0) return answer;
    const auto v = static_cast<std::uint16_t>(mh::Noise() >> 16);
    if (r & 0x100) SetWord(mh::Mem(kScratch + 2 * ((r >> 12) % 8)), v);
    else SetWord(mh::Mem(kVertex + 2 * ((r >> 12) % 16)), v);
    return answer;
}
// Math_Ratan2: near the seeded angle three times in four while it is on.
std::uint32_t RatanEffect(const std::uint32_t*, std::uint32_t answer) {
    if (g_ratan_on && mh::Noise() % 4 != 0) return g_ratan;
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

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S37_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S37_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S37_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S37_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSpriteOnce},
    // the sprite calls
    {S37_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S37_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    // the draw library (all ours)
    {S37_OURS(Math_Sin), 1, {kAll}, kG, 0, 0, {}, &StirScratch},
    {S37_OURS(Math_Cos), 1, {kAll}, kG, 0, 0, {}, &StirScratch},
    {S37_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0, {}, &RatanEffect},
    {S37_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S37_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S37_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S37_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S37_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S37_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S37_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S37_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S37_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S37_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread); the stack
    // pointers masked, the vector by its bytes
    {S37_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S37_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S37_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S37_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S37_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    // the projections: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S37_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S37_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S37_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S37_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    {S37_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed, in no group: the dx / dz turn by direction
    {S37_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    // this group's own, called directly: the two walks' record tasks and the
    // record free (each logs the task it ran for), the pool allocators (an
    // index or 0xFF), the matrix push and the draws
    {S37_RAW(0x4F5BC0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S37_RAW(0x4F6D90), 0, {}, mh::Answer::kPhase, 0, 0},
    {S37_RAW(0x4F6290), 0, {}, mh::Answer::kPhase, 0, 0},
    {S37_RAW(0x4F6230), 0, {}, mh::Answer::kByte, 0xFF, 0x1F},
    {S37_RAW(0x4F7190), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},
    {S37_RAW(0x4F6020), 0, {}, kG, 0, 0},
    {S37_RAW(0x4F60D0), 0, {}, kG, 0, 0},
    {S37_RAW(0x4F69C0), 0, {}, kG, 0, 0},
    {S37_RAW(0x4F6B60), 0, {}, kG, 0, 0},
    {S37_RAW(0x4F6F70), 0, {}, kG, 0, 0},
    {S37_RAW(0x4F7830), 0, {}, kG, 0, 0},
    {S37_RAW(0x4F79D0), 0, {}, kG, 0, 0},
    {S37_RAW(0x4F7E10), 0, {}, kG, 0, 0},
    {S37_RAW(0x4F8160), 0, {}, kG, 0, 0},
};
#undef S37_OURS
#undef S37_RAW

// The eleven .data handler tables the dispatchers read in place
// (MagmaBreathRecord_TaskTable and the rest, symbols.toml).
const mh::DataTable kTables[] = {
    {0x65C244, 1}, {0x65C248, 4}, {0x65C258, 1}, {0x65C25C, 8}, {0x65C27C, 1}, {0x65C280, 3},
    {0x65C28C, 4}, {0x65C29C, 6}, {0x65C2B4, 4}, {0x65C2C4, 3}, {0x65C2D0, 5},
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850..
    {kFrameSet, 4},                       // the frame-offset table pointer the Run steps swap
    {kMagmaPool, kMagmaBytes},            // MagmaBreath_Pool
    {kGeoPool, kGeoBytes},                // GeoBreath_Pool
    {0x80F980, 0x200},                    // Gfx_ClutStrip row 2
    {0x812980, 0x200},                    // Gfx_ClutStrip row 26
    {0x80B980, 0x200},                    // Gfx_ClutStripSource row 2
    {0x80E980, 0x200},                    // Gfx_ClutStripSource row 26
    {kField, 0x10},                       // Field_Kind2Z, Field_Kind2X, .., MapView_Redraw (0x905E69)
    {kShiftX, 2},                         // Camera_ShiftX
    {kCamera, 2},                         // Camera_Angles[0]
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* MagmaRecord(unsigned i) { return mh::Mem(kMagmaPool + (i & 31) * 0x84u); }
unsigned char* GeoRecord(unsigned i) { return mh::Mem(kGeoPool + (i & 63) * 0x84u); }

// A real slot or record for a pool record's owner (the walk makes it the
// owner cell, which the recorders write through).
const void* SomeOwner(std::uint32_t v) {
    return (v & 4) ? static_cast<const void*>(mh::SpriteRecord(v & 1)) : static_cast<const void*>(mh::TaskAt(v & 3));
}

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 16); break;
    case 3: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 4:
        if (v & 1) MagmaRecord(v >> 1)[0] ^= 1;
        else GeoRecord(v >> 1)[0] ^= 1;
        break;
    case 5: SetLong(mh::Mem(kField + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 6: mh::SetPointer(kActorRecord, mh::SpriteRecord(v)); break;
    case 7:
        if (v % 3 == 0) SetWord(mh::Mem(kShiftX), h >> 16);
        else if (v % 3 == 1) SetWord(mh::Mem(kCamera), h >> 16);
        else mh::Mem(kField + 9)[0] = Byte(h >> 24);
        break;
    default: break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

// Each pool record's owner a real one, its in-use bit as the fill left it.
void FillPools() {
    for (unsigned i = 0; i < 32; ++i) mh::SetPointer(kMagmaPool + i * 0x84u + 0x80, SomeOwner(mh::Next()));
    for (unsigned i = 0; i < 64; ++i) mh::SetPointer(kGeoPool + i * 0x84u + 0x80, SomeOwner(mh::Next()));
}
// For an allocator: a third of the time every record taken, a third all but
// one (and that one a boundary a third of those), else as the fill left it.
void FillForAlloc(unsigned char* (*record)(unsigned), unsigned records) {
    const unsigned mode = mh::Next() % 3;
    if (mode == 2) return;
    for (unsigned i = 0; i < records; ++i) record(i)[0] = static_cast<unsigned char>(record(i)[0] | 1);
    if (mode == 1) {
        const unsigned pick = mh::Next() % 3;
        const unsigned free = pick == 0 ? 0 : pick == 1 ? records - 1 : mh::Next() % records;
        record(free)[0] = static_cast<unsigned char>(record(free)[0] & ~1u);
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kActorRecord, mh::SpriteRecord(mh::Next()));
    FillPools();
    g_ratan_on = false;
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kMagmaBreath_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kGeoBreath_Task: case kCombustion_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kCombustionChild_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kMagmaBreathRecord_Task: case kGeoBreathChild_Task: case kGeoBreathRecord_Task: sc[1] = 0; break;
    case kMagmaBreathRecord_Run: case kCombustionGlow_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kGeoBreathChild_Run: sc[2] = Byte(mh::Next() % 8); break;
    case kGeoBreathRecord_Run: case kCombustionMote_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kCombustionSprite_Run: sc[2] = Byte(mh::Next() % 6); break;
    case kCombustionFlash_Run: sc[2] = Byte(mh::Next() % 5); break;
    // the allocators: full, all but one, or as filled
    case kMagmaBreath_PoolAlloc: FillForAlloc(&MagmaRecord, 32); break;
    case kGeoBreath_PoolAlloc: FillForAlloc(&GeoRecord, 64); break;
    // the counters: at their thresholds
    case kMagmaBreath_Strike: case kMagmaBreathRecord_Launch: case kGeoBreathChild_Swirl: case kGeoBreathChild_Shake:
    case kCombustion_Wait: case kCombustionSprite_Start: case kCombustionSprite_Fall: case kCombustionSprite_Flash:
    case kCombustionGlow_Start: case kCombustionMote_Grow: case kCombustionFlash_End: case kGeoBreathRecord_Fly:
    case kCombustionSprite_Shake:
        Near(sc[9], 1);
        break;
    case kMagmaBreathRecord_Burn:
        if (mh::Half()) sc[0xA] = Byte(mh::Next() & 1);
        Near(sc[9], 1);
        break;
    case kMagmaBreathRecord_Fly: {
        if (mh::Half()) sc[9] = 0;
        // the last angle, and Math_Ratan2's answer a turn of about 0x600 or
        // 0xA00 (or none) from it, either way, with any bits above 0xFFF
        const std::uint32_t last = mh::Next();
        SetLong(sc + 0x18, static_cast<std::int32_t>(last));
        const std::uint32_t turn = MH_PICK(0x600, 0x601, 0x5FF, 0xA00, 0x9FF, 0xA01, 0, 0x800);
        g_ratan = ((mh::Half() ? last + turn : last - turn) & 0xFFF) | (mh::Next() & 0xFFFFF000u);
        g_ratan_on = true;
        break;
    }
    case kGeoBreathChild_Burst:
        Near(sc[9], 1);
        if (mh::Often()) sc[0xA] = Byte(1 + mh::Next() % 3);
        break;
    case kGeoBreathChild_Quake: case kGeoBreathChild_Settle:
        if (mh::Often()) Frame_Counter &= ~7u;
        Near(sc[9], 1);
        break;
    case kGeoBreathChild_End:
        if (mh::Half()) sc[0x5D] = Byte(mh::Next() % 4);
        break;
    case kGeoBreathRecord_Fade:
        if (mh::Often()) sc[0xA] = Byte(1 + mh::Next() % 3);
        break;
    case kCombustionSprite_Fade:
        if (mh::Often()) sc[0x5D] = Byte(0x8F + mh::Next() % 3);
        break;
    case kCombustionGlow_Grow: Near(sc[9], 0xE); break;
    case kCombustionMote_Shrink: case kCombustionFlash_Hold: case kCombustionFlash_Spin: Near(sc[0xA], 1); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    const mh::Group group = {
        "magic_s37", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    mh::Run(group);
    g_ratan_on = false;
}

}  // namespace magic_s37
