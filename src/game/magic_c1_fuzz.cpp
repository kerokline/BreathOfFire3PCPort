// BOF3X_SHADOW=magic_c1: group C1's six overlays (MAGIC010, 080, 113, 145,
// 146, 213) through the spell round's shared harness (magic_harness.h), once
// at start-up. docs/magic_c1.md section 9.
//
// The clone table is tools/magic_rows.py --unit MAGIC010 / 080 / 113 / 145 /
// 146 / 213 --clones (2026-09-26; capstone, every jump internal, no jump
// table, nothing REFUSED), names given. What this group needs beyond the
// standard set is said through the harness's own fields (no harness edit):
//
//   - the draws: `deref` on the GTE projections (the vertices in
//     Prim_VertexScratch, rewritten every step), and an `effect` on
//     Gfx_CommitPrim / MapView_LinkPrimAt that logs the fuzz's packet buffer
//     and advances Gfx_PacketNext by the size as the real ones do, so every
//     primitive of a draw is compared, not only the last;
//   - the turn 0x446770 writes the task's +0xC / +0x10 (an `effect`), which
//     the puffs read again after it;
//   - the two pool allocs answer al (`ret_mask` 0xFF); their recorders answer
//     0..0x3F or 0xFF, as the real ones;
//   - the two pool walks make each record Sprite_Current and its +0x80 the
//     owner, which the recorders write through: the seed keeps every
//     record's +0x80 a real slot or record.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_c1.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_c1 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

// tools/magic_rows.py --clones, 2026-09-26, names given.
// MAGIC010
constexpr mh::Imm kImms49DEF0[] = {{0xF, 0x49DF30}, {0x17, 0x4D9FD0}, {0x22, 0x49DF70}, {0x2A, 0x43F460}};
constexpr mh::CallSite kCalls49DF30[] = {{0x6, 0x454DC0}, {0x1A, 0x454CC0}};
constexpr mh::CallSite kCalls49DF70[] = {{0x68, 0x454DC0}, {0x74, 0x4FBDB0}};
// MAGIC080
constexpr mh::Imm kImms4FC330[] = {{0xF, 0x4FC360}, {0x17, 0x4FC3C0}};
constexpr mh::CallSite kCalls4FC360[] = {{0x29, 0x5720C0}, {0x3B, 0x5B93D2}};
constexpr mh::CallSite kCalls4FC3C0[] = {{0x0, 0x4FBD10}, {0x1B, 0x4FC540}, {0x22, 0x4FC420}, {0x48, 0x4530D0}, {0x57, 0x4351F0}};
constexpr mh::CallSite kCalls4FC420[] = {{0x17, 0x5A7760}, {0xD6, 0x5A79E0}, {0xFF, 0x461E50}};
constexpr mh::CallSite kCalls4FC540[] = {{0x13, 0x5A75D0}, {0xE5, 0x5A79A0}, {0xF5, 0x5A79E0}, {0x138, 0x461E50}};
// MAGIC113
constexpr mh::CallSite kCalls4D67F0[] = {{0x5D, 0x4B7D40}, {0x62, 0x4D6C90}, {0x67, 0x5A7BC0}};
constexpr mh::Imm kImms4D67F0[] = {{0xF, 0x4D6860}, {0x17, 0x4D6900}, {0x22, 0x4D6A30}, {0x2A, 0x4D6AC0}, {0x32, 0x4D6B70}, {0x3A, 0x4D6C60}, {0x42, 0x4D6C80}, {0x4A, 0x4D7BD0}};
constexpr mh::CallSite kCalls4D6860[] = {{0x1E, 0x4530D0}, {0x2D, 0x4351F0}};
constexpr mh::CallSite kCalls4D6900[] = {{0x22, 0x435180}, {0x9B, 0x435180}, {0x114, 0x587740}};
constexpr mh::CallSite kCalls4D6A30[] = {{0x10, 0x435180}};
constexpr mh::CallSite kCalls4D6AC0[] = {{0x14, 0x435180}, {0x91, 0x587740}};
constexpr mh::CallSite kCalls4D6B70[] = {{0x7, 0x4DF820}, {0xE, 0x4DF820}, {0x30, 0x435180}};
constexpr mh::CallSite kCalls4D6C90[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x32, 0x5A7A00}, {0x48, 0x5A7A50}, {0x6C, 0x5A75F0}, {0x73, 0x5A7780}, {0xA3, 0x5A7A00}, {0xB9, 0x5A7A50}, {0x108, 0x5A84A0}, {0x10E, 0x5A9310}, {0x153, 0x461E50}, {0x179, 0x5A77C0}, {0x182, 0x461E50}};
constexpr mh::CallSite kCalls4D6E50[] = {{0x1D, 0x4B7D40}, {0x3A, 0x4D6ED0}, {0x3F, 0x5A7BC0}};
constexpr mh::CallSite kCalls4D6ED0[] = {{0x14, 0x5A77C0}, {0x1D, 0x461E50}, {0x24, 0x5A7A00}, {0x3B, 0x5A7A50}, {0x80, 0x5A7650}, {0x96, 0x5A7780}, {0xB1, 0x5A8250}, {0xBA, 0x5A9110}, {0xC0, 0x5A7A00}, {0xD6, 0x5A7A50}, {0x107, 0x5A8250}, {0x110, 0x5A9110}, {0x166, 0x461E50}, {0x18F, 0x5A77C0}, {0x198, 0x461E50}};
constexpr mh::CallSite kCalls4D7080[] = {{0x1D, 0x4B7D40}, {0x22, 0x4D7130}, {0x27, 0x5A7BC0}};
constexpr mh::CallSite kCalls4D7100[] = {{0x20, 0x4351F0}};
constexpr mh::CallSite kCalls4D7130[] = {{0x15, 0x5A77C0}, {0x1E, 0x461E50}, {0x56, 0x5A7650}, {0x6C, 0x5A7780}, {0x88, 0x5A7A00}, {0xB1, 0x5A7A50}, {0xE0, 0x5A8250}, {0xE9, 0x5A9110}, {0x102, 0x5A7A00}, {0x12B, 0x5A7A50}, {0x1A1, 0x5A8250}, {0x1AA, 0x5A9110}, {0x200, 0x461E50}, {0x229, 0x5A77C0}, {0x232, 0x461E50}};
constexpr mh::CallSite kCalls4D7380[] = {{0x1D, 0x4B7D40}, {0x22, 0x4D7440}, {0x27, 0x5A7BC0}};
constexpr mh::CallSite kCalls4D73E0[] = {{0x10, 0x587740}};
constexpr mh::CallSite kCalls4D7410[] = {{0x20, 0x4351F0}};
constexpr mh::CallSite kCalls4D7440[] = {{0x2F, 0x5A7A50}, {0x48, 0x5A7A00}, {0x7D, 0x5A7A50}, {0x95, 0x5A7A00}, {0xE5, 0x5B93D2}, {0x153, 0x5A7A50}, {0x16B, 0x5A7A00}, {0x1A0, 0x5A7A50}, {0x1B8, 0x5A7A00}, {0x204, 0x5A77C0}, {0x20F, 0x572FA0}, {0x21B, 0x5A7610}, {0x222, 0x5A7780}, {0x2CD, 0x5A85F0}, {0x2D3, 0x5A9350}, {0x2DE, 0x572FA0}, {0x306, 0x5A77C0}, {0x30F, 0x461E50}};
constexpr mh::CallSite kCalls4D7760[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x56, 0x5A77C0}, {0x5F, 0x461E50}};
constexpr mh::CallSite kCalls4D77D0[] = {{0xE0, 0x587740}, {0xF2, 0x5891F0}};
constexpr mh::CallSite kCalls4D78D0[] = {{0x0, 0x589410}, {0x29, 0x4351F0}, {0x43, 0x5891F0}, {0x4B, 0x588F20}, {0x67, 0x587740}, {0x75, 0x587740}, {0x7E, 0x588F20}};
// MAGIC145
constexpr mh::Imm kImms4E9A70[] = {{0xF, 0x4E9AA0}, {0x17, 0x49DA50}, {0x22, 0x43F460}};
constexpr mh::CallSite kCalls4E9AA0[] = {{0x21, 0x435180}, {0xAF, 0x587900}, {0xBC, 0x452F70}};
constexpr mh::CallSite kCalls4E9B90[] = {{0x23, 0x4E9C90}};
constexpr mh::CallSite kCalls4E9BC0[] = {{0x61, 0x446770}, {0xA6, 0x4FBD10}};
constexpr mh::CallSite kCalls4E9C90[] = {{0x10, 0x5A77C0}, {0x26, 0x572FA0}, {0x54, 0x5A75D0}, {0x5C, 0x5A7780}, {0x66, 0x5A7A50}, {0x94, 0x5A7A00}, {0xC2, 0x5A7A50}, {0xF0, 0x5A7A00}, {0x121, 0x5A7A50}, {0x14F, 0x5A7A00}, {0x17D, 0x5A7A50}, {0x1AB, 0x5A7A00}, {0x1E2, 0x5A79A0}, {0x1F2, 0x5A79E0}, {0x246, 0x572FA0}};
// MAGIC146
constexpr mh::CallSite kCalls4E9EF0[] = {{0x5B, 0x4EA260}};
constexpr mh::Imm kImms4E9EF0[] = {{0x16, 0x4E9F70}, {0x1E, 0x49DA50}, {0x26, 0x43F460}};
constexpr mh::CallSite kCalls4E9F70[] = {{0x4D, 0x4456C0}, {0x5D, 0x435180}, {0xCC, 0x4456C0}, {0xDC, 0x435180}};
constexpr mh::CallSite kCalls4EA130[] = {{0x62, 0x587900}, {0xB2, 0x4EA3F0}, {0x116, 0x452F70}};
constexpr mh::CallSite kCalls4EA280[] = {{0x23, 0x4E9C90}};
constexpr mh::CallSite kCalls4EA2B0[] = {{0x92, 0x446770}, {0xD6, 0x4FBD10}};
constexpr mh::CallSite kCalls4EA3B0[] = {{0x2E, 0x4F6290}};
// MAGIC213
constexpr mh::CallSite kCalls4F4A60[] = {{0x53, 0x4F4EA0}};
constexpr mh::Imm kImms4F4A60[] = {{0x16, 0x4F4AE0}, {0x1E, 0x4E5200}};
constexpr mh::CallSite kCalls4F4AE0[] = {{0x43, 0x4456C0}, {0x53, 0x435180}, {0xB9, 0x4456C0}, {0xC9, 0x435180}, {0x14A, 0x587900}};
constexpr mh::CallSite kCalls4F4C80[] = {{0xA1, 0x4F5050}, {0xFB, 0x454DC0}, {0x109, 0x454CC0}};
constexpr mh::CallSite kCalls4F4E10[] = {{0x66, 0x454D60}, {0x75, 0x4530D0}};
constexpr mh::CallSite kCalls4F4EC0[] = {{0x2D, 0x588F20}};
constexpr mh::CallSite kCalls4F4F00[] = {{0x34, 0x5A7A00}, {0x57, 0x5A7A50}, {0x114, 0x5891F0}};
constexpr mh::CallSite kCalls4F5030[] = {{0x0, 0x589410}, {0x11, 0x4F6290}};

#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define C1_P(fn) reinterpret_cast<const void*>(&::fn)
const mh::Clone kClones[] = {
    {"WhiteFlag_Task", 0x49DEF0, 0x36, nullptr, 0, kImms49DEF0, MH_N(kImms49DEF0), nullptr, 0, C1_P(WhiteFlag_Task)},
    {"WhiteFlag_TintSource", 0x49DF30, 0x3D, kCalls49DF30, MH_N(kCalls49DF30), nullptr, 0, nullptr, 0, C1_P(WhiteFlag_TintSource)},
    {"WhiteFlag_Untint", 0x49DF70, 0x85, kCalls49DF70, MH_N(kCalls49DF70), nullptr, 0, nullptr, 0, C1_P(WhiteFlag_Untint)},
    {"Magic080_Task", 0x4FC330, 0x26, nullptr, 0, kImms4FC330, MH_N(kImms4FC330), nullptr, 0, C1_P(Magic080_Task)},
    {"Magic080_Start", 0x4FC360, 0x5D, kCalls4FC360, MH_N(kCalls4FC360), nullptr, 0, nullptr, 0, C1_P(Magic080_Start)},
    {"Magic080_Run", 0x4FC3C0, 0x5D, kCalls4FC3C0, MH_N(kCalls4FC3C0), nullptr, 0, nullptr, 0, C1_P(Magic080_Run)},
    {"Magic080_DrawText", 0x4FC420, 0x11C, kCalls4FC420, MH_N(kCalls4FC420), nullptr, 0, nullptr, 0, C1_P(Magic080_DrawText)},
    {"Magic080_DrawGlyphs", 0x4FC540, 0x157, kCalls4FC540, MH_N(kCalls4FC540), nullptr, 0, nullptr, 0, C1_P(Magic080_DrawGlyphs)},
    {"Pentagram_Task", 0x4D67F0, 0x70, kCalls4D67F0, MH_N(kCalls4D67F0), kImms4D67F0, MH_N(kImms4D67F0), nullptr, 0, C1_P(Pentagram_Task)},
    {"Pentagram_Start", 0x4D6860, 0x9D, kCalls4D6860, MH_N(kCalls4D6860), nullptr, 0, nullptr, 0, C1_P(Pentagram_Start)},
    {"Pentagram_Rings", 0x4D6900, 0x12C, kCalls4D6900, MH_N(kCalls4D6900), nullptr, 0, nullptr, 0, C1_P(Pentagram_Rings)},
    {"Pentagram_Star", 0x4D6A30, 0x8D, kCalls4D6A30, MH_N(kCalls4D6A30), nullptr, 0, nullptr, 0, C1_P(Pentagram_Star)},
    {"Pentagram_Band", 0x4D6AC0, 0xA9, kCalls4D6AC0, MH_N(kCalls4D6AC0), nullptr, 0, nullptr, 0, C1_P(Pentagram_Band)},
    {"Pentagram_Sprites", 0x4D6B70, 0xE2, kCalls4D6B70, MH_N(kCalls4D6B70), nullptr, 0, nullptr, 0, C1_P(Pentagram_Sprites)},
    {"Pentagram_WaitSprites", 0x4D6C60, 0x18, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(Pentagram_WaitSprites)},
    {"Pentagram_WaitChildren", 0x4D6C80, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(Pentagram_WaitChildren)},
    {"Pentagram_DrawDisc", 0x4D6C90, 0x192, kCalls4D6C90, MH_N(kCalls4D6C90), nullptr, 0, nullptr, 0, C1_P(Pentagram_DrawDisc)},
    {"PentagramChild_Task", 0x4D6E30, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(PentagramChild_Task)},
    {"PentagramRing_Run", 0x4D6E50, 0x45, kCalls4D6E50, MH_N(kCalls4D6E50), nullptr, 0, nullptr, 0, C1_P(PentagramRing_Run)},
    {"PentagramRing_Grow", 0x4D6EA0, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(PentagramRing_Grow)},
    {"PentagramRing_Draw", 0x4D6ED0, 0x1A7, kCalls4D6ED0, MH_N(kCalls4D6ED0), nullptr, 0, nullptr, 0, C1_P(PentagramRing_Draw)},
    {"PentagramStar_Run", 0x4D7080, 0x2D, kCalls4D7080, MH_N(kCalls4D7080), nullptr, 0, nullptr, 0, C1_P(PentagramStar_Run)},
    {"PentagramStar_Grow", 0x4D70B0, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(PentagramStar_Grow)},
    {"PentagramFx_WaitRelease", 0x4D70E0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(PentagramFx_WaitRelease)},
    {"PentagramFx_Fade", 0x4D7100, 0x26, kCalls4D7100, MH_N(kCalls4D7100), nullptr, 0, nullptr, 0, C1_P(PentagramFx_Fade)},
    {"PentagramStar_Draw", 0x4D7130, 0x242, kCalls4D7130, MH_N(kCalls4D7130), nullptr, 0, nullptr, 0, C1_P(PentagramStar_Draw)},
    {"PentagramBand_Run", 0x4D7380, 0x2D, kCalls4D7380, MH_N(kCalls4D7380), nullptr, 0, nullptr, 0, C1_P(PentagramBand_Run)},
    {"PentagramBand_Grow", 0x4D73B0, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(PentagramBand_Grow)},
    {"PentagramBand_WaitRelease", 0x4D73E0, 0x21, kCalls4D73E0, MH_N(kCalls4D73E0), nullptr, 0, nullptr, 0, C1_P(PentagramBand_WaitRelease)},
    {"PentagramBand_Fade", 0x4D7410, 0x26, kCalls4D7410, MH_N(kCalls4D7410), nullptr, 0, nullptr, 0, C1_P(PentagramBand_Fade)},
    {"PentagramBand_Draw", 0x4D7440, 0x31F, kCalls4D7440, MH_N(kCalls4D7440), nullptr, 0, nullptr, 0, C1_P(PentagramBand_Draw)},
    {"PentagramSprite_Run", 0x4D7760, 0x68, kCalls4D7760, MH_N(kCalls4D7760), nullptr, 0, nullptr, 0, C1_P(PentagramSprite_Run)},
    {"PentagramSprite_Start", 0x4D77D0, 0xF9, kCalls4D77D0, MH_N(kCalls4D77D0), nullptr, 0, nullptr, 0, C1_P(PentagramSprite_Start)},
    {"PentagramSprite_Animate", 0x4D78D0, 0x83, kCalls4D78D0, MH_N(kCalls4D78D0), nullptr, 0, nullptr, 0, C1_P(PentagramSprite_Animate)},
    {"Ink_Task", 0x4E9A70, 0x2E, nullptr, 0, kImms4E9A70, MH_N(kImms4E9A70), nullptr, 0, C1_P(Ink_Task)},
    {"Ink_Start", 0x4E9AA0, 0xC5, kCalls4E9AA0, MH_N(kCalls4E9AA0), nullptr, 0, nullptr, 0, C1_P(Ink_Start)},
    {"InkPuff_Task", 0x4E9B70, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(InkPuff_Task)},
    {"InkPuff_Run", 0x4E9B90, 0x29, kCalls4E9B90, MH_N(kCalls4E9B90), nullptr, 0, nullptr, 0, C1_P(InkPuff_Run)},
    {"InkPuff_Start", 0x4E9BC0, 0xC9, kCalls4E9BC0, MH_N(kCalls4E9BC0), nullptr, 0, nullptr, 0, C1_P(InkPuff_Start)},
    {"Ink_DrawPuff", 0x4E9C90, 0x251, kCalls4E9C90, MH_N(kCalls4E9C90), nullptr, 0, nullptr, 0, C1_P(Ink_DrawPuff)},
    {"InkInk_Task", 0x4E9EF0, 0x7D, kCalls4E9EF0, MH_N(kCalls4E9EF0), kImms4E9EF0, MH_N(kImms4E9EF0), nullptr, 0, C1_P(InkInk_Task)},
    {"InkInk_Start", 0x4E9F70, 0x172, kCalls4E9F70, MH_N(kCalls4E9F70), nullptr, 0, nullptr, 0, C1_P(InkInk_Start)},
    {"InkInkActor_Task", 0x4EA0F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(InkInkActor_Task)},
    {"InkInkActor_Run", 0x4EA110, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(InkInkActor_Run)},
    {"InkInkActor_Start", 0x4EA130, 0x124, kCalls4EA130, MH_N(kCalls4EA130), nullptr, 0, nullptr, 0, C1_P(InkInkActor_Start)},
    {"InkInkPuff_Dispatch", 0x4EA260, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(InkInkPuff_Dispatch)},
    {"InkInkPuff_Run", 0x4EA280, 0x29, kCalls4EA280, MH_N(kCalls4EA280), nullptr, 0, nullptr, 0, C1_P(InkInkPuff_Run)},
    {"InkInkPuff_Start", 0x4EA2B0, 0xF7, kCalls4EA2B0, MH_N(kCalls4EA2B0), nullptr, 0, nullptr, 0, C1_P(InkInkPuff_Start)},
    {"InkInkPuff_Fade", 0x4EA3B0, 0x34, kCalls4EA3B0, MH_N(kCalls4EA3B0), nullptr, 0, nullptr, 0, C1_P(InkInkPuff_Fade)},
    {"InkInkPuff_Alloc", 0x4EA3F0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(InkInkPuff_Alloc), 0xFF},
    {"Magic213_Task", 0x4F4A60, 0x75, kCalls4F4A60, MH_N(kCalls4F4A60), kImms4F4A60, MH_N(kImms4F4A60), nullptr, 0, C1_P(Magic213_Task)},
    {"Magic213_Start", 0x4F4AE0, 0x153, kCalls4F4AE0, MH_N(kCalls4F4AE0), nullptr, 0, nullptr, 0, C1_P(Magic213_Start)},
    {"Magic213Actor_Task", 0x4F4C40, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(Magic213Actor_Task)},
    {"Magic213Actor_Run", 0x4F4C60, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(Magic213Actor_Run)},
    {"Magic213Actor_Start", 0x4F4C80, 0x120, kCalls4F4C80, MH_N(kCalls4F4C80), nullptr, 0, nullptr, 0, C1_P(Magic213Actor_Start)},
    {"ActorFx_TintUp", 0x4F4DA0, 0x65, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(ActorFx_TintUp)},
    {"Magic213Actor_Untint", 0x4F4E10, 0x86, kCalls4F4E10, MH_N(kCalls4F4E10), nullptr, 0, nullptr, 0, C1_P(Magic213Actor_Untint)},
    {"Magic213Mote_Dispatch", 0x4F4EA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(Magic213Mote_Dispatch)},
    {"Magic213Mote_Run", 0x4F4EC0, 0x3D, kCalls4F4EC0, MH_N(kCalls4F4EC0), nullptr, 0, nullptr, 0, C1_P(Magic213Mote_Run)},
    {"Magic213Mote_Start", 0x4F4F00, 0x126, kCalls4F4F00, MH_N(kCalls4F4F00), nullptr, 0, nullptr, 0, C1_P(Magic213Mote_Start)},
    {"Magic213Mote_End", 0x4F5030, 0x17, kCalls4F5030, MH_N(kCalls4F5030), nullptr, 0, nullptr, 0, C1_P(Magic213Mote_End)},
    {"Magic213Mote_Alloc", 0x4F5050, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, C1_P(Magic213Mote_Alloc), 0xFF},
};
#undef MH_N
#undef C1_P

enum : unsigned {
    kWhiteFlag_Task, kWhiteFlag_TintSource, kWhiteFlag_Untint,
    kMagic080_Task, kMagic080_Start, kMagic080_Run, kMagic080_DrawText, kMagic080_DrawGlyphs,
    kPentagram_Task, kPentagram_Start, kPentagram_Rings, kPentagram_Star, kPentagram_Band, kPentagram_Sprites,
    kPentagram_WaitSprites, kPentagram_WaitChildren, kPentagram_DrawDisc, kPentagramChild_Task, kPentagramRing_Run,
    kPentagramRing_Grow, kPentagramRing_Draw, kPentagramStar_Run, kPentagramStar_Grow, kPentagramFx_WaitRelease,
    kPentagramFx_Fade, kPentagramStar_Draw, kPentagramBand_Run, kPentagramBand_Grow, kPentagramBand_WaitRelease,
    kPentagramBand_Fade, kPentagramBand_Draw, kPentagramSprite_Run, kPentagramSprite_Start, kPentagramSprite_Animate,
    kInk_Task, kInk_Start, kInkPuff_Task, kInkPuff_Run, kInkPuff_Start, kInk_DrawPuff,
    kInkInk_Task, kInkInk_Start, kInkInkActor_Task, kInkInkActor_Run, kInkInkActor_Start, kInkInkPuff_Dispatch,
    kInkInkPuff_Run, kInkInkPuff_Start, kInkInkPuff_Fade, kInkInkPuff_Alloc,
    kMagic213_Task, kMagic213_Start, kMagic213Actor_Task, kMagic213Actor_Run, kMagic213Actor_Start, kActorFx_TintUp,
    kMagic213Actor_Untint, kMagic213Mote_Dispatch, kMagic213Mote_Run, kMagic213Mote_Start, kMagic213Mote_End,
    kMagic213Mote_Alloc, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the callees the standard set lacks -----------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define C1_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define C1_THEIRS(address) #address, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;

// The fuzz's packet buffer: Gfx_PacketNext points into it every round. The
// commits log it whole and advance the pointer by the size, as the real ones
// do; so every primitive of a draw is compared, and a copy that reads the
// pointer once where the original reads it again lands elsewhere.
constexpr unsigned kPacketBytes = 0x800;
alignas(16) unsigned char g_packets[kPacketBytes];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 7) * 0x40; }
void Advance(std::uint32_t size) {
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_packets || p + 0x100 > g_packets + kPacketBytes) p = g_packets + (size & 0x3C);
    Gfx_PacketNext = p;
}
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(g_packets, sizeof g_packets);
    Advance(a[1]);
    return answer;
}
std::uint32_t LinkEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(g_packets, sizeof g_packets);
    Advance(a[3]);
    return answer;
}
// 0x446770 turns the task's +0xC / +0x10 by its direction: the stand-in
// writes them from the stream, so a copy that does not read them again after
// the call shows.
std::uint32_t TurnEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* task = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    mh::FillBytes(task + 0xC, 8);
    return answer;
}
// Sprite_UpdateScreen draws through the frame-offset table pointer 0x9039D8.
std::uint32_t UpdateScreenEffect(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(static_cast<std::uint32_t>(Long(mh::Mem(0x9039D8))));
    return answer;
}

const mh::Callee kCallees[] = {
    {C1_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0},
    {C1_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    {C1_OURS(Tint_Release), 1, {kU8}, kG, 0, 0},
    {C1_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0},
    {C1_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &UpdateScreenEffect},
    {C1_OURS(AreaMap_Elevation), 2, {kAll, kAll}, kG, 0, 0},
    {C1_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {C1_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {C1_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {C1_OURS(Gpu_SetCode6C), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {C1_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {C1_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    // The projections: the vertices logged by what they hold (six bytes of
    // each SVECTOR), the outputs in the packet buffer by address, the depth
    // and flag pointers into the caller's frame masked off.
    {C1_OURS(Gte_RotTransPers), 4, {kAll, kAll, 0, 0}, kG, 0, 0, {6}},
    {C1_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {C1_OURS(Gte_RotTransPers4), 10, {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {C1_OURS(Gte_StoreDepthF), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    {C1_OURS(Gte_PrimDepths4_10B), 1, {kAll}, kG, 0, 0},
    // Capcom's, and other groups' called by address.
    {C1_THEIRS(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    {C1_THEIRS(0x4F6290), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4DF820), 1, {kAll}, kG, 0, 0},
    // This group's own, called directly.
    {C1_THEIRS(0x4D6C90), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4D6ED0), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4D7130), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4D7440), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4E9C90), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4EA260), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4F4EA0), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4FC420), 0, {}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4FC540), 0, {}, mh::Answer::kPhase, 0, 0},
    // The phases PentagramSprite_Run and Magic213Mote_Run call with the
    // frame-offset table 0x9039D8 switched: listed before their .data tables
    // register them as plain handlers, so each call also logs the pointer.
    {C1_THEIRS(0x4E47F0), 0, {0x9039D8}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4D77D0), 0, {0x9039D8}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4D78D0), 0, {0x9039D8}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4F4F00), 0, {0x9039D8}, mh::Answer::kPhase, 0, 0},
    {C1_THEIRS(0x4F5030), 0, {0x9039D8}, mh::Answer::kPhase, 0, 0},
    // the allocs: an index 0..0x3F, or 0xFF (none free)
    {C1_THEIRS(0x4EA3F0), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},
    {C1_THEIRS(0x4F5050), 0, {}, mh::Answer::kByte, 0xFF, 0x3F},
};

// The .data handler tables the dispatchers read in place (symbols.toml), each
// cell once: where two tables abut, each lists its own.
const mh::DataTable kTables[] = {
    {0x65B9F0, 4}, {0x65BA08, 3}, {0x65BA1C, 3}, {0x65BA28, 3}, {0x65BA34, 3},
    {0x65BE00, 1}, {0x65BE04, 3}, {0x65BE34, 1}, {0x65BE38, 2}, {0x65BE40, 1}, {0x65BE44, 3},
    {0x65C1E4, 1}, {0x65C1E8, 4}, {0x65C1F8, 1}, {0x65C1FC, 2},
};

constexpr std::uint32_t kScratch = 0x903850, kVertices = 0x9037A0;
constexpr std::uint32_t kInkPool = 0x6A6F40, kMotePool = 0x6B2958, kPoolBytes = 64 * 0x84;
const mh::Region kRegions[] = {
    {kScratch, 0x10},
    {kVertices, 0x20},
    {0x7E0670, 4},                               // Gfx_PacketNext
    {Key(g_packets), sizeof g_packets},
    {Key(MoveScript_TintRecords), 0xC00},
    {0x812980, 0x60},                            // Gfx_ClutStrip 0x1A00..0x1A2F
    {0x80E980, 0x60},                            // Gfx_ClutStripSource, the same
    {0x905E60, 8},                               // Field_Kind2Z, Field_Kind2X
    {0x9039D8, 4},                               // the frame-offset table pointer
    {0x803154, 1},
    {0x92BF14, 1},
    {kInkPool, kPoolBytes},
    {kMotePool, kPoolBytes},
    // the overlays' .data the functions read, less the handler cells
    {0x65BA00, 8},                               // PentagramRing_Radii
    {0x65BA14, 6},                               // PentagramStar_Points
    {0x65BE10, 0x24},                            // InkPuff_Offsets
    {0x65BE50, 0x24},                            // InkInkPuff_Offsets
    {0x65DA28, 0x1C},                            // Magic080_GlyphCells
};

unsigned char* Sc() { return Sprite_Current; }
unsigned char* PoolRecord(std::uint32_t pool, unsigned i) { return mh::Mem(pool + (i & 63) * 0x84u); }

// A real slot or record for a pool record's owner (the walks make it the
// owner cell, which the recorders write through).
const void* SomeOwner(std::uint32_t v) {
    return (v & 4) ? static_cast<const void*>(mh::SpriteRecord(v & 1)) : static_cast<const void*>(mh::TaskAt(v & 3));
}

// The group's cells an effect reads again after a call: Gfx_PacketNext, a
// scratch word, a vertex word, the owner's count +0xB near its thresholds, a
// pool record's live bit, the target's side bit.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFF;
    const auto word = static_cast<std::uint16_t>(h >> 16);
    switch ((h >> 8) % 7) {
    case 0: mh::SetPointer(0x7E0670, PacketAt(v)); break;
    case 1: SetWord(mh::Mem(kScratch + (v % 8) * 2), word); break;
    case 2: SetWord(mh::Mem(kVertices + (v % 16) * 2), word); break;
    case 3: {
        static const unsigned char kCounts[] = {3, 4, 5, 6, 0x80, 0x84};
        mh::Pointer(mh::at::kOwner)[0xB] = kCounts[v % 6];
        break;
    }
    case 4: PoolRecord(v & 0x80 ? kMotePool : kInkPool, v)[0] ^= 1; break;
    case 5: mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Mem(mh::at::kTarget)[0] ^ 0x40); break;
    default: break;
    }
}

// --- the seed --------------------------------------------------------------------

unsigned char Near(unsigned at, unsigned below, unsigned above) {
    return static_cast<unsigned char>(at - below + mh::Next() % (below + above + 1));
}

// An actor index for +4 that fits the target byte's side most of the time.
unsigned char ActorFor(bool enemies) {
    if (!mh::Often()) return static_cast<unsigned char>(mh::Next() % 11);
    return static_cast<unsigned char>(enemies ? 3 + mh::Next() % 8 : mh::Next() % 3);
}

void FillPool(std::uint32_t pool) {
    const unsigned mode = mh::Next() % 4;
    const unsigned taken = mh::Next() % 65;
    for (unsigned i = 0; i < 64; ++i) {
        unsigned char* const rec = PoolRecord(pool, i);
        if (mode == 0) rec[0] = static_cast<unsigned char>(rec[0] | 1);                 // full
        else if (mode == 1) rec[0] = static_cast<unsigned char>(i < taken ? rec[0] | 1 : rec[0] & ~1u);
        mh::SetPointer(pool + i * 0x84u + 0x80, SomeOwner(mh::Next()));
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    mh::SetPointer(0x7E0670, PacketAt(mh::Next()));
    FillPool(kInkPool);
    FillPool(kMotePool);
    if (mh::Half()) mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Mem(mh::at::kTarget)[0] | 0x40);
    const bool enemies = (mh::Mem(mh::at::kTarget)[0] & 0x40) != 0;
    unsigned char* const owner = mh::Pointer(mh::at::kOwner);
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kWhiteFlag_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 4); break;
    case kMagic080_Task: case kMagic213_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kPentagram_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 8); if (mh::Half()) sc[0] = 0; break;
    case kInk_Task: case kInkInk_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kPentagramChild_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 4); break;
    case kInkPuff_Task: case kInkInkActor_Task: case kInkInkPuff_Dispatch: case kMagic213Actor_Task:
    case kMagic213Mote_Dispatch:
        sc[1] = 0;
        break;
    case kPentagramRing_Run: case kPentagramStar_Run: case kPentagramBand_Run: case kPentagramSprite_Run:
    case kInkPuff_Run: case kInkInkPuff_Run:
        sc[2] = static_cast<unsigned char>(mh::Next() % 3);
        if (mh::Half()) sc[0] = 0;
        if (mh::Half()) sc[0xB] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kInkInkActor_Run: case kMagic213Mote_Run:
        sc[2] = static_cast<unsigned char>(mh::Next() % 2);
        if (mh::Half()) sc[0] = 0;
        break;
    case kMagic213Actor_Run: sc[2] = static_cast<unsigned char>(mh::Next() % 4); break;
    // the count-downs and count-ups: at, below and past their ends
    case kWhiteFlag_Untint: case kMagic080_Run: case kInkPuff_Start: case kInkInkActor_Start: case kInkInkPuff_Start:
    case kMagic213Actor_Start: case kActorFx_TintUp: case kMagic213Mote_Start:
        if (mh::Often()) sc[9] = static_cast<unsigned char>(1 + mh::Next() % 2);
        if (k == kMagic080_Run && mh::Half()) sc[0xB] = 0;
        if (k == kInkPuff_Start || k == kInkInkPuff_Start) {
            if (mh::Often()) sc[0xB] = static_cast<unsigned char>(mh::Next() % 3);
        }
        if (k == kInkInkActor_Start || k == kMagic213Actor_Start) sc[4] = ActorFor(enemies);
        break;
    case kPentagram_Start: {
        static const unsigned char kSides[] = {0, 1, 2, 3, 4, 10};
        mh::Mem(mh::at::kTarget)[0] = kSides[mh::Next() % 6];
        mh::Mem(mh::at::kActor)[0] = static_cast<unsigned char>(mh::Next() % 5);
        break;
    }
    case kPentagram_Rings: if (mh::Often()) sc[9] = Near(0xF, 1, 1); break;
    case kPentagram_Star: if (mh::Often()) sc[0xB] = Near(3, 1, 1); break;
    case kPentagram_Band: if (mh::Often()) sc[0xB] = Near(4, 1, 1); break;
    case kPentagram_Sprites: if (mh::Often()) sc[0xB] = Near(5, 1, 1); break;
    case kPentagram_WaitSprites: if (mh::Often()) sc[0xB] = Near(6, 1, 1); break;
    case kPentagram_WaitChildren: if (mh::Often()) sc[0xB] = Near(0x80, 1, 1); break;
    case kPentagramRing_Grow: if (mh::Often()) sc[9] = Near(0x20, 1, 1); break;
    case kPentagramStar_Grow: if (mh::Often()) sc[9] = Near(0x27, 1, 1); break;
    case kPentagramBand_Grow: if (mh::Often()) sc[9] = Near(0x30, 1, 1); break;
    case kPentagramFx_WaitRelease: case kPentagramBand_WaitRelease: if (mh::Often()) owner[0xB] = Near(0x84, 1, 1); break;
    case kPentagramFx_Fade: if (mh::Often()) sc[0xA] = Near(0xB, 1, 1); break;
    case kPentagramBand_Fade: if (mh::Often()) sc[0xA] = Near(0xF, 1, 1); break;
    case kInkInkPuff_Fade: if (mh::Often()) sc[0xA] = static_cast<unsigned char>(1 + mh::Next() % 2); break;
    // the draws: loops bounded by +9, their boundaries
    case kPentagramRing_Draw:
        sc[9] = static_cast<unsigned char>(mh::Next() % 0x24);
        if (mh::Half()) sc[2] = 2;
        break;
    case kPentagramStar_Draw:
        sc[9] = static_cast<unsigned char>(mh::Next() % 0x30);
        if (mh::Half()) sc[2] = 2;
        break;
    case kPentagramBand_Draw:
        sc[9] = mh::Often() ? static_cast<unsigned char>(mh::Next() % 0x24) : Near(0x10, 1, 1);
        mh::SetRandHint(mh::Next() & 0x3F);
        break;
    case kPentagramSprite_Start:
        sc[0xB] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Half()) sc[4] = 0;
        break;
    case kPentagramSprite_Animate:
        if (mh::Often()) sc[9] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Half()) sc[0xB] = 0;
        break;
    case kMagic213Actor_Untint: {
        const unsigned tint = sc[0xA];
        if (mh::Half()) MoveScript_TintRecords[tint * 12u + 2] = static_cast<unsigned char>(1 + mh::Next() % 2);
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    const mh::Group group = {
        "magic_c1", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000, nullptr, 3,
    };
    mh::Run(group);
}

}  // namespace magic_c1
