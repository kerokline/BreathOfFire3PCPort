// Internal to event_objs.cpp and event_objs_fuzz.cpp: the addresses the
// event script's object ops touch that have no name in symbols.toml, and
// every call the 22 functions make - through pointers, so that the start-up
// fuzz can stand recording functions in for them, for the originals' copies
// and for ours alike. Several callees are ours in this module; through the
// pointers each function is still tested alone. docs/event-objs.md.
#pragma once

#include <cstdint>

namespace event_objs {

// --- Addresses without a name --------------------------------------------
// Row 15 of Gfx_ClutStrip (0x80F580 + 15 * 256 * 2): 64 bytes of CLUT per
// Sprite_Current +5. Sprite_ShadeFadeBegin sets bit 15 of its words.
constexpr std::uint32_t kShadeClut = 0x811380;
// Row 15 of Gfx_ClutStripSource: where Sprite_ShadeFadeStep reloads a
// palette from (Sprite_LoadPalette's dst; docs/field-event.md section 3).
constexpr std::uint32_t kShadeSource = 0x80D380;
// The per-direction step, two dwords (x, z) per direction - move_cmds'
// kKind2Steps: (-2048, -2048), (0, -2048), (1024, -1024), (2048, 0), ... The
// half steps of directions 2 and 6 are why their jumps take twice the frames.
constexpr std::uint32_t kJumpSteps = 0x6696DC;
// The party list Party_Count counts (field_event.cpp; move_cmds' kPartyLists).
constexpr std::uint32_t kPartyList = 0x904062;
// The actor records, 0xA4 bytes each (docs/field-event.md section 2):
// Field_ActorStates is +0x10 of the first, the equipment bytes +0x12..+0x17.
constexpr unsigned kRecordSize = 0xA4;

// --- Callees owned elsewhere or still Capcom's (raw addresses) ------------
// 0x5725C0 is group M's (field_misc.cpp, round 6): MapView_CheckHeightScale,
// AreaMap_Slope(x, z, direction), MapView_HeightScale = 0; the slope's long
// back and DamageScratch's first byte 1 on a slope, else 0. The direction is
// read as its low byte.
constexpr std::uint32_t kSlopeAt = 0x5725C0;
// Capcom's, unread beyond their use here (none reached by the shop route):
constexpr std::uint32_t kCellsAllWide = 0x535490;   // AreaMap_CellsAll's other footprint (3 x 3)
constexpr std::uint32_t kCellsNoneWide = 0x535D60;  // AreaMap_CellsNone's
constexpr std::uint32_t kTurnProbe = 0x535610;      // (x, z, 0, height word): al, the way is blocked
constexpr std::uint32_t kFloorHurt = 0x534C20;      // (kind byte): the floor's damage by kind 0..8
constexpr std::uint32_t kFlash = 0x534DB0;          // (n byte): the leader's CLUT flash, sound 0x108
constexpr std::uint32_t kHpLose = 0x537480;         // (amount, member byte): ax, the HP taken
constexpr std::uint32_t kHpGain = 0x5373F0;         // (amount, member byte)

struct Callees {
    void (__cdecl* jump_setup)();                                              // Field_JumpSetUp (ours)
    void (__cdecl* jump_camera)();                                             // Field_JumpCamera (ours)
    void (__cdecl* jump_start)();                                              // Field_JumpStart (ours)
    long (__cdecl* slope_at)(long, long, unsigned);                            // 0x5725C0 (group M)
    long (__cdecl* ground_at)(long, long);                                     // MapView_GroundAt
    unsigned char (__cdecl* shade_raise)(unsigned);                            // Sprite_ShadeRaise (ours)
    void (__cdecl* load_palette)(unsigned short*, unsigned);                   // Sprite_LoadPalette
    unsigned char (__cdecl* cells_all)(long, long, unsigned, unsigned, unsigned);   // AreaMap_CellsAll (ours)
    unsigned char (__cdecl* cells_all4)(long, long, unsigned, unsigned);       // AreaMap_CellsAll4 (ours)
    unsigned char (__cdecl* cells_all_wide)(long, long, unsigned, unsigned);   // 0x535490
    unsigned char (__cdecl* cells_none)(long, long, unsigned, unsigned, unsigned);  // AreaMap_CellsNone (ours)
    unsigned char (__cdecl* cells_none4)(long, long, unsigned, unsigned);      // AreaMap_CellsNone4 (ours)
    unsigned char (__cdecl* cells_none_wide)(long, long, unsigned, unsigned);  // 0x535D60
    unsigned char (__cdecl* byte_at)(short, short);                            // AreaMap_ByteAt
    void (__cdecl* floor_hurt)(unsigned);                                      // 0x534C20
    void (__cdecl* flash)(unsigned);                                           // 0x534DB0
    int (__cdecl* party_count)(unsigned);                                      // Party_Count
    unsigned (__cdecl* hp_lose)(unsigned, unsigned);                           // 0x537480
    void (__cdecl* hp_gain)(unsigned, unsigned);                               // 0x5373F0
    unsigned char (__cdecl* effect_free)();                                    // Effect_FindFree
    unsigned char (__cdecl* tile_turn)(unsigned, unsigned, unsigned);          // Field_TileTurn (ours)
    unsigned char (__cdecl* turn_probe)(long, long, unsigned, unsigned);       // 0x535610
    unsigned char (__cdecl* equip_count)(unsigned, unsigned, unsigned);        // Actor_EquipCount (ours)
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=event_objs: the start-up fuzz, event_objs_fuzz.cpp. Clones
// every original before EventObjs_Inject patches it.
void SelfTest();

}  // namespace event_objs
