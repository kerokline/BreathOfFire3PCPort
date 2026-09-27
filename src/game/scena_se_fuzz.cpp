// BOF3X_SHADOW=scena_se: group SE's six functions through the scenario
// harness (scenario_harness.h), once at start-up. docs/scena_se.md section 4.
//
// The clone table is tools/scenario_rows.py --unit SE --clones (2026-09-27,
// SCH's 222eb0f; capstone, every jump internal, no jump table, nothing
// REFUSED), names given, the three addresses not taken left out. Each
// function's call shape, as the tool and the reading give it:
//
//   Field_StartEventBattle  a direct cdecl callee of chapter code, 1 word (the id's byte)
//   Scena08_PartyJoin784    a call-table entry (Scenario_CallA, chapter 8 A[5]), no word read
//   Effect_SpawnAtCell      a direct cdecl callee (the cell pickups), 3 words
//   EventObj_Face           a direct callee, no words
//   EventOp_0x              a direct cdecl callee (EventScript_Op's table default), 1 word: the op
//   Party_AddToLists        a direct cdecl callee, 1 word, answers in al
//
// None is a vtable slot, a hook or a state handler; none reads the chapter
// bytes. Every function takes its arguments from Args, so all six are
// called with ten words (cdecl: the extra ones are the caller's).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_se.h"
#include "game/scena_se_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_se {
namespace {

namespace sh = scenario_harness;
using move_script::SetWord;

// tools/scenario_rows.py --unit SE --clones, 2026-09-27, names given.
constexpr sh::CallSite kCalls519F70[] = {{0x9, 0x533EF0}, {0x10, 0x533EF0}, {0x17, 0x533EF0}, {0x1F, 0x533E00}};
constexpr sh::CallSite kCalls524870[] = {{0x1, 0x589810}, {0x58, 0x5720C0}};
constexpr sh::CallSite kCalls579D70[] = {{0x28, 0x5891F0}, {0x35, 0x57C4C0}};
constexpr sh::CallSite kCalls57A010[] = {{0x3F, 0x579E30}, {0x4B, 0x589590}, {0xDE, 0x5720C0}, {0x174, 0x579DB0}, {0x1B3, 0x579D70}};
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
const sh::Clone kClones[] = {
    {"Field_StartEventBattle", 0x4410B0, 0x43, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Field_StartEventBattle), 0, false, sh::Shape::kEntry},
    {"Scena08_PartyJoin784", 0x519F70, 0x24, kCalls519F70, SH_N(kCalls519F70), nullptr, 0, nullptr, 0,
     reinterpret_cast<const void*>(&::Scena08_PartyJoin784), 0, false, sh::Shape::kEntry},
    {"Effect_SpawnAtCell", 0x524870, 0x76, kCalls524870, SH_N(kCalls524870), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Effect_SpawnAtCell), 0, false, sh::Shape::kEntry},
    {"EventObj_Face", 0x579D70, 0x3C, kCalls579D70, SH_N(kCalls579D70), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EventObj_Face)},
    {"EventOp_0x", 0x57A010, 0x1C2, kCalls57A010, SH_N(kCalls57A010), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::EventOp_0x), 0, false, sh::Shape::kEntry},
    {"Party_AddToLists", 0x591CC0, 0xE5, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Party_AddToLists), 0xFF, false, sh::Shape::kEntry},
};
#undef SH_N

enum : unsigned { kStartEventBattle, kPartyJoin784, kSpawnAtCell, kFace, kOp0x, kAddToLists, kCount };
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// --- the callees ---------------------------------------------------------------
//
// The harness's standard set holds Effect_FindFree (0xFF..0x13),
// AreaMap_Elevation, Sprite_SetAnimation / Bank, Sprite_FaceDirection,
// EventObj_Reset, EventObj_Face (moved to SH_OURS by this group) and
// Party_Join. Listed here: EventObj_SetFlags, to log the byte it reads, and
// the one callee nobody names.

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define SE_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;

// EventOp_0x's op, 17 bytes; the rest spare.
alignas(16) unsigned char g_op[0x20];

// EventOp_0x reads the count word and the op's bytes again after its calls.
// The harness's disturbance reaches the group's own cells one call in
// twenty-four, too seldom to tell a byte read before a call from one read
// after it (controls C16, C17 were not refused at first); so each of its
// four callees moves one of them, from the recorders' stream, after the
// recorder has logged: an op byte, op[3]'s bit 7 and low bits, or the
// count within the table.
std::uint32_t StirOp(const std::uint32_t*, std::uint32_t answer) {
    const std::uint32_t n = sh::Noise();
    switch (n % 4) {
    case 0: g_op[(n >> 8) % 17] = static_cast<unsigned char>(n >> 16); break;
    case 1: SetWord(sh::Mem(at::kCount), ((n >> 8) & 0xFF) % at::kObjectCount); break;
    case 2: g_op[3] = static_cast<unsigned char>(g_op[3] ^ (0x80 | ((n >> 8) & 0xF))); break;
    default: break;
    }
    return answer;
}

const sh::Callee kCallees[] = {
    // the flags byte it reads, as it is at the call (flags[0] only)
    {SE_OURS(EventObj_SetFlags), 1, {0}, kG, 0, 0, {1}, &StirOp},
    // the standard set's listings, with the stir
    {SE_OURS(EventObj_Reset), 0, {}, kG, 0, 0, {}, &StirOp},
    {SE_OURS(Sprite_SetAnimationBank), 1, {0xFFFFu}, sh::Answer::kFlag, 0, 0, {}, &StirOp},
    {SE_OURS(AreaMap_Elevation), 2, {0xFFFFFFFFu, 0xFFFFFFFFu}, kG, 0, 0, {}, &StirOp},
    // nobody's: the members' palettes reloaded
    {"0x533E00", at::kPartyPalettes, at::kPartyPalettes, 0, {}, kG, 0, 0},
};

// --- the state -------------------------------------------------------------------

// CharacterRecords' first eight: MoveScript_EffectState's 24 entries name
// records 0..7 (read 2026-09-27), so a member id below 24 writes inside.
constexpr unsigned kRecords = 8;

// Beyond the harness's 22 (which hold the chapter bytes, Cond_Flags with
// both party lists, Field_MemberCount, the count word 0x903850, Sprite_Current,
// ObjTrio, Sprite_Objects and Effect_Objects).
const sh::Region kRegions[] = {
    {Key(&Field_ScriptFlags2), 4},
    {0x904AA0, 0xB0},                                    // the battle bytes: 0x904AAA, 0x904AE4, 0x904AE5
    {Key(&Field_ActiveMember), 4},
    {bof3::addr::CharacterRecords, kRecords * at::kObjectStride},
    {Key(g_op), sizeof g_op},
};

unsigned char* Object(unsigned k) { return Sprite_Objects + (k % at::kObjectCount) * at::kObjectStride; }
unsigned char* Mem(std::uint32_t a) { return sh::Mem(a); }

// What a function reads again after a call and a caller could have moved:
// EventOp_0x's count (read after four calls, kept below 30 so no store
// leaves the table), Sprite_Current (read after every call), and the op's
// bytes (read after the calls where the original reads them).
void Disturb(std::uint32_t h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch (h % 5) {
    case 0: SetWord(Mem(at::kCount), v % at::kObjectCount); break;
    case 1: Sprite_Current = Object(v); break;
    case 2: g_op[v % 17] = static_cast<unsigned char>(h >> 16); break;
    case 3: SetWord(Mem(at::kBank), h >> 16); break;
    default: break;
    }
}

// --- the seed --------------------------------------------------------------------

void Seed(unsigned k) {
    // the pointer cells the random fill left as garbage
    Sprite_Current = Object(sh::Next());
    Field_ActiveMember = Object(sh::Next());
    // the count: a slot, and now and then 30 or 31 (nothing placed)
    SetWord(Mem(at::kCount), sh::Often() || k != kOp0x ? sh::Next() % at::kObjectCount : at::kObjectCount + sh::Next() % 2);
    if (sh::Half()) g_op[3] = static_cast<unsigned char>(g_op[3] ^ 0x80);
    // the member count either side of 3
    Field_MemberCount = static_cast<unsigned char>(sh::Next() % 5);
    // the lists: ids below 24 (a record inside the region), the second list
    // often the first's ids in another order, so the search finds one
    unsigned char* const lists = Mem(at::kPartyLists);
    for (unsigned i = 0; i < 6; ++i) lists[i] = static_cast<unsigned char>(sh::Next() % 24);
    if (sh::Often()) {
        const unsigned r = sh::Next() % 3;
        for (unsigned j = 0; j < 3; ++j) lists[3 + j] = lists[(j + r) % 3];
    }
    // the records' bit 1 (a member the search passes over) about half the time
    for (unsigned r = 0; r < kRecords; ++r) {
        unsigned char* const f = Mem(bof3::addr::CharacterRecords + 0xB + r * at::kObjectStride);
        *f = static_cast<unsigned char>(sh::Half() ? *f | 2 : *f & ~2u);
    }
    // EventObj_Face's +7 bit 3, both ways
    if (k == kFace && sh::Half()) Sprite_Current[7] = static_cast<unsigned char>(Sprite_Current[7] ^ 8);
}

void Args(unsigned k, std::uint32_t* a) {
    switch (k) {
    case kOp0x: a[0] = Key(g_op); break;
    case kAddToLists: a[0] = (a[0] & 0xFFFFFF00u) | (a[0] % 24); break;   // an id below 24, garbage above
    default: break;   // the id, the state and the cell: any words (masked by the originals)
    }
}

}  // namespace

void SelfTest() {
    sh::Group group = {
        "scena_se", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        nullptr, 0, kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 2000, nullptr, 0, &Args,
    };
    // Chapter 8, Scena08_PartyJoin784's; none of the six reads the chapter
    // bytes or the flag row.
    group.chapter = 8;
    sh::Run(group);
}

}  // namespace scena_se
