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

#include "common/image_base.h"

#include "bof3/symbols.gen.h"

namespace shop_states {

using U = std::uint32_t;

namespace at {

// The menu state block 0x929F00 (docs/menu-screens.md), as the shop mode uses it.
constexpr ::bof3::image::Va kState{0x929F00};         // u8: the mode's state, ShopMode_States' index
constexpr ::bof3::image::Va kStep{0x929F01};          // u8: the inn's step, Inn_Steps' index
constexpr ::bof3::image::Va kSub{0x929F02};           // u8: the step's state (the three state tables' index)
constexpr ::bof3::image::Va kCount{0x929F04};         // u8: the menus' frame counter
// kClear0A: now ShopStates_Clear0A (symbols.toml). u8: cleared by ShopMode_Begin (its reader not read)
// kAnswer: now FieldS_Answer (symbols.toml). u8: Menu_YesNo's answer, 1 yes
constexpr ::bof3::image::Va kObject{0x929F0C};        // s8: the object touched (Sprite_Objects index); 0xFE a save point, 0xFF none
// kSavedObject: now AreaW3c_Load0F (symbols.toml). u8: kObject kept while a forced save point runs
constexpr ::bof3::image::Va kShopKind{0x929EC2};      // u8: the state - 2 when no object is touched
constexpr ::bof3::image::Va kShopNumber{0x929EC3};    // u8: the object's +0x1C; the inn's night costs ten times it
// kShopRecord: now ShopStates2_ShopList (symbols.toml). u32: 0x658930 + 23 * kShopNumber
constexpr ::bof3::image::Va kShopRecords{0x658930};   // 23-byte records, one per shop number
constexpr U kShopRecordBytes = 23;

// The eight character records (0xA4 bytes from 0x903A70; docs/field-event.md):
// ShopMode_Begin walks them from +0x10, Field_ActorStates.
// (The data names in symbols.gen.h are macros, so the addresses are spelled
// out here, each with its name.)
// kActorStates: now Field_ActorStates (symbols.toml). Field_ActorStates: +0 the status word, +8 HP, +0x10 max HP
constexpr U kActorStride = 0xA4;
constexpr U kActors = 8;

// kSpriteObjects: now Sprite_Objects (symbols.toml). Sprite_Objects, 0xA4 bytes each
constexpr U kSpriteStride = 0xA4;
constexpr U kSpriteCount = 30;         // to 0x7E01B8
constexpr ::bof3::image::Va kSpriteCurrent{0x937F88}; // Sprite_Current
// kScriptFlags: now Field_ScriptFlags (symbols.toml). Field_ScriptFlags, u16, bit 8 cleared on the way out
// kGameStep: now Game_Step (symbols.toml). Game_Step, u16
// kInputFlags: now Field_InputFlags (symbols.toml). Field_InputFlags, u8, 0x40 forces the save point
// kArea: now Game_AreaNumber (symbols.toml). Game_AreaNumber, u16
constexpr ::bof3::image::Va kPressed{0x7E1BEC};       // Input_Pressed, u16 (the inn's chooser reads it as a dword)
// kConfirm: now Field_ConfirmButtons (symbols.toml). Field_ConfirmButtons, u16
// kCancel: now Field_CancelButtons (symbols.toml). Field_CancelButtons, u16

// kColour: now Config_WindowColour (symbols.toml). s8: the window colour (Config), a CLUT row
constexpr ::bof3::image::Va kMsgFlags{0x7DEE44};      // the text layer's flag word; bit 1 read as "the system message is done"
constexpr ::bof3::image::Va kText{0x904BA0};          // the shared print buffer
constexpr ::bof3::image::Va kFormatD{0x5E10C0};       // "%d"
// kZenny: now Party_Zenny (symbols.toml). u32: the party's zenny
// kFade: now Transition_Busy (symbols.toml). Transition_Busy, u16: non-zero while a transition runs
constexpr ::bof3::image::Va kRepeatLatch{0x7E01B8};   // u16: Input_AutoRepeat's latch
// kRepeatTimer: now ItemUse_RepeatTimer (symbols.toml). u16: Input_AutoRepeat's timer
constexpr ::bof3::image::Va kSlot{0x9036D4};          // u32: the save cursor, 0..15
constexpr ::bof3::image::Va kSlotTop{0x8034D0};       // u32: the first of the three slots shown
// kInnFlag: now ShopStates_InnFlag (symbols.toml). u8: with kSaveBack clear, FieldSave_End opens message 0xD0 (unread)
constexpr ::bof3::image::Va kSaveBack{0x6BC880};      // u8: set by the save menu's state 5 (0x5800D0, R2C's FieldSave_Written)
// kChoice: now ShopStates_Choice (symbols.toml). u8: the inn's three-way cursor (the save menu's too)
// kCancelled: now ShopStates_Cancelled (symbols.toml). u8: the inn's prompt was cancelled

// The dispatch tables (symbols.toml [[data]]).
// kModeStates: now ShopMode_States (symbols.toml). ShopMode_States, 11, by kState
// kInnSteps: now Inn_Steps (symbols.toml). Inn_Steps, 5, by kStep
// kPromptStates: now InnPrompt_States (symbols.toml). InnPrompt_States, 6, by kSub
// kNightStates: now InnNight_States (symbols.toml). InnNight_States, 7, by kSub
// kSaveStates: now FieldSave_States (symbols.toml). FieldSave_States, 9, by kSub
constexpr ::bof3::image::Va kTablesEnd{0x663FB8};     // the next table (0x580300's) starts here

}  // namespace at

// Crt_sprintf is Capcom's, so its name in symbols.gen.h is a macro, not an
// addr constant the fuzz's call tables could use.
constexpr ::bof3::image::CodeVa kCrtSprintf{0x5B9380};

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
