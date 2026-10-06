// BOF3X_SHADOW=scena_sc9a: chapter 9's first block through the scenario
// harness (scenario_harness.h), once at start-up. docs/scena_sc9a.md section 4.
//
// The clone table (tools/scenario_rows.py --unit SC9a --clones, checked
// against a capstone reading of every function), each clone's call shape in
// its comment; the callees the standard set lacks or records otherwise; the
// state and run tables swapped for recorders, and typed stand-ins written into
// the object table; the regions beyond the standard ones; a seed per role; a
// disturbance of the chapter's cells.
//
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/scena_sc9a.h"
#include "game/scena_sc9a_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sc9a {
namespace {

namespace sh = scenario_harness;

// tools/scenario_rows.py --unit SC9a --clones (at 3e410e7), 2026-09-27: every
// jump internal, nothing REFUSED. Each function's call shape after its name.
// 0x553B30 Scena09_Frame: 0xE bytes; +0x7 note: jmp through .data 0x6613fc, 3 code entries (a data_tables entry); root: chapter 9 slot 0, the frame (Field_ModeDispatch); shape: vtable slot 0 (the frame): no arguments
// 0x553B40 Scena09_EnterArea: 0x728 bytes; shape: state handler (Scena09_States entry 1): no arguments
constexpr sh::CallSite kCalls553B40[] = {{0x1F, 0x57C140}, {0x38, 0x57C140}, {0x48, 0x57C7C0}, {0x54, 0x57C0F0}, {0x69, 0x57C0F0}, {0xA4, 0x5341C0}, {0xAF, 0x5341C0}, {0xB6, 0x5341C0}, {0xBD, 0x5341A0}, {0xD1, 0x5341C0}, {0xDB, 0x5341C0}, {0xE6, 0x5341C0}, {0xEC, 0x5341C0}, {0xF3, 0x5341A0}, {0x102, 0x5341C0}, {0x109, 0x5341A0}, {0x128, 0x57C140}, {0x136, 0x5734F0}, {0x147, 0x57C140}, {0x15B, 0x57C140}, {0x178, 0x57C140}, {0x184, 0x57C7C0}, {0x1BB, 0x57C140}, {0x1CF, 0x57C0F0}, {0x1DD, 0x57C7C0}, {0x209, 0x57C140}, {0x22A, 0x57C7C0}, {0x256, 0x57C140}, {0x264, 0x5734F0}, {0x282, 0x57C140}, {0x29B, 0x57C140}, {0x2A7, 0x57C7C0}, {0x2EE, 0x5341C0}, {0x2F9, 0x5341C0}, {0x307, 0x5341C0}, {0x311, 0x5341C0}, {0x318, 0x5341A0}, {0x331, 0x57C140}, {0x369, 0x5341C0}, {0x373, 0x5341C0}, {0x379, 0x5341A0}, {0x398, 0x57C140}, {0x3AD, 0x57C140}, {0x3BB, 0x5734F0}, {0x3DB, 0x5341C0}, {0x3E6, 0x5341C0}, {0x3F1, 0x5341C0}, {0x3F8, 0x5341A0}, {0x408, 0x57C140}, {0x41D, 0x57C140}, {0x486, 0x57C140}, {0x49A, 0x57C110}, {0x4C5, 0x57C140}, {0x4E0, 0x57C7C0}, {0x51D, 0x57C140}, {0x529, 0x57C7C0}, {0x553, 0x589810}, {0x588, 0x589810}, {0x5A4, 0x589810}, {0x5D9, 0x589810}, {0x621, 0x57C140}, {0x636, 0x57C140}, {0x64F, 0x57C7C0}, {0x683, 0x57C0F0}};
constexpr sh::JumpTable kTables553B40[] = {{0x9E, 0x6D4, 12}, {0x2E8, 0x704, 9}};
// 0x554270 Scena09_Run: 0xE bytes; +0x7 note: jmp through .data 0x661408, 17 code entries (a data_tables entry); shape: state handler (Scena09_States entry 2): no arguments
// 0x554280 Scena09_Run1: 0x7D bytes; shape: state handler (Scena09_Runs entry 1): no arguments
constexpr sh::CallSite kCalls554280[] = {{0x23, 0x57C0F0}, {0x2B, 0x57C7A0}, {0x5E, 0x57C0F0}, {0x74, 0x594E00}};
// 0x554300 Scena09_Run2: 0x1C8 bytes; shape: state handler (Scena09_Runs entry 2): no arguments
constexpr sh::CallSite kCalls554300[] = {{0x31, 0x5734F0}, {0x38, 0x531F90}, {0x70, 0x57C0F0}, {0x86, 0x594E00}, {0xBA, 0x587B40}, {0xC1, 0x587910}, {0xD1, 0x587A00}, {0xE9, 0x587AE0}, {0x117, 0x57C0F0}, {0x125, 0x57C110}, {0x12D, 0x57C7A0}, {0x14D, 0x57C0F0}, {0x163, 0x594E00}, {0x17D, 0x57C0F0}, {0x185, 0x57C7A0}};
constexpr sh::JumpTable kTables554300[] = {{0x17, 0x198, 12}};
// 0x5544D0 Scena09_Run3: 0x320 bytes; shape: state handler (Scena09_Runs entry 3): no arguments
constexpr sh::CallSite kCalls5544D0[] = {{0x28, 0x4976D0}, {0x35, 0x5734F0}, {0x3B, 0x531F90}, {0x5D, 0x495040}, {0x92, 0x589810}, {0xFD, 0x589810}, {0x1C9, 0x57C0F0}, {0x1DF, 0x594E00}, {0x1F8, 0x495040}, {0x227, 0x4976D0}, {0x23C, 0x587B40}, {0x255, 0x587910}, {0x266, 0x587A00}, {0x271, 0x5341A0}, {0x286, 0x57C0F0}, {0x2B1, 0x594E00}, {0x2BA, 0x587AE0}, {0x2CB, 0x57C7A0}};
constexpr sh::JumpTable kTables5544D0[] = {{0x14, 0x2E8, 14}};
// 0x5547F0 Scena09_Run4: 0x477 bytes; shape: state handler (Scena09_Runs entry 4): no arguments
constexpr sh::CallSite kCalls5547F0[] = {{0x30, 0x57C0F0}, {0x46, 0x594E00}, {0x6D, 0x57C7A0}, {0x96, 0x57C0F0}, {0xA4, 0x57C140}, {0xB0, 0x57C7A0}, {0xF1, 0x57C0F0}, {0xFE, 0x57C140}, {0x10A, 0x57C7A0}, {0x138, 0x531F90}, {0x175, 0x594E00}, {0x181, 0x5734F0}, {0x189, 0x589810}, {0x264, 0x57C0F0}, {0x298, 0x57C0F0}, {0x2C2, 0x594E00}, {0x2DD, 0x594E00}, {0x305, 0x57C7A0}, {0x31C, 0x572650}, {0x323, 0x531F90}, {0x360, 0x594E00}, {0x396, 0x594E00}, {0x3A2, 0x5734F0}, {0x3D0, 0x57C0F0}, {0x3D8, 0x57C7A0}};
constexpr sh::JumpTable kTables5547F0[] = {{0x1C, 0x408, 19}};
// 0x554C70 Scena09_Run5: 0xC4 bytes; shape: state handler (Scena09_Runs entry 5): no arguments
constexpr sh::CallSite kCalls554C70[] = {{0x2D, 0x531F90}, {0x4E, 0x57C0F0}, {0x56, 0x57C7A0}, {0x80, 0x57C7A0}};
constexpr sh::JumpTable kTables554C70[] = {{0x13, 0xA8, 7}};
// 0x554D40 Scena09_Run6: 0x88A bytes; shape: state handler (Scena09_Runs entry 6): no arguments
constexpr sh::CallSite kCalls554D40[] = {{0x31, 0x57C7A0}, {0x54, 0x4976D0}, {0x6E, 0x4976D0}, {0xBF, 0x57C0F0}, {0xC7, 0x57C7A0}, {0xDB, 0x531F90}, {0x111, 0x594E00}, {0x140, 0x594E00}, {0x16F, 0x594E00}, {0x188, 0x57C0F0}, {0x1C5, 0x594E00}, {0x1DD, 0x531F90}, {0x1FD, 0x495040}, {0x23B, 0x594E00}, {0x245, 0x589810}, {0x2D1, 0x495040}, {0x307, 0x57C0F0}, {0x34B, 0x594E00}, {0x363, 0x531F90}, {0x383, 0x495040}, {0x3C1, 0x594E00}, {0x3CB, 0x589810}, {0x457, 0x495040}, {0x48D, 0x57C0F0}, {0x4CA, 0x594E00}, {0x4E2, 0x4976D0}, {0x512, 0x531F90}, {0x541, 0x57C7A0}, {0x558, 0x495040}, {0x59D, 0x594E00}, {0x5A9, 0x495040}, {0x5DF, 0x57C0F0}, {0x5EC, 0x57C0F0}, {0x629, 0x594E00}, {0x640, 0x57C7A0}, {0x678, 0x4976D0}, {0x6A8, 0x495040}, {0x6AD, 0x533E50}, {0x6B4, 0x587B40}, {0x6CE, 0x57C7A0}, {0x6E3, 0x587910}, {0x705, 0x587A00}, {0x719, 0x495040}, {0x740, 0x587AE0}, {0x748, 0x57C7A0}};
constexpr sh::JumpTable kTables554D40[] = {{0x1C, 0x75C, 49}};
// 0x5555D0 Scena09_Run7: 0xF8 bytes; shape: state handler (Scena09_Runs entry 7): no arguments
constexpr sh::CallSite kCalls5555D0[] = {{0x27, 0x531F90}, {0x4D, 0x57C0F0}, {0x55, 0x57C7A0}, {0xA3, 0x594E00}, {0xC7, 0x57C7A0}};
constexpr sh::JumpTable kTables5555D0[] = {{0x14, 0xDC, 7}};
// 0x5556D0 Scena09_Run8: 0x4A1 bytes; shape: state handler (Scena09_Runs entry 8): no arguments
constexpr sh::CallSite kCalls5556D0[] = {{0x68, 0x531F90}, {0x92, 0x57C0F0}, {0x9A, 0x57C7A0}, {0xC9, 0x587740}, {0xD7, 0x531F90}, {0xFF, 0x4976D0}, {0x132, 0x594E00}, {0x148, 0x495040}, {0x159, 0x587740}, {0x18E, 0x532ED0}, {0x1E0, 0x589810}, {0x2B3, 0x4976D0}, {0x2D0, 0x4976D0}, {0x2ED, 0x4976D0}, {0x30A, 0x4976D0}, {0x342, 0x587740}, {0x376, 0x532ED0}, {0x37D, 0x4410B0}, {0x3C9, 0x531F90}, {0x3EE, 0x57C0F0}, {0x3FE, 0x57C7A0}};
constexpr sh::JumpTable kTables5556D0[] = {{0x1F, 0x418, 24}};
// 0x555B80 Scena09_Run9: 0x2BC bytes; shape: state handler (Scena09_Runs entry 9): no arguments
constexpr sh::CallSite kCalls555B80[] = {{0x35, 0x531F90}, {0x3C, 0x5734F0}, {0x5B, 0x531F90}, {0x78, 0x589810}, {0xC2, 0x532ED0}, {0xC9, 0x4410B0}, {0xEF, 0x531F90}, {0xF6, 0x495040}, {0x12A, 0x57C0F0}, {0x140, 0x594E00}, {0x15F, 0x495040}, {0x170, 0x587740}, {0x1B2, 0x57C7A0}, {0x1C6, 0x4976D0}, {0x1F1, 0x531F90}, {0x212, 0x57C0F0}, {0x21A, 0x57C7A0}, {0x22E, 0x4976D0}, {0x24E, 0x57C7A0}};
constexpr sh::JumpTable kTables555B80[] = {{0x1B, 0x260, 15}};
// 0x555E40 Scena09_Run14: 0xFC bytes; shape: state handler (Scena09_Runs entry 14): no arguments
constexpr sh::CallSite kCalls555E40[] = {{0x2F, 0x594E00}, {0x47, 0x495040}, {0x4C, 0x533E50}, {0x53, 0x587910}, {0x73, 0x587A00}, {0x99, 0x594E00}, {0xAA, 0x57C0F0}, {0xC1, 0x57C7A0}};
constexpr sh::JumpTable kTables555E40[] = {{0x13, 0xE8, 5}};
// 0x555F40 Scena09_Run15: 0xC77 bytes; shape: state handler (Scena09_Runs entry 15): no arguments
constexpr sh::CallSite kCalls555F40[] = {{0x3B, 0x531F90}, {0x61, 0x57C0F0}, {0x69, 0x57C7A0}, {0x94, 0x594E00}, {0xBD, 0x5341C0}, {0xCA, 0x57C0F0}, {0xD2, 0x57C7A0}, {0xFC, 0x57C7A0}, {0x128, 0x57C110}, {0x135, 0x531F90}, {0x172, 0x594E00}, {0x192, 0x57C0F0}, {0x19A, 0x57C7A0}, {0x1CA, 0x531F90}, {0x1F1, 0x57C0F0}, {0x1F9, 0x57C7A0}, {0x20E, 0x531F90}, {0x238, 0x4976D0}, {0x268, 0x594E00}, {0x298, 0x57C0F0}, {0x2A0, 0x57C7A0}, {0x2C5, 0x4976D0}, {0x2DF, 0x57C110}, {0x2EB, 0x57C0F0}, {0x304, 0x495040}, {0x337, 0x4976D0}, {0x375, 0x57C0F0}, {0x38B, 0x594E00}, {0x397, 0x5734F0}, {0x3BE, 0x587910}, {0x3CF, 0x587A00}, {0x3DC, bof3::addr::Sound_ResumeAll}, {0x566, 0x495040}, {0x5A6, 0x57C0F0}, {0x5BC, 0x594E00}, {0x5E0, 0x495040}, {0x60F, 0x5734F0}, {0x634, 0x57C0F0}, {0x649, 0x57C0F0}, {0x651, 0x57C7A0}, {0x671, 0x4976D0}, {0x696, 0x57C7A0}, {0x6BB, 0x4976D0}, {0x708, 0x594E00}, {0x722, 0x5734F0}, {0x74C, 0x57C7A0}, {0x783, 0x531F90}, {0x7D0, 0x594E00}, {0x7DC, 0x5734F0}, {0x811, 0x57C0F0}, {0x827, 0x594E00}, {0x85D, 0x594E00}, {0x892, 0x57C0F0}, {0x8A8, 0x594E00}, {0x8C7, 0x57C0F0}, {0x8CF, 0x57C7A0}, {0x8FA, 0x594E00}, {0x91A, 0x57C0F0}, {0x922, 0x57C7A0}, {0x937, 0x5341A0}, {0x95B, 0x594E00}, {0x974, 0x495040}, {0x9A3, 0x5734F0}, {0x9C3, 0x495040}, {0x9E8, 0x533E50}, {0x9F6, 0x587B40}, {0xA02, 0x587910}, {0xA13, 0x587A00}, {0xA2B, 0x4976D0}, {0xA79, 0x57C0F0}, {0xA8C, 0x594E00}, {0xA9D, 0x57C0F0}, {0xAAC, 0x57C7A0}};
constexpr sh::JumpTable kTables555F40[] = {{0x1E, 0xAD4, 67}};
// 0x556BC0 Scena09_Run16: 0x4E bytes; shape: state handler (Scena09_Runs entry 16): no arguments
constexpr sh::CallSite kCalls556BC0[] = {{0x20, 0x57C0F0}, {0x28, 0x57C7A0}, {0x3E, 0x531F90}};
// 0x556C10 Scena09_Run10: 0x2B bytes; shape: state handler (Scena09_Runs entry 10): no arguments
constexpr sh::CallSite kCalls556C10[] = {{0x14, 0x531F90}};
// 0x556C40 Scena09_Run11: 0x490 bytes; shape: state handler (Scena09_Runs entry 11): no arguments
constexpr sh::CallSite kCalls556C40[] = {{0x22, 0x531F90}, {0x47, 0x589810}, {0xB2, 0x589810}, {0xFD, 0x5720C0}, {0x163, 0x589810}, {0x1EF, 0x589810}, {0x26D, 0x589810}, {0x394, 0x56F670}, {0x3AD, 0x57C7A0}, {0x3BB, 0x57C0F0}, {0x3D1, 0x57C7A0}, {0x3D8, 0x531F90}, {0x3E7, 0x56D6F0}, {0x3FC, 0x57C7A0}, {0x403, 0x531F90}};
constexpr sh::JumpTable kTables556C40[] = {{0x1C, 0x424, 18}};
// 0x5570D0 Scena09_ObjectTrigger: 0x1F bytes; +0x14 note: call through .data 0x661450, 16 code entries (a data_tables entry); root: chapter 9 slot 1, the object trigger (0x56D6D0, the object); shape: vtable slot 1 (the object trigger): the object
// 0x5570F0 Scena09_Object01: 0x19 bytes; shape: state handler (Scena09_Objects entry 1): (object, bits) ignored
constexpr sh::CallSite kCalls5570F0[] = {{0x0, 0x57C7C0}};
// 0x557110 Scena09_Object02: 0x19 bytes; shape: state handler (Scena09_Objects entry 2): (object, bits) ignored
constexpr sh::CallSite kCalls557110[] = {{0x0, 0x57C7C0}};
// 0x557130 Scena09_Object03: 0x14 bytes; shape: state handler (Scena09_Objects entry 3): (object, bits) ignored
constexpr sh::CallSite kCalls557130[] = {{0x0, 0x57C7C0}};
// 0x557150 Scena09_Object04: 0x14 bytes; shape: state handler (Scena09_Objects entry 4): (object, bits) ignored
constexpr sh::CallSite kCalls557150[] = {{0x0, 0x57C7C0}};

// Scena09_Run15 has 73 call sites, more than the harness re-aims in one clone
// (64). So this file makes its copy itself - bof3::CloneOriginal with every
// site re-aimed at a trampoline that calls the harness's recorder for the
// callee (StandIn: the same log entry, disturbance and answer a re-aimed site
// gets), the jump table moved into the copy - and hands the harness, as its
// "original", a six-byte `jmp [copy]` of its own, which the harness clones
// like any function with no calls. Theirs is still Capcom's bytes, only
// relocated here instead of in the harness (scena_sc12_fuzz.cpp's Run4 / Run8
// are the model).
template <typename F> F Stub(F f) {
    return reinterpret_cast<F>(const_cast<void*>(sh::StandIn(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f)))));
}
void __cdecl TFlagsSet(unsigned char* b, unsigned i) { Stub(Flags_Set)(b, i); }
void __cdecl TFlagsClear(unsigned char* b, unsigned i) { Stub(Flags_Clear)(b, i); }
void __cdecl TClear40() { Stub(ScriptFlags_Clear40)(); }
void __cdecl TMsg(unsigned short id) { Stub(Msg_OpenScript)(id); }
void __cdecl TChangeArea(unsigned area, int x, int z, unsigned flags) { Stub(Field_ChangeArea)(area, x, z, flags); }
void __cdecl TKind2(unsigned char a) { Stub(Kind2_Place)(a); }
unsigned __cdecl TDropIn(unsigned e) { return Stub(Party_DropIn)(e); }
void __cdecl TTransition(unsigned char k) { Stub(Transition_Start)(k); }
void __cdecl TPartyPass() { Stub(reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(at::kPartyPass)))(); }
void __cdecl TFadeOutStop(int f) { Stub(Music_FadeOutStop)(f); }
void __cdecl TLoadStream(unsigned id) { Stub(Sound_LoadStream)(id); }
int __cdecl TStreamDone() { return Stub(Sound_StreamDone)(); }
void __cdecl TCallA(unsigned n) { Stub(Scenario_CallA)(n); }
void __cdecl TCallB(unsigned n) { Stub(Scenario_CallB)(n); }
void __cdecl TResumeAll() { Stub(Sound_ResumeAll)(); }

struct Tramp { std::uint32_t target; const void* to; };
const Tramp kTramps[] = {
    {0x57C0F0, reinterpret_cast<const void*>(&TFlagsSet)},    {0x57C110, reinterpret_cast<const void*>(&TFlagsClear)},
    {0x57C7A0, reinterpret_cast<const void*>(&TClear40)},     {0x4976D0, reinterpret_cast<const void*>(&TMsg)},
    {0x594E00, reinterpret_cast<const void*>(&TChangeArea)},  {0x5734F0, reinterpret_cast<const void*>(&TKind2)},
    {0x531F90, reinterpret_cast<const void*>(&TDropIn)},      {0x495040, reinterpret_cast<const void*>(&TTransition)},
    {0x533E50, reinterpret_cast<const void*>(&TPartyPass)},   {0x587B40, reinterpret_cast<const void*>(&TFadeOutStop)},
    {0x587910, reinterpret_cast<const void*>(&TLoadStream)},  {0x587A00, reinterpret_cast<const void*>(&TStreamDone)},
    {0x5341A0, reinterpret_cast<const void*>(&TCallA)},       {0x5341C0, reinterpret_cast<const void*>(&TCallB)},
    {bof3::addr::Sound_ResumeAll, reinterpret_cast<const void*>(&TResumeAll)},
};

}  // namespace
}  // namespace scena_sc9a

extern "C" {
void* g_sc9a_run15_copy = nullptr;
__attribute__((naked)) void Sc9aRun15Theirs() { asm("jmp *_g_sc9a_run15_copy"); }
}

namespace scena_sc9a {
namespace {

constexpr std::uint32_t kJmpWrapper = 6;   // FF 25 disp32

void* CopyWithTramps(const char* name, std::uint32_t base, std::uint32_t size, const sh::CallSite* sites, int n,
                     const sh::JumpTable* tables, int n_tables) {
    static bof3::CloneCall calls[128];
    if (n > 128) bof3::Fatal("scena_sc9a: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) {
        const void* to = nullptr;
        for (const Tramp& t : kTramps)
            if (t.target == sites[i].target) to = t.to;
        if (!to) bof3::Fatal("scena_sc9a: %s: no trampoline for 0x%X", name, (unsigned)sites[i].target);
        calls[i] = {sites[i].offset, to, sites[i].target};
    }
    void* copy = bof3::CloneOriginal(name, base, size, calls, n);
    for (int i = 0; i < n_tables; ++i)
        move_script::Relocate(copy, base, size, {tables[i].jmp_disp, tables[i].table, tables[i].entries});
    return copy;
}

std::uint32_t Wrapper(void (*f)()) {
    const auto* p = reinterpret_cast<const unsigned char*>(f);
    if (p[0] != 0xFF || p[1] != 0x25) bof3::Fatal("scena_sc9a: the jmp wrapper at %p is not FF 25", static_cast<const void*>(p));
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p));
}

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const sh::Clone kClones[] = {
    {"Scena09_Frame", 0x553B30, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Frame), 0, false, sh::Shape::kSlot},
    {"Scena09_EnterArea", 0x553B40, 0x728, kCalls553B40, SH_N(kCalls553B40), nullptr, 0, kTables553B40, SH_N(kTables553B40), reinterpret_cast<const void*>(&::Scena09_EnterArea), 0, false, sh::Shape::kState},
    {"Scena09_Run", 0x554270, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Run), 0, false, sh::Shape::kState},
    {"Scena09_Run1", 0x554280, 0x7D, kCalls554280, SH_N(kCalls554280), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Run1), 0, false, sh::Shape::kState},
    {"Scena09_Run2", 0x554300, 0x1C8, kCalls554300, SH_N(kCalls554300), nullptr, 0, kTables554300, SH_N(kTables554300), reinterpret_cast<const void*>(&::Scena09_Run2), 0, false, sh::Shape::kState},
    {"Scena09_Run3", 0x5544D0, 0x320, kCalls5544D0, SH_N(kCalls5544D0), nullptr, 0, kTables5544D0, SH_N(kTables5544D0), reinterpret_cast<const void*>(&::Scena09_Run3), 0, false, sh::Shape::kState},
    {"Scena09_Run4", 0x5547F0, 0x477, kCalls5547F0, SH_N(kCalls5547F0), nullptr, 0, kTables5547F0, SH_N(kTables5547F0), reinterpret_cast<const void*>(&::Scena09_Run4), 0, false, sh::Shape::kState},
    {"Scena09_Run5", 0x554C70, 0xC4, kCalls554C70, SH_N(kCalls554C70), nullptr, 0, kTables554C70, SH_N(kTables554C70), reinterpret_cast<const void*>(&::Scena09_Run5), 0, false, sh::Shape::kState},
    {"Scena09_Run6", 0x554D40, 0x88A, kCalls554D40, SH_N(kCalls554D40), nullptr, 0, kTables554D40, SH_N(kTables554D40), reinterpret_cast<const void*>(&::Scena09_Run6), 0, false, sh::Shape::kState},
    {"Scena09_Run7", 0x5555D0, 0xF8, kCalls5555D0, SH_N(kCalls5555D0), nullptr, 0, kTables5555D0, SH_N(kTables5555D0), reinterpret_cast<const void*>(&::Scena09_Run7), 0, false, sh::Shape::kState},
    {"Scena09_Run8", 0x5556D0, 0x4A1, kCalls5556D0, SH_N(kCalls5556D0), nullptr, 0, kTables5556D0, SH_N(kTables5556D0), reinterpret_cast<const void*>(&::Scena09_Run8), 0, false, sh::Shape::kState},
    {"Scena09_Run9", 0x555B80, 0x2BC, kCalls555B80, SH_N(kCalls555B80), nullptr, 0, kTables555B80, SH_N(kTables555B80), reinterpret_cast<const void*>(&::Scena09_Run9), 0, false, sh::Shape::kState},
    {"Scena09_Run14", 0x555E40, 0xFC, kCalls555E40, SH_N(kCalls555E40), nullptr, 0, kTables555E40, SH_N(kTables555E40), reinterpret_cast<const void*>(&::Scena09_Run14), 0, false, sh::Shape::kState},
    {"Scena09_Run15", Wrapper(&Sc9aRun15Theirs), kJmpWrapper, nullptr, 0, nullptr, 0, nullptr, 0,   // this file's copy (above)
     reinterpret_cast<const void*>(&::Scena09_Run15), 0, false, sh::Shape::kState},
    {"Scena09_Run16", 0x556BC0, 0x4E, kCalls556BC0, SH_N(kCalls556BC0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Run16), 0, false, sh::Shape::kState},
    {"Scena09_Run10", 0x556C10, 0x2B, kCalls556C10, SH_N(kCalls556C10), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Run10), 0, false, sh::Shape::kState},
    {"Scena09_Run11", 0x556C40, 0x490, kCalls556C40, SH_N(kCalls556C40), nullptr, 0, kTables556C40, SH_N(kTables556C40), reinterpret_cast<const void*>(&::Scena09_Run11), 0, false, sh::Shape::kState},
    {"Scena09_ObjectTrigger", 0x5570D0, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_ObjectTrigger), 0, false, sh::Shape::kObject},
    {"Scena09_Object01", 0x5570F0, 0x19, kCalls5570F0, SH_N(kCalls5570F0), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Object01), 0, false, sh::Shape::kState},
    {"Scena09_Object02", 0x557110, 0x19, kCalls557110, SH_N(kCalls557110), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Object02), 0, false, sh::Shape::kState},
    {"Scena09_Object03", 0x557130, 0x14, kCalls557130, SH_N(kCalls557130), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Object03), 0, false, sh::Shape::kState},
    {"Scena09_Object04", 0x557150, 0x14, kCalls557150, SH_N(kCalls557150), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Scena09_Object04), 0, false, sh::Shape::kState},
};
enum : unsigned {
    kFrame, kEnterArea, kRun, kRun1, kRun2, kRun3, kRun4, kRun5, kRun6, kRun7, kRun8, kRun9, kRun14, kRun15, kRun16,
    kRun10, kRun11, kObjectTrigger, kObject01, kObject02, kObject03, kObject04, kCount
};
static_assert(sizeof kClones / sizeof kClones[0] == kCount, "one role per clone");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }

// Scena09_Objects' entries take arguments (the object and the flag row), which
// a DataTable's handler recorder does not log. So the seed writes a stand-in
// of the exact type into every entry, one per index (a wrong index is a
// different log), and the table is a region (the harness puts it back). Each
// logs against the dispatcher's own address, which no clone calls.
template <unsigned I> void __cdecl ObjectEntry(unsigned char* object, std::uint32_t row) {
    sh::Record(0x5570D0, I, Key(object), row);
    sh::Stir();
}
using ObjectFn = void (__cdecl*)(unsigned char*, std::uint32_t);
const ObjectFn kObjectEntries[16] = {
    &ObjectEntry<0>,  &ObjectEntry<1>,  &ObjectEntry<2>,  &ObjectEntry<3>,  &ObjectEntry<4>,  &ObjectEntry<5>,
    &ObjectEntry<6>,  &ObjectEntry<7>,  &ObjectEntry<8>,  &ObjectEntry<9>,  &ObjectEntry<10>, &ObjectEntry<11>,
    &ObjectEntry<12>, &ObjectEntry<13>, &ObjectEntry<14>, &ObjectEntry<15>,
};

// The callees the standard set lacks, or records otherwise than this block
// needs (docs/scenario_harness.md section 4; the group's listing stands).
#define SC9A_RAW(name, address) name, address, address
#define SC9A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    // tested on al alone (test al, al): garbage above a 0 must not matter
    {SC9A_OURS(Flags_Test), 2, {kAll, kU8}, sh::Answer::kFlag, 0, 0},
    // group SE's (round ten): the event battle's set-up by the id's byte
    {SC9A_OURS(Field_StartEventBattle), 1, {kU8}, sh::Answer::kGarbage, 0, 0},
    // a pass over the eight records at 0x903A70 and the party
    {SC9A_RAW("0x533E50", at::kPartyPass), 0, {}, sh::Answer::kGarbage, 0, 0},
    // the log slot of the object stand-ins above (keyed on the dispatcher's
    // own address, which no clone calls)
    {"Scena09_Objects[i]", 0x5570D0, 0x5570D0, 3, {kAll, kAll, kAll}, sh::Answer::kGarbage, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&ObjectEntry<0>)},
};
#undef SC9A_OURS
#undef SC9A_RAW

// The state and run tables take no arguments: swapped for recorders.
const sh::DataTable kTables[] = {{at::kStates, at::kStateCount}, {at::kRuns, at::kRunCount}};

// Beyond the standard regions: the selector, Cond_ByteFE, the music byte,
// Field_ScriptFlags2, Field_MenuButton, Field_Kind2Hold, run 11's slot cell,
// and the object table the seed writes stand-ins into.
const sh::Region kRegions[] = {
    {at::kSelector, 4},
    {at::kCondFE, 1},
    {at::kMusicCurrent, 1},
    {at::kScriptFlags2, 2},
    {at::kMenuButton, 2},
    {at::kHold, 1},
    {at::kSlot11, 4},
    {at::kObjects, 4 * at::kObjectCount},
};

unsigned char& B(std::uint32_t a) { return *sh::Mem(a); }
void SetW(std::uint32_t a, std::uint32_t v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(sh::Mem(a), &w, 2);
}
void SetD(std::uint32_t a, std::uint32_t v) { std::memcpy(sh::Mem(a), &v, 4); }

// The values the chapter's code compares with.
std::uint32_t AnArea() {
    return SH_PICK(0x25, 0x27, 0x29, 0x35, 0x37, 0x45, 0x31, 0x64, 0x76, 0x77, 0x2D, 0x41, 0x57, 0x10, 0x73, 0x26, 0x75);
}
std::uint32_t AnFD() { return SH_PICK(0, 1, 2, 4, 3); }
std::uint32_t ASelector() { return (7 + sh::Next() % 12) | (sh::Half() ? 0x80 : 0); }
std::uint32_t AnY() { return SH_PICK(0x1FFFFFF, 0x2000000, 0x2000001, 0x7FFFFFF, 0x8000000, 0x8000001, 0x80000000u); }
// Counter 3: one below each sweep's limit (0xD, 0x20, 0x14, 0x28, 0xA), at
// it, or an effect slot.
std::uint32_t ACount3() {
    return sh::Half() ? SH_PICK(0xB, 0xC, 0x1E, 0x1F, 0x12, 0x13, 0x26, 0x27, 8, 9, 0, 0xFF) : sh::Next() % 20;
}

unsigned RunOf(unsigned k) {
    switch (k) {
    case kRun1: return 1; case kRun2: return 2; case kRun3: return 3; case kRun4: return 4; case kRun5: return 5;
    case kRun6: return 6; case kRun7: return 7; case kRun8: return 8; case kRun9: return 9; case kRun10: return 10;
    case kRun11: return 11; case kRun14: return 14; case kRun15: return 15; case kRun16: return 16;
    default: return 0;
    }
}

// Each run's steps (the cases its switch holds), and one past.
std::uint32_t AStep(unsigned k) {
    switch (k) {
    case kRun1: return SH_PICK(0, 1, 2, 3);
    case kRun2: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 0xA, 0xB, 0xC);
    case kRun3: return sh::Next() % 0xF;
    case kRun4: return SH_PICK(0, 1, 2, 3, 5, 6, 7, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x10, 0x11, 0x12, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23);
    case kRun5: return sh::Next() % 8;
    case kRun6: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1E, 0x1F,
                               0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
                               0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69,
                               0x6A);
    case kRun7: return sh::Next() % 8;
    case kRun8: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 0xA, 0xB, 0xF, 0x10, 0x11, 0x14, 0x15, 0x16, 0x19, 0x1A, 0x1E, 0x1F, 0x20,
                               0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29);
    case kRun9: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0x14, 0x15, 0x16, 0x17, 0x1E, 0x1F, 0x20);
    case kRun10: return SH_PICK(0, 0, 1);
    case kRun11: return SH_PICK(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0x1E, 0x1F, 0x23, 0x24);
    case kRun14: return sh::Next() % 6;
    case kRun15: return SH_PICK(0, 1, 2, 0xA, 0xB, 0xC, 0xD, 0xF, 0x10, 0x11, 0x14, 0x15, 0x16, 0x17, 0x1E, 0x1F, 0x28, 0x29,
                                0x2A, 0x32, 0x33, 0x34, 0x35, 0x3C, 0x3D, 0x3E, 0x3F, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C,
                                0x4D, 0x4E, 0x4F, 0x50, 0x51, 0x52, 0x53, 0x54, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x64, 0x65,
                                0x66, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80,
                                0x82, 0x83, 0x84, 0x8C, 0x8D, 0x8E, 0x8F, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97);
    case kRun16: return SH_PICK(0, 1, 2);
    default: return sh::Next();
    }
}

// The counter-0 value each run's step waits on (read off ours), so a step's
// action is reached, not only its wait.
struct Wait { unsigned char run, step, count; };
const Wait kWaits[] = {
    {1, 2, 0x11},
    {2, 2, 0x12}, {2, 5, 0x11}, {2, 7, 0x12}, {2, 0xB, 0xB},
    {3, 4, 2}, {3, 7, 4}, {3, 8, 0x16},
    {4, 2, 1}, {4, 0xB, 7}, {4, 0xF, 9}, {4, 0x10, 0xA}, {4, 0x11, 5}, {4, 0x1F, 7}, {4, 0x20, 5}, {4, 0x22, 0x11},
    {5, 1, 9}, {5, 6, 2},
    {6, 8, 0xA}, {6, 8, 0x14}, {6, 0x15, 1}, {6, 0x16, 2}, {6, 0x17, 0xF}, {6, 0x19, 1}, {6, 0x1A, 4}, {6, 0x1F, 1},
    {6, 0x24, 2}, {6, 0x25, 9}, {6, 0x29, 1}, {6, 0x2E, 2}, {6, 0x2F, 8}, {6, 0x33, 0xA}, {6, 0x33, 0x14}, {6, 0x34, 1},
    {6, 0x39, 2}, {6, 0x3A, 7}, {6, 0x65, 0x64}, {6, 0x65, 0x65},
    {7, 6, 1},
    {8, 0x10, 1}, {8, 0x15, 1}, {8, 0x21, 3}, {8, 0x22, 6}, {8, 0x23, 7}, {8, 0x24, 0xA}, {8, 0x28, 1},
    {9, 1, 2}, {9, 2, 0x25}, {9, 3, 0x28}, {9, 4, 0x32}, {9, 6, 1}, {9, 8, 0xE}, {9, 0x15, 0xA}, {9, 0x16, 0x19},
    {11, 1, 0x25}, {11, 2, 0x26}, {11, 4, 0x28}, {11, 5, 0x29}, {11, 8, 0x32}, {11, 0xA, 0x35}, {11, 0xC, 0x37},
    {11, 0xD, 0x3B}, {11, 0xE, 0},
    {14, 1, 0xF},
    {15, 1, 5}, {15, 0xC, 0x19}, {15, 0x15, 4}, {15, 0x16, 0xF}, {15, 0x29, 7}, {15, 0x33, 5}, {15, 0x3E, 0x1E},
    {15, 0x4D, 4}, {15, 0x4F, 5}, {15, 0x50, 7}, {15, 0x51, 0x1E}, {15, 0x52, 0x1F}, {15, 0x53, 0x1F}, {15, 0x5A, 0x4E},
    {15, 0x5C, 1}, {15, 0x5E, 5}, {15, 0x72, 8}, {15, 0x7A, 0xF}, {15, 0x7C, 0xA}, {15, 0x7D, 0x14}, {15, 0x7E, 5},
    {15, 0x7F, 6}, {15, 0x83, 7}, {15, 0x8D, 1}, {15, 0x8F, 7},
    {16, 1, 3},
};

// A count paired with the step two times in three: one of the values kWaits
// lists for this run and step (the first, or another at random), or one off.
void PairCount(unsigned run, unsigned step) {
    unsigned n = 0;
    unsigned char pick[4];
    for (const Wait& w : kWaits)
        if (w.run == run && w.step == step && n < 4) pick[n++] = w.count;
    if (!n || !sh::Often()) return;
    const unsigned char c = pick[sh::Next() % n];
    B(at::kCounters) = sh::Often() ? c : static_cast<unsigned char>(c + (sh::Half() ? 1 : 0xFF));
}

unsigned g_k;

void Seed(unsigned k) {
    g_k = k;
    // The stand-ins into the object table.
    for (unsigned i = 0; i < at::kObjectCount; ++i) SetD(at::kObjects + 4 * i, KeyOf(kObjectEntries[i]));
    // The cells the chapter compares, each most of the time at a value a
    // branch tests.
    if (sh::Often()) SetW(at::kArea, AnArea());
    if (sh::Often()) B(at::kCounters + 1) = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3));
    if (sh::Often()) B(at::kCounters + 2) = static_cast<unsigned char>(SH_PICK(0, 1, 2, 3));
    B(at::kCounters + 3) = static_cast<unsigned char>(ACount3());
    if (sh::Often()) B(at::kRequest) = static_cast<unsigned char>(SH_PICK(0, 2, 6));
    if (sh::Half()) SetW(at::kWait, 0);
    if (sh::Often()) B(at::kSelector) = static_cast<unsigned char>(ASelector());
    if (sh::Often()) B(at::kCondFD) = static_cast<unsigned char>(AnFD());
    if (sh::Half()) B(at::kHold) = 0;
    if (sh::Half()) B(at::kScriptFlags2) = static_cast<unsigned char>(B(at::kScriptFlags2) & 0xF8);
    if (sh::Often()) B(at::kMember1Byte) = static_cast<unsigned char>(SH_PICK(3, 2, 0));
    if (sh::Often()) B(at::kMember2Byte) = static_cast<unsigned char>(SH_PICK(3, 2, 0));
    if (sh::Half()) SetW(at::kInputPressed, 0);
    if (sh::Half()) SetW(at::kMenuButton, sh::Half() ? 0 : 1u << (sh::Next() % 16));
    for (unsigned i = 0; i < 3; ++i)
        if (sh::Often()) B(at::kPartyList + i) = static_cast<unsigned char>(SH_PICK(7, 2, 4, 8, 5, 1));
    if (sh::Often()) SetD(at::kObjTrioY, AnY());
    if (sh::Often()) SetW(at::kTimer, SH_PICK(0, 1, 2, 0x5F));
    if (sh::Often()) B(at::kSlot11) = static_cast<unsigned char>(sh::Next() % 20);
    if (B(at::kCounters + 3) < 20 && sh::Half()) B(at::kEffects + B(at::kCounters + 3) * at::kEffectStride) = 0;
    if (B(at::kSlot11) < 20 && sh::Half())
        B(at::kEffects + B(at::kSlot11) * at::kEffectStride) = static_cast<unsigned char>(sh::Next() & 0xFE);
    switch (k) {
    case kEnterArea:
        if (sh::Often()) SetW(at::kArea, SH_PICK(0x25, 0x27, 0x29, 0x35, 0x37, 0x45, 0x31, 0x64, 0x76, 0x77));
        if (sh::Often()) B(at::kCounters + 2) = static_cast<unsigned char>(SH_PICK(1, 2));
        break;
    case kFrame: B(at::kState) = static_cast<unsigned char>(sh::Next() % 3); break;
    case kRun: B(at::kRun) = static_cast<unsigned char>(sh::Next() % 17); break;
    case kObjectTrigger: sh::SpriteRecord(0)[0x86] = static_cast<unsigned char>(sh::Next() % 16); break;
    case kObject01: case kObject02: case kObject03: case kObject04: break;
    default:
        if (RunOf(k)) {
            B(at::kStep) = static_cast<unsigned char>(AStep(k));
            PairCount(RunOf(k), B(at::kStep));
        }
        break;
    }
}

void Args(unsigned k, std::uint32_t* a) {
    if (k == kObjectTrigger) a[0] = Key(sh::SpriteRecord(0));
}

// After a call, two in three (beyond the harness's own): the counters 1..3,
// the area, the selector, an effect's in-use byte, the kind-2 hold,
// Cond_ByteFD, a bit of the script flags this block sets, run 11's slot, the
// members' bytes, the party list, Field_ScriptFlags2's low bits, the input
// and menu words, ObjTrio +0x3C. Drawn from `h` alone.
void Disturb(std::uint32_t h) {
    const unsigned char v = static_cast<unsigned char>(h >> 16);
    switch ((h >> 8) % 14) {
    case 0: B(at::kCounters + 1 + (h >> 12) % 3) = static_cast<unsigned char>((h >> 24) & 1 ? v % 20 : v); break;
    case 1: {
        static const std::uint16_t kAreas[] = {0x25, 0x27, 0x29, 0x35, 0x37, 0x45, 0x31, 0x64, 0x76, 0x77, 0x2D, 0x10};
        SetW(at::kArea, kAreas[(h >> 16) % 12]);
        break;
    }
    case 2: B(at::kSelector) = static_cast<unsigned char>(7 + (h >> 16) % 12); break;
    case 3: B(at::kEffects + ((h >> 12) % 20) * at::kEffectStride) = static_cast<unsigned char>(v & 1); break;
    case 4: B(at::kHold) = static_cast<unsigned char>(v & 1); break;
    case 5: {
        static const unsigned char kFD[] = {0, 1, 2, 4};
        B(at::kCondFD) = kFD[(h >> 16) % 4];
        break;
    }
    case 6: {
        static const unsigned char kBits[] = {8, 0x80, 0x40, 2, 4};
        B(at::kScriptFlags) = static_cast<unsigned char>(B(at::kScriptFlags) ^ kBits[(h >> 16) % 5]);
        break;
    }
    case 7: B(at::kSlot11) = static_cast<unsigned char>(v % 20); break;
    case 8: B((h >> 24) & 1 ? at::kMember1Byte : at::kMember2Byte) = static_cast<unsigned char>(2 + (v % 3)); break;
    case 9: {
        static const unsigned char kKinds[] = {7, 2, 4, 8, 5};
        B(at::kPartyList + (h >> 12) % 3) = kKinds[(h >> 16) % 5];
        break;
    }
    case 10: B(at::kScriptFlags2) = static_cast<unsigned char>(B(at::kScriptFlags2) ^ (1u << ((h >> 16) % 3))); break;
    case 11: SetW((h >> 24) & 1 ? at::kInputPressed : at::kMenuButton, (h >> 16) & 1 ? 0 : 1u << ((h >> 17) % 16)); break;
    case 12: {
        static const std::uint32_t kY[] = {0x1FFFFFF, 0x2000000, 0x2000001, 0x7FFFFFF, 0x8000000};
        SetD(at::kObjTrioY, kY[(h >> 16) % 5]);
        break;
    }
    default: B(at::kCounters + 3) = static_cast<unsigned char>(v % 20); break;
    }
}

// After every disturbance (two calls in three), for Run15 counter 2 (below),
// and for EnterArea the cells
// it reads again after its calls - the area, counter 2, Cond_ByteFD and
// ObjTrio +0x3C - each moved half the time, since the harness's disturbance
// reaches the group's cells only one time in sixteen. Noise() is the
// recorders' stream, the same on both passes.
void Settle() {
    if (g_k == kRun15) {
        // Run 15 stores counter 2 between calls (steps 0x5E, 0x6F, 0x72,
        // 0x78): moved half the time, so a store on the wrong side of a call
        // shows
        const std::uint32_t n = sh::Noise();
        if (n & 1) B(at::kCounters + 2) = static_cast<unsigned char>(n >> 8);
        return;
    }
    if (g_k != kEnterArea) return;
    const std::uint32_t n = sh::Noise();
    if (n & 1) {
        static const std::uint16_t kAreas[] = {0x25, 0x27, 0x29, 0x35, 0x37, 0x45, 0x31, 0x64, 0x76, 0x77, 0x57, 0x73};
        SetW(at::kArea, kAreas[(n >> 8) % 12]);
    }
    if (n & 2) B(at::kCounters + 2) = static_cast<unsigned char>(1 + (n >> 16) % 2);
    if (n & 4) {
        static const unsigned char kFD[] = {0, 1, 2, 4};
        B(at::kCondFD) = kFD[(n >> 20) % 4];
    }
    if (n & 8) SetD(at::kObjTrioY, (n >> 24) & 1 ? 0x2000000 : 0x2000001);
}

}  // namespace

void SelfTest() {
    g_sc9a_run15_copy = CopyWithTramps("Scena09_Run15", 0x555F40, 0xC77, kCalls555F40, SH_N(kCalls555F40), kTables555F40,
                                       SH_N(kTables555F40));
    sh::Group group = {"scena_sc9a",
                       kClones,
                       kCount,
                       kCallees,
                       sizeof kCallees / sizeof kCallees[0],
                       kTables,
                       sizeof kTables / sizeof kTables[0],
                       kRegions,
                       sizeof kRegions / sizeof kRegions[0],
                       &Seed,
                       &Disturb,
                       6000};
    group.args = &Args;
    group.settle = &Settle;
    group.chapter = 9;
    sh::Run(group);
}

}  // namespace scena_sc9a
#undef SH_N
