// Moved here by tools/move_funcs.py from battle_draw.cpp, d3d_draw.cpp, d3d_list.cpp, d3d_rest.cpp, display_env.cpp, display_setup.cpp, field_misc.cpp, gfx_clut.cpp, gfx_image.cpp, gfx_texcache.cpp, gfx_vram_ops.cpp, glyph_draw.cpp, psx_rest.cpp, sound_rest.cpp, sprite_draw.cpp, sprt_draw.cpp, tex_cells.cpp, tex_page.cpp, win_main.cpp (docs/source-layout-round.md).
#include "game/platform/d3d.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <emmintrin.h>
#include <string>
#include <type_traits>
#include <windows.h>

#include "audio/mp3.h"
#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle_draw.h"
#include "game/battle_draw_callees.h"
#include "game/d3d_draw.h"
#include "game/d3d_draw_callees.h"
#include "game/d3d_lines.h"
#include "game/d3d_list_callees.h"
#include "game/d3d_rest.h"
#include "game/d3d_rest_callees.h"
#include "game/display_env_callees.h"
#include "game/display_setup.h"
#include "game/field_misc.h"
#include "game/field_misc_callees.h"
#include "game/game_clock.h"
#include "game/gfx_clut.h"
#include "game/gfx_texcache.h"
#include "game/gfx_vram_ops.h"
#include "game/glyph_draw.h"
#include "game/glyph_draw_callees.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/map_rim.h"
#include "game/msgbox.h"
#include "game/pause_text.h"
#include "game/psx_rest_callees.h"
#include "game/quad_far_edge.h"
#include "game/quick_menu.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound_callees.h"
#include "game/sound_in.h"
#include "game/sound_rest.h"
#include "game/sound_rest_callees.h"
#include "game/sound_sp.h"
#include "game/sound_st.h"
#include "game/sprite_draw.h"
#include "game/sprt_draw.h"
#include "game/sprt_draw_callees.h"
#include "game/tex_cells_callees.h"
#include "game/tex_page.h"
#include "game/tex_page_callees.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/wide_staging.h"
#include "game/widescreen.h"
#include "game/win_main.h"
#include "hook/code_bytes.h"
#include "hook/cpu_context.h"
#include "hook/detour.h"
#include "hook/draw_order.h"
#include "hook/exe_host.h"
#include "hook/exe_icon.h"
#include "hook/exe_image.h"
#include "hook/input_script.h"
#include "hook/log.h"
#include "hook/run_speed.h"
#include "hook/snapshot.h"
#include "hook/stallwatch.h"
#include "hook/stp_census.h"
#include "render/fb_grab.h"
#include "render/render_d3d11.h"
#include "render/render_shim.h"

namespace {

using namespace battle_draw;

}  // namespace

using namespace d3d_draw;

using namespace d3d_list;

using namespace d3d_rest;

using namespace display_env;

namespace {

using namespace field_misc;

}  // namespace

using namespace psx_rest;

using namespace sound_rest;

using namespace tex_cells;

using namespace tex_page;

namespace {

unsigned char* AtBattleDraw(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

battle_draw::U LongBattleDraw(const unsigned char* p) {
    battle_draw::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

float FloatBattleDraw(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutLongBattleDraw(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutFloatBattleDraw(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// --- the x87 sequences, as the handlers execute them (d3d_draw.cpp and
// field_misc.cpp have the same; each rounds to the control word's precision
// exactly as Capcom's) ---
// fld a; fmul b; fstp
inline float X87MulBattleDraw(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// fld a; fdiv b; fstp
inline float X87DivBattleDraw(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfdivs %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// D3d_Device, read afresh before every COM call as the original does.
void** DeviceBattleDraw() { return reinterpret_cast<void**>(static_cast<std::uintptr_t>(*reinterpret_cast<const volatile std::uint32_t*>(AtBattleDraw(::bof3::Img(D3d_Device_addr))))); }

void* MethodBattleDraw(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }

using Com3 = long(__stdcall*)(void*, U, U);

using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

long SetTextureBattleDraw(battle_draw::U stage, battle_draw::U texture) {
    void** device = DeviceBattleDraw();
    return reinterpret_cast<Com3>(MethodBattleDraw(device, 0x98))(device, stage, texture);
}

// DrawPrimitive(type, D3DFVF_TLVERTEX, D3d_Vertices, count, 0).
long DrawVerticesBattleDraw(battle_draw::U type, battle_draw::U count) {
    void** device = DeviceBattleDraw();
    return reinterpret_cast<Com6>(MethodBattleDraw(device, 0x70))(device, type, 0x1C4, ::bof3::Img(D3d_Vertices_addr), count, 0);
}

unsigned char* VertexBattleDraw(battle_draw::U i) { return AtBattleDraw(::bof3::Img(D3d_Vertices_addr) + i * 0x20); }

battle_draw::U DrawModeBattleDraw() { return LongBattleDraw(AtBattleDraw(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF; }

// sx, sy = the scales times the corner's float x, y; sz its z as it is (a mov);
// rhw = 0.1 / z; the diffuse as given. Specular, tu and tv are not written.
void PutCorner(unsigned char* out, const unsigned char* xyz, battle_draw::U diffuse) {
    PutFloatBattleDraw(out + 0x00, X87MulBattleDraw(D3d_ScaleX, FloatBattleDraw(xyz)));
    PutFloatBattleDraw(out + 0x04, X87MulBattleDraw(D3d_ScaleY, FloatBattleDraw(xyz + 4)));
    PutLongBattleDraw(out + 0x08, LongBattleDraw(xyz + 8));
    PutFloatBattleDraw(out + 0x0C, X87DivBattleDraw(FloatBattleDraw(AtBattleDraw(battle_draw::kRhwNumerator)), FloatBattleDraw(xyz + 8)));
    PutLongBattleDraw(out + 0x10, diffuse);
}

// The Gouraud handlers' shared shape: n colours (r, g, b at +4 + i * 0x10, the
// code byte +7 and the mode read again for each), all before any corner; then
// n corners of 0x10 bytes from +8 (float x, y, z).
void GouraudCorners(const unsigned char* prim, battle_draw::U n) {
    unsigned long diffuse[3];
    for (battle_draw::U i = 0; i < n; ++i) {
        const unsigned char* const rgb = prim + 4 + i * 0x10;
        battle_draw::g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], DrawModeBattleDraw(), &diffuse[i], nullptr);
    }
    for (battle_draw::U i = 0; i < n; ++i) PutCorner(VertexBattleDraw(i), prim + 8 + i * 0x10, static_cast<battle_draw::U>(diffuse[i]));
}

}  // namespace

namespace d3d_draw {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

std::int32_t Short(const unsigned char* p) {
    std::int16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

float Float(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

float FloatAt(U address) { return Float(At(address)); }

void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// fld a; fmul b; fstp
inline float X87Mul(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// fld a; fdiv b; fstp
inline float X87Div(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfdivs %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// fld a; fstp - a copy, except that a signalling NaN comes out quiet
inline float X87Pass(float a) {
    float r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(a) : "st");
    return r;
}

// fld a; fadd b; fmul c; fstp - (a + b) * c
inline float X87AddMul(float a, float b, float c) {
    float r;
    __asm__ volatile("flds %1\n\tfadds %2\n\tfmuls %3\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b), "m"(c) : "st");
    return r;
}

// fild i; fmul s; fadd b (or fsubr b: b - that); fmul c; fstp
inline float X87IntMulAddMul(std::int32_t i, float s, float b, float c, bool subtract) {
    float r;
    if (subtract)
        __asm__ volatile("fildl %1\n\tfmuls %2\n\tfsubrs %3\n\tfmuls %4\n\tfstps %0"
                         : "=m"(r)
                         : "m"(i), "m"(s), "m"(b), "m"(c)
                         : "st");
    else
        __asm__ volatile("fildl %1\n\tfmuls %2\n\tfadds %3\n\tfmuls %4\n\tfstps %0"
                         : "=m"(r)
                         : "m"(i), "m"(s), "m"(b), "m"(c)
                         : "st");
    return r;
}

// k / n, n an integer (exactly representable: the original's fild, or fild and
// fstp to a float, of a u16)
inline float X87DivInt(float k, std::int32_t n) {
    float r;
    __asm__ volatile("flds %1\n\tfidivl %2\n\tfstps %0" : "=m"(r) : "m"(k), "m"(n) : "st");
    return r;
}

// fild e; fsub k; then / n
inline float X87IntSubDivInt(std::int32_t e, float k, std::int32_t n) {
    float r;
    __asm__ volatile("fildl %1\n\tfsubs %2\n\tfidivl %3\n\tfstps %0" : "=m"(r) : "m"(e), "m"(k), "m"(n) : "st");
    return r;
}

// D3d_Device, read afresh before every COM call as the original does.
void** Device() { return reinterpret_cast<void**>(static_cast<std::uintptr_t>(*reinterpret_cast<const volatile std::uint32_t*>(At(::bof3::Img(D3d_Device_addr))))); }

void* Method(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }

using Com1 = long(__stdcall*)(void*);

using Com3 = long(__stdcall*)(void*, U, U);

using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

long SetRenderState(U state, U value) {
    void** device = Device();
    return reinterpret_cast<Com3>(Method(device, 0x58))(device, state, value);
}

long SetTexture(U stage, U texture) {
    void** device = Device();
    return reinterpret_cast<Com3>(Method(device, 0x98))(device, stage, texture);
}

long Scene(U offset) {   // 0x24 BeginScene, 0x28 EndScene
    void** device = Device();
    return reinterpret_cast<Com1>(Method(device, offset))(device);
}

// DrawPrimitive(type, D3DFVF_TLVERTEX, D3d_Vertices, count, 0).
long DrawVertices(U type, U count) {
    void** device = Device();
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, type, 0x1C4, ::bof3::Img(D3d_Vertices_addr), count, 0);
}

unsigned char* Vertex(U i) { return At(::bof3::Img(D3d_Vertices_addr) + i * 0x20); }

// sx, sy = scale times the primitive's float x, y; sz the primitive's z as it
// is (a mov); rhw = 0.1 / z.
void PutPosition(unsigned char* out, const unsigned char* xyz) {
    PutFloat(out + 0x00, X87Mul(D3d_ScaleX, Float(xyz)));
    PutFloat(out + 0x04, X87Mul(D3d_ScaleY, Float(xyz + 4)));
    PutLong(out + 0x08, Long(xyz + 8));
    PutFloat(out + 0x0C, X87Div(FloatAt(kRhwNumerator), Float(xyz + 8)));
}

// tu, tv = D3d_TexCoords[u], [v], through fld / fstp.
void PutTexel(unsigned char* out, const unsigned char* uv) {
    PutFloat(out + 0x18, X87Pass(FloatAt(::bof3::Img(kTexCoords.v) + uv[0] * 4u)));
    PutFloat(out + 0x1C, X87Pass(FloatAt(::bof3::Img(kTexCoords.v) + uv[1] * 4u)));
}

void QuadFarEdge(const unsigned char* uv, U stride) {
    if (!g_quad_far) return;
    float x[4], y[4], tu[4], tv[4];
    unsigned char u[4], v[4];
    for (U i = 0; i < 4; ++i) {
        const unsigned char* const out = Vertex(i);
        x[i] = Float(out + 0x00);
        y[i] = Float(out + 0x04);
        tu[i] = Float(out + 0x18);
        tv[i] = Float(out + 0x1C);
        u[i] = uv[i * stride];
        v[i] = uv[i * stride + 1];
    }
    const bool point = render::CurrentState().point_filter;
    quad_far_edge::ApplyAxis(x, y, u, tu, point);
    quad_far_edge::ApplyAxis(x, y, v, tv, point);
    for (U i = 0; i < 4; ++i) {
        PutFloat(Vertex(i) + 0x18, tu[i]);
        PutFloat(Vertex(i) + 0x1C, tv[i]);
    }
}

}  // namespace

}  // namespace d3d_draw

namespace {

// The line handlers and the tile: no texture (SetTexture(0, NULL)), the blend
// mode from Gfx_DrawTpage - read before the colour and read again for the
// blend - and no specular: the colour helper is handed a null pointer, so the
// vertices' specular, tu and tv are left as the last draw left them.
long DrawLines(const unsigned char* prim, d3d_draw::U corners) {
    const d3d_draw::U mode = d3d_draw::Long(d3d_draw::At(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF;
    unsigned long diffuse;
    d3d_draw::g.prim_color(prim[4], prim[5], prim[6], prim[7], mode, &diffuse, nullptr);
    for (d3d_draw::U i = 0; i < corners; ++i) {
        unsigned char* out = d3d_draw::Vertex(i);
        d3d_draw::PutPosition(out, prim + 8 + i * 0xC);
        d3d_draw::PutLong(out + 0x10, static_cast<d3d_draw::U>(diffuse));
    }
    d3d_draw::SetTexture(0, 0);
    d3d_draw::g.ret_only(0);
    d3d_draw::g.ret_only(1);
    d3d_draw::g.set_blend(prim[7], d3d_draw::Long(d3d_draw::At(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF);
    d3d_draw::g.set_shade(1);
    return d3d_lines::g_wide ? d3d_lines::DrawWide(corners) : d3d_draw::DrawVertices(3, corners);   // LINESTRIP; DIV-0079 the quads
}

void NoteCellFoot(d3d_draw::U slot, unsigned first, unsigned count) {
    if (slot >= 128) return;
    CellFoot f{0, 0, 0, 0, 0};
    for (unsigned k = 0; k < count; ++k) {
        const unsigned char* r = d3d_draw::At(::bof3::Img(SpriteCell_Table_addr) + (first + k) * 8u);
        const unsigned size = r[3], tpage = d3d_draw::Word(r + 4), u = r[6], v = r[7];
        const bool four_bit = (tpage & 0x180) == 0;
        const int w_tex = static_cast<int>(size & 0xF) * 8, h = static_cast<int>(size >> 4) * 8;
        const int x0 = static_cast<int>((tpage & 0xF) * 64u) + (four_bit ? static_cast<int>(u) / 4 : static_cast<int>(u) / 2);
        const int x1 = x0 + (four_bit ? w_tex / 4 : w_tex / 2) - 1;
        const int y0 = static_cast<int>((tpage & 0x10) << 4) + static_cast<int>(v);
        const int y1 = y0 + h - 1;
        if (!f.valid) { f = CellFoot{1, x0, y0, x1, y1}; continue; }
        if (x0 < f.x0) f.x0 = x0;
        if (y0 < f.y0) f.y0 = y0;
        if (x1 > f.x1) f.x1 = x1;
        if (y1 > f.y1) f.y1 = y1;
    }
    d3d_draw::g_cell_foot[slot] = f;
    if (d3d_draw::PoseLogOn() && f.valid)
        bof3::Log("poselog     frame %lu cell build entry %u cells %u..+%u foot %d,%d..%d,%d", static_cast<unsigned long>(Frame_Counter),
                  static_cast<unsigned>(slot), first, count, f.x0, f.y0, f.x1, f.y1);
}

}  // namespace

namespace d3d_list {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

U Word(U address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}

void Scene(U offset) {   // 0x24 BeginScene, 0x28 EndScene; the device re-read each time
    void** device = reinterpret_cast<void**>(static_cast<std::uintptr_t>(*reinterpret_cast<const volatile std::uint32_t*>(At(::bof3::Img(D3d_Device_addr)))));
    reinterpret_cast<long(__stdcall*)(void*)>((*reinterpret_cast<void***>(device))[offset / 4])(device);
}

// Codes 0xE0..0xE3 and 0xE8..0xEB, both tables: the PSX draw mode. Byte +6 bit
// 0 to Gfx_DrawEnable (which the NEXT draw tests on entry), bit 1 to 0x7DED16,
// the tpage word +4 to Gfx_DrawTpage, and the 8 bytes a non-null +8 points at to
// Gfx_TexCacheKey.
void DrawMode(U p) {
    Gfx_DrawEnable = At(p + 6)[0] & 1;
    At(::bof3::Img(Gfx_DrawEnable_addr) - 1)[0] = (At(p + 6)[0] >> 1) & 1;
    Gfx_DrawTpage = Word(p + 4);
    const U key = Long(p + 8);
    if (key != 0) {
        Gfx_TexCacheKey[0] = Long(key);
        Gfx_TexCacheKey[1] = Long(key + 4);
    }
}

// Codes 0xF0..0xF3: Gfx_TexCacheKey from the 8 bytes +8 points at - not
// checked for null, as the original does not.
void TexKey(U p) {
    const U key = Long(p + 8);
    Gfx_TexCacheKey[0] = Long(key);
    Gfx_TexCacheKey[1] = Long(key + 4);
}

// Codes 0xEC..0xEF: a VRAM move, the rect at +8, the destination +0x10, +0x14.
// DIV-0125 (render/fb_grab.h), after the original's move and not in
// it: a move out of a display buffer also has the backend grab that part of
// the picture drawn so far, as the PlayStation's GPU would have moved it. The
// rectangle is read before the move, which clips it in place.
void MoveImage(U p) {
    auto* rect = reinterpret_cast<short*>(At(p + 8));
    const int x = rect[0], y = rect[1], w = rect[2], h = rect[3];
    const int to_x = static_cast<int>(Long(p + 0x10)), to_y = static_cast<int>(Long(p + 0x14));
    g.move_image(rect, to_x, to_y);
    render::MoveFromFrame(x, y, w, h, to_x, to_y);
}

// A primitive's entry by its index, (code & 0xFC) - 0x20: the switch the
// compiler made the two byte tables of (kSoftBytes, 0xD1 bytes; kD3dBytes,
// 0xD5) - the draw's logic, ours since the function is (docs/operand-store.md).
// Entries 0..20 are the handlers of each table, by the PlayStation's GPU code:
// the eight polygons 0x20..0x3C (F3, FT3, F4, FT4, G3, GT3, G4, GT4), the six
// lines 0x40 and 0x48..0x5C (F2, F3, F4, G2, G3, G4), the rectangles 0x60,
// 0x64, 0x68, 0x6C, 0x74 and 0x7C (tile, sprite, 1 x 1 tile, the glyph, 8 x 8
// and 16 x 16 sprites) and the port's cell sprite 0x84; 21 the draw mode (0xE0
// and 0xE8), 22 the VRAM move (0xEC), 23 the texture key (0xF0) and, Direct3D
// only, 24 the alpha modulate (0xF4). Every other index is none: 24 in the
// software table, 25 in Direct3D's.
U Entry(U index, bool software) {
    switch (index) {
    case 0x00: case 0x04: case 0x08: case 0x0C: case 0x10: case 0x14: case 0x18: case 0x1C:
        return index / 4;   // the polygons
    case 0x20: return 8;
    case 0x28: return 9;
    case 0x2C: return 10;
    case 0x30: return 11;
    case 0x38: return 12;
    case 0x3C: return 13;
    case 0x40: return 14;
    case 0x44: return 15;
    case 0x48: return 16;
    case 0x4C: return 17;
    case 0x54: return 18;
    case 0x5C: return 19;
    case 0x64: return 20;
    case 0xC0: case 0xC8: return 21;
    case 0xCC: return 22;
    case 0xD0: return 23;
    case 0xD4: return 24;   // past the software table's 0xD1 bytes: Dispatch never asks it there
    default: return software ? 24 : 25;
    }
}

// One primitive: (code & 0xFC) - 0x20, unsigned, through the byte table to an
// entry. Entries 0..20 are handlers; the software table's 21..23 and the
// Direct3D table's 21..24 are the cases above; the last entry of each is none.
void Dispatch(U p, bool software) {
    const U index = (At(p + 7)[0] & 0xFCu) - 0x20u;
    if (index > (software ? 0xD0u : 0xD4u)) return;
    const U entry = Entry(index, software);
    if (entry < kHandlers) {
        if (stp_census::g_on && !software) stp_census::Note(p);   // BOF3X_STPCENSUS, diagnostic only: reads, draws nothing
        (software ? g.soft : g.d3d)[entry](At(p));
        return;
    }
    switch (entry) {
    case 21: DrawMode(p); return;
    case 22: MoveImage(p); return;
    case 23: TexKey(p); return;
    case 24:
        if (!software) g.alpha_modulate(At(p + 8)[0] ^ 1u);
        return;
    case 25:
        if (!software) return;
        break;
    default: break;
    }
    bof3::Fatal("Gfx_DrawOTag: byte table entry %u at code 0x%02X", (unsigned)entry, (unsigned)At(p + 7)[0]);
}

}  // namespace

}  // namespace d3d_list

namespace d3d_rest {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Addr(const void* p) { return Low32(p); }

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

std::int32_t Short(const unsigned char* p) {
    std::int16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

float Float(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

float FloatAt(U address) { return Float(At(address)); }

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// fld a; fmul b; fstp
inline float X87Mul(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// fld a; fdiv b; fstp
inline float X87Div(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfdivs %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// fld a; fstp - a copy, except that a signalling NaN comes out quiet
inline float X87Pass(float a) {
    float r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(a) : "st");
    return r;
}

// fild i; fmul k; fstp
inline float X87IntMul(std::int32_t i, float k) {
    float r;
    __asm__ volatile("fildl %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(i), "m"(k) : "st");
    return r;
}

// The CRT's _ftol 0x5B9550 on st0: round toward zero set in a copy of the
// control word for one fistp to 64 bits, the word put back; the low dword.
#define BOF3X_FTOL                       \
    "fnstcw %[saved]\n\t"                \
    "movw %[saved], %%ax\n\t"            \
    "orb $0x0C, %%ah\n\t"                \
    "movw %%ax, %[truncating]\n\t"       \
    "fldcw %[truncating]\n\t"            \
    "fistpll %[result]\n\t"              \
    "fldcw %[saved]\n\t"

// fld a; fmul b; _ftol
inline U X87MulFtol(float a, float b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile("flds %[a]\n\tfmuls %[b]\n\t" BOF3X_FTOL
                     : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
                     : [a] "m"(a), [b] "m"(b)
                     : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// fild i; fmul b; _ftol
inline U X87IntMulFtol(std::int32_t i, float b) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile("fildl %[i]\n\tfmuls %[b]\n\t" BOF3X_FTOL
                     : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
                     : [i] "m"(i), [b] "m"(b)
                     : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// fild qword chan; fild dword c; fmul k; fmulp st(1); _ftol - D3d_FlattenFT3's red
inline U X87RedFtol(std::int64_t chan, std::int32_t c, float k) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile("fildll %[chan]\n\tfildl %[c]\n\tfmuls %[k]\n\tfmulp %%st, %%st(1)\n\t" BOF3X_FTOL
                     : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
                     : [chan] "m"(chan), [c] "m"(c), [k] "m"(k)
                     : "eax", "st", "st(1)", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// fild dword c; fmul k; fimul dword chan; _ftol - its green and blue
inline U X87FimulFtol(std::int32_t c, float k, std::int32_t chan) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile("fildl %[c]\n\tfmuls %[k]\n\tfimull %[chan]\n\t" BOF3X_FTOL
                     : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
                     : [chan] "m"(chan), [c] "m"(c), [k] "m"(k)
                     : "eax", "st", "memory");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// D3d_Device, read afresh before every call as the original does.
void** Device() { return reinterpret_cast<void**>(static_cast<std::uintptr_t>(*reinterpret_cast<const volatile std::uint32_t*>(At(::bof3::Img(D3d_Device_addr))))); }

void* Method(void** object, U offset) { return (*reinterpret_cast<void***>(object))[offset / 4]; }

using Com2 = long(__stdcall*)(void*, U);

using Com3 = long(__stdcall*)(void*, U, U);

using Com4 = long(__stdcall*)(void*, U, U, U);

using Com5 = long(__stdcall*)(void*, U, U, U, U);

using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

long SetTexture(U stage, U texture) {
    void** device = Device();
    return reinterpret_cast<Com3>(Method(device, 0x98))(device, stage, texture);
}

// IDirect3DDevice3::SetTextureStageState(stage, type, value).
long SetStageState(U stage, U type, U value) {
    void** device = Device();
    return reinterpret_cast<Com4>(Method(device, 0xA0))(device, stage, type, value);
}

// DrawPrimitive(type, D3DFVF_TLVERTEX, D3d_Vertices, count, 0).
long DrawVertices(U type, U count) {
    void** device = Device();
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, type, 0x1C4, ::bof3::Img(D3d_Vertices_addr), count, 0);
}

// A surface pointer read from its global at the moment of the call.
void** SurfaceAt(U address) { return reinterpret_cast<void**>(static_cast<std::uintptr_t>(*reinterpret_cast<const volatile std::uint32_t*>(At(address)))); }   // the slot's 32-bit word

// IDirectDrawSurface4::Lock(rect, desc, flags, event), Unlock(rect), Blt(dst rect, src, src rect, flags, fx).
long Lock(void** surface, void* desc) {
    return reinterpret_cast<Com5>(Method(surface, 0x64))(surface, 0, Addr(desc), 1, 0);   // DDLOCK_WAIT
}

long Unlock(void** surface) { return reinterpret_cast<Com2>(Method(surface, 0x80))(surface, 0); }

unsigned char* Vertex(U i) { return At(::bof3::Img(D3d_Vertices_addr) + i * 0x20); }

U DrawMode() { return Long(At(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF; }

// sx, sy = scale times the corner's float x, y; sz its z as it is (a mov);
// rhw = 0.1 / z.
void PutPosition(unsigned char* out, const unsigned char* xyz) {
    PutFloat(out + 0x00, X87Mul(D3d_ScaleX, Float(xyz)));
    PutFloat(out + 0x04, X87Mul(D3d_ScaleY, Float(xyz + 4)));
    PutLong(out + 0x08, Long(xyz + 8));
    PutFloat(out + 0x0C, X87Div(FloatAt(kRhwNumerator), Float(xyz + 8)));
}

// tu, tv = D3d_TexCoords[u], [v], through fld / fstp.
void PutTexel(unsigned char* out, const unsigned char* uv) {
    PutFloat(out + 0x18, X87Pass(FloatAt(::bof3::Img(kTexCoords.v) + uv[0] * 4u)));
    PutFloat(out + 0x1C, X87Pass(FloatAt(::bof3::Img(kTexCoords.v) + uv[1] * 4u)));
}

}  // namespace

}  // namespace d3d_rest

namespace display_env {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

float Float(U address) {
    float v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

// The back buffer cleared: Gfx_RenderFlags bit 0 (the software surfaces) a
// colour fill of 0 through Blt, whatever the draw environment's colour;
// otherwise the viewport's Clear of Gfx_ScreenRect to its background - the
// material D3d_SetBackColor last set. The DDBLTFX is the original's stack
// block: 0x64 bytes zeroed, dwSize 0x64, dwFillColor (+0x50) 0 again.
void ClearBack() {
    if (Gfx_RenderFlags & 1) {
        unsigned char fx[kBltFxBytes];
        std::memset(fx, 0, sizeof fx);
        const U size = kBltFxBytes;
        std::memcpy(fx, &size, sizeof size);
        void* const back = DDraw_BackBuffer;
        Method<ComBlt>(back, kBlt)(back, nullptr, nullptr, nullptr, kBltWait | kBltColorFill, fx);
    } else {
        void* const viewport = D3d_Viewport;
        Method<ComClear>(viewport, kClear)(viewport, 1, At(::bof3::Img(Gfx_ScreenRect_addr)), kClearTarget);
    }
}

}  // namespace

}  // namespace display_env

namespace {

using U = std::uint32_t;

unsigned char* AtDisplaySetup(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

void PutLongDisplaySetup(U address, U v) { std::memcpy(AtDisplaySetup(address), &v, sizeof v); }

void PutWordDisplaySetup(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(AtDisplaySetup(address), &w, sizeof w);
}

void PutFloatDisplaySetup(U address, float v) { std::memcpy(AtDisplaySetup(address), &v, sizeof v); }

void PutPtr(U address, const void* p) { PutLongDisplaySetup(address, Low32(p)); }

void Zero(U address, U bytes) { std::memset(AtDisplaySetup(address), 0, bytes); }

// --- the globals (display-setup.md section 3) -------------------------------------
// kDesktopH: now PsxRest_TexCacheEnd (symbols.toml).
// kDesktopW: now Display_DesktopWidth (symbols.toml).
// kDesktopBpp: now Display_DesktopBpp (symbols.toml).
// kTexCache: now Gfx_TexCache (symbols.toml).
constexpr U kTexCacheBytes = 0x6000;

// kClutRows: now Gfx_ClutRows (symbols.toml).
constexpr U kClutRowsBytes = 0x1000;

constexpr ::bof3::image::Va kFontCache{0x7C9F50};

constexpr U kFontCacheBytes = 0xA00, kFontEntry = 0x14;

constexpr ::bof3::image::Va kBackdropBlockDisplaySetup{0x6BE9F8};

constexpr U kBackdropBytes = 0x1C;

// kAfterDrawBlock: now D3d_CaptureReady (symbols.toml).
constexpr U kAfterDrawBytes = 0x1C;

// kCellCache: now D3d_CellTexCache (symbols.toml).
constexpr U kCellCacheBytes = 0x1400;

// kPixelFormat: now Gfx_PixelFormat (symbols.toml).
constexpr U kPixelFormatRecord = 0x40;

// kShadeCache: now D3d_ShadeModeCache (symbols.toml).
constexpr ::bof3::image::Va kRenderFlagsDisplaySetup{0x6C3A4C};

// kUnread6C3A48: now Display_ZeroedUnread (symbols.toml).
// kTexCoords: now D3d_TexCoords (symbols.toml).
// kDeviceCountLess1: now Display_DeviceCountLess1 (symbols.toml).
constexpr ::bof3::image::Va kDeviceRecords{0x6C3A50};

constexpr U kDeviceRecord = 0x13C;

constexpr ::bof3::image::Va kModeLists{0x6BE1E8};

constexpr U kModeListsBytes = 0x810;

// kDirectDraw: now Dd_DirectDraw (symbols.toml).
// kClipper: now PsxRest_Clipper (symbols.toml).
// kZBuffer: now PsxRest_ZBuffer (symbols.toml).
constexpr ::bof3::image::Va kDirect3D{0x7CC34C};

constexpr ::bof3::image::Va kDeviceDesc{0x7CC238};

constexpr U kDeviceDescBytes = 0xFC;

// D3DDEVICEDESC fields D3d_FitTextureSize reads (tex-cells.md section 3).
constexpr U kDescTextureCaps = 0x84, kDescMinW = 0xAC, kDescMinH = 0xB0, kDescMaxW = 0xB4, kDescMaxH = 0xB8;

// What the HAL device on this machine reported through the original set-up,
// sampled read-only from a running game on 2026-09-23 (render-backend.md
// section 4): the texture caps and size limits, and the three pixel-format
// records. Reproduced so that the texture builders make the same textures.
struct Format {
    U bpp, rmask, gmask, bmask, amask;
};

// Sampled: 0x7DED60 = record 0 rank 2, 1-5-5-5 with DDPF_ALPHAPIXELS; record 1
// rank 3, 5-5-5 without; record 2 rank 1, X-8-8-8 32-bit; Gfx_RenderFlags
// 0x202 (bit 0x20 clear: an alpha format exists); D3DDEVICEDESC +0x84 0xCCD,
// min 1 x 1, max 0x4000 x 0x4000; D3d_ScaleX/Y 2.0; 0x7CADE0 16; the back
// buffer's pitch 0xA00 (32-bit); desktop 32 bpp.
constexpr Format kFormatAlpha = {16, 0x7C00, 0x03E0, 0x001F, 0x8000};   // record 0: textures with alpha

constexpr U kRankAlpha = 2;

constexpr Format kFormatOpaque = {16, 0x7C00, 0x03E0, 0x001F, 0};       // record 1: textures without

constexpr U kRankOpaque = 3;

constexpr Format kFormatScreen = {32, 0xFF0000, 0x00FF00, 0x0000FF, 0}; // record 2: the screen

constexpr U kRankScreen = 1;

constexpr bool kNoAlphaFormat = false;                                  // Gfx_RenderFlags bit 0x20

constexpr U kTextureCaps = 0xCCD;                                       // D3DPTEXTURECAPS_*

constexpr U kMinTexture = 1, kMaxTexture = 0x4000;

constexpr U kModeBits = 16;

U TopBit(U mask) {
    U top = 0;
    for (U i = 0; i < 32; ++i)
        if (mask & (1u << i)) top = i;
    return top;
}

// A Gfx_PixelFormat record as 0x5A62C0 fills it (display-setup.md section 5.6):
// +0 rank, +1 BGR order, +2 bpp, +3 bytes per pixel, +4/+8/+0xC the shift of
// each mask's top bit down from bit 23, +0x10.. the masks, +0x1C alpha,
// +0x20 the DDPIXELFORMAT.
void FillFormat(U record, const Format& f, U rank) {
    Zero(record, kPixelFormatRecord);
    AtDisplaySetup(record)[0] = static_cast<unsigned char>(rank);
    AtDisplaySetup(record)[1] = (f.rmask == 0x1F || f.rmask == 0xFF) ? 1 : 0;
    AtDisplaySetup(record)[2] = static_cast<unsigned char>(f.bpp);
    AtDisplaySetup(record)[3] = static_cast<unsigned char>(f.bpp / 8);
    PutLongDisplaySetup(record + 0x04, 23 - TopBit(f.rmask));
    PutLongDisplaySetup(record + 0x08, 23 - TopBit(f.gmask));
    PutLongDisplaySetup(record + 0x0C, 23 - TopBit(f.bmask));
    PutLongDisplaySetup(record + 0x10, f.rmask);
    PutLongDisplaySetup(record + 0x14, f.gmask);
    PutLongDisplaySetup(record + 0x18, f.bmask);
    PutLongDisplaySetup(record + 0x1C, f.amask);
    PutLongDisplaySetup(record + 0x20, 0x20);                        // dwSize
    PutLongDisplaySetup(record + 0x24, 0x40 | (f.amask ? 1 : 0));   // DDPF_RGB | DDPF_ALPHAPIXELS
    PutLongDisplaySetup(record + 0x2C, f.bpp);
    PutLongDisplaySetup(record + 0x30, f.rmask);
    PutLongDisplaySetup(record + 0x34, f.gmask);
    PutLongDisplaySetup(record + 0x38, f.bmask);
    PutLongDisplaySetup(record + 0x3C, f.amask);
}

// DIV-0042: the game's numbers for a target at k, written at set-up and again
// from the render fiber whenever the target is remade (render::RequestScale):
// D3d_ScaleX/Y, DIV-0010's far-edge table, Gfx_ScreenRect's size, and the
// primary and back surfaces' size as GetSurfaceDesc reports it.
void Rescale(U k) {
    display_setup::g_scale = k;
    const U width = render::TargetWidth(), height = render::TargetHeight();
    Gfx_ScreenRect[2] = width;
    Gfx_ScreenRect[3] = height;
    PutFloatDisplaySetup(::bof3::Img(D3d_ScaleX_addr), static_cast<float>(k));
    PutFloatDisplaySetup(::bof3::Img(D3d_ScaleY_addr), static_cast<float>(k));
    SprtDraw_SetScale(k);
    auto* primary = static_cast<render::Surface*>(DDraw_Primary.Native());   // the slots' 32-bit words
    auto* back = static_cast<render::Surface*>(DDraw_BackBuffer.Native());
    if (primary && back) {
        if (!render::ResizeSurface(primary, width, height) || !render::ResizeSurface(back, width, height))
            bof3::Fatal("Display_Setup: out of memory resizing the primary and back surfaces to %u x %u", width, height);
        Display_BackPitch = back->pitch;
    }
}

// DIVERGENCE DIV-0036: the render target is 320k x 240k, k chosen once at
// set-up (display-overhaul.md 4b; the owner's rule, 2026-09-23): a borderless
// window takes the largest k whose target fits its client - the monitor - and
// a window takes the launcher's size, BOF3X_SCALE, 2 when unset. A resize or
// F8 later changes only the present's integer scale of that target.
render::Options ReadOptions(HWND hwnd, bool borderless) {
    render::Options o = {};
    o.hwnd = hwnd;
    char text[32];
    // DIV-0042: the client decides, in both modes - the window opened at the
    // remembered size (win_main.cpp) or the BOF3X_SCALE fallback, the
    // borderless window at its monitor's.
    RECT client = {};
    GetClientRect(hwnd, &client);
    const U k = DisplaySetup_ScaleForClient(static_cast<U>(client.right - client.left), static_cast<U>(client.bottom - client.top));
    bof3::Log("DIV-0036    scale %u for the %s client %ld x %ld%s", k, borderless ? "borderless" : "windowed",
              client.right - client.left, client.bottom - client.top, DisplaySetup_Snap() ? "" : " (DIV-0042: fitted, not snapped)");
    o.logical_w = 320;
    o.logical_h = 240;
    o.scale = k;
    o.pad_x = Widescreen_Columns();   // DIV-0041: the target is (320 + 2 c) k wide (c 53 or 120), the view centred in it
    display_setup::g_scale = k;
    o.point_filter = false;
    if (GetEnvironmentVariableA("BOF3X_FILTER", text, sizeof text) > 0) {
        if (std::strcmp(text, "point") == 0) o.point_filter = true;
        else if (std::strcmp(text, "linear") != 0) bof3::Fatal("BOF3X_FILTER=%s: point or linear", text);
    }
    o.vsync = GetEnvironmentVariableA("BOF3X_VSYNC", nullptr, 0) > 0;
    o.snap = DisplaySetup_Snap();
    return o;
}

unsigned char* AtFieldMisc(field_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

field_misc::U LongFieldMisc(const unsigned char* p) {
    field_misc::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

float FloatFieldMisc(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutLongFieldMisc(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutFloatFieldMisc(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// --- the x87 sequences, as the draw handlers execute them (d3d_draw.cpp has the
// same; each rounds to the control word's precision exactly as Capcom's) ---
// fld a; fmul b; fstp
inline float X87MulFieldMisc(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// fld a; fdiv b; fstp
inline float X87DivFieldMisc(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfdivs %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// D3d_Device, read afresh before every COM call as the original does.
void** DeviceFieldMisc() { return reinterpret_cast<void**>(static_cast<std::uintptr_t>(*reinterpret_cast<const volatile std::uint32_t*>(AtFieldMisc(::bof3::Img(D3d_Device_addr))))); }

void* MethodFieldMisc(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }

long SetTextureFieldMisc(field_misc::U stage, field_misc::U texture) {
    void** device = DeviceFieldMisc();
    return reinterpret_cast<Com3>(MethodFieldMisc(device, 0x98))(device, stage, texture);
}

// DrawPrimitive(type, D3DFVF_TLVERTEX, D3d_Vertices, count, 0).
long DrawVerticesFieldMisc(field_misc::U type, field_misc::U count) {
    void** device = DeviceFieldMisc();
    return reinterpret_cast<Com6>(MethodFieldMisc(device, 0x70))(device, type, 0x1C4, ::bof3::Img(D3d_Vertices_addr), count, 0);
}

unsigned char* VertexFieldMisc(field_misc::U i) { return AtFieldMisc(::bof3::Img(D3d_Vertices_addr) + i * 0x20); }

field_misc::U DrawModeFieldMisc() { return LongFieldMisc(AtFieldMisc(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF; }

// sx, sy = the scales times the corner's float x, y; sz its z as it is (a mov);
// rhw = 0.1 / z. Diffuse as given; specular, tu and tv are not written.
void PutFlatCorner(unsigned char* out, const unsigned char* xyz, field_misc::U diffuse) {
    PutFloatFieldMisc(out + 0x00, X87MulFieldMisc(D3d_ScaleX, FloatFieldMisc(xyz)));
    PutFloatFieldMisc(out + 0x04, X87MulFieldMisc(D3d_ScaleY, FloatFieldMisc(xyz + 4)));
    PutLongFieldMisc(out + 0x08, LongFieldMisc(xyz + 8));
    PutFloatFieldMisc(out + 0x0C, X87DivFieldMisc(FloatFieldMisc(AtFieldMisc(field_misc::kRhwNumerator)), FloatFieldMisc(xyz + 8)));
    PutLongFieldMisc(out + 0x10, diffuse);
}

struct ClutRow {
    std::uint32_t generation;         // +0: incremented on every conversion of the row
    ::bof3::Slot32<void> pixels;      // +4: 0x1000 bytes (a Crt_malloc block, below 4 GB), or null until first wanted
};

ClutRow* Rows() { return reinterpret_cast<ClutRow*>(Gfx_ClutRows); }

struct PixelFormat {
    std::uint8_t unread[3];
    std::uint8_t bytes_per_pixel;   // +3
    std::uint32_t r_shift, g_shift, b_shift;
    std::uint32_t r_mask, g_mask, b_mask;
    std::uint32_t opaque_bits;      // +0x1C
};

const PixelFormat& FormatGfxClut() { return *reinterpret_cast<const PixelFormat*>(Gfx_PixelFormat); }

std::uint16_t* ShadowCell(int x, int y) { return Gfx_VramShadow + ((y << 10) + x); }

// x86 shr takes its count modulo 32; say so rather than leave it to the compiler.
std::uint32_t Shr(std::uint32_t v, std::uint32_t count) { return v >> (count & 31); }

// What the cache holds is two COM interfaces per entry, and all this file
// ever does with one is Release it: vtable slot 2, stdcall.
struct ComObject {
    struct Vtbl {
        void* query_interface;
        void* add_ref;
        unsigned long (__stdcall* release)(ComObject*);
    }* vtbl;
};

// One of 1,024 entries at 0x6C3F40: 32 texture pages of 64 x 256 (16 across,
// 2 down, as PSX VRAM), 32 entries a page, filled from slot 0 with no holes.
// Only the fields this file touches are named.
struct TexCacheEntry {
    std::uint8_t state;        // 0 free - and the end of the page's list; 1 built; 2 stale
    std::uint8_t mode;         // +1: PSX colour mode, (tpage >> 7) & 3: 0 4-bit, 1 8-bit, 2 15-bit.
                               //     Non-zero is wider than a page in VRAM, which is why such
                               //     an entry is also dropped when the NEXT page changes.
    std::uint16_t clut;        // +2: PSX CLUT id the texture was built with
    std::uint32_t generation;  // +4: that CLUT row's generation at the time (Gfx_ClutRows)
    std::uint32_t key[2];      // +8: Gfx_TexCacheKey at the time
    ::bof3::Slot32<ComObject> a;   // +0x10: the render shim's surface objects (ours, below 4 GB)
    ::bof3::Slot32<ComObject> b;   // +0x14
};

constexpr int kPerPage = 32;

constexpr int kEntries = 32 * kPerPage;

struct RectGfxTexcache { int left, top, right, bottom; };

// USER32 IntersectRect, which the original calls: right and bottom are
// exclusive, and an empty rect intersects nothing.
bool Intersects(const RectGfxTexcache& p, const RectGfxTexcache& q) {
    const int l = p.left > q.left ? p.left : q.left;
    const int r = p.right < q.right ? p.right : q.right;
    const int t = p.top > q.top ? p.top : q.top;
    const int b = p.bottom < q.bottom ? p.bottom : q.bottom;
    return l < r && t < b;
}

struct Tally { unsigned dropped = 0, closed_up = 0, staled = 0; };

// The whole of the original's effect on the table, with the one thing it does
// outside the table - Release - handed to the caller, so that the shadow check
// runs exactly the code the game runs.
//
// As the original has it: both rects are built INCLUSIVE (x + w - 1, page x +
// 0x3F) and then given to an exclusive intersection. So the last column and
// row of every page, and of the rect, do not count: a rect one cell wide or
// high invalidates nothing, and one that reaches only a page's column 63 or
// row 255 leaves that page alone.
template <class Release>
Tally Invalidate(TexCacheEntry* table, const short* rect, int mode, Release release) {
    Tally tally;
    const RectGfxTexcache dirty{rect[0], rect[1], rect[0] + rect[2] - 1, rect[1] + rect[3] - 1};

    int page = 0;
    for (int py = 0; py < 0x200; py += 0x100) {
        for (int px = 0; px < 0x400; px += 0x40, ++page) {
            if (!Intersects(RectGfxTexcache{px, py, px + 0x3F, py + 0xFF}, dirty)) continue;

            // The page's own entries. Mode 0 stops at the first free slot;
            // mode non-zero visits all 32.
            TexCacheEntry* own = table + page * kPerPage;
            for (int i = 0; i < kPerPage; ++i) {
                TexCacheEntry& e = own[i];
                if (mode != 0) {
                    if (e.state == 1) { e.state = 2; ++tally.staled; }
                    continue;
                }
                if (e.state == 0) break;
                if (e.b) release(e.b);
                if (e.a) release(e.a);
                std::memset(&e, 0, sizeof e);
                ++tally.dropped;
            }

            // Then the page to the left, for its 8- and 15-bit textures, which
            // reach into this one - not for the first page of a row. Here a dropped entry is
            // closed up rather than left as a hole, because a hole would end
            // that page's list for the loop above.
            if ((page & 0xF) == 0) continue;
            TexCacheEntry* left = own - kPerPage;
            for (int i = 0; i < kPerPage;) {
                TexCacheEntry& e = left[i];
                if (mode != 0) {
                    if (e.state == 1 && e.mode != 0) { e.state = 2; ++tally.staled; }
                    ++i;
                    continue;
                }
                if (e.state == 0) break;
                if (e.mode == 0) { ++i; continue; }
                if (e.b) release(e.b);
                if (e.a) release(e.a);
                std::memmove(&e, &e + 1, sizeof e * static_cast<unsigned>(kPerPage - 1 - i));
                std::memset(&left[kPerPage - 1], 0, sizeof e);
                ++tally.closed_up;
                // and look at slot i again: it now holds the next entry
            }
        }
    }
    return tally;
}

TexCacheEntry* Table() { return reinterpret_cast<TexCacheEntry*>(Gfx_TexCache); }

TexCacheEntry g_plan[kEntries];

struct {
    unsigned calls, mode0, mode1, mismatches;
    unsigned dropped, closed_up, staled, would_release;
} g_shadow;

void ShadowReport(const char* why) {
    bof3::Log("shadow      Gfx_InvalidateTextures %s: %u calls (%u mode 0, %u mode non-zero), "
              "%u MISMATCHES; ours dropped %u, closed up %u, staled %u, would release %u",
              why, g_shadow.calls, g_shadow.mode0, g_shadow.mode1, g_shadow.mismatches,
              g_shadow.dropped, g_shadow.closed_up, g_shadow.staled, g_shadow.would_release);
}

void ShadowCall(const short* rect, int mode) {
    const short r[4] = {rect[0], rect[1], rect[2], rect[3]};
    std::memcpy(g_plan, Table(), sizeof g_plan);
    unsigned releases = 0;
    const Tally t = Invalidate(g_plan, r, mode, [&](ComObject*) { ++releases; });

    gfx_texcache::g_original_clone(rect, mode);

    ++g_shadow.calls;
    ++(mode == 0 ? g_shadow.mode0 : g_shadow.mode1);
    g_shadow.dropped += t.dropped;
    g_shadow.closed_up += t.closed_up;
    g_shadow.staled += t.staled;
    g_shadow.would_release += releases;
    if (std::memcmp(g_plan, Table(), sizeof g_plan) != 0) {
        if (++g_shadow.mismatches <= 8) {
            int first = 0;
            while (std::memcmp(&g_plan[first], &Table()[first], sizeof(TexCacheEntry)) == 0) ++first;
            bof3::Log("shadow      Gfx_InvalidateTextures MISMATCH call %u: rect {%d,%d,%d,%d} mode %d, "
                      "first differing entry %d (page %d slot %d): ours state %u, original state %u",
                      g_shadow.calls, r[0], r[1], r[2], r[3], mode, first, first / kPerPage,
                      first % kPerPage, g_plan[first].state, Table()[first].state);
        }
    }
    if (g_shadow.calls == 1 || g_shadow.calls % 256 == 0) ShadowReport("so far");
}

}  // namespace

namespace glyph_draw {

namespace {

using U = std::uint32_t;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

std::int32_t Short(const unsigned char* p) {
    std::int16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// The device, read afresh before every COM call as the original does.
void** Device() { return reinterpret_cast<void**>(static_cast<std::uintptr_t>(*reinterpret_cast<const volatile std::uint32_t*>(At(::bof3::Img(D3d_Device_addr))))); }

void* Method(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }

using Com1 = long(__stdcall*)(void*);

using Com3 = long(__stdcall*)(void*, U, U);

using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

// kVertices: now D3d_Vertices (symbols.toml). D3d_Vertices: 4 x D3DTLVERTEX
// kScaleX: now D3d_ScaleX (symbols.toml).
// kScaleY: now D3d_ScaleY (symbols.toml).
constexpr U kSz = 0x3F7D70A4;         // 0.99f, as the original stores it: an immediate

constexpr U kRhw = 0x3DCCCCCD;        // 0.1f

}  // namespace

}  // namespace glyph_draw

namespace psx_rest {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

void PutWord(U address, std::uint16_t v) { std::memcpy(At(address), &v, sizeof v); }

void* Ptr(U address) { return At(Long(address)); }

// `rep stosd` of zeros: n dwords from address, first to last.
void ZeroDwords(U address, U n) {
    for (U i = 0; i < n; ++i) PutLong(address + i * 4, 0);
}

// One COM pointer released through its vtable's +8 if it is not null. The
// caller says whether the slot is zeroed after (inside the test, as the
// originals do it).
void ReleaseIf(U slot, bool zero) {
    void* const object = Ptr(slot);
    if (!object) return;
    Method<ComCall>(object, kRelease)(object);
    if (zero) PutLong(slot, 0);
}

}  // namespace

}  // namespace psx_rest

using sound::ComCall;

using sound::ComCreate;

namespace sound_rest {

namespace {

unsigned char* At(std::uint32_t address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

std::uint32_t Address(const volatile void* p) { return Low32(p); }

void SetDword(std::uint32_t address, std::uint32_t v) { std::memcpy(At(address), &v, sizeof v); }

void SetWord(std::uint32_t address, std::uint16_t v) { std::memcpy(At(address), &v, sizeof v); }

void Release(void* object) { sound::Method<ComCall>(object, sound::kRelease)(object); }

}  // namespace

}  // namespace sound_rest

namespace {

template <class T> void Put(unsigned char* p, unsigned offset, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(p + offset, &w, sizeof w);
    } else {
        std::memcpy(p + offset, &v, sizeof v);
    }
}

constexpr unsigned kPrimBytes = 0x20, kCellBytes = 8, kPartBytes = 5;

}  // namespace

namespace sprt_draw {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

float Float(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

float FloatAt(U address) { return Float(At(address)); }

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

// fld a; fmul b; fstp
inline float X87Mul(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfmuls %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// fld a; fdiv b; fstp
inline float X87Div(float a, float b) {
    float r;
    __asm__ volatile("flds %1\n\tfdivs %2\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b) : "st");
    return r;
}

// fld a; fstp - a copy, except that a signalling NaN comes out quiet
inline float X87Pass(float a) {
    float r;
    __asm__ volatile("flds %1\n\tfstps %0" : "=m"(r) : "m"(a) : "st");
    return r;
}

// fld a; fadd b; fmul c; fstp - (a + b) * c, one rounding after each
inline float X87AddMul(float a, float b, float c) {
    float r;
    __asm__ volatile("flds %1\n\tfadds %2\n\tfmuls %3\n\tfstps %0" : "=m"(r) : "m"(a), "m"(b), "m"(c) : "st");
    return r;
}

// fild i; fadd b; fmul c; fstp - (i + b) * c
inline float X87IntAddMul(std::int32_t i, float b, float c) {
    float r;
    __asm__ volatile("fildl %1\n\tfadds %2\n\tfmuls %3\n\tfstps %0" : "=m"(r) : "m"(i), "m"(b), "m"(c) : "st");
    return r;
}

// D3d_Device, read afresh before the COM call as the original does.
void** Device() { return reinterpret_cast<void**>(static_cast<std::uintptr_t>(*reinterpret_cast<const volatile std::uint32_t*>(At(::bof3::Img(D3d_Device_addr))))); }

void* Method(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }

using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

unsigned char* Vertex(U i) { return At(::bof3::Img(D3d_Vertices_addr) + i * 0x20); }

// The body all three share. `size_at` is 0 for SPRT - the size is the
// primitive's u16 w +0x18 and h +0x1A, pushed through fild - or the address of
// the fixed size's float, 8.0 or 16.0, with `extent` 8 or 16 its far index.
//
// The primitive (Menu_DrawPiece 0x57D860 builds a SPRT, the 8 x 8 font 0x517090
// a SPRT_8): r, g, b, code at +4..+7; float x +8, y +0xC, z +0x10; u8 u +0x14,
// v +0x15; u16 CLUT +0x16; SPRT only: u16 w +0x18, h +0x1A.
//
// Corners, in the strip order: (x, y), (x + w, y), (x, y + h), (x + w, y + h),
// each times the scale; texture (near u, near v), (far u, near v), (near u,
// far v), (far u, far v). Every one of the 0x80 bytes is written. The far
// edges' sums are made on the x87 before the multiply, and the copies the
// original makes by `mov` (the second corner's sy, the fourth's sx, the
// repeated texture coordinates) are the same bits here.
//
// Every read of the primitive, the scales and the tables is between the
// colour helper and the texture bind, as in the original; the code byte and
// Gfx_DrawTpage are read again for the blend after the texture and the two
// bare rets - kept, since a callee could change either.
long DrawSprite(const unsigned char* prim, U size_at, U extent) {
    unsigned long diffuse, specular;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], Long(At(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF, &diffuse, &specular);

    const float x = Float(prim + 8), y = Float(prim + 0xC), z = Float(prim + 0x10);
    const U u = prim[0x14], v = prim[0x15];
    const float left = X87Mul(D3d_ScaleX, x);
    const float top = X87Mul(D3d_ScaleY, y);
    const float near_u = X87Pass(FloatAt(::bof3::Img(kTexCoords) + u * 4)), near_v = X87Pass(FloatAt(::bof3::Img(kTexCoords) + v * 4));
    float right, bottom;
    U far_j_u, far_j_v;
    if (size_at == 0) {
        // u16, zero-extended: a w of 0x8000 or more is a wide sprite, never a
        // negative one.
        const U w = Word(prim + 0x18), h = Word(prim + 0x1A);
        right = X87IntAddMul(static_cast<std::int32_t>(w), x, D3d_ScaleX);
        bottom = X87IntAddMul(static_cast<std::int32_t>(h), y, D3d_ScaleY);
        far_j_u = u + w;
        far_j_v = v + h;
    } else {
        right = X87AddMul(x, FloatAt(size_at), D3d_ScaleX);
        bottom = X87AddMul(y, FloatAt(size_at), D3d_ScaleY);
        far_j_u = u + extent;
        far_j_v = v + extent;
    }
    // The far edge: Capcom's tc[j - 1], or DIV-0010's g_far[j]. Not bounded,
    // in either (D37): the address is 32-bit arithmetic, as the original's
    // [reg * 4 + disp32].
    const U base = g_far_base;
    const float far_u = X87Pass(FloatAt(base + far_j_u * 4));
    const float far_v = X87Pass(FloatAt(base + far_j_v * 4));
    const float sz = X87Pass(z);
    const float rhw = X87Div(FloatAt(kRhwNumerator), z);

    const float xs[4] = {left, right, left, right}, ys[4] = {top, top, bottom, bottom};
    const float us[4] = {near_u, far_u, near_u, far_u}, vs[4] = {near_v, near_v, far_v, far_v};
    for (U i = 0; i < 4; ++i) {
        unsigned char* out = Vertex(i);
        PutFloat(out + 0x00, xs[i]);
        PutFloat(out + 0x04, ys[i]);
        PutFloat(out + 0x08, sz);
        PutFloat(out + 0x0C, rhw);
        PutLong(out + 0x10, static_cast<U>(diffuse));
        PutLong(out + 0x14, static_cast<U>(specular));
        PutFloat(out + 0x18, us[i]);
        PutFloat(out + 0x1C, vs[i]);
    }

    g.bind_texture(Long(At(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF, Word(prim + 0x16));
    g.ret_only(1);
    g.ret_only(1);
    g.set_blend(prim[7], Long(At(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF);
    g.set_shade(1);   // flat
    // DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, D3d_Vertices, 4, 0);
    // its result is the handler's (the draw 0x59EE50 does not read it).
    void** device = Device();
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, 5, 0x1C4, ::bof3::Img(D3d_Vertices_addr), 4, 0);
    // Not kept: the original lets the colour helper write the diffuse into
    // the caller's pushed `prim` slot, which the caller pops unread, and uses
    // a local as fild's operand.
}

}  // namespace

}  // namespace sprt_draw

namespace tex_cells {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

void* Ptr(U address) { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(address)); }

U Addr(const void* p) { return Low32(p); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Word(U address) { return Word(At(address)); }

std::int32_t Short(U address) {
    std::int16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}

void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

// One COM call: slot `offset` of the object's vtable, stdcall, `this` first.
template <typename... A> long Com(U object, U offset, A... args) {
    void* o = Ptr(object);
    auto fn = reinterpret_cast<long(__stdcall*)(void*, A...)>((*reinterpret_cast<void***>(o))[offset / 4]);
    return fn(o, args...);
}

// IDirectDrawSurface4 byte offsets; the device's two.
constexpr U kRelease = 0x08, kBlt = 0x14, kLock = 0x64, kSetColorKey = 0x74, kUnlock = 0x80;

constexpr U kBeginScene = 0x24, kEndScene = 0x28;   // IDirect3DDevice3

constexpr U kLockWait = 1;                           // DDLOCK_WAIT

constexpr U kBltWait = 0x1000000;                    // DDBLT_WAIT

constexpr U kBltColorFillWait = 0x1000400;           // DDBLT_COLORFILL | DDBLT_WAIT

constexpr U kSrcBltKey = 8;                          // DDCKEY_SRCBLT

void* const kNoRect = nullptr;

// A DDSURFACEDESC2 on the stack: lPitch +0x10, lpSurface +0x24.
struct alignas(4) Desc {
    unsigned char b[0x7C];
    U Pitch() const { return Long(b + 0x10); }
    U Surface() const { return Long(b + 0x24); }
};

struct Rect {
    std::int32_t l, t, r, b;
};

// The Direct3D build's height for a texture narrower than the cells' extent:
// `fild` the texture width, `fidiv` the used width, `fimul` the used height,
// then the CRT's _ftol 0x5B9550 - round-toward-zero set in a copy of the
// control word for one `fistp` to 64 bits (0x5A36FA..0x5A3713). Rounded at the
// control word's precision, as the original; the caller keeps the low 16 bits.
U ScaledHeight(std::int32_t texture_width, std::int32_t used_width, std::int32_t used_height) {
    std::int64_t result;
    unsigned short saved, truncating;
    __asm__ volatile(
        "fildl %[tw]\n\t"
        "fidivl %[uw]\n\t"
        "fimull %[uh]\n\t"
        "fnstcw %[saved]\n\t"
        "movw %[saved], %%ax\n\t"
        "orb $0x0C, %%ah\n\t"
        "movw %%ax, %[truncating]\n\t"
        "fldcw %[truncating]\n\t"
        "fistpll %[result]\n\t"
        "fldcw %[saved]\n\t"
        : [result] "=m"(result), [saved] "=m"(saved), [truncating] "=m"(truncating)
        : [tw] "m"(texture_width), [uw] "m"(used_width), [uh] "m"(used_height)
        : "eax", "st");
    return static_cast<U>(static_cast<std::uint64_t>(result));
}

// One pass over the cell records [first, first + count) onto the locked
// staging surface: D3d_BuildCellTexture's loop (0x5A33BA..0x5A351E) and
// D3d_RefreshCellTexture's (0x5A385A..0x5A3944), which differ only in the
// extent and checksum the build keeps. Each record: s16 x +0, s8 y +2, the
// size byte +3 (w = low nibble * 8, h = high nibble * 8), the tpage word +4
// (bits 0..3 the page column, 4 the page row, 7..8 the colour mode - 0 4-bit,
// anything else 8-bit - and 9 mirrored), u +6, v +7. The texels start at the
// page's VRAM plus v rows and u bytes (u / 2 for 4-bit); they go to (x + 160,
// y + 128) of the stage, 160 and 128 its centre, at the pitch and the depth
// (byte 0x7DED63, read per record) as they are then - nothing bounds either.
// Every field is read before the record's unpacker runs; the build's checksum
// adds the record's two dwords after it.
struct Extent {
    std::int32_t left, top, right, bottom;
    U checksum;
};

void DrawCells(U first, U count, const void* palette, const tex_cells::Desc& desc, Extent* extent) {
    if (static_cast<std::int32_t>(first) >= static_cast<std::int32_t>(first + count)) return;
    U record = ::bof3::Img(SpriteCell_Table_addr) + first * 8;
    U n = count;
    do {
        const unsigned char* r = At(record);
        const std::int32_t x = Short(record);
        const std::int32_t y = static_cast<std::int8_t>(r[2]);
        const U tpage = Word(r + 4);
        const U mode = tpage & 0x180;
        const U u = mode != 0 ? r[6] : static_cast<U>(r[6]) >> 1;
        const U w = (r[3] & 0xFu) << 3;
        const U h = (static_cast<U>(r[3]) >> 1) & 0x78;
        const U src = ::bof3::Img(Gfx_VramShadow_addr) + (((((tpage & 0x10) << 4) + r[7]) << 11) + u) + ((tpage & 0xF) << 7);
        const U bpp = At(::bof3::Img(Gfx_PixelFormat_addr))[3];
        const U dst = desc.Surface() + static_cast<U>(x + 0xA0) * bpp + static_cast<U>(y + 0x80) * desc.Pitch();
        const bool flip = (Word(r + 4) & 0x200) != 0;
        const Unpack unpack = flip ? (mode != 0 ? g.unpack8_flip : g.unpack4_flip) : (mode != 0 ? g.unpack8 : g.unpack4);
        unpack(Ptr(dst), Ptr(src), palette, static_cast<int>(w), static_cast<int>(h), static_cast<int>(desc.Pitch()));
        if (extent) {
            if (extent->left > x) extent->left = x;
            if (extent->right < x + static_cast<std::int32_t>(w)) extent->right = x + static_cast<std::int32_t>(w);
            if (extent->top > y) extent->top = y;
            if (extent->bottom < y + static_cast<std::int32_t>(h)) extent->bottom = y + static_cast<std::int32_t>(h);
            extent->checksum += Long(record);
            extent->checksum += Long(record + 4);
        }
        record += 8;
    } while (--n != 0);
}

}  // namespace

}  // namespace tex_cells

namespace tex_page {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

std::int32_t Short(U address) {
    std::int16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

// One COM call: slot `offset` of the object's vtable, stdcall, `this` first.
template <typename... A> long Com(U object, U offset, A... args) {
    void* o = reinterpret_cast<void*>(static_cast<std::uintptr_t>(object));
    auto fn = reinterpret_cast<long(__stdcall*)(void*, A...)>((*reinterpret_cast<void***>(o))[offset / 4]);
    return fn(o, args...);
}

// IDirectDrawSurface4 and IDirectDraw4 slots, as byte offsets.
constexpr U kQueryInterface = 0x00, kBlt = 0x14, kCreateSurface = 0x18, kLock = 0x64, kSetColorKey = 0x74,
            kUnlock = 0x80;

constexpr U kLockWait = 1;             // DDLOCK_WAIT

constexpr U kBltWait = 0x1000000;      // DDBLT_WAIT

constexpr U kSrcBltKey = 8;            // DDCKEY_SRCBLT

constexpr void* kNoRect = nullptr;     // Lock's RECT *: the whole surface

// A DDSURFACEDESC2 on the stack, as the original keeps one: dwSize +0,
// dwFlags +4, dwHeight +8, dwWidth +0xC, lPitch +0x10, lpSurface +0x24, the
// DDPIXELFORMAT +0x48, ddsCaps +0x68 (dwCaps, dwCaps2 +0x6C, ...),
// dwTextureStage +0x78.
struct alignas(4) Desc {
    unsigned char b[0x7C];
    U Pitch() const { return Long(b + 0x10); }
    void* Surface() const { return reinterpret_cast<void*>(static_cast<std::uintptr_t>(Long(b + 0x24))); }
};

// The rectangle every Blt of a page names on both sides: {0, 0, 256, 256}.
struct Rect {
    std::int32_t l, t, r, b;
};

// Where the page's texels start in the VRAM shadow: the page's column
// ((page & 0xF) * 64 cells = * 0x80 bytes) and row block ((page & 0x10) * 16
// rows), moved by the key's y and by its x in the colour mode's units - half a
// byte a texel for mode 0, a byte for 1, two for 2, nothing for any other mode.
U Source(U page, U mode, std::int32_t key_x, std::int32_t key_y) {
    const U y = ((page & 0x10) << 4) + static_cast<U>(key_y);
    U x = (page & 0xF) << 7;
    switch (mode) {
    case 0: x += static_cast<U>(key_x >> 1); break;
    case 1: x += static_cast<U>(key_x); break;
    case 2: x += static_cast<U>(key_x) * 2; break;
    default: break;
    }
    return ::bof3::Img(Gfx_VramShadow_addr) + (y << 11) + x;
}

U Generation(U row_address) { return Long(row_address); }

}  // namespace

}  // namespace tex_page

// psx_rest's callee seam, moved here from psx_rest.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace psx_rest {
namespace {

// Win32 through the game's own import slots, as the originals call it.
template <class Fn>
Fn Import(U slot) {
    std::uint32_t word;   // the import slot is a 32-bit word: Windows' function on i686, its thunk on x86-64 (design 4)
    std::memcpy(&word, At(slot), sizeof word);
    return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(word));
}
unsigned long __stdcall ImportSetTextColor(void* hdc, unsigned long color) {
    return Import<unsigned long (__stdcall*)(void*, unsigned long)>(::bof3::Img(PsxRest_ImportSetTextColor_addr))(hdc, color);
}
int __stdcall ImportSetBkMode(void* hdc, int mode) {
    return Import<int (__stdcall*)(void*, int)>(::bof3::Img(PsxRest_ImportSetBkMode_addr))(hdc, mode);
}
int __stdcall ImportTextOutA(void* hdc, int x, int y, const char* text, int n) {
    return Import<int (__stdcall*)(void*, int, int, const char*, int)>(::bof3::Img(PsxRest_ImportTextOutA_addr))(hdc, x, y, text, n);
}
int __stdcall ImportMessageBoxA(void* hwnd, const char* text, const char* caption, unsigned type) {
    return Import<int (__stdcall*)(void*, const char*, const char*, unsigned)>(::bof3::Img(PsxRest_ImportMessageBoxA_addr))(hwnd, text, caption,
                                                                                                  type);
}

}  // namespace

const Callees kOriginals = {
    Gfx_FreeClutRows, Gfx_ReleaseTexCache, Font_ReleaseTexCache, Display_ReleaseOrphans, Display_ReleaseBackdrop,
    D3d_FreeCellTextures, D3d_ReleaseAfterDraw,
    Crt_free, D3d_FreeCellTexture, SndStream_Stop, Music_Release,
    ImportSetTextColor, ImportSetBkMode, ImportTextOutA, ImportMessageBoxA,
};
Callees g = kOriginals;

}  // namespace psx_rest

// tex_cells's callee seam, moved here from tex_cells.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace tex_cells {

const Callees kOriginals = {
    Font_UnpackGlyph, Cell_Unpack4, Cell_Unpack8, Cell_Unpack4Flip, Cell_Unpack8Flip, Gfx_ClutPixels,
};
Callees g = kOriginals;

}  // namespace tex_cells

// d3d_draw's callee seam, moved here from d3d_draw.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace d3d_draw {

const Callees kOriginals = {
    D3d_PrimColor,
    D3d_BindTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(kRetOnly.v))),
    D3d_SetBlend,
    D3d_SetShadeMode,
    D3d_CellTexture,
    Gfx_TexCacheFind,
    D3d_BuildPageTexture,
    D3d_RefreshPageTexture,
    D3d_BuildCellTexture,
    D3d_RefreshCellTexture,
};
Callees g = kOriginals;

}  // namespace d3d_draw

// d3d_list's callee seam, moved here from d3d_list.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace d3d_list {
namespace {

using Handler = long(__cdecl*)(const unsigned char*);
Handler H(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(::bof3::CodePlace(address))); }

}  // namespace

// The handlers of each table, by entry, as the original's case blocks call them.
const Callees kOriginals = {
    {
        // the software surfaces, first table 0x59F2A0
        H(0x5A3A60), H(0x5A3B60), H(0x5A3D70), H(0x5A3E90), H(0x5A3FB0), H(0x5A40C0), H(0x5A41A0),
        H(0x5A42E0), H(kRetOnly.v), H(kRetOnly.v), H(kRetOnly.v), H(kRetOnly.v), H(kRetOnly.v), H(kRetOnly.v),
        H(0x5A4400), H(0x5A45B0), H(0x5A4500), H(0x5A4900), H(0x5A46E0), H(0x5A47F0), H(0x5A4C40),
    },
    {
        // Direct3D, second table 0x59F3D8
        H(bof3::addr::D3d_DrawPolyF3), H(bof3::addr::D3d_DrawPolyFT3), H(bof3::addr::D3d_DrawPolyF4), H(bof3::addr::D3d_DrawPolyFT4), H(bof3::addr::D3d_DrawPolyG3),
        H(bof3::addr::D3d_DrawPolyGT3), H(bof3::addr::D3d_DrawPolyG4),
        H(bof3::addr::D3d_DrawPolyGT4), H(bof3::addr::D3d_DrawLineF2), H(bof3::addr::D3d_DrawLineF3), H(bof3::addr::D3d_DrawLineF4), H(bof3::addr::D3d_DrawLineG2), H(bof3::addr::D3d_DrawLineG3), H(bof3::addr::D3d_DrawLineG4),
        H(bof3::addr::D3d_DrawTile), H(bof3::addr::D3d_DrawSprt), H(bof3::addr::D3d_DrawTile1), H(bof3::addr::D3d_DrawGlyph), H(bof3::addr::D3d_DrawSprt8), H(bof3::addr::D3d_DrawSprt16), H(bof3::addr::D3d_DrawCellSprite),
    },
    Gfx_MoveImage,
    // by the original's address (group PH, docs/d3d-rest.md): BOF3X_ORIGINAL=<name> restores Capcom's for the walk
    bof3::orig::D3d_SetAlphaModulate,
    bof3::orig::D3d_AfterDraw,
};
Callees g = kOriginals;

}  // namespace d3d_list

// d3d_rest's callee seam, moved here from d3d_rest.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace d3d_rest {

const Callees kOriginals = {
    D3d_PrimColor,
    D3d_BindTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(kRetOnly.v))),
    D3d_SetBlend,
    D3d_SetShadeMode,
    Gfx_ClutPixels,
    Dd_InitSurfaceDesc,
    Dd_CreatePlainSurface,
    D3d_FitTextureSize,
    Dd_CreateTextureSurface,
};
Callees g = kOriginals;

}  // namespace d3d_rest

// glyph_draw's callee seam, moved here from glyph_draw.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace glyph_draw {

const Callees kOriginals = {
    D3d_PrimColor,
    Font_GlyphTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(kRetOnly.v))),
    D3d_SetBlend,
    D3d_SetShadeMode,
    Font_BuildGlyphTexture,
};
Callees g = kOriginals;

}  // namespace glyph_draw

// sprt_draw's callee seam, moved here from sprt_draw.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace sprt_draw {

const Callees kOriginals = {
    D3d_PrimColor,
    D3d_BindTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(kRetOnly.v))),
    D3d_SetBlend,
    D3d_SetShadeMode,
};
Callees g = kOriginals;

}  // namespace sprt_draw

// tex_page's callee seam, moved here from tex_page.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace tex_page {

const Callees kOriginals = {
    Tex_Convert4,
    Tex_Convert8,
    Tex_Convert16,
    Gfx_ClutPixels,
};
Callees g = kOriginals;

}  // namespace tex_page

// original 0x59E650. Fills rect with one BYTE, then drops the textures under
// it. As the original has it:
//   - the PSX call is ClearImage(rect, r, g, b); this one reads two colour
//     arguments and makes ((g & 0xF) << 2) | (r >> 3) of them - not a 15-bit
//     colour, and only its low byte is used, for both bytes of every cell. The
//     one caller found (0x461E46) passes 0, 0, where none of it matters.
//   - no bounds check of any kind; h is read from the rect again every row.
extern "C" void __cdecl Gfx_ClearImage(const short* rect, unsigned char red, unsigned char green) {
    const auto fill = static_cast<std::uint8_t>(((green & 0x0F) << 2) | (red >> 3));
    auto* dst = reinterpret_cast<std::uint8_t*>(Gfx_VramShadow) + ((rect[1] << 10) + rect[0]) * 2;
    const std::uint32_t row_bytes = static_cast<std::uint32_t>(rect[2] << 1);
    for (int row = 0; row < rect[3]; ++row) {
        std::memset(dst, fill, row_bytes);
        dst += 0x800;
    }
    Gfx_InvalidateTextures(rect, 0);
}

// original 0x59E700. Throws away (mode 0) or marks stale (mode non-zero) every
// cached texture built from the VRAM cells under rect: four s16, x y w h.
// Callers: Gfx_LoadImage and 0x59E6EA (the end of a row-by-row fill of the
// shadow) with mode 0; 0x59EA5C, after 0x5AA5D6 has been given the shadow and a
// rect, with mode 1.
extern "C" void __cdecl Gfx_InvalidateTextures(const short* rect, int mode) {
    // DIV-0125 (render/fb_grab.h), not in the original: every write of
    // the VRAM shadow passes here, and ends any frame-buffer grab it covers.
    render::ShadowWritten(rect[0], rect[1], rect[2], rect[3]);
    D3dDraw_CellsShadowWritten(rect[0], rect[1], rect[2], rect[3]);   // DIV-0181
    if (gfx_texcache::g_original_clone) {
        ShadowCall(rect, mode);
        return;
    }
    Invalidate(Table(), rect, mode, [](ComObject* o) { o->vtbl->release(o); });
}

// 0x59E930 (no PSX pair read; the PC counterpart of libgpu's StoreImage, as
// Gfx_LoadImage 0x59EA70 is of LoadImage): the rectangle (s16 x, y, w, h) of
// Gfx_VramShadow copied row by row to `to`, packed (w * 2 bytes a row). x, y
// and w are read once; h is read again after every row (through the rectangle's
// pointer: a rectangle the copy overwrites changes the count), and nothing is
// copied when it is not above 0 (16-bit, signed). No bounds are checked: a
// rectangle outside the 1024 x 512 shadow reads past it, and a negative w is a
// copy of about 4 GB - as the original. Each row a forward copy of dwords then
// the 0..3 bytes left (rep movsd, rep movsb). Returns w * 2, which none of its
// four callers (the effect states 0x475EC0, 0x480730, 0x490BB0, 0x491410, all
// ours) reads.
int Gfx_StoreImage(const short* rect, void* to) {
    const unsigned char* r = reinterpret_cast<const unsigned char*>(rect);
    const d3d_rest::U x = static_cast<d3d_rest::U>(d3d_rest::Short(r)), y = static_cast<d3d_rest::U>(d3d_rest::Short(r + 2));
    d3d_rest::U from = ::bof3::Img(Gfx_VramShadow_addr) + ((y << 10) + x) * 2;
    const d3d_rest::U bytes = static_cast<d3d_rest::U>(d3d_rest::Short(r + 4)) << 1;
    if (d3d_rest::Short(r + 6) <= 0) return static_cast<int>(bytes);
    d3d_rest::U dest = d3d_rest::Addr(to);
    std::int32_t i = 0;
    do {
        d3d_rest::U s = from, d = dest;
        for (d3d_rest::U n = bytes >> 2; n != 0; --n, s += 4, d += 4) {
            d3d_rest::U v;
            std::memcpy(&v, d3d_rest::At(s), 4);
            std::memcpy(d3d_rest::At(d), &v, 4);
        }
        for (d3d_rest::U n = bytes & 3; n != 0; --n, ++s, ++d) d3d_rest::At(d)[0] = d3d_rest::At(s)[0];
        from += 0x800;
        dest += bytes;
        ++i;
    } while (i < d3d_rest::Short(r + 6));
    return static_cast<int>(bytes);
}

// original 0x59E9A0. Clips rect to the shadow IN PLACE, moves it to
// (to_x, to_y), rewrites rect's x and y as the destination, and marks the
// textures under the destination stale (mode 1) rather than dropping them.
// As the original has it:
//   - the caller's rect is changed, as above;
//   - a source hanging off the left or top is clipped and the destination
//     shifted by the same (negative) amount; one off the right or bottom is
//     only shortened; the DESTINATION is never clipped at all;
//   - w == 0 or h == 0 returns at once, but clipping can produce either - or a
//     negative - afterwards, and Gfx_MoveCells takes them as they come.
extern "C" void __cdecl Gfx_MoveImage(short* rect, int to_x, int to_y) {
    if (rect[2] == 0 || rect[3] == 0) return;

    const short x = rect[0];
    if (x < 0) {
        rect[2] = static_cast<short>(rect[2] + x);
        rect[0] = 0;
        to_x += x;
    } else if (x + rect[2] > 0x400) {
        rect[2] = static_cast<short>(0x400 - x);
    }
    const short y = rect[1];
    if (y < 0) {
        rect[3] = static_cast<short>(rect[3] + y);
        rect[1] = 0;
        to_y += y;
    } else if (y + rect[3] > 0x200) {
        rect[3] = static_cast<short>(0x200 - y);
    }

    Gfx_MoveCells(Gfx_VramShadow, rect, to_x, to_y);
    rect[0] = static_cast<short>(to_x);
    rect[1] = static_cast<short>(to_y);
    Gfx_InvalidateTextures(rect, 1);
}

// original 0x59EA70. The PC counterpart of the PSX LoadImage(RECT*, u_long*):
// copies h rows of w 16-bit cells into the 1024 x 512 shadow of PSX VRAM, then
// throws away every cached texture built from the cells it overwrote.
//
// As the original has it:
//   - only the upper bounds are checked. A negative x or y passes and writes
//     before the rectangle it names; a negative w reaches the copy as a huge
//     length. Nothing in the shipped data does either.
//   - a rect that fails the check is dropped whole, with no invalidation.
//   - h is read from the rect again on every row, and a rect with h <= 0 that
//     passes the check copies nothing but still invalidates.
extern "C" void __cdecl Gfx_LoadImage(const short* rect, const void* pixels) {
    const int x = rect[0], w = rect[2];
    if (x + w > 0x400) return;
    const int y = rect[1];
    if (y + rect[3] > 0x200) return;

    auto* dst = reinterpret_cast<std::uint8_t*>(Gfx_VramShadow) + ((y << 10) + x) * 2;
    auto* src = static_cast<const std::uint8_t*>(pixels);
    const std::uint32_t row = static_cast<std::uint32_t>(w + w);
    for (int n = 0; n < rect[3]; ++n) {
        std::memcpy(dst, src, row);
        dst += 0x800;
        src += row;
    }

    Gfx_InvalidateTextures(rect, 0);
}

// original 0x59EB00. Gfx_LoadImage's sibling for data that is mostly unchanged
// from frame to frame: a row is copied, and converted, only if it differs from
// what the shadow already holds. It does not invalidate textures.
//
// As the original has it: no bounds check at all, where Gfx_LoadImage has two;
// the row bound and the x and w handed on are read from the rect again on
// every row, the row length once.
extern "C" void __cdecl Gfx_LoadImageIfChanged(const short* rect, const void* pixels) {
    int y = rect[1];
    const std::uint32_t row_bytes = static_cast<std::uint32_t>(rect[2] << 1);
    auto* dst = reinterpret_cast<std::uint8_t*>(ShadowCell(rect[0], y));
    auto* src = static_cast<const std::uint8_t*>(pixels);
    for (; y < rect[3] + rect[1]; ++y) {
        if (std::memcmp(dst, src, row_bytes) != 0) {
            std::memcpy(dst, src, row_bytes);
            Gfx_ConvertRow(rect[0], y, rect[2]);
        }
        src += row_bytes;
        dst += 0x800;
    }
}

// original 0x59EBB0. Converts w cells of VRAM row y, from x, into the row's
// buffer. As the original has it: nothing is checked - not y against the 512
// rows, not x + w against the buffer's 1,024 cells; and the generation moves
// on even when w <= 0 converts nothing.
//
// PSX meaning carried over: cell 0 is transparent and stays 0, and any other
// cell that the display format would round to 0 is forced to 1, so that it
// stays opaque.
//
// DIVERGENCE DIV-0160 (docs/stp-blend.md): Capcom's gives every non-zero cell
// the opaque bits, so bit 15 - which on the PlayStation picks the texels a
// semi-transparent primitive blends - is lost. While the textures carry it
// (render::StpTexels), a cell with bit 15 set gets no opaque bits: its colour
// with alpha clear, still forced to 1 if it would round to 0, so it is never
// read as transparent (render_shim.h). Cells without bit 15 are unchanged.
extern "C" void __cdecl Gfx_ConvertRow(int x, int y, int w) {
    ClutRow& row = Rows()[y];
    if (row.pixels == nullptr) row.pixels = Crt_malloc(0x1000);

    const PixelFormat& f = FormatGfxClut();
    auto* out = static_cast<std::uint8_t*>(row.pixels) + (x << (f.bytes_per_pixel >> 1));
    const std::uint16_t* src = ShadowCell(x, y);
    const bool stp = gfx_clut::g_stp_test || render::StpTexels();   // DIV-0160
    for (int n = 0; n < w; ++n) {
        const std::uint32_t v = src[n];
        std::uint32_t p = (Shr((v & 0x3E0) << 14, f.g_shift) & f.g_mask) |
                          (Shr((v & 0x1F) << 19, f.r_shift) & f.r_mask) |
                          (Shr((v & 0x7C00) << 9, f.b_shift) & f.b_mask);
        const std::uint32_t opaque = stp && (v & 0x8000) ? 0 : f.opaque_bits;
        if (p != 0) p |= opaque;
        else if (v != 0) p = 1 | opaque;
        if (f.bytes_per_pixel == 2) {
            const auto p16 = static_cast<std::uint16_t>(p);
            std::memcpy(out, &p16, 2);
            out += 2;
        } else {
            std::memcpy(out, &p, 4);
            out += 4;
        }
    }
    ++row.generation;
}

// 0x59ECE0; sole caller Gfx_Present, with 1. Only the low byte of flags is
// read. Bit 0: the back buffer cleared (ClearBack above). Bit 1: the frame
// shown as Gfx_Present shows it - windowed, the Blt and nothing more;
// otherwise the Flip and then the back buffer cleared again, the render flag
// read afresh. No result is checked, DDERR_SURFACELOST included.
void Gfx_ClearPresent(unsigned flags) {
    const unsigned char f = static_cast<unsigned char>(flags);
    if (f & 1) ClearBack();
    if (!(f & 2)) return;
    const display_env::U render = display_env::Long(::bof3::Img(display_env::kRenderFlags.v));
    void* const primary = DDraw_Primary;
    if (render & 0x200) {
        display_env::Method<ComBlt>(primary, display_env::kBlt)(primary, display_env::At(::bof3::Img(Gfx_WindowRect_addr)), DDraw_BackBuffer, display_env::At(::bof3::Img(Gfx_ScreenRect_addr)), display_env::kBltWait, nullptr);
        return;
    }
    display_env::Method<ComFlip>(primary, kFlip)(primary, DDraw_BackBuffer, kFlipWait);
    ClearBack();
}

// 0x59EDF0; sole caller Gpu_PutDispEnv. Windowed (Gfx_RenderFlags bit 0x200,
// which the set-up sets with DDSCL_NORMAL, 0x5A563E): Blt of the whole back
// buffer, Gfx_ScreenRect, onto the primary at Gfx_WindowRect, DDBLT_WAIT, no
// DDBLTFX. Otherwise Flip(back buffer, DDFLIP_WAIT) on the primary. Either
// returning DDERR_SURFACELOST: Restore on the primary - read again, and the
// back buffer is not restored. Then, when the draw environment's isbg byte
// (Gpu_DrawEnv +0x18, read after the present) is set, Gfx_ClearPresent(1)
// clears the back buffer for the next frame. Nothing else is checked.
void Gfx_Present(void) {
    const display_env::U flags = display_env::Long(::bof3::Img(display_env::kRenderFlags.v));
    void* const primary = DDraw_Primary;
    long result;
    if (flags & 0x200)
        result = display_env::Method<ComBlt>(primary, display_env::kBlt)(primary, display_env::At(::bof3::Img(Gfx_WindowRect_addr)), DDraw_BackBuffer, display_env::At(::bof3::Img(Gfx_ScreenRect_addr)), display_env::kBltWait,
                                               nullptr);
    else
        result = display_env::Method<ComFlip>(primary, kFlip)(primary, DDraw_BackBuffer, kFlipWait);
    if (result == kSurfaceLost) {
        void* const again = DDraw_Primary;
        display_env::Method<display_env::ComCall>(again, kRestore)(again);
    }
    if (Gpu_DrawEnv[0x18] != 0) display_env::g.clear_present(1);
}

// 0x59EE50, called once a rendered frame from WinMain (0x4FCE6F) with the
// ordering table. Nothing at all unless Gfx_DrawEnable. Gfx_RenderFlags bit 0
// (read once) picks the software surfaces' table; otherwise the walk is inside
// BeginScene / EndScene and uses the Direct3D table. The walk: from the table
// pointer, while the link word is not -1, follow it - a link with its top byte
// set is masked to 24 bits and the node it reaches is NOT drawn (known-defects
// D4), a clean one is drawn - and read the drawn node's link after its handler.
// Then every glyph texture and every cell texture is marked unused, and if
// anything set the word 0x7CADEA during the draw (it is cleared on entry),
// D3d_AfterDraw 0x59F580 runs (a tail jump in the original; it reads no
// argument).
void Gfx_DrawOTag(unsigned long* ot) {
    if (Gfx_DrawEnable == 0) return;
    if (draw_order::g_on) draw_order::LogWalk(ot);   // BOF3X_DRAWORDER, diagnostic only: reads the table, draws nothing
    if (D3d_AfterDrawRequest != 0) {
        // Whether D3d_AfterDraw's request ever fires is unmeasured (display-setup.md section 7): say so once.
        static bool seen;
        if (!seen) {
            seen = true;
            bof3::Log("Gfx_DrawOTag: D3d_AfterDrawRequest was set on entry (first time)");
        }
        D3d_AfterDrawRequest = 0;
    }
    const bool software = (Gfx_RenderFlags & 1) != 0;
    if (!software) d3d_list::Scene(0x24);
    if (stp_census::g_on && !software) stp_census::Frame();
    d3d_list::U node = Low32(ot);
    while (d3d_list::Long(node) != 0xFFFFFFFFu) {
        d3d_list::U p = d3d_list::Long(node);
        if (p & 0xFF000000u) p &= 0xFFFFFFu;
        else Dispatch(p, software);
        node = p;
    }
    if (!software) d3d_list::Scene(0x28);
    for (d3d_list::U a = ::bof3::Img(kFontInUse); a < ::bof3::Img(kFontInUseEnd); a += 0x14) d3d_list::PutWord(a, 0);
    for (d3d_list::U a = ::bof3::Img(kCellInUse); a < ::bof3::Img(kCellInUseEnd); a += 0x28) d3d_list::PutWord(a, 0);
    if (D3d_AfterDrawRequest != 0) d3d_list::g.after_draw();
}

// 0x59F520, called by Gfx_DrawOTag for the codes 0xF4..0xF7 with byte +8 ^ 1
// (its only caller). Non-zero (a dword test): unless D3d_AlphaOpCache 0x66B720
// is already set, stage 0's ALPHAOP (4) to MODULATE (4) and the cache 1. Zero:
// unless the cache is already 0, ALPHAOP to SELECTARG2 (3) - the diffuse's
// alpha alone - and the cache 0. The cache is read once, before the call, and
// written after it. Nothing reads what it returns.
void D3d_SetAlphaModulate(unsigned on) {
    const d3d_rest::U cache = D3d_AlphaOpCache;
    if (on != 0) {
        if (cache != 0) return;
        SetStageState(0, 4, 4);
        D3d_AlphaOpCache = 1;
    } else {
        if (cache == 0) return;
        SetStageState(0, 4, 3);
        D3d_AlphaOpCache = 0;
    }
}

// 0x59F580, a tail jump of Gfx_DrawOTag when D3d_AfterDrawRequest is set after
// the walk (nothing in .text sets it - docs/d3d-rest.md section 5). A capture of
// the frame into a texture: nothing at all under Gfx_RenderFlags bit 0.
//
// On first use (the capture surface 0x7CADF8 null): the scale 1.0, the used
// size 0x140 x 0xF0; a plain surface of 0x140 x 0x140 in the screen's format
// (2); D3d_FitTextureSize(0x140, 0xF0) into the dwords 0x7CADF0 / 0x7CADF2 -
// which overlap - and a texture surface of the low words; if the texture is
// narrower than 0x140, the used width becomes its width, the scale width /
// 320 (fild, fmul, fstp) and the used height 0xF0 times the scale (_ftol).
// Either creation failing returns at once.
//
// Then: the steps ScaleX and ScaleY times 65536 (_ftol), 16.16; one surface
// description, locked through the capture surface (its pixels kept) and then
// through DDraw_BackBuffer (its pixels and pitch read from it); either lock
// failing returns - the second leaving the capture surface locked. 240 rows of
// 320 pixels, nearest, from the back buffer at (x * step >> 16, y * step >> 16)
// into the capture surface packed (its pitch never read): 16-bit when the
// screen's bytes per pixel (0x7DEDE3, read after the locks) is 2, else 32. The
// back buffer unlocked, then the capture surface; Blt of the capture surface's
// (0, 0, 320, 240) onto the texture's (0, 0, used w, used h), DDBLT_WAIT; and
// the ready word 0x7CADE8 set to 1, whatever Blt answered. Nothing in .text but
// Display_Setup and the teardown reads the ready word or the texture.
void D3d_AfterDraw() {
    if (Gfx_RenderFlags & 1) return;
    if (d3d_rest::Long(d3d_rest::At(::bof3::Img(D3d_CaptureSurface_addr))) == 0) {
        d3d_rest::PutLong(d3d_rest::At(::bof3::Img(D3dRest_CaptureScale_addr)), 0x3F800000); // 1.0
        D3dRest_CaptureW = 0x140;
        D3dRest_CaptureH = 0xF0;
        if (!d3d_rest::g.create_plain(0x140, 0x140, reinterpret_cast<::bof3::Slot32<void>*>(d3d_rest::At(::bof3::Img(D3d_CaptureSurface_addr))), 2)) return;
        d3d_rest::g.fit_size(d3d_rest::Long(d3d_rest::At(::bof3::Img(D3dRest_CaptureW_addr))) & 0xFFFF, static_cast<std::uint32_t>(D3dRest_CaptureH), reinterpret_cast<unsigned*>(d3d_rest::At(::bof3::Img(kCaptureTexW.v))),
                   reinterpret_cast<unsigned*>(d3d_rest::At(::bof3::Img(kCaptureTexH.v))));
        if (!d3d_rest::g.create_texture(d3d_rest::Long(d3d_rest::At(::bof3::Img(kCaptureTexW.v))) & 0xFFFF, d3d_rest::Word(d3d_rest::At(::bof3::Img(kCaptureTexH.v))),
                              reinterpret_cast<::bof3::Slot32<void>*>(d3d_rest::At(::bof3::Img(D3d_CaptureTexSurface_addr))),
                              reinterpret_cast<::bof3::Slot32<void>*>(d3d_rest::At(::bof3::Img(D3d_CaptureTexture_addr))), 2))
            return;
        const d3d_rest::U width = d3d_rest::Word(d3d_rest::At(::bof3::Img(kCaptureTexW.v)));
        if (width < 0x140) {
            const d3d_rest::U height = D3dRest_CaptureH;
            D3dRest_CaptureW = width;
            d3d_rest::PutFloat(d3d_rest::At(::bof3::Img(D3dRest_CaptureScale_addr)), X87IntMul(static_cast<std::int32_t>(width), d3d_rest::FloatAt(kPerColumn)));
            D3dRest_CaptureH = X87IntMulFtol(static_cast<std::int32_t>(height), D3dRest_CaptureScale);
        }
    }
    const d3d_rest::U step_x = X87MulFtol(D3d_ScaleX, d3d_rest::FloatAt(kFixedOne));
    const d3d_rest::U step_y = X87MulFtol(D3d_ScaleY, d3d_rest::FloatAt(kFixedOne));
    alignas(4) unsigned char desc[0x7C];
    d3d_rest::g.init_desc(desc);
    if (Lock(SurfaceAt(::bof3::Img(D3d_CaptureSurface_addr)), desc) != 0) return;
    d3d_rest::U out = d3d_rest::Long(desc + 0x24);
    if (Lock(SurfaceAt(::bof3::Img(DDraw_BackBuffer_addr)), desc) != 0) return;
    const bool narrow = D3dRest_ScreenBpp == 2;
    const d3d_rest::U from = d3d_rest::Long(desc + 0x24);
    d3d_rest::U y = 0;
    for (d3d_rest::U row = 0; row < 0xF0; ++row) {
        // sar, then imul: the low 32 bits, which an unsigned multiply gives too
        const d3d_rest::U line = static_cast<d3d_rest::U>(static_cast<std::int32_t>(y) >> 16) * d3d_rest::Long(desc + 0x10);
        d3d_rest::U x = 0;
        for (d3d_rest::U column = 0; column < 0x140; ++column) {
            if (narrow) {
                const d3d_rest::U at = ((x >> 15) & 0x1FFFE) + line;
                std::memcpy(d3d_rest::At(out), d3d_rest::At(from + at), 2);
                out += 2;
            } else {
                const d3d_rest::U at = ((x >> 14) & 0x3FFFC) + line;
                std::memcpy(d3d_rest::At(out), d3d_rest::At(from + at), 4);
                out += 4;
            }
            x += step_x;
        }
        y += step_y;
    }
    Unlock(SurfaceAt(::bof3::Img(DDraw_BackBuffer_addr)));
    Unlock(SurfaceAt(::bof3::Img(D3d_CaptureSurface_addr)));
    const d3d_rest::U dst_rect[4] = {0, 0, d3d_rest::Long(d3d_rest::At(::bof3::Img(D3dRest_CaptureW_addr))) & 0xFFFF, static_cast<std::uint32_t>(D3dRest_CaptureH)};
    const d3d_rest::U src_rect[4] = {0, 0, 0x140, 0xF0};
    void** source = SurfaceAt(::bof3::Img(D3d_CaptureSurface_addr));
    void** texture = SurfaceAt(::bof3::Img(D3d_CaptureTexSurface_addr));
    reinterpret_cast<d3d_rest::Com6>(d3d_rest::Method(texture, 0x14))(texture, d3d_rest::Addr(dst_rect), d3d_rest::Addr(source), d3d_rest::Addr(src_rect), 0x1000000, 0);
    D3d_CaptureReady = 1;
}

// 0x59F840. Zeroes a DDSURFACEDESC2 and sets its dwSize (0x7C) and its pixel
// format's dwSize (0x20, at +0x48). The original leaves eax 0; no caller reads
// it (each overwrites eax before any use: 0x59F86F, 0x59F90F, 0x5A0133,
// 0x5A05C4, 0x5A06C5, and the rest read memory into it next).
void Dd_InitSurfaceDesc(void* desc) {
    auto* d = static_cast<unsigned char*>(desc);
    std::memset(d, 0, 0x7C);
    tex_page::PutLong(d, 0x7C);
    tex_page::PutLong(d + 0x48, 0x20);
}

// 0x59F860. A plain surface of width and height each rounded up to a multiple
// of 4, caps 0x840 (OFFSCREENPLAIN | SYSTEMMEMORY) under Gfx_RenderFlags bit 0
// and 0x1800 (TEXTURE | SYSTEMMEMORY) otherwise, flags 0x1007 (CAPS | HEIGHT |
// WIDTH | PIXELFORMAT), the pixel format the 0x20 bytes at 0x7DED80 +
// format * 0x40. Returns 1 if IDirectDraw4::CreateSurface returned DD_OK, else 0.
// `surface` and `texture` are the 4-byte slots the game hands over (an image
// word or a cache entry's field), which the render shim fills as x86 COM
// fills an out-pointer: four bytes.
int Dd_CreatePlainSurface(unsigned width, unsigned height, ::bof3::Slot32<void>* surface, unsigned format) {
    tex_page::Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    const tex_page::U caps = (Gfx_RenderFlags & 1) ? 0x840u : 0x1800u;
    tex_page::PutLong(desc.b + 0xC, (width + 3) & ~3u);
    tex_page::PutLong(desc.b + 8, (height + 3) & ~3u);
    tex_page::PutLong(desc.b + 0x68, caps);
    tex_page::PutLong(desc.b + 4, 0x1007);
    tex_page::PutLong(desc.b + 0x6C, 0);
    std::memcpy(desc.b + 0x48, tex_page::At(::bof3::Img(kFormats.v) + (format << 6)), 0x20);
    const tex_page::U dd = tex_page::Long(::bof3::Img(tex_page::kDirectDraw.v));
    return tex_page::Com(dd, kCreateSurface, desc.b, surface, 0u) == 0 ? 1 : 0;
}

// 0x59F900. A managed texture: width and height as given, flags 0x101007
// (... | TEXTURESTAGE), caps 0x1000 (TEXTURE), caps2 0x10 (TEXTUREMANAGE),
// texture stage 0, the pixel format as above. On failure returns 0 and does
// nothing more; on success asks the new surface (read back from *surface) for
// IID_IDirect3DTexture2 into *texture and returns 1 whatever that answers.
int Dd_CreateTextureSurface(unsigned width, unsigned height, ::bof3::Slot32<void>* surface, ::bof3::Slot32<void>* texture, unsigned format) {
    tex_page::Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    tex_page::PutLong(desc.b + 0xC, width);
    tex_page::PutLong(desc.b + 4, 0x101007);
    tex_page::PutLong(desc.b + 0x68, 0x1000);
    tex_page::PutLong(desc.b + 0x6C, 0x10);
    tex_page::PutLong(desc.b + 0x78, 0);
    tex_page::PutLong(desc.b + 8, height);
    std::memcpy(desc.b + 0x48, tex_page::At(::bof3::Img(kFormats.v) + (format << 6)), 0x20);
    const tex_page::U dd = tex_page::Long(::bof3::Img(tex_page::kDirectDraw.v));
    if (tex_page::Com(dd, kCreateSurface, desc.b, surface, 0u) != 0) return 0;
    tex_page::Com(tex_page::Long(Low32(surface)), kQueryInterface, tex_page::At(kTexture2Iid), texture);
    return 1;
}

// 0x59F9B0. The texture size the device takes for a w x h image, from
// D3d_DeviceDesc: under D3DPTEXTURECAPS_POW2 (bit 1 of the caps' low byte,
// read once) each side is the device's minimum (1 for 0) doubled until it is
// not below the side - signed compares, so a side above 0x40000000 never ends
// and a negative one keeps the minimum; else the sides as given. Each then
// clamped to the device's maximum (unsigned), and under SQUAREONLY (bit 5)
// both made the larger (signed). *out_w is written before *out_h. Callers
// D3d_AfterDraw 0x59F603 and D3d_BuildCellTexture; neither reads eax after.
void D3d_FitTextureSize(unsigned w, unsigned h, unsigned* out_w, unsigned* out_h) {
    const tex_cells::U caps = D3d_DeviceDesc[0x84];
    tex_cells::U tw = w, th = h;
    if (caps & 2) {
        tw = tex_cells::Long(::bof3::Img(kMinTexWidth.v));
        if (tw == 0) tw = 1;
        while (static_cast<std::int32_t>(tw) < static_cast<std::int32_t>(w)) tw <<= 1;
        th = tex_cells::Long(::bof3::Img(kMinTexHeight.v));
        if (th == 0) th = 1;
        while (static_cast<std::int32_t>(th) < static_cast<std::int32_t>(h)) th <<= 1;
    }
    const tex_cells::U max_w = tex_cells::Long(::bof3::Img(kMaxTexWidth.v));
    if (tw > max_w) tw = max_w;
    const tex_cells::U max_h = tex_cells::Long(::bof3::Img(kMaxTexHeight.v));
    if (th > max_h) th = max_h;
    if ((caps & 0x20) && tw != th) {
        const tex_cells::U side = static_cast<std::int32_t>(tw) > static_cast<std::int32_t>(th) ? tw : th;
        *out_w = side;
        *out_h = side;
        return;
    }
    *out_w = tw;
    *out_h = th;
}

// 0x59FA50, table entry 0 (code 0x20, called at 0x59F0AA): a flat untextured
// triangle. Three corners of 0xC bytes from +8 (float x, y, z); the colour of
// r, g, b +4..+6 and the code +7 in the blend mode Gfx_DrawTpage's low word,
// with a null specular - so the vertices' specular, tu and tv stay as the last
// draw left them; SetTexture(0, NULL); 0x437CC0(0) twice; the blend with
// Gfx_DrawTpage read again; flat; a triangle list of 3.
long D3d_DrawPolyF3(const unsigned char* prim) {
    unsigned long diffuse;
    d3d_rest::g.prim_color(prim[4], prim[5], prim[6], prim[7], d3d_rest::DrawMode(), &diffuse, nullptr);
    for (d3d_rest::U i = 0; i < 3; ++i) {
        unsigned char* out = d3d_rest::Vertex(i);
        d3d_rest::PutPosition(out, prim + 8 + i * 0xC);
        d3d_rest::PutLong(out + 0x10, static_cast<d3d_rest::U>(diffuse));
    }
    d3d_rest::SetTexture(0, 0);
    d3d_rest::g.ret_only(0);
    d3d_rest::g.ret_only(0);
    d3d_rest::g.set_blend(prim[7], d3d_rest::DrawMode());
    d3d_rest::g.set_shade(1);   // flat
    return d3d_rest::DrawVertices(4, 3);   // TRIANGLELIST
}

// 0x59FBA0. A primitive's PSX colour as a Direct3D diffuse / specular pair.
// Only the low byte of `code` is tested; r, g, b are whole dwords (every caller
// passes a byte zero-extended). Alpha 0xFF, unless code bit 1 (semi-transparent):
// then by the tpage's blend field, (mode >> 5) & 3, through the table 0x59FC84 -
// 0x80, 0xFF, 0xFF, 0x40. Colour: bit 2 clear (untextured) - r, g, b as given,
// OR-ed in unmasked; bits 2 and 0 (texture, raw) - 0xFF each; bit 2 alone
// (texture modulated) - each doubled (32-bit, so it wraps), and a doubled value
// above 0xFF becomes 0xFF with its low byte going to the specular. The specular
// is written only through a non-null pointer, after the diffuse.
void D3d_PrimColor(unsigned r, unsigned g, unsigned b, unsigned code, unsigned mode, unsigned long* diffuse,
                   unsigned long* specular) {
    const unsigned char c = static_cast<unsigned char>(code);
    d3d_draw::U alpha = 0xFF;
    if (c & 2) {
        static constexpr d3d_draw::U kAlpha[4] = {0x80, 0xFF, 0xFF, 0x40};
        alpha = kAlpha[(mode >> 5) & 3];
    }
    d3d_draw::U red = r, green = g, blue = b;
    d3d_draw::U excess_r = 0, excess_g = 0, excess_b = 0;
    if (c & 4) {
        if (c & 1) {
            red = green = blue = 0xFF;
        } else {
            red = r << 1;
            if (red > 0xFF) {
                excess_r = red & 0xFF;
                red = 0xFF;
            }
            green = g << 1;
            if (green > 0xFF) {
                excess_g = green & 0xFF;
                green = 0xFF;
            }
            blue = b << 1;
            if (blue > 0xFF) {
                excess_b = blue & 0xFF;
                blue = 0xFF;
            }
        }
    }
    *diffuse = ((((alpha << 8) | red) << 8 | green) << 8) | blue;
    if (specular) *specular = ((excess_r << 8) | excess_g) << 8 | excess_b;
}

// 0x59FCA0. The PSX semi-transparency as three render states: ALPHABLENDENABLE
// (0x1B) on, SRCBLEND (0x13), DESTBLEND (0x14). Code bit 1 (the low byte only)
// clear: 5, 6 (SRCALPHA, INVSRCALPHA - the alpha is 0xFF, so opaque). Set: by
// (mode >> 5) & 3 through 0x59FD6C - 5, 6 / 2, 2 / 1, 4 / 5, 2.
void D3d_SetBlend(unsigned code, unsigned mode) {
    static constexpr d3d_draw::U kBlend[4][2] = {{5, 6}, {2, 2}, {1, 4}, {5, 2}};
    const d3d_draw::U* pair = (code & 2) ? kBlend[(mode >> 5) & 3] : kBlend[0];
    SetRenderState(0x1B, 1);
    SetRenderState(0x13, pair[0]);
    SetRenderState(0x14, pair[1]);
}

// 0x59FD80. SHADEMODE (9), only when it differs from the cache; the cache is
// written after the call.
void D3d_SetShadeMode(unsigned mode) {
    if (*reinterpret_cast<volatile d3d_draw::U*>(d3d_draw::At(::bof3::Img(D3d_ShadeModeCache_addr))) == mode) return;
    SetRenderState(9, mode);
    D3d_ShadeModeCache = mode;
}

// 0x59FDB0, table entry 1 (code 0x24, called at 0x59F0B8): a textured flat
// triangle. Three corners of 0x10 bytes from +8: float x, y, z, byte u, v; the
// CLUT word +0x16 and the tpage word +0x26 in the first two. If the three
// (u, v) words are equal, D3d_FlattenFT3 first (16-bit compares). Then one
// colour, with a specular, in the tpage's blend mode; the corners; the fourth
// vertex's z (0x7CA9C0) set to 0, as D3d_DrawPolyG3 does. Flattened: no
// texture coordinates and SetTexture(0, NULL); else tu, tv per corner and
// D3d_BindTexture(tpage, clut). 0x437CC0 with the tpage's bits 0x400 and 0x800,
// the blend, flat, a triangle list of 3 - the tpage word re-read after each
// call.
long D3d_DrawPolyFT3(unsigned char* prim) {
    bool flattened = false;
    const d3d_rest::U uv = d3d_rest::Word(prim + 0x14);
    if (uv == d3d_rest::Word(prim + 0x24) && uv == d3d_rest::Word(prim + 0x34)) {
        D3d_FlattenFT3(prim, uv);
        flattened = true;
    }
    unsigned long diffuse, specular;
    d3d_rest::g.prim_color(prim[4], prim[5], prim[6], prim[7], d3d_rest::Word(prim + 0x26), &diffuse, &specular);
    for (d3d_rest::U i = 0; i < 3; ++i) {
        unsigned char* out = d3d_rest::Vertex(i);
        d3d_rest::PutPosition(out, prim + 8 + i * 0x10);
        d3d_rest::PutLong(out + 0x10, static_cast<d3d_rest::U>(diffuse));
        d3d_rest::PutLong(out + 0x14, static_cast<d3d_rest::U>(specular));
    }
    d3d_rest::PutLong(d3d_rest::Vertex(3) + 8, 0);
    if (flattened) {
        d3d_rest::SetTexture(0, 0);
    } else {
        for (d3d_rest::U i = 0; i < 3; ++i) d3d_rest::PutTexel(d3d_rest::Vertex(i), prim + 0x14 + i * 0x10);
        d3d_rest::g.bind_texture(d3d_rest::Word(prim + 0x26), d3d_rest::Word(prim + 0x16));
    }
    d3d_rest::g.ret_only(d3d_rest::Word(prim + 0x26) & 0x400);
    d3d_rest::g.ret_only(d3d_rest::Word(prim + 0x26) & 0x800);
    d3d_rest::g.set_blend(prim[7], d3d_rest::Word(prim + 0x26));
    d3d_rest::g.set_shade(1);   // flat
    return d3d_rest::DrawVertices(4, 3);   // TRIANGLELIST
}

// 0x59FFE0. The texture of a PSX texture page and CLUT, found in or built into
// Gfx_TexCache, set on stage 0. Page = tpage & 0x1F, colour mode =
// (tpage >> 7) & 3. A miss (slot 0x20) builds - outside the scene - and sets
// what the builder returns; a hit sets the entry's texture, and an entry in
// state 2 (its CLUT changed) is refreshed outside the scene first. Kept: the
// texture handed to SetTexture on that path is the one read BEFORE the refresh.
long D3d_BindTexture(unsigned tpage, unsigned clut) {
    const int page = static_cast<int>(tpage & 0x1F);
    const int mode = static_cast<int>((tpage >> 7) & 3);
    const int slot = d3d_draw::g.tex_find(page, static_cast<int>(clut), mode);
    d3d_draw::U texture;
    if (slot == 0x20) {
        if (draw_order::Tagging())
            bof3::Log("d3d_draw: page cache miss - page %d clut %04X mode %d built", page, clut, mode);
        d3d_draw::Scene(0x28);   // EndScene
        texture = static_cast<d3d_draw::U>(d3d_draw::g.tex_build(page, static_cast<int>(clut), mode));
        d3d_draw::Scene(0x24);   // BeginScene
    } else {
        const unsigned char* e = d3d_draw::At(::bof3::Img(Gfx_TexCache_addr) + static_cast<d3d_draw::U>(page * 32 + slot) * 0x18);
        const unsigned char state = e[0];
        texture = d3d_draw::Long(e + 0x14);
        if (state == 2) {
            d3d_draw::Scene(0x28);
            d3d_draw::g.tex_refresh(page, slot);
            d3d_draw::Scene(0x24);
        }
    }
    // DIV-0125 (render/fb_grab.h), not in the original: a direct-colour
    // page tells the backend which VRAM cells its texels come from - the
    // page's corner moved by Gfx_TexCacheKey's x, y (cells, in this mode), the
    // window w x h its 256 texels tile - so a frame-buffer grab there can be
    // sampled. Does nothing under Capcom's DirectX 6 objects.
    if (mode & 2) {
        const unsigned char* key = d3d_draw::At(::bof3::Img(Gfx_TexCacheKey_addr));
        render::NoteDirectPage(reinterpret_cast<void*>(static_cast<std::uintptr_t>(texture)),
                               (page & 0xF) * 64 + d3d_draw::Short(key), (page & 0x10) * 16 + d3d_draw::Short(key + 2), d3d_draw::Short(key + 4),
                               d3d_draw::Short(key + 6));
    }
    return d3d_draw::SetTexture(0, texture);
}

// 0x5A0080. Builds the first free entry (state byte 0) of Gfx_TexCache page
// `page` from VRAM: 256 x 256 texels of the window Gfx_TexCacheKey (x, y, w, h)
// names, in colour mode `mode`, through CLUT `clut` unless mode bit 1 (direct).
//
// None free of the page's 32: returns 0. Under Gfx_RenderFlags bit 0 (the
// software surfaces) a 256 x 256 plain surface into the entry's +0x10, locked
// and filled directly; returns the slot, the entry's +0x14 zeroed. Otherwise a
// managed 256 x 256 texture into +0x10 / +0x14, colour-keyed on 0 under
// Gfx_RenderFlags bit 5, filled by a Blt of {0, 0, 256, 256} from the staging
// surface Dd_StageSurface, which is locked and filled; returns the texture (+0x14,
// read after the Blt). A surface that cannot be made, or a Lock that fails,
// returns 0 with the entry left free - and the surface just made left in it,
// unreleased (known-defects D30). On success: state 1, byte +1 the mode's low
// byte, +8 the key's 8 bytes (read last); unless direct, +2 the CLUT's low 16
// bits and +4 its row's generation (Gfx_ClutRows at (clut sar 6) * 8, read
// after the Unlock).
//
// Two tests of `mode` disagree for values beyond 3, as the original's do: the
// x offset and the direct test (`test al, 2`) read the low bits, but 4-bit
// against 8-bit is the whole dword against 0. The callers pass 0..3 (tpage
// bits 7..8).
unsigned long D3d_BuildPageTexture(int page, int clut, int mode) {
    const tex_page::U p = static_cast<tex_page::U>(page);
    const tex_page::U m = static_cast<tex_page::U>(mode);
    tex_page::U slot = 0;
    for (tex_page::U e = ::bof3::Img(Gfx_TexCache_addr) + ((p * 3) << 8); slot < 0x20; ++slot, e += kEntryBytes)
        if (tex_page::At(e)[0] == 0) break;
    // Diagnostic only, no behaviour: a page with no free entry (the original
    // draws the primitive untextured, D3d_BindTexture with a 0) is logged, the
    // first 64 times; BOF3X_TEXPAGELOG=1 logs every build.
    {
        static bool want = false, asked = false;
        static unsigned full_logged = 0;
        if (!asked) {
            char text[8];
            want = GetEnvironmentVariableA("BOF3X_TEXPAGELOG", text, sizeof text) != 0 && text[0] == '1';
            asked = true;
        }
        if (slot == 0x20 && full_logged < 64) {
            ++full_logged;
            bof3::Log("tex_page: page %u FULL (32 entries) at frame %u: clut 0x%X mode %u key %d,%d %dx%d", (unsigned)p,
                      (unsigned)Frame_Counter, (unsigned)clut, (unsigned)m, (int)tex_page::Short(::bof3::Img(kTexKey.v)), (int)tex_page::Short(::bof3::Img(kTexKey.v) + 2),
                      (int)tex_page::Short(::bof3::Img(kTexKey.v) + 4), (int)tex_page::Short(::bof3::Img(kTexKey.v) + 6));
        } else if (want && slot != 0x20) {
            bof3::Log("tex_page: build page %u slot %u frame %u clut 0x%X mode %u key %d,%d %dx%d", (unsigned)p, (unsigned)slot,
                      (unsigned)Frame_Counter, (unsigned)clut, (unsigned)m, (int)tex_page::Short(::bof3::Img(kTexKey.v)), (int)tex_page::Short(::bof3::Img(kTexKey.v) + 2),
                      (int)tex_page::Short(::bof3::Img(kTexKey.v) + 4), (int)tex_page::Short(::bof3::Img(kTexKey.v) + 6));
        }
    }
    if (slot == 0x20) return 0;
    const tex_page::U src = Source(p, m, tex_page::Short(::bof3::Img(kTexKey.v)), tex_page::Short(::bof3::Img(kTexKey.v) + 2));
    tex_page::Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    const tex_page::U entry = ::bof3::Img(Gfx_TexCache_addr) + ((p << 5) + slot) * kEntryBytes;
    const tex_page::U surface_slot = entry + 0x10;
    tex_page::U result;
    if (Gfx_RenderFlags & 1) {
        if (!Dd_CreatePlainSurface(0x100, 0x100, reinterpret_cast<::bof3::Slot32<void>*>(tex_page::At(surface_slot)), 0)) return 0;
        const void* table = nullptr;
        if (!(m & 2)) table = tex_page::g.clut_pixels(static_cast<unsigned>(clut));
        if (tex_page::Com(tex_page::Long(surface_slot), tex_page::kLock, tex_page::kNoRect, desc.b, tex_page::kLockWait, 0u) != 0) return 0;
        if (m & 2) tex_page::g.convert16(desc.Surface(), tex_page::At(src), tex_page::Short(::bof3::Img(kTexKey.v) + 4), tex_page::Short(::bof3::Img(kTexKey.v) + 6), static_cast<int>(desc.Pitch()));
        else if (m != 0) tex_page::g.convert8(desc.Surface(), tex_page::At(src), table, tex_page::Short(::bof3::Img(kTexKey.v) + 4), tex_page::Short(::bof3::Img(kTexKey.v) + 6), static_cast<int>(desc.Pitch()));
        else tex_page::g.convert4(desc.Surface(), tex_page::At(src), table, tex_page::Short(::bof3::Img(kTexKey.v) + 4), tex_page::Short(::bof3::Img(kTexKey.v) + 6), static_cast<int>(desc.Pitch()));
        tex_page::Com(tex_page::Long(surface_slot), tex_page::kUnlock, 0u);
        if (!(m & 2)) {
            tex_page::PutWord(tex_page::At(entry + 2), static_cast<tex_page::U>(clut));
            tex_page::PutLong(entry + 4, Generation(::bof3::Img(tex_page::kClutRows.v) + static_cast<tex_page::U>(clut >> 6) * 8));
        }
        tex_page::PutLong(entry + 0x14, 0);
        result = slot;
    } else {
        if (!Dd_CreateTextureSurface(0x100, 0x100, reinterpret_cast<::bof3::Slot32<void>*>(tex_page::At(surface_slot)),
                                     reinterpret_cast<::bof3::Slot32<void>*>(tex_page::At(entry + 0x14)), 0))
            return 0;
        if (Gfx_RenderFlags & 0x20) {
            tex_page::U key[2] = {0, 0};   // DDCOLORKEY {0, 0}
            tex_page::Com(tex_page::Long(surface_slot), tex_page::kSetColorKey, tex_page::kSrcBltKey, key);
        }
        const void* table = nullptr;
        if (!(m & 2)) table = tex_page::g.clut_pixels(static_cast<unsigned>(clut));
        if (tex_page::Com(tex_page::Long(::bof3::Img(tex_page::kStage.v)), tex_page::kLock, tex_page::kNoRect, desc.b, tex_page::kLockWait, 0u) != 0) return 0;
        if (m & 2) tex_page::g.convert16(desc.Surface(), tex_page::At(src), tex_page::Short(::bof3::Img(kTexKey.v) + 4), tex_page::Short(::bof3::Img(kTexKey.v) + 6), static_cast<int>(desc.Pitch()));
        else if (m != 0) tex_page::g.convert8(desc.Surface(), tex_page::At(src), table, tex_page::Short(::bof3::Img(kTexKey.v) + 4), tex_page::Short(::bof3::Img(kTexKey.v) + 6), static_cast<int>(desc.Pitch()));
        else tex_page::g.convert4(desc.Surface(), tex_page::At(src), table, tex_page::Short(::bof3::Img(kTexKey.v) + 4), tex_page::Short(::bof3::Img(kTexKey.v) + 6), static_cast<int>(desc.Pitch()));
        tex_page::Com(tex_page::Long(::bof3::Img(tex_page::kStage.v)), tex_page::kUnlock, 0u);
        if (!(m & 2)) {
            tex_page::PutWord(tex_page::At(entry + 2), static_cast<tex_page::U>(clut));
            tex_page::PutLong(entry + 4, Generation(::bof3::Img(tex_page::kClutRows.v) + static_cast<tex_page::U>(clut >> 6) * 8));
        }
        tex_page::Rect rect = {0, 0, 0x100, 0x100};
        tex_page::Com(tex_page::Long(surface_slot), tex_page::kBlt, &rect, tex_page::Long(::bof3::Img(tex_page::kStage.v)), &rect, tex_page::kBltWait, 0u);
        result = tex_page::Long(entry + 0x14);
    }
    tex_page::At(entry)[0] = 1;
    tex_page::At(entry)[1] = static_cast<unsigned char>(m);
    tex_page::PutLong(entry + 8, Gfx_TexCacheKey[0]);
    tex_page::PutLong(entry + 0xC, Gfx_TexCacheKey[1]);
    return result;
}

// original 0x5A04C0. The converted colours of a CLUT, by PSX CLUT id: x / 16 in
// the low six bits, y above. Converts the first 256 cells of the row if the row
// has never been wanted before. As the original has it, y is not checked, and
// an id can name a row up to 1,023.
extern "C" void* __cdecl Gfx_ClutPixels(unsigned clut_id) {
    clut_id &= 0xFFFF;
    const unsigned y = clut_id >> 6;
    const unsigned x = (clut_id & 0x3F) << 4;
    if (Rows()[y].pixels == nullptr) Gfx_ConvertRow(0, static_cast<int>(y), 0x100);
    return static_cast<std::uint8_t*>(Rows()[y].pixels) + (x << (FormatGfxClut().bytes_per_pixel >> 1));
}

// 0x5A0510. Re-fills entry `slot` of page `page` from VRAM in place - the same
// surfaces, the same key and mode, as D3d_BuildPageTexture filled them - for
// an entry in state 2 (its CLUT's generation moved, or its VRAM was written
// under it). The state byte is set to 1 FIRST, before anything can fail; a
// failed Lock returns with nothing more done. The mode byte is re-read after
// the Lock for 4-bit against 8-bit (`test al, al` - the byte, where the build
// tested the dword). Unless direct: Gfx_ClutPixels of the entry's CLUT word
// (the original pushes it with the upper half of ecx left over from the x
// arithmetic; Gfx_ClutPixels masks it to 16 bits at 0x5A04C5, so it is passed
// clean here), and after the fill +4 the generation of row (clut >> 6),
// unsigned this time. Software: lock the entry's own surface. Direct3D: the
// staging surface, then a Blt of {0, 0, 256, 256} into the entry's surface.
void D3d_RefreshPageTexture(int page, int slot) {
    const tex_page::U p = static_cast<tex_page::U>(page);
    const tex_page::U entry = ::bof3::Img(Gfx_TexCache_addr) + ((p << 5) + static_cast<tex_page::U>(slot)) * kEntryBytes;
    tex_page::At(entry)[0] = 1;
    const tex_page::U mode = tex_page::At(entry)[1];
    const tex_page::U src = Source(p, mode, tex_page::Short(entry + 8), tex_page::Short(entry + 0xA));
    tex_page::Desc desc;
    tex_page::Rect rect = {0, 0, 0x100, 0x100};
    if (mode & 2) {
        Dd_InitSurfaceDesc(desc.b);
        if (Gfx_RenderFlags & 1) {
            if (tex_page::Com(tex_page::Long(entry + 0x10), tex_page::kLock, tex_page::kNoRect, desc.b, tex_page::kLockWait, 0u) != 0) return;
            tex_page::g.convert16(desc.Surface(), tex_page::At(src), tex_page::Short(entry + 0xC), tex_page::Short(entry + 0xE), static_cast<int>(desc.Pitch()));
            tex_page::Com(tex_page::Long(entry + 0x10), tex_page::kUnlock, 0u);
            return;
        }
        if (tex_page::Com(tex_page::Long(::bof3::Img(tex_page::kStage.v)), tex_page::kLock, tex_page::kNoRect, desc.b, tex_page::kLockWait, 0u) != 0) return;
        tex_page::g.convert16(desc.Surface(), tex_page::At(src), tex_page::Short(entry + 0xC), tex_page::Short(entry + 0xE), static_cast<int>(desc.Pitch()));
        tex_page::Com(tex_page::Long(::bof3::Img(tex_page::kStage.v)), tex_page::kUnlock, 0u);
        tex_page::Com(tex_page::Long(entry + 0x10), tex_page::kBlt, &rect, tex_page::Long(::bof3::Img(tex_page::kStage.v)), &rect, tex_page::kBltWait, 0u);
        return;
    }
    const void* table = tex_page::g.clut_pixels(tex_page::Word(tex_page::At(entry + 2)));
    Dd_InitSurfaceDesc(desc.b);
    const bool software = (Gfx_RenderFlags & 1) != 0;
    const tex_page::U locked = software ? entry + 0x10 : ::bof3::Img(Dd_StageSurface_addr);
    if (tex_page::Com(tex_page::Long(locked), tex_page::kLock, tex_page::kNoRect, desc.b, tex_page::kLockWait, 0u) != 0) return;
    if (tex_page::At(entry)[1] != 0)
        tex_page::g.convert8(desc.Surface(), tex_page::At(src), table, tex_page::Short(entry + 0xC), tex_page::Short(entry + 0xE), static_cast<int>(desc.Pitch()));
    else
        tex_page::g.convert4(desc.Surface(), tex_page::At(src), table, tex_page::Short(entry + 0xC), tex_page::Short(entry + 0xE), static_cast<int>(desc.Pitch()));
    tex_page::Com(tex_page::Long(locked), tex_page::kUnlock, 0u);
    if (!software) tex_page::Com(tex_page::Long(entry + 0x10), tex_page::kBlt, &rect, tex_page::Long(::bof3::Img(tex_page::kStage.v)), &rect, tex_page::kBltWait, 0u);
    tex_page::PutLong(entry + 4, Generation(::bof3::Img(tex_page::kClutRows.v) + (tex_page::Word(tex_page::At(entry + 2)) >> 6) * 8));
}

// original 0x5A0830. The slot, 0..31, of the page's entry for this texture, or
// 32 if it has none. A texture is identified by Gfx_TexCacheKey - eight bytes
// the draw sets first - and, unless it is 15-bit, by its CLUT id.
//
// It is also where a palette change reaches a texture: a hit whose CLUT row
// has been converted again since the entry was built (Gfx_ClutRows generation)
// is marked stale on the way out. Not for 15-bit textures, which have no CLUT.
//
// As the original has it: only bit 1 of mode is looked at, so 3 is 15-bit too;
// 4-bit and 8-bit entries are not told apart; the CLUT id is compared as 32
// bits against a 16-bit field, so an id with high bits set never hits; and
// clut >> 6 indexes Gfx_ClutRows unchecked, up to row 1,023 of 512.
extern "C" int __cdecl Gfx_TexCacheFind(int page, int clut, int mode) {
    TexCacheEntry* own = Table() + page * kPerPage;
    for (int i = 0; i < kPerPage; ++i) {
        TexCacheEntry& e = own[i];
        if (e.state == 0) return kPerPage;
        if (std::memcmp(e.key, Gfx_TexCacheKey, sizeof e.key) != 0) continue;
        if (mode & 2) {
            if (e.mode == 2) return i;
            continue;
        }
        if (static_cast<int>(e.clut) != clut) continue;
        struct Row { std::uint32_t generation; ::bof3::Slot32<void> pixels; };   // gfx_clut.cpp's ClutRow: 8 bytes on every width
        if (e.generation != reinterpret_cast<const Row*>(Gfx_ClutRows)[clut >> 6].generation) e.state = 2;
        return i;
    }
    return kPerPage;
}

// 0x5A0910, called only by D3d_DrawPolyFT3 (0x59FDCB) when the three corners'
// (u, v) words are equal: the triangle samples one texel, so it is drawn
// untextured in that texel's colour. The texel by D3d_PageTexel4(word +0xE,
// word +0x16, uv & 0xFFFF) - the PSX's POLY_FT3 offsets of the CLUT and the
// tpage, not this port's float layout, where +0xE is the high half of corner 0's
// y and +0x16 the CLUT (docs/d3d-rest.md section 4, latent). Its red, green
// and blue by Gfx_PixelFormat's masks and shifts, each to bits 16..23 then
// >> 16; the primitive's r, g, b each times its channel times 1/128 (x87,
// _ftol), clamped to 0xFF (unsigned), written back over the primitive - each
// read before the one before it is stored - and the code's bit 2 (textured)
// cleared. The primitive itself changes: drawn again without being rebuilt it
// is a POLY_F3 (code 0x20) of the modulated colour.
void D3d_FlattenFT3(unsigned char* prim, d3d_rest::U uv) {
    const d3d_rest::U pixel = D3d_PageTexel4(d3d_rest::Word(prim + 0xE), d3d_rest::Word(prim + 0x16), uv & 0xFFFF);
    const unsigned char* format = d3d_rest::At(::bof3::Img(Gfx_PixelFormat_addr));
    const d3d_rest::U green = ((d3d_rest::Long(format + 0x14) & pixel) << (d3d_rest::Long(format + 0x8) & 31)) >> 16;
    const d3d_rest::U blue = ((d3d_rest::Long(format + 0x18) & pixel) << (d3d_rest::Long(format + 0xC) & 31)) >> 16;
    const d3d_rest::U red = ((d3d_rest::Long(format + 0x10) & pixel) << (d3d_rest::Long(format + 0x4) & 31)) >> 16;
    const float k = d3d_rest::FloatAt(kModulate);
    d3d_rest::U r = X87RedFtol(red, prim[4], k);
    if (r > 0xFF) r = 0xFF;
    const d3d_rest::U gg = prim[5];
    prim[4] = static_cast<unsigned char>(r);
    d3d_rest::U v = X87FimulFtol(static_cast<std::int32_t>(gg), k, static_cast<std::int32_t>(green));
    if (v > 0xFF) v = 0xFF;
    const d3d_rest::U b = prim[6];
    prim[5] = static_cast<unsigned char>(v);
    v = X87FimulFtol(static_cast<std::int32_t>(b), k, static_cast<std::int32_t>(blue));
    if (v > 0xFF) v = 0xFF;
    prim[6] = static_cast<unsigned char>(v);
    prim[7] &= 0xFB;
}

// 0x5A0A40, called only by D3d_FlattenFT3 (0x5A093A): the 4-bit texel at
// (u, v) of a texture page in Gfx_VramShadow, through a CLUT. The page from
// `page` as a tpage word: x = (page & 0xF) * 64 cells, y = 256 rows when bit
// 0x10; u = uv's low byte, v = uv >> 8 (arithmetic). The byte at
// Gfx_VramShadow + ((y + v) * 16 + page x) * 128 + ((uv >> 1) & 0x7F), and of it
// the HIGH nibble for an even u, the low for an odd (Tex_Convert4 reads the low
// nibble first: docs/d3d-rest.md section 4, latent). Then Gfx_ClutPixels(clut)
// - the converted palette - indexed by the nibble: a u16 when
// Gfx_PixelFormat's bytes per texel (+3, read after the call) is 2, else a
// dword.
d3d_rest::U D3d_PageTexel4(d3d_rest::U clut, d3d_rest::U page, d3d_rest::U uv) {
    d3d_rest::U row = ((page & 0x10) << 4) + static_cast<d3d_rest::U>(static_cast<std::int32_t>(uv) >> 8);
    const d3d_rest::U offset = ((row << 4) + (page & 0xF)) << 7;
    const d3d_rest::U column = static_cast<d3d_rest::U>(static_cast<std::int32_t>(uv) >> 1) & 0x7F;
    const d3d_rest::U texels = d3d_rest::At(::bof3::Img(Gfx_VramShadow_addr) + offset + column)[0];
    const d3d_rest::U shift = (static_cast<d3d_rest::U>(~uv) & 1) << 2;
    const d3d_rest::U index = (texels >> shift) & 0xF;
    const unsigned char* palette = static_cast<const unsigned char*>(d3d_rest::g.clut_pixels(clut));
    if (d3d_rest::At(::bof3::Img(Gfx_PixelFormat_addr) + 3)[0] == 2) return d3d_rest::Word(palette + index * 2);
    return d3d_rest::Long(palette + index * 4);
}

// original 0x5A0AB0, code 0x28 (Gpu_SetPolyF4): a flat quad, four corners of
// 0xC bytes from +8 (float x, y, z), one colour from +4..+6.
extern "C" long __cdecl D3d_DrawPolyF4(const unsigned char* prim) {
    unsigned long diffuse;
    field_misc::g.prim_color(prim[4], prim[5], prim[6], prim[7], DrawModeFieldMisc(), &diffuse, nullptr);
    for (field_misc::U i = 0; i < 4; ++i) PutFlatCorner(VertexFieldMisc(i), prim + 8 + i * 0xC, static_cast<field_misc::U>(diffuse));
    SetTextureFieldMisc(0, 0);
    field_misc::g.ret_only(0);
    field_misc::g.ret_only(0);
    field_misc::g.set_blend(prim[7], DrawModeFieldMisc());
    field_misc::g.set_shade(1);   // flat
    return DrawVerticesFieldMisc(5, 4);   // TRIANGLESTRIP
}

// 0x5A0C40, code 0x2C (Gpu_SetPolyFT4): a textured flat quad. Four corners of
// 0x10 bytes from +8: float x, y, z, u, v, and the CLUT (+0x16) / tpage (+0x26)
// words in the first two. One colour for all four, in the tpage's blend mode.
long D3d_DrawPolyFT4(const unsigned char* prim) {
    unsigned long diffuse, specular;
    d3d_draw::g.prim_color(prim[4], prim[5], prim[6], prim[7], d3d_draw::Word(prim + 0x26), &diffuse, &specular);
    for (d3d_draw::U i = 0; i < 4; ++i) {
        unsigned char* out = d3d_draw::Vertex(i);
        d3d_draw::PutPosition(out, prim + 8 + i * 0x10);
        d3d_draw::PutLong(out + 0x10, static_cast<d3d_draw::U>(diffuse));
        d3d_draw::PutLong(out + 0x14, static_cast<d3d_draw::U>(specular));
    }
    for (d3d_draw::U i = 0; i < 4; ++i) d3d_draw::PutTexel(d3d_draw::Vertex(i), prim + 0x14 + i * 0x10);
    QuadFarEdge(prim + 0x14, 0x10);   // DIV-0199
    d3d_draw::g.bind_texture(d3d_draw::Word(prim + 0x26), d3d_draw::Word(prim + 0x16));
    d3d_draw::g.ret_only(d3d_draw::Word(prim + 0x26) & 0x400);
    d3d_draw::g.ret_only(d3d_draw::Word(prim + 0x26) & 0x800);
    d3d_draw::g.set_blend(prim[7], d3d_draw::Word(prim + 0x26));
    d3d_draw::g.set_shade(1);   // flat
    return d3d_draw::DrawVertices(5, 4);   // TRIANGLESTRIP
}

// original 0x5A0E80, table entry 4 (code 0x30, called at 0x59F0E2): a Gouraud
// triangle - three corners, a triangle list of 3. And, as the original has it,
// the fourth vertex's z (0x7CA9C0) set to 0 - a store no draw of this handler
// reads (three vertices), which the next four-vertex handler overwrites.
extern "C" long __cdecl D3d_DrawPolyG3(const unsigned char* prim) {
    GouraudCorners(prim, 3);
    PutLongBattleDraw(VertexBattleDraw(3) + 8, 0);
    SetTextureBattleDraw(0, 0);
    battle_draw::g.ret_only(0);
    battle_draw::g.ret_only(0);
    battle_draw::g.set_blend(prim[7], DrawModeBattleDraw());
    battle_draw::g.set_shade(2);   // Gouraud
    return DrawVerticesBattleDraw(4, 3);   // TRIANGLELIST
}

// 0x5A1050, table entry 5 (code 0x34, called at 0x59F0F0): a textured Gouraud
// triangle - D3d_DrawPolyGT4 with three corners. Corners of 0x14 bytes from +4:
// r, g, b, (code or pad), float x, y, z, byte u, v; the CLUT +0x16 and the tpage
// +0x2A. A colour per corner with a specular, each in the tpage's blend mode
// (re-read), all three before any vertex; per corner the position, the pair
// and the texel; D3d_BindTexture(tpage, clut); 0x437CC0(0) twice; the blend;
// Gouraud; a triangle list of 3.
long D3d_DrawPolyGT3(const unsigned char* prim) {
    unsigned long diffuse[3], specular[3];
    for (d3d_rest::U i = 0; i < 3; ++i) {
        const unsigned char* rgb = prim + 4 + i * 0x14;
        d3d_rest::g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], d3d_rest::Word(prim + 0x2A), &diffuse[i], &specular[i]);
    }
    for (d3d_rest::U i = 0; i < 3; ++i) {
        unsigned char* out = d3d_rest::Vertex(i);
        const unsigned char* in = prim + 8 + i * 0x14;
        d3d_rest::PutPosition(out, in);
        d3d_rest::PutLong(out + 0x10, static_cast<d3d_rest::U>(diffuse[i]));
        d3d_rest::PutLong(out + 0x14, static_cast<d3d_rest::U>(specular[i]));
        d3d_rest::PutTexel(out, in + 0xC);
    }
    d3d_rest::g.bind_texture(d3d_rest::Word(prim + 0x2A), d3d_rest::Word(prim + 0x16));
    d3d_rest::g.ret_only(0);
    d3d_rest::g.ret_only(0);
    d3d_rest::g.set_blend(prim[7], d3d_rest::Word(prim + 0x2A));
    d3d_rest::g.set_shade(2);   // Gouraud
    return d3d_rest::DrawVertices(4, 3);   // TRIANGLELIST
}

// original 0x5A1290, code 0x38 (Gpu_SetPolyG4): a Gouraud quad, four corners of
// 0x10 bytes from +4: r, g, b, (code or pad), float x, y, z. The four colours
// first, each with the code byte and the mode read again; then the corners.
extern "C" long __cdecl D3d_DrawPolyG4(const unsigned char* prim) {
    unsigned long diffuse[4];
    for (field_misc::U i = 0; i < 4; ++i) {
        const unsigned char* rgb = prim + 4 + i * 0x10;
        field_misc::g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], DrawModeFieldMisc(), &diffuse[i], nullptr);
    }
    for (field_misc::U i = 0; i < 4; ++i) PutFlatCorner(VertexFieldMisc(i), prim + 8 + i * 0x10, static_cast<field_misc::U>(diffuse[i]));
    SetTextureFieldMisc(0, 0);
    field_misc::g.ret_only(0);
    field_misc::g.ret_only(0);
    field_misc::g.set_blend(prim[7], DrawModeFieldMisc());
    field_misc::g.set_shade(2);   // Gouraud
    return DrawVerticesFieldMisc(5, 4);
}

// 0x5A14C0, code 0x3C (Gpu_SetPolyGT4): a textured Gouraud quad. Four corners
// of 0x14 bytes from +4: r, g, b, (code or pad), float x, y, z, u, v, and the
// CLUT (+0x16) / tpage (+0x2A) words. A colour per corner, each in the tpage's
// blend mode, all four computed before any vertex is written.
long D3d_DrawPolyGT4(const unsigned char* prim) {
    unsigned long diffuse[4], specular[4];
    for (d3d_draw::U i = 0; i < 4; ++i) {
        const unsigned char* rgb = prim + 4 + i * 0x14;
        d3d_draw::g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], d3d_draw::Word(prim + 0x2A), &diffuse[i], &specular[i]);
    }
    for (d3d_draw::U i = 0; i < 4; ++i) {
        unsigned char* out = d3d_draw::Vertex(i);
        const unsigned char* in = prim + 8 + i * 0x14;
        d3d_draw::PutPosition(out, in);
        d3d_draw::PutLong(out + 0x10, static_cast<d3d_draw::U>(diffuse[i]));
        d3d_draw::PutLong(out + 0x14, static_cast<d3d_draw::U>(specular[i]));
        d3d_draw::PutTexel(out, in + 0xC);
    }
    QuadFarEdge(prim + 0x14, 0x14);   // DIV-0199
    d3d_draw::g.bind_texture(d3d_draw::Word(prim + 0x2A), d3d_draw::Word(prim + 0x16));
    d3d_draw::g.ret_only(0);
    d3d_draw::g.ret_only(0);
    d3d_draw::g.set_blend(prim[7], d3d_draw::Word(prim + 0x2A));
    d3d_draw::g.set_shade(2);   // Gouraud
    return d3d_draw::DrawVertices(5, 4);
}

// 0x5A17A0, code 0x40 (Gpu_SetLineF2): one flat line, two corners of 0xC bytes
// from +8 (float x, y, z).
long D3d_DrawLineF2(const unsigned char* prim) { return DrawLines(prim, 2); }

// original 0x5A18B0, table entry 11 (code 0x50, called at 0x59F144): a Gouraud
// line - two corners, a line strip of 2. The bare-ret calls get 0 and 1, as
// the other lines' do.
extern "C" long __cdecl D3d_DrawLineG2(const unsigned char* prim) {
    GouraudCorners(prim, 2);
    SetTextureBattleDraw(0, 0);
    battle_draw::g.ret_only(0);
    battle_draw::g.ret_only(1);
    battle_draw::g.set_blend(prim[7], DrawModeBattleDraw());
    battle_draw::g.set_shade(2);
    return d3d_lines::g_wide ? d3d_lines::DrawWide(2) : DrawVerticesBattleDraw(3, 2);   // LINESTRIP; DIV-0079 the quads
}

// original 0x5A1A00, code 0x48 (Gpu_SetLineF3): a flat two-segment polyline,
// three corners of 0xC bytes from +8. The two bare-ret calls get 0 and 1, as
// the other lines' do.
extern "C" long __cdecl D3d_DrawLineF3(const unsigned char* prim) {
    unsigned long diffuse;
    field_misc::g.prim_color(prim[4], prim[5], prim[6], prim[7], DrawModeFieldMisc(), &diffuse, nullptr);
    for (field_misc::U i = 0; i < 3; ++i) PutFlatCorner(VertexFieldMisc(i), prim + 8 + i * 0xC, static_cast<field_misc::U>(diffuse));
    SetTextureFieldMisc(0, 0);
    field_misc::g.ret_only(0);
    field_misc::g.ret_only(1);
    field_misc::g.set_blend(prim[7], DrawModeFieldMisc());
    field_misc::g.set_shade(1);
    return d3d_lines::g_wide ? d3d_lines::DrawWide(3) : DrawVerticesFieldMisc(3, 3);   // LINESTRIP; DIV-0079 the quads
}

// original 0x5A1B50, table entry 12 (code 0x58, called at 0x59F152): a Gouraud
// two-segment polyline - three corners, a line strip of 3.
extern "C" long __cdecl D3d_DrawLineG3(const unsigned char* prim) {
    GouraudCorners(prim, 3);
    SetTextureBattleDraw(0, 0);
    battle_draw::g.ret_only(0);
    battle_draw::g.ret_only(1);
    battle_draw::g.set_blend(prim[7], DrawModeBattleDraw());
    battle_draw::g.set_shade(2);
    return d3d_lines::g_wide ? d3d_lines::DrawWide(3) : DrawVerticesBattleDraw(3, 3);   // LINESTRIP; DIV-0079 the quads
}

// 0x5A1D10, code 0x4C (Gpu_SetLineF4): a flat three-segment polyline, four
// corners of 0xC bytes from +8.
long D3d_DrawLineF4(const unsigned char* prim) { return DrawLines(prim, 4); }

// 0x5A1EA0, table entry 13 (code 0x5C, called at 0x59F160): a Gouraud
// three-segment polyline - D3d_DrawLineG3 (battle_draw.cpp) with four corners.
// Four colours (r, g, b at +4 + i * 0x10, the code +7 and Gfx_DrawTpage's low
// word read again for each, a null specular) before any vertex; four corners of
// 0x10 from +8 (float x, y, z); SetTexture(0, NULL); 0x437CC0(0), (1); the
// blend; Gouraud; a line strip of 4.
long D3d_DrawLineG4(const unsigned char* prim) {
    unsigned long diffuse[4];
    for (d3d_rest::U i = 0; i < 4; ++i) {
        const unsigned char* rgb = prim + 4 + i * 0x10;
        d3d_rest::g.prim_color(rgb[0], rgb[1], rgb[2], prim[7], d3d_rest::DrawMode(), &diffuse[i], nullptr);
    }
    for (d3d_rest::U i = 0; i < 4; ++i) {
        unsigned char* out = d3d_rest::Vertex(i);
        d3d_rest::PutPosition(out, prim + 8 + i * 0x10);
        d3d_rest::PutLong(out + 0x10, static_cast<d3d_rest::U>(diffuse[i]));
    }
    d3d_rest::SetTexture(0, 0);
    d3d_rest::g.ret_only(0);
    d3d_rest::g.ret_only(1);
    d3d_rest::g.set_blend(prim[7], d3d_rest::DrawMode());
    d3d_rest::g.set_shade(2);   // Gouraud
    return d3d_lines::g_wide ? d3d_lines::DrawWide(4) : d3d_rest::DrawVertices(3, 4);   // LINESTRIP; DIV-0079 the quads
}

// 0x5A20D0, code 0x60 (Gpu_SetTile): a flat untextured rectangle - float x
// +8, y +0xC, z +0x10, w +0x14, h +0x18. The far edges are (x + w) and (y + h)
// times the scale, summed and multiplied on the x87 as the original does; z goes
// through fld / fst to all four corners (a signalling NaN comes out quiet, unlike
// the other handlers' mov).
long D3d_DrawTile(const unsigned char* prim) {
    const d3d_draw::U mode = d3d_draw::Long(d3d_draw::At(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF;
    unsigned long diffuse;
    d3d_draw::g.prim_color(prim[4], prim[5], prim[6], prim[7], mode, &diffuse, nullptr);
    const float x = d3d_draw::Float(prim + 8), y = d3d_draw::Float(prim + 0xC), z = d3d_draw::Float(prim + 0x10);
    const float left = d3d_draw::X87Mul(D3d_ScaleX, x);
    const float top = d3d_draw::X87Mul(D3d_ScaleY, y);
    const float right = X87AddMul(d3d_draw::Float(prim + 0x14), x, D3d_ScaleX);
    const float bottom = X87AddMul(d3d_draw::Float(prim + 0x18), y, D3d_ScaleY);
    const float sz = d3d_draw::X87Pass(z);
    const float rhw = d3d_draw::X87Div(d3d_draw::FloatAt(d3d_draw::kRhwNumerator), z);
    const float xs[4] = {left, right, left, right}, ys[4] = {top, top, bottom, bottom};
    for (d3d_draw::U i = 0; i < 4; ++i) {
        unsigned char* out = d3d_draw::Vertex(i);
        d3d_draw::PutFloat(out + 0x00, xs[i]);
        d3d_draw::PutFloat(out + 0x04, ys[i]);
        d3d_draw::PutFloat(out + 0x08, sz);
        d3d_draw::PutFloat(out + 0x0C, rhw);
        d3d_draw::PutLong(out + 0x10, static_cast<d3d_draw::U>(diffuse));
    }
    d3d_draw::SetTexture(0, 0);
    d3d_draw::g.ret_only(1);
    d3d_draw::g.ret_only(1);
    d3d_draw::g.set_blend(prim[7], d3d_draw::Long(d3d_draw::At(::bof3::Img(Gfx_DrawTpage_addr))) & 0xFFFF);
    d3d_draw::g.set_shade(1);
    return d3d_draw::DrawVertices(5, 4);
}

// 0x5A2220, table entry 16 (code 0x68, called at 0x59F17C): a one-pixel tile,
// drawn as ONE point - float x +8, y +0xC, z +0x10; the colour as the tile's
// (null specular, Gfx_DrawTpage); SetTexture(0, NULL); 0x437CC0(1) twice; the
// blend; flat; a point list of 1. At a scale of 2 that is one pixel of the four
// the PlayStation's pixel covers (docs/d3d-rest.md section 4).
//
// DIVERGENCE DIV-0077 (the owner, 2026-10-06): with g_tile1_quad on, the tile
// is the PlayStation pixel's whole footprint - a D3d_ScaleX by D3d_ScaleY quad
// from the scaled corner, as D3d_DrawTile draws a TILE of w = h = 1 (a
// triangle strip of four); the colour, blend and shade as the point's. The
// switch is set by D3dRest_Inject after the self-test, which compares the
// point; BOF3X_TILE1=0 leaves it off (the original's point).
unsigned char g_tile1_quad = 0;

long D3d_DrawTile1(const unsigned char* prim) {
    unsigned long diffuse;
    d3d_rest::g.prim_color(prim[4], prim[5], prim[6], prim[7], d3d_rest::DrawMode(), &diffuse, nullptr);
    if (g_tile1_quad != 0) {
        const float x = d3d_rest::Float(prim + 8), y = d3d_rest::Float(prim + 0xC), z = d3d_rest::Float(prim + 0x10);
        const float left = d3d_rest::X87Mul(D3d_ScaleX, x);
        const float top = d3d_rest::X87Mul(D3d_ScaleY, y);
        const float right = d3d_rest::X87Mul(D3d_ScaleX, x + 1.0f);
        const float bottom = d3d_rest::X87Mul(D3d_ScaleY, y + 1.0f);
        const float rhw = d3d_rest::X87Div(d3d_rest::FloatAt(d3d_rest::kRhwNumerator), z);
        const float xs[4] = {left, right, left, right}, ys[4] = {top, top, bottom, bottom};
        for (d3d_rest::U i = 0; i < 4; ++i) {
            unsigned char* out = d3d_rest::Vertex(i);
            d3d_rest::PutFloat(out + 0x00, xs[i]);
            d3d_rest::PutFloat(out + 0x04, ys[i]);
            d3d_rest::PutLong(out + 0x08, d3d_rest::Long(prim + 0x10));
            d3d_rest::PutFloat(out + 0x0C, rhw);
            d3d_rest::PutLong(out + 0x10, static_cast<d3d_rest::U>(diffuse));
        }
        d3d_rest::SetTexture(0, 0);
        d3d_rest::g.ret_only(1);
        d3d_rest::g.ret_only(1);
        d3d_rest::g.set_blend(prim[7], d3d_rest::DrawMode());
        d3d_rest::g.set_shade(1);
        return d3d_rest::DrawVertices(5, 4);   // TRIANGLESTRIP
    }
    unsigned char* out = d3d_rest::Vertex(0);
    d3d_rest::PutPosition(out, prim + 8);
    d3d_rest::PutLong(out + 0x10, static_cast<d3d_rest::U>(diffuse));
    d3d_rest::SetTexture(0, 0);
    d3d_rest::g.ret_only(1);
    d3d_rest::g.ret_only(1);
    d3d_rest::g.set_blend(prim[7], d3d_rest::DrawMode());
    d3d_rest::g.set_shade(1);
    return d3d_rest::DrawVertices(1, 1);   // POINTLIST
}

// 0x5A2300, code 0x64 (SPRT): a sprite of any size, w and h from the primitive.
long D3d_DrawSprt(const unsigned char* prim) { return sprt_draw::DrawSprite(prim, 0, 0); }

// 0x5A2520, code 0x74 (SPRT_8): an 8 x 8 sprite - x + 8.0 from 0x5C41CC.
long D3d_DrawSprt8(const unsigned char* prim) { return sprt_draw::DrawSprite(prim, sprt_draw::kEight, 8); }

// 0x5A2710, code 0x7C (SPRT_16): a 16 x 16 sprite - x + 16.0 from 0x5C41D0.
long D3d_DrawSprt16(const unsigned char* prim) { return sprt_draw::DrawSprite(prim, sprt_draw::kSixteen, 16); }

// 0x5A2900. The primitive is POLY_FT4-shaped (Text_EmitGlyph 0x516D50 and
// four other builders of code 0x6C): r, g, b, code at +4..+7; corner i's
// s16 x, y at +8 + 8i / +0xA + 8i and its u, v bytes at +0xC + 8i / +0xD + 8i;
// the CLUT id at +0xE and the glyph index at +0x16.
long D3d_DrawGlyph(const unsigned char* prim) {
    using namespace glyph_draw;
    // Two locals the colour helper fills; it always writes both (it is handed
    // a non-null specular pointer).
    unsigned long diffuse, specular;
    glyph_draw::g.prim_color(prim[4], prim[5], prim[6], prim[7], 0, &diffuse, &specular);

    // The four corners: sx, sy = the primitive's x, y times the scale, no
    // half-pixel offset; sz 0.99 and rhw 0.1 as immediates; the colour pair.
    // Each product is exact before it is rounded to float (a 16-bit integer
    // times a float), so x87 and SSE agree whatever the precision control.
    unsigned char* v = glyph_draw::At(::bof3::Img(D3d_Vertices_addr));
    for (glyph_draw::U i = 0; i < 4; ++i) {
        unsigned char* out = v + i * 0x20;
        const unsigned char* in = prim + i * 8;
        glyph_draw::PutFloat(out + 0x00, static_cast<float>(glyph_draw::Short(in + 8)) * D3d_ScaleX);
        glyph_draw::PutFloat(out + 0x04, static_cast<float>(glyph_draw::Short(in + 0xA)) * D3d_ScaleY);
        glyph_draw::PutLong(out + 0x08, kSz);
        glyph_draw::PutLong(out + 0x0C, kRhw);
        glyph_draw::PutLong(out + 0x10, static_cast<glyph_draw::U>(diffuse));
        glyph_draw::PutLong(out + 0x14, static_cast<glyph_draw::U>(specular));
    }
    // The texture coordinates: the byte u, v doubled (the glyph texture holds
    // each PSX texel as 2 x 2) times the double 1/32 at 0x5C4618 - the 24 x 24
    // glyph in a 32 x 32 surface. Capcom's add nothing to 2u: every screen
    // pixel then samples exactly on the edge between two texels (D17).
    // DIV-0025 adds the pixel-centre inset (TexelInset, per axis from the
    // scale) on both the near and the far edge, so the texel-per-pixel scale
    // is kept; g_texel_inset is the switch (GlyphTexelCentres), 0 for
    // Capcom's arithmetic.
    const double inv32 = *reinterpret_cast<const double*>(glyph_draw::At(kInv32));
    const double inset_u = g_texel_inset != 0.0f ? TexelInset(D3d_ScaleX) : 0.0;
    const double inset_v = g_texel_inset != 0.0f ? TexelInset(D3d_ScaleY) : 0.0;
    for (glyph_draw::U i = 0; i < 4; ++i) {
        unsigned char* out = v + i * 0x20;
        const unsigned char* in = prim + i * 8;
        glyph_draw::PutFloat(out + 0x18, static_cast<float>((2.0 * in[0xC] + inset_u) * inv32));
        glyph_draw::PutFloat(out + 0x1C, static_cast<float>((2.0 * in[0xD] + inset_v) * inv32));
    }

    // The glyph's texture (its return is not read), then the state: two calls
    // of a bare ret, the blend for the primitive's code - re-read from the
    // primitive - and flat shading. The blend's alpha enable is then turned
    // off again here, unconditionally: glyphs draw unblended.
    glyph_draw::g.glyph_texture(glyph_draw::Word(prim + 0x16), glyph_draw::Word(prim + 0xE));
    glyph_draw::g.ret_only(1);
    glyph_draw::g.ret_only(1);
    glyph_draw::g.set_blend(prim[7], 0);
    glyph_draw::g.set_shade(1);
    void** device = glyph_draw::Device();
    reinterpret_cast<glyph_draw::Com3>(glyph_draw::Method(device, 0x58))(device, 0x1B, 0);   // SetRenderState(ALPHABLENDENABLE, FALSE)
    device = glyph_draw::Device();
    // DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, vertices, 4, 0); its
    // result is the handler's (the caller 0x59F1B4 does not read it).
    return reinterpret_cast<glyph_draw::Com6>(glyph_draw::Method(device, 0x70))(device, 5, 0x1C4, ::bof3::Img(D3d_Vertices_addr), 4, 0);
    // Not kept: the original computes every fild's operand in its own
    // argument slot, so the caller's pushed `prim` comes back as 2 * the last
    // v. The caller pops it unread (`add esp, 4` at 0x59F1B9).
}

// 0x5A2BC0. Finds - or builds - the texture of one glyph in one CLUT, sets it
// on the device, marks the entry used this frame and returns its index.
int Font_GlyphTexture(unsigned glyph, unsigned clut) {
    using namespace glyph_draw;
    // One pass: a hit is the glyph word and the CLUT word equal to the
    // arguments - compared as 32 bits, the words zero-extended, so an argument
    // with its high half set never hits - and the generation the entry was
    // built from equal to the CLUT row's now (Gfx_ClutRows, row clut >> 6,
    // unsigned). On the way, the first entry not used this frame.
    int free_slot = -1;
    glyph_draw::U i;
    for (i = 0; i < glyph_draw::kEntries; ++i) {
        const unsigned char* e = glyph_draw::At(::bof3::Img(Font_TexCache_addr) + i * kEntry);
        if (glyph_draw::Word(e) == glyph && glyph_draw::Word(e + 2) == clut && glyph_draw::Long(e + 4) == glyph_draw::Long(glyph_draw::At(::bof3::Img(Gfx_ClutRows_addr) + (clut >> 6) * 8))) break;
        if (free_slot == -1 && glyph_draw::Word(e + 0x10) == 0) free_slot = static_cast<int>(i);
    }
    if (i == glyph_draw::kEntries && free_slot != -1) {
        i = static_cast<glyph_draw::U>(free_slot);
        if (Gfx_RenderFlags & 1) {
            glyph_draw::g.build_texture(free_slot, glyph, clut);
        } else {
            // A texture is built outside the scene.
            void** device = glyph_draw::Device();
            reinterpret_cast<Com1>(glyph_draw::Method(device, 0x28))(device);   // EndScene
            glyph_draw::g.build_texture(free_slot, glyph, clut);
            device = glyph_draw::Device();
            reinterpret_cast<Com1>(glyph_draw::Method(device, 0x24))(device);   // BeginScene
        }
    }
    // Kept, and D18 in docs/known-defects.md: with all 128 entries used this
    // frame and none a hit, i is 128 here - one entry past the table, which is
    // the vertex block. SetTexture is handed the bits of vertex 0's sy
    // (0x7CA95C) as a texture, and the in-use word lands on the low half of
    // vertex 0's sz (0x7CA960). The addresses below reach exactly those bytes.
    if (!(Gfx_RenderFlags & 1)) {
        void** device = glyph_draw::Device();
        reinterpret_cast<glyph_draw::Com3>(glyph_draw::Method(device, 0x98))(device, 0, glyph_draw::Long(glyph_draw::At(::bof3::Img(Font_TexCache_addr) + i * kEntry + 0xC))); // SetTexture(0, tex)
    }
    glyph_draw::At(::bof3::Img(Font_TexCache_addr) + i * kEntry + 0x10)[0] = 1;
    glyph_draw::At(::bof3::Img(Font_TexCache_addr) + i * kEntry + 0x11)[0] = 0;
    return static_cast<int>(i);
}

// 0x5A2CA0. Builds Font_TexCache entry `slot` for `glyph` in `clut`: the 16
// colours from Gfx_ClutPixels(clut) (first, before anything else is read), the
// glyph at Font_GlyphData + glyph * 0x120 (the pointer read after that call).
//
// Software surfaces (Gfx_RenderFlags bit 0): if the entry has no surface (+8),
// a 32 x 32 plain one is made into +8, cleared to 0 and +0xC zeroed; the
// entry's surface is locked and the glyph unpacked into it. Direct3D: if no
// surface, a 32 x 32 texture into +8 / +0xC, colour-keyed on 0 under flag bit
// 5 (re-read after the create); the staging surface is locked, the glyph
// unpacked into it, and {0, 0, 24, 24} Blt from it into the entry's surface.
// An existing surface is reused as it is - not cleared, not keyed. A failed
// create or Lock returns at once, the entry's glyph, CLUT and generation not
// written; otherwise +0 the glyph's low 16 bits, +2 the CLUT's, +4 the
// generation of Gfx_ClutRows at (clut sar 6) * 8 - signed, on the whole
// argument. Every surface pointer is re-read from the entry or from
// Dd_StageSurface at each use, as the original's are.
void Font_BuildGlyphTexture(int slot, unsigned glyph, unsigned clut) {
    const void* palette = tex_cells::g.clut_pixels(clut);
    const tex_cells::U src = tex_cells::Long(::bof3::Img(kGlyphData.v)) + glyph * 0x120u;
    tex_cells::Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    const tex_cells::U entry = ::bof3::Img(Font_TexCache_addr) + static_cast<tex_cells::U>(slot) * tex_cells::kFontEntryBytes;
    const tex_cells::U surface_slot = entry + 8;
    if (Gfx_RenderFlags & 1) {
        if (tex_cells::Long(surface_slot) == 0) {
            if (!Dd_CreatePlainSurface(0x20, 0x20, reinterpret_cast<::bof3::Slot32<void>*>(tex_cells::At(surface_slot)), 0)) return;
            Dd_ClearSurface(tex_cells::Ptr(tex_cells::Long(surface_slot)));
            tex_cells::PutLong(entry + 0xC, 0);
        }
        if (tex_cells::Com(tex_cells::Long(surface_slot), tex_cells::kLock, tex_cells::kNoRect, desc.b, tex_cells::kLockWait, 0u) != 0) return;
        tex_cells::g.unpack_glyph(tex_cells::Ptr(desc.Surface()), tex_cells::Ptr(src), palette, static_cast<int>(desc.Pitch()));
        tex_cells::Com(tex_cells::Long(surface_slot), tex_cells::kUnlock, 0u);
    } else {
        if (tex_cells::Long(surface_slot) == 0) {
            if (!Dd_CreateTextureSurface(0x20, 0x20, reinterpret_cast<::bof3::Slot32<void>*>(tex_cells::At(surface_slot)),
                                         reinterpret_cast<::bof3::Slot32<void>*>(tex_cells::At(entry + 0xC)), 0))
                return;
            if (Gfx_RenderFlags & 0x20) {
                tex_cells::U key[2] = {0, 0};   // DDCOLORKEY {0, 0}
                tex_cells::Com(tex_cells::Long(surface_slot), tex_cells::kSetColorKey, tex_cells::kSrcBltKey, key);
            }
        }
        if (tex_cells::Com(tex_cells::Long(::bof3::Img(tex_cells::kStage.v)), tex_cells::kLock, tex_cells::kNoRect, desc.b, tex_cells::kLockWait, 0u) != 0) return;
        tex_cells::g.unpack_glyph(tex_cells::Ptr(desc.Surface()), tex_cells::Ptr(src), palette, static_cast<int>(desc.Pitch()));
        tex_cells::Com(tex_cells::Long(::bof3::Img(tex_cells::kStage.v)), tex_cells::kUnlock, 0u);
        tex_cells::Rect rect = {0, 0, 0x18, 0x18};
        const tex_cells::U surface = tex_cells::Long(surface_slot);
        const tex_cells::U stage = tex_cells::Long(::bof3::Img(tex_cells::kStage.v));
        tex_cells::Com(surface, tex_cells::kBlt, &rect, stage, &rect, tex_cells::kBltWait, 0u);
    }
    tex_cells::PutWord(entry, glyph);
    tex_cells::PutWord(entry + 2, clut);
    tex_cells::PutLong(entry + 4, tex_cells::Long(::bof3::Img(tex_cells::kClutRows.v) + static_cast<tex_cells::U>(static_cast<std::int32_t>(clut) >> 6) * 8));
}

// 0x5A2E70. Fills the whole of `surface` with colour 0: Blt(NULL, NULL, NULL,
// DDBLT_COLORFILL | DDBLT_WAIT, &fx), fx a zeroed DDBLTFX of 0x64 bytes with
// its dwSize set. Returns what the Blt returned; no caller reads it (0x5A2D38,
// 0x5A3339, 0x5A37E3, and the two in the Direct3D set-up, 0x5A55FF and
// 0x5A56F4, each `add esp, 4` and move on).
long Dd_ClearSurface(void* surface) {
    alignas(4) unsigned char fx[0x64] = {};
    tex_cells::PutLong(fx, 0x64);
    return tex_cells::Com(tex_cells::Addr(surface), tex_cells::kBlt, tex_cells::kNoRect, tex_cells::kNoRect, tex_cells::kNoRect, kBltColorFillWait, static_cast<void*>(fx));
}

// 0x5A2EB0, code 0x84 (Gpu_SetCode84 - no PSX GPU code; the port's): a sprite
// made of SpriteCell records, drawn from one texture D3d_CellTexture composes
// and caches. Float x +8, y +0xC, scale x +0x10, scale y +0x14; u16 first cell
// +0x18, cell count +0x1A, CLUT +0x1C, flags +0x1E (bit 0x400 flips x, and it
// is the colour's and the blend's tpage). The cache entry gives the sprite's
// extent in cell units (s16 left +8, right +0xA, top +0xC, bottom +0xE) and
// its texture's used size (u16 +0, +2) and full size (u16 +4, +6); the texture
// coordinates run from half a texel in to half a texel short of the used size.
// sz 0.99 and rhw 0.1 as immediates. The entry index is not checked: an index
// out of the table reads past it, as the original does.
long D3d_DrawCellSprite(const unsigned char* prim) {
    unsigned long diffuse, specular;
    d3d_draw::g.prim_color(prim[4], prim[5], prim[6], prim[7], d3d_draw::Word(prim + 0x1E), &diffuse, &specular);
    const int index = d3d_draw::g.cell_texture(d3d_draw::Word(prim + 0x18), d3d_draw::Word(prim + 0x1A), d3d_draw::Word(prim + 0x1C), d3d_draw::Word(prim + 0x1E));
    const unsigned char* e = d3d_draw::At(::bof3::Img(D3d_CellTexCache_addr) + static_cast<d3d_draw::U>(index) * kCellEntry);
    const bool flip = (prim[0x1F] & 4) != 0;
    const float x = d3d_draw::Float(prim + 8), y = d3d_draw::Float(prim + 0xC), sx = d3d_draw::Float(prim + 0x10), sy = d3d_draw::Float(prim + 0x14);
    const float left = X87IntMulAddMul(d3d_draw::Short(e + 8), sx, x, D3d_ScaleX, flip);
    const float right = X87IntMulAddMul(d3d_draw::Short(e + 0xA), sx, x, D3d_ScaleX, flip);
    const float top = X87IntMulAddMul(d3d_draw::Short(e + 0xC), sy, y, D3d_ScaleY, false);
    const float bottom = X87IntMulAddMul(d3d_draw::Short(e + 0xE), sy, y, D3d_ScaleY, false);
    const float half = d3d_draw::FloatAt(kHalf);
    const auto used_w = static_cast<std::int32_t>(d3d_draw::Word(e + 0)), used_h = static_cast<std::int32_t>(d3d_draw::Word(e + 2));
    const auto full_w = static_cast<std::int32_t>(d3d_draw::Word(e + 4)), full_h = static_cast<std::int32_t>(d3d_draw::Word(e + 6));
    const float u0 = X87DivInt(half, full_w), v0 = X87DivInt(half, full_h);
    const float u1 = X87IntSubDivInt(used_w, half, full_w), v1 = X87IntSubDivInt(used_h, half, full_h);
    const float xs[4] = {left, right, left, right}, ys[4] = {top, top, bottom, bottom};
    const float us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
    for (d3d_draw::U i = 0; i < 4; ++i) {
        unsigned char* out = d3d_draw::Vertex(i);
        d3d_draw::PutFloat(out + 0x00, xs[i]);
        d3d_draw::PutFloat(out + 0x04, ys[i]);
        d3d_draw::PutLong(out + 0x08, 0x3F7D70A4);   // 0.99
        d3d_draw::PutLong(out + 0x0C, 0x3DCCCCCD);   // 0.1
        d3d_draw::PutLong(out + 0x10, static_cast<d3d_draw::U>(diffuse));
        d3d_draw::PutLong(out + 0x14, static_cast<d3d_draw::U>(specular));
        d3d_draw::PutFloat(out + 0x18, us[i]);
        d3d_draw::PutFloat(out + 0x1C, vs[i]);
    }
    d3d_draw::g.ret_only(1);
    d3d_draw::g.set_blend(prim[7], d3d_draw::Word(prim + 0x1E));
    d3d_draw::g.set_shade(1);
    return d3d_draw::DrawVertices(5, 4);
}

int D3d_CellTexture(unsigned first, unsigned count, unsigned clut, unsigned flags) {
    d3d_draw::U best = 0xFFFF;
    d3d_draw::U victim = count;
    d3d_draw::U i;
    for (i = 0; i < kCellEntries; ++i) {
        const unsigned char* e = d3d_draw::At(::bof3::Img(D3d_CellTexCache_addr) + i * kCellEntry);
        if (d3d_draw::Long(e) == 0) {
            d3d_draw::g.cell_build(static_cast<int>(i), first, count, clut, flags & 0x800);
            NoteCellFoot(i, first, count);
            goto used;
        }
        if (d3d_draw::Word(e + 0x12) == count && d3d_draw::Word(e + 0x10) == clut) {
            d3d_draw::U sum = 0;
            const d3d_draw::U bytes = count * 8;
            for (d3d_draw::U k = 0; k < bytes; k += 4) sum += d3d_draw::Long(d3d_draw::At(::bof3::Img(SpriteCell_Table_addr) + first * 8 + k));
            if (d3d_draw::Long(e + 0x18) == sum) {
                if (d3d_draw::Long(e + 0x1C) != d3d_draw::Long(d3d_draw::At(::bof3::Img(Gfx_ClutRows_addr) + (clut >> 6) * 8)) && d3d_draw::Word(e + 0x14) == 0)
                    d3d_draw::g.cell_refresh(static_cast<int>(i), first, count, clut);
                goto used;
            }
        }
        if (static_cast<std::int32_t>(best) > static_cast<std::int32_t>(d3d_draw::Word(e + 0x16)) && d3d_draw::Word(e + 0x14) == 0) {
            best = d3d_draw::Word(e + 0x16);
            victim = i;
        }
    }
    i = victim;
    if (victim == count) {
        // D27 (docs/known-defects.md): no entry qualified - all 128 used this
        // frame - and the original builds into entry `count`. Diagnostic only.
        static unsigned logged = 0;
        if (logged < 50) {
            ++logged;
            bof3::Log("d3d_draw: D27 - the cell-texture cache has no victim (all %u used this frame); entry %u (= count) rebuilt",
                      kCellEntries, count);
        }
    }
    d3d_draw::g.cell_build(static_cast<int>(i), first, count, clut, flags & 0x800);
    NoteCellFoot(i, first, count);
used:
    const unsigned char render_flags = Gfx_RenderFlags;
    unsigned char* e = d3d_draw::At(::bof3::Img(D3d_CellTexCache_addr) + i * kCellEntry);
    d3d_draw::PutWord(e + 0x16, d3d_draw::Word(e + 0x16) + 1);
    d3d_draw::PutWord(e + 0x14, 1);
    if (!(render_flags & 1)) d3d_draw::SetTexture(0, d3d_draw::Long(e + 0x24));
    return static_cast<int>(i);
}

// 0x5A32B0. Composes the `count` SpriteCell records from `first` into
// D3d_CellTexCache entry `slot`. D3d_CellTexture passes its flags & 0x800 as
// `backdrop`.
//
// 1. The staging surface's canvas: unless Gfx_RenderFlags bit 0 (read once,
//    on entry), with `backdrop` non-zero and a surface at Dd_CellBackdrop
//    0x6BEA00, a Blt of that surface's {0, 0, 320, 240} to the stage's {0, 8,
//    320, 248}; otherwise Dd_ClearSurface of the stage.
// 2. Lock the stage (a failure returns at once, the entry untouched); the
//    palette Gfx_ClutPixels(clut); every record drawn (DrawCells), keeping the
//    extent - from (0, 0), so it always holds the origin - and the 32-bit sum
//    of the records' dwords; Unlock.
// 3. D3d_FreeCellTexture(slot), then the entry: extent s16 +8 left, +0xA
//    right, +0xC top, +0xE bottom; used size +0 right - left, +2 bottom - top;
//    +0x10 the CLUT's low 16 bits, +0x12 the count's; +0x18 the sum; +0x1C the
//    generation of Gfx_ClutRows at (clut shr 6) * 8 - unsigned here.
// 4. The flag byte re-read. Software: a plain surface of the used size into
//    +0x20 (a failure returns), a Blt of the stage's {left + 160, top + 128,
//    right + 160, bottom + 128} to its {0, 0, used w, used h}, and +4 / +6
//    the used size (re-read) rounded up to 4 in 16 bits. Direct3D: device
//    EndScene; D3d_FitTextureSize of the used size (re-read); a texture of
//    that size into +0x20 / +0x24 - a failure returns WITHOUT BeginScene
//    (known-defects D31); if the texture is narrower than the used width
//    (signed), the used height becomes texture width / used width * used
//    height through the x87 and _ftol and the used width the texture's, both
//    stored and the Blt's destination made {0, 0, those}; the same Blt into
//    +0x20; +4 / +6 the texture's size; device BeginScene. Only the width is
//    tested: a texture shorter than the used height gets a destination taller
//    than itself.
void D3d_BuildCellTexture(int slot, unsigned first, unsigned count, unsigned clut, unsigned backdrop) {
    const tex_cells::U flags = Gfx_RenderFlags;
    tex_cells::U from_surface = 0;
    if (!(flags & 1) && backdrop != 0 && (from_surface = tex_cells::Long(::bof3::Img(kBackdrop.v))) != 0) {
        tex_cells::Rect to = {0, 8, 0x140, 0xF8};
        tex_cells::Rect from = {0, 0, 0x140, 0xF0};
        tex_cells::Com(tex_cells::Long(::bof3::Img(tex_cells::kStage.v)), tex_cells::kBlt, &to, from_surface, &from, tex_cells::kBltWait, 0u);
    } else {
        Dd_ClearSurface(Dd_StageSurface);
    }
    Extent extent = {0, 0, 0, 0, 0};
    tex_cells::Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    if (tex_cells::Com(tex_cells::Long(::bof3::Img(tex_cells::kStage.v)), tex_cells::kLock, tex_cells::kNoRect, desc.b, tex_cells::kLockWait, 0u) != 0) return;
    const void* palette = tex_cells::g.clut_pixels(clut);
    DrawCells(first, count, palette, desc, &extent);
    tex_cells::Com(tex_cells::Long(::bof3::Img(tex_cells::kStage.v)), tex_cells::kUnlock, 0u);
    D3d_FreeCellTexture(slot);

    const tex_cells::U entry = ::bof3::Img(D3d_CellTexCache_addr) + static_cast<tex_cells::U>(slot) * kCellEntryBytes;
    tex_cells::PutWord(entry + 8, static_cast<tex_cells::U>(extent.left));
    tex_cells::PutWord(entry + 0xA, static_cast<tex_cells::U>(extent.right));
    tex_cells::PutWord(entry + 0xC, static_cast<tex_cells::U>(extent.top));
    tex_cells::PutWord(entry + 0xE, static_cast<tex_cells::U>(extent.bottom));
    tex_cells::PutWord(entry + 0, static_cast<tex_cells::U>(extent.right - extent.left));
    tex_cells::PutWord(entry + 2, static_cast<tex_cells::U>(extent.bottom - extent.top));
    tex_cells::PutWord(entry + 0x12, count);
    tex_cells::PutWord(entry + 0x10, clut);
    tex_cells::PutLong(entry + 0x18, extent.checksum);
    tex_cells::PutLong(entry + 0x1C, tex_cells::Long(::bof3::Img(tex_cells::kClutRows.v) + (clut >> 6) * 8));
    tex_cells::Rect from = {extent.left + 0xA0, extent.top + 0x80, extent.right + 0xA0, extent.bottom + 0x80};
    const tex_cells::U flags_now = Gfx_RenderFlags;
    tex_cells::Rect to = {0, 0, static_cast<std::int32_t>(tex_cells::Word(entry + 0)), static_cast<std::int32_t>(tex_cells::Word(entry + 2))};
    if (flags_now & 1) {
        if (!Dd_CreatePlainSurface(static_cast<unsigned>(to.r), static_cast<unsigned>(to.b),
                                   reinterpret_cast<::bof3::Slot32<void>*>(tex_cells::At(entry + 0x20)), 0))
            return;
        const tex_cells::U surface = tex_cells::Long(entry + 0x20);
        const tex_cells::U stage = tex_cells::Long(::bof3::Img(tex_cells::kStage.v));
        tex_cells::Com(surface, tex_cells::kBlt, &to, stage, &from, tex_cells::kBltWait, 0u);
        tex_cells::PutWord(entry + 4, (tex_cells::Word(entry + 0) + 3) & 0xFFFC);
        tex_cells::PutWord(entry + 6, (tex_cells::Word(entry + 2) + 3) & 0xFFFC);
        return;
    }
    tex_cells::Com(tex_cells::Long(::bof3::Img(kDevice.v)), kEndScene);
    unsigned texture_w, texture_h;
    D3d_FitTextureSize(tex_cells::Word(entry + 0), tex_cells::Word(entry + 2), &texture_w, &texture_h);
    if (!Dd_CreateTextureSurface(texture_w, texture_h, reinterpret_cast<::bof3::Slot32<void>*>(tex_cells::At(entry + 0x20)),
                                 reinterpret_cast<::bof3::Slot32<void>*>(tex_cells::At(entry + 0x24)), 0))
        return;
    const tex_cells::U used_w = tex_cells::Word(entry + 0);
    if (static_cast<std::int32_t>(texture_w) < static_cast<std::int32_t>(used_w)) {
        const tex_cells::U used_h = tex_cells::Word(entry + 2);
        tex_cells::PutWord(entry + 2, ScaledHeight(static_cast<std::int32_t>(texture_w), static_cast<std::int32_t>(used_w),
                                        static_cast<std::int32_t>(used_h)));
        tex_cells::PutWord(entry + 0, texture_w);
        to.r = static_cast<std::int32_t>(texture_w & 0xFFFF);
        to.b = static_cast<std::int32_t>(tex_cells::Word(entry + 2));
    }
    const tex_cells::U surface = tex_cells::Long(entry + 0x20);
    const tex_cells::U stage = tex_cells::Long(::bof3::Img(tex_cells::kStage.v));
    tex_cells::Com(surface, tex_cells::kBlt, &to, stage, &from, tex_cells::kBltWait, 0u);
    tex_cells::PutWord(entry + 4, texture_w);
    tex_cells::PutWord(entry + 6, texture_h);
    tex_cells::Com(tex_cells::Long(::bof3::Img(kDevice.v)), kBeginScene);
}

// 0x5A3790. Empties D3d_CellTexCache entry `slot`: Release of the texture
// (+0x24) if any, then of the surface (+0x20, read after that Release) if
// any, then all 0x28 bytes zeroed. No bound on `slot`. Callers
// D3d_BuildCellTexture and the Direct3D teardown 0x5A6760 (all 128).
void D3d_FreeCellTexture(int slot) {
    const tex_cells::U entry = ::bof3::Img(D3d_CellTexCache_addr) + static_cast<tex_cells::U>(slot) * kCellEntryBytes;
    const tex_cells::U texture = tex_cells::Long(entry + 0x24);
    if (texture != 0) tex_cells::Com(texture, tex_cells::kRelease);
    const tex_cells::U surface = tex_cells::Long(entry + 0x20);
    if (surface != 0) tex_cells::Com(surface, tex_cells::kRelease);
    std::memset(tex_cells::At(entry), 0, kCellEntryBytes);
}

// 0x5A37D0. Re-draws D3d_CellTexCache entry `slot`'s cells for a changed
// palette, into the surface it already has: the stage cleared (always -
// never the backdrop, whatever the build had), locked (a failure returns at
// once), the palette Gfx_ClutPixels(clut), the records drawn as the build
// draws them (no extent, no checksum), unlocked; +0x1C the generation of
// Gfx_ClutRows at (clut shr 6) * 8. Then the Blt the build made, from the
// entry's stored extent (s16 +8..+0xE, read after that store) to {0, 0, +0,
// +2}: under Gfx_RenderFlags bit 0 (read after the Unlock) at once, else
// between device EndScene and BeginScene with the entry's surface read after
// the EndScene. The surface must exist: nothing tests +0x20 (D31).
void D3d_RefreshCellTexture(int slot, unsigned first, unsigned count, unsigned clut) {
    Dd_ClearSurface(Dd_StageSurface);
    tex_cells::Desc desc;
    Dd_InitSurfaceDesc(desc.b);
    if (tex_cells::Com(tex_cells::Long(::bof3::Img(tex_cells::kStage.v)), tex_cells::kLock, tex_cells::kNoRect, desc.b, tex_cells::kLockWait, 0u) != 0) return;
    const void* palette = tex_cells::g.clut_pixels(clut);
    DrawCells(first, count, palette, desc, nullptr);
    tex_cells::Com(tex_cells::Long(::bof3::Img(tex_cells::kStage.v)), tex_cells::kUnlock, 0u);
    const tex_cells::U entry = ::bof3::Img(D3d_CellTexCache_addr) + static_cast<tex_cells::U>(slot) * kCellEntryBytes;
    tex_cells::PutLong(entry + 0x1C, tex_cells::Long(::bof3::Img(tex_cells::kClutRows.v) + (clut >> 6) * 8));
    tex_cells::Rect from = {tex_cells::Short(entry + 8) + 0xA0, tex_cells::Short(entry + 0xC) + 0x80, tex_cells::Short(entry + 0xA) + 0xA0,
                 tex_cells::Short(entry + 0xE) + 0x80};
    const tex_cells::U flags = Gfx_RenderFlags;
    tex_cells::Rect to = {0, 0, static_cast<std::int32_t>(tex_cells::Word(entry + 0)), static_cast<std::int32_t>(tex_cells::Word(entry + 2))};
    if (flags & 1) {
        const tex_cells::U surface = tex_cells::Long(entry + 0x20);
        const tex_cells::U stage = tex_cells::Long(::bof3::Img(tex_cells::kStage.v));
        tex_cells::Com(surface, tex_cells::kBlt, &to, stage, &from, tex_cells::kBltWait, 0u);
        return;
    }
    tex_cells::Com(tex_cells::Long(::bof3::Img(kDevice.v)), kEndScene);
    const tex_cells::U surface = tex_cells::Long(entry + 0x20);
    const tex_cells::U stage = tex_cells::Long(::bof3::Img(tex_cells::kStage.v));
    tex_cells::Com(surface, tex_cells::kBlt, &to, stage, &from, tex_cells::kBltWait, 0u);
    tex_cells::Com(tex_cells::Long(::bof3::Img(kDevice.v)), kBeginScene);
}

// 0x5A5050; sole caller Gpu_PutDrawEnv. Nothing unless D3d_BackMaterial and
// D3d_Viewport are both set (the software surfaces create neither). A
// D3DMATERIAL on the stack, zeroed: dwSize 0x50, diffuse and ambient both
// (r, g, b, 1.0) with each channel (c & 0xFF) * the image's 1/255 as a float,
// dwRampSize 0x20; SetMaterial on the material read at entry (not again),
// then SetBackground on the viewport read again with D3d_BackMaterialHandle.
// The x87 (fild, fmul by a float, fstp to a float) is exact before the store
// under any precision - a byte times a 24-bit mantissa fits in 32 bits - so
// one rounding of the exact product to float is what it computes; ours is
// that, in double.
void D3d_SetBackColor(unsigned r, unsigned g, unsigned b) {
    void* const material = D3d_BackMaterial;
    if (material == nullptr) return;
    if (display_env::Long(::bof3::Img(kViewport.v)) == 0) return;
    const double k = display_env::Float(kInverse255At);
    const float c[3] = {static_cast<float>((r & 0xFF) * k), static_cast<float>((g & 0xFF) * k),
                        static_cast<float>((b & 0xFF) * k)};
    const float one = 1.0f;
    unsigned char m[kMaterialBytes];
    std::memset(m, 0, sizeof m);
    const display_env::U size = kMaterialBytes, ramp = 0x20;
    std::memcpy(m, &size, 4);
    std::memcpy(m + 0x04, c, 12);        // diffuse
    std::memcpy(m + 0x10, &one, 4);
    std::memcpy(m + 0x14, c, 12);        // ambient
    std::memcpy(m + 0x20, &one, 4);
    std::memcpy(m + 0x4C, &ramp, 4);     // dwRampSize
    display_env::Method<ComPointer>(material, kSetMaterial)(material, m);
    void* const viewport = D3d_Viewport;
    display_env::Method<ComValue>(viewport, kSetBackground)(viewport, D3d_BackMaterialHandle);
}

extern "C" void __cdecl Display_WindowMoved(int x, int y) {
    Gfx_WindowRect[0] = x;
    Gfx_WindowRect[1] = y;
    Gfx_WindowRect[2] = x + Gfx_ScreenRect[2];   // 0x66B710, the right edge
    Gfx_WindowRect[3] = y + Gfx_ScreenRect[3];   // 0x66B714, the bottom
}

extern "C" int __cdecl Display_Setup(void* hwnd_, int* fullscreen, int* device, int* /*mode*/) {
    HWND hwnd = static_cast<HWND>(hwnd_);

    // 2.1 the desktop
    HDC dc = GetDC(nullptr);
    Display_DesktopWidth = static_cast<U>(GetDeviceCaps(dc, HORZRES));
    PsxRest_TexCacheEnd = static_cast<U>(GetDeviceCaps(dc, VERTRES));
    Display_DesktopBpp = static_cast<U>(GetDeviceCaps(dc, BITSPIXEL) * GetDeviceCaps(dc, PLANES));
    ReleaseDC(nullptr, dc);

    // 2.2 the caches, reset - every one the original zeroes
    Zero(::bof3::Img(Gfx_TexCache_addr), kTexCacheBytes);
    Zero(::bof3::Img(Gfx_ClutRows_addr), kClutRowsBytes);
    Zero(::bof3::Img(Font_TexCache_addr), kFontCacheBytes);
    for (U i = 0; i < kFontCacheBytes / kFontEntry; ++i) PutWordDisplaySetup(::bof3::Img(kFontCache.v) + i * kFontEntry, 0xFFFF);
    Zero(::bof3::Img(kBackdropBlockDisplaySetup.v), kBackdropBytes);
    Zero(::bof3::Img(D3d_CaptureReady_addr), kAfterDrawBytes);
    Zero(::bof3::Img(D3d_CellTexCache_addr), kCellCacheBytes);
    Zero(::bof3::Img(Gfx_PixelFormat_addr), 3 * kPixelFormatRecord);
    PutLongDisplaySetup(::bof3::Img(kRenderFlagsDisplaySetup.v), 0);
    Display_ZeroedUnread = 0;
    D3d_ShadeModeCache = 0;

    // 2.3 the texture coordinates: i / 256 + 0.002 in double, stored as float
    for (U i = 0; i < 256; ++i) PutFloatDisplaySetup(::bof3::Img(D3d_TexCoords_addr) + i * 4, static_cast<float>(i * 0.00390625 + 0.0020000000949949026));

    // 2.4 the device records: 0 the original's synthetic software renderer (kept
    // so that F7's name overlay reads it), 1 this backend. No enumeration.
    if (static_cast<std::int32_t>(Display_DeviceCountLess1) < 1) {
        Zero(::bof3::Img(kDeviceRecords.v), 2 * kDeviceRecord);
        Zero(::bof3::Img(kModeLists.v), kModeListsBytes);
        PutLongDisplaySetup(::bof3::Img(kDeviceRecords.v) + 0, 1);
        PutLongDisplaySetup(::bof3::Img(kDeviceRecords.v) + 4, 1);
        std::memcpy(AtDisplaySetup(::bof3::Img(kDeviceRecords.v) + 0x18), "Software Render", 16);
        PutLongDisplaySetup(::bof3::Img(kDeviceRecords.v) + kDeviceRecord + 0, 1);
        PutLongDisplaySetup(::bof3::Img(kDeviceRecords.v) + kDeviceRecord + 4, 1);
        std::memcpy(AtDisplaySetup(::bof3::Img(kDeviceRecords.v) + kDeviceRecord + 0x18), "Direct3D 11 (bof3x)", 20);
        Display_DeviceCountLess1 = 1;
    }
    // 2.5 which device: the original wraps an index past the last record to 0
    // and takes record 0 as the software renderer. Here every index is the
    // hardware path (DIV-0031); the wrap is kept for F7's cycling.
    if (*device < 0 || *device > 1) *device = 0;
    PutLongDisplaySetup(::bof3::Img(kRenderFlagsDisplaySetup.v), 2);   // bit 1: the device allows windowed

    // 2.6 the mode and the scale: the logical picture is the target's size
    const render::Options options = ReadOptions(hwnd, *fullscreen != 0);
    if (!display_setup::g_backend_up) {
        render::InitShim(256 * 1024, 32 * 1024, 8 * 1024 * 1024);
        render::InitD3d11(options);
        render::SetRescaleHook(&Rescale);
        display_setup::g_backend_up = true;
    }
    const U width = render::TargetWidth(), height = render::TargetHeight();
    Gfx_ScreenRect[2] = width;
    Gfx_ScreenRect[3] = height;
    Display_ModeBpp = kModeBits;
    // The game's scale is k, the view's - not the target's width over 320,
    // which a wide target (DIV-0041) would make 1.33k: the game keeps
    // drawing a 320-wide view and the backend shifts it into the target.
    PutFloatDisplaySetup(::bof3::Img(D3d_ScaleX_addr), static_cast<float>(display_setup::g_scale));
    PutFloatDisplaySetup(::bof3::Img(D3d_ScaleY_addr), static_cast<float>(display_setup::g_scale));
    SprtDraw_SetScale(display_setup::g_scale);   // DIV-0010's far edge follows the scale (display-overhaul.md 4b)
    Zero(::bof3::Img(D3d_DeviceDesc_addr), kDeviceDescBytes);
    PutLongDisplaySetup(::bof3::Img(kDeviceDesc.v) + kDescTextureCaps, kTextureCaps);
    PutLongDisplaySetup(::bof3::Img(kDeviceDesc.v) + kDescMinW, kMinTexture);
    PutLongDisplaySetup(::bof3::Img(kDeviceDesc.v) + kDescMinH, kMinTexture);
    PutLongDisplaySetup(::bof3::Img(kDeviceDesc.v) + kDescMaxW, kMaxTexture);
    PutLongDisplaySetup(::bof3::Img(kDeviceDesc.v) + kDescMaxH, kMaxTexture);

    // 2.7 the windowed path, always: DirectDraw, primary, back buffer, no
    // clipper, Gfx_RenderFlags bit 0x200. The window rect stays 0x5A5130's.
    Dd_DirectDraw = render::DirectDrawObject();
    PutPtr(::bof3::Img(kDirect3D.v), render::DirectDrawObject());   // released by the teardown, read by nothing else
    render::Surface* primary = render::MakeSurface(width, height, 32, 0x200, 0, true, false);
    render::Surface* back = render::MakeSurface(width, height, 32, 0x2040, 0, false, true);
    if (!primary || !back) bof3::Fatal("Display_Setup: out of memory for the primary and back surfaces");
    DDraw_Primary = primary;
    DDraw_BackBuffer = back;
    PsxRest_Clipper = 0;
    PsxRest_ZBuffer = 0;
    *reinterpret_cast<U*>(AtDisplaySetup(::bof3::Img(Gfx_RenderFlags_addr))) |= 0x200;
    (void)fullscreen;   // never forced: no display mode is set (DIV-0031)

    // 2.9 the device
    D3d_Device = render::DeviceObject();

    // 2.10 the texture formats, the staging surface, the back buffer's pitch
    FillFormat(::bof3::Img(Gfx_PixelFormat_addr) + 0 * kPixelFormatRecord, kFormatAlpha, kRankAlpha);
    FillFormat(::bof3::Img(Gfx_PixelFormat_addr) + 1 * kPixelFormatRecord, kFormatOpaque, kRankOpaque);
    FillFormat(::bof3::Img(Gfx_PixelFormat_addr) + 2 * kPixelFormatRecord, kFormatScreen, kRankScreen);
    if (kNoAlphaFormat) *reinterpret_cast<U*>(AtDisplaySetup(::bof3::Img(Gfx_RenderFlags_addr))) |= 0x20;
    if (!Dd_CreatePlainSurface(0x140, 0x100, reinterpret_cast<::bof3::Slot32<void>*>(AtDisplaySetup(::bof3::Img(Dd_StageSurface_addr))), 0))
        bof3::Fatal("Display_Setup: the staging surface");
    Display_BackPitch = back->pitch;

    // 2.11 viewport, material, states - through the backend's objects, so that
    // the device holds the same state the original's did
    D3d_Viewport = render::ViewportObject();
    D3d_BackMaterial = render::MaterialObject();
    D3d_BackMaterialHandle = 1;
    render::SetClearColor(0xFF000000u);
    render::PipeState& s = render::CurrentState();
    s.texture = nullptr;
    s.alpha_modulate = true;                 // ALPHAOP MODULATE
    s.point_filter = options.point_filter;   // MIN/MAGFILTER: LINEAR, or POINT under BOF3X_FILTER (DIV-0012)
    s.alpha_test = true;                     // ALPHATESTENABLE 1
    s.alpha_ref = 8;                         // ALPHAREF 8
    s.alpha_func = 5;                        // ALPHAFUNC GREATER
    s.specular = false;                      // SPECULARENABLE 0
    s.color_key = false;                     // COLORKEYENABLE is never set by the game: alpha does it
    s.blend_enable = false;
    s.src_blend = 2;
    s.dst_blend = 1;
    s.flat = false;

    bof3::LogFlush();
    bof3::Log("Display_Setup: %u x %u, scale %u, %s filter, device %d", width, height, display_setup::g_scale,
              options.point_filter ? "point" : "linear", *device);
    return 0;
}

// original 0x5A6380. The seven above, in the original's order (the cell
// textures before the after-draw block), then each DirectX 6 slot released and
// zeroed: the material, the viewport, the device, the IDirect3D3, the clipper,
// the Z-buffer, the stage, the back buffer, the primary. Then DirectDraw: in
// fullscreen (Gfx_RenderFlags bit 0x100, read after every release before it)
// SetCooperativeLevel(hwnd, DDSCL_NORMAL) on the object as first read, then
// RestoreDisplayMode and Release each on the slot read again; Release; zero.
extern "C" void __cdecl Display_Teardown(void* hwnd) {
    psx_rest::g.free_clut_rows();
    psx_rest::g.release_tex_cache();
    psx_rest::g.release_font_cache();
    psx_rest::g.release_orphans();
    psx_rest::g.release_backdrop();
    psx_rest::g.free_cell_textures();
    psx_rest::g.release_after_draw();
    ReleaseIf(::bof3::Img(D3d_BackMaterial_addr), true);
    ReleaseIf(::bof3::Img(D3d_Viewport_addr), true);
    ReleaseIf(::bof3::Img(D3d_Device_addr), true);
    ReleaseIf(::bof3::Img(PsxRest_Direct3D_addr), true);
    ReleaseIf(::bof3::Img(PsxRest_Clipper_addr), true);
    ReleaseIf(::bof3::Img(PsxRest_ZBuffer_addr), true);
    ReleaseIf(::bof3::Img(Dd_StageSurface_addr), true);
    ReleaseIf(::bof3::Img(DDraw_BackBuffer_addr), true);
    ReleaseIf(::bof3::Img(DDraw_Primary_addr), true);
    void* dd = Dd_DirectDraw;
    if (!dd) return;
    if (psx_rest::Long(::bof3::Img(psx_rest::kRenderFlags.v)) & 0x100) {
        psx_rest::Method<ComCoop>(dd, psx_rest::kSetCooperativeLevel)(dd, hwnd, kDdsclNormal);
        dd = Dd_DirectDraw;
        psx_rest::Method<psx_rest::ComCall>(dd, kRestoreDisplayMode)(dd);
        dd = Dd_DirectDraw;
    }
    psx_rest::Method<psx_rest::ComCall>(dd, psx_rest::kRelease)(dd);
    psx_rest::PutLong(::bof3::Img(psx_rest::kDirectDraw.v), 0);
}

// original 0x5A64B0. Every Gfx_ClutRows row buffer (the pointer of each of the
// 512 pairs) to Crt_free, and the pair - the generation counter with it -
// zeroed. A pair whose pointer is null is left as it is.
extern "C" void __cdecl Gfx_FreeClutRows() {
    for (psx_rest::U at = ::bof3::Img(Gfx_ClutRows_addr) + 4; at < ::bof3::Img(kClutRowsEnd) + 4; at += 8) {
        void* const row = psx_rest::Ptr(at);
        if (!row) continue;
        psx_rest::g.free(row);
        psx_rest::PutLong(at - 4, 0);
        psx_rest::PutLong(at, 0);
    }
}

// original 0x5A64E0. Each of Gfx_TexCache's 32 pages from its first entry to
// its first entry whose state byte is 0: the texture at +0x14 released, then
// the surface at +0x10 - read after that Release - and the 0x18 bytes zeroed.
extern "C" void __cdecl Gfx_ReleaseTexCache() {
    for (psx_rest::U page = ::bof3::Img(Gfx_TexCache_addr); page < ::bof3::Img(kTexCacheEnd); page += kTexPageBytes) {
        for (psx_rest::U i = 0; i < 0x20; ++i) {
            const psx_rest::U entry = page + i * kTexEntryBytes;
            if (!psx_rest::At(entry)[0]) break;
            ReleaseIf(entry + 0x14, false);
            ReleaseIf(entry + 0x10, false);
            ZeroDwords(entry, 6);
        }
    }
}

// original 0x5A6540. All 128 of Font_TexCache: the texture +0xC released, then
// the surface +8 (read after), the entry's five dwords zeroed and its glyph
// word +0 set back to 0xFFFF - the set-up's empty value.
extern "C" void __cdecl Font_ReleaseTexCache() {
    for (psx_rest::U entry = ::bof3::Img(Font_TexCache_addr); entry < ::bof3::Img(kFontCacheEnd); entry += psx_rest::kFontEntryBytes) {
        ReleaseIf(entry + 0xC, false);
        ReleaseIf(entry + 8, false);
        ZeroDwords(entry, 5);
        psx_rest::PutWord(entry, 0xFFFF);
    }
}

// original 0x5A6590. Six COM pointers at 0x7CAE20..0x7CAE37 that no other
// instruction of the image names: +0x10 then +8, +0x14 then +0xC, +4, +0 -
// each released and zeroed when it is not null (docs/psx-rest.md section 2).
extern "C" void __cdecl Display_ReleaseOrphans() {
    for (psx_rest::U at = ::bof3::Img(PsxRest_Orphans_addr) + 8; at < ::bof3::Img(PsxRest_Orphans_addr) + 0x10; at += 4) {
        ReleaseIf(at + 8, true);
        ReleaseIf(at, true);
    }
    ReleaseIf(::bof3::Img(PsxRest_Orphans_addr) + 4, true);
    ReleaseIf(::bof3::Img(PsxRest_Orphans_addr), true);
}

// original 0x5A65F0. The backdrop block: 0x6BEA0C then 0x6BEA04, 0x6BEA10 then
// 0x6BEA08, then Dd_CellBackdrop 0x6BEA00 released (none zeroed by itself);
// then the seven dwords from 0x6BE9F8 zeroed, and 0x6BE9FC = 0x12345678,
// which nothing reads.
extern "C" void __cdecl Display_ReleaseBackdrop() {
    for (psx_rest::U at = ::bof3::Img(kBackdropPairs); at < ::bof3::Img(kBackdropPairs) + 8; at += 4) {
        ReleaseIf(at + 8, false);
        ReleaseIf(at, false);
    }
    ReleaseIf(::bof3::Img(Dd_CellBackdrop_addr), false);
    ZeroDwords(::bof3::Img(kBackdropBlock.v), 7);
    PsxRest_BackdropMagic = kMagic;
}

// original 0x5A6650. D3d_AfterDraw's three surfaces - the texture, its
// surface, the capture surface - released, then its seven dwords zeroed.
extern "C" void __cdecl D3d_ReleaseAfterDraw() {
    ReleaseIf(::bof3::Img(D3d_CaptureTexture_addr), false);
    ReleaseIf(::bof3::Img(D3d_CaptureTexSurface_addr), false);
    ReleaseIf(::bof3::Img(D3d_CaptureSurface_addr), false);
    ZeroDwords(::bof3::Img(D3d_CaptureReady_addr), 7);
}

extern "C" char* __cdecl Display_DeviceName(int device) {
    return reinterpret_cast<char*>(static_cast<std::uintptr_t>(::bof3::Img(0x6C3A68) + static_cast<U>(device) * 0x13C));
}

// original 0x5A66B0. GDI text on the back buffer: GetDC on DDraw_BackBuffer as
// read at entry, and nothing more if that fails; else SetTextColor white,
// SetBkMode TRANSPARENT, the text's length counted after them, TextOutA at
// (x, y), and ReleaseDC on DDraw_BackBuffer read again. Nothing reads the
// HRESULT left in eax (the one caller, WinMain's overlay, is ours).
extern "C" void __cdecl Display_TextOut(int x, int y, const char* text) {
    void* const surface = DDraw_BackBuffer;
    void* hdc = nullptr;   // the original's is the slot `push ecx` made: GetDC writes it (docs/psx-rest.md section 3)
    if (psx_rest::Method<ComGetDC>(surface, kGetDC)(surface, &hdc)) return;
    psx_rest::g.set_text_color(hdc, kWhite);
    psx_rest::g.set_bk_mode(hdc, kTransparent);
    const int n = static_cast<int>(std::strlen(text));
    psx_rest::g.text_out(hdc, x, y, text, n);
    void* const again = DDraw_BackBuffer;
    psx_rest::Method<ComReleaseDC>(again, kReleaseDC)(again, hdc);
}

// original 0x5A6720. MessageBoxA(NULL, text, the image's caption, MB_ICONHAND)
// with the text from the error table: a code below 100 (signed) indexes
// 0x66B6A8, any other 0x66B568 - which puts 100..103 at the four entries
// after the first table's twenty. Nothing checks the code; nothing reads the
// box's answer.
extern "C" void __cdecl Display_ErrorBox(int code) {
    const psx_rest::U base = code < 100 ? ::bof3::Img(kErrorText.v) : ::bof3::Img(kErrorTextHigh.v);
    const char* const text = reinterpret_cast<const char*>(psx_rest::Ptr(base + static_cast<psx_rest::U>(code) * 4));
    psx_rest::g.message_box(nullptr, text, reinterpret_cast<const char*>(psx_rest::At(::bof3::Img(kErrorCaption.v))), kIconHand);
}

// original 0x5A6760. D3d_FreeCellTexture of every D3d_CellTexCache slot,
// 0 to 127.
extern "C" void __cdecl D3d_FreeCellTextures() {
    for (psx_rest::U slot = 0; slot < kCellSlots; ++slot) psx_rest::g.free_cell_texture(static_cast<int>(slot));
}

// original 0x5A6780: the cell table is empty again. Once a logic frame.
extern "C" void __cdecl SpriteCell_Reset() { SpriteCell_Count = 0; }

// original 0x5A6790: one cell record, at SpriteCell_Table[SpriteCell_Count],
// and its index. Byte +3 packs the size as ((h & 0xF8) << 1 | w >> 3) in a
// byte - so a height of 128 or more loses its top bit, as the original has it.
// No bound on the count, as the original has it; SpriteCell_Reset is the only
// thing that lowers it.
extern "C" unsigned long __cdecl SpriteCell_Add(unsigned x, unsigned y, unsigned flags, unsigned u, unsigned v,
                                                unsigned w, unsigned h) {
    const unsigned long index = SpriteCell_Count;
    unsigned char* cell = SpriteCell_Table + index * kCellBytes;
    Put<std::uint16_t>(cell, 0, static_cast<std::uint16_t>(x));
    cell[2] = static_cast<unsigned char>(y);
    cell[3] = static_cast<unsigned char>(((h & 0xF8) << 1) | ((w & 0xFF) >> 3));
    Put<std::uint16_t>(cell, 4, static_cast<std::uint16_t>(flags));
    cell[6] = static_cast<unsigned char>(u);
    cell[7] = static_cast<unsigned char>(v);
    SpriteCell_Count = index + 1;
    return index;
}

// original 0x5A6800. Takes ownership of a malloc'd glyph table - the port's
// Chinese font, 288 bytes a glyph - and frees the one it replaces. The size is
// passed by LoadDatFile and never read.
extern "C" void __cdecl Font_SetGlyphData(void* owned_glyphs, unsigned size) {
    if (Font_GlyphData != nullptr) Crt_free(Font_GlyphData);
    Font_GlyphData = owned_glyphs;
    // DIV-0016: the original never reads the size. We do, and only to keep the
    // string draw's glyph-index guard in step with the table it now holds.
    TextDraw_SetGlyphCount(size / 0x120);
}

// original 0x5A6830: the sound set-up, Game_Init's (hwnd Game_Hwnd). The
// DirectSound device, made once - nothing if Snd_Device is already set - in
// exclusive cooperation with the window, and its primary buffer set to
// 22,050 Hz 8-bit stereo PCM. Nothing more: the voices' buffers come with
// each DAT's bank (Snd_LoadBank), the music's stream and the MP3 decoder with
// each track (Music_Start), the SND stream with each stream. 0, or 100..103
// for the step that failed, with what that step had made let go and cleared;
// Game_Init does not read it, and a game without a device plays silent
// (SndBuf_FromWave and Music_Start do nothing without one).
// As the original has it: any non-zero result is a failure (S_FALSE too);
// DirectSoundCreate writes Snd_Device directly, and Snd_Device and Snd_Primary
// are read again after every call; on the format's failure the primary is
// read after the device's Release and before Snd_Device is cleared. The
// format's words are stored around the dwords as the original orders them.
extern "C" int __cdecl Snd_Init(void* hwnd) {
    if (Snd_Device != nullptr) return 0;
    // The slot as the out parameter DirectSoundCreate writes (void**): the
    // 32-bit spelling, since Snd_Device is a 4-byte Slot32<void> and the
    // callee is Windows'. B5's sound layer (docs/x64-build-design.md 6.3)
    // replaces this call on both widths; until then it is 32-bit only.
    if (SoundRest_g.create(nullptr, reinterpret_cast<void**>(&Snd_Device), nullptr) != 0) {
        Snd_Device = nullptr;
        return kNoDevice;
    }
    void* device = Snd_Device;
    if (sound::Method<ComCooperate>(device, sound_rest::kSetCooperativeLevel)(device, hwnd, kExclusive) != 0) {
        Release(Snd_Device);
        Snd_Device = nullptr;
        return kNoCooperation;
    }
    const std::uint32_t desc = Address(Snd_BufferDesc);
    std::memset(sound_rest::At(desc), 0, 0x24);
    SetDword(desc + 0x00, 0x24);             // dwSize
    SetDword(desc + 0x04, kPrimaryBuffer);   // dwFlags
    SetDword(desc + 0x08, 0);                // dwBufferBytes
    SetDword(desc + 0x0C, 0);                // dwReserved
    SetDword(desc + 0x10, 0);                // lpwfxFormat
    device = Snd_Device;
    if (sound::Method<ComCreate>(device, sound::kCreateSoundBuffer)(device, sound_rest::At(desc), reinterpret_cast<void**>(&Snd_Primary), nullptr) != 0) {
        Release(Snd_Device);
        Snd_Device = nullptr;
        return kNoPrimary;
    }
    const std::uint32_t format = Address(Snd_WaveFormat);
    SetWord(format + 0x00, 1);        // wFormatTag, PCM
    SetWord(format + 0x02, 2);        // nChannels
    SetWord(format + 0x0C, 2);        // nBlockAlign
    SetDword(format + 0x04, 22050);   // nSamplesPerSec
    SetDword(format + 0x08, 44100);   // nAvgBytesPerSec
    SetWord(format + 0x0E, 8);        // wBitsPerSample
    SetWord(format + 0x10, 0);        // cbSize
    void* const primary = Snd_Primary;
    if (sound::Method<ComFormat>(primary, kSetFormat)(primary, sound_rest::At(format)) != 0) {
        Release(Snd_Device);
        void* const made = Snd_Primary;
        Snd_Device = nullptr;
        Release(made);
        Snd_Primary = nullptr;
        return kNoFormat;
    }
    return 0;
}

void D3d_Inject() {
    BOF3_INJECT(Gfx_ClearImage);
    BOF3_INJECT(Gfx_InvalidateTextures);
    BOF3_INJECT(Gfx_StoreImage);
    BOF3_INJECT(Gfx_MoveImage);
    BOF3_INJECT(Gfx_LoadImage);
    BOF3_INJECT(Gfx_LoadImageIfChanged);
    BOF3_INJECT(Gfx_ConvertRow);
    BOF3_INJECT(Gfx_ClearPresent);
    BOF3_INJECT(Gfx_Present);
    BOF3_INJECT(D3d_SetAlphaModulate);
    BOF3_INJECT(D3d_AfterDraw);
    BOF3_INJECT(Dd_InitSurfaceDesc);
    BOF3_INJECT(Dd_CreatePlainSurface);
    BOF3_INJECT(Dd_CreateTextureSurface);
    BOF3_INJECT(D3d_FitTextureSize);
    BOF3_INJECT(D3d_DrawPolyF3);
    BOF3_INJECT(D3d_PrimColor);
    BOF3_INJECT(D3d_SetBlend);
    BOF3_INJECT(D3d_SetShadeMode);
    BOF3_INJECT(D3d_DrawPolyFT3);
    BOF3_INJECT(D3d_BindTexture);
    BOF3_INJECT(D3d_BuildPageTexture);
    BOF3_INJECT(Gfx_ClutPixels);
    BOF3_INJECT(D3d_RefreshPageTexture);
    BOF3_INJECT(Gfx_TexCacheFind);
    BOF3_INJECT(D3d_FlattenFT3);
    BOF3_INJECT(D3d_PageTexel4);
    BOF3_INJECT(D3d_DrawPolyF4);
    BOF3_INJECT(D3d_DrawPolyFT4);
    BOF3_INJECT(D3d_DrawPolyG3);
    BOF3_INJECT(D3d_DrawPolyGT3);
    BOF3_INJECT(D3d_DrawPolyG4);
    BOF3_INJECT(D3d_DrawPolyGT4);
    BOF3_INJECT(D3d_DrawLineF2);
    BOF3_INJECT(D3d_DrawLineG2);
    BOF3_INJECT(D3d_DrawLineF3);
    BOF3_INJECT(D3d_DrawLineG3);
    BOF3_INJECT(D3d_DrawLineF4);
    BOF3_INJECT(D3d_DrawLineG4);
    BOF3_INJECT(D3d_DrawTile);
    BOF3_INJECT(D3d_DrawTile1);
    BOF3_INJECT(D3d_DrawSprt);
    BOF3_INJECT(D3d_DrawSprt8);
    BOF3_INJECT(D3d_DrawSprt16);
    BOF3_INJECT(D3d_DrawGlyph);
    BOF3_INJECT(Font_GlyphTexture);
    BOF3_INJECT(Font_BuildGlyphTexture);
    BOF3_INJECT(Dd_ClearSurface);
    BOF3_INJECT(D3d_DrawCellSprite);
    BOF3_INJECT(D3d_CellTexture);
    BOF3_INJECT(D3d_BuildCellTexture);
    BOF3_INJECT(D3d_FreeCellTexture);
    BOF3_INJECT(D3d_RefreshCellTexture);
    BOF3_INJECT(D3d_SetBackColor);
    BOF3_INJECT(Display_WindowMoved);
    BOF3_INJECT(Display_Setup);
    BOF3_INJECT(Display_Teardown);
    BOF3_INJECT(Gfx_FreeClutRows);
    BOF3_INJECT(Gfx_ReleaseTexCache);
    BOF3_INJECT(Font_ReleaseTexCache);
    BOF3_INJECT(Display_ReleaseOrphans);
    BOF3_INJECT(Display_ReleaseBackdrop);
    BOF3_INJECT(D3d_ReleaseAfterDraw);
    BOF3_INJECT(Display_DeviceName);
    BOF3_INJECT(Display_TextOut);
    BOF3_INJECT(Display_ErrorBox);
    BOF3_INJECT(D3d_FreeCellTextures);
    BOF3_INJECT(SpriteCell_Reset);
    BOF3_INJECT(SpriteCell_Add);
    BOF3_INJECT(Font_SetGlyphData);
    BOF3_INJECT(Snd_Init);
}

void D3dFromD3dRest_Inject() {
    // DIVERGENCE DIV-0077: after the self-test, which compares Capcom's point.
    {
        char text[16];
        const DWORD n = GetEnvironmentVariableA("BOF3X_TILE1", text, sizeof text);
        const bool off = n == 1 && text[0] == '0';
        if (n > 1 || (n == 1 && text[0] != '0' && text[0] != '1')) bof3::Fatal("BOF3X_TILE1 must be 0 or 1");
        if (!off) {
            static const std::uint8_t was = 0, is = 1;
            bof3::PatchBytes("Tile1Quad", Low32(&g_tile1_quad),
                             &was, &is, 1);
            bof3::Log("DIV-0077    TILE_1 drawn as a scale-sized quad, the PlayStation pixel's footprint (BOF3X_TILE1=0 for the point)");
        }
    }
}
