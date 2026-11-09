// Moved here by tools/move_funcs.py from display_env.cpp, field_misc.cpp, psx_gpu.cpp, psx_rest.cpp (docs/source-layout-round.md).
#include "game/platform/gpu_2.h"

#include <cstdint>
#include <cstring>
#include <emmintrin.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/d3d_lines.h"
#include "game/display_env.h"
#include "game/display_env_callees.h"
#include "game/field_misc.h"
#include "game/field_misc_callees.h"
#include "game/map_rim.h"
#include "game/psx_gpu.h"
#include "game/psx_rest.h"
#include "game/psx_rest_callees.h"
#include "game/sound_sp.h"
#include "game/sound_st.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace display_env;

namespace {

using namespace field_misc;

}  // namespace

using namespace psx_rest;

namespace display_env {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

U Addr(const volatile void* p) { return Low32(p); }

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

// `rep movsd`: n dwords, first to last, each read just before it is stored -
// so a source that overlaps the destination from below repeats itself, as the
// original's does.
void CopyDwords(U dst, U src, U n) {
    for (U i = 0; i < n; ++i) PutLong(dst + i * 4, Long(src + i * 4));
}

}  // namespace

}  // namespace display_env

namespace {

field_misc::U LongFieldMisc(const unsigned char* p) {
    field_misc::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutLongFieldMisc(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }

constexpr std::uint32_t kLinkFlag = 0x80000000u;

}  // namespace

namespace psx_rest {

namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }

}  // namespace

}  // namespace psx_rest

// original 0x5A7810 (PSX SetDrawMove 0x8017B554): the port's move-image
// primitive - +4 the code dword 0xEC000000 (Gfx_DrawOTag hands it to
// Gfx_MoveImage(prim + 8, +0x10, +0x14)), +8 and +0xC the rect's eight bytes,
// +0x10 x and +0x14 y as whole dwords. In this order, the rect read after the
// code is stored (they may overlap). The PSX packs x and y into one word and
// makes a zero-sized rect a no-op; the PC does neither.
extern "C" unsigned char* __cdecl Gpu_SetDrawMove(unsigned char* prim, const unsigned char* rect, unsigned long x,
                                                  unsigned long y) {
    PutLongFieldMisc(prim + 4, 0xEC000000u);
    PutLongFieldMisc(prim + 8, LongFieldMisc(rect));
    const field_misc::U second = LongFieldMisc(rect + 4);
    PutLongFieldMisc(prim + 0xC, second);
    PutLongFieldMisc(prim + 0x10, static_cast<field_misc::U>(x));
    PutLongFieldMisc(prim + 0x14, static_cast<field_misc::U>(y));
    return prim;
}

// original 0x5A7840. libgpu SetTexWindow(DR_TWIN *, RECT *) as the port has it:
// code 0xF0 in the top byte of +4 (the three bytes below it zero), and at +8
// the RECT's ADDRESS - not the PSX's packed 0xE2 word. Gfx_DrawOTag's case for
// 0xF0 copies the rect from that pointer into Gfx_TexCacheKey
// (docs/d3d-draw.md section 3), so the rect must outlive the frame's draw.
extern "C" void __cdecl Gpu_SetTexWindow(unsigned char* prim, const unsigned char* rect) {
    psx_rest::PutLong(Low32(prim + 4), kTexWindowCode);
    const psx_rest::U address = Low32(rect);
    std::memcpy(prim + 8, &address, 4);
}

// 0x5A7860, PutDispEnv by shape; called once a rendered frame by WinMain
// (0x4FCE46) with the current buffer's DISPENV. When the display's y - the
// word at +2 - differs from the last one put, the frame is shown
// (Gfx_Present); then the 0x14 bytes are kept at Gpu_DispEnv, after the
// present, so a present that changed them is copied over. Only y is compared:
// the double buffer's two environments differ in nothing else (0x4FD110).
// The eax the original leaves is not read by its caller.
void Gpu_PutDispEnv(const unsigned char* env) {
    if (Word(::bof3::Img(kDispEnv.v) + 2) != Word(Addr(env) + 2)) display_env::g.present();
    CopyDwords(::bof3::Img(Gpu_DispEnv_addr), Addr(env), 5);
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
    const display_env::U e = Addr(env);
    const display_env::U r = display_env::At(e + 0x19)[0];
    if (Gpu_DrawEnv[0x19] != r || Gpu_DrawEnv[0x1a] != display_env::At(e + 0x1A)[0] ||
        Gpu_DrawEnv[0x1b] != display_env::At(e + 0x1B)[0])
        display_env::g.set_back_color(r, display_env::At(e + 0x1A)[0], display_env::At(e + 0x1B)[0]);
    CopyDwords(::bof3::Img(Gpu_DrawEnv_addr), e, 0x17);
}

// 0x5A78E0, SetDefDispEnv by shape; twice from the set-up 0x4FD110. The
// display rect {x, y, w, h} as four words, and nothing else: the PSX
// library's (SLPS 0x8017AF5C) also zeroes the screen rect and the three flag
// bytes, which the port's never reads. Returns env.
unsigned char* Gpu_SetDefDispEnv(unsigned char* env, int x, int y, int w, int h) {
    const display_env::U e = Addr(env);
    display_env::PutWord(e, static_cast<display_env::U>(x));
    display_env::PutWord(e + 2, static_cast<display_env::U>(y));
    display_env::PutWord(e + 4, static_cast<display_env::U>(w));
    display_env::PutWord(e + 6, static_cast<display_env::U>(h));
    return env;
}

// 0x5A7910, SetDefDrawEnv by shape; twice from the set-up. Clip {x, y, w, h};
// the texture window +0x0C {0, 0, 0x100, 0x100}; dtd and dfe 1; the dword at
// +0x18 - isbg and the clear colour - 0. The draw offset +0x08 and the tpage
// +0x14 are NOT written (the PSX library's, SLPS 0x8017AE8C, sets the offset
// to {x, y}, tpage 0x0A and the window to zeros; dfe there depends on the
// height). The stores in the original's order. Returns env.
unsigned char* Gpu_SetDefDrawEnv(unsigned char* env, int x, int y, int w, int h) {
    const display_env::U e = Addr(env);
    display_env::PutWord(e, static_cast<display_env::U>(x));
    display_env::PutWord(e + 4, static_cast<display_env::U>(w));
    display_env::At(e + 0x16)[0] = 1;
    display_env::At(e + 0x17)[0] = 1;
    display_env::PutWord(e + 2, static_cast<display_env::U>(y));
    display_env::PutLong(e + 0x18, 0);
    display_env::PutWord(e + 0x0C, 0);
    display_env::PutWord(e + 0x0E, 0);
    display_env::PutWord(e + 6, static_cast<display_env::U>(h));
    display_env::PutWord(e + 0x10, 0x100);
    display_env::PutWord(e + 0x12, 0x100);
    return env;
}

// original 0x5A7960. An ordering table of n entries, each linking to the one
// below it and the lowest to the terminator, every link with bit 31 set.
//
// As the original has it: for n < 2 the terminator goes to ot[n - 1], which
// for n < 1 is outside the table.
extern "C" void __cdecl Gpu_ClearOTagR(unsigned long* ot, int n) {
    std::uintptr_t at = reinterpret_cast<std::uintptr_t>(ot) + static_cast<std::uintptr_t>(n) * 4u - 4u;
    for (int k = n - 1; k > 0; --k) {
        *reinterpret_cast<unsigned long*>(at) = static_cast<unsigned long>(at - 4u) | kLinkFlag;
        at -= 4u;
    }
    *reinterpret_cast<unsigned long*>(at) =
        Low32(&Gpu_OtTerminator) | kLinkFlag;
}

// originals 0x5A79A0, 0x5A79E0: libgpu's getTPage and getClut, bit for bit.
extern "C" unsigned __cdecl Gpu_GetTPage(unsigned tp, unsigned abr, int x, int y) {
    return ((tp & 3u) << 7) | ((abr & 3u) << 5) | ((static_cast<unsigned>(y) & 0x200u) << 2) |
           (static_cast<unsigned>(x >> 6) & 0xFu) | (static_cast<unsigned>(y >> 4) & 0x10u);
}

extern "C" unsigned __cdecl Gpu_GetClut(int x, int y) {
    return (static_cast<unsigned>(x >> 4) & 0x3Fu) | (static_cast<unsigned>(y) << 6);
}

void Gpu2_Inject() {
    BOF3_INJECT(Gpu_SetDrawMove);
    BOF3_INJECT(Gpu_SetTexWindow);
    BOF3_INJECT(Gpu_PutDispEnv);
    BOF3_INJECT(Gpu_PutDrawEnv);
    BOF3_INJECT(Gpu_SetDefDispEnv);
    BOF3_INJECT(Gpu_SetDefDrawEnv);
    BOF3_INJECT(Gpu_ClearOTagR);
    BOF3_INJECT(Gpu_GetTPage);
    BOF3_INJECT(Gpu_GetClut);
}
