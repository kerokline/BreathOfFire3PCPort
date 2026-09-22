// Internal to event_script.cpp and event_script_fuzz.cpp: every call the 28
// functions make, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. Most of the callees are ours in this module; through the pointers
// each function is tested alone.
#pragma once

#include <cstdint>

namespace event_script {

using Step = const unsigned char* (__cdecl*)(const unsigned char*);
using ArmStep = const unsigned char* (__cdecl*)(const unsigned char*, unsigned char*, unsigned char*);
using Handler = void (__cdecl*)(const unsigned char*);
// A condition, as the interpreter calls it: the address of the script
// position, which it may move; the answer is al. Read here as the whole of
// eax, and masked by the caller, because Capcom's conditions leave the rest
// of eax holding whatever it held (EventCond_ByteFA: the position's bytes).
using Condition = unsigned (__cdecl*)(const unsigned char**);

struct Callees {
    // the interpreter
    Step op;               // EventScript_Op
    Step control;          // EventScript_Control
    Step if_;              // EventScript_If
    Step if_not;           // EventScript_IfNot
    Step switch_;          // EventScript_Switch
    ArmStep run_step;      // EventScript_IfRunStep
    ArmStep skip_step;     // EventScript_IfSkipStep
    Step skip_control;     // EventScript_SkipControl
    Step skip_if;          // EventScript_SkipIf (Capcom's)
    Step skip_switch;      // EventScript_SkipSwitch (Capcom's)
    Step case_run;         // EventScript_CaseRun
    Step case_skip;        // EventScript_CaseSkip
    // EventScript_Conditions as an array: entry 0 at this address. The
    // switch indexes it signed and unmasked, so it is read from -128 to 127.
    const Condition* conditions;
    // EventScript_Op's handlers by the high nibble; 0 and F are both EventOp_0x.
    Handler handlers[16];
    // the placements
    void (__cdecl* reset)();                        // EventObj_Reset
    unsigned char (__cdecl* set_bank)(unsigned short);   // Sprite_SetAnimationBank
    long (__cdecl* elevation)(long, long);          // AreaMap_Elevation
    void (__cdecl* set_flags)(const unsigned char*);  // EventObj_SetFlags
    void (__cdecl* face)();                         // EventObj_Face (Capcom's)
    void (__cdecl* clear_record)(unsigned);         // PartyRecord_Clear
    void (__cdecl* init_entry)(unsigned char*);     // Sprite_InitFromEntry
    void (__cdecl* kind2_place)(unsigned char);     // Kind2_Place (Capcom's)
    // the rest
    void (__cdecl* run)(const unsigned char*);      // EventScript_Run
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);  // Flags_Test
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (event_script_fuzz.cpp): clones every function with each
// call re-aimed at a recording stand-in, runs ours against the clones, and
// ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace event_script
