// BOF3X_SHADOW=magic_s05: group S05's two units (MAGIC017, MAGIC018/019)
// through the spell round's shared harness (magic_harness.h), once at
// start-up. docs/magic_s05.md section 4.
//
// The clone table is tools/magic_rows.py --unit MAGIC017 / MAGIC018/MAGIC019
// --clones (2026-09-26; capstone, every jump internal, no jump table, no
// REFUSED line), names given. Beyond the standard set this group lists two of
// Capcom's unnamed callees and three of the standard ones recorded its own
// way. Everything the harness lacks is built here, not in the harness:
//
//   - 0x446770 (the dx / dz turn) logs the task's direction and pair, and
//     writes a new pair where the real one writes, so what the caller reads
//     back is compared (group S31's);
//   - 0x435A70 (an enemy's animation by battle index) logs its two bytes;
//   - Sprite_ScriptTickOnce and Sprite_UpdateScreen act on Sprite_Current:
//     their stand-ins log which sprite, and the tick answers from its own
//     stream, not the disturbance's (the kFlag blind spot, group E's
//     workaround: a kFlag recorder answers 0 exactly when its disturbance did
//     nothing, so a re-read after a "no" would never be tested);
//   - BattleActor_FxSize, whose answer Magic017Image_Return stores through
//     Sprite_Current read again after the call, answers from its own stream
//     too, for the same reason.
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s05.h"
#include "game/move_script_bytes.h"

namespace magic_s05 {
namespace {

namespace mh = magic_harness;
using move_script::Long;
using move_script::SetLong;

// tools/magic_rows.py --unit MAGIC017 --clones, 2026-09-26, names given.
// 0x4A11E0: 0x2E bytes  Magic017_Task
constexpr mh::Imm kImms4A11E0[] = {{0xF, 0x4A1210}, {0x17, 0x4A13B0}, {0x22, 0x4A13E0}};
// 0x4A1210: 0x198 bytes  Magic017_Start
constexpr mh::CallSite kCalls4A1210[] = {{0x67, 0x4FB830}, {0x75, 0x435180}, {0x140, 0x4FBF50}, {0x179, 0x4FBE30}, {0x18B, 0x587900}};
// 0x4A13B0: 0x2D bytes  Magic017_Wait
constexpr mh::CallSite kCalls4A13B0[] = {{0x10, 0x4FB830}};
// 0x4A13E0: 0x11 bytes  Magic017_End
constexpr mh::CallSite kCalls4A13E0[] = {{0x0, 0x4FC000}, {0xC, 0x4351F0}};
// 0x4A1400: 0x12 bytes; +0xB note: jmp through .data 0x65a69c, 7 code entries (a data_tables entry)  Magic017Image_Task
//   (one entry is this unit's: the next, 0x4A2480, is MAGIC008's table)
// 0x4A1420: 0x7C bytes  Magic017Image_Run
constexpr mh::CallSite kCalls4A1420[] = {{0x73, 0x588F20}};
constexpr mh::Imm kImms4A1420[] = {{0xF, 0x4A14A0}, {0x17, 0x4A1560}, {0x22, 0x4A15D0}, {0x2A, 0x4A16D0}, {0x32, 0x4A17A0}, {0x3A, 0x4A17F0}, {0x42, 0x4A1810}, {0x4A, 0x4A1890}, {0x52, 0x4A1920}, {0x5A, 0x4AEE90}};
// 0x4A14A0: 0xB4 bytes  Magic017Image_Appear
constexpr mh::CallSite kCalls4A14A0[] = {{0x99, 0x446770}};
// 0x4A1560: 0x66 bytes  Magic017Image_Drift
// 0x4A15D0: 0xF8 bytes  Magic017Image_Leap
constexpr mh::CallSite kCalls4A15D0[] = {{0x4D, 0x446770}, {0xC4, 0x446770}, {0xDE, 0x587900}};
// 0x4A16D0: 0xD0 bytes  Magic017Image_Return
constexpr mh::CallSite kCalls4A16D0[] = {{0xA4, 0x435A70}, {0xAC, 0x4FC1F0}, {0xCB, 0x4351F0}};
// 0x4A17A0: 0x4B bytes  Magic017Image_Strike
constexpr mh::CallSite kCalls4A17A0[] = {{0xB, 0x589410}, {0x31, 0x4530D0}, {0x3A, 0x4FC030}};
// 0x4A17F0: 0x1B bytes  Magic017Image_Script
constexpr mh::CallSite kCalls4A17F0[] = {{0x0, 0x589410}};
// 0x4A1810: 0x7F bytes  Magic017Image_Reshow
constexpr mh::CallSite kCalls4A1810[] = {{0x27, 0x435A70}};
// 0x4A1890: 0x82 bytes  Magic017Image_Home
// 0x4A1920: 0x58 bytes  Magic017Image_Done

// tools/magic_rows.py --unit MAGIC018/MAGIC019 --clones, 2026-09-26, names given.
// 0x4A1980: 0x36 bytes  Magic018Row5_Task
constexpr mh::Imm kImms4A1980[] = {{0xF, 0x4A19C0}, {0x17, 0x4A1AF0}, {0x22, 0x43FE80}, {0x2A, 0x43F460}};
// 0x4A19C0: 0x126 bytes  Magic018Row5_Start
constexpr mh::CallSite kCalls4A19C0[] = {{0x67, 0x4FB830}, {0x70, 0x435180}, {0x119, 0x587900}};
// 0x4A1AF0: 0x2C bytes  Magic018Row5_Wait
constexpr mh::CallSite kCalls4A1AF0[] = {{0xF, 0x4FB830}};
// 0x4A1B20: 0x2E bytes  Magic019_Task
constexpr mh::Imm kImms4A1B20[] = {{0xF, 0x4A1B50}, {0x17, 0x4A1CC0}, {0x22, 0x43F460}};
// 0x4A1B50: 0x163 bytes  Magic019_Start
constexpr mh::CallSite kCalls4A1B50[] = {{0x66, 0x4FB830}, {0x6F, 0x435180}, {0x113, 0x4FBF50}, {0x14C, 0x4FBE30}, {0x157, 0x4FBED0}};
// 0x4A1CC0: 0x29 bytes  Magic019_End
constexpr mh::CallSite kCalls4A1CC0[] = {{0xB, 0x4FC000}, {0x14, 0x4FB830}, {0x23, 0x4351F0}};
// 0x4A1CF0: 0x36 bytes  Magic018Row53_Task
constexpr mh::Imm kImms4A1CF0[] = {{0xF, 0x4A1D30}, {0x17, 0x4A1EC0}, {0x22, 0x43FE80}, {0x2A, 0x43F460}};
// 0x4A1D30: 0x185 bytes  Magic018Row53_Start
constexpr mh::CallSite kCalls4A1D30[] = {{0x67, 0x4FB830}, {0x75, 0x435180}, {0x133, 0x4FBF50}, {0x16C, 0x4FBE30}};
// 0x4A1EC0: 0x32 bytes  Magic018Row53_Wait
constexpr mh::CallSite kCalls4A1EC0[] = {{0xC, 0x4FC000}, {0x15, 0x4FB830}};
// 0x4A1F00: 0x36 bytes  Magic018Row54_Task
constexpr mh::Imm kImms4A1F00[] = {{0xF, 0x4A1F40}, {0x17, 0x4A2160}, {0x22, 0x43FE80}, {0x2A, 0x43F460}};
// 0x4A1F40: 0x119 bytes  Magic018Row54_Start
constexpr mh::CallSite kCalls4A1F40[] = {{0x66, 0x4FB830}, {0x6F, 0x435180}};
// 0x4A2060: 0x2E bytes  Magic018Row68_Task
constexpr mh::Imm kImms4A2060[] = {{0xF, 0x4A2090}, {0x17, 0x4A2160}, {0x22, 0x43FE80}};
// 0x4A2090: 0xCB bytes  Magic018Row68_Start
constexpr mh::CallSite kCalls4A2090[] = {{0x18, 0x4FB830}, {0x21, 0x435180}};
// 0x4A2160: 0x28 bytes  Magic018_WaitOneImage
constexpr mh::CallSite kCalls4A2160[] = {{0x17, 0x4FB830}};

#define MH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define S05_OURS(name) reinterpret_cast<const void*>(&::name)
const mh::Clone kClones[] = {
    {"Magic017_Task", 0x4A11E0, 0x2E, nullptr, 0, kImms4A11E0, MH_N(kImms4A11E0), nullptr, 0, S05_OURS(Magic017_Task)},
    {"Magic017_Start", 0x4A1210, 0x198, kCalls4A1210, MH_N(kCalls4A1210), nullptr, 0, nullptr, 0, S05_OURS(Magic017_Start)},
    {"Magic017_Wait", 0x4A13B0, 0x2D, kCalls4A13B0, MH_N(kCalls4A13B0), nullptr, 0, nullptr, 0, S05_OURS(Magic017_Wait)},
    {"Magic017_End", 0x4A13E0, 0x11, kCalls4A13E0, MH_N(kCalls4A13E0), nullptr, 0, nullptr, 0, S05_OURS(Magic017_End)},
    {"Magic017Image_Task", 0x4A1400, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Task)},
    {"Magic017Image_Run", 0x4A1420, 0x7C, kCalls4A1420, MH_N(kCalls4A1420), kImms4A1420, MH_N(kImms4A1420), nullptr, 0, S05_OURS(Magic017Image_Run)},
    {"Magic017Image_Appear", 0x4A14A0, 0xB4, kCalls4A14A0, MH_N(kCalls4A14A0), nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Appear)},
    {"Magic017Image_Drift", 0x4A1560, 0x66, nullptr, 0, nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Drift)},
    {"Magic017Image_Leap", 0x4A15D0, 0xF8, kCalls4A15D0, MH_N(kCalls4A15D0), nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Leap)},
    {"Magic017Image_Return", 0x4A16D0, 0xD0, kCalls4A16D0, MH_N(kCalls4A16D0), nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Return)},
    {"Magic017Image_Strike", 0x4A17A0, 0x4B, kCalls4A17A0, MH_N(kCalls4A17A0), nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Strike)},
    {"Magic017Image_Script", 0x4A17F0, 0x1B, kCalls4A17F0, MH_N(kCalls4A17F0), nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Script)},
    {"Magic017Image_Reshow", 0x4A1810, 0x7F, kCalls4A1810, MH_N(kCalls4A1810), nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Reshow)},
    {"Magic017Image_Home", 0x4A1890, 0x82, nullptr, 0, nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Home)},
    {"Magic017Image_Done", 0x4A1920, 0x58, nullptr, 0, nullptr, 0, nullptr, 0, S05_OURS(Magic017Image_Done)},
    {"Magic018Row5_Task", 0x4A1980, 0x36, nullptr, 0, kImms4A1980, MH_N(kImms4A1980), nullptr, 0, S05_OURS(Magic018Row5_Task)},
    {"Magic018Row5_Start", 0x4A19C0, 0x126, kCalls4A19C0, MH_N(kCalls4A19C0), nullptr, 0, nullptr, 0, S05_OURS(Magic018Row5_Start)},
    {"Magic018Row5_Wait", 0x4A1AF0, 0x2C, kCalls4A1AF0, MH_N(kCalls4A1AF0), nullptr, 0, nullptr, 0, S05_OURS(Magic018Row5_Wait)},
    {"Magic019_Task", 0x4A1B20, 0x2E, nullptr, 0, kImms4A1B20, MH_N(kImms4A1B20), nullptr, 0, S05_OURS(Magic019_Task)},
    {"Magic019_Start", 0x4A1B50, 0x163, kCalls4A1B50, MH_N(kCalls4A1B50), nullptr, 0, nullptr, 0, S05_OURS(Magic019_Start)},
    {"Magic019_End", 0x4A1CC0, 0x29, kCalls4A1CC0, MH_N(kCalls4A1CC0), nullptr, 0, nullptr, 0, S05_OURS(Magic019_End)},
    {"Magic018Row53_Task", 0x4A1CF0, 0x36, nullptr, 0, kImms4A1CF0, MH_N(kImms4A1CF0), nullptr, 0, S05_OURS(Magic018Row53_Task)},
    {"Magic018Row53_Start", 0x4A1D30, 0x185, kCalls4A1D30, MH_N(kCalls4A1D30), nullptr, 0, nullptr, 0, S05_OURS(Magic018Row53_Start)},
    {"Magic018Row53_Wait", 0x4A1EC0, 0x32, kCalls4A1EC0, MH_N(kCalls4A1EC0), nullptr, 0, nullptr, 0, S05_OURS(Magic018Row53_Wait)},
    {"Magic018Row54_Task", 0x4A1F00, 0x36, nullptr, 0, kImms4A1F00, MH_N(kImms4A1F00), nullptr, 0, S05_OURS(Magic018Row54_Task)},
    {"Magic018Row54_Start", 0x4A1F40, 0x119, kCalls4A1F40, MH_N(kCalls4A1F40), nullptr, 0, nullptr, 0, S05_OURS(Magic018Row54_Start)},
    {"Magic018Row68_Task", 0x4A2060, 0x2E, nullptr, 0, kImms4A2060, MH_N(kImms4A2060), nullptr, 0, S05_OURS(Magic018Row68_Task)},
    {"Magic018Row68_Start", 0x4A2090, 0xCB, kCalls4A2090, MH_N(kCalls4A2090), nullptr, 0, nullptr, 0, S05_OURS(Magic018Row68_Start)},
    {"Magic018_WaitOneImage", 0x4A2160, 0x28, kCalls4A2160, MH_N(kCalls4A2160), nullptr, 0, nullptr, 0, S05_OURS(Magic018_WaitOneImage)},
};
#undef S05_OURS
#undef MH_N

enum : unsigned {
    kMagic017_Task, kMagic017_Start, kMagic017_Wait, kMagic017_End, kMagic017Image_Task, kMagic017Image_Run,
    kMagic017Image_Appear, kMagic017Image_Drift, kMagic017Image_Leap, kMagic017Image_Return, kMagic017Image_Strike,
    kMagic017Image_Script, kMagic017Image_Reshow, kMagic017Image_Home, kMagic017Image_Done,
    kMagic018Row5_Task, kMagic018Row5_Start, kMagic018Row5_Wait, kMagic019_Task, kMagic019_Start, kMagic019_End,
    kMagic018Row53_Task, kMagic018Row53_Start, kMagic018Row53_Wait, kMagic018Row54_Task, kMagic018Row54_Start,
    kMagic018Row68_Task, kMagic018Row68_Start, kMagic018_WaitOneImage, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// --- the callees' stand-ins and effects ---------------------------------------

// Sprite_ScriptTickOnce / Sprite_UpdateScreen: which sprite; the tick's
// answer (0 a third of the time) from its own stream, apart from Stir's.
std::uint32_t FlagAnswer() {
    const std::uint32_t h = mh::Noise();
    return h % 3 == 0 ? h & 0xFFFFFF00u : h | 0x10;
}
std::uint32_t __cdecl RecTickOnce() {
    mh::Record(bof3::addr::Sprite_ScriptTickOnce, Key(Sprite_Current));
    mh::Stir();
    return FlagAnswer();
}
void __cdecl RecUpdateScreen() {
    mh::Record(bof3::addr::Sprite_UpdateScreen, Key(Sprite_Current));
    mh::Stir();
}
// BattleActor_FxSize: any byte, from its own stream.
std::uint32_t SizeAnswer(const std::uint32_t*, std::uint32_t) { return mh::Noise(); }
// 0x446770 turns the task's +0xC / +0x10 by its +8: the inputs logged, a new
// pair written where the real one writes.
std::uint32_t TurnEffect(const std::uint32_t* a, std::uint32_t answer) {
    auto* task = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    mh::Note(task[8], static_cast<std::uint32_t>(Long(task + 0xC)), static_cast<std::uint32_t>(Long(task + 0x10)));
    mh::FillBytes(task + 0xC, 8);
    return answer;
}

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
constexpr mh::Answer kG = mh::Answer::kGarbage;
#define S05_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define S05_RAW(address) #address, address, address
#define S05_CUSTOM(f) kG, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&f)
const mh::Callee kCallees[] = {
    // standard ones, recorded this group's way
    {S05_OURS(Sprite_ScriptTickOnce), 0, {}, S05_CUSTOM(RecTickOnce)},
    {S05_OURS(Sprite_UpdateScreen), 0, {}, S05_CUSTOM(RecUpdateScreen)},
    {S05_OURS(BattleActor_FxSize), 0, {}, kG, 0, 0, {}, &SizeAnswer},
    // Capcom's, unnamed: the dx / dz turn by direction; an enemy's animation
    // by battle index (both bytes read, the upper ones pushed stale)
    {S05_RAW(0x446770), 1, {kAll}, kG, 0, 0, {}, &TurnEffect},
    {S05_RAW(0x435A70), 2, {kU8, kU8}, kG, 0, 0},
};
#undef S05_OURS
#undef S05_RAW
#undef S05_CUSTOM

// Magic017Image_Task's jmp table: one entry this unit's.
const mh::DataTable kTables[] = {{0x65A69C, 1}};

unsigned char Byte(std::uint32_t v) { return static_cast<unsigned char>(v); }
unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return mh::Pointer(mh::at::kOwner); }

// Half the time the byte one step before a threshold, or at it; else as the
// random fill left it.
void Near(unsigned char& b, unsigned before) {
    if (mh::Half()) b = Byte(before + (mh::Half() ? 1u : 0u));
}

void Seed(unsigned k) {
    unsigned char* const sc = Sc();
    // an actor anywhere on the field half the time (party 0..2, enemies 0..7)
    if (mh::Half()) mh::Mem(mh::at::kActor)[0] = Byte(mh::Next() % 11);
    switch (k) {
    // the dispatchers: inside their tables
    case kMagic017_Task: case kMagic019_Task: case kMagic018Row68_Task: sc[1] = Byte(mh::Next() % 3); break;
    case kMagic018Row5_Task: case kMagic018Row53_Task: case kMagic018Row54_Task: sc[1] = Byte(mh::Next() % 4); break;
    case kMagic017Image_Task: sc[1] = 0; break;
    case kMagic017Image_Run:
        sc[2] = Byte(mh::Next() % 10);
        if (mh::Half()) sc[0] = 0;
        break;
    // the counters at their thresholds
    case kMagic017Image_Appear: case kMagic017Image_Drift: case kMagic017Image_Reshow: case kMagic017Image_Home:
    case kMagic017Image_Done:
        Near(sc[9], 1);
        break;
    case kMagic017Image_Leap:
        Near(sc[9], 0xF);
        if (mh::Half()) sc[0xB] = Byte(mh::Next() % 2);
        break;
    case kMagic017Image_Return:
        Near(sc[9], 1);
        if (mh::Often()) sc[0xB] = Byte(mh::Next() % 3);
        break;
    case kMagic017Image_Strike:
        if (mh::Often()) Owner()[0xB] = Byte(mh::Half() ? 1 : mh::Next() % 3);
        Near(sc[9], 1);
        break;
    // the waits: the images' count one either side of its end
    case kMagic017_Wait: case kMagic018Row53_Wait:
        if (mh::Often()) sc[0xB] = Byte(mh::Next() % 2);
        break;
    case kMagic018Row5_Wait: case kMagic019_End: case kMagic018_WaitOneImage:
        if (mh::Often()) sc[0xB] = Byte(mh::Half() ? 0xFF : 0xFE);
        break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    mh::Group group = {
        "magic_s05", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables,     sizeof kTables / sizeof kTables[0], nullptr, 0, &Seed,
        nullptr,     2000,
    };
    mh::Run(group);
}

}  // namespace magic_s05
