// Internal to field_modes.cpp and field_modes_fuzz.cpp: every call the
// seventeen functions make, through pointers, so that the start-up fuzz can
// stand recording functions in for them - for the originals' copies and for
// ours alike. Seven of the callees are ours in this module (the three area
// set-ups, the tail dispatcher, the two CLUT strip functions and
// Scenario_CallA); through the pointers each function is tested alone.
// Indirect calls - the scenario tables and the state tables - are read from
// memory as the original reads them, so the fuzz swaps the tables instead.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace field_modes {

// Unnamed addresses, each read in docs/field-modes.md. The PSX twin of each
// scenario byte is in the scenario block at 0x8014686C (the sibling's
// docs/LOADER_RECORDS.md).
// kScenarioTables: now Scenario_Hooks (symbols.toml). 20 pointers, one per chapter, to a vtable (PSX 0x801C944C)
// kCallATables: now Scenario_CallATables (symbols.toml). 20 pointers, one per chapter, to Scenario_CallA's table (PSX 0x801CDC4C)
constexpr std::uint32_t kTailTable = 0x662CD0;        // Field_ModeTail's two entries (PSX 0x801D949C)
// kTailRunTable: now Field_ModeTailKinds (symbols.toml). Field_ModeTailRun's entries
constexpr std::uint32_t kStateTable = 0x6619FC;       // scenario 16's states (PSX 0x801F8538); its +0xC is
constexpr std::uint32_t kRunTable = 0x661A08;         // the run table Scena16_Run reads (PSX 0x801F8544)
// kTailPhase: now SaveMenu_NewGameByte (symbols.toml). s8, Field_ModeTail's index (PSX 0x801448E6)
// kTailKind: now AreaW0a_TailKind (symbols.toml). s8, Field_ModeTailRun's index (PSX 0x801448E7)
// kState: now MoveScript_PartyRecords[0x62] (symbols.toml). s8, scenario 16's state (PSX 0x8014686E)
constexpr std::uint32_t kStep = 0x8034E5;             // u8, the scene's step (PSX 0x80146871)
// kTimer: now MoveScript_PartyRecords[0x66] (symbols.toml). u16, the scene's timer (PSX 0x80146872)
// kBadge: now Title_BadgeOn (symbols.toml). the demo's corner badge (docs/mode-tasks.md section 1)
constexpr std::uint32_t kCounters = 0x903848;         // four script counter bytes (PSX 0x80146860; MoveScript_CounterOps)
inline auto& kSlot = SharedScratch;  // the effect slot just taken; also a word (PSX 0x1F800000)
constexpr std::uint32_t kSlot_addr = SharedScratch_addr; // kSlot's address, for the raw accessors that take one
// kFlagsPtr: now Chapter_FlagRow (symbols.toml). the flag bits Flags_Test / Flags_Set are given (PSX 0x80146868)
// kPending: now FieldModes_Pending (symbols.toml). the pending area change, as Field_ChangeArea's arguments
// kMusicTrack: now Music_Track (symbols.toml). the track last started, 0xFF none

// The callees without a name in symbols.toml (docs/field-modes.md section 6
// has what each is): called by address.
constexpr ::bof3::image::CodeVa kChangeArea{bof3::addr::Field_ChangeArea};       // PSX Field_ChangeArea 0x801A0A30
constexpr ::bof3::image::CodeVa kStartMusic{bof3::addr::Music_LoadFile};       // PSX 0x801625AC
constexpr ::bof3::image::CodeVa kTaskEnd{bof3::addr::Task_Exit};          // ends the current task; never returns
constexpr ::bof3::image::CodeVa kEffectAlloc{bof3::addr::Effect_FindFree};      // PSX 0x8019701C
constexpr ::bof3::image::CodeVa kPlaceParty{bof3::addr::Party_DropIn};       // PSX 0x801BF1A8
constexpr ::bof3::image::CodeVa kOpenScript{bof3::addr::Msg_OpenScript};       // Msg_OpenScript
constexpr ::bof3::image::CodeVa kViewShift{bof3::addr::MapView_FillCells};        // PSX 0x80155154

struct Callees {
    void (__cdecl* call_a)(unsigned);                                             // Scenario_CallA (ours)
    void (__cdecl* change_area)(int, int, int, int);
    void (__cdecl* start_music)(int);
    int (__cdecl* load_done)();
    void (__cdecl* sleep)(int);
    void (__cdecl* task_end)();
    void (__cdecl* obj_trio)();                                                   // ObjTrio_SetBit40
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);
    void (__cdecl* flags_set)(unsigned char*, unsigned);
    void (__cdecl* script_flags)();                                               // ScriptFlags_Set40
    void (__cdecl* set_elevation)(int);                                           // MapView_SetElevation
    void (__cdecl* kind2_place)(unsigned char);
    unsigned char (__cdecl* effect_alloc)();
    void (__cdecl* place_party)(int);
    void (__cdecl* sound)(unsigned short);                                        // Sound_PlayEffect
    void (__cdecl* music_stop)(int);                                              // Music_FadeOutStop
    void (__cdecl* transition)(unsigned char);                                    // Transition_Start
    void (__cdecl* music_play)(unsigned, int);                                    // Music_Play
    const unsigned char* (__cdecl* text)(int, int, int, int, const unsigned char*);  // Text_DrawAt
    void (__cdecl* open_script)(unsigned short);
    void (__cdecl* view_shift)();
    void (__cdecl* clut_fade)(unsigned);                                          // ClutStrip_FadeTo (ours)
    void (__cdecl* clut_restore)();                                               // ClutStrip_Restore (ours)
    void (__cdecl* area_1f)();                                                    // Scena16_Area1F (ours)
    void (__cdecl* area_04)();                                                    // Scena16_Area04 (ours)
    void (__cdecl* area_02)();                                                    // Scena16_Area02 (ours)
    void (__cdecl* mode_tail)();                                                  // Field_ModeTail (ours)
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (field_modes_fuzz.cpp): clones every function with each
// call re-aimed at a recording stand-in and every table it reads swapped for
// recording entries, runs ours against the clones, and ends the process
// through bof3::Fatal on any difference.
void SelfTest();

}  // namespace field_modes
