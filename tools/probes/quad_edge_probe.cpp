// A probe for DIV-0199 (known-defects.md D198): random textured quads drawn
// through Direct3D 11 (WARP) the way the backend draws them - pre-transformed
// vertices with Direct3D 6's integer pixel centres (the backend's 0.5 offset),
// a 256 x 256 page, the point or linear sampler with wrap - once with the
// port's texture coordinates (tc[u] = (u + 0.512) / 256 at every corner) and
// once with game/quad_far_edge.h's far edge, the function the game calls.
//
// Each quad's page holds the run of texels the PlayStation walks for it (its
// rasteriser: game pixel i takes floor(t0 + (t1 - t0) i / W), the right
// column and bottom row never drawn, so [t0, t1) running up and
// [t1 + 1, t0 + 1) running down) as art - red 0, green and blue the
// texel's u and v index - and pure red everywhere else. So:
//   bleed     a pixel with any red in it sampled outside the art (any filter);
//   mismatch  (point only, rectangles) a pixel whose texel is not the one the
//             PlayStation draws at that game pixel.
// Kinds: 1:1 (u + w over w pixels), stretched / shrunk, inclusive builder
// (u + w - 1 over w), mirrored (u + w - 1 down to u - 1), and skewed quads
// (no rectangle: the table fallback; bleed only, the art the corners' box less
// its far row and column).
//
// Build and run (from the repository root):
//   i686-w64-mingw32-clang++ -O2 -std=c++20 -Isrc -o quad_edge_probe.exe tools/probes/quad_edge_probe.cpp -ld3d11 -ld3dcompiler_47
//   ./quad_edge_probe.exe <out_dir> [quads per kind]
// It prints one line per scale, filter and kind, and writes <out_dir>/sheet_k<k>_<filter>.ppm:
// rows of quads, each as PlayStation | port | ours, at the scale.
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#include "game/quad_far_edge.h"

namespace {

ID3D11Device* dev;
ID3D11DeviceContext* ctx;
ID3D11Texture2D *page, *target, *staging;
ID3D11ShaderResourceView* page_srv;
ID3D11RenderTargetView* target_rtv;
ID3D11SamplerState *smp_point, *smp_linear;
ID3D11Buffer *vb, *cb;
constexpr int kTarget = 320;

void Check(HRESULT hr, const char* what) {
    if (FAILED(hr)) {
        std::fprintf(stderr, "%s: 0x%08lX\n", what, static_cast<unsigned long>(hr));
        std::exit(1);
    }
}

const char kShader[] = R"(
cbuffer C : register(b0) { float2 target_size; float2 pad; };
Texture2D tex : register(t0);
SamplerState smp : register(s0);
struct VSIn { float4 pos : POSITION; float2 uv : TEXCOORD0; };
struct PSIn { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
PSIn VS(VSIn i) {
    PSIn o;
    float w = i.pos.w > 0.0 ? 1.0 / i.pos.w : 1.0;
    float2 ndc = float2((i.pos.x + 0.5) / target_size.x * 2.0 - 1.0, 1.0 - (i.pos.y + 0.5) / target_size.y * 2.0);
    o.pos = float4(ndc * w, saturate(i.pos.z) * w, w);
    o.uv = i.uv;
    return o;
}
float4 PS(PSIn i) : SV_Target { return tex.Sample(smp, i.uv); }
)";

struct Vtx {
    float x, y, z, rhw, u, v;
};

void Setup() {
    D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    Check(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev, nullptr, &ctx),
          "D3D11CreateDevice (WARP)");
    ID3DBlob *vs_blob, *ps_blob, *err = nullptr;
    Check(D3DCompile(kShader, sizeof kShader - 1, "probe", nullptr, nullptr, "VS", "vs_4_0", 0, 0, &vs_blob, &err), "VS");
    Check(D3DCompile(kShader, sizeof kShader - 1, "probe", nullptr, nullptr, "PS", "ps_4_0", 0, 0, &ps_blob, &err), "PS");
    ID3D11VertexShader* vs;
    ID3D11PixelShader* ps;
    Check(dev->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), nullptr, &vs), "vs");
    Check(dev->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(), nullptr, &ps), "ps");
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    ID3D11InputLayout* il;
    Check(dev->CreateInputLayout(layout, 2, vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(), &il), "layout");

    D3D11_TEXTURE2D_DESC d = {};
    d.Width = d.Height = 256;
    d.MipLevels = d.ArraySize = 1;
    d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    Check(dev->CreateTexture2D(&d, nullptr, &page), "page");
    Check(dev->CreateShaderResourceView(page, nullptr, &page_srv), "page srv");
    d.Width = d.Height = kTarget;
    d.BindFlags = D3D11_BIND_RENDER_TARGET;
    Check(dev->CreateTexture2D(&d, nullptr, &target), "target");
    Check(dev->CreateRenderTargetView(target, nullptr, &target_rtv), "rtv");
    d.BindFlags = 0;
    d.Usage = D3D11_USAGE_STAGING;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    Check(dev->CreateTexture2D(&d, nullptr, &staging), "staging");

    // As the backend's MakeSampler(point, false).
    D3D11_SAMPLER_DESC s = {};
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    s.MaxAnisotropy = 1;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    s.MaxLOD = D3D11_FLOAT32_MAX;
    s.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    Check(dev->CreateSamplerState(&s, &smp_point), "point");
    s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    Check(dev->CreateSamplerState(&s, &smp_linear), "linear");

    D3D11_BUFFER_DESC b = {};
    b.ByteWidth = sizeof(Vtx) * 4;
    b.Usage = D3D11_USAGE_DYNAMIC;
    b.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    b.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Check(dev->CreateBuffer(&b, nullptr, &vb), "vb");
    const float consts[4] = {kTarget, kTarget, 0, 0};
    D3D11_SUBRESOURCE_DATA init = {consts, 0, 0};
    b.ByteWidth = 16;
    b.Usage = D3D11_USAGE_IMMUTABLE;
    b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    b.CPUAccessFlags = 0;
    Check(dev->CreateBuffer(&b, &init, &cb), "cb");

    const UINT stride = sizeof(Vtx), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetInputLayout(il);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(vs, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &cb);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->PSSetShaderResources(0, 1, &page_srv);
    ctx->OMSetRenderTargets(1, &target_rtv, nullptr);
    D3D11_VIEWPORT vp = {0, 0, kTarget, kTarget, 0, 1};
    ctx->RSSetViewports(1, &vp);
    ID3D11RasterizerState* rs;
    D3D11_RASTERIZER_DESC r = {};
    r.FillMode = D3D11_FILL_SOLID;
    r.CullMode = D3D11_CULL_NONE;
    r.DepthClipEnable = TRUE;
    Check(dev->CreateRasterizerState(&r, &rs), "rs");
    ctx->RSSetState(rs);
}

// One quad, PlayStation corner order (0 top left, 1 top right, 2 bottom left,
// 3 bottom right): game positions and texel bytes.
struct Quad {
    float gx[4], gy[4];
    unsigned char u[4], v[4];
    bool rect;
    int x0, y0, w, h;   // the rectangle, game pixels
    int kind;
};

const char* const kKinds[] = {"1:1", "stretched", "inclusive", "mirrored", "skewed"};
constexpr int kKindCount = 5;

int PsxTexel(int t0, int t1, int i, int n) {
    return static_cast<int>(std::floor(t0 + static_cast<double>(t1 - t0) * i / n));
}

Quad Random(std::mt19937& rng, int kind) {
    auto r = [&](int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); };
    Quad q = {};
    q.kind = kind;
    q.rect = kind != 4;
    q.x0 = r(2, 10);
    q.y0 = r(2, 10);
    int tw = r(1, 40), th = r(1, 40);
    const int ub = r(20, 200 - tw), vb = r(20, 200 - th);
    int u0 = ub, u1 = ub + tw, v0 = vb, v1 = vb + th;
    q.w = tw;
    q.h = th;
    if (kind == 1) {
        q.w = std::max(1, static_cast<int>(tw * (0.25 + r(0, 300) / 100.0)));
        q.h = std::max(1, static_cast<int>(th * (0.25 + r(0, 300) / 100.0)));
    } else if (kind == 2) {
        u1 = ub + tw - 1;
        v1 = vb + th - 1;
    } else if (kind == 3) {
        if (r(0, 1)) std::swap(u0, u1), --u0, --u1;
        else std::swap(v0, v1), --v0, --v1;
    }
    q.w = std::min(q.w, 60);
    q.h = std::min(q.h, 60);
    const float xs[4] = {float(q.x0), float(q.x0 + q.w), float(q.x0), float(q.x0 + q.w)};
    const float ys[4] = {float(q.y0), float(q.y0), float(q.y0 + q.h), float(q.y0 + q.h)};
    const int us[4] = {u0, u1, u0, u1}, vs[4] = {v0, v0, v1, v1};
    for (int i = 0; i < 4; ++i) {
        q.gx[i] = xs[i];
        q.gy[i] = ys[i];
        q.u[i] = static_cast<unsigned char>(us[i]);
        q.v[i] = static_cast<unsigned char>(vs[i]);
    }
    if (kind == 4)
        for (int i = 0; i < 4; ++i) {
            q.gx[i] += r(-6, 6) + r(0, 99) / 100.0f;
            q.gy[i] += r(-6, 6) + r(0, 99) / 100.0f;
            q.gx[i] = std::max(q.gx[i], 0.0f) + 12;
            q.gy[i] = std::max(q.gy[i], 0.0f) + 12;
        }
    return q;
}

// The page: art over the PlayStation's texels, red elsewhere.
std::vector<unsigned> Page(const Quad& q, int& ulo, int& uhi, int& vlo, int& vhi) {
    if (q.rect) {
        // The run the PlayStation walks: [t0, t1) up, [t1 + 1, t0 + 1) down.
        auto run = [](int t0, int t1, int& lo, int& hi) {
            lo = t1 > t0 ? t0 : t1 < t0 ? t1 + 1 : t0;   // t0 == t1: the one texel, every pixel
            hi = t1 > t0 ? t1 - 1 : t0;
        };
        run(q.u[0], q.u[1], ulo, uhi);
        run(q.v[0], q.v[2], vlo, vhi);
    } else {
        ulo = std::min({q.u[0], q.u[1], q.u[2], q.u[3]});
        uhi = std::max({q.u[0], q.u[1], q.u[2], q.u[3]}) - 1;
        vlo = std::min({q.v[0], q.v[1], q.v[2], q.v[3]});
        vhi = std::max({q.v[0], q.v[1], q.v[2], q.v[3]}) - 1;
    }
    std::vector<unsigned> px(256 * 256, 0xFF0000FFu);   // red (ABGR)
    for (int v = vlo; v <= vhi; ++v)
        for (int u = ulo; u <= uhi; ++u) {
            const unsigned g = 32 + 3 * (u - ulo), b = 32 + 3 * (v - vlo);
            px[v * 256 + u] = 0xFF000000u | (b << 16) | (g << 8);
        }
    return px;
}

float Tc(unsigned i) { return (static_cast<float>(i) + 0.512f) / 256.0f; }

std::vector<unsigned> Draw(const Quad& q, int k, bool ours, bool point) {
    Vtx v[4];
    float x[4], y[4], tu[4], tv[4];
    for (int i = 0; i < 4; ++i) {
        x[i] = q.gx[i] * k;
        y[i] = q.gy[i] * k;
        tu[i] = Tc(q.u[i]);
        tv[i] = Tc(q.v[i]);
    }
    if (ours) {
        quad_far_edge::ApplyAxis(x, y, q.u, tu, point);
        quad_far_edge::ApplyAxis(x, y, q.v, tv, point);
    }
    for (int i = 0; i < 4; ++i) v[i] = {x[i], y[i], 0.5f, 1.0f, tu[i], tv[i]};
    D3D11_MAPPED_SUBRESOURCE m;
    Check(ctx->Map(vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), "map vb");
    std::memcpy(m.pData, v, sizeof v);
    ctx->Unmap(vb, 0);
    const float clear[4] = {0, 0, 0, 0};
    ctx->ClearRenderTargetView(target_rtv, clear);
    ctx->PSSetSamplers(0, 1, point ? &smp_point : &smp_linear);
    ctx->Draw(4, 0);
    ctx->CopyResource(staging, target);
    Check(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m), "map staging");
    std::vector<unsigned> out(kTarget * kTarget);
    for (int r = 0; r < kTarget; ++r)
        std::memcpy(&out[r * kTarget], static_cast<unsigned char*>(m.pData) + r * m.RowPitch, kTarget * 4);
    ctx->Unmap(staging, 0);
    return out;
}

// The PlayStation's picture of a rectangle at scale k, as the probe's colours.
std::vector<unsigned> Psx(const Quad& q, int k, int ulo, int vlo) {
    std::vector<unsigned> out(kTarget * kTarget, 0);
    for (int j = 0; j < q.h; ++j)
        for (int i = 0; i < q.w; ++i) {
            const int u = PsxTexel(q.u[0], q.u[1], i, q.w), v = PsxTexel(q.v[0], q.v[2], j, q.h);
            const unsigned c = 0xFF000000u | (unsigned(32 + 3 * (v - vlo)) << 16) | (unsigned(32 + 3 * (u - ulo)) << 8);
            for (int b = 0; b < k; ++b)
                for (int a = 0; a < k; ++a) out[((q.y0 + j) * k + b) * kTarget + (q.x0 + i) * k + a] = c;
        }
    return out;
}

struct Tally {
    long quads = 0, bleed_quads = 0, bleed_px = 0, mismatch_quads = 0, mismatch_px = 0, px = 0;
};

void Measure(const std::vector<unsigned>& img, const std::vector<unsigned>& psx, bool rect, bool point, Tally& t) {
    long bleed = 0, mismatch = 0, drawn = 0;
    for (int i = 0; i < kTarget * kTarget; ++i) {
        const unsigned c = img[i];
        if (!(c >> 24)) continue;
        ++drawn;
        if ((c & 0xFF) > 4) ++bleed;
        if (rect && point && (c | 0xFF000000u) != psx[i]) ++mismatch;
    }
    ++t.quads;
    t.px += drawn;
    t.bleed_px += bleed;
    t.bleed_quads += bleed > 0;
    t.mismatch_px += mismatch;
    t.mismatch_quads += mismatch > 0;
}

void WritePpm(const std::string& path, const std::vector<unsigned>& img, int w, int h) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return;
    std::fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; ++i) {
        const unsigned c = img[i];
        const unsigned char rgb[3] = {static_cast<unsigned char>(c), static_cast<unsigned char>(c >> 8),
                                      static_cast<unsigned char>(c >> 16)};
        std::fwrite(rgb, 1, 3, f);
    }
    std::fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string out_dir = argc > 1 ? argv[1] : ".";
    const int per_kind = argc > 2 ? std::atoi(argv[2]) : 200;
    Setup();
    std::printf("%-5s %-6s %-10s %-4s %7s %12s %12s %14s %14s\n", "scale", "filter", "kind", "rule", "quads",
                "bleed quads", "bleed px", "mismatch quads", "mismatch px");
    for (int k = 1; k <= 4; ++k) {
        for (int filter = 0; filter < 2; ++filter) {
            const bool point = filter == 0;
            // The sheet: one row per kind, 6 quads a row, each cell PSX | port | ours.
            constexpr int kCols = 6;
            const int cell = 72 * k;
            const int sheet_w = kCols * 3 * cell, sheet_h = kKindCount * cell;
            std::vector<unsigned> sheet(sheet_w * sheet_h, 0xFF404040u);
            std::mt19937 rng(1234);
            for (int kind = 0; kind < kKindCount; ++kind) {
                Tally port, ours;
                for (int n = 0; n < per_kind; ++n) {
                    const Quad q = Random(rng, kind);
                    int ulo, uhi, vlo, vhi;
                    const std::vector<unsigned> px = Page(q, ulo, uhi, vlo, vhi);
                    ctx->UpdateSubresource(page, 0, nullptr, px.data(), 256 * 4, 0);
                    const std::vector<unsigned> psx = q.rect ? Psx(q, k, ulo, vlo) : std::vector<unsigned>(kTarget * kTarget, 0);
                    const std::vector<unsigned> a = Draw(q, k, false, point), b = Draw(q, k, true, point);
                    Measure(a, psx, q.rect, point, port);
                    Measure(b, psx, q.rect, point, ours);
                    if (n < kCols) {
                        const std::vector<unsigned>* imgs[3] = {&psx, &a, &b};
                        for (int s = 0; s < 3; ++s)
                            for (int y = 0; y < cell && y < kTarget; ++y)
                                for (int x = 0; x < cell && x < kTarget; ++x) {
                                    const unsigned c = (*imgs[s])[y * kTarget + x];
                                    if (c >> 24) sheet[(kind * cell + y) * sheet_w + (n * 3 + s) * cell + x] = c;
                                }
                    }
                }
                const Tally* ts[2] = {&port, &ours};
                for (int r = 0; r < 2; ++r)
                    std::printf("%-5d %-6s %-10s %-4s %7ld %12ld %12ld %14s %14s\n", k, point ? "point" : "linear",
                                kKinds[kind], r ? "ours" : "port", ts[r]->quads, ts[r]->bleed_quads, ts[r]->bleed_px,
                                (point && kind != 4) ? std::to_string(ts[r]->mismatch_quads).c_str() : "-",
                                (point && kind != 4) ? std::to_string(ts[r]->mismatch_px).c_str() : "-");
            }
            WritePpm(out_dir + "/sheet_k" + std::to_string(k) + "_" + (point ? "point" : "linear") + ".ppm", sheet, sheet_w,
                     sheet_h);
        }
    }
    return 0;
}
