// BOF3X_SHADOW=magic_s26: group S26's three overlays (MAGIC114, MAGIC115,
// MAGIC117) through the spell round's shared harness (magic_harness.h), once
// at start-up. docs/magic_s26.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC114 / 115 / 117 --clones
// (2026-09-26; capstone, every jump internal but Magic114_DrawTriangle's two
// jump tables, which the harness moves into the copy), names given. What the
// standard set does not cover, built here and not in the harness:
//
//   - the draws: every GTE callee logs what its vectors hold (`deref`), the
//     matrix pushes' GTE callees write a result where the real ones write (as
//     group S22's), and Gfx_CommitPrim / MapView_LinkPrimAt are stand-ins of
//     this file's that log the primitive's bytes before moving Gfx_PacketNext
//     on (every primitive of a loop is built at the same place otherwise, and
//     only the last would be compared);
//   - 0x446770 (the dx / dz turn by direction, Capcom's, in no group) is a
//     stand-in that logs the task and turns it for real, so a value read
//     before the turn where the original reads it after shows;
//   - Magic114_DrawTriangle, called by Magic114_DrawRing with a pointer into
//     its frame, is a stand-in that fills the eight depth keys, which
//     MagicFx_LinkByDepth then logs by their bytes;
//   - the sprite children's phases and Sprite_UpdateScreen log the
//     frame-offset table pointer 0x9039D8 the runners swap around them.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s26.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s26 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC114 / MAGIC115 / MAGIC117 --clones, 2026-09-26, names given.
constexpr mh::CallSite kCalls4D7960[] = {{0x49, 0x5A77C0}, {0x52, 0x461E50}, {0x6B, 0x4D7C10}, {0x8D, 0x5A77C0}, {0x96, 0x461E50}};
constexpr mh::Imm kImms4D7960[] = {{0x15, 0x4D7A00}, {0x1D, 0x4D7AA0}, {0x25, 0x4D7AD0}, {0x2D, 0x4D7B00}, {0x35, 0x4D7B70}, {0x3D, 0x4D7BA0}, {0x45, 0x4D7BD0}};
constexpr mh::CallSite kCalls4D7A00[] = {{0x37, 0x587900}};
constexpr mh::CallSite kCalls4D7B00[] = {{0x26, 0x587900}, {0x2F, 0x435180}};
constexpr mh::CallSite kCalls4D7B70[] = {{0x1D, 0x4D91A0}};
constexpr mh::CallSite kCalls4D7BD0[] = {{0x20, 0x4530D0}, {0x2F, 0x4351F0}};
constexpr mh::CallSite kCalls4D7C10[] = {{0x6, 0x4D8000}, {0x4D, 0x446770}, {0x9A, 0x446770}, {0xAD, 0x4D7D00}, {0xD8, 0x4FB880}, {0xE0, 0x5A7BC0}};
constexpr mh::CallSite kCalls4D7D00[] = {{0xC, 0x5A75F0}, {0xBF, 0x5A87A0}};
// The first table has 8 entries (`cmp ecx, 7; ja`): magic_rows.py printed 14,
// running on into the second (0x4D7FE4), which the move then relocated twice.
constexpr mh::JumpTable kTables4D7D00[] = {{0xF3, 0x2C4, 8}, {0x1E4, 0x2E4, 6}};
constexpr mh::CallSite kCalls4D8000[] = {{0x3, 0x5A7B90}, {0x7A, 0x5A8200}, {0x89, 0x5A8060}, {0x9D, 0x5A7D70}, {0xA7, 0x5A8DE0}, {0xB1, 0x5A8E00}};
constexpr mh::CallSite kCalls4D8100[] = {{0x0, 0x4FC0E0}, {0x3A, 0x446770}};
constexpr mh::CallSite kCalls4D81A0[] = {{0x2, 0x4D82B0}, {0xB, 0x435180}, {0x86, 0x435180}, {0x106, 0x4351F0}};
constexpr mh::CallSite kCalls4D82B0[] = {{0x31, 0x446770}, {0x67, 0x5A77C0}, {0x72, 0x572FA0}, {0x7E, 0x5A7610}, {0x85, 0x5A7780}, {0xAA, 0x446770}, {0x10C, 0x446770}, {0x17D, 0x446770}, {0x22D, 0x5A8950}, {0x261, 0x572FA0}};
constexpr mh::CallSite kCalls4D8530[] = {{0x23, 0x4D8DE0}, {0x32, 0x4D8A60}, {0x3A, 0x4D86A0}};
constexpr mh::CallSite kCalls4D8570[] = {{0x1E, 0x587900}};
constexpr mh::CallSite kCalls4D8600[] = {{0x21, 0x452F70}};
constexpr mh::CallSite kCalls4D8660[] = {{0x35, 0x4351F0}};
constexpr mh::CallSite kCalls4D86A0[] = {{0x1C, 0x5A77C0}, {0x25, 0x461E50}, {0x46, 0x446770}, {0xA6, 0x446770}, {0x118, 0x5A75D0}, {0x120, 0x5A7780}, {0x1A6, 0x446770}, {0x222, 0x446770}, {0x278, 0x5A79A0}, {0x288, 0x5A79E0}, {0x314, 0x5A8950}, {0x335, 0x5A7A00}, {0x391, 0x461E50}};
constexpr mh::CallSite kCalls4D8A60[] = {{0x2B, 0x5A77C0}, {0x34, 0x461E50}, {0x55, 0x446770}, {0xB5, 0x446770}, {0x125, 0x5A75D0}, {0x12C, 0x5A7780}, {0x1B2, 0x446770}, {0x22E, 0x446770}, {0x283, 0x5A79A0}, {0x293, 0x5A79E0}, {0x317, 0x5A8950}, {0x34F, 0x461E50}};
constexpr mh::CallSite kCalls4D8DE0[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x43, 0x446770}, {0x92, 0x446770}, {0x114, 0x5A7610}, {0x11B, 0x5A7780}, {0x18E, 0x446770}, {0x1F9, 0x446770}, {0x25E, 0x5A8950}, {0x2BD, 0x461E50}};
constexpr mh::CallSite kCalls4D9100[] = {{0x72, 0x5891F0}, {0x7A, 0x588F20}};
constexpr mh::CallSite kCalls4D9180[] = {{0x0, 0x589410}, {0x9, 0x4351F0}, {0xE, 0x588F20}};
constexpr mh::CallSite kCalls4D91A0[] = {{0x1B, 0x4D9320}, {0x64, 0x5A77C0}, {0x76, 0x572FA0}, {0x82, 0x5A7650}, {0x8A, 0x5A7780}, {0x90, 0x5A7A50}, {0xA5, 0x5A7A00}, {0xD5, 0x5A8250}, {0xE1, 0x5A9110}, {0xEA, 0x5A7A50}, {0xFF, 0x5A7A00}, {0x12F, 0x5A8250}, {0x138, 0x5A9110}, {0x157, 0x572FA0}, {0x16B, 0x5A7BC0}};
constexpr mh::CallSite kCalls4D9320[] = {{0x3, 0x5A7B90}, {0x4E, 0x446770}, {0xD8, 0x5A8200}, {0xE7, 0x5A8060}, {0xFB, 0x5A7D70}, {0x105, 0x5A8DE0}, {0x10F, 0x5A8E00}};
constexpr mh::Imm kImms4D9440[] = {{0xF, 0x4D9480}, {0x17, 0x4D9560}, {0x22, 0x4D95B0}, {0x2A, 0x4F7350}};
constexpr mh::CallSite kCalls4D9480[] = {{0x2F, 0x435180}, {0x60, 0x454DC0}, {0x6E, 0x454CC0}, {0xB6, 0x587900}};
constexpr mh::CallSite kCalls4D95B0[] = {{0x67, 0x454D60}};
constexpr mh::CallSite kCalls4D9650[] = {{0x2D, 0x588F20}};
constexpr mh::CallSite kCalls4D9690[] = {{0xD7, 0x5891F0}};
constexpr mh::CallSite kCalls4D9790[] = {{0x2, 0x5893A0}, {0x28, 0x5A7A00}, {0x50, 0x5A7A50}, {0x87, 0x435180}};
constexpr mh::CallSite kCalls4D98E0[] = {{0x24, 0x5893A0}, {0x4A, 0x5A7A00}, {0x72, 0x5A7A50}};
constexpr mh::CallSite kCalls4D9990[] = {{0x2D, 0x588F20}};
constexpr mh::CallSite kCalls4D99D0[] = {{0x9E, 0x5891F0}};
constexpr mh::CallSite kCalls4D9A80[] = {{0x0, 0x589410}, {0x5, 0x589410}};
constexpr mh::CallSite kCalls4D9AA0[] = {{0x24, 0x589410}};
constexpr mh::Imm kImms4D9F40[] = {{0xF, 0x4D9F80}, {0x17, 0x4D9FD0}, {0x22, 0x4DA040}, {0x2A, 0x43F460}};
constexpr mh::CallSite kCalls4D9F80[] = {{0x6, 0x454DC0}, {0x1A, 0x454CC0}};
constexpr mh::CallSite kCalls4DA040[] = {{0x7F, 0x454DC0}, {0x8B, 0x4FBDB0}};
constexpr mh::Imm kImms4DA0E0[] = {{0xF, 0x49DF30}, {0x17, 0x4DA120}, {0x22, 0x4DA190}, {0x2A, 0x43F460}};
constexpr mh::CallSite kCalls4DA190[] = {{0x68, 0x454DC0}, {0x74, 0x4FBDB0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Magic114_Task", 0x4D7960, 0x9F, kCalls4D7960, MH_N(kCalls4D7960), kImms4D7960, MH_N(kImms4D7960), nullptr, 0, reinterpret_cast<const void*>(&::Magic114_Task)},
    {"Magic114_Start", 0x4D7A00, 0x95, kCalls4D7A00, MH_N(kCalls4D7A00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_Start)},
    {"Magic114_Grow", 0x4D7AA0, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_Grow)},
    {"Magic114_Hold", 0x4D7AD0, 0x2A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_Hold)},
    {"Magic114_Launch", 0x4D7B00, 0x6D, kCalls4D7B00, MH_N(kCalls4D7B00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_Launch)},
    {"Magic114_Rays", 0x4D7B70, 0x22, kCalls4D7B70, MH_N(kCalls4D7B70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_Rays)},
    {"Magic114_Shrink", 0x4D7BA0, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_Shrink)},
    {"Magic114_End", 0x4D7BD0, 0x35, kCalls4D7BD0, MH_N(kCalls4D7BD0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_End)},
    {"Magic114_DrawRing", 0x4D7C10, 0xEC, kCalls4D7C10, MH_N(kCalls4D7C10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_DrawRing)},
    {"Magic114_DrawTriangle", 0x4D7D00, 0x2FC, kCalls4D7D00, MH_N(kCalls4D7D00), nullptr, 0, kTables4D7D00, MH_N(kTables4D7D00), reinterpret_cast<const void*>(&::Magic114_DrawTriangle)},
    {"Magic114_PushMatrix", 0x4D8000, 0xBA, kCalls4D8000, MH_N(kCalls4D8000), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_PushMatrix)},
    {"Magic114_ChildTask", 0x4D80C0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_ChildTask)},
    {"Magic114_TrailRun", 0x4D80E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_TrailRun)},
    {"Magic114_TrailAim", 0x4D8100, 0xA0, kCalls4D8100, MH_N(kCalls4D8100), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_TrailAim)},
    {"Magic114_TrailSpawn", 0x4D81A0, 0x10D, kCalls4D81A0, MH_N(kCalls4D81A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_TrailSpawn)},
    {"Magic114_DrawTrail", 0x4D82B0, 0x271, kCalls4D82B0, MH_N(kCalls4D82B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_DrawTrail)},
    {"Magic114_BeamRun", 0x4D8530, 0x40, kCalls4D8530, MH_N(kCalls4D8530), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_BeamRun)},
    {"Magic114_BeamWait", 0x4D8570, 0x81, kCalls4D8570, MH_N(kCalls4D8570), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_BeamWait)},
    {"Magic114_BeamGrow", 0x4D8600, 0x52, kCalls4D8600, MH_N(kCalls4D8600), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_BeamGrow)},
    {"Magic114_BeamFade", 0x4D8660, 0x3B, kCalls4D8660, MH_N(kCalls4D8660), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_BeamFade)},
    {"Magic114_DrawBeamCore", 0x4D86A0, 0x3B7, kCalls4D86A0, MH_N(kCalls4D86A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_DrawBeamCore)},
    {"Magic114_DrawBeamStrip", 0x4D8A60, 0x372, kCalls4D8A60, MH_N(kCalls4D8A60), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_DrawBeamStrip)},
    {"Magic114_DrawBeamGlow", 0x4D8DE0, 0x2E3, kCalls4D8DE0, MH_N(kCalls4D8DE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_DrawBeamGlow)},
    {"Magic114_SpriteRun", 0x4D90D0, 0x27, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_SpriteRun)},
    {"Magic114_SpriteStart", 0x4D9100, 0x7F, kCalls4D9100, MH_N(kCalls4D9100), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_SpriteStart)},
    {"Magic114_SpriteTick", 0x4D9180, 0x13, kCalls4D9180, MH_N(kCalls4D9180), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_SpriteTick)},
    {"Magic114_DrawRays", 0x4D91A0, 0x178, kCalls4D91A0, MH_N(kCalls4D91A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_DrawRays)},
    {"Magic114_PushRayMatrix", 0x4D9320, 0x118, kCalls4D9320, MH_N(kCalls4D9320), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic114_PushRayMatrix)},
    {"Magic115_Task", 0x4D9440, 0x36, nullptr, 0, kImms4D9440, MH_N(kImms4D9440), nullptr, 0, reinterpret_cast<const void*>(&::Magic115_Task)},
    {"Magic115_Start", 0x4D9480, 0xDB, kCalls4D9480, MH_N(kCalls4D9480), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_Start)},
    {"Magic115_TintUp", 0x4D9560, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_TintUp)},
    {"Magic115_TintDown", 0x4D95B0, 0x78, kCalls4D95B0, MH_N(kCalls4D95B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_TintDown)},
    {"Magic115_ChildTask", 0x4D9630, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_ChildTask)},
    {"Magic115_OrbitRun", 0x4D9650, 0x3D, kCalls4D9650, MH_N(kCalls4D9650), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_OrbitRun)},
    {"Magic115_OrbitStart", 0x4D9690, 0xF1, kCalls4D9690, MH_N(kCalls4D9690), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_OrbitStart)},
    {"Magic115_Orbit", 0x4D9790, 0x148, kCalls4D9790, MH_N(kCalls4D9790), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_Orbit)},
    {"Magic115_OrbitFade", 0x4D98E0, 0xA9, kCalls4D98E0, MH_N(kCalls4D98E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_OrbitFade)},
    {"Magic115_MoteRun", 0x4D9990, 0x3D, kCalls4D9990, MH_N(kCalls4D9990), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_MoteRun)},
    {"Magic115_MoteStart", 0x4D99D0, 0xAF, kCalls4D99D0, MH_N(kCalls4D99D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_MoteStart)},
    {"Magic115_MoteTick", 0x4D9A80, 0x18, kCalls4D9A80, MH_N(kCalls4D9A80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_MoteTick)},
    {"Magic115_MoteFade", 0x4D9AA0, 0x3B, kCalls4D9AA0, MH_N(kCalls4D9AA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic115_MoteFade)},
    {"Magic117_Task", 0x4D9F40, 0x36, nullptr, 0, kImms4D9F40, MH_N(kImms4D9F40), nullptr, 0, reinterpret_cast<const void*>(&::Magic117_Task)},
    {"Magic117_TintSet", 0x4D9F80, 0x47, kCalls4D9F80, MH_N(kCalls4D9F80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic117_TintSet)},
    {"Magic117_Brighten", 0x4D9FD0, 0x64, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic117_Brighten)},
    {"Magic117_Dim", 0x4DA040, 0x9C, kCalls4DA040, MH_N(kCalls4DA040), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic117_Dim)},
    {"Magic117_TaskB", 0x4DA0E0, 0x36, nullptr, 0, kImms4DA0E0, MH_N(kImms4DA0E0), nullptr, 0, reinterpret_cast<const void*>(&::Magic117_TaskB)},
    {"Magic117_Darken", 0x4DA120, 0x64, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic117_Darken)},
    {"Magic117_Lighten", 0x4DA190, 0x85, kCalls4DA190, MH_N(kCalls4DA190), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic117_Lighten)},
};
#undef MH_N
enum : unsigned {
    kMagic114_Task, kMagic114_Start, kMagic114_Grow, kMagic114_Hold, kMagic114_Launch, kMagic114_Rays, kMagic114_Shrink, kMagic114_End, kMagic114_DrawRing, kMagic114_DrawTriangle, kMagic114_PushMatrix, kMagic114_ChildTask, kMagic114_TrailRun, kMagic114_TrailAim, kMagic114_TrailSpawn, kMagic114_DrawTrail, kMagic114_BeamRun, kMagic114_BeamWait, kMagic114_BeamGrow, kMagic114_BeamFade, kMagic114_DrawBeamCore, kMagic114_DrawBeamStrip, kMagic114_DrawBeamGlow, kMagic114_SpriteRun, kMagic114_SpriteStart, kMagic114_SpriteTick, kMagic114_DrawRays, kMagic114_PushRayMatrix, kMagic115_Task, kMagic115_Start, kMagic115_TintUp, kMagic115_TintDown, kMagic115_ChildTask, kMagic115_OrbitRun, kMagic115_OrbitStart, kMagic115_Orbit, kMagic115_OrbitFade, kMagic115_MoteRun, kMagic115_MoteStart, kMagic115_MoteTick, kMagic115_MoteFade, kMagic117_Task, kMagic117_TintSet, kMagic117_Brighten, kMagic117_Dim, kMagic117_TaskB, kMagic117_Darken, kMagic117_Lighten, kCount
};

static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;

constexpr std::uint32_t kScratch = 0x903850, kVertices = 0x9037A0, kFrameSet = 0x9039D8, kTurn = 0x446770;

// --- the group's memory ----------------------------------------------------------

// The packets the draws build: Gfx_PacketNext is aimed at one of eight places
// in this buffer (and moved between them by the disturbance); the commit and
// link stand-ins move it on as the real ones do, kept inside.
alignas(16) unsigned char g_packets[0x400];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 7) * 0x40; }
void Advance(unsigned size) {
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_packets || p > g_packets + 0x300) p = PacketAt(size >> 2);
    Gfx_PacketNext = p;
}
// Magic114_DrawTriangle's keys when it is fuzzed alone: 256 of them, for
// any +0xB it reads after its projection.
alignas(16) int g_keys[0x100];

unsigned char* Sc() { return Sprite_Current; }
unsigned char* TintOf(unsigned i) { return MoveScript_TintRecords + 12 * i; }

// --- the stand-ins of this file's --------------------------------------------------

void __cdecl RecCommit(std::uint32_t slot, std::uint32_t size) {
    mh::Record(bof3::addr::Gfx_CommitPrim, slot & 0xFF, size & 0xFF);
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    Advance(size);
    mh::Stir();
}
void __cdecl RecLink(std::uint32_t x, std::uint32_t z, std::uint32_t dy, std::uint32_t size) {
    mh::Record(bof3::addr::MapView_LinkPrimAt, x, z, dy & 0xFF, size & 0xFF);
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    Advance(size);
    mh::Stir();
}
// The turn: logged with what it reads, then done for real (Capcom's code).
void __cdecl RecTurn(unsigned char* task) {
    mh::Record(kTurn, Key(task), task[8], static_cast<std::uint32_t>(Long(task + 0xC)),
               static_cast<std::uint32_t>(Long(task + 0x10)));
    reinterpret_cast<void (__cdecl*)(unsigned char*)>(static_cast<std::uintptr_t>(kTurn))(task);
    mh::Stir();
}
// Magic114_DrawTriangle inside the ring: what it reads, and every key written.
void __cdecl RecTriangle(int* keys) {
    mh::Record(bof3::addr::Magic114_DrawTriangle, Key(Sc()), Sc()[0xB], Sc()[0xA], Sc()[1]);
    for (unsigned i = 0; i < 8; ++i) keys[i] = static_cast<int>(mh::Noise());
    mh::Stir();
}
void __cdecl RecUpdateScreen() {
    mh::Record(bof3::addr::Sprite_UpdateScreen, Key(Sc()), static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))));
    mh::Stir();
}
// A sprite child's phase, run with the effects' frame-offset table.
template <std::uint32_t A> void __cdecl RecSpritePhase() {
    mh::Record(A, Key(Sc()), Sc()[1] | static_cast<std::uint32_t>(Sc()[2]) << 8,
               static_cast<std::uint32_t>(Long(mh::Mem(kFrameSet))), static_cast<std::uint32_t>(Long(mh::Mem(mh::at::kOwner))));
    mh::Stir();
}

// The GTE stand-ins of the matrix pushes (group S22's): a result from the
// inputs, written where the real callee writes.
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

#define S26_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S26_RAW(text, address) text, address, address
#define S26_CUSTOM(fn) 0, {}, kG, 0, 0, {}, nullptr, reinterpret_cast<const void*>(fn)
#define S26_SPRITE_PHASE(address) {S26_RAW(#address, address), S26_CUSTOM(&RecSpritePhase<address>)}
const mh::Callee kCallees[] = {
    // this file's stand-ins
    {S26_OURS(Gfx_CommitPrim), S26_CUSTOM(&RecCommit)},
    {S26_OURS(MapView_LinkPrimAt), S26_CUSTOM(&RecLink)},
    {S26_RAW("0x446770", kTurn), S26_CUSTOM(&RecTurn)},
    {S26_RAW("Magic114_DrawTriangle", 0x4D7D00), S26_CUSTOM(&RecTriangle)},
    {S26_OURS(Sprite_UpdateScreen), S26_CUSTOM(&RecUpdateScreen)},
    S26_SPRITE_PHASE(0x4D9100), S26_SPRITE_PHASE(0x4D9180), S26_SPRITE_PHASE(0x4D9690), S26_SPRITE_PHASE(0x4D9790),
    S26_SPRITE_PHASE(0x4D98E0), S26_SPRITE_PHASE(0x4AF490), S26_SPRITE_PHASE(0x4D99D0), S26_SPRITE_PHASE(0x4D9A80),
    S26_SPRITE_PHASE(0x4D9AA0),
    // the depth sort: the keys (the caller's frame) by their bytes
    {S26_OURS(MagicFx_LinkByDepth), 7, {kAll, kAll, 0, kAll, kU8, kU8, kU8}, kG, 0, 0, {0, 0, 32}},
    // the GTE: vectors by their bytes, the caller's frame pointers not at all
    {S26_OURS(Gte_PushMatrix), 0, {}, kG, 0, 0},
    {S26_OURS(Gte_RotTrans), 3, {0, 0, 0}, kG, 0, 0, {6}, &RotTransEffect},
    {S26_OURS(Gte_RotMatrix), 2, {0, 0}, kG, 0, 0, {6}, &RotMatrixEffect},
    {S26_OURS(Gte_MulMatrix0), 3, {kAll, 0, 0}, kG, 0, 0, {0, 18}, &MulMatrixEffect},
    {S26_OURS(Gte_SetRotMatrix), 1, {0}, kG, 0, 0, {18}},
    {S26_OURS(Gte_SetTransMatrix), 1, {0}, kG, 0, 0, {}, &SetTransEffect},
    {S26_OURS(Gte_RotAverage3), 8, {0, 0, 0, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S26_OURS(Gte_RotAverage4), 10, {0, 0, 0, 0, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6, 6}},
    {S26_OURS(Gte_RotTransPers), 4, {0, kAll, 0, 0}, kG, 0, 0, {6}},
    {S26_OURS(Gte_StoreDepthF), 1, {kAll}, kG, 0, 0},
    {S26_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S26_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    // the GPU
    {S26_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S26_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S26_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S26_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S26_OURS(Gpu_SetPolyFT4), 1, {kAll}, kG, 0, 0},
    {S26_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    {S26_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S26_OURS(Gpu_GetClut), 2, {kAll, kAll}, kG, 0, 0},
    // sprites and tints
    {S26_OURS(Sprite_SetAnimation), 1, {kAll}, kG, 0, 0},
    {S26_OURS(Sprite_ScriptTick), 0, {}, mh::Answer::kFlag, 0, 0},
    {S26_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    {S26_OURS(Tint_Release), 1, {kU8}, kG, 0, 0},
    // this group's own, called directly by address (not through a table)
    {S26_RAW("Magic114_DrawRing", 0x4D7C10), 0, {}, mh::Answer::kPhase, 0, 0},
    {S26_RAW("Magic114_PushMatrix", 0x4D8000), 0, {}, mh::Answer::kPhase, 0, 0},
    {S26_RAW("Magic114_DrawTrail", 0x4D82B0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S26_RAW("Magic114_DrawBeamCore", 0x4D86A0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S26_RAW("Magic114_DrawBeamStrip", 0x4D8A60), 1, {kU8}, kG, 0, 0},
    {S26_RAW("Magic114_DrawBeamGlow", 0x4D8DE0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S26_RAW("Magic114_DrawRays", 0x4D91A0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S26_RAW("Magic114_PushRayMatrix", 0x4D9320), 0, {}, mh::Answer::kPhase, 0, 0},
};
#undef S26_SPRITE_PHASE
#undef S26_CUSTOM

// The .data handler tables the dispatchers read in place (symbols.toml).
const mh::DataTable kTables[] = {
    {0x65BA68, 3}, {0x65BA74, 2}, {0x65BA7C, 3}, {0x65BA88, 2}, {0x65BA90, 2}, {0x65BA98, 4}, {0x65BAA8, 4},
};

const mh::Region kRegions[] = {
    {0x7E0670, 4},                                   // Gfx_PacketNext
    {Key(g_packets), sizeof g_packets},
    {Key(g_keys), sizeof g_keys},
    {kVertices, 0x20},                               // Prim_VertexScratch
    {kScratch, 0x10},                                // DamageScratch
    {kFrameSet, 4},
    {0x905E60, 8},                                   // Field_Kind2Z, Field_Kind2X
    {Key(MoveScript_TintRecords), 0xC00},            // an unchecked byte index of 12-byte records
    {Key(Gfx_ClutStrip + 0x200), 0x200},             // row 2, Magic114_Start's
    {Key(Gfx_ClutStrip + 0x1A00), 0x200},            // row 26
    {Key(Gfx_ClutStripSource + 0x200), 0x200},
    {Key(Gfx_ClutStripSource + 0x1A00), 0x200},
    {Key(Magic114_RingOffsets), 0x28},               // MAGIC114's .data up to its handler tables
};

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFF;
    const auto word = static_cast<std::uint16_t>(h >> 16);
    unsigned char* const s = Sc();
    switch ((h >> 8) % 9) {
    case 0: Gfx_PacketNext = PacketAt(v); break;
    case 1: SetWord(mh::Mem(kVertices + (v % 16) * 2), word); break;
    case 2: SetWord(mh::Mem(kScratch + (v % 8) * 2), word); break;
    case 3: SetLong(mh::Mem(kFrameSet), static_cast<std::int32_t>(h)); break;
    case 4: {
        static const unsigned kFields[] = {0xC, 0x10, 0x14, 0x20};
        SetLong(s + kFields[v % 4], static_cast<std::int32_t>(static_cast<std::int16_t>(word)));
        break;
    }
    case 5: {
        static const unsigned kFields[] = {0x3E, 0x3F, 0x5C, 0x5D, 0x5E, 0x5F, 0x24, 0x25, 0x29, 0x2A};
        s[kFields[v % 10]] = static_cast<unsigned char>(word);
        break;
    }
    case 6: SetLong(mh::Mem(0x905E60 + (v & 4)), static_cast<std::int32_t>(h)); break;
    case 7: TintOf(v & 1 ? s[0xA] : s[0xB])[2 + (v >> 1) % 3] = static_cast<unsigned char>(word); break;
    case 8: {
        static const unsigned kFields[] = {0x3E, 0x3F, 0x9};
        mh::Pointer(mh::at::kOwner)[kFields[v % 3]] = static_cast<unsigned char>(word);
        break;
    }
    default: break;
    }
}

// --- the seed ------------------------------------------------------------------

unsigned char Around(unsigned edge) { return static_cast<unsigned char>(edge - 1 + mh::Next() % 3); }

void Seed(unsigned k) {
    unsigned char* const s = Sc();
    Gfx_PacketNext = PacketAt(mh::Next());
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kMagic114_Task:
        s[1] = static_cast<unsigned char>(mh::Next() % 7);
        if (mh::Half()) s[0] = 0;
        break;
    case kMagic114_ChildTask: s[1] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kMagic114_TrailRun: case kMagic114_SpriteRun: s[2] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kMagic114_BeamRun:
        s[2] = static_cast<unsigned char>(mh::Next() % 3);
        if (mh::Half()) s[0] = 0;
        break;
    case kMagic115_Task: case kMagic117_Task: case kMagic117_TaskB: s[1] = static_cast<unsigned char>(mh::Next() % 4); break;
    case kMagic115_ChildTask: s[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kMagic115_OrbitRun: case kMagic115_MoteRun:
        s[2] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Half()) s[0] = 0;
        break;
    // the counts: each side of their ends
    case kMagic114_Grow: if (mh::Often()) s[0xA] = Around(0x21); break;
    case kMagic114_Hold: if (mh::Often()) s[9] = Around(0x21); break;
    case kMagic114_Launch: if (mh::Often()) s[9] = Around(0x20); break;
    case kMagic114_Rays: case kMagic114_End: case kMagic114_BeamWait: case kMagic114_BeamFade:
        if (mh::Often()) s[9] = Around(1);
        break;
    case kMagic114_Shrink: if (mh::Often()) s[0xA] = Around(1); break;
    case kMagic114_TrailSpawn:
        if (mh::Half()) s[9] = 0;
        else if (mh::Half()) s[9] = Around(0xC);
        break;
    case kMagic114_BeamGrow:
        if (mh::Often()) SetLong(s + 0x14, mh::Half() ? 0x30 - static_cast<int>(mh::Next() % 3) : 0xC0 - static_cast<int>(mh::Next() % 3));
        break;
    case kMagic114_DrawTriangle:
        s[0xB] = static_cast<unsigned char>(mh::Next() % 10);
        if (mh::Often()) s[1] = static_cast<unsigned char>(2 + mh::Next() % 3);
        if (mh::Half()) Frame_Counter &= ~0x18u;
        break;
    case kMagic114_DrawBeamCore: case kMagic114_DrawBeamStrip: case kMagic114_DrawBeamGlow:
        if (mh::Often()) SetLong(s + 0x14, static_cast<std::int32_t>(mh::Next() % 70) - 4);
        s[2] = static_cast<unsigned char>(mh::Next() % 3);
        break;
    case kMagic114_DrawRays:
        if (mh::Often()) s[9] = static_cast<unsigned char>(0x17 + mh::Next() % 4);
        s[8] = static_cast<unsigned char>(mh::Next() % 4);
        break;
    case kMagic115_TintUp: if (mh::Often()) s[9] = Around(6); break;
    case kMagic115_TintDown: if (mh::Often()) TintOf(s[0xA])[2] = static_cast<unsigned char>(mh::Next() % 3); break;
    case kMagic115_Orbit: case kMagic115_OrbitFade:
        if (mh::Often()) s[9] = static_cast<unsigned char>(mh::Half() ? 0x3F : 7 + 8 * (mh::Next() % 8));
        break;
    case kMagic117_Brighten: case kMagic117_Darken: if (mh::Often()) s[9] = Around(0xF); break;
    case kMagic117_Dim:
        if (mh::Often()) s[9] = Around(1);
        if (mh::Often()) s[0xA] = Around(1);
        break;
    case kMagic117_Lighten: if (mh::Often()) s[9] = Around(1); break;
    default: break;
    }
}

// Magic114_DrawTriangle's pointer: the keys of this file's (the others take
// none and ignore the words).
void Args(unsigned k, std::uint32_t* a) {
    if (k == kMagic114_DrawTriangle) a[0] = Key(g_keys);
}

}  // namespace

void SelfTest() {
    mh::Group group = {
        "magic_s26", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000,
    };
    // A dispatcher that reads its phase again after a call (Magic114_Task
    // after the ring) indexes a table of two or more: keep it inside.
    group.phase_span = 2;
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s26
