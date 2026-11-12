#include "game/solo/solo_scenes.h"

#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/solo/solo.h"
#include "hook/behaviour.h"
#include "hook/detour.h"
#include "hook/host.h"
#include "hook/log.h"
#include "os/os.h"

// The solo-character mode's scene guards (solo_scenes.h; docs/solo-character-
// scenes.md has the sites and the reads behind each). What each address is:
//
//   Cond_Flags +0xD2   0x904062  the first party list, three ids, 0xFF after
//                                the last (Party_Remove compacts it)
//   Cond_Flags +0xD5   0x904065  the second list, the same members
//   Field_MemberCount            the walkers (ObjTrio records 0..count-1)

namespace solo {
namespace {

constexpr unsigned kIds = 24;   // Character_IndexById's entries
constexpr unsigned char kNone = 0xFF;

// ---- The decisions, on state passed in (the self-test drives them) --------

unsigned char MemberIdFor(bool active, unsigned slot, unsigned count, unsigned char raw) {
    return active && slot >= count ? kNone : raw;
}

unsigned char SlotPoseFor(bool active, unsigned slot, unsigned count, unsigned char raw) {
    return active && slot >= count ? 0 : raw;
}

void PadFor(bool active, unsigned char* saved, unsigned n) {
    if (!active) return;
    for (unsigned i = n; i < 3; ++i) saved[i] = kNone;
}

// In neither list below the count (at most the three bytes each list has).
bool NotWalkingFor(bool active, unsigned char id, const unsigned char* first, const unsigned char* second,
                   unsigned count) {
    if (!active) return false;
    const unsigned n = count < 3 ? count : 3;
    for (unsigned i = 0; i < n; ++i)
        if (first[i] == id || second[i] == id) return false;
    return true;
}

bool NoIdFor(bool active, unsigned char id) { return active && id >= kIds; }

unsigned char* Lists() { return move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2); }

}  // namespace

bool Scene_Active() { return bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter) && Solo_Active(); }

unsigned char Scene_MemberId(unsigned slot, unsigned char raw) {
    if (!bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter)) return raw;
    return MemberIdFor(Solo_Active(), slot, Field_MemberCount, raw);
}

unsigned char Scene_SlotPose(unsigned slot, unsigned char raw) {
    if (!bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter)) return raw;
    return SlotPoseFor(Solo_Active(), slot, Field_MemberCount, raw);
}

void Scene_PadSaved(unsigned char* saved, unsigned n) { PadFor(Scene_Active(), saved, n); }

bool Scene_SavedNone(unsigned char id) { return id == kNone && Scene_Active(); }

bool Scene_RemoveNotWalking(unsigned char id) {
    if (!bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter)) return false;
    const unsigned char* const lists = Lists();
    const bool none = NotWalkingFor(Solo_Active(), id, lists, lists + 3, Field_MemberCount);
    if (none)
        bof3::Log("DIV-0228 solo scenes: Party_Remove(%02X) with id %02X walking in no slot (count %u, first %02X %02X "
                  "%02X): nothing removed",
                  id, id, static_cast<unsigned>(Field_MemberCount), lists[0], lists[1], lists[2]);
    return none;
}

bool Scene_JoinNoId(unsigned char id) {
    if (id < kIds || !bof3::BehaviourOn(bof3::behaviours::Id::SoloCharacter)) return false;
    const bool none = NoIdFor(Solo_Active(), id);
    if (none) bof3::Log("DIV-0228 solo scenes: Party_Join(%02X) of a slot the fold emptied: nothing joined", id);
    return none;
}

void SoloScenes_SelfTest() {
    int checks = 0;
    const auto expect = [&](bool ok, const char* name) {
        if (!ok) bof3::Fatal("solo scenes self-test: case \"%s\" does not hold", name);
        ++checks;
    };
    // Walker slots: off, the raw byte whatever the count; on, 0xFF / 0 past it.
    for (unsigned count = 0; count <= 3; ++count)
        for (unsigned slot = 0; slot < 3; ++slot) {
            expect(MemberIdFor(false, slot, count, 0x42) == 0x42, "member id, mode off: the raw byte");
            expect(SlotPoseFor(false, slot, count, 0x02) == 0x02, "walk-in byte, mode off: the raw byte");
            expect(MemberIdFor(true, slot, count, 0x42) == (slot < count ? 0x42 : kNone), "member id, mode on");
            expect(SlotPoseFor(true, slot, count, 0x02) == (slot < count ? 0x02 : 0), "walk-in byte, mode on");
        }
    // Saves: off leaves the earlier save's bytes; on pads past n.
    {
        unsigned char s[3] = {5, 6, 8};
        PadFor(false, s, 1);
        expect(s[0] == 5 && s[1] == 6 && s[2] == 8, "pad, mode off: nothing");
        PadFor(true, s, 3);
        expect(s[0] == 5 && s[1] == 6 && s[2] == 8, "pad, three saved: nothing");
        PadFor(true, s, 2);
        expect(s[0] == 5 && s[1] == 6 && s[2] == kNone, "pad, two saved: the third 0xFF");
        PadFor(true, s, 0);
        expect(s[0] == kNone && s[1] == kNone && s[2] == kNone, "pad, none saved: all 0xFF");
    }
    // Party_Remove's test.
    {
        const unsigned char first[3] = {7, 2, kNone}, second[3] = {2, 7, kNone};
        expect(!NotWalkingFor(false, 5, first, second, 2), "remove, mode off: always the original");
        expect(!NotWalkingFor(true, 7, first, second, 2), "remove, the hero walking");
        expect(!NotWalkingFor(true, 2, first, second, 2), "remove, the other walking");
        expect(NotWalkingFor(true, 5, first, second, 2), "remove, a reserve member: nothing");
        expect(NotWalkingFor(true, 0, first, second, 2), "remove, an ecx byte (0): nothing");
        expect(NotWalkingFor(true, kNone, first, second, 2), "remove, a padded 0xFF: nothing");
        expect(NotWalkingFor(true, 7, first, second, 0), "remove, no one walking: nothing");
        const unsigned char f3[3] = {7, 2, 4}, s3[3] = {7, 2, 4};
        expect(!NotWalkingFor(true, 4, f3, s3, 3), "remove, three walking: the original");
        const unsigned char f6[3] = {7, 2, kNone}, s6[3] = {5, kNone, kNone};
        expect(!NotWalkingFor(true, 5, f6, s6, 2), "remove, in the second list only (a scene's own): the original");
        // A count past three reads the three bytes each list has, no more.
        expect(NotWalkingFor(true, 9, f3, s3, 7), "remove, a count past three: the lists' three bytes");
    }
    // Party_Join's test.
    expect(!NoIdFor(false, kNone), "join, mode off: the original");
    expect(NoIdFor(true, kNone), "join of 0xFF: nothing");
    expect(NoIdFor(true, 24), "join of 24: nothing");
    expect(!NoIdFor(true, 23), "join of 23: the original");
    expect(!NoIdFor(true, 7), "join of 7: the original");
    // A folded team through a save, a leave-all and a restore, as the chapter
    // 9/10 pair ScenaCall_SaveAndLeave then Scena10_RestoreSaved runs it
    // (scena_calls.cpp, scena_10_2.cpp), on state of its own: walkers 7, 2,
    // ObjTrio[2] off with a stale 4; the saved bytes stale 0, 0, 5.
    {
        unsigned char first[3] = {7, 2, kNone}, second[3] = {7, 2, kNone};
        unsigned count = 2;
        const unsigned char trio[3] = {7, 2, 4};
        unsigned char saved[3] = {0, 0, 5};
        // the save: count ids, the rest padded
        for (unsigned i = 0; i < count; ++i) saved[i] = MemberIdFor(true, i, count, trio[i]);
        PadFor(true, saved, count);
        expect(saved[0] == 7 && saved[1] == 2 && saved[2] == kNone, "folded save: 7 2 FF");
        // the leave of each not 7: 2 walks, then the padded byte walks nowhere
        const auto leave = [&](unsigned char id) {
            if (NotWalkingFor(true, id, first, second, count)) return;
            unsigned k = 0;
            while (k < count && first[k] != id) ++k;
            for (; k + 1 < count; ++k) first[k] = first[k + 1];
            first[count - 1] = kNone;
            k = 0;
            while (k < count && second[k] != id) ++k;
            for (; k + 1 < count; ++k) second[k] = second[k + 1];
            second[count - 1] = kNone;
            --count;
        };
        for (unsigned char id : saved)
            if (id != 7) leave(id);
        expect(count == 1 && first[0] == 7 && second[0] == 7, "folded leave-all: the hero stays, alone");
        // the restore joins each saved id that is not 7 and is a character
        unsigned joins = 0;
        for (unsigned char id : saved)
            if (id != 7 && !NoIdFor(true, id)) ++joins;
        expect(joins == 1, "folded restore: one join (2), the 0xFF none");
    }
    bof3::Log("solo scenes self-test: %d checks passed", checks);
}

void SoloScenes_Inject() {
    if (bof3::IsExeHost() ? bof3::os::GetEnv("BOF3X_SELFTEST_ONLY", nullptr, 0) > 0 : bof3::WantsShadow("solo_scenes"))
        SoloScenes_SelfTest();
}

}  // namespace solo
