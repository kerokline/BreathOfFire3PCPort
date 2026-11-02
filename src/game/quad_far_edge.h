// DIVERGENCE DIV-0199 (known-defects.md D198): a screen-rectangle textured
// quad samples the texels the PlayStation draws for it and never the one past
// its far edge. Pure arithmetic, no game state: D3d_DrawPolyFT4 /
// D3d_DrawPolyGT4 (d3d_draw.cpp) call it with the vertices they have just
// written, and tools/probes/quad_edge_probe.cpp renders random quads through
// Direct3D 11 with it and without it.
//
// The PlayStation rasteriser never draws a polygon's right column or bottom
// row: game pixel g of a side W pixels long takes texel floor(t0 + (t1 - t0)
// g / W), so a builder's far coordinate (u + w) is never sampled. The port
// takes every corner's coordinate from D3d_TexCoords, tc[i] = (i + 0.512) /
// 256 - the centre of texel i - so the far corner sits half a texel past the
// last texel, and above scale 1 the last screen pixels sample the texel after
// it: the stray line under the ENGINE gauge (#179), the fishing menu's frame
// lines, the crane button's border. The near edge is half a texel late too,
// so at scale 2 and up every texel boundary inside the quad is half a game
// pixel off the PlayStation's.
//
// Ours, for a quad that is a rectangle on screen along the axis a coordinate
// varies on (every 2D quad the menus build), per coordinate, in texel units,
// n the screen pixels between the near side (smaller x or y) and the far one,
// and with pixel centres on the integers (Direct3D 6, which the backend keeps:
// the near vertex's value is the first pixel's sample):
//
// - point filtering: the PlayStation's mapping exactly - pixel i samples
//   s + (e - s) (i + 0.5) / n, where [s, e) is the run of texels the
//   PlayStation walks: [t0, t1) running up, [t1 + 1, t0 + 1) running down (a
//   mirrored quad). Every screen pixel inside game pixel g lands on the texel
//   the PlayStation draws at g whenever the quad is drawn 1:1 or by a whole
//   factor; a stretched one changes texel inside a game pixel, never past
//   the run.
// - bilinear filtering: from the centre of the first texel the PlayStation
//   draws to the centre of the last, first pixel to last pixel - DIV-0010's
//   rule for sprites (sprt_draw.cpp) with the quad's own n, so a stretched
//   quad (the gauge: 4 texels over 16 game pixels) never blends in the texel
//   past its run either.
//
// Any other quad - a model's polygon, a rotated or sheared one - keeps the
// original's coordinates: the port's uniform +0.512 keeps neighbouring
// polygons that share an edge continuous, and moving one far corner without
// its neighbour's near one would open a half-texel seam along every shared
// edge of a mesh. A coordinate the same at all four corners is left alone.
#pragma once

#include <cmath>

namespace quad_far_edge {

// One coordinate (u or v) of the four corners: t the primitive's bytes, o the
// vertex values the original wrote (tc[t], in 1/256), rewritten in place for a
// screen rectangle. x, y the corners' screen positions.
inline void ApplyAxis(const float x[4], const float y[4], const unsigned char t[4], float o[4], bool point) {
    if (t[0] == t[1] && t[0] == t[2] && t[0] == t[3]) return;
    for (int d = 0; d < 2; ++d) {
        const float* const p = d == 0 ? x : y;
        float lo = p[0], hi = p[0];
        for (int i = 1; i < 4; ++i) {
            lo = std::fmin(lo, p[i]);
            hi = std::fmax(hi, p[i]);
        }
        if (!(hi - lo >= 1.0f)) continue;
        int near_n = 0, far_n = 0, near_i = -1, far_i = -1;
        bool along = true;   // the coordinate the same on each side
        for (int i = 0; i < 4; ++i) {
            if (p[i] == lo) {
                if (near_i >= 0 && t[i] != t[near_i]) along = false;
                near_i = i;
                ++near_n;
            } else if (p[i] == hi) {
                if (far_i >= 0 && t[i] != t[far_i]) along = false;
                far_i = i;
                ++far_n;
            }
        }
        if (near_n != 2 || far_n != 2) return;           // not a screen rectangle: the original's
        if (!along || t[near_i] == t[far_i]) continue;   // it does not run along this axis: the other one
        const double t0 = t[near_i], t1 = t[far_i];
        const double n = static_cast<double>(hi) - lo;
        const bool up = t1 > t0;
        double a, b;
        if (point) {
            const double s = up ? t0 : t0 + 1.0, e = up ? t1 : t1 + 1.0;
            a = s + (e - s) * 0.5 / n;
            b = a + (e - s);
        } else {
            const double first = t0, last = up ? t1 - 1.0 : t1 + 1.0;
            a = first + 0.5;
            b = a + (last - first) * (n >= 2.0 ? n / (n - 1.0) : 1.0);
        }
        for (int i = 0; i < 4; ++i) o[i] = static_cast<float>((p[i] == lo ? a : b) / 256.0);
        return;
    }
}

}  // namespace quad_far_edge
