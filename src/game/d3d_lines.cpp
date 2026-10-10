// DIV-0079: LINE primitives as quads of the scale's width. See d3d_lines.h.
//
// The PlayStation drew a line from pixel a to pixel b one pixel wide, both
// ends included: on the x axis it covers [a, b + 1) in 320 x 240 pixels. The
// port's handlers scale each corner to (x * ScaleX, y * ScaleY) - the top-left
// of the scaled pixel, as D3d_DrawTile's corners are - and hand Direct3D a
// LINESTRIP, which is one screen pixel wide whatever the scale. Ours moves
// each end to its pixel's centre, extends the segment half a pixel at both
// ends (the square cap) and half a pixel to each side, each axis at its own
// scale, so an axis-aligned line covers exactly the pixels the PlayStation's
// did and a diagonal is a smooth band of the same width. A zero-length line
// is one pixel, as the PlayStation's was.
#include "game/d3d_lines.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace d3d_lines {

unsigned char g_wide = 0;

namespace {

using U = std::uint32_t;
// symbols.gen.h also defines each data name as a typed macro, which would
// expand inside bof3::addr:: - the constants are read with the macros set
// aside (battle_flow.cpp's way). The values are the names' (rule 3).
#pragma push_macro("D3d_ScaleX")
#undef D3d_ScaleX
#pragma push_macro("D3d_ScaleY")
#undef D3d_ScaleY
#pragma push_macro("D3d_Vertices")
#undef D3d_Vertices
#pragma push_macro("D3d_Device")
#undef D3d_Device
constexpr U kScaleX = bof3::addr::D3d_ScaleX;       // float
constexpr U kScaleY = bof3::addr::D3d_ScaleY;       // float
constexpr U kVertices = bof3::addr::D3d_Vertices;   // 4 x D3DTLVERTEX of 0x20
constexpr U kDevice = bof3::addr::D3d_Device;       // an IDirect3DDevice3 *
#pragma pop_macro("D3d_Device")
#pragma pop_macro("D3d_Vertices")
#pragma pop_macro("D3d_ScaleY")
#pragma pop_macro("D3d_ScaleX")

unsigned char* At(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
float FloatAt(const unsigned char* p) {
    float v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutFloat(unsigned char* p, float v) { std::memcpy(p, &v, sizeof v); }

using Com6 = long(__stdcall*)(void*, U, U, U, U, U);
// DrawPrimitive(TRIANGLESTRIP, D3DFVF_TLVERTEX, D3d_Vertices, 4, 0), the device
// read afresh as the handlers read it.
long DrawStrip4() {
    void** device = *reinterpret_cast<void** volatile*>(At(kDevice));
    void* method = (*reinterpret_cast<void***>(device))[0x70 / 4];
    return reinterpret_cast<Com6>(method)(device, 5, 0x1C4, kVertices, 4, 0);
}

struct End {
    float x, y;
    unsigned char rest[0x18];   // z, rhw, diffuse, specular, tu, tv as the handler left them
};

}  // namespace

long DrawWide(unsigned corners) {
    if (corners < 2 || corners > 4) bof3::Fatal("d3d_lines::DrawWide: %u corners (2..4)", corners);
    const float sx = FloatAt(At(kScaleX)), sy = FloatAt(At(kScaleY));
    const float hx = sx * 0.5f, hy = sy * 0.5f;
    End ends[4];
    for (unsigned i = 0; i < corners; ++i) {
        const unsigned char* v = At(kVertices + i * 0x20);
        ends[i].x = FloatAt(v) + hx;   // the pixel's centre
        ends[i].y = FloatAt(v + 4) + hy;
        std::memcpy(ends[i].rest, v + 8, sizeof ends[i].rest);
    }
    long result = 0;
    for (unsigned i = 0; i + 1 < corners; ++i) {
        const End& a = ends[i];
        const End& b = ends[i + 1];
        float dx = b.x - a.x, dy = b.y - a.y;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len > 0.0f) {
            dx /= len;
            dy /= len;
        } else {
            dx = 1.0f;
            dy = 0.0f;
        }
        const float ex = dx * hx, ey = dy * hy;    // half a pixel along the line
        const float wx = -dy * hx, wy = dx * hy;   // half a pixel across it
        const float xs[4] = {a.x - ex + wx, a.x - ex - wx, b.x + ex + wx, b.x + ex - wx};
        const float ys[4] = {a.y - ey + wy, a.y - ey - wy, b.y + ey + wy, b.y + ey - wy};
        for (unsigned k = 0; k < 4; ++k) {
            unsigned char* out = At(kVertices + k * 0x20);
            PutFloat(out, xs[k]);
            PutFloat(out + 4, ys[k]);
            std::memcpy(out + 8, (k < 2 ? a : b).rest, sizeof a.rest);
        }
        result = DrawStrip4();
    }
    return result;
}

void Arm() {
    char text[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_LINES", text, sizeof text);
    if (n > 1 || (n == 1 && text[0] != '0' && text[0] != '1')) bof3::Fatal("BOF3X_LINES must be 0 or 1");
    if (n == 1 && text[0] == '0') {
        bof3::Log("DIV-0079    off (BOF3X_LINES=0): LINE primitives one screen pixel wide, the original's strip");
        return;
    }
    static const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("LinesWide", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_wide)), &was, &is, 1);
    bof3::Log("DIV-0079    LINE primitives drawn as quads of the scale's width (BOF3X_LINES=0 for the strip)");
}

}  // namespace d3d_lines
