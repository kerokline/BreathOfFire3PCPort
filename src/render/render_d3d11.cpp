// Direct3D 11 execution of the recorded frame (render_d3d11.h,
// docs/render-backend.md section 3).
//
// Pre-transformed vertices, as DirectX 6 took them: a TLVERTEX's sx, sy are
// pixel positions with the pixel's centre at the integer (Direct3D before 10),
// so the vertex shader adds half a pixel to land on Direct3D 11's centres;
// rhw is 1 / w, and the attributes interpolate perspective-correctly against
// it as they did then. No depth test (the game never enables one), no
// culling, no lighting: the ordering table already ordered everything.
#include "render/render_d3d11.h"

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>

#include <cstring>

#include "hook/log.h"
#include "render/crt.h"

namespace render {
namespace {

// --- state ---------------------------------------------------------------------------

Options g_opt;
ID3D11Device* g_device;
ID3D11DeviceContext* g_ctx;
IDXGISwapChain* g_swap;
ID3D11RenderTargetView* g_window_rtv;
U g_window_w, g_window_h;

ID3D11Texture2D* g_target;
ID3D11RenderTargetView* g_target_rtv;
ID3D11ShaderResourceView* g_target_srv;
U g_target_w, g_target_h;
U g_pad_x;   // target pixels each side of the game's view (DIV-0041): every vertex x moves by it
U g_scale;   // the target's k
U g_pending_scale;   // DIV-0042: a k asked for by RequestScale, applied after the next present; 0 none
void (*g_rescale_hook)(U);

ID3D11VertexShader* g_vs;
ID3D11PixelShader* g_ps;
ID3D11InputLayout* g_layout;
ID3D11VertexShader* g_present_vs;
ID3D11PixelShader* g_present_ps;
ID3D11Buffer* g_vertices;
U g_vertex_capacity;
ID3D11Buffer* g_cb_frame;
ID3D11Buffer* g_cb_draw;
ID3D11SamplerState* g_sampler_point;
ID3D11SamplerState* g_sampler_linear;
ID3D11SamplerState* g_sampler_present_point;
ID3D11SamplerState* g_sampler_present_linear;
ID3D11RasterizerState* g_raster;
ID3D11BlendState* g_blend_off;
bool g_crt;   // DIV-0037: the present draws through crt.cpp

struct BlendEntry {
    U src, dst;
    ID3D11BlendState* state;
};
BlendEntry g_blends[16];
U g_n_blends;

struct GpuTexture {
    ID3D11Texture2D* texture;
    ID3D11ShaderResourceView* srv;
    U width, height;
};

unsigned char* g_scratch;   // a texture's pixels converted to B8G8R8A8
U g_scratch_bytes;

DWORD g_main_thread;

// --- helpers -------------------------------------------------------------------------

// The game's x87 arithmetic runs under control word 0x027F
// (docs/psx-library-layer.md section 2, 11 million calls measured), and a
// driver's present is free to change it - the fixed-point conversions in the
// frame after went wrong the first time this backend showed a frame. Every
// entry into Direct3D keeps the word it found.
class FpuGuard {
public:
    FpuGuard() { __asm__ volatile("fnstcw %0" : "=m"(cw_)); }
    ~FpuGuard() { __asm__ volatile("fldcw %0" : : "m"(cw_)); }
    FpuGuard(const FpuGuard&) = delete;
    FpuGuard& operator=(const FpuGuard&) = delete;

private:
    std::uint16_t cw_;
};

void Check(HRESULT hr, const char* what) {
    if (FAILED(hr)) bof3::Fatal("render: %s failed, HRESULT 0x%08lX", what, static_cast<unsigned long>(hr));
}

template <class T> void Release(T*& p) {
    if (p) {
        p->Release();
        p = nullptr;
    }
}


// --- shaders ---------------------------------------------------------------------------

const char kSceneShader[] = R"(
cbuffer FrameConstants : register(b0) { float2 target_size; float2 view_pad; };
cbuffer DrawConstants : register(b1) { uint flags; uint alpha_ref; uint alpha_func; uint draw_pad; };
Texture2D tex : register(t0);
SamplerState smp : register(s0);
struct VSIn { float4 pos : POSITION; float4 diffuse : COLOR0; float4 specular : COLOR1; float2 uv : TEXCOORD0; };
struct PSIn { float4 pos : SV_Position; float4 diffuse : COLOR0; float4 specular : COLOR1; float2 uv : TEXCOORD0; };
PSIn VS(VSIn i) {
    PSIn o;
    float w = i.pos.w > 0.0 ? 1.0 / i.pos.w : 1.0;
    float2 ndc = float2((i.pos.x + view_pad.x + PIXEL_OFFSET) / target_size.x * 2.0 - 1.0, 1.0 - (i.pos.y + PIXEL_OFFSET) / target_size.y * 2.0);
    o.pos = float4(ndc * w, saturate(i.pos.z) * w, w);
    o.diffuse = i.diffuse;
    o.specular = i.specular;
    o.uv = i.uv;
    return o;
}
float4 PS(PSIn i) : SV_Target {
    float4 c = i.diffuse;
    if (flags & 1u) {
        float4 t = tex.Sample(smp, i.uv);
        if ((flags & 2u) && t.a < 0.5) discard;
        c.rgb *= t.rgb;
        if (flags & 4u) c.a *= t.a;
    }
    if (flags & 8u) c.rgb += i.specular.rgb;
    if (flags & 16u) {
        uint a = (uint)round(saturate(c.a) * 255.0);
        bool keep = true;
        if (alpha_func == 1u) keep = false;
        else if (alpha_func == 2u) keep = a < alpha_ref;
        else if (alpha_func == 3u) keep = a == alpha_ref;
        else if (alpha_func == 4u) keep = a <= alpha_ref;
        else if (alpha_func == 5u) keep = a > alpha_ref;
        else if (alpha_func == 6u) keep = a != alpha_ref;
        else if (alpha_func == 7u) keep = a >= alpha_ref;
        if (!keep) discard;
    }
    return c;
}
)";

const char kPresentShader[] = R"(
Texture2D tex : register(t0);
SamplerState smp : register(s0);
struct PSIn { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
PSIn VS(uint id : SV_VertexID) {
    PSIn o;
    float2 uv = float2(id & 1u, id >> 1u);
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    o.uv = uv;
    return o;
}
float4 PS(PSIn i) : SV_Target { return float4(tex.Sample(smp, i.uv).rgb, 1.0); }
)";

// The scene shader's pixel-centre offset: 0.5, Direct3D 6's integer centres on
// Direct3D 11's half-integer ones. BOF3X_PIXEL_OFFSET replaces it for an
// experiment - HANDOFF's edge-pixel A/B tries 0.498046875 (0.5 - 1/512, the
// D3D8-to-9 wrappers' value) against rb1's 27 captures that differ at edges.
// Digits, one point and a sign only; anything else is refused.
char g_pixel_offset[24] = "0.5";

void ReadPixelOffset() {
    char text[24];
    const DWORD n = GetEnvironmentVariableA("BOF3X_PIXEL_OFFSET", text, sizeof text);
    if (n == 0) return;
    if (n >= sizeof text) bof3::Fatal("BOF3X_PIXEL_OFFSET: too long");
    for (const char* p = text; *p; ++p)
        if (!((*p >= '0' && *p <= '9') || *p == '.' || (*p == '-' && p == text)))
            bof3::Fatal("BOF3X_PIXEL_OFFSET=%s: a decimal number", text);
    std::memcpy(g_pixel_offset, text, sizeof text);
    bof3::Log("render: scene pixel-centre offset %s (BOF3X_PIXEL_OFFSET)", g_pixel_offset);
}

ID3DBlob* Compile(const char* source, const char* entry, const char* profile) {
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const D3D_SHADER_MACRO macros[] = {{"PIXEL_OFFSET", g_pixel_offset}, {nullptr, nullptr}};
    const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, macros, nullptr, entry, profile,
                                  D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(hr)) {
        const char* text = errors ? static_cast<const char*>(errors->GetBufferPointer()) : "(no message)";
        bof3::Fatal("render: shader %s (%s) failed to compile: %s", entry, profile, text);
    }
    Release(errors);
    return code;
}

// --- set-up ----------------------------------------------------------------------------

void MakeWindowTarget() {
    Release(g_window_rtv);
    ID3D11Texture2D* back = nullptr;
    Check(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)), "GetBuffer");
    Check(g_device->CreateRenderTargetView(back, nullptr, &g_window_rtv), "CreateRenderTargetView (window)");
    Release(back);
}

void MakeTarget(U w, U h) {
    Release(g_target_srv);
    Release(g_target_rtv);
    Release(g_target);
    D3D11_TEXTURE2D_DESC d = {};
    d.Width = w;
    d.Height = h;
    d.MipLevels = 1;
    d.ArraySize = 1;
    d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    Check(g_device->CreateTexture2D(&d, nullptr, &g_target), "CreateTexture2D (target)");
    Check(g_device->CreateRenderTargetView(g_target, nullptr, &g_target_rtv), "CreateRenderTargetView (target)");
    Check(g_device->CreateShaderResourceView(g_target, nullptr, &g_target_srv), "CreateShaderResourceView (target)");
    g_target_w = w;
    g_target_h = h;
    const float clear[4] = {0, 0, 0, 1};
    g_ctx->ClearRenderTargetView(g_target_rtv, clear);
}

ID3D11SamplerState* MakeSampler(bool point, bool clamp) {
    D3D11_SAMPLER_DESC s = {};
    s.Filter = point ? D3D11_FILTER_MIN_MAG_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    s.AddressU = s.AddressV = s.AddressW = clamp ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
    s.MaxAnisotropy = 1;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    s.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState* out = nullptr;
    Check(g_device->CreateSamplerState(&s, &out), "CreateSamplerState");
    return out;
}

ID3D11BlendState* BlendFor(bool enable, U src, U dst) {
    if (!enable) return g_blend_off;
    for (U i = 0; i < g_n_blends; ++i)
        if (g_blends[i].src == src && g_blends[i].dst == dst) return g_blends[i].state;
    if (g_n_blends >= 16) bof3::Fatal("render: more than 16 blend modes");
    if (src < 1 || src > 11 || dst < 1 || dst > 11) bof3::Fatal("render: blend factors %u / %u", src, dst);
    D3D11_BLEND_DESC b = {};
    b.RenderTarget[0].BlendEnable = TRUE;
    // D3DBLEND and D3D11_BLEND share their numbering for 1..11.
    b.RenderTarget[0].SrcBlend = static_cast<D3D11_BLEND>(src);
    b.RenderTarget[0].DestBlend = static_cast<D3D11_BLEND>(dst);
    b.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    b.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    b.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    b.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    b.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ID3D11BlendState* out = nullptr;
    Check(g_device->CreateBlendState(&b, &out), "CreateBlendState");
    g_blends[g_n_blends++] = {src, dst, out};
    return out;
}

ID3D11Buffer* MakeConstants(U bytes) {
    D3D11_BUFFER_DESC d = {};
    d.ByteWidth = bytes;
    d.Usage = D3D11_USAGE_DYNAMIC;
    d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Buffer* out = nullptr;
    Check(g_device->CreateBuffer(&d, nullptr, &out), "CreateBuffer (constants)");
    return out;
}

void PutConstants(ID3D11Buffer* buffer, const void* data, U bytes) {
    D3D11_MAPPED_SUBRESOURCE m;
    Check(g_ctx->Map(buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), "Map (constants)");
    std::memcpy(m.pData, data, bytes);
    g_ctx->Unmap(buffer, 0);
}

// --- textures ------------------------------------------------------------------------------

// The shift and width of a channel mask.
void MaskBits(U mask, U* shift, U* bits) {
    *shift = 0;
    *bits = 0;
    if (mask == 0) return;
    while (!((mask >> *shift) & 1)) ++*shift;
    while (*shift + *bits < 32 && ((mask >> (*shift + *bits)) & 1)) ++*bits;
}
// A channel of `raw` under a mask of `bits` at `shift`, widened to 8 bits with
// the high bits repeated below, as DirectDraw expanded them.
U Channel(U raw, U mask, U shift, U bits) {
    if (bits == 0) return 0;
    const U v = (raw & mask) >> shift;
    if (bits >= 8) return v >> (bits - 8);
    return (v << (8 - bits)) | (v >> (2 * bits > 8 ? 2 * bits - 8 : 0));
}

// Converts `pixels` (the surface's format) to B8G8R8A8 in the scratch buffer:
// alpha 0 for a texel whose alpha bit is clear (a format with alpha) or that
// equals the surface's colour key; 255 otherwise.
const unsigned char* Convert(const Surface* s, const unsigned char* pixels) {
    const U bytes = s->width * s->height * 4;
    if (bytes > g_scratch_bytes) {
        if (g_scratch) HeapFree(GetProcessHeap(), 0, g_scratch);
        g_scratch = static_cast<unsigned char*>(HeapAlloc(GetProcessHeap(), 0, bytes));
        if (!g_scratch) bof3::Fatal("render: out of memory converting a %u x %u texture", s->width, s->height);
        g_scratch_bytes = bytes;
    }
    const bool keyed = s->has_color_key;
    const U key = s->color_key;
    U rs, rb, gs, gb, bs, bb;
    MaskBits(s->pf_rmask, &rs, &rb);
    MaskBits(s->pf_gmask, &gs, &gb);
    MaskBits(s->pf_bmask, &bs, &bb);
    const U raw_mask = (s->bpp == 32 ? 0xFFFFFFFFu : 0xFFFFu) & ~s->pf_amask;
    for (U y = 0; y < s->height; ++y) {
        const unsigned char* row = pixels + y * s->pitch;
        unsigned char* out = g_scratch + y * s->width * 4;
        for (U x = 0; x < s->width; ++x) {
            U raw;
            if (s->bpp == 32) {
                std::memcpy(&raw, row + x * 4, 4);
            } else {
                std::uint16_t p;
                std::memcpy(&p, row + x * 2, 2);
                raw = p;
            }
            U a = 0xFF;
            if (s->pf_amask && !(raw & s->pf_amask)) a = 0;
            if (keyed && ((raw ^ key) & raw_mask) == 0) a = 0;
            const U v = (a << 24) | (Channel(raw, s->pf_rmask, rs, rb) << 16) | (Channel(raw, s->pf_gmask, gs, gb) << 8) |
                        Channel(raw, s->pf_bmask, bs, bb);
            std::memcpy(out + x * 4, &v, 4);
        }
    }
    return g_scratch;
}

GpuTexture* GpuOf(Surface* s) {
    auto* g = static_cast<GpuTexture*>(s->gpu);
    if (g && (g->width != s->width || g->height != s->height)) {
        Release(g->srv);
        Release(g->texture);
        HeapFree(GetProcessHeap(), 0, g);
        g = nullptr;
        s->gpu = nullptr;
    }
    if (!g) {
        g = static_cast<GpuTexture*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(GpuTexture)));
        if (!g) bof3::Fatal("render: out of memory for a texture");
        D3D11_TEXTURE2D_DESC d = {};
        d.Width = s->width;
        d.Height = s->height;
        d.MipLevels = 1;
        d.ArraySize = 1;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        Check(g_device->CreateTexture2D(&d, nullptr, &g->texture), "CreateTexture2D");
        Check(g_device->CreateShaderResourceView(g->texture, nullptr, &g->srv), "CreateShaderResourceView");
        g->width = s->width;
        g->height = s->height;
        s->gpu = g;
        s->dirty = true;
    }
    return g;
}

// Makes the GPU texture hold what `version` sees.
GpuTexture* Bind(TexVersion* version) {
    Surface* s = version->surface;
    if (!s->pixels) bof3::Fatal("render: a draw uses a released surface");
    GpuTexture* g = GpuOf(s);
    if (version->pixels) {
        // A snapshot: what the surface held when the draw was recorded.
        g_ctx->UpdateSubresource(g->texture, 0, nullptr, Convert(s, version->pixels), s->width * 4, 0);
        s->dirty = true;   // the live pixels come next
    } else if (s->dirty) {
        g_ctx->UpdateSubresource(g->texture, 0, nullptr, Convert(s, s->pixels), s->width * 4, 0);
        s->dirty = false;
    }
    return g;
}

// --- the frame -------------------------------------------------------------------------------

bool SameState(const PipeState& a, const PipeState& b) {
    return a.texture == b.texture && a.src_blend == b.src_blend && a.dst_blend == b.dst_blend &&
           a.blend_enable == b.blend_enable && a.alpha_test == b.alpha_test && a.alpha_ref == b.alpha_ref &&
           a.alpha_func == b.alpha_func && a.color_key == b.color_key && a.specular == b.specular &&
           a.alpha_modulate == b.alpha_modulate && a.point_filter == b.point_filter && a.topology == b.topology;
}

void Draw(const PipeState& s, U first, U count) {
    struct {
        U flags, ref, func, pad;
    } c = {};
    if (s.texture) c.flags |= 1;
    if (s.texture && s.color_key) c.flags |= 2;
    if (s.alpha_modulate) c.flags |= 4;
    if (s.specular) c.flags |= 8;
    if (s.alpha_test) c.flags |= 16;
    c.ref = s.alpha_ref;
    c.func = s.alpha_func;
    PutConstants(g_cb_draw, &c, sizeof c);
    ID3D11ShaderResourceView* srv = s.texture ? Bind(s.texture)->srv : nullptr;
    g_ctx->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* sampler = s.point_filter ? g_sampler_point : g_sampler_linear;
    g_ctx->PSSetSamplers(0, 1, &sampler);
    const float factor[4] = {0, 0, 0, 0};
    g_ctx->OMSetBlendState(BlendFor(s.blend_enable, s.src_blend, s.dst_blend), factor, 0xFFFFFFFF);
    g_ctx->IASetPrimitiveTopology(s.topology == 0 ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
                                  : s.topology == 1 ? D3D11_PRIMITIVE_TOPOLOGY_LINELIST
                                                    : D3D11_PRIMITIVE_TOPOLOGY_POINTLIST);
    g_ctx->Draw(count, first);
}

void RunFrame(Frame& frame) {
    // Vertices, once.
    if (frame.n_vertices > 0) {
        if (frame.n_vertices > g_vertex_capacity) bof3::Fatal("render: %u vertices exceed the buffer", frame.n_vertices);
        D3D11_MAPPED_SUBRESOURCE m;
        Check(g_ctx->Map(g_vertices, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), "Map (vertices)");
        std::memcpy(m.pData, frame.vertices, frame.n_vertices * sizeof(Vertex));
        g_ctx->Unmap(g_vertices, 0);
    }
    const UINT stride = sizeof(Vertex), offset = 0;
    g_ctx->IASetInputLayout(g_layout);
    g_ctx->IASetVertexBuffers(0, 1, &g_vertices, &stride, &offset);
    g_ctx->VSSetShader(g_vs, nullptr, 0);
    g_ctx->PSSetShader(g_ps, nullptr, 0);
    g_ctx->VSSetConstantBuffers(0, 1, &g_cb_frame);
    g_ctx->PSSetConstantBuffers(1, 1, &g_cb_draw);
    g_ctx->RSSetState(g_raster);
    D3D11_VIEWPORT vp = {0, 0, static_cast<float>(g_target_w), static_cast<float>(g_target_h), 0, 1};
    g_ctx->RSSetViewports(1, &vp);
    g_ctx->OMSetRenderTargets(1, &g_target_rtv, nullptr);
    struct {
        float w, h, pad_x, pad_y;
    } fc = {static_cast<float>(g_target_w), static_cast<float>(g_target_h), static_cast<float>(g_pad_x), 0};
    PutConstants(g_cb_frame, &fc, sizeof fc);

    // Consecutive draws in one state merge into one call.
    bool pending = false;
    PipeState state = {};
    U first = 0, count = 0;
    for (U i = 0; i < frame.n_commands; ++i) {
        const Command& c = frame.commands[i];
        if (c.kind == Cmd::kClear) {
            if (pending) Draw(state, first, count);
            pending = false;
            const float rgba[4] = {((c.color >> 16) & 0xFF) / 255.0f, ((c.color >> 8) & 0xFF) / 255.0f,
                                   (c.color & 0xFF) / 255.0f, 1.0f};
            g_ctx->ClearRenderTargetView(g_target_rtv, rgba);
            continue;
        }
        if (pending && SameState(state, c.state) && first + count == c.first) {
            count += c.count;
            continue;
        }
        if (pending) Draw(state, first, count);
        state = c.state;
        first = c.first;
        count = c.count;
        pending = true;
    }
    if (pending) Draw(state, first, count);
    ID3D11ShaderResourceView* none = nullptr;
    g_ctx->PSSetShaderResources(0, 1, &none);
}

void Show() {
    RECT client;
    GetClientRect(static_cast<HWND>(g_opt.hwnd), &client);
    const U cw = static_cast<U>(client.right - client.left), ch = static_cast<U>(client.bottom - client.top);
    if (cw == 0 || ch == 0) return;   // minimised: nothing to show
    if (cw != g_window_w || ch != g_window_h) {
        Release(g_window_rtv);
        g_ctx->OMSetRenderTargets(0, nullptr, nullptr);
        Check(g_swap->ResizeBuffers(0, cw, ch, DXGI_FORMAT_UNKNOWN, 0), "ResizeBuffers");
        MakeWindowTarget();
        g_window_w = cw;
        g_window_h = ch;
    }
    // DIV-0042: with snap, an integer scale of the target on the window,
    // centred, black borders - and the target follows the client (RequestScale),
    // so in a window that is 1:1. Without snap, or when the client is smaller
    // than the target (a window dragged small, or F8 back to a window from a
    // borderless start, DIV-0036), the largest fit of the target's shape:
    // the picture fills the client's height or width, never cropped.
    const U kx = cw / g_target_w, ky = ch / g_target_h;
    const U k = g_opt.snap ? (kx < ky ? kx : ky) : 0;
    U w = g_target_w * k, h = g_target_h * k;
    if (k == 0) {
        if (static_cast<std::uint64_t>(cw) * g_target_h >= static_cast<std::uint64_t>(ch) * g_target_w) {
            h = ch;
            w = static_cast<U>(static_cast<std::uint64_t>(ch) * g_target_w / g_target_h);
        } else {
            w = cw;
            h = static_cast<U>(static_cast<std::uint64_t>(cw) * g_target_h / g_target_w);
        }
    }
    const float black[4] = {0, 0, 0, 1};
    g_ctx->ClearRenderTargetView(g_window_rtv, black);
    g_ctx->OMSetRenderTargets(1, &g_window_rtv, nullptr);
    D3D11_VIEWPORT vp = {static_cast<float>((cw > w ? cw - w : 0) / 2), static_cast<float>((ch > h ? ch - h : 0) / 2),
                         static_cast<float>(w), static_cast<float>(h), 0, 1};
    g_ctx->RSSetViewports(1, &vp);
    const float factor[4] = {0, 0, 0, 0};
    g_ctx->OMSetBlendState(g_blend_off, factor, 0xFFFFFFFF);
    if (g_crt) {
        CrtDraw(g_ctx, g_target_srv, g_window_rtv, vp);
        const HRESULT hr = g_swap->Present(g_opt.vsync ? 1 : 0, 0);
        if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) Check(hr, "Present");
        return;
    }
    g_ctx->IASetInputLayout(nullptr);
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    g_ctx->VSSetShader(g_present_vs, nullptr, 0);
    g_ctx->PSSetShader(g_present_ps, nullptr, 0);
    g_ctx->PSSetShaderResources(0, 1, &g_target_srv);
    ID3D11SamplerState* sampler = g_opt.point_filter ? g_sampler_present_point : g_sampler_present_linear;
    g_ctx->PSSetSamplers(0, 1, &sampler);
    g_ctx->Draw(4, 0);
    ID3D11ShaderResourceView* none = nullptr;
    g_ctx->PSSetShaderResources(0, 1, &none);
    const HRESULT hr = g_swap->Present(g_opt.vsync ? 1 : 0, 0);
    if (FAILED(hr) && hr != DXGI_STATUS_OCCLUDED) Check(hr, "Present");
}

}  // namespace

namespace {
void InitOnFiber(const Options& options) {
    FpuGuard fpu;
    g_opt = options;
    DXGI_SWAP_CHAIN_DESC sd = {};
    char swap[16] = "discard";
    GetEnvironmentVariableA("BOF3X_SWAP", swap, sizeof swap);
    const bool flip = swap[0] == 'f', sequential = swap[0] == 's';
    sd.BufferCount = flip ? 2 : 1;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = static_cast<HWND>(options.hwnd);
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = flip ? DXGI_SWAP_EFFECT_FLIP_DISCARD : sequential ? DXGI_SWAP_EFFECT_SEQUENTIAL : DXGI_SWAP_EFFECT_DISCARD;
    bof3::Log("render: swap effect %s", swap);
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (GetEnvironmentVariableA("BOF3X_D3D_DEBUG", nullptr, 0) > 0) flags |= D3D11_CREATE_DEVICE_DEBUG;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 3,
                                               D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &got, &g_ctx);
    if (FAILED(hr) && (flags & D3D11_CREATE_DEVICE_DEBUG)) {
        bof3::Log("render: no debug layer (0x%08lX), retrying without", static_cast<unsigned long>(hr));
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, levels, 3,
                                           D3D11_SDK_VERSION, &sd, &g_swap, &g_device, &got, &g_ctx);
    }
    Check(hr, "D3D11CreateDeviceAndSwapChain");
    IDXGIDevice* dxgi_device = nullptr;
    if (SUCCEEDED(g_device->QueryInterface(__uuidof(IDXGIDevice), reinterpret_cast<void**>(&dxgi_device)))) {
        IDXGIAdapter* adapter = nullptr;
        if (SUCCEEDED(dxgi_device->GetAdapter(&adapter))) {
            IDXGIFactory* factory = nullptr;
            if (SUCCEEDED(adapter->GetParent(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory)))) {
                // The game's WndProc owns Alt+Enter and the window's size.
                factory->MakeWindowAssociation(static_cast<HWND>(options.hwnd), DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
                factory->Release();
            }
            adapter->Release();
        }
        dxgi_device->Release();
    }
    RECT client;
    GetClientRect(static_cast<HWND>(options.hwnd), &client);
    g_window_w = static_cast<U>(client.right - client.left);
    g_window_h = static_cast<U>(client.bottom - client.top);
    MakeWindowTarget();
    g_pad_x = options.pad_x * options.scale;
    g_scale = options.scale;
    MakeTarget((options.logical_w + 2 * options.pad_x) * options.scale, options.logical_h * options.scale);
    ReadPixelOffset();

    ID3DBlob* vs = Compile(kSceneShader, "VS", "vs_4_0");
    ID3DBlob* ps = Compile(kSceneShader, "PS", "ps_4_0");
    Check(g_device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vs), "CreateVertexShader");
    Check(g_device->CreatePixelShader(ps->GetBufferPointer(), ps->GetBufferSize(), nullptr, &g_ps), "CreatePixelShader");
    const D3D11_INPUT_ELEMENT_DESC layout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 1, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    Check(g_device->CreateInputLayout(layout, 4, vs->GetBufferPointer(), vs->GetBufferSize(), &g_layout), "CreateInputLayout");
    vs->Release();
    ps->Release();
    ID3DBlob* pvs = Compile(kPresentShader, "VS", "vs_4_0");
    ID3DBlob* pps = Compile(kPresentShader, "PS", "ps_4_0");
    Check(g_device->CreateVertexShader(pvs->GetBufferPointer(), pvs->GetBufferSize(), nullptr, &g_present_vs), "CreateVertexShader (present)");
    Check(g_device->CreatePixelShader(pps->GetBufferPointer(), pps->GetBufferSize(), nullptr, &g_present_ps), "CreatePixelShader (present)");
    pvs->Release();
    pps->Release();

    const Frame& frame = CurrentFrame();
    g_vertex_capacity = frame.max_vertices;
    D3D11_BUFFER_DESC vb = {};
    vb.ByteWidth = g_vertex_capacity * sizeof(Vertex);
    vb.Usage = D3D11_USAGE_DYNAMIC;
    vb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Check(g_device->CreateBuffer(&vb, nullptr, &g_vertices), "CreateBuffer (vertices)");
    g_cb_frame = MakeConstants(16);
    g_cb_draw = MakeConstants(16);
    g_sampler_point = MakeSampler(true, false);
    g_sampler_linear = MakeSampler(false, false);
    g_sampler_present_point = MakeSampler(true, true);
    g_sampler_present_linear = MakeSampler(false, true);
    D3D11_RASTERIZER_DESC rs = {};
    rs.FillMode = D3D11_FILL_SOLID;
    rs.CullMode = D3D11_CULL_NONE;
    rs.DepthClipEnable = FALSE;
    Check(g_device->CreateRasterizerState(&rs, &g_raster), "CreateRasterizerState");
    D3D11_BLEND_DESC off = {};
    off.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Check(g_device->CreateBlendState(&off, &g_blend_off), "CreateBlendState (off)");
    g_crt = CrtWanted();
    if (g_crt) CrtInit(g_device, g_target_w, g_target_h, options.scale);

    bof3::Log("render: Direct3D 11 feature level 0x%X, window %u x %u, target %u x %u (view %u x %u at %u, %u columns a side), present %s",
              got, g_window_w, g_window_h, g_target_w, g_target_h, options.logical_w, options.logical_h, options.scale,
              options.pad_x, options.point_filter ? "point" : "linear");
    bof3::Log("DIV-0042    present: %s", options.snap ? "whole multiples of the picture" : "the picture fitted to the client");
}

// --- the fiber ----------------------------------------------------------------------------
//
// The game calls the present from inside a cooperative task, on one of its
// 16 KB stacks (docs/SCAFFOLDING.md section 3) - and those stacks are carved
// out of the main thread's own stack, so the TEB's bounds do not tell them
// apart. A DXGI present wants far more than 16 KB (the first attempt ran off
// the task's stack into the scheduler's records, docs/render-backend.md
// section 5). So every entry into Direct3D runs on a fiber of its own with a
// 1 MB stack: the game thread converts itself to a fiber once, switches to
// the render fiber for the length of the call, and gets its own context back
// exactly as it left it - the game's esp swapping is untouched, because a
// fiber switch saves and restores esp like any other register.

void* g_game_fiber;
void* g_render_fiber;
enum class Job { kNone, kInit, kPresent };
Job g_job;
Frame* g_job_frame;
const Options* g_job_options;

void InitOnFiber(const Options& options);
void PresentOnFiber(Frame& frame);

void CALLBACK RenderFiber(void*) {
    for (;;) {
        switch (g_job) {
        case Job::kInit: InitOnFiber(*g_job_options); break;
        case Job::kPresent: PresentOnFiber(*g_job_frame); break;
        case Job::kNone: break;
        }
        g_job = Job::kNone;
        SwitchToFiber(g_game_fiber);
    }
}

void RunOnFiber(Job job) {
    if (!g_render_fiber) {
        g_game_fiber = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
        if (!g_game_fiber) g_game_fiber = GetCurrentFiber();   // already a fiber
        g_render_fiber = CreateFiberEx(256 * 1024, 1024 * 1024, FIBER_FLAG_FLOAT_SWITCH, &RenderFiber, nullptr);
        if (!g_render_fiber) bof3::Fatal("render: CreateFiberEx failed, error %lu", GetLastError());
    }
    g_job = job;
    SwitchToFiber(g_render_fiber);
}

}  // namespace

void PresentFrame(Frame& frame) {
    if (GetCurrentThreadId() != g_main_thread) bof3::Fatal("render: the present on thread %lu, not the game's", GetCurrentThreadId());
    g_job_frame = &frame;
    RunOnFiber(Job::kPresent);
}

namespace {
// DIV-0042: the target remade at the scale RequestScale asked for. After the
// present, so the frame just drawn was recorded and run at one size; before
// the game builds the next, whose draw handlers read the scale the hook now
// writes (every reader of D3d_ScaleX/Y is in 0x59F680..0x5A5500, the OT walk
// inside the present - image scan 2026-09-23).
void ApplyPendingScale() {
    const U k = g_pending_scale;
    g_pending_scale = 0;
    if (k == 0 || k == g_scale) return;
    g_scale = k;
    g_pad_x = g_opt.pad_x * k;
    MakeTarget((g_opt.logical_w + 2 * g_opt.pad_x) * k, g_opt.logical_h * k);
    if (g_crt) CrtResize(g_device, g_target_w, g_target_h, k);
    bof3::Log("DIV-0042    target %u x %u at scale %u", g_target_w, g_target_h, k);
    if (g_rescale_hook) g_rescale_hook(k);
}

void PresentOnFiber(Frame& frame) {
    FpuGuard fpu;
    SweepReleased();
    RunFrame(frame);
    Show();
    ApplyPendingScale();
}
}  // namespace

// --- public ----------------------------------------------------------------------------------

void InitD3d11(const Options& options) {
    g_main_thread = GetCurrentThreadId();
    g_job_options = &options;
    RunOnFiber(Job::kInit);
    SetPresentHook(&PresentFrame);
}

void SweepReleased() {
    U n;
    Surface* const* all = AllSurfaces(&n);
    for (U i = 0; i < n; ++i) {
        Surface* s = all[i];
        if (s->refs == 0 && s->gpu) {
            auto* g = static_cast<GpuTexture*>(s->gpu);
            Release(g->srv);
            Release(g->texture);
            HeapFree(GetProcessHeap(), 0, g);
            s->gpu = nullptr;
        }
    }
}

U TargetWidth() { return g_target_w; }
U TargetScale() { return g_scale; }

void RequestScale(U k) {
    if (k < 1 || k > 8) bof3::Fatal("render: RequestScale(%u)", k);
    g_pending_scale = k;
}

void SetRescaleHook(void (*hook)(U)) { g_rescale_hook = hook; }
U TargetHeight() { return g_target_h; }

}  // namespace render
