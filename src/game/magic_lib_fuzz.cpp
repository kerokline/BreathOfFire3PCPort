// BOF3X_SHADOW=magic_lib: the effect library through the spell round's shared
// harness (magic_harness.h), once at start-up. docs/magic_lib.md section 3.
//
// The library's functions, unlike a spell's phases, take arguments and
// answer, and three of their callees cannot be recorders that answer garbage:
// Gte_VectorNormalS writes its answer through a pointer (run for real, what
// it read noted), Battle_ActorIsOut must leave someone standing (every actor
// out is a division by zero, in the original as in ours), and the CLUT
// helpers' kinds 5..7 divide by zero. So the group sets Group::args, each
// clone's ret_mask, and an effect on four callees (magic_harness.md section 7).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_lib.h"
#include "game/move_script_bytes.h"

namespace magic_lib {
namespace {

namespace mh = magic_harness;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// tools/magic_rows.py --unit LIBRARY --clones, 2026-09-25 (capstone: every
// jump internal, no jump table; the two popup tasks' four stack-table
// immediates each). 0x4FAF90 is MAGIC226/227's (docs/magic_lib.md section 1).
constexpr mh::CallSite kCalls4FB0A0[] = {{0x47, 0x4FB3E0}};
constexpr mh::Imm kImms4FB0A0[] = {{0xF, 0x4FB100}, {0x17, 0x4FB190}, {0x22, 0x4FB1F0}, {0x2A, 0x4FB230}};
constexpr mh::CallSite kCalls4FB100[] = {{0x47, 0x4FBD10}};
constexpr mh::CallSite kCalls4FB230[] = {{0x20, 0x4351F0}};
constexpr mh::CallSite kCalls4FB260[] = {{0x47, 0x4FB3E0}};
constexpr mh::Imm kImms4FB260[] = {{0xF, 0x4FB2C0}, {0x17, 0x4FB190}, {0x22, 0x4FB1F0}, {0x2A, 0x4FB230}};
constexpr mh::CallSite kCalls4FB2C0[] = {{0xCC, 0x4FBD10}};
constexpr mh::CallSite kCalls4FB3E0[] = {{0x11, 0x5A77C0}, {0x1A, 0x461E50}, {0x26, 0x5A75D0}, {0xF8, 0x5A79A0},
                                         {0x108, 0x5A79E0}, {0x192, 0x461E50}, {0x1A1, 0x5A75D0}, {0x26C, 0x5A79A0},
                                         {0x27C, 0x5A79E0}, {0x2F8, 0x461E50}};
constexpr mh::CallSite kCalls4FB6F0[] = {{0x76, 0x44FC10}};
constexpr mh::CallSite kCalls4FB790[] = {{0x21, 0x4FB6F0}, {0x44, 0x4FB6F0}, {0x56, 0x435180}};
constexpr mh::CallSite kCalls4FB880[] = {{0x101, 0x5A7560}};
constexpr mh::CallSite kCalls4FB9F0[] = {{0x44, 0x5A8C00}};
constexpr mh::CallSite kCalls4FBA90[] = {{0x4F, 0x5A8C00}};
constexpr mh::CallSite kCalls4FBB40[] = {{0x35, 0x5A7A70}, {0x52, 0x5A7A50}, {0x71, 0x5A7A00}};
constexpr mh::CallSite kCalls4FC0E0[] = {{0x39, 0x4456C0}, {0x86, 0x4456C0}};
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
// The last field is what of eax is compared (Clone::ret_mask): 0 for a
// function that answers nothing.
#define ML_C(name, base, size, calls, ret) \
    {#name, base, size, calls, MH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), ret}
#define ML_P(name, base, size, ret) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), ret}
const mh::Clone kClones[] = {
    {"BuffPopup_Task", 0x4FB0A0, 0x53, kCalls4FB0A0, MH_N(kCalls4FB0A0), kImms4FB0A0, MH_N(kImms4FB0A0), nullptr, 0,
     reinterpret_cast<const void*>(&::BuffPopup_Task)},
    ML_C(BuffPopup_Start, 0x4FB100, 0x8D, kCalls4FB100, 0),
    ML_P(BuffPopup_Rise, 0x4FB190, 0x55, 0),
    ML_P(BuffPopup_Fall, 0x4FB1F0, 0x37, 0),
    ML_C(BuffPopup_End, 0x4FB230, 0x26, kCalls4FB230, 0),
    {"BuffPopupAt_Task", 0x4FB260, 0x53, kCalls4FB260, MH_N(kCalls4FB260), kImms4FB260, MH_N(kImms4FB260), nullptr, 0,
     reinterpret_cast<const void*>(&::BuffPopupAt_Task)},
    ML_C(BuffPopupAt_Start, 0x4FB2C0, 0x112, kCalls4FB2C0, 0),
    ML_C(BuffPopup_Draw, 0x4FB3E0, 0x304, kCalls4FB3E0, 0),
    ML_C(MagicFx_ApplyBuff, 0x4FB6F0, 0x9C, kCalls4FB6F0, 0xFF),
    ML_C(MagicFx_BuffPopup, 0x4FB790, 0x98, kCalls4FB790, 0),
    ML_C(MagicFx_LinkByDepth, 0x4FB880, 0x16F, kCalls4FB880, 0),
    ML_C(MagicFx_StepToward, 0x4FB9F0, 0x9D, kCalls4FB9F0, 0),
    ML_C(MagicFx_StepTowardPoint, 0x4FBA90, 0xA8, kCalls4FBA90, 0),
    ML_C(MagicFx_StepAround, 0x4FBB40, 0x8B, kCalls4FBB40, kAll),
    ML_P(MagicFx_NearSprite3D, 0x4FBBD0, 0x5A, kAll),
    ML_P(MagicFx_NearSprite, 0x4FBC30, 0x40, kAll),
    ML_P(MagicFx_NearPoint3D, 0x4FBC70, 0x57, kAll),
    ML_P(MagicFx_NearPoint, 0x4FBCD0, 0x3B, kAll),
    ML_P(SpriteClut_SetStp, 0x4FBE30, 0xA0, 0),
    ML_P(SpriteClut_ClearEntry31, 0x4FBED0, 0x72, 0),
    ML_P(SpriteClut_CopyToFxRow, 0x4FBF50, 0xAF, kAll),
    ML_P(SpriteClut_RestoreFxRow, 0x4FC000, 0x21, 0),
    ML_C(MagicFx_CenterOnSide, 0x4FC0E0, 0x102, kCalls4FC0E0, 0),
    ML_P(BattleActor_FxSizeB, 0x4FC260, 0x6B, 0xFF),
    ML_P(MagicFx_FormationOffset, 0x4FC2D0, 0x52, 0),
};
#undef ML_C
#undef ML_P
enum : unsigned {
    kTask, kStart, kRise, kFall, kEnd, kAtTask, kAtStart, kDraw, kApply, kPopup, kLink, kToward, kTowardPoint, kAround,
    kNear3D, kNear, kNearPoint3D, kNearPoint, kSetStp, kClear31, kCopyRow, kRestoreRow, kCenter, kSizeB, kFormation,
    kCount
};
static_assert(sizeof kClones / sizeof kClones[0] == kCount, "one enum per clone");

unsigned g_out;          // Battle_ActorIsOut's answers, a bit per actor 0..10

// --- the effects (callees' recorders that compute their answers) --------------------------------------------------------------------

std::uint32_t VectorNormalAct(const std::uint32_t* a, std::uint32_t) {
    const auto* const in = reinterpret_cast<const long*>(static_cast<std::uintptr_t>(a[0]));
    mh::Note(static_cast<std::uint32_t>(in[0]), static_cast<std::uint32_t>(in[1]), static_cast<std::uint32_t>(in[2]), 0);
    return static_cast<std::uint32_t>(::Gte_VectorNormalS(in, reinterpret_cast<short*>(static_cast<std::uintptr_t>(a[1]))));
}
std::uint32_t ActorIsOutAct(const std::uint32_t* a, std::uint32_t) { return 0x5A5A5A00u | ((g_out >> ((a[0] & 0xFF) % 11)) & 1u); }
// Gfx_CommitPrim moves the packet pointer on by the size, as the real one
// does when the pool has room, so two primitives in a row land apart.
std::uint32_t CommitAct(const std::uint32_t* a, std::uint32_t) {
    unsigned char* const cell = mh::Mem(0x7E0670);
    move_script::SetLong(cell, move_script::Long(cell) + static_cast<std::int32_t>(a[1] & 0xFF));
    return 0;
}
// The buff roll: what it would read - the target byte, the result record
// pointer and the stats copy - noted; the answer a flag from the stat.
std::uint32_t BuffRollAct(const std::uint32_t* a, std::uint32_t) {
    mh::Note(mh::Mem(0x904B54)[0], static_cast<std::uint32_t>(move_script::Long(mh::Mem(0x904B60))),
             static_cast<std::uint32_t>(move_script::Long(mh::Mem(0x939F80))),
             static_cast<std::uint32_t>(move_script::Long(mh::Mem(0x939F9C))));
    const std::uint32_t h = (a[0] & 0xFF) * 0x9E3779B1u + mh::Mem(0x904B54)[0];
    return (h >> 9) % 3 == 0 ? h & 0xFFFFFF00u : h | 0x10;
}

// The callees the standard set lacks. Gte_VectorNormalS's two pointers are
// each function's own frame (masked off; its act notes what `in` held).
#define ML_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
const mh::Callee kCallees[] = {
    {ML_OURS(BuffPopup_Draw), 1, {kU8}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &CommitAct},
    {ML_OURS(Gpu_SetPolyFT4), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Gpu_GetTPage), 4, {kAll, kAll, kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Gpu_GetClut), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Gpu_LinkPrim), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Gte_VectorNormalS), 2, {0, 0}, mh::Answer::kGarbage, 0, 0, {}, &VectorNormalAct},
    {ML_OURS(Math_Ratan2), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Math_Cos), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Math_Sin), 1, {kAll}, mh::Answer::kGarbage, 0, 0},
    {ML_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kGarbage, 0, 0, {}, &ActorIsOutAct},
    // the buff roll: 1 in al when resisted - unnamed, the engine's
    {"0x44FC10", 0x44FC10, 0x44FC10, 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &BuffRollAct},
};
#undef ML_OURS

// --- the state -----------------------------------------------------------------

constexpr std::uint32_t kPacketNext = 0x7E0670, kOrigin = 0x7E0688, kBufferIndex = 0x905B89;
constexpr std::uint32_t kPackets = 0x7E1C00;   // the first pool's start (Gfx_PacketPools)
constexpr std::uint32_t kPoolEnd = 0x7F1BAC;
constexpr std::uint32_t kStrip = 0x80F580;
constexpr unsigned kStripWords = 0x2000;
constexpr std::uint32_t kFxRowWord = 0x200;    // 0x80F980, the strip's row 2

int g_keys[16];

const mh::Region kRegions[] = {
    {kPacketNext, 4}, {kOrigin, 4}, {0x905B88, 4}, {0x8022C0, 0xA90}, {0x904B50, 0x40}, {0x939F80, 0x20},
    {kPackets, 0x200}, {0x8C5652, 0x8C * 16}, {kStrip, kStripWords * 2}, {0x80B980, 0x200}, {Key(g_keys), sizeof g_keys},
};

// --- the seed ---------------------------------------------------------------------

void SetU32(unsigned char* p, std::uint32_t v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }
std::uint32_t U32(const unsigned char* p) { return static_cast<std::uint32_t>(move_script::Long(p)); }
unsigned char* Record() { return mh::Half() ? mh::SpriteRecord(mh::Next()) : mh::TaskAt(mh::Next()); }

// The arguments are drawn in Seed (the state after the fill) and handed out
// by Args.
std::uint32_t g_args[8];
bool g_have_args;

// A CLUT record: kind 0..4 (5..7 divide by zero, both sides), an index whose
// cell and count stay inside the strip; for the copy, now and then a cell
// just below row 2 so source and target overlap.
void SeedClutRecord(unsigned char* s, unsigned k) {
    static const unsigned char kCounts[5] = {0x01, 0x10, 0x02, 0x04, 0x08};
    static const unsigned char kDivisors[5] = {0x10, 0x01, 0x08, 0x04, 0x02};
    static const unsigned char kMults[5] = {0x10, 0x00, 0x20, 0x40, 0x80};
    for (;;) {
        const unsigned kind = mh::Next() % 5;
        s[0x28] = static_cast<unsigned char>(kind);
        s[0x27] = static_cast<unsigned char>(mh::Next());
        if (mh::Half()) s[0x24] = static_cast<unsigned char>(s[0x24] & ~4u);
        if (k == kCopyRow && mh::Next() % 4 == 0) {
            s[0x24] = static_cast<unsigned char>(s[0x24] & ~4u);
            s[0x27] = static_cast<unsigned char>(kDivisors[kind] * (mh::Next() % 3));   // rows 0..2
        }
        unsigned row = s[0x27] / kDivisors[kind];
        const unsigned col = ((s[0x27] % kDivisors[kind]) * kMults[kind]) & 0xFF;
        if (s[0x24] & 4) row = (row + 0x10) & 0xFF;
        const unsigned cell = row * 0x100 + col;
        const unsigned span = k == kClear31 ? 32 : kCounts[kind] * 16u;
        if (cell + span <= kStripWords && (k != kCopyRow || kFxRowWord + span <= kStripWords)) return;
    }
}

void Seed(unsigned k) {
    unsigned char* const sc = Sprite_Current;
    for (std::uint32_t& w : g_args) w = mh::Next();
    g_have_args = true;
    mh::Mem(kBufferIndex)[0] = static_cast<unsigned char>(mh::Next() % 2);
    switch (k) {
    case kTask:
    case kAtTask:
        sc[1] = static_cast<unsigned char>(mh::Next() % 4);
        if (mh::Next() % 4 == 0) sc[0] = 0;
        break;
    case kStart:
    case kAtStart:
        if (mh::Often()) sc[9] = 1;
        sc[0xB] = static_cast<unsigned char>(mh::Next() % 11);
        break;
    case kRise:
        sc[0xA] = static_cast<unsigned char>(MH_PICK(6, 7, 8, 9, 0xFF));
        if (mh::Often()) sc[9] = 0xB;
        break;
    case kFall:
        if (mh::Often()) sc[9] = 0xF;
        break;
    case kEnd:
        if (mh::Often()) sc[9] = 0x1B;
        break;
    case kDraw:
        SetU32(mh::Mem(kPacketNext), kPackets + mh::Next() % 0x40);
        g_args[0] = (g_args[0] & 0xFFFFFF00u) | mh::Next() % 9;
        break;
    case kApply:
        g_args[1] = (g_args[1] & 0xFFFFFF00u) | mh::Next() % 11;
        break;
    case kPopup:
        // bit 6 of the target: who is an enemy
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(mh::Half() ? mh::Next() % 11 : 0x40 | mh::Next());
        break;
    case kLink: {
        // the depth near its bounds half the time, the pool's end near the
        // next primitive half the time
        for (int& key : g_keys) key = mh::Often() ? static_cast<int>(mh::Next() % 5) - 1 : static_cast<int>(mh::Next());
        const unsigned count = mh::Next() % 13;
        const unsigned size = mh::Next() & 0xFF;
        const unsigned bias = mh::Next() & 0xFF;
        const short ox = static_cast<short>(mh::Next() % 64 - 32), oz = static_cast<short>(mh::Next() % 64 - 32);
        move_script::SetWord(mh::Mem(kOrigin), static_cast<unsigned short>(ox));
        move_script::SetWord(mh::Mem(kOrigin + 2), static_cast<unsigned short>(oz));
        std::uint32_t x = (mh::Next() % 64) << 16 | (mh::Next() % 4 == 0 ? 0 : mh::Next() & 0xFFFF);
        std::uint32_t z = mh::Next() % 4 == 0 ? 0 : mh::Next() & 0xFFFF;
        const int want = mh::Half() ? static_cast<int>(MH_PICK(static_cast<std::uint32_t>(-1), 0, 1, 0x36, 0x37, 0x38))
                                    : static_cast<int>(mh::Next() % 0x38);
        const int hx = static_cast<short>(x >> 16);
        const int hz = want - hx + ((z & 0xFFFF) == 0) + ((x & 0xFFFF) == 0) + oz + ox - static_cast<int>(bias) - 2;
        z |= static_cast<std::uint32_t>(hz) << 16;
        if (mh::Next() % 8 == 0) x = mh::Next(), z = mh::Next();
        const std::uint32_t end = kPoolEnd + (static_cast<std::uint32_t>(mh::Mem(kBufferIndex)[0]) << 16);
        SetU32(mh::Mem(kPacketNext),
               mh::Half() ? kPackets + (mh::Next() & 0xFFF) : end - size + MH_PICK(static_cast<std::uint32_t>(-1), 0, 1, 2));
        g_args[0] = x;
        g_args[1] = z;
        g_args[2] = Key(g_keys);
        g_args[4] = (g_args[4] & 0xFFFFFF00u) | count;
        g_args[5] = (g_args[5] & 0xFFFFFF00u) | size;
        g_args[6] = (g_args[6] & 0xFFFFFF00u) | bias;
        break;
    }
    case kToward:
    case kAround:
        g_args[0] = Key(Record());
        break;
    case kTowardPoint:
        break;
    case kNear3D:
    case kNear:
    case kNearPoint3D:
    case kNearPoint: {
        // the other point within +-size of Sprite_Current, at the edges often:
        // the test is (mine - other + size / 2) <= size, so other = mine +
        // size / 2 - d passes exactly when d <= size
        const std::uint32_t size = mh::Half() ? MH_PICK(0x60000, 0xC000, 0x10000, 0x200, 1, 0) : mh::Next();
        const std::uint32_t half = size >> 1;
        const auto near = [&](std::uint32_t mine) {
            const std::uint32_t edges[4] = {0, size, size + 1, size - 1};
            const std::uint32_t d = mh::Next() % 5 == 0 ? mh::Next()
                                    : mh::Half()        ? edges[mh::Next() % 4]
                                    : size != 0         ? mh::Next() % size
                                                        : 0;
            return mine + half - d;
        };
        const std::uint32_t x = near(U32(sc + 0x34)), z = near(U32(sc + 0x38));
        // the height: ((size >> 9) + mine - other) & 0xFFFF against size >> 8
        const std::uint32_t h = mh::Half() ? (size >> 8) + MH_PICK(0, 1) : mh::Next() % ((size >> 8) % 0x10000 + 2);
        const unsigned yl = ((size >> 9) + move_script::Word(sc + 0x3E) - h) & 0xFFFF;
        if (k == kNear3D || k == kNear) {
            unsigned char* const other = Record();
            SetU32(other + 0x34, x);
            SetU32(other + 0x38, z);
            move_script::SetWord(other + 0x3E, yl);
            g_args[0] = Key(other);
            g_args[1] = size;
        } else {
            g_args[0] = x;
            g_args[1] = z;
            g_args[2] = (g_args[2] & 0xFFFF) | yl << 16;
            g_args[k == kNearPoint3D ? 3 : 2] = size;
        }
        break;
    }
    case kSetStp:
    case kClear31:
    case kCopyRow: {
        unsigned char* const s = Record();
        SeedClutRecord(s, k);
        g_args[0] = Key(s);
        break;
    }
    case kCenter: {
        mh::Mem(mh::at::kTarget)[0] = static_cast<unsigned char>(MH_PICK(0, 1, 2, 0x40, 0x43, 0x80, 0x81, 0xC0, 0xC3, 0x7F));
        // at most one of the party out and two enemies standing: every one
        // out is a division by zero (both sides), and this leaves a control
        // that drops one actor a count, not an abort
        do g_out = mh::Next() & 0x7FF;
        while (__builtin_popcount(g_out & 7) > 1 || __builtin_popcount(g_out >> 3) > 6);
        break;
    }
    case kSizeB:
        mh::Mem(mh::at::kActor)[0] = static_cast<unsigned char>(mh::Next() % 11);
        // the enemy's type among the first 16 rows of its table (a region)
        for (unsigned e = 0; e < 8; ++e) mh::EnemyOf(static_cast<unsigned char>(e + 3))[0xF0] = static_cast<unsigned char>(mh::Next() % 16);
        if (mh::Often()) mh::Mem(0x904B89)[0] = static_cast<unsigned char>(mh::Next() % 28);
        break;
    case kFormation:
        if (mh::Often()) mh::Mem(0x904B89)[0] = static_cast<unsigned char>(mh::Next() % 28);
        break;
    default:
        break;
    }
}

void Args(unsigned, std::uint32_t* a) {
    if (!g_have_args) return;
    std::memcpy(a, g_args, sizeof g_args);
    g_have_args = false;
}

// The group's cells a caller may read again after a call: the packet
// pointer (between two cells of the pool), the buffer index, the ability
// target and result record.
void Disturb(std::uint32_t h) {
    switch ((h >> 8) % 4) {
    case 0: SetU32(mh::Mem(kPacketNext), kPackets + ((h >> 12) & 1) * 0x80 + ((h >> 16) & 0x3F)); break;
    case 1: mh::Mem(kBufferIndex)[0] = static_cast<unsigned char>((h >> 12) & 1); break;
    case 2: mh::Mem(0x904B54)[0] = static_cast<unsigned char>(h >> 16); break;
    default: SetU32(mh::Mem(0x904B60), h); break;
    }
}

}  // namespace

void SelfTest() {
    const mh::Group group = {
        "magic_lib", kClones, kCount, kCallees, sizeof kCallees / sizeof kCallees[0], nullptr, 0,
        kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000, nullptr, 0, &Args,
    };
    mh::Run(group);
}

}  // namespace magic_lib
