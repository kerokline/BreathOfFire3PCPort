// BOF3X_SHADOW=magic_engine: the engine-side rows of Magic_Rows (0, 108, 123,
// 126, 128) and Head Cracker's rock through the spell round's shared harness
// (magic_harness.h), once at start-up. docs/magic_engine.md section 4.
//
// These lie outside the BMAGIC band, so tools/magic_rows.py --unit does not
// print their clone table: it was built by hand from a capstone descent of
// each function (every jump internal, no jump table, nothing REFUSED), with
// the band tool's clone_sites() run over the same extents for the calls and
// the [esp + k] immediates - and two immediates it does not see added by
// hand: HeadCracker_Task loads Drop's and WaitTarget's addresses with `mov
// ecx, imm32` / `mov eax, imm32` (+0x4, +0x9) and stores each three times.
//
// Beyond the standard set: the sprite callees as custom stand-ins that log
// Sprite_Current (they act on it, and these functions switch it to the
// caster and back around them), the cue player 0x437450, Battle_ActorIsOut,
// and AreaMap_Elevation with an answer that meets the rock's landing test.
// Beyond the standard regions: the current-enemy pointer 0x939AD8, a cue
// table of the fuzz's own it leads to, and the pending word 0x904B82.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_engine.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace magic_engine {
namespace {

namespace mh = magic_harness;
namespace at = magic_harness::at;

// 0x4378D0: 0x26 bytes
constexpr mh::Imm kImms4378D0[] = {{0xF, 0x47FDA0}, {0x17, 0x437900}};
// 0x47FDA0: 0x12 bytes
// 0x437900: 0x2D bytes
constexpr mh::CallSite kCalls437900[] = {{0x13, 0x4530D0}, {0x22, 0x4351F0}};
// 0x4525F0: 0x26 bytes
constexpr mh::Imm kImms4525F0[] = {{0xF, 0x452620}, {0x17, 0x452660}};
// 0x452620: 0x3B bytes
// 0x452660: 0x17 bytes
constexpr mh::CallSite kCalls452660[] = {{0x11, 0x4351F0}};
// 0x43F3B0: 0x2E bytes
constexpr mh::Imm kImms43F3B0[] = {{0xF, 0x43F3E0}, {0x17, 0x43F430}, {0x22, 0x43F460}};
// 0x43F3E0: 0x4B bytes
constexpr mh::CallSite kCalls43F3E0[] = {{0x2C, 0x437450}, {0x33, 0x5891F0}};
// 0x43F430: 0x2C bytes
constexpr mh::CallSite kCalls43F430[] = {{0x18, 0x589410}};
// 0x43F460: 0x1A bytes
constexpr mh::CallSite kCalls43F460[] = {{0x6, 0x4530D0}, {0x15, 0x4351F0}};
// 0x43FC80: 0x58 bytes; the first two by hand (mov ecx / mov eax, imm32)
constexpr mh::Imm kImms43FC80[] = {{0x4, 0x43FDC0}, {0x9, 0x43FE00}, {0x34, 0x43FCE0},
                                   {0x3C, 0x43FD20}, {0x44, 0x43FD80}, {0x4C, 0x43FE80}};
// 0x43FCE0: 0x3E bytes
constexpr mh::CallSite kCalls43FCE0[] = {{0x1A, 0x5891F0}};
// 0x43FD20: 0x53 bytes
constexpr mh::CallSite kCalls43FD20[] = {{0x1D, 0x587740}, {0x46, 0x589410}};
// 0x43FD80: 0x35 bytes
constexpr mh::CallSite kCalls43FD80[] = {{0x18, 0x589410}};
// 0x43FDC0: 0x3E bytes
constexpr mh::CallSite kCalls43FDC0[] = {{0x5, 0x435180}};
// 0x43FE00: 0x75 bytes
constexpr mh::CallSite kCalls43FE00[] = {{0x13, 0x4456C0}};
// 0x43FE80: 0xC bytes
constexpr mh::CallSite kCalls43FE80[] = {{0x7, 0x4351F0}};
// 0x43FE90: 0x1A bytes
constexpr mh::Imm kImms43FE90[] = {{0xD, 0x43FEB0}};
// 0x43FEB0: 0x43 bytes
constexpr mh::CallSite kCalls43FEB0[] = {{0x35, 0x5893A0}, {0x3A, 0x588F20}};
constexpr mh::Imm kImms43FEB0[] = {{0xF, 0x43FF00}, {0x17, 0x440010}, {0x22, 0x440060}};
// 0x43FF00: 0x110 bytes
constexpr mh::CallSite kCalls43FF00[] = {{0x5, 0x589590}, {0xFF, 0x5891F0}};
// 0x440010: 0x4C bytes
constexpr mh::CallSite kCalls440010[] = {{0x2A, 0x5720C0}};
// 0x440060: 0x1B bytes
constexpr mh::CallSite kCalls440060[] = {{0x6, 0x4530D0}, {0x16, 0x4351F0}};
#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define ME_FN(name) reinterpret_cast<const void*>(&::name)
const mh::Clone kClones[] = {
    {"MagicHold_Task", 0x4378D0, 0x26, nullptr, 0, kImms4378D0, MH_N(kImms4378D0), nullptr, 0, ME_FN(MagicHold_Task)},
    {"Task_StartHold60", 0x47FDA0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, ME_FN(Task_StartHold60)},
    {"MagicHold_Countdown", 0x437900, 0x2D, kCalls437900, MH_N(kCalls437900), nullptr, 0, nullptr, 0, ME_FN(MagicHold_Countdown)},
    {"RestoreForm_Task", 0x4525F0, 0x26, nullptr, 0, kImms4525F0, MH_N(kImms4525F0), nullptr, 0, ME_FN(RestoreForm_Task)},
    {"RestoreForm_Start", 0x452620, 0x3B, nullptr, 0, nullptr, 0, nullptr, 0, ME_FN(RestoreForm_Start)},
    {"RestoreForm_Wait", 0x452660, 0x17, kCalls452660, MH_N(kCalls452660), nullptr, 0, nullptr, 0, ME_FN(RestoreForm_Wait)},
    {"Paralyzer_Task", 0x43F3B0, 0x2E, nullptr, 0, kImms43F3B0, MH_N(kImms43F3B0), nullptr, 0, ME_FN(Paralyzer_Task)},
    {"Paralyzer_Start", 0x43F3E0, 0x4B, kCalls43F3E0, MH_N(kCalls43F3E0), nullptr, 0, nullptr, 0, ME_FN(Paralyzer_Start)},
    {"MagicFx_WaitOwnerAnim", 0x43F430, 0x2C, kCalls43F430, MH_N(kCalls43F430), nullptr, 0, nullptr, 0, ME_FN(MagicFx_WaitOwnerAnim)},
    {"MagicFx_FlagTargetEnd", 0x43F460, 0x1A, kCalls43F460, MH_N(kCalls43F460), nullptr, 0, nullptr, 0, ME_FN(MagicFx_FlagTargetEnd)},
    {"HeadCracker_Task", 0x43FC80, 0x58, nullptr, 0, kImms43FC80, MH_N(kImms43FC80), nullptr, 0, ME_FN(HeadCracker_Task)},
    {"HeadCracker_Start", 0x43FCE0, 0x3E, kCalls43FCE0, MH_N(kCalls43FCE0), nullptr, 0, nullptr, 0, ME_FN(HeadCracker_Start)},
    {"HeadCracker_Windup", 0x43FD20, 0x53, kCalls43FD20, MH_N(kCalls43FD20), nullptr, 0, nullptr, 0, ME_FN(HeadCracker_Windup)},
    {"HeadCracker_WaitCaster", 0x43FD80, 0x35, kCalls43FD80, MH_N(kCalls43FD80), nullptr, 0, nullptr, 0, ME_FN(HeadCracker_WaitCaster)},
    {"HeadCracker_Drop", 0x43FDC0, 0x3E, kCalls43FDC0, MH_N(kCalls43FDC0), nullptr, 0, nullptr, 0, ME_FN(HeadCracker_Drop)},
    {"HeadCracker_WaitTarget", 0x43FE00, 0x75, kCalls43FE00, MH_N(kCalls43FE00), nullptr, 0, nullptr, 0, ME_FN(HeadCracker_WaitTarget)},
    {"MagicFx_DoneAndFree", 0x43FE80, 0xC, kCalls43FE80, MH_N(kCalls43FE80), nullptr, 0, nullptr, 0, ME_FN(MagicFx_DoneAndFree)},
    {"HeadCrackerRock_Task", 0x43FE90, 0x1A, nullptr, 0, kImms43FE90, MH_N(kImms43FE90), nullptr, 0, ME_FN(HeadCrackerRock_Task)},
    {"HeadCrackerRock_Run", 0x43FEB0, 0x43, kCalls43FEB0, MH_N(kCalls43FEB0), kImms43FEB0, MH_N(kImms43FEB0), nullptr, 0,
     ME_FN(HeadCrackerRock_Run)},
    {"HeadCrackerRock_Start", 0x43FF00, 0x110, kCalls43FF00, MH_N(kCalls43FF00), nullptr, 0, nullptr, 0, ME_FN(HeadCrackerRock_Start)},
    {"HeadCrackerRock_Fall", 0x440010, 0x4C, kCalls440010, MH_N(kCalls440010), nullptr, 0, nullptr, 0, ME_FN(HeadCrackerRock_Fall)},
    {"HeadCrackerRock_Land", 0x440060, 0x1B, kCalls440060, MH_N(kCalls440060), nullptr, 0, nullptr, 0, ME_FN(HeadCrackerRock_Land)},
};
#undef ME_FN
#undef MH_N
enum : unsigned {
    kMagicHold_Task,
    kTask_StartHold60,
    kMagicHold_Countdown,
    kRestoreForm_Task,
    kRestoreForm_Start,
    kRestoreForm_Wait,
    kParalyzer_Task,
    kParalyzer_Start,
    kMagicFx_WaitOwnerAnim,
    kMagicFx_FlagTargetEnd,
    kHeadCracker_Task,
    kHeadCracker_Start,
    kHeadCracker_Windup,
    kHeadCracker_WaitCaster,
    kHeadCracker_Drop,
    kHeadCracker_WaitTarget,
    kMagicFx_DoneAndFree,
    kHeadCrackerRock_Task,
    kHeadCrackerRock_Run,
    kHeadCrackerRock_Start,
    kHeadCrackerRock_Fall,
    kHeadCrackerRock_Land,
    kCount,
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one role per clone");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// --- the sprite callees: each acts on Sprite_Current, so its stand-in logs it --

std::uint32_t FlagAnswer() {
    const std::uint32_t h = mh::Noise();
    return h % 3 == 0 ? h & 0xFFFFFF00u : h | 0x10;
}
void __cdecl RecSetAnimation(std::uint32_t animation) {
    mh::Record(bof3::addr::Sprite_SetAnimation, animation & 0xFF, Key(Sprite_Current));
    mh::Stir();
}
std::uint32_t __cdecl RecSetAnimationBank(std::uint32_t bank) {
    mh::Record(bof3::addr::Sprite_SetAnimationBank, bank & 0xFFFF, Key(Sprite_Current));
    mh::Stir();
    return mh::Noise();
}
std::uint32_t __cdecl RecTickOnce() {
    mh::Record(bof3::addr::Sprite_ScriptTickOnce, Key(Sprite_Current));
    mh::Stir();
    return FlagAnswer();
}
std::uint32_t __cdecl RecTick() {
    mh::Record(bof3::addr::Sprite_ScriptTick, Key(Sprite_Current));
    mh::Stir();
    return FlagAnswer();
}
void __cdecl RecUpdateScreen() {
    mh::Record(bof3::addr::Sprite_UpdateScreen, Key(Sprite_Current));
    mh::Stir();
}

// AreaMap_Elevation's answer (ax, signed, is read): a third of the time the
// ground a rock at the current Sprite_Current's height is one above, at or
// one below - so the landing test meets both sides of its bound.
std::uint32_t GroundAnswer(const std::uint32_t*, std::uint32_t answer) {
    if (answer % 3) return answer;
    const int height = static_cast<short>(move_script::Word(Sprite_Current + 0x3E));
    const int ground = height - 0x180 + static_cast<int>((answer >> 2) % 3) - 1;
    return (answer & 0xFFFF0000u) | (static_cast<std::uint32_t>(ground) & 0xFFFF);
}

// Battle_ActorIsOut: a kFlag recorder answers 0 exactly when its own
// disturbance did nothing (both come from one hash), so a caller's read after
// a "not out" answer never saw a moved cell - HeadCracker_WaitTarget's
// re-read of the target was invisible (control T3). This answers from its own
// stream instead, and a quarter of the time moves the target to any of the
// eleven actors (whose state bytes the seed makes 6 half the time).
std::uint32_t OutAnswer(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t h = mh::Noise();
    if (h % 4 == 0) mh::Mem(at::kTarget)[0] = static_cast<unsigned char>((h >> 8) % 11);
    return (h >> 4) % 3 == 0 ? answer | 0x10 : answer & 0xFFFFFF00u;
}

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define ME_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define ME_CUSTOM(f) mh::Answer::kGarbage, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&f)
const mh::Callee kCallees[] = {
    {ME_OURS(Sprite_SetAnimation), 0, {}, ME_CUSTOM(RecSetAnimation)},
    {ME_OURS(Sprite_SetAnimationBank), 0, {}, ME_CUSTOM(RecSetAnimationBank)},
    {ME_OURS(Sprite_ScriptTickOnce), 0, {}, ME_CUSTOM(RecTickOnce)},
    {ME_OURS(Sprite_ScriptTick), 0, {}, ME_CUSTOM(RecTick)},
    {ME_OURS(Sprite_UpdateScreen), 0, {}, ME_CUSTOM(RecUpdateScreen)},
    {ME_OURS(Battle_ActorIsOut), 1, {kU8}, mh::Answer::kFlag, 0, 0, {}, &OutAnswer},
    {ME_OURS(AreaMap_Elevation), 2, {kAll, kAll}, mh::Answer::kGarbage, 0, 0, {}, &GroundAnswer},
    // Sound_PlayEffect(id) unless id is 0xFFFF: reads the low word
    {"0x437450", 0x437450, 0x437450, 1, {kU16}, mh::Answer::kGarbage, 0, 0},
};
#undef ME_CUSTOM
#undef ME_OURS

// The regions the standard ones lack.
constexpr std::uint32_t kCurrentEnemy = 0x939AD8;
constexpr std::uint32_t kPending = 0x904B82;
alignas(4) unsigned char g_cues[16];
mh::Region g_regions[3];

// A record the current-enemy pointer may name: an enemy object or one of the
// harness's sprite records (both hold +0xF8).
unsigned char* EnemyLike(std::uint32_t v) { return v & 8 ? mh::SpriteRecord(v) : mh::EnemyOf(static_cast<unsigned char>(3 + v % 8)); }

void Seed(unsigned k) {
    unsigned char* const sc = Sprite_Current;
    // Every round: the current enemy points at a record whose +0xF8 leads into
    // the fuzz's cue table (Paralyzer_Start reads through both, unchecked).
    unsigned char* const enemy = EnemyLike(mh::Next());
    mh::SetPointer(kCurrentEnemy, enemy);
    mh::SetPointer(static_cast<std::uint32_t>(Key(enemy)) + 0xF8, g_cues + 2 * (mh::Next() % 4));
    switch (k) {
    case kMagicHold_Task:
    case kRestoreForm_Task:
        sc[1] = static_cast<unsigned char>(mh::Next() % 2);
        break;
    case kMagicHold_Countdown:
        if (mh::Often()) sc[9] = static_cast<unsigned char>(MH_PICK(0, 0, 1, 2, 0xFF));
        break;
    case kRestoreForm_Wait:
        if (mh::Half()) move_script::SetWord(mh::Mem(kPending), 0);
        else if (mh::Half()) move_script::SetWord(mh::Mem(kPending), MH_PICK(1, 0x100, 0x400, 0x8000));
        break;
    case kParalyzer_Task:
        sc[1] = static_cast<unsigned char>(mh::Next() % 3);
        break;
    case kParalyzer_Start:
        // a cue whose +4 is 0xFFFF (0x437450 plays nothing), wraps, or is 0
        for (unsigned i = 0; i < 4; ++i)
            if (mh::Half()) move_script::SetWord(g_cues + 2 * i, MH_PICK(0xFFFB, 0xFFFC, 0xFFFF, 0, 0x100));
        break;
    case kHeadCracker_Task:
        sc[1] = static_cast<unsigned char>(mh::Next() % 10);
        break;
    case kHeadCracker_Windup:
        if (mh::Often())
            move_script::SetLong(sc + 0xC, static_cast<std::int32_t>(MH_PICK(1, 1, 0, 2, 0x80000000u, 0xFFFFFFFFu, 0x10000)));
        break;
    case kHeadCracker_WaitTarget: {
        if (mh::Often()) sc[0xA] = 0;
        else if (mh::Half()) sc[0xA] = static_cast<unsigned char>(MH_PICK(1, 0xFF));
        // every actor's state 6 half the time, so a target moved by the
        // stand-ins lands on either side of the test
        for (unsigned t = 0; t < 11; ++t) {
            unsigned char* const actor = t < 3 ? mh::PartyOf(static_cast<unsigned char>(t)) : mh::EnemyOf(static_cast<unsigned char>(t));
            if (mh::Half()) actor[1] = 6;
            else if (mh::Half()) actor[1] = static_cast<unsigned char>(MH_PICK(5, 7, 0, 0x86));
        }
        break;
    }
    case kHeadCrackerRock_Task:
        sc[1] = 0;
        break;
    case kHeadCrackerRock_Run:
        sc[2] = static_cast<unsigned char>(mh::Next() % 3);
        break;
    case kHeadCrackerRock_Start:
        mh::Mem(at::kTarget)[0] = static_cast<unsigned char>(MH_PICK(0, 1, 2, 2, 3, 3, 4, 10));
        break;
    case kHeadCrackerRock_Fall:
        // heights near the sign bit and speeds that carry across it
        if (mh::Half()) move_script::SetWord(sc + 0x3E, MH_PICK(0x7FFF, 0x8000, 0x800, 0, 0xFFFF));
        if (mh::Half()) move_script::SetLong(sc + 0x20, -16);
        break;
    default:
        break;
    }
}

}  // namespace

void SelfTest() {
    unsigned r = 0;
    g_regions[r++] = {kCurrentEnemy, 4};
    g_regions[r++] = {kPending, 2};
    g_regions[r++] = {Key(g_cues), sizeof g_cues};
    const mh::Group group = {
        "magic_engine", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        nullptr, 0, g_regions, r, &Seed, nullptr, 2000,
    };
    mh::Run(group);
}

}  // namespace magic_engine
