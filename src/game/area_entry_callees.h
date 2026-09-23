// Internal to area_entry.cpp and area_entry_fuzz.cpp: the addresses the five
// functions touch that have no name in symbols.toml, and every call they make
// - through pointers, so that the start-up fuzz can stand recording functions
// in for them, for the originals' copies and for ours alike. Three callees
// are ours in this module (Party_SetUpMembers and Party_SwapMembers under
// Party_DropIn, ObjTrio_SwapFields under Party_SwapMembers); through the
// pointers each function is still tested alone. docs/area-entry.md.
#pragma once

#include <cstdint>

namespace area_entry {

// --- Addresses without a name --------------------------------------------
// The pending area change Field_ChangeArea writes (PSX 0x80143F10..1C;
// window_task_callees.h has the same four).
constexpr std::uint32_t kPendingArea = 0x937F82;    // u16
constexpr std::uint32_t kPendingX = 0x903860;       // s32, 16.16
constexpr std::uint32_t kPendingZ = 0x90384C;       // s32, 16.16
constexpr std::uint32_t kPendingFlags = 0x905B88;   // u8
// Bits 4-6 of a link's flags byte: the PSX scratchpad's 0x1F800001, which
// the PC keeps at DamageScratch + 1. What reads it is not established.
constexpr std::uint32_t kLinkHigh = 0x903851;
// Party_DropIn's three scratch lists, the PSX scratchpad's 0x1F800000,
// +4 and +8: per wanted position the record's +0x2C byte, per wanted
// position the record slot found, per record a copy of the party list that
// is struck out (0xFF) as records are taken. Four bytes apart, so a list of
// more than four overruns the next, as on the PSX.
constexpr std::uint32_t kPickColumn = 0x903850;
constexpr std::uint32_t kPickSlot = 0x903854;
constexpr std::uint32_t kPickFree = 0x903858;
// The 3-byte party list (Party_Join's, move_cmds_callees.h kPartyLists).
constexpr std::uint32_t kPartyList = 0x904062;
// The eight 0xA4-byte character records MoveScript_EffectState indexes by
// member (its values are 0..7), copied to a party record's +0x80 (PSX
// 0x80144964); symbols.toml line 84's save-record block.
constexpr std::uint32_t kCharacterRecords = 0x903A70;
// The palettes Sprite_LoadPalette fills: 64 bytes per record's +5 byte
// (PSX 0x8002D600).
constexpr std::uint32_t kPalettes = 0x80D380;

// ObjTrio's records (the party members' sprites) and the fields read here.
constexpr unsigned kRecord = 0x14C;
constexpr unsigned kRecColumn = 0x2C, kRecMember = 0x89, kRecPalette = 5;
constexpr unsigned kRecContext = 0x124, kRecScript = 0x130, kRecCharacter = 0x80, kRecCharacterIndex = 0x148;
constexpr unsigned kCharacterBytes = 0xA4;

struct Callees {
    unsigned char (__cdecl* flags_test)(const unsigned char*, unsigned);        // Flags_Test (ours)
    void (__cdecl* classify)();                                                 // Area_ClassifyPending (ours)
    void (__cdecl* pick_music)(unsigned, unsigned, unsigned);                   // Area_PickMusic (ours)
    void (__cdecl* kind2_place)(unsigned char);                                 // Kind2_Place (ours)
    void (__cdecl* member_clear)(unsigned);                                     // Member_ClearState (ours)
    void (__cdecl* context_reset)(unsigned char*);                              // ScriptContext_Reset (ours)
    void (__cdecl* load_palette)(unsigned short*, unsigned);                    // Sprite_LoadPalette (ours)
    void (__cdecl* member_sprite)(unsigned, unsigned);                          // Field_MemberSprite (ours)
    void (__cdecl* set_up)(unsigned, const unsigned char*);                     // Party_SetUpMembers (this module)
    void (__cdecl* swap_members)(unsigned, unsigned, unsigned);                 // Party_SwapMembers (this module)
    void (__cdecl* swap_fields)(unsigned, unsigned, unsigned);                  // ObjTrio_SwapFields (this module)
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=area_entry: the start-up fuzz, area_entry_fuzz.cpp. Clones
// every original before AreaEntry_Inject patches it.
void SelfTest();

}  // namespace area_entry
