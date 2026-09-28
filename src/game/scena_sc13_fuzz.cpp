// BOF3X_SHADOW=scena_sc13: chapters 13 and 14 through the scenario harness
// (scenario_harness.h), once at start-up - two runs, one per chapter byte (a
// Group sets one chapter; SC3's pattern). docs/scena_sc13.md section 4.
//
// The clone tables (tools/scenario_rows.py --unit SC13 --clones, checked
// against a capstone reading of every function), each clone's call shape in
// its Clone line; the callees the standard set lacks or records otherwise;
// the state and run tables swapped for recorders and typed stand-ins written
// into the object tables; the regions beyond the standard ones; a seed per
// role; a disturbance of the chapters' cells. Scena14_EnterArea has 87 call
// sites, more than the harness re-aims (64): this file copies it itself.
//
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/scena_sc13.h"
#include "game/scena_sc13_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc13 {
namespace {

namespace sh = scenario_harness;

// Scena14_EnterArea (87 calls) has more call sites than the harness re-aims in
// one clone (64). So this file makes its copy itself - bof3::CloneOriginal
// with every site re-aimed at a trampoline that calls the harness's recorder
// for the callee (StandIn: the same log entry, disturbance and answer a
// re-aimed site gets, on either pass) - and hands the harness, as its
// "original", a six-byte `jmp [copy]` of its own (scena_sc12_fuzz.cpp's
// pattern). Theirs is still Capcom's bytes, only relocated here.
template <typename F> F Stub(F f) {
    return reinterpret_cast<F>(const_cast<void*>(sh::StandIn(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f)))));
}
template <typename F> F StubAt(std::uint32_t address) { return Stub(reinterpret_cast<F>(static_cast<std::uintptr_t>(address))); }
unsigned char __cdecl TFlagsTest(const unsigned char* b, unsigned i) { return Stub(Flags_Test)(b, i); }
void __cdecl TFlagsSet(unsigned char* b, unsigned i) { Stub(Flags_Set)(b, i); }
void __cdecl TFlagsClear(unsigned char* b, unsigned i) { Stub(Flags_Clear)(b, i); }
void __cdecl TCallA(unsigned n) { Stub(Scenario_CallA)(n); }
void __cdecl TCallB(unsigned n) { Stub(Scenario_CallB)(n); }
unsigned __cdecl TDropIn(unsigned e) { return Stub(Party_DropIn)(e); }
void __cdecl TGrey() { Stub(Scena14_GreyClut)(); }
void __cdecl TPlace(int x, int z, unsigned kind) { StubAt<PlaceFn>(at::kPartyPlace)(x, z, kind); }
unsigned char __cdecl TSpawn(unsigned kind, unsigned x, unsigned y, unsigned z, unsigned life, unsigned w) {
    return Stub(Scena14_SpawnEffect)(kind, x, y, z, life, w);
}
void __cdecl TSound(unsigned short id) { Stub(Sound_PlayEffect)(id); }
void __cdecl TArea141c() { StubAt<VoidFn>(at::kArea141c)(); }
long __cdecl TElevation(long x, long z) { return Stub(AreaMap_Elevation)(x, z); }
unsigned char __cdecl TFindFree() { return Stub(Effect_FindFree)(); }
void __cdecl TViewReset() { Stub(Field_ViewReset)(); }

struct Tramp { std::uint32_t target; const void* to; };
const Tramp kTramps[] = {
    {0x57C140, reinterpret_cast<const void*>(&TFlagsTest)}, {0x57C0F0, reinterpret_cast<const void*>(&TFlagsSet)},
    {0x57C110, reinterpret_cast<const void*>(&TFlagsClear)}, {0x5341A0, reinterpret_cast<const void*>(&TCallA)},
    {0x5341C0, reinterpret_cast<const void*>(&TCallB)},      {0x531F90, reinterpret_cast<const void*>(&TDropIn)},
    {0x564E10, reinterpret_cast<const void*>(&TGrey)},       {0x532ED0, reinterpret_cast<const void*>(&TPlace)},
    {0x564E80, reinterpret_cast<const void*>(&TSpawn)},      {0x587740, reinterpret_cast<const void*>(&TSound)},
    {0x4205D0, reinterpret_cast<const void*>(&TArea141c)},   {0x5720C0, reinterpret_cast<const void*>(&TElevation)},
    {0x589810, reinterpret_cast<const void*>(&TFindFree)},   {0x56F670, reinterpret_cast<const void*>(&TViewReset)},
};

}  // namespace
}  // namespace scena_sc13

extern "C" {
void* g_sc13_ea14_copy = nullptr;
__attribute__((naked)) void Sc13EnterArea14Theirs() { asm("jmp *_g_sc13_ea14_copy"); }
}

namespace scena_sc13 {
namespace {

constexpr std::uint32_t kJmpWrapper = 6;   // FF 25 disp32

void* CopyWithTramps(const char* name, std::uint32_t base, std::uint32_t size, const sh::CallSite* sites, int n) {
    static bof3::CloneCall calls[128];
    if (n > 128) bof3::Fatal("scena_sc13: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) {
        const void* to = nullptr;
        for (const Tramp& t : kTramps)
            if (t.target == sites[i].target) to = t.to;
        if (!to) bof3::Fatal("scena_sc13: %s: no trampoline for 0x%X", name, (unsigned)sites[i].target);
        calls[i] = {sites[i].offset, to, sites[i].target};
    }
    return bof3::CloneOriginal(name, base, size, calls, n);
}

std::uint32_t Wrapper(void (*f)()) {
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p[0] != 0xFF || p[1] != 0x25) bof3::Fatal("scena_sc13: the jmp wrapper at %p is not FF 25", static_cast<const void*>(p));
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

// tools/scenario_rows.py --unit SC13 --clones (at e4882fb), 2026-09-28: every
// jump internal, nothing REFUSED; each extent and call list agrees with the
// capstone reading (docs/scena_sc13.md section 1). Each clone's shape and
// answer mask in its Clone line.
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
// ---- chapter 13
// Scena13_Frame 0x561DB0: 0xE bytes; +0x7 note: jmp through .data 0x66179c, 3 code entries (a data_tables entry); root: chapter 13 slot 0, the frame (Field_ModeDispatch)
// Scena13_Start 0x561DC0: 0x3B bytes
constexpr sh::CallSite kCalls561DC0[] = {{0x0, 0x57C7C0}, {0xC, 0x533E50}, {0x13, 0x5341A0}, {0x1F, 0x57C0F0}, {0x2B, 0x57C110}};
// Scena13_EnterArea 0x561E00: 0x47E bytes
constexpr sh::CallSite kCalls561E00[] = {{0x30, 0x57C140}, {0x45, 0x57C140}, {0x51, 0x57C7C0}, {0x75, 0x57C140}, {0x89, 0x57C140}, {0x97, 0x5341A0}, {0x9D, 0x531F90}, {0xC7, 0x57C140}, {0x10E, 0x57C140}, {0x11C, 0x531F90}, {0x129, 0x57C0F0}, {0x143, 0x57C140}, {0x150, 0x531F90}, {0x19B, 0x57C140}, {0x1B9, 0x589810}, {0x219, 0x57C140}, {0x227, 0x420A90}, {0x237, 0x587740}, {0x249, 0x57C140}, {0x257, 0x5341A0}, {0x25E, 0x531F90}, {0x26C, 0x57C0F0}, {0x286, 0x57C140}, {0x294, 0x5341A0}, {0x29B, 0x531F90}, {0x2AD, 0x57C140}, {0x2BA, 0x420A90}, {0x2D6, 0x57C0F0}, {0x2F0, 0x57C140}, {0x2FE, 0x587BE0}, {0x304, 0x531F90}, {0x334, 0x57C140}, {0x342, 0x531F90}, {0x369, 0x57C140}, {0x377, 0x531F90}, {0x385, 0x57C0F0}, {0x3A4, 0x57C140}, {0x3B6, 0x5341A0}, {0x3BC, 0x531F90}, {0x3C4, 0x562700}, {0x3D2, 0x589810}, {0x43F, 0x57C140}, {0x453, 0x57C140}, {0x461, 0x5341A0}, {0x468, 0x531F90}, {0x470, 0x5627A0}};
// Scena13_Run 0x562280: 0xE bytes; +0x7 note: jmp through .data 0x6617a8, 9 code entries (a data_tables entry)
// Scena13_Run1 0x562290: 0x170 bytes
constexpr sh::CallSite kCalls562290[] = {{0x17, 0x589810}, {0x83, 0x495040}, {0xAA, 0x5A9949}, {0xDD, 0x57C0F0}, {0xF0, 0x594E00}, {0x120, 0x495040}, {0x143, 0x57C7A0}};
constexpr sh::JumpTable kTables562290[] = {{0x13, 0x158, 6}};
// Scena13_Run2 0x562400: 0x2F8 bytes
constexpr sh::CallSite kCalls562400[] = {{0x28, 0x594E00}, {0x45, 0x589810}, {0x70, 0x587B40}, {0x95, 0x57C0F0}, {0xAE, 0x594E00}, {0xD9, 0x589810}, {0x120, 0x57C0F0}, {0x136, 0x594E00}, {0x153, 0x589810}, {0x17E, 0x587B40}, {0x1A4, 0x57C0F0}, {0x1BD, 0x594E00}, {0x1E3, 0x495040}, {0x22F, 0x5341A0}, {0x236, 0x587910}, {0x243, 0x57C0F0}, {0x253, 0x587A00}, {0x25C, 0x533E50}, {0x272, 0x594E00}, {0x2A3, 0x495040}, {0x2B4, 0x57C7A0}};
constexpr sh::JumpTable kTables562400[] = {{0x13, 0x2C8, 12}};
// Scena13_SpawnPairA 0x562700: 0x94 bytes
constexpr sh::CallSite kCalls562700[] = {{0x5, 0x57CD90}, {0x29, 0x57CD90}, {0x70, 0x57AD10}, {0x86, 0x57AD10}};
// Scena13_SpawnPairB 0x5627A0: 0x94 bytes
constexpr sh::CallSite kCalls5627A0[] = {{0x5, 0x57CD90}, {0x29, 0x57CD90}, {0x70, 0x57AD10}, {0x86, 0x57AD10}};
// Scena13_Run3 0x562840: 0x4A9 bytes
constexpr sh::CallSite kCalls562840[] = {{0x2E, 0x589810}, {0xD8, 0x589810}, {0x18C, 0x57C7A0}, {0x193, 0x531F90}, {0x1AE, 0x591920}, {0x1B5, 0x5734F0}, {0x1DD, 0x4976D0}, {0x227, 0x5720C0}, {0x232, 0x56F670}, {0x239, 0x4976D0}, {0x24A, 0x495040}, {0x26B, 0x589810}, {0x2FA, 0x57C0F0}, {0x313, 0x594E00}, {0x345, 0x57C7C0}, {0x35B, 0x594E00}, {0x3A8, 0x589810}, {0x408, 0x594E00}, {0x42B, 0x57C7A0}, {0x439, 0x57C0F0}};
constexpr sh::JumpTable kTables562840[] = {{0x1D, 0x450, 16}};
// Scena13_Run4 0x562CF0: 0x3A0 bytes
constexpr sh::CallSite kCalls562CF0[] = {{0x26, 0x4976D0}, {0x4B, 0x57C7A0}, {0x94, 0x57CE80}, {0xC6, 0x57C7A0}, {0xD4, 0x57C0F0}, {0x101, 0x594E00}, {0x12B, 0x572650}, {0x132, 0x4976D0}, {0x17E, 0x57C0F0}, {0x194, 0x594E00}, {0x1C3, 0x4976D0}, {0x201, 0x594E00}, {0x22E, 0x495040}, {0x235, 0x587BE0}, {0x25D, 0x4976D0}, {0x26B, 0x587B40}, {0x28B, 0x5A9949}, {0x2C1, 0x57C0F0}, {0x2D4, 0x594E00}, {0x302, 0x495040}, {0x31D, 0x57C7A0}};
constexpr sh::JumpTable kTables562CF0[] = {{0x20, 0x33C, 17}};
// Scena13_Run5 0x563090: 0x300 bytes
constexpr sh::CallSite kCalls563090[] = {{0x1B, 0x587B40}, {0x25, 0x587740}, {0x2F, 0x587740}, {0x4C, 0x563390}, {0x95, 0x563390}, {0xE9, 0x587740}, {0xF3, 0x587740}, {0x103, 0x587AE0}, {0x12B, 0x57C7A0}, {0x132, 0x531F90}, {0x150, 0x587740}, {0x15A, 0x587740}, {0x184, 0x531F90}, {0x18D, 0x572650}, {0x1AE, 0x589810}, {0x232, 0x587910}, {0x239, 0x4976D0}, {0x252, 0x587A00}, {0x26E, 0x587AE0}, {0x297, 0x57C7A0}, {0x2A5, 0x57C0F0}};
constexpr sh::JumpTable kTables563090[] = {{0x15, 0x2BC, 17}};
// Scena13_ToneLevels 0x563390: 0x5C bytes
constexpr sh::CallSite kCalls563390[] = {{0x14, 0x5A7A00}, {0x2C, 0x587890}, {0x3A, 0x5A7A00}, {0x52, 0x587890}};
// Scena13_Run6 0x5633F0: 0x624 bytes
constexpr sh::CallSite kCalls5633F0[] = {{0x2C, 0x594E00}, {0x4A, 0x589810}, {0xF0, 0x57C0F0}, {0x106, 0x594E00}, {0x12D, 0x57C0F0}, {0x13A, 0x57C0F0}, {0x153, 0x594E00}, {0x189, 0x57C110}, {0x1B2, 0x495040}, {0x1D3, 0x589810}, {0x278, 0x589810}, {0x2B9, 0x495040}, {0x303, 0x587740}, {0x30A, 0x4976D0}, {0x337, 0x57C0F0}, {0x345, 0x57C110}, {0x35E, 0x594E00}, {0x38D, 0x495040}, {0x3D7, 0x57C7A0}, {0x3E5, 0x57C0F0}, {0x404, 0x4976D0}, {0x429, 0x57C7A0}, {0x44D, 0x5341A0}, {0x454, 0x531F90}, {0x471, 0x5720C0}, {0x4A6, 0x57C0F0}, {0x4B4, 0x57C110}, {0x4CD, 0x594E00}, {0x50A, 0x4976D0}, {0x533, 0x57C0F0}, {0x54C, 0x594E00}, {0x57A, 0x57C7A0}, {0x591, 0x57C0F0}};
constexpr sh::JumpTable kTables5633F0[] = {{0x14, 0x5A8, 31}};
// Scena13_Run7 0x563A20: 0x4F8 bytes
constexpr sh::CallSite kCalls563A20[] = {{0x27, 0x495040}, {0x2E, 0x587B40}, {0x58, 0x587AE0}, {0x76, 0x563F20}, {0xB6, 0x495040}, {0xE0, 0x589810}, {0x154, 0x563F20}, {0x1B6, 0x563F20}, {0x21E, 0x495040}, {0x22C, 0x563F20}, {0x269, 0x495040}, {0x2BF, 0x587BE0}, {0x2F0, 0x587B40}, {0x2F8, 0x589810}, {0x3B9, 0x56F670}, {0x3CA, 0x532ED0}, {0x3F6, 0x4410B0}, {0x417, 0x531F90}, {0x41F, 0x589810}, {0x45A, 0x57C7A0}, {0x468, 0x57C0F0}};
constexpr sh::JumpTable kTables563A20[] = {{0x14, 0x480, 30}};
// Scena13_Caption 0x563F20: 0xB9 bytes
constexpr sh::CallSite kCalls563F20[] = {{0x59, 0x516B30}, {0x70, 0x56C0A0}, {0x7B, 0x56C110}, {0xA9, 0x56C0A0}, {0xB4, 0x56C110}};
// Scena13_Run8 0x563FE0: 0x140 bytes
constexpr sh::CallSite kCalls563FE0[] = {{0x35, 0x531F90}, {0x42, 0x57C0F0}, {0x5E, 0x4976D0}, {0x83, 0x57C7A0}, {0xCB, 0x594E00}, {0xF0, 0x57C0F0}, {0xF8, 0x57C7A0}, {0x109, 0x56D6F0}};
constexpr sh::JumpTable kTables563FE0[] = {{0x14, 0x110, 12}};
// Scena13_ObjectTrigger 0x564120: 0x1F bytes; +0x14 note: call through .data 0x661810, 4 code entries (a data_tables entry); root: chapter 13 slot 1, the object trigger (0x56D6D0, the object)
// Scena13_Object00 0x564140: 0xC bytes
// Scena13_Object01 0x564150: 0x14 bytes
constexpr sh::CallSite kCalls564150[] = {{0x0, 0x57C7C0}};
// Scena13_Object02 0x564170: 0x14 bytes
constexpr sh::CallSite kCalls564170[] = {{0x0, 0x57C7C0}};
// Scena13_Object03 0x564190: 0x2B bytes
constexpr sh::CallSite kCalls564190[] = {{0x8, 0x57C0F0}, {0x10, 0x57C7C0}};
// Scena13_StepHook 0x5641C0: 0x40B bytes; root: chapter 13 slot 2, the step hook (x, z), al
constexpr sh::CallSite kCalls5641C0[] = {{0x2A, 0x57C140}, {0x47, 0x57C7C0}, {0x84, 0x57C140}, {0xB3, 0x57C7C0}, {0x109, 0x57C0F0}, {0x13C, 0x57C140}, {0x159, 0x57C7C0}, {0x17A, 0x57C140}, {0x18F, 0x57C140}, {0x1AC, 0x57C7C0}, {0x1DB, 0x57C140}, {0x1F8, 0x57C7C0}, {0x266, 0x57C140}, {0x276, 0x57C7C0}, {0x284, 0x57C140}, {0x2A6, 0x57C140}, {0x2D3, 0x57C140}, {0x2F2, 0x531F90}, {0x319, 0x57C140}, {0x32E, 0x57C140}, {0x366, 0x531F90}, {0x39F, 0x57C7C0}, {0x3CF, 0x57C140}, {0x3EC, 0x57C7C0}};
// Scena13_ArriveHook 0x5645D0: 0xC8 bytes; root: chapter 13 slot 3, the arrive hook (x, z), al
constexpr sh::CallSite kCalls5645D0[] = {{0x2A, 0x57C140}, {0x3F, 0x57C140}, {0x5A, 0x57C7C0}, {0x8E, 0x57C140}, {0xB2, 0x57C0F0}, {0xBB, 0x572650}};
const sh::Clone kClones13[] = {
    {"Scena13_Frame", 0x561DB0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_Frame), 0x0, false, sh::Shape::kSlot},
    {"Scena13_Start", 0x561DC0, 0x3B, kCalls561DC0, SH_N(kCalls561DC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_Start), 0x0, false, sh::Shape::kState},
    {"Scena13_EnterArea", 0x561E00, 0x47E, kCalls561E00, SH_N(kCalls561E00), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_EnterArea), 0x0, false, sh::Shape::kState},
    {"Scena13_Run", 0x562280, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_Run), 0x0, false, sh::Shape::kState},
    {"Scena13_Run1", 0x562290, 0x170, kCalls562290, SH_N(kCalls562290), nullptr, 0, kTables562290, SH_N(kTables562290), reinterpret_cast<const void*>(&::Scena13_Run1), 0x0, false, sh::Shape::kState},
    {"Scena13_Run2", 0x562400, 0x2F8, kCalls562400, SH_N(kCalls562400), nullptr, 0, kTables562400, SH_N(kTables562400), reinterpret_cast<const void*>(&::Scena13_Run2), 0x0, false, sh::Shape::kState},
    {"Scena13_SpawnPairA", 0x562700, 0x94, kCalls562700, SH_N(kCalls562700), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_SpawnPairA), 0x0, false, sh::Shape::kState},
    {"Scena13_SpawnPairB", 0x5627A0, 0x94, kCalls5627A0, SH_N(kCalls5627A0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_SpawnPairB), 0x0, false, sh::Shape::kState},
    {"Scena13_Run3", 0x562840, 0x4A9, kCalls562840, SH_N(kCalls562840), nullptr, 0, kTables562840, SH_N(kTables562840), reinterpret_cast<const void*>(&::Scena13_Run3), 0x0, false, sh::Shape::kState},
    {"Scena13_Run4", 0x562CF0, 0x3A0, kCalls562CF0, SH_N(kCalls562CF0), nullptr, 0, kTables562CF0, SH_N(kTables562CF0), reinterpret_cast<const void*>(&::Scena13_Run4), 0x0, false, sh::Shape::kState},
    {"Scena13_Run5", 0x563090, 0x300, kCalls563090, SH_N(kCalls563090), nullptr, 0, kTables563090, SH_N(kTables563090), reinterpret_cast<const void*>(&::Scena13_Run5), 0x0, false, sh::Shape::kState},
    {"Scena13_ToneLevels", 0x563390, 0x5C, kCalls563390, SH_N(kCalls563390), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_ToneLevels), 0x0, false, sh::Shape::kEntry},
    {"Scena13_Run6", 0x5633F0, 0x624, kCalls5633F0, SH_N(kCalls5633F0), nullptr, 0, kTables5633F0, SH_N(kTables5633F0), reinterpret_cast<const void*>(&::Scena13_Run6), 0x0, false, sh::Shape::kState},
    {"Scena13_Run7", 0x563A20, 0x4F8, kCalls563A20, SH_N(kCalls563A20), nullptr, 0, kTables563A20, SH_N(kTables563A20), reinterpret_cast<const void*>(&::Scena13_Run7), 0x0, false, sh::Shape::kState},
    {"Scena13_Caption", 0x563F20, 0xB9, kCalls563F20, SH_N(kCalls563F20), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_Caption), 0x0, false, sh::Shape::kEntry},
    {"Scena13_Run8", 0x563FE0, 0x140, kCalls563FE0, SH_N(kCalls563FE0), nullptr, 0, kTables563FE0, SH_N(kTables563FE0), reinterpret_cast<const void*>(&::Scena13_Run8), 0x0, false, sh::Shape::kState},
    {"Scena13_ObjectTrigger", 0x564120, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_ObjectTrigger), 0x0, false, sh::Shape::kObject},
    {"Scena13_Object00", 0x564140, 0xC, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_Object00), 0x0, false, sh::Shape::kObject},
    {"Scena13_Object01", 0x564150, 0x14, kCalls564150, SH_N(kCalls564150), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_Object01), 0x0, false, sh::Shape::kObject},
    {"Scena13_Object02", 0x564170, 0x14, kCalls564170, SH_N(kCalls564170), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_Object02), 0x0, false, sh::Shape::kObject},
    {"Scena13_Object03", 0x564190, 0x2B, kCalls564190, SH_N(kCalls564190), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_Object03), 0x0, false, sh::Shape::kObject},
    {"Scena13_StepHook", 0x5641C0, 0x40B, kCalls5641C0, SH_N(kCalls5641C0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_StepHook), 0xFF, false, sh::Shape::kHook},
    {"Scena13_ArriveHook", 0x5645D0, 0xC8, kCalls5645D0, SH_N(kCalls5645D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena13_ArriveHook), 0xFF, false, sh::Shape::kHook},
};
// ---- chapter 14
// Scena14_Frame 0x5646A0: 0xE bytes; +0x7 note: jmp through .data 0x661834, 3 code entries (a data_tables entry); root: chapter 14 slot 0, the frame (Field_ModeDispatch)
// ScenaShared_State0 0x5646B0: 0x8 bytes
// Scena14_EnterArea 0x5646C0: 0x741 bytes
constexpr sh::CallSite kCalls5646C0[] = {{0x13, 0x57C140}, {0x28, 0x57C140}, {0x3B, 0x5341A0}, {0x42, 0x531F90}, {0x5C, 0x564E10}, {0x74, 0x57C140}, {0x88, 0x57C140}, {0x96, 0x5341A0}, {0x9D, 0x531F90}, {0xAE, 0x532ED0}, {0xC9, 0x57C140}, {0xDE, 0x57C140}, {0xEC, 0x5341A0}, {0xF3, 0x531F90}, {0x104, 0x532ED0}, {0x11E, 0x57C140}, {0x133, 0x57C140}, {0x141, 0x5341A0}, {0x148, 0x531F90}, {0x168, 0x57C140}, {0x17F, 0x5341A0}, {0x186, 0x531F90}, {0x1AB, 0x564E80}, {0x1B5, 0x587740}, {0x1CE, 0x57C140}, {0x1E3, 0x57C0F0}, {0x1EA, 0x531F90}, {0x1F2, 0x4205D0}, {0x215, 0x57C140}, {0x229, 0x57C0F0}, {0x230, 0x5341A0}, {0x249, 0x57C140}, {0x25E, 0x57C0F0}, {0x265, 0x531F90}, {0x27E, 0x57C140}, {0x293, 0x57C0F0}, {0x299, 0x531F90}, {0x2C3, 0x57C140}, {0x2D7, 0x57C140}, {0x2E5, 0x5341A0}, {0x2EC, 0x531F90}, {0x2FD, 0x532ED0}, {0x319, 0x57C140}, {0x32E, 0x57C0F0}, {0x349, 0x57C140}, {0x360, 0x5341A0}, {0x367, 0x531F90}, {0x388, 0x57C140}, {0x39E, 0x5341A0}, {0x3A4, 0x531F90}, {0x3E1, 0x57C140}, {0x422, 0x564E80}, {0x438, 0x57C140}, {0x451, 0x57C140}, {0x466, 0x5341A0}, {0x46D, 0x531F90}, {0x482, 0x57C140}, {0x4A5, 0x57C140}, {0x4B3, 0x5341A0}, {0x4BA, 0x531F90}, {0x4C8, 0x57C0F0}, {0x4D0, 0x589810}, {0x50E, 0x5720C0}, {0x539, 0x57C140}, {0x571, 0x57C140}, {0x58A, 0x57C140}, {0x5BF, 0x564E80}, {0x5C6, 0x5341C0}, {0x5CD, 0x531F90}, {0x5DF, 0x57C140}, {0x5F4, 0x57C0F0}, {0x5FA, 0x531F90}, {0x614, 0x57C140}, {0x628, 0x57C140}, {0x63D, 0x57C0F0}, {0x64B, 0x57C110}, {0x652, 0x531F90}, {0x66E, 0x56F670}, {0x693, 0x57C140}, {0x6A8, 0x57C0F0}, {0x6AF, 0x5341A0}, {0x6B6, 0x531F90}, {0x6D2, 0x57C140}, {0x6E6, 0x57C0F0}, {0x6ED, 0x531F90}, {0x715, 0x57C140}, {0x722, 0x531F90}};
// Scena14_GreyClut 0x564E10: 0x70 bytes
// Scena14_SpawnEffect 0x564E80: 0x67 bytes
constexpr sh::CallSite kCalls564E80[] = {{0x0, 0x589810}};
// Scena14_Run 0x564EF0: 0xE bytes; +0x7 note: jmp through .data 0x661840, 10 code entries (a data_tables entry)
// Scena14_Run1 0x564F00: 0x336 bytes
constexpr sh::CallSite kCalls564F00[] = {{0x3C, 0x531F90}, {0x65, 0x57C0F0}, {0x7B, 0x594E00}, {0xC4, 0x4976D0}, {0xFF, 0x564E80}, {0x117, 0x5905D0}, {0x13F, 0x4976D0}, {0x17A, 0x564E80}, {0x192, 0x5905D0}, {0x1BA, 0x4976D0}, {0x1DB, 0x57C7A0}, {0x1E9, 0x57C110}, {0x202, 0x594E00}, {0x22F, 0x5905D0}, {0x253, 0x57C110}, {0x272, 0x594E00}, {0x284, 0x57C0F0}, {0x292, 0x57C0F0}, {0x2AB, 0x594E00}, {0x2C4, 0x57C0F0}};
constexpr sh::JumpTable kTables564F00[] = {{0x1E, 0x2F4, 10}};
// Scena14_Run2 0x565240: 0x634 bytes
constexpr sh::CallSite kCalls565240[] = {{0xAA, 0x532ED0}, {0xEA, 0x587740}, {0x135, 0x587740}, {0x162, 0x565880}, {0x1A1, 0x587740}, {0x1CF, 0x565880}, {0x243, 0x587740}, {0x254, 0x532ED0}, {0x25B, 0x4410B0}, {0x27D, 0x594E00}, {0x28B, 0x57C0F0}, {0x298, 0x57C0F0}, {0x2CF, 0x495040}, {0x2F7, 0x4976D0}, {0x339, 0x594E00}, {0x36F, 0x495040}, {0x3AF, 0x57C7A0}, {0x3BD, 0x57C0F0}, {0x3D5, 0x4976D0}, {0x3FA, 0x57C7A0}, {0x41E, 0x57C7A0}, {0x425, 0x531F90}, {0x432, 0x57C0F0}, {0x43D, 0x590BB0}, {0x453, 0x57C7A0}, {0x461, 0x57C0F0}, {0x468, 0x531F90}, {0x48D, 0x589810}, {0x4D4, 0x594E00}, {0x522, 0x4976D0}, {0x575, 0x594E00}};
constexpr sh::JumpTable kTables565240[] = {{0x14, 0x5A0, 37}};
// Scena14_Shake 0x565880: 0x28 bytes
// Scena14_LeaveToC4 0x5658B0: 0x5A bytes
constexpr sh::CallSite kCalls5658B0[] = {{0x32, 0x594E00}, {0x3A, 0x57C7C0}};
// Scena14_Run3 0x565910: 0x2D8 bytes
constexpr sh::CallSite kCalls565910[] = {{0x38, 0x594E00}, {0x5D, 0x57C0F0}, {0x73, 0x594E00}, {0xB6, 0x495040}, {0xFA, 0x594E00}, {0x120, 0x57C0F0}, {0x139, 0x594E00}, {0x16A, 0x594E00}, {0x189, 0x495040}, {0x1B0, 0x4976D0}, {0x1E9, 0x533E50}, {0x1F0, 0x5A9949}, {0x214, 0x57C7C0}, {0x229, 0x57C0F0}, {0x23F, 0x594E00}, {0x26D, 0x495040}, {0x290, 0x57C7A0}};
constexpr sh::JumpTable kTables565910[] = {{0x13, 0x2A4, 13}};
// Scena14_Run4 0x565BF0: 0x410 bytes
constexpr sh::CallSite kCalls565BF0[] = {{0x2B, 0x57C0F0}, {0x44, 0x594E00}, {0x7D, 0x564E80}, {0xBF, 0x564E80}, {0xDF, 0x531F90}, {0xFE, 0x495040}, {0x126, 0x4976D0}, {0x15B, 0x57C0F0}, {0x174, 0x594E00}, {0x1AB, 0x589810}, {0x1D6, 0x566000}, {0x1E4, 0x495040}, {0x203, 0x566000}, {0x20A, 0x4976D0}, {0x25A, 0x564E80}, {0x288, 0x566000}, {0x29A, 0x495040}, {0x2A9, 0x566000}, {0x2CB, 0x4976D0}, {0x313, 0x594E00}, {0x347, 0x495040}, {0x358, 0x587A20}, {0x371, 0x454810}, {0x391, 0x57C0F0}};
constexpr sh::JumpTable kTables565BF0[] = {{0x18, 0x3AC, 25}};
// Scena14_ScrollView 0x566000: 0x83 bytes
constexpr sh::CallSite kCalls566000[] = {{0x47, 0x5720C0}, {0x53, 0x5725F0}, {0x58, 0x56FCA0}, {0x74, 0x5720C0}};
// Scena14_Run5 0x566090: 0x3CA bytes
constexpr sh::CallSite kCalls566090[] = {{0x71, 0x564E80}, {0x99, 0x57C7A0}, {0xBC, 0x5734F0}, {0xE4, 0x57C0F0}, {0x139, 0x532ED0}, {0x199, 0x565880}, {0x1A4, 0x589810}, {0x1DB, 0x565880}, {0x20D, 0x565880}, {0x235, 0x4410B0}, {0x247, 0x57C7A0}, {0x266, 0x57C0F0}, {0x27F, 0x4976D0}, {0x2A5, 0x57C7A0}, {0x2C4, 0x531F90}, {0x2DF, 0x587B80}, {0x2EA, 0x587910}, {0x2F1, 0x591900}, {0x2F8, 0x4976D0}, {0x310, 0x587A00}, {0x322, 0x587B90}, {0x348, 0x57C7A0}};
constexpr sh::JumpTable kTables566090[] = {{0x1D, 0x35C, 19}};
// Scena14_Run6 0x566460: 0xBE0 bytes
constexpr sh::CallSite kCalls566460[] = {{0x19, 0x57C7C0}, {0x20, 0x531F90}, {0x46, 0x57C0F0}, {0x5F, 0x594E00}, {0xA3, 0x567040}, {0xF8, 0x564E80}, {0x146, 0x567040}, {0x190, 0x57C0F0}, {0x198, 0x57C7A0}, {0x1B7, 0x531F90}, {0x21C, 0x531F90}, {0x229, 0x531F90}, {0x241, 0x589810}, {0x2B8, 0x57C0F0}, {0x2CB, 0x594E00}, {0x2FA, 0x495040}, {0x351, 0x564E80}, {0x372, 0x495040}, {0x379, 0x587BE0}, {0x39B, 0x587B40}, {0x3A9, 0x4976D0}, {0x3EA, 0x594E00}, {0x428, 0x564E10}, {0x42F, 0x495040}, {0x47B, 0x495040}, {0x4A9, 0x4976D0}, {0x4EF, 0x594E00}, {0x52F, 0x4976D0}, {0x559, 0x594E00}, {0x599, 0x4976D0}, {0x5C0, 0x594E00}, {0x600, 0x4976D0}, {0x627, 0x594E00}, {0x688, 0x4976D0}, {0x6AF, 0x594E00}, {0x710, 0x4976D0}, {0x73A, 0x594E00}, {0x79B, 0x4976D0}, {0x7C5, 0x594E00}, {0x7F2, 0x589810}, {0x847, 0x587740}, {0x866, 0x589810}, {0x89D, 0x587740}, {0x8BC, 0x589810}, {0x902, 0x587740}, {0x932, 0x594E00}, {0x97C, 0x587A20}, {0x9BF, 0x564E80}, {0x9FD, 0x57C0F0}, {0xA3D, 0x564E80}, {0xA96, 0x57C7A0}, {0xA9F, 0x587AE0}};
constexpr sh::JumpTable kTables566460[] = {{0x15, 0xAB8, 74}};
// Scena14_TalkByMember 0x567040: 0x93 bytes
constexpr sh::CallSite kCalls567040[] = {{0x7B, 0x4976D0}};
// Scena14_Run7 0x5670E0: 0x6CC bytes
constexpr sh::CallSite kCalls5670E0[] = {{0x1A, 0x57C7A0}, {0x27, 0x57C0F0}, {0x2E, 0x531F90}, {0x48, 0x57C7A0}, {0x56, 0x57C0F0}, {0x5D, 0x531F90}, {0x65, 0x4204D0}, {0x7C, 0x57C7A0}, {0x8A, 0x57C0F0}, {0x91, 0x531F90}, {0x99, 0x420580}, {0xB0, 0x57C7A0}, {0xBD, 0x57C0F0}, {0xC4, 0x531F90}, {0xCC, 0x420670}, {0xE3, 0x57C7A0}, {0xF1, 0x57C0F0}, {0xF8, 0x531F90}, {0x100, 0x420710}, {0x117, 0x57C7A0}, {0x125, 0x57C0F0}, {0x12D, 0x531F90}, {0x15D, 0x587B40}, {0x1AA, 0x454810}, {0x1B9, 0x531F90}, {0x1C5, 0x587AE0}, {0x251, 0x532ED0}, {0x258, 0x4410B0}, {0x27F, 0x594E00}, {0x2C3, 0x564E80}, {0x2CD, 0x587A20}, {0x2ED, 0x589810}, {0x32A, 0x5720C0}, {0x392, 0x4976D0}, {0x3BB, 0x5341A0}, {0x3C2, 0x5A9949}, {0x427, 0x531F90}, {0x438, 0x532ED0}, {0x4D6, 0x495040}, {0x4EE, 0x589810}, {0x52F, 0x5720C0}, {0x558, 0x4410B0}, {0x582, 0x57C0F0}, {0x598, 0x594E00}, {0x5B2, 0x587B80}, {0x5B9, 0x587910}, {0x5C0, 0x591900}, {0x5C7, 0x4976D0}, {0x5E1, 0x587A00}, {0x5F3, 0x587B90}, {0x61A, 0x56D6F0}, {0x628, 0x57C0F0}, {0x630, 0x57C7A0}};
constexpr sh::JumpTable kTables5670E0[] = {{0x16, 0x648, 33}};
// Scena14_ObjectTrigger 0x5677B0: 0x1F bytes; +0x14 note: call through .data 0x661884, 8 code entries (a data_tables entry); root: chapter 14 slot 1, the object trigger (0x56D6D0, the object)
// Scena14_Object00 0x5677D0: 0x39 bytes
constexpr sh::CallSite kCalls5677D0[] = {{0xD, 0x42C0A0}, {0x13, 0x4976D0}, {0x23, 0x42BA90}, {0x29, 0x4976D0}};
// Scena14_Object01 0x567810: 0x39 bytes
constexpr sh::CallSite kCalls567810[] = {{0xD, 0x42C0A0}, {0x13, 0x4976D0}, {0x23, 0x42BA90}, {0x29, 0x4976D0}};
// Scena14_Object02 0x567850: 0x39 bytes
constexpr sh::CallSite kCalls567850[] = {{0xD, 0x42C0A0}, {0x13, 0x4976D0}, {0x23, 0x42BA90}, {0x29, 0x4976D0}};
// Scena14_Object03 0x567890: 0x39 bytes
constexpr sh::CallSite kCalls567890[] = {{0xD, 0x42C0A0}, {0x13, 0x4976D0}, {0x23, 0x42BA90}, {0x29, 0x4976D0}};
// Scena14_Object04 0x5678D0: 0x39 bytes
constexpr sh::CallSite kCalls5678D0[] = {{0xD, 0x42C0A0}, {0x13, 0x4976D0}, {0x23, 0x42BA90}, {0x29, 0x4976D0}};
// Scena14_Object05 0x567910: 0x12 bytes
constexpr sh::CallSite kCalls567910[] = {{0x0, 0x57C7C0}};
// Scena14_Object06 0x567930: 0x1F bytes
constexpr sh::CallSite kCalls567930[] = {{0x0, 0x57C7C0}};
// Scena14_Object07 0x567950: 0x1F bytes
constexpr sh::CallSite kCalls567950[] = {{0x0, 0x57C7C0}};
// Scena14_StepHook 0x567970: 0x117 bytes; root: chapter 14 slot 2, the step hook (x, z), al
constexpr sh::CallSite kCalls567970[] = {{0x28, 0x57C140}, {0x47, 0x57C7C0}, {0x76, 0x57C140}, {0x90, 0x536700}, {0xAB, 0x536700}, {0xC7, 0x536700}, {0xE6, 0x536700}, {0xFA, 0x57C7C0}};
// Scena14_ArriveHook 0x567A90: 0x32F bytes; root: chapter 14 slot 3, the arrive hook (x, z), al
constexpr sh::CallSite kCalls567A90[] = {{0x32, 0x57C140}, {0x57, 0x57C7C0}, {0x74, 0x57C140}, {0x91, 0x57C7C0}, {0xAE, 0x57C140}, {0xCB, 0x57C7C0}, {0xF1, 0x57C140}, {0x105, 0x57C140}, {0x122, 0x57C7C0}, {0x13F, 0x57C140}, {0x15C, 0x57C7C0}, {0x193, 0x57C140}, {0x1B2, 0x57C7C0}, {0x1D5, 0x57C140}, {0x1F2, 0x57C7C0}, {0x1F9, 0x587B40}, {0x203, 0x587A20}, {0x239, 0x57C140}, {0x254, 0x57C7C0}, {0x28F, 0x57C140}, {0x2AC, 0x57C7C0}, {0x2ED, 0x57C140}, {0x302, 0x57C140}, {0x30E, 0x57C7C0}};
const sh::Clone kClones14[] = {
    {"Scena14_Frame", 0x5646A0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Frame), 0x0, false, sh::Shape::kSlot},
    {"ScenaShared_State0", 0x5646B0, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::ScenaShared_State0), 0x0, false, sh::Shape::kState},
    {"Scena14_EnterArea", Wrapper(&Sc13EnterArea14Theirs), kJmpWrapper, nullptr, 0, nullptr, 0, nullptr, 0,   // this file's copy
     reinterpret_cast<const void*>(&::Scena14_EnterArea), 0x0, false, sh::Shape::kState},
    {"Scena14_GreyClut", 0x564E10, 0x70, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_GreyClut), 0x0, false, sh::Shape::kState},
    {"Scena14_SpawnEffect", 0x564E80, 0x67, kCalls564E80, SH_N(kCalls564E80), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_SpawnEffect), 0xFF, false, sh::Shape::kEntry},
    {"Scena14_Run", 0x564EF0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Run), 0x0, false, sh::Shape::kState},
    {"Scena14_Run1", 0x564F00, 0x336, kCalls564F00, SH_N(kCalls564F00), nullptr, 0, kTables564F00, SH_N(kTables564F00), reinterpret_cast<const void*>(&::Scena14_Run1), 0x0, false, sh::Shape::kState},
    {"Scena14_Run2", 0x565240, 0x634, kCalls565240, SH_N(kCalls565240), nullptr, 0, kTables565240, SH_N(kTables565240), reinterpret_cast<const void*>(&::Scena14_Run2), 0x0, false, sh::Shape::kState},
    {"Scena14_Shake", 0x565880, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Shake), 0x0, false, sh::Shape::kEntry},
    {"Scena14_LeaveToC4", 0x5658B0, 0x5A, kCalls5658B0, SH_N(kCalls5658B0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_LeaveToC4), 0x0, false, sh::Shape::kState},
    {"Scena14_Run3", 0x565910, 0x2D8, kCalls565910, SH_N(kCalls565910), nullptr, 0, kTables565910, SH_N(kTables565910), reinterpret_cast<const void*>(&::Scena14_Run3), 0x0, false, sh::Shape::kState},
    {"Scena14_Run4", 0x565BF0, 0x410, kCalls565BF0, SH_N(kCalls565BF0), nullptr, 0, kTables565BF0, SH_N(kTables565BF0), reinterpret_cast<const void*>(&::Scena14_Run4), 0x0, false, sh::Shape::kState},
    {"Scena14_ScrollView", 0x566000, 0x83, kCalls566000, SH_N(kCalls566000), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_ScrollView), 0x0, false, sh::Shape::kState},
    {"Scena14_Run5", 0x566090, 0x3CA, kCalls566090, SH_N(kCalls566090), nullptr, 0, kTables566090, SH_N(kTables566090), reinterpret_cast<const void*>(&::Scena14_Run5), 0x0, false, sh::Shape::kState},
    {"Scena14_Run6", 0x566460, 0xBE0, kCalls566460, SH_N(kCalls566460), nullptr, 0, kTables566460, SH_N(kTables566460), reinterpret_cast<const void*>(&::Scena14_Run6), 0x0, false, sh::Shape::kState},
    {"Scena14_TalkByMember", 0x567040, 0x93, kCalls567040, SH_N(kCalls567040), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_TalkByMember), 0xFFFF, false, sh::Shape::kEntry},
    {"Scena14_Run7", 0x5670E0, 0x6CC, kCalls5670E0, SH_N(kCalls5670E0), nullptr, 0, kTables5670E0, SH_N(kTables5670E0), reinterpret_cast<const void*>(&::Scena14_Run7), 0x0, false, sh::Shape::kState},
    {"Scena14_ObjectTrigger", 0x5677B0, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_ObjectTrigger), 0x0, false, sh::Shape::kObject},
    {"Scena14_Object00", 0x5677D0, 0x39, kCalls5677D0, SH_N(kCalls5677D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Object00), 0x0, false, sh::Shape::kObject},
    {"Scena14_Object01", 0x567810, 0x39, kCalls567810, SH_N(kCalls567810), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Object01), 0x0, false, sh::Shape::kObject},
    {"Scena14_Object02", 0x567850, 0x39, kCalls567850, SH_N(kCalls567850), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Object02), 0x0, false, sh::Shape::kObject},
    {"Scena14_Object03", 0x567890, 0x39, kCalls567890, SH_N(kCalls567890), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Object03), 0x0, false, sh::Shape::kObject},
    {"Scena14_Object04", 0x5678D0, 0x39, kCalls5678D0, SH_N(kCalls5678D0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Object04), 0x0, false, sh::Shape::kObject},
    {"Scena14_Object05", 0x567910, 0x12, kCalls567910, SH_N(kCalls567910), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Object05), 0x0, false, sh::Shape::kObject},
    {"Scena14_Object06", 0x567930, 0x1F, kCalls567930, SH_N(kCalls567930), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Object06), 0x0, false, sh::Shape::kObject},
    {"Scena14_Object07", 0x567950, 0x1F, kCalls567950, SH_N(kCalls567950), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_Object07), 0x0, false, sh::Shape::kObject},
    {"Scena14_StepHook", 0x567970, 0x117, kCalls567970, SH_N(kCalls567970), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_StepHook), 0xFF, false, sh::Shape::kHook},
    {"Scena14_ArriveHook", 0x567A90, 0x32F, kCalls567A90, SH_N(kCalls567A90), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena14_ArriveHook), 0xFF, false, sh::Shape::kHook},
};

enum : unsigned {
    k13Frame, k13Start, k13EnterArea, k13Run, k13Run1, k13Run2, k13SpawnPairA, k13SpawnPairB, k13Run3, k13Run4, k13Run5,
    k13ToneLevels, k13Run6, k13Run7, k13Caption, k13Run8, k13ObjectTrigger, k13Object00, k13Object01, k13Object02,
    k13Object03, k13StepHook, k13ArriveHook, k13Count
};
enum : unsigned {
    k14Frame, k14State0, k14EnterArea, k14GreyClut, k14SpawnEffect, k14Run, k14Run1, k14Run2, k14Shake, k14LeaveToC4,
    k14Run3, k14Run4, k14ScrollView, k14Run5, k14Run6, k14TalkByMember, k14Run7, k14ObjectTrigger, k14Object00,
    k14Object01, k14Object02, k14Object03, k14Object04, k14Object05, k14Object06, k14Object07, k14StepHook,
    k14ArriveHook, k14Count
};
static_assert(sizeof kClones13 / sizeof kClones13[0] == k13Count, "one role per chapter-13 clone");
static_assert(sizeof kClones14 / sizeof kClones14[0] == k14Count, "one role per chapter-14 clone");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// The object tables' entries take arguments (the object and the flag row),
// which a DataTable's handler recorder does not log. So the seed writes a
// stand-in of the exact type into every entry, one per index (a wrong index
// is a different log), and the two tables are regions (the harness puts them
// back). Each logs against its dispatcher's own address, which no clone calls.
template <unsigned I> void __cdecl ObjectEntry13(unsigned char* object, std::uint32_t row) {
    sh::Record(0x564120, I, Key(object), row);
    sh::Stir();
}
template <unsigned I> void __cdecl ObjectEntry14(unsigned char* object, std::uint32_t row) {
    sh::Record(0x5677B0, I, Key(object), row);
    sh::Stir();
}
using ObjectFn = void (__cdecl*)(unsigned char*, std::uint32_t);
const ObjectFn kObjectEntries13[4] = {&ObjectEntry13<0>, &ObjectEntry13<1>, &ObjectEntry13<2>, &ObjectEntry13<3>};
const ObjectFn kObjectEntries14[8] = {&ObjectEntry14<0>, &ObjectEntry14<1>, &ObjectEntry14<2>, &ObjectEntry14<3>,
                                      &ObjectEntry14<4>, &ObjectEntry14<5>, &ObjectEntry14<6>, &ObjectEntry14<7>};

unsigned char& B(std::uint32_t a) { return *sh::Mem(a); }
std::uint16_t W(std::uint32_t a) {
    std::uint16_t w;
    std::memcpy(&w, sh::Mem(a), 2);
    return w;
}
void SetW(std::uint32_t a, std::uint32_t v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(sh::Mem(a), &w, 2);
}
void SetD(std::uint32_t a, std::uint32_t v) { std::memcpy(sh::Mem(a), &v, 4); }

// EventOp_6x runs its record on the object whose slot is in the word 0x903850,
// which the caller writes before each call: logged with the call.
std::uint32_t NoteSlot(const std::uint32_t*, std::uint32_t answer) {
    sh::Note(W(at::kSlotWord));
    return answer;
}
// The step hook of chapter 14 counts cells whose byte is 0xA6: half the
// answers are that byte, so the count is reached.
std::uint32_t CellByte(const std::uint32_t*, std::uint32_t answer) {
    return (sh::Noise() & 1) ? (answer & 0xFFFFFF00u) | 0xA6 : answer;
}

// The callees the standard set lacks, or records otherwise than these
// chapters need (docs/scenario_harness.md section 4; the group's listing
// stands).
#define SC13_RAW(name, address) name, address, address
#define SC13_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define SC13_THEIRS(name) #name, KeyOf(name), KeyOf(name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
const sh::Callee kCallees[] = {
    // tested on al alone (test al, al): garbage above a 0 must not matter
    {SC13_OURS(Flags_Test), 2, {kAll, kU8}, sh::Answer::kFlag, 0, 0},
    // ours now (group SE); the standard set lists it by its raw address
    {SC13_OURS(Field_StartEventBattle), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC13_OURS(Menu_DrawHand), 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    // clamped to the byte level (field_modes.cpp); its callers here leave garbage above
    {SC13_OURS(ClutStrip_FadeTo), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    // any sine: the absolute value, the shift and the mask all take their turn
    {SC13_OURS(Math_Sin), 1, {kAll}, sh::Answer::kGarbage, 0, 0},
    {SC13_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, sh::Answer::kGarbage, 0, 0, {}, &CellByte},
    {SC13_THEIRS(EventOp_6x), 1, {kAll}, sh::Answer::kGarbage, 0, 0, {}, &NoteSlot},
    // the group's own, called directly
    {SC13_OURS(Scena13_SpawnPairA), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC13_OURS(Scena13_SpawnPairB), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC13_OURS(Scena13_ToneLevels), 1, {kU16}, sh::Answer::kGarbage, 0, 0},
    {SC13_OURS(Scena13_Caption), 2, {kU16, kU8}, sh::Answer::kGarbage, 0, 0},
    {SC13_OURS(Scena14_GreyClut), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC13_OURS(Scena14_SpawnEffect), 6, {kU8, kU16, kU16, kU16, kU8, kAll}, sh::Answer::kFlag, 0, 0},
    {SC13_OURS(Scena14_Shake), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC13_OURS(Scena14_ScrollView), 0, {}, sh::Answer::kPhase, 0, 0},
    {SC13_OURS(Scena14_TalkByMember), 2, {kAll, kAll}, sh::Answer::kGarbage, 0, 0},
    // nobody's (group SX's this wave, or area code of a later one)
    {SC13_RAW("0x533E50", at::kPartyPass), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x57CD90", at::kFreeSprite), 0, {}, sh::Answer::kByte, 0xFF, 0x1D},
    {SC13_RAW("0x587B80", at::kSound587B80), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x591900", at::kParty591900), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x587890", at::kVoiceLevel), 2, {kU16, kAll}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x591920", at::kFindByte), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x420A90", at::kArea143), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x4204D0", at::kArea141a), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x420580", at::kArea141b), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x4205D0", at::kArea141c), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x420670", at::kArea141d), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x420710", at::kArea141e), 0, {}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x42BA90", at::kArea191), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    {SC13_RAW("0x42C0A0", at::kArea192), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    // the log slots of the object tables' stand-ins (keyed on the dispatchers'
    // own addresses, which no clone calls)
    {"Scena13_Objects[i]", 0x564120, 0x564120, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&ObjectEntry13<0>)},
    {"Scena14_Objects[i]", 0x5677B0, 0x5677B0, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&ObjectEntry14<0>)},
};
#undef SC13_OURS
#undef SC13_RAW
#undef SC13_THEIRS

// The state and run tables take no arguments: swapped for recorders.
const sh::DataTable kTables13[] = {{at::kStates13, at::kStateCount13}, {at::kRuns13, at::kRunCount13}};
const sh::DataTable kTables14[] = {{at::kStates14, at::kStateCount14}, {at::kRuns14, at::kRunCount14}};

// Beyond the harness's 22 standard regions.
const sh::Region kRegions13[] = {
    {at::kSlot13, 8},             // Scena13_Slot, Scena14_Slot
    {at::kExtraWord, 4},          // the word 0x802290
    {0x92BEE0, 4},                // MapView_ElevationOffset 0x92BEE2
    {0x904130, 0x28},             // Music_Track 0x904131, 0x904152, 0x904153
    {at::kMusicCurrent, 1},
    {at::kEntryByte, 1},
    {at::kCondFE, 1},
    {0x9039F0, 8},                // 0x9039F3..F5
    {0x903800, 8},                // Camera_ShiftY, the object pointer 0x903804
    {at::kCaptionText, 0x20},     // the caption offsets
    {at::kCaptionIndex, 4},
    {at::kObjects13, 4 * at::kObjectCount13},
};
const sh::Region kRegions14[] = {
    {at::kSlot13, 8},
    {0x92BEE0, 4},
    {0x904130, 0x28},
    {at::kMusicCurrent, 1},
    {at::kEntryByte, 1},
    {at::kCondFE, 1},
    {0x903800, 8},
    {at::kClut, at::kClutWords * 2},
    {at::kOrigin, 2},
    {at::kRecords, 8 * at::kRecordStride},
    {at::kRecordIndex, 1},
    {0x929F0C, 8},                // 0x929F10
    {0x92BF18, 4},                // Draw_OtSlot 0x92BF19
    {at::kSortOnX, 1},
    {at::kObjects14, 4 * at::kObjectCount14},
};

// Each run's steps (its switch's cases) and one past; and the counter value
// each step waits on (read off ours), so a step's action is reached.
struct Steps { unsigned char chapter, run; const unsigned char* v; unsigned n; };
constexpr unsigned char kSteps13_1[] = {0, 1, 2, 3, 4, 5, 6};
constexpr unsigned char kSteps13_2[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC};
constexpr unsigned char kSteps13_3[] = {0, 1, 2, 3, 4, 5, 6, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19};
constexpr unsigned char kSteps13_4[] = {0, 1, 2, 9, 0xA, 0xB, 0xC, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
constexpr unsigned char kSteps13_5[] = {0, 1, 2, 3, 4, 5, 6, 7, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11};
constexpr unsigned char kSteps13_6[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
constexpr unsigned char kSteps13_7[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E};
constexpr unsigned char kSteps13_8[] = {0, 1, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC};
constexpr unsigned char kSteps14_1[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0x13, 0x14, 0x15, 0x18, 0x19, 0x1A};
constexpr unsigned char kSteps14_2[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25};
constexpr unsigned char kSteps14_3[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD};
constexpr unsigned char kSteps14_4[] = {0, 1, 2, 3, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19};
constexpr unsigned char kSteps14_5[] = {0, 1, 2, 3, 5, 6, 7, 8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x14, 0x15, 0x16, 0x1E, 0x1F, 0x20, 0x21, 0x22};
constexpr unsigned char kSteps14_7[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1E, 0x1F, 0x20, 0x21};
#define SC13_STEPS(c, r, a) {c, r, a, sizeof a}
const Steps kSteps[] = {
    SC13_STEPS(13, 1, kSteps13_1),
    SC13_STEPS(13, 2, kSteps13_2),
    SC13_STEPS(13, 3, kSteps13_3),
    SC13_STEPS(13, 4, kSteps13_4),
    SC13_STEPS(13, 5, kSteps13_5),
    SC13_STEPS(13, 6, kSteps13_6),
    SC13_STEPS(13, 7, kSteps13_7),
    SC13_STEPS(13, 8, kSteps13_8),
    SC13_STEPS(14, 1, kSteps14_1),
    SC13_STEPS(14, 2, kSteps14_2),
    SC13_STEPS(14, 3, kSteps14_3),
    SC13_STEPS(14, 4, kSteps14_4),
    SC13_STEPS(14, 5, kSteps14_5),
    SC13_STEPS(14, 7, kSteps14_7),
};
#undef SC13_STEPS
// counter 0 unless `counter` says 3 (0x90384B)
struct Wait { unsigned char chapter, run, step, counter, value; };
const Wait kWaits[] = {
    {13, 1, 1, 0, 8},
    {13, 2, 1, 0, 3}, {13, 2, 2, 0, 0}, {13, 2, 3, 0, 2}, {13, 2, 4, 0, 0}, {13, 2, 5, 0, 5}, {13, 2, 6, 0, 0}, {13, 2, 7, 0, 1},
    {13, 3, 0, 0, 2}, {13, 3, 2, 0, 3}, {13, 3, 4, 0, 0}, {13, 3, 0xB, 0, 0x12}, {13, 3, 0x14, 0, 1},
    {13, 4, 0xA, 0, 0x10}, {13, 4, 0x16, 0, 7}, {13, 4, 0x17, 0, 8}, {13, 4, 0x19, 0, 1}, {13, 4, 0x1A, 0, 0x18},
    {13, 5, 0xC, 0, 1}, {13, 5, 0xE, 0, 4}, {13, 5, 0x10, 0, 0},
    {13, 6, 1, 0, 7}, {13, 6, 3, 0, 0xE}, {13, 6, 4, 0, 0xD}, {13, 6, 8, 0, 0xB}, {13, 6, 9, 0, 0xC}, {13, 6, 0xF, 0, 0},
    {13, 6, 0x1A, 0, 2}, {13, 6, 0x1D, 0, 0xA}, {13, 6, 0x1E, 0, 0},
    {13, 7, 0, 0, 0xB}, {13, 7, 7, 0, 0xE}, {13, 7, 0xA, 0, 0}, {13, 7, 0x14, 0, 9}, {13, 7, 0x17, 0, 0xB}, {13, 7, 0x19, 0, 0x34},
    {13, 7, 0x1A, 0, 0x37}, {13, 7, 0x1D, 0, 0},
    {13, 8, 8, 0, 0x28},
    {14, 1, 1, 0, 1},
    {14, 2, 0, 0, 2}, {14, 2, 2, 0, 4}, {14, 2, 4, 0, 6}, {14, 2, 6, 0, 8}, {14, 2, 0xC, 0, 1}, {14, 2, 0x11, 0, 0}, {14, 2, 0x1E, 0, 5},
    {14, 3, 0, 0, 2}, {14, 3, 1, 0, 8}, {14, 3, 4, 0, 0xA}, {14, 3, 5, 0, 0x18}, {14, 3, 6, 0, 0x1B}, {14, 3, 7, 0, 0x24},
    {14, 4, 1, 0, 1}, {14, 4, 2, 0, 2}, {14, 4, 0xB, 3, 0x18}, {14, 4, 0x18, 3, 0x31},
    {14, 5, 0, 0, 1}, {14, 5, 1, 0, 2}, {14, 5, 2, 0, 0}, {14, 5, 6, 0, 1}, {14, 5, 0xA, 0, 0x1C}, {14, 5, 0xB, 0, 0x1E},
    {14, 5, 0x1F, 0, 1}, {14, 5, 0x21, 0, 0},
    {14, 6, 1, 3, 0x14}, {14, 6, 4, 0, 0xB}, {14, 6, 7, 0, 0}, {14, 6, 0xB, 0, 0x12}, {14, 6, 0xC, 0, 0x23}, {14, 6, 0xE, 0, 0},
    {14, 6, 0xF, 0, 1}, {14, 6, 0x10, 0, 2}, {14, 6, 0x11, 0, 3}, {14, 6, 0x12, 0, 0}, {14, 6, 0x40, 0, 0x14}, {14, 6, 0x41, 0, 0x15},
    {14, 6, 0x42, 0, 0x16}, {14, 6, 0x43, 0, 0}, {14, 6, 0x45, 0, 3}, {14, 6, 0x49, 0, 0},
    {14, 7, 6, 0, 7}, {14, 7, 0xB, 0, 0x19}, {14, 7, 0xD, 0, 0}, {14, 7, 0x10, 0, 1}, {14, 7, 0x11, 0, 9}, {14, 7, 0x12, 0, 0xA},
    {14, 7, 0x20, 0, 0},
};

int g_chapter = 13;
unsigned g_k;
unsigned g_seconds, g_index;

unsigned RunOf(unsigned k) {
    if (g_chapter == 13) {
        switch (k) {
        case k13Run1: return 1; case k13Run2: return 2; case k13Run3: return 3; case k13Run4: return 4;
        case k13Run5: return 5; case k13Run6: return 6; case k13Run7: return 7; case k13Run8: return 8;
        default: return 0;
        }
    }
    switch (k) {
    case k14Run1: return 1; case k14Run2: return 2; case k14Run3: return 3; case k14Run4: return 4;
    case k14Run5: return 5; case k14Run6: return 6; case k14Run7: return 7;
    default: return 0;
    }
}

// The areas and Cond_ByteFD values each chapter's code compares with.
const unsigned short kAreas13[] = {0x56, 0x58, 0x70, 0x7E, 0x7F, 0x8F, 0x90, 0x91, 0x97, 0x9B, 0xC2, 0x55};
const unsigned short kAreas14[] = {0xB, 0x2B, 0x67, 0x78, 0x8D, 0x8E, 0x90, 0x93, 0x94, 0x96, 0xAC, 0xBD, 0xBF, 0xC0, 0xC1, 0xC5, 0x8C};
std::uint32_t AnArea(std::uint32_t r) {
    return g_chapter == 13 ? kAreas13[r % (sizeof kAreas13 / sizeof kAreas13[0])] : kAreas14[r % (sizeof kAreas14 / sizeof kAreas14[0])];
}
std::uint32_t AnFD() { return SH_PICK(0, 1, 2, 3, 4, 5, 6); }
std::uint32_t AMember() { return SH_PICK(2, 4, 5, 6, 7, 8, 9); }

// A coordinate: a 16-bit cell with a fraction (0 half the time), the cell at
// a range's edge or one either side.
std::uint32_t Cell(unsigned lo, unsigned n) {
    const std::uint32_t c = sh::Often() ? SH_PICK(0, 1) * (n - 1) + lo + SH_PICK(0, 0, 0xFFFFFFFFu, 1) : lo + sh::Next() % (n + 2) - 1;
    return (c & 0xFFFF) << 16 | (sh::Half() ? 0 : sh::Next() & 0xFFFF);
}
// A whole coordinate at a bound, one either side, or a cell off.
std::uint32_t Exact(std::uint32_t v) { return v + SH_PICK(0, 0, 0, 1, 0xFFFFFFFFu, 0x10000); }

// Each hook's rectangles, with the area and Cond_ByteFD (0xFF any) they are
// tested in. kind 0: x exact, z by its cell; 1: x and z by their cells; 2: x
// below a bound (signed), z by its cell; 3: x by its cell, z below a bound.
struct Rect { unsigned short area; unsigned char fd, kind; std::uint32_t x; unsigned xn; std::uint32_t z; unsigned zn; };
const Rect kStep13[] = {{0x8F, 1, 1, 0x25, 1, 0x1D, 2},   // area 0x90's cell tested first in 0x8F
                        {0x56, 1, 0, 0x428000, 0, 0x3A, 2}, {0x70, 4, 1, 0x36, 3, 0x32, 4},    {0x7F, 0, 0, 0x38000, 0, 6, 3},
                        {0x8F, 0, 0, 0x3B8000, 0, 0x1F, 4}, {0x8F, 1, 0, 0x428000, 0, 0x64, 3}, {0x90, 2, 1, 0x25, 1, 0x1D, 2},
                        {0x91, 0xFF, 1, 0xA, 2, 0x62, 3},   {0x97, 0xFF, 0, 0x148000, 0, 0x12, 9}, {0x9B, 0, 0, 0x28000, 0, 0x30, 2}};
const Rect kArrive13[] = {{0x8F, 0, 1, 0x2E, 1, 0x1F, 4}, {0x91, 5, 1, 0x40, 2, 0x6A, 1}};
const Rect kStep14[] = {{0x94, 4, 0, 0x738000, 0, 0x36, 2}, {0xBF, 0xFF, 1, 0x10, 0x40, 0x10, 0x40}};
const Rect kArrive14[] = {{0x8D, 0, 2, 0x4F8000, 0, 7, 8},    {0x8D, 0, 2, 0x498000, 0, 9, 4},    {0x8D, 0, 2, 0x408000, 0, 9, 4},
                          {0x8D, 1, 2, 0x398000, 0, 0x31, 4}, {0x8D, 1, 2, 0x2D8000, 0, 0x31, 4}, {0x8E, 1, 3, 0x13, 4, 0x1D8000, 0},
                          {0x8E, 3, 2, 0x2C8000, 0, 0x26, 4}, {0x94, 0, 1, 0x12, 3, 0x1C, 1},     {0xAC, 0, 2, 0xB8000, 0, 0x3F, 2},
                          {0xBF, 0, 1, 0x1D, 5, 0x14, 2}};
const Rect* g_rect = nullptr;   // the rectangle Seed chose for this round's hook, or none

// Seed's half of a hook's round: a rectangle, its area and Cond_ByteFD.
void PickRect(const Rect* r, unsigned n) {
    g_rect = sh::Often() ? &r[sh::Next() % n] : nullptr;
    if (!g_rect) return;
    if (sh::Often()) SetW(at::kArea, g_rect->area);
    if (g_rect->fd != 0xFF && sh::Often()) B(at::kCondFD) = g_rect->fd;
}
// Args' half: x and z at the rectangle's edges.
void AtRect(std::uint32_t* a) {
    if (!g_rect) return;
    const Rect& q = *g_rect;
    switch (q.kind) {
    case 0: a[0] = Exact(q.x); a[1] = Cell(q.z, q.zn); break;
    case 1: a[0] = Cell(q.x, q.xn); a[1] = Cell(q.z, q.zn); break;
    case 2: a[0] = Exact(q.x) - SH_PICK(0, 0x10000, 0x8000); a[1] = Cell(q.z, q.zn); break;
    default: a[0] = Cell(q.x, q.xn); a[1] = Exact(q.z) - SH_PICK(0, 0x10000, 0x8000); break;
    }
}

// The in-use byte of an effect record 0 (its effect over).
void Effect0(unsigned slot) { B(at::kEffects + (slot & 0xFF) * at::kEffectStride) = 0; }

void Seed(unsigned k) {
    g_k = k;
    // what must stay in range every round: the members counted (chapter 14's
    // run 7 copies a record per member into ObjTrio), the slot cells, the
    // record index, the object pointer run 6 of chapter 13 writes through
    B(at::kMemberCount) = static_cast<unsigned char>(sh::Next() % 4);
    B(at::kSlot13) = static_cast<unsigned char>(sh::Next() % 0x14);
    B(at::kSlot14) = static_cast<unsigned char>(sh::Next() % 0x14);
    B(at::kRecordIndex) = static_cast<unsigned char>(sh::Next() % 8);
    sh::SetPointer(at::kCamObject, sh::SpriteRecord(sh::Next()));
    for (unsigned i = 0; i < at::kObjectCount13; ++i) SetD(at::kObjects13 + 4 * i, KeyOf(kObjectEntries13[i]));
    for (unsigned i = 0; i < at::kObjectCount14; ++i) SetD(at::kObjects14 + 4 * i, KeyOf(kObjectEntries14[i]));
    // the cells the chapters compare, each most of the time at a value a
    // branch tests
    if (sh::Often()) SetW(at::kArea, AnArea(sh::Next()));
    if (sh::Often()) B(at::kCondFD) = static_cast<unsigned char>(AnFD());
    if (sh::Often()) B(at::kRequest) = static_cast<unsigned char>(SH_PICK(0, 2, 6, 1));
    if (sh::Half()) SetW(at::kWait, 0);
    if (sh::Often()) SetW(at::kTimer, SH_PICK(0, 1, 2, 0x10, 0xB3, 0xB4, 0xB5, 0xF0, 0xF1));
    if (sh::Often()) B(at::kCounters + 3) = static_cast<unsigned char>(SH_PICK(0x14, 0x18, 0x31, 0x30, 0x20));
    if (sh::Half()) Effect0(B(g_chapter == 13 ? at::kSlot13 : at::kSlot14));
    for (unsigned m = 0; m < 3; ++m)
        if (sh::Often()) B(at::kLeaderName + m * at::kTrioStride) = static_cast<unsigned char>(AMember());
    for (unsigned m = 0; m < 3; ++m)
        if (sh::Half()) B(at::kTrio + m * at::kTrioStride + 0x137) = 0;
    if (sh::Often()) B(at::kFacing) = static_cast<unsigned char>(SH_PICK(0, 6, 7, 1, 5));
    if (sh::Often()) SetW(at::kExtraWord, SH_PICK(0x70, 0x91, 0x71));
    if (sh::Often()) SetW(at::kInputHeld, SH_PICK(0x2000, 0x8000, 0xA000, 0));
    if (sh::Half()) B(at::kInputPressed) = static_cast<unsigned char>(B(at::kInputPressed) | 0x20);
    if (sh::Often())
        SetD(at::kExtra1Angle, (sh::Next() & 0xFFFFF000u) | SH_PICK(0x49F, 0x4A0, 0x4E0, 0x4E1, 0x4C0, 0xCC0, 0xFF8, 0x8));
    if (sh::Half()) B(at::kKind2X + 4) = 5;   // 0x905E68
    if (sh::Half()) SetD(at::kFocusX, 0x77FF);
    if (sh::Often()) B(at::kEffects) = static_cast<unsigned char>(sh::Next() & 0xBF);
    g_rect = nullptr;
    switch (g_chapter * 100 + k) {
    case 1300 + k13StepHook: PickRect(kStep13, sizeof kStep13 / sizeof kStep13[0]); break;
    case 1300 + k13ArriveHook: PickRect(kArrive13, sizeof kArrive13 / sizeof kArrive13[0]); break;
    case 1400 + k14StepHook: PickRect(kStep14, sizeof kStep14 / sizeof kStep14[0]); break;
    case 1400 + k14ArriveHook: PickRect(kArrive14, sizeof kArrive14 / sizeof kArrive14[0]); break;
    case 1300 + k13EnterArea:   // the areas and Cond_ByteFD values it tests, together
        if (sh::Often()) SetW(at::kArea, SH_PICK(0x56, 0x58, 0x70, 0x7E, 0x8F, 0x90, 0x91, 0xC2));
        if (sh::Often()) B(at::kCondFD) = static_cast<unsigned char>(SH_PICK(0, 1, 2, 4));
        break;
    case 1400 + k14EnterArea:
        if (sh::Often()) SetW(at::kArea, SH_PICK(0xB, 0x2B, 0x67, 0x78, 0x8D, 0x8E, 0x90, 0x93, 0x96, 0xAC, 0xBD, 0xBF, 0xC1, 0xC5));
        if (sh::Often()) B(at::kCondFD) = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3));
        break;
    case 1300 + k13Frame: case 1400 + k14Frame: B(at::kState) = static_cast<unsigned char>(sh::Next() % 3); break;
    case 1300 + k13Run: B(at::kRun) = static_cast<unsigned char>(sh::Next() % at::kRunCount13); break;
    case 1400 + k14Run: B(at::kRun) = static_cast<unsigned char>(sh::Next() % at::kRunCount14); break;
    case 1300 + k13ObjectTrigger:
        for (unsigned i = 0; i < 4; ++i) sh::SpriteRecord(i)[0x86] = static_cast<unsigned char>(sh::Next() % at::kObjectCount13);
        break;
    case 1400 + k14ObjectTrigger:
        for (unsigned i = 0; i < 4; ++i) sh::SpriteRecord(i)[0x86] = static_cast<unsigned char>(sh::Next() % at::kObjectCount14);
        break;
    case 1300 + k13Caption: {
        // the clock at each of the caption's edges, or anything
        g_seconds = SH_PICK(5, 10, 1, 2, 0, 0x89);
        g_index = sh::Next() % 16;
        const std::uint32_t total = (g_seconds * 30 - 1) & 0xFFFF;
        const std::uint32_t edges[] = {0, 1, 0x1F, 0x20, 0x21, total - 0x21, total - 0x20, total - 0x1F, total - 1, total, total + 1};
        const std::uint32_t clock = sh::Often() ? sh::Pick(edges, sizeof edges / sizeof edges[0]) : sh::Next();
        SetW(at::kTimer, total - clock);
        break;
    }
    default: {
        const unsigned run = RunOf(k);
        if (run == 0) break;
        for (const Steps& s : kSteps)
            if (s.chapter == g_chapter && s.run == run) B(at::kStep) = s.v[sh::Next() % s.n];
        if (g_chapter == 14 && run == 6) B(at::kStep) = static_cast<unsigned char>(sh::Next() % 0x4B);
        if (sh::Often())
            for (const Wait& w : kWaits)
                if (w.chapter == g_chapter && w.run == run && w.step == B(at::kStep)) B(at::kCounters + w.counter) = w.value;
        break;
    }
    }
}

void Args(unsigned k, std::uint32_t* a) {
    switch (g_chapter * 100 + k) {
    case 1300 + k13StepHook: case 1300 + k13ArriveHook: case 1400 + k14StepHook: case 1400 + k14ArriveHook: AtRect(a); break;
    case 1300 + k13Caption:
        a[0] = (a[0] & 0xFFFF0000u) | g_index;
        a[1] = (a[1] & 0xFFFFFF00u) | (g_seconds & 0xFF);
        break;
    case 1300 + k13ToneLevels:
        if (sh::Often()) a[0] = (a[0] & 0xFFFFF000u) | SH_PICK(0x4C0, 0xCC0, 0x4A0, 0x4E0, 0, 0xFFF, 0x2C0, 0x8C0);
        break;
    case 1400 + k14TalkByMember:
        a[0] = sh::Half() ? at::kTalkWho14 : at::kTalkWho14 + 4;
        a[1] = sh::Half() ? at::kTalkLines14 : at::kTalkLines14 + 8;
        break;
    default: break;
    }
}

// After a call, two in three (beyond the harness's own), from the hash given:
// counter 3, the area, Cond_ByteFD, a slot cell, an effect's in-use byte, who
// leads, the facing, the script flags' bits 3 and 4, the member count, the
// word 0x802290, the word 0x903850 (a caption's clock), Field_StatusBits bit 0.
void Disturb(std::uint32_t h) {
    const unsigned char v = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 13) {
    case 0: B(at::kCounters + 3) = (h >> 16) & 1 ? static_cast<unsigned char>(0x18 + (h >> 17) % 0x1A) : v; break;
    case 1: SetW(at::kArea, AnArea(h >> 16)); break;
    case 2: B(at::kCondFD) = static_cast<unsigned char>((h >> 16) % 7); break;
    case 3: B((h >> 16) & 1 ? at::kSlot13 : at::kSlot14) = static_cast<unsigned char>((h >> 17) % 0x14); break;
    case 4: B(at::kEffects + ((h >> 12) % 20) * at::kEffectStride) = static_cast<unsigned char>(v & 1); break;
    case 5: B(at::kLeaderName + ((h >> 12) % 3) * at::kTrioStride) = static_cast<unsigned char>(2 + (h >> 16) % 8); break;
    case 6: B(at::kFacing) = static_cast<unsigned char>((h >> 16) % 8); break;
    case 7: B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) ^ (8u << ((h >> 16) & 1))); break;
    case 8: B(at::kMemberCount) = static_cast<unsigned char>((h >> 16) % 4); break;
    case 9: SetW(at::kExtraWord, (h >> 16) & 1 ? 0x70 : 0x91); break;
    case 10: SetW(at::kSlotWord, (h >> 16) % 0x40); break;
    case 11: B(at::kStatusBits) = static_cast<unsigned char>(B(at::kStatusBits) ^ 1); break;
    default: B(at::kFacing) = static_cast<unsigned char>((h >> 16) & 1 ? 0 : 6 + ((h >> 17) & 1)); break;
    }
}

// After every disturbance, drawing on Noise() (the recorders' stream, the
// same on both passes): for every function, Field_StatusBits bit 0 flipped a
// quarter of the time and the object pointer 0x903804 moved a quarter of the
// time; for the functions that read the area again after their calls (the
// enter-area states and the hooks), the area moved to one of the chapter's,
// Cond_ByteFD and the facing moved, each half the time.
void Settle() {
    const std::uint32_t n = sh::Noise();
    // a bit of Field_StatusBits, which chapter 14's trips read again after
    // their area change or ScriptFlags_Set40 (and then set or clear bit 0):
    // a quarter of the time
    if ((n & 0x30) == 0x30) B(at::kStatusBits) = static_cast<unsigned char>(B(at::kStatusBits) ^ (1u << ((n >> 20) & 7)));
    // the object pointer chapter 13's run 6 reads again after its calls
    if ((n & 0xC0) == 0xC0) sh::SetPointer(at::kCamObject, sh::SpriteRecord(n >> 24));
    const bool rereads = g_chapter == 13 ? (g_k == k13EnterArea || g_k == k13StepHook || g_k == k13ArriveHook)
                                         : (g_k == k14EnterArea || g_k == k14StepHook || g_k == k14ArriveHook);
    if (!rereads) return;
    // chapter 13's step hook half the time between its areas 0x8F and 0x90
    // at Cond_ByteFD 1 / 2 (it loads the facing in one and stores it in the
    // other)
    if (g_chapter == 13 && g_k == k13StepHook && (n & 8)) {
        const bool second = (n >> 8) & 1;
        SetW(at::kArea, second ? 0x90 : 0x8F);
        B(at::kCondFD) = static_cast<unsigned char>(second ? 2 : 1);
        B(at::kFacing) = static_cast<unsigned char>((n >> 16) & 1 ? 0 : 5 + (n >> 17) % 3);
        return;
    }
    if (n & 1) SetW(at::kArea, AnArea(n >> 8));
    // Cond_ByteFD and the facing, read again after the flag tests
    if (n & 2) B(at::kCondFD) = static_cast<unsigned char>((n >> 12) % 6);
    if (n & 4) B(at::kFacing) = static_cast<unsigned char>((n >> 16) & 1 ? 0 : 5 + (n >> 17) % 3);
}

void RunChapter(int chapter, const sh::Clone* clones, unsigned n, const sh::DataTable* tables, unsigned n_tables,
                const sh::Region* regions, unsigned n_regions, const char* shadow) {
    g_chapter = chapter;
    sh::Group group = {shadow, clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables, regions, n_regions,
                       &Seed, &Disturb, 16000};
    group.args = &Args;
    group.settle = &Settle;
    group.chapter = chapter;
    sh::Run(group);
}

}  // namespace

// Two runs of the harness, one per chapter byte: chapter 13's 23 functions
// with Cond_ByteFA 13, chapter 14's 28 with 14.
void SelfTest() {
    g_sc13_ea14_copy = CopyWithTramps("Scena14_EnterArea", 0x5646C0, 0x741, kCalls5646C0, SH_N(kCalls5646C0));
    RunChapter(13, kClones13, k13Count, kTables13, sizeof kTables13 / sizeof kTables13[0], kRegions13,
               sizeof kRegions13 / sizeof kRegions13[0], "scena_sc13 (chapter 13)");
    RunChapter(14, kClones14, k14Count, kTables14, sizeof kTables14 / sizeof kTables14[0], kRegions14,
               sizeof kRegions14 / sizeof kRegions14[0], "scena_sc13 (chapter 14)");
}

}  // namespace scena_sc13
#undef SH_N
