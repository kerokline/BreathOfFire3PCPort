#include "game/sprite_anim.h"

#include <cstdint>
#include <cstring>
#include <type_traits>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// A sprite object's animation script, as the four functions here use it
// (originals 0x589350..0x5894CF, one contiguous block):
//
//   header   2 bytes; byte 1 is the number of steps
//   steps    2 bytes each: how many ticks to hold, and which frame
//            - or, where a step would start, a byte >= 0x80 and then the
//            number of the step to go to
//   frames   u16 each, straight after the last step
//
// The object is larger and mostly unread; it is addressed by offset for that
// reason. kScript points past the header once Sprite_ScriptStart has run.
constexpr unsigned kSteps = 0x49;      // u8
constexpr unsigned kHold = 0x4A;       // u8, ticks left on this step
constexpr unsigned kScript = 0x50;     // const u8*, null for none
constexpr unsigned kFrames = 0x54;     // const u16*
constexpr unsigned kPosition = 0x58;   // u16 byte index into the steps

template <class T> T Get(const unsigned char* object, unsigned at) {
    // A pointer-typed field is a 32-bit word of the image on every width.
    if constexpr (std::is_pointer_v<T>) {
        std::uint32_t w;
        std::memcpy(&w, object + at, sizeof w);
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(w));
    } else {
        T v;
        std::memcpy(&v, object + at, sizeof v);
        return v;
    }
}
template <class T> void Put(unsigned char* object, unsigned at, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(object + at, &w, sizeof w);
    } else {
        std::memcpy(object + at, &v, sizeof v);
    }
}

// --- BOF3X_SHADOW=sprite_anim: a differential fuzz, once at start-up -----------
// The four originals only ever transfer among themselves, so ONE byte-copy of
// 0x589350..0x5894CF holds runnable copies of all four, the copies calling
// each other and never ours. Sprite_Current is pointed at a fake object whose
// script is a random buffer longer than 16 bits can index - so a position of 0xFFFF, where the
// 16-bit increment wraps between two reads, is in bounds - about half of whose
// bytes are >= 0x80, an eighth exactly that and an eighth one less. For the ticks the hold is 1 two rounds in three (so that they
// fire), and the position is at the end of the script one round in four. The
// whole object and the return value compared. Sprite_Current is put back.

constexpr ::bof3::image::CodeVa kBlock{0x589350};
constexpr std::uint32_t kBlockSize = 0x180;

struct Originals {
    void (__cdecl* start)(unsigned);
    unsigned char (__cdecl* tick)();
    unsigned char (__cdecl* tick_once)();
    void (__cdecl* step)();
};

void SelfTest(const Originals& theirs) {
    constexpr unsigned kRounds = 12000;
    constexpr unsigned kObject = 0x80;
    // Start's script begins up to 0x7FFF in, skips 2, and is indexed by 16 bits.
    static unsigned char script[0x8000 + 2 + 0x10000 + 2];
    static unsigned short frames[256];
    std::uint32_t rng = 0x27D4EB2Fu;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    // A quarter of the bytes sit on the jump threshold, 0x7F and 0x80.
    for (auto& b : script) b = next() % 4 == 0 ? static_cast<unsigned char>(0x7F + next() % 2) : static_cast<unsigned char>(next());
    for (auto& w : frames) w = static_cast<unsigned short>(next());

    unsigned char* const saved = Sprite_Current;
    unsigned bad = 0, count[4] = {}, fired = 0, jumped = 0, ended = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        unsigned char input[kObject], their_out[kObject], ours[kObject];
        for (auto& b : input) b = static_cast<unsigned char>(next());
        const unsigned which = next() % 4;
        ++count[which];
        // Start reads a header, so its script begins anywhere in the first
        // half; the others' need the whole 64 KB above them.
        const unsigned char* const base = which == 0 ? script + next() % 0x8000 : script;
        Put<const unsigned char*>(input, kScript, which == 3 && next() % 10 == 0 ? nullptr : base);
        Put<const unsigned short*>(input, kFrames, frames);
        if (next() % 3) input[kHold] = 1;
        if (next() % 4 == 0) Put<std::uint16_t>(input, kPosition, static_cast<std::uint16_t>(input[kSteps] * 2 + next() % 2));
        if (next() % 8 == 0) Put<std::uint16_t>(input, kPosition, static_cast<std::uint16_t>(0xFFFE + next() % 2));
        const unsigned argument = next();   // Start reads 16 bits of it

        unsigned result[2] = {0, 0};
        for (int pass = 0; pass < 2; ++pass) {
            unsigned char* const object = pass ? ours : their_out;
            std::memcpy(object, input, kObject);
            Sprite_Current = object;
            switch (which) {
                case 0:
                    if (pass) reinterpret_cast<void (__cdecl*)(unsigned)>(reinterpret_cast<void*>(&Sprite_ScriptStart))(argument);
                    else theirs.start(argument);
                    break;
                case 1: result[pass] = pass ? Sprite_ScriptTick() : theirs.tick(); break;
                case 2: result[pass] = pass ? Sprite_ScriptTickOnce() : theirs.tick_once(); break;
                default: if (pass) Sprite_ScriptStep(); else theirs.step(); break;
            }
        }
        if (which == 1 || which == 2) {
            if (input[kHold] == 1) ++fired;
            if (input[kHold] == 1 && result[1] && Get<std::uint16_t>(input, kPosition) >> 1 == input[kSteps]) ++ended;
            else if (result[1]) ++jumped;
        }
        if ((std::memcmp(their_out, ours, kObject) != 0 || result[0] != result[1]) && ++bad <= 8)
            bof3::Log("shadow      sprite_anim self-test MISMATCH round %u: function %u, position 0x%X, steps %u, hold %u: "
                      "result %u vs ours %u", round, which, Get<std::uint16_t>(input, kPosition), input[kSteps],
                      input[kHold], result[0], result[1]);
    }
    Sprite_Current = saved;
    bof3::Log("shadow      sprite_anim self-test: %u rounds (%u Start, %u Tick, %u TickOnce, %u Step; of the ticks %u "
              "fired, %u at the script's end, %u followed a jump), %u MISMATCHES; 0x80 object bytes and result compared",
              kRounds, count[0], count[1], count[2], count[3], fired, ended, jumped, bad);
    if (bad) bof3::Fatal("the sprite script functions differ from the originals in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void SpriteAnim_Inject() {
    // 0x589350..0x5894CF: four functions and padding; every relative transfer
    // in the block - three calls and a tail jump to 0x589470 - lands inside it
    // (disasm 2026-09-20). Cloned as one, before any of it is patched.
    if (bof3::WantsShadow("sprite_anim")) {
        auto* block = static_cast<unsigned char*>(bof3::CloneOriginal("sprite_anim block", kBlock.v, kBlockSize));
        if (block) {
            auto at = [block](std::uint32_t original) { return static_cast<void*>(block + (original - kBlock.v)); };
            SelfTest(Originals{
                reinterpret_cast<void (__cdecl*)(unsigned)>(at(bof3::addr::Sprite_ScriptStart)),
                reinterpret_cast<unsigned char (__cdecl*)()>(at(bof3::addr::Sprite_ScriptTick)),
                reinterpret_cast<unsigned char (__cdecl*)()>(at(bof3::addr::Sprite_ScriptTickOnce)),
                reinterpret_cast<void (__cdecl*)()>(at(bof3::addr::Sprite_ScriptStep))});
        }
    }
}
