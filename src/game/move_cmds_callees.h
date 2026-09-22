// Internal to move_cmds.cpp and move_cmds_fuzz.cpp: the addresses the
// movement commands touch that have no name in symbols.toml, and every call
// the fifteen functions make - through pointers, so that the start-up fuzz can
// stand recording functions in for them, for the originals' copies and for
// ours alike. Several callees are ours in this module (Math_Ratan2 under
// Party_MoveMember, the object kind, the detach and the move under the attach
// command, the handle's position under that move); through the pointers each
// function is still tested alone. docs/move-cmds.md.
#pragma once

#include <cstdint>

namespace move_cmds {

// --- Addresses without a name --------------------------------------------
// The kind-2 object's per-direction step, two dwords (x, z) per direction:
// MoveCmd_MoveKind2 reads [dir * 8] and [dir * 8 + 4]. The PSX 0x80182438
// (FUN_801C6DF4's -0x7FE7DBC8), 0xD4 bytes before Field_DirectionSteps.
constexpr std::uint32_t kKind2Steps = 0x6696DC;
// The dword MoveCmd_HandlePosition keeps Sprite_Current in while it points
// Sprite_Current at the handle's object - the PSX scratchpad's 0x1F800010,
// which the PC also names MapView_CornerPtr for MapView_Build's use of it.
constexpr std::uint32_t kSavedSprite = 0x92A0C0;
// The two 3-byte party lists (Party_Join's and Party_Count's, field_event.cpp)
// and the flag byte before them.
constexpr std::uint32_t kPartyFlags = 0x904061;
constexpr std::uint32_t kPartyLists = 0x904062;
// MoveScript_Variable's sources that have no name of their own.
constexpr std::uint32_t kCounters = 0x903848;    // the A ops' four counter bytes (variables 3..6)
constexpr std::uint32_t kFlagsPtr = 0x929ED0;    // the chapter's flag dword's address (variable 2)
constexpr std::uint32_t kNameIndex = 0x802DC9;   // ObjTrio +0x89 (variable 11)
constexpr std::uint32_t kStoryFlags = 0x904030;  // Cond_Flags + 0xA0's first dword (variable 12)
// Math_Ratan2's two doubles.
constexpr std::uint32_t kScaleAt = 0x5C4658;     // 2048.0
constexpr std::uint32_t kInversePiAt = 0x5C4650; // 0.3184713375796178, 1 / 3.14

// The kind-2 object's fields (Sprite_Kind2 + n).
constexpr unsigned kK2Rise = 0x14, kK2X = 0x34, kK2Z = 0x38, kK2Height = 0x3E;
constexpr unsigned kK2Context = 0x80, kK2Script = 0x83, kK2Speed = 0x84, kK2Flags = 0x85, kK2Steps = 0x87;

struct Callees {
    long (__cdecl* elevation)(long, long);                                  // AreaMap_Elevation
    int (__cdecl* ratan2)(float, float);                                    // Math_Ratan2 (ours)
    void (__cdecl* context_reset)(unsigned char*);                          // ScriptContext_Reset
    void (__cdecl* set_elevation)(int);                                     // MapView_SetElevation
    void (__cdecl* attach_offset)(long*, unsigned char);                    // MoveCmd_AttachOffset
    int (__cdecl* object_kind)();                                           // MoveScript_ObjectKind (ours)
    void (__cdecl* detach)();                                               // MoveCmd_Detach (ours)
    void (__cdecl* attach_move)(unsigned char);                             // MoveCmd_AttachMove (ours)
    long* (__cdecl* handle_position)(long*, unsigned char, unsigned char);  // MoveCmd_HandlePosition (ours)
    void (__cdecl* partyset_load)(unsigned, unsigned, unsigned, unsigned);  // PartySet_Load
    unsigned char (__cdecl* party_join)(unsigned);                          // Party_Join
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=move_cmds: the start-up fuzz, move_cmds_fuzz.cpp. Clones every
// original before MoveCmds_Inject patches it.
void SelfTest();

}  // namespace move_cmds
