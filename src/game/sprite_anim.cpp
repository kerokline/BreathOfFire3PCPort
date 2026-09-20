#include "game/sprite_anim.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

// The fields of a sprite object this file touches. The object is larger and
// mostly unread; it is addressed by offset for that reason.
constexpr unsigned kValue = 0x4A;      // u8, from the script
constexpr unsigned kScript = 0x50;     // const u8*, null for none
constexpr unsigned kTable = 0x54;      // const u16*
constexpr unsigned kPosition = 0x58;   // u16 index into the script
constexpr unsigned kLookedUp = 0x5A;   // u16, from the table

template <class T> T Get(const unsigned char* object, unsigned at) {
    T v;
    std::memcpy(&v, object + at, sizeof v);
    return v;
}
template <class T> void Put(unsigned char* object, unsigned at, T v) { std::memcpy(object + at, &v, sizeof v); }

// --- BOF3X_SHADOW=sprite_anim: a differential fuzz, once at start-up -----------
// No calls, one internal jump: a byte-copy runs in place. Sprite_Current is
// pointed at a fake object with a full 64 KB + 1 script, so that a position of
// 0xFFFF - where the 16-bit increment wraps between the two reads - is in
// bounds for both; one round in eight starts there or at 0xFFFE, one in ten
// has no script. The whole object compared. Sprite_Current is put back.

using StepFn = void (__cdecl*)();

void SelfTest(StepFn theirs) {
    constexpr unsigned kRounds = 4000;
    constexpr unsigned kObject = 0x80;
    static unsigned char script[0x10001];
    static unsigned short table[256];
    std::uint32_t rng = 0x27D4EB2Fu;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    for (auto& b : script) b = static_cast<unsigned char>(next());
    for (auto& w : table) w = static_cast<unsigned short>(next());

    unsigned char* const saved = Sprite_Current;
    unsigned bad = 0, no_script = 0, wrapped = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        unsigned char input[kObject], their_out[kObject], ours[kObject];
        for (auto& b : input) b = static_cast<unsigned char>(next());
        const bool none = next() % 10 == 0;
        Put<const unsigned char*>(input, kScript, none ? nullptr : script);
        Put<const unsigned short*>(input, kTable, table);
        if (next() % 8 == 0) Put<std::uint16_t>(input, kPosition, static_cast<std::uint16_t>(0xFFFE + next() % 2));
        no_script += none;
        if (!none && Get<std::uint16_t>(input, kPosition) >= 0xFFFE) ++wrapped;

        std::memcpy(their_out, input, kObject);
        Sprite_Current = their_out;
        theirs();
        std::memcpy(ours, input, kObject);
        Sprite_Current = ours;
        Sprite_ScriptStep();
        if (std::memcmp(their_out, ours, kObject) != 0 && ++bad <= 8)
            bof3::Log("shadow      Sprite_ScriptStep self-test MISMATCH round %u: position 0x%X", round,
                      Get<std::uint16_t>(input, kPosition));
    }
    Sprite_Current = saved;
    bof3::Log("shadow      Sprite_ScriptStep self-test: %u rounds (%u with no script, %u wrapping the position), "
              "%u MISMATCHES; 0x80 object bytes compared", kRounds, no_script, wrapped, bad);
    if (bad) bof3::Fatal("Sprite_ScriptStep differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

// original 0x589470. Takes the next two bytes of the current sprite object's
// script: the first is kept as it is, the second picks a word out of the
// object's table. An object with no script is left alone.
//
// As the original has it: the position is 16 bits and wraps; nothing checks
// it, or the index, against anything.
extern "C" void __cdecl Sprite_ScriptStep(void) {
    unsigned char* const object = Sprite_Current;
    const auto* script = Get<const unsigned char*>(object, kScript);
    if (script == nullptr) return;
    std::uint16_t position = Get<std::uint16_t>(object, kPosition);
    Put<std::uint8_t>(object, kValue, script[position]);
    ++position;
    const std::uint8_t index = script[position];
    ++position;
    Put<std::uint16_t>(object, kPosition, position);
    Put<std::uint16_t>(object, kLookedUp, Get<const unsigned short*>(object, kTable)[index]);
}

void SpriteAnim_Inject() {
    // 0x589470..0x5894CF: no calls, one jump, internal (disasm 2026-09-20).
    if (bof3::WantsShadow("sprite_anim"))
        SelfTest(reinterpret_cast<StepFn>(
            bof3::CloneOriginal("Sprite_ScriptStep", bof3::addr::Sprite_ScriptStep, 0x60)));
    BOF3_INJECT(Sprite_ScriptStep);
}
