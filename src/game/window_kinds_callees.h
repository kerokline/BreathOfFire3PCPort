// Internal to window_kinds.cpp and window_kinds_fuzz.cpp: every call the
// twelve functions make, through pointers, so that the start-up fuzz can
// stand recording functions in for them - for the originals' copies and for
// ours alike.
//
// Three of the twelve dispatch through a table they build on their OWN stack
// (`mov [esp + k], imm32`, then `call [esp + eax * 4]`): there is no table in
// memory to swap, so the copies carry the handlers' absolute addresses in
// their immediates and the fuzz re-aims those in the copy
// (window_kinds_fuzz.cpp, PatchImm), as window_task_fuzz.cpp does.
// docs/window_kinds.md.
#pragma once

#include <cstdint>

namespace window_kinds {

namespace at {

constexpr std::uint32_t kRecords = 0x803160;       // 22 window records of 0x24 bytes (WindowRecords)
constexpr std::uint32_t kRecordsEnd = 0x803478;
constexpr std::uint32_t kCurrent = 0x905B84;       // dword: the record the window layer is running

// The battle message ring (docs/battle_windows.md section 2): 16 entries of
// 8 bytes, +0 flags (bit 0 waits for a button, bit 1 on the timer), +1 the
// timer (0xFF for none), +4 the text. Read index 0x93C2A0, write index
// 0x93C2A1.
constexpr std::uint32_t kMsgHead = 0x93C2A0;
constexpr std::uint32_t kMsgTail = 0x93C2A1;
constexpr std::uint32_t kMsgRing = 0x93C2C0;
constexpr std::uint32_t kMsgOpen = 0x939F60;       // u8: 1 while the message window is out (7 readers in the battle code)

// The banner pool (docs/battle_misc.md section 1.1): 8 entries of 0xC bytes,
// +1 the kind, +4 the text, +0xA the byte the banner window draws it in.
constexpr std::uint32_t kBannerPool = 0x93B8E0;
constexpr std::uint32_t kBannerText = 0x93B8E4;    // kBannerPool + 4
constexpr std::uint32_t kBannerColour = 0x93B8EA;  // kBannerPool + 0xA
constexpr std::uint32_t kBannerCurrent = 0x93B8C0; // dword: the entry BattleBanner_Dispatch last visited
constexpr std::uint32_t kBannerMask = 0x904AE9;    // u8: the kinds BattleBanner_Dispatch has seen this frame

constexpr std::uint32_t kScratch = 0x903850;       // DamageScratch's byte 0; the gauge keeps a value there

}  // namespace at

// The callees with no name of their own in symbols.gen.h.
constexpr std::uint32_t kNop = 0x437CC0;           // a bare `ret` (symbols.toml, Battle_ElementAffinity's note)
// Record handler 4's six kinds, by record byte +2. Slots 2 and 3 are not
// immediates: the original stores eax (0) there - two holes in the table.
// 0x598570 and 0x5986C0 are group CD's (round eight); 0x597FA0 and 0x5984B0
// are in no group and unread. All four stay Capcom's here.
constexpr std::uint32_t kResultKinds[6] = {0x597FA0, 0x5984B0, 0, 0, 0x598570, 0x5986C0};

using Handler = void (__cdecl*)();

struct Callees {
    // ours, other modules'
    void (__cdecl* message_box)(int, int);                                          // BattleWin_DrawMessageBox
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);  // Text_DrawAt
    void (__cdecl* draw_message)();                                                 // BattleWin_DrawMessage
    unsigned char (__cdecl* msg_advance)();                                         // BattleMsg_Advance
    void (__cdecl* free_current)();                                                 // Window_FreeCurrent
    // the three stack-built tables, as the originals' immediates
    Handler banner[3];    // BattleWin_BannerRun, by record byte +3
    Handler message[4];   // BattleWin_MessageRun, by record byte +3
    Handler result[6];    // Window_Handler4Kinds, by record byte +2 (two nulls)
};

extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz (window_kinds_fuzz.cpp): clones all twelve with every
// call out re-aimed at a recording stand-in and every stack-built table's
// immediates re-aimed in the copy, runs ours against the clones from the same
// random state, and ends the process through bof3::Fatal on any difference.
void SelfTest();

}  // namespace window_kinds
