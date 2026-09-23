// The glyph handler and its texture lookup, read to the last instruction with
// capstone against bof3/BOF3.exe (docs/glyph-draw.md sections 1-3). Both are
// the PC port's own: the PlayStation drew these as GPU primitives, so there is
// no PSX twin to read them against.
//
//   D3d_DrawGlyph      0x5A2900..0x5A2BB3 (0x2B4 bytes), straight-line
//   Font_GlyphTexture  0x5A2BC0..0x5A2C96 (0xD7 bytes)
//
// DIVERGENCE DIV-0025 lives in D3d_DrawGlyph: the texture coordinates sample
// texel centres, (2u + 0.5) / 32, where Capcom's sample texel edges, 2u / 32
// (docs/known-defects.md D17). The offset is g_texel_inset, 0 until
// GlyphDraw_Inject sets it through PatchBytes under the name
// "GlyphTexelCentres" - so BOF3X_ORIGINAL=GlyphTexelCentres runs our function
// with Capcom's arithmetic, and BOF3X_ORIGINAL=D3d_DrawGlyph runs Capcom's.
#include "game/glyph_draw.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/glyph_draw_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace glyph_draw {

const Callees kOriginals = {
    D3d_PrimColor,
    Font_GlyphTexture,
    reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(kRetOnly)),
    D3d_SetBlend,
    D3d_SetShadeMode,
    Font_BuildGlyphTexture,
};
Callees g = kOriginals;

float g_texel_inset = 0.0f;

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
float GetFloat(U address) {
    float v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

// The device, read afresh before every COM call as the original does.
void** Device() { return *reinterpret_cast<void** volatile*>(At(0x7CC350)); }
void* Method(void** device, U offset) { return (*reinterpret_cast<void***>(device))[offset / 4]; }

using Com1 = long(__stdcall*)(void*);
using Com3 = long(__stdcall*)(void*, U, U);
using Com6 = long(__stdcall*)(void*, U, U, U, U, U);

constexpr U kVertices = 0x7CA958;     // D3d_Vertices: 4 x D3DTLVERTEX
constexpr U kScaleX = 0x7C9F4C, kScaleY = 0x7C9F48;
constexpr U kSz = 0x3F7D70A4;         // 0.99f, as the original stores it: an immediate
constexpr U kRhw = 0x3DCCCCCD;        // 0.1f

}  // namespace
}  // namespace glyph_draw

// 0x5A2900. The primitive is POLY_FT4-shaped (Text_EmitGlyph 0x516D50 and
// four other builders of code 0x6C): r, g, b, code at +4..+7; corner i's
// s16 x, y at +8 + 8i / +0xA + 8i and its u, v bytes at +0xC + 8i / +0xD + 8i;
// the CLUT id at +0xE and the glyph index at +0x16.
long D3d_DrawGlyph(const unsigned char* prim) {
    using namespace glyph_draw;
    // Two locals the colour helper fills; it always writes both (it is handed
    // a non-null specular pointer).
    unsigned long diffuse, specular;
    g.prim_color(prim[4], prim[5], prim[6], prim[7], 0, &diffuse, &specular);

    // The four corners: sx, sy = the primitive's x, y times the scale, no
    // half-pixel offset; sz 0.99 and rhw 0.1 as immediates; the colour pair.
    // Each product is exact before it is rounded to float (a 16-bit integer
    // times a float), so x87 and SSE agree whatever the precision control.
    unsigned char* v = At(kVertices);
    for (U i = 0; i < 4; ++i) {
        unsigned char* out = v + i * 0x20;
        const unsigned char* in = prim + i * 8;
        PutFloat(out + 0x00, static_cast<float>(Short(in + 8)) * GetFloat(kScaleX));
        PutFloat(out + 0x04, static_cast<float>(Short(in + 0xA)) * GetFloat(kScaleY));
        PutLong(out + 0x08, kSz);
        PutLong(out + 0x0C, kRhw);
        PutLong(out + 0x10, static_cast<U>(diffuse));
        PutLong(out + 0x14, static_cast<U>(specular));
    }
    // The texture coordinates: the byte u, v doubled (the glyph texture holds
    // each PSX texel as 2 x 2) times the double 1/32 at 0x5C4618 - the 24 x 24
    // glyph in a 32 x 32 surface. Capcom's add nothing to 2u: every screen
    // pixel then samples exactly on the edge between two texels (D17).
    // DIV-0025 adds g_texel_inset, 0.5 - the texel's centre - on both the near
    // and the far edge, so the 1:1 scale is kept. All values exact in float.
    const double inv32 = *reinterpret_cast<const double*>(At(kInv32));
    const double inset = g_texel_inset;
    for (U i = 0; i < 4; ++i) {
        unsigned char* out = v + i * 0x20;
        const unsigned char* in = prim + i * 8;
        PutFloat(out + 0x18, static_cast<float>((2.0 * in[0xC] + inset) * inv32));
        PutFloat(out + 0x1C, static_cast<float>((2.0 * in[0xD] + inset) * inv32));
    }

    // The glyph's texture (its return is not read), then the state: two calls
    // of a bare ret, the blend for the primitive's code - re-read from the
    // primitive - and flat shading. The blend's alpha enable is then turned
    // off again here, unconditionally: glyphs draw unblended.
    g.glyph_texture(Word(prim + 0x16), Word(prim + 0xE));
    g.ret_only(1);
    g.ret_only(1);
    g.set_blend(prim[7], 0);
    g.set_shade(1);
    void** device = Device();
    reinterpret_cast<Com3>(Method(device, 0x58))(device, 0x1B, 0);   // SetRenderState(ALPHABLENDENABLE, FALSE)
    device = Device();
    // DrawPrimitive(D3DPT_TRIANGLESTRIP, D3DFVF_TLVERTEX, vertices, 4, 0); its
    // result is the handler's (the caller 0x59F1B4 does not read it).
    return reinterpret_cast<Com6>(Method(device, 0x70))(device, 5, 0x1C4, kVertices, 4, 0);
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
    U i;
    for (i = 0; i < kEntries; ++i) {
        const unsigned char* e = At(kCache + i * kEntry);
        if (Word(e) == glyph && Word(e + 2) == clut && Long(e + 4) == Long(At(kClutRows + (clut >> 6) * 8))) break;
        if (free_slot == -1 && Word(e + 0x10) == 0) free_slot = static_cast<int>(i);
    }
    if (i == kEntries && free_slot != -1) {
        i = static_cast<U>(free_slot);
        if (At(kRenderFlags)[0] & 1) {
            g.build_texture(free_slot, glyph, clut);
        } else {
            // A texture is built outside the scene.
            void** device = Device();
            reinterpret_cast<Com1>(Method(device, 0x28))(device);   // EndScene
            g.build_texture(free_slot, glyph, clut);
            device = Device();
            reinterpret_cast<Com1>(Method(device, 0x24))(device);   // BeginScene
        }
    }
    // Kept, and D-N1 in docs/known-defects.md: with all 128 entries used this
    // frame and none a hit, i is 128 here - one entry past the table, which is
    // the vertex block. SetTexture is handed the bits of vertex 0's sy
    // (0x7CA95C) as a texture, and the in-use word lands on the low half of
    // vertex 0's sz (0x7CA960). The addresses below reach exactly those bytes.
    if (!(At(kRenderFlags)[0] & 1)) {
        void** device = Device();
        reinterpret_cast<Com3>(Method(device, 0x98))(device, 0, Long(At(kCache + i * kEntry + 0xC)));   // SetTexture(0, tex)
    }
    At(kCache + i * kEntry + 0x10)[0] = 1;
    At(kCache + i * kEntry + 0x11)[0] = 0;
    return static_cast<int>(i);
}

void GlyphDraw_Inject() {
    if (bof3::WantsShadow("glyph_draw")) glyph_draw::SelfTest();
    BOF3_INJECT(D3d_DrawGlyph);
    BOF3_INJECT(Font_GlyphTexture);

    // DIVERGENCE DIV-0025: glyphs sample texel centres. Patched into our own
    // constant so that it has a name of its own for BOF3X_ORIGINAL.
    static const float was = 0.0f, is = 0.5f;
    std::uint8_t was_bytes[4], is_bytes[4];
    std::memcpy(was_bytes, &was, 4);
    std::memcpy(is_bytes, &is, 4);
    bof3::PatchBytes("GlyphTexelCentres",
                     static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&glyph_draw::g_texel_inset)),
                     was_bytes, is_bytes, 4);
    bof3::Log("DIV-0025    glyph texels: (2u + %.1f) / 32", static_cast<double>(glyph_draw::g_texel_inset));
}
