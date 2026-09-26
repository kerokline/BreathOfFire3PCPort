// BOF3X_SHADOW=magic_s29: group S29's two overlays (MAGIC125, MAGIC126)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s29.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC125 / MAGIC126 --clones
// (2026-09-26; no jump tables, no REFUSED lines), the placeholders renamed.
// Beyond the harness's standard set this lists: the GTE / GPU / Math callees
// as recorders (the vertices logged by what they hold, `deref`), the two
// commits with an `effect` that logs the primitive they link (every primitive
// of a draw is built in the same bytes, so the regions at the end hold only
// the last), Math_Ratan2 with an `effect` that lands the seeker's turn on its
// bounds, MagicFx_NearSprite answering a whole-eax bool, the CRT's _ftol left
// to the copy (Answer::kThrough), the two raw callees of other units, and the
// group's own functions called directly (Answer::kPhase, the pool's current
// record logged for MAGIC126's); the eleven .data tables of handlers, in three
// runs; and every region the functions touch beyond the standard ones: the
// scratch, the vertices, the packet pointer and a buffer of the fuzz's own, the
// two pools and the current record, the CLUT row. No harness edits.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s29.h"
#include "game/magic_s29_callees.h"
#include "game/move_script_bytes.h"

namespace magic_s29 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;

// 0x4E0910: 0x75 bytes
constexpr mh::CallSite kCalls4E0910[] = {{0x53, 0x4E1240}};
constexpr mh::Imm kImms4E0910[] = {{0x16, 0x4E0990}, {0x1E, 0x4F7350}};
// 0x4E0990: 0xC8 bytes
constexpr mh::CallSite kCalls4E0990[] = {{0x1E, 0x4FC0E0}, {0x37, 0x435180}, {0x79, 0x435180}, {0xBD, 0x587900}};
// 0x4E0A60: 0x12 bytes; +0xB note: jmp through .data 0x65bba4, 26 code entries (a data_tables entry)
// 0x4E0A80: 0x77 bytes; +0xB note: call through .data 0x65bbac, 24 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E0A80[] = {{0x2C, 0x5B93D2}, {0x3E, 0x5B93D2}, {0x50, 0x5B93D2}, {0x62, 0x4B7D40}, {0x67, 0x4E0C20}, {0x6C, 0x4E0E00}, {0x71, 0x5A7BC0}};
// 0x4E0B00: 0xB0 bytes
constexpr mh::CallSite kCalls4E0B00[] = {{0x51, 0x5B93D2}, {0x63, 0x5B93D2}, {0x75, 0x5B93D2}};
// 0x4E0BB0: 0x2E bytes
// 0x4E0BE0: 0x35 bytes
constexpr mh::CallSite kCalls4E0BE0[] = {{0x2F, 0x4351F0}};
// 0x4E0C20: 0x1D7 bytes
constexpr mh::CallSite kCalls4E0C20[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0x35, 0x5A7A00}, {0x4E, 0x5A7A50}, {0xDD, 0x5A7A00}, {0xF6, 0x5A7A50}, {0x12A, 0x5A75F0}, {0x132, 0x5A7780}, {0x15C, 0x5A84A0}, {0x162, 0x5A9310}, {0x197, 0x461E50}, {0x1BE, 0x5A77C0}, {0x1C7, 0x461E50}};
// 0x4E0E00: 0x223 bytes
constexpr mh::CallSite kCalls4E0E00[] = {{0x1A, 0x5A7A00}, {0x33, 0x5A7A50}, {0xD4, 0x5A7A00}, {0xED, 0x5A7A50}, {0x150, 0x5A77C0}, {0x15A, 0x572FA0}, {0x166, 0x5A7610}, {0x16E, 0x5A7780}, {0x1ED, 0x5A85F0}, {0x1F6, 0x5A9350}, {0x201, 0x572FA0}};
// 0x4E1030: 0x33 bytes; +0xB note: call through .data 0x65bbb8, 21 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E1030[] = {{0x23, 0x4B7D40}, {0x28, 0x4E0E00}, {0x2D, 0x5A7BC0}};
// 0x4E1070: 0x9E bytes
// 0x4E1110: 0xD1 bytes
constexpr mh::CallSite kCalls4E1110[] = {{0x3F, 0x452F70}, {0x49, 0x587900}, {0x55, 0x4E1940}, {0x8A, 0x5B93D2}};
// 0x4E11F0: 0x41 bytes
constexpr mh::CallSite kCalls4E11F0[] = {{0x3B, 0x4351F0}};
// 0x4E1240: 0x12 bytes; +0xB note: jmp through .data 0x65bbc4, 18 code entries (a data_tables entry)
// 0x4E1260: 0x89 bytes; +0xB note: call through .data 0x65bbc8, 17 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E1260[] = {{0x32, 0x5A77C0}, {0x48, 0x572FA0}, {0x4D, 0x4FBD10}, {0x52, 0x4E1510}, {0x57, 0x4E16E0}, {0x6A, 0x5A77C0}, {0x80, 0x572FA0}};
// 0x4E12F0: 0x135 bytes
constexpr mh::CallSite kCalls4E12F0[] = {{0x7A, 0x5A7A00}, {0xA8, 0x5A7A50}, {0xE3, 0x5B93D2}, {0xF5, 0x5B93D2}, {0x107, 0x5B93D2}};
// 0x4E1430: 0xD1 bytes
constexpr mh::CallSite kCalls4E1430[] = {{0x62, 0x5A7A00}, {0x8B, 0x5A7A50}, {0xCB, 0x4F6290}};
// 0x4E1510: 0x1C5 bytes
constexpr mh::CallSite kCalls4E1510[] = {{0x17, 0x5A7A00}, {0x3B, 0x5A7A50}, {0x9C, 0x5A75F0}, {0xA4, 0x5A7780}, {0xF7, 0x5A7A00}, {0x128, 0x5A7A50}, {0x1A6, 0x572FA0}};
// 0x4E16E0: 0x255 bytes
constexpr mh::CallSite kCalls4E16E0[] = {{0x25, 0x5A7A00}, {0x49, 0x5A7A50}, {0x6D, 0x5A7A00}, {0x91, 0x5A7A50}, {0xC7, 0x5A7610}, {0xCE, 0x5A7780}, {0x11C, 0x5A7A00}, {0x14D, 0x5A7A50}, {0x17E, 0x5A7A00}, {0x1AF, 0x5A7A50}, {0x236, 0x572FA0}};
// 0x4E1940: 0x57 bytes
// 0x4E19A0: 0x61 bytes
constexpr mh::CallSite kCalls4E19A0[] = {{0x49, 0x4E2BF0}};
constexpr mh::Imm kImms4E19A0[] = {{0x15, 0x4E1A10}, {0x1D, 0x4F7350}};
// 0x4E1A10: 0xC5 bytes
constexpr mh::CallSite kCalls4E1A10[] = {{0x1C, 0x4FC0E0}, {0x21, 0x4FBD10}, {0x43, 0x435180}, {0x95, 0x587900}};
// 0x4E1AE0: 0x12 bytes; +0xB note: jmp through .data 0x65bbd0, 15 code entries (a data_tables entry)
// 0x4E1B00: 0x3D bytes; +0xB note: call through .data 0x65bbdc, 12 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E1B00[] = {{0x23, 0x4B7D40}, {0x28, 0x4E1D40}, {0x2D, 0x4E1FB0}, {0x32, 0x4E1B90}, {0x37, 0x5A7BC0}};
// 0x4E1B40: 0x2B bytes
// 0x4E1B70: 0x14 bytes
// 0x4E1B90: 0x1AE bytes
constexpr mh::CallSite kCalls4E1B90[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x71, 0x5A7A00}, {0x8A, 0x5A7A50}, {0xC6, 0x5A75F0}, {0xCD, 0x5A7780}, {0xFB, 0x5A7A00}, {0x114, 0x5A7A50}, {0x151, 0x5A84A0}, {0x157, 0x5A9310}, {0x18C, 0x461E50}};
// 0x4E1D40: 0x267 bytes
constexpr mh::CallSite kCalls4E1D40[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x3A, 0x5B93D2}, {0x8E, 0x5A7A00}, {0xA7, 0x5A7A50}, {0xC0, 0x5A7A00}, {0xD9, 0x5A7A50}, {0x11C, 0x5A7610}, {0x123, 0x5A7780}, {0x15D, 0x5A7A00}, {0x176, 0x5A7A50}, {0x18F, 0x5A7A00}, {0x1A8, 0x5A7A50}, {0x1EE, 0x5A85F0}, {0x1F7, 0x5A9350}, {0x246, 0x461E50}};
// 0x4E1FB0: 0x251 bytes
constexpr mh::CallSite kCalls4E1FB0[] = {{0x18, 0x5A77C0}, {0x21, 0x461E50}, {0x78, 0x5A7A00}, {0x91, 0x5A7A50}, {0xAA, 0x5A7A00}, {0xC3, 0x5A7A50}, {0x106, 0x5A7610}, {0x10D, 0x5A7780}, {0x147, 0x5A7A00}, {0x160, 0x5A7A50}, {0x179, 0x5A7A00}, {0x192, 0x5A7A50}, {0x1D8, 0x5A85F0}, {0x1E1, 0x5A9350}, {0x230, 0x461E50}};
// 0x4E2210: 0x2E bytes; +0xB note: call through .data 0x65bbec, 8 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E2210[] = {{0x23, 0x4FBD10}, {0x28, 0x4E22E0}};
// 0x4E2240: 0x7F bytes
constexpr mh::CallSite kCalls4E2240[] = {{0x1E, 0x587900}};
// 0x4E22C0: 0x1D bytes
// 0x4E22E0: 0x3BE bytes
constexpr mh::CallSite kCalls4E22E0[] = {{0x53, 0x5A77C0}, {0x69, 0x572FA0}, {0x75, 0x5A7630}, {0x7D, 0x5A7780}, {0x162, 0x5A79A0}, {0x175, 0x5A79E0}, {0x222, 0x572FA0}, {0x22E, 0x5A7630}, {0x236, 0x5A7780}, {0x303, 0x5A79A0}, {0x313, 0x5A79E0}, {0x3B2, 0x572FA0}};
// 0x4E26A0: 0x2E bytes; +0xB note: call through .data 0x65bbfc, 4 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E26A0[] = {{0x23, 0x4FBD10}, {0x28, 0x4E2A10}};
// 0x4E26D0: 0x10A bytes
constexpr mh::CallSite kCalls4E26D0[] = {{0x7E, 0x446770}, {0xE4, 0x5A7A70}};
// 0x4E27E0: 0xD4 bytes
constexpr mh::CallSite kCalls4E27E0[] = {{0x1B, 0x4FB9F0}, {0x5E, 0x5A7A70}, {0x78, 0x4FBC30}};
// 0x4E28C0: 0x11E bytes
constexpr mh::CallSite kCalls4E28C0[] = {{0x18, 0x587900}, {0x21, 0x435180}, {0x58, 0x435180}, {0x9C, 0x4E3140}, {0xCA, 0x5B93D2}, {0x100, 0x452F70}};
// 0x4E29E0: 0x28 bytes
constexpr mh::CallSite kCalls4E29E0[] = {{0x22, 0x4351F0}};
// 0x4E2A10: 0x1D6 bytes
constexpr mh::CallSite kCalls4E2A10[] = {{0x16, 0x5A77C0}, {0x1F, 0x461E50}, {0x9F, 0x5A75F0}, {0xA6, 0x5A7780}, {0xD7, 0x5A7A00}, {0x105, 0x5A7A50}, {0x13B, 0x5A7A00}, {0x169, 0x5A7A50}, {0x1BF, 0x461E50}};
// 0x4E2BF0: 0x12 bytes; +0xB note: jmp through .data 0x65bc1c, 24 code entries (a data_tables entry)
// 0x4E2C10: 0x2E bytes; +0xB note: call through .data 0x65bc20, 23 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E2C10[] = {{0x23, 0x4E31C0}, {0x28, 0x4E2F60}};
// 0x4E2C40: 0xEC bytes
constexpr mh::CallSite kCalls4E2C40[] = {{0x4D, 0x5A7A00}, {0x78, 0x5A7A50}};
// 0x4E2D30: 0xB6 bytes
constexpr mh::CallSite kCalls4E2D30[] = {{0x3C, 0x5A7A00}, {0x64, 0x5A7A50}};
// 0x4E2DF0: 0xB0 bytes
constexpr mh::CallSite kCalls4E2DF0[] = {{0x40, 0x5A7A00}, {0x68, 0x5A7A50}};
// 0x4E2EA0: 0xB5 bytes
constexpr mh::CallSite kCalls4E2EA0[] = {{0x2F, 0x5A7A00}, {0x57, 0x5A7A50}, {0xAF, 0x4E3190}};
// 0x4E2F60: 0x1D2 bytes
constexpr mh::CallSite kCalls4E2F60[] = {{0x5A, 0x5A77C0}, {0x70, 0x572FA0}, {0x7C, 0x5A75D0}, {0x84, 0x5A7780}, {0x15D, 0x5A79A0}, {0x170, 0x5A79E0}, {0x1C7, 0x572FA0}};
// 0x4E3140: 0x4F bytes
// 0x4E3190: 0x26 bytes
// 0x4E31C0: 0x95 bytes
constexpr mh::CallSite kCalls4E31C0[] = {{0x43, 0x5A7750}, {0x5B, 0x5A8250}, {0x64, 0x5A9110}, {0x6E, 0x5B9550}, {0x80, 0x5B9550}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"DivineBreath_Task", 0x4E0910, 0x75, kCalls4E0910, MH_N(kCalls4E0910), kImms4E0910, MH_N(kImms4E0910), nullptr, 0, reinterpret_cast<const void*>(&::DivineBreath_Task)},
    {"DivineBreath_Start", 0x4E0990, 0xC8, kCalls4E0990, MH_N(kCalls4E0990), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBreath_Start)},
    {"DivineBreathFx_Task", 0x4E0A60, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBreathFx_Task)},
    {"DivineBeam_Run", 0x4E0A80, 0x77, kCalls4E0A80, MH_N(kCalls4E0A80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBeam_Run)},
    {"DivineBeam_Wait", 0x4E0B00, 0xB0, kCalls4E0B00, MH_N(kCalls4E0B00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBeam_Wait)},
    {"DivineBeam_Descend", 0x4E0BB0, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBeam_Descend)},
    {"DivineBeam_Widen", 0x4E0BE0, 0x35, kCalls4E0BE0, MH_N(kCalls4E0BE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBeam_Widen)},
    {"DivineBeam_DrawDisc", 0x4E0C20, 0x1D7, kCalls4E0C20, MH_N(kCalls4E0C20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBeam_DrawDisc)},
    {"DivineBeam_DrawWall", 0x4E0E00, 0x223, kCalls4E0E00, MH_N(kCalls4E0E00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBeam_DrawWall)},
    {"DivineBurst_Run", 0x4E1030, 0x33, kCalls4E1030, MH_N(kCalls4E1030), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBurst_Run)},
    {"DivineBurst_Wait", 0x4E1070, 0x9E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBurst_Wait)},
    {"DivineBurst_Shrink", 0x4E1110, 0xD1, kCalls4E1110, MH_N(kCalls4E1110), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBurst_Shrink)},
    {"DivineBurst_Fade", 0x4E11F0, 0x41, kCalls4E11F0, MH_N(kCalls4E11F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineBurst_Fade)},
    {"DivineMote_Task", 0x4E1240, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineMote_Task)},
    {"DivineMote_Run", 0x4E1260, 0x89, kCalls4E1260, MH_N(kCalls4E1260), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineMote_Run)},
    {"DivineMote_Launch", 0x4E12F0, 0x135, kCalls4E12F0, MH_N(kCalls4E12F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineMote_Launch)},
    {"DivineMote_Fly", 0x4E1430, 0xD1, kCalls4E1430, MH_N(kCalls4E1430), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineMote_Fly)},
    {"DivineMote_DrawStar", 0x4E1510, 0x1C5, kCalls4E1510, MH_N(kCalls4E1510), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineMote_DrawStar)},
    {"DivineMote_DrawRing", 0x4E16E0, 0x255, kCalls4E16E0, MH_N(kCalls4E16E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineMote_DrawRing)},
    {"DivineMote_Alloc", 0x4E1940, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DivineMote_Alloc), 0xFF},
    {"ShadowBreath_Task", 0x4E19A0, 0x61, kCalls4E19A0, MH_N(kCalls4E19A0), kImms4E19A0, MH_N(kImms4E19A0), nullptr, 0, reinterpret_cast<const void*>(&::ShadowBreath_Task)},
    {"ShadowBreath_Start", 0x4E1A10, 0xC5, kCalls4E1A10, MH_N(kCalls4E1A10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowBreath_Start)},
    {"ShadowBreathFx_Task", 0x4E1AE0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowBreathFx_Task)},
    {"ShadowOrb_Run", 0x4E1B00, 0x3D, kCalls4E1B00, MH_N(kCalls4E1B00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowOrb_Run)},
    {"ShadowOrb_Grow", 0x4E1B40, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowOrb_Grow)},
    {"ShadowOrb_WaitChildren", 0x4E1B70, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowOrb_WaitChildren)},
    {"ShadowOrb_DrawDisc", 0x4E1B90, 0x1AE, kCalls4E1B90, MH_N(kCalls4E1B90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowOrb_DrawDisc)},
    {"ShadowOrb_DrawRim", 0x4E1D40, 0x267, kCalls4E1D40, MH_N(kCalls4E1D40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowOrb_DrawRim)},
    {"ShadowOrb_DrawBand", 0x4E1FB0, 0x251, kCalls4E1FB0, MH_N(kCalls4E1FB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowOrb_DrawBand)},
    {"ShadowGlow_Run", 0x4E2210, 0x2E, kCalls4E2210, MH_N(kCalls4E2210), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowGlow_Run)},
    {"ShadowGlow_Wait", 0x4E2240, 0x7F, kCalls4E2240, MH_N(kCalls4E2240), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowGlow_Wait)},
    {"ShadowGlow_Grow", 0x4E22C0, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowGlow_Grow)},
    {"ShadowGlow_Draw", 0x4E22E0, 0x3BE, kCalls4E22E0, MH_N(kCalls4E22E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowGlow_Draw)},
    {"ShadowSeeker_Run", 0x4E26A0, 0x2E, kCalls4E26A0, MH_N(kCalls4E26A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowSeeker_Run)},
    {"ShadowSeeker_Launch", 0x4E26D0, 0x10A, kCalls4E26D0, MH_N(kCalls4E26D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowSeeker_Launch)},
    {"ShadowSeeker_Home", 0x4E27E0, 0xD4, kCalls4E27E0, MH_N(kCalls4E27E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowSeeker_Home)},
    {"ShadowSeeker_Burst", 0x4E28C0, 0x11E, kCalls4E28C0, MH_N(kCalls4E28C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowSeeker_Burst)},
    {"ShadowSeeker_Fade", 0x4E29E0, 0x28, kCalls4E29E0, MH_N(kCalls4E29E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowSeeker_Fade)},
    {"ShadowSeeker_Draw", 0x4E2A10, 0x1D6, kCalls4E2A10, MH_N(kCalls4E2A10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowSeeker_Draw)},
    {"ShadowMote_Task", 0x4E2BF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Task)},
    {"ShadowMote_Run", 0x4E2C10, 0x2E, kCalls4E2C10, MH_N(kCalls4E2C10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Run)},
    {"ShadowMote_Wait", 0x4E2C40, 0xEC, kCalls4E2C40, MH_N(kCalls4E2C40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Wait)},
    {"ShadowMote_Spread", 0x4E2D30, 0xB6, kCalls4E2D30, MH_N(kCalls4E2D30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Spread)},
    {"ShadowMote_Lift", 0x4E2DF0, 0xB0, kCalls4E2DF0, MH_N(kCalls4E2DF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Lift)},
    {"ShadowMote_Fade", 0x4E2EA0, 0xB5, kCalls4E2EA0, MH_N(kCalls4E2EA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Fade)},
    {"ShadowMote_Draw", 0x4E2F60, 0x1D2, kCalls4E2F60, MH_N(kCalls4E2F60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Draw)},
    {"ShadowMote_Alloc", 0x4E3140, 0x4F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Alloc), 0xFF},
    {"ShadowMote_Free", 0x4E3190, 0x26, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Free)},
    {"ShadowMote_Project", 0x4E31C0, 0x95, kCalls4E31C0, MH_N(kCalls4E31C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ShadowMote_Project)},
};
#undef MH_N
enum : unsigned {
    kDivineBreath_Task, kDivineBreath_Start, kDivineBreathFx_Task, kDivineBeam_Run, kDivineBeam_Wait, kDivineBeam_Descend, kDivineBeam_Widen, kDivineBeam_DrawDisc, kDivineBeam_DrawWall, kDivineBurst_Run, kDivineBurst_Wait, kDivineBurst_Shrink, kDivineBurst_Fade, kDivineMote_Task, kDivineMote_Run, kDivineMote_Launch, kDivineMote_Fly, kDivineMote_DrawStar, kDivineMote_DrawRing, kDivineMote_Alloc, kShadowBreath_Task, kShadowBreath_Start, kShadowBreathFx_Task, kShadowOrb_Run, kShadowOrb_Grow, kShadowOrb_WaitChildren, kShadowOrb_DrawDisc, kShadowOrb_DrawRim, kShadowOrb_DrawBand, kShadowGlow_Run, kShadowGlow_Wait, kShadowGlow_Grow, kShadowGlow_Draw, kShadowSeeker_Run, kShadowSeeker_Launch, kShadowSeeker_Home, kShadowSeeker_Burst, kShadowSeeker_Fade, kShadowSeeker_Draw, kShadowMote_Task, kShadowMote_Run, kShadowMote_Wait, kShadowMote_Spread, kShadowMote_Lift, kShadowMote_Fade, kShadowMote_Draw, kShadowMote_Alloc, kShadowMote_Free, kShadowMote_Project, kCount
};

static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the callees the standard set lacks -------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu;
template <typename F> constexpr std::uint32_t KeyOf(F f) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f)); }
#define S29_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S29_RAW(name, address) name, address, address

// A commit logs the primitive it links (the 0x58 bytes at Gfx_PacketNext, a
// POLY_GT4's): the draws write every primitive into the same bytes of the
// buffer, so the regions at the end hold only the last one.
std::uint32_t LogPrimitive(const std::uint32_t*, std::uint32_t answer) {
    mh::NoteBytes(Gfx_PacketNext, 0x58);
    return answer;
}
// Math_Ratan2's answer is the seeker's heading; ShadowSeeker_Home compares
// |(+0x10 & 0xFFF) - (answer & 0xFFF)| with 0x600 and 0xA00. A third of the
// time the answer lands on either bound or one beside it.
std::uint32_t HeadingAnswer(const std::uint32_t*, std::uint32_t answer) {
    if (answer % 3) return answer;
    static const int kD[] = {0x600, 0x601, 0x5FF, 0xA00, 0x9FF, 0xA01, -0x600, -0x601, -0xA00, -0x9FF};
    const int last = static_cast<int>(Long(Sprite_Current + 0x10)) & 0xFFF;
    return (answer & 0xFFFFF000u) | (static_cast<std::uint32_t>(last - kD[(answer >> 2) % 10]) & 0xFFFu);
}

const mh::Callee kCallees[] = {
    {S29_OURS(Math_Sin), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Math_Cos), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Math_Ratan2), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &HeadingAnswer},
    {S29_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &LogPrimitive},
    {S29_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &LogPrimitive},
    {S29_OURS(Gpu_SetPolyG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gpu_SetPolyG4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gpu_SetPolyGT4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gpu_SetPolyFT4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gpu_SetTile1), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gpu_GetClut), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    // The vertices by what they hold (their scratch is rewritten every step);
    // the outputs are in the fuzz's own buffer, the depth in the caller's frame.
    {S29_OURS(Gte_RotTransPers3), 7, {0, 0, 0, kAll, kAll, kAll, 0}, mh::Answer::kGarbage, 0, 0, {8, 8, 8}},
    {S29_OURS(Gte_RotTransPers4), 9, {0, 0, 0, 0, kAll, kAll, kAll, kAll, 0}, mh::Answer::kGarbage, 0, 0, {8, 8, 8, 8}},
    {S29_OURS(Gte_RotTransPers), 3, {0, kAll, 0}, mh::Answer::kGarbage, 0, 0, {6}},
    {S29_OURS(Gte_PrimDepths3_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gte_PrimDepths4_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_OURS(Gte_StoreDepthF), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    // MagicFx_NearSprite's caller tests all of eax.
    {S29_OURS(MagicFx_NearSprite), 2, {kAll, kAll}, mh::Answer::kBool, 0, 0},
    // The CRT's _ftol: pure x87, the copy keeps it (ours truncates inline).
    {S29_RAW("_ftol", 0x5B9550), 0, {}, mh::Answer::kThrough, 0, 0},
    // Not ours: the engine's turn, MAGIC219's record free (logged as the task
    // it frees).
    {S29_RAW("0x446770", kTurnByFacing), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S29_RAW("0x4F6290", kFreeRecord), 0, {}, mh::Answer::kPhase, 0, 0},
    // The group's own, called directly: logged as the task they run for.
    {S29_RAW("DivineMote_Task", 0x4E1240), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("DivineBeam_DrawDisc", 0x4E0C20), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("DivineBeam_DrawWall", 0x4E0E00), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("DivineMote_DrawStar", 0x4E1510), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("DivineMote_DrawRing", 0x4E16E0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowOrb_DrawDisc", 0x4E1B90), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowOrb_DrawRim", 0x4E1D40), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowOrb_DrawBand", 0x4E1FB0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowGlow_Draw", 0x4E22E0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowSeeker_Draw", 0x4E2A10), 0, {}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowMote_Task", 0x4E2BF0), 0, {cell::kShadeCurrent}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowMote_Draw", 0x4E2F60), 0, {cell::kShadeCurrent}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowMote_Free", 0x4E3190), 0, {cell::kShadeCurrent}, mh::Answer::kPhase, 0, 0},
    {S29_RAW("ShadowMote_Project", 0x4E31C0), 0, {cell::kShadeCurrent}, mh::Answer::kPhase, 0, 0},
    // The allocators: a record's index inside the pool (neither caller tests
    // for 0xFF, so the recorders never answer past the pool).
    {S29_RAW("DivineMote_Alloc", 0x4E1940), 0, {}, mh::Answer::kByte, 0, cell::kMotes - 1},
    {S29_RAW("ShadowMote_Alloc", 0x4E3140), 0, {}, mh::Answer::kByte, 0, cell::kShades - 1},
};
#undef S29_OURS
#undef S29_RAW

// The .data dispatch runs, each read in place: 0x65BBA4 is DivineBreathFx_Kinds
// (2), DivineBeam_Steps (3), DivineBurst_Steps (3), DivineMote_TaskTable (1)
// and DivineMote_Steps (2) end to end; 0x65BBD0 ShadowBreathFx_Kinds (3),
// ShadowOrb_Steps (4), ShadowGlow_Steps (4), ShadowSeeker_Steps (4); 0x65BC1C
// ShadowMote_TaskTable (1) and ShadowMote_Steps (4).
const mh::DataTable kTables[] = {{tbl::kDivineKinds, 11}, {tbl::kShadowKinds, 15}, {tbl::kShadeTask, 5}};

// --- the state ---------------------------------------------------------------

constexpr unsigned kPrimBytes = 0x100;
alignas(16) unsigned char g_prims[kPrimBytes];   // what Gfx_PacketNext points into

mh::Region g_regions[] = {
    {cell::kScratch, 0x10},
    {cell::kVertices, 0x20},
    {cell::kPacketNext, 4},
    {0, kPrimBytes},   // g_prims (filled in at start-up)
    {cell::kMotePool, cell::kMotes * cell::kMoteStride},
    {cell::kShadePool, cell::kShades * cell::kShadeStride},
    {cell::kShadeCurrent, 4},
    {cell::kClutRow, 0x200},
    {cell::kClutSource, 0x200},
};

unsigned char* PrimAt(std::uint32_t v) { return g_prims + 4 * (v % 16); }
unsigned char* MoteAt(std::uint32_t v) { return mh::Mem(cell::kMotePool + (v % cell::kMotes) * cell::kMoteStride); }
unsigned char* ShadeAt(std::uint32_t v) { return mh::Mem(cell::kShadePool + (v % cell::kShades) * cell::kShadeStride); }
unsigned char* Cur() { return mh::Pointer(cell::kShadeCurrent); }
unsigned char* AnOwner(std::uint32_t v) { return v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v); }

// A byte at one of the values, or one either side, half the time.
void Near(unsigned char& b, unsigned v) {
    if (mh::Half()) b = static_cast<unsigned char>(v + (mh::Next() % 3) - 1);
}
unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }

void Seed(unsigned k) {
    // Everything any function dereferences, put back inside.
    Gfx_PacketNext = PrimAt(mh::Next());
    mh::SetPointer(cell::kActorSprite, mh::SpriteRecord(mh::Next()));
    mh::SetPointer(cell::kShadeCurrent, ShadeAt(mh::Next()));
    for (unsigned i = 0; i < cell::kMotes; ++i)
        mh::SetPointer(cell::kMotePool + i * cell::kMoteStride + 0x80, AnOwner(mh::Next()));
    for (unsigned i = 0; i < cell::kShades; ++i)
        mh::SetPointer(cell::kShadePool + i * cell::kShadeStride + 0x1C, AnOwner(mh::Next()));
    unsigned char* const sc = Sprite_Current;
    unsigned char* const cur = Cur();
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kDivineBreath_Task: case kShadowBreath_Task: case kDivineBreathFx_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kShadowBreathFx_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kDivineMote_Task: sc[1] = 0; break;
    case kShadowMote_Task: cur[1] = 0; break;
    case kDivineBeam_Run: case kDivineBurst_Run:
        sc[2] = Byte(mh::Next() % 3);
        if (mh::Half()) sc[0] = 0;
        break;
    case kDivineMote_Run:
        sc[2] = Byte(mh::Next() % 2);
        if (mh::Half()) sc[0] = 0;
        break;
    case kShadowOrb_Run: case kShadowGlow_Run: case kShadowSeeker_Run:
        sc[2] = Byte(mh::Next() % 4);
        if (mh::Half()) sc[0] = 0;
        break;
    case kShadowMote_Run:
        cur[2] = Byte(mh::Next() % 4);
        if (mh::Half()) cur[0] = 0;
        break;
    // the count-downs: one step from their ends
    case kDivineBeam_Wait: case kDivineBeam_Widen: case kDivineBurst_Wait: case kDivineMote_Launch:
    case kShadowGlow_Wait: case kShadowSeeker_Launch:
        Near(sc[9], 1);
        break;
    case kDivineBeam_Descend: Near(sc[0xA], 1); break;
    case kDivineBurst_Shrink: {
        const std::uint32_t step = MH_PICK(1, 2, 3, 0x21, 0x40);
        SetLong(sc + 0x20, static_cast<std::int32_t>(step));
        const std::uint32_t taken = step == 1 ? 1 : step - 2;
        if (mh::Often()) SetLong(sc + 0x14, static_cast<std::int32_t>(0x30 + taken - 1 + mh::Next() % 3));
        break;
    }
    case kDivineBurst_Fade:
        Near(sc[9], 1);
        if (mh::Half()) SetLong(sc + 0x14, static_cast<std::int32_t>(7 + mh::Next() % 3));
        break;
    case kDivineMote_Fly:
        if (mh::Half()) SetLong(sc + 0xC, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(sc + 0x18)) + mh::Next() % 3 - 1));
        break;
    case kShadowOrb_Grow: Near(sc[9], 0xE); break;
    case kShadowOrb_WaitChildren: mh::Pointer(mh::at::kOwner)[0xB] = Byte(MH_PICK(0, 1, 2, 3, 0x80)); break;
    case kShadowGlow_Grow: Near(sc[9], 0x94); break;
    case kShadowSeeker_Home: sc[9] = Byte(MH_PICK(0xB, 0xC, 0xF, 0x10, 0x11, 0x40)); break;
    case kShadowSeeker_Burst: if (mh::Half()) sc[0xB] = 0; break;
    case kShadowSeeker_Fade: sc[9] = Byte(MH_PICK(0, 1, 2, 3, 0x7F, 0x80, 0x81, 0x82)); break;
    case kShadowSeeker_Draw: if (mh::Often()) sc[0xB] = Byte(mh::Next() % 8); break;
    case kShadowMote_Wait: Near(cur[5], 1); break;
    case kShadowMote_Spread: Near(cur[6], 0xF); break;
    case kShadowMote_Lift:
        if (mh::Half()) SetLong(cur + 8, static_cast<std::int32_t>(0x780000 + mh::Next() % 3 - 1));
        break;
    case kShadowMote_Fade: Near(cur[6], 1); break;
    // the allocators: a full pool half the time, one record free in half of those
    case kDivineMote_Alloc: case kShadowMote_Alloc:
        if (mh::Half()) {
            const bool motes = k == kDivineMote_Alloc;
            const unsigned n = motes ? cell::kMotes : cell::kShades;
            for (unsigned i = 0; i < n; ++i) (motes ? MoteAt(i) : ShadeAt(i))[0] |= 1;
            if (mh::Half()) (motes ? MoteAt(mh::Next()) : ShadeAt(mh::Next()))[0] &= 0xFE;
        }
        break;
    default: break;
    }
}

// The group's own cells a callee may move: the packet pointer, the current
// record, a scratch or vertex byte, a byte of the current record below its
// owner, a pool record's in-use bit.
void Disturb(std::uint32_t h) {
    const auto b = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 7) {
    case 0: Gfx_PacketNext = PrimAt(h >> 12); break;
    case 1: mh::SetPointer(cell::kShadeCurrent, ShadeAt(h >> 12)); break;
    case 2: mh::Mem(cell::kScratch + (h >> 12) % 0x10)[0] = b; break;
    case 3: mh::Mem(cell::kVertices + (h >> 12) % 0x20)[0] = b; break;
    case 4: Cur()[(h >> 12) % 0x1C] = b; break;
    case 5: MoteAt(h >> 12)[0] ^= 1; break;
    default: ShadeAt(h >> 12)[0] ^= 1; break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[3].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    const mh::Group group = {
        "magic_s29", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
        sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed, &Disturb, 2000,
    };
    mh::Run(group);
}

}  // namespace magic_s29
