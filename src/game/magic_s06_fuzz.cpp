// BOF3X_SHADOW=magic_s06: group S06's two overlays (MAGIC008, MAGIC020)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s06.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC008 / MAGIC020 --clones
// (2026-09-26; capstone, every jump internal, no jump table, no REFUSED line),
// names given, and five immediates added by hand: the two blows' stack tables
// (0x4A2DC0, 0x4A2F40) load three of their handlers through a register
// (`mov ecx, 0x4A2FB0` / `mov eax, ..` / `mov edx, ..`, then stored to the
// stack), which the tool does not list. Beyond the standard set this group
// lists the draw callees, the sprite calls, Battle_ActorIsOut,
// MagicFx_NearSprite, the engine's 0x446770 and 0x435A70, and its own three
// functions called directly. Everything the harness lacks is built here, not
// in the harness:
//
//   - the draws: Gfx_CommitPrim logs each primitive's bytes (both quads of a
//     flash are built in the same buffer) and moves Gfx_PacketNext on through
//     a packet buffer of the fuzz's own;
//   - the sprite calls that act on Sprite_Current (the script ticks, the
//     animation, the screen updates) log which sprite; Sprite_QueueOverlay
//     logs the frame-offset table 0x9039D8 the slashes swap round it;
//   - Battle_ActorIsOut answers from its own stream and a quarter of the time
//     moves the target (the harness's kFlag blind spot, group E's fix): the
//     blows' reaction re-reads the target after a "not out";
//   - MagicFx_NearSprite answers a whole-eax bool (its callers test eax);
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s06.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s06 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC008 / MAGIC020 --clones, 2026-09-26, names
// given; the two blows' register-loaded handlers added (offsets 4, 9, 0x31).
// 0x4A2190: 0x2E bytes  Magic008_Task
constexpr mh::Imm kImms4A2190[] = {{0xF, 0x4A21C0}, {0x17, 0x4A2320}, {0x22, 0x4E5200}};
// 0x4A21C0: 0x156 bytes  Magic008_Start
constexpr mh::CallSite kCalls4A21C0[] = {{0x47, 0x4A2440}, {0x6B, 0x4FB830}, {0x74, 0x435180}};
// 0x4A2320: 0x113 bytes  Magic008_Apply
constexpr mh::CallSite kCalls4A2320[] = {{0x58, 0x4FB6F0}, {0x68, 0x435180}, {0xAD, 0x435180}, {0xF5, 0x4FB830}};
// 0x4A2440: 0x15 bytes  Magic008_BuffStat
// 0x4A2460: 0x12 bytes; jmp through .data 0x65A6A0 (Magic008Double_Bodies)  Magic008Double_Task
// 0x4A2480: 0x56 bytes  Magic008Grow_Run
constexpr mh::CallSite kCalls4A2480[] = {{0x4D, 0x588F20}};
constexpr mh::Imm kImms4A2480[] = {{0xF, 0x4A24E0}, {0x17, 0x4A2520}, {0x22, 0x4A2580}, {0x2A, 0x4A25C0}, {0x32, 0x4A25F0}, {0x3A, 0x4AEE90}};
// 0x4A24E0: 0x33 bytes  Magic008Grow_Start
// 0x4A2520: 0x52 bytes  Magic008Grow_Grow
constexpr mh::CallSite kCalls4A2520[] = {{0x3B, 0x4FC1F0}};
// 0x4A2580: 0x3F bytes  Magic008Grow_Strike
constexpr mh::CallSite kCalls4A2580[] = {{0x0, 0x589410}, {0x25, 0x4530D0}, {0x2E, 0x4FC030}};
// 0x4A25C0: 0x28 bytes  Magic008Grow_Wait
constexpr mh::CallSite kCalls4A25C0[] = {{0x0, 0x589410}, {0xE, 0x587900}};
// 0x4A25F0: 0x4E bytes  Magic008Grow_Shrink
// 0x4A2640: 0x4E bytes  Magic008Glow_Run
constexpr mh::CallSite kCalls4A2640[] = {{0x45, 0x588F20}};
constexpr mh::Imm kImms4A2640[] = {{0xF, 0x4A2690}, {0x17, 0x4A2750}, {0x22, 0x4A27B0}, {0x2A, 0x4A2800}, {0x32, 0x4A2830}};
// 0x4A2690: 0xBE bytes  Magic008Glow_Start
constexpr mh::CallSite kCalls4A2690[] = {{0xA4, 0x587900}};
// 0x4A2750: 0x59 bytes  Magic008Glow_Brighten
constexpr mh::CallSite kCalls4A2750[] = {{0x42, 0x4FC1F0}};
// 0x4A27B0: 0x49 bytes  Magic008Glow_Strike
constexpr mh::CallSite kCalls4A27B0[] = {{0x0, 0x589410}, {0x25, 0x4530D0}, {0x2E, 0x4FC030}};
// 0x4A2800: 0x28 bytes  Magic008Glow_Wait
constexpr mh::CallSite kCalls4A2800[] = {{0x0, 0x589410}, {0xE, 0x587900}};
// 0x4A2830: 0x52 bytes  Magic008Glow_Fade
constexpr mh::CallSite kCalls4A2830[] = {{0x4C, 0x4351F0}};
// 0x4A2890: 0x36 bytes  Magic008Mirror_Run
constexpr mh::Imm kImms4A2890[] = {{0xF, 0x4A28D0}, {0x17, 0x4A28F0}, {0x22, 0x4A2950}, {0x2A, 0x4A2990}};
// 0x4A28D0: 0x1B bytes  Magic008Mirror_Size
constexpr mh::CallSite kCalls4A28D0[] = {{0x0, 0x4FC1F0}, {0x16, 0x588F20}};
// 0x4A28F0: 0x58 bytes  Magic008Mirror_Strike
constexpr mh::CallSite kCalls4A28F0[] = {{0x19, 0x589410}, {0x22, 0x4FC030}, {0x2C, 0x589410}, {0x39, 0x4FC030}, {0x53, 0x588F20}};
// 0x4A2950: 0x31 bytes  Magic008Mirror_Hit
constexpr mh::CallSite kCalls4A2950[] = {{0x0, 0x589410}, {0xF, 0x4530D0}, {0x2C, 0x588F20}};
// 0x4A2990: 0x2A bytes  Magic008Mirror_Flash
constexpr mh::CallSite kCalls4A2990[] = {{0x16, 0x4A29C0}, {0x25, 0x4351F0}};
// 0x4A29C0: 0x124 bytes  Magic008_DrawFlash
constexpr mh::CallSite kCalls4A29C0[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x33, 0x5A7610}, {0x3B, 0x5A7780}, {0x90, 0x461E50}, {0x9C, 0x5A7610}, {0xA4, 0x5A7780}, {0xF9, 0x461E50}, {0x10E, 0x5A77C0}, {0x117, 0x461E50}};
// 0x4A2AF0: 0x5C bytes  Magic008Dash_Run
constexpr mh::CallSite kCalls4A2AF0[] = {{0x53, 0x588F20}};
constexpr mh::Imm kImms4A2AF0[] = {{0xF, 0x4A2B50}, {0x17, 0x4A2C40}, {0x22, 0x4A2CD0}, {0x2A, 0x4A01C0}, {0x32, 0x4A2D70}, {0x3A, 0x4AEE90}};
// 0x4A2B50: 0xE5 bytes  Magic008Dash_Start
constexpr mh::CallSite kCalls4A2B50[] = {{0xC6, 0x446770}, {0xCE, 0x4FC1F0}};
// 0x4A2C40: 0x86 bytes  Magic008Dash_Follow
constexpr mh::CallSite kCalls4A2C40[] = {{0x2B, 0x589410}};
// 0x4A2CD0: 0x95 bytes  Magic008Dash_Strike
constexpr mh::CallSite kCalls4A2CD0[] = {{0x4F, 0x4FC030}, {0x5A, 0x4530D0}};
// 0x4A2D70: 0x4A bytes  Magic008Dash_Fade
// 0x4A2DC0: 0x58 bytes  Magic008TwoBlows_Run
constexpr mh::CallSite kCalls4A2DC0[] = {{0x4F, 0x588F20}};
constexpr mh::Imm kImms4A2DC0[] = {{0x4, 0x4A2FB0}, {0x9, 0x4A2E20}, {0x2C, 0x4ED5C0}, {0x34, 0x4A2E50}, {0x3C, 0x4AEE90}};
// 0x4A2E20: 0x26 bytes  Magic008Blow_WaitTwo
constexpr mh::CallSite kCalls4A2E20[] = {{0x0, 0x589410}};
// 0x4A2E50: 0xEE bytes  Magic008Blow_ReactTwo
constexpr mh::CallSite kCalls4A2E50[] = {{0x6, 0x4456C0}, {0x5B, 0x5891F0}, {0xA2, 0x435A70}, {0xAA, 0x4FC1F0}};
// 0x4A2F40: 0x65 bytes  Magic008ThreeBlows_Run
constexpr mh::CallSite kCalls4A2F40[] = {{0x5C, 0x588F20}};
constexpr mh::Imm kImms4A2F40[] = {{0x4, 0x4A2FB0}, {0x9, 0x4A2FF0}, {0x31, 0x4A3020}, {0x39, 0x4ED5C0}, {0x49, 0x4AEE90}};
// 0x4A2FB0: 0x3F bytes  Magic008Blow_Strike
constexpr mh::CallSite kCalls4A2FB0[] = {{0x0, 0x589410}, {0x22, 0x4FC030}, {0x2E, 0x4530D0}};
// 0x4A2FF0: 0x26 bytes  Magic008Blow_WaitThree
constexpr mh::CallSite kCalls4A2FF0[] = {{0x0, 0x589410}};
// 0x4A3020: 0x121 bytes  Magic008Blow_ReactThree
constexpr mh::CallSite kCalls4A3020[] = {{0x6, 0x4456C0}, {0x5B, 0x5891F0}, {0xA2, 0x435A70}, {0xAA, 0x4FC1F0}, {0x104, 0x5B93D2}};
// 0x4A3150: 0x46 bytes  Magic020_Task
constexpr mh::Imm kImms4A3150[] = {{0xF, 0x4A31A0}, {0x17, 0x4A33E0}, {0x22, 0x4A34B0}, {0x2A, 0x4A34E0}, {0x32, 0x43FE80}, {0x3A, 0x43F460}};
// 0x4A31A0: 0x23E bytes  Magic020_Start
constexpr mh::CallSite kCalls4A31A0[] = {{0x67, 0x4FB830}, {0x75, 0x435180}, {0x137, 0x435180}, {0x1DB, 0x4FBF50}, {0x214, 0x4FBE30}, {0x22C, 0x4FC030}};
// 0x4A33E0: 0xC7 bytes  Magic020_Darken
constexpr mh::CallSite kCalls4A33E0[] = {{0x12, 0x4A3A70}, {0x3C, 0x587900}, {0x61, 0x435180}};
// 0x4A34B0: 0x29 bytes  Magic020_Hold
constexpr mh::CallSite kCalls4A34B0[] = {{0x0, 0x4A3A70}};
// 0x4A34E0: 0x2D bytes  Magic020_End
constexpr mh::CallSite kCalls4A34E0[] = {{0xB, 0x4FC000}, {0x1C, 0x4FB830}};
// 0x4A3510: 0x12 bytes; jmp through .data 0x65A6D0 (Magic020Child_Bodies)  Magic020Child_Task
// 0x4A3530: 0x6C bytes  Magic020Lead_Run
constexpr mh::CallSite kCalls4A3530[] = {{0x63, 0x588F20}};
constexpr mh::Imm kImms4A3530[] = {{0xF, 0x4A35A0}, {0x17, 0x4A3600}, {0x22, 0x4A3650}, {0x2A, 0x4A3680}, {0x32, 0x4A36B0}, {0x3A, 0x4A3700}, {0x42, 0x4A3770}, {0x4A, 0x4AEE90}};
// 0x4A35A0: 0x60 bytes  Magic020Lead_Start
// 0x4A3600: 0x45 bytes  Magic020Lead_Approach
constexpr mh::CallSite kCalls4A3600[] = {{0x8, 0x4FB9F0}, {0x19, 0x4FBC30}, {0x25, 0x4FC1F0}};
// 0x4A3650: 0x22 bytes  Magic020Lead_Tick
constexpr mh::CallSite kCalls4A3650[] = {{0x0, 0x589410}};
// 0x4A3680: 0x28 bytes  Magic020Lead_WaitSlashes
constexpr mh::CallSite kCalls4A3680[] = {{0xC, 0x589410}};
// 0x4A36B0: 0x4F bytes  Magic020Lead_Flash
// 0x4A3700: 0x6B bytes  Magic020Lead_Return
// 0x4A3770: 0x48 bytes  Magic020Lead_Brighten
// 0x4A37C0: 0x44 bytes  Magic020Shadow_Run
constexpr mh::CallSite kCalls4A37C0[] = {{0x3B, 0x588F20}};
constexpr mh::Imm kImms4A37C0[] = {{0xF, 0x4A3810}, {0x17, 0x4A38D0}, {0x22, 0x4AEE90}};
// 0x4A3810: 0xBF bytes  Magic020Shadow_Start
// 0x4A38D0: 0x2E bytes  Magic020Shadow_Approach
constexpr mh::CallSite kCalls4A38D0[] = {{0x8, 0x4FB9F0}, {0x19, 0x4FBC30}};
// 0x4A3900: 0x50 bytes  Magic020Slash_Run
constexpr mh::CallSite kCalls4A3900[] = {{0x3D, 0x5890E0}};
constexpr mh::Imm kImms4A3900[] = {{0x19, 0x4A3950}, {0x24, 0x4A3A30}};
// 0x4A3950: 0xE0 bytes  Magic020Slash_Start
constexpr mh::CallSite kCalls4A3950[] = {{0x4E, 0x4FBD10}, {0xCD, 0x5891F0}};
// 0x4A3A30: 0x31 bytes  Magic020Slash_Play
constexpr mh::CallSite kCalls4A3A30[] = {{0x0, 0x5893A0}, {0x1B, 0x4530D0}, {0x2B, 0x4351F0}};
// 0x4A3A70: 0xA9 bytes  Magic020_DrawFade
constexpr mh::CallSite kCalls4A3A70[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x24, 0x5A7740}, {0x2C, 0x5A7780}, {0x80, 0x461E50}, {0x93, 0x5A77C0}, {0x9F, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Magic008_Task", 0x4A2190, 0x2E, nullptr, 0, kImms4A2190, MH_N(kImms4A2190), nullptr, 0, reinterpret_cast<const void*>(&::Magic008_Task)},
    {"Magic008_Start", 0x4A21C0, 0x156, kCalls4A21C0, MH_N(kCalls4A21C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008_Start)},
    {"Magic008_Apply", 0x4A2320, 0x113, kCalls4A2320, MH_N(kCalls4A2320), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008_Apply)},
    {"Magic008_BuffStat", 0x4A2440, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008_BuffStat), 0xFF},
    {"Magic008Double_Task", 0x4A2460, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Double_Task)},
    {"Magic008Grow_Run", 0x4A2480, 0x56, kCalls4A2480, MH_N(kCalls4A2480), kImms4A2480, MH_N(kImms4A2480), nullptr, 0, reinterpret_cast<const void*>(&::Magic008Grow_Run)},
    {"Magic008Grow_Start", 0x4A24E0, 0x33, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Grow_Start)},
    {"Magic008Grow_Grow", 0x4A2520, 0x52, kCalls4A2520, MH_N(kCalls4A2520), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Grow_Grow)},
    {"Magic008Grow_Strike", 0x4A2580, 0x3F, kCalls4A2580, MH_N(kCalls4A2580), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Grow_Strike)},
    {"Magic008Grow_Wait", 0x4A25C0, 0x28, kCalls4A25C0, MH_N(kCalls4A25C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Grow_Wait)},
    {"Magic008Grow_Shrink", 0x4A25F0, 0x4E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Grow_Shrink)},
    {"Magic008Glow_Run", 0x4A2640, 0x4E, kCalls4A2640, MH_N(kCalls4A2640), kImms4A2640, MH_N(kImms4A2640), nullptr, 0, reinterpret_cast<const void*>(&::Magic008Glow_Run)},
    {"Magic008Glow_Start", 0x4A2690, 0xBE, kCalls4A2690, MH_N(kCalls4A2690), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Glow_Start)},
    {"Magic008Glow_Brighten", 0x4A2750, 0x59, kCalls4A2750, MH_N(kCalls4A2750), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Glow_Brighten)},
    {"Magic008Glow_Strike", 0x4A27B0, 0x49, kCalls4A27B0, MH_N(kCalls4A27B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Glow_Strike)},
    {"Magic008Glow_Wait", 0x4A2800, 0x28, kCalls4A2800, MH_N(kCalls4A2800), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Glow_Wait)},
    {"Magic008Glow_Fade", 0x4A2830, 0x52, kCalls4A2830, MH_N(kCalls4A2830), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Glow_Fade)},
    {"Magic008Mirror_Run", 0x4A2890, 0x36, nullptr, 0, kImms4A2890, MH_N(kImms4A2890), nullptr, 0, reinterpret_cast<const void*>(&::Magic008Mirror_Run)},
    {"Magic008Mirror_Size", 0x4A28D0, 0x1B, kCalls4A28D0, MH_N(kCalls4A28D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Mirror_Size)},
    {"Magic008Mirror_Strike", 0x4A28F0, 0x58, kCalls4A28F0, MH_N(kCalls4A28F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Mirror_Strike)},
    {"Magic008Mirror_Hit", 0x4A2950, 0x31, kCalls4A2950, MH_N(kCalls4A2950), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Mirror_Hit)},
    {"Magic008Mirror_Flash", 0x4A2990, 0x2A, kCalls4A2990, MH_N(kCalls4A2990), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Mirror_Flash)},
    {"Magic008_DrawFlash", 0x4A29C0, 0x124, kCalls4A29C0, MH_N(kCalls4A29C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008_DrawFlash)},
    {"Magic008Dash_Run", 0x4A2AF0, 0x5C, kCalls4A2AF0, MH_N(kCalls4A2AF0), kImms4A2AF0, MH_N(kImms4A2AF0), nullptr, 0, reinterpret_cast<const void*>(&::Magic008Dash_Run)},
    {"Magic008Dash_Start", 0x4A2B50, 0xE5, kCalls4A2B50, MH_N(kCalls4A2B50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Dash_Start)},
    {"Magic008Dash_Follow", 0x4A2C40, 0x86, kCalls4A2C40, MH_N(kCalls4A2C40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Dash_Follow)},
    {"Magic008Dash_Strike", 0x4A2CD0, 0x95, kCalls4A2CD0, MH_N(kCalls4A2CD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Dash_Strike)},
    {"Magic008Dash_Fade", 0x4A2D70, 0x4A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Dash_Fade)},
    {"Magic008TwoBlows_Run", 0x4A2DC0, 0x58, kCalls4A2DC0, MH_N(kCalls4A2DC0), kImms4A2DC0, MH_N(kImms4A2DC0), nullptr, 0, reinterpret_cast<const void*>(&::Magic008TwoBlows_Run)},
    {"Magic008Blow_WaitTwo", 0x4A2E20, 0x26, kCalls4A2E20, MH_N(kCalls4A2E20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Blow_WaitTwo)},
    {"Magic008Blow_ReactTwo", 0x4A2E50, 0xEE, kCalls4A2E50, MH_N(kCalls4A2E50), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Blow_ReactTwo)},
    {"Magic008ThreeBlows_Run", 0x4A2F40, 0x65, kCalls4A2F40, MH_N(kCalls4A2F40), kImms4A2F40, MH_N(kImms4A2F40), nullptr, 0, reinterpret_cast<const void*>(&::Magic008ThreeBlows_Run)},
    {"Magic008Blow_Strike", 0x4A2FB0, 0x3F, kCalls4A2FB0, MH_N(kCalls4A2FB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Blow_Strike)},
    {"Magic008Blow_WaitThree", 0x4A2FF0, 0x26, kCalls4A2FF0, MH_N(kCalls4A2FF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Blow_WaitThree)},
    {"Magic008Blow_ReactThree", 0x4A3020, 0x121, kCalls4A3020, MH_N(kCalls4A3020), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic008Blow_ReactThree)},
    {"Magic020_Task", 0x4A3150, 0x46, nullptr, 0, kImms4A3150, MH_N(kImms4A3150), nullptr, 0, reinterpret_cast<const void*>(&::Magic020_Task)},
    {"Magic020_Start", 0x4A31A0, 0x23E, kCalls4A31A0, MH_N(kCalls4A31A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020_Start)},
    {"Magic020_Darken", 0x4A33E0, 0xC7, kCalls4A33E0, MH_N(kCalls4A33E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020_Darken)},
    {"Magic020_Hold", 0x4A34B0, 0x29, kCalls4A34B0, MH_N(kCalls4A34B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020_Hold)},
    {"Magic020_End", 0x4A34E0, 0x2D, kCalls4A34E0, MH_N(kCalls4A34E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020_End)},
    {"Magic020Child_Task", 0x4A3510, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Child_Task)},
    {"Magic020Lead_Run", 0x4A3530, 0x6C, kCalls4A3530, MH_N(kCalls4A3530), kImms4A3530, MH_N(kImms4A3530), nullptr, 0, reinterpret_cast<const void*>(&::Magic020Lead_Run)},
    {"Magic020Lead_Start", 0x4A35A0, 0x60, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Lead_Start)},
    {"Magic020Lead_Approach", 0x4A3600, 0x45, kCalls4A3600, MH_N(kCalls4A3600), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Lead_Approach)},
    {"Magic020Lead_Tick", 0x4A3650, 0x22, kCalls4A3650, MH_N(kCalls4A3650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Lead_Tick)},
    {"Magic020Lead_WaitSlashes", 0x4A3680, 0x28, kCalls4A3680, MH_N(kCalls4A3680), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Lead_WaitSlashes)},
    {"Magic020Lead_Flash", 0x4A36B0, 0x4F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Lead_Flash)},
    {"Magic020Lead_Return", 0x4A3700, 0x6B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Lead_Return)},
    {"Magic020Lead_Brighten", 0x4A3770, 0x48, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Lead_Brighten)},
    {"Magic020Shadow_Run", 0x4A37C0, 0x44, kCalls4A37C0, MH_N(kCalls4A37C0), kImms4A37C0, MH_N(kImms4A37C0), nullptr, 0, reinterpret_cast<const void*>(&::Magic020Shadow_Run)},
    {"Magic020Shadow_Start", 0x4A3810, 0xBF, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Shadow_Start)},
    {"Magic020Shadow_Approach", 0x4A38D0, 0x2E, kCalls4A38D0, MH_N(kCalls4A38D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Shadow_Approach)},
    {"Magic020Slash_Run", 0x4A3900, 0x50, kCalls4A3900, MH_N(kCalls4A3900), kImms4A3900, MH_N(kImms4A3900), nullptr, 0, reinterpret_cast<const void*>(&::Magic020Slash_Run)},
    {"Magic020Slash_Start", 0x4A3950, 0xE0, kCalls4A3950, MH_N(kCalls4A3950), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Slash_Start)},
    {"Magic020Slash_Play", 0x4A3A30, 0x31, kCalls4A3A30, MH_N(kCalls4A3A30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020Slash_Play)},
    {"Magic020_DrawFade", 0x4A3A70, 0xA9, kCalls4A3A70, MH_N(kCalls4A3A70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic020_DrawFade)},
};
#undef MH_N

enum : unsigned {
    kMagic008_Task, kMagic008_Start, kMagic008_Apply, kMagic008_BuffStat, kMagic008Double_Task, kMagic008Grow_Run,
    kMagic008Grow_Start, kMagic008Grow_Grow, kMagic008Grow_Strike, kMagic008Grow_Wait, kMagic008Grow_Shrink,
    kMagic008Glow_Run, kMagic008Glow_Start, kMagic008Glow_Brighten, kMagic008Glow_Strike, kMagic008Glow_Wait,
    kMagic008Glow_Fade, kMagic008Mirror_Run, kMagic008Mirror_Size, kMagic008Mirror_Strike, kMagic008Mirror_Hit,
    kMagic008Mirror_Flash, kMagic008_DrawFlash, kMagic008Dash_Run, kMagic008Dash_Start, kMagic008Dash_Follow,
    kMagic008Dash_Strike, kMagic008Dash_Fade, kMagic008TwoBlows_Run, kMagic008Blow_WaitTwo, kMagic008Blow_ReactTwo,
    kMagic008ThreeBlows_Run, kMagic008Blow_Strike, kMagic008Blow_WaitThree, kMagic008Blow_ReactThree,
    kMagic020_Task, kMagic020_Start, kMagic020_Darken, kMagic020_Hold, kMagic020_End, kMagic020Child_Task,
    kMagic020Lead_Run, kMagic020Lead_Start, kMagic020Lead_Approach, kMagic020Lead_Tick, kMagic020Lead_WaitSlashes,
    kMagic020Lead_Flash, kMagic020Lead_Return, kMagic020Lead_Brighten, kMagic020Shadow_Run, kMagic020Shadow_Start,
    kMagic020Shadow_Approach, kMagic020Slash_Run, kMagic020Slash_Start, kMagic020Slash_Play, kMagic020_DrawFade,
    kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the fuzz's own memory ------------------------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The packet buffer Gfx_PacketNext points into while the fuzz runs.
constexpr unsigned kPrimBytes = 0x1000;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + 4 * (k % 64); }

constexpr std::uint32_t kAbility = 0x904B80, kFrameSet = 0x9039D8, kKind2 = 0x905E60, kQueueCount = 0x9035A0;

unsigned g_k = kCount;   // the function being fuzzed (the seed sets it)

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim: the primitive at Gfx_PacketNext into the log (the real one
// links it), then Gfx_PacketNext on by its size, kept in the buffer.
std::uint32_t CommitEffect(const std::uint32_t* a, std::uint32_t answer) {
    const unsigned size = a[1] & 0xFF;
    mh::NoteBytes(Gfx_PacketNext, size);
    unsigned char* p = Gfx_PacketNext + size;
    if (p < g_prims || p + 0x100 > g_prims + kPrimBytes) p = g_prims + (size & 0x3C);
    Gfx_PacketNext = p;
    return answer;
}
// The sprite calls that act on Sprite_Current: which sprite.
std::uint32_t NoteSprite(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current));
    return answer;
}
// Sprite_QueueOverlay: which sprite and the frame-offset table.
std::uint32_t NoteSpriteFrames(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
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
// Battle_ActorIsOut: a kFlag recorder answers 0 exactly when its own
// disturbance did nothing, so the reaction's re-read of the target after a
// "not out" would never see a moved cell. This answers from its own stream,
// and a quarter of the time moves the target to any of the eleven actors
// (group E's magic_engine_fuzz.cpp).
std::uint32_t OutAnswer(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t h = mh::Noise();
    if (h % 4 == 0) mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>((h >> 8) % 11);
    return (h >> 4) % 3 == 0 ? answer | 0x10 : answer & 0xFFFFFF00u;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S06_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S06_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S06_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S06_OURS(Sprite_ScriptTickOnce), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S06_OURS(BattleActor_UpdateScreenXY), 0, {}, kG, 0, 0, {}, &NoteSprite},
    {S06_OURS(MagicFx_NearSprite), 2, {kAll, kAll}, mh::Answer::kBool, 0, 0},
    // the sprite calls
    {S06_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0, {}, &NoteSprite},
    {S06_OURS(Sprite_SetAnimation), 1, {kU8}, kG, 0, 0, {}, &NoteSprite},
    {S06_OURS(Sprite_QueueOverlay), 0, {}, kG, 0, 0, {}, &NoteSpriteFrames},
    {S06_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &OutAnswer},
    // the draw library (all ours)
    {S06_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S06_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S06_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S06_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S06_OURS(Gpu_SetTile), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the dx / dz turn by direction; an enemy's animation
    {S06_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    {S06_RAW(0x435A70), 2, {kU8, kAll}, kG, 0, 0},
    // this group's own, called directly
    {S06_RAW(0x4A2440), 0, {}, kG, 0, 0},
    {S06_RAW(0x4A29C0), 0, {}, kG, 0, 0},
    {S06_RAW(0x4A3A70), 0, {}, kG, 0, 0},
};
#undef S06_OURS
#undef S06_RAW

// The two .data handler tables the dispatchers jump through
// (Magic008Double_Bodies, Magic020Child_Bodies).
const mh::DataTable kTables[] = {{0x65A6A0, 6}, {0x65A6D0, 3}};

mh::Region g_regions[] = {
    {0x7E0670, 4},                        // Gfx_PacketNext
    {0, kPrimBytes},                      // g_prims (filled in at start-up)
    {0x80E980, 0x40},                     // Gfx_ClutStripSource words 0x1A00..0x1A1F
    {0x812980, 0x40},                     // Gfx_ClutStrip words 0x1A00..0x1A1F
    {kKind2, 8},                          // Field_Kind2Z, Field_Kind2X
    {kFrameSet, 4},                       // the frame-offset table pointer the slashes swap
    {kAbility, 2},                        // the ability being cast
    {kQueueCount, 1},                     // Gfx_UploadQueueCount
    {0x903680, 0x228},                    // Gfx_UploadQueueX / _Y by any count; 0x903852 inside
    {0x92BF20, 0x400},                    // Gfx_UploadQueueRecord by any count
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return mh::Pointer(mh::at::kOwner); }

// The group's cells a recorder may move (the harness's case 14), from the
// hash only.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    switch ((h >> 8) % 6) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetLong(mh::Mem(kKind2 + 4 * (v & 1)), static_cast<std::int32_t>(h)); break;
    case 2: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 3: {
        static const unsigned kIds[] = {7, 0xA4, 0xB};
        SetWord(mh::Mem(kAbility), (v & 3) < 3 ? kIds[v & 3] : h >> 16);
        break;
    }
    case 4: mh::Mem(kQueueCount)[0] = Byte(h >> 24); break;
    case 5: SetWord(mh::Mem(0x903852), h >> 16); break;
    default: break;
    }
}

// Slash_Start reads +0xB after a call as an index into its twelve animations:
// a disturbed +0xB is put back inside them (past them ours aborts).
void Settle() {
    if (g_k == kMagic020Slash_Start) Sc()[0xB] %= 12;
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}
void Ability(unsigned id) {
    if (mh::Half()) SetWord(mh::Mem(kAbility), id + (mh::Often() ? 0u : mh::Next() % 3 - 1));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    g_k = k;
    Gfx_PacketNext = PrimAt(mh::Next());
    if (mh::Often()) mh::Mem(kQueueCount)[0] = Byte(1 + mh::Next() % 20);
    if (mh::Half()) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kMagic008_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kMagic008Double_Task: sc[1] = Byte(mh::Next() % 6); break;
    case kMagic008Grow_Run: case kMagic008Dash_Run: sc[2] = Byte(mh::Next() % 6); break;
    case kMagic008Glow_Run: sc[2] = Byte(mh::Next() % 5); break;
    case kMagic008Mirror_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kMagic008TwoBlows_Run: sc[2] = Byte(mh::Next() % 7); break;
    case kMagic008ThreeBlows_Run: sc[2] = Byte(mh::Next() % 10); break;
    case kMagic020_Task: sc[1] = Byte(mh::Next() % 6); break;
    case kMagic020Child_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kMagic020Lead_Run: sc[2] = Byte(mh::Next() % 8); break;
    case kMagic020Shadow_Run: sc[2] = Byte(mh::Next() % 3); break;
    case kMagic020Slash_Run: sc[2] = Byte(mh::Next() % 2); break;
    // the ability tests
    case kMagic008_Start: case kMagic008_BuffStat: Ability(7); break;
    case kMagic008Mirror_Flash:
        Ability(0xA4);
        sc[9] = Byte(mh::Often() ? 0xA + mh::Next() % 3 : mh::Next());
        break;
    case kMagic008_Apply:
        if (mh::Often()) sc[0xB] = 0;
        if (mh::Half()) sc[4] = mh::Half() ? 0xFF : Byte(mh::Next() % 4);
        break;
    // the counters: at their thresholds
    case kMagic008Grow_Grow: case kMagic008Grow_Strike: case kMagic008Grow_Shrink: case kMagic008Glow_Brighten:
    case kMagic008Glow_Strike: case kMagic008Glow_Fade: case kMagic008Mirror_Strike: case kMagic008Blow_Strike:
    case kMagic020Lead_Tick: case kMagic020Lead_Flash: case kMagic020Shadow_Start:
        Near(sc[9], 1);
        break;
    case kMagic008Dash_Follow: Near(sc[9], 1); break;
    case kMagic008Dash_Strike:
        Near(sc[9], 1);
        if (mh::Half()) sc[0xB] = 0;
        break;
    case kMagic008Dash_Start: sc[0xB] = Byte(mh::Next() % 3); break;
    case kMagic008Dash_Fade: if (mh::Half()) sc[0x5D] = Byte(0x82 + mh::Next() % 3); break;
    case kMagic020Lead_Return: if (mh::Half()) sc[0x5D] = Byte(0x87 + mh::Next() % 3); break;
    case kMagic020Lead_Brighten: if (mh::Half()) sc[0x5D] = Byte(0xEF + mh::Next() % 3); break;
    case kMagic008Blow_WaitTwo: if (mh::Half()) sc[2] = Byte(4 + mh::Next() % 3); break;
    case kMagic008Blow_WaitThree: if (mh::Half()) sc[2] = Byte(7 + mh::Next() % 3); break;
    // the reactions: each actor's state 6 half the time
    case kMagic008Blow_ReactTwo: case kMagic008Blow_ReactThree:
        for (unsigned char i = 0; i < 11; ++i) {
            unsigned char* const r = i < 3 ? mh::PartyOf(i) : mh::EnemyOf(i);
            if (mh::Half()) r[1] = 6;
        }
        if (k == kMagic008Blow_ReactThree) {
            Ability(0xB);
            // +2 at 9 (the cut's skip) half the time; a quarter of the time
            // 7..9, so that after React the test meets 8..10 (the re-run's
            // A120, docs/magic_s06.md section 6).
            if (mh::Half()) sc[2] = 9;
            else if (mh::Half()) sc[2] = Byte(7 + mh::Next() % 3);
            mh::SetRandHint(mh::Half() ? 0 : 8);
        }
        break;
    // MAGIC020's waits on +0xB
    case kMagic020_Darken:
        if (mh::Often()) sc[0xB] = 0xFF;
        if (mh::Often()) sc[9] = Byte(0xC + (mh::Half() ? 0u : 4u * (mh::Next() % 3) - 4u));
        break;
    case kMagic020_Hold:
        if (mh::Often()) sc[0xB] = 0;
        Near(sc[9], 1);
        break;
    case kMagic020_End: if (mh::Often()) sc[0xB] = 0xFF; break;
    case kMagic020Lead_WaitSlashes: if (mh::Often()) Owner()[0xB] = 0; break;
    case kMagic020Slash_Start:
        sc[0xB] = Byte(mh::Next() % 12);
        Near(sc[9], 1);
        if (mh::Half()) sc[8] = Byte(mh::Next() % 4);
        break;
    case kMagic020Slash_Play: if (mh::Half()) sc[0xB] = Byte(6 + mh::Next() % 3); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(g_prims));
    mh::Group group = {
        "magic_s06", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.settle = &Settle;
    mh::Run(group);
}

}  // namespace magic_s06
