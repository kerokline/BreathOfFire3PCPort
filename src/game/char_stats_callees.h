// Internal to char_stats.cpp and char_stats_fuzz.cpp: the addresses the stat,
// inventory and item-table functions touch that have no name in symbols.toml,
// and every call they make - through pointers, so that the start-up fuzz can
// stand recording functions in for them, for the originals' copies and for
// ours alike. Most callees are ours in this module (the equipment passes under
// Char_RecalcStats, the two byte steps under them); through the pointers each
// function is still tested alone. docs/char-stats.md.
#pragma once

#include <cstdint>

namespace char_stats {

namespace at {

// The persistent character records (CharacterRecords, docs/item-use.md
// section 2): the PSX record's offsets + 4.
constexpr std::uint32_t kCharRecords = 0x903A70;
constexpr std::uint32_t kCharStride = 0xA4;
constexpr unsigned kRecordBytes = 0xA4;
constexpr unsigned kFlags = 0x0B;        // u8, bit 0 counted by Inventory_Count's equipped branch
constexpr unsigned kWeapon = 0x12;       // u8 weapon id, then three armour ids and two accessory ids
constexpr unsigned kArmour = 0x13;
constexpr unsigned kAccessory = 0x16;
constexpr unsigned kHp = 0x18;           // u16
constexpr unsigned kHalveBits = 0x1D;    // u8, PSX +0x19: each bit steps one resistance byte by 2
constexpr unsigned kHpScale = 0x1E;      // u8, PSX +0x1A
constexpr unsigned kTrait = 0x1F;        // u8, PSX +0x1B: the list of 0x667548 to apply, 0xFF none
constexpr unsigned kEffective = 0x20;    // 0x20 bytes, rebuilt from the base block at +0x40
constexpr unsigned kMaxHp = 0x20;        // u16
constexpr unsigned kAtk = 0x24, kDef = 0x26, kAgi = 0x28, kInt = 0x2A;   // u16 each
constexpr unsigned kResist = 0x2F;       // nine bytes 0..6 (7 set by a step of 0), Stat_AddResist
constexpr unsigned kPercent = 0x38;      // five bytes 0..100, Stat_AddCap100
constexpr unsigned kBase = 0x40;         // the base block, 0x20 bytes

constexpr std::uint32_t kRosterBonus = 0x903640;   // 5 bytes per persistent record (PSX 0x80148668)
constexpr std::uint32_t kTraitLists = 0x667548;    // pointer per trait byte to (kind, amount) pairs, 0xE ends
constexpr std::uint32_t kInvIds = 0x656B00;        // id list pointer per category (0x904154 + 0x80 * cat;
                                                   // category 4 the key items 0x904554)
constexpr std::uint32_t kInvCounts = 0x656B14;     // count list pointer per category (the fifth is 0)
constexpr std::uint32_t kKeyItems = 0x904554;      // 32 id bytes (the PSX 0x80145448)
constexpr std::uint32_t kExtraAccessory = 0x904130;   // u8: Inventory_Count's category-3 extras
constexpr std::uint32_t kExtraItem = 0x90412E;        // u8, with its count at 0x90412F
constexpr std::uint32_t kExtraCount = 0x90412F;
constexpr std::uint32_t kTextRecords = 0x904CE0;   // Text_Records, 32 bytes each
// The item tables (symbols.toml's NameTable_*), their strides and the fields
// read here.
constexpr std::uint32_t kConsumables = 0x656B28, kConsumableStride = 0x16;
constexpr std::uint32_t kKeyItemTable = 0x657310, kKeyItemStride = 0x14;
constexpr std::uint32_t kWeapons = 0x657450, kWeaponStride = 0x1C;
constexpr std::uint32_t kArmourTable = 0x657D68, kArmourStride = 0x1A;
constexpr std::uint32_t kAccessories = 0x658450, kAccessoryStride = 0x18;

}  // namespace at

struct Callees {
    void (__cdecl* set_draw_mode)(unsigned char*, int, int, unsigned, unsigned long);   // Gpu_SetDrawMode (psx_gpu.cpp)
    void (__cdecl* commit_prim)(unsigned, unsigned);                                   // Gfx_CommitPrim (draw_emit.cpp)
    void (__cdecl* set_poly_ft4)(unsigned char*);                                      // Gpu_SetPolyFT4 (psx_gpu.cpp)
    void (__cdecl* set_sprt)(unsigned char*);                                          // Gpu_SetSprt (psx_gpu.cpp)
    short (__cdecl* add_clamped)(unsigned short*, unsigned);                           // Stat_AddClamped (item_use.cpp)
    unsigned char (__cdecl* add_cap99)(unsigned char*, unsigned);                      // Stat_AddCap99 (item_use.cpp)
    unsigned char (__cdecl* add_resist)(unsigned char*, unsigned);                     // Stat_AddResist (ours)
    unsigned char (__cdecl* add_cap100)(unsigned char*, unsigned);                     // Stat_AddCap100 (ours)
    void (__cdecl* apply_weapon)(unsigned char*);                                      // Char_ApplyWeapon (ours)
    void (__cdecl* apply_armour)(unsigned char*);                                      // Char_ApplyArmour (ours)
    void (__cdecl* apply_accessories)(unsigned char*);                                 // Char_ApplyAccessories (ours)
    void (__cdecl* apply_traits)(unsigned char*);                                      // Char_ApplyTraits (ours)
    void (__cdecl* recalc)(unsigned char*);                                            // Char_RecalcStats (ours)
};

extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=char_stats: the start-up fuzz, char_stats_fuzz.cpp. Clones
// every original before CharStats_Inject patches it.
void SelfTest();

}  // namespace char_stats
