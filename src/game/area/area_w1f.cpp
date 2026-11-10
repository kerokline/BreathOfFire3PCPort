// World 1's areas 68..69 and 71..75: the PSX's BIN/WORLD01/AREA068..075.EMI
// compiled into the exe at 0x40CEF0..0x40EB90 (Area_Descriptors entries 68..75;
// area 70 has no code). Round ten, group AR1F: the band's 59 functions, none
// ours before, each read to its last instruction with capstone (2026-09-28) and
// taken through the area harness (area_harness.h). docs/area_w1f.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); where the original jumps or calls through a pointer read past a
// table (the running object's state byte, area 75's phase and list bytes) ours
// aborts with a message instead of faulting (round nine section 6: no ledger
// entry). Every call goes through the harness (AH_CALL / AH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies;
// the group's own callees are called the same way, so each function is
// fuzzed alone.
#include "game/area_w1f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w1f_callees.h"
#include "game/cheats.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = area_w1f::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(std::uint32_t address) { return *Mem(address); }
void AddWord(unsigned char* p, unsigned v) { SetWord(p, Word(p) + v); }
void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
// Field object `index` (Sprite_Objects, 0xA4 bytes); effect record `slot`
// (Effect_Objects, 0x80 bytes); party record `member` (ObjTrio, 0x14C bytes).
unsigned char* ObjectAt(unsigned index) { return rv::Bytes(Sprite_Objects) + index * at::kObjectStride; }
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }
// A record's index among the field objects as the originals compute it: the
// pointer less Sprite_Objects, divided by 0xA4 as a signed dword (imul by
// 0x63E7063F, sar 6, the sign added back): C's truncating division.
unsigned char ObjectIndex(const unsigned char* record) {
    const auto delta = static_cast<std::int32_t>(Low32(record) -
                                                 ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(delta / static_cast<std::int32_t>(at::kObjectStride));
}

// A two-entry .data state table read in place by the running object's +4 and
// jumped through (area 68's handlers 2 and 3, 69's 0, 75's 7 and 11). The
// original indexes it unchecked: a state of 2 or more jumps through the bytes
// after the table - ours aborts there.
void RunState(const char* who, std::uint32_t table) {
    const unsigned state = Sprite_Current[4];
    if (state >= 2)
        bof3::Fatal("%s: the running object's state %u is past its 2-entry table 0x%X (the original jumps through the "
                    "bytes after it)",
                    who, state, static_cast<unsigned>(table));
    reinterpret_cast<area_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + state * 4)))))();
}

// ---- area 68 ----

// The glides' state 0 (0x40D090, 0x40D380): the running object's count +0xA =
// `count`, its state 1, and the leader's script word +0x12E (through
// Field_State) back by 2.
void GlideBegin(unsigned char count) {
    Sprite_Current[0xA] = count;
    Sprite_Current[4] = 1;
    AddWord(Field_State + 0x12E, 0xFFFE);
}
// The glides' state 1 (0x40D0C0, 0x40D150, 0x40D3B0): with the count +0xA at 0
// the state goes back to 0; else x +0x34 moves by steps[count & 0xF] << 11 and
// z +0x38 by -steps[count & 0xF] << 11 (signed bytes), the count goes down by
// one and the script word +0x12E back by 2.
void GlideStep(std::uint32_t steps) {
    unsigned char* const cur = Sprite_Current;
    auto* const tCur = rv::As<records::SpriteObject>(cur);
    const unsigned char count = cur[0xA];
    if (count == 0) {
        cur[4] = 0;
        return;
    }
    const auto dx = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(B(steps + (count & 0xF)))));
    tCur->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->x) + (dx << 11));
    const auto dz = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<signed char>(B(steps + (cur[0xA] & 0xF)))));
    tCur->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(tCur->z) + ((0u - dz) << 11));
    cur[0xA] = static_cast<unsigned char>(cur[0xA] - 1);
    AddWord(Field_State + 0x12E, 0xFFFE);
}

// ---- areas 72 and 73 ----

// Areas 72 and 73's inits (0x40D4B0, 0x40D580, one body over two tables): a
// weighted draw - Rand() & 0x3F walked down the eight weights, the first it
// falls below chosen (none: 8); field objects 0..7 but the chosen one get +0
// = 0; the chosen one is put at one of the eight cells (Rand() & 7: x and z
// bytes << 16), its word +0x3E the ground there; Field_StepCount = the
// leader's zone counter less 5 (either way).
void PlaceRandomObject(std::uint32_t cells, std::uint32_t weights) {
    auto roll = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned chosen = 0;
    for (; chosen < 8; ++chosen) {
        const unsigned char weight = B(weights + chosen);
        if (roll < weight) break;
        roll = static_cast<unsigned char>(roll - weight);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != chosen) ObjectAt(k)[0] = 0;
    if (chosen < 8) {
        const unsigned pick = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = ObjectAt(chosen);
        auto* const tObject = rv::As<records::SpriteObject>(object);
        const auto x = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(cells + pick * 2)) << 16);
        const auto z = static_cast<std::int32_t>(static_cast<std::uint32_t>(B(cells + pick * 2 + 1)) << 16);
        tObject->x = x;
        tObject->z = z;
        const long ground = AH_CALL(AreaMap_Elevation)(tObject->x, z);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->height_whole, static_cast<unsigned>(ground));
    }
    Field_StepCount = static_cast<unsigned short>(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

// ---- area 75 ----

}  // namespace

// ===========================================================================
// Area 68 (descriptor 0x607D88): seven choices (three others' bodies), nine
// handlers (one another's), two glide state tables, object trigger 25.
// ===========================================================================

// ===========================================================================
// Area 69 (descriptor 0x608E98): one handler and its glide states.
// ===========================================================================

// ===========================================================================
// Area 71 (descriptor 0x609358): two handlers.
// ===========================================================================

// ===========================================================================
// Areas 72 and 73 (descriptors 0x609428, 0x6094F8): an init each.
// ===========================================================================

// ===========================================================================
// Area 74 (descriptor 0x60A2C0): five choices (two others'), seven handlers
// (the choices' bodies among them), object trigger 28.
// ===========================================================================

// ===========================================================================
// Area 75 (descriptor 0x60AA98): fifteen choices (choices 2..14 are handlers
// 0..12; handlers 8 and 9 are other blocks'), three handler state tables,
// the step hook, object trigger 41, tail kind 30 and its press phases.
// ===========================================================================

void AreaW1f_Inject() {
    if (bof3::WantsShadow("area_w1f")) area_w1f::SelfTest();
}
