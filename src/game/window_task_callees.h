// Internal to window_task.cpp and window_task_fuzz.cpp: every call the
// twenty-four functions make, through pointers, so that the start-up fuzz can
// stand recording functions in for them - for the originals' copies and for
// ours alike. Most of the callees are ours in this module; through the
// pointers each function is tested alone.
//
// The dispatch tables of this family are NOT in memory: four of the functions
// build them on their own stack out of `mov [esp + k], imm32` and then
// `call [esp + eax * 4]`. So there is no table to swap - the copies carry the
// absolute addresses in their immediates, and the fuzz re-aims those in the
// copy (window_task_fuzz.cpp, PatchImm). docs/window-task.md.
#pragma once

#include <cstdint>

namespace window_task {

namespace at {

// The window records and the one the layer is running.
constexpr std::uint32_t kRecords = 0x803160;        // 22 records of 0x24 bytes (symbols.toml block WindowRecords)
constexpr std::uint32_t kRecordsEnd = 0x803478;     // one past the last
constexpr std::uint32_t kCurrent = 0x905B84;        // dword: the record Field_RunTaskRecords set
constexpr std::uint32_t kPass = 0x802D20;           // u8: the pass Field_RunTaskRecords starts from

// MsgBoxState (PC 0x7DEE40 = PSX 0x8014909C, symbols.toml block MsgBoxState).
constexpr std::uint32_t kListText = 0x7DEE50;       // +0x10, a dword the list draws from
constexpr std::uint32_t kListSet = 0x7DEE65;        // +0x25, u8: the list's geometry set
constexpr std::uint32_t kListCount = 0x7DEE66;      // +0x26, s8: the last item's index
constexpr std::uint32_t kListCursor = 0x7DEE67;     // +0x27, s8: the item the cursor is on
constexpr std::uint32_t kPlacement = 0x7DEE6E;      // +0x2E, u8: the 0x0C control code's argument (PSX 0x801490CA)
constexpr std::uint32_t kListRows = 0x7DEE6F;       // +0x2F, a byte per list item

// The placement tables, all indexed by kPlacement.
constexpr std::uint32_t kBoxXY = 0x66ADD8;          // 8 x (u8 x, u8 y), the wide box
constexpr std::uint32_t kBoxAltXY = 0x66ADE8;       // 8 x (u8 x, u8 y), the narrow box (bit 0x40)
constexpr std::uint32_t kSetXY = 0x66ADFC;          // 8-byte stride, u16 x at +0, u16 y at +2, by kListSet
constexpr std::uint32_t kPlaceXY = 0x66AE10;        // MsgBox_PlacementTable, 4-byte stride, u16 x, u16 y

// Field_ChangeArea's cells. The PSX keeps all of these in one block at
// 0x80143F10..1F; the port scattered them (docs/window-task.md section 2).
constexpr std::uint32_t kPendingArea = 0x937F82;    // u16 (PSX 0x80143F10)
constexpr std::uint32_t kPendingX = 0x903860;       // s32, 16.16 (PSX 0x80143F14)
constexpr std::uint32_t kPendingZ = 0x90384C;       // s32, 16.16 (PSX 0x80143F18)
constexpr std::uint32_t kPendingFlags = 0x905B88;   // u8 (PSX 0x80143F1C) - Gfx_BufferIndex is the byte after it
constexpr std::uint32_t kPendingKind = 0x937F98;    // u8 (PSX 0x80143F1D, docs/field-modes.md section 4)
constexpr std::uint32_t kAreaTrack = 0x904CD0;      // u8: the track the next area wants (PSX 0x80143F1F)
constexpr std::uint32_t kMusicTrack = 0x904131;     // u8: the track Music_Play last started, 0xFF none

constexpr std::uint32_t kAreaKinds = 0x66ADC0;      // 11 u16 area numbers Area_ClassifyPending answers 0xB for
constexpr std::uint32_t kAreaKindsEnd = 0x66ADD6;
constexpr std::uint32_t kZoneLists = 0x668D80;      // an 8-byte zone record list per area (field_event_callees.h)
constexpr std::uint32_t kMusicSets = 0x669A48;      // 8-byte records: const u8 *list at +0, u8 count at +4
constexpr std::uint32_t kClutRow = 0x903A5A;        // s8: the window colour Window_DrawFrame builds a CLUT id from

}  // namespace at

// The callees with no name of their own in symbols.gen.h: called by address.
// MsgBox_FrameTask is group H's - its entry and body are theirs; we only call
// it, and only through this address (as title_states.cpp calls
// Field_ModeDispatch).
constexpr std::uint32_t kMsgBoxFrameTask = 0x4977F0;   // PSX 0x80150508
constexpr std::uint32_t kListSetUp = 0x596330;         // kind 1's state 0 calls it; unread
constexpr std::uint32_t kListDraw = 0x596090;          // kind 1's frame; unread
constexpr std::uint32_t kListCursorDraw = 0x596120;    // kind 1's frame, a tail jump; unread
constexpr std::uint32_t kSetDraw = 0x596020;           // kind 2's frame; unread
constexpr std::uint32_t kSetCursorDraw = 0x5960D0;     // kind 2's frame, a tail jump; unread

// The eight handlers of Field_RunTaskRecords' local table that are not ours:
// entry 0 is Window_Run, entries 1..8 are the other record kinds, unread.
constexpr std::uint32_t kRecordHandlers[9] = {
    0x5954B0, 0x596530, 0x5968E0, 0x596FA0, 0x597F60, 0x598890, 0x599B50, 0x59B220, 0x59CB00,
};

using Handler = void (__cdecl*)();

struct Callees {
    // ours, in this file
    void (__cdecl* classify)();                                        // Area_ClassifyPending
    void (__cdecl* pick_music)(unsigned, unsigned, unsigned);          // Area_PickMusic
    const unsigned char* (__cdecl* zone_at)(unsigned, unsigned, unsigned);  // Area_ZoneAtIn
    void (__cdecl* draw_frame)(int, int, int, int);                    // Window_DrawFrame
    void (__cdecl* draw_outline)(int, int, int, int);                  // Window_DrawOutline
    void (__cdecl* draw_line)(int, int, int, int, unsigned);           // Window_DrawLine
    void (__cdecl* free_current)();                                    // Window_FreeCurrent
    // other modules' and Capcom's
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);  // Flags_Test (event_script.cpp)
    unsigned char (__cdecl* msgbox_frame)();                           // MsgBox_FrameTask, group H's
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);  // Gpu_SetDrawMode
    void (__cdecl* commit)(unsigned, unsigned);                        // Gfx_CommitPrim
    void (__cdecl* set_tile)(unsigned char*);                          // Gpu_SetTile
    void (__cdecl* set_poly_gt4)(unsigned char*);                      // Gpu_SetPolyGT4
    void (__cdecl* set_line_f4)(unsigned char*);                       // Gpu_SetLineF4
    void (__cdecl* set_line_f2)(unsigned char*);                       // Gpu_SetLineF2
    void (__cdecl* set_semi)(unsigned char*, unsigned);                // Gpu_SetSemiTrans
    unsigned (__cdecl* get_tpage)(unsigned, unsigned, int, int);       // Gpu_GetTPage
    unsigned (__cdecl* get_clut)(int, int);                            // Gpu_GetClut
    // unread, called only by the list and name kinds
    void (__cdecl* list_set_up)();
    void (__cdecl* list_draw)();
    void (__cdecl* list_cursor)();
    void (__cdecl* set_draw)();
    void (__cdecl* set_cursor)();
    // the four stack-built dispatch tables, as the originals' immediates
    Handler records[9];      // Field_RunTaskRecords, by record byte +1
    Handler kinds[3];        // Window_Run, by record byte +2
    Handler kind0[5];        // Window_Kind0States, by record byte +3
    Handler kind1[5];        // Window_Kind1States
    Handler kind2[5];        // Window_Kind2States
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (window_task_fuzz.cpp): clones all twenty-four with every
// call out re-aimed at a recording stand-in and every stack-built table's
// immediates re-aimed in the copy, runs ours against the clones from the same
// random state, and ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace window_task
