// Moved here by tools/move_funcs.py from battle_sprites.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/battle/battle_5.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_sprites_callees.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/sound_sp.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = rest_3g::at;

}  // namespace

using namespace battle_sprites;

namespace battle_sprites {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

namespace {

unsigned char* Enemy(std::uint32_t i) { return At(::bof3::Img(Battle_Enemies_addr) + i * battle_sprites::at::kEnemyStride); }

void OrLong(unsigned char* at, std::uint32_t bits) { SetLong(at, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(at)) | bits)); }

unsigned char* EnemyData(std::uint32_t id) { return At(::bof3::Img(battle_sprites::at::kEnemyData.v) + id * battle_sprites::at::kEnemyDataStride); }

}  // namespace

}  // namespace battle_sprites

namespace {

using move_script::At;

}  // namespace

// original 0x494A80 (0x800A9894): one name window per living enemy at battle
// start. The living enemies' kinds (record +0x8C) are grouped into a list of
// up to eight {kind, count}, sorted by count, most first; the column's x and
// y come from 0x656A34 by the layout byte; then kind by kind, from the bottom
// up, each enemy of that kind (among the first 0x904AB2 records, alive or
// not, up to the kind's count) gets window record 0xC - n claimed
// (0x59E2D0(0xC - n, 3)) and filled: +2 = 4, +3 = 0, the x at +4 and +0x10,
// the row's y at +6 and +0x12, +0xA = the actor, +8 = +9 = 0. The window's
// handler draws the name (Battle_CopyEnemyData put it at record +0x80); this
// function reads no text.
//
// As the original has it:
//  - for layouts 0 and 1 the column is first measured (each kind's count * 8
//    + 0xD, and 2 between kinds) and starts that far below the table's y;
//  - the list is ended only by a kind of 0xFFFF: with eight different kinds
//    the walk runs on past the list's eight entries into the stack above the
//    original's frame - its return address, then the caller's frame - and
//    decrements a count there when a kind matches. Ours reads and writes
//    the same words: its entry stack pointer is the original's (the detour
//    is a jmp), so list entry 8 + k is the dword at entry esp + 4k.
extern "C" void __cdecl Battle_OpenEnemyNames(void) {
    std::uint16_t kinds[8] = {0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF};
    std::uint16_t counts[8] = {};
    unsigned char* const above = static_cast<unsigned char*>(__builtin_frame_address(0)) + 4;
    auto kind = [&](unsigned k) -> std::uint16_t { return k < 8 ? kinds[k] : Word(above + (k - 8) * 4); };
    auto count = [&](unsigned k) -> std::uint16_t { return k < 8 ? counts[k] : Word(above + (k - 8) * 4 + 2); };
    auto set_count = [&](unsigned k, unsigned v) {
        if (k < 8) counts[k] = static_cast<std::uint16_t>(v);
        else SetWord(above + (k - 8) * 4 + 2, v);
    };

    unsigned char listed = 0;
    unsigned char enemies = Battle_EnemyCount;
    if (enemies) {
        for (unsigned e = 0, left = enemies; left; --left, ++e) {
            auto* const enemy_rec = rv::As<records::BattleEnemy>(Enemy(e));
            if (!(enemy_rec->flags & 1)) continue;
            const std::uint16_t k = enemy_rec->enemy_kind;
            unsigned c = 0;
            while (c < listed && kinds[c] != k) ++c;
            if (c < listed) {
                ++counts[c];
                continue;
            }
            kinds[listed] = k;
            counts[listed] = 1;
            ++listed;
        }
        enemies = Battle_EnemyCount;
    }
    if (enemies > 1 && listed > 1)
        for (unsigned char d = 1; d < listed; ++d)
            for (unsigned j = static_cast<unsigned char>(d - 1); static_cast<int>(j) < listed - 1; ++j)
                if (static_cast<short>(counts[j]) < static_cast<short>(counts[j + 1])) {
                    const std::uint16_t tk = kinds[j], tc = counts[j];
                    kinds[j] = kinds[j + 1];
                    counts[j] = counts[j + 1];
                    kinds[j + 1] = tk;
                    counts[j + 1] = tc;
                }

    const unsigned layout = At(::bof3::Img(battle_sprites::at::kLayout.v))[0];
    const std::uint16_t x = Word(At(::bof3::Img(battle_sprites::at::kNamePlaces.v) + layout * 4));
    std::uint16_t y = Word(At(::bof3::Img(battle_sprites::at::kNamePlaces.v) + layout * 4 + 2));
    if (layout < 2) {
        for (unsigned char k = 0;;) {
            y = static_cast<std::uint16_t>(y + static_cast<std::uint16_t>(count(k) << 3) + 0xD);
            ++k;
            if (kind(k) == 0xFFFF) break;
            y = static_cast<std::uint16_t>(y + 2);
        }
    }
    if (kind(0) == 0xFFFF) return;
    unsigned char shown = 0;
    unsigned char k = 0;
    do {
        unsigned char left = static_cast<unsigned char>(count(k));
        y = static_cast<std::uint16_t>(y - left * 8u - 0xD);
        const std::uint16_t top = y;
        std::uint16_t row = y;
        if (Battle_EnemyCount)
            for (unsigned char e = 0; left;) {
                if (kind(k) == rv::As<records::BattleEnemy>(Enemy(e))->enemy_kind) {
                    set_count(k, count(k) - 1u);
                    g.window_open(static_cast<unsigned char>(0xC - shown), 3);
                    unsigned char* const w = At(::bof3::Img(WindowRecords_addr) + 0x1B0 - shown * battle_sprites::at::kWindowStride);
                    auto* const window_rec = rv::As<records::WindowRecord>(w);
                    window_rec->kind = 4;
                    window_rec->state = 0;
                    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->home_x, x);  // handler 3 (window_open's second argument): a battle window
                    window_rec->x = x;
                    rv::Store<unsigned>(rv::As<records::BattleWindow>(w)->home_y, row);
                    window_rec->y = row;
                    w[0xA] = static_cast<unsigned char>(e + 3);
                    w[8] = 0;
                    w[9] = 0;
                    ++shown;
                    --left;
                    row = static_cast<std::uint16_t>(row + 8);
                }
                if (!(++e < Battle_EnemyCount)) break;
            }
        y = static_cast<std::uint16_t>(top - 2);
        ++k;
    } while (kind(k) != 0xFFFF);
}

// original 0x494E70 (PSX 0x800A9EB0; BattleEnd_Finish, BattleLoss_ResetParty, BattleLoss_Restart,
// Escape_Leave): bytes +0..+4 of the eight enemy records 0.
extern "C" void __cdecl BattleEnemy_ClearStates(void) {
    for (unsigned i = 0; i < rest_3g::at::kEnemyCount; ++i) {
        unsigned char* const e = At(::bof3::Img(Battle_Enemies_addr) + rest_3g::at::kEnemyStride * i);
        for (unsigned k = 0; k < 5; ++k) e[k] = 0;
    }
}

// original 0x494EA0: bit `kind` (16 bits) of the set at 0x904068 - an enemy
// kind the party has beaten before: Battle_EnemyDefeated (battle_flow.cpp)
// sets it, Battle_SetupEnemy copies it to the record's +0x8F, and the HP
// gauge (battle_windows.cpp EnemyGauge) and Identify's panel (magic_s12.cpp)
// read that copy. The set lies in the save's flag block (Cond_Flags +0xD8).
extern "C" unsigned char __cdecl Battle_EnemyKindFlag(unsigned kind) {
    const std::uint32_t k = kind & 0xFFFF;
    return (static_cast<std::uint32_t>(Long(At(::bof3::Img(Cond_Flags_addr) + 0xd8 + (k >> 5) * 4))) & (1u << (k & 31))) ? 1 : 0;
}

// original 0x494ED0: sets that bit. Called from 0x437470 (group BB).
extern "C" void __cdecl Battle_SetEnemyKindFlag(unsigned kind) {
    const std::uint32_t k = kind & 0xFFFF;
    OrLong(At(::bof3::Img(Cond_Flags_addr) + 0xd8 + (k >> 5) * 4), 1u << (k & 31));
}

// original 0x494F00: enemy `slot`'s +0xF2 / +0xF3 from the enemy data `id`
// (16 bits) by Sprite_Current's +8 (the layout ^ 2): 0 (+0x79, +0x7B),
// 1 (-+0x79, +0x7B), 2 (-+0x78, +0x7A), 3 (+0x78, +0x7A); others nothing.
extern "C" void __cdecl Battle_SetEnemyOffset(unsigned slot, unsigned id) {
    const unsigned char side = Sprite_Current[8];
    if (side > 3) return;
    const unsigned char* const data = EnemyData(id & 0xFFFF);
    unsigned char* const e = Enemy(slot & 0xFF);
    switch (side) {
    case 0: e[0xF2] = data[0x79]; e[0xF3] = data[0x7B]; break;
    case 1: e[0xF2] = static_cast<unsigned char>(-data[0x79]); e[0xF3] = data[0x7B]; break;
    case 2: e[0xF2] = static_cast<unsigned char>(-data[0x78]); e[0xF3] = data[0x7A]; break;
    default: e[0xF2] = data[0x78]; e[0xF3] = data[0x7A]; break;
    }
}

void Battle5FromBattleSprites_Inject() {
    BOF3_INJECT(Battle_OpenEnemyNames);
    BOF3_INJECT(Battle_EnemyKindFlag);
    BOF3_INJECT(Battle_SetEnemyKindFlag);
    BOF3_INJECT(Battle_SetEnemyOffset);
}

void Battle5FromRest3g_Inject() {
    BOF3_INJECT(BattleEnemy_ClearStates);
}
