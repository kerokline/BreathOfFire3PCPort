// Moved here by tools/move_funcs.py from magic_s25.cpp (docs/source-layout-round.md).
#include "game/magic/spell_depress.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s25.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }

// imul (a 32-bit product that wraps), then sar 0xC.
int Mul(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)); }

int Sar12(int v) { return v >> 12; }

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

// fild dword, fst dword: an int as the single the original stores.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

// The battle bytes these read beyond the harness's names.
// kCasterRecord: now MagicS23_ActorSprite (symbols.toml). a record pointer; its +8 is the facing
constexpr ::bof3::image::Va kSideCorner{0x904AAC};     // low byte: a corner of SpellDepress_Corners

// A child's ping-pong of its +0x64 between -0x80 and 0x80 in steps of 8, +0xB
// the direction (Depress).
void Swing() {
    unsigned char* sc = Sprite_Current;
    const std::int32_t w = static_cast<std::int32_t>(rv::As<records::BattleTask>(sc)->angle[0]);
    if (sc[0xB] == 0) {
        rv::Store<std::int32_t>(rv::As<records::BattleTask>(sc)->angle[0], w + 8);
        sc = Sprite_Current;
        if (static_cast<std::int32_t>(rv::As<records::BattleTask>(sc)->angle[0]) >= 0x80) sc[0xB] = 1;
    } else {
        rv::Store<std::int32_t>(rv::As<records::BattleTask>(sc)->angle[0], w - 8);
        sc = Sprite_Current;
        if (static_cast<std::int32_t>(rv::As<records::BattleTask>(sc)->angle[0]) <= -0x80) sc[0xB] = 0;
    }
}

}  // namespace

#define S25_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// A vertex of the vortex: the sprite's screen point plus a ring entry, as a
// float into the GT4 and the F4 alike (the GT4's first).
void VortexVertex(unsigned char* g, unsigned go, unsigned char* f, unsigned fo, const unsigned char* entry) {
    const int x = S16(Sprite_Current + 0x2E) + Long(entry);
    PutFloat(g + go, x);
    PutFloat(f + fo, x);
    const int y = S16(Sprite_Current + 0x30) + Long(entry + 4);
    PutFloat(g + go + 4, y);
    PutFloat(f + fo + 4, y);
}

// A texture pair from the rings' low bytes plus +9.
void VortexUV(unsigned char* g, unsigned o, std::uint32_t entry) {
    g[o] = static_cast<unsigned char>(Mem(entry)[0] + Sprite_Current[9]);
    g[o + 1] = static_cast<unsigned char>(Mem(entry + 4)[0] + Sprite_Current[9]);
}

}  // namespace

// original 0x4D49E0 (Magic_Rows row 12): phase +1 through a four-entry stack
// table - start, open, hold, close.
S25_EXPORT void __cdecl SpellDepress_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::SpellDepress_Start, bof3::addr::SpellDepress_Open,
                                                 bof3::addr::SpellDepress_Hold, bof3::addr::SpellDepress_Close};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 4) bof3::Fatal("SpellDepress_Task: phase %u, past the four-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
}

// original 0x4D4A20: cast on the caster's own side (the target byte 0x80 - the
// party - by a party member, or 0x40 - the enemies - by an enemy), the target
// flagged 0x40, the done flag and the slot freed at once. Otherwise sound
// 0x100; +9 2; the sprite's screen box (x - 2, y + 240 * Gfx_BufferIndex - 2,
// 4 x 4) moved into VRAM at (0x340, 0x100) (DR_MOVE, slot 3); the sprite's
// screen point from SpellDepress_Corners by the side corner 0x904AAC (xor 2
// unless the target is the enemy side); +0x64 0; on.
S25_EXPORT void __cdecl SpellDepress_Start(void) {
    if (Mem(::bof3::Img(at::kTarget))[0] == 0x80) {
        if (Mem(::bof3::Img(at::kActor))[0] < 3) Sprite_Current[1] = 1;
    }
    if (Mem(::bof3::Img(at::kTarget))[0] == 0x40 && Mem(::bof3::Img(at::kActor))[0] > 2) Sprite_Current[1] = 1;
    // (the original's first test skips the second when the caster is an enemy;
    // the target byte cannot be both, so the two are the same)
    if (Sprite_Current[1] != 0) {
        MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
        Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    MH_CALL(Sound_PlayEffect)(0x100);
    Sprite_Current[9] = 2;
    const unsigned char* sc = Sprite_Current;
    short rect[4];
    rect[0] = static_cast<short>(static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x) - sc[9]);
    rect[1] = static_cast<short>(Gfx_BufferIndex * 0xF0 + Word(sc + 0x30) - sc[9]);
    rect[2] = static_cast<short>(sc[9] * 2);
    rect[3] = static_cast<short>(sc[9] * 2);
    MH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, reinterpret_cast<const unsigned char*>(rect), 0x340, 0x100);
    MH_CALL(Gfx_CommitPrim)(3, 0x18);
    const std::uint32_t corners = ::bof3::Img(bof3::addr::SpellDepress_Corners);
    unsigned corner;
    if (Mem(::bof3::Img(at::kTarget))[0] & 0x40) {
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, Word(Mem(corners + (Mem(::bof3::Img(kSideCorner.v))[0]) * 8)));
        corner = Mem(::bof3::Img(kSideCorner.v))[0];
    } else {
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, Word(Mem(corners + (Mem(::bof3::Img(kSideCorner.v))[0] ^ 2u) * 8)));
        corner = Mem(::bof3::Img(kSideCorner.v))[0] ^ 2u;
    }
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, Word(Mem(corners + 4 + corner * 8)));
    rv::Store<std::int32_t>(rv::Current<records::BattleTask>()->angle[0], 0);
    Bump(Sprite_Current[1]);
}

// original 0x4D4BA0 (phase 1): +9 up by 2; at 0x54 on, +0xA 0x40; the vortex
// drawn with the twist +0x64.
S25_EXPORT void __cdecl SpellDepress_Open(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] + 2);
    unsigned char* sc = Sprite_Current;
    if (sc[9] == 0x54) {
        Bump(sc[1]);
        Sprite_Current[0xA] = 0x40;
        sc = Sprite_Current;
    }
    MH_CALL(SpellDepress_DrawVortex)(static_cast<std::int32_t>(rv::As<records::BattleTask>(sc)->angle[0]));
}

// original 0x4D4BE0 (phase 2): +0xA down; when it was 0 the target flagged
// 0x10 (Battle_SetTargetFlags) and on; the vortex drawn; the twist swung.
S25_EXPORT void __cdecl SpellDepress_Hold(void) {
    unsigned char* const sc = Sprite_Current;
    const unsigned char was = sc[0xA];
    sc[0xA] = static_cast<unsigned char>(was - 1);
    if (was == 0) {
        MH_CALL(Battle_SetTargetFlags)(Mem(::bof3::Img(at::kTarget))[0], 0x10);
        Bump(Sprite_Current[1]);
    }
    MH_CALL(SpellDepress_DrawVortex)(static_cast<std::int32_t>(rv::Current<records::BattleTask>()->angle[0]));
    Swing();
}

// original 0x4D4C60 (phase 3): +9 down by 2; at 2 the target flagged 0x40,
// the done flag and the slot freed; else the vortex drawn and the twist
// swung.
S25_EXPORT void __cdecl SpellDepress_Close(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 2);
    const unsigned char* const sc = Sprite_Current;
    if (sc[9] == 2) {
        MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(at::kTarget))[0]);
        Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
        MH_CALL(BattleTask_FreeCurrent)();
        return;
    }
    MH_CALL(SpellDepress_DrawVortex)(static_cast<std::int32_t>(rv::As<records::BattleTask>(sc)->angle[0]));
    Swing();
}

// original 0x4D4CE0 (step, the twist): the target's screen box, grabbed into
// VRAM by SpellDepress_Start, drawn back as a vortex. SpellDepress_Rings
// (0x6989C0): nine rings of sixteen points, ring r of radius (+9 >> 3) * r;
// the box redrawn (DR_MOVE again, then tpage 0x11D, slot 3); the outer ring
// (SpellDepress_Outer, 0x698940) at radius +9; then eight bands inwards, each
// ring (SpellDepress_Inner, 0x698E40) turned by a twist that grows by `step`
// a band, sixteen quads a band - a black F4 under a GT4 of page 0x11D whose
// texture is the rings' own offsets from the box's centre (+9), shaded 0x70
// - 0x10 a band inside and 0x10 more outside; the winding by the sign of the
// task's +0x64. Slot 3 throughout.
S25_EXPORT void __cdecl SpellDepress_DrawVortex(int step) {
    const std::uint32_t rings = ::bof3::Img(bof3::addr::SpellDepress_Rings), outer = ::bof3::Img(bof3::addr::SpellDepress_Outer),
                        inner = ::bof3::Img(bof3::addr::SpellDepress_Inner);
    for (int r = 0; r < 9; ++r) {
        int a = 0;
        for (unsigned k = 0; k < 16; ++k) {
            unsigned char* const e = Mem(rings + 8 * (16 * r + k));
            const int s = MH_CALL(Math_Sin)(a);
            SetLong(e, Sar12(Mul(Mul(s, Sprite_Current[9] >> 3), r)));
            const int c = MH_CALL(Math_Cos)(a);
            SetLong(e + 4, Sar12(Mul(Mul(c, Sprite_Current[9] >> 3), r)));
            a += 0x100;
        }
    }
    {
        const unsigned char* const sc = Sprite_Current;
        short rect[4];
        rect[0] = static_cast<short>(static_cast<std::uint16_t>(rv::As<records::BattleTask>(sc)->screen_x) - sc[9]);
        rect[1] = static_cast<short>(Gfx_BufferIndex * 0xF0 + Word(sc + 0x30) - sc[9]);
        rect[2] = static_cast<short>(sc[9] * 2);
        rect[3] = static_cast<short>(sc[9] * 2);
        MH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, reinterpret_cast<const unsigned char*>(rect), 0x340, 0x100);
    }
    MH_CALL(Gfx_CommitPrim)(3, 0x18);
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x11D, 0);
    MH_CALL(Gfx_CommitPrim)(3, 0xC);
    {
        int a = 0;
        for (unsigned k = 0; k < 16; ++k) {
            unsigned char* const e = Mem(outer + 8 * k);
            const int s = MH_CALL(Math_Sin)(a);
            SetLong(e, Sar12(Mul(s, Sprite_Current[9])));
            const int c = MH_CALL(Math_Cos)(a);
            SetLong(e + 4, Sar12(Mul(c, Sprite_Current[9])));
            a += 0x100;
        }
    }
    unsigned char shade = 0x70;
    int band_row = 0x70, ring = 7, twist = step, turned = 0;
    for (int band = 8; band != 0; --band) {
        turned += twist;
        int a = turned;
        for (unsigned k = 0; k < 16; ++k) {
            unsigned char* const e = Mem(inner + 8 * k);
            const int s = MH_CALL(Math_Sin)(a);
            SetLong(e, Sar12(Mul(Mul(s, Sprite_Current[9] >> 3), ring)));
            const int c = MH_CALL(Math_Cos)(a);
            SetLong(e + 4, Sar12(Mul(Mul(c, Sprite_Current[9] >> 3), ring)));
            a += 0x100;
        }
        const auto outside = static_cast<unsigned char>(shade + 0x10);
        for (unsigned k = 1; k <= 16; ++k) {
            unsigned char* const f = Gfx_PacketNext;
            MH_CALL(Gpu_SetPolyF4)(f);
            f[4] = 0;
            f[5] = 0;
            f[6] = 0;
            unsigned char* const g = Gfx_PacketNext + 0x38;
            MH_CALL(Gpu_SetPolyGT4)(g);
            rv::Store<unsigned>(rv::As<records::GpuPolyGT4>(g)->tpage, 0x11D);
            for (unsigned o : {4u, 5u, 6u, 0x18u, 0x19u, 0x1Au}) g[o] = outside;
            for (unsigned o : {0x2Cu, 0x2Du, 0x2Eu, 0x40u, 0x41u, 0x42u}) g[o] = shade;
            const unsigned was = (k - 1) & 0xF, now = k & 0xF;
            const unsigned char* const outer_was = Mem(outer + 8 * was);
            const unsigned char* const outer_now = Mem(outer + 8 * now);
            const unsigned char* const inner_was = Mem(inner + 8 * was);
            const unsigned char* const inner_now = Mem(inner + 8 * now);
            const std::uint32_t tex_was = (was + static_cast<unsigned>(band_row)) << 3;
            const std::uint32_t tex_now = (static_cast<unsigned>(band_row) + now) << 3;
            if (static_cast<std::int32_t>(rv::Current<records::BattleTask>()->angle[0]) > 0) {
                VortexVertex(g, 8, f, 8, outer_was);
                VortexVertex(g, 0x1C, f, 0x14, outer_now);
                VortexVertex(g, 0x30, f, 0x20, inner_was);
                VortexVertex(g, 0x44, f, 0x2C, inner_now);
                VortexUV(g, 0x14, rings + 0x80 + tex_was);
                VortexUV(g, 0x28, rings + 0x80 + tex_now);
                VortexUV(g, 0x3C, rings + tex_was);
                VortexUV(g, 0x50, rings + tex_now);
            } else {
                VortexVertex(g, 0x1C, f, 0x14, outer_was);
                VortexVertex(g, 8, f, 8, outer_now);
                VortexVertex(g, 0x44, f, 0x2C, inner_was);
                VortexVertex(g, 0x30, f, 0x20, inner_now);
                VortexUV(g, 0x28, rings + 0x80 + tex_was);
                VortexUV(g, 0x14, rings + 0x80 + tex_now);
                VortexUV(g, 0x50, rings + tex_was);
                VortexUV(g, 0x3C, rings + tex_now);
            }
            MH_CALL(Gfx_CommitPrim)(3, 0x38);
            MH_CALL(Gfx_CommitPrim)(3, 0x54);
        }
        std::memmove(Mem(outer), Mem(inner), 0x80);
        twist += step;
        shade = static_cast<unsigned char>(shade - 0x10);
        --ring;
        band_row -= 0x10;
    }
}

void SpellDepress_Inject() {
    BOF3_INJECT(SpellDepress_Task);
    BOF3_INJECT(SpellDepress_Start);
    BOF3_INJECT(SpellDepress_Open);
    BOF3_INJECT(SpellDepress_Hold);
    BOF3_INJECT(SpellDepress_Close);
    BOF3_INJECT(SpellDepress_DrawVortex);
}
