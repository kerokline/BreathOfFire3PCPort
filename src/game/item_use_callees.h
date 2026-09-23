// Internal to item_use.cpp and item_use_fuzz.cpp: the addresses the field
// menu's item effects touch that have no name in symbols.toml, and every call
// they make - through pointers, so that the start-up fuzz can stand recording
// functions in for them, for the originals' copies and for ours alike. Most
// callees are ours in this module (the character-stat helpers under the
// handlers); through the pointers each function is still tested alone.
// docs/item-use.md.
//
// Three dispatches are not calls to a named function:
//   - ItemUse_Dispatch calls through ItemUse_Handlers 0x658DB0, a table in
//     .data; ours reads it as the original does, and the fuzz swaps its 34
//     entries for recorders.
//   - MsgBox_ChoiceCommit / MsgBox_MenuCommit call through the area
//     descriptor's +0x34 (a table of handlers per choice id); the fuzz points
//     Area_Descriptors at descriptors of its own.
//   - MsgBox_SystemChoice 0x498A30 (Capcom's) reaches the sixteen
//     MsgBox_SysChoice8x through a table on its own stack; they take no
//     arguments and call nothing, so each is tested alone.
#pragma once

#include <cstdint>

namespace item_use {

namespace at {

constexpr std::uint32_t kCharRecords = 0x903A70;   // CharacterRecords (a block), stride 0xA4
constexpr std::uint32_t kCharStride = 0xA4;
constexpr std::uint32_t kWorkRecords = 0x802DC0;   // member i's working copy of its character record,
constexpr std::uint32_t kWorkStride = 0x14C;       // 0x14C apart (PartyWorkingRecords' stride)
// A record's fields (the PSX record's offsets + 4, docs/item-use.md section 2).
constexpr unsigned kStatus = 0x10;      // u16 status bits
constexpr unsigned kHp = 0x18;          // u16
constexpr unsigned kAp = 0x1A;          // u16
constexpr unsigned kHpScale = 0x1E;     // u8, Char_RecalcStats' max-HP scale
constexpr unsigned kMaxHp = 0x20;       // u16, effective
constexpr unsigned kMaxAp = 0x22;       // u16, effective
constexpr unsigned kRecordBytes = 0xA4;

constexpr std::uint32_t kPartyLists = 0x904062;    // Party_Count's 3-byte lists (move_cmds_callees.h)
constexpr std::uint32_t kMemberRecord = 0x66972C;  // u8 per party entry: its record index (MoveScript_EffectState)
constexpr std::uint32_t kItemFlags = 0x656B38;     // NameTable_Consumables + 0x10: each record's flags byte
constexpr std::uint32_t kItemStride = 22;
constexpr std::uint32_t kStoryFlags = 0x904030;    // Cond_Flags + 0xA0 (title-states.md section 3)
// ItemUse_FaerieTiara's cells.
constexpr std::uint32_t kMenuState = 0x929F01;     // u8, set to 0xA
constexpr std::uint32_t kTiaraCount = 0x905B61;    // u8, 0xB9 less the clear flags counted
constexpr std::uint32_t kLeaderXZ = 0x802D74;      // two dwords (ObjTrio +0x34 / +0x38)
constexpr std::uint32_t kReturnPoint = 0x904148;   // two dwords, then the area word at 0x904150
constexpr std::uint32_t kWaterJug = 0x90405F;      // u8, set to 0xF0
// The message box (msgbox_callees.h names the same cells).
constexpr std::uint32_t kSubState = 0x7DEE41;      // u8
constexpr std::uint32_t kMessage = 0x7DEE48;       // u16
constexpr std::uint32_t kColorHigh = 0x7DEE58;     // u8
constexpr std::uint32_t kChoiceId = 0x7DEE64;      // u8
constexpr std::uint32_t kCursor = 0x7DEE67;        // s8 as the system choices read it
constexpr std::uint32_t kWindow0State = 0x803163;  // u8
constexpr std::uint32_t kWindow1State = 0x803187;  // u8
constexpr std::uint32_t kSysMessages = 0x658F08;   // u16 pairs, 4 bytes per system choice id
constexpr std::uint32_t kSysFlag = 0x929F0B;       // u8, choice 0x80's answer
constexpr std::uint32_t kSysCounter = 0x903848;    // u8, MoveScript variable 3 (move_cmds' kCounters)
// Input_AutoRepeat's two words.
constexpr std::uint32_t kRepeatTimer = 0x7E1BE0;   // u16
constexpr std::uint32_t kRepeatLatch = 0x7E01B8;   // u16

}  // namespace at

constexpr std::uint32_t kSystemChoice = 0x498A30;  // MsgBox_SystemChoice, left Capcom's (docs/item-use.md)
constexpr std::uint32_t kInventoryAdd = 0x590BB0;  // Inventory_Add, Capcom's; called with a fourth dword it does not read

using Handler = unsigned char (__cdecl*)(unsigned, unsigned);
using Choice = void (__cdecl*)();

struct Callees {
    unsigned char (__cdecl* heal_hp)(unsigned, unsigned, unsigned);                  // Char_HealHp (ours)
    unsigned char (__cdecl* heal_ap)(unsigned, unsigned, unsigned);                  // Char_HealAp (ours)
    unsigned char (__cdecl* clear_status)(unsigned, unsigned, unsigned);             // Char_ClearStatus (ours)
    unsigned char (__cdecl* add_cap999)(unsigned short*, unsigned);                  // Stat_AddCap999 (ours)
    unsigned char (__cdecl* add_cap99)(unsigned char*, unsigned);                    // Stat_AddCap99 (ours)
    short (__cdecl* add_clamped)(unsigned short*, unsigned);                         // Stat_AddClamped (ours)
    int (__cdecl* party_count)(unsigned);                                            // Party_Count (field_event.cpp)
    int (__cdecl* rand)();                                                           // Rand, the CRT's
    void (__cdecl* recalc)(unsigned char*);                                          // Char_RecalcStats, Capcom's
    unsigned char (__cdecl* inventory_add)(unsigned, unsigned, unsigned, unsigned);  // Inventory_Add, four pushed
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);             // Flags_Test (event_script.cpp)
    void (__cdecl* system_choice)();                                                 // MsgBox_SystemChoice, Capcom's
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=item_use: the start-up fuzz, item_use_fuzz.cpp. Clones every
// original before ItemUse_Inject patches it.
void SelfTest();

}  // namespace item_use
