// BOF3X_SHADOW=magic_c3: group C3's two overlays (MAGIC002, MAGIC111) through
// the spell round's shared harness (magic_harness.h), once at start-up.
// docs/magic_c3.md section 5.
//
// The clone table is tools/magic_rows.py --unit MAGIC002 / MAGIC111 --clones
// (2026-09-26; capstone, every jump internal, no jump table), names given.
// What the harness's own fields do here:
//
//   - every primitive is built at Gfx_PacketNext, which no recorder advances,
//     so a draw's quads all land in the same bytes: Gfx_CommitPrim's `effect`
//     logs each primitive's bytes as it is committed (S20, S24);
//   - Gte_VectorNormalS is called for real on both sides (Answer::kThrough):
//     the ball hands it a vector in its own frame and reads the normal back,
//     and the square root's logged argument (the normal's dot with the light)
//     is what compares them;
//   - Magic002Ball_Draw takes six arguments (Group::args), four of them read
//     as shorts and one as a byte.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_c3.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_c3 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;

// tools/magic_rows.py --unit MAGIC002 / MAGIC111 --clones, 2026-09-26, names given.
constexpr mh::Imm kImms499D80[] = {{0xF, 0x499DB0}, {0x17, 0x499E90}, {0x22, 0x43FE80}};
constexpr mh::CallSite kCalls499DB0[] = {{0x7, 0x435180}};
constexpr mh::Imm kImms499EB0[] = {{0xF, 0x499EE0}, {0x17, 0x499F70}};
constexpr mh::CallSite kCalls499EE0[] = {{0x74, 0x499FF0}};
constexpr mh::CallSite kCalls499F70[] = {{0x5C, 0x499FF0}, {0x6F, 0x4351F0}};
constexpr mh::CallSite kCalls499FF0[] = {{0x2B, 0x5A79A0}, {0x43, 0x5A77C0}, {0x4C, 0x461E50}, {0xBF, 0x5A8C00}, {0xE3, 0x5A7A90}, {0x11A, 0x5A7A90}, {0x1E1, 0x5A7A00}, {0x1FC, 0x5A7A50}, {0x23C, 0x5A8C00}, {0x266, 0x5A7A90}, {0x29B, 0x5A7A90}, {0x2E2, 0x5A7A50}, {0x304, 0x5A7A00}, {0x33A, 0x5A8C00}, {0x362, 0x5A7A90}, {0x399, 0x5A7A90}, {0x40F, 0x5A8C00}, {0x435, 0x5A7A90}, {0x46A, 0x5A7A90}, {0x581, 0x5A7610}, {0x68F, 0x5A7780}, {0x698, 0x461E50}, {0x6A4, 0x5A7610}, {0x75A, 0x5A7780}, {0x763, 0x461E50}};
constexpr mh::Imm kImms4D6110[] = {{0xF, 0x4D6140}, {0x17, 0x4D62C0}, {0x22, 0x43FE80}};
constexpr mh::CallSite kCalls4D6140[] = {{0x66, 0x435180}, {0xFD, 0x435180}, {0x12B, 0x435180}, {0x15A, 0x587900}, {0x164, 0x587900}};
constexpr mh::CallSite kCalls4D62C0[] = {{0x10, 0x4FB830}, {0x28, 0x4530D0}};
constexpr mh::CallSite kCalls4D6320[] = {{0x35, 0x588F20}};
constexpr mh::Imm kImms4D6320[] = {{0xF, 0x4D6360}, {0x17, 0x4D6380}, {0x22, 0x4AEE90}};
constexpr mh::CallSite kCalls4D63A0[] = {{0x2B, 0x4D6420}};
constexpr mh::CallSite kCalls4D63E0[] = {{0x0, 0x5B93D2}};
constexpr mh::CallSite kCalls4D6420[] = {{0x12, 0x5A77C0}, {0x1B, 0x461E50}, {0x27, 0x5A7610}, {0x2F, 0x5A7780}, {0x7F, 0x5A7A00}, {0xA1, 0x5A7A00}, {0xC5, 0x5A7A00}, {0x117, 0x5A7A00}, {0x139, 0x5A7A00}, {0x15D, 0x5A7A00}, {0x1B2, 0x5A7A00}, {0x1D4, 0x5A7A00}, {0x1F8, 0x5A7A00}, {0x24A, 0x5A7A00}, {0x26C, 0x5A7A00}, {0x290, 0x5A7A00}, {0x2B9, 0x461E50}, {0x2CA, 0x5A77C0}, {0x2D3, 0x461E50}};
constexpr mh::CallSite kCalls4D6700[] = {{0x23, 0x4D6750}};
constexpr mh::CallSite kCalls4D6750[] = {{0x10, 0x5A77C0}, {0x19, 0x461E50}, {0x25, 0x5A75B0}, {0x2D, 0x5A7780}, {0x6E, 0x461E50}, {0x7F, 0x5A77C0}, {0x8B, 0x461E50}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const mh::Clone kClones[] = {
    {"Magic002_Task", 0x499D80, 0x2E, nullptr, 0, kImms499D80, MH_N(kImms499D80), nullptr, 0, reinterpret_cast<const void*>(&::Magic002_Task)},
    {"Magic002_Start", 0x499DB0, 0xDE, kCalls499DB0, MH_N(kCalls499DB0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic002_Start)},
    {"Magic002_Wait", 0x499E90, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic002_Wait)},
    {"Magic002Ball_Task", 0x499EB0, 0x26, nullptr, 0, kImms499EB0, MH_N(kImms499EB0), nullptr, 0, reinterpret_cast<const void*>(&::Magic002Ball_Task)},
    {"Magic002Ball_Fly", 0x499EE0, 0x8B, kCalls499EE0, MH_N(kCalls499EE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic002Ball_Fly)},
    {"Magic002Ball_Shatter", 0x499F70, 0x75, kCalls499F70, MH_N(kCalls499F70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic002Ball_Shatter)},
    {"Magic002Ball_Draw", 0x499FF0, 0x7BE, kCalls499FF0, MH_N(kCalls499FF0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic002Ball_Draw)},
    {"Magic111_Task", 0x4D6110, 0x2E, nullptr, 0, kImms4D6110, MH_N(kImms4D6110), nullptr, 0, reinterpret_cast<const void*>(&::Magic111_Task)},
    {"Magic111_Start", 0x4D6140, 0x17C, kCalls4D6140, MH_N(kCalls4D6140), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111_Start)},
    {"Magic111_Wait", 0x4D62C0, 0x39, kCalls4D62C0, MH_N(kCalls4D62C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111_Wait)},
    {"Magic111Child_Task", 0x4D6300, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Child_Task)},
    {"Magic111Double_Run", 0x4D6320, 0x3E, kCalls4D6320, MH_N(kCalls4D6320), kImms4D6320, MH_N(kImms4D6320), nullptr, 0, reinterpret_cast<const void*>(&::Magic111Double_Run)},
    {"Magic111Double_Begin", 0x4D6360, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Double_Begin)},
    {"Magic111Double_Wait", 0x4D6380, 0x1B, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Double_Wait)},
    {"Magic111Wash_Run", 0x4D63A0, 0x31, kCalls4D63A0, MH_N(kCalls4D63A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Wash_Run)},
    {"Magic111Wash_Start", 0x4D63E0, 0x3B, kCalls4D63E0, MH_N(kCalls4D63E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Wash_Start)},
    {"Magic111Wash_Draw", 0x4D6420, 0x2E0, kCalls4D6420, MH_N(kCalls4D6420), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Wash_Draw)},
    {"Magic111Flash_Run", 0x4D6700, 0x29, kCalls4D6700, MH_N(kCalls4D6700), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Flash_Run)},
    {"Magic111Flash_Wait", 0x4D6730, 0x1E, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Flash_Wait)},
    {"Magic111Flash_Draw", 0x4D6750, 0x96, kCalls4D6750, MH_N(kCalls4D6750), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Magic111Flash_Draw)},
};
#undef MH_N

enum : unsigned {
    kMagic002_Task, kMagic002_Start, kMagic002_Wait, kMagic002Ball_Task, kMagic002Ball_Fly, kMagic002Ball_Shatter,
    kMagic002Ball_Draw, kMagic111_Task, kMagic111_Start, kMagic111_Wait, kMagic111Child_Task, kMagic111Double_Run,
    kMagic111Double_Begin, kMagic111Double_Wait, kMagic111Wash_Run, kMagic111Wash_Start, kMagic111Wash_Draw,
    kMagic111Flash_Run, kMagic111Flash_Wait, kMagic111Flash_Draw, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the callees the standard set lacks ---------------------------------------

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define C3_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define C3_RAW(address) #address, address, address
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;

// A commit logs the primitive it commits, as many bytes as it says (the
// draws build every primitive in the same bytes of the buffer, so the regions
// at the end hold only the last).
std::uint32_t LogPrimitive(const std::uint32_t* a, std::uint32_t answer) {
    mh::NoteBytes(Gfx_PacketNext, a[1] & 0xFF);
    return answer;
}

const mh::Callee kCallees[] = {
    {C3_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, kG, 0, 0},
    {C3_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0},
    {C3_OURS(Gfx_CommitPrim), 2, {kU8, kU8}, kG, 0, 0, {}, &LogPrimitive},
    {C3_OURS(Gpu_SetPolyG4), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Gpu_SetPolyF4), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, kG, 0, 0},
    {C3_OURS(Gte_VectorNormalS), 0, {}, mh::Answer::kThrough, 0, 0},
    {C3_OURS(Math_Sin), 1, {kAll}, kG, 0, 0},
    {C3_OURS(Math_Cos), 1, {kAll}, kG, 0, 0},
    // Capcom's, unnamed, in no group: the integer square root (fsqrt, _ftol).
    {C3_RAW(0x5A7A90), 1, {kAll}, kG, 0, 0},
    // This group's own, called by address: the ball's draw with its six
    // arguments (x, y, radius, spread and scale read as shorts, turn as a
    // byte), and the two full-screen draws the children tail-jump to.
    {C3_RAW(0x499FF0), 6, {kU16, kU16, kU16, kU8, kU16, kU16}, kG, 0, 0},
    {"Magic111Wash_Draw", 0x4D6420, 0x4D6420, 0, {}, mh::Answer::kPhase, 0, 0},
    {"Magic111Flash_Draw", 0x4D6750, 0x4D6750, 0, {}, mh::Answer::kPhase, 0, 0},
};

// The .data handler tables the dispatchers read in place (symbols.toml).
const mh::DataTable kTables[] = {{0x65B9C8, 3}, {0x65B9D4, 4}, {0x65B9E4, 3}};

// The packets the draws fill: Gfx_PacketNext is aimed at one of four places
// in this buffer (and moved between them by the disturbance), so a copy that
// keeps the pointer where the original reads it again shows.
alignas(16) unsigned char g_packets[0x200];
unsigned char* PacketAt(unsigned k) { return g_packets + (k & 3) * 0x40; }

constexpr std::uint32_t kAngles = 0x903850;   // MAGIC111's wash: two words
const mh::Region kRegions[] = {
    {kAngles, 4},
    {0x7E0670, 4},   // Gfx_PacketNext
    {Key(g_packets), sizeof g_packets},
};

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return mh::Pointer(mh::at::kOwner); }

// The group's cells a function reads again after a call: Gfx_PacketNext, the
// wash's two angle words, the fade byte +9 the wash reads after each sine,
// the owner's child count.
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 12) & 0xFF;
    const auto word = static_cast<std::uint16_t>(h >> 16);
    switch ((h >> 8) % 5) {
    case 0: mh::SetPointer(0x7E0670, PacketAt(v)); break;
    case 1: SetWord(mh::Mem(kAngles + (v & 1) * 2), word); break;
    case 2: Sc()[9] = static_cast<unsigned char>(word); break;
    case 3: Owner()[0xB] = static_cast<unsigned char>(word & 3); break;
    default: break;
    }
}

// --- the seed ------------------------------------------------------------------

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    mh::SetPointer(0x7E0670, PacketAt(mh::Next()));
    // Magic111_Start copies the acting actor's record: every party member
    // and every enemy the originals index (0x904B34 = 3..10).
    mh::Mem(mh::at::kActor)[0] = static_cast<unsigned char>(mh::Next() % 11);
    switch (k) {
    // the dispatchers: an index inside the table (a phase past it aborts ours)
    case kMagic002_Task: case kMagic111_Task: case kMagic111Child_Task:
        sc[1] = static_cast<unsigned char>(mh::Next() % 3);
        break;
    case kMagic002Ball_Task: sc[1] = static_cast<unsigned char>(mh::Next() % 2); break;
    case kMagic111Double_Run: case kMagic111Flash_Run: case kMagic111Wash_Run:
        sc[2] = static_cast<unsigned char>(mh::Next() % (k == kMagic111Wash_Run ? 4 : 3));
        if (mh::Half()) sc[0] = 0;
        break;
    // the waits: each side of their thresholds
    case kMagic002_Wait: {
        const std::uint32_t at = static_cast<std::uint32_t>(Long(mh::Pointer(mh::at::kCurrentSlot) + 0x80));
        if (mh::Often()) mh::Mem(at)[9] = static_cast<unsigned char>(0x27 + mh::Next() % 3);
        break;
    }
    case kMagic111_Wait: if (mh::Half()) sc[0xB] = 0; break;
    case kMagic111Double_Wait: case kMagic111Flash_Wait:
        if (mh::Often()) Owner()[0xB] = static_cast<unsigned char>(mh::Next() % 4);
        break;
    case kMagic002Ball_Fly: if (mh::Often()) sc[9] = static_cast<unsigned char>(9 + mh::Next() % 3); break;
    case kMagic002Ball_Shatter: if (mh::Often()) sc[9] = static_cast<unsigned char>(0x27 + mh::Next() % 3); break;
    default: break;
    }
}

// Magic002Ball_Draw's six words: small and on-screen half the time, anything
// the rest (the draw reads x, y, radius, spread and scale as shorts, the turn
// as a byte; the high halves must not matter).
void Args(unsigned k, std::uint32_t* a) {
    if (k != kMagic002Ball_Draw) return;
    if (mh::Half()) {
        a[0] = (a[0] & 0xFFFF0000u) | (mh::Next() % 320);
        a[1] = (a[1] & 0xFFFF0000u) | (mh::Next() % 240);
        a[2] = (a[2] & 0xFFFF0000u) | (mh::Next() % 0x80);
        a[4] = (a[4] & 0xFFFF0000u) | (0x100 + mh::Next() % 0x80);
        a[5] = (a[5] & 0xFFFF0000u) | (0x100 - (mh::Next() % 0x100));
    }
}

}  // namespace

void SelfTest() {
    const mh::Group group = {
        "magic_c3", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000,
        nullptr, 0, &Args,
    };
    mh::Run(group);
}

}  // namespace magic_c3
