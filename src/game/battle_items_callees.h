// Internal to battle_items.cpp and battle_items_fuzz.cpp: the addresses the
// battle's item and ability effects touch that have no name in symbols.toml,
// and every call they make - through pointers, so that the start-up fuzz can
// stand recording functions in for them, for the originals' copies and for
// ours alike. docs/battle_items.md.
//
// Calls into other groups' functions of the seventh round, or into anything
// unnamed, go through the raw addresses below and are never bound here:
//   0x572FA0  a depth-sorted Gfx_CommitPrim (x, z, slot, size) - PSX 0x801564C4;
//             unnamed, 400-odd call sites
//   0x4456C0  "is this battle actor out" (group BB's)
//   0x435A20  an enemy's animation (unnamed; calls BB's 0x4358D0)
//   0x446A50  an actor's sound by index through Field_State +0x2C (group BF's)
//   0x437450  Sound_PlayEffect unless the id is 0xFFFF (unnamed)
//   0x454CC0  Sprite_SetTint (group BG's this round)
//   0x587900  Sound_PlayById (group BG's this round)
//
// Two dispatches are not calls to a named function:
//   - Sparkle_Dispatch 0x4B8FE0 jumps through Sparkle_Types 0x65AE28, a table
//     in .data whose one real entry is 0x4B9000 (left Capcom's, see the doc);
//     ours reads it as the original does, and the fuzz swaps four entries.
//   - SndStream_Play / SndStream_Stop call the IDirectSoundBuffer's vtable;
//     the fuzz hands them a fake buffer.
#pragma once

#include <cstdint>

namespace battle_items {

namespace at {

// The sparkle pool: 128 records of 0x2C bytes, and the one being updated.
constexpr std::uint32_t kSparklePool = 0x684790;
constexpr std::uint32_t kSparkleStride = 0x2C;
constexpr unsigned kSparkles = 0x80;
constexpr std::uint32_t kSparkleCurrent = 0x685D90;   // unsigned char *, set by the pool walk 0x4B8D70
constexpr std::uint32_t kFxActor = 0x93B940;          // unsigned char *, the battle actor the effect plays on
// A sparkle's fields.
constexpr unsigned kFlags = 0;        // bit 0 in use
constexpr unsigned kType = 1;         // Sparkle_Types index (always 0 as 0x4B8D70 sets it)
constexpr unsigned kPhase = 2;        // 0 launch, 1 rise, 2 fade (0x4B9000's own stack table)
constexpr unsigned kShade = 3;        // row of the colour table
constexpr unsigned kKind = 4;         // the effect's kind: colour rows, life
constexpr unsigned kTimer = 5;        // u8: launch delay, then brightness
constexpr unsigned kCount = 6;        // u8
constexpr unsigned kLimit = 7;        // u8: rise length; before launch, the offset row
constexpr unsigned kBaseX = 8;        // s16
constexpr unsigned kDriftY = 0xA;     // s16
constexpr unsigned kWave = 0xC;       // u16: the sway's phase
constexpr unsigned kPosX = 0x14;      // three dwords: the actor's +0x34 / +0x38 / +0x3C
constexpr unsigned kX = 0x20;         // s16 screen x
constexpr unsigned kY = 0x22;         // s16 screen y
// The constant tables (.data).
constexpr std::uint32_t kSparkleOffsets = 0x65AD34;   // 32 x (s16 dx, s16 dy)
constexpr std::uint32_t kSparkleColours = 0x65ADD4;   // rgb rows, row = shade + 4 * kind
constexpr std::uint32_t kSparkleLife = 0x65AE20;      // u8 per kind
constexpr std::uint32_t kSparkleTypes = 0x65AE28;     // the dispatch table

// The PC's copy of the PSX scratchpad (DamageScratch) and the vertex scratch.
constexpr std::uint32_t kScratch = 0x903850;          // s16 words at +0 .. +0xE
constexpr std::uint32_t kVertex = 0x9037A0;           // Prim_VertexScratch: four SVECTORs, 8 bytes apart

// The battle actor's records.
constexpr std::uint32_t kActorIndex = 0x904B34;       // u8: 0..2 a party member, 3.. an enemy
constexpr std::uint32_t kPartyRecords = 0x802D40;     // ObjTrio, stride 0x14C
constexpr std::uint32_t kPartyStride = 0x14C;
constexpr std::uint32_t kEnemyRecords = 0x93B960;     // stride 0x128
constexpr std::uint32_t kEnemyStride = 0x128;
constexpr std::uint32_t kEnemySoundMode = 0x904AAA;   // u8: non-zero plays the enemy's own cue
constexpr std::uint32_t kFxSizeSelect = 0x904B89;     // u8
constexpr std::uint32_t kFxSizeTable = 0x65C3EC;      // u8 by a party record's +0x89
constexpr std::uint32_t kFxSizeAltTable = 0x65C3F8;   // u8 by 0x904B89, for a member with +0x134 bit 1
constexpr std::uint32_t kEnemyFxSize = 0x8C5652;      // u8, stride 0x8C by the enemy's type byte

// The SND stream's buffer (an IDirectSoundBuffer *).
constexpr std::uint32_t kStreamBuffer = 0x7DE3C8;

}  // namespace at

// Raw addresses of callees this group does not own (see the top).
constexpr std::uint32_t kCommitSorted = 0x572FA0;
constexpr std::uint32_t kActorIsOut = 0x4456C0;
constexpr std::uint32_t kEnemyAnimation = 0x435A20;
constexpr std::uint32_t kActorSound = 0x446A50;
constexpr std::uint32_t kEnemySound = 0x437450;
constexpr std::uint32_t kSetTint = 0x454CC0;
constexpr std::uint32_t kPlayById = 0x587900;

struct Callees {
    void (__cdecl* set_draw_mode)(unsigned char*, int, int, unsigned, unsigned long);   // Gpu_SetDrawMode (ours)
    void (__cdecl* commit)(unsigned, unsigned);                                         // Gfx_CommitPrim (ours)
    void (__cdecl* commit_sorted)(unsigned, unsigned, unsigned, unsigned);              // 0x572FA0
    int (__cdecl* sin)(int);                                                            // Math_Sin (ours)
    int (__cdecl* cos)(int);                                                            // Math_Cos (ours)
    void (__cdecl* set_poly_g3)(unsigned char*);                                        // Gpu_SetPolyG3 (ours, here)
    void (__cdecl* set_line_g2)(unsigned char*);                                        // Gpu_SetLineG2 (ours, here)
    void (__cdecl* set_line_g3)(unsigned char*);                                        // Gpu_SetLineG3 (ours, here)
    void (__cdecl* set_tile1)(unsigned char*);                                          // Gpu_SetTile1 (ours, here)
    void (__cdecl* set_poly_gt4)(unsigned char*);                                       // Gpu_SetPolyGT4 (ours)
    void (__cdecl* set_semi)(unsigned char*, unsigned);                                 // Gpu_SetSemiTrans (ours)
    unsigned (__cdecl* get_tpage)(unsigned, unsigned, int, int);                        // Gpu_GetTPage (ours)
    unsigned (__cdecl* get_clut)(int, int);                                             // Gpu_GetClut (ours)
    // The three projections get the flag pointer libgte has; the port's do not read it.
    long (__cdecl* rtp)(const short*, float*, long*, long*);                            // Gte_RotTransPers (ours)
    long (__cdecl* rtp3)(const short*, const short*, const short*, float*, float*, float*, long*, long*);
    long (__cdecl* rtp4)(const short*, const short*, const short*, const short*, float*, float*, float*, float*,
                         long*, long*);                                                 // Gte_RotTransPers3 / 4 (ours)
    void (__cdecl* depths3)(unsigned char*);                                            // Gte_PrimDepths3_10B (ours)
    void (__cdecl* depths4)(unsigned char*);                                            // Gte_PrimDepths4_14 (ours)
    void (__cdecl* store_depth)(float*);                                                // Gte_StoreDepthF (ours)
    void (__cdecl* push_matrix)();                                                      // Gte_PushMatrix (ours)
    void (__cdecl* rot_trans)(const short*, long*, long*);                              // Gte_RotTrans (ours)
    short* (__cdecl* rot_matrix)(const short*, short*);                                 // Gte_RotMatrix (ours)
    short* (__cdecl* mul_matrix0)(const short*, const short*, short*);                  // Gte_MulMatrix0 (ours)
    void (__cdecl* set_rot)(const unsigned long*);                                      // Gte_SetRotMatrix (ours)
    void (__cdecl* set_trans)(const unsigned long*);                                    // Gte_SetTransMatrix (ours)
    int (__cdecl* rand)();                                                              // Rand, the CRT's
    void (__cdecl* set_animation)(unsigned);                                            // Sprite_SetAnimation (ours), a whole dword
    void (__cdecl* enemy_animation)(unsigned, unsigned);                                // 0x435A20
    unsigned char (__cdecl* is_out)(unsigned);                                          // 0x4456C0
    unsigned char (__cdecl* set_tint)(unsigned char*, unsigned, unsigned, unsigned, unsigned);   // 0x454CC0
    void (__cdecl* actor_sound)(unsigned);                                              // 0x446A50
    void (__cdecl* enemy_sound)(unsigned);                                              // 0x437450
    void (__cdecl* play_by_id)(unsigned);                                               // 0x587900
    void (__cdecl* stream_stop)();                                                      // SndStream_Stop (ours, here)
    void* (__cdecl* create_from_wave)(const unsigned char*);                            // SndBuf_CreateFromWave (ours)
    unsigned (__cdecl* find_data)(const unsigned char*, const unsigned char**);         // Wave_FindData (ours)
    long (__cdecl* buffer_write)(void*, const void*, unsigned, unsigned);               // SndBuf_Write (ours)
    void (__cdecl* sparkle_free)();                                                     // Sparkle_Free (ours, here)
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=battle_items: the start-up fuzz, battle_items_fuzz.cpp. Clones
// every original before BattleItems_Inject patches it.
void SelfTest();

}  // namespace battle_items
