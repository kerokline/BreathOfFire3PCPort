// BOF3X_SHADOW=magic_s10: group S10's five overlays (MAGIC052..MAGIC056)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s10.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC052 .. MAGIC056 --clones
// (2026-09-26; capstone, every jump internal, no jump table, no REFUSED
// line), names given. Beyond the standard set this group lists the draw
// callees (the GTE and libgpu entry points, Math_Sin / Math_Cos,
// Gfx_CommitPrim, MapView_LinkPrimAt), the sprite calls, Battle_ActorIsOut,
// Battle_PlayActorCue, the engine's 0x446770 / 0x435A20 / 0x435A70, the
// other units' 0x4F6020 / 0x4FA440 / 0x4F6290, and the functions of its own
// that its functions call directly. Everything the harness lacks is built
// here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes (every primitive of a draw is built in the same buffer, so the
//     state compare alone would see only the last) and move Gfx_PacketNext on
//     through a packet buffer of the fuzz's own, as the real ones do; the
//     projections log their SVECTORs through `deref`;
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair, so what the caller reads back is compared;
//   - the sprite calls that act on Sprite_Current (the script ticks, the
//     animation, the screen update) log which sprite - Howling and
//     Sacrifice make a party record Sprite_Current for one call - and the
//     screen update logs the frame-offset table 0x9039D8 the children swap;
//   - Battle_PlayActorCue logs Field_State, which Sacrifice points at the
//     member's record for the call;
//   - the pool alloc answers al (`ret_mask` 0xFF); its recorder answers
//     0..0x1F or 0xFF, as the real one;
//   - the pool walk makes each record Sprite_Current and its +0x80 the
//     owner, which the recorders write through: the seed keeps every
//     record's +0x80 a real slot or record;
//   - a `settle` keeps the record number +0xB of HowlingChild_End inside its
//     side's records (the original indexes them unchecked after a call).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s10.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s10 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC052 .. MAGIC056 --clones, 2026-09-26, names
// given.
// MAGIC052
// 0x4ABCA0: 0x2E bytes  Ovum_Task
constexpr mh::Imm kImms4ABCA0[] = {{0xF, 0x4ABCD0}, {0x17, 0x4ABCF0}, {0x22, 0x4ABDD0}};
// 0x4ABCD0: 0x1C bytes  Ovum_Start
// 0x4ABCF0: 0xD8 bytes  Ovum_Spawn
constexpr mh::CallSite kCalls4ABCF0[] = {{0x1E, 0x435180}, {0x56, 0x5B93D2}, {0x5B, 0x5B93D2}, {0x74, 0x5B93D2}};
// 0x4ABDD0: 0x2F bytes  Ovum_End
constexpr mh::CallSite kCalls4ABDD0[] = {{0x1A, 0x4530D0}, {0x29, 0x4351F0}};
// 0x4ABE00: 0x1A bytes  OvumChild_Task
constexpr mh::Imm kImms4ABE00[] = {{0xD, 0x4ABE20}};
// 0x4ABE20: 0x52 bytes  OvumChild_Run
constexpr mh::CallSite kCalls4ABE20[] = {{0x3F, 0x588F20}};
constexpr mh::Imm kImms4ABE20[] = {{0x19, 0x4ABE80}, {0x24, 0x4E8640}, {0x2C, 0x4AEE90}};
// 0x4ABE80: 0x97 bytes  OvumChild_Start
constexpr mh::CallSite kCalls4ABE80[] = {{0x7C, 0x587900}, {0x86, 0x5891F0}};
// MAGIC053
// 0x4ABF20: 0x7D bytes  Lavaburst_Task
constexpr mh::CallSite kCalls4ABF20[] = {{0x5B, 0x4AC6B0}};
constexpr mh::Imm kImms4ABF20[] = {{0x16, 0x4ABFA0}, {0x1E, 0x4F9F70}, {0x26, 0x4F7350}};
// 0x4ABFA0: 0x137 bytes  Lavaburst_Start
constexpr mh::CallSite kCalls4ABFA0[] = {{0x1C, 0x4FC0E0}, {0x21, 0x4FBD10}, {0x82, 0x435180}};
// 0x4AC0E0: 0x12 bytes; +0xB note: jmp through .data 0x65a9fc, 1 code entries (a data_tables entry)  LavaburstChild_Task
// 0x4AC100: 0x5A bytes; +0x15 note: call through .data 0x65aa00, 5 code entries (a data_tables entry)  LavaburstChild_Run
constexpr mh::CallSite kCalls4AC100[] = {{0x31, 0x4FBD10}, {0x36, 0x588F20}, {0x3B, 0x4F6020}, {0x40, 0x4AC510}, {0x45, 0x4FA440}, {0x4A, 0x5A7BC0}};
// 0x4AC160: 0x1F8 bytes  LavaburstChild_Launch
constexpr mh::CallSite kCalls4AC160[] = {{0x2C, 0x5B93D2}, {0x45, 0x5A7A00}, {0x70, 0x5A7A50}, {0xCE, 0x446770}, {0x12F, 0x446770}, {0x1CA, 0x5891F0}};
// 0x4AC360: 0x106 bytes  LavaburstChild_Rise
constexpr mh::CallSite kCalls4AC360[] = {{0x83, 0x587900}, {0x97, 0x4ACA50}, {0xE2, 0x5891F0}};
// 0x4AC470: 0x48 bytes  LavaburstChild_Shake
constexpr mh::CallSite kCalls4AC470[] = {{0x0, 0x589410}};
// 0x4AC4C0: 0x24 bytes  LavaburstChild_Settle
constexpr mh::CallSite kCalls4AC4C0[] = {{0x12, 0x589410}};
// 0x4AC4F0: 0x1A bytes  LavaburstChild_End
constexpr mh::CallSite kCalls4AC4F0[] = {{0x14, 0x4351F0}};
// 0x4AC510: 0x19F bytes  LavaburstChild_DrawGlow
constexpr mh::CallSite kCalls4AC510[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x43, 0x5A7A00}, {0x5C, 0x5A7A50}, {0x98, 0x5A75F0}, {0xA0, 0x5A7780}, {0xCE, 0x5A7A00}, {0xE7, 0x5A7A50}, {0x124, 0x5A84A0}, {0x12A, 0x5A9310}, {0x181, 0x461E50}};
// 0x4AC6B0: 0x12 bytes; +0xB note: jmp through .data 0x65aa14, 1 code entries (a data_tables entry)  LavaburstRecord_Task
// 0x4AC6D0: 0x33 bytes; +0xB note: call through .data 0x65aa18, 3 code entries (a data_tables entry)  LavaburstRecord_Run
constexpr mh::CallSite kCalls4AC6D0[] = {{0x23, 0x4B7D40}, {0x28, 0x4AC840}, {0x2D, 0x5A7BC0}};
// 0x4AC710: 0x89 bytes  LavaburstRecord_Grow
constexpr mh::CallSite kCalls4AC710[] = {{0x26, 0x5A7A00}, {0x4A, 0x5A7A50}};
// 0x4AC7A0: 0x91 bytes  LavaburstRecord_Shrink
constexpr mh::CallSite kCalls4AC7A0[] = {{0x24, 0x5A7A00}, {0x48, 0x5A7A50}, {0x8B, 0x4F6290}};
// 0x4AC840: 0x207 bytes  LavaburstRecord_Draw
constexpr mh::CallSite kCalls4AC840[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A75D0}, {0x30, 0x5A7780}, {0x5A, 0x5A7A50}, {0x77, 0x5A7A00}, {0x9B, 0x5A7A50}, {0xB8, 0x5A7A00}, {0xDC, 0x5A7A50}, {0xF9, 0x5A7A00}, {0x120, 0x5A7A50}, {0x13D, 0x5A7A00}, {0x169, 0x5A79A0}, {0x178, 0x5A79E0}, {0x1E7, 0x5A85F0}, {0x1F0, 0x5A9290}, {0x1F9, 0x461E50}};
// 0x4ACA50: 0x57 bytes  Lavaburst_PoolAlloc
// MAGIC054
// 0x4ACAB0: 0x36 bytes  Howling_Task
constexpr mh::Imm kImms4ACAB0[] = {{0xF, 0x4ACAF0}, {0x17, 0x4CF740}, {0x22, 0x43FE80}, {0x2A, 0x4D7BD0}};
// 0x4ACAF0: 0x1E2 bytes  Howling_Start
constexpr mh::CallSite kCalls4ACAF0[] = {{0x14, 0x452F70}, {0x6B, 0x4456C0}, {0x7E, 0x435A20}, {0x87, 0x435180}, {0x11A, 0x4456C0}, {0x13C, 0x5891F0}, {0x14B, 0x435180}, {0x1D2, 0x587900}};
// 0x4ACCE0: 0x12 bytes; +0xB note: jmp through .data 0x65aa24, 1 code entries (a data_tables entry)  HowlingChild_Task
// 0x4ACD00: 0x56 bytes  HowlingChild_Run
constexpr mh::CallSite kCalls4ACD00[] = {{0x4D, 0x588F20}};
constexpr mh::Imm kImms4ACD00[] = {{0xF, 0x4ACD60}, {0x17, 0x4ACE70}, {0x22, 0x4ACED0}, {0x2A, 0x4ACF10}, {0x32, 0x4ACF70}, {0x3A, 0x4AF490}};
// 0x4ACD60: 0x10F bytes  HowlingChild_Start
// 0x4ACE70: 0x5E bytes  HowlingChild_Out
constexpr mh::CallSite kCalls4ACE70[] = {{0x0, 0x5893A0}};
// 0x4ACED0: 0x3A bytes  HowlingChild_Hold
constexpr mh::CallSite kCalls4ACED0[] = {{0x0, 0x5893A0}};
// 0x4ACF10: 0x57 bytes  HowlingChild_Back
constexpr mh::CallSite kCalls4ACF10[] = {{0x0, 0x5893A0}};
// 0x4ACF70: 0x6B bytes  HowlingChild_End
constexpr mh::CallSite kCalls4ACF70[] = {{0x0, 0x589410}};
// MAGIC055
// 0x4ACFE0: 0x5C bytes  Ebonfire_Task
constexpr mh::CallSite kCalls4ACFE0[] = {{0x35, 0x4FBD10}, {0x44, 0x4C54F0}, {0x49, 0x4B7D40}, {0x4E, 0x4AD6F0}, {0x53, 0x5A7BC0}};
constexpr mh::Imm kImms4ACFE0[] = {{0xF, 0x4AD040}, {0x17, 0x4AD130}, {0x22, 0x4AD160}};
// 0x4AD040: 0xEC bytes  Ebonfire_Start
constexpr mh::CallSite kCalls4AD040[] = {{0x47, 0x435180}, {0xBA, 0x587900}, {0xC8, 0x452F70}};
// 0x4AD1C0: 0x33 bytes; +0xB note: call through .data 0x65aa28, 4 code entries (a data_tables entry)  EbonfireRing_Task
constexpr mh::CallSite kCalls4AD1C0[] = {{0x23, 0x4B7D40}, {0x28, 0x4AD350}, {0x2D, 0x5A7BC0}};
// 0x4AD300: 0x43 bytes  EbonfireRing_End
constexpr mh::CallSite kCalls4AD300[] = {{0x3D, 0x4351F0}};
// 0x4AD350: 0x39C bytes  EbonfireRing_Draw
constexpr mh::CallSite kCalls4AD350[] = {{0x54, 0x5A7A50}, {0x6D, 0x5A7A00}, {0x8D, 0x5A7A00}, {0xAE, 0x5A7A50}, {0xC7, 0x5A7A00}, {0x13E, 0x5A7A50}, {0x157, 0x5A7A00}, {0x177, 0x5A7A00}, {0x1B9, 0x5A7A50}, {0x1D2, 0x5A7A00}, {0x218, 0x5A77C0}, {0x223, 0x572FA0}, {0x22F, 0x5A7630}, {0x237, 0x5A7780}, {0x24D, 0x5A79A0}, {0x25D, 0x5A79E0}, {0x35D, 0x5A85F0}, {0x366, 0x5A93A0}, {0x371, 0x572FA0}};
// MAGIC056
// 0x4AD8A0: 0x2E bytes  Sacrifice_Task
constexpr mh::Imm kImms4AD8A0[] = {{0xF, 0x4AD8D0}, {0x17, 0x4ADA30}, {0x22, 0x43FE80}};
// 0x4AD8D0: 0x15B bytes  Sacrifice_Start
constexpr mh::CallSite kCalls4AD8D0[] = {{0x3, 0x4FC0E0}, {0x26, 0x4FB830}, {0x2F, 0x435180}, {0x10A, 0x4FBF50}, {0x143, 0x4FBE30}};
// 0x4ADA30: 0xC0 bytes  Sacrifice_Wait
constexpr mh::CallSite kCalls4ADA30[] = {{0x18, 0x4FC000}, {0x21, 0x4FB830}, {0x2D, 0x4530D0}, {0xAF, 0x454DC0}};
// 0x4ADAF0: 0x12 bytes; +0xB note: jmp through .data 0x65aa38, 4 code entries (a data_tables entry)  SacrificeChild_Task
// 0x4ADB10: 0x33 bytes; +0xB note: call through .data 0x65aa48, 3 code entries (a data_tables entry)  SacrificeRing_Run
constexpr mh::CallSite kCalls4ADB10[] = {{0x23, 0x4B7D40}, {0x28, 0x4ADC40}, {0x2D, 0x5A7BC0}};
// 0x4ADB50: 0x50 bytes  SacrificeFx_TakeOwnerPos
// 0x4ADBA0: 0x4F bytes  SacrificeRing_Grow
constexpr mh::CallSite kCalls4ADBA0[] = {{0x13, 0x452F70}};
// 0x4ADBF0: 0x46 bytes  SacrificeRing_Fade
constexpr mh::CallSite kCalls4ADBF0[] = {{0x40, 0x4351F0}};
// 0x4ADC40: 0x416 bytes  SacrificeRing_Draw
constexpr mh::CallSite kCalls4ADC40[] = {{0xD8, 0x5A7A50}, {0xF1, 0x5A7A50}, {0x10B, 0x5A7A00}, {0x125, 0x5A7A50}, {0x13E, 0x5A7A00}, {0x15E, 0x5A7A00}, {0x178, 0x5A7A50}, {0x191, 0x5A7A00}, {0x1F0, 0x5A7A00}, {0x209, 0x5A7A50}, {0x23C, 0x5A7A00}, {0x255, 0x5A7A50}, {0x29B, 0x5A77C0}, {0x2A6, 0x572FA0}, {0x2B2, 0x5A7630}, {0x2BA, 0x5A7780}, {0x2F0, 0x5A85F0}, {0x2F6, 0x5A93A0}, {0x309, 0x5A79A0}, {0x319, 0x5A79E0}, {0x3C1, 0x572FA0}};
// 0x4AE060: 0x33 bytes; +0xB note: call through .data 0x65aa54, 4 code entries (a data_tables entry)  SacrificeDisc_Run
constexpr mh::CallSite kCalls4AE060[] = {{0x23, 0x4B7D40}, {0x28, 0x4AE130}, {0x2D, 0x5A7BC0}};
// 0x4AE0A0: 0x2A bytes  SacrificeDisc_Grow
// 0x4AE0D0: 0x1C bytes  SacrificeDisc_Hold
// 0x4AE0F0: 0x3A bytes  SacrificeDisc_Fade
constexpr mh::CallSite kCalls4AE0F0[] = {{0x34, 0x4351F0}};
// 0x4AE130: 0x1E6 bytes  SacrificeDisc_Draw
constexpr mh::CallSite kCalls4AE130[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x37, 0x5A7A50}, {0x61, 0x5A7A50}, {0xA9, 0x5A7A00}, {0xC2, 0x5A7A50}, {0xFE, 0x5A75F0}, {0x106, 0x5A7780}, {0x134, 0x5A7A00}, {0x14D, 0x5A7A50}, {0x1B6, 0x5A84A0}, {0x1BC, 0x5A9310}, {0x1C5, 0x461E50}};
// 0x4AE320: 0x74 bytes  SacrificeActor_Run
constexpr mh::CallSite kCalls4AE320[] = {{0x6B, 0x588F20}};
constexpr mh::Imm kImms4AE320[] = {{0xF, 0x4AE3A0}, {0x17, 0x4AE3C0}, {0x22, 0x4AE410}, {0x2A, 0x4AE480}, {0x32, 0x4AE4E0}, {0x3A, 0x4AE620}, {0x42, 0x4AE680}, {0x4A, 0x4AE760}, {0x52, 0x4AEE90}};
// 0x4AE3A0: 0x1A bytes  SacrificeActor_Start
// 0x4AE3C0: 0x44 bytes  SacrificeActor_Play
constexpr mh::CallSite kCalls4AE3C0[] = {{0x0, 0x589410}};
// 0x4AE410: 0x6B bytes  SacrificeActor_Darken
// 0x4AE480: 0x58 bytes  SacrificeActor_Lighten
// 0x4AE4E0: 0x13C bytes  SacrificeActor_Split
constexpr mh::CallSite kCalls4AE4E0[] = {{0x23, 0x435180}, {0x5A, 0x435180}, {0x92, 0x587900}, {0xC6, 0x446A50}, {0x103, 0x5891F0}, {0x115, 0x5891F0}, {0x11F, 0x435A70}};
// 0x4AE620: 0x51 bytes  SacrificeActor_Wait
constexpr mh::CallSite kCalls4AE620[] = {{0x0, 0x5893A0}};
// 0x4AE680: 0xD9 bytes  SacrificeActor_Return
constexpr mh::CallSite kCalls4AE680[] = {{0x0, 0x5893A0}, {0x9C, 0x5891F0}, {0xAE, 0x5891F0}, {0xC8, 0x435A70}};
// 0x4AE760: 0x57 bytes  SacrificeActor_End
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Ovum_Task", 0x4ABCA0, 0x2E, nullptr, 0, kImms4ABCA0, MH_N(kImms4ABCA0), nullptr, 0, reinterpret_cast<const void*>(&::Ovum_Task)},
    {"Ovum_Start", 0x4ABCD0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Ovum_Start)},
    {"Ovum_Spawn", 0x4ABCF0, 0xD8, kCalls4ABCF0, MH_N(kCalls4ABCF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Ovum_Spawn)},
    {"Ovum_End", 0x4ABDD0, 0x2F, kCalls4ABDD0, MH_N(kCalls4ABDD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Ovum_End)},
    {"OvumChild_Task", 0x4ABE00, 0x1A, nullptr, 0, kImms4ABE00, MH_N(kImms4ABE00), nullptr, 0, reinterpret_cast<const void*>(&::OvumChild_Task)},
    {"OvumChild_Run", 0x4ABE20, 0x52, kCalls4ABE20, MH_N(kCalls4ABE20), kImms4ABE20, MH_N(kImms4ABE20), nullptr, 0, reinterpret_cast<const void*>(&::OvumChild_Run)},
    {"OvumChild_Start", 0x4ABE80, 0x97, kCalls4ABE80, MH_N(kCalls4ABE80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::OvumChild_Start)},
    {"Lavaburst_Task", 0x4ABF20, 0x7D, kCalls4ABF20, MH_N(kCalls4ABF20), kImms4ABF20, MH_N(kImms4ABF20), nullptr, 0, reinterpret_cast<const void*>(&::Lavaburst_Task)},
    {"Lavaburst_Start", 0x4ABFA0, 0x137, kCalls4ABFA0, MH_N(kCalls4ABFA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Lavaburst_Start)},
    {"LavaburstChild_Task", 0x4AC0E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstChild_Task)},
    {"LavaburstChild_Run", 0x4AC100, 0x5A, kCalls4AC100, MH_N(kCalls4AC100), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstChild_Run)},
    {"LavaburstChild_Launch", 0x4AC160, 0x1F8, kCalls4AC160, MH_N(kCalls4AC160), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstChild_Launch)},
    {"LavaburstChild_Rise", 0x4AC360, 0x106, kCalls4AC360, MH_N(kCalls4AC360), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstChild_Rise)},
    {"LavaburstChild_Shake", 0x4AC470, 0x48, kCalls4AC470, MH_N(kCalls4AC470), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstChild_Shake)},
    {"LavaburstChild_Settle", 0x4AC4C0, 0x24, kCalls4AC4C0, MH_N(kCalls4AC4C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstChild_Settle)},
    {"LavaburstChild_End", 0x4AC4F0, 0x1A, kCalls4AC4F0, MH_N(kCalls4AC4F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstChild_End)},
    {"LavaburstChild_DrawGlow", 0x4AC510, 0x19F, kCalls4AC510, MH_N(kCalls4AC510), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstChild_DrawGlow)},
    {"LavaburstRecord_Task", 0x4AC6B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstRecord_Task)},
    {"LavaburstRecord_Run", 0x4AC6D0, 0x33, kCalls4AC6D0, MH_N(kCalls4AC6D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstRecord_Run)},
    {"LavaburstRecord_Grow", 0x4AC710, 0x89, kCalls4AC710, MH_N(kCalls4AC710), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstRecord_Grow)},
    {"LavaburstRecord_Shrink", 0x4AC7A0, 0x91, kCalls4AC7A0, MH_N(kCalls4AC7A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstRecord_Shrink)},
    {"LavaburstRecord_Draw", 0x4AC840, 0x207, kCalls4AC840, MH_N(kCalls4AC840), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::LavaburstRecord_Draw)},
    {"Lavaburst_PoolAlloc", 0x4ACA50, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Lavaburst_PoolAlloc), 0xFF},
    {"Howling_Task", 0x4ACAB0, 0x36, nullptr, 0, kImms4ACAB0, MH_N(kImms4ACAB0), nullptr, 0, reinterpret_cast<const void*>(&::Howling_Task)},
    {"Howling_Start", 0x4ACAF0, 0x1E2, kCalls4ACAF0, MH_N(kCalls4ACAF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Howling_Start)},
    {"HowlingChild_Task", 0x4ACCE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HowlingChild_Task)},
    {"HowlingChild_Run", 0x4ACD00, 0x56, kCalls4ACD00, MH_N(kCalls4ACD00), kImms4ACD00, MH_N(kImms4ACD00), nullptr, 0, reinterpret_cast<const void*>(&::HowlingChild_Run)},
    {"HowlingChild_Start", 0x4ACD60, 0x10F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HowlingChild_Start)},
    {"HowlingChild_Out", 0x4ACE70, 0x5E, kCalls4ACE70, MH_N(kCalls4ACE70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HowlingChild_Out)},
    {"HowlingChild_Hold", 0x4ACED0, 0x3A, kCalls4ACED0, MH_N(kCalls4ACED0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HowlingChild_Hold)},
    {"HowlingChild_Back", 0x4ACF10, 0x57, kCalls4ACF10, MH_N(kCalls4ACF10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HowlingChild_Back)},
    {"HowlingChild_End", 0x4ACF70, 0x6B, kCalls4ACF70, MH_N(kCalls4ACF70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::HowlingChild_End)},
    {"Ebonfire_Task", 0x4ACFE0, 0x5C, kCalls4ACFE0, MH_N(kCalls4ACFE0), kImms4ACFE0, MH_N(kImms4ACFE0), nullptr, 0, reinterpret_cast<const void*>(&::Ebonfire_Task)},
    {"Ebonfire_Start", 0x4AD040, 0xEC, kCalls4AD040, MH_N(kCalls4AD040), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Ebonfire_Start)},
    {"EbonfireRing_Task", 0x4AD1C0, 0x33, kCalls4AD1C0, MH_N(kCalls4AD1C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EbonfireRing_Task)},
    {"EbonfireRing_End", 0x4AD300, 0x43, kCalls4AD300, MH_N(kCalls4AD300), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EbonfireRing_End)},
    {"EbonfireRing_Draw", 0x4AD350, 0x39C, kCalls4AD350, MH_N(kCalls4AD350), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EbonfireRing_Draw)},
    {"Sacrifice_Task", 0x4AD8A0, 0x2E, nullptr, 0, kImms4AD8A0, MH_N(kImms4AD8A0), nullptr, 0, reinterpret_cast<const void*>(&::Sacrifice_Task)},
    {"Sacrifice_Start", 0x4AD8D0, 0x15B, kCalls4AD8D0, MH_N(kCalls4AD8D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Sacrifice_Start)},
    {"Sacrifice_Wait", 0x4ADA30, 0xC0, kCalls4ADA30, MH_N(kCalls4ADA30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Sacrifice_Wait)},
    {"SacrificeChild_Task", 0x4ADAF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeChild_Task)},
    {"SacrificeRing_Run", 0x4ADB10, 0x33, kCalls4ADB10, MH_N(kCalls4ADB10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeRing_Run)},
    {"SacrificeFx_TakeOwnerPos", 0x4ADB50, 0x50, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeFx_TakeOwnerPos)},
    {"SacrificeRing_Grow", 0x4ADBA0, 0x4F, kCalls4ADBA0, MH_N(kCalls4ADBA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeRing_Grow)},
    {"SacrificeRing_Fade", 0x4ADBF0, 0x46, kCalls4ADBF0, MH_N(kCalls4ADBF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeRing_Fade)},
    {"SacrificeRing_Draw", 0x4ADC40, 0x416, kCalls4ADC40, MH_N(kCalls4ADC40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeRing_Draw)},
    {"SacrificeDisc_Run", 0x4AE060, 0x33, kCalls4AE060, MH_N(kCalls4AE060), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeDisc_Run)},
    {"SacrificeDisc_Grow", 0x4AE0A0, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeDisc_Grow)},
    {"SacrificeDisc_Hold", 0x4AE0D0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeDisc_Hold)},
    {"SacrificeDisc_Fade", 0x4AE0F0, 0x3A, kCalls4AE0F0, MH_N(kCalls4AE0F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeDisc_Fade)},
    {"SacrificeDisc_Draw", 0x4AE130, 0x1E6, kCalls4AE130, MH_N(kCalls4AE130), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeDisc_Draw)},
    {"SacrificeActor_Run", 0x4AE320, 0x74, kCalls4AE320, MH_N(kCalls4AE320), kImms4AE320, MH_N(kImms4AE320), nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_Run)},
    {"SacrificeActor_Start", 0x4AE3A0, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_Start)},
    {"SacrificeActor_Play", 0x4AE3C0, 0x44, kCalls4AE3C0, MH_N(kCalls4AE3C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_Play)},
    {"SacrificeActor_Darken", 0x4AE410, 0x6B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_Darken)},
    {"SacrificeActor_Lighten", 0x4AE480, 0x58, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_Lighten)},
    {"SacrificeActor_Split", 0x4AE4E0, 0x13C, kCalls4AE4E0, MH_N(kCalls4AE4E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_Split)},
    {"SacrificeActor_Wait", 0x4AE620, 0x51, kCalls4AE620, MH_N(kCalls4AE620), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_Wait)},
    {"SacrificeActor_Return", 0x4AE680, 0xD9, kCalls4AE680, MH_N(kCalls4AE680), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_Return)},
    {"SacrificeActor_End", 0x4AE760, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SacrificeActor_End)},
};
#undef MH_N

enum : unsigned {
    kOvum_Task, kOvum_Start, kOvum_Spawn, kOvum_End, kOvumChild_Task, kOvumChild_Run, kOvumChild_Start,
    kLavaburst_Task, kLavaburst_Start, kLavaburstChild_Task, kLavaburstChild_Run, kLavaburstChild_Launch,
    kLavaburstChild_Rise, kLavaburstChild_Shake, kLavaburstChild_Settle, kLavaburstChild_End, kLavaburstChild_DrawGlow,
    kLavaburstRecord_Task, kLavaburstRecord_Run, kLavaburstRecord_Grow, kLavaburstRecord_Shrink, kLavaburstRecord_Draw,
    kLavaburst_PoolAlloc,
    kHowling_Task, kHowling_Start, kHowlingChild_Task, kHowlingChild_Run, kHowlingChild_Start, kHowlingChild_Out,
    kHowlingChild_Hold, kHowlingChild_Back, kHowlingChild_End,
    kEbonfire_Task, kEbonfire_Start, kEbonfireRing_Task, kEbonfireRing_End, kEbonfireRing_Draw,
    kSacrifice_Task, kSacrifice_Start, kSacrifice_Wait, kSacrificeChild_Task, kSacrificeRing_Run,
    kSacrificeFx_TakeOwnerPos, kSacrificeRing_Grow, kSacrificeRing_Fade, kSacrificeRing_Draw, kSacrificeDisc_Run,
    kSacrificeDisc_Grow, kSacrificeDisc_Hold, kSacrificeDisc_Fade, kSacrificeDisc_Draw, kSacrificeActor_Run,
    kSacrificeActor_Start, kSacrificeActor_Play, kSacrificeActor_Darken, kSacrificeActor_Lighten, kSacrificeActor_Split,
    kSacrificeActor_Wait, kSacrificeActor_Return, kSacrificeActor_End, kCount
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
                        kFrameSet = 0x9039D8, kCamera = 0x929EC8, kFieldState = 0x905D98, kPool = 0x67F700,
                        kPoolBytes = 32 * 0x84;

// The function being fuzzed, for the settle.
unsigned g_k = 0;

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
// The sprite calls that act on Sprite_Current: which sprite; the screen
// update, the frame-offset table too.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
std::uint32_t NoteSpriteFrames(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
    return answer;
}
// Battle_PlayActorCue reads its sound table through Field_State.
std::uint32_t NoteFieldState(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(static_cast<std::uint32_t>(Long(mh::Mem(kFieldState))));
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
#define S10_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S10_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S10_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S10_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    // the sprite and battle calls
    {S10_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S10_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S10_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    {S10_OURS(Battle_PlayActorCue), 1, {kU8}, kG, 0, 0, {}, &NoteFieldState},
    // the draw library (all ours)
    {S10_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S10_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S10_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S10_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S10_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S10_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S10_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S10_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    {S10_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S10_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S10_OURS(Gpu_SetPolyGT4), 1, {kAll}, kG, 0, 0},
    // the projections: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S10_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S10_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S10_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S10_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0},
    {S10_OURS(Gte_PrimDepths4_14), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed, in no group: the dx / dz turn by direction; the
    // enemy animations (the index's low byte, the animation passed on)
    {S10_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    {S10_RAW(0x435A20), 2, {kU8, kAll}, kG, 0, 0},
    {S10_RAW(0x435A70), 2, {kU8, kAll}, kG, 0, 0},
    // other units', not yet ours: MAGIC219's two, MAGIC226/227's draw
    {S10_RAW(0x4F6020), 0, {}, kG, 0, 0},
    {S10_RAW(0x4FA440), 0, {}, kG, 0, 0},
    {S10_RAW(0x4F6290), 0, {}, kG, 0, 0},
    // this group's own, called directly
    {S10_RAW(0x4AC6B0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S10_RAW(0x4AC510), 0, {}, kG, 0, 0},
    {S10_RAW(0x4AC840), 0, {}, kG, 0, 0},
    {S10_RAW(0x4ACA50), 0, {}, mh::Answer::kByte, 0xFF, 0x1F},
    {S10_RAW(0x4AD350), 0, {}, kG, 0, 0},
    {S10_RAW(0x4ADC40), 0, {}, kG, 0, 0},
    {S10_RAW(0x4AE130), 0, {}, kG, 0, 0},
};
#undef S10_OURS
#undef S10_RAW

// The .data handler tables the dispatchers read in place (symbols.toml).
const mh::DataTable kTables[] = {
    {0x65A9FC, 1},   // LavaburstChild_TaskTable
    {0x65AA00, 5},   // LavaburstChild_Steps
    {0x65AA14, 1},   // LavaburstRecord_TaskTable
    {0x65AA18, 3},   // LavaburstRecord_Steps
    {0x65AA24, 1},   // HowlingChild_TaskTable
    {0x65AA28, 4},   // FxRing_PhasesTwin
    {0x65AA38, 4},   // SacrificeChild_Kinds
    {0x65AA48, 3},   // SacrificeRing_Steps
    {0x65AA54, 4},   // SacrificeDisc_Steps
};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {kVertex, 0x20},                      // Prim_VertexScratch, four SVECTORs
    {kScratch, 0x10},                     // 0x903850..
    {kFrameSet, 4},                       // the frame-offset table pointer the children swap
    {kCamera, 2},                         // Camera_Angles[0]
    {kFieldState, 4},                     // Field_State
    {kPool, kPoolBytes},                  // Lavaburst_Pool
    {0x812980, 0x200},                    // Gfx_ClutStrip row 26
    {0x80E980, 0x200},                    // Gfx_ClutStripSource row 26
    {0x80F980, 0x20},                     // Gfx_ClutStrip row 2, the first 16 words
    {0x80B980, 0x20},                     // Gfx_ClutStripSource row 2, the same
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* PoolRecord(unsigned i) { return mh::Mem(kPool + (i & 31) * 0x84u); }

// A real slot or record for a pool record's owner (the walk makes it the
// owner cell, which the recorders write through).
const void* SomeOwner(std::uint32_t v) {
    return (v & 4) ? static_cast<const void*>(mh::SpriteRecord(v & 1)) : static_cast<const void*>(mh::TaskAt(v & 3));
}

// The group's cells a recorder may move (the harness's case 14):
// Gfx_PacketNext, a vertex word, a scratch word, the frame-offset table, the
// camera angle, a pool record's live bit, the actor's sprite pointer, the
// event-battle byte.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kVertex + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 16); break;
    case 3: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 4: SetWord(mh::Mem(kCamera), h >> 16); break;
    case 5: PoolRecord(v)[0] ^= 1; break;
    case 6: mh::SetPointer(kActorRecord, mh::SpriteRecord(v)); break;
    case 7: mh::Mem(kEventBattle)[0] = Byte(h >> 24); break;
    default: break;
    }
}

// HowlingChild_End reads its record number +0xB after a call and indexes the
// side's records by it, unchecked: kept inside them.
void Settle() {
    if (g_k != kHowlingChild_End) return;
    const bool enemies = (mh::Mem(mh::at::kTarget)[0] & 0x40) != 0;
    Sc()[0xB] = Byte(Sc()[0xB] % (enemies ? 8u : 3u));
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

void FillPool() {
    const unsigned mode = mh::Next() % 4;
    const unsigned taken = mh::Next() % 33;
    for (unsigned i = 0; i < 32; ++i) {
        unsigned char* const rec = PoolRecord(i);
        if (mode == 0) rec[0] = Byte(rec[0] | 1);                                   // full
        else if (mode == 1) rec[0] = Byte(i < taken ? rec[0] | 1 : rec[0] & ~1u);   // the first `taken`
        mh::SetPointer(kPool + i * 0x84u + 0x80, SomeOwner(mh::Next()));
    }
}

void Seed(unsigned k) {
    g_k = k;
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(kActorRecord, mh::SpriteRecord(mh::Next()));
    FillPool();
    if (mh::Half()) mh::Mem(mh::at::kTarget)[0] = Byte(mh::Mem(mh::at::kTarget)[0] | 0x40);
    if (mh::Often()) mh::Mem(mh::at::kActor)[0] = Byte(mh::Next() % 11);
    const bool enemies = (mh::Mem(mh::at::kTarget)[0] & 0x40) != 0;
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kOvum_Task: case kSacrifice_Task: case kEbonfire_Task: case kLavaburst_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kHowling_Task: case kEbonfireRing_Task: case kSacrificeChild_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kOvumChild_Task: case kLavaburstChild_Task: case kLavaburstRecord_Task: case kHowlingChild_Task: sc[1] = 0; break;
    case kOvumChild_Run: case kLavaburstRecord_Run: case kSacrificeRing_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kSacrificeDisc_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kLavaburstChild_Run:
        sc[2] = Byte(mh::Next() % 5);
        if (mh::Half()) sc[0] = Byte(sc[0] ^ 1);
        break;
    case kHowlingChild_Run: sc[2] = Byte(mh::Next() % 6); break;
    case kSacrificeActor_Run: sc[2] = Byte(mh::Next() % 9); break;
    // the counters: at their thresholds
    case kOvum_Spawn:
        if (mh::Often()) sc[0xA] = Byte(mh::Next() % 2);
        if (mh::Often()) sc[9] = Byte(mh::Next() % 2);
        break;
    case kOvum_End: if (mh::Often()) sc[0xA] = Byte(mh::Next() % 2); break;
    case kOvumChild_Start: sc[9] = Byte(mh::Next() % 3); break;
    case kLavaburstChild_Launch: case kLavaburstChild_Rise: case kLavaburstChild_Shake: case kLavaburstRecord_Grow:
    case kHowlingChild_Hold: case kSacrificeActor_Split: case kSacrificeActor_Wait:
        if (mh::Often()) sc[9] = Byte(1 + mh::Next() % 2);
        break;
    case kLavaburstChild_Settle: if (mh::Half()) sc[0xA] = 0; break;
    case kLavaburstChild_End: if (mh::Often()) sc[4] = 0; break;
    case kLavaburstRecord_Shrink: if (mh::Often()) sc[0xA] = Byte(1 + mh::Next() % 2); break;
    case kEbonfireRing_End: Near(sc[0xA], 0xE); break;
    case kSacrifice_Wait: if (mh::Often()) sc[0xB] = 0; break;
    case kSacrificeRing_Grow:
        if (mh::Often()) {
            static const unsigned char kAt[] = {3, 4, 5, 0xB, 0xC};
            sc[9] = kAt[mh::Next() % 5];
        }
        break;
    case kSacrificeRing_Fade: Near(sc[9], 0x26); break;
    case kSacrificeRing_Draw: sc[9] = Byte(mh::Often() ? 0x10 + mh::Next() % 0x20 : mh::Next()); break;
    case kSacrificeDisc_Grow: Near(sc[9], 0xA); break;
    case kSacrificeDisc_Hold: Near(sc[9], 0x1C); break;
    case kSacrificeDisc_Fade:
        if (mh::Half()) sc[9] = Byte(0x2D + mh::Next() % 2);
        if (mh::Often()) sc[0xA] = Byte(1 + mh::Next() % 2);
        break;
    case kSacrificeDisc_Draw: sc[2] = Byte(mh::Next() % 3); break;
    case kSacrificeActor_Darken: case kSacrificeActor_Return:
        if (mh::Often()) sc[0x5D] = Byte(0x90 + (mh::Half() ? 0x10 : 0));
        break;
    case kSacrificeActor_Lighten: case kSacrificeActor_End:
        if (mh::Often()) sc[0x5D] = Byte(0xF0 - (mh::Half() ? 0x10 : 0));
        if (k == kSacrificeActor_End && mh::Often()) mh::Pointer(mh::at::kOwner)[0xB] = 1;
        break;
    // Howling: the event battle of kind 0x37; the record number inside the
    // side; the scale step one frame from its end
    case kHowling_Start: if (mh::Half()) mh::Mem(kEventBattle)[0] = mh::Half() ? 0x37 : 0x36; break;
    case kHowlingChild_Start: case kHowlingChild_End: sc[0xB] = Byte(mh::Next() % (enemies ? 8 : 3)); break;
    case kHowlingChild_Out: case kHowlingChild_Back:
        if (mh::Often()) {
            const std::uint32_t end = k == kHowlingChild_Out ? 0u : 0xC00u;
            SetLong(sc + 0xC, static_cast<std::int32_t>(end - static_cast<std::uint32_t>(Long(sc + 0x18)) +
                                                        (mh::Half() ? 1u : 0u)));
        }
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s10", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.settle = &Settle;
    mh::Run(group);
}

}  // namespace magic_s10
