// Round twelve group FE2 (docs/field_e2.md): the field engine's resident code
// in 0x5341C0..0x5372D8, 0x56D240..0x5729F8 and 0x593960..0x594060 that no
// earlier group took - 51 functions:
//
//   the event script's rest   Scenario_CallB, Field_AfterBattleTally and its
//                             two record counts, Field_FaceMode11Object,
//                             Field_FloorHurt, the wide cell tests,
//                             Field_ObjectTrigger and the two hooks under it
//   the leader's hop helpers  Leader_* (called by FC3's and FE1's states)
//   the mode-11 object        Mode11_Object* (the object at 0x905DA0 that
//                             Game_Mode 11 moves) and its draw
//   the field tail            FieldTail_* (seven Field_ModeTailKinds slots)
//   draw layers               three MapCell_Handlers kinds, AreaMap_ClearCell
//   the trade screen          ItemTrade_* (0x66A47C / 0x66A484's states)
//
// Every call goes through the scenario harness (SH_CALL / SH_AT, a table's
// word read in place), so the start-up fuzz (field_e2_fuzz.cpp) stands
// recorders in for ours as for the originals' copies. Faithful: the one
// difference is DIV-0023's, extended - the fourth word of the SVECTORs the
// draw functions build on their stacks is 0 here where the originals leave
// stale stack (docs/DIVERGENCE.md DIV-0023). Where an original indexes past
// its table, divides by 0 or walks a record list past its end, ours aborts
// with a message (docs/field_e2.md section 6).
#include "game/field_e2.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/field_e2_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_e2::at;
// move_script::At: named in full below (source-layout B0, a clash on At)
// move_script::Long: named in full below (source-layout B0, a clash on Long)
using move_script::SetLong;
// move_script::SetWord: named in full below (source-layout B0, a clash on SetWord)
// move_script::Word: named in full below (source-layout B0, a clash on Word)
using U = std::uint32_t;

unsigned char& BFieldE2(U a) { return *move_script::At(a); }
U LFieldE2(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }
void SetL(U a, U v) { move_script::SetLong(move_script::At(a), static_cast<std::int32_t>(v)); }
std::uint16_t WFieldE2(U a) { return move_script::Word(move_script::At(a)); }
void SetW(U a, unsigned v) { move_script::SetWord(move_script::At(a), v); }
U Key(const void* p) { return Low32(p); }

// The record views (game/record_views.h): the leader helpers run on the leader,
// an ObjTrio record, and Field_State is one.
U LongAt(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }
int S8FieldE2(unsigned v) { return static_cast<signed char>(v); }
int S16FieldE2(unsigned v) { return static_cast<short>(v); }

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

}  // namespace

// ============================================================================
// The event script's rest
// ============================================================================

// The map's cell byte at (x, z), whole words (AreaMap_ByteAt reads them as
// s16s), each and 0xF0 when `mask` (a byte) is set.
// ============================================================================
// The leader's hop and step helpers (FC3's leader states 0x525CE0..0x525F90
// and FE1's 0x52F970..0x52FB50 call them; PSX twins 0x801C5D14..0x801C66DC,
// pairs_propagated "call", each a hypothesis). Each works on Sprite_Current
// and reads it again after every call, as the originals do.
// ============================================================================

// The hop's set-up, 0x535FE0 and 0x5363C0 alike: Field_State +0x128 = +0x70
// + 2, Field_JumpSetUp; +0x14 = 0; +9 = (+0x70 + 1) * +9 * 2 and +0xA = +9 / 2
// (bytes); Sprite_EnsureAnimation(+8 + 8), Sprite_ApplyVelocity.
// ============================================================================
// The party-set error screen
// ============================================================================

// ============================================================================
// The mode-11 object (0x905DA0; Game_Mode 11, which Field_Request 9 enters,
// runs Mode11_ObjectFrame then 0x5172C0 each frame: docs/mode-tasks.md)
// ============================================================================

// ============================================================================
// The object triggers and the cell hook
// ============================================================================

// original 0x56E020 (PSX 0x801A96D4): Field_ObjectTriggers' entry for the object's +0x86 (the
// table read from 0x662E1C: entry n is Field_ObjectTriggers[n - 1], entry 0
// WorldMap_FieldHooks' last), called with (object, the story flags
// 0x904030). Past the table ours aborts where the original calls data.
// PSX twin 0x801A96D4 (call, a hypothesis).
extern "C" void __cdecl Field_ObjectTriggerByKind(unsigned char* object) {
    const U entry = CodeAtFieldE2(::bof3::Img(WorldMap_FieldHooks_addr) + 0x2c, object[0x86], "Field_ObjectTriggerByKind");
    reinterpret_cast<void (__cdecl*)(unsigned char*, unsigned char*)>(static_cast<std::uintptr_t>(entry))(object, move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0));
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
        const U hooks = LFieldE2(::bof3::Img(at::kScenarioHooks) + 4u * static_cast<U>(static_cast<std::int32_t>(Cond_ByteFA)));
        const U entry = CodeAtFieldE2(hooks, 1, "Field_ObjectTrigger");
        reinterpret_cast<void (__cdecl*)(unsigned char*)>(static_cast<std::uintptr_t>(entry))(object);
    }
    object[0x86] = 0xFF;
}

// original 0x56D7A0 (PSX 0x801A8A3C): the cell a leader faces, (x, z): the chapter's vtable
// slot 4 (Scenario_Hooks[Cond_ByteFA] +0x10), when it is not 0, with (x, z);
// its al not negative (movsx) answers 1. Else Area_CellHook(x, z), whose eax
// it answers. Sprite_Current, read on entry, is put back on both paths.
// Called by Field_LeaderTalkTest. PSX twin 0x801A8A3C (call-anchored, a
// hypothesis).
extern "C" int __cdecl Scenario_CellHook(unsigned x, unsigned z) {
    unsigned char* const kept = Sprite_Current;
    const U hooks = LFieldE2(::bof3::Img(at::kScenarioHooks) + 4u * static_cast<U>(static_cast<std::int32_t>(Cond_ByteFA)));
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

// ============================================================================
// The field tail: seven of Field_ModeTailKinds' slots (Field_ModeTailRun
// jumps through them by the s8 0x9039F3 each field frame). Each is a state
// machine on the s8 0x9039F4 with an argument byte 0x9039F5; the area code
// arms them (kind, state 0, argument). A state out of range does nothing.
// No PSX twin paired for any.
// ============================================================================

namespace {
signed char TailState() { return AreaW0c_TailState; }
void TailDone() {
    AreaW0a_TailKind = 0;
    AreaW0c_TailState = 0;
    AreaW0c_TailSub = 0;
}
void TailStep(unsigned char state) { AreaW0c_TailState = state; }
}  // namespace

// original 0x56D930 (slot 4): 0 - Field_ScriptFlags2 |= 7, LoadDatFile(0x31C),
// the state up; 1 - once File_LoadDone, Gfx_ClutStripCopyRow(2), 0x585A00,
// the state up; 2 - a tail jump to 0x586670 (a dispatch on 0x9398CF); 3 -
// the party set 0x90412C |= 0x80 and Snd_LoadBankFile(0x2C2 + its low seven
// bits), the state up; 4 - once File_LoadDone, Field_ScriptFlags2 &= ~7,
// ScriptFlags_Clear40, the tail cleared; 5 - with Cond_ByteFE at 2, state 0.
extern "C" void __cdecl FieldTail_LoadBank(void) {
    switch (TailState()) {
    case 0:
        BFieldE2(::bof3::Img(at::kScriptFlags2)) = static_cast<unsigned char>(BFieldE2(::bof3::Img(at::kScriptFlags2)) | 7);
        SH_CALL(LoadDatFile)(0x31C);
        AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
        return;
    case 1:
        if (SH_CALL(File_LoadDone)() == 0) return;
        SH_CALL(Gfx_ClutStripCopyRow)(2);
        SH_AT(void (__cdecl*)(), at::kLoadWait.v)();
        AreaW0c_TailState = static_cast<unsigned char>(static_cast<unsigned char>(AreaW0c_TailState) + 1);
        return;
    case 2:
        SH_AT(void (__cdecl*)(), at::kLoadStep.v)();
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
        SH_CALL(Party_DropIn)(BFieldE2(::bof3::Img(at::kDropInTable) + AreaW0c_TailSub));
        TailStep(2);
        return;
    case 2:
        if (BFieldE2(::bof3::Img(at::kCounterB)) != 0x20) return;
        SetW(::bof3::Img(at::kTailTimer), 0x10);
        TailStep(3);
        return;
    case 3: {
        const auto left = static_cast<std::uint16_t>(WFieldE2(::bof3::Img(at::kTailTimer)) - 1u);
        SetW(::bof3::Img(at::kTailTimer), left);
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
        if (BFieldE2(::bof3::Img(at::kCounterB)) != 0x31) return;
        BFieldE2(::bof3::Img(at::kCounterB)) = 0;
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
        BFieldE2(::bof3::Img(at::kFieldEC2)) = 1;
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
        BFieldE2(::bof3::Img(at::kFieldEC2)) = 1;
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
        char* const name = reinterpret_cast<char*>(move_script::At(::bof3::Img(at::kTextRecords)));
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
        BFieldE2(::bof3::Img(at::kScriptFlags2)) = static_cast<unsigned char>(BFieldE2(::bof3::Img(at::kScriptFlags2)) | 0x40);
        SH_CALL(ScriptFlags_Clear40)();
        AreaW0a_TailKind = 0;
        AreaW0c_TailState = 0;
        return;
    }
    default: return;
    }
}

// ============================================================================
// Draw layers: three MapCell_Handlers kinds (DrawLayer_Open calls each with
// the cell's record, its byte 1 and byte 0), and a map cell cleared
// ============================================================================

namespace {
// A vertex dword of the quads: the s8 at byte 3 and the s8 at byte 2, each
// doubled, about the cell's origin, and the low word; the fourth word 0
// (DIV-0023, as map_cells.cpp's QuadVertex).
void QuadVertex(short* v, U dword, U x0, U y0) {
    v[0] = static_cast<short>(x0 + 2u * static_cast<U>(S8FieldE2(dword >> 24)));
    v[1] = static_cast<short>(y0 + 2u * static_cast<U>(S8FieldE2(dword >> 16)));
    v[2] = static_cast<short>(dword);
    v[3] = 0;
}
// The four corners (v[0..3], eight words each) projected into the POLY_FT4 or
// POLY_G4 at prim as the originals order them - v1, v2, v3, then v0 to the
// first point - and their depths.
void Project4(short* v, unsigned char* prim) {
    long unused;
    SH_CALL(Gte_RotTransPers4)(v + 4, v + 8, v + 12, v, reinterpret_cast<float*>(prim + 0x18), reinterpret_cast<float*>(prim + 0x28),
                               reinterpret_cast<float*>(prim + 0x38), reinterpret_cast<float*>(prim + 8), &unused);
    SH_CALL(Gte_StoreDepthF4)(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                              reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
}
}  // namespace

// original 0x570870 (MapCell_Handlers[16], in Area_TestCondition's extent):
// a cell's animated quads. Unless the record's condition (its low word) says
// no, or its byte +2 is 1, walks subrecords from dword 1 until the dword
// index equals byte +2 again: four vertex dwords (QuadVertex about ((b1 -
// 0x80) * 128, (b0 - 0x80) * 128)) into a POLY_FT4 at Gfx_PacketNext
// (Gte_RotTransPers4, Gte_StoreDepthF4, shade off); then a frame dword F:
// Frame_Counter % (F & 0xFF) (unsigned) against the threshold bytes from F's
// byte 2 on - the first above it (signed compare) picks the frame -, n =
// ((F >> 8 & 0xFF) + 5) / 4 dwords of thresholds, the texture word n + frame
// dwords after F (Prim_SetTexture), committed to Draw_OtSlot (0x48); the next
// subrecord n + (F >> 8 & 0xFF) dwords after F. Kept as the original: a byte
// +2 no subrecord reaches never ends the walk, and the threshold scan has no
// bound. A period of 0 divides by 0: ours aborts. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawFrames(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(WFieldE2(Key(record))) == 0) return;
    const U x0 = (b1 - 0x80u) << 7, y0 = (b0 - 0x80u) << 7;
    if ((LongAt(record) & 0xFF0000u) == 0x10000u) return;
    U at = 1;
    do {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        alignas(4) short v[16];
        for (U k = 0; k < 4; ++k) QuadVertex(v + 4 * k, LongAt(record + (at + k) * 4u), x0, y0);
        at += 4;
        Project4(v, prim);
        const U frames = LongAt(record + at * 4u);
        const U period = frames & 0xFF;
        if (period == 0) bof3::Fatal("MapCell_DrawFrames: a frame period of 0 at record 0x%X + %u (the original divides by it)",
                                     (unsigned)Key(record), (unsigned)(at * 4u));
        const auto phase = static_cast<std::int32_t>(Frame_Counter % period);
        U frame = 0;
        for (const unsigned char* p = record + at * 4u + 2; phase >= static_cast<std::int32_t>(*p); ++p) ++frame;
        const U skip = (((frames >> 8) & 0xFF) + 5u) >> 2;
        SH_CALL(Prim_SetTexture)(LongAt(record + (frame + skip + at) * 4u), prim, 1);
        const unsigned char slot = Draw_OtSlot;
        at += record[at * 4u + 1] + skip;
        SH_CALL(Gfx_CommitPrim)(slot, at::kPrimBytes);
    } while (at != record[2]);
}

// original 0x570BC0 (MapCell_Handlers[33], in MapCell_FlatOverlay's extent): a
// cell's shaded quads. Unless the record's condition says no: unless byte +2
// is 1, subrecords of eight dwords from dword 1 until the dword index equals
// byte +2 - a draw mode (tpage 0x95 | (bits 24..25 of the first colour) << 5,
// dtd 1) committed to Draw_OtSlot (0xC), a POLY_G4 (Gpu_SetPolyG4) at
// Gfx_PacketNext, four vertex dwords (QuadVertex) through Gte_RotTransPers4
// and Gte_StoreDepthF4, semi-transparent by the first colour's bit 31, four
// colours (bytes 2, 1, 0 of each dword to r, g, b), committed to Draw_OtSlot
// (0x44). Then, and when byte +2 is 1, a draw mode (tpage 0x95) committed to
// Draw_OtSlot (0xC). A byte +2 no subrecord reaches never ends the walk, as
// the original. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawShaded(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(WFieldE2(Key(record))) == 0) return;
    const U x0 = (b1 - 0x80u) << 7, y0 = (b0 - 0x80u) << 7;
    if ((LongAt(record) & 0xFF0000u) != 0x10000u) {
        U at = 1;
        const unsigned char* sub = record + 4;
        do {
            const U tpage = (((LongAt(sub + 0x10) >> 24) & 3u) << 5) | 0x95u;
            SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, tpage, 0);
            SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
            unsigned char* const prim = Gfx_PacketNext;
            SH_CALL(Gpu_SetPolyG4)(prim);
            at += 4;
            alignas(4) short v[16];
            for (U k = 0; k < 4; ++k) QuadVertex(v + 4 * k, LongAt(sub + 4u * k), x0, y0);
            sub += 0x10;
            Project4(v, prim);
            SH_CALL(Gpu_SetSemiTrans)(prim, LongAt(sub) >> 31);
            static const unsigned kColours[4] = {4, 0x14, 0x24, 0x34};
            for (U k = 0; k < 4; ++k) {
                const U c = LongAt(sub + 4u * k);
                prim[kColours[k]] = static_cast<unsigned char>(c >> 16);
                prim[kColours[k] + 1] = static_cast<unsigned char>(c >> 8);
                prim[kColours[k] + 2] = static_cast<unsigned char>(c);
            }
            sub += 0x10;
            at += 4;
            SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x44);
        } while (at != record[2]);
    }
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
}

// original 0x570DE0 (MapCell_Handlers[34], in MapCell_FlatOverlay's extent): a
// cell's spinning quads. Unless the record's condition says no: the centre
// (words of ((b1 - 0x80) * 64 + s8 byte 3 of dword 1) * 2, ((b0 - 0x80) * 64 +
// s8 byte 2) * 2 and dword 1's low word) and a spin S = the three 10-bit
// fields of dword 2 (bits 20, 10, 0) times (Frame_Counter & 0xFFF) * 4, as
// words. Unless byte +2 is 3, subrecords of six dwords from dword 3 until
// the dword index equals byte +2: under a pushed matrix the angles (S + the
// subrecord's first dword's three fields * 4) & 0xFFF through Gte_RotMatrix,
// the centre through Gte_RotTrans into its translation, Gte_MulMatrix0 with
// Camera_Matrix, set as rotation and translation; a POLY_FT4, four vertex
// dwords of three 10-bit fields each (bit 9 the sign; the top field keeps
// bits 10 and 11 when its bit 9 is clear, as the original's shift leaves
// them) through Gte_RotTransPers4 and Gte_StoreDepthF4, the texture word
// (dword 5) through Prim_SetTexture, committed (0x48) to slot 6 when that
// word, read again, has bit 30, else Draw_OtSlot; the matrix popped. The
// SVECTORs' fourth words are 0 (DIV-0023). A byte +2 no subrecord reaches
// never ends the walk, as the original. No PSX twin paired.
extern "C" void __cdecl MapCell_DrawSpinning(const unsigned char* record, unsigned b1, unsigned b0) {
    if (SH_CALL(Area_TestCondition)(WFieldE2(Key(record))) == 0) return;
    const U d1 = LongAt(record + 4), d2 = LongAt(record + 8);
    const U counter = Frame_Counter & 0xFFFu;
    alignas(4) short centre[4];
    centre[0] = static_cast<short>((static_cast<U>(static_cast<std::uint16_t>(S8FieldE2(d1 >> 24))) + ((b1 - 0x80u) << 6)) << 1);
    centre[1] = static_cast<short>((static_cast<U>(static_cast<std::uint16_t>(S8FieldE2(d1 >> 16))) + ((b0 - 0x80u) << 6)) << 1);
    centre[2] = static_cast<short>(d1);
    centre[3] = 0;
    const U spin[3] = {((d2 >> 20) & 0x3FFu) * counter * 4u, ((d2 >> 10) & 0x3FFu) * counter * 4u, (d2 & 0x3FFu) * counter * 4u};
    if ((LongAt(record) & 0xFF0000u) == 0x30000u) return;
    U at = 3;
    const unsigned char* sub = record + 0xC;
    do {
        SH_CALL(Gte_PushMatrix)();
        const U d = LongAt(sub);
        alignas(4) short angles[4];
        angles[0] = static_cast<short>((spin[0] + ((d >> 20) & 0x3FFu) * 4u) & 0xFFFu);
        angles[1] = static_cast<short>((spin[1] + ((d >> 10) & 0x3FFu) * 4u) & 0xFFFu);
        angles[2] = static_cast<short>((spin[2] + (d & 0x3FFu) * 4u) & 0xFFFu);
        angles[3] = 0;
        alignas(4) unsigned char matrix[0x20];
        SH_CALL(Gte_RotTrans)(centre, reinterpret_cast<long*>(matrix + 0x14));
        SH_CALL(Gte_RotMatrix)(angles, reinterpret_cast<short*>(matrix));
        SH_CALL(Gte_MulMatrix0)(Camera_Matrix, reinterpret_cast<short*>(matrix), reinterpret_cast<short*>(matrix));
        SH_CALL(Gte_SetRotMatrix)(reinterpret_cast<unsigned long*>(matrix));
        SH_CALL(Gte_SetTransMatrix)(reinterpret_cast<unsigned long*>(matrix));
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(prim);
        SH_CALL(Gpu_SetShadeTex)(prim, 0);
        alignas(4) short v[16];
        for (U k = 0; k < 4; ++k) {
            const U e = LongAt(sub + 4u + 4u * k);
            U fx = e >> 20, fy = (e >> 10) & 0x3FFu, fz = e & 0x3FFu;
            if (fx & 0x200u) fx |= 0xFFFFFC00u;
            if (fy & 0x200u) fy |= 0xFFFFFC00u;
            if (fz & 0x200u) fz |= 0xFFFFFC00u;
            v[4 * k] = static_cast<short>(fx);
            v[4 * k + 1] = static_cast<short>(fy);
            v[4 * k + 2] = static_cast<short>(fz);
            v[4 * k + 3] = 0;
        }
        Project4(v, prim);
        SH_CALL(Prim_SetTexture)(LongAt(sub + 0x14), prim, 1);
        SH_CALL(Gfx_CommitPrim)((LongAt(sub + 0x14) & 0x40000000u) ? 6u : Draw_OtSlot, at::kPrimBytes);
        SH_CALL(Gte_PopMatrix)();
        at += 6;
        sub += 0x18;
    } while (at != record[2]);
}

// original 0x5728D0 (PSX 0x80155E74): a map cell's byte cleared and its drawn records
// retyped. AreaMap_Bytes[x + z * width] = 0 (s16 x and z, the header's width
// byte). Then the cell in the view: c = x + 1 - MapView_Origin's x, r = z -
// its z (s16s); with c + r in 0..0x37 and c - r in 0..0x37, the row (c + r +
// MapView_Row + 1, wrapped once at 0x38) and column ((c - r) / 2 +
// MapView_Column + 1, wrapped once at 0x1C) name a MapView_Cells word; not
// 0, it plus AreaMap_CellBase's word is a run of records in the area block
// (their count the high word of the dword before), walked by each record's
// byte +2 dwords, and every record of kind 3, 9 or 0x27 (the top byte)
// becomes kind 0x30. Ours aborts where the original reads past MapView_Cells
// (a row or column past its wrap) or where the walk would never end (a step
// of 0, or past the run's end). Called by FC2's 0x46D180, Field_CellPickup
// and 19 Capcom functions; FieldHidden_ClearCell's cells. PSX twin
// 0x80155E74 (gap4, a hypothesis).
extern "C" void __cdecl AreaMap_ClearCell(unsigned x, unsigned z) {
    const int zs = static_cast<short>(z), xs = static_cast<short>(x);
    const U width = LongAt(AreaMap_Header) & 0xFF;
    const_cast<unsigned char*>(AreaMap_Bytes.Native())[static_cast<std::int32_t>(width * static_cast<U>(zs)) + xs] = 0;
    const int c = static_cast<short>(x + 1u - static_cast<std::uint16_t>(MapView_Origin[0]));
    const int r = static_cast<short>(z - static_cast<std::uint16_t>(MapView_Origin[1]));
    int row = c + r;
    if (row >= 0x38 || row < 0) return;
    row += MapView_Row + 1;
    if (row >= 0x38) row -= 0x38;
    int column = c - r;
    if (column >= 0x38 || column < 0) return;
    column = column / 2 + MapView_Column + 1;
    if (column >= 0x1C) column -= 0x1C;
    if (row < 0 || row >= 0x38 || column < 0 || column >= 0x1C)
        bof3::Fatal("AreaMap_ClearCell: view cell (%d, %d) past MapView_Cells' 0x38 x 0x1C (the original reads there)", row, column);
    const U item = WFieldE2(::bof3::Img(at::kMapCells) + 2u * static_cast<U>(column + row * 0x1C));
    if (item == 0) return;
    const U n = item + (AreaMap_CellBase & 0xFFFF);
    U p = ::bof3::Img(AreaMap_Header_addr) + n * 4u;
    const U end = p + (LFieldE2(::bof3::Img(at::kCellRecords) - 4u + n * 4u) >> 16) * 4u - 4u;
    while (p != end) {
        if (p > end) bof3::Fatal("AreaMap_ClearCell: the record walk passed its end 0x%X at 0x%X (the original runs on)", (unsigned)end, (unsigned)p);
        const U v = LFieldE2(p);
        const U kind = v & 0xFF000000u;
        if (kind == 0x03000000u || kind == 0x09000000u || kind == 0x27000000u) SetL(p, (v & 0xFFFFFFu) | 0x30000000u);
        const U step = BFieldE2(p + 2);
        if (step == 0) bof3::Fatal("AreaMap_ClearCell: a record of 0 dwords at 0x%X (the original never ends)", (unsigned)p);
        p += step * 4u;
    }
}

// ============================================================================
// The trade screen (ItemTrade_States 0x66A470, dispatched by 0x593950 on
// 0x93985C - nobody's): a list of entries (0x66AD10's row 0x905B88, ten
// record numbers) of the 8-byte records at 0x66AB58 (+0 the item, +1 its
// category, +2..+4 three ingredients, +5..+7 their amounts); a count to
// choose; a yes / no. The state handlers are hidden starts in
// Sprite_ClutWord's extent. PSX twins 0x800F5048..0x800F5828 (table-
// anchored and callers, each a hypothesis).
// ============================================================================

namespace {
// The record the row's entry k names (0x66AD10[k + row * 10] * 8), unchecked.
U TradeRecord(int k) {
    return ::bof3::Img(at::kTradeRecords) + static_cast<U>(BFieldE2(::bof3::Img(at::kTradeIndex) + static_cast<U>(k + AreaEntry_PendingFlags * 10))) * 8u;
}
void TradeBox() { SH_AT(void (__cdecl*)(int, int, int, int, int), at::kTradeBox.v)(0x14, 0x12, 0x118, 0x13, 0); }
void TradeList(unsigned flag) { SH_AT(void (__cdecl*)(unsigned), at::kTradeList.v)(flag); }
unsigned char TradeLacks(unsigned k, unsigned quantity) {
    return SH_AT(unsigned char (__cdecl*)(unsigned, unsigned), at::kTradeLacks.v)(k, quantity);
}
const unsigned char* PoolFieldE2(U word_cell) { return move_script::At(::bof3::Img(bof3::addr::MessagePools) + (LFieldE2(word_cell) & 0xFFFF)); }
}  // namespace

// original 0x593960 (ItemTrade_States[0]; PSX 0x800F5048): the open step (ItemTrade_OpenSteps
// 0x66A47C by 0x93985E, past its two ours aborts only where the table runs
// into what is not code), then the frame 0x469750(0x14, 0x12, 0x118, 0x13, 0),
// the list 0x594410(0), 0x5947D0 and a tail jump to 0x5942C0.
extern "C" void __cdecl ItemTrade_Open(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAtFieldE2(::bof3::Img(ItemTrade_OpenSteps_addr), Effect1g_TradeStep, "ItemTrade_Open")))();
    TradeBox();
    TradeList(0);
    SH_AT(void (__cdecl*)(), at::kTradeCursor.v)();
    SH_AT(void (__cdecl*)(), at::kTradeFrame.v)();
}

// original 0x5939A0 (ItemTrade_OpenSteps[0]; PSX 0x800F50B4): Transition_Start(1), the pick
// 0x6BE08C = 0, the quantity 0x6BE08E = 1, the row count 0x6BE08D = 0x594790's
// al, the step up.
extern "C" void __cdecl ItemTrade_OpenStart(void) {
    SH_CALL(Transition_Start)(1);
    Effect1g_TradePick = 0;
    Effect1g_TradeQuantity = 1;
    Effect1g_TradeRowCount = SH_AT(unsigned char (__cdecl*)(), at::kTradeRows.v)();
    Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep + 1);
}

// original 0x5939D0 (ItemTrade_OpenSteps[1]; PSX 0x800F510C): once Transition_Busy is
// 0, the state 0x93985C = 1 and the step 0.
extern "C" void __cdecl ItemTrade_OpenWait(void) {
    if (Transition_Busy != 0) return;
    Effect1g_TradeState = 1;
    Effect1g_TradeStep = 0;
}

// original 0x5939F0 (ItemTrade_States[1]; PSX 0x800F5138): the run step (ItemTrade_RunSteps
// 0x66A484 by 0x93985E: pick, count, confirm, and 0x594060's message), then
// 0x5942C0, 0x5947D0 and, with the pick's bits 6 and 7 clear, the hand at
// (0x1A, 0x51 + 13 * the s8 pick).
extern "C" void __cdecl ItemTrade_Run(void) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(CodeAtFieldE2(::bof3::Img(ItemTrade_RunSteps_addr), Effect1g_TradeStep, "ItemTrade_Run")))();
    SH_AT(void (__cdecl*)(), at::kTradeFrame.v)();
    SH_AT(void (__cdecl*)(), at::kTradeCursor.v)();
    const unsigned char pick = static_cast<unsigned char>(Effect1g_TradePick);
    if (pick & 0xC0) return;
    SH_CALL(Menu_DrawHand)(0x1A, static_cast<int>(13u * static_cast<U>(S8FieldE2(pick)) + 0x51u), 0);
}

// original 0x593A30 (ItemTrade_RunSteps[0]; PSX 0x800F51BC): the pick. Up and down
// (Input_AutoRepeat of the pressed word's 0x5000) move it, wrapping at the
// row count, with sound 0x100. Confirm: 0x594700(pick, 1) answering (an
// ingredient short) - sound 0x107; else the two Inventory_Count of the
// entry's item (in the bag, then equipped) summed as a byte: 99 or more - the
// pick's bit 7, sound 0x104, the quantity 0, step 3 (0x594060's message);
// else bit 6, sound 0x104, the step up. Cancel: bit 7, 0x6BE08F = 1, sound
// 0x106, the state 0x93985C up. Then the frame, and with bits 6 and 7 clear
// the entry's help line (Item_HelpMessage through Msg_SystemPtr) at (0x18,
// 0x14); the list 0x594410(0).
extern "C" void __cdecl ItemTrade_PickItem(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0x5000u);
    if (moved & 0x1000u) {
        const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) - 1);
        Effect1g_TradePick = pick;
        if (static_cast<signed char>(pick) < 0) Effect1g_TradePick = static_cast<unsigned char>(Effect1g_TradeRowCount - 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else if (moved & 0x4000u) {
        const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) + 1);
        Effect1g_TradePick = pick;
        if (static_cast<signed char>(pick) >= Effect1g_TradeRowCount) Effect1g_TradePick = 0;
        SH_CALL(Sound_PlayEffect)(0x100);
    }
    const std::uint16_t pressed = Input_Pressed;
    if (Field_ConfirmButtons & pressed) {
        if (TradeLacks(static_cast<unsigned char>(Effect1g_TradePick), 1) != 0) {
            SH_CALL(Sound_PlayEffect)(0x107);
        } else {
            const U record = TradeRecord(Effect1g_TradePick);
            const auto bag = static_cast<unsigned char>(SH_CALL(Inventory_Count)(BFieldE2(record + 1), BFieldE2(record), 1));
            const auto worn = static_cast<unsigned char>(SH_CALL(Inventory_Count)(BFieldE2(record + 1), BFieldE2(record), 0));
            if (static_cast<unsigned char>(bag + worn) >= 0x63) {
                Effect1g_TradePick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) | 0x80);
                SH_CALL(Sound_PlayEffect)(0x104);
                Effect1g_TradeQuantity = 0;
                Effect1g_TradeStep = 3;
            } else {
                Effect1g_TradePick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) | 0x40);
                SH_CALL(Sound_PlayEffect)(0x104);
                BFieldE2(::bof3::Img(at::kTradeStep)) = static_cast<unsigned char>(Effect1g_TradeStep + 1);
            }
        }
    } else if (Field_CancelButtons & pressed) {
        Effect1g_TradeAnswer = 1;
        Effect1g_TradePick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) | 0x80);
        SH_CALL(Sound_PlayEffect)(0x106);
        Effect1g_TradeState = static_cast<unsigned char>(Effect1g_TradeState + 1);
    }
    TradeBox();
    const unsigned char pick = static_cast<unsigned char>(Effect1g_TradePick);
    if ((pick & 0xC0) == 0) {
        const U record = TradeRecord(S8FieldE2(pick));
        const unsigned help = SH_CALL(Item_HelpMessage)(BFieldE2(record + 1), BFieldE2(record));
        const unsigned char* const text = SH_CALL(Msg_SystemPtr)(help);
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, text);
    }
    TradeList(0);
}

// original 0x593C60 (ItemTrade_RunSteps[1]; PSX 0x800F550C): the count. Input_AutoRepeat of
// the pressed word's 0xF000: 0x2000 +1, 0x1000 +10, 0x4000 -10, 0x8000 -1
// (a byte), the quantity 0x6BE08E held to 1..99 (s8); lowered while
// 0x594700(pick & 0xF, quantity) answers; lowered while it plus the item's
// two Inventory_Count summed as a byte passes 99; sound 0x100 if it moved.
// Confirm: 0x6BE08F = 0, sound 0x104, the step up. Cancel: the pick's bit 6
// cleared, the quantity 1, sound 0x106, the step down. Then the frame, the
// prompt (MessagePools + the word at 0x80361C) at (0x18, 0x14), the list
// 0x594410(1), 0x594AD0 and the hand at (0x1A, 0x6B).
extern "C" void __cdecl ItemTrade_PickCount(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xF000u) & 0xFFFFu;
    unsigned char step = 0;
    if (moved == 0x8000u) step = 0xFF;
    else if (moved == 0x4000u) step = 0xF6;
    else if (moved == 0x1000u) step = 0xA;
    else if (moved == 0x2000u) step = 1;
    const unsigned char old = static_cast<unsigned char>(Effect1g_TradeQuantity);
    auto quantity = static_cast<unsigned char>(old + step);
    if (static_cast<signed char>(quantity) < 1) quantity = 1;
    else if (static_cast<signed char>(quantity) > 0x63) quantity = 0x63;
    const unsigned k = static_cast<unsigned char>(Effect1g_TradePick) & 0xFu;
    Effect1g_TradeQuantity = quantity;
    while (TradeLacks(k, quantity) != 0) {
        quantity = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradeQuantity) - 1);
        Effect1g_TradeQuantity = quantity;
    }
    const U record = TradeRecord(static_cast<int>(k));
    const auto bag = static_cast<unsigned char>(SH_CALL(Inventory_Count)(BFieldE2(record + 1), BFieldE2(record), 1));
    const auto worn = static_cast<unsigned char>(SH_CALL(Inventory_Count)(BFieldE2(record + 1), BFieldE2(record), 0));
    const U held = static_cast<unsigned char>(bag + worn);
    // The s8 quantity comes down until it plus the held count is 99 or less:
    // with more than 227 held no s8 does, and the original loops for ever.
    if (held > 227) bof3::Fatal("ItemTrade_PickCount: %u held - no quantity brings the sum to 99 (the original never ends)", (unsigned)held);
    unsigned char q = static_cast<unsigned char>(Effect1g_TradeQuantity);
    while (static_cast<int>(S8FieldE2(q)) + static_cast<int>(held) > 0x63) q = static_cast<unsigned char>(q - 1);
    Effect1g_TradeQuantity = q;
    if (q != old) SH_CALL(Sound_PlayEffect)(0x100);
    const std::uint16_t pressed = Input_Pressed;
    if (Field_ConfirmButtons & pressed) {
        Effect1g_TradeAnswer = 0;
        SH_CALL(Sound_PlayEffect)(0x104);
        Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep + 1);
    } else if (Field_CancelButtons & pressed) {
        Effect1g_TradeQuantity = 1;
        Effect1g_TradePick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) & 0xBF);
        SH_CALL(Sound_PlayEffect)(0x106);
        Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep - 1);
    }
    TradeBox();
    SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, PoolFieldE2(::bof3::Img(at::kPoolWordPick)));
    TradeList(1);
    SH_AT(void (__cdecl*)(), at::kTradeCount.v)();
    SH_CALL(Menu_DrawHand)(0x1A, 0x6B, 0);
}

// DIVERGENCE DIV-0027 (amended 2026-10-03, after the live check): the per-item
// "Is <item> OK?" answers. Capcom draws the two answer words, one space apart,
// at x 0xDE and the hand at 0xDC + 36 * the answer - stops fitted to the Chinese
// words: under the English overlay the hand on No covers Yes (the owner's
// masterAndManillo.txt, frames 2700 and 3030). With the flag on (FieldE2_Inject,
// a Latin overlay, after the self-test) the layout is the load / save screen's,
// as the leave prompt's is (effect_1g.cpp, yes_no_layout.cpp): the first answer
// three spaces further left, the second where it was, the hand two units left
// of each.
unsigned char g_trade_confirm_layout = 0;

namespace {

struct ConfirmAnswers {
    const unsigned char* line;   // a static buffer, good until the next call
    int x;
    int stop[2];                 // the hand on the first answer, on the second
};

ConfirmAnswers ConfirmLayout(const unsigned char* s) {
    constexpr unsigned kMoved = 3;
    static unsigned char line[32];
    static const unsigned char kSpace[2] = {0x20, 0};
    unsigned first = 0;
    while (s[first] && s[first] != 0x20) ++first;
    unsigned second = first;
    while (s[second] == 0x20) ++second;
    unsigned end = second;
    while (s[end] && s[end] != 0x20) ++end;
    bool plain = first != 0 && second != first && end != second && s[end] == 0 && end + kMoved < sizeof line;
    for (unsigned i = 0; plain && i < end; ++i) plain = s[i] >= 0x20 && s[i] < 0x80;
    if (!plain) bof3::Fatal("DIV-0027: the trade's answers are not two one-byte words with spaces between");
    unsigned n = 0;
    for (unsigned i = 0; i < first; ++i) line[n++] = s[i];
    for (unsigned i = 0; i < second - first + kMoved; ++i) line[n++] = 0x20;
    const unsigned second_at = n;
    for (unsigned i = second; i < end; ++i) line[n++] = s[i];
    line[n] = 0;
    const int x = 0xDE - static_cast<int>(kMoved) * TextAdvance_Of(kSpace);
    int pen = x;
    for (unsigned i = 0; i < second_at; ++i) pen += TextAdvance_Of(line + i);
    return {line, x, {x - 2, pen - 2}};
}

}  // namespace

// original 0x593E60 (ItemTrade_RunSteps[2]): the yes / no. Left or right
// (Input_AutoRepeat of 0xA000) flips the hand 0x6BE08F, sound 0x100. Else
// cancel: sound 0x106, the step down; confirm on no: the same; confirm on
// yes: sound 0x105, the pick's bit 6 cleared, Inventory_Add(the entry's
// category, item, the quantity) (a fourth word 0 pushed), 0x594D90 (the
// ingredients taken), the quantity 1 and step 0. Then the frame; at step 2
// the item's name (Item_NamePtr, 16 bytes) into Text_Records, the two
// prompts (the words at 0x80360C, 0x803614) at (0x18, 0x14) and (0xDE, 0x14),
// the hand at (0xDC + 36 * the answer, 0x16); the list 0x594410(1); at any
// step but 0 a tail jump to 0x594AD0.
extern "C" void __cdecl ItemTrade_Confirm(void) {
    const U moved = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000u);
    if (moved & 0xA000u) {
        Effect1g_TradeAnswer = static_cast<unsigned char>(Effect1g_TradeAnswer ^ 1);
        SH_CALL(Sound_PlayEffect)(0x100);
    } else {
        const std::uint16_t pressed = Input_Pressed;
        if (Field_CancelButtons & pressed) {
            SH_CALL(Sound_PlayEffect)(0x106);
            Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep - 1);
        } else if (Field_ConfirmButtons & pressed) {
            if (Effect1g_TradeAnswer != 0) {
                SH_CALL(Sound_PlayEffect)(0x106);
                Effect1g_TradeStep = static_cast<unsigned char>(Effect1g_TradeStep - 1);
            } else {
                SH_CALL(Sound_PlayEffect)(0x105);
                const auto pick = static_cast<unsigned char>(static_cast<unsigned char>(Effect1g_TradePick) & 0xBF);
                Effect1g_TradePick = pick;
                const U record = TradeRecord(S8FieldE2(pick));
                SH_CALL(Inventory_Add)(BFieldE2(record + 1), BFieldE2(record), static_cast<unsigned char>(Effect1g_TradeQuantity));
                SH_AT(void (__cdecl*)(), at::kTradeTake.v)();
                Effect1g_TradeQuantity = 1;
                Effect1g_TradeStep = 0;
            }
        }
    }
    TradeBox();
    if (Effect1g_TradeStep == 2) {
        const U record = TradeRecord(static_cast<int>(static_cast<unsigned char>(Effect1g_TradePick) & 0xFu));
        const unsigned char* const name = SH_CALL(Item_NamePtr)(BFieldE2(record + 1), BFieldE2(record));
        std::memcpy(move_script::At(::bof3::Img(at::kTextRecords)), name, 0x10);
        SH_CALL(Text_DrawAt)(0x18, 0x14, 0, 0xFF, PoolFieldE2(::bof3::Img(at::kPoolWordName)));
        if (g_trade_confirm_layout) {
            const ConfirmAnswers a = ConfirmLayout(PoolFieldE2(::bof3::Img(at::kPoolWordAsk)));
            SH_CALL(Text_DrawAt)(a.x, 0x14, 0, 0xFF, a.line);
            SH_CALL(Menu_DrawHand)(a.stop[Effect1g_TradeAnswer != 0 ? 1 : 0], 0x16, 0);
        } else {
            SH_CALL(Text_DrawAt)(0xDE, 0x14, 0, 0xFF, PoolFieldE2(::bof3::Img(at::kPoolWordAsk)));
            SH_CALL(Menu_DrawHand)(static_cast<int>(Effect1g_TradeAnswer * 36u + 0xDCu), 0x16, 0);
        }
    }
    TradeList(1);
    if (Effect1g_TradeStep == 0) return;
    SH_AT(void (__cdecl*)(), at::kTradeCount.v)();
}

// ============================================================================

void FieldE2_Inject() {
    if (bof3::WantsShadow("field_e2")) field_e2::SelfTest();
    {
        // DIV-0166: after the self-test, which compared Capcom's unterminated copy.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("GeneNameNul", Low32(&field_e2::g_name_nul), &was, &is, 1);
        bof3::Log("DIV-0166    message 0xFA's gene name is terminated after its 8-byte copy (on unless the line above says OFF)");
    }
    BOF3_INJECT(Field_ObjectTrigger);
    BOF3_INJECT(Scenario_CellHook);
    BOF3_INJECT(FieldTail_LoadBank);
    BOF3_INJECT(FieldTail_DropInMove);
    BOF3_INJECT(FieldTail_HealAndMenu);
    BOF3_INJECT(FieldTail_Message);
    BOF3_INJECT(FieldTail_WorldMapHook);
    BOF3_INJECT(FieldTail_FlagMessage);
    BOF3_INJECT(FieldTail_StoryWarp);
    BOF3_INJECT(Field_ObjectTriggerByKind);
    BOF3_INJECT(MapCell_DrawFrames);
    BOF3_INJECT(MapCell_DrawShaded);
    BOF3_INJECT(MapCell_DrawSpinning);
    BOF3_INJECT(AreaMap_ClearCell);
    BOF3_INJECT(ItemTrade_Open);
    BOF3_INJECT(ItemTrade_OpenStart);
    BOF3_INJECT(ItemTrade_OpenWait);
    BOF3_INJECT(ItemTrade_Run);
    BOF3_INJECT(ItemTrade_PickItem);
    BOF3_INJECT(ItemTrade_PickCount);
    BOF3_INJECT(ItemTrade_Confirm);
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's prompt; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("TradeConfirmLayout", Low32(&g_trade_confirm_layout),
                         &was, &is, 1);
        bof3::Log("DIV-0027    Manillo's \"Is <item> OK?\": answers re-spaced, the hand two units left of each");
    }
}
