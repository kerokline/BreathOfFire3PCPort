// BOF3X_SHADOW=scena_sx: group SX's eighteen functions through the scenario
// harness (scenario_harness.h), once at start-up. docs/scena_sx.md section 4.
//
// The clone table is tools/scenario_rows.py's machinery with the nineteen
// addresses in place of SE's (2026-09-28, capstone, every jump internal, no
// jump table), names given, 0x587B80 (not taken) left out. One correction by
// hand: the tool's descent stops MapView_FillCells at 0x10 bytes ("REFUSED
// +0xE short jmp out to 0x56fcb4") - its entry jumps over its own loop head
// 0x56FCB0 - where the function runs to its ret at +0x76 (0x77 bytes, read
// 2026-09-28; pc_funcs.json says 119); its two calls are added. Each
// function's call shape (none is a vtable slot, a hook or a state handler;
// every one is a direct cdecl callee):
//
//   Char_LevelUp            1 word (a byte used)
//   Party_PlaceForBattle    3 words: x, z, the event battle (a byte)
//   Party_ReloadPalettes    none
//   Party_HealJoined        none
//   Party_Remove            1 word (a byte used)
//   Sprite_FlashClut        1 word (a byte used, below 4)
//   Char_LoseHp             2 words, answers in eax (Field_Bit80Tick reads ax)
//   Field_SetStatus80       none
//   Field_CellTriggerAt     4 words: the table, the count, x, z; answers in eax
//   MapView_FillCells       none
//   Camera_TurnToDegrees    2 words, answers in eax (0x57C5A0's)
//   Camera_EaseAngleFB      2 words, answers in al
//   Sprite_FindFree         none, answers in al
//   AbilityList_Add         4 words, answers in al
//   KeyItem_Add             1 word, answers in al
//   Inventory_Remove        3 words, answers in al
//   Zenny_Sub               1 word, answers in al
//   Zenny_Add               2 words, answers in al
//
// None reads the chapter bytes or the flag row, so the chapter is 0.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sx.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sx {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// scenario_rows.py's clone table (SE's machinery over the nineteen), 2026-09-28, names given.
constexpr sh::CallSite kCalls498DE0[] = {{0x1AE, 0x590C90}, {0x1C0, 0x590C90}, {0x1E3, 0x590660}};
constexpr sh::CallSite kCalls532ED0[] = {{0x88, 0x532FD0}, {0xD6, 0x589330}};
constexpr sh::CallSite kCalls533E00[] = {{0x1F, 0x454DC0}, {0x27, 0x5366A0}};
constexpr sh::CallSite kCalls533E50[] = {{0x16, 0x590660}};
constexpr sh::CallSite kCalls534030[] = {{0x2, 0x534010}, {0x129, 0x536730}};
constexpr sh::CallSite kCalls534DB0[] = {{0x74, 0x587740}};
constexpr sh::CallSite kCalls56FCA0[] = {{0x58, 0x56F910}, {0x6C, 0x571FF0}};   // by hand, the extent's correction
constexpr sh::CallSite kCalls57C550[] = {{0x3E, 0x57C5A0}};
constexpr sh::CallSite kCalls590C90[] = {{0x1E, 0x591EC0}};
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define SX_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kE = sh::Shape::kEntry;
const sh::Clone kClones[] = {
    {"Char_LevelUp", 0x498DE0, 0x1F3, kCalls498DE0, SH_N(kCalls498DE0), nullptr, 0, nullptr, 0, SX_FN(Char_LevelUp), 0, false, kE},
    {"Party_PlaceForBattle", 0x532ED0, 0xF9, kCalls532ED0, SH_N(kCalls532ED0), nullptr, 0, nullptr, 0, SX_FN(Party_PlaceForBattle), 0, false, kE},
    {"Party_ReloadPalettes", 0x533E00, 0x48, kCalls533E00, SH_N(kCalls533E00), nullptr, 0, nullptr, 0, SX_FN(Party_ReloadPalettes), 0, false, kE},
    {"Party_HealJoined", 0x533E50, 0x94, kCalls533E50, SH_N(kCalls533E50), nullptr, 0, nullptr, 0, SX_FN(Party_HealJoined), 0, false, kE},
    {"Party_Remove", 0x534030, 0x166, kCalls534030, SH_N(kCalls534030), nullptr, 0, nullptr, 0, SX_FN(Party_Remove), 0, false, kE},
    {"Sprite_FlashClut", 0x534DB0, 0x96, kCalls534DB0, SH_N(kCalls534DB0), nullptr, 0, nullptr, 0, SX_FN(Sprite_FlashClut), 0, false, kE},
    {"Char_LoseHp", 0x537480, 0x78, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(Char_LoseHp), 0xFFFFFFFFu, false, kE},
    {"Field_SetStatus80", 0x56D6F0, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(Field_SetStatus80), 0, false, kE},
    {"Field_CellTriggerAt", 0x56D800, 0xB0, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(Field_CellTriggerAt), 0xFFFFFFFFu, false, kE},
    {"MapView_FillCells", 0x56FCA0, 0x77, kCalls56FCA0, SH_N(kCalls56FCA0), nullptr, 0, nullptr, 0, SX_FN(MapView_FillCells), 0, false, kE},
    {"Camera_TurnToDegrees", 0x57C550, 0x47, kCalls57C550, SH_N(kCalls57C550), nullptr, 0, nullptr, 0, SX_FN(Camera_TurnToDegrees), 0xFFFFFFFFu, false, kE},
    {"Camera_EaseAngleFB", 0x57C6B0, 0xEE, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(Camera_EaseAngleFB), 0xFF, false, kE},
    {"Sprite_FindFree", 0x57CD90, 0x30, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(Sprite_FindFree), 0xFF, false, kE},
    {"AbilityList_Add", 0x590C90, 0x4F, kCalls590C90, SH_N(kCalls590C90), nullptr, 0, nullptr, 0, SX_FN(AbilityList_Add), 0xFF, false, kE},
    {"KeyItem_Add", 0x591900, 0x20, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(KeyItem_Add), 0xFF, false, kE},
    {"Inventory_Remove", 0x591B60, 0x5D, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(Inventory_Remove), 0xFF, false, kE},
    {"Zenny_Sub", 0x591BC0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(Zenny_Sub), 0xFF, false, kE},
    {"Zenny_Add", 0x591BE0, 0x3A, nullptr, 0, nullptr, 0, nullptr, 0, SX_FN(Zenny_Add), 0xFF, false, kE},
};
#undef SX_FN
#undef SH_N

enum : unsigned {
    kLevelUp, kPlace, kPalettes, kHeal, kRemove, kFlash, kLoseHp, kStatus80, kCellAt, kFill, kTurn, kEase, kFindFree,
    kAbility, kKeyItem, kInvRemove, kZennySub, kZennyAdd, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define SX_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr std::uint32_t kAll = 0xFFFFFFFFu;

unsigned char* Mem(std::uint32_t a) { return sh::Mem(a); }

// SH_PICK over values computed each call (SH_PICK's list is a static, fixed
// at its first use).
template <typename... T> std::uint32_t PickOf(T... v) {
    const std::uint32_t values[] = {static_cast<std::uint32_t>(v)...};
    return sh::Pick(values, sizeof...(v));
}
unsigned char* Rec(unsigned n) { return Mem(bof3::addr::CharacterRecords + (n & 7) * at::kRecordStride); }
unsigned char* Obj(unsigned n) { return ObjTrio + (n % 3) * at::kObjStride; }

// Field_CellTriggerAt's table: eight 5-byte records of the fuzz's own.
alignas(16) unsigned char g_cells[0x28];

// --- the moves --------------------------------------------------------------------
//
// What the functions read again after a call and a caller could have moved:
// the level-up's and the heal's record bytes (the bias bytes, the stat words,
// +0x20 / +0x22 / +0x2E), the member count, the formation byte, Field_State,
// MapView_Column, 0x904060, a list byte, the event battle's x, Sprite_Current.
// The harness's disturbance reaches a group cell about one call in 24, so the
// group's own callees also move one after each call (Stir, from the
// recorders' stream).
void Move(std::uint32_t h) {
    const unsigned v = (h >> 8) & 0xFF;
    const unsigned w = (h >> 16) & 0xFF;
    switch (h % 11) {
    case 0: {
        static const unsigned char kOffsets[] = {0x0A, 0x20, 0x22, 0x2E, 0x40, 0x42, 0x44, 0x46, 0x48, 0x4A, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x0B};
        Rec(v)[kOffsets[w % sizeof kOffsets]] = static_cast<unsigned char>(h >> 24);
        break;
    }
    case 1: Field_MemberCount = static_cast<unsigned char>(v % 4); break;
    case 2: Mem(at::kFormation)[0] = static_cast<unsigned char>(v); break;
    case 3: Field_State = Obj(v); break;
    case 4: MapView_Column = static_cast<short>(v % 0x1C); break;
    case 5: Mem(at::kLeaderSlot)[0] = static_cast<unsigned char>(v % 5); break;
    case 6: Mem(at::kPartyLists)[v % 6] = static_cast<unsigned char>(w % 24); break;
    case 7: SetLong(Mem(v & 1 ? at::kBattleZ : at::kBattleX), static_cast<std::int32_t>(h)); break;
    case 8: Sprite_Current = Obj(v); break;
    case 9: Obj(v)[0x89] = static_cast<unsigned char>(w % 24); break;
    default: break;
    }
}

std::uint32_t Stir(const std::uint32_t*, std::uint32_t answer) {
    Move(sh::Noise());
    return answer;
}

// 0x591EC0's answer is the list AbilityList_Add fills: one of the four
// 10-byte lists of a record inside the region (the real one gives the
// member's record +0x60 / +0x6A / +0x74 / +0x7E by the id's class).
std::uint32_t ListOf(const std::uint32_t* a, std::uint32_t answer) {
    static const unsigned char kLists[] = {0x60, 0x6A, 0x74, 0x7E};
    return Key(Rec(a[0]) + kLists[answer & 3]);
}

const sh::Callee kCallees[] = {
    // ours, called by name
    {SX_OURS(AbilityList_Add), 4, {0xFF, 0xFF, 0xFF, 0xFF}, sh::Answer::kFlag, 0, 0, {}, &Stir},
    {SX_OURS(Char_RecalcStats), 1, {kAll}, kG, 0, 0, {}, &Stir},
    {SX_OURS(Sprite_ReleaseTint), 1, {kAll}, kG, 0, 0, {}, &Stir},
    {SX_OURS(Sprite_LoadPalette), 2, {kAll, kAll}, kG, 0, 0, {}, &Stir},
    {SX_OURS(Party_JoinReset), 0, {}, kG, 0, 0, {}, &Stir},
    {SX_OURS(Member_ClearState), 1, {kAll}, kG, 0, 0, {}, &Stir},
    {SX_OURS(MapView_CellToMap), 3, {kAll, kAll, kAll}, kG, 0, 0, {}, &Stir},
    {SX_OURS(MapView_PlaceRuns), 0, {}, kG, 0, 0, {}, &Stir},
    {SX_OURS(Sound_PlayEffect), 1, {0xFFFF}, kG, 0, 0, {}, &Stir},
    // nobody's, by address
    {"0x532FD0", at::kFormationPlace, at::kFormationPlace, 3, {kAll, kAll, kAll}, kG, 0, 0, {}, &Stir},
    {"0x591EC0", at::kAbilityListOf, at::kAbilityListOf, 3, {kAll, kAll, kAll}, kG, 0, 0, {}, &ListOf},
    {"0x57C5A0", at::kCameraTurnYaw, at::kCameraTurnYaw, 2, {kAll, kAll}, sh::Answer::kFlag, 0, 0},
};
#undef SX_OURS

// --- the state -----------------------------------------------------------------------

// Beyond the harness's 22 (which hold Cond_Flags with the zenny 0x904058,
// 0x904060 and both party lists; Field_MemberCount, Camera_Angles,
// Cond_AngleFB; Game_AreaNumber; Sprite_Current; Sprite_Kind2; ObjTrio and
// Field_State; Sprite_Objects; MapView_Row / Column).
const sh::Region kRegions[] = {
    {bof3::addr::CharacterRecords, 8 * at::kRecordStride},
    {0x904098, 0x9045F4 - 0x904098},   // 0x904138; the inventory's lists, the key items, the shared ability list
    {0x904AA0, 0xB0},                  // the battle bytes: the formation 0x904AAC
    {at::kBattleX, 8},
    {at::kSlotPositions, 0x20},
    {Key(Gfx_ClutStrip), 0x4000},
    {at::kEaseStep, 4},
    {at::kEaseAcc, 8},
    {Key(ObjTrio) - at::kObjStride, at::kObjStride},   // Party_Remove at a count of 0 clears its +0
    {Key(g_cells), sizeof g_cells},
};

void Disturb(std::uint32_t h) { Move(h); }

// --- the seed ------------------------------------------------------------------------

// The EXP a record needs for `level` (the sum of its rows 1..level).
std::int32_t ExpFor(unsigned id, unsigned level) {
    const unsigned char* const t = move_script::At(bof3::addr::Char_ExpTable);
    std::int32_t sum = 0;
    for (unsigned l = 1; l <= level && l < 99; ++l) sum += Word(t + (l + id * 99) * 8);
    return sum;
}

void SeedParty() {
    // the member count 0..3 (every loop over it stays inside ObjTrio), mostly 1..3
    Field_MemberCount = static_cast<unsigned char>(sh::Often() ? 1 + sh::Next() % 3 : sh::Next() % 4);
    unsigned char* const lists = Mem(at::kPartyLists);
    for (unsigned i = 0; i < 6; ++i) lists[i] = static_cast<unsigned char>(sh::Next() % 24);
    if (sh::Often()) {
        const unsigned r = sh::Next() % 3;
        for (unsigned j = 0; j < 3; ++j) lists[3 + j] = lists[(j + r) % 3];
    }
    if (sh::Half()) lists[sh::Next() % 6] = 0xFF;
    for (unsigned i = 0; i < 3; ++i) {
        Obj(i)[0x89] = sh::Often() ? lists[sh::Next() % 3] : static_cast<unsigned char>(sh::Next() % 24);
        Obj(i)[0x148] = static_cast<unsigned char>(sh::Next() % 8);
    }
}

void Seed(unsigned k) {
    SeedParty();
    switch (k) {
    case kLevelUp:
        for (unsigned r = 0; r < 8; ++r) {
            unsigned char* const rec = Rec(r);
            const unsigned old = PickOf(0, 1, 1 + sh::Next() % 97, 1 + sh::Next() % 40, 97, 98, 99, 100 + sh::Next() % 150);
            rec[0xA] = static_cast<unsigned char>(old);
            const unsigned target = old < 99 ? old + sh::Next() % 6 : sh::Next() % 99;
            std::int32_t exp = ExpFor(r, target) + static_cast<std::int32_t>(PickOf(0, 0, 1, 0xFFFFFFFFu, sh::Next() % 50));
            if (sh::Next() % 16 == 0) exp = static_cast<std::int32_t>(PickOf(0x80000000u, 0x7FFFFFFFu, 0xFFFFFFFFu));
            SetLong(rec + 0xC, exp);
            for (unsigned s = 0; s < 6; ++s)
                SetWord(rec + 0x40 + 2 * s, PickOf(sh::Next() % 1000, 990 + sh::Next() % 10, 999, 1000 + sh::Next() % 10,
                                                    0xFFF0 + sh::Next() % 16, sh::Next() % 0x10000));
        }
        break;
    case kHeal:
        for (unsigned r = 0; r < 8; ++r) Rec(r)[0xB] = static_cast<unsigned char>(Rec(r)[0xB] ^ (sh::Half() ? 1 : 0));
        break;
    case kRemove:
        Mem(at::kLeaderSlot)[0] = static_cast<unsigned char>(sh::Next() % 5);
        break;
    case kLoseHp:
        for (unsigned r = 0; r < 8; ++r) {
            unsigned char* const rec = Rec(r);
            const unsigned max = sh::Next() % 1000;
            SetWord(rec + 0x20, max);
            SetWord(rec + 0x18, PickOf(0, 1, 2, max / 4, max / 4 + 1, sh::Next() % 1000, max));
        }
        break;
    case kCellAt: {
        // the area a byte (the records' +0 is), the leader's facing, records
        // near the cell the arguments name
        Game_AreaNumber = static_cast<unsigned short>(sh::Often() ? sh::Next() % 8 : sh::Next());
        Mem(at::kLeaderFacing)[0] = static_cast<unsigned char>(sh::Next() % 10);
        for (unsigned i = 0; i < 8; ++i) {
            unsigned char* const r = g_cells + 5 * i;
            r[0] = static_cast<unsigned char>(sh::Often() ? Game_AreaNumber : sh::Next() % 8);
            r[1] = static_cast<unsigned char>(sh::Next() % 8);
            r[2] = static_cast<unsigned char>(sh::Next() % 8);
            r[3] = static_cast<unsigned char>((sh::Half() ? 0x80 : 0) | (sh::Next() & 0x70) |
                                              (sh::Often() ? PickOf(8, Mem(at::kLeaderFacing)[0]) : sh::Next() % 16));
            r[4] = static_cast<unsigned char>(PickOf(0, 1, 1, 2, 3, 4, 0xFF));
        }
        break;
    }
    case kFill:
        MapView_Row = static_cast<short>(PickOf(0, 0x36, 0x37, sh::Next() % 0x38, sh::Next() % 0x38, sh::Next() % 0x10000));
        MapView_Column = static_cast<short>(PickOf(0, 0x1A, 0x1B, sh::Next() % 0x1C, sh::Next() % 0x1C, sh::Next() % 0x10000));
        break;
    case kEase:
        SetLong(Mem(at::kEaseLeft), static_cast<std::int32_t>(sh::Half() ? 0 : PickOf(1, 2, 3, 0xFFFFFFFFu, sh::Next() % 40)));
        Mem(at::kKind2Speed)[0] = static_cast<unsigned char>(sh::Next() % 6);   // Field_MoveSpeeds 0, 1, 2, 4, 8, 16
        Mem(at::kKind2Mode)[0] = static_cast<unsigned char>(PickOf(2, 6, sh::Next() % 8, sh::Next()));
        break;
    case kFindFree:
        for (unsigned i = 0; i < at::kSpriteCount; ++i)
            if (Sprite_Objects[i * at::kRecordStride] == 0) Sprite_Objects[i * at::kRecordStride] = 1;
        if (sh::Often()) Sprite_Objects[(sh::Next() % at::kSpriteCount) * at::kRecordStride] = 0;
        if (sh::Half()) Sprite_Objects[(sh::Next() % at::kSpriteCount) * at::kRecordStride] = 0;
        break;
    case kAbility:
    case kKeyItem: {
        // the lists mostly full, a hole now and then
        unsigned char* const shared = Mem(at::kAbilityShared);
        unsigned char* const keys = Mem(at::kKeyItems);
        for (unsigned i = 0; i < 0x80; ++i) shared[i] = static_cast<unsigned char>(shared[i] | 1);
        for (unsigned i = 0; i < 0x20; ++i) keys[i] = static_cast<unsigned char>(keys[i] | 1);
        for (unsigned r = 0; r < 8; ++r)
            for (unsigned i = 0x60; i < 0x88; ++i) Rec(r)[i] = static_cast<unsigned char>(Rec(r)[i] | 1);
        if (sh::Often()) {
            shared[sh::Next() % 0x80] = 0;
            keys[sh::Next() % 0x20] = 0;
            for (unsigned r = 0; r < 8; ++r) Rec(r)[0x60 + sh::Next() % 0x28] = 0;
        }
        break;
    }
    case kZennySub:
    case kZennyAdd:
        SetLong(Mem(bof3::addr::Party_Zenny),
                static_cast<std::int32_t>(PickOf(0, 100, sh::Next() % 10000, at::kZennyCap - sh::Next() % 1000, at::kZennyCap,
                                                  0xFFFFFFF0u + sh::Next() % 16, sh::Next())));
        break;
    default: break;
    }
}

// the arguments each function reads (garbage above the bytes it masks)
void Args(unsigned k, std::uint32_t* a) {
    const std::uint32_t hi = a[9] & 0xFFFFFF00u;
    switch (k) {
    case kLevelUp: a[0] = hi | (sh::Next() % 8); break;
    case kPlace: break;   // x, z any; the event battle any byte (EventBattle_Records read only)
    case kRemove: a[0] = hi | (sh::Often() ? Mem(at::kPartyLists)[sh::Next() % 6] : sh::Next() % 24); break;
    case kFlash: a[0] = hi | (sh::Next() % 4); break;
    case kLoseHp:
        a[0] = PickOf(0, 1, 2, 10, sh::Next() % 1000, sh::Next() % 0x10000, sh::Next());
        a[1] = hi | (sh::Next() % 24);
        break;
    case kCellAt: {
        a[0] = Key(g_cells);
        a[1] = hi | PickOf(0, 1, 2, 4, 8, sh::Next() % 9);
        const unsigned char* const r = g_cells + 5 * (sh::Next() % 8);
        const unsigned d = sh::Next() % 5;
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Often() ? static_cast<unsigned>(r[1] + ((r[3] & 0x80) ? 0 : d)) & 0xFF : sh::Next() % 12);
        a[3] = (a[3] & 0xFFFFFF00u) | (sh::Often() ? static_cast<unsigned>(r[2] + ((r[3] & 0x80) ? d : 0)) & 0xFF : sh::Next() % 12);
        break;
    }
    case kEase: {
        // an angle whose step cannot overflow; a second argument not 0 (the original divides by it)
        a[0] = (a[0] & 0xFFFF0000u) | (static_cast<std::uint32_t>(static_cast<int>(sh::Next() % 1441) - 720) & 0xFFFF);
        const int f = static_cast<int>(1 + sh::Next() % 5);
        a[1] = (a[1] & 0xFFFFFF00u) | (static_cast<std::uint32_t>(sh::Half() ? f : -f) & 0xFF);
        break;
    }
    case kAbility:
        a[0] = hi | PickOf(0, 1 + sh::Next() % 0xFF, sh::Next() % 0x100);
        a[1] = (a[1] & 0xFFFFFF00u) | (sh::Next() % 8);
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Half() ? 0 : 1 + sh::Next() % 0xFF);
        break;
    case kKeyItem: break;
    case kInvRemove: {
        const unsigned cat = sh::Next() % 5;
        a[0] = hi | cat;
        const std::uint32_t ids = static_cast<std::uint32_t>(Long(move_script::At(bof3::addr::Inventory_IdLists + 4 * cat)));
        const unsigned slot = sh::Next() % 0x80;
        unsigned char item = static_cast<unsigned char>(sh::Next());
        if (sh::Often() && (cat != 4 || slot < 0x20)) {
            if (item == 0) item = 1;
            Mem(ids + slot)[0] = item;   // planted; an earlier slot may hold it too
        }
        a[1] = (a[1] & 0xFFFFFF00u) | (sh::Next() % 8 == 0 ? 0 : item);
        unsigned n = 1 + sh::Next() % 99;
        if (cat != 4) {
            const std::uint32_t counts = static_cast<std::uint32_t>(Long(move_script::At(bof3::addr::Inventory_CountLists + 4 * cat)));
            const unsigned have = Mem(counts + slot)[0];
            if (sh::Often()) n = PickOf(have, have + 1, have > 0 ? have - 1 : 0, have / 2 + 1);
        }
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Next() % 10 == 0 ? 0 : n & 0xFF);
        break;
    }
    case kZennySub:
    case kZennyAdd: {
        const auto money = static_cast<std::uint32_t>(Long(Mem(bof3::addr::Party_Zenny)));
        a[0] = PickOf(money, money + 1, money - 1, 0, sh::Next() % 10000, at::kZennyCap - money, at::kZennyCap - money + 1, sh::Next());
        a[1] = (a[1] & 0xFFFFFF00u) | (sh::Half() ? 0 : 1 + sh::Next() % 0xFF);
        break;
    }
    default: break;
    }
}

}  // namespace

void SelfTest() {
    sh::Group group = {
        "scena_sx", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        nullptr, 0, kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 3000, nullptr, 0, &Args,
    };
    group.chapter = 0;   // none of the eighteen reads the chapter bytes or the flag row
    sh::Run(group);
}

}  // namespace scena_sx
