// A probe for the native Linux build (docs/linux-round.md section 6, call 1):
// does DXVK-native carry the renderer's Direct3D 11 path on Linux, with an
// SDL3 window and the build's own shader bytecode? Not a player file.
//
// It opens an SDL3 window, makes the device and swap chain the way
// render_d3d11.cpp's InitOnFiber does (B8G8R8A8, one buffer, DISCARD), creates
// the present shaders from src/render/shaders_compiled.h (Windows' compiler's
// DXBC), draws a 320 x 240 test picture to the 640 x 480 back buffer with the
// point sampler, reads the back buffer back and checks every pixel against
// the texel it doubles (alpha is not compared: the picture is opaque). Then it
// presents frames for a few seconds, with no vsync, and says how many a
// second. Exit 0 when every pixel matches, 1 when some differ, 2 when a setup
// call fails.
//
// Build (Linux, DXVK built native with -Dnative_sdl3=enabled into DXVK):
//   clang++ -std=c++20 -O2 -I src -I $DXVK/include/dxvk tools/linux_probe/dxvk_probe.cpp \
//     -L $DXVK/lib -ldxvk_d3d11 -ldxvk_dxgi $(pkg-config --cflags --libs sdl3) -Wl,-rpath,$DXVK/lib
//   DXVK_WSI_DRIVER=SDL3 ./a.out [out.bmp]   (the variable picks DXVK's SDL3 window layer;
//                                              out.bmp, optional, is the read-back frame)
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <d3d11.h>
#include <dxgi.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "render/shaders_compiled.h"

namespace {

constexpr unsigned kW = 320, kH = 240, kScale = 2;

// Reports a failed setup call; the value is main's exit code for it.
int Fail(const char* what, HRESULT hr) {
    std::fprintf(stderr, "dxvk_probe: %s failed, HRESULT 0x%08X\n", what, static_cast<unsigned>(hr));
    return 2;
}

std::uint32_t Texel(unsigned x, unsigned y) {   // 0xAARRGGBB, as B8G8R8A8 holds it little-endian
    const std::uint32_t r = x & 0xFF, g = (y * 255) / (kH - 1), b = ((x / 8) ^ (y / 8)) & 1 ? 0xFF : 0x20;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

void WriteBmp(const char* path, const std::vector<std::uint32_t>& px, unsigned w, unsigned h) {
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return;
    const std::uint32_t data = w * h * 4, size = 54 + data;
    std::uint8_t hd[54] = {'B', 'M'};
    auto put = [&](int at, std::uint32_t v) { for (int i = 0; i < 4; ++i) hd[at + i] = static_cast<std::uint8_t>(v >> (8 * i)); };
    put(2, size); put(10, 54); put(14, 40); put(18, w); put(22, static_cast<std::uint32_t>(-static_cast<int>(h)));
    hd[26] = 1; hd[28] = 32; put(34, data);
    std::fwrite(hd, 1, 54, f);
    std::fwrite(px.data(), 4, px.size(), f);
    std::fclose(f);
}

}  // namespace

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) { std::fprintf(stderr, "dxvk_probe: SDL_Init: %s\n", SDL_GetError()); return 2; }
    SDL_Window* win = SDL_CreateWindow("dxvk_probe", kW * kScale, kH * kScale, SDL_WINDOW_VULKAN);
    if (!win) { std::fprintf(stderr, "dxvk_probe: SDL_CreateWindow: %s\n", SDL_GetError()); return 2; }

    DXGI_SWAP_CHAIN_DESC sd = {};
    sd.BufferCount = 1;
    sd.BufferDesc.Width = kW * kScale;
    sd.BufferDesc.Height = kH * kScale;
    sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = reinterpret_cast<HWND>(win);   // DXVK-native: the window handle is the SDL_Window*
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain* swap = nullptr;
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                                               levels, 3, D3D11_SDK_VERSION, &sd, &swap, &dev, &got, &ctx);
    if (FAILED(hr)) return Fail("D3D11CreateDeviceAndSwapChain", hr);
    std::printf("device: feature level 0x%X\n", static_cast<unsigned>(got));

    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    if (FAILED(hr = dev->CreateVertexShader(bof3::shaders::kPresentVS, sizeof bof3::shaders::kPresentVS, nullptr, &vs)))
        return Fail("CreateVertexShader (the build's present VS)", hr);
    if (FAILED(hr = dev->CreatePixelShader(bof3::shaders::kPresentPS, sizeof bof3::shaders::kPresentPS, nullptr, &ps)))
        return Fail("CreatePixelShader (the build's present PS)", hr);

    std::vector<std::uint32_t> pic(kW * kH);
    for (unsigned y = 0; y < kH; ++y)
        for (unsigned x = 0; x < kW; ++x) pic[y * kW + x] = Texel(x, y);
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = kW; td.Height = kH; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT; td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    const D3D11_SUBRESOURCE_DATA init = {pic.data(), kW * 4, 0};
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* srv = nullptr;
    if (FAILED(hr = dev->CreateTexture2D(&td, &init, &tex))) return Fail("CreateTexture2D", hr);
    if (FAILED(hr = dev->CreateShaderResourceView(tex, nullptr, &srv))) return Fail("CreateShaderResourceView", hr);
    D3D11_SAMPLER_DESC smp = {};
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    ID3D11SamplerState* sampler = nullptr;
    if (FAILED(hr = dev->CreateSamplerState(&smp, &sampler))) return Fail("CreateSamplerState", hr);

    ID3D11Texture2D* back = nullptr;
    ID3D11RenderTargetView* rtv = nullptr;
    if (FAILED(hr = swap->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back)))) return Fail("GetBuffer", hr);
    if (FAILED(hr = dev->CreateRenderTargetView(back, nullptr, &rtv))) return Fail("CreateRenderTargetView", hr);

    auto draw = [&] {
        const float black[4] = {0, 0, 0, 1};
        ctx->ClearRenderTargetView(rtv, black);
        ctx->OMSetRenderTargets(1, &rtv, nullptr);
        const D3D11_VIEWPORT vp = {0, 0, float(kW * kScale), float(kH * kScale), 0, 1};
        ctx->RSSetViewports(1, &vp);
        ctx->IASetInputLayout(nullptr);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);
        ctx->PSSetShaderResources(0, 1, &srv);
        ctx->PSSetSamplers(0, 1, &sampler);
        ctx->Draw(4, 0);
    };

    // The check: one frame, read back before the present.
    draw();
    D3D11_TEXTURE2D_DESC bd;
    back->GetDesc(&bd);
    bd.Usage = D3D11_USAGE_STAGING; bd.BindFlags = 0; bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ; bd.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    if (FAILED(hr = dev->CreateTexture2D(&bd, nullptr, &staging))) return Fail("CreateTexture2D (staging)", hr);
    ctx->CopyResource(staging, back);
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(hr = ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m))) return Fail("Map", hr);
    std::vector<std::uint32_t> out(bd.Width * bd.Height);
    unsigned bad = 0;
    for (unsigned y = 0; y < bd.Height; ++y) {
        const auto* row = reinterpret_cast<const std::uint32_t*>(static_cast<const std::uint8_t*>(m.pData) + y * m.RowPitch);
        for (unsigned x = 0; x < bd.Width; ++x) {
            out[y * bd.Width + x] = row[x];
            if ((row[x] | 0xFF000000u) != Texel(x / kScale, y / kScale)) ++bad;
        }
    }
    ctx->Unmap(staging, 0);
    std::printf("readback: %u x %u, %u of %u pixels differ from the doubled picture\n", bd.Width, bd.Height, bad,
                bd.Width * bd.Height);
    if (argc > 1) WriteBmp(argv[1], out, bd.Width, bd.Height);
    if (FAILED(hr = swap->Present(0, 0))) return Fail("Present", hr);

    // The pace: frames for about three seconds, no vsync.
    const auto t0 = std::chrono::steady_clock::now();
    unsigned frames = 0;
    for (;;) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {}
        draw();
        if (FAILED(hr = swap->Present(0, 0))) return Fail("Present (the run)", hr);
        ++frames;
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        if (s >= 3.0) { std::printf("presents: %u in %.2f s, %.0f a second\n", frames, s, frames / s); break; }
    }
    return bad == 0 ? 0 : 1;
}
