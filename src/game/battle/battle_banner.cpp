// Moved here by tools/move_funcs.py from battle_e4.cpp, battle_misc.cpp, battle_odds.cpp, battle_setup.cpp (docs/source-layout-round.md).
#include "game/battle/battle_banner.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/battle_e4.h"
#include "game/battle_e4_callees.h"
#include "game/battle_misc.h"
#include "game/battle_misc_callees.h"
#include "game/battle_odds.h"
#include "game/battle_odds_callees.h"
#include "game/battle_setup.h"
#include "game/battle_setup_callees.h"
#include "game/battle_text.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_e4::at;

}  // namespace

namespace {

using namespace battle_misc;

using namespace battle_odds;

}  // namespace

using namespace battle_setup;

namespace {

using move_script::At;

using move_script::Word;

// The party's records by a byte (0..2 in play), the enemies' objects by an
// actor byte (3..10: object actor - 3), as every original indexes them - the
// byte alone, the product wrapping as 32-bit arithmetic does.
unsigned char* PartyBattleE4(unsigned member) { return move_script::At(::bof3::Img(ObjTrio_addr) + (member & 0xFF) * battle_e4::at::kPartyStride); }

unsigned char* CharRecord(unsigned index) { return move_script::At(::bof3::Img(CharacterRecords_addr) + (index & 0xFF) * battle_e4::at::kCharStride); }

unsigned char* AtBattleMisc(battle_misc::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

battle_misc::U LongBattleMisc(const unsigned char* p) {
    battle_misc::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

void PutLong(unsigned char* p, battle_misc::U v) { std::memcpy(p, &v, sizeof v); }

battle_misc::U Addr(const void* p) { return Low32(p); }

unsigned char* Banner(battle_misc::U i) { return AtBattleMisc(::bof3::Img(battle_misc::kBanners.v) + i * 0xC); }

// The shared fill of BattleBanner_Add and BattleBanner_Set: +0 |= 1, +1 kind,
// +2 b2, +3 layer, word +8 the timer's byte, +4 the text, +0xA = 0, in the
// original's order.
void FillBanner(unsigned char* entry, battle_misc::U kind, battle_misc::U b2, battle_misc::U layer, battle_misc::U timer, battle_misc::U text) {
    entry[0] = static_cast<unsigned char>(entry[0] | 1);
    entry[1] = static_cast<unsigned char>(kind);
    entry[2] = static_cast<unsigned char>(b2);
    entry[3] = static_cast<unsigned char>(layer);
    PutWord(entry + 8, timer & 0xFF);
    PutLong(entry + 4, text);
    entry[0xA] = 0;
}

unsigned char* AtBattleOdds(battle_odds::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

battle_odds::U WordBattleOdds(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

battle_odds::U BannerCurrent() { return BattleMisc_BannerCurrent; }

}  // namespace

// original 0x44A5C0 (PSX 0x801DE404 BattleBanner_Dispatch): kBannerKinds = 0; for
// layer 0 then 1, each of the 8 entries: made kBannerCurrent (every entry, before
// any test); skipped unless active, on this layer, and its kind byte shares no
// bit with kBannerKinds (read again for each entry); then SharedScratch = its
// index (the PSX scratchpad byte) and the handler for its kind byte, read
// again, through a five-entry table the original builds on its stack: a bare
// ret (0x437CC0) for kinds 0, 3 and 4, 0x44A740 for kind 1, 0x44A7B0 for kind
// 2 (the PSX's kind 4 handler is an empty function too). No bound on the kind:
// index 5 of the original's table is its own return address, which cannot be
// reproduced, so ours aborts loudly (CLAUDE.md rule 4) - the kinds the callers
// push are constants 1 and 2 (docs/battle_misc.md section 1).
extern "C" void __cdecl BattleBanner_Dispatch(void) {
    BattleMisc_BannerKinds = 0;
    for (battle_misc::U layer = 0; layer < 2; ++layer) {
        for (battle_misc::U i = 0; i < 8; ++i) {
            unsigned char* const entry = Banner(i);
            BattleMisc_BannerCurrent = Addr(entry);
            if (entry[0] == 0) continue;
            if (entry[3] != layer) continue;
            if (BattleMisc_BannerKinds & entry[1]) continue;
            SharedScratch.b_00 = static_cast<unsigned char>(i);
            const battle_misc::U kind = entry[1];
            if (kind >= 5) bof3::Fatal("banner entry %u has kind %u; the original's stack table holds five", i, kind);
            battle_misc::g.banner[kind]();
        }
    }
}

// original 0x44A650 (PSX 0x801DE528): the first inactive entry from 2 to 7 filled
// (FillBanner); entries 0 and 1 are BattleBanner_Set's to name. eax is the
// entry's offset (index * 12), or 0x15 (7 * 3, the last index's lea) when every
// entry is taken, and nothing is written. Several of the 31 callers only
// overwrite al before returning, so ours returns the register.
extern "C" unsigned long __cdecl BattleBanner_Add(unsigned kind, unsigned b2, unsigned layer, unsigned timer,
                                                  const char* text) {
    for (battle_misc::U i = 2; i < 8; ++i) {
        if (Banner(i)[0] != 0) continue;
        FillBanner(Banner(i), kind, b2, layer, timer, Addr(text));
        return i * 12;
    }
    return 7 * 3;
}

// original 0x44A6E0 (PSX 0x801DE5D4): entry (slot & 0xFF) filled, no bound. eax
// the entry's offset.
extern "C" unsigned long __cdecl BattleBanner_Set(unsigned slot, unsigned kind, unsigned b2, unsigned layer,
                                                  unsigned timer, const char* text) {
    FillBanner(Banner(slot & 0xFF), kind, b2, layer, timer, Addr(text));
    return (slot & 0xFF) * 12;
}

// original 0x44A740 (BattleBanner_Dispatch's kind 1; the name banner
// BattleBanner_ShowName sets): the entry kBannerCurrent points at. A timer
// (word +8) of 0xFF runs forever. Otherwise the kind byte is or'ed into the
// kinds seen, the timer counted down, and while it has not reached 0 - both
// read through kBannerCurrent again - window record 4's +0xA gets
// SharedScratch (the entry's index, which the dispatch stored). At 0: the
// entry's active byte 0; its kind taken back out of the kinds seen (the
// pointer read again, the kinds byte before the kind byte); then
// BattleBanner_NoneOfKind(1), and when no kind-1 entry is left (al set)
// window record 4's +3 = 0. eax is the entry pointer, or NoneOfKind's eax.
extern "C" unsigned long __cdecl BattleBanner_TickKind1(void) {
    unsigned char* e = AtBattleOdds(BannerCurrent());
    if (WordBattleOdds(e + 8) != 0xFF) {
        BattleMisc_BannerKinds = static_cast<unsigned char>(BattleMisc_BannerKinds | e[1]);
        PutWord(e + 8, WordBattleOdds(e + 8) - 1);
        e = AtBattleOdds(BannerCurrent());
        if (WordBattleOdds(e + 8) == 0) {
            e[0] = 0;
            const battle_odds::U entry = BannerCurrent();
            const unsigned char kinds = BattleMisc_BannerKinds;
            const unsigned char kind = AtBattleOdds(entry + 1)[0];
            BattleMisc_BannerKinds = static_cast<unsigned char>(kinds & static_cast<unsigned char>(~kind));
            const battle_odds::U eax = battle_odds::g.none_of_kind(1);
            if (eax & 0xFF) WindowRecords[4].state = 0;
            return eax;
        }
    }
    Window4_Index = SharedScratch.b_00;
    return Low32(e);
}

// original 0x44A7B0 (kind 2; PSX 0x801DE71C): as kind 1 with three
// differences. The kind is or'ed into the kinds seen before the timer is
// tested, even for 0xFF; it is never taken back out; and the test is
// NoneOfKind(4), after which window record 0's +3 = 2. While the timer runs,
// record 0's +0xA gets SharedScratch. The word is decremented through a
// register (only its low 16 bits stored) and the pointer read again only then;
// at 0xFF the entry the first read found is tested, which is never 0.
extern "C" unsigned long __cdecl BattleBanner_TickKind2(void) {
    unsigned char* e = AtBattleOdds(BannerCurrent());
    const unsigned char kinds = BattleMisc_BannerKinds;
    BattleMisc_BannerKinds = static_cast<unsigned char>(kinds | e[1]);
    const battle_odds::U timer = WordBattleOdds(e + 8);
    if (timer != 0xFF) {
        PutWord(e + 8, timer - 1);
        e = AtBattleOdds(BannerCurrent());
    }
    if (WordBattleOdds(e + 8) == 0) {
        e[0] = 0;
        const battle_odds::U eax = battle_odds::g.none_of_kind(4);
        if (eax & 0xFF) WindowRecords[0].state = 2;
        return eax;
    }
    Window0_ParamA = SharedScratch.b_00;
    return Low32(e);
}

// original 0x44A810 (PSX 0x801DE7CC): bytes +0, +1 and +2 of all 8 entries to 0.
// eax is the walk's pointer past the last entry, 0x93B941.
extern "C" unsigned long __cdecl BattleBanner_ClearAll(void) {
    for (battle_misc::U i = 0; i < 8; ++i) {
        Banner(i)[0] = 0;
        Banner(i)[1] = 0;
        Banner(i)[2] = 0;
    }
    return ::bof3::Img(battle_misc::kBanners.v) + 1 + 8 * 12;
}

// original 0x44A830 (PSX 0x801DE820 BattleBanner_NoneOfKind): al 0 when an active
// entry's kind byte is kind & 0xFF, else 1 - and eax is exactly that (the
// upper bits are the entry offset's, which is below 0x100).
extern "C" unsigned char __cdecl BattleBanner_NoneOfKind(unsigned kind) {
    for (battle_misc::U i = 0; i < 8; ++i)
        if (Banner(i)[0] != 0 && Banner(i)[1] == (kind & 0xFF)) return 0;
    return 1;
}

// original 0x44A880 (PSX 0x801DE888): queue entry kQueueWrite (a byte, no bound
// on what it indexes) gets +0 = a, +1 = b, dword +4 = value; kQueueWrite =
// (index + 1) & 0xF. eax the entry's offset, index * 8 (0x42FF2F returns it).
extern "C" unsigned long __cdecl BattleQueue_Push(unsigned a, unsigned b, unsigned long value) {
    const battle_misc::U index = BattleActorCopies_QueueWrite;
    unsigned char* const entry = AtBattleMisc(::bof3::Img(kQueue.v) + index * 8);
    entry[0] = static_cast<unsigned char>(a);
    entry[1] = static_cast<unsigned char>(b);
    PutLong(entry + 4, static_cast<battle_misc::U>(value));
    BattleActorCopies_QueueWrite = static_cast<unsigned char>((index + 1) & 0xF);
    return index * 8;
}

// original 0x44A8C0 (PSX 0x801DE8F4): al 1 while the write index differs from the
// read index. The one caller (0x42E942) tests al.
extern "C" unsigned char __cdecl BattleQueue_Pending(void) {
    return BattleActorCopies_QueueWrite != BattleWindows_MsgHead;
}

// original 0x44A8E0 (PSX 0x801DE914): Str_CopyN(kBannerText, kMessages[msg &
// 0xFF], 8) - the PC's table of string pointers where the PSX indexes 8-byte
// strings - then banner entry 0's byte +2 = b2. Returns Str_CopyN's eax.
// DIVERGENCE DIV-0052: 12 bytes, the US release's count, once a language
// overlay has repointed the table at its longer words (src/game/battle_text.cpp).
extern "C" unsigned long __cdecl BattleBanner_SetMessage(unsigned msg, unsigned b2) {
    const battle_misc::U eax = battle_misc::g.copy_n(::bof3::Img(kBannerText.v), LongBattleMisc(AtBattleMisc(::bof3::Img(kMessages.v) + (msg & 0xFF) * 4)), BattleMessages_CopyRoom());
    Banner(0)[2] = static_cast<unsigned char>(b2);
    return eax;
}

// original 0x44A910 (PSX 0x801DE964): the member's name (its character record
// by 0x66972C of its +0x89) into Text_Records[0], 8 bytes (Str_CopyN).
extern "C" void __cdecl Battle_MemberNameToText(unsigned member) {
    const unsigned char c = Character_IndexById[rv::As<records::PartyMember>(PartyBattleE4(member))->char_id];
    BH_CALL(Str_CopyN)(reinterpret_cast<char*>(At(::bof3::Img(battle_e4::at::kTextRecord0.v))), reinterpret_cast<const char*>(CharRecord(c)), 8);
}

// original 0x44A960 (PSX 0x801DE9D4, gap20): the enemy's 12-byte name (its
// working record's head) into Text_Records[0].
extern "C" void __cdecl Battle_EnemyNameToText(unsigned actor) {
    BH_CALL(Str_CopyN)(reinterpret_cast<char*>(At(::bof3::Img(battle_e4::at::kTextRecord0.v))),
                       reinterpret_cast<const char*>(At(::bof3::Img(Battle_Enemies_addr) + 0x80 + ((actor & 0xFF) - 3u) * battle_e4::at::kEnemyStride)), 0xC);
}

// original 0x44A990 (PSX 0x801DEA20): an actor's name as banner 0.
// Str_CopyN(kBannerText, actor + 0x80, 8) - the enemy object's name, the 16
// bytes the port prepended to the working record (the PSX copies 5 from +0x74);
// when kSuffixOn is set, the string kNameSuffix points at appended, inline:
// its length plus its NUL measured first, then that many bytes copied to the
// text's NUL, a dword at a time and the rest by bytes (repne scasb, rep movsd,
// rep movsb). Then BattleBanner_Set(0, 1, 1, 0, 0xFF, kBannerText), whose eax
// is returned.
extern "C" unsigned long __cdecl BattleBanner_ShowName(const unsigned char* actor) {
    battle_misc::g.copy_n(::bof3::Img(kBannerText.v), Addr(actor) + 0x80, 8);
    if (BattlePhases_B7A != 0) {
        const battle_misc::U src = BattleMisc_NameSuffix;
        battle_misc::U n = 0;
        while (AtBattleMisc(src + n)[0] != 0) ++n;
        ++n;
        battle_misc::U dst = ::bof3::Img(kBannerText.v);
        while (AtBattleMisc(dst)[0] != 0) ++dst;
        battle_misc::U k = 0;
        for (; k + 4 <= (n & ~3u); k += 4) PutLong(AtBattleMisc(dst + k), LongBattleMisc(AtBattleMisc(src + k)));
        for (battle_misc::U r = 0; r < (n & 3); ++r, ++k) AtBattleMisc(dst + k)[0] = AtBattleMisc(src + k)[0];
    }
    return battle_misc::g.banner_set(0, 1, 1, 0, 0xFF, ::bof3::Img(kBannerText.v));
}

// original 0x44AA00 (PSX 0x801DEA9C, the same): one of two system lines at
// random (Rand & 1) by 0x904B90: 1 lines 0 / 1, 2..4 lines 2 / 3, 0 or above
// 4 lines 4 / 0 - the six bytes the original builds on its stack - put
// through 0x44A6E0(0, 2, 0, 0, 0xFF, text).
extern "C" void __cdecl Battle_OpeningMessage() {
    static constexpr unsigned char kLines[6] = {0, 1, 2, 3, 4, 0};
    const std::uint32_t kind = static_cast<std::uint32_t>(move_script::Long(At(::bof3::Img(battle_setup::at::kOpening.v))));
    const unsigned base = kind == 1 ? 0 : (kind == 0 || kind > 4) ? 4 : 2;
    const unsigned id = kLines[base + (static_cast<unsigned>(battle_setup::g.rand()) & 1)];
    battle_setup::g.message_at(0, 2, 0, 0, 0xFF, battle_setup::g.msg(id));
}

// original 0x44AA90 (PSX 0x801DEB80): a banner line - system message
// 0x64E52C[row][column] (u16, ten to a row; neither bounded) through
// BattleBanner_Add(2, 0, 0, 0x2D, it).
extern "C" void __cdecl BattleBanner_AddLine(unsigned row, unsigned column) {
    const unsigned id = Word(At(::bof3::Img(battle_e4::at::kBannerLines.v) + 2 * ((row & 0xFF) * 10 + (column & 0xFF))));
    const auto* const line = reinterpret_cast<const char*>(BH_CALL(Msg_SystemPtr)(id));
    BH_CALL(BattleBanner_Add)(2, 0, 0, 0x2D, line);
}

void BattleBanner_Inject() {
    BOF3_INJECT(BattleBanner_Dispatch);
    BOF3_INJECT(BattleBanner_Add);
    BOF3_INJECT(BattleBanner_Set);
    BOF3_INJECT(BattleBanner_TickKind1);
    BOF3_INJECT(BattleBanner_TickKind2);
    BOF3_INJECT(BattleBanner_ClearAll);
    BOF3_INJECT(BattleBanner_NoneOfKind);
    BOF3_INJECT(BattleQueue_Push);
    BOF3_INJECT(BattleQueue_Pending);
    BOF3_INJECT(BattleBanner_SetMessage);
    BOF3_INJECT(Battle_MemberNameToText);
    BOF3_INJECT(Battle_EnemyNameToText);
    BOF3_INJECT(BattleBanner_ShowName);
    BOF3_INJECT(Battle_OpeningMessage);
    BOF3_INJECT(BattleBanner_AddLine);
}
