// The raw addresses effect_5f.cpp reads that symbols.toml does not name - each a
// load-bearing constant (CLAUDE.md rule 3). docs/effect_5f.md. Every callee of
// the group is named (ours, or Capcom's Rand); what is raw here is the image's
// .data the programs read in place, and the cells they share.
#pragma once

#include <cstdint>

namespace effect_5f::at {

// --- the leader: ObjTrio record 0's point (ObjTrio + 0x34 / + 0x38, 16.16) ----
constexpr std::uint32_t kLeaderX = 0x802D74;
constexpr std::uint32_t kLeaderZ = 0x802D78;
constexpr std::uint32_t kObjTrioStride = 0x14C;   // ObjTrio's records
constexpr unsigned kObjTrioCount = 3;

// --- Cond_Flags' story row (Flags_Test's bank) ---------------------------------
constexpr std::uint32_t kStoryFlags = 0x904030;

// --- the vertex scratch: Prim_VertexScratch's four vertices of three s16 and a
// pad (8 bytes each) ---------------------------------------------------------
constexpr std::uint32_t kV0 = 0x9037A0, kV1 = 0x9037A8, kV2 = 0x9037B0, kV3 = 0x9037B8;

// --- sub-kind 0x27's placements, by the record's starting +0x36 (two) --------
constexpr std::uint32_t kSub27Height = 0x65E8A4;   // u16 x2: the record's +0x3E
constexpr std::uint32_t kSub27Texture = 0x65E8A8;  // u16 x2: the record's +0x2E (the texture's low word)
constexpr std::uint32_t kSub27Cell = 0x65E8AC;     // u8 pairs x2: the cell (x, z)
constexpr unsigned kSub27Places = 2;

// --- sub-kind 0x42's placements and its draw's shapes --------------------------
constexpr std::uint32_t kSub42Places = 0x65E8E0;   // 4-byte records x3: x, z, the variant (+8), the story flag (+0xB)
constexpr unsigned kSub42PlaceCount = 3;
constexpr std::uint32_t kSub42Shapes = 0x65E8EC;   // 12-byte records x3: four vertices of three s8
constexpr unsigned kSub42ShapeCount = 3;
constexpr std::uint32_t kSub42Pieces = 0x65E910;   // 0x18-byte records x2 (by variant): six pieces of four
                                                   // bytes - dx, dz, the shape, the texture's low byte
constexpr unsigned kSub42Variants = 2;
constexpr unsigned kSub42PieceCount = 6;
constexpr std::uint32_t kSub42Lift = 0x65E940;     // u16 x2 (by variant): added to each vertex's height
constexpr std::uint32_t kSub42ShapeTexture = 0x65E944;   // u8 x3 (by shape): the texture's page byte

// --- sub-kinds 0x29, 0x48 and 0x49's placements, by the starting +0x36 (four) --
constexpr std::uint32_t kSub29Height = 0x65E948;   // u16 x4: taken from the ground's half height into +0x3E
constexpr std::uint32_t kSub29Texture = 0x65E950;  // u32 x4: the record's +0x20
constexpr std::uint32_t kSub29Cell = 0x65E960;     // u8 pairs x4: the cell (x, z)
constexpr unsigned kSub29Places = 4;
constexpr std::uint32_t kSub29Slide = 0x65E97C;    // s8 x2: each half's direction (the draw's two quads)

// --- sub-kinds 0x4B / 0x4C ---------------------------------------------------
constexpr std::uint32_t kSub4BBounds = 0x65E9A4;   // u8 pairs x2 (by +0x34's low word being 0): +0x2E, +0x30

// --- sub-kind 0x2A's placements ------------------------------------------------
constexpr std::uint32_t kSub2AHeight = 0x65E9BC;   // u16 x6
constexpr std::uint32_t kSub2ACell = 0x65E9C8;     // u8 pairs x6: the cell (x, z)
constexpr unsigned kSub2APlaces = 6;
constexpr std::uint32_t kSub2ASlide = 0x65E9D4;    // s8 x2: each half's direction

// --- sub-kind 0x3C's two textures ------------------------------------------------
constexpr std::uint32_t kSub3CTextures = 0x65E9E4; // u32 x2

// --- the draw items (DrawItems' 0x90-byte records; the pool is draw_pool's) ----
constexpr std::uint32_t kDrawItemStride = 0x90;
constexpr std::uint32_t kItemHalfB = 0x7E;         // the u16 a placed cell's item keeps (the record's +8 set)
constexpr std::uint32_t kItemHalfA = 0x8E;         // the same for +8 clear

// --- AreaMap_Header's fields (the loaded area block) ----------------------------
constexpr std::uint32_t kMapWidth = 0x8CB580;      // u8
constexpr std::uint32_t kMapDepth = 0x8CB581;      // u8
constexpr std::uint32_t kMapCellBase = 0x8CB582;   // u16: the cell words start at 2 x it (words)
constexpr std::uint32_t kMapTextures = 0x8CB584;   // u32s, indexed past the cells by the cell word

}  // namespace effect_5f::at
