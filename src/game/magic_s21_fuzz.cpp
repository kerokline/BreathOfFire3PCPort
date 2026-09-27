// BOF3X_SHADOW=magic_s21: group S21's three overlays (MAGIC093, 094, 095)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s21.md section 4.
//
// The clone table (tools/magic_rows.py --unit MAGIC093 / 094 / 095 --clones),
// the callees the standard set lacks - the GTE and GPU calls the draws make,
// each with an Effect that logs what it reads through its pointers and fills
// what it writes, and ours that call one another -, the seven .data handler
// tables, the regions beyond the standard ones (the pool, the jitter bytes,
// the scratch, the CLUT strip, a primitive buffer of the fuzz's own) and a
// seed per function. Everything else is the harness's.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s21.h"
#include "game/move_script_bytes.h"

extern "C" {
unsigned char __cdecl FlamePool_Alloc(void);
void __cdecl FlamePool_Dispatch(void);
void __cdecl Inferno_TargetCentre(void);
void __cdecl FlameColumn_DrawDisc(void);
void __cdecl FlameColumn_DrawBand(void);
void __cdecl FlameSpark_Draw(void);
void __cdecl FrostRing_PushMatrix(void);
void __cdecl FrostRing_DrawShards(void);
void __cdecl FrostRing_DrawDisc(void);
void __cdecl Iceblast_DrawSpires(void);
void __cdecl Iceblast_DrawSpiresInner(void);
void __cdecl Iceblast_DrawDisc(void);
void __cdecl IceShard_Draw(void);
}

namespace magic_s21 {
namespace {

namespace mh = magic_harness;
namespace at = magic_harness::at;

std::uint32_t K(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KF(F f) { return K(reinterpret_cast<const void*>(f)); }

// --- the fuzz's own memory and the group's regions ---------------------------------

constexpr unsigned kPrimBytes = 0x1000;
alignas(16) unsigned char g_prims[kPrimBytes];

constexpr std::uint32_t kPool = 0x6948D8;   // 32 slots of 0x84, then the 56 jitter bytes
constexpr std::uint32_t kPoolBytes = 32 * 0x84 + 0x40;
constexpr std::uint32_t kJitter = 0x695958;

// --- the clone table ------------------------------------------------------------

// tools/magic_rows.py --unit MAGIC093 / MAGIC094 / MAGIC095 --clones,
// 2026-09-25 (capstone: every jump internal, no jump table, nothing REFUSED).
// 0x4C6300: 0x75 bytes
constexpr mh::CallSite kCalls4C6300[] = {{0x53, 0x4C6700}};
constexpr mh::Imm kImms4C6300[] = {{0x16, 0x4C6380}, {0x1E, 0x4C6540}};
// 0x4C6380: 0x1B3 bytes
constexpr mh::CallSite kCalls4C6380[] = {{0x1E, 0x4C7320}, {0x54, 0x4456C0}, {0x64, 0x435180}, {0xA9, 0x4456C0}, {0xB9, 0x435180}, {0xFB, 0x4C72C0}, {0x138, 0x4C72C0}, {0x1AC, 0x587900}};
// 0x4C6540: 0x20 bytes
constexpr mh::CallSite kCalls4C6540[] = {{0x1A, 0x4351F0}};
// 0x4C6560: 0x12 bytes; +0xB note: jmp through .data 0x65b604, 20 code entries (a data_tables entry)
// 0x4C6580: 0x12 bytes; +0xB note: jmp through .data 0x65b608, 19 code entries (a data_tables entry)
// 0x4C65A0: 0xAF bytes
constexpr mh::CallSite kCalls4C65A0[] = {{0x3E, 0x454DC0}, {0x4C, 0x454CC0}, {0x7A, 0x454DC0}, {0x88, 0x454CC0}, {0x98, 0x452F70}};
// 0x4C6650: 0x98 bytes
constexpr mh::CallSite kCalls4C6650[] = {{0x25, 0x4530D0}, {0x43, 0x454DC0}, {0x57, 0x4530D0}, {0x78, 0x454DC0}, {0x87, 0x4FBDB0}};
// 0x4C66F0: 0xD bytes
constexpr mh::CallSite kCalls4C66F0[] = {{0x8, 0x4351F0}};
// 0x4C6700: 0x12 bytes; +0xB note: jmp through .data 0x65b614, 16 code entries (a data_tables entry)
// 0x4C6720: 0x45 bytes; +0xB note: call through .data 0x65b61c, 14 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C6720[] = {{0x30, 0x4B7D40}, {0x35, 0x4C6940}, {0x3A, 0x4C6B10}, {0x3F, 0x5A7BC0}};
// 0x4C6770: 0xCC bytes
constexpr mh::CallSite kCalls4C6770[] = {{0x0, 0x5B93D2}, {0x43, 0x5A7A00}, {0x71, 0x5A7A50}, {0xAC, 0x5B93D2}};
// 0x4C6840: 0xAA bytes
constexpr mh::CallSite kCalls4C6840[] = {{0x37, 0x5A7A00}, {0x65, 0x5A7A50}};
// 0x4C68F0: 0x14 bytes
// 0x4C6910: 0x27 bytes
constexpr mh::CallSite kCalls4C6910[] = {{0x21, 0x4F6290}};
// 0x4C6940: 0x1C6 bytes
constexpr mh::CallSite kCalls4C6940[] = {{0x15, 0x5A77C0}, {0x2B, 0x572FA0}, {0x3F, 0x5A7A00}, {0x80, 0x5A7A00}, {0x99, 0x5A7A50}, {0xD5, 0x5A75F0}, {0xDD, 0x5A7780}, {0x10B, 0x5A7A00}, {0x124, 0x5A7A50}, {0x161, 0x5A84A0}, {0x167, 0x5A9310}, {0x1A4, 0x572FA0}};
// 0x4C6B10: 0x26C bytes
constexpr mh::CallSite kCalls4C6B10[] = {{0x14, 0x5A77C0}, {0x2A, 0x572FA0}, {0x3E, 0x5A7A00}, {0x64, 0x5A7A00}, {0x99, 0x5A7A00}, {0xB2, 0x5A7A50}, {0xCB, 0x5A7A00}, {0xE4, 0x5A7A50}, {0x127, 0x5A7610}, {0x12F, 0x5A7780}, {0x14F, 0x5A7A00}, {0x168, 0x5A7A50}, {0x19B, 0x5A7A00}, {0x1B4, 0x5A7A50}, {0x1FA, 0x5A85F0}, {0x203, 0x5A9350}, {0x24B, 0x572FA0}};
// 0x4C6D80: 0x2E bytes; +0xB note: call through .data 0x65b62c, 10 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C6D80[] = {{0x23, 0x4FBD10}, {0x28, 0x4C6F70}};
// 0x4C6DB0: 0x11D bytes
constexpr mh::CallSite kCalls4C6DB0[] = {{0x55, 0x5B93D2}, {0x73, 0x5A7A00}, {0x9C, 0x5A7A50}, {0xEF, 0x587900}};
// 0x4C6ED0: 0x1D bytes
// 0x4C6EF0: 0x2A bytes
// 0x4C6F20: 0x41 bytes
constexpr mh::CallSite kCalls4C6F20[] = {{0x3B, 0x4F6290}};
// 0x4C6F70: 0x346 bytes
constexpr mh::CallSite kCalls4C6F70[] = {{0x4D, 0x5A77C0}, {0x63, 0x572FA0}, {0x6F, 0x5A7630}, {0x77, 0x5A7780}, {0x174, 0x5A79A0}, {0x187, 0x5A79E0}, {0x1C1, 0x572FA0}, {0x1CD, 0x5A7630}, {0x1D5, 0x5A7780}, {0x2F0, 0x5A79A0}, {0x300, 0x5A79E0}, {0x33A, 0x572FA0}};
// 0x4C72C0: 0x57 bytes
// 0x4C7320: 0x10B bytes
constexpr mh::CallSite kCalls4C7320[] = {{0x2A, 0x4456C0}, {0x7E, 0x4456C0}};
// 0x4C7430: 0x26 bytes
constexpr mh::Imm kImms4C7430[] = {{0xF, 0x4C7460}, {0x17, 0x4C7510}};
// 0x4C7460: 0xAC bytes
constexpr mh::CallSite kCalls4C7460[] = {{0x4D, 0x435180}, {0xA2, 0x587900}};
// 0x4C7510: 0x98 bytes
constexpr mh::CallSite kCalls4C7510[] = {{0x6C, 0x454DC0}, {0x77, 0x4FBDB0}, {0x83, 0x4530D0}, {0x92, 0x4351F0}};
// 0x4C75B0: 0x52 bytes; +0xB note: call through .data 0x65b63c, 6 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C75B0[] = {{0x1E, 0x4C7A80}, {0x23, 0x4C7740}, {0x28, 0x4C7B30}, {0x2D, 0x5A7BC0}, {0x40, 0x5A77C0}, {0x49, 0x461E50}};
// 0x4C7610: 0x2D bytes
constexpr mh::CallSite kCalls4C7610[] = {{0x8, 0x452F70}};
// 0x4C7640: 0x85 bytes
constexpr mh::CallSite kCalls4C7640[] = {{0x5C, 0x454DC0}, {0x6A, 0x454CC0}};
// 0x4C76D0: 0x38 bytes
// 0x4C7710: 0x28 bytes
constexpr mh::CallSite kCalls4C7710[] = {{0x22, 0x4351F0}};
// 0x4C7740: 0x337 bytes
constexpr mh::CallSite kCalls4C7740[] = {{0xD1, 0x5A7A50}, {0xE7, 0x5A7A00}, {0x11D, 0x5A7A50}, {0x133, 0x5A7A00}, {0x178, 0x5A7A50}, {0x18E, 0x5A7A00}, {0x1D7, 0x5A77C0}, {0x1E2, 0x572FA0}, {0x1EE, 0x5A75F0}, {0x1F6, 0x5A7780}, {0x2E9, 0x5A84A0}, {0x2EF, 0x5A9310}, {0x2FA, 0x572FA0}};
// 0x4C7A80: 0xA7 bytes
constexpr mh::CallSite kCalls4C7A80[] = {{0x3, 0x5A7B90}, {0x67, 0x5A8200}, {0x76, 0x5A8060}, {0x8A, 0x5A7D70}, {0x94, 0x5A8DE0}, {0x9E, 0x5A8E00}};
// 0x4C7B30: 0x19F bytes
constexpr mh::CallSite kCalls4C7B30[] = {{0x5B, 0x5A77C0}, {0x71, 0x572FA0}, {0x7D, 0x5A75F0}, {0x84, 0x5A7780}, {0xB3, 0x5A7A00}, {0xDA, 0x5A7A50}, {0x107, 0x5A7A00}, {0x12E, 0x5A7A50}, {0x186, 0x572FA0}};
// 0x4C7CD0: 0x48 bytes
constexpr mh::CallSite kCalls4C7CD0[] = {{0x2B, 0x4B7D40}, {0x3F, 0x5A7BC0}};
constexpr mh::Imm kImms4C7CD0[] = {{0x7, 0x4C7D20}, {0xF, 0x4C7E30}, {0x17, 0x4C7EC0}, {0x1F, 0x4C7F20}, {0x27, 0x4C7F50}};
// 0x4C7D20: 0x10B bytes
constexpr mh::CallSite kCalls4C7D20[] = {{0x49, 0x5B93D2}, {0x52, 0x5B93D2}, {0x5D, 0x5B93D2}, {0x76, 0x435180}, {0xE4, 0x587900}};
// 0x4C7E30: 0x8A bytes
constexpr mh::CallSite kCalls4C7E30[] = {{0x0, 0x4C87E0}, {0x47, 0x454DC0}, {0x55, 0x454CC0}, {0x65, 0x452F70}};
// 0x4C7EC0: 0x51 bytes
constexpr mh::CallSite kCalls4C7EC0[] = {{0x0, 0x4C7FE0}, {0x5, 0x4C83E0}, {0xA, 0x4C87E0}, {0x2C, 0x587900}, {0x36, 0x587900}};
// 0x4C7F20: 0x2C bytes
constexpr mh::CallSite kCalls4C7F20[] = {{0x0, 0x4C7FE0}, {0x5, 0x4C83E0}, {0xA, 0x4C87E0}};
// 0x4C7F50: 0x85 bytes
constexpr mh::CallSite kCalls4C7F50[] = {{0x0, 0x4C87E0}, {0x59, 0x454DC0}, {0x64, 0x4FBDB0}, {0x70, 0x4530D0}, {0x7F, 0x4351F0}};
// 0x4C7FE0: 0x3F4 bytes
constexpr mh::CallSite kCalls4C7FE0[] = {{0x38, 0x5A7A00}, {0x4F, 0x5A7A50}, {0xA8, 0x5A7A00}, {0xBF, 0x5A7A50}, {0x1A7, 0x5A7A00}, {0x1BD, 0x5A7A50}, {0x1FC, 0x5A7A00}, {0x212, 0x5A7A50}, {0x283, 0x5A77C0}, {0x28E, 0x572FA0}, {0x29A, 0x5A76F0}, {0x2A2, 0x5A7780}, {0x2D5, 0x5A85F0}, {0x2DE, 0x5A9460}, {0x3B9, 0x572FA0}};
// 0x4C83E0: 0x3F3 bytes
constexpr mh::CallSite kCalls4C83E0[] = {{0x42, 0x5A7A00}, {0x59, 0x5A7A50}, {0xB2, 0x5A7A00}, {0xC9, 0x5A7A50}, {0x1A2, 0x5A7A00}, {0x1B8, 0x5A7A50}, {0x1F7, 0x5A7A00}, {0x20D, 0x5A7A50}, {0x27E, 0x5A77C0}, {0x289, 0x572FA0}, {0x295, 0x5A7610}, {0x29D, 0x5A7780}, {0x2D0, 0x5A85F0}, {0x2D9, 0x5A9350}, {0x3B6, 0x572FA0}};
// 0x4C87E0: 0x190 bytes
constexpr mh::CallSite kCalls4C87E0[] = {{0x14, 0x5A7A00}, {0x2A, 0x5A7A50}, {0x85, 0x5A7A00}, {0x9B, 0x5A7A50}, {0xC3, 0x5A77C0}, {0xD9, 0x572FA0}, {0xE5, 0x5A75F0}, {0xEC, 0x5A7780}, {0x14F, 0x5A84A0}, {0x158, 0x5A9310}, {0x16E, 0x572FA0}};
// 0x4C8970: 0x1C bytes; +0x10 note: call through .data 0x65b64c, 2 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4C8970[] = {{0x0, 0x4B7D40}, {0x17, 0x5A7BC0}};
// 0x4C8990: 0x28 bytes
// 0x4C89C0: 0x23 bytes
constexpr mh::CallSite kCalls4C89C0[] = {{0x0, 0x4C89F0}, {0x1D, 0x4351F0}};
// 0x4C89F0: 0x345 bytes
constexpr mh::CallSite kCalls4C89F0[] = {{0xB2, 0x5A7A00}, {0xC8, 0x5A7A50}, {0x139, 0x5A77C0}, {0x144, 0x572FA0}, {0x150, 0x5A7570}, {0x158, 0x5A7780}, {0x181, 0x5A7A00}, {0x1AF, 0x5A7A50}, {0x1DC, 0x5A7A00}, {0x206, 0x5A7A50}, {0x237, 0x5A7A00}, {0x260, 0x5A7A50}, {0x2D3, 0x5B93D2}, {0x304, 0x5A84A0}, {0x30A, 0x5A91C0}, {0x314, 0x572FA0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Inferno_Task", 0x4C6300, 0x75, kCalls4C6300, MH_N(kCalls4C6300), kImms4C6300, MH_N(kImms4C6300), nullptr, 0, reinterpret_cast<const void*>(&::Inferno_Task)},
    {"Inferno_Start", 0x4C6380, 0x1B3, kCalls4C6380, MH_N(kCalls4C6380), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Inferno_Start)},
    {"Inferno_End", 0x4C6540, 0x20, kCalls4C6540, MH_N(kCalls4C6540), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Inferno_End)},
    {"FxScorch_Dispatch", 0x4C6560, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxScorch_Dispatch)},
    {"FxScorch_Run", 0x4C6580, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxScorch_Run)},
    {"FxScorch_Delay", 0x4C65A0, 0xAF, kCalls4C65A0, MH_N(kCalls4C65A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxScorch_Delay)},
    {"FxScorch_Hit", 0x4C6650, 0x98, kCalls4C6650, MH_N(kCalls4C6650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxScorch_Hit)},
    {"FxScorch_Free", 0x4C66F0, 0xD, kCalls4C66F0, MH_N(kCalls4C66F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FxScorch_Free)},
    {"FlamePool_Dispatch", 0x4C6700, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlamePool_Dispatch)},
    {"FlameColumn_Task", 0x4C6720, 0x45, kCalls4C6720, MH_N(kCalls4C6720), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameColumn_Task)},
    {"FlameColumn_Start", 0x4C6770, 0xCC, kCalls4C6770, MH_N(kCalls4C6770), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameColumn_Start)},
    {"FlameColumn_Grow", 0x4C6840, 0xAA, kCalls4C6840, MH_N(kCalls4C6840), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameColumn_Grow)},
    {"FlameColumn_Hold", 0x4C68F0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameColumn_Hold)},
    {"FlameColumn_Fade", 0x4C6910, 0x27, kCalls4C6910, MH_N(kCalls4C6910), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameColumn_Fade)},
    {"FlameColumn_DrawDisc", 0x4C6940, 0x1C6, kCalls4C6940, MH_N(kCalls4C6940), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameColumn_DrawDisc)},
    {"FlameColumn_DrawBand", 0x4C6B10, 0x26C, kCalls4C6B10, MH_N(kCalls4C6B10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameColumn_DrawBand)},
    {"FlameSpark_Task", 0x4C6D80, 0x2E, kCalls4C6D80, MH_N(kCalls4C6D80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameSpark_Task)},
    {"FlameSpark_Start", 0x4C6DB0, 0x11D, kCalls4C6DB0, MH_N(kCalls4C6DB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameSpark_Start)},
    {"FlameSpark_Grow", 0x4C6ED0, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameSpark_Grow)},
    {"FlameSpark_Stretch", 0x4C6EF0, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameSpark_Stretch)},
    {"FlameSpark_Fade", 0x4C6F20, 0x41, kCalls4C6F20, MH_N(kCalls4C6F20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameSpark_Fade)},
    {"FlameSpark_Draw", 0x4C6F70, 0x346, kCalls4C6F70, MH_N(kCalls4C6F70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlameSpark_Draw)},
    {"FlamePool_Alloc", 0x4C72C0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FlamePool_Alloc), 0xFF},
    {"Inferno_TargetCentre", 0x4C7320, 0x10B, kCalls4C7320, MH_N(kCalls4C7320), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Inferno_TargetCentre)},
    {"Frost_Task", 0x4C7430, 0x26, nullptr, 0, kImms4C7430, MH_N(kImms4C7430), nullptr, 0, reinterpret_cast<const void*>(&::Frost_Task)},
    {"Frost_Start", 0x4C7460, 0xAC, kCalls4C7460, MH_N(kCalls4C7460), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Frost_Start)},
    {"Frost_Wait", 0x4C7510, 0x98, kCalls4C7510, MH_N(kCalls4C7510), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Frost_Wait)},
    {"FrostRing_Task", 0x4C75B0, 0x52, kCalls4C75B0, MH_N(kCalls4C75B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FrostRing_Task)},
    {"FrostRing_Mark", 0x4C7610, 0x2D, kCalls4C7610, MH_N(kCalls4C7610), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FrostRing_Mark)},
    {"FrostRing_Grow", 0x4C7640, 0x85, kCalls4C7640, MH_N(kCalls4C7640), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FrostRing_Grow)},
    {"FrostRing_Spin", 0x4C76D0, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FrostRing_Spin)},
    {"FrostRing_Fade", 0x4C7710, 0x28, kCalls4C7710, MH_N(kCalls4C7710), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FrostRing_Fade)},
    {"FrostRing_DrawShards", 0x4C7740, 0x337, kCalls4C7740, MH_N(kCalls4C7740), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FrostRing_DrawShards)},
    {"FrostRing_PushMatrix", 0x4C7A80, 0xA7, kCalls4C7A80, MH_N(kCalls4C7A80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FrostRing_PushMatrix)},
    {"FrostRing_DrawDisc", 0x4C7B30, 0x19F, kCalls4C7B30, MH_N(kCalls4C7B30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::FrostRing_DrawDisc)},
    {"Iceblast_Task", 0x4C7CD0, 0x48, kCalls4C7CD0, MH_N(kCalls4C7CD0), kImms4C7CD0, MH_N(kImms4C7CD0), nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_Task)},
    {"Iceblast_Start", 0x4C7D20, 0x10B, kCalls4C7D20, MH_N(kCalls4C7D20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_Start)},
    {"Iceblast_Chill", 0x4C7E30, 0x8A, kCalls4C7E30, MH_N(kCalls4C7E30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_Chill)},
    {"Iceblast_Rise", 0x4C7EC0, 0x51, kCalls4C7EC0, MH_N(kCalls4C7EC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_Rise)},
    {"Iceblast_Sink", 0x4C7F20, 0x2C, kCalls4C7F20, MH_N(kCalls4C7F20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_Sink)},
    {"Iceblast_End", 0x4C7F50, 0x85, kCalls4C7F50, MH_N(kCalls4C7F50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_End)},
    {"Iceblast_DrawSpires", 0x4C7FE0, 0x3F4, kCalls4C7FE0, MH_N(kCalls4C7FE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_DrawSpires)},
    {"Iceblast_DrawSpiresInner", 0x4C83E0, 0x3F3, kCalls4C83E0, MH_N(kCalls4C83E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_DrawSpiresInner)},
    {"Iceblast_DrawDisc", 0x4C87E0, 0x190, kCalls4C87E0, MH_N(kCalls4C87E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Iceblast_DrawDisc)},
    {"IceShard_Task", 0x4C8970, 0x1C, kCalls4C8970, MH_N(kCalls4C8970), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IceShard_Task)},
    {"IceShard_Wait", 0x4C8990, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IceShard_Wait)},
    {"IceShard_Run", 0x4C89C0, 0x23, kCalls4C89C0, MH_N(kCalls4C89C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IceShard_Run)},
    {"IceShard_Draw", 0x4C89F0, 0x345, kCalls4C89F0, MH_N(kCalls4C89F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IceShard_Draw)},
};
#undef MH_N
enum : unsigned {
    kInfernoTask, kInfernoStart, kInfernoEnd, kFxScorchDispatch, kFxScorchRun, kFxScorchDelay, kFxScorchHit, kFxScorchFree, kFlamePoolDispatch, kFlameColumnTask, kFlameColumnStart, kFlameColumnGrow, kFlameColumnHold, kFlameColumnFade, kFlameColumnDrawDisc, kFlameColumnDrawBand, kFlameSparkTask, kFlameSparkStart, kFlameSparkGrow, kFlameSparkStretch, kFlameSparkFade, kFlameSparkDraw, kFlamePoolAlloc, kInfernoTargetCentre, kFrostTask, kFrostStart, kFrostWait, kFrostRingTask, kFrostRingMark, kFrostRingGrow, kFrostRingSpin, kFrostRingFade, kFrostRingDrawShards, kFrostRingPushMatrix, kFrostRingDrawDisc, kIceblastTask, kIceblastStart, kIceblastChill, kIceblastRise, kIceblastSink, kIceblastEnd, kIceblastDrawSpires, kIceblastDrawSpiresInner, kIceblastDrawDisc, kIceShardTask, kIceShardWait, kIceShardRun, kIceShardDraw
};

// The .data tables its dispatchers read in place (tools/magic_rows.py's
// notes), their entries swapped for recorders while the fuzz runs.
const mh::DataTable kTables[] = {
    {0x65B604, 1},   // FxScorch_Phases, by +1
    {0x65B608, 3},   // FxScorch_Steps, by +2
    {0x65B614, 2},   // FlamePool_Types, by +1
    {0x65B61C, 4},   // FlameColumn_Phases, by +2
    {0x65B62C, 4},   // FlameSpark_Phases, by +2
    {0x65B63C, 4},   // FrostRing_Phases, by +1
    {0x65B64C, 2},   // IceShard_Phases, by +1
};

const mh::Region kRegions[] = {
    {kPool, kPoolBytes},
    {0x9037A0, 0x20},                  // Prim_VertexScratch
    {0x903850, 0x10},                  // DamageScratch
    {0x80E980, 0x200},                 // the CLUT strip's buffer
    {0x812980, 0x200},                 // the CLUT strip
    {0x7E0670, 4},                     // Gfx_PacketNext
    {0, kPrimBytes},                   // g_prims (its address at start-up)
};
mh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

// --- the callees' effects ------------------------------------------------------------

std::uint32_t Fnv(std::uint32_t h, const void* p, unsigned n) {
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 0x01000193u;
    return h;
}
const void* P(std::uint32_t a) { return reinterpret_cast<const void*>(static_cast<std::uintptr_t>(a)); }
void* W(std::uint32_t a) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(a)); }

// Math_Sin / Math_Cos: in the range the tables answer, a quarter of the time
// anything.
std::uint32_t Trig(const std::uint32_t*, std::uint32_t h) {
    return h % 4 == 0 ? h : static_cast<std::uint32_t>(static_cast<int>((h >> 8) % 8193) - 4096);
}
// Battle_ActorIsOut: out half the time, but never the last of either side
// (party 2, enemy index 10), so Inferno_TargetCentre never divides by zero.
std::uint32_t IsOut(const std::uint32_t* a, std::uint32_t h) {
    const unsigned i = a[0] & 0xFF;
    return (h & 0xFFFFFF00u) | (i == 2 || i == 10 ? 0u : (h >> 9) & 1u);
}
// Sprite_SetTint's and Gpu_SetDrawMode's fifth argument.
std::uint32_t Fifth(const std::uint32_t* a, std::uint32_t h) {
    mh::Note(a[4]);
    return h;
}
// The primitive cursor moved on by the size, half the time, inside the buffer.
void Advance(std::uint32_t size) {
    unsigned char* const at = Gfx_PacketNext;
    if (mh::Noise() % 2 && at >= g_prims && at + (size & 0xFF) < g_prims + kPrimBytes / 2) Gfx_PacketNext = at + (size & 0xFF);
}
std::uint32_t Link(const std::uint32_t* a, std::uint32_t h) {
    Advance(a[3]);
    return h;
}
std::uint32_t Commit(const std::uint32_t* a, std::uint32_t h) {
    Advance(a[1]);
    return h;
}
// A primitive setter writes the code byte.
std::uint32_t SetPrim(const std::uint32_t* a, std::uint32_t h) {
    mh::FillBytes(static_cast<unsigned char*>(W(a[0])) + 7, 1);
    return h;
}
// RotTransPers3 / 4: the vertices' contents and where the outputs lie, one
// entry; the screen points (two floats each) and the depth filled.
std::uint32_t Rtp3(const std::uint32_t* a, std::uint32_t h) {
    std::uint32_t f = 0x811C9DC5u;
    for (int i = 0; i < 3; ++i) f = Fnv(f, P(a[i]), 6);
    mh::Note(f ^ (a[4] - a[3]) * 0x10001u ^ (a[5] - a[3]) * 0x1003u);
    for (int i = 3; i < 6; ++i) mh::FillBytes(W(a[i]), 8);
    mh::FillBytes(W(a[6]), 4);
    return h;
}
std::uint32_t Rtp4(const std::uint32_t* a, std::uint32_t h) {
    std::uint32_t f = 0x811C9DC5u;
    for (int i = 0; i < 4; ++i) f = Fnv(f, P(a[i]), 6);
    mh::Note(f ^ (a[5] - a[4]) * 0x10001u ^ (a[6] - a[4]) * 0x1003u ^ (a[7] - a[4]) * 0x101u);
    for (int i = 4; i < 8; ++i) mh::FillBytes(W(a[i]), 8);
    mh::FillBytes(W(a[8]), 4);
    return h;
}
// The matrix calls work on the caller's stack: what is logged is the
// content, and where each block lies relative to the one before - the
// original's MATRIX layout (the translation RotTrans fills at +0x14 of the
// block RotMatrix and MulMatrix0 then fill) has to be ours too.
std::uint32_t g_trans, g_matrix;
std::uint32_t RotTrans(const std::uint32_t* a, std::uint32_t h) {
    mh::NoteBytes(P(a[0]), 6);
    g_trans = a[1];
    mh::FillBytes(W(a[1]), 12);
    return h;
}
std::uint32_t RotMatrix(const std::uint32_t* a, std::uint32_t) {
    mh::Note(Fnv(0x811C9DC5u, P(a[0]), 6) ^ (g_trans - a[1]));
    g_matrix = a[1];
    mh::FillBytes(W(a[1]), 18);
    return a[1];
}
std::uint32_t MulMatrix0(const std::uint32_t* a, std::uint32_t) {
    mh::Note(Fnv(0x811C9DC5u, P(a[1]), 18) ^ (a[1] - g_matrix) ^ (a[2] - g_matrix) << 16);
    mh::FillBytes(W(a[2]), 18);
    return a[2];
}
std::uint32_t SetRot(const std::uint32_t* a, std::uint32_t h) {
    mh::Note(Fnv(0x811C9DC5u, P(a[0]), 18) ^ (a[0] - g_matrix));
    return h;
}
std::uint32_t SetTrans(const std::uint32_t* a, std::uint32_t h) {
    mh::Note(Fnv(0x811C9DC5u, P(a[0] + 0x14), 12) ^ (a[0] - g_matrix));
    return h;
}

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
#define S21_OURS(name) #name, ::bof3::addr::name, KF(&::name)
#define S21_AT(address) #address, address, address

const mh::Callee kCallees[] = {
    // Capcom's and ours the draws call
    {S21_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &IsOut},
    {S21_OURS(Sprite_SetTint), 4, {kAll, kU8, kU8, kU8}, mh::Answer::kGarbage, 0, 0, {}, &Fifth},
    {S21_OURS(Math_Sin), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, &Trig},
    {S21_OURS(Math_Cos), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, &Trig},
    {S21_OURS(Gpu_SetDrawMode), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &Fifth},
    {S21_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &Link},
    {S21_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &Commit},
    {S21_OURS(Gpu_SetPolyG3), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, &SetPrim},
    {S21_OURS(Gpu_SetPolyG4), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, &SetPrim},
    {S21_OURS(Gpu_SetPolyGT4), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, &SetPrim},
    {S21_AT(0x5A76F0), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, &SetPrim},
    {S21_AT(0x5A7570), 1, {kAll}, mh::Answer::kGarbage, 0, 0, {}, &SetPrim},
    {S21_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Gpu_GetClut), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Gte_RotTransPers3), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &Rtp3},
    {S21_OURS(Gte_RotTransPers4), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &Rtp4},
    {S21_OURS(Gte_PrimDepths3_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Gte_PrimDepths3_0C), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Gte_PrimDepths4_10B), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Gte_PrimDepths4_10C), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Gte_PushMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Gte_RotTrans), 2, {0, 0}, mh::Answer::kGarbage, 0, 0, {}, &RotTrans},
    {S21_OURS(Gte_RotMatrix), 2, {0, 0}, mh::Answer::kGarbage, 0, 0, {}, &RotMatrix},
    {S21_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, mh::Answer::kGarbage, 0, 0, {}, &MulMatrix0},
    {S21_OURS(Gte_SetRotMatrix), 1, {0}, mh::Answer::kGarbage, 0, 0, {}, &SetRot},
    {S21_OURS(Gte_SetTransMatrix), 1, {0}, mh::Answer::kGarbage, 0, 0, {}, &SetTrans},
    // a pool slot's free: MAGIC219's (group S37), by its address
    {S21_AT(0x4F6290), 0, {}, mh::Answer::kGarbage, 0, 0},
    // ours, calling one another
    {S21_OURS(FlamePool_Alloc), 0, {}, mh::Answer::kByte, 0, 31},
    {S21_OURS(FlamePool_Dispatch), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Inferno_TargetCentre), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(FlameColumn_DrawDisc), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(FlameColumn_DrawBand), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(FlameSpark_Draw), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(FrostRing_PushMatrix), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(FrostRing_DrawShards), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(FrostRing_DrawDisc), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Iceblast_DrawSpires), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Iceblast_DrawSpiresInner), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(Iceblast_DrawDisc), 0, {}, mh::Answer::kGarbage, 0, 0},
    {S21_OURS(IceShard_Draw), 0, {}, mh::Answer::kGarbage, 0, 0},
};
#undef S21_OURS
#undef S21_AT

// --- the seed and the group's disturbance ----------------------------------------------

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return mh::Pointer(at::kOwner); }
unsigned char Near(unsigned v) { return static_cast<unsigned char>(mh::Half() ? v : v + (mh::Next() % 3) - 1); }
void AllTargets() {
    if (mh::Half()) mh::Mem(at::kTarget)[0] = static_cast<unsigned char>(0x40 | (mh::Next() % 11));
}

void Seed(unsigned k) {
    // every draw writes at the cursor: the fuzz's buffer
    Gfx_PacketNext = g_prims + 0x10 * (mh::Next() % 4);
    // the pool's owners inside what the disturbance may write through
    for (unsigned i = 0; i < 32; ++i) {
        unsigned char* const slot = mh::Mem(kPool + i * 0x84);
        const std::uint32_t v = mh::Next();
        move_script::SetLong(slot + 0x80, static_cast<std::int32_t>(K(v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v >> 3))));
    }
    unsigned char* const sc = Sc();
    switch (k) {
    case kInfernoTask:
    case kFrostTask:
        sc[1] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kIceblastTask:
        sc[1] = static_cast<unsigned char>(mh::Next() % 5);
        break;
    case kIceShardTask:
        // the table has two entries and the bytes past it are not code: every
        // slot the disturbance may make current stays inside it
        for (unsigned t = 0; t < 4; ++t) mh::TaskAt(t)[1] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kInfernoStart:
    case kInfernoTargetCentre:
        AllTargets();
        break;
    case kInfernoEnd:
        if (mh::Often()) sc[0xB] = 0;
        if (mh::Often()) sc[9] = 0;
        break;
    case kFxScorchDelay:
        AllTargets();
        sc[9] = Near(1);
        break;
    case kFxScorchHit:
        AllTargets();
        Owner()[0xB] = Near(8);
        break;
    case kFlameColumnTask:
    case kFlameSparkTask:
        // every entry of the four-entry table, then the draw's gate
        sc[2] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Next() % 4 == 0) sc[0] = 0;
        break;
    case kFlameColumnGrow:
        sc[9] = Near(0xF);
        break;
    case kFlameColumnHold:
        Owner()[0xB] = Near(8);
        break;
    case kFlameColumnFade:
    case kFlameSparkStart:
        sc[9] = Near(1);
        break;
    case kFlameSparkGrow:
        sc[9] = static_cast<unsigned char>(mh::Half() ? 0xA0 : mh::Next());
        break;
    case kFlameSparkStretch:
        sc[0xA] = Near(0x31);
        break;
    case kFlameSparkFade:
        sc[0xB] = Near(1);
        break;
    case kFlamePoolAlloc:
        for (unsigned i = 0; i < 32; ++i) {
            unsigned char* const slot = mh::Mem(kPool + i * 0x84);
            slot[0] = static_cast<unsigned char>(mh::Next() % 5 ? slot[0] | 1 : slot[0] & ~1u);
        }
        if (mh::Next() % 4 == 0) {
            // every slot in use, or all but the last
            for (unsigned i = 0; i < 32; ++i) mh::Mem(kPool + i * 0x84)[0] |= 1;
            if (mh::Half()) mh::Mem(kPool + 31 * 0x84)[0] &= ~1u;
        }
        break;
    case kFrostWait:
        if (mh::Half()) sc[0xB] = 0xFF;
        break;
    case kFrostRingTask:
        sc[1] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Next() % 3 == 0) Owner()[0xB] = 0xFF;
        break;
    case kFrostRingGrow:
    case kFrostRingSpin:
        sc[9] = Near(0xF);
        sc[0xA] = Near(0x10);
        break;
    case kFrostRingFade:
        sc[9] = Near(0x1F);
        break;
    case kFrostRingDrawShards:
    case kFrostRingDrawDisc:
        sc[1] = static_cast<unsigned char>(MH_PICK(2, 3, 1, 4, 0xFF));
        if (mh::Half()) sc[9] = static_cast<unsigned char>(mh::Next() % 0x40);
        break;
    case kIceblastChill:
        sc[0xA] = static_cast<unsigned char>(MH_PICK(0xB, 0xC, 0xD, 0xF, 0x10));
        break;
    case kIceblastRise:
        sc[9] = Near(0x3F);
        break;
    case kIceblastSink:
        sc[9] = Near(1);
        break;
    case kIceblastEnd:
        sc[0xA] = Near(1);
        break;
    case kIceblastDrawSpires:
    case kIceblastDrawSpiresInner:
        if (mh::Often()) sc[9] = static_cast<unsigned char>(mh::Next() % 0x40);
        break;
    case kIceShardWait:
        if (mh::Half()) Owner()[1] = 3;
        sc[9] = Near(1);
        break;
    case kIceShardRun:
        sc[9] = Near(0x1F);
        break;
    case kIceShardDraw:
        if (mh::Often()) sc[0xB] = static_cast<unsigned char>(mh::Next() % 12);
        if (mh::Half()) sc[0xA] = 0;
        if (mh::Half()) sc[9] = static_cast<unsigned char>(mh::Next() % 0x40);
        if (mh::Next() % 4 == 0) {
            // a height exactly 0 (the clamp's edge): row 0, even; the column
            // bytes 0..4 zero and the point bytes from 5 on -4 * +9, so the
            // height -j[col] + j[k] + 4 * +9 is 0 from the fourth column on
            sc[0xB] = 0;
            sc[0xA] = 0;
            sc[9] = static_cast<unsigned char>(1 + mh::Next() % 8);
            std::memset(mh::Mem(kJitter), 0, 5);
            std::memset(mh::Mem(kJitter + 5), static_cast<unsigned char>(-4 * sc[9]), 0x38 - 5);
        }
        break;
    default:
        break;
    }
}

// The cells ours might keep where the originals read again: the primitive
// cursor, a scratch or vertex word, a jitter byte, a byte of a pool slot.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 5) {
    case 0: Gfx_PacketNext = g_prims + 4 * (v % 64); break;
    case 1: move_script::SetWord(mh::Mem(0x903850 + 2 * (v % 8)), h >> 20); break;
    case 2: move_script::SetWord(mh::Mem(0x9037A0 + 2 * (v % 16)), h >> 20); break;
    case 3: mh::Mem(kJitter + v % 0x38)[0] = static_cast<unsigned char>(h >> 24); break;
    default: mh::Mem(kPool + (v % 32) * 0x84 + (h >> 24) % 0x14)[0] = static_cast<unsigned char>(h >> 12); break;
    }
}

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[sizeof kRegions / sizeof kRegions[0] - 1].at = K(g_prims);
    mh::Group group = {
        "magic_s21", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed, &Disturb, 2000,
    };
    group.phase_span = 2;
    mh::Run(group);
}

}  // namespace magic_s21
