#include "game/field_input.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

constexpr unsigned kMemberSize = 0x14C;
constexpr unsigned kStateSize = 0xA4;
static_assert(Field_Members_count == kMemberSize);
static_assert(Field_ActorStates_count == kStateSize);

// --- BOF3X_SHADOW=field_input: a differential fuzz, once at start-up -----------
// No calls, every jump internal: a byte-copy runs in place against the same
// globals. The fuzz owns, and puts back, kFuzzMembers member records and all
// 256 state records a byte can index. Counts 0 to kFuzzMembers; the state bit
// set for about one index in six, so that with several members "any of them"
// and "the first of them" disagree often; a random held dword.

constexpr unsigned kFuzzMembers = 8;

using CopyFn = void (__cdecl*)();

void SelfTest(CopyFn theirs) {
    constexpr unsigned kRounds = 8000;
    static unsigned char saved_members[kFuzzMembers * kMemberSize], saved_states[256 * kStateSize];
    std::memcpy(saved_members, Field_Members, sizeof saved_members);
    std::memcpy(saved_states, Field_ActorStates, sizeof saved_states);
    const unsigned char saved_flags = Field_InputFlags, saved_count = Field_MemberCount;
    std::uint32_t saved_held;   // the original reads it as a dword; the fuzz sets all four bytes
    std::memcpy(&saved_held, &Input_Held, sizeof saved_held);
    const unsigned short saved_out = Field_InputHeld;

    std::uint32_t rng = 0x94D049BBu;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };

    unsigned bad = 0, exchanged = 0, all_members = 0, first_disagrees = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        for (unsigned i = 0; i < 256; ++i)
            Field_ActorStates[i * kStateSize] = static_cast<unsigned char>(next() % 6 == 0 ? next() | 0x20u : next() & ~0x20u);
        for (unsigned i = 0; i < kFuzzMembers; ++i) Field_Members[i * kMemberSize] = static_cast<unsigned char>(next());
        Field_InputFlags = static_cast<unsigned char>(next());
        Field_MemberCount = static_cast<unsigned char>(next() % (kFuzzMembers + 1));
        const std::uint32_t held = next();
        std::memcpy(&Input_Held, &held, sizeof held);

        Field_InputHeld = 0xA5A5;
        theirs();
        const unsigned short their_out = Field_InputHeld;
        Field_InputHeld = 0xA5A5;
        Field_CopyInput();
        const unsigned short ours = Field_InputHeld;

        if (ours != static_cast<unsigned short>(held)) ++exchanged;
        if (Field_InputFlags & 1) {
            ++all_members;
            const bool first = Field_MemberCount != 0 && (Field_ActorStates[Field_Members[0] * kStateSize] & 0x20);
            if (first != (ours != static_cast<unsigned short>(held)) ) ++first_disagrees;
        }
        if (their_out != ours && ++bad <= 8)
            bof3::Log("shadow      Field_CopyInput self-test MISMATCH round %u: held 0x%08X flags 0x%02X count %u: "
                      "0x%04X vs ours 0x%04X", round, held, Field_InputFlags, Field_MemberCount, their_out, ours);
    }
    std::memcpy(Field_Members, saved_members, sizeof saved_members);
    std::memcpy(Field_ActorStates, saved_states, sizeof saved_states);
    Field_InputFlags = saved_flags;
    Field_MemberCount = saved_count;
    std::memcpy(&Input_Held, &saved_held, sizeof saved_held);
    Field_InputHeld = saved_out;
    bof3::Log("shadow      Field_CopyInput self-test: %u rounds (%u looking at every member, in %u of which the first "
              "member alone would have answered otherwise; %u changed the word), %u MISMATCHES",
              kRounds, all_members, first_disagrees, exchanged, bad);
    if (bad) bof3::Fatal("Field_CopyInput differs from the original in %u of %u self-test rounds", bad, kRounds);
}

bool HasState20(unsigned member) {
    return (Field_ActorStates[Field_Members[member * kMemberSize] * kStateSize] & 0x20) != 0;
}

}  // namespace

// original 0x531BD0. The field's copy of the held buttons - with bits 12-13
// and 14-15 changing places while a member's state byte has bit 0x20. Which
// members count is Field_InputFlags bit 0: set, any of them; clear, the first,
// whatever the count says.
//
// As the original has it: the exchanged word also loses bits 9 and 10, and
// with bit 0 set and a count of 0 nothing is exchanged.
extern "C" void __cdecl Field_CopyInput(void) {
    bool exchange = false;
    if (Field_InputFlags & 1) {
        for (unsigned i = 0; i < Field_MemberCount; ++i)
            if (HasState20(i)) exchange = true;
    } else {
        exchange = HasState20(0);
    }
    const std::uint32_t held = Input_Held;
    if (!exchange) {
        Field_InputHeld = static_cast<unsigned short>(held);
        return;
    }
    Field_InputHeld = static_cast<unsigned short>(((held & 0xF000u) << 2) | ((held >> 2) & 0x3000u) | (held & 0x9FFu));
}

void FieldInput_Inject() {
    // 0x531BD0..0x531C69: no calls, every jump internal (disasm 2026-09-20).
    if (bof3::WantsShadow("field_input"))
        SelfTest(reinterpret_cast<CopyFn>(
            bof3::CloneOriginal("Field_CopyInput", bof3::addr::Field_CopyInput, 0x9A)));
    BOF3_INJECT(Field_CopyInput);
}
