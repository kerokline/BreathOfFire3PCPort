// Internal to battle_misc.cpp and battle_misc_fuzz.cpp: the addresses the
// group's functions touch that have no name in symbols.toml, and every call
// they make - through pointers, so that the start-up fuzz can stand recording
// functions in for them, for the originals' copies and for ours alike. Calls
// into other groups' functions (the round's other rows, docs/
// takeover-queue-round7.md) and into unnamed ones go through raw addresses and
// are never bound here. Four callees are ours in this module (Str_CopyN,
// BattleBanner_Set, Battle_InitActorContext and the banner handlers' table,
// which is the original's); through the pointers each function is still tested
// alone. docs/battle_misc.md.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace battle_misc {

using U = std::uint32_t;

// --- Addresses without a name --------------------------------------------------
// The menu cursor's pulse: a byte counter stepped by 2 and its direction flag
// (PSX 0x80146304 / 0x80146300).
constexpr U kPulse = 0x904AC8;
constexpr U kPulseDown = 0x904AC4;
// The actor sound cue table: u16, 3 per cue row, indexed by the actor record's
// word +0x2C plus cue * 3 (PSX 0x801EAF80).
constexpr U kCueTable = 0x64E3BC;
// The current enemy object (0x93B960 + n * 0x128), PSX 0x801EB458.
constexpr U kCurrentEnemy = 0x939AD8;
// The enemy objects: 8 of 0x128 bytes; the working record is at +0x80
// (EnemyWorkingRecords 0x93B9E0). The party's are ObjTrio's, 3 of 0x14C.
constexpr U kEnemyObjects = 0x93B960;
constexpr U kEnemyStride = 0x128;
constexpr U kPartyStride = 0x14C;
// The actor contexts: records of 0x84 bytes, the first 0x80 a copy of an
// object's, +0x80 the object (PSX 0x801EC2A0, 0x78 bytes, the object at +0x74).
constexpr U kContexts = 0x93A000;
constexpr U kContextStride = 0x84;
// The per-set sound lists: 256 pointers, each to dwords (key << 16 | id)
// ending in -1; the id + 0x1000 is a Sound_LoadStream id.
constexpr U kSoundLists = 0x6565BC;
// The actors' "acting" bits: a u16, bit n for actor n (PSX 0x801463BE).
constexpr U kActorBits = 0x904B82;
// Window record 16 of the window task's records (0x803160 + 16 * 0x24) and the
// three after it: the item window. Record 16's fields the two set-ups write.
constexpr U kItemWindow = 0x8033A0;
constexpr U kItemWindowPage = 0x8033AB;      // byte: the inventory page
constexpr U kItemWindowCursor = 0x8033AC;    // byte: the cursor's item index
constexpr U kItemWindowExtra = 0x80340C;     // three bytes 0x80340C..E
constexpr U kWindowCurrent = 0x905B84;       // the window task's current record
constexpr U kItemActor = 0x929F06;           // byte: the actor the item window is for
constexpr U kActingActor = 0x939EC4;         // pointer: the acting actor's context, its byte +5 the actor
constexpr U kActorPages = 0x9045FC;          // 3 bytes per actor: page, a word's byte, cursor
constexpr U kCommandSource = 0x939FA0;       // pointer: its dword +0x10 holds the command flags
constexpr U kPartyPages = 0x904605;          // 3 bytes: page, a byte, cursor (the party's item window)
// The banner pool: 8 entries of 0xC (PSX 0x801EB460): +0 active, +1 kind, +2 a
// byte, +3 the layer (0 or 1), +4 the text, +8 the timer (a u16, 0xFF forever),
// +0xA a byte. The current entry, and the kinds seen this frame.
constexpr U kBanners = 0x93B8E0;
constexpr U kBannerCurrent = 0x93B8C0;
constexpr U kBannerKinds = 0x904AE9;
constexpr U kRetOnly = 0x437CC0;             // a bare ret: kinds 0, 3 and 4
constexpr U kBannerKind1 = 0x44A740;         // kind 1's tick (timer, then NoneOfKind(1))
constexpr U kBannerKind2 = 0x44A7B0;         // kind 2's tick (timer, then NoneOfKind(4))
// The message queue: 16 entries of 8 (+0, +1 bytes, +4 a dword), its write
// and read indices (PSX 0x801EB520, 0x801EC298 / 0x801EBE74).
constexpr U kQueue = 0x93C2C0;
constexpr U kQueueWrite = 0x93C2A1;
constexpr U kQueueRead = 0x93C2A0;
// The banner's text buffer, the message table (localised strings: entry 1 is
// also the name suffix) and the suffix switch.
constexpr U kBannerText = 0x904EC0;
constexpr U kMessages = 0x669DE0;
constexpr U kNameSuffix = 0x669DE4;
constexpr U kSuffixOn = 0x904B7A;
// The enemy AI rows: 0x8C bytes per script (the enemy's byte +0xF0), four rows
// of 16 checked per turn (PSX 0x800E407C, 0x88 per script).
constexpr U kAiScripts = 0x8C5600;
constexpr U kActingKind = 0x904B35;          // byte 1 of 0x904B34 (PSX 0x80146371)

// The named data, by the address its symbols.gen.h macro names.
namespace at {
inline U Of(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
inline U Sprite_CurrentAt() { return Of(&Sprite_Current); }
inline U Field_StateAt() { return Of(&Field_State); }
inline U ObjTrioAt() { return Of(ObjTrio); }
inline U Sprite_ObjectsAt() { return Of(Sprite_Objects); }
}  // namespace at

// Every callee returns eax whole (U), since several of the group's functions
// pass a callee's eax on to their callers.
struct Callees {
    U (__cdecl* alloc_context)(U, U);           // 0x435180 (group BB): a context slot, al
    U (__cdecl* actor_out)(U);                  // 0x4456C0 (group BB): al 1 when the actor is absent or bit 0x40
    U (__cdecl* set_tint)(U, U, U, U, U);       // Sprite_SetTint 0x454CC0 (group BG)
    U (__cdecl* play_effect)(U);                // Sound_PlayEffect 0x587740 (ours)
    U (__cdecl* load_stream)(U);                // Sound_LoadStream 0x587910 (ours)
    U (__cdecl* item_list)(U, U, U);            // 0x591E50 (group BD): an actor's inventory page
    U (__cdecl* item_usable)(U, U, U);          // 0x57DA70 (group BD): al 0 when usable
    U (__cdecl* free_window)();                 // Window_FreeCurrent 0x59E310 (ours)
    U (__cdecl* ai_condition)(U);               // 0x44B320 (unnamed): EnemyAI_CondPartyFlag
    U (__cdecl* ai_row_done)(U, U);             // 0x44B2C0 (group BE): EnemyAI_RowDone
    U (__cdecl* ai_apply)(U, U);                // 0x44B3A0 (unnamed): EnemyAI_ApplyAction
    U (__cdecl* ai_set_done)(U, U, U);          // 0x44B2E0 (unnamed): EnemyAI_SetRowDone
    U (__cdecl* ai_finish)();                   // 0x44B920 (unnamed)
    U (__cdecl* update_screen_a)();             // Sprite_UpdateScreenA 0x57B830 (ours)
    U (__cdecl* update_screen_slot)();          // Sprite_UpdateScreenSlot 0x588F00 (group BG)
    U (__cdecl* init_context)();                // Battle_InitActorContext 0x446BD0 (this module)
    U (__cdecl* copy_n)(U, U, U);               // Str_CopyN 0x5171A0 (this module)
    U (__cdecl* banner_set)(U, U, U, U, U, U);  // BattleBanner_Set 0x44A6E0 (this module)
    U (__cdecl* banner[5])();                   // the dispatch's table: ret, kind 1, kind 2, ret, ret
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_misc: the start-up fuzz, battle_misc_fuzz.cpp. Clones
// every original before BattleMisc_Inject patches it.
void SelfTest();

}  // namespace battle_misc
