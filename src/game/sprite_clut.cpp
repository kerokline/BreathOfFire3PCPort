#include "game/sprite_clut.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr unsigned kPending = 0x138;   // Field_State: bit 3, "a CLUT wants putting back"
constexpr unsigned kClut = 0x27;       // sprite object: which CLUT is its

// --- BOF3X_SHADOW=sprite_clut: a differential fuzz, once at start-up -----------
// No calls, every jump internal: a byte-copy runs in place against the same
// globals. The fuzz owns both strips, the dirty flag, Field_State and
// Sprite_Current for its duration and puts them back. Random strips, every
// CLUT byte, the pending bit set two rounds in three with the rest of its
// byte noise; both strips, the flag and the state block compared.

using RestoreFn = void (__cdecl*)();

void SelfTest(RestoreFn theirs) {
    constexpr unsigned kRounds = 3000;
    static unsigned short saved_strip[Gfx_ClutStrip_count], saved_source[Gfx_ClutStripSource_count];
    static unsigned short strip_in[Gfx_ClutStrip_count], their_strip[Gfx_ClutStrip_count];
    static unsigned short source_in[Gfx_ClutStripSource_count];
    std::memcpy(saved_strip, Gfx_ClutStrip, sizeof saved_strip);
    std::memcpy(saved_source, Gfx_ClutStripSource, sizeof saved_source);
    const unsigned char saved_dirty = Gfx_ClutStripDirty;
    unsigned char* const saved_state = Field_State;
    unsigned char* const saved_current = Sprite_Current;

    std::uint32_t rng = 0xD6E8FEB8u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    for (auto& w : source_in) w = static_cast<unsigned short>(next());
    std::memcpy(Gfx_ClutStripSource, source_in, sizeof source_in);

    unsigned bad = 0, pending = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (auto& w : strip_in) w = static_cast<unsigned short>(next());
        unsigned char state_in[0x140], object[0x80];
        for (auto& b : state_in) b = static_cast<unsigned char>(next());
        for (auto& b : object) b = static_cast<unsigned char>(next());
        if (next() % 3) state_in[kPending] |= 8; else state_in[kPending] &= static_cast<unsigned char>(~8u);
        object[kClut] = static_cast<unsigned char>(round);   // every CLUT in turn
        const unsigned char dirty_in = static_cast<unsigned char>(next() % 2 ? 0 : next());
        pending += (state_in[kPending] & 8) != 0;

        unsigned char state[2][0x140], dirty[2];
        for (int pass = 0; pass < 2; ++pass) {
            std::memcpy(Gfx_ClutStrip, strip_in, sizeof strip_in);
            std::memcpy(state[pass], state_in, sizeof state_in);
            Gfx_ClutStripDirty = dirty_in;
            Field_State = state[pass];
            Sprite_Current = object;
            if (pass) Sprite_RestoreClut(); else theirs();
            dirty[pass] = Gfx_ClutStripDirty;
            if (pass == 0) std::memcpy(their_strip, Gfx_ClutStrip, sizeof their_strip);
        }
        if ((dirty[0] != dirty[1] || std::memcmp(state[0], state[1], sizeof state_in) != 0 ||
             std::memcmp(their_strip, Gfx_ClutStrip, sizeof their_strip) != 0 ||
             std::memcmp(source_in, Gfx_ClutStripSource, sizeof source_in) != 0) && ++bad <= 8)
            bof3::Log("shadow      Sprite_RestoreClut self-test MISMATCH round %u: clut %u, flags 0x%02X",
                      round, object[kClut], state_in[kPending]);
    }
    std::memcpy(Gfx_ClutStrip, saved_strip, sizeof saved_strip);
    std::memcpy(Gfx_ClutStripSource, saved_source, sizeof saved_source);
    Gfx_ClutStripDirty = saved_dirty;
    Field_State = saved_state;
    Sprite_Current = saved_current;
    bof3::Log("shadow      Sprite_RestoreClut self-test: %u rounds (%u with the bit set; every CLUT byte), "
              "%u MISMATCHES; both strips, the dirty flag and the state block compared", kRounds, pending, bad);
    if (bad) bof3::Fatal("Sprite_RestoreClut differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x534E50. If the field state says one is wanted: puts the current
// sprite object's CLUT back as it was loaded - entries 1 to 31, entry 0 is
// left - marks the strip for upload, and clears the request.
extern "C" void __cdecl Sprite_RestoreClut(void) {
    unsigned char* const state = Field_State;
    if (!(state[kPending] & 8)) return;
    for (unsigned n = 1; n < 0x20; ++n) {
        const unsigned at = Sprite_Current[kClut] * 32u + n;
        Gfx_ClutStrip[at] = Gfx_ClutStripSource[at];
    }
    Gfx_ClutStripDirty = 1;
    state[kPending] &= 0xF7;
}

void SpriteClut_Inject() {
    // 0x534E50..0x534EB7: no calls, every jump internal (disasm 2026-09-20).
    if (bof3::WantsShadow("sprite_clut"))
        SelfTest(reinterpret_cast<RestoreFn>(
            bof3::CloneOriginal("Sprite_RestoreClut", bof3::addr::Sprite_RestoreClut, 0x68)));
    BOF3_INJECT(Sprite_RestoreClut);
}
