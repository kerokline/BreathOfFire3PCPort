// BOF3X_SHADOW=boss_harness_eh: the boss harness's own self-test of its
// round-twelve widening (group EH; docs/boss_harness.md section 10.8).
//
// Eleven of Capcom's functions from the battle runs - none taken, none of
// them anybody's yet (BE1..BE7 own them, analysis/round12_cut.tsv) - each
// driven in the shape round twelve added, with Capcom's code on both sides:
// "theirs" is the harness's copy (every call re-aimed at its recorder), and
// "ours" is this file's copy of the same bytes with every call and every
// stack-table immediate re-aimed at a route into boss_harness::StandIn - the
// recorder the harness stands in for that callee or handler - so the two
// passes must agree on every state byte and every log entry. What that proves
// is the harness, not the functions: each shape calls, seeds and compares;
// Clone::state_cell and Via::state_cell draw and plant the absolute byte; the
// engine frame's pointers (0x905B84, Field_State, 0x904B3C) hold; the engine
// set's typed stand-ins (Msg_SystemPtr's text pointer, Crt_sprintf's buffer,
// the text draws' strings, BattleBanner_Add's) log the same on both sides.
// BOF3X_EH_CONTROL=1..9 plants one change in this file's copy (below); each
// must be refused by a count.
//
// The functions (capstone, 2026-09-28; extents to each one's last ret):
//   0x42F5F0  kStep     BattleAction_KindSteps[0] (by 0x904AA3): three calls,
//                       0x8031F3 = 1, the acting sprite 0x904B3C's +1 = 9,
//                       0x904AA3 + 1
//   0x42F640  kStep     BattleAction_KindSteps[1]: word 0x904B82 zero ->
//                       0x904AA8 |= 4, 0x904AA1 = 3, 0x904AA2 = 0x904AA3 = 0
//   0x42F5E0  kDispatch jmp [0x64AEB4 + 4 * byte 0x904AA3]: state_cell
//                       0x904AA3, five entries (0x42F5F0 .. 0x42FAB0, then
//                       0x76767676 - not code)
//   0x42F640  kStep     again, through 0x42F5E0 with Via::state_cell 0x904AA3
//                       = 1 (planted in 0x64AEB8)
//   0x598DC0  kWindow   a window's stack table by the record's +3 (three
//                       immediates 0x598DF0 / 0x598E10 / 0x598E50)
//   0x598DF0  kWindow   its entry 0: the record's +4 / +6 / +3 through 0x905B84
//   0x441A10  kMember   jmp [0x64E07C + 4 * Sprite_Current[+2]] (the first two
//                       entries drawn)
//   0x441A30  kMember   Sprite_Current +0x4B / +0x58 into Field_State +0x12F /
//                       +0x140, +4 = 0, +2 = 1
//   0x4457F0  kHelper   one word (a byte of it), al: an actor Battle_ActorIsOut
//                       answers 0 for, 0xFF for none (the sibling's default
//                       target search)
//   0x453A90  kHelper   one word, al: bit (word & 0xFF) of 0x904088
//   0x42D8C0  kHelper   two words: BATE's window - Menu_DrawBox / Border, four
//                       Msg_SystemPtr + Text_DrawAt, two Crt_sprintf into
//                       0x904BA0 + Text_DrawFont12, 0x42DB40, two Text_DrawSmall
#include "game/boss_harness_eh.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/boss_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace boss_harness_eh {
namespace {

namespace bh = boss_harness;
using S = bh::Shape;
using U = std::uint32_t;

// --- the routes: a call site of this file's copy reaches the recorder the
// harness stands in for its target (by the target's address, as a re-aimed
// site of the harness's own copy does). cdecl: ten words forwarded, eax back.
using FnArgs = U (__cdecl*)(U, U, U, U, U, U, U, U, U, U);
template <U T>
U __cdecl Route(U a0, U a1, U a2, U a3, U a4, U a5, U a6, U a7, U a8, U a9) {
    return reinterpret_cast<FnArgs>(const_cast<void*>(bh::StandIn(T)))(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9);
}
struct RouteFor { U target; const void* to; };
#define EH_ROUTE(a) {a, reinterpret_cast<const void*>(&Route<a>)}
const RouteFor kRoutes[] = {
    EH_ROUTE(0x4301B0), EH_ROUTE(0x44A650), EH_ROUTE(0x446FB0),   // 0x42F5F0's
    EH_ROUTE(0x598DF0), EH_ROUTE(0x598E10), EH_ROUTE(0x598E50),   // 0x598DC0's immediates
    EH_ROUTE(0x4456C0),                                           // 0x4457F0's
    EH_ROUTE(0x57CF60), EH_ROUTE(0x5762D0), EH_ROUTE(0x497740), EH_ROUTE(0x516B30), EH_ROUTE(0x5B9380),
    EH_ROUTE(0x516F60), EH_ROUTE(0x42DB40), EH_ROUTE(0x516E70),   // 0x42D8C0's
};
#undef EH_ROUTE
const void* RouteTo(U target) {
    for (const RouteFor& r : kRoutes)
        if (r.target == target) return r.to;
    bof3::Fatal("boss_harness_eh: no route for 0x%X", (unsigned)target);
}

// --- the call sites and immediates (magic_rows.clone_sites over each extent)
constexpr bh::CallSite kCalls42F5F0[] = {{0x0, 0x4301B0}, {0x13, 0x44A650}, {0x26, 0x446FB0}};
constexpr bh::Imm kImms598DC0[] = {{0xF, 0x598DF0}, {0x17, 0x598E10}, {0x22, 0x598E50}};
constexpr bh::CallSite kCalls4457F0[] = {{0x17, 0x4456C0}, {0x42, 0x4456C0}, {0x6A, 0x4456C0}, {0x93, 0x4456C0}};
constexpr bh::CallSite kCalls42D8C0[] = {
    {0x2A, 0x57CF60}, {0x3B, 0x5762D0}, {0x45, 0x497740},  {0x5A, 0x516B30},  {0x89, 0x497740},
    {0xA6, 0x516B30}, {0xB2, 0x497740}, {0xC8, 0x516B30},  {0xDB, 0x5B9380},  {0xF7, 0x516F60},
    {0x10C, 0x5B9380}, {0x11A, 0x516F60}, {0x121, 0x42DB40}, {0x143, 0x497740}, {0x159, 0x516B30},
    {0x163, 0x497740}, {0x175, 0x516E70}, {0x17F, 0x497740}, {0x191, 0x516E70}};

// This file's copy of an original: every call site at its route, every
// stack-table immediate at the route for that handler (the harness registers
// a handler recorder for each), BOF3X_EH_CONTROL's byte if it names this one.
struct Control { unsigned n; U base, offset; unsigned char was, now; const char* what; };
const Control kControls[] = {
    {1, 0x598DF0, 0x9, 0x5A, 0x5B, "kWindow: the record's +4 stored 0x5B, not 0x5A"},
    {2, 0x42F640, 0x18, 0x03, 0x04, "kStep: 0x904AA1 = 4, not 3 (the via run's copy too)"},
    {3, 0x453A90, 0x2A, 0x95, 0x94, "kHelper: sete, not setne (the answer)"},
    {4, 0x441A30, 0x3C, 0x01, 0x02, "kMember: Sprite_Current +2 = 2, not 1"},
    // 5 is not a byte: the stack table's entry 1 aimed at entry 2's handler (Copy). A
    // byte control there (by the record's +2, not +3) indexes past the three
    // entries with a random byte and faults instead of counting.
    // (by 0x904AA2 instead faults: that byte is not drawn below five) - the load
    // of 0x904AA3 made a store of al (0): always entry 0, and the byte zeroed
    {6, 0x42F5E0, 0x2, 0xA0, 0xA2, "kDispatch state_cell: the byte stored, not read (entry 0 always)"},
    {7, 0x42D8C0, 0xD2, 0xDC, 0xD8, "the engine's Crt_sprintf: the other format"},
    {8, 0x4457F0, 0x7, 0x03, 0x04, "kHelper: cmp bl, 4, not 3 (the side's bound)"},
    {9, 0x42F5F0, 0x24, 0x01, 0x02, "kStep: window 4's +3 (0x8031F3) = 2, not 1"},
};
unsigned g_control;

void* Copy(const char* name, U base, U size, const bh::CallSite* sites, int n, const bh::Imm* imms, int n_imms) {
    bof3::CloneCall calls[32];
    if (n > 32) bof3::Fatal("boss_harness_eh: %s has %d calls", name, n);
    for (int i = 0; i < n; ++i) calls[i] = {sites[i].offset, RouteTo(sites[i].target), sites[i].target};
    auto* copy = static_cast<unsigned char*>(bof3::CloneOriginal(name, base, size, calls, n));
    for (int i = 0; i < n_imms; ++i) {
        U had;
        std::memcpy(&had, copy + imms[i].offset, sizeof had);
        if (had != imms[i].value)
            bof3::Fatal("boss_harness_eh: %s +0x%X holds 0x%X, not 0x%X", name, (unsigned)imms[i].offset, (unsigned)had,
                        (unsigned)imms[i].value);
        // control 5: the stack table's entry 1 aimed at entry 2's handler
        const U aim = g_control == 5 && base == 0x598DC0 && had == 0x598E10 ? 0x598E50 : had;
        if (aim != had) bof3::Log("shadow      boss_harness_eh: control 5 planted (kWindow dispatch: entry 1 is 0x598E50's)");
        const U to = static_cast<U>(reinterpret_cast<std::uintptr_t>(RouteTo(aim)));
        std::memcpy(copy + imms[i].offset, &to, sizeof to);
    }
    for (const Control& c : kControls) {
        if (c.n != g_control || c.base != base) continue;
        if (copy[c.offset] != c.was)
            bof3::Fatal("boss_harness_eh: control %u: %s +0x%X holds 0x%02X, not 0x%02X", c.n, name, (unsigned)c.offset,
                        copy[c.offset], c.was);
        copy[c.offset] = c.now;
        bof3::Log("shadow      boss_harness_eh: control %u planted (%s)", c.n, c.what);
    }
    return copy;
}

// --- the seeds and the words
enum : unsigned { kKindStep0, kKindStep1, kKindDispatch, kKindVia, kWinDispatch, kWin0, kObjDispatch, kObjSub, kTarget,
                  kFlagBit, kBateWindow, kCount };

void Seed(unsigned k) {
    switch (k) {
    case kKindStep1:
    case kKindVia:
        // the word 0x904B82 at zero half the time: the branch that writes
        if (bh::Half()) move_script::SetWord(bh::Mem(0x904B82), 0);
        break;
    case kObjDispatch:
        bh::OtherStates(2, 13, 0, 0);   // +1 inside BattleObj_StateTable's 13 (not read here; kept sane)
        break;
    default:
        break;
    }
}

void Args(unsigned k, U* a) {
    if (k == kTarget) {
        // an actor byte: the party's 0..2, the enemies' 3..10, and beyond; garbage above half the time
        a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Often() ? bh::Next() % 13 : bh::Next() & 0xFF);
    } else if (k == kFlagBit) {
        a[0] = (bh::Half() ? bh::Next() & 0xFFFFFF00u : 0) | (bh::Next() & 0xFF);
    }
}

}  // namespace

void SelfTest() {
    const char* const control = std::getenv("BOF3X_EH_CONTROL");
    g_control = control ? static_cast<unsigned>(std::atoi(control)) : 0;

    void* const ours42F5F0 = Copy("0x42F5F0 (eh)", 0x42F5F0, 0x45, kCalls42F5F0, 3, nullptr, 0);
    void* const ours42F640 = Copy("0x42F640 (eh)", 0x42F640, 0x24, nullptr, 0, nullptr, 0);
    void* const ours42F5E0 = Copy("0x42F5E0 (eh)", 0x42F5E0, 0xE, nullptr, 0, nullptr, 0);
    void* const ours598DC0 = Copy("0x598DC0 (eh)", 0x598DC0, 0x2E, nullptr, 0, kImms598DC0, 3);
    void* const ours598DF0 = Copy("0x598DF0 (eh)", 0x598DF0, 0x20, nullptr, 0, nullptr, 0);
    void* const ours441A10 = Copy("0x441A10 (eh)", 0x441A10, 0x12, nullptr, 0, nullptr, 0);
    void* const ours441A30 = Copy("0x441A30 (eh)", 0x441A30, 0x3E, nullptr, 0, nullptr, 0);
    void* const ours4457F0 = Copy("0x4457F0 (eh)", 0x4457F0, 0xB1, kCalls4457F0, 4, nullptr, 0);
    void* const ours453A90 = Copy("0x453A90 (eh)", 0x453A90, 0x2D, nullptr, 0, nullptr, 0);
    void* const ours42D8C0 = Copy("0x42D8C0 (eh)", 0x42D8C0, 0x19F, kCalls42D8C0, 19, nullptr, 0);

    // name, base, size, calls, n, imms, n, tables, n, ours, ret_mask, calm, shape, state_at, states, via, state_cell
    const bh::Clone clones[] = {
        {"0x42F5F0 kStep", 0x42F5F0, 0x45, kCalls42F5F0, 3, nullptr, 0, nullptr, 0, ours42F5F0, 0, false, S::kStep},
        {"0x42F640 kStep", 0x42F640, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, ours42F640, 0, false, S::kStep},
        {"0x42F5E0 kDispatch state_cell", 0x42F5E0, 0xE, nullptr, 0, nullptr, 0, nullptr, 0, ours42F5E0, 0, false,
         S::kDispatch, 1, 5, {}, 0x904AA3},
        {"0x42F640 kStep via 0x42F5E0", 0x42F640, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, ours42F640, 0, false, S::kStep, 1, 0,
         {0x42F5E0, 0x64AEB8, 0, 1, 0x904AA3}},
        {"0x598DC0 kWindow dispatch", 0x598DC0, 0x2E, nullptr, 0, kImms598DC0, 3, nullptr, 0, ours598DC0, 0, false, S::kWindow, 3, 3},
        {"0x598DF0 kWindow", 0x598DF0, 0x20, nullptr, 0, nullptr, 0, nullptr, 0, ours598DF0, 0, false, S::kWindow},
        {"0x441A10 kMember dispatch", 0x441A10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, ours441A10, 0, false, S::kMember, 2, 2},
        {"0x441A30 kMember", 0x441A30, 0x3E, nullptr, 0, nullptr, 0, nullptr, 0, ours441A30, 0, false, S::kMember},
        {"0x4457F0 kHelper", 0x4457F0, 0xB1, kCalls4457F0, 4, nullptr, 0, nullptr, 0, ours4457F0, 0xFF, false, S::kHelper},
        {"0x453A90 kHelper", 0x453A90, 0x2D, nullptr, 0, nullptr, 0, nullptr, 0, ours453A90, 0xFF, false, S::kHelper},
        {"0x42D8C0 kHelper", 0x42D8C0, 0x19F, kCalls42D8C0, 19, nullptr, 0, nullptr, 0, ours42D8C0, 0, false, S::kHelper},
    };
    static_assert(sizeof clones / sizeof clones[0] == kCount, "one enum entry a clone, in order");

    // the two .data tables the dispatchers jump through: BattleAction_KindSteps'
    // sub-table (five entries) and BattleObj_StateTable's +2 table (the two drawn)
    const bh::DataTable tables[] = {{0x64AEB4, 5}, {0x64E07C, 2}};
    // 0x42DB40 is BE1's (not the engine set's): a recorder of this test's own
    constexpr U kAll = 0xFFFFFFFFu;
    const bh::Callee callees[] = {{"0x42DB40", 0x42DB40, 0x42DB40, 2, {kAll, kAll}, bh::Answer::kGarbage, 0, 0}};

    bh::Group g{"boss_harness_eh", clones, kCount, callees, 1, tables, 2, nullptr, 0, &Seed, nullptr, 4000};
    g.args = &Args;
    g.engine = true;
    bh::Run(g);
}

}  // namespace boss_harness_eh

void BossHarnessEh_Inject() {
    // the harness's own self-test; no function is taken
    if (bof3::WantsShadow("boss_harness_eh")) boss_harness_eh::SelfTest();
}
