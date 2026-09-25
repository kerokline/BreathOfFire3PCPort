// Internal to shop_states.cpp and shop_states_fuzz.cpp: the addresses the
// shop overlay's first table reads and writes, and every call its 25
// functions make, through pointers, so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. docs/shop_states.md.
//
// The five dispatch tables are in .data, not on a stack: the fuzz fills
// their entries with recorders for the test and puts them back.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace shop_states {

using U = std::uint32_t;

namespace at {

// The menu state block 0x929F00 (docs/menu-screens.md), as the shop mode uses it.
constexpr U kState = 0x929F00;         // u8: the mode's state, ShopMode_States' index
constexpr U kStep = 0x929F01;          // u8: the inn's step, Inn_Steps' index
constexpr U kSub = 0x929F02;           // u8: the step's state (the three state tables' index)
constexpr U kCount = 0x929F04;         // u8: the menus' frame counter
constexpr U kClear0A = 0x929F0A;       // u8: cleared by ShopMode_Begin (its reader not read)
constexpr U kAnswer = 0x929F0B;        // u8: Menu_YesNo's answer, 1 yes
constexpr U kObject = 0x929F0C;        // s8: the object touched (Sprite_Objects index); 0xFE a save point, 0xFF none
constexpr U kSavedObject = 0x929F0F;   // u8: kObject kept while a forced save point runs
constexpr U kShopKind = 0x929EC2;      // u8: the state - 2 when no object is touched
constexpr U kShopNumber = 0x929EC3;    // u8: the object's +0x1C; the inn's night costs ten times it
constexpr U kShopRecord = 0x903844;    // u32: 0x658930 + 23 * kShopNumber
constexpr U kShopRecords = 0x658930;   // 23-byte records, one per shop number
constexpr U kShopRecordBytes = 23;

// The eight character records (0xA4 bytes from 0x903A70; docs/field-event.md):
// ShopMode_Begin walks them from +0x10, Field_ActorStates.
// (The data names in symbols.gen.h are macros, so the addresses are spelled
// out here, each with its name.)
constexpr U kActorStates = 0x903A80;   // Field_ActorStates: +0 the status word, +8 HP, +0x10 max HP
constexpr U kActorStride = 0xA4;
constexpr U kActors = 8;

constexpr U kSpriteObjects = 0x7DEE80; // Sprite_Objects, 0xA4 bytes each
constexpr U kSpriteStride = 0xA4;
constexpr U kSpriteCount = 30;         // to 0x7E01B8
constexpr U kSpriteCurrent = 0x937F88; // Sprite_Current
constexpr U kScriptFlags = 0x9039A2;   // Field_ScriptFlags, u16, bit 8 cleared on the way out
constexpr U kGameStep = 0x66C7EA;      // Game_Step, u16
constexpr U kInputFlags = 0x905BA2;    // Field_InputFlags, u8, 0x40 forces the save point
constexpr U kArea = 0x904EFC;          // Game_AreaNumber, u16
constexpr U kPressed = 0x7E1BEC;       // Input_Pressed, u16 (the inn's chooser reads it as a dword)
constexpr U kConfirm = 0x90358E;       // Field_ConfirmButtons, u16
constexpr U kCancel = 0x903590;        // Field_CancelButtons, u16

constexpr U kColour = 0x903A5A;        // s8: the window colour (Config), a CLUT row
constexpr U kMsgFlags = 0x7DEE44;      // the text layer's flag word; bit 1 read as "the system message is done"
constexpr U kText = 0x904BA0;          // the shared print buffer
constexpr U kFormatD = 0x5E10C0;       // "%d"
constexpr U kZenny = 0x904058;         // u32: the party's zenny
constexpr U kFade = 0x66C810;          // MoveScript_WaitWordDA, u16: non-zero while a transition runs
constexpr U kRepeatLatch = 0x7E01B8;   // u16: Input_AutoRepeat's latch
constexpr U kRepeatTimer = 0x7E1BE0;   // u16: Input_AutoRepeat's timer
constexpr U kSlot = 0x9036D4;          // u32: the save cursor, 0..15
constexpr U kSlotTop = 0x8034D0;       // u32: the first of the three slots shown
constexpr U kInnFlag = 0x66C7DA;       // u8: with kSaveBack clear, FieldSave_End opens message 0xD0 (unread)
constexpr U kSaveBack = 0x6BC880;      // u8: set by the save menu's state 5 (0x5800D0, not ours)
constexpr U kChoice = 0x6BC881;        // u8: the inn's three-way cursor (the save menu's too)
constexpr U kCancelled = 0x6BC882;     // u8: the inn's prompt was cancelled

// The dispatch tables (symbols.toml [[data]]).
constexpr U kModeStates = 0x663E40;    // ShopMode_States, 11, by kState
constexpr U kInnSteps = 0x663F4C;      // Inn_Steps, 5, by kStep
constexpr U kPromptStates = 0x663F60;  // InnPrompt_States, 6, by kSub
constexpr U kNightStates = 0x663F78;   // InnNight_States, 7, by kSub
constexpr U kSaveStates = 0x663F94;    // FieldSave_States, 9, by kSub
constexpr U kTablesEnd = 0x663FB8;     // the next table (0x580300's) starts here

}  // namespace at

// Crt_sprintf is Capcom's, so its name in symbols.gen.h is a macro, not an
// addr constant the fuzz's call tables could use.
constexpr U kCrtSprintf = 0x5B9380;

struct Callees {
    void (__cdecl* title_box)(int, int, int, int, int);                              // Menu_DrawTitleBox
    void (__cdecl* money_box)(int, int, int, unsigned);                              // Menu_DrawMoneyBox
    const unsigned char* (__cdecl* draw_text)(int, int, int, int, const unsigned char*);  // Text_DrawAt
    const unsigned char* (__cdecl* system_ptr)(unsigned);                            // Msg_SystemPtr
    void (__cdecl* open_system)(unsigned);                                           // Msg_OpenSystem
    void (__cdecl* play)(unsigned short);                                            // Sound_PlayEffect
    void (__cdecl* stream)(unsigned);                                                // Sound_LoadStream
    int (__cdecl* stream_done)();                                                    // Sound_StreamDone
    void (__cdecl* slots)(int, int, unsigned);                                       // SaveMenu_DrawSlots
    void (__cdecl* read_summaries)();                                                // Save_ReadSummaries
    unsigned (__cdecl* auto_repeat)(unsigned);                                       // Input_AutoRepeat
    unsigned char (__cdecl* yes_no)();                                               // Menu_YesNo
    void (__cdecl* transition)(unsigned char);                                       // Transition_Start
    void (__cdecl* face)(unsigned char);                                             // Sprite_FaceDirection
    void (__cdecl* reset_windows)();                                                 // Window_ResetAll
    int (__cdecl* sprintf_)(char*, const char*, ...);                                // Crt_sprintf
    void (__cdecl* text_record)(unsigned, unsigned, const unsigned char*);           // TextRecord_Set
    void (__cdecl* choices)(int, int);                                               // SaveMenu_DrawChoices
    void (__cdecl* black)();                                                         // Menu_DrawBlackScreen
    void (__cdecl* restore)(unsigned);                                               // Party_RestoreAll
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (shop_states_fuzz.cpp): clones all 25 with every call out
// re-aimed at a recording stand-in and the five dispatch tables filled with
// recorders, runs ours against the clones from the same random state, and
// ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace shop_states
