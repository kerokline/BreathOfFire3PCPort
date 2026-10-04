// The raw addresses effect_5g.cpp and its fuzz read that symbols.toml does not
// name - each a load-bearing constant (CLAUDE.md rule 3). docs/effect_5g.md.
// Every callee of the group is ours (by name) or the group's own; nothing is
// called by a raw address. The constants below are cells and the image's
// read-only tables the states index (read in place, never copied).
#pragma once

#include <cstdint>

namespace effect_5g::at {

// --- the leader (ObjTrio record 0) and the party records ---------------------------
constexpr std::uint32_t kLeaderX = 0x802D74;        // ObjTrio +0x34: the leader's x, 16.16
constexpr std::uint32_t kLeaderZ = 0x802D78;        // ObjTrio +0x38: its z, 16.16
constexpr std::uint32_t kTrioStride = 0x14C;        // ObjTrio's records (sub-kind 0x3A walks all three)
constexpr unsigned kTrioCount = 3;

// --- sub-kind 0x2B's variants, indexed by the spawn's x cell (+0x36) ---------------
// Three tables back to back with room for four entries each (the fourth all
// zero); ours aborts on an index past the room.
constexpr std::uint32_t kSub2BHeights = 0x65EA00;   // s16 each: the height +0x32 and the lift into +0x3E
constexpr std::uint32_t kSub2BTextures = 0x65EA08;  // byte each: the texture's top byte (+0xC = it << 24)
constexpr std::uint32_t kSub2BCells = 0x65EA0C;     // two bytes each: the cell x, z put into +0x36, +0x3A
constexpr unsigned kSub2BVariants = 4;
constexpr std::uint32_t kSub2BSides = 0x65EA14;     // two s8: the two panels' slide signs (the draw's loop)

// --- sub-kind 0x36's shade steps --------------------------------------------------
constexpr std::uint32_t kSub36Blues = 0x65EA18;     // four bytes: the fill's blue (<< 3), indexed by +2
constexpr unsigned kSub36Steps = 4;

// --- sub-kinds 0x2C's and 0x4A's variants, indexed by the spawn's x cell -----------
// Room for six heights, five textures, six cells (the sixth zero); five
// variants in all (the textures' room).
constexpr std::uint32_t kSub2CHeights = 0x65EA1C;   // s16 each: the lift into +0x3E
constexpr std::uint32_t kSub2CTextures = 0x65EA28;  // dword each: +0x20, the texture word
constexpr std::uint32_t kSub2CCells = 0x65EA3C;     // two bytes each: the cell x, z
constexpr unsigned kSub2CVariants = 5;
constexpr std::uint32_t kSub2CSides = 0x65EA5C;     // two s8: the two panels' slide signs (room four, before
                                                    // EffectKind18Sub3A_States)
constexpr std::uint32_t kSub3ASides = 0x65EA74;     // s8 by the panel argument (room four, before
                                                    // EffectKind18Sub4A_States)
constexpr unsigned kSidesRoom = 4;
constexpr std::uint32_t kSub4ASubKinds = 0x65EA88;  // a byte every four: +0xB by the variant (five)
constexpr std::uint32_t kSub4AMarks = 0x65EA9C;     // a byte every four: the map byte AreaMap_SetByte writes (five)

// --- sound ids ------------------------------------------------------------------------
constexpr unsigned kSoundOpen = 0x200;              // sub-kinds 0x2B / 0x2C: the leader comes near
constexpr unsigned kSoundShut = 0x201;              // sub-kinds 0x2B / 0x2C: closed again
constexpr unsigned kSoundOpen3A = 0x202;            // sub-kind 0x3A: a member comes near
constexpr unsigned kSoundShut3A = 0x203;            // sub-kind 0x3A: closed again

}  // namespace effect_5g::at
