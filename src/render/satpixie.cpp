// The SatPixie CRT look (satpixie.h, DIV-0043, docs/crt-look.md section 7).
//
// A port of github.com/Conkwer/satpixie-crt-shader's RetroArch preset
// (satpixie-crt.slangp: accumulate.slang, blur_horiz.slang, blur_vert.slang,
// satpixie-crt.slang) to HLSL 4.0, pass for pass and line for line where the
// languages let it; the arithmetic is the preset's. That preset is Conkwer's
// fork of Mattias Gustavsson's "newpixie" CRT, "adapted for slang by hunterk";
// both are offered under the MIT licence or as public domain, and the notice
// is kept in docs/THIRD_PARTY.md. Differences from the preset, all
// deliberate: the source is the render target at its scale k (the preset
// runs its first pass at twice the game's resolution); "OutputSize" for the
// picture pass is the picture's rectangle on the window; the parameters are
// read from BOF3X_SATPIXIE; overscan_crop defaults to 0 here, since the
// game's own UI runs to the picture's edge; and vignette_aspect defaults to
// 0, the vignette over the whole picture, because the preset's 4:3 shape
// darkens the middle of a wide picture (DIV-0041) and leaves its bands
// bright - the owner saw exactly that on the title's mural.
#include "render/satpixie.h"

#include <windows.h>
#include <d3dcompiler.h>

#include <cstdlib>
#include <cstring>

#include "hook/log.h"

namespace render {
namespace {

const char kShader[] = R"(
cbuffer Params : register(b0) {
    float2 source_size; float2 output_size;
    float frame_count; float acc_modulate; float blur_x; float blur_y;
    float natural_vision; float gamma; float ghosting_on; float chroma_on;
    float chroma_strength; float vignette_on; float vignette_aspect; float wiggle_toggle;
    float scanroll; float overscan_crop; float shadow_mask; float pad0;
};
Texture2D tex0 : register(t0);
Texture2D tex1 : register(t1);
SamplerState lin : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V VS(uint id : SV_VertexID) {
    V o;
    float2 uv = float2(id & 1u, id >> 1u);
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    o.uv = uv;
    return o;
}

// accumulate.slang: tex0 the previous frame's horizontal blur (PassFeedback1), tex1 the source.
float4 Accumulate(V i) : SV_Target {
    float4 a = tex0.Sample(lin, i.uv) * acc_modulate;
    float4 b = tex1.Sample(lin, i.uv);
    return max(a, b * 0.96);
}

// blur_horiz.slang / blur_vert.slang: a 9-tap Gaussian along `blur` (in texels of the output).
float4 Blur9(float2 uv, float2 blur) {
    float4 sum = tex0.Sample(lin, uv) * 0.2270270270;
    sum += tex0.Sample(lin, uv - 4.0 * blur) * 0.0162162162;
    sum += tex0.Sample(lin, uv - 3.0 * blur) * 0.0540540541;
    sum += tex0.Sample(lin, uv - 2.0 * blur) * 0.1216216216;
    sum += tex0.Sample(lin, uv - 1.0 * blur) * 0.1945945946;
    sum += tex0.Sample(lin, uv + 1.0 * blur) * 0.1945945946;
    sum += tex0.Sample(lin, uv + 2.0 * blur) * 0.1216216216;
    sum += tex0.Sample(lin, uv + 3.0 * blur) * 0.0540540541;
    sum += tex0.Sample(lin, uv + 4.0 * blur) * 0.0162162162;
    return sum;
}
float4 BlurHoriz(V i) : SV_Target { return Blur9(i.uv, float2(blur_x, 0.0) / source_size); }
float4 BlurVert(V i) : SV_Target { return Blur9(i.uv, float2(0.0, blur_y) / source_size); }

// satpixie-crt.slang: tex0 is blur2 (the preset's main colour source).
float3 tsample(float2 tc) {
    if (overscan_crop > 0.5) tc = tc * float2(1.025, 0.92) + float2(-0.0125, 0.04);
    float3 s = pow(abs(tex0.Sample(lin, float2(tc.x, 1.0 - tc.y)).rgb), 2.2);
    return s * 1.25;
}
float3 filmic(float3 c) {
    float3 x = max(0.0, c - 0.004);
    return (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
}
float rand(float2 co) { return frac(sin(dot(co, float2(12.9898, 78.233))) * 43758.5453); }

float4 Picture(V i) : SV_Target {
    float2 resolution = output_size;
    float time = fmod(frame_count, 849.0) * 36.0;
    float2 uv = i.uv;
    uv.y = 1.0 - uv.y;
    float2 scuv = uv;
    float3 col;
    float x = wiggle_toggle * sin(0.1 * time + uv.y * 13.0) * sin(0.23 * time + uv.y * 19.0) *
              sin(0.3 + 0.11 * time + uv.y * 23.0) * 0.0012;
    float o = sin(uv.y * 1.5) / resolution.x;
    x += o * 0.25;
    time = fmod(frame_count, 640.0);
    if (chroma_on > 0.5) {
        float strength = chroma_strength * 0.0015;
        col.r = tsample(float2(x + scuv.x + strength, scuv.y + strength * 0.7)).x + 0.02;
        col.g = tsample(float2(x + scuv.x, scuv.y)).y + 0.02;
        col.b = tsample(float2(x + scuv.x - strength, scuv.y - strength * 0.7)).z + 0.02;
    } else {
        col = tsample(float2(x + scuv.x, scuv.y)) + 0.02;
    }
    float lum = clamp(col.r * 0.299 + col.g * 0.587 + col.b * 0.114, 0.0, 1.0);
    lum = pow(1.0 - pow(lum, 2.0), 1.0);
    lum = (1.0 - lum) * 0.85 + 0.15;
    float ghs = 0.15;
    if (ghosting_on > 0.5) {
        float ghost_chroma = chroma_on > 0.5 ? chroma_strength * 0.001 : 0.0;
        float3 r = tsample(float2(x - 0.014 + ghost_chroma, -0.027 + ghost_chroma * 0.7) * 0.85 +
                           0.007 * float2(0.35 * sin(1.0 / 7.0 + 15.0 * uv.y + 0.9 * time),
                                          0.35 * sin(2.0 / 7.0 + 10.0 * uv.y + 1.37 * time)) +
                           float2(scuv.x + 0.001, scuv.y + 0.001)) * float3(0.5, 0.25, 0.25);
        float3 g = tsample(float2(x - 0.019, -0.020) * 0.85 +
                           0.007 * float2(0.35 * cos(1.0 / 9.0 + 15.0 * uv.y + 0.5 * time),
                                          0.35 * sin(2.0 / 9.0 + 10.0 * uv.y + 1.50 * time)) +
                           float2(scuv.x + 0.000, scuv.y - 0.002)) * float3(0.25, 0.5, 0.25);
        float3 b = tsample(float2(x - 0.017 - ghost_chroma, -0.003 - ghost_chroma * 0.7) * 0.85 +
                           0.007 * float2(0.35 * sin(2.0 / 3.0 + 15.0 * uv.y + 0.7 * time),
                                          0.35 * cos(2.0 / 3.0 + 10.0 * uv.y + 1.63 * time)) +
                           float2(scuv.x - 0.002, scuv.y + 0.000)) * float3(0.25, 0.25, 0.5);
        col += (ghs * (1.0 - 0.299)) * pow(clamp(3.0 * r, 0.0, 1.0), 2.0) * lum;
        col += (ghs * (1.0 - 0.587)) * pow(clamp(3.0 * g, 0.0, 1.0), 2.0) * lum;
        col += (ghs * (1.0 - 0.114)) * pow(clamp(3.0 * b, 0.0, 1.0), 2.0) * lum;
    }
    if (natural_vision > 0.5) col = pow(col, gamma / 2.2);
    else col *= float3(0.95, 1.05, 0.95);
    col = clamp(col * 1.3 + 0.75 * col * col + 1.25 * col * col * col * col * col, 0.0, 10.0);
    if (vignette_on > 0.5) {
        float vignette = 1.0;
        float2 vuv = scuv;
        if (vignette_aspect > 0.5) {
            float aspect = resolution.x / resolution.y;
            float targetAspect = 4.0 / 2.99;
            float scale = aspect / targetAspect;
            float border = (1.0 - (1.0 / scale)) * 0.5;
            if (vuv.x > border && vuv.x < (1.0 - border)) {
                float vx = (vuv.x - border) / (1.0 - 2.0 * border);
                float vy = vuv.y;
                float vig = 16.0 * vx * vy * (1.0 - vx) * (1.0 - vy);
                vignette = 1.3 * pow(0.1 + vig, 0.5);
            }
        } else {
            float vig = 16.0 * vuv.x * vuv.y * (1.0 - vuv.x) * (1.0 - vuv.y);
            vignette = 1.3 * pow(0.1 + vig, 0.5);
        }
        col *= vignette;
    }
    time *= scanroll;
    float scans = clamp(0.35 + 0.18 * sin(6.0 * time - scuv.y * resolution.y * 1.5), 0.0, 1.0);
    float s = pow(scans, 0.9);
    col = col * s;
    if (shadow_mask > 0.5) {
        float maskU = i.uv.x;
        float warpedPx = maskU * resolution.x;
        float stripeWidth = 3.0;
        float idx = warpedPx / stripeWidth;
        float center = frac(idx) - 0.5;
        float aa = fwidth(idx);
        float maskLine = clamp(1.0 - abs(center) / aa, 0.0, 1.0);
        if (shadow_mask < 1.5) {
            col *= lerp(1.1, 0.8, maskLine);
        } else {
            float stripePhase = frac(idx);
            float3 phaseOffsets = float3(0.0, 1.0 / 3.0, 2.0 / 3.0);
            float3 rawDistance = abs(stripePhase - phaseOffsets);
            float3 circDistance = min(rawDistance, 1.0 - rawDistance);
            float3 maskLineRGB = clamp(1.0 - circDistance / aa, 0.0, 1.0);
            col *= lerp(0.75, 1.5, maskLineRGB);
        }
    }
    col = filmic(col);
    float2 seed = scuv * resolution;
    col -= 0.015 * pow(float3(rand(seed + time), rand(seed + time * 2.0), rand(seed + time * 3.0)), 1.5);
    col *= (1.0 - 0.004 * (sin(50.0 * time + uv.y * 2.0) * 0.5 + 0.5));
    return float4(col, 1.0);
}
)";

// The parameters, the preset's names and defaults (overscan_crop 0 here).
struct Params {
    float source_w, source_h, output_w, output_h;
    float frame_count, acc_modulate, blur_x, blur_y;
    float natural_vision, gamma, ghosting_on, chroma_on;
    float chroma_strength, vignette_on, vignette_aspect, wiggle_toggle;
    float scanroll, overscan_crop, shadow_mask, pad0;
};
static_assert(sizeof(Params) == 80, "the cbuffer's layout");

Params g_p = {1, 1, 1, 1,
              0, 0.65f, 0.0f, 0.0f,
              1, 2.3f, 0, 1,
              0.7f, 1, 0, 0,   // vignette_aspect 0: the whole picture (the preset's 1 shapes it for 4:3 and leaves DIV-0041's bands bright)
              1, 0, 0, 0};

struct Knob {
    const char* name;
    float* value;
    float lo, hi;
};
const Knob kKnobs[] = {
    {"acc_modulate", &g_p.acc_modulate, 0, 1},      {"blur_x", &g_p.blur_x, 0, 5},
    {"blur_y", &g_p.blur_y, 0, 5},                  {"natural_vision", &g_p.natural_vision, 0, 1},
    {"gamma", &g_p.gamma, 1.8f, 2.6f},              {"ghosting_on", &g_p.ghosting_on, 0, 1},
    {"chroma_on", &g_p.chroma_on, 0, 1},            {"chroma_strength", &g_p.chroma_strength, 0, 5},
    {"vignette_on", &g_p.vignette_on, 0, 1},        {"vignette_aspect", &g_p.vignette_aspect, 0, 1},
    {"wiggle_toggle", &g_p.wiggle_toggle, 0, 1},    {"scanroll", &g_p.scanroll, 0, 1},
    {"overscan_crop", &g_p.overscan_crop, 0, 1},    {"shadow_mask", &g_p.shadow_mask, 0, 2},
};

ID3D11VertexShader* g_vs;
ID3D11PixelShader* g_accumulate;
ID3D11PixelShader* g_blur_h;
ID3D11PixelShader* g_blur_v;
ID3D11PixelShader* g_picture;
ID3D11Buffer* g_cb;
ID3D11SamplerState* g_linear;
// accum (this frame), blur1[2] (this frame's and the previous frame's
// horizontal blur - the accumulate pass feeds on the previous), blur2.
enum { kAccum, kBlur1A, kBlur1B, kBlur2, kTextures };
ID3D11Texture2D* g_tex[kTextures];
ID3D11RenderTargetView* g_rtv[kTextures];
ID3D11ShaderResourceView* g_srv[kTextures];
unsigned g_flip;   // which of blur1A / blur1B is this frame's
U g_w, g_h;

void Check(HRESULT hr, const char* what) {
    if (FAILED(hr)) bof3::Fatal("render: SatPixie %s failed, HRESULT 0x%08lX", what, static_cast<unsigned long>(hr));
}

ID3DBlob* Compile(const char* entry, const char* profile) {
    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    const HRESULT hr = D3DCompile(kShader, sizeof kShader - 1, "satpixie", nullptr, nullptr, entry, profile,
                                  D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (FAILED(hr))
        bof3::Fatal("render: SatPixie shader %s failed to compile: %s", entry,
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

// BOF3X_SATPIXIE: "acc_modulate=0.5,chroma_on=0" - unknown names, bad numbers
// and values outside the preset's range are Fatal.
void ReadKnobs() {
    char text[1024];
    const DWORD n = GetEnvironmentVariableA("BOF3X_SATPIXIE", text, sizeof text);
    if (n == 0) return;
    if (n >= sizeof text) bof3::Fatal("BOF3X_SATPIXIE: longer than %u bytes", static_cast<unsigned>(sizeof text));
    for (char* item = std::strtok(text, ","); item; item = std::strtok(nullptr, ",")) {
        char* eq = std::strchr(item, '=');
        if (!eq) bof3::Fatal("BOF3X_SATPIXIE: '%s' is not name=value", item);
        *eq = 0;
        char* end = nullptr;
        const float v = std::strtof(eq + 1, &end);
        if (end == eq + 1 || *end) bof3::Fatal("BOF3X_SATPIXIE: %s=%s is not a number", item, eq + 1);
        bool found = false;
        for (const Knob& knob : kKnobs) {
            if (std::strcmp(knob.name, item) != 0) continue;
            if (v < knob.lo || v > knob.hi) bof3::Fatal("BOF3X_SATPIXIE: %s=%g is outside %g..%g", item, v, knob.lo, knob.hi);
            *knob.value = v;
            found = true;
        }
        if (!found) bof3::Fatal("BOF3X_SATPIXIE: no setting called '%s'", item);
    }
}

void MakeSized(ID3D11Device* device, U w, U h) {
    for (int i = 0; i < kTextures; ++i) {
        if (g_srv[i]) { g_srv[i]->Release(); g_srv[i] = nullptr; }
        if (g_rtv[i]) { g_rtv[i]->Release(); g_rtv[i] = nullptr; }
        if (g_tex[i]) { g_tex[i]->Release(); g_tex[i] = nullptr; }
        D3D11_TEXTURE2D_DESC d = {};
        d.Width = w;
        d.Height = h;
        d.MipLevels = 1;
        d.ArraySize = 1;
        d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        d.SampleDesc.Count = 1;
        d.Usage = D3D11_USAGE_DEFAULT;
        d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        Check(device->CreateTexture2D(&d, nullptr, &g_tex[i]), "texture");
        Check(device->CreateRenderTargetView(g_tex[i], nullptr, &g_rtv[i]), "target");
        Check(device->CreateShaderResourceView(g_tex[i], nullptr, &g_srv[i]), "view");
    }
    g_w = w;
    g_h = h;
    g_p.source_w = static_cast<float>(w);
    g_p.source_h = static_cast<float>(h);
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

void PutParams(ID3D11DeviceContext* ctx) {
    D3D11_MAPPED_SUBRESOURCE m;
    Check(ctx->Map(g_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m), "Map (params)");
    std::memcpy(m.pData, &g_p, sizeof g_p);
    ctx->Unmap(g_cb, 0);
}

}  // namespace

void SatpixieResize(ID3D11Device* device, U target_w, U target_h, U) { MakeSized(device, target_w, target_h); }

void SatpixieInit(ID3D11Device* device, U target_w, U target_h, U) {
    ReadKnobs();
    ID3DBlob* vs = Compile("VS", "vs_4_0");
    Check(device->CreateVertexShader(vs->GetBufferPointer(), vs->GetBufferSize(), nullptr, &g_vs), "VS");
    vs->Release();
    g_accumulate = Pixel(device, "Accumulate");
    g_blur_h = Pixel(device, "BlurHoriz");
    g_blur_v = Pixel(device, "BlurVert");
    g_picture = Pixel(device, "Picture");

    D3D11_BUFFER_DESC b = {};
    b.ByteWidth = sizeof(Params);
    b.Usage = D3D11_USAGE_DYNAMIC;
    b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    b.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Check(device->CreateBuffer(&b, nullptr, &g_cb), "params");

    D3D11_SAMPLER_DESC s = {};
    s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    s.MaxAnisotropy = 1;
    s.ComparisonFunc = D3D11_COMPARISON_NEVER;
    s.MaxLOD = D3D11_FLOAT32_MAX;
    Check(device->CreateSamplerState(&s, &g_linear), "sampler");
    MakeSized(device, target_w, target_h);
    bof3::Log("DIV-0043    SatPixie look: modulate %.2f, blur %.2f / %.2f, natural %g, gamma %.2f, ghosting %g, chroma %g x %.2f, "
              "vignette %g (aspect %g), interference %g, rolling %g, overscan %g, mask %g; %u x %u",
              g_p.acc_modulate, g_p.blur_x, g_p.blur_y, g_p.natural_vision, g_p.gamma, g_p.ghosting_on, g_p.chroma_on,
              g_p.chroma_strength, g_p.vignette_on, g_p.vignette_aspect, g_p.wiggle_toggle, g_p.scanroll, g_p.overscan_crop,
              g_p.shadow_mask, target_w, target_h);
}

void SatpixieDraw(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* target, ID3D11RenderTargetView* window,
                  const D3D11_VIEWPORT& picture, unsigned frame) {
    g_p.frame_count = static_cast<float>(frame);
    g_p.output_w = picture.Width;
    g_p.output_h = picture.Height;
    PutParams(ctx);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->VSSetShader(g_vs, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &g_cb);
    ctx->PSSetSamplers(0, 1, &g_linear);
    const D3D11_VIEWPORT full = {0, 0, static_cast<float>(g_w), static_cast<float>(g_h), 0, 1};
    const unsigned now = kBlur1A + (g_flip & 1), prev = kBlur1A + ((g_flip + 1) & 1);
    Pass(ctx, g_accumulate, g_rtv[kAccum], full, g_srv[prev], target);
    Pass(ctx, g_blur_h, g_rtv[now], full, g_srv[kAccum], nullptr);
    Pass(ctx, g_blur_v, g_rtv[kBlur2], full, g_srv[now], nullptr);
    Pass(ctx, g_picture, window, picture, g_srv[kBlur2], nullptr);
    g_flip ^= 1;
    ID3D11ShaderResourceView* none[2] = {nullptr, nullptr};
    ctx->PSSetShaderResources(0, 2, none);
}

}  // namespace render
