// BOF3X_SHADOW=magic_s28: group S28's three overlays (MAGIC122, 123, 124) and
// Port_DroppedCall through the spell round's shared harness (magic_harness.h),
// once at start-up. docs/magic_s28.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC122 / 123 / 124 --clones
// (2026-09-26; capstone, no jump tables, no REFUSED lines), names given. Beyond
// the standard set the group lists the GPU calls its draws make (a commit logs
// the primitive it links: every primitive of a draw is built in the same
// bytes), the trigonometry, its own functions it calls directly (logged as
// the task they run for), its two pool allocators and MAGIC219's record free;
// the seven .data handler tables; the regions its functions read (the two
// pools, the scratch, the CLUT row, the angle bytes, a primitive buffer of the
// fuzz's own); a seed per function; a disturbance of its own cells, and a
// settle that keeps the divisors the draws read again after every call (the
// beam's +0xB, the bolt's +0xA) from 0 - where the original faults as ours
// aborts.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s28.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s28 {
namespace {

namespace mh = magic_harness;
namespace at = magic_harness::at;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC122 / 123 / 124 --clones, 2026-09-26, names given.
constexpr mh::Imm kImms4DD8B0[] = {{0xF, 0x4DD8E0}, {0x17, 0x4BDC10}};
constexpr mh::CallSite kCalls4DD8E0[] = {{0x49, 0x435180}};
constexpr mh::CallSite kCalls4DDA10[] = {{0x2A, 0x4DE370}, {0x2F, 0x4DDCE0}};
constexpr mh::CallSite kCalls4DDA50[] = {{0x2A, 0x4FC0E0}, {0x2F, 0x4FBD10}, {0x7D, 0x4FBD10}, {0x8B, 0x4FC2D0}, {0x15D, 0x5A7A70}, {0x19E, 0x587900}};
constexpr mh::CallSite kCalls4DDC30[] = {{0x2E, 0x452F70}};
constexpr mh::CallSite kCalls4DDC90[] = {{0x3C, 0x4351F0}};
constexpr mh::CallSite kCalls4DDCE0[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x87, 0x5A7A00}, {0xA8, 0x5A7A00}, {0xC2, 0x5A7A50}, {0xFD, 0x5A7630}, {0x105, 0x5A7780}, {0x123, 0x5A7A00}, {0x157, 0x5A7A00}, {0x185, 0x5A7A50}, {0x1B0, 0x5A7A00}, {0x1E4, 0x5A7A00}, {0x212, 0x5A7A50}, {0x287, 0x5A79E0}, {0x29E, 0x5A79A0}, {0x398, 0x461E50}, {0x3A4, 0x5A7630}, {0x3AC, 0x5A7780}, {0x3C7, 0x5A7A00}, {0x3FC, 0x5A7A00}, {0x42A, 0x5A7A50}, {0x455, 0x5A7A00}, {0x48A, 0x5A7A00}, {0x4B8, 0x5A7A50}, {0x52D, 0x5A79E0}, {0x544, 0x5A79A0}, {0x63E, 0x461E50}, {0x66C, 0x5A77C0}, {0x675, 0x461E50}};
constexpr mh::CallSite kCalls4DE370[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x87, 0x5A7A00}, {0xA8, 0x5A7A00}, {0xC2, 0x5A7A50}, {0xFD, 0x5A7610}, {0x105, 0x5A7780}, {0x125, 0x5A7A00}, {0x162, 0x5A7A00}, {0x190, 0x5A7A50}, {0x1BB, 0x5A7A00}, {0x1F5, 0x5A7A00}, {0x223, 0x5A7A50}, {0x2FC, 0x461E50}, {0x308, 0x5A7610}, {0x310, 0x5A7780}, {0x32E, 0x5A7A00}, {0x36B, 0x5A7A00}, {0x399, 0x5A7A50}, {0x3C4, 0x5A7A00}, {0x3FF, 0x5A7A00}, {0x42D, 0x5A7A50}, {0x50B, 0x461E50}, {0x538, 0x5A77C0}, {0x541, 0x461E50}};
constexpr mh::CallSite kCalls4DE8C0[] = {{0x35, 0x5A77C0}, {0x3E, 0x461E50}, {0x72, 0x4DEB80}, {0x9B, 0x5A77C0}, {0xA4, 0x461E50}};
constexpr mh::Imm kImms4DE8C0[] = {{0x16, 0x4DE980}, {0x1E, 0x4F7350}};
constexpr mh::CallSite kCalls4DE980[] = {{0x2E, 0x4FC0E0}, {0x33, 0x4FBD10}, {0x81, 0x4FBD10}, {0x8F, 0x4FC2D0}, {0x163, 0x5A7A70}, {0x17F, 0x4DF450}, {0x1CE, 0x587900}, {0x1DB, 0x452F70}};
constexpr mh::CallSite kCalls4DEBC0[] = {{0x3E, 0x5B93D2}, {0x47, 0x5B93D2}, {0x67, 0x5B93D2}, {0x84, 0x5B93D2}, {0x94, 0x5B93D2}, {0xA8, 0x5B93D2}, {0xB6, 0x5B93D2}};
constexpr mh::CallSite kCalls4DECA0[] = {{0x1D, 0x5A7A00}, {0x4A, 0x5A7A00}, {0x70, 0x5A7A50}, {0xF7, 0x587900}, {0x139, 0x4DEE50}, {0x13E, 0x4DEFD0}};
constexpr mh::CallSite kCalls4DEDF0[] = {{0x3B, 0x4F6290}, {0x40, 0x4DF210}};
constexpr mh::CallSite kCalls4DEE40[] = {{0x8, 0x4F6290}};
constexpr mh::CallSite kCalls4DEE50[] = {{0x20, 0x5A75F0}, {0x28, 0x5A7780}, {0x72, 0x5A7A00}, {0xA0, 0x5A7A50}, {0xE6, 0x5A7A00}, {0x114, 0x5A7A50}, {0x15C, 0x461E50}};
constexpr mh::CallSite kCalls4DEFD0[] = {{0x15, 0x5B93D2}, {0x40, 0x5A7610}, {0x47, 0x5A7780}, {0x6D, 0x5A7A00}, {0x9B, 0x5A7A50}, {0xC9, 0x5A7A00}, {0xF7, 0x5A7A50}, {0x13D, 0x5A7A00}, {0x16B, 0x5A7A50}, {0x199, 0x5A7A00}, {0x1C7, 0x5A7A50}, {0x21B, 0x461E50}};
constexpr mh::CallSite kCalls4DF210[] = {{0x29, 0x5B93D2}, {0x47, 0x5A7A00}, {0x6A, 0x5A7A50}, {0x93, 0x5A75F0}, {0x9B, 0x5A7780}, {0xBE, 0x5A7A00}, {0xEC, 0x5A7A50}, {0x12E, 0x5A7A00}, {0x15C, 0x5A7A50}, {0x1A1, 0x5A7A00}, {0x1CF, 0x5A7A50}, {0x21D, 0x461E50}};
constexpr mh::CallSite kCalls4DF4B0[] = {{0x45, 0x5A77C0}, {0x4E, 0x461E50}, {0x82, 0x4DF830}, {0x9C, 0x4DF6D0}, {0xB0, 0x5A77C0}, {0xB9, 0x461E50}};
constexpr mh::Imm kImms4DF4B0[] = {{0x16, 0x4DF580}, {0x1E, 0x4F9F70}, {0x26, 0x4DF6A0}, {0x2E, 0x4F7350}};
constexpr mh::CallSite kCalls4DF580[] = {{0x62, 0x4FBD10}, {0x67, 0x4FC2D0}, {0xB2, 0x4E08B0}, {0x106, 0x587900}};
constexpr mh::CallSite kCalls4DF6D0[] = {{0x15, 0x5B93D2}, {0x39, 0x5A75F0}, {0x41, 0x5A7780}, {0x71, 0x5A7A00}, {0x94, 0x5A7A50}, {0xBD, 0x5A7A00}, {0xE0, 0x5A7A50}, {0x127, 0x461E50}};
constexpr mh::CallSite kCalls4DF850[] = {{0x27, 0x4DFAE0}, {0x44, 0x4E08B0}};
constexpr mh::CallSite kCalls4DF8F0[] = {{0x1F, 0x4FC0E0}, {0x24, 0x4FBD10}, {0xB4, 0x5A7A70}, {0xD5, 0x5B93D2}, {0xDF, 0x5B93D2}, {0xFF, 0x5B93D2}};
constexpr mh::CallSite kCalls4DFA40[] = {{0xF, 0x5A7A00}};
constexpr mh::CallSite kCalls4DFA90[] = {{0xF, 0x5A7A00}, {0x4A, 0x4F6290}};
constexpr mh::CallSite kCalls4DFAE0[] = {{0x6, 0x5B93D2}, {0x6B, 0x5A7A00}, {0x7D, 0x5A7A00}, {0x97, 0x5A7A50}, {0xE5, 0x5A7A70}, {0x12D, 0x5B93D2}, {0x162, 0x5A7A00}, {0x17D, 0x5A7A50}, {0x1A6, 0x5A7A00}, {0x1AD, 0x5B93D2}, {0x1CF, 0x5A7A00}, {0x1DE, 0x5A7A00}, {0x1F8, 0x5A7A50}, {0x259, 0x5A7A70}, {0x26B, 0x5A7610}, {0x273, 0x5A7780}, {0x295, 0x5A7A00}, {0x2C3, 0x5A7A50}, {0x2F1, 0x5A7A00}, {0x31F, 0x5A7A50}, {0x3E7, 0x461E50}, {0x3F3, 0x5A7610}, {0x3FB, 0x5A7780}, {0x408, 0x5A7A00}, {0x439, 0x5A7A50}, {0x46A, 0x5A7A00}, {0x49B, 0x5A7A50}, {0x4CC, 0x5A7A00}, {0x4FA, 0x5A7A50}, {0x528, 0x5A7A00}, {0x556, 0x5A7A50}, {0x5DB, 0x461E50}, {0x5E7, 0x5A7610}, {0x5EF, 0x5A7780}, {0x611, 0x5A7A00}, {0x63F, 0x5A7A50}, {0x66D, 0x5A7A00}, {0x69B, 0x5A7A50}, {0x763, 0x461E50}, {0x76F, 0x5A7610}, {0x777, 0x5A7780}, {0x784, 0x5A7A00}, {0x7B5, 0x5A7A50}, {0x7E6, 0x5A7A00}, {0x817, 0x5A7A50}, {0x848, 0x5A7A00}, {0x876, 0x5A7A50}, {0x8A4, 0x5A7A00}, {0x8D2, 0x5A7A50}, {0x957, 0x461E50}};
constexpr mh::CallSite kCalls4E0470[] = {{0x23, 0x4E0550}, {0x28, 0x4E06C0}};
constexpr mh::CallSite kCalls4E0510[] = {{0x2F, 0x4F6290}};
constexpr mh::CallSite kCalls4E0550[] = {{0x51, 0x5A75F0}, {0x59, 0x5A7780}, {0x88, 0x5A7A00}, {0xAB, 0x5A7A50}, {0xD4, 0x5A7A00}, {0xF7, 0x5A7A50}, {0x144, 0x461E50}};
constexpr mh::CallSite kCalls4E06C0[] = {{0x56, 0x5A7610}, {0x5E, 0x5A7780}, {0x64, 0x5A7A00}, {0x87, 0x5A7A50}, {0xB0, 0x5A7A00}, {0xD3, 0x5A7A50}, {0xF6, 0x5A7A00}, {0x11B, 0x5A7A50}, {0x142, 0x5A7A00}, {0x165, 0x5A7A50}, {0x1C1, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Firebreath_Task", 0x4DD8B0, 0x26, nullptr, 0, kImms4DD8B0, MH_N(kImms4DD8B0), nullptr, 0, reinterpret_cast<const void*>(&::Firebreath_Task)},
    {"Firebreath_Start", 0x4DD8E0, 0x103, kCalls4DD8E0, MH_N(kCalls4DD8E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Firebreath_Start)},
    {"BreathBeam_Task", 0x4DD9F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_Task)},
    {"BreathBeam_Run", 0x4DDA10, 0x35, kCalls4DDA10, MH_N(kCalls4DDA10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_Run)},
    {"BreathBeam_Aim", 0x4DDA50, 0x1A8, kCalls4DDA50, MH_N(kCalls4DDA50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_Aim)},
    {"BreathBeam_Widen", 0x4DDC00, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_Widen)},
    {"BreathBeam_Hit", 0x4DDC30, 0x3F, kCalls4DDC30, MH_N(kCalls4DDC30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_Hit)},
    {"BreathBeam_Hold", 0x4DDC70, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_Hold)},
    {"BreathBeam_Fade", 0x4DDC90, 0x42, kCalls4DDC90, MH_N(kCalls4DDC90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_Fade)},
    {"BreathBeam_DrawTextured", 0x4DDCE0, 0x682, kCalls4DDCE0, MH_N(kCalls4DDCE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_DrawTextured)},
    {"BreathBeam_DrawGlow", 0x4DE370, 0x54E, kCalls4DE370, MH_N(kCalls4DE370), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathBeam_DrawGlow)},
    {"Icebreath_Task", 0x4DE8C0, 0xB4, kCalls4DE8C0, MH_N(kCalls4DE8C0), kImms4DE8C0, MH_N(kImms4DE8C0), nullptr, 0, reinterpret_cast<const void*>(&::Icebreath_Task)},
    {"Icebreath_Start", 0x4DE980, 0x1F9, kCalls4DE980, MH_N(kCalls4DE980), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Icebreath_Start)},
    {"IcePool_Dispatch", 0x4DEB80, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IcePool_Dispatch)},
    {"BreathMote_Task", 0x4DEBA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathMote_Task)},
    {"BreathMote_Launch", 0x4DEBC0, 0xDA, kCalls4DEBC0, MH_N(kCalls4DEBC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathMote_Launch)},
    {"BreathMote_Fly", 0x4DECA0, 0x144, kCalls4DECA0, MH_N(kCalls4DECA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathMote_Fly)},
    {"BreathMote_Burst", 0x4DEDF0, 0x45, kCalls4DEDF0, MH_N(kCalls4DEDF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathMote_Burst)},
    {"BreathMote_Free", 0x4DEE40, 0xD, kCalls4DEE40, MH_N(kCalls4DEE40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathMote_Free)},
    {"BreathMote_DrawHex", 0x4DEE50, 0x173, kCalls4DEE50, MH_N(kCalls4DEE50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathMote_DrawHex)},
    {"BreathMote_DrawRing", 0x4DEFD0, 0x232, kCalls4DEFD0, MH_N(kCalls4DEFD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathMote_DrawRing)},
    {"BreathMote_DrawBurst", 0x4DF210, 0x23E, kCalls4DF210, MH_N(kCalls4DF210), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::BreathMote_DrawBurst)},
    {"IcePool_Alloc", 0x4DF450, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::IcePool_Alloc), 0xFF},
    {"Thunderbreath_Task", 0x4DF4B0, 0xC9, kCalls4DF4B0, MH_N(kCalls4DF4B0), kImms4DF4B0, MH_N(kImms4DF4B0), nullptr, 0, reinterpret_cast<const void*>(&::Thunderbreath_Task)},
    {"Thunderbreath_Start", 0x4DF580, 0x111, kCalls4DF580, MH_N(kCalls4DF580), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Thunderbreath_Start)},
    {"Thunderbreath_Wait", 0x4DF6A0, 0x23, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Thunderbreath_Wait)},
    {"Thunderbreath_DrawOrb", 0x4DF6D0, 0x146, kCalls4DF6D0, MH_N(kCalls4DF6D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Thunderbreath_DrawOrb)},
    {"Port_DroppedCall", 0x4DF820, 0x1, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Port_DroppedCall)},
    {"ThunderPool_Dispatch", 0x4DF830, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderPool_Dispatch)},
    {"ThunderBolt_Task", 0x4DF850, 0x9E, kCalls4DF850, MH_N(kCalls4DF850), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderBolt_Task)},
    {"ThunderBolt_Aim", 0x4DF8F0, 0x12D, kCalls4DF8F0, MH_N(kCalls4DF8F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderBolt_Aim)},
    {"ThunderBolt_Grow", 0x4DFA20, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderBolt_Grow)},
    {"ThunderBolt_Flicker", 0x4DFA40, 0x43, kCalls4DFA40, MH_N(kCalls4DFA40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderBolt_Flicker)},
    {"ThunderBolt_Fade", 0x4DFA90, 0x50, kCalls4DFA90, MH_N(kCalls4DFA90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderBolt_Fade)},
    {"ThunderBolt_Draw", 0x4DFAE0, 0x98E, kCalls4DFAE0, MH_N(kCalls4DFAE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderBolt_Draw)},
    {"ThunderSpark_Task", 0x4E0470, 0x2E, kCalls4E0470, MH_N(kCalls4E0470), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderSpark_Task)},
    {"ThunderSpark_Start", 0x4E04A0, 0x39, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderSpark_Start)},
    {"ThunderSpark_Grow", 0x4E04E0, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderSpark_Grow)},
    {"ThunderSpark_Fade", 0x4E0510, 0x35, kCalls4E0510, MH_N(kCalls4E0510), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderSpark_Fade)},
    {"ThunderSpark_DrawFan", 0x4E0550, 0x163, kCalls4E0550, MH_N(kCalls4E0550), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderSpark_DrawFan)},
    {"ThunderSpark_DrawRing", 0x4E06C0, 0x1E2, kCalls4E06C0, MH_N(kCalls4E06C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderSpark_DrawRing)},
    {"ThunderPool_Alloc", 0x4E08B0, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ThunderPool_Alloc), 0xFF},
};
#undef MH_N

enum : unsigned {
    kFirebreath_Task, kFirebreath_Start, kBreathBeam_Task, kBreathBeam_Run, kBreathBeam_Aim, kBreathBeam_Widen,
    kBreathBeam_Hit, kBreathBeam_Hold, kBreathBeam_Fade, kBreathBeam_DrawTextured, kBreathBeam_DrawGlow,
    kIcebreath_Task, kIcebreath_Start, kIcePool_Dispatch, kBreathMote_Task, kBreathMote_Launch, kBreathMote_Fly,
    kBreathMote_Burst, kBreathMote_Free, kBreathMote_DrawHex, kBreathMote_DrawRing, kBreathMote_DrawBurst,
    kIcePool_Alloc, kThunderbreath_Task, kThunderbreath_Start, kThunderbreath_Wait, kThunderbreath_DrawOrb,
    kPort_DroppedCall, kThunderPool_Dispatch, kThunderBolt_Task, kThunderBolt_Aim, kThunderBolt_Grow,
    kThunderBolt_Flicker, kThunderBolt_Fade, kThunderBolt_Draw, kThunderSpark_Task, kThunderSpark_Start,
    kThunderSpark_Grow, kThunderSpark_Fade, kThunderSpark_DrawFan, kThunderSpark_DrawRing, kThunderPool_Alloc,
    kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// The function being fuzzed (the seed's k), for the settle and one effect.
unsigned g_k;

// --- the state -------------------------------------------------------------------

constexpr std::uint32_t kScratch = 0x903850, kVertices = 0x9037A0;
constexpr std::uint32_t kMotePool = 0x69ACC8, kBoltPool = 0x69DB30;
constexpr unsigned kMotes = 90, kBolts = 64;
constexpr std::uint32_t kAngles = 0x65BB78;

// The primitives the draws fill: Gfx_PacketNext is aimed at one of four places
// in this buffer and moved on by a commit (or the disturbance), so a copy that
// keeps the pointer where the original reads it again shows.
constexpr unsigned kPrimBytes = 0x200;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(std::uint32_t v) { return g_prims + 0x10 * (v % 8); }

std::uint32_t K(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return K(reinterpret_cast<const void*>(f)); }

const mh::Region kRegions[] = {
    {kScratch, 0x10},
    {kVertices, 0x20},
    {0x7E0670, 4},                               // Gfx_PacketNext
    {kMotePool, kMotes * 0x84},                  // BreathMote_Pool
    {kBoltPool, kBolts * 0x84},                  // ThunderPool
    {0x812980, 0x60},                            // CLUT row 26's first 48 words
    {0x80E980, 0x60},                            // and its buffer
    {kAngles, 8},                                // BreathMote_Angles
    {0, kPrimBytes},                             // g_prims (its address at start-up)
};
mh::Region g_regions[sizeof kRegions / sizeof kRegions[0]];

// --- the callees' effects --------------------------------------------------------------

// Math_Sin / Math_Cos: in the range the tables answer, a quarter of the time
// anything.
std::uint32_t Trig(const std::uint32_t*, std::uint32_t h) {
    return h % 4 == 0 ? h : static_cast<std::uint32_t>(static_cast<int>((h >> 8) % 8193) - 4096);
}
// A commit logs the primitive it links (the size's bytes at Gfx_PacketNext),
// then moves the cursor on by the size half the time, inside the buffer.
std::uint32_t Commit(const std::uint32_t* a, std::uint32_t h) {
    unsigned char* const at = Gfx_PacketNext;
    const unsigned size = a[1] & 0xFF;
    mh::NoteBytes(at, size);
    if (mh::Noise() % 2 && at >= g_prims && at + size + 0x60 < g_prims + kPrimBytes) Gfx_PacketNext = at + size;
    return h;
}
// Gpu_SetDrawMode's fifth argument.
std::uint32_t Fifth(const std::uint32_t* a, std::uint32_t h) {
    mh::Note(a[4]);
    return h;
}
// A primitive setter writes the code byte.
std::uint32_t SetPrim(const std::uint32_t* a, std::uint32_t h) {
    mh::FillBytes(reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0])) + 7, 1);
    return h;
}
// ThunderPool_Alloc: "none free" (0xFF) a quarter of the time when the caller
// tests it (ThunderBolt_Task); Thunderbreath_Start, which never meets a full
// pool in play (it clears the pool first), always gets a record.
std::uint32_t BoltAlloc(const std::uint32_t*, std::uint32_t h) {
    if (g_k == kThunderBolt_Task && mh::Noise() % 4 == 0) return h | 0xFF;
    return h;
}

constexpr std::uint32_t kAll = 0xFFFFFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage, kPhase = mh::Answer::kPhase;
#define S28_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S28_AT(address) #address, address, address

const mh::Callee kCallees[] = {
    {S28_OURS(Math_Sin), 1, {kAll}, kG, 0, 0, {}, &Trig},
    {S28_OURS(Math_Cos), 1, {kAll}, kG, 0, 0, {}, &Trig},
    {S28_OURS(Math_Ratan2), 2, {kAll, kAll}, kG, 0, 0},
    {S28_OURS(Gpu_SetDrawMode), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &Fifth},
    {S28_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &Commit},
    {S28_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0, {}, &SetPrim},
    {S28_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0, {}, &SetPrim},
    {S28_OURS(Gpu_SetPolyGT4), 1, {kAll}, kG, 0, 0, {}, &SetPrim},
    {S28_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S28_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S28_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    // a pool record's free: MAGIC219's (group S37), by its address
    {S28_AT(0x4F6290), 0, {}, kG, 0, 0},
    // the group's own, called directly: logged as the task they run for
    {S28_OURS(BreathBeam_DrawTextured), 0, {}, kPhase, 0, 0},
    {S28_OURS(BreathBeam_DrawGlow), 0, {}, kPhase, 0, 0},
    {S28_OURS(IcePool_Dispatch), 0, {}, kPhase, 0, 0},
    {S28_OURS(BreathMote_DrawHex), 0, {}, kPhase, 0, 0},
    {S28_OURS(BreathMote_DrawRing), 0, {}, kPhase, 0, 0},
    {S28_OURS(BreathMote_DrawBurst), 0, {}, kPhase, 0, 0},
    {S28_OURS(ThunderPool_Dispatch), 0, {}, kPhase, 0, 0},
    {S28_OURS(Thunderbreath_DrawOrb), 0, {}, kPhase, 0, 0},
    {S28_OURS(ThunderBolt_Draw), 0, {}, kPhase, 0, 0},
    {S28_OURS(ThunderSpark_DrawFan), 0, {}, kPhase, 0, 0},
    {S28_OURS(ThunderSpark_DrawRing), 0, {}, kPhase, 0, 0},
    // the allocators: a record's index (the starts use it unchecked; only the
    // bolt's spark tests 0xFF)
    {S28_OURS(IcePool_Alloc), 0, {}, mh::Answer::kByte, 0, kMotes - 1},
    {S28_OURS(ThunderPool_Alloc), 0, {}, mh::Answer::kByte, 0, kBolts - 1, {}, &BoltAlloc},
};
#undef S28_OURS
#undef S28_AT

// The .data tables the dispatchers read in place (symbols.toml [[data]]),
// their entries swapped for recorders while the fuzz runs.
const mh::DataTable kTables[] = {
    {0x65BB48, 1},   // BreathBeam_Phases, by +1
    {0x65BB4C, 5},   // BreathBeam_Steps, by +2
    {0x65BB60, 2},   // IcePool_Kinds, by +1
    {0x65BB68, 4},   // BreathMote_Steps, by +2
    {0x65BB80, 2},   // ThunderPool_Kinds, by +1
    {0x65BB88, 4},   // ThunderBolt_Steps, by +2
    {0x65BB98, 3},   // ThunderSpark_Steps, by +2
};

// --- the seed, the disturbance and the settle -----------------------------------------

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return mh::Pointer(at::kOwner); }
unsigned char Near(unsigned v) { return static_cast<unsigned char>(mh::Half() ? v : v + (mh::Next() % 3) - 1); }
unsigned char* RecordOf(std::uint32_t pool, unsigned i) { return mh::Mem(pool + i * 0x84); }

void SeedPool(std::uint32_t pool, unsigned n) {
    // every record's owner inside what the disturbance may write through
    for (unsigned i = 0; i < n; ++i) {
        const std::uint32_t v = mh::Next();
        SetLong(RecordOf(pool, i) + 0x80, static_cast<std::int32_t>(K(v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v >> 3))));
    }
}
void SeedAlloc(std::uint32_t pool, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        unsigned char* const rec = RecordOf(pool, i);
        rec[0] = static_cast<unsigned char>(mh::Next() % 6 ? rec[0] | 1 : rec[0] & ~1u);
    }
    if (mh::Next() % 4 == 0) {
        // every record in use, or all but the last
        for (unsigned i = 0; i < n; ++i) RecordOf(pool, i)[0] |= 1;
        if (mh::Half()) RecordOf(pool, n - 1)[0] &= ~1u;
    }
}
// A screen coordinate at or either side of the bounds BreathMote_Fly tests.
void SetCoord(unsigned char* at, unsigned bound) {
    const std::uint32_t pick = mh::Next() % 8;
    static const int kNear[] = {-1, 0, 1};
    if (pick < 3) SetWord(at, static_cast<unsigned>(kNear[pick]));
    else if (pick < 6) SetWord(at, bound + kNear[pick - 3]);
    else if (pick == 6) SetWord(at, 1 + mh::Next() % (bound - 1));
}

void Seed(unsigned k) {
    g_k = k;
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    SeedPool(kMotePool, kMotes);
    SeedPool(kBoltPool, kBolts);
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kFirebreath_Task: case kIcebreath_Task: case kIcePool_Dispatch: case kThunderPool_Dispatch:
        sc[1] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kThunderbreath_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 4); break;
    case kBreathBeam_Task: sc[1] = 0; break;
    case kBreathBeam_Run:
        sc[2] = static_cast<unsigned char>(mh::Next() % 5);
        if (mh::Next() % 4 == 0) sc[2] = 0;
        if (mh::Half()) Owner()[0xB] = 0xFF;
        break;
    case kBreathMote_Task: case kThunderBolt_Task:
        sc[2] = static_cast<unsigned char>(mh::Next() % 4);
        if (k == kThunderBolt_Task) {
            if (mh::Next() % 4 == 0) sc[0] = 0;
            if (mh::Next() % 4 == 0) sc[2] = 1;
            if (mh::Half()) sc[0xB] = static_cast<unsigned char>(3 + 4 * (mh::Next() % 8) + (mh::Half() ? 0 : 1));
            if (mh::Next() % 4 == 0) sc[0xA] = 0;
        }
        break;
    case kThunderSpark_Task:
        sc[2] = static_cast<unsigned char>(mh::Next() % 3);
        if (mh::Next() % 4 == 0) sc[0] = 0;
        break;
    // the count-downs and count-ups: at, and either side of, their ends
    case kBreathBeam_Aim: case kBreathMote_Launch: case kThunderBolt_Aim:
        sc[9] = Near(1);
        sc[8] = static_cast<unsigned char>(mh::Next() % 5);
        break;
    case kIcebreath_Start: sc[8] = static_cast<unsigned char>(mh::Next() % 5); break;
    case kBreathBeam_Widen: sc[9] = Near(7); break;
    case kBreathBeam_Hit: sc[9] = Near(0x17); break;
    case kBreathBeam_Hold: sc[9] = Near(0x77); break;
    case kBreathBeam_Fade: sc[9] = Near(0x87); break;
    case kBreathMote_Burst: sc[0xA] = Near(1); break;
    case kBreathMote_Fly: {
        sc[3] = static_cast<unsigned char>(mh::Next() % 8);
        sc[0xA] = Near(sc[3] + 6u);
        sc[9] = Near(1);
        SetCoord(sc + 0x2E, 0x140);
        SetCoord(sc + 0x30, 0xF0);
        break;
    }
    case kThunderbreath_Wait: sc[0xB] = Near(2); sc[0xA] = Near(1); break;
    case kThunderBolt_Grow: sc[0xA] = Near(0xC); break;
    case kThunderBolt_Flicker: SetLong(sc + 0xC, Near(0xF)); break;
    case kThunderBolt_Fade: sc[9] = static_cast<unsigned char>(1 + mh::Next() % 3); break;
    case kThunderSpark_Grow: sc[9] = Near(0xC); break;
    case kThunderSpark_Fade: sc[9] = Near(4); break;
    case kIcePool_Alloc: SeedAlloc(kMotePool, kMotes); break;
    case kThunderPool_Alloc: SeedAlloc(kBoltPool, kBolts); break;
    // the draws: their loops short enough for the log, their divisors not 0
    case kBreathBeam_DrawTextured: case kBreathBeam_DrawGlow:
        sc[0xA] = static_cast<unsigned char>(mh::Next() % 5);
        sc[0xB] = static_cast<unsigned char>(1 + mh::Next() % 0x11);
        break;
    case kThunderBolt_Draw:
        sc[0xA] = static_cast<unsigned char>(mh::Half() ? mh::Next() % 5 : 7 + mh::Next() % 3);
        break;
    default: break;
    }
}

// What the draws divide by after a call, put back after every disturbance:
// the beam's +0xB and the bolt's +0xA (also its loop bound), in the current
// task and every slot the disturbance may make current.
void Settle() {
    const bool beam = g_k == kBreathBeam_DrawTextured || g_k == kBreathBeam_DrawGlow;
    const bool bolt = g_k == kThunderBolt_Draw;
    if (!beam && !bolt) return;
    for (unsigned t = 0; t < 5; ++t) {
        unsigned char* const s = t < 4 ? mh::TaskAt(t) : Sc();
        if (beam) {
            if (s[0xB] == 0) s[0xB] = 1;
            if (s[0xA] > 5) s[0xA] = static_cast<unsigned char>(s[0xA] % 5);
        } else if (s[0xA] == 0 || s[0xA] > 9) {
            s[0xA] = static_cast<unsigned char>(1 + s[0xA] % 9);
        }
    }
}

// The cells ours might keep where the originals read again: the primitive
// cursor, a scratch word, a vertex word, an angle byte, a byte of a pool
// record.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 6) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: case 2: SetWord(mh::Mem(kScratch + 2 * (v % 8)), h >> 20); break;
    case 3: SetWord(mh::Mem(kVertices + 2 * (v % 2)), h >> 20); break;
    case 4: mh::Mem(kAngles + v % 8)[0] = static_cast<unsigned char>(h >> 24); break;
    default: {
        const bool motes = (h >> 24) & 1;
        unsigned char* const rec = RecordOf(motes ? kMotePool : kBoltPool, v % (motes ? kMotes : kBolts));
        rec[(h >> 25) % 0x14] = static_cast<unsigned char>(h >> 12);
        break;
    }
    }
}

}  // namespace

void SelfTest() {
    for (unsigned i = 0; i < sizeof kRegions / sizeof kRegions[0]; ++i) g_regions[i] = kRegions[i];
    g_regions[sizeof kRegions / sizeof kRegions[0] - 1].at = K(g_prims);
    mh::Group group = {
        "magic_s28", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed, &Disturb, 2000,
    };
    group.settle = &Settle;
    mh::Run(group);
}

}  // namespace magic_s28
