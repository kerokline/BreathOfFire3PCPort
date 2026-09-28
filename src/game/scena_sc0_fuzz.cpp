// BOF3X_SHADOW=scena_sc0: scenario chapter 0 through the scenario round's
// shared harness (scenario_harness.h), once at start-up. docs/scena_sc0.md
// section 4.
//
// The clone table (tools/scenario_rows.py --unit SC0 --clones, 2026-09-27),
// the group's own functions called directly (kPhase), the chapter's three
// .data tables, the regions the chapter touches beyond the standard ones (the
// dwords 0x904608.., the CLUT rows 0x80BC00 / 0x80FC00, 0x904EE0, 0x904CD0,
// MapView_Origin, one Area_Descriptors entry and a descriptor block of the
// fuzz's own), a seed by role and a disturbance of the chapter's counters.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sc0.h"
#include "game/scena_sc0_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc0 {
namespace {

namespace sh = scenario_harness;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// tools/scenario_rows.py --unit SC0 --clones, 2026-09-27 (capstone: every jump
// internal; the jump tables bounded at their cmp / ja; nothing REFUSED).
// 0x537F20: 0xE bytes; +0x7 note: jmp through .data 0x660ce4, 3 code entries (a data_tables entry); root: chapter 0 slot 0
// 0x537F30: 0xA2 bytes
constexpr sh::CallSite kCalls537F30[] = {{0x5, 0x5341A0}, {0x18, 0x594E00}, {0x76, 0x454810}, {0x81, 0x5A9949}, {0x89, 0x454810}};
// 0x537FE0: 0x1F3 bytes
constexpr sh::CallSite kCalls537FE0[] = {{0x1B, 0x531F90}, {0x30, 0x589810}, {0x9C, 0x5381E0}, {0xB3, 0x57C140}, {0xCD, 0x57C0F0}, {0xD5, 0x57C7C0}, {0xEF, 0x57C810}, {0xF4, 0x538300}, {0x124, 0x587740}, {0x17C, 0x57C140}, {0x194, 0x57C0F0}, {0x1DB, 0x531F90}};
// 0x5381E0: 0x116 bytes
constexpr sh::CallSite kCalls5381E0[] = {{0x16, 0x57C140}, {0x35, 0x5725F0}, {0x3C, 0x5734F0}, {0x4D, 0x589810}, {0xAA, 0x57C810}, {0xCC, 0x57C0F0}, {0xDD, 0x57C140}, {0xF2, 0x57C0F0}, {0xF9, 0x531F90}};
// 0x538300: 0x1EC bytes
constexpr sh::CallSite kCalls538300[] = {{0x2E, 0x57C140}, {0x40, 0x587A20}, {0x48, 0x454810}, {0x53, 0x5A9949}, {0x5B, 0x454810}, {0x7C, 0x57C0F0}, {0x86, 0x5725F0}, {0x9E, 0x531F90}, {0xA4, 0x5734F0}, {0xAC, 0x589810}, {0x13A, 0x57C140}, {0x159, 0x531F90}, {0x163, 0x57C7A0}, {0x17B, 0x57C140}, {0x19F, 0x57C140}, {0x1B5, 0x57C140}};
constexpr sh::JumpTable kTables538300[] = {{0x22, 0x1D8, 5}};
// 0x5384F0: 0xE bytes; +0x7 note: jmp through .data 0x660cf0, 12 code entries (a data_tables entry)
// 0x538500: 0x20C bytes
constexpr sh::CallSite kCalls538500[] = {{0x27, 0x4976D0}, {0x37, 0x587AE0}, {0x7C, 0x57C7C0}, {0x81, 0x57C810}, {0x88, 0x495040}, {0xB7, 0x495040}, {0xDE, 0x4976D0}, {0x110, 0x594E00}, {0x149, 0x589810}, {0x1C5, 0x594E00}, {0x1D3, 0x587B40}};
constexpr sh::JumpTable kTables538500[] = {{0x13, 0x1EC, 8}};
// 0x538710: 0x158 bytes
constexpr sh::CallSite kCalls538710[] = {{0x2D, 0x495040}, {0x35, 0x589810}, {0xFC, 0x56FCA0}, {0x11D, 0x587AE0}, {0x130, 0x594E00}};
constexpr sh::JumpTable kTables538710[] = {{0x13, 0x148, 4}};
// 0x538870: 0x298 bytes
constexpr sh::CallSite kCalls538870[] = {{0x27, 0x4976D0}, {0x62, 0x4976D0}, {0x88, 0x4976D0}, {0xED, 0x589810}, {0x156, 0x589810}, {0x1FB, 0x495040}, {0x21E, 0x4976D0}, {0x22C, 0x587B40}, {0x253, 0x594E00}};
constexpr sh::JumpTable kTables538870[] = {{0x13, 0x26C, 11}};
// 0x538B10: 0x478 bytes
constexpr sh::CallSite kCalls538B10[] = {{0x14, 0x5B93D2}, {0x64, 0x57C0F0}, {0x71, 0x57C0F0}, {0x7F, 0x57C0F0}, {0x8D, 0x57C0F0}, {0x1B5, 0x589810}, {0x20F, 0x5720C0}, {0x262, 0x495040}, {0x2EA, 0x57C0F0}, {0x2FD, 0x594E00}, {0x345, 0x454810}, {0x356, 0x587AE0}, {0x3E5, 0x532ED0}, {0x3EC, 0x4410B0}, {0x3FA, 0x57C0F0}, {0x410, 0x57C7A0}, {0x417, 0x531F90}};
constexpr sh::JumpTable kTables538B10[] = {{0x50, 0x430, 18}};
// 0x538F90: 0x213 bytes (its byte table 0x539170 is read from the original)
constexpr sh::CallSite kCalls538F90[] = {{0x33, 0x57C7C0}, {0x3A, 0x531F90}, {0x48, 0x57C0F0}, {0x74, 0x532ED0}, {0x7B, 0x4410B0}, {0x91, 0x57C7A0}, {0xB0, 0x57C7C0}, {0xB7, 0x531F90}, {0xC5, 0x57C0F0}, {0xF1, 0x532ED0}, {0xF8, 0x4410B0}, {0x115, 0x57C7C0}, {0x11C, 0x531F90}, {0x12A, 0x57C0F0}, {0x152, 0x532ED0}, {0x159, 0x4410B0}, {0x169, 0x57C7A0}, {0x195, 0x594E00}};
constexpr sh::JumpTable kTables538F90[] = {{0x1B, 0x1B4, 11}};
// 0x5391B0: 0x24C bytes
constexpr sh::CallSite kCalls5391B0[] = {{0x20, 0x531F90}, {0x27, 0x5734F0}, {0x53, 0x532ED0}, {0x5A, 0x4410B0}, {0x75, 0x4976D0}, {0xA9, 0x495040}, {0xCC, 0x56F670}, {0xE0, 0x5725F0}, {0x131, 0x5734F0}, {0x145, 0x5725F0}, {0x14D, 0x589810}, {0x1EC, 0x587AE0}, {0x213, 0x594E00}};
constexpr sh::JumpTable kTables5391B0[] = {{0x13, 0x228, 9}};
// 0x539400: 0x2F bytes
constexpr sh::CallSite kCalls539400[] = {{0x11, 0x57C0F0}, {0x18, 0x531F90}};
// 0x539430: 0x76 bytes
constexpr sh::CallSite kCalls539430[] = {{0x31, 0x594E00}, {0x3B, 0x587740}};
// 0x5394B0: 0x11C bytes
constexpr sh::CallSite kCalls5394B0[] = {{0x1B, 0x57C6B0}, {0x45, 0x5B93D2}, {0x5A, 0x5B93D2}, {0x9C, 0x594E00}, {0xB0, 0x57C6B0}, {0xEB, 0x594E00}};
constexpr sh::JumpTable kTables5394B0[] = {{0x13, 0x10C, 4}};
// 0x5395D0: 0x380 bytes
constexpr sh::CallSite kCalls5395D0[] = {{0xC, 0x5B93D2}, {0x7C, 0x56FCA0}, {0xB4, 0x495040}, {0xBE, 0x587740}, {0x137, 0x539950}, {0x18F, 0x539950}, {0x1BF, 0x589330}, {0x23B, 0x5B93D2}, {0x24F, 0x5B93D2}, {0x2BA, 0x589330}, {0x2C7, 0x587740}, {0x301, 0x495040}, {0x349, 0x594E00}, {0x351, 0x56D6F0}};
constexpr sh::JumpTable kTables5395D0[] = {{0x94, 0x358, 10}};
// 0x539950: 0xBA bytes
constexpr sh::CallSite kCalls539950[] = {{0xB3, 0x587740}};
// 0x539A10: 0x1F bytes; +0x14 note: call through .data 0x660d58, 1 code entries (a data_tables entry); root: chapter 0 slot 1
// 0x539A30: 0x81 bytes; root: chapter 0 slot 2, the step hook (x, z), al
constexpr sh::CallSite kCalls539A30[] = {{0x1E, 0x57C140}, {0x5A, 0x57C140}, {0x66, 0x57C7C0}};
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const sh::Clone kClones[] = {
    {"Scena00_Frame", 0x537F20, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_Frame)},
    {"Scena00_Start", 0x537F30, 0xA2, kCalls537F30, SH_N(kCalls537F30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_Start), 0, false, sh::Shape::kState},
    {"Scena00_EnterArea", 0x537FE0, 0x1F3, kCalls537FE0, SH_N(kCalls537FE0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_EnterArea), 0, false, sh::Shape::kState},
    {"Scena00_Area02", 0x5381E0, 0x116, kCalls5381E0, SH_N(kCalls5381E0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_Area02), 0, false, sh::Shape::kState},
    {"Scena00_Area18", 0x538300, 0x1EC, kCalls538300, SH_N(kCalls538300), nullptr, 0, kTables538300, SH_N(kTables538300), reinterpret_cast<const void*>(&::Scena00_Area18), 0, false, sh::Shape::kState},
    {"Scena00_Run", 0x5384F0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_Run), 0, false, sh::Shape::kState},
    {"Scena00_Run2", 0x538500, 0x20C, kCalls538500, SH_N(kCalls538500), nullptr, 0, kTables538500, SH_N(kTables538500), reinterpret_cast<const void*>(&::Scena00_Run2), 0, false, sh::Shape::kState},
    {"Scena00_Run3", 0x538710, 0x158, kCalls538710, SH_N(kCalls538710), nullptr, 0, kTables538710, SH_N(kTables538710), reinterpret_cast<const void*>(&::Scena00_Run3), 0, false, sh::Shape::kState},
    {"Scena00_Run4", 0x538870, 0x298, kCalls538870, SH_N(kCalls538870), nullptr, 0, kTables538870, SH_N(kTables538870), reinterpret_cast<const void*>(&::Scena00_Run4), 0, false, sh::Shape::kState},
    {"Scena00_Run5", 0x538B10, 0x478, kCalls538B10, SH_N(kCalls538B10), nullptr, 0, kTables538B10, SH_N(kTables538B10), reinterpret_cast<const void*>(&::Scena00_Run5), 0, false, sh::Shape::kState},
    {"Scena00_Run6", 0x538F90, 0x213, kCalls538F90, SH_N(kCalls538F90), nullptr, 0, kTables538F90, SH_N(kTables538F90), reinterpret_cast<const void*>(&::Scena00_Run6), 0, false, sh::Shape::kState},
    {"Scena00_Run7", 0x5391B0, 0x24C, kCalls5391B0, SH_N(kCalls5391B0), nullptr, 0, kTables5391B0, SH_N(kTables5391B0), reinterpret_cast<const void*>(&::Scena00_Run7), 0, false, sh::Shape::kState},
    {"Scena00_Run8", 0x539400, 0x2F, kCalls539400, SH_N(kCalls539400), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_Run8), 0, false, sh::Shape::kState},
    {"Scena00_Run9", 0x539430, 0x76, kCalls539430, SH_N(kCalls539430), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_Run9), 0, false, sh::Shape::kState},
    {"Scena00_Run10", 0x5394B0, 0x11C, kCalls5394B0, SH_N(kCalls5394B0), nullptr, 0, kTables5394B0, SH_N(kTables5394B0), reinterpret_cast<const void*>(&::Scena00_Run10), 0, false, sh::Shape::kState},
    {"Scena00_Run11", 0x5395D0, 0x380, kCalls5395D0, SH_N(kCalls5395D0), nullptr, 0, kTables5395D0, SH_N(kTables5395D0), reinterpret_cast<const void*>(&::Scena00_Run11), 0, false, sh::Shape::kState},
    {"Scena00_Steer", 0x539950, 0xBA, kCalls539950, SH_N(kCalls539950), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_Steer), 0, false, sh::Shape::kState},
    {"Scena00_ObjectHook", 0x539A10, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_ObjectHook), 0, false, sh::Shape::kObject},
    {"Scena00_StepHook", 0x539A30, 0x81, kCalls539A30, SH_N(kCalls539A30), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena00_StepHook), 0xFF, false, sh::Shape::kHook},
};
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];

// The group's own functions its clones call directly: a recorder that logs
// the chapter bytes they run with (the clones' E8s re-aimed; ours by name).
// Scena00_ObjectHooks' one entry: a stand-in of its exact type in the table
// itself (the seed writes it; the region puts the table back), logging the
// object and the row the hook passes - a DataTable's handler recorder logs
// no arguments, and the entry is also the runs' bare ret, called with none.
// Its log slot is keyed on the hook's own address, which no clone calls.
void __cdecl ObjectEntry(unsigned char* object, unsigned char* row) {
    sh::Record(0x539A10, Key(object), Key(row));
    sh::Stir();
}

#define SH_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
const sh::Callee kCallees[] = {
    {SH_OURS(Scena00_Area02), 0, {}, sh::Answer::kPhase, 0, 0},
    {SH_OURS(Scena00_Area18), 0, {}, sh::Answer::kPhase, 0, 0},
    {SH_OURS(Scena00_Steer), 0, {}, sh::Answer::kPhase, 0, 0},
    {"Scena00_ObjectHooks[0]", 0x539A10, 0x539A10, 2, {0xFFFFFFFFu, 0xFFFFFFFFu}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&ObjectEntry)},
};
#undef SH_OURS

const sh::DataTable kTables[] = {{kStates, 3}, {kRuns, 12}};

// The area Scena00_Steer and Scena00_Run11 read the descriptor of: its
// Area_Descriptors entry points at a block of the fuzz's own, whose +0xC
// points at corner words of the fuzz's own.
constexpr unsigned kFakeArea = 0x19;
alignas(16) unsigned char g_desc[0x20];
alignas(16) unsigned char g_corner[0x40];

sh::Region g_regions[16];

unsigned Index(const char* name) {
    for (unsigned k = 0; k < kCount; ++k)
        if (std::strcmp(kClones[k].name, name) == 0) return k;
    bof3::Fatal("scena_sc0: no clone %s", name);
}
unsigned g_k;   // the clone being seeded, for Disturb
unsigned kRun2K, kRun4K, kEnterK, kRun5K, kStartK, kFrameK, kRunK, kSteerK, kRun11K, kObjectK, kHookK;

unsigned char& B(std::uint32_t a) { return sh::Mem(a)[0]; }
void SetW(std::uint32_t a, unsigned v) { move_script::SetWord(sh::Mem(a), v); }
void SetL(std::uint32_t a, std::uint32_t v) { move_script::SetLong(sh::Mem(a), static_cast<std::int32_t>(v)); }

// The steps each function's jump table (or chain) holds, for the step seed.
unsigned StepsOf(unsigned k) {
    static const struct { const char* name; unsigned steps; } kSteps[] = {
        {"Scena00_Run2", 8}, {"Scena00_Run3", 4}, {"Scena00_Run4", 11}, {"Scena00_Run5", 18}, {"Scena00_Run6", 0x33},
        {"Scena00_Run7", 9}, {"Scena00_Run8", 2}, {"Scena00_Run9", 3}, {"Scena00_Run10", 4}, {"Scena00_Run11", 10},
    };
    for (const auto& s : kSteps)
        if (std::strcmp(kClones[k].name, s.name) == 0) return s.steps;
    return 0;
}

void Seed(unsigned k) {
    g_k = k;
    // the areas the chapter tests, and Cond_ByteFD's five cases
    if (sh::Often()) SetW(kArea, SH_PICK(1, 2, 4, 0x18, 0x19, 0x1F));
    if (sh::Often()) B(kByteFD) = static_cast<unsigned char>(sh::Next() % 5);
    // the step: every case of the function's table, and one past it
    if (const unsigned steps = StepsOf(k); steps && sh::Often()) {
        unsigned s = sh::Next() % (steps + 1);
        if (std::strcmp(kClones[k].name, "Scena00_Run6") == 0 && sh::Often())
            s = SH_PICK(0, 0xA, 0xB, 0xD, 0x14, 0x15, 0x17, 0x1E, 0x1F, 0x21, 0x32);
        B(kStep) = static_cast<unsigned char>(s);
    }
    // the counters the steps wait on, each boundary in
    if (sh::Next() % 6)
        B(kCounter) = static_cast<unsigned char>(SH_PICK(1, 3, 4, 5, 7, 8, 0xA, 0xB, 0xC, 0xD, 0x11, 0x13, 0x14, 0x15, 0x1E, 0x1F, 0x24, 0x25));
    if (sh::Often())
        B(kByte4A) = static_cast<unsigned char>(SH_PICK(0, 1, 3, 9, 0x13, 0x14, 0x15, 0x17, 0x23, 0x30, 0x35, 0x38, 0x71));
    if (sh::Often()) B(kByte4B) = static_cast<unsigned char>(SH_PICK(0, 1, 0x54, 0x60));
    if (sh::Half()) B(kCount2) = static_cast<unsigned char>(SH_PICK(0x1E, 0x1F, 0));
    if (sh::Half()) SetW(kWait, 0);
    if (sh::Half()) B(kRequest) = 2;
    if (sh::Often()) SetW(kTimer, SH_PICK(0, 1, 2, 0x21, 0x7B, 0x7C));
    if (sh::Half()) SetL(kFocusZ, SH_PICK(0x4400, 0x5000));
    if (sh::Half()) Frame_Counter = Frame_Counter & ~0x1Fu;
    if (sh::Half()) B(kInput + 1) = static_cast<unsigned char>(sh::Half() ? 0 : SH_PICK(0x10, 0x20, 0x40, 0x80));
    if (sh::Half()) B(kObjTrio + 1) = 2;
    // area 1's effect, and the counter it reads again after taking the slot
    if (k == kEnterK && sh::Half()) {
        SetW(kArea, 1);
        B(kByte4A) = sh::Half() ? 0 : 3;
    }
    // run 11's step 5 at the counter's last count, no direction pressed
    if (k == kRun11K && sh::Half()) {
        B(kStep) = 5;
        B(kCounter) = static_cast<unsigned char>(SH_PICK(0x13, 0x14));
        B(kInput + 1) = 0;
    }
    // the message steps that store Field_Request between two calls
    if ((k == kRun2K || k == kRun4K) && sh::Half()) {
        B(kStep) = k == kRun2K ? 0 : 9;
        SetW(kWait, 0);
    }
    // run 5's shaking steps, with something to shake
    if (k == kRun5K && sh::Next() % 3 == 0) {
        B(kStep) = static_cast<unsigned char>(4 + sh::Next() % 3);
        B(kByte4A) = static_cast<unsigned char>(1 + sh::Next() % 0x20);
        Frame_Counter = Frame_Counter & ~0xFu;
    }
    // the tables' indices, inside what the original can jump to
    if (k == kFrameK) B(0x8034E2) = static_cast<unsigned char>(sh::Next() % 15);
    if (k == kRunK) B(kRun) = static_cast<unsigned char>(sh::Next() % 12);
    if (k == kObjectK) sh::SpriteRecord(0)[0x86] = 0;
    SetL(kObjectHooks, Key(reinterpret_cast<const void*>(&ObjectEntry)));
    // the area Steer and Run11 read the descriptor of
    if (k == kSteerK || k == kRun11K) {
        SetW(kArea, kFakeArea);
        SetL(kDescriptors + 4 * kFakeArea, Key(g_desc));
        SetL(Key(g_desc) + 0xC, Key(g_corner));
    }
}

void Args(unsigned k, std::uint32_t* a) {
    if (k == kObjectK) a[0] = Key(sh::SpriteRecord(0));
    if (k == kHookK && sh::Often()) a[0] = (SH_PICK(0x2B, 0x14) << 16) | (a[0] & 0xFFFF);
}

// the chapter's own cells a handler reads again after a call: half the time
// to a value a step compares with, else any byte
void Disturb(std::uint32_t h) {
    static const unsigned char kValues[] = {0, 1, 3, 0x13, 0x1E, 0x1F, 0x23, 0x35, 0x54, 0x60, 0x71};
    const auto b = h & 0x10000 ? kValues[(h >> 17) % sizeof kValues] : static_cast<unsigned char>(h >> 24);
    if (g_k == kEnterK) {   // area 1: the count read again after Effect_FindFree
        B(kByte4A) = h & 0x100 ? 3 : 0;
        return;
    }
    if (g_k == kRun2K || g_k == kRun4K) {   // Field_Request moved off 2 by the call after the store
        B(kRequest) = static_cast<unsigned char>(h >> 24 == 2 ? 0 : h >> 24);
        return;
    }
    switch ((h >> 8) % 4) {
    case 0: B(kByte4A) = b; break;
    case 1: B(kByte4B) = b; break;
    case 2: B(kCount2) = b; break;
    default: B(kObjTrio + 1) = b; break;
    }
}

}  // namespace

void SelfTest() {
    kStartK = Index("Scena00_Start");
    kRun5K = Index("Scena00_Run5");
    kEnterK = Index("Scena00_EnterArea");
    kRun2K = Index("Scena00_Run2");
    kRun4K = Index("Scena00_Run4");
    kFrameK = Index("Scena00_Frame");
    kRunK = Index("Scena00_Run");
    kSteerK = Index("Scena00_Steer");
    kRun11K = Index("Scena00_Run11");
    kObjectK = Index("Scena00_ObjectHook");
    kHookK = Index("Scena00_StepHook");
    unsigned r = 0;
    g_regions[r++] = {0x904600, 0x60};                        // Scena00_Start's 18 dwords 0x904608..
    g_regions[r++] = {0x80BC00, 0x40};                        // Scena00_Area18's CLUT rows
    g_regions[r++] = {0x80FC00, 0x40};
    g_regions[r++] = {0x904EE0, 4};
    g_regions[r++] = {0x904CD0, 4};
    g_regions[r++] = {0x7E0688, 4};                           // MapView_Origin (0x7E068A)
    g_regions[r++] = {kDescriptors + 4 * kFakeArea, 4};
    g_regions[r++] = {Key(g_desc), sizeof g_desc};
    g_regions[r++] = {Key(g_corner), sizeof g_corner};
    g_regions[r++] = {kObjectHooks, 4};                       // Scena00_ObjectHooks, the stand-in written by the seed
    sh::Group group = {
        "scena_sc0", kClones, kCount, kCallees, sizeof kCallees / sizeof kCallees[0],
        kTables, sizeof kTables / sizeof kTables[0], g_regions, r, &Seed, &Disturb, 4000,
    };
    group.args = &Args;
    group.chapter = 0;
    sh::Run(group);
}

}  // namespace scena_sc0
