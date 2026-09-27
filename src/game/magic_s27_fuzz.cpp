// BOF3X_SHADOW=magic_s27: group S27's three overlays (MAGIC118, 120, 121)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s27.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC118 / 120 / 121 --clones
// (2026-09-26; capstone, every jump internal but the two jump tables of
// WhelpBreathSprite_Start and WhelpBreathFlames_PushMatrix, which the harness
// moves into the copy), names given. What the group adds to the harness, all
// through its fields (no harness edit):
//
//   - the draws' commits (Gfx_CommitPrim, MapView_LinkPrimAt) log the
//     primitive as it stands and advance Gfx_PacketNext as the real ones do,
//     so every primitive of a draw is compared, not only the last;
//   - the projections log their vertices by what they hold (`deref`), and
//     WhelpBreathFlames_PushMatrix's GTE callees write a result where the real
//     ones write;
//   - WhelpBreathSprite_Run's eight phases log the sprite bank pointer the
//     task switched to around them;
//   - `settle` keeps WhelpBreathBeam_Draw's divisor (+0xB) off 0 - the
//     original divides by it; ours aborts there - and the two beams' step
//     counts (+0xA) short enough for the log.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s27.h"
#include "game/magic_s27_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s27 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

// tools/magic_rows.py --unit MAGIC118 / 120 / 121 --clones, 2026-09-26, names given.
constexpr mh::CallSite kCalls4DA220[] = {{0x4B, 0x4B7D40}, {0x50, 0x4DA8B0}, {0x55, 0x4DAB00}, {0x5A, 0x5A7BC0}};
constexpr mh::Imm kImms4DA220[] = {{0xF, 0x4DA290}, {0x17, 0x4DA370}, {0x22, 0x4A6640}, {0x2A, 0x4DA3B0}, {0x32, 0x4F7350}};
constexpr mh::CallSite kCalls4DA290[] = {{0x3B, 0x435180}, {0xC4, 0x587900}};
constexpr mh::CallSite kCalls4DA370[] = {{0x21, 0x452F70}};
constexpr mh::CallSite kCalls4DA3D0[] = {{0x23, 0x4B7D40}, {0x28, 0x4DA540}, {0x2D, 0x5A7BC0}};
constexpr mh::CallSite kCalls4DA4E0[] = {{0x50, 0x4351F0}};
constexpr mh::CallSite kCalls4DA540[] = {{0x4E, 0x5A7A50}, {0x67, 0x5A7A00}, {0x80, 0x5A7A00}, {0x9B, 0x5A7A50}, {0xB4, 0x5A7A00}, {0x129, 0x5A7A50}, {0x142, 0x5A7A00}, {0x162, 0x5A7A00}, {0x1A0, 0x5A7A50}, {0x1B9, 0x5A7A00}, {0x1FE, 0x5A77C0}, {0x209, 0x572FA0}, {0x215, 0x5A7630}, {0x21D, 0x5A7780}, {0x27F, 0x5A79A0}, {0x28F, 0x5A79E0}, {0x327, 0x5A85F0}, {0x330, 0x5A93A0}, {0x33B, 0x572FA0}};
constexpr mh::CallSite kCalls4DA8B0[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x54, 0x5A7A00}, {0x6D, 0x5A7A50}, {0x86, 0x5A7A00}, {0x9F, 0x5A7A50}, {0xDD, 0x5A7610}, {0xE4, 0x5A7780}, {0x11E, 0x5A7A00}, {0x137, 0x5A7A50}, {0x150, 0x5A7A00}, {0x169, 0x5A7A50}, {0x1AF, 0x5A85F0}, {0x1B8, 0x5A9350}, {0x207, 0x461E50}, {0x22C, 0x5A77C0}, {0x235, 0x461E50}};
constexpr mh::CallSite kCalls4DAB00[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x51, 0x5A7A00}, {0x6A, 0x5A7A50}, {0xA6, 0x5A75F0}, {0xAE, 0x5A7780}, {0xDC, 0x5A7A00}, {0xF5, 0x5A7A50}, {0x132, 0x5A84A0}, {0x138, 0x5A9310}, {0x18F, 0x461E50}, {0x1B6, 0x5A77C0}, {0x1BF, 0x461E50}};
constexpr mh::Imm kImms4DACD0[] = {{0xF, 0x4DAD00}, {0x17, 0x4DAE80}, {0x22, 0x4BDC10}};
constexpr mh::CallSite kCalls4DAD00[] = {{0x49, 0x435180}, {0x7C, 0x435180}, {0xBB, 0x435180}};
constexpr mh::CallSite kCalls4DAE80[] = {{0xF, 0x435180}, {0x41, 0x435180}};
constexpr mh::CallSite kCalls4DAF20[] = {{0x2A, 0x4DB0D0}};
constexpr mh::CallSite kCalls4DAF50[] = {{0x2A, 0x4FC0E0}, {0x2F, 0x4FBD10}, {0x4F, 0x4FC2D0}, {0x87, 0x4FBD10}, {0xF0, 0x5A7A70}, {0x131, 0x587900}};
constexpr mh::CallSite kCalls4DB0D0[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x8C, 0x5A7A00}, {0xB3, 0x5A7A00}, {0xCD, 0x5A7A50}, {0x108, 0x5A7630}, {0x110, 0x5A7780}, {0x12E, 0x5A7A00}, {0x16D, 0x5A7A00}, {0x19B, 0x5A7A50}, {0x1C6, 0x5A7A00}, {0x205, 0x5A7A00}, {0x233, 0x5A7A50}, {0x2A8, 0x5A79E0}, {0x2BF, 0x5A79A0}, {0x3BC, 0x461E50}, {0x3C8, 0x5A7630}, {0x3D0, 0x5A7780}, {0x3EB, 0x5A7A00}, {0x42F, 0x5A7A00}, {0x45D, 0x5A7A50}, {0x488, 0x5A7A00}, {0x4C7, 0x5A7A00}, {0x4F5, 0x5A7A50}, {0x56A, 0x5A79E0}, {0x581, 0x5A79A0}, {0x67E, 0x461E50}, {0x6AC, 0x5A77C0}, {0x6B5, 0x461E50}};
constexpr mh::CallSite kCalls4DB7A0[] = {{0xE, 0x5A77C0}, {0x17, 0x461E50}};
constexpr mh::CallSite kCalls4DB7F0[] = {{0xE5, 0x5891F0}, {0xEF, 0x5891F0}, {0x108, 0x5891F0}};
constexpr mh::JumpTable kTables4DB7F0[] = {{0xB8, 0x148, 4}};
constexpr mh::CallSite kCalls4DB950[] = {{0x0, 0x588F20}, {0x5, 0x589410}, {0xE, 0x4351F0}};
constexpr mh::CallSite kCalls4DB970[] = {{0x0, 0x5893A0}, {0x5, 0x588F20}, {0x23, 0x4351F0}};
constexpr mh::CallSite kCalls4DB9A0[] = {{0x0, 0x4DBA90}};
constexpr mh::CallSite kCalls4DB9D0[] = {{0x0, 0x4DBA90}, {0x1D, 0x4351F0}};
constexpr mh::CallSite kCalls4DBA00[] = {{0x0, 0x4DBF00}};
constexpr mh::CallSite kCalls4DBA30[] = {{0x0, 0x4DBF00}};
constexpr mh::CallSite kCalls4DBA60[] = {{0x0, 0x4DBF00}, {0x1E, 0x4351F0}};
constexpr mh::CallSite kCalls4DBA90[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x3D, 0x4456C0}, {0x7F, 0x446770}, {0xC0, 0x4DBE10}, {0xC5, 0x4DBBC0}, {0xCA, 0x5A7BC0}};
constexpr mh::CallSite kCalls4DBBC0[] = {{0x38, 0x5A75D0}, {0x40, 0x5A7780}, {0x6E, 0x5A7A50}, {0x8B, 0x5A7A00}, {0xAF, 0x5A7A50}, {0xCC, 0x5A7A00}, {0xF0, 0x5A7A50}, {0x10D, 0x5A7A00}, {0x131, 0x5A7A50}, {0x14E, 0x5A7A00}, {0x17B, 0x5A79A0}, {0x18B, 0x5A79E0}, {0x214, 0x5A85F0}, {0x21A, 0x5A9290}, {0x223, 0x461E50}};
constexpr mh::CallSite kCalls4DBE10[] = {{0x3, 0x5A7B90}, {0x92, 0x5A8200}, {0xA1, 0x5A8060}, {0xB5, 0x5A7D70}, {0xBF, 0x5A8DE0}, {0xC9, 0x5A8E00}};
constexpr mh::JumpTable kTables4DBE10[] = {{0x2B, 0xD4, 4}};
constexpr mh::CallSite kCalls4DBF00[] = {{0xE, 0x4456C0}, {0x4A, 0x4B7D40}, {0x4F, 0x4DBF70}, {0x54, 0x5A7BC0}};
constexpr mh::CallSite kCalls4DBF70[] = {{0x30, 0x5A77C0}, {0x39, 0x461E50}, {0x45, 0x5A75F0}, {0x4C, 0x5A7780}, {0x82, 0x5A7A00}, {0x9B, 0x5A7A50}, {0xD7, 0x5A7A00}, {0xF0, 0x5A7A50}, {0x178, 0x5A84A0}, {0x181, 0x5A9310}, {0x18A, 0x461E50}};
constexpr mh::Imm kImms4DC110[] = {{0xF, 0x4DC140}, {0x17, 0x4BDC10}};
constexpr mh::CallSite kCalls4DC140[] = {{0x49, 0x435180}, {0xEF, 0x587900}};
constexpr mh::CallSite kCalls4DC280[] = {{0x34, 0x5B93D2}, {0x44, 0x5B93D2}, {0x54, 0x5B93D2}, {0x6F, 0x4DD0B0}, {0x81, 0x4DD0B0}, {0x92, 0x4DD0B0}, {0xA2, 0x4DD4B0}, {0xB4, 0x4DD4B0}, {0xC5, 0x4DD4B0}, {0xCD, 0x4DC5D0}};
constexpr mh::CallSite kCalls4DC360[] = {{0x2B, 0x4FC0E0}, {0x30, 0x4FBD10}, {0x50, 0x4FC2D0}, {0x88, 0x4FBD10}, {0xF1, 0x5A7A70}, {0x102, 0x5B93D2}, {0x112, 0x5B93D2}, {0x122, 0x5B93D2}};
constexpr mh::CallSite kCalls4DC4F0[] = {{0x21, 0x452F70}};
constexpr mh::CallSite kCalls4DC590[] = {{0x36, 0x4351F0}};
constexpr mh::CallSite kCalls4DC5D0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x82, 0x5A7A00}, {0xA1, 0x5A7A50}, {0x117, 0x5B93D2}, {0x136, 0x5A7610}, {0x13E, 0x5A7780}, {0x160, 0x5A7A00}, {0x18E, 0x5A7A50}, {0x1BC, 0x5A7A00}, {0x1EA, 0x5A7A50}, {0x2EE, 0x461E50}, {0x2FA, 0x5A7610}, {0x302, 0x5A7780}, {0x323, 0x5A7A00}, {0x351, 0x5A7A50}, {0x37F, 0x5A7A00}, {0x3AD, 0x5A7A50}, {0x4B4, 0x461E50}, {0x4C0, 0x5A7610}, {0x4C8, 0x5A7780}, {0x559, 0x5A7A00}, {0x587, 0x5A7A50}, {0x5B5, 0x5A7A00}, {0x5E3, 0x5A7A50}, {0x676, 0x5A7A00}, {0x6A4, 0x5A7A50}, {0x6D2, 0x5A7A00}, {0x700, 0x5A7A50}, {0x79F, 0x461E50}, {0x7AB, 0x5A7610}, {0x7B3, 0x5A7780}, {0x847, 0x5A7A00}, {0x875, 0x5A7A50}, {0x8A3, 0x5A7A00}, {0x8D1, 0x5A7A50}, {0x962, 0x5A7A00}, {0x990, 0x5A7A50}, {0x9BE, 0x5A7A00}, {0x9EC, 0x5A7A50}, {0xA8B, 0x461E50}, {0xABE, 0x5A77C0}, {0xAC7, 0x461E50}};
constexpr mh::CallSite kCalls4DD0B0[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x53, 0x5A7A00}, {0x72, 0x5A7A50}, {0xC5, 0x5A7A00}, {0xE6, 0x5A7A50}, {0x100, 0x5A7A00}, {0x119, 0x5A7A00}, {0x141, 0x5A7A50}, {0x163, 0x5B93D2}, {0x18E, 0x5A7650}, {0x196, 0x5A7780}, {0x19E, 0x5B93D2}, {0x1BF, 0x5A7A00}, {0x1E1, 0x5A7A50}, {0x202, 0x5A7A00}, {0x234, 0x5A7A00}, {0x25B, 0x5A7A50}, {0x292, 0x5A7A00}, {0x2B3, 0x5A7A50}, {0x2E3, 0x5A7A00}, {0x2EA, 0x5B93D2}, {0x32E, 0x5A7A00}, {0x364, 0x5A7A50}, {0x3BE, 0x461E50}, {0x3DF, 0x5A77C0}, {0x3E8, 0x461E50}};
constexpr mh::CallSite kCalls4DD4B0[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x53, 0x5A7A00}, {0x72, 0x5A7A50}, {0xC5, 0x5A7A00}, {0xE6, 0x5A7A50}, {0x100, 0x5A7A00}, {0x119, 0x5A7A00}, {0x141, 0x5A7A50}, {0x163, 0x5B93D2}, {0x18E, 0x5A7650}, {0x196, 0x5A7780}, {0x19E, 0x5B93D2}, {0x1BF, 0x5A7A00}, {0x1E1, 0x5A7A50}, {0x202, 0x5A7A00}, {0x234, 0x5A7A00}, {0x25B, 0x5A7A50}, {0x292, 0x5A7A00}, {0x2B3, 0x5A7A50}, {0x2E3, 0x5A7A00}, {0x2EA, 0x5B93D2}, {0x32E, 0x5A7A00}, {0x364, 0x5A7A50}, {0x3BE, 0x461E50}, {0x3DF, 0x5A77C0}, {0x3E8, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Burn_Task", 0x4DA220, 0x63, kCalls4DA220, MH_N(kCalls4DA220), kImms4DA220, MH_N(kImms4DA220), nullptr, 0, reinterpret_cast<const void*>(&::Burn_Task)},
    {"Burn_Start", 0x4DA290, 0xDF, kCalls4DA290, MH_N(kCalls4DA290), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Burn_Start)},
    {"Burn_WaitFlag", 0x4DA370, 0x32, kCalls4DA370, MH_N(kCalls4DA370), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Burn_WaitFlag)},
    {"SpellFx_Countdown", 0x4DA3B0, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::SpellFx_Countdown)},
    {"BurnFlame_Task", 0x4DA3D0, 0x33, kCalls4DA3D0, MH_N(kCalls4DA3D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BurnFlame_Task)},
    {"BurnFlame_Start", 0x4DA410, 0x86, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BurnFlame_Start)},
    {"BurnFlame_Rise", 0x4DA4A0, 0x3F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BurnFlame_Rise)},
    {"BurnFlame_Fade", 0x4DA4E0, 0x56, kCalls4DA4E0, MH_N(kCalls4DA4E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BurnFlame_Fade)},
    {"BurnFlame_Draw", 0x4DA540, 0x366, kCalls4DA540, MH_N(kCalls4DA540), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BurnFlame_Draw)},
    {"Burn_DrawRing", 0x4DA8B0, 0x245, kCalls4DA8B0, MH_N(kCalls4DA8B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Burn_DrawRing)},
    {"Burn_DrawDisc", 0x4DAB00, 0x1CE, kCalls4DAB00, MH_N(kCalls4DAB00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Burn_DrawDisc)},
    {"WhelpBreath_Task", 0x4DACD0, 0x2E, nullptr, 0, kImms4DACD0, MH_N(kImms4DACD0), nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreath_Task)},
    {"WhelpBreath_Start", 0x4DAD00, 0x176, kCalls4DAD00, MH_N(kCalls4DAD00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreath_Start)},
    {"WhelpBreath_WaitBeam", 0x4DAE80, 0x7B, kCalls4DAE80, MH_N(kCalls4DAE80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreath_WaitBeam)},
    {"WhelpBreathChild_Task", 0x4DAF00, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathChild_Task)},
    {"WhelpBreathBeam_Run", 0x4DAF20, 0x30, kCalls4DAF20, MH_N(kCalls4DAF20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathBeam_Run)},
    {"WhelpBreathBeam_Aim", 0x4DAF50, 0x13B, kCalls4DAF50, MH_N(kCalls4DAF50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathBeam_Aim)},
    {"WhelpBreathBeam_Grow", 0x4DB090, 0x39, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathBeam_Grow)},
    {"WhelpBreathBeam_Draw", 0x4DB0D0, 0x6C2, kCalls4DB0D0, MH_N(kCalls4DB0D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathBeam_Draw)},
    {"WhelpBreathSprite_Run", 0x4DB7A0, 0x46, kCalls4DB7A0, MH_N(kCalls4DB7A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathSprite_Run)},
    {"WhelpBreathSprite_Start", 0x4DB7F0, 0x158, kCalls4DB7F0, MH_N(kCalls4DB7F0), nullptr, 0, kTables4DB7F0, MH_N(kTables4DB7F0), reinterpret_cast<const void*>(&::WhelpBreathSprite_Start)},
    {"WhelpBreathSprite_Play", 0x4DB950, 0x14, kCalls4DB950, MH_N(kCalls4DB950), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathSprite_Play)},
    {"WhelpBreathSprite_Hold", 0x4DB970, 0x29, kCalls4DB970, MH_N(kCalls4DB970), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathSprite_Hold)},
    {"WhelpBreathFlames_Grow", 0x4DB9A0, 0x21, kCalls4DB9A0, MH_N(kCalls4DB9A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathFlames_Grow)},
    {"WhelpBreathFlames_End", 0x4DB9D0, 0x23, kCalls4DB9D0, MH_N(kCalls4DB9D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathFlames_End)},
    {"WhelpBreathGlow_Grow", 0x4DBA00, 0x21, kCalls4DBA00, MH_N(kCalls4DBA00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathGlow_Grow)},
    {"WhelpBreathGlow_Hold", 0x4DBA30, 0x22, kCalls4DBA30, MH_N(kCalls4DBA30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathGlow_Hold)},
    {"WhelpBreathGlow_End", 0x4DBA60, 0x24, kCalls4DBA60, MH_N(kCalls4DBA60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathGlow_End)},
    {"WhelpBreathFlames_Draw", 0x4DBA90, 0x12F, kCalls4DBA90, MH_N(kCalls4DBA90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathFlames_Draw)},
    {"WhelpBreathFlames_DrawColumn", 0x4DBBC0, 0x248, kCalls4DBBC0, MH_N(kCalls4DBBC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathFlames_DrawColumn)},
    {"WhelpBreathFlames_PushMatrix", 0x4DBE10, 0xE4, kCalls4DBE10, MH_N(kCalls4DBE10), nullptr, 0, kTables4DBE10, MH_N(kTables4DBE10), reinterpret_cast<const void*>(&::WhelpBreathFlames_PushMatrix)},
    {"WhelpBreathGlow_Draw", 0x4DBF00, 0x6B, kCalls4DBF00, MH_N(kCalls4DBF00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathGlow_Draw)},
    {"WhelpBreathGlow_DrawFan", 0x4DBF70, 0x19A, kCalls4DBF70, MH_N(kCalls4DBF70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WhelpBreathGlow_DrawFan)},
    {"DragonBreath_Task", 0x4DC110, 0x26, nullptr, 0, kImms4DC110, MH_N(kImms4DC110), nullptr, 0, reinterpret_cast<const void*>(&::DragonBreath_Task)},
    {"DragonBreath_Start", 0x4DC140, 0x112, kCalls4DC140, MH_N(kCalls4DC140), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreath_Start)},
    {"DragonBreathBeam_Task", 0x4DC260, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Task)},
    {"DragonBreathBeam_Run", 0x4DC280, 0xD3, kCalls4DC280, MH_N(kCalls4DC280), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Run)},
    {"DragonBreathBeam_Aim", 0x4DC360, 0x169, kCalls4DC360, MH_N(kCalls4DC360), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Aim)},
    {"DragonBreathBeam_Widen", 0x4DC4D0, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Widen)},
    {"DragonBreathBeam_Hit", 0x4DC4F0, 0x32, kCalls4DC4F0, MH_N(kCalls4DC4F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Hit)},
    {"DragonBreathBeam_Brighten", 0x4DC530, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Brighten)},
    {"DragonBreathBeam_Hold", 0x4DC550, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Hold)},
    {"DragonBreathBeam_Dim", 0x4DC570, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Dim)},
    {"DragonBreathBeam_End", 0x4DC590, 0x3C, kCalls4DC590, MH_N(kCalls4DC590), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_End)},
    {"DragonBreathBeam_Draw", 0x4DC5D0, 0xAD5, kCalls4DC5D0, MH_N(kCalls4DC5D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_Draw)},
    {"DragonBreathBeam_DrawLinesA", 0x4DD0B0, 0x3F5, kCalls4DD0B0, MH_N(kCalls4DD0B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_DrawLinesA)},
    {"DragonBreathBeam_DrawLinesB", 0x4DD4B0, 0x3F5, kCalls4DD4B0, MH_N(kCalls4DD4B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::DragonBreathBeam_DrawLinesB)},
};
#undef MH_N

enum : unsigned {
    kBurn_Task, kBurn_Start, kBurn_WaitFlag, kSpellFx_Countdown, kBurnFlame_Task, kBurnFlame_Start, kBurnFlame_Rise, kBurnFlame_Fade, kBurnFlame_Draw, kBurn_DrawRing, kBurn_DrawDisc, kWhelpBreath_Task, kWhelpBreath_Start, kWhelpBreath_WaitBeam, kWhelpBreathChild_Task, kWhelpBreathBeam_Run, kWhelpBreathBeam_Aim, kWhelpBreathBeam_Grow, kWhelpBreathBeam_Draw, kWhelpBreathSprite_Run, kWhelpBreathSprite_Start, kWhelpBreathSprite_Play, kWhelpBreathSprite_Hold, kWhelpBreathFlames_Grow, kWhelpBreathFlames_End, kWhelpBreathGlow_Grow, kWhelpBreathGlow_Hold, kWhelpBreathGlow_End, kWhelpBreathFlames_Draw, kWhelpBreathFlames_DrawColumn, kWhelpBreathFlames_PushMatrix, kWhelpBreathGlow_Draw, kWhelpBreathGlow_DrawFan, kDragonBreath_Task, kDragonBreath_Start, kDragonBreathBeam_Task, kDragonBreathBeam_Run, kDragonBreathBeam_Aim, kDragonBreathBeam_Widen, kDragonBreathBeam_Hit, kDragonBreathBeam_Brighten, kDragonBreathBeam_Hold, kDragonBreathBeam_Dim, kDragonBreathBeam_End, kDragonBreathBeam_Draw, kDragonBreathBeam_DrawLinesA, kDragonBreathBeam_DrawLinesB, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the callees the standard set lacks ---------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define S27_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S27_RAW(address) #address, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;

// The fuzz's own packet buffer: Gfx_PacketNext points into it every round.
// The commits advance it by their size as the real ones do (kept inside:
// a draw writes up to 0x54 past it), and log the primitive they commit.
constexpr unsigned kPacketBytes = 0x400;
alignas(16) unsigned char g_packets[kPacketBytes];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 7) * 0x40; }
void Advance(std::uint32_t size) {
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_packets || p + 0x100 > g_packets + kPacketBytes) p = g_packets + (size & 0x3C);
    Gfx_PacketNext = p;
}
void LogPrimitive(std::uint32_t size) {
    const unsigned char* const p = Gfx_PacketNext;
    if (p >= g_packets && p + 0x60 <= g_packets + kPacketBytes) mh::NoteBytes(p, (size & 0xFF) < 0x60 ? size & 0xFF : 0x60);
}
std::uint32_t Committed(const std::uint32_t* a, std::uint32_t answer) {
    LogPrimitive(a[1]);
    Advance(a[1]);
    return answer;
}
std::uint32_t Linked(const std::uint32_t* a, std::uint32_t answer) {
    LogPrimitive(a[3]);
    Advance(a[3]);
    return answer;
}
// The GTE stand-ins of WhelpBreathFlames_PushMatrix: a result from the inputs,
// written where the real callee writes (docs/magic_s22.md section 5).
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
// WhelpBreathSprite_Run's phases: the sprite bank they run under, and the
// slot and phase bytes a handler's recorder would log.
std::uint32_t NoteBank(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(static_cast<std::uint32_t>(Long(mh::Mem(raw::kSpriteBank))), Key(Sprite_Current),
             Sprite_Current[1] | static_cast<std::uint32_t>(Sprite_Current[2]) << 8);
    return answer;
}

const mh::Callee kCallees[] = {
    {S27_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    {S27_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    // Gte_RotTrans: the original pushes three (the flag unread).
    {S27_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S27_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S27_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S27_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S27_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    {S27_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0},
    {S27_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S27_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &Linked},
    {S27_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &Committed},
    {S27_OURS(Gpu_SetPolyGT4), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S27_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S27_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    // The projections: the vertices by what they hold, the packet pointers as
    // they are, the depth and flag pointers (the caller's frame) masked off.
    {S27_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S27_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S27_OURS(Gte_PrimDepths4_14), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Gte_PrimDepths4_10), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {S27_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0},
    {S27_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction.
    {S27_RAW(raw::kTurnOffset), 1, {kAll}, kG, 0, 0},
    // This group's own, called directly by its others.
    {S27_RAW(0x4DA540), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DA8B0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DAB00), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DB0D0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DBA90), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DBBC0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DBE10), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DBF00), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DBF70), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DC5D0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S27_RAW(0x4DD0B0), 1, {kU8}, kG, 0, 0},
    {S27_RAW(0x4DD4B0), 1, {kU8}, kG, 0, 0},
    // WhelpBreathSprite_Phases' eight entries: each logs the bank it ran under.
    {S27_RAW(0x4DB7F0), 0, {}, kG, 0, 0, {}, &NoteBank},
    {S27_RAW(0x4DB950), 0, {}, kG, 0, 0, {}, &NoteBank},
    {S27_RAW(0x4DB970), 0, {}, kG, 0, 0, {}, &NoteBank},
    {S27_RAW(0x4DB9A0), 0, {}, kG, 0, 0, {}, &NoteBank},
    {S27_RAW(0x4DB9D0), 0, {}, kG, 0, 0, {}, &NoteBank},
    {S27_RAW(0x4DBA00), 0, {}, kG, 0, 0, {}, &NoteBank},
    {S27_RAW(0x4DBA30), 0, {}, kG, 0, 0, {}, &NoteBank},
    {S27_RAW(0x4DBA60), 0, {}, kG, 0, 0, {}, &NoteBank},
};

// The .data handler tables the dispatchers read in place (symbols.toml:
// BurnFlame_Phases, WhelpBreathChild_Kinds, WhelpBreathBeam_Phases,
// WhelpBreathSprite_Phases, DragonBreathChild_Kinds, DragonBreathBeam_Phases).
const mh::DataTable kTables[] = {
    {0x65BAB8, 3}, {0x65BAC4, 2}, {0x65BACC, 5}, {0x65BAE0, 8}, {0x65BB28, 1}, {0x65BB2C, 7},
};

constexpr std::uint32_t kScratch = 0x903850, kVertices = 0x9037A0;
const mh::Region kRegions[] = {
    {kScratch, 0x10},
    {kVertices, 0x20},
    {0x7E0670, 4},                                   // Gfx_PacketNext
    {Key(g_packets), sizeof g_packets},
    {Key(Gfx_ClutStrip + 0x1A00), 0x200},            // row 26, the three Start functions'
    {Key(Gfx_ClutStripSource + 0x1A00), 0x200},
    {raw::kSpriteBank, 4},
    {Key(WhelpBreathFlames_Ages), 8},
    {Key(WhelpBreath_FlameOffsets), 0x28},           // MAGIC120's .data up to its handler table
};

unsigned char* Sc() { return Sprite_Current; }
unsigned g_k;   // the function being fuzzed, for settle

// The group's cells a recorder may move: Gfx_PacketNext, a scratch word, a
// vertex word, the task's fields the harness leaves alone, a flame's age.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFF;
    const auto word = static_cast<std::uint16_t>(h >> 16);
    switch ((h >> 8) % 6) {
    case 0: mh::SetPointer(0x7E0670, PacketAt(v)); break;
    case 1: SetWord(mh::Mem(kScratch + (v % 8) * 2), word); break;
    case 2: SetWord(mh::Mem(kVertices + (v % 16) * 2), word); break;
    case 3: {
        static const unsigned kFields[] = {0x10, 0x14, 0x15, 0x20, 0x2E, 0x2F, 0x31, 0x3E, 0x5D, 0x5E, 0x5F};
        Sc()[kFields[v % 11]] = static_cast<unsigned char>(word);
        break;
    }
    case 4: WhelpBreathFlames_Ages[v % 6] = static_cast<unsigned char>(word); break;
    default: break;
    }
}

// After every disturbance: WhelpBreathBeam_Draw divides by +0xB (the
// original faults at 0, ours aborts), the beams walk +0xA steps, and
// WhelpBreathSprite_Run indexes its table after two calls.
void Settle() {
    unsigned char* const s = Sc();
    if (g_k == kWhelpBreathBeam_Draw) {
        if (s[0xB] == 0) s[0xB] = 1;
        if (s[0xA] > 7) s[0xA] &= 7;
    } else if (g_k == kDragonBreathBeam_Draw) {
        if (s[0xA] > 7) s[0xA] &= 7;
    } else if (g_k == kWhelpBreathSprite_Run) {
        // it reads +2 after its draw mode's two calls: past its eight entries
        // the original calls through the next table's bytes, ours aborts
        if (s[2] >= 8) s[2] &= 7;
    }
}

// --- the seed ------------------------------------------------------------------

void Seed(unsigned k) {
    g_k = k;
    unsigned char* const sc = Sc();
    mh::SetPointer(0x7E0670, PacketAt(mh::Next()));
    auto near = [](unsigned at, unsigned span) { return static_cast<unsigned char>(at + mh::Next() % span); };
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kBurn_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 5); if (mh::Half()) sc[0] = 0; break;
    case kBurnFlame_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 3); if (mh::Half()) sc[0] = 0; break;
    case kWhelpBreath_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kWhelpBreathChild_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kWhelpBreathBeam_Run:
        sc[2] = static_cast<unsigned char>(mh::Next() % 5);
        if (mh::Half()) mh::Pointer(mh::at::kOwner)[0xB] = 0xFF;
        break;
    case kWhelpBreathSprite_Run: sc[2] = static_cast<unsigned char>(mh::Next() % 8); break;
    case kDragonBreath_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kDragonBreathBeam_Task: sc[1] = 0; break;
    case kDragonBreathBeam_Run:
        sc[2] = static_cast<unsigned char>(mh::Next() % 7);
        if (mh::Half()) sc[0] = 0;
        if (mh::Half()) Frame_Counter &= ~7u;
        break;
    // the counters: either side of their ends
    case kBurn_WaitFlag: if (mh::Often()) sc[9] = near(0xE, 3); break;
    case kSpellFx_Countdown: case kBurnFlame_Start: case kBurnFlame_Fade: case kWhelpBreathBeam_Aim:
    case kWhelpBreathSprite_Hold: case kWhelpBreathGlow_End: case kDragonBreathBeam_Aim:
        if (mh::Often()) sc[9] = near(1, 2);
        break;
    case kDragonBreathBeam_End:
        if (mh::Often()) sc[9] = near(1, 2);
        if (mh::Half()) sc[0xB] = 0;
        break;
    case kBurnFlame_Rise: if (mh::Often()) sc[9] = near(0x11, 3); break;
    case kWhelpBreath_WaitBeam: if (mh::Half()) sc[0xB] = 1; break;
    case kWhelpBreathBeam_Grow: if (mh::Often()) sc[9] = near(6, 3); break;
    case kWhelpBreathSprite_Start: sc[0xB] = static_cast<unsigned char>(mh::Next() % 5); break;
    case kWhelpBreathFlames_Grow: if (mh::Often()) sc[9] = near(0x5E, 3); break;
    case kWhelpBreathFlames_End: if (mh::Often()) sc[9] = near(0x6E, 3); break;
    case kWhelpBreathGlow_Grow: if (mh::Often()) sc[9] = near(0xE, 3); break;
    case kWhelpBreathGlow_Hold: if (mh::Often()) sc[0xA] = near(1, 2); break;
    case kDragonBreathBeam_Widen: if (mh::Often()) sc[0xA] = static_cast<unsigned char>(0x10 + (mh::Next() % 3) * 8); break;
    case kDragonBreathBeam_Hit: if (mh::Often()) sc[0xB] = near(6, 3); break;
    case kDragonBreathBeam_Brighten: if (mh::Often()) sc[4] = static_cast<unsigned char>(0xC + (mh::Next() % 3) * 2); break;
    case kDragonBreathBeam_Hold: if (mh::Often()) SetLong(sc + 0xC, static_cast<std::int32_t>(0x2C + mh::Next() % 3)); break;
    case kDragonBreathBeam_Dim: if (mh::Often()) sc[4] = static_cast<unsigned char>((mh::Next() % 3) * 2); break;
    // the draws: their loops' bounds, the ages either side of 0x10
    case kWhelpBreathBeam_Draw:
        sc[0xA] = static_cast<unsigned char>(mh::Next() % 6);
        sc[0xB] = static_cast<unsigned char>(mh::Half() ? 1 + mh::Next() % 3 : 1 + mh::Next() % 0xFF);
        break;
    case kDragonBreathBeam_Draw: sc[0xA] = static_cast<unsigned char>(mh::Next() % 7); break;
    case kWhelpBreathFlames_Draw:
        sc[9] = static_cast<unsigned char>(mh::Next() % 0x14);
        sc[2] = static_cast<unsigned char>(mh::Half() ? 3 : 4);
        break;
    case kWhelpBreathFlames_DrawColumn:
        sc[9] = static_cast<unsigned char>(mh::Next() % 0x14);
        for (unsigned j = 0; j < 6; ++j)
            if (mh::Often()) WhelpBreathFlames_Ages[j] = static_cast<unsigned char>(mh::Next() % 0x11);
        break;
    case kWhelpBreathFlames_PushMatrix: case kWhelpBreathGlow_DrawFan:
        if (mh::Often()) sc[8] = static_cast<unsigned char>(mh::Next() % 6);
        break;
    default: break;
    }
}

// DragonBreathBeam_DrawLinesA / B take the line's length in their argument's
// low byte (garbage above, as the original's al): mostly short.
void Args(unsigned k, std::uint32_t* a) {
    if (k == kDragonBreathBeam_DrawLinesA || k == kDragonBreathBeam_DrawLinesB)
        if (mh::Often()) a[0] = (a[0] & ~0xFFu) | (mh::Next() % 0x10);
}

}  // namespace

void SelfTest() {
    mh::Group group = {
        "magic_s27", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000,
    };
    group.settle = &Settle;
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s27
