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

namespace field_modes {

// Unnamed addresses, each read in docs/field-modes.md. The PSX twin of each
// scenario byte is in the scenario block at 0x8014686C (the sibling's
// docs/LOADER_RECORDS.md).
constexpr std::uint32_t kScenarioTables = 0x662C80;   // 20 pointers, one per chapter, to a vtable (PSX 0x801C944C)
constexpr std::uint32_t kCallATables = 0x660B84;      // 20 pointers, one per chapter, to Scenario_CallA's table (PSX 0x801CDC4C)
constexpr std::uint32_t kTailTable = 0x662CD0;        // Field_ModeTail's two entries (PSX 0x801D949C)
constexpr std::uint32_t kTailRunTable = 0x662CE8;     // Field_ModeTailRun's entries
constexpr std::uint32_t kStateTable = 0x6619FC;       // scenario 16's states (PSX 0x801F8538); its +0xC is
constexpr std::uint32_t kRunTable = 0x661A08;         // the run table Scena16_Run reads (PSX 0x801F8544)
constexpr std::uint32_t kTailPhase = 0x9039F2;        // s8, Field_ModeTail's index (PSX 0x801448E6)
constexpr std::uint32_t kTailKind = 0x9039F3;         // s8, Field_ModeTailRun's index (PSX 0x801448E7)
constexpr std::uint32_t kState = 0x8034E2;            // s8, scenario 16's state (PSX 0x8014686E)
constexpr std::uint32_t kStep = 0x8034E5;             // u8, the scene's step (PSX 0x80146871)
constexpr std::uint32_t kTimer = 0x8034E6;            // u16, the scene's timer (PSX 0x80146872)
constexpr std::uint32_t kBadge = 0x66C7F8;            // the demo's corner badge (docs/mode-tasks.md section 1)
constexpr std::uint32_t kCounters = 0x903848;         // four script counter bytes (PSX 0x80146860; MoveScript_CounterOps)
constexpr std::uint32_t kSlot = 0x903850;             // the effect slot just taken; also a word (PSX 0x1F800000)
constexpr std::uint32_t kFlagsPtr = 0x929ED0;         // the flag bits Flags_Test / Flags_Set are given (PSX 0x80146868)
constexpr std::uint32_t kPending = 0x903A04;          // the pending area change, as Field_ChangeArea's arguments
constexpr std::uint32_t kMusicTrack = 0x904131;       // the track last started, 0xFF none

// The callees without a name in symbols.toml (docs/field-modes.md section 6
// has what each is): called by address.
constexpr std::uint32_t kChangeArea = 0x594E00;       // PSX Field_ChangeArea 0x801A0A30
constexpr std::uint32_t kStartMusic = 0x587A20;       // PSX 0x801625AC
constexpr std::uint32_t kTaskEnd = 0x5A99AD;          // ends the current task; never returns
constexpr std::uint32_t kEffectAlloc = 0x589810;      // PSX 0x8019701C
constexpr std::uint32_t kPlaceParty = 0x531F90;       // PSX 0x801BF1A8
constexpr std::uint32_t kOpenScript = 0x4976D0;       // Msg_OpenScript
constexpr std::uint32_t kViewShift = 0x56FCA0;        // PSX 0x80155154

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
