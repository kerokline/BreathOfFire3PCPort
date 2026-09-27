// BOF3X_SHADOW=magic_s32: group S32's two overlays (MAGIC144, MAGIC150)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s32.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC144 / MAGIC150 --clones
// (2026-09-27; capstone, every jump internal, no jump table, no REFUSED line),
// names given. Beyond the standard set this group lists the draw callees (the
// libgpu entry points, Math_Sin / Math_Cos, Gte_RotTransPers3 and its depths,
// Gfx_CommitPrim, MapView_LinkPrimAt), Sprite_SetTint, MAGIC092's flame draw,
// Capcom's three unnamed helpers of the map camera (0x494060, 0x494110,
// 0x4941B0) and the functions of its own its functions call directly.
// Everything the harness lacks is built here, not in the harness:
//
//   - the draws: Gfx_CommitPrim and MapView_LinkPrimAt log each primitive's
//     bytes and move Gfx_PacketNext on through a packet buffer of the fuzz's
//     own, as the real ones do (every primitive of a draw is built at the
//     pointer, so without the log only the last would be compared); the buffer
//     keeps 0x100 bytes below the pointer, because EyeBeam_DrawSpiralSegment
//     copies the 0x44 bytes before it;
//   - 0x494110 (a world point projected into a vertex) logs its vector by its
//     twelve bytes and writes a vertex where the real one writes - the
//     callers' vertex is a local of theirs, copied into the packet after the
//     call; 0x4941B0 logs the three vertices it compares;
//   - the calls that act on Sprite_Current (the screen point, the free,
//     MAGIC092's flame) log which task, so a walk that forgets to swap it
//     shows;
//   - the disturbance moves the pool's records (never their owners, which the
//     walk hands on), the beam's state, the sparks and the spiral points; the
//     spark cell only back to the first spark (a cell moved forward walks the
//     sparks' loop onto the cell itself, which the loop then writes through -
//     a fault on both sides that no caller can reach: the cell is only ever
//     the spark being walked).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s32.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_s32 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// tools/magic_rows.py --unit MAGIC144 / MAGIC150 --clones, 2026-09-27, names given.
// 0x4E9140: 0x75 bytes  WallOfFire_Task
constexpr mh::CallSite kCalls4E9140[] = {{0x53, 0x4E95E0}};
constexpr mh::Imm kImms4E9140[] = {{0x16, 0x4E91C0}, {0x1E, 0x4E5200}};
// 0x4E91C0: 0xAD bytes  WallOfFire_Start
constexpr mh::CallSite kCalls4E91C0[] = {{0x2F, 0x435180}};
// 0x4E9270: 0x12 bytes; +0xB note: jmp through .data 0x65bdc4 (a data_tables entry)  WallOfFireChild_Task
// 0x4E9290: 0x33 bytes; +0xB note: call through .data 0x65bdc8 (a data_tables entry)  WallOfFireChild_Run
constexpr mh::CallSite kCalls4E9290[] = {{0x23, 0x4B7D40}, {0x28, 0x4E9420}, {0x2D, 0x5A7BC0}};
// 0x4E92D0: 0x97 bytes  WallOfFireChild_Spawn
constexpr mh::CallSite kCalls4E92D0[] = {{0x17, 0x4E9A10}, {0x7F, 0x587900}};
// 0x4E9370: 0x50 bytes  WallOfFireChild_Tint
constexpr mh::CallSite kCalls4E9370[] = {{0x20, 0x454DC0}, {0x34, 0x454CC0}, {0x3F, 0x4530D0}};
// 0x4E93C0: 0xF bytes  WallOfFireChild_WaitFlame
// 0x4E93D0: 0x46 bytes  WallOfFireChild_End
constexpr mh::CallSite kCalls4E93D0[] = {{0x24, 0x454DC0}, {0x30, 0x4FBDB0}, {0x40, 0x4351F0}};
// 0x4E9420: 0x1B1 bytes  WallOfFire_DrawDisc
constexpr mh::CallSite kCalls4E9420[] = {{0x19, 0x5A77C0}, {0x22, 0x461E50}, {0x31, 0x5A7A00}, {0x4A, 0x5A7A50}, {0x97, 0x5A75F0}, {0x9E, 0x5A7780}, {0xCC, 0x5A7A00}, {0xE5, 0x5A7A50}, {0x137, 0x5A84A0}, {0x13D, 0x5A9310}, {0x172, 0x461E50}, {0x198, 0x5A77C0}, {0x1A1, 0x461E50}};
// 0x4E95E0: 0x12 bytes; +0xB note: jmp through .data 0x65bdd8 (a data_tables entry)  WallOfFireMote_Run
// 0x4E9600: 0x2E bytes; +0xB note: call through .data 0x65bde0 (a data_tables entry)  WallOfFireFlame_Run
constexpr mh::CallSite kCalls4E9600[] = {{0x23, 0x4FBD10}, {0x28, 0x4C5BA0}};
// 0x4E9630: 0x5C bytes  WallOfFireFlame_Start
// 0x4E9690: 0x1D bytes  WallOfFireFlame_Rise
// 0x4E96B0: 0x67 bytes  WallOfFireFlame_Fade
// 0x4E9720: 0x4F bytes; +0xB note: call through .data 0x65bdf0 (a data_tables entry)  WallOfFireSpark_Run
constexpr mh::CallSite kCalls4E9720[] = {{0x23, 0x4FBD10}, {0x49, 0x4E9850}};
// 0x4E9770: 0x51 bytes  WallOfFireSpark_Start
// 0x4E97D0: 0x14 bytes  WallOfFireSpark_Wait
// 0x4E97F0: 0x51 bytes  WallOfFireSpark_End
// 0x4E9850: 0x1C0 bytes  WallOfFireSpark_Draw
constexpr mh::CallSite kCalls4E9850[] = {{0x4, 0x5B93D2}, {0x40, 0x5A77C0}, {0x56, 0x572FA0}, {0x62, 0x5A75F0}, {0x69, 0x5A7780}, {0x98, 0x5A7A00}, {0xC2, 0x5A7A50}, {0xF2, 0x5A7A00}, {0x11C, 0x5A7A50}, {0x1A7, 0x572FA0}};
// 0x4E9A10: 0x57 bytes  WallOfFire_PoolAlloc
// 0x4EA450: 0x4E bytes  EyeBeam_Task
constexpr mh::Imm kImms4EA450[] = {{0xF, 0x4EA4A0}, {0x17, 0x4EA520}, {0x22, 0x4EA550}, {0x2A, 0x4EA570}, {0x32, 0x4EA5C0}, {0x3A, 0x4EA630}, {0x42, 0x43FE80}};
// 0x4EA4A0: 0x75 bytes  EyeBeam_Start
constexpr mh::CallSite kCalls4EA4A0[] = {{0x52, 0x4EAB70}, {0x6E, 0x587740}};
// 0x4EA520: 0x27 bytes  EyeBeam_Charge
constexpr mh::CallSite kCalls4EA520[] = {{0x0, 0x4EABA0}, {0x5, 0x4EAC00}};
// 0x4EA550: 0x1B bytes  EyeBeam_WaitSparks
constexpr mh::CallSite kCalls4EA550[] = {{0x0, 0x4EAC00}};
// 0x4EA570: 0x46 bytes  EyeBeam_Widen
constexpr mh::CallSite kCalls4EA570[] = {{0x10, 0x4EA670}, {0x3F, 0x587740}};
// 0x4EA5C0: 0x66 bytes  EyeBeam_Fire
constexpr mh::CallSite kCalls4EA5C0[] = {{0x1D, 0x4EA670}, {0x53, 0x4530D0}, {0x5D, 0x587740}};
// 0x4EA630: 0x32 bytes  EyeBeam_Narrow
constexpr mh::CallSite kCalls4EA630[] = {{0x10, 0x4EA670}};
// 0x4EA670: 0x48 bytes  EyeBeam_Draw
constexpr mh::CallSite kCalls4EA670[] = {{0x10, 0x5A79A0}, {0x28, 0x5A77C0}, {0x31, 0x461E50}, {0x39, 0x494060}, {0x3E, 0x4EA6C0}, {0x43, 0x4EA890}};
// 0x4EA6C0: 0x1CA bytes  EyeBeam_DrawCylinder
constexpr mh::CallSite kCalls4EA6C0[] = {{0x1C, 0x5A75B0}, {0x24, 0x5A7780}, {0x33, 0x5A7A50}, {0x52, 0x5A7A00}, {0x7A, 0x494110}, {0xB1, 0x494110}, {0xEC, 0x5A7A50}, {0x10B, 0x5A7A00}, {0x133, 0x494110}, {0x16D, 0x494110}, {0x19A, 0x4941B0}, {0x1AB, 0x461E50}};
// 0x4EA890: 0x136 bytes  EyeBeam_DrawSpiral
constexpr mh::CallSite kCalls4EA890[] = {{0x7, 0x494060}, {0x2F, 0x5A7A50}, {0x4F, 0x5A7A00}, {0x73, 0x4EA9D0}, {0xA7, 0x5B93D2}, {0xC9, 0x5A7A50}, {0xE9, 0x5A7A00}, {0x10D, 0x4EA9D0}, {0x117, 0x4EAA10}};
// 0x4EA9D0: 0x3B bytes  EyeBeam_ProjectSpiralPoint
constexpr mh::CallSite kCalls4EA9D0[] = {{0xA, 0x494110}, {0x1D, 0x494110}, {0x31, 0x494110}};
// 0x4EAA10: 0x158 bytes  EyeBeam_DrawSpiralSegment
constexpr mh::CallSite kCalls4EAA10[] = {{0xB, 0x5A7610}, {0x13, 0x5A7780}, {0xAE, 0x461E50}, {0xF5, 0x461E50}, {0x101, 0x5A7650}, {0x109, 0x5A7780}, {0x14B, 0x461E50}};
// 0x4EAB70: 0x2E bytes  EyeBeamSparks_Clear
// 0x4EABA0: 0x56 bytes  EyeBeamSparks_Spawn
constexpr mh::CallSite kCalls4EABA0[] = {{0x32, 0x5B93D2}};
// 0x4EAC00: 0x134 bytes  EyeBeamSparks_Draw
constexpr mh::CallSite kCalls4EAC00[] = {{0x14, 0x5A79A0}, {0x2C, 0x5A77C0}, {0x35, 0x461E50}, {0x3D, 0x494060}, {0x6F, 0x5A7A50}, {0x98, 0x5A7A00}, {0xC3, 0x494110}, {0xDF, 0x4EAD40}};
// 0x4EAD40: 0x122 bytes  EyeBeamSpark_DrawDisc
constexpr mh::CallSite kCalls4EAD40[] = {{0x29, 0x5A75F0}, {0x31, 0x5A7780}, {0x42, 0x5A7A50}, {0x5B, 0x5A7A00}, {0x7F, 0x5A7A50}, {0x98, 0x5A7A00}, {0xEA, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"WallOfFire_Task", 0x4E9140, 0x75, kCalls4E9140, MH_N(kCalls4E9140), kImms4E9140, MH_N(kImms4E9140), nullptr, 0, reinterpret_cast<const void*>(&::WallOfFire_Task)},
    {"WallOfFire_Start", 0x4E91C0, 0xAD, kCalls4E91C0, MH_N(kCalls4E91C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFire_Start)},
    {"WallOfFireChild_Task", 0x4E9270, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireChild_Task)},
    {"WallOfFireChild_Run", 0x4E9290, 0x33, kCalls4E9290, MH_N(kCalls4E9290), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireChild_Run)},
    {"WallOfFireChild_Spawn", 0x4E92D0, 0x97, kCalls4E92D0, MH_N(kCalls4E92D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireChild_Spawn)},
    {"WallOfFireChild_Tint", 0x4E9370, 0x50, kCalls4E9370, MH_N(kCalls4E9370), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireChild_Tint)},
    {"WallOfFireChild_WaitFlame", 0x4E93C0, 0xF, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireChild_WaitFlame)},
    {"WallOfFireChild_End", 0x4E93D0, 0x46, kCalls4E93D0, MH_N(kCalls4E93D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireChild_End)},
    {"WallOfFire_DrawDisc", 0x4E9420, 0x1B1, kCalls4E9420, MH_N(kCalls4E9420), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFire_DrawDisc)},
    {"WallOfFireMote_Run", 0x4E95E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireMote_Run)},
    {"WallOfFireFlame_Run", 0x4E9600, 0x2E, kCalls4E9600, MH_N(kCalls4E9600), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireFlame_Run)},
    {"WallOfFireFlame_Start", 0x4E9630, 0x5C, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireFlame_Start)},
    {"WallOfFireFlame_Rise", 0x4E9690, 0x1D, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireFlame_Rise)},
    {"WallOfFireFlame_Fade", 0x4E96B0, 0x67, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireFlame_Fade)},
    {"WallOfFireSpark_Run", 0x4E9720, 0x4F, kCalls4E9720, MH_N(kCalls4E9720), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireSpark_Run)},
    {"WallOfFireSpark_Start", 0x4E9770, 0x51, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireSpark_Start)},
    {"WallOfFireSpark_Wait", 0x4E97D0, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireSpark_Wait)},
    {"WallOfFireSpark_End", 0x4E97F0, 0x51, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireSpark_End)},
    {"WallOfFireSpark_Draw", 0x4E9850, 0x1C0, kCalls4E9850, MH_N(kCalls4E9850), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFireSpark_Draw)},
    {"WallOfFire_PoolAlloc", 0x4E9A10, 0x57, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::WallOfFire_PoolAlloc), 0xFF},
    {"EyeBeam_Task", 0x4EA450, 0x4E, nullptr, 0, kImms4EA450, MH_N(kImms4EA450), nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_Task)},
    {"EyeBeam_Start", 0x4EA4A0, 0x75, kCalls4EA4A0, MH_N(kCalls4EA4A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_Start)},
    {"EyeBeam_Charge", 0x4EA520, 0x27, kCalls4EA520, MH_N(kCalls4EA520), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_Charge)},
    {"EyeBeam_WaitSparks", 0x4EA550, 0x1B, kCalls4EA550, MH_N(kCalls4EA550), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_WaitSparks)},
    {"EyeBeam_Widen", 0x4EA570, 0x46, kCalls4EA570, MH_N(kCalls4EA570), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_Widen)},
    {"EyeBeam_Fire", 0x4EA5C0, 0x66, kCalls4EA5C0, MH_N(kCalls4EA5C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_Fire)},
    {"EyeBeam_Narrow", 0x4EA630, 0x32, kCalls4EA630, MH_N(kCalls4EA630), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_Narrow)},
    {"EyeBeam_Draw", 0x4EA670, 0x48, kCalls4EA670, MH_N(kCalls4EA670), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_Draw)},
    {"EyeBeam_DrawCylinder", 0x4EA6C0, 0x1CA, kCalls4EA6C0, MH_N(kCalls4EA6C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_DrawCylinder)},
    {"EyeBeam_DrawSpiral", 0x4EA890, 0x136, kCalls4EA890, MH_N(kCalls4EA890), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_DrawSpiral)},
    {"EyeBeam_ProjectSpiralPoint", 0x4EA9D0, 0x3B, kCalls4EA9D0, MH_N(kCalls4EA9D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_ProjectSpiralPoint)},
    {"EyeBeam_DrawSpiralSegment", 0x4EAA10, 0x158, kCalls4EAA10, MH_N(kCalls4EAA10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeam_DrawSpiralSegment)},
    {"EyeBeamSparks_Clear", 0x4EAB70, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeamSparks_Clear)},
    {"EyeBeamSparks_Spawn", 0x4EABA0, 0x56, kCalls4EABA0, MH_N(kCalls4EABA0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeamSparks_Spawn)},
    {"EyeBeamSparks_Draw", 0x4EAC00, 0x134, kCalls4EAC00, MH_N(kCalls4EAC00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeamSparks_Draw), 0xFF},
    {"EyeBeamSpark_DrawDisc", 0x4EAD40, 0x122, kCalls4EAD40, MH_N(kCalls4EAD40), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EyeBeamSpark_DrawDisc)},
};
#undef MH_N

enum : unsigned {
    kWallOfFire_Task, kWallOfFire_Start, kWallOfFireChild_Task, kWallOfFireChild_Run, kWallOfFireChild_Spawn,
    kWallOfFireChild_Tint, kWallOfFireChild_WaitFlame, kWallOfFireChild_End, kWallOfFire_DrawDisc, kWallOfFireMote_Run,
    kWallOfFireFlame_Run, kWallOfFireFlame_Start, kWallOfFireFlame_Rise, kWallOfFireFlame_Fade, kWallOfFireSpark_Run,
    kWallOfFireSpark_Start, kWallOfFireSpark_Wait, kWallOfFireSpark_End, kWallOfFireSpark_Draw, kWallOfFire_PoolAlloc,
    kEyeBeam_Task, kEyeBeam_Start, kEyeBeam_Charge, kEyeBeam_WaitSparks, kEyeBeam_Widen, kEyeBeam_Fire, kEyeBeam_Narrow,
    kEyeBeam_Draw, kEyeBeam_DrawCylinder, kEyeBeam_DrawSpiral, kEyeBeam_ProjectSpiralPoint, kEyeBeam_DrawSpiralSegment,
    kEyeBeamSparks_Clear, kEyeBeamSparks_Spawn, kEyeBeamSparks_Draw, kEyeBeamSpark_DrawDisc, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The group's cells (magic_s32.cpp).
constexpr std::uint32_t kS = 0x903850, kV = 0x9037A0;
constexpr std::uint32_t kPool = 0x6A5680, kRecord = 0x84;
constexpr unsigned kPoolCount = 48;
constexpr std::uint32_t kBeam = 0x6A9040, kBeamBytes = 0xC8;   // EyeBeam_State to the end of EyeBeam_SpiralPoints
constexpr std::uint32_t kSparks = 0x6A9058, kSparkCell = 0x6A9098;
constexpr std::uint32_t kSpiralLast = 0x6A90A0, kSpiralNew = 0x6A90D4;

// --- the fuzz's own memory ------------------------------------------------------

// The packet buffer Gfx_PacketNext points into while the fuzz runs: never
// within 0x100 bytes of its start (EyeBeam_DrawSpiralSegment copies the 0x44
// bytes before the pointer) or of its end (a draw writes up to 0x58 past it).
constexpr unsigned kPrimBytes = 0x2000;
constexpr unsigned kPrimMargin = 0x100;
alignas(16) unsigned char g_prims[kPrimBytes];
unsigned char* PrimAt(unsigned k) { return g_prims + kPrimMargin + 4 * (k % 64); }

// A projected point handed to EyeBeamSpark_DrawDisc when it is fuzzed alone:
// two floats and a depth.
alignas(4) unsigned char g_centre[12];

// --- the callees' effects (after the recorder's log and disturbance) ------------

// Gfx_CommitPrim / MapView_LinkPrimAt: the primitive at Gfx_PacketNext into
// the log (the real ones link it), then Gfx_PacketNext on by its size, kept in
// the buffer's middle.
void Advance(std::uint32_t size) {
    mh::NoteBytes(Gfx_PacketNext, size & 0xFF);
    unsigned char* p = Gfx_PacketNext + (size & 0xFF);
    if (p < g_prims + kPrimMargin || p + kPrimMargin > g_prims + kPrimBytes) p = g_prims + kPrimMargin + (size & 0x3C);
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
// The calls that act on Sprite_Current: which task.
std::uint32_t NoteCurrent(const std::uint32_t*, std::uint32_t answer) {
    mh::Note(Key(Sprite_Current), static_cast<std::uint32_t>(Long(mh::Mem(mh::at::kOwner))));
    return answer;
}
// 0x494110 writes the projected vertex (two floats and a depth) at its second
// argument.
std::uint32_t ProjectEffect(const std::uint32_t* a, std::uint32_t answer) {
    mh::FillBytes(reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[1])), 12);
    return answer;
}

// --- the callees the standard set lacks -----------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S32_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S32_RAW(address) #address, address, address
const mh::Callee kCallees[] = {
    // listed over the standard ones for their effects
    {S32_OURS(BattleActor_UpdateScreenXY), 0, {}, kG, 0, 0, {}, &NoteCurrent},
    {S32_OURS(BattleTask_FreeCurrent), 0, {}, kG, 0, 0, {}, &NoteCurrent},
    {S32_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, kG, 0, 0},
    // group S20's flame (a tail jmp of WallOfFireFlame_Run)
    {S32_OURS(Magic092_DrawFlame), 0, {}, kG, 0, 0, {}, &NoteCurrent},
    // the draw library (all ours)
    {S32_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {S32_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    {S32_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, kG, 0, 0, {}, &CommitEffect},
    {S32_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0, {}, &LinkEffect},
    {S32_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S32_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {S32_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {S32_OURS(Gpu_SetPolyG3), 1, {kAll}, kG, 0, 0},
    {S32_OURS(Gpu_SetPolyF4), 1, {kAll}, kG, 0, 0},
    {S32_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {S32_OURS(Gpu_SetLineF2), 1, {kAll}, kG, 0, 0},
    // the projection: each SVECTOR by its six bytes, the outputs by address,
    // the depth and flag (the caller's stack) not at all
    {S32_OURS(Gte_RotTransPers3), 8, {kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, kG, 0, 0, {6, 6, 6}},
    {S32_OURS(Gte_PrimDepths3_10B), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed: the map camera; a point projected (its vector a local
    // of the caller's or a spiral point: its twelve bytes; the vertex it
    // writes, often the caller's local, not by address); the facing test of
    // three packet vertices (each by its two floats)
    {S32_RAW(0x494060), 0, {}, kG, 0, 0},
    {S32_RAW(0x494110), 2, {0, 0}, kG, 0, 0, {12}, &ProjectEffect},
    {S32_RAW(0x4941B0), 3, {0, 0, 0}, kG, 0, 0, {8, 8, 8}},
    // this group's own, called directly
    {S32_RAW(0x4E95E0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S32_RAW(0x4E9420), 0, {}, mh::Answer::kPhase, 0, 0},
    {S32_RAW(0x4E9850), 0, {}, mh::Answer::kPhase, 0, 0},
    {S32_RAW(0x4E9A10), 0, {}, mh::Answer::kByte, 0xFF, 0x2F},   // 0xFF, or a record 0..47
    {S32_RAW(0x4EA670), 0, {}, mh::Answer::kPhase, 0, 0},
    {S32_RAW(0x4EA6C0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S32_RAW(0x4EA890), 0, {}, mh::Answer::kPhase, 0, 0},
    {S32_RAW(0x4EA9D0), 1, {kAll}, kG, 0, 0},
    {S32_RAW(0x4EAA10), 1, {kAll}, kG, 0, 0},
    {S32_RAW(0x4EAB70), 0, {}, mh::Answer::kPhase, 0, 0},
    {S32_RAW(0x4EABA0), 0, {}, mh::Answer::kPhase, 0, 0},
    {S32_RAW(0x4EAC00), 0, {}, mh::Answer::kFlag, 0, 0},
    {S32_RAW(0x4EAD40), 4, {0, kU16, kU8, kU8}, kG, 0, 0, {12}},
};
#undef S32_OURS
#undef S32_RAW

// The five .data handler tables the dispatchers read in place
// (WallOfFireChild_TaskTable and the rest, symbols.toml).
const mh::DataTable kTables[] = {{0x65BDC4, 1}, {0x65BDC8, 4}, {0x65BDD8, 2}, {0x65BDE0, 4}, {0x65BDF0, 4}};

mh::Region g_regions[] = {
    {0x7E0670, 4},                  // Gfx_PacketNext
    {0, kPrimBytes},                // g_prims (filled in at start-up)
    {0, sizeof g_centre},           // g_centre (filled in at start-up)
    {kV, 0x20},                     // Prim_VertexScratch, four SVECTORs
    {kS, 0x10},                     // 0x903850..
    {kPool, kPoolCount * kRecord},  // WallOfFire_Pool
    {kBeam, kBeamBytes},            // EyeBeam_State, _Sparks, _SparkCurrent, _SpiralPoints
    {0x80E980, 0x20},               // Gfx_ClutStripSource row 26, the first sixteen
    {0x812980, 0x20},               // Gfx_ClutStrip row 26, the first sixteen
};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Record(unsigned i) { return mh::Mem(kPool + i * kRecord); }
unsigned char* Spark(unsigned i) { return mh::Mem(kSparks + 8 * i); }
unsigned char* ValidOwner(std::uint32_t v) { return v & 4 ? mh::SpriteRecord(v) : mh::TaskAt(v); }

// The group's cells a recorder may move (the harness's case 14).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFFF;
    const unsigned char b = Byte(h >> 24);
    switch ((h >> 8) % 8) {
    case 0: Gfx_PacketNext = PrimAt(v); break;
    case 1: SetWord(mh::Mem(kV + 2 * (v % 16)), h >> 16); break;
    case 2: SetWord(mh::Mem(kS + 2 * (v % 8)), h >> 16); break;
    case 3: Record(v % kPoolCount)[(h >> 20) % 0x80] = b; break;   // never the owner at +0x80
    case 4: SetWord(mh::Mem(kBeam + 2 * (v % 12)), h >> 16); break;
    case 5: mh::Mem(kSparks + v % 0x40)[0] = b; break;
    case 6: SetLong(mh::Mem(kSparkCell), static_cast<std::int32_t>(kSparks)); break;
    default: mh::Mem(kSpiralLast + v % 0x68)[0] = b; break;
    }
}

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    Gfx_PacketNext = PrimAt(mh::Next());
    // The walk makes each live record the task and its +0x80 the owner a
    // recorder writes through: every record's owner a real slot or record.
    for (unsigned i = 0; i < kPoolCount; ++i) mh::SetPointer(kPool + i * kRecord + 0x80, ValidOwner(mh::Next()));
    // Half the sparks dead; some at their last frame (radius 0x60).
    for (unsigned i = 0; i < 8; ++i) {
        if (mh::Half()) SetWord(Spark(i), 0);
        if (mh::Next() % 4 == 0) SetWord(Spark(i) + 2, 0x60);
    }
    SetLong(mh::Mem(kSparkCell), static_cast<std::int32_t>(kSparks + 8 * (mh::Next() % 8)));
    if (mh::Next() % 4 == 0) sc[0] = 0;
    switch (k) {
    // the dispatchers: inside their tables
    case kWallOfFire_Task: sc[1] = Byte(mh::Next() % 2); break;
    case kWallOfFireChild_Task: sc[1] = 0; break;
    case kWallOfFireMote_Run: sc[1] = Byte(mh::Next() % 2); break;
    case kWallOfFireChild_Run: case kWallOfFireFlame_Run: case kWallOfFireSpark_Run: sc[2] = Byte(mh::Next() % 4); break;
    case kEyeBeam_Task: sc[1] = Byte(mh::Next() % 7); break;
    // the counters: at their thresholds
    case kWallOfFireChild_Tint: Near(sc[9], 0xD); break;
    case kWallOfFireChild_WaitFlame: if (mh::Half()) sc[0xB] = Byte(mh::Half() ? 0x80 : 0x7F); break;
    case kWallOfFireChild_End:
        if (mh::Half()) sc[9] = 0;
        if (mh::Half()) sc[0xB] = Byte(MH_PICK(0x80, 0x81, 0, 0x7F));
        break;
    case kWallOfFire_DrawDisc: sc[9] = Byte(MH_PICK(0xF, 0x10, 0, 0xFF, 7)); break;
    case kWallOfFireFlame_Rise: Near(sc[9], 0xC); break;
    case kWallOfFireFlame_Fade:
        Near(sc[9], 2);
        if (mh::Half()) Frame_Counter ^= 1u;
        break;
    case kWallOfFireSpark_Wait: if (mh::Half()) mh::Pointer(mh::at::kOwner)[0xB] = Byte(mh::Half() ? 0x80 : 0x7F); break;
    case kWallOfFireSpark_End: Near(sc[9], 0); break;
    case kWallOfFire_PoolAlloc:
        // all taken a quarter of the time, else the first free anywhere
        for (unsigned i = 0; i < kPoolCount; ++i) Record(i)[0] = Byte(Record(i)[0] | 1);
        if (mh::Next() % 4 != 0) Record(mh::Next() % kPoolCount)[0] &= 0xFE;
        break;
    case kEyeBeam_Charge: case kEyeBeam_Widen: case kEyeBeam_Fire: case kEyeBeam_Narrow: Near(sc[9], 0); break;
    case kEyeBeamSparks_Spawn:
        // all live a third of the time (the search runs off the end)
        if (mh::Next() % 3 == 0)
            for (unsigned i = 0; i < 8; ++i) SetWord(Spark(i), Word(Spark(i)) | 1u);
        break;
    case kEyeBeamSparks_Draw:
        // none live a third of the time (the answer's other side)
        if (mh::Next() % 3 == 0)
            for (unsigned i = 0; i < 8; ++i) SetWord(Spark(i), 0);
        break;
    default: break;
    }
}

// The functions that take a pointer: a spiral point (the new one, whose next
// 0x34 bytes the segment does not read, or the previous one) or the
// projected centre.
void Args(unsigned k, std::uint32_t* a) {
    switch (k) {
    case kEyeBeam_ProjectSpiralPoint: a[0] = mh::Half() ? kSpiralNew : kSpiralLast; break;
    case kEyeBeam_DrawSpiralSegment: a[0] = kSpiralLast; break;
    case kEyeBeamSpark_DrawDisc: a[0] = Key(g_centre); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    g_regions[1].at = Key(g_prims);
    g_regions[2].at = Key(g_centre);
    mh::Group group = {
        "magic_s32", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], g_regions, sizeof g_regions / sizeof g_regions[0], &Seed,
        &Disturb,    2000,
    };
    group.args = &Args;
    mh::Run(group);
}

}  // namespace magic_s32
