// The PSX library's display calls as the port implements them over DirectDraw
// and Direct3D, and the sound bank load - group S of the fifth takeover round
// (docs/takeover-queue-round5.md). Each function read to its last instruction
// with capstone against bof3/BOF3.exe, 2026-09-23 (docs/display-env.md):
//
//   Gpu_PutDispEnv    0x5A7860  0x26    Gfx_Present       0x59EDF0  0x60
//   Gpu_PutDrawEnv    0x5A7890  0x4D    Gfx_ClearPresent  0x59ECE0  0x107
//   Gpu_SetDefDispEnv 0x5A78E0  0x28    D3d_SetBackColor  0x5A5050  0xE0
//   Gpu_SetDefDrawEnv 0x5A7910  0x4A    Snd_LoadBank      0x587CD0  0xDE
//
// Faithful: no divergence. Every call out goes through display_env::g, so the
// start-up fuzz can stand recorders in for the callees, and every COM call
// through the object's vtable as the original makes it.
#include "game/display_env.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/display_env_callees.h"
#include "hook/detour.h"

namespace display_env {

const Callees kOriginals = {
    Gfx_Present, Gfx_ClearPresent, D3d_SetBackColor, Crt_malloc, Crt_free, SndBuf_Release, SndBuf_FromWave,
};
Callees g = kOriginals;

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const volatile void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
U Word(U address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutWord(U address, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
void* Ptr(U address) { return At(Long(address)); }
float Float(U address) {
    float v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}

// `rep movsd`: n dwords, first to last, each read just before it is stored -
// so a source that overlaps the destination from below repeats itself, as the
// original's does.
void CopyDwords(U dst, U src, U n) {
    for (U i = 0; i < n; ++i) PutLong(dst + i * 4, Long(src + i * 4));
}

// The back buffer cleared: Gfx_RenderFlags bit 0 (the software surfaces) a
// colour fill of 0 through Blt, whatever the draw environment's colour;
// otherwise the viewport's Clear of Gfx_ScreenRect to its background - the
// material D3d_SetBackColor last set. The DDBLTFX is the original's stack
// block: 0x64 bytes zeroed, dwSize 0x64, dwFillColor (+0x50) 0 again.
void ClearBack() {
    if (At(kRenderFlags)[0] & 1) {
        unsigned char fx[kBltFxBytes];
        std::memset(fx, 0, sizeof fx);
        const U size = kBltFxBytes;
        std::memcpy(fx, &size, sizeof size);
        void* const back = Ptr(kBackBuffer);
        Method<ComBlt>(back, kBlt)(back, nullptr, nullptr, nullptr, kBltWait | kBltColorFill, fx);
    } else {
        void* const viewport = Ptr(kViewport);
        Method<ComClear>(viewport, kClear)(viewport, 1, At(kScreenRect), kClearTarget);
    }
}

}  // namespace
}  // namespace display_env

using namespace display_env;

// 0x5A7860, PutDispEnv by shape; called once a rendered frame by WinMain
// (0x4FCE46) with the current buffer's DISPENV. When the display's y - the
// word at +2 - differs from the last one put, the frame is shown
// (Gfx_Present); then the 0x14 bytes are kept at Gpu_DispEnv, after the
// present, so a present that changed them is copied over. Only y is compared:
// the double buffer's two environments differ in nothing else (0x4FD110).
// The eax the original leaves is not read by its caller.
void Gpu_PutDispEnv(const unsigned char* env) {
    if (Word(kDispEnv + 2) != Word(Addr(env) + 2)) g.present();
    CopyDwords(kDispEnv, Addr(env), 5);
}

// 0x5A7890, PutDrawEnv by shape; once a rendered frame from WinMain
// (0x4FCE55), the current buffer's DRAWENV. When any of the clear colour's
// three bytes +0x19..+0x1B differs from the last one put - compared in that
// order, stopping at the first difference - D3d_SetBackColor(r, g, b); then
// the 0x5C bytes to Gpu_DrawEnv. That copy is what resets Gfx_TexCacheKey,
// Gfx_DrawTpage, 0x7DED16 and Gfx_DrawEnable (+0x0C..+0x17) every frame, and
// what puts the isbg byte where Gfx_Present reads it. The original pushes
// the three bytes in dwords whose upper bits are its caller's registers;
// D3d_SetBackColor masks them off (0x5A5084, 0x5A5089, 0x5A509F), so ours
// passes them zero-extended.
void Gpu_PutDrawEnv(const unsigned char* env) {
    const U e = Addr(env);
    const U r = At(e + 0x19)[0];
    if (At(kDrawEnv + 0x19)[0] != r || At(kDrawEnv + 0x1A)[0] != At(e + 0x1A)[0] ||
        At(kDrawEnv + 0x1B)[0] != At(e + 0x1B)[0])
        g.set_back_color(r, At(e + 0x1A)[0], At(e + 0x1B)[0]);
    CopyDwords(kDrawEnv, e, 0x17);
}

// 0x5A78E0, SetDefDispEnv by shape; twice from the set-up 0x4FD110. The
// display rect {x, y, w, h} as four words, and nothing else: the PSX
// library's (SLPS 0x8017AF5C) also zeroes the screen rect and the three flag
// bytes, which the port's never reads. Returns env.
unsigned char* Gpu_SetDefDispEnv(unsigned char* env, int x, int y, int w, int h) {
    const U e = Addr(env);
    PutWord(e, static_cast<U>(x));
    PutWord(e + 2, static_cast<U>(y));
    PutWord(e + 4, static_cast<U>(w));
    PutWord(e + 6, static_cast<U>(h));
    return env;
}

// 0x5A7910, SetDefDrawEnv by shape; twice from the set-up. Clip {x, y, w, h};
// the texture window +0x0C {0, 0, 0x100, 0x100}; dtd and dfe 1; the dword at
// +0x18 - isbg and the clear colour - 0. The draw offset +0x08 and the tpage
// +0x14 are NOT written (the PSX library's, SLPS 0x8017AE8C, sets the offset
// to {x, y}, tpage 0x0A and the window to zeros; dfe there depends on the
// height). The stores in the original's order. Returns env.
unsigned char* Gpu_SetDefDrawEnv(unsigned char* env, int x, int y, int w, int h) {
    const U e = Addr(env);
    PutWord(e, static_cast<U>(x));
    PutWord(e + 4, static_cast<U>(w));
    At(e + 0x16)[0] = 1;
    At(e + 0x17)[0] = 1;
    PutWord(e + 2, static_cast<U>(y));
    PutLong(e + 0x18, 0);
    PutWord(e + 0x0C, 0);
    PutWord(e + 0x0E, 0);
    PutWord(e + 6, static_cast<U>(h));
    PutWord(e + 0x10, 0x100);
    PutWord(e + 0x12, 0x100);
    return env;
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
    const U flags = Long(kRenderFlags);
    void* const primary = Ptr(kPrimary);
    long result;
    if (flags & 0x200)
        result = Method<ComBlt>(primary, kBlt)(primary, At(kWindowRect), Ptr(kBackBuffer), At(kScreenRect), kBltWait,
                                               nullptr);
    else
        result = Method<ComFlip>(primary, kFlip)(primary, Ptr(kBackBuffer), kFlipWait);
    if (result == kSurfaceLost) {
        void* const again = Ptr(kPrimary);
        Method<ComCall>(again, kRestore)(again);
    }
    if (At(kDrawEnvBackground)[0] != 0) g.clear_present(1);
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
    const U render = Long(kRenderFlags);
    void* const primary = Ptr(kPrimary);
    if (render & 0x200) {
        Method<ComBlt>(primary, kBlt)(primary, At(kWindowRect), Ptr(kBackBuffer), At(kScreenRect), kBltWait, nullptr);
        return;
    }
    Method<ComFlip>(primary, kFlip)(primary, Ptr(kBackBuffer), kFlipWait);
    ClearBack();
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
    void* const material = Ptr(kBackMaterial);
    if (material == nullptr) return;
    if (Long(kViewport) == 0) return;
    const double k = Float(kInverse255At);
    const float c[3] = {static_cast<float>((r & 0xFF) * k), static_cast<float>((g & 0xFF) * k),
                        static_cast<float>((b & 0xFF) * k)};
    const float one = 1.0f;
    unsigned char m[kMaterialBytes];
    std::memset(m, 0, sizeof m);
    const U size = kMaterialBytes, ramp = 0x20;
    std::memcpy(m, &size, 4);
    std::memcpy(m + 0x04, c, 12);        // diffuse
    std::memcpy(m + 0x10, &one, 4);
    std::memcpy(m + 0x14, c, 12);        // ambient
    std::memcpy(m + 0x20, &one, 4);
    std::memcpy(m + 0x4C, &ramp, 4);     // dwRampSize
    Method<ComPointer>(material, kSetMaterial)(material, m);
    void* const viewport = Ptr(kViewport);
    Method<ComValue>(viewport, kSetBackground)(viewport, Long(kBackHandle));
}

// 0x587CD0; callers LoadDatFile (0x454704, ours) and 0x4547E9, for each
// kind-2 chunk: bank = the chunk's tag. The record is Sound_Banks +
// (bank - 1) * 0x384, in 32 bits - nothing checks the bank (the shipped data
// has 1..6). If the record already holds sample data: Crt_free of it, then
// for each of the 64 voice entries whose buffer is set, SndBuf_Release of it
// and every Sound_Channels dword equal to the entry's buffer - read again
// after the release - cleared; the entries themselves are left. Then the
// payload's first 0x380 bytes over the record's head (a dword copy), and the
// rest - size - 0x380, unsigned, unchecked - to a Crt_malloc buffer that is
// not tested for null, dwords then bytes. Then each voice entry whose first
// dword is non-zero becomes data + offset - 0x380 (the data pointer read
// from the record each time) and its buffer SndBuf_FromWave of that. An
// entry whose offset is 0 keeps the buffer dword the payload's head carried.
void Snd_LoadBank(unsigned bank, const void* payload, unsigned size) {
    const U record = kBanks + (static_cast<U>(bank) - 1u) * kBankBytes;
    const U old = Long(record + kBankHeader);
    if (old != 0) {
        g.free(At(old));
        for (U i = 0; i < 64; ++i) {
            const U entry = record + kVoices + 4 + i * 8;
            const U buffer = Long(entry);
            if (buffer == 0) continue;
            g.release(At(buffer));
            const U now = Long(entry);
            for (U c = kChannels; c < kChannelsEnd; c += 4)
                if (Long(c) == now) PutLong(c, 0);
        }
    }
    const U src = Addr(payload);
    CopyDwords(record, src, kBankHeader / 4);
    const U bytes = static_cast<U>(size) - kBankHeader;
    const U data = Addr(g.malloc(bytes));
    PutLong(record + kBankHeader, data);
    CopyDwords(data, src + kBankHeader, bytes >> 2);
    for (U i = bytes & ~3u; i < bytes; ++i) At(data)[i] = At(src + kBankHeader)[i];
    for (U i = 0; i < 64; ++i) {
        const U entry = record + kVoices + i * 8;
        const U offset = Long(entry);
        if (offset == 0) continue;
        const U sample = Long(record + kBankHeader) + offset - kBankHeader;
        PutLong(entry, sample);
        PutLong(entry + 4, Addr(g.from_wave(At(sample))));
    }
}

void DisplayEnv_Inject() {
    if (bof3::WantsShadow("display_env")) display_env::SelfTest();
    BOF3_INJECT(Gpu_PutDispEnv);
    BOF3_INJECT(Gpu_PutDrawEnv);
    BOF3_INJECT(Gpu_SetDefDispEnv);
    BOF3_INJECT(Gpu_SetDefDrawEnv);
    BOF3_INJECT(Gfx_Present);
    BOF3_INJECT(Gfx_ClearPresent);
    BOF3_INJECT(D3d_SetBackColor);
    BOF3_INJECT(Snd_LoadBank);
}
