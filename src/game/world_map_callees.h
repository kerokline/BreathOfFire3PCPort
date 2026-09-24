// Internal to world_map.cpp and world_map_fuzz.cpp: the addresses the group's
// functions touch that have no name in symbols.toml, and every call they make -
// through pointers, so that the start-up fuzz can stand recording functions in
// for them, for the originals' copies and for ours alike. Three callees are
// ours in this module (WorldMap_DrawFrame under WorldMap_FrameStep,
// WorldMap_DrawSprite and WorldMap_DrawNeedle under the frame and the HUD);
// through the pointers each function is still tested alone. Two are Capcom's
// and stay so: the event function 0x531920 (another queue group's) and the
// state-0 block 0x411310 that eleven map state machines share. docs/world-map.md
// section 7.
#pragma once

#include <cstdint>

#include "bof3/symbols.gen.h"

namespace world_map {

using U = std::uint32_t;

// --- Addresses without a name ------------------------------------------------
// The map task's state table: byte +2 of Sprite_Current indexes four entries
// (0x411310 - shared by eleven such tables - 0x404180, 0x4041B0, 0x4041E0).
// Entries 4..7 at 0x5EF608 are the HUD state machine 0x404230's table, read
// by 0x404230 alone.
constexpr U kStateTable = 0x5EF5F8;
constexpr U kState0 = 0x411310;   // state 0: y = -0x30 and state 1 unless the mode byte is 2 or Field_ScriptFlags bit 8
// The frame's event test: 0x531920(x, y), one of Field_LeaderCellEvent's
// exits (docs/event-ops.md section 11); al non-zero shows the third legend.
constexpr U kCellEvent = 0x531920;
// The dial page's sprite table: 22 entries of (w, h, u, v) bytes, tpage 0x9C,
// CLUT 0x7B80 - the dial (0), three legend labels (1..3), the region box and
// its cap (4, 5), sixteen 8 x 8 key glyphs (6..21). Read by index & 0xFF,
// unchecked: 22 and up read the button table and the state tables after it.
constexpr U kSpriteTable = 0x5EF618;
// The button table: six entries of (u16 mask, u8 sprite index, pad). The
// frame searches the first six for the first legend's key and EIGHT for the
// second's - entries 6 and 7 are the words of the state table 0x5EF688
// (0x004253C0, 0x004046A0), so a button word with bit 8, 9, 10, 12 or 14 set
// picks sprite 0x42 or 0x40 (docs/world-map.md section 7.3). Kept.
constexpr U kButtonTable = 0x5EF670;
constexpr U kButtonTableEnd6 = 0x5EF688;
constexpr U kButtonTableEnd8 = 0x5EF690;
// The field's button map, nine words at 0x903580 (defaults at 0x656AEC:
// 0023 0080 0010 0008 0004 0100 0040 0023 0040): word 0 is the first legend's
// button, word 6 the second's; word 2 is Field_MenuButton, 7 Field_ConfirmButtons.
constexpr U kButtonMap0 = 0x903580;
constexpr U kButtonMap6 = 0x90358C;
// The world map's mode byte (docs/event-ops.md: "pad 0x100 cycles 0x9045FA"):
// 2 holds the frame on screen and keeps state 0 from starting a slide.
constexpr U kMapMode = 0x9045FA;
// The party-set byte (symbols.toml: 0x90412C & 0x7F, 0xFF for none): set 0xC
// shows the third legend.
constexpr U kPartySet = 0x90412C;
// The cells the leader stands on: the high words of Field_Kind2X / Field_Kind2Z.
constexpr U kLeaderCellX = 0x905E66;
constexpr U kLeaderCellZ = 0x905E62;
// The area's 0x80010000 section in LoadDatFile's arena: the region label is
// the string at 0x803580 + the low word of the dword at +8.
constexpr U kAreaText = 0x803580;
constexpr U kAreaTextOffset = 0x803588;
// Gfx_CommitPrim's bound on the packet pool: Gfx_PacketPools + 0x10000 - 0x54,
// plus Gfx_BufferIndex << 16 (symbols.toml, Gfx_CommitPrim).
constexpr U kPoolBound = 0x7F1BAC;
// Gfx_FrameNodes: 0x38 records of 0x30 bytes; the dword at +4 of the record
// (Gfx_BufferIndex + row * 6) * 8 is the tail of that map-view row's list for
// this buffer (docs/sprite-draw-order.md section 7: the port's ordering table).
constexpr U kRowTails = 0x8022C4;
// Gfx_FrameNodes record count and stride, for the fuzz's region.
constexpr U kRows = 0x38;
constexpr U kRowStride = 0x30;

// The named data, by the address its symbols.gen.h macro names.
namespace at {
inline U Of(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
inline U Sprite_CurrentAt() { return Of(&Sprite_Current); }
inline U Gfx_PacketNextAt() { return Of(&Gfx_PacketNext); }
inline U Draw_PassFlagsAt() { return Of(&Draw_PassFlags); }
inline U Field_ScriptFlagsAt() { return Of(&Field_ScriptFlags); }
inline U Field_ScriptFlags2At() { return Of(&Field_ScriptFlags2); }
inline U Camera_AnglesAt() { return Of(Camera_Angles); }
inline U Prim_VertexScratchAt() { return Of(Prim_VertexScratch); }
inline U MapView_OriginAt() { return Of(MapView_Origin); }
inline U Gfx_BufferIndexAt() { return Of(&Gfx_BufferIndex); }
inline U DrawItemsAt() { return Of(DrawItems); }
}  // namespace at

struct Callees {
    void (__cdecl* state0)();                                                  // 0x411310, Capcom's shared block
    void (__cdecl* draw_frame)(int, int);                                      // WorldMap_DrawFrame 0x404390 (this module)
    void (__cdecl* set_draw_mode)(unsigned char*, int, int, unsigned, unsigned long);   // Gpu_SetDrawMode 0x5A77C0 (ours)
    void (__cdecl* commit_prim)(unsigned, unsigned);                           // Gfx_CommitPrim 0x461E50 (ours)
    void (__cdecl* draw_sprite)(int, int, unsigned);                           // WorldMap_DrawSprite 0x404560 (this module)
    unsigned char (__cdecl* byte_at)(short, short);                            // AreaMap_ByteAt 0x536700 (ours)
    unsigned char (__cdecl* cell_event)(short, short);                         // 0x531920, Capcom's
    void (__cdecl* draw_needle)(int, int);                                     // WorldMap_DrawNeedle 0x408530 (this module)
    void (__cdecl* set_sprt)(unsigned char*);                                  // Gpu_SetSprt 0x5A7710 (ours)
    void (__cdecl* set_semi_trans)(unsigned char*, unsigned);                  // Gpu_SetSemiTrans 0x5A7780 (ours)
    const unsigned char* (__cdecl* text_draw_at)(int, int, int, int, const unsigned char*);   // Text_DrawAt 0x516B30 (ours)
    void (__cdecl* push_matrix)();                                             // Gte_PushMatrix 0x5A7B90 (ours)
    short* (__cdecl* rot_matrix)(const short*, short*);                        // Gte_RotMatrix 0x5A8060 (ours)
    void (__cdecl* set_rot_matrix)(const unsigned long*);                      // Gte_SetRotMatrix 0x5A8DE0 (ours)
    void (__cdecl* set_trans_matrix)(const unsigned long*);                    // Gte_SetTransMatrix 0x5A8E00 (ours)
    unsigned char* (__cdecl* set_poly_g4)(unsigned char*);                     // Gpu_SetPolyG4 0x5A7610 (ours)
    // Gte_RotTransPers4 0x5A85F0 (ours): the original pushes TEN arguments -
    // libgte's (v0..v3, sxy0..sxy3, p, flag) - and ours reads nine (cdecl: the
    // tenth is harmless); the needle hands both locals as the original does.
    long (__cdecl* rot_trans_pers4)(const short*, const short*, const short*, const short*, float*, float*, float*,
                                    float*, long*, long*);
    void (__cdecl* prim_depths)(void*);                                        // Gte_PrimDepths4_10B 0x5A9350 (ours)
    void (__cdecl* pop_matrix)();                                              // Gte_PopMatrix 0x5A7BC0 (ours)
    unsigned long (__cdecl* item_at)(long, long);                              // MapView_ItemAt 0x572ED0 (ours)
    void (__cdecl* link_prim)(unsigned long*, unsigned long);                  // Gpu_LinkPrim 0x5A7560 (ours)
};
extern const Callees kOriginals;
extern Callees g;

// BOF3X_SHADOW=world_map: the start-up fuzz, world_map_fuzz.cpp. Clones every
// original before WorldMap_Inject patches it.
void SelfTest();

}  // namespace world_map
