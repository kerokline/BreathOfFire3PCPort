// BOF3X_SHADOW=magic_s30: group S30's two overlays (MAGIC130, MAGIC131)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s30.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC130 / MAGIC131 --clones
// (2026-09-26; capstone, every jump internal, no jump table), names given.
// What the group needs beyond the standard set, all through the harness's
// fields (no harness edit):
//
//   - every draw builds its primitives in the fuzz's own packet buffer (the
//     stand-ins never move Gfx_PacketNext), so Gfx_CommitPrim's `effect` logs
//     the buffer as it stands at each commit - else only a draw's last
//     primitive would be compared;
//   - the projections' vertices are logged by what they hold (`deref`), and
//     Gte_RotTransPers4 / Gte_RotTransPers / Gte_StoreDepthF have `effect`s
//     that write where the real ones write (screen floats, a depth), so the
//     mote's truncation to its screen point (the CRT's _ftol, left to the
//     copy: kThrough) sees NaNs, huge and fractional values;
//   - the mote pool 0x6A4E38 and its current cell are a region, every mote's
//     owner +0x28 seeded to a record the disturbance may write through
//     (Kaiser_Task makes it the owner around each mote's call);
//   - KaiserMote_Alloc answers al (`ret_mask` 0xFF), and its recorder answers
//     0xFF or 0..0x2F only (an index past the pool would write outside it).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s30.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s30 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC130 / MAGIC131 --clones, 2026-09-26, names given.
// 0x4E4420: 0x2E bytes
constexpr mh::Imm kImms4E4420[] = {{0xF, 0x4E4450}, {0x17, 0x4F9F70}, {0x22, 0x4F7350}};
// 0x4E4450: 0xEE bytes
constexpr mh::CallSite kCalls4E4450[] = {{0x1, 0x4FC0E0}, {0x37, 0x435180}, {0x67, 0x435180}, {0xDB, 0x587900}, {0xE5, 0x587900}};
// 0x4E4540: 0x12 bytes; +0xB note: jmp through .data 0x65bc54, 10 code entries (a data_tables entry)
// 0x4E4560: 0x3D bytes; +0x15 note: call through .data 0x65bc5c, 8 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E4560[] = {{0x2D, 0x588F20}};
// 0x4E45A0: 0xE4 bytes
constexpr mh::CallSite kCalls4E45A0[] = {{0xBD, 0x5891F0}};
// 0x4E4690: 0x39 bytes
// 0x4E46D0: 0x42 bytes
constexpr mh::CallSite kCalls4E46D0[] = {{0x3C, 0x4351F0}};
// 0x4E4720: 0x4E bytes; +0xB note: call through .data 0x65bc6c, 4 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E4720[] = {{0x21, 0x5A77C0}, {0x2A, 0x461E50}, {0x43, 0x4E4810}, {0x48, 0x4E49F0}};
// 0x4E4770: 0x5F bytes
// 0x4E47D0: 0x1C bytes
// 0x4E47F0: 0x1D bytes
// 0x4E4810: 0x1D1 bytes
constexpr mh::CallSite kCalls4E4810[] = {{0x50, 0x5A7A00}, {0x74, 0x5A7A50}, {0xA5, 0x5A75F0}, {0xAD, 0x5A7780}, {0x100, 0x5A7A00}, {0x131, 0x5A7A50}, {0x1B3, 0x461E50}};
// 0x4E49F0: 0x256 bytes
constexpr mh::CallSite kCalls4E49F0[] = {{0x14, 0x5B93D2}, {0x33, 0x5A7A00}, {0x57, 0x5A7A50}, {0x7B, 0x5A7A00}, {0x9F, 0x5A7A50}, {0xD5, 0x5A7610}, {0xDC, 0x5A7780}, {0x12A, 0x5A7A00}, {0x15B, 0x5A7A50}, {0x18C, 0x5A7A00}, {0x1BD, 0x5A7A50}, {0x237, 0x461E50}};
// 0x4E4C50: 0xD0 bytes
constexpr mh::CallSite kCalls4E4C50[] = {{0x64, 0x5A77C0}, {0x6D, 0x461E50}, {0x98, 0x4E6200}, {0xB8, 0x5A77C0}, {0xC1, 0x461E50}};
constexpr mh::Imm kImms4E4C50[] = {{0x15, 0x4E4D20}, {0x1D, 0x4E4DF0}, {0x25, 0x4E4E40}, {0x2D, 0x4E4EE0}, {0x35, 0x4E4F40}, {0x3D, 0x4E5090}, {0x45, 0x4E5100}, {0x4D, 0x4E5200}};
// 0x4E4D20: 0xC9 bytes
constexpr mh::CallSite kCalls4E4D20[] = {{0x2C, 0x4FC0E0}, {0x61, 0x435180}, {0xBF, 0x587900}};
// 0x4E4DF0: 0x4C bytes
constexpr mh::CallSite kCalls4E4DF0[] = {{0x3B, 0x454590}};
// 0x4E4E40: 0x94 bytes
constexpr mh::CallSite kCalls4E4E40[] = {{0x0, 0x454810}, {0x26, 0x435180}};
// 0x4E4EE0: 0x60 bytes
constexpr mh::CallSite kCalls4E4EE0[] = {{0x11, 0x435180}};
// 0x4E4F40: 0x150 bytes
constexpr mh::CallSite kCalls4E4F40[] = {{0x7C, 0x454590}, {0xA1, 0x454590}, {0xC6, 0x454590}, {0xE4, 0x454590}, {0x100, 0x454590}, {0x13E, 0x536AC0}};
// 0x4E5090: 0x68 bytes
constexpr mh::CallSite kCalls4E5090[] = {{0x0, 0x454810}, {0x57, 0x454590}};
// 0x4E5100: 0xF4 bytes
constexpr mh::CallSite kCalls4E5100[] = {{0x1, 0x454810}, {0x56, 0x5891F0}, {0x76, 0x5891F0}, {0x93, 0x5891F0}, {0x9C, 0x4456C0}, {0xB4, 0x5891F0}};
// 0x4E5200: 0x19 bytes
constexpr mh::CallSite kCalls4E5200[] = {{0x13, 0x4351F0}};
// 0x4E5220: 0x12 bytes; +0xB note: jmp through .data 0x65bc88, 20 code entries (a data_tables entry)
// 0x4E5240: 0x98 bytes; +0x15 note: call through .data 0x65bc98, 16 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E5240[] = {{0x2D, 0x588F20}};
// 0x4E52E0: 0x1E2 bytes
constexpr mh::CallSite kCalls4E52E0[] = {{0x1B5, 0x5891F0}, {0x1BF, 0x587900}};
// 0x4E54D0: 0xC8 bytes
// 0x4E55A0: 0x60 bytes
// 0x4E5600: 0x3B bytes
constexpr mh::CallSite kCalls4E5600[] = {{0x0, 0x5893A0}, {0x20, 0x5891F0}};
// 0x4E5640: 0x178 bytes
constexpr mh::CallSite kCalls4E5640[] = {{0x1, 0x589410}, {0x2A, 0x587900}, {0x38, 0x452F70}, {0x41, 0x435180}, {0x78, 0x435180}, {0xB5, 0x4E6830}, {0x100, 0x4E6830}};
// 0x4E57C0: 0x1B bytes
constexpr mh::CallSite kCalls4E57C0[] = {{0x0, 0x589410}};
// 0x4E57E0: 0xA8 bytes
constexpr mh::CallSite kCalls4E57E0[] = {{0x24, 0x4530D0}};
// 0x4E5890: 0x6D bytes
constexpr mh::CallSite kCalls4E5890[] = {{0x67, 0x4351F0}};
// 0x4E5900: 0x29 bytes; +0xB note: call through .data 0x65bcb8, 8 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E5900[] = {{0x23, 0x4E59A0}};
// 0x4E5930: 0x12 bytes
// 0x4E5950: 0x1D bytes
// 0x4E5970: 0x28 bytes
constexpr mh::CallSite kCalls4E5970[] = {{0xC, 0x454810}};
// 0x4E59A0: 0x96 bytes
constexpr mh::CallSite kCalls4E59A0[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x25, 0x5A75B0}, {0x2D, 0x5A7780}, {0x6E, 0x461E50}, {0x7F, 0x5A77C0}, {0x8B, 0x461E50}};
// 0x4E5A40: 0x33 bytes; +0xB note: call through .data 0x65bccc, 3 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E5A40[] = {{0x23, 0x4B7D40}, {0x28, 0x4E5B40}, {0x2D, 0x5A7BC0}};
// 0x4E5A80: 0x53 bytes
// 0x4E5AE0: 0x1D bytes
// 0x4E5B00: 0x36 bytes
constexpr mh::CallSite kCalls4E5B00[] = {{0x30, 0x4351F0}};
// 0x4E5B40: 0x351 bytes
constexpr mh::CallSite kCalls4E5B40[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0x74, 0x5A7A00}, {0xB8, 0x5A7A00}, {0xD2, 0x5A7A50}, {0xFE, 0x5A7A00}, {0x118, 0x5A7A50}, {0x131, 0x5A7A00}, {0x198, 0x5A7A00}, {0x1B8, 0x5A7A50}, {0x1F2, 0x5A7A00}, {0x212, 0x5A7A50}, {0x231, 0x5A75D0}, {0x239, 0x5A7780}, {0x24C, 0x5A79A0}, {0x25C, 0x5A79E0}, {0x2E7, 0x5A85F0}, {0x2F0, 0x5A9290}, {0x320, 0x461E50}};
// 0x4E5EA0: 0x33 bytes; +0xB note: call through .data 0x65bce8, 5 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E5EA0[] = {{0x23, 0x4B7D40}, {0x28, 0x4E5FB0}, {0x2D, 0x5A7BC0}};
// 0x4E5EE0: 0x60 bytes
// 0x4E5F40: 0x29 bytes
// 0x4E5F70: 0x36 bytes
constexpr mh::CallSite kCalls4E5F70[] = {{0x30, 0x4351F0}};
// 0x4E5FB0: 0x250 bytes
constexpr mh::CallSite kCalls4E5FB0[] = {{0x26, 0x5A77C0}, {0x2F, 0x461E50}, {0x72, 0x5A7A00}, {0x8B, 0x5A7A50}, {0xF0, 0x5A7A00}, {0x109, 0x5A7A50}, {0x14A, 0x5A75D0}, {0x15B, 0x5A7780}, {0x178, 0x5A79A0}, {0x187, 0x5A79E0}, {0x200, 0x5A85F0}, {0x209, 0x5A9290}, {0x225, 0x461E50}};
// 0x4E6200: 0x12 bytes; +0xB note: jmp through .data 0x65bcf4, 2 code entries (a data_tables entry)
// 0x4E6220: 0x2E bytes; +0xB note: call through .data 0x65bd0c, 3 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E6220[] = {{0x23, 0x4E68B0}, {0x28, 0x4E6670}};
// 0x4E6250: 0x9D bytes
constexpr mh::CallSite kCalls4E6250[] = {{0x20, 0x5A7A00}, {0x44, 0x5A7A50}};
// 0x4E62F0: 0x96 bytes
constexpr mh::CallSite kCalls4E62F0[] = {{0x38, 0x5A7A00}, {0x61, 0x5A7A50}};
// 0x4E6390: 0x9F bytes
constexpr mh::CallSite kCalls4E6390[] = {{0x2A, 0x5A7A00}, {0x53, 0x5A7A50}, {0x99, 0x4E6880}};
// 0x4E6430: 0x3C bytes; +0xB note: call through .data 0x65bd58, 46 code entries (a data_tables entry)
constexpr mh::CallSite kCalls4E6430[] = {{0x32, 0x4E68B0}, {0x37, 0x4E6670}};
// 0x4E6470: 0xB6 bytes
constexpr mh::CallSite kCalls4E6470[] = {{0x20, 0x5A7A00}, {0x44, 0x5A7A50}};
// 0x4E6530: 0x96 bytes
constexpr mh::CallSite kCalls4E6530[] = {{0x38, 0x5A7A00}, {0x61, 0x5A7A50}};
// 0x4E65D0: 0x9F bytes
constexpr mh::CallSite kCalls4E65D0[] = {{0x2A, 0x5A7A00}, {0x53, 0x5A7A50}, {0x99, 0x4E6880}};
// 0x4E6670: 0x1B6 bytes
constexpr mh::CallSite kCalls4E6670[] = {{0x6, 0x5B93D2}, {0x28, 0x5B93D2}, {0x66, 0x5A75F0}, {0x6E, 0x5A7780}, {0xB3, 0x5A7A00}, {0xE1, 0x5A7A50}, {0x11C, 0x5A7A00}, {0x14A, 0x5A7A50}, {0x195, 0x461E50}};
// 0x4E6830: 0x4A bytes
// 0x4E6880: 0x2F bytes
// 0x4E68B0: 0x95 bytes
constexpr mh::CallSite kCalls4E68B0[] = {{0x43, 0x5A7750}, {0x5B, 0x5A8250}, {0x64, 0x5A9110}, {0x6E, 0x5B9550}, {0x80, 0x5B9550}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Venom_Task", 0x4E4420, 0x2E, nullptr, 0, kImms4E4420, MH_N(kImms4E4420), nullptr, 0, reinterpret_cast<const void*>(&::Venom_Task)},
    {"Venom_Start", 0x4E4450, 0xEE, kCalls4E4450, MH_N(kCalls4E4450), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Venom_Start)},
    {"VenomChild_Task", 0x4E4540, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomChild_Task)},
    {"VenomSprite_Run", 0x4E4560, 0x3D, kCalls4E4560, MH_N(kCalls4E4560), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomSprite_Run)},
    {"VenomSprite_Start", 0x4E45A0, 0xE4, kCalls4E45A0, MH_N(kCalls4E45A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomSprite_Start)},
    {"VenomSprite_ShadeUp", 0x4E4690, 0x39, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomSprite_ShadeUp)},
    {"VenomSprite_ShadeDown", 0x4E46D0, 0x42, kCalls4E46D0, MH_N(kCalls4E46D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomSprite_ShadeDown)},
    {"VenomRing_Run", 0x4E4720, 0x4E, kCalls4E4720, MH_N(kCalls4E4720), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomRing_Run)},
    {"VenomRing_Start", 0x4E4770, 0x5F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomRing_Start)},
    {"VenomRing_Grow", 0x4E47D0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomRing_Grow)},
    {"MagicFx_CountDown9", 0x4E47F0, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_CountDown9)},
    {"VenomRing_DrawFan", 0x4E4810, 0x1D1, kCalls4E4810, MH_N(kCalls4E4810), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomRing_DrawFan)},
    {"VenomRing_DrawBand", 0x4E49F0, 0x256, kCalls4E49F0, MH_N(kCalls4E49F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::VenomRing_DrawBand)},
    {"Kaiser_Task", 0x4E4C50, 0xD0, kCalls4E4C50, MH_N(kCalls4E4C50), kImms4E4C50, MH_N(kImms4E4C50), nullptr, 0, reinterpret_cast<const void*>(&::Kaiser_Task)},
    {"Kaiser_Start", 0x4E4D20, 0xC9, kCalls4E4D20, MH_N(kCalls4E4D20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Kaiser_Start)},
    {"Kaiser_LoadFormFile", 0x4E4DF0, 0x4C, kCalls4E4DF0, MH_N(kCalls4E4DF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Kaiser_LoadFormFile)},
    {"Kaiser_HideParty", 0x4E4E40, 0x94, kCalls4E4E40, MH_N(kCalls4E4E40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Kaiser_HideParty)},
    {"Kaiser_WaitChildren", 0x4E4EE0, 0x60, kCalls4E4EE0, MH_N(kCalls4E4EE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Kaiser_WaitChildren)},
    {"Kaiser_ReloadParty", 0x4E4F40, 0x150, kCalls4E4F40, MH_N(kCalls4E4F40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Kaiser_ReloadParty)},
    {"Kaiser_LoadSetFile", 0x4E5090, 0x68, kCalls4E5090, MH_N(kCalls4E5090), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Kaiser_LoadSetFile)},
    {"Kaiser_ShowParty", 0x4E5100, 0xF4, kCalls4E5100, MH_N(kCalls4E5100), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Kaiser_ShowParty)},
    {"MagicFx_EndWhenChildrenDone", 0x4E5200, 0x19, kCalls4E5200, MH_N(kCalls4E5200), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_EndWhenChildrenDone)},
    {"KaiserChild_Task", 0x4E5220, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserChild_Task)},
    {"KaiserSprite_Run", 0x4E5240, 0x98, kCalls4E5240, MH_N(kCalls4E5240), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_Run)},
    {"KaiserSprite_Start", 0x4E52E0, 0x1E2, kCalls4E52E0, MH_N(kCalls4E52E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_Start)},
    {"KaiserSprite_Glide", 0x4E54D0, 0xC8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_Glide)},
    {"KaiserSprite_Brake", 0x4E55A0, 0x60, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_Brake)},
    {"KaiserSprite_Tick", 0x4E5600, 0x3B, kCalls4E5600, MH_N(kCalls4E5600), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_Tick)},
    {"KaiserSprite_Breathe", 0x4E5640, 0x178, kCalls4E5640, MH_N(kCalls4E5640), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_Breathe)},
    {"KaiserSprite_WaitScript", 0x4E57C0, 0x1B, kCalls4E57C0, MH_N(kCalls4E57C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_WaitScript)},
    {"KaiserSprite_Leave", 0x4E57E0, 0xA8, kCalls4E57E0, MH_N(kCalls4E57E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_Leave)},
    {"KaiserSprite_Exit", 0x4E5890, 0x6D, kCalls4E5890, MH_N(kCalls4E5890), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserSprite_Exit)},
    {"KaiserFlash_Run", 0x4E5900, 0x29, kCalls4E5900, MH_N(kCalls4E5900), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserFlash_Run)},
    {"MagicFx_ClearCount9", 0x4E5930, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_ClearCount9)},
    {"MagicFx_CountUp9By2", 0x4E5950, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::MagicFx_CountUp9By2)},
    {"KaiserFlash_WaitLoad", 0x4E5970, 0x28, kCalls4E5970, MH_N(kCalls4E5970), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserFlash_WaitLoad)},
    {"KaiserFlash_Draw", 0x4E59A0, 0x96, kCalls4E59A0, MH_N(kCalls4E59A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserFlash_Draw)},
    {"KaiserPillar_Run", 0x4E5A40, 0x33, kCalls4E5A40, MH_N(kCalls4E5A40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserPillar_Run)},
    {"KaiserPillar_Start", 0x4E5A80, 0x53, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserPillar_Start)},
    {"KaiserPillar_Grow", 0x4E5AE0, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserPillar_Grow)},
    {"KaiserPillar_Fade", 0x4E5B00, 0x36, kCalls4E5B00, MH_N(kCalls4E5B00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserPillar_Fade)},
    {"KaiserPillar_Draw", 0x4E5B40, 0x351, kCalls4E5B40, MH_N(kCalls4E5B40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserPillar_Draw)},
    {"KaiserRing_Run", 0x4E5EA0, 0x33, kCalls4E5EA0, MH_N(kCalls4E5EA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserRing_Run)},
    {"KaiserRing_Start", 0x4E5EE0, 0x60, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserRing_Start)},
    {"KaiserRing_Grow", 0x4E5F40, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserRing_Grow)},
    {"KaiserRing_Fade", 0x4E5F70, 0x36, kCalls4E5F70, MH_N(kCalls4E5F70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserRing_Fade)},
    {"KaiserRing_Draw", 0x4E5FB0, 0x250, kCalls4E5FB0, MH_N(kCalls4E5FB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserRing_Draw)},
    {"KaiserMote_Task", 0x4E6200, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMote_Task)},
    {"KaiserMoteA_Run", 0x4E6220, 0x2E, kCalls4E6220, MH_N(kCalls4E6220), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMoteA_Run)},
    {"KaiserMoteA_Start", 0x4E6250, 0x9D, kCalls4E6250, MH_N(kCalls4E6250), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMoteA_Start)},
    {"KaiserMoteA_Spread", 0x4E62F0, 0x96, kCalls4E62F0, MH_N(kCalls4E62F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMoteA_Spread)},
    {"KaiserMoteA_Fade", 0x4E6390, 0x9F, kCalls4E6390, MH_N(kCalls4E6390), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMoteA_Fade)},
    {"KaiserMoteB_Run", 0x4E6430, 0x3C, kCalls4E6430, MH_N(kCalls4E6430), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMoteB_Run)},
    {"KaiserMoteB_Start", 0x4E6470, 0xB6, kCalls4E6470, MH_N(kCalls4E6470), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMoteB_Start)},
    {"KaiserMoteB_Spread", 0x4E6530, 0x96, kCalls4E6530, MH_N(kCalls4E6530), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMoteB_Spread)},
    {"KaiserMoteB_Fade", 0x4E65D0, 0x9F, kCalls4E65D0, MH_N(kCalls4E65D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMoteB_Fade)},
    {"KaiserMote_Draw", 0x4E6670, 0x1B6, kCalls4E6670, MH_N(kCalls4E6670), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMote_Draw)},
    {"KaiserMote_Alloc", 0x4E6830, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMote_Alloc), 0xFF},
    {"KaiserMote_Free", 0x4E6880, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMote_Free)},
    {"KaiserMote_UpdateScreenXY", 0x4E68B0, 0x95, kCalls4E68B0, MH_N(kCalls4E68B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::KaiserMote_UpdateScreenXY)},
};
#undef MH_N

enum : unsigned {
    kVenom_Task, kVenom_Start, kVenomChild_Task, kVenomSprite_Run, kVenomSprite_Start, kVenomSprite_ShadeUp,
    kVenomSprite_ShadeDown, kVenomRing_Run, kVenomRing_Start, kVenomRing_Grow, kMagicFx_CountDown9, kVenomRing_DrawFan,
    kVenomRing_DrawBand,
    kKaiser_Task, kKaiser_Start, kKaiser_LoadFormFile, kKaiser_HideParty, kKaiser_WaitChildren, kKaiser_ReloadParty,
    kKaiser_LoadSetFile, kKaiser_ShowParty, kMagicFx_EndWhenChildrenDone, kKaiserChild_Task, kKaiserSprite_Run,
    kKaiserSprite_Start, kKaiserSprite_Glide, kKaiserSprite_Brake, kKaiserSprite_Tick, kKaiserSprite_Breathe,
    kKaiserSprite_WaitScript, kKaiserSprite_Leave, kKaiserSprite_Exit, kKaiserFlash_Run, kMagicFx_ClearCount9,
    kMagicFx_CountUp9By2, kKaiserFlash_WaitLoad, kKaiserFlash_Draw, kKaiserPillar_Run, kKaiserPillar_Start,
    kKaiserPillar_Grow, kKaiserPillar_Fade, kKaiserPillar_Draw, kKaiserRing_Run, kKaiserRing_Start, kKaiserRing_Grow,
    kKaiserRing_Fade, kKaiserRing_Draw, kKaiserMote_Task, kKaiserMoteA_Run, kKaiserMoteA_Start, kKaiserMoteA_Spread,
    kKaiserMoteA_Fade, kKaiserMoteB_Run, kKaiserMoteB_Start, kKaiserMoteB_Spread, kKaiserMoteB_Fade, kKaiserMote_Draw,
    kKaiserMote_Alloc, kKaiserMote_Free, kKaiserMote_UpdateScreenXY, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the state ----------------------------------------------------------------

constexpr std::uint32_t kScratch = 0x903850, kVertices = 0x9037A0, kPacketNext = 0x7E0670, kSpriteBank = 0x9039D8;
constexpr std::uint32_t kMotePool = 0x6A4E38, kMoteStride = 0x2C, kMoteCurrent = 0x6A5678;
constexpr unsigned kMotes = 48;
constexpr std::uint32_t kFacing = 0x904AAC, kForm = 0x904B89, kPartySet = 0x90412C, kMemberCount = 0x929EC0;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packets the draws fill: Gfx_PacketNext is aimed at one of four places
// in this buffer (and moved between them by the disturbance).
alignas(16) unsigned char g_packets[0x200];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 3) * 0x40; }
unsigned char* Mote(unsigned i) { return mh::Mem(kMotePool + i * kMoteStride); }

const mh::Region kRegions[] = {
    {kScratch, 0x10},
    {kVertices, 0x20},
    {kPacketNext, 4},
    {Key(g_packets), sizeof g_packets},
    {kSpriteBank, 4},
    {0x80F980, 0x200},              // Gfx_ClutStrip row 1 (KaiserSprite_Run's nine at 0x80F9C2 inside)
    {0x812980, 0x200},              // Gfx_ClutStrip row 26
    {0x80B980, 0x220},              // Gfx_ClutStripSource row 1, and the 0x107 words the sprite cycle reads
    {0x80E980, 0x200},              // Gfx_ClutStripSource row 26
    {kMotePool, kMotes * kMoteStride + 4},   // KaiserMote_Pool and KaiserMote_Current
    {0x905E60, 8},                  // Field_Kind2Z, Field_Kind2X
    {kMemberCount, 4},              // Field_MemberCount
    {0x904B88, 4},                  // 0x904B89
    {kPartySet, 4},                 // the loaded party set
};

// --- the callees the standard set lacks ---------------------------------------

#define S30_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S30_RAW(address) #address, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;

// Each commit logs the packet buffer as it stands.
std::uint32_t Committed(const std::uint32_t*, std::uint32_t answer) {
    mh::NoteBytes(g_packets, sizeof g_packets);
    return answer;
}
// A screen float the projections write: small values with fractions, and
// the NaN, infinities and out-of-range values _ftol answers 0 for.
float ScreenFloat() {
    const std::uint32_t n = mh::Noise();
    switch (n % 8) {
    case 0: { std::uint32_t bits = 0x7FC00000u; float f; std::memcpy(&f, &bits, 4); return f; }
    case 1: return 1e20f;
    case 2: return -3e19f;
    case 3: return static_cast<float>(static_cast<int>((n >> 3) % 200001) - 100000) + 0.75f;
    default: return static_cast<float>(static_cast<int>((n >> 3) % 1201) - 600) + static_cast<float>((n >> 16) & 3) * 0.25f;
    }
}
void PutScreen(std::uint32_t at) {
    auto* p = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(at));
    const float x = ScreenFloat(), y = ScreenFloat();
    std::memcpy(p, &x, 4);
    std::memcpy(p + 4, &y, 4);
}
std::uint32_t Rtp1Effect(const std::uint32_t* a, std::uint32_t answer) {
    PutScreen(a[1]);
    return answer;
}
std::uint32_t Rtp4Effect(const std::uint32_t* a, std::uint32_t answer) {
    for (unsigned i = 4; i < 8; ++i) PutScreen(a[i]);
    return answer;
}
std::uint32_t DepthEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::FillBytes(reinterpret_cast<void*>(static_cast<std::uintptr_t>(a[0])), 4);
    return answer;
}

const mh::Callee kCallees[] = {
    {S30_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S30_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &Committed},
    {S30_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S30_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S30_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S30_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S30_OURS(Gpu_SetPolyF4), 1, {kAll}, kG, 0, 0},
    {S30_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S30_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S30_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S30_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    // the originals push the depth and flag outputs in their own frames: masked off
    {S30_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6},
     &Rtp4Effect},
    {S30_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0},
    {S30_OURS(Gpu_SetTile1), 1, {kAll}, kG, 0, 0},
    {S30_OURS(Gte_RotTransPers), 4, {0, kAll, 0, 0}, kG, 0, 0, {6}, &Rtp1Effect},
    {S30_OURS(Gte_StoreDepthF), 1, {kAll}, kG, 0, 0, {}, &DepthEffect},
    // the CRT's _ftol: pure x87, the copy keeps calling it; ours truncates inline
    {S30_RAW(0x5B9550), 0, {}, mh::Answer::kThrough, 0, 0},
    {S30_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0},
    {S30_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0},
    {S30_OURS(LoadDatFile), 1, {kAll}, kG, 0, 0},
    {S30_OURS(File_LoadDone), 0, {}, mh::Answer::kBool, 0, 0},
    {S30_OURS(PartySet_Select), 2, {kU8, kU8}, kG, 0, 0},
    {S30_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    // the group's own, called directly (by address) by its others
    {S30_RAW(0x4E4810), 0, {}, mh::Answer::kPhase, 0, 0},                     // VenomRing_DrawFan
    {S30_RAW(0x4E49F0), 0, {}, mh::Answer::kPhase, 0, 0},                     // VenomRing_DrawBand
    {S30_RAW(0x4E59A0), 0, {}, mh::Answer::kPhase, 0, 0},                     // KaiserFlash_Draw
    {S30_RAW(0x4E5B40), 0, {}, mh::Answer::kPhase, 0, 0},                     // KaiserPillar_Draw
    {S30_RAW(0x4E5FB0), 0, {}, mh::Answer::kPhase, 0, 0},                     // KaiserRing_Draw
    {S30_RAW(0x4E6200), 0, {kMoteCurrent}, mh::Answer::kPhase, 0, 0},         // KaiserMote_Task
    {S30_RAW(0x4E68B0), 0, {kMoteCurrent}, mh::Answer::kPhase, 0, 0},         // KaiserMote_UpdateScreenXY
    {S30_RAW(0x4E6670), 0, {kMoteCurrent}, mh::Answer::kPhase, 0, 0},         // KaiserMote_Draw
    {S30_RAW(0x4E6880), 0, {kMoteCurrent}, mh::Answer::kPhase, 0, 0},         // KaiserMote_Free
    {S30_RAW(0x4E6830), 0, {}, mh::Answer::kByte, 0xFF, 0x2F},                // KaiserMote_Alloc: none, or 0..47
};
#undef S30_OURS
#undef S30_RAW

// The .data dispatch tables the dispatchers read in place (symbols.toml).
const mh::DataTable kTables[] = {
    {0x65BC54, 2}, {0x65BC5C, 4}, {0x65BC6C, 4},                   // Venom_ChildKinds, VenomSprite_ / VenomRing_Phases
    {0x65BC88, 4}, {0x65BC98, 8}, {0x65BCB8, 5}, {0x65BCCC, 3},   // Kaiser_ChildKinds, KaiserSprite_ / Flash_ / Pillar_Phases
    {0x65BCE8, 3}, {0x65BCF4, 2}, {0x65BD0C, 3}, {0x65BD58, 3},   // KaiserRing_Phases, KaiserMote_Kinds, MoteA_ / MoteB_Phases
};

// --- the seed and the disturbance ------------------------------------------------

// 0x904B89 values whose 0x64E9BC entry is not null (entries 10, 19, 20 are:
// docs/magic_s30.md section 8); the party sets MAGIC131 tests, and others.
// Both by a number the caller draws: the seed's from mh::Next, the
// disturbance's from its hash (the two passes must see the same).
constexpr unsigned char kForms[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14, 15, 16, 17, 18, 21, 22, 23, 24};
constexpr unsigned char kSets[] = {7, 0xD, 0xE, 0xF, 7, 0xD, 0xE, 0xF, 0, 3, 0x10, 0x12};
unsigned char Form(std::uint32_t n) { return kForms[n % sizeof kForms]; }
unsigned char PartySetValue(std::uint32_t n) {
    const unsigned char v = kSets[n % sizeof kSets];
    return (n >> 8) % 4 == 0 ? static_cast<unsigned char>(v | 0x80) : v;
}

unsigned char* Sc() { return Sprite_Current; }

void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFF;
    const auto word = static_cast<std::uint16_t>(h >> 16);
    switch ((h >> 8) % 11) {
    case 0: mh::SetPointer(kPacketNext, PacketAt(v)); break;
    case 1: SetWord(mh::Mem(kScratch + (v % 8) * 2), word); break;
    case 2: SetWord(mh::Mem(kVertices + (v % 16) * 2), word); break;
    case 3: mh::SetPointer(kMoteCurrent, Mote(v % kMotes)); break;
    case 4: {
        // a field of the current mote, never its owner +0x28
        static const unsigned kFields[] = {0, 5, 6, 7, 0xC, 0xD, 0x14, 0x18, 0x1E, 0x20, 0x22, 0x26};
        mh::Pointer(kMoteCurrent)[kFields[v % 12]] = static_cast<unsigned char>(word);
        break;
    }
    case 5: SetWord(mh::Mem(0x905E60 + (v % 4) * 2), word); break;
    case 6: mh::Mem(kFacing)[0] = static_cast<unsigned char>(word); break;
    case 7: mh::Mem(kPartySet)[0] = PartySetValue(h >> 16); break;
    case 8: mh::Mem(kMemberCount)[0] = static_cast<unsigned char>(word % 5); break;
    case 9: mh::Mem(kForm)[0] = Form(h >> 16); break;
    case 10: {
        static const unsigned kFields[] = {4, 0xC, 0xE, 0x10, 0x14, 0x15, 0x18, 0x1C, 0x2E, 0x2F, 0x31, 0x5D};
        Sc()[kFields[v % 12]] = static_cast<unsigned char>(word);
        break;
    }
    default: break;
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    mh::SetPointer(kPacketNext, PacketAt(mh::Next()));
    for (unsigned i = 0; i < kMotes; ++i) {
        const std::uint32_t o = mh::Next();
        mh::SetPointer(kMotePool + i * kMoteStride + 0x28, o & 4 ? mh::SpriteRecord(o & 1) : mh::TaskAt(o));
    }
    mh::SetPointer(kMoteCurrent, Mote(mh::Next() % kMotes));
    unsigned char* const cur = mh::Pointer(kMoteCurrent);
    mh::Mem(kForm)[0] = Form(mh::Next());
    mh::Mem(kPartySet)[0] = PartySetValue(mh::Next());
    mh::Mem(kMemberCount)[0] = static_cast<unsigned char>(mh::Next() % 5);
    if (mh::Often()) mh::Mem(kFacing)[0] = static_cast<unsigned char>(mh::Next() % 4);
    const auto low = [](unsigned n) { return static_cast<unsigned char>(1 + mh::Next() % n); };
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kVenom_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kVenomChild_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kKaiser_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 8); break;
    case kKaiserChild_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 4); break;
    case kVenomSprite_Run: case kVenomRing_Run: case kKaiserSprite_Run: case kKaiserFlash_Run: case kKaiserPillar_Run:
    case kKaiserRing_Run: {
        const unsigned n = k == kKaiserSprite_Run ? 8 : k == kKaiserFlash_Run ? 5 : k == kVenomSprite_Run || k == kVenomRing_Run ? 4 : 3;
        sc[2] = static_cast<unsigned char>(mh::Next() % n);
        const std::uint32_t pick = mh::Next() % 4;
        if (pick == 0) sc[0] = 0;
        else if (pick == 1) sc[0] = static_cast<unsigned char>(sc[0] & ~1u);
        else if (pick == 2) sc[0] = static_cast<unsigned char>(sc[0] | 1);
        if (k == kKaiserSprite_Run) {
            if (mh::Half()) Frame_Counter &= ~3u;
            if (mh::Often()) sc[4] = static_cast<unsigned char>(0x6F + mh::Next() % 0x10);
        }
        break;
    }
    case kKaiserMote_Task: cur[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kKaiserMoteA_Run: case kKaiserMoteB_Run:
        cur[2] = static_cast<unsigned char>(mh::Next() % 3);
        if (mh::Next() % 4 == 0) cur[0] = 0;
        if (k == kKaiserMoteB_Run) {
            const std::uint32_t pick = mh::Next() % 3;
            if (pick != 2) SetWord(cur + 0x26, pick);
        }
        break;
    // the count-downs: one or two steps from their ends
    case kMagicFx_CountDown9: case kKaiser_LoadFormFile: case kKaiser_ReloadParty: case kKaiserSprite_Start:
    case kKaiserSprite_Glide: case kKaiserSprite_Brake: case kKaiserSprite_Tick: case kKaiserSprite_Breathe:
    case kKaiserSprite_Leave:
        if (mh::Often()) sc[9] = low(2);
        if (k == kKaiser_LoadFormFile && mh::Often()) mh::Mem(kForm)[0] = static_cast<unsigned char>(MH_PICK(7, 8, 9, 0));
        if (k == kKaiser_ReloadParty)
            for (unsigned i = 0; i < 5; ++i) {
                unsigned char* const m = mh::PartyOf(static_cast<unsigned char>(i));
                if (mh::Half()) m[0x89] = 4;
                if (mh::Half()) m[0x134] = static_cast<unsigned char>(m[0x134] | 1);
                else m[0x134] = static_cast<unsigned char>(m[0x134] & ~1u);
            }
        break;
    case kVenomSprite_ShadeUp: if (mh::Often()) sc[0x5D] = static_cast<unsigned char>(MH_PICK(0xF0, 0xF8, 0xE8)); break;
    case kVenomSprite_ShadeDown: if (mh::Often()) sc[0x5D] = static_cast<unsigned char>(MH_PICK(0x88, 0x90, 0x78)); break;
    case kVenomRing_Start: if (mh::Often()) sc[0xB] = static_cast<unsigned char>(mh::Next() % 6); break;
    case kVenomRing_Grow: if (mh::Often()) sc[0xA] = static_cast<unsigned char>(MH_PICK(0xE, 0xF, 0x10)); break;
    case kKaiser_WaitChildren: case kMagicFx_EndWhenChildrenDone: if (mh::Half()) sc[0xB] = 0; break;
    case kKaiser_ShowParty:
        for (unsigned i = 0; i < 5; ++i) {
            unsigned char* const m = mh::PartyOf(static_cast<unsigned char>(i));
            SetWord(m + 0x90, MH_PICK(0, 0x80, 0x2000, 0x2080, 0x800, 0x7F7F));
            m[0x91] = static_cast<unsigned char>(mh::Half() ? m[0x91] | 8 : m[0x91] & ~8u);
        }
        break;
    case kKaiserSprite_Exit:
        if (mh::Often()) sc[9] = static_cast<unsigned char>(MH_PICK(7, 8, 6));
        if (mh::Often()) sc[0xA] = low(2);
        break;
    case kMagicFx_CountUp9By2: if (mh::Often()) sc[9] = static_cast<unsigned char>(MH_PICK(0xC, 0xE, 0xF)); break;
    case kKaiserFlash_WaitLoad: if (mh::Half()) mh::Pointer(mh::at::kOwner)[9] = 0; break;
    case kKaiserPillar_Grow: if (mh::Often()) sc[9] = static_cast<unsigned char>(MH_PICK(0x38, 0x3C, 0x3E)); break;
    case kKaiserPillar_Fade: if (mh::Often()) sc[0x5D] = static_cast<unsigned char>(MH_PICK(4, 8, 6)); break;
    case kKaiserRing_Grow: if (mh::Often()) SetLong(sc + 0x14, static_cast<std::int32_t>(MH_PICK(0x1C0, 0x1E0, 0x1F0))); break;
    case kKaiserRing_Fade: if (mh::Often()) sc[9] = static_cast<unsigned char>(MH_PICK(2, 4, 3)); break;
    case kKaiserMoteA_Start: if (mh::Often()) cur[7] = static_cast<unsigned char>(mh::Next() % 8); break;
    case kKaiserMoteB_Start: if (mh::Often()) cur[7] = static_cast<unsigned char>(mh::Next() % 28); break;
    case kKaiserMoteA_Spread: case kKaiserMoteB_Spread:
        if (mh::Often()) SetWord(cur + 0xC, MH_PICK(0x1C0, 0x1E0, 0x1F0));
        if (mh::Often()) cur[7] = static_cast<unsigned char>(mh::Next() % 28);
        break;
    case kKaiserMoteA_Fade: case kKaiserMoteB_Fade:
        if (mh::Often()) cur[5] = static_cast<unsigned char>(MH_PICK(2, 4, 3));
        break;
    case kKaiserMote_Draw: if (mh::Half()) cur[1] = 0; break;
    case kKaiserMote_Alloc: {
        // all taken, or the first n
        const unsigned n = mh::Half() ? kMotes : mh::Next() % kMotes;
        for (unsigned i = 0; i < n; ++i) Mote(i)[0] = static_cast<unsigned char>(Mote(i)[0] | 1);
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    const mh::Group group = {
        "magic_s30", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000,
    };
    mh::Run(group);
}

}  // namespace magic_s30
