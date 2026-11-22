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
    return checks;
}

}  // namespace solo
