// Internal to save_menu.cpp and save_menu_fuzz.cpp: the addresses the save and
// load menus, the inn, the shop's price helpers and the sound stream touch,
// and every call they make - through pointers, so that the start-up fuzz can
// stand recording functions in for them, for Capcom's copies and for ours
// alike. docs/save-menu.md.
//
// Calls into other groups' functions go through raw addresses here (the rule
// for calls across groups, docs/takeover-queue-round6.md): the window pieces
// of group Y (0x57CF60, 0x5762D0, 0x575690, 0x574AB0, 0x576960, 0x573CE0,
// 0x5749F0, Menu_YesNo 0x5747D0) and the stats helpers of group W
// (Menu_DrawHand 0x5905D0, Char_RecalcStats 0x590660, 0x5918E0, 0x591B60,
// Inventory_Add 0x590BB0).
#pragma once

#include <cstdint>

namespace save_menu {

using U = std::uint32_t;

// --- Data, all Capcom's addresses (docs/save-menu.md section 2) -------------
namespace at {
// The title and load flow, task 0's mode 1 (0x588E88 calls it through 0x667294).
constexpr U kGameStep = 0x66C7EA;       // Game_Step, u16: the step 0x587DB0 dispatches on
constexpr U kStepTable = 0x6671F4;      // six code pointers, indexed by Game_Step unchecked
constexpr U kMenuTable = 0x66720C;      // fifteen, indexed by kFlowState unchecked (title menu)
constexpr U kLoadTable = 0x667228;      // = kMenuTable + 7 entries: the load menu's eight
constexpr U kFlowState = 0x6BDF86;      // u8: both menus' state
constexpr U kCursor = 0x6BDF85;         // s8: the title menu's row
constexpr U kAnySave = 0x6BDF8E;        // u8: Save_ListFiles found something (three rows, not two)
constexpr U kRowBright = 0x6BDF90;      // u8[3]: each row's brightness
constexpr U kErrorText = 0x6BDF84;      // u8: the load error's message, 0x11 or 0x12 (+0xA6)
constexpr U kApplyDelay = 0x6BDF94;     // u8: frames before the loaded game is applied
constexpr U kDefaultPad = 0x6BDF96;     // u8: Start held during the load - the default buttons
constexpr U kErrorKind = 0x929ED4;      // u32: 1 no save found, 2 a read or checksum failure
constexpr U kCounter = 0x929F04;        // u8: the menus' frame counter
constexpr U kMenuFlags = 0x7DEE44;      // u8: bit 1 set once the title frame is up
constexpr U kWaitWord = 0x66C810;       // MoveScript_WaitWordDA, u16: a transition is running
constexpr U kPressed = 0x7E1BEC;        // Input_Pressed (read as a dword by 0x587FA0)
constexpr U kHeld = 0x7E1BE8;           // Input_Held (read as a dword)
constexpr U kButtons = 0x903580;        // the pad map, 0x12 bytes: menu +4, confirm +0xE, cancel +0x10
constexpr U kConfirm = 0x90358E;        // Field_ConfirmButtons, u16
constexpr U kCancel = 0x903590;         // Field_CancelButtons, u16
constexpr U kSavedButtons = 0x90469C;   // the pad map in the game block (block +0xCBC)
constexpr U kSlot = 0x9036D4;           // u32: the load / save cursor, 0..15
constexpr U kSlotTop = 0x8034D0;        // u32: the first of the three slots shown
constexpr U kAnswer = 0x929F0B;         // u8: Menu_YesNo's answer
constexpr U kWindowStyle = 0x903A5B;    // u8, 0x575690's argument (block +0x7B)
constexpr U kWindowColor = 0x903A5A;    // u8, the window frames' last argument (block +0x7A)
constexpr U kPath = 0x904BA0;           // char[]: the save file name built with 0x664068
constexpr U kSaveName = 0x664068;       // "BISLPS%02X.DAT"
constexpr U kGameBlock = 0x9039E0;      // the live game block, 0x10B0 bytes (save-interchange.md)
constexpr U kBlockBytes = 0x10B0;
constexpr U kStaging = 0x92A0E0;        // Save_Staging
constexpr U kChecksum = 0x92A150;       // Save_Staging + 0x70, u16
constexpr U kFileBytes = 0x12B0;
constexpr U kClearBytes = 0x1C00;       // what 0x5883C0 zeroes from Save_Staging
constexpr U kScriptFlags = 0x9039A2;    // Field_ScriptFlags, u16
constexpr U kScriptFlagsSaved = 0x90412D;   // u8, block +0x74D
// 0x587DC0's field set-up.
constexpr U kScriptFlags2 = 0x905BA4;   // Field_ScriptFlags2
constexpr U kPartySet = 0x90412C;       // u8, PartySet_Select's current set
constexpr U kFieldState = 0x905D98;     // Field_State
constexpr U kSpriteCurrent = 0x937F88;  // Sprite_Current
constexpr U kObjTrio = 0x802D40;        // ObjTrio
constexpr U kFlow9039D6 = 0x9039D6;     // u8, cleared
// 0x5880E0, the new game.
constexpr U kNewGameByte = 0x9039F2;    // u8, cleared
constexpr U kAreaNumber = 0x904EFC;     // Game_AreaNumber, u16
constexpr U kCondFlags = 0x903F90;      // Cond_Flags: its first dword of every 8 bytes cleared
constexpr U kCondFlagsEnd = 0x904030;
// 0x5885D0, the loaded game applied.
constexpr U kMemberCount = 0x929EC0;    // Field_MemberCount, u8
constexpr U kCondPointer = 0x929ED0;    // u32
constexpr U kCounters = 0x903848;       // four MoveScript counters, u8
constexpr U kPosition = 0x8034E0;       // four dwords from the block's first 16 bytes
// 0x588800 and the growth rolls.
constexpr U kCharRecords = 0x903A70;    // stride 0xA4
constexpr U kCharStride = 0xA4;
constexpr U kGrowthTable = 0x667248;    // 8 x {lo, hi, n}
constexpr U kGrowthValues = 0x667260;   // u8 per roll sum
constexpr U kGrowth = 0x903640;         // 8 x 5 bytes, written then cleared
constexpr U kGameTask = 0x495800;       // task 0's body for a game
constexpr U kMenuStates = 0x929F02;     // 0x929F02, 0x929F03, 0x929F05 cleared
// The title menu's pieces.
constexpr U kPacketNext = 0x7E0670;     // Gfx_PacketNext
constexpr U kPieces = 0x66726C;         // 10-byte records: u16 u, v, w, h, clut
constexpr U kRowWidths[3] = {0x5888E8, 0x5888ED, 0x5888F2};   // the imm8 of 0x5888D0's three movs (DIV-0014)
// The save menu's slots.
constexpr U kSummaries = 0x905BC0;      // 16 x 0x1C: the slot summaries; +0x15 the directory index or 0xFF
constexpr U kSummaryBytes = 0x1C;
constexpr U kDirectory = 0x929F40;      // Save_Directory, 16 x 0x18: name, size at +0x14
constexpr U kDirectoryBytes = 0x18;
constexpr U kListPattern = 0x65289C;    // "BISLPS??.DAT"
constexpr U kSummaryAt = 0xCA0;         // the summary's offset in a save file
// The sound banks and the stream.
constexpr U kDatNames = 0x64F368;       // Dat_FileNames
constexpr U kDatFormat = 0x652894;      // "DAT\%s"
constexpr U kStreamKind = 0x6BDE40;     // u32: id >> 12 of the last stream
constexpr U kStreamData = 0x6BDE44;     // void *: the last stream's file
constexpr U kStreamTables = 0x6653B0;   // name tables per kind
constexpr U kStreamFormat = 0x666F9C;   // "SND\%s.DAT"
constexpr U kFadeCount = 0x6BDE54;      // Music_FadeCount
constexpr U kMusicTrack = 0x904131;     // Music_Track, u8
constexpr U kMusicBuffer = 0x7DE3C4;    // Music_Buffer, an IDirectSoundBuffer *
constexpr float kStreamVolume = 127.0f; // 0x42FE0000, pushed by 0x5879E0
// The CLUT strip.
constexpr U kClutStrip = 0x80F580;      // Gfx_ClutStrip
constexpr U kClutSource = 0x80B580;     // Gfx_ClutStrip - 0x4000
constexpr U kClutDirty = 0x937F90;      // Gfx_ClutStripDirty
// The inn.
constexpr U kFlagsA = 0x904654, kFlagsB = 0x904657;   // Flags_Test bit arrays
constexpr U kInnByte = 0x9045FB;        // u8, cleared below 0x1E
// The shop.
constexpr U kShopWindowByte = 0x929EC3; // u8 (the high byte of the word 0x929EC2)
constexpr U kMembers = 0x802E88;        // Field_Members: +0xC8 of each 0x14C working record
constexpr U kMemberStride = 0x14C;
constexpr U kDiscountItem = 0x1B;
constexpr U kChoiceCursor = 0x6BC881;   // u8: the save menu's three-way cursor
}  // namespace at

// --- Other groups' functions and Capcom's own, by address ---------------------
constexpr U kWindowFrame = 0x57CF60;    // group Y: (x, y, w, h, 0, colour)
constexpr U kWindowBox = 0x5762D0;      // group Y: (x, y, w, h)
constexpr U kWindowBack = 0x575690;     // group Y: (style byte)
constexpr U kWindowFrame5 = 0x574AB0;   // group Y: (x, y, w, h, colour byte)
constexpr U kSlotDraw = 0x576960;       // (slot, x, y, summary or 0)
constexpr U kSlotCursor = 0x573CE0;     // group Y: (x, y, w, h, flag, n)
constexpr U kItemPrice = 0x5749F0;      // group Y: (kind, item) -> price in ax
constexpr U kMenuYesNo = 0x5747D0;      // Menu_YesNo, group Y
constexpr U kDrawHand = 0x5905D0;       // Menu_DrawHand, group W
constexpr U kRecalcStats = 0x590660;    // Char_RecalcStats, group W
constexpr U kShopFlag = 0x5918E0;       // group W: (1) -> al
constexpr U kInventoryRemove = 0x591B60;   // (kind, item, count, 0)
constexpr U kInventoryAdd = 0x590BB0;   // Inventory_Add, group W; a fourth dword it does not read
constexpr U kFindFirst = 0x5B979A;      // the CRT's _findfirst
constexpr U kFindNext = 0x5B9867;       // the CRT's _findnext
constexpr U kTaskRestart = 0x5A9976;    // restarts the current task at an entry; never returns
constexpr U kVoicePlay = 0x5A7140;      // a stream of kind 1 or more: plays the WAV file image
constexpr U kVoiceIsPlaying = 0x5A7200;

struct Callees {
    // Files and the C runtime
    int (__cdecl* file_open)(const char*, int, int);          // File_Open (ours)
    int (__cdecl* file_size)(int);                             // File_Size (ours)
    int (__cdecl* file_seek)(int, int);                        // File_Seek (ours)
    unsigned (__cdecl* file_read)(int, void*, unsigned);       // File_Read (ours)
    void (__cdecl* file_close)(int);                           // File_Close (ours)
    void* (__cdecl* malloc)(unsigned);                         // Crt_malloc
    void (__cdecl* free)(void*);                               // Crt_free
    int (__cdecl* sprintf)(char*, const char*, ...);           // Crt_sprintf
    long (__cdecl* find_first)(const char*, void*);            // _findfirst
    int (__cdecl* find_next)(long, void*);                     // _findnext
    int (__cdecl* rand)();                                     // Rand
    // Sound
    void (__cdecl* load_bank)(unsigned, const void*, unsigned);   // Snd_LoadBank (ours)
    void (__cdecl* play_effect)(unsigned short);                   // Sound_PlayEffect (ours)
    void (__cdecl* music_stop)();                              // Music_Stop (ours)
    void (__cdecl* music_start)(const void*, unsigned, int);   // Music_Start (ours)
    void (__cdecl* music_volume)(float);                       // Music_SetVolume (ours)
    int (__cdecl* music_playing)();                            // Music_IsPlaying (this module)
    void (__cdecl* voice_play)(const void*);                   // 0x5A7140
    int (__cdecl* voice_playing)();                            // 0x5A7200
    // Characters and the shop
    unsigned char (__cdecl* clear_status)(unsigned, unsigned, unsigned);   // Char_ClearStatus (ours)
    void (__cdecl* recalc)(unsigned char*);                    // Char_RecalcStats (W)
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);   // Flags_Test (ours)
    int (__cdecl* party_count)(unsigned);                      // Party_Count (ours)
    unsigned (__cdecl* item_price)(unsigned, unsigned);        // 0x5749F0 (Y)
    unsigned char (__cdecl* shop_flag)(unsigned);              // 0x5918E0 (W)
    unsigned char (__cdecl* inventory_remove)(unsigned, unsigned, unsigned, unsigned);   // 0x591B60
    unsigned char (__cdecl* inventory_add)(unsigned, unsigned, unsigned, unsigned);      // Inventory_Add (W)
    // Windows and text
    void (__cdecl* window_frame)(int, int, int, int, int, unsigned);   // 0x57CF60 (Y)
    void (__cdecl* window_box)(int, int, int, int);            // 0x5762D0 (Y)
    void (__cdecl* window_back)(unsigned);                     // 0x575690 (Y)
    void (__cdecl* window_frame5)(int, int, int, int, unsigned);   // 0x574AB0 (Y)
    void (__cdecl* slot_draw)(unsigned, int, int, const unsigned char*);   // 0x576960
    void (__cdecl* slot_cursor)(int, int, int, int, unsigned, int);        // 0x573CE0 (Y)
    unsigned char (__cdecl* yes_no)();                         // Menu_YesNo (Y)
    const unsigned char* (__cdecl* msg_ptr)(unsigned);         // Msg_SystemPtr (ours)
    const unsigned char* (__cdecl* text_draw)(int, int, int, int, const unsigned char*);   // Text_DrawAt (ours)
    void (__cdecl* draw_hand)(int, int, int);                  // Menu_DrawHand (W)
    unsigned (__cdecl* auto_repeat)(unsigned);                 // Input_AutoRepeat (ours)
    // Primitives
    void (__cdecl* set_tile)(unsigned char*);                  // Gpu_SetTile (ours)
    void (__cdecl* set_sprt)(unsigned char*);                  // Gpu_SetSprt (ours)
    void (__cdecl* set_semi)(unsigned char*, unsigned);        // Gpu_SetSemiTrans (ours)
    void (__cdecl* set_draw_mode)(unsigned char*, int, int, unsigned, unsigned long);   // Gpu_SetDrawMode (ours)
    unsigned (__cdecl* get_tpage)(unsigned, unsigned, int, int);   // Gpu_GetTPage (ours)
    unsigned (__cdecl* get_clut)(int, int);                    // Gpu_GetClut (ours)
    void (__cdecl* commit)(unsigned, unsigned);                // Gfx_CommitPrim (ours)
    // The flow
    void (__cdecl* transition)(unsigned char);                    // Transition_Start (ours)
    void (__cdecl* member_sprite)(unsigned, unsigned);         // Field_MemberSprite (ours)
    void (__cdecl* set_animation)(unsigned char);                 // Sprite_SetAnimation (ours)
    void (__cdecl* init_characters)();                         // NewGame_InitCharacters
    void (__cdecl* scenario_start)(int);                       // Scenario_Start (ours)
    void (__cdecl* scenario_load)();                           // Scenario_Load (ours)
    void (__cdecl* party_load)(unsigned);                      // Field_PartyLoad (ours)
    void (__cdecl* party_select)(unsigned, unsigned);          // PartySet_Select (ours)
    int (__cdecl* load_done)();                                // File_LoadDone (ours)
    void (__cdecl* task_sleep)(int);                           // Task_Sleep
    void (__cdecl* task_restart)(U);                           // 0x5A9976
    // This module's own, called by each other
    int (__cdecl* list_files)();                               // Save_ListFiles
    int (__cdecl* read_file)(const char*, unsigned, int);      // Save_ReadFile
    void (__cdecl* read_summaries)();                          // Save_ReadSummaries
    void (__cdecl* draw_frame)();                              // TitleMenu_DrawFrame
    void (__cdecl* draw_rows)(unsigned);                       // TitleMenu_DrawRows
    void (__cdecl* draw_row)(int, unsigned, unsigned);         // TitleMenu_DrawRow
    void (__cdecl* draw_piece)(int, int, unsigned, unsigned);  // TitleMenu_DrawPiece
    void (__cdecl* draw_slots)(int, int, unsigned);            // SaveMenu_DrawSlots
    void (__cdecl* roll_growth)(unsigned);                     // NewGame_RollGrowth
    unsigned char (__cdecl* roll_sum)(unsigned, unsigned, unsigned);   // NewGame_RollSum
};
extern const Callees kOriginals;
extern Callees g;

// The bodies behind the two naked entries, which pass the ecx their caller
// left - the original's uninitialised local (docs/save-menu.md section 3).
extern "C" void __cdecl SaveMenu_LoadErrorBody(U ecx);
extern "C" int __cdecl SaveMenu_MusicPlayingBody(U ecx);

// BOF3X_SHADOW=save_menu: the start-up fuzz, save_menu_fuzz.cpp. Clones every
// original before SaveMenu_Inject patches it.
void SelfTest();

}  // namespace save_menu
