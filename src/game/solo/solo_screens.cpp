#include "game/solo/solo_screens.h"

#include "game/solo/solo.h"
#include "hook/log.h"

// DIVERGENCE DIV-0228 (amended 2026-11-22) (docs/solo-character-shops.md): the merchant's buy
// screen and the masters' member screens show the solo champion alone.

namespace solo {
namespace {

// The decision, from inputs alone: what the count and a slot's record are.
unsigned CountOf(bool champion_only, unsigned game_count) { return champion_only ? 1u : game_count; }

// -1: the slot does not exist while the champion is shown alone.
int RecordOf(bool champion_only, int champion_record, unsigned slot, unsigned game_record) {
    if (!champion_only) return static_cast<int>(game_record);
    return slot == 0 ? champion_record : -1;
}

// The joined list with `champion` first: the others keep their order. The
// new count, or -1 when the list would pass its eight bytes.
int ChampionFirst(int champion, unsigned char* list, unsigned count) {
    unsigned char others[8];
    unsigned n = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (list[i] == champion) continue;
        if (n == 7) return -1;
        others[n++] = list[i];
    }
    list[0] = static_cast<unsigned char>(champion);
    for (unsigned i = 0; i < n; ++i) list[1 + i] = others[i];
    return static_cast<int>(n + 1);
}

}  // namespace

bool SoloScreens_ChampionOnly() { return Solo_ChampionFights(); }

unsigned SoloScreens_Count(unsigned game_count) { return CountOf(SoloScreens_ChampionOnly(), game_count); }

unsigned SoloScreens_Record(unsigned slot, unsigned game_record, const char* who) {
    if (!SoloScreens_ChampionOnly()) return game_record;
    const int record = RecordOf(true, Solo_ChampionRecord(), slot, game_record);
    if (record < 0)
        bof3::Fatal("DIV-0228 %s: slot %u was asked while the screen shows the solo champion alone (its one slot is 0)",
                    who, slot);
    return static_cast<unsigned>(record);
}

unsigned SoloScreens_ChampionFirst(unsigned char* list, unsigned count) {
    if (!SoloScreens_ChampionOnly()) return count;
    if (count > 8) bof3::Fatal("DIV-0228 the Skill Notes' joined list: %u records, past its eight", count);
    const int n = ChampionFirst(Solo_ChampionRecord(), list, count);
    if (n < 0) bof3::Fatal("DIV-0228 the Skill Notes' joined list: the champion does not fit in its eight bytes");
    return static_cast<unsigned>(n);
}

int Solo_ScreensSelfTest() {
    int checks = 0;
    // Off: the game's own count and record, whatever the slot.
    for (unsigned count = 0; count <= 3; ++count) {
        if (CountOf(false, count) != count) bof3::Fatal("solo screens self-test: the count %u with the mode off", count);
        ++checks;
    }
    for (unsigned slot = 0; slot < 3; ++slot)
        for (unsigned record = 0; record < 8; ++record) {
            if (RecordOf(false, 4, slot, record) != static_cast<int>(record))
                bof3::Fatal("solo screens self-test: slot %u record %u with the mode off", slot, record);
            ++checks;
        }
    // On: one member, the champion, at slot 0 only.
    for (unsigned count = 0; count <= 3; ++count) {
        if (CountOf(true, count) != 1) bof3::Fatal("solo screens self-test: the count with the champion alone");
        ++checks;
    }
    static const int kChampions[] = {0, 1, 2, 4, 5, 6};
    for (const int champion : kChampions)
        for (unsigned record = 0; record < 8; ++record) {
            if (RecordOf(true, champion, 0, record) != champion)
                bof3::Fatal("solo screens self-test: slot 0 is not champion %d", champion);
            if (RecordOf(true, champion, 1, record) != -1 || RecordOf(true, champion, 2, record) != -1)
                bof3::Fatal("solo screens self-test: a slot past 0 exists for champion %d", champion);
            checks += 2;
        }
    // The Skill Notes' list: the champion first, the others in their order.
    {
        unsigned char joined[8] = {0, 1, 4, 5};   // the champion among them
        if (ChampionFirst(4, joined, 4) != 4 || joined[0] != 4 || joined[1] != 0 || joined[2] != 1 || joined[3] != 5)
            bof3::Fatal("solo screens self-test: a joined champion is not moved to the front");
        ++checks;
        unsigned char absent[8] = {0, 1, 5};      // the champion not joined
        if (ChampionFirst(4, absent, 3) != 4 || absent[0] != 4 || absent[1] != 0 || absent[2] != 1 || absent[3] != 5)
            bof3::Fatal("solo screens self-test: a champion who is not joined is not put in front");
        ++checks;
        unsigned char first[8] = {0, 1};          // already first
        if (ChampionFirst(0, first, 2) != 2 || first[0] != 0 || first[1] != 1)
            bof3::Fatal("solo screens self-test: a champion already first moved the list");
        ++checks;
        unsigned char none[8] = {};               // nobody joined
        if (ChampionFirst(6, none, 0) != 1 || none[0] != 6)
            bof3::Fatal("solo screens self-test: the champion alone in an empty list");
        ++checks;
        unsigned char all[8] = {0, 1, 2, 3, 4, 5, 6, 7};
        if (ChampionFirst(6, all, 8) != 8 || all[0] != 6 || all[1] != 0 || all[6] != 5 || all[7] != 7)
            bof3::Fatal("solo screens self-test: the full list of eight");
        ++checks;
        unsigned char full[8] = {0, 1, 2, 3, 3, 5, 7, 7};   // eight without the champion: no room
        if (ChampionFirst(6, full, 8) != -1) bof3::Fatal("solo screens self-test: a ninth record was taken");
        ++checks;
    }
    return checks;
}

}  // namespace solo
