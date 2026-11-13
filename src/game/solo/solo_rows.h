#pragma once

// What the solo-character mode's modules share about the party-set row table
// and their self-test gate (solo_battle.cpp, solo_party_set.cpp, solo.cpp).
// Nothing here writes game state.
//
//   kPartySetRows  0x669750  MoveScript_EffectArg + 0xC, as PartySet_Find and
//                            Field_MemberSprite name it (mode_11.cpp, field_6.cpp):
//                            19 rows of 3 ids, sorted (PartySet_Find's table,
//                            symbols.toml 0x5368F0's evidence; Field_MemberSprite
//                            0x533BA0 takes a member's column in row
//                            0x90412C & 0x7F; read 2026-11-12, round doc M1)

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "common/image_base.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "os/os.h"

namespace solo {

// 0x669750: kept for the sentences that name the table.
constexpr std::uint32_t kPartySetRows = MoveScript_EffectArg_addr + 0xC;
constexpr unsigned kPartySetRowCount = 0x13;
constexpr unsigned char kNone = 0xFF;   // no id in a party list, no party set

// ChooseRow's and PartySetExtra_Arm's answers besides a row: keep the current
// set (it holds what is needed), or no row will do.
constexpr int kKeep = -1;
constexpr int kNoRow = -2;

// The live table in the image, through the typed global as
// Sprite_SetAnimationAt reads it (sprite.cpp: MoveScript_EffectArg[0xc + row
// + column]).
inline unsigned char* PartySetRows() { return reinterpret_cast<unsigned char*>(MoveScript_EffectArg) + 0xC; }

// Whether `row` (0..18) of `rows` holds `id`.
inline bool RowHolds(const unsigned char* rows, unsigned row, unsigned char id) {
    for (unsigned c = 0; c < 3; ++c)
        if (rows[row * 3 + c] == id) return true;
    return false;
}

// The first row of `rows` holding `id`, scanning rows 0..18 and each row's
// columns in order: its column, the row in *row; -1 when no row holds it.
inline int FirstColumnOf(const unsigned char* rows, unsigned char id, unsigned* row = nullptr) {
    for (unsigned r = 0; r < kPartySetRowCount; ++r)
        for (int c = 0; c < 3; ++c)
            if (rows[r * 3 + c] == id) {
                if (row) *row = r;
                return c;
            }
    return -1;
}

// The solo modules' self-tests run under the exe host's BOF3X_SELFTEST_ONLY
// or the hosted BOF3X_SHADOW=solo (or '*').
inline bool SoloSelfTestWanted() {
    return bof3::IsExeHost() ? bof3::os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : bof3::WantsShadow("solo");
}

}  // namespace solo
