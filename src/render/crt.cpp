// The CRT look (crt.h, DIV-0037, docs/crt-look.md).
//
// Four passes, every one ours:
//
//   1. Shrink: the render target (320k x 240k) to the game's own 320 x 240,
//      each texel the mean of its k x k block in linear light.
//   2, 3. A separable Gaussian blur of that, across and then down: the glow.
//   4. The picture: for each window pixel, the two source lines above and
//      below it, each a Gaussian beam whose width grows with the line's
//      brightness (a bright line blooms into the gap, a dark one leaves it
//      dark), mixed with the flat picture by a scanline strength that is
//      itself lower for bright lines; the glow added (halation) and
//      optionally mixed in (diffusion); gamma out. No phosphor mask (the
//      owner, 2026-09-23) and no curvature.
//
// Colours are linearised by a power, worked on, and re-encoded by its
// inverse, because the swap chain is UNORM and the game's colours are
// display-referred. Line colours are sampled from the target at each source
// line's centre with the bilinear sampler, so at k > 1 horizontal detail finer
// than 320 columns - the target's polygon edges - survives.
#include "render/crt.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <cstdlib>
#include <cstring>

#include "hook/log.h"

namespace render {
namespace {

const char kCrtShader[] = R"(
cbuffer Crt : register(b0) {
    float2 src_size; float2 target_size;
    float halation; float diffusion; float scan_dark; float scan_bright;
    float beam_dark; float beam_bright; float brightness; float pad0;
    float gamma_in; float gamma_out; float k; float pad;
};
Texture2D pic : register(t0);
Texture2D glow : register(t1);
SamplerState lin : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V VS(uint id : SV_VertexID) {
    V o;
    float2 uv = float2(id & 1u, id >> 1u);
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    o.uv = uv;
    return o;
}
float3 ToLinear(float3 c) { return pow(max(c, 0.0), gamma_in); }

float4 Shrink(V i) : SV_Target {
    int n = (int)k;
    int2 base = int2(i.pos.xy) * n;
    float3 sum = 0.0;
    [loop] for (int y = 0; y < n; ++y)
        [loop] for (int x = 0; x < n; ++x)
            sum += ToLinear(pic.Load(int3(base + int2(x, y), 0)).rgb);
    return float4(sum / (k * k), 1.0);
}

float3 Blur(int2 p, int2 step) {
    float3 sum = 0.0;
    float total = 0.0;
    int2 last = int2(src_size) - 1;
    [unroll] for (int d = -6; d <= 6; ++d) {
        float w = exp(-(float)(d * d) / 8.0);   // sigma 2 source pixels
        sum += w * glow.Load(int3(clamp(p + step * d, int2(0, 0), last), 0)).rgb;
        total += w;
    }
    return sum / total;
}
float4 BlurAcross(V i) : SV_Target { return float4(Blur(int2(i.pos.xy), int2(1, 0)), 1.0); }
float4 BlurDown(V i) : SV_Target { return float4(Blur(int2(i.pos.xy), int2(0, 1)), 1.0); }

float3 SourceLine(float n, float u) {
    float v = (clamp(n, 0.0, src_size.y - 1.0) + 0.5) / src_size.y;
    return ToLinear(pic.SampleLevel(lin, float2(u, v), 0).rgb);
}
float Beam(float d, float3 c) {
    float w = lerp(beam_dark, beam_bright, saturate(max(c.r, max(c.g, c.b))));
    float t = d / w;
    return exp(-2.0 * t * t);
}
float4 Picture(V i) : SV_Target {
    float y = i.uv.y * src_size.y - 0.5;
    float n = floor(y);
    float f = y - n;
    float3 c0 = SourceLine(n, i.uv.x);
    float3 c1 = SourceLine(n + 1.0, i.uv.x);
    float3 beams = c0 * Beam(f, c0) + c1 * Beam(1.0 - f, c1);
    float3 flat = f < 0.5 ? c0 : c1;
    float strength = lerp(scan_dark, scan_bright, saturate(dot(flat, float3(0.2126, 0.7152, 0.0722))));
    float3 c = lerp(flat, beams, strength);
    float3 g = glow.SampleLevel(lin, i.uv, 0).rgb;
    c = lerp(c, g, diffusion) + g * halation;
    return float4(pow(saturate(c * brightness), 1.0 / gamma_out), 1.0);
}
)";

// The look's numbers. The defaults are a first guess for the owner to tune
// in game with BOF3X_CRT="name=value,..." - every name below.
struct Constants {
    float src_w, src_h, target_w, target_h;
    float halation, diffusion, scan_dark, scan_bright;
    float beam_dark, beam_bright, brightness, pad0;
    float gamma_in, gamma_out, k, pad;
};
static_assert(sizeof(Constants) == 64, "the cbuffer's layout");

Constants g_c = {320, 240, 640, 480,
                 0.06f, 0.0f, 0.825f, 0.375f,   // halation, diffusion, scanline strength on dark / bright lines
                 0.55f, 0.85f, 1.25f, 0,       // beam width on dark / bright lines (in lines), brightness
                 2.2f, 2.2f, 2, 0};

struct Knob {
    const char* name;
    float* value;
};
const Knob kKnobs[] = {
    {"halation", &g_c.halation},     {"diffusion", &g_c.diffusion},     {"scan_dark", &g_c.scan_dark},
    {"scan_bright", &g_c.scan_bright}, {"beam_dark", &g_c.beam_dark},   {"beam_bright", &g_c.beam_bright},
    {"brightness", &g_c.brightness},   {"gamma_in", &g_c.gamma_in},
    {"gamma_out", &g_c.gamma_out},
};

ID3D11VertexShader* g_vs;
ID3D11PixelShader* g_shrink;
ID3D11PixelShader* g_across;
ID3D11PixelShader* g_down;
ID3D11PixelShader* g_picture;
ID3D11Buffer* g_cb;
ID3D11SamplerState* g_linear;
ID3D11Texture2D* g_tex[2];
ID3D11RenderTargetView* g_rtv[2];
ID3D11ShaderResourceView* g_srv[2];

void Check(HRESULT hr, const char* what) {
    if (FAILED(hr)) bof3::Fatal("render: CRT %s failed, HRESULT 0x%08lX", what, static_cast<unsigned long>(hr));
}

ID3DBlob* Compile(const char* entry, const char* profile) {
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT hr = D3DCompile(kCrtShader, sizeof kCrtShader - 1, "crt", nullptr, nullptr, entry, profile,
                                  D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(hr))
        bof3::Fatal("render: CRT shader %s failed to compile: %s", entry,
                    errors ? static_cast<const char*>(errors->GetBufferPointer()) : "(no message)");
    if (errors) errors->Release();
    return code;
}

ID3D11PixelShader* Pixel(ID3D11Device* device, const char* entry) {
    ID3DBlob* code = Compile(entry, "ps_4_0");
    ID3D11PixelShader* out = nullptr;
    Check(device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &out), entry);
    code->Release();
    return out;
}

// BOF3X_CRT: "halation=0.1,brightness=1.2" - unknown names and bad numbers are Fatal,
// so a typo is not a silent default.
void ReadKnobs() {
    char text[512];
    const DWORD n = GetEnvironmentVariableA("BOF3X_CRT", text, sizeof text);
    if (n == 0) return;
    if (n >= sizeof text) bof3::Fatal("BOF3X_CRT: longer than %u bytes", static_cast<unsigned>(sizeof text));
    for (char* item = std::strtok(text, ","); item; item = std::strtok(nullptr, ",")) {
        char* eq = std::strchr(item, '=');
        if (!eq) bof3::Fatal("BOF3X_CRT: '%s' is not name=value", item);
        *eq = 0;
        char* end = nullptr;
        const float v = std::strtof(eq + 1, &end);
        if (end == eq + 1 || *end) bof3::Fatal("BOF3X_CRT: %s=%s is not a number", item, eq + 1);
        bool found = false;
        for (const Knob& knob : kKnobs)
            if (std::strcmp(knob.name, item) == 0) *knob.value = v, found = true;
        if (!found) bof3::Fatal("BOF3X_CRT: no setting called '%s'", item);
    }
}

void Pass(ID3D11DeviceContext* ctx, ID3D11PixelShader* ps, ID3D11RenderTargetView* out, const D3D11_VIEWPORT& vp,
          ID3D11ShaderResourceView* t0, ID3D11ShaderResourceView* t1) {
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx->PSSetShaderResources(0, 2, none);
    ctx->OMSetRenderTargets(1, &out, nullptr);
    ctx->RSSetViewports(1, &vp);
    ID3D11ShaderResourceView* in[2] = {t0, t1};
    ctx->PSSetShaderResources(0, 2, in);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->Draw(4, 0);
}

}  // namespace

bool CrtWanted() {
    char text[16];
    const DWORD n = GetEnvironmentVariableA("BOF3X_PRESENT", text, sizeof text);
    if (n == 0) return false;
    if (std::strcmp(text, "crt") == 0) return true;
    if (std::strcmp(text, "clean") == 0) return false;
    bof3::Fatal("BOF3X_PRESENT=%s: crt or clean", text);
}

void CrtInit(ID3D11Device* device, U target_w, U target_h) {
    ReadKnobs();
    g_c.target_w = static_cast<float>(target_w);
    g_c.target_h = static_cast<float>(target_h);
    g_c.k = static_cast<float>(target_w / 320);

    ID3DBlob* vs = Compile("VS", "vs_4_0");
    Check(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vs), "VS");
    vs->Release();
    g_shrink = Pixel(device, "Shrink");
    g_across = Pixel(device, "BlurAcross");
    g_down = Pixel(device, "BlurDown");
    g_picture = Pixel(device, "Picture");

    D3D11_BUFFER_DESC b = {};
    b.ByteWidth = sizeof(Constants);
    b.Usage = D3D11_USAGE_IMMUTABLE;
    b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA init = {&g_c, 0, 0};
    Check(device->CreateBuffer(&b, &init, &g_cb), "constants");

    D3D11_SAMPLER_DESC s = {};
    s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    s.MaxAnisotropy = 1;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    s.MaxLOD = D3D11_FLOAT32_MAX;
    Check(device->CreateSamplerState(&s, &g_linear), "sampler");

    for (int i = 0; i < 2; ++i) {
        D3D11_TEXTURE2D_DESC d = {};
        d.Width = 320;
        d.Height = 240;
        d.MipLevels = 1;
        d.ArraySize = 1;
        d.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        Check(device->CreateTexture2D(&d, nullptr, &g_tex[i]), "glow texture");
        Check(device->CreateRenderTargetView(g_tex[i], nullptr, &g_rtv[i]), "glow target");
        Check(device->CreateShaderResourceView(g_tex[i], nullptr, &g_srv[i]), "glow view");
    }
    bof3::Log("DIV-0037    CRT look: halation %.3f, diffusion %.3f, scanlines %.2f..%.2f, beam %.2f..%.2f, "
              "brightness %.2f, gamma %.2f / %.2f, target %u x %u",
              g_c.halation, g_c.diffusion, g_c.scan_dark, g_c.scan_bright, g_c.beam_dark, g_c.beam_bright,
              g_c.brightness, g_c.gamma_in, g_c.gamma_out, target_w, target_h);
}

void CrtDraw(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* target, ID3D11RenderTargetView* window,
             const D3D11_VIEWPORT& picture) {
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(g_vs, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &g_cb);
    ctx->PSSetSamplers(0, 1, &g_linear);
    const D3D11_VIEWPORT source = {0, 0, 320, 240, 0, 1};
    Pass(ctx, g_shrink, g_rtv[0], source, target, nullptr);
    Pass(ctx, g_across, g_rtv[1], source, nullptr, g_srv[0]);
    Pass(ctx, g_down, g_rtv[0], source, nullptr, g_srv[1]);
    Pass(ctx, g_picture, window, picture, target, g_srv[0]);
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx->PSSetShaderResources(0, 2, none);
}

}  // namespace render
