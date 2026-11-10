// Moved here by tools/move_funcs.py from area_cell_hook.cpp, event_ops.cpp, field_e2.cpp, field_modes.cpp, rest_2b.cpp, scena_sx.cpp, title_states.cpp (docs/source-layout-round.md).
#include "game/field/field_7.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/area_cell_hook_callees.h"
#include "game/area_harness.h"
#include "game/cheats.h"
#include "game/event_ops_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/field/field_7_callees.h"
#include "game/field_e2.h"
#include "game/field_e2_callees.h"
#include "game/field_modes_callees.h"
#include "game/lang_layout.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/rdata_consts.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2b.h"
#include "game/rest_2b_callees.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/title_states.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_7::at;

}  // namespace

using namespace event_ops;

namespace {

using namespace field_modes;

}  // namespace

namespace event_ops {

using move_script::At;

using move_script::Long;

using move_script::Word;

}  // namespace event_ops

namespace {

// The whole part of a 16.16 position, as the original pushes it: the high
// half of the dword, which the callee reads as a short.
short Whole(std::uint32_t v) { return static_cast<short>(v >> 16); }

}  // namespace

// original 0x56E050: the area's step hook - Area_ReturnGate first (1 if it
// fires), then the handler of 38 areas (docs/event-ops.md section 6), its al
// sign-extended; 0 for any other area.
// As the original has it: area 0x4C's handler 0x40EB90 is called with no
// arguments pushed (it reads none).
// DIV-0191 (switch MountainPasses, docs/mountain-passes.md section 2): on
// the two maps with a new road, a step whose target is a door cell, a Mt.
// Levett band cell or the Zublo stored exit arms that place's own link
// (Area_LinkAt of its link cell), so the plate, confirm and the stored
// exit act on the right place whichever cell the party came from. The
// original arms only on arrival at the link cell itself.
static void MountainPasses_StepArm(unsigned short area, int cx, int cz) {
    struct Arm { unsigned char x, z, lx, lz; };
    static constexpr Arm kZublo[] = {{0x37, 0x1D, 0x37, 0x1E}, {0x36, 0x1B, 0x35, 0x1B}, {0x33, 0x1A, 0x33, 0x1B}};
    static constexpr Arm kLevett[] = {{0x38, 0x38, 0x39, 0x38}, {0x37, 0x39, 0x39, 0x38}, {0x38, 0x39, 0x39, 0x38},
                                      {0x37, 0x3A, 0x36, 0x3B}, {0x36, 0x3A, 0x36, 0x3B}};
    const Arm* arms = nullptr;
    unsigned n = 0;
    if (area == 87) { arms = kZublo; n = sizeof kZublo / sizeof kZublo[0]; }
    else if (area == 16) { arms = kLevett; n = sizeof kLevett / sizeof kLevett[0]; }
    for (unsigned i = 0; i < n; ++i)
        if (arms[i].x == cx && arms[i].z == cz) { event_ops::g.link_at(arms[i].lx, arms[i].lz); return; }
}

namespace {

// move_script::SetWord: named in full below (source-layout B0, a clash on SetWord)
// move_script::Word: named in full below (source-layout B0, a clash on Word)
using U = std::uint32_t;

unsigned char& BFieldE2(U a) { return *move_script::At(a); }

U LFieldE2(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }

std::uint16_t WFieldE2(U a) { return move_script::Word(move_script::At(a)); }

void SetW(U a, unsigned v) { move_script::SetWord(move_script::At(a), v); }

using Handler = void (__cdecl*)();

// A .data table's entry read in place (swapped for recorders while the fuzz
// runs); an entry that is not code - an index past the table - aborts, where
// the original jumps there (scena_sc0.cpp's CodeAt).
U CodeAtFieldE2(U table, U index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = LFieldE2(cell);
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    (unsigned)index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

signed char TailState() { return AreaW0c_TailState; }

void TailDone() {
    AreaW0a_TailKind = 0;
    AreaW0c_TailState = 0;
    AreaW0c_TailSub = 0;
}

void TailStep(unsigned char state) { AreaW0c_TailState = state; }

}  // namespace

namespace field_modes {

using move_script::At;

using move_script::Long;

}  // namespace field_modes

namespace {

using VoidFn = void (__cdecl*)();

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
VoidFn Entry(std::uint32_t table, int index) {
    return reinterpret_cast<VoidFn>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(field_modes::Long(field_modes::At(table + 4u * static_cast<std::uint32_t>(index))))));
}

using move_script::At;

// kRank: now AreaW3c_ItemPick (symbols.toml). u8: the score's rank 0..5
constexpr ::bof3::image::Va kTriggerCounter{0x903848};   // u8: the chapters' counter the handlers wait on

unsigned char& BRest2b(U address) { return move_script::At(address)[0]; }

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

// kPartyCombo: now AreaW0b_PartySet (symbols.toml). 0xFF when the demo ends (PSX 0x80145020)
// kMusicTrack: now Music_Track (symbols.toml). the track Music_Play last started, 0xFF none
constexpr ::bof3::image::Va kScenarioBytes{0x8034E0};  // Cond_ByteFA and the nine bytes after it (PSX 0x8014686C)

const std::uint32_t kCondFlags = Low32(Cond_Flags);

}  // namespace

// original 0x56D5E0: start scenario chapter `chapter` (PSX 0x801A870C). The
// chapter byte is Cond_ByteFA (PSX 0x8014686C, the index into the scenario
// vtable table the dispatcher Field_ModeDispatch reads); the scenario's
// state bytes after it are cleared, and the chapter's own Cond_Flags dword
// zeroed and remembered at 0x929ED0. Then its DAT (0x306 + chapter), a wait
// for it - a field frame without the event script each frame, unless no
// area is loaded (Game_AreaNumber 0xFFFF) or an area change is pending
// (Field_Request 5) - and the scenario's first frame (a tail jump).
//
// As the original has it: the argument's low byte only, sign-extended for
// the Cond_Flags index, so chapters 0x80..0xFF reach below Cond_Flags; no
// bounds check. The bytes 0x8034E1..0x8034E9 are zeroed whatever the chapter
// (MoveScript_Var7 is 0x8034E4, Field_StepCountPrev 0x8034E8). File_LoadDone
// is asked before any frame; on the PC it is always done, so the loop body
// never runs.
extern "C" void __cdecl Scenario_Start(int chapter) {
    const auto c = static_cast<signed char>(chapter);
    unsigned char* const s = move_script::At(::bof3::Img(kScenarioBytes.v));
    s[0] = static_cast<unsigned char>(c);
    s[2] = 0;
    s[3] = 0;
    s[4] = 0;
    move_script::SetWord(s + 6, 0);
    s[5] = 0;
    s[1] = 0;
    move_script::SetWord(s + 8, 0);
    const std::uint32_t flags = kCondFlags + static_cast<std::uint32_t>(8 * static_cast<int>(c));
    move_script::SetLong(move_script::At(flags), 0);
    Chapter_FlagRow = static_cast<std::int32_t>(flags);
    title_states::gTitleStates.scenario_load();
    while (title_states::gTitleStates.load_done() == 0) {
        if (Game_AreaNumber != 0xFFFF && Field_Request != 5) title_states::gTitleStates.loading_frame();
        title_states::gTitleStates.sleep(1);
    }
    title_states::gTitleStates.mode_dispatch();
}

// original 0x56D670: the current chapter's DAT (PSX 0x801A880C, which loads
// 0x295 + chapter, SCENA<chapter>.EMI). As the original has it: the chapter
// byte sign-extended.
extern "C" void __cdecl Scenario_Load(void) {
    title_states::gTitleStates.load_dat(static_cast<int>(Cond_ByteFA) + 0x306);
}

// original 0x56D690 (PSX 0x801A8834): slot 0 of the current chapter's vtable, then the tail
// dispatcher (PSX thunk 0x801A8834, which calls slot 0 and 0x801A8BF8).
// As the original has it: the chapter Cond_ByteFA is signed and unchecked;
// the table is read afresh; the tail is a jump, here a call.
extern "C" void __cdecl Field_ModeDispatch(void) {
    const auto vtable = static_cast<std::uint32_t>(field_modes::Long(field_modes::At(::bof3::Img(Scenario_Hooks_addr) + 4u * static_cast<std::uint32_t>(static_cast<int>(Cond_ByteFA)))));
    Entry(vtable, 0)();
    field_modes::g.mode_tail();
}

// original 0x56D6B0 (PSX 0x801A8880): with the object's +0x89 bit 6, Field_ObjectTriggerByKind
// (object); else the chapter's vtable slot 1 (Scenario_Hooks[Cond_ByteFA]
// +4) with the object. Then +0x86 = 0xFF. Called by Field_ObjectIdle when
// +0x86 is not 0xFF, and by FE1's 0x52F8F0. PSX twin 0x801A8880 (call, a
// hypothesis).
extern "C" void __cdecl Field_ObjectTrigger(unsigned char* object) {
    if (object[0x89] & 0x40) {
        SH_CALL(Field_ObjectTriggerByKind)(object);
    } else {
        const U hooks = LFieldE2(::bof3::Img(field_e2::at::kScenarioHooks) + 4u * static_cast<U>(static_cast<std::int32_t>(Cond_ByteFA)));
        const U entry = CodeAtFieldE2(hooks, 1, "Field_ObjectTrigger");
        reinterpret_cast<void (__cdecl*)(unsigned char*)>(static_cast<std::uintptr_t>(entry))(object);
    }
    object[0x86] = 0xFF;
}

// original 0x56D6F0: Field_StatusBits |= 0x80. No PSX twin paired.
extern "C" void __cdecl Field_SetStatus80(void) { Field_StatusBits = static_cast<unsigned char>(Field_StatusBits | 0x80); }

// original 0x56D700: the step's hook (no PSX twin found) - the chapter
// record's +8 (by Cond_ByteFA, signed), and if that declines, Area_StepHook.
// Sprite_Current is put back after. The answer is the hook's al,
// sign-extended, or Area_StepHook's.
extern "C" int __cdecl Scenario_StepHook(long x, long z) {
    const ChapterHooks* const record = event_ops::g.chapter_hooks[Cond_ByteFA];
    unsigned char* const saved = Sprite_Current;
    int r = static_cast<signed char>(reinterpret_cast<Hook>(static_cast<std::uintptr_t>(record->step))(x, z));
    if (r == 0) r = event_ops::g.area_step(x, z);
    Sprite_Current = saved;
    return r;
}

// original 0x56D750: the same with the record's +0xC and Area_ArriveHook -
// called when a walk lands (0x52E580's path; nothing in this file).
extern "C" int __cdecl Scenario_ArriveHook(long x, long z) {
    const ChapterHooks* const record = event_ops::g.chapter_hooks[Cond_ByteFA];
    unsigned char* const saved = Sprite_Current;
    int r = static_cast<signed char>(reinterpret_cast<Hook>(static_cast<std::uintptr_t>(record->arrive))(x, z));
    if (r == 0) r = event_ops::g.area_arrive(x, z);
    Sprite_Current = saved;
    return r;
}

// original 0x56D7A0 (PSX 0x801A8A3C): the cell a leader faces, (x, z): the chapter's vtable
// slot 4 (Scenario_Hooks[Cond_ByteFA] +0x10), when it is not 0, with (x, z);
// its al not negative (movsx) answers 1. Else Area_CellHook(x, z), whose eax
// it answers. Sprite_Current, read on entry, is put back on both paths.
// Called by Field_LeaderTalkTest. PSX twin 0x801A8A3C (call-anchored, a
// hypothesis).
extern "C" int __cdecl Scenario_CellHook(unsigned x, unsigned z) {
    unsigned char* const kept = Sprite_Current;
    const U hooks = LFieldE2(::bof3::Img(field_e2::at::kScenarioHooks) + 4u * static_cast<U>(static_cast<std::int32_t>(Cond_ByteFA)));
    const U hook = LFieldE2(hooks + 0x10);
    if (hook != 0) {
        const U entry = CodeAtFieldE2(hooks, 4, "Scenario_CellHook");
        const unsigned char answer = reinterpret_cast<unsigned char (__cdecl*)(unsigned, unsigned)>(static_cast<std::uintptr_t>(entry))(x, z);
        if (static_cast<signed char>(answer) >= 0) {
            Sprite_Current = kept;
            return 1;
        }
    }
    const int answer = SH_CALL(Area_CellHook)(x, z);
    Sprite_Current = kept;
    return answer;
}

// original 0x56D800: which of `count` (a byte) 5-byte cell records at `table`
// the cell (x, z) (bytes) stands in, facing right: +0 the area (a byte, to
// Game_AreaNumber), +1 x, +2 z, +3 bit 7 a run along z (else along x) and the
// facing in its low nibble (8: any; else ObjTrio's first record +8 must
// match), +4 the run's length (0: none). The first record matching answers
// its index; one whose cell matches but whose facing does not is passed over
// (the next record, not the next cell); none, 0xFF. The run's cells are
// tested from its far end; the sums are not wrapped at a byte. The chapters'
// cell hooks call it with their Scena<NN>_CellRecords. No PSX twin paired.
extern "C" std::uint32_t __cdecl Field_CellTriggerAt(unsigned char* table, unsigned count_arg, unsigned x_arg,
                                                      unsigned z_arg) {
    const unsigned count = count_arg & 0xFF;
    const unsigned x = x_arg & 0xFF, z = z_arg & 0xFF;
    for (unsigned i = 0; i < count; ++i) {
        const unsigned char* const r = table + 5 * i;
        bool hit = false;
        for (int k = static_cast<int>(r[4]) - 1; k >= 0; --k) {
            if (r[0] != Game_AreaNumber) break;
            if (r[3] & 0x80) {
                if (x == r[1] && z == static_cast<unsigned>(r[2] + k)) hit = true;
            } else {
                if (x == static_cast<unsigned>(r[1] + k) && z == r[2]) hit = true;
            }
            if (hit) break;
        }
        if (!hit) continue;
        const unsigned facing = r[3] & 0xF;
        if (facing == 8 || facing == ObjTrio[0].direction) return i;
    }
    return 0xFF;
}

// original 0x56D8B0: a tail jump through the two-entry table 0x662CD0 on the
// s8 at 0x9039F2 (PSX 0x801A8BF8 on 0x801448E6): 0 is the set-up 0x56D8C0,
// which sets it to 1; 1 is Field_ModeTailRun. Unchecked.
extern "C" void __cdecl Field_ModeTail(void) { Entry(::bof3::Img(kTailTable), SaveMenu_NewGameByte)(); }

// original 0x56D8C0: state 0 of the dispatcher 0x56D8B0 (PSX 0x801A8C34):
// its state byte to 1 so the next frame runs entry 1 (0x56D920), the six
// bytes and two words around it zeroed, and 16 bytes of the story flags
// (Cond_Flags + 0xA0) seeded from the table at 0x662CD8.
//
// As the original has it: four dword copies (the PSX copies bytewise); the
// table and the flags do not overlap, so the order is unobservable.
extern "C" void __cdecl Scenario_SubInit(void) {
    unsigned char* const s = move_script::At(::bof3::Img(ScenaSc7_DrawPool_addr));
    s[2] = 1;
    s[0] = 0;
    s[3] = 0;
    s[4] = 0;
    move_script::SetWord(s + 6, 0);
    s[5] = 0;
    s[1] = 0;
    move_script::SetWord(s + 8, 0);
    for (unsigned i = 0; i < 16; i += 4) move_script::SetLong(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0 + i), move_script::Long(move_script::At(::bof3::Img(Scenario_SubInitTable_addr) + i)));
}

// original 0x56D920: a tail jump through 0x662CE8 on the s8 at 0x9039F3 (PSX
// 0x801A8CB4 on 0x801448E7); entry 0 is a bare ret. Unchecked.
extern "C" void __cdecl Field_ModeTailRun(void) { Entry(::bof3::Img(Field_ModeTailKinds_addr), AreaW0a_TailKind)(); }

// original 0x56D930 (slot 4): 0 - Field_ScriptFlags2 |= 7, LoadDatFile(0x31C),
// the state up; 1 - once File_LoadDone, Gfx_ClutStripCopyRow(2), 0x585A00,
// the state up; 2 - a tail jump to 0x586670 (a dispatch on 0x9398CF); 3 -
// the party set 0x90412C |= 0x80 and Snd_LoadBankFile(0x2C2 + its low seven
// bits), the state up; 4 - once File_LoadDone, Field_ScriptFlags2 &= ~7,
// ScriptFlags_Clear40, the tail cleared; 5 - with Cond_ByteFE at 2, state 0.
extern "C" void __cdecl FieldTail_LoadBank(void) {
    switch (TailState()) {
    case 0:
        BFieldE2(::bof3::Img(field_e2::at::kScriptFlags2)) = static_cast<unsigned char>(BFieldE2(::bof3::Img(field_e2::at::kScriptFlags2)) | 7);
        SH_CALL(LoadDatFile)(0x31C);
        AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
        return;
    case 1:
        if (SH_CALL(File_LoadDone)() == 0) return;
        SH_CALL(Gfx_ClutStripCopyRow)(2);
        SH_AT(void (__cdecl*)(), field_e2::at::kLoadWait.v)();
        AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
        return;
    case 2:
        SH_AT(void (__cdecl*)(), field_e2::at::kLoadStep.v)();
        return;
    case 3: {
        const auto set = static_cast<unsigned char>(AreaW0b_PartySet | 0x80);
        AreaW0b_PartySet = set;
        SH_CALL(Snd_LoadBankFile)((set & 0x7Fu) + 0x2C2u);
        AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
        return;
    }
    case 4:
        if (SH_CALL(File_LoadDone)() == 0) return;
        Field_ScriptFlags2 = Field_ScriptFlags2 & 0xFFF8u;
        SH_CALL(ScriptFlags_Clear40)();
        TailDone();
        return;
    case 5:
        if (Cond_ByteFE == 2) AreaW0c_TailState = 0;
        return;
    default: return;
    }
}

// original 0x56DA10 (slot 5): 0 - unless a message is up (Field_Request 2),
// ScriptFlags_Set40 and state 1; 1 - Party_DropIn(0x662DE8[argument]), state
// 2; 2 - once the counter byte 0x90384B is 0x20, the timer 0x9039F6 = 0x10
// and state 3; 3 - once the timer counts down to 0, Field_ChangeArea by the
// argument (0 and 4: area 0x6C at (0x11, 0x59) facing 0x85; 1 and 2: 0x22 at
// (0x34, 0x43), 0x81; 3 and 5: 4 at (0x3E, 5), 0x81; above 5 none) and state
// 4; 4 - once the counter byte is 0x31, it 0, ScriptFlags_Clear40, story
// flags 0x1E and 0x1F cleared, the tail cleared.
extern "C" void __cdecl FieldTail_DropInMove(void) {
    switch (TailState()) {
    case 0:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Set40)();
        TailStep(1);
        return;
    case 1:
        SH_CALL(Party_DropIn)(BFieldE2(::bof3::Img(field_e2::at::kDropInTable) + AreaW0c_TailSub));
        TailStep(2);
        return;
    case 2:
        if (BFieldE2(::bof3::Img(field_e2::at::kCounterB)) != 0x20) return;
        SetW(::bof3::Img(field_e2::at::kTailTimer), 0x10);
        TailStep(3);
        return;
    case 3: {
        const auto left = static_cast<std::uint16_t>(WFieldE2(::bof3::Img(field_e2::at::kTailTimer)) - 1u);
        SetW(::bof3::Img(field_e2::at::kTailTimer), left);
        if (left != 0) return;
        switch (AreaW0c_TailSub) {
        case 0:
        case 4: SH_CALL(Field_ChangeArea)(0x6C, 0x110000, 0x590000, 0x85); break;
        case 1:
        case 2: SH_CALL(Field_ChangeArea)(0x22, 0x340000, 0x430000, 0x81); break;
        case 3:
        case 5: SH_CALL(Field_ChangeArea)(4, 0x3E0000, 0x50000, 0x81); break;
        default: break;
        }
        TailStep(4);
        return;
    }
    case 4:
        if (BFieldE2(::bof3::Img(field_e2::at::kCounterB)) != 0x31) return;
        BFieldE2(::bof3::Img(field_e2::at::kCounterB)) = 0;
        SH_CALL(ScriptFlags_Clear40)();
        SH_CALL(Flags_Clear)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1E);
        SH_CALL(Flags_Clear)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1F);
        TailDone();
        return;
    default: return;
    }
}

// original 0x56DB80 (slot 10): two runs through a byte table, states 0..3 and
// 20..23 the same four steps - ScriptFlags_Set40 and Transition_Start(0);
// once Transition_Busy is 0, Draw_PassFlags 0, Party_HealJoined,
// Sound_StopMusic, Sound_LoadStream(0); once Sound_StreamDone,
// Sound_ResumeAll, Transition_Start(1), Draw_PassFlags 0x1F; once the wait
// word is 0 - then state 10 for the first, and for the second the menu set
// up (0x929F00 = 0, 0x929F0C = 0xFE, 0x929EC2 = 1, 0x929EC3 = 0, Game_Mode 7)
// and state 6. State 5: unless a message is up, ScriptFlags_Set40 and state
// 6; 6: the same menu set-up and state 7; 7: once Game_Mode is 2, state 10;
// 10: the argument not 0xFF, Msg_OpenScript(it) and Field_Request 2 - state
// 11; 11: once no message is up, ScriptFlags_Clear40, the tail cleared.
// States 4, 8, 9, 12..19 do nothing.
extern "C" void __cdecl FieldTail_HealAndMenu(void) {
    static const unsigned char kCase[24] = {0, 1, 2, 3, 13, 4, 5, 6, 13, 13, 7, 8, 13, 13, 13, 13, 13, 13, 13, 13, 9, 10, 11, 12};
    const signed char state = TailState();
    if (static_cast<U>(static_cast<std::int32_t>(state)) > 0x17) return;
    auto rest = [](unsigned char next) {
        if (Transition_Busy != 0) return;
        Draw_PassFlags = 0;
        SH_CALL(Party_HealJoined)();
        SH_CALL(Sound_StopMusic)();
        SH_CALL(Sound_LoadStream)(0);
        TailStep(next);
    };
    auto resume = [](unsigned char next) {
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        SH_CALL(Sound_ResumeAll)();
        SH_CALL(Transition_Start)(1);
        Draw_PassFlags = 0x1F;
        TailStep(next);
    };
    switch (kCase[state]) {
    case 0:
    case 9:
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(Transition_Start)(0);
        TailStep(kCase[state] == 0 ? 1 : 0x15);
        return;
    case 1: rest(2); return;
    case 10: rest(0x16); return;
    case 2: resume(3); return;
    case 11: resume(0x17); return;
    case 3:
        if (Transition_Busy != 0) return;
        TailStep(0xA);
        return;
    case 4:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Set40)();
        TailStep(6);
        return;
    case 5:
        BattleE1_Mode = 0;
        ScenaSc1_ShopByteB = 0;
        ModeStates_ShopObject = 0xFE;
        BFieldE2(::bof3::Img(field_e2::at::kFieldEC2)) = 1;
        Game_Mode = 7;
        TailStep(7);
        return;
    case 6:
        if (Game_Mode != 2) return;
        TailStep(0xA);
        return;
    case 7: {
        const unsigned char message = AreaW0c_TailSub;
        if (message != 0xFF) {
            SH_CALL(Msg_OpenScript)(message);
            Field_Request = 2;
        }
        TailStep(0xB);
        return;
    }
    case 8:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Clear40)();
        TailDone();
        return;
    case 12:
        if (Transition_Busy != 0) return;
        BattleE1_Mode = 0;
        ModeStates_ShopObject = 0xFE;
        BFieldE2(::bof3::Img(field_e2::at::kFieldEC2)) = 1;
        ScenaSc1_ShopByteB = 0;
        Game_Mode = 7;
        TailStep(6);
        return;
    default: return;
    }
}

// original 0x56DDD0 (slot 6): 0 - ScriptFlags_Set40, Msg_OpenScript(0xD8),
// Field_Request 2 and the state up; 1 - once no message is up,
// ScriptFlags_Clear40 and the tail cleared.
extern "C" void __cdecl FieldTail_Message(void) {
    switch (TailState()) {
    case 0: {
        SH_CALL(ScriptFlags_Set40)();
        SH_CALL(Msg_OpenScript)(0xD8);
        const auto next = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
        Field_Request = 2;
        AreaW0c_TailState = next;
        return;
    }
    case 1:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Clear40)();
        TailDone();
        return;
    default: return;
    }
}

// original 0x56DE30 (slot 27): a tail jump through WorldMap_FieldHooks by
// WorldMap_RecordIndex's low byte (0..11; past the twelve ours aborts where
// the original jumps into Field_ObjectTriggers).
extern "C" void __cdecl FieldTail_WorldMapHook(void) {
    const U index = SH_CALL(WorldMap_RecordIndex)() & 0xFF;
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAtFieldE2(::bof3::Img(WorldMap_FieldHooks_addr), index, "FieldTail_WorldMapHook")))();
}

// original 0x56DE50 (slot 44): 0 - unless a message is up: bit (argument) of
// 0x904650 set, the system message 0x4172 + argument copied (8 bytes, the
// CRT's copy) into Text_Records, Msg_OpenSystem(0xFA), Field_Request 2,
// Sound_StopMusic, Sound_LoadStream(9), state 1; 1 - once Sound_StreamDone
// and no message is up, Sound_ResumeAll, the byte 0x903804 points at 0,
// ScriptFlags_Clear40, the tail cleared. Bit (argument) of 0x904650 is the
// dragon gene's (Gene_Bits) and message 0xFA names it.
//
// DIVERGENCE DIV-0166 (known-defects D351): the copy (`push 8` ... `call
// 0x5B9450` at 0x56DED1..0x56DEE2) is strncpy, so a name of 8 bytes or more
// leaves Text_Records' first record unterminated and the message prints on
// into whatever an earlier copy left at +8. The US disc (GAME.EMI#0
// 0x801A83DC; a `sb $zero` to the buffer + 8 after the copy) terminates it;
// the JP (0x801A93E4) does not. With g_name_nul (1 through
// PatchBytes("GeneNameNul") in FieldE2_Inject, after the self-test) the byte
// at +8 is 0 after the copy, as on the US disc.
namespace field_e2 { std::uint8_t g_name_nul = 0; }

extern "C" void __cdecl FieldTail_FlagMessage(void) {
    switch (TailState()) {
    case 0: {
        if (Field_Request == 2) return;
        SH_CALL(Flags_Set)(move_script::At(::bof3::Img(Gene_Bits_addr)), AreaW0c_TailSub);
        const unsigned char* const text = SH_CALL(Msg_SystemPtr)(AreaW0c_TailSub + 0x4172u);
        char* const name = reinterpret_cast<char*>(move_script::At(::bof3::Img(field_e2::at::kTextRecords)));
        SH_CALL(Crt_strncpy)(name, reinterpret_cast<const char*>(text), 8);
        if (field_e2::g_name_nul) name[8] = 0;
        SH_CALL(Msg_OpenSystem)(0xFA);
        Field_Request = 2;
        SH_CALL(Sound_StopMusic)();
        SH_CALL(Sound_LoadStream)(9);
        TailStep(1);
        return;
    }
    case 1:
        if (SH_CALL(Sound_StreamDone)() == 0) return;
        if (Field_Request == 2) return;
        SH_CALL(Sound_ResumeAll)();
        *move_script::At(AreaW1a_FocusObject) = 0;
        SH_CALL(ScriptFlags_Clear40)();
        TailDone();
        return;
    default: return;
    }
}

// original 0x56DF10 (slot 55): 0 - ScriptFlags_Set40, Msg_OpenScript(0x21)
// when Inventory_Count(0, 0x57, 0) is not 0, else (0x20); Field_Request 2,
// the state up; 1 - once no message is up, ScriptFlags_Clear40, the kind and
// state 0 (the argument kept); 2 - once no message is up: the highest set
// story flag n of 0xFF..0xF6 picks the area 0xB9 - (0xFF - n) (0xAF with
// none: the word of a dword 0xB9 + k * 0xFFFF); the leader's position and
// Game_AreaNumber kept at 0x904148..0x904150; Field_ChangeArea(area,
// 0x430000, 0x160000, 4), Field_ScriptFlags2 |= 0x40, ScriptFlags_Clear40,
// the kind and state 0.
extern "C" void __cdecl FieldTail_StoryWarp(void) {
    switch (TailState()) {
    case 0: {
        SH_CALL(ScriptFlags_Set40)();
        const bool has = static_cast<std::uint16_t>(SH_CALL(Inventory_Count)(0, 0x57, 0)) != 0;
        SH_CALL(Msg_OpenScript)(has ? 0x21 : 0x20);
        const auto next = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
        Field_Request = 2;
        AreaW0c_TailState = next;
        return;
    }
    case 1:
        if (Field_Request == 2) return;
        SH_CALL(ScriptFlags_Clear40)();
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
        return;
    case 2: {
        if (Field_Request == 2) return;
        U flag = 0xFF, area = 0xB9;
        for (;;) {
            if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), flag) != 0) break;
            --flag;
            area += 0xFFFF;
            if (static_cast<std::int32_t>(flag) < 0xF6) break;
        }
        AreaW2d_ReturnPoint = static_cast<U>(ObjTrio[0].x);
        AreaW4d_ReturnZ = static_cast<U>(ObjTrio[0].z);
        AreaW4d_ReturnArea = Game_AreaNumber;
        SH_CALL(Field_ChangeArea)(area, 0x430000, 0x160000, 4);
        BFieldE2(::bof3::Img(field_e2::at::kScriptFlags2)) = static_cast<unsigned char>(BFieldE2(::bof3::Img(field_e2::at::kScriptFlags2)) | 0x40);
        SH_CALL(ScriptFlags_Clear40)();
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
        return;
    }
    default: return;
    }
}

// original 0x56E020 (PSX 0x801A96D4): Field_ObjectTriggers' entry for the object's +0x86 (the
// table read from 0x662E1C: entry n is Field_ObjectTriggers[n - 1], entry 0
// WorldMap_FieldHooks' last), called with (object, the story flags
// 0x904030). Past the table ours aborts where the original calls data.
// PSX twin 0x801A96D4 (call, a hypothesis).
extern "C" void __cdecl Field_ObjectTriggerByKind(unsigned char* object) {
    const U entry = CodeAtFieldE2(::bof3::Img(WorldMap_FieldHooks_addr) + 0x2c, object[0x86], "Field_ObjectTriggerByKind");
    reinterpret_cast<void (__cdecl*)(unsigned char*, unsigned char*)>(static_cast<std::uintptr_t>(entry))(object, move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0));
}

// original 0x56E040: Field_ObjectTriggers[1] (0x662E20) - the chapters'
// counter 0x903848 = 0xF0; al 0 (the rest of eax the caller's). Its two
// arguments (the object, 0x904030) are not read.
extern "C" unsigned char __cdecl Field_TriggerCounterF0(unsigned char* object, unsigned char* flags) {
    (void)object;
    (void)flags;
    BRest2b(::bof3::Img(kTriggerCounter.v)) = 0xF0;
    return 0;
}

extern "C" int __cdecl Area_StepHook(long x, long z) {
    if (event_ops::g.return_gate(x, z)) return 1;
    if (g_mountain_passes) MountainPasses_StepArm(Game_AreaNumber, Whole(static_cast<std::uint32_t>(x)), Whole(static_cast<std::uint32_t>(z)));
    static const unsigned short kAreas[kStepCases] = {
        0x24, 0x2A, 0x2E, 0x31, 0x3B, 0x4B, 0x4C, 0x61, 0x64, 0x69, 0x6A, 0x70, 0x74, 0x87, 0x8C, 0x8F, 0x91, 0x92, 0x96,
        0xAA, 0xAB, 0xAC, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBF, 0xC0, 0xC1, 0xC5,
    };
    const unsigned short area = Game_AreaNumber;
    for (unsigned c = 0; c < kStepCases; ++c) {
        if (kAreas[c] != area) continue;
        if (c == kNoArgsCase) return static_cast<signed char>(reinterpret_cast<unsigned char (__cdecl*)()>(reinterpret_cast<void*>(event_ops::g.area_step_handlers[c]))());
        return static_cast<signed char>(event_ops::g.area_step_handlers[c](x, z));
    }
    return 0;
}

// original 0x56E440: the return gate (PSX FUN_801A9A98, test for test) - outside area 0xBB,
// with Field_InputFlags exactly 0x40 and Cond_ByteFD 1, a step onto a 0xA1
// cell at z 0x10.8 or more while story flag 0x77 is set: the flag cleared,
// the return point's counter byte 0, Field_ChangeArea to the return point
// 0x904148 with 4.
extern "C" unsigned char __cdecl Area_ReturnGate(long x, long z) {
    if (Game_AreaNumber == 0xBB) return 0;
    if (Field_InputFlags != 0x40) return 0;
    if (Cond_ByteFD != 1) return 0;
    const auto row = static_cast<std::int32_t>(static_cast<std::uint32_t>(z) + 0x8000u) >> 16;
    if (event_ops::g.byte_at(Whole(static_cast<std::uint32_t>(x)), static_cast<short>(row)) != 0xA1) return 0;
    if (z < 0x108000) return 0;
    if (!event_ops::g.flags_test(event_ops::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77)) return 0;
    event_ops::At(::bof3::Img(AreaW2d_ReturnPoint_addr))[0xA] = 0;
    event_ops::g.flags_clear(event_ops::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77);
    const unsigned char* const back = event_ops::At(::bof3::Img(AreaW2d_ReturnPoint_addr));
    event_ops::g.change_area(event_ops::Word(back + 8), event_ops::Long(back), event_ops::Long(back + 4), 4);
    return 1;
}

// original 0x56E4E0: the area's arrive hook - the handler of 8 areas, its al
// sign-extended; 0 for any other area.
extern "C" int __cdecl Area_ArriveHook(long x, long z) {
    static const unsigned short kAreas[kArriveCases] = {0x28, 0x30, 0x6F, 0x94, 0xA7, 0xA9, 0xAB, 0xAD};
    const unsigned short area = Game_AreaNumber;
    for (unsigned c = 0; c < kArriveCases; ++c)
        if (kAreas[c] == area) return static_cast<signed char>(event_ops::g.area_arrive_handlers[c](x, z));
    return 0;
}

// original 0x56E670 (called only by 0x56D7A0, when the chapter's +0x10 is
// null or answers below 0): the first pair whose area byte - the low byte of
// its dword, zero-extended - equals the whole u16 Game_AreaNumber (so an area
// of 0x100 or more matches none); none, 0; else that pair's handler (x, z),
// its al sign-extended. The table is read in place, as the original's, so
// the fuzz's recorders stand in for its handlers.
extern "C" int __cdecl Area_CellHook(unsigned x, unsigned z) {
    namespace at = area_cell_hook::at;
    const unsigned area = Game_AreaNumber;
    unsigned i = 0;
    while (i < at::kPairCount && area_harness::Mem(::bof3::Img(Area_CellHooks_addr) + at::kPairStride * i)[0] != area) ++i;
    if (i == at::kPairCount) return 0;
    const auto handler = reinterpret_cast<area_harness::Hook>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(move_script::Long(area_harness::Mem(::bof3::Img(Area_CellHooks_addr) + at::kPairStride * i + 4)))));
    return static_cast<signed char>(handler(x, z));
}

void Field7_Inject() {
    BOF3_INJECT(Scenario_Start);
    BOF3_INJECT(Scenario_Load);
    BOF3_INJECT(Field_ModeDispatch);
    BOF3_INJECT(Field_ObjectTrigger);
    BOF3_INJECT(Field_SetStatus80);
    BOF3_INJECT(Scenario_StepHook);
    BOF3_INJECT(Scenario_ArriveHook);
    BOF3_INJECT(Scenario_CellHook);
    BOF3_INJECT(Field_CellTriggerAt);
    BOF3_INJECT(Field_ModeTail);
    BOF3_INJECT(Scenario_SubInit);
    BOF3_INJECT(Field_ModeTailRun);
    BOF3_INJECT(FieldTail_LoadBank);
    BOF3_INJECT(FieldTail_DropInMove);
    BOF3_INJECT(FieldTail_HealAndMenu);
    BOF3_INJECT(FieldTail_Message);
    BOF3_INJECT(FieldTail_WorldMapHook);
    BOF3_INJECT(FieldTail_FlagMessage);
    BOF3_INJECT(FieldTail_StoryWarp);
    BOF3_INJECT(Field_ObjectTriggerByKind);
    BOF3_INJECT(Field_TriggerCounterF0);
    BOF3_INJECT(Area_StepHook);
    BOF3_INJECT(Area_ReturnGate);
    BOF3_INJECT(Area_ArriveHook);
    BOF3_INJECT(Area_CellHook);
}

void Field7FromFieldE2_Inject() {
    {
        // DIV-0166: after the self-test, which compared Capcom's unterminated copy.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("GeneNameNul", Low32(&field_e2::g_name_nul), &was, &is, 1);
        bof3::Log("DIV-0166    message 0xFA's gene name is terminated after its 8-byte copy (on unless the line above says OFF)");
    }
}
