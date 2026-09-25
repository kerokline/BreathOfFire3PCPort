// The window-kind handlers: the enemy HP gauge's three states that
// Window_DispatchKind 0x597A30 picks between (reached from the enemy target
// window 0x597320), the battle banner window and the battle message window -
// kinds 6 and 7 of the window task's battle handler 0x596FA0 - with the
// states each runs, and record handler 4 of Field_RunTaskRecords.
// docs/window_kinds.md.
//
// Everything here is a faithful replacement: no DIVERGENCE.md entry is owed.
// Where the original indexes a table it built on its own stack without a
// bound, ours aborts loudly on an index past the table (CLAUDE.md rule 4), as
// Field_RunTaskRecords and Window_Run do: the original's slot past the end is
// its own return address, which cannot be reproduced.
#include "game/window_kinds.h"

#include <cstddef>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/window_kinds_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace window_kinds {

using move_script::At;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

namespace {

template <typename T> T As(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

// The record the window layer is running, re-read from 0x905B84 wherever the
// original re-reads it (after every call out). Volatile so that ours cannot
// cache it either: the fuzz's stand-ins repoint it between calls.
unsigned char* Rec() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(at::kCurrent))));
}

short S(const unsigned char* at) { return static_cast<short>(Word(at)); }

// 55 * v / max, as `lea` x 3 and `idiv` compute it: 32-bit signed, truncated.
int Scale(int v, int max) { return v * 55 / max; }

}  // namespace

const Callees kOriginals = {
    BattleWin_DrawMessageBox,
    Text_DrawAt,
    BattleWin_DrawMessage,
    BattleMsg_Advance,
    Window_FreeCurrent,
    {As<Handler>(bof3::addr::BattleWin_BannerSlideIn), As<Handler>(kNop), As<Handler>(bof3::addr::BattleWin_BannerSlideOut)},
    {As<Handler>(bof3::addr::BattleWin_MessageWait), As<Handler>(bof3::addr::BattleWin_MessageSlideIn),
     As<Handler>(bof3::addr::BattleWin_MessageShow), As<Handler>(bof3::addr::BattleWin_MessageSlideOut)},
    {As<Handler>(kResultKinds[0]), As<Handler>(kResultKinds[1]), nullptr, nullptr, As<Handler>(kResultKinds[4]),
     As<Handler>(kResultKinds[5])},
};
Callees g = kOriginals;

}  // namespace window_kinds

using namespace window_kinds;

// ===========================================================================
// The enemy HP gauge (Window_DispatchKind's three handlers)
// ===========================================================================
//
// Window_DispatchKind is called from one place, the enemy target window
// 0x597320 (group CL's) at 0x59738E, with eight pointers: the window record
// w's +0xB (the gauge's length, 0..0x37 pixels), the enemy's max HP (+0x30 of
// EnemyWorkingRecords 0x93B9E0 + 0x128 (w[+0xA] - 3)), w + 0x1C (the max HP
// last seen), w + 0x14 (the HP the gauge last showed), the enemy's HP (+0x24),
// w + 0xD (the drained part still drawn), w + 0x18 (the step a frame) and
// w + 8 (the gauge state - the index Window_DispatchKind dispatches on). The
// answer in eax is dead at the caller, so ours return nothing.

// original 0x597A80, gauge state 0 (settled). When the max HP changed since
// it was last seen: the HP clamped to it, the gauge recomputed as 55 * HP /
// max (at least 1 while the HP is non-zero) when the shown HP is the HP, else
// 55 * shown / max capped at 0x37 (the shown HP then catching up), and the
// max remembered. Then, if the HP fell below the shown HP, state 1: the gauge
// drops to its new length at once and the difference becomes the drained part
// (all of it when the HP is 0), stepped by a sixteenth of it; if it rose,
// state 2, stepped by a sixteenth of the pixel difference. Either way the
// step is at least 1 and the shown HP becomes the HP.
//
// As the original has it: every value is re-read from memory where the
// original re-reads it (the HP after each gauge store, the max for each
// divide); the rise's two divides share the one max read; the old gauge
// length passes through DamageScratch's first byte 0x903850, which is left
// holding it. No bound on the max: a max HP of 0 divides by zero in both.
extern "C" void __cdecl Window_HpGaugeTrack(unsigned char* gauge, const unsigned char* max_hp, unsigned char* max_seen,
                                            unsigned char* shown, unsigned char* hp, unsigned char* drain,
                                            unsigned char* step, unsigned char* kind) {
    const short max = S(max_hp);
    if (S(max_seen) != max) {
        if (!(max > S(hp))) SetWord(hp, static_cast<std::uint16_t>(max));
        const short was = S(shown);
        const short now = S(hp);
        if (was == now) {
            const auto q = static_cast<unsigned char>(Scale(now, S(max_hp)));
            gauge[0] = q;
            if (S(hp) != 0 && q == 0) gauge[0] = 1;
        } else {
            const auto q = static_cast<unsigned char>(Scale(was, S(max_hp)));
            gauge[0] = q;
            if (q > 0x37) {
                gauge[0] = 0x37;
                SetWord(shown, Word(hp));
            }
        }
        SetWord(max_seen, Word(max_hp));
    }
    const short was = S(shown);
    const short now = S(hp);
    if (was > now) {
        if (now == 0) {
            drain[0] = gauge[0];
            gauge[0] = 0;
        } else {
            At(at::kScratch)[0] = gauge[0];
            const auto q = static_cast<unsigned char>(Scale(S(hp), S(max_hp)));
            gauge[0] = q;
            if (S(hp) != 0 && q == 0) gauge[0] = 1;
            drain[0] = static_cast<unsigned char>(At(at::kScratch)[0] - gauge[0]);
        }
        SetWord(step, static_cast<unsigned>(drain[0] >> 4));
        kind[0] = 1;
    } else if (was < now) {
        const int max_now = S(max_hp);
        const int from = Scale(was, max_now);
        const int to = Scale(now, max_now);
        SetWord(step, static_cast<unsigned>((to - from) / 16));
        kind[0] = 2;
    } else {
        return;
    }
    if (Word(step) == 0) SetWord(step, 1);
    SetWord(shown, Word(hp));
}

// original 0x597BD0, gauge state 1 (draining): the drained part loses the
// step while it is longer than the step, else it and the state go to 0. The
// gauge is kept at 1 or more while the HP is non-zero.
//
// As the original has it: the drained byte is compared, zero-extended, with
// the step as a signed word; the subtraction takes the step's low byte.
extern "C" void __cdecl Window_HpGaugeDrain(unsigned char* gauge, const unsigned char* max_hp, unsigned char* max_seen,
                                            unsigned char* shown, unsigned char* hp, unsigned char* drain,
                                            unsigned char* step, unsigned char* kind) {
    (void)max_hp;
    (void)max_seen;
    (void)shown;
    const unsigned char d = drain[0];
    if (static_cast<int>(d) > static_cast<int>(S(step))) {
        drain[0] = static_cast<unsigned char>(d - step[0]);
    } else {
        drain[0] = 0;
        kind[0] = 0;
    }
    if (Word(hp) != 0 && gauge[0] == 0) gauge[0] = 1;
}

// original 0x597C10, gauge state 2 (filling): the gauge grows by the step
// while it stays more than a step short of 55 * HP / max, else it takes that
// length and the state goes to 0. At 1 or more while the HP is non-zero.
//
// As the original has it: the HP, the step and the max are read once, before
// any store; the growth adds the step's low byte to the gauge byte.
extern "C" void __cdecl Window_HpGaugeFill(unsigned char* gauge, const unsigned char* max_hp, unsigned char* max_seen,
                                           unsigned char* shown, unsigned char* hp, unsigned char* drain,
                                           unsigned char* step, unsigned char* kind) {
    (void)max_seen;
    (void)shown;
    (void)drain;
    const int target = Scale(S(hp), S(max_hp));
    const int by = S(step);
    const unsigned char c = gauge[0];
    if (target - by > static_cast<int>(c)) {
        gauge[0] = static_cast<unsigned char>(step[0] + c);
    } else {
        gauge[0] = static_cast<unsigned char>(target);
        kind[0] = 0;
    }
    if (Word(hp) != 0 && gauge[0] == 0) gauge[0] = 1;
}

// ===========================================================================
// The battle banner window (kind 6 of 0x596FA0)
// ===========================================================================

// original 0x597C70: the banner window's state (record byte +3, three on its
// stack: 0x597CF0, the bare ret 0x437CC0, 0x597D10), then the message box at
// the record's +4 / +6 and the text of banner pool entry +0xA
// (0x93B8E0 + 0xC n: the text +4, drawn in its byte +0xA) through Text_DrawAt
// at (+4 + 4, +6 + 3), to its end.
//
// As the original has it: the record is re-read after the state and after the
// box; the colour argument is eax after `mov al, [...]` on 0xC n, so it
// carries 0xC n's bits above the low byte (n of 22 or more makes them
// non-zero) - ours passes the same dword. The coordinates' upper halves are
// the registers' leftovers, which the callees never read (they take words).
extern "C" void __cdecl BattleWin_BannerRun(void) {
    const unsigned state = Rec()[3];
    if (state >= 3) bof3::Fatal("a battle banner window's state byte is %u; the original's stack table holds three", state);
    g.banner[state]();
    const unsigned char* w = Rec();
    g.message_box(Word(w + 4), Word(w + 6));
    w = Rec();
    const std::uint32_t e = static_cast<std::uint32_t>(w[0xA]) * 0xC;
    const auto* const text = As<const unsigned char*>(static_cast<std::uint32_t>(Long(At(at::kBannerText + e))));
    const std::uint32_t colour = (e & ~0xFFu) | At(at::kBannerColour + e)[0];
    g.text_draw_at(static_cast<std::uint16_t>(Word(w + 4) + 4), static_cast<std::uint16_t>(Word(w + 6) + 3),
                   static_cast<int>(colour), 0xFF, text);
}

// original 0x597CF0, banner state 0: the window slides down 8 a frame until
// its y (+6) is 0x12, then the state steps on. A y that is not 0x12 modulo 8
// never arrives (the word wraps), as in the original.
extern "C" void __cdecl BattleWin_BannerSlideIn(void) {
    unsigned char* const w = Rec();
    const std::uint16_t y = Word(w + 6);
    if (y == 0x12) {
        ++w[3];
        return;
    }
    SetWord(w + 6, static_cast<std::uint16_t>(y + 8));
}

// original 0x597D10, banner state 2: the window slides up 8 a frame until its
// y is -0x16 (0xFFEA); there the kind bits of the banner entry 0x93B8C0 points
// at (+1) are cleared from the mask 0x904AE9 and the record is freed (a tail
// jump to Window_FreeCurrent).
//
// As the original has it: 0x93B8C0 is whichever entry BattleBanner_Dispatch
// visited last, not one this record names.
extern "C" void __cdecl BattleWin_BannerSlideOut(void) {
    unsigned char* const w = Rec();
    const std::uint16_t y = Word(w + 6);
    if (y == 0xFFEA) {
        const auto* const entry = As<const unsigned char*>(static_cast<std::uint32_t>(Long(At(at::kBannerCurrent))));
        const auto keep = static_cast<unsigned char>(~entry[1]);
        At(at::kBannerMask)[0] = static_cast<unsigned char>(At(at::kBannerMask)[0] & keep);
        g.free_current();
        return;
    }
    SetWord(w + 6, static_cast<std::uint16_t>(y - 8));
}

// ===========================================================================
// The battle message window (kind 7 of 0x596FA0)
// ===========================================================================

// original 0x597D50: the message window's four states by record byte +3
// (0x597D90, 0x597DC0, 0x597DF0, 0x597E60, on its stack).
extern "C" void __cdecl BattleWin_MessageRun(void) {
    const unsigned state = Rec()[3];
    if (state >= 4) bof3::Fatal("a battle message window's state byte is %u; the original's stack table holds four", state);
    g.message[state]();
}

// original 0x597D90, message state 0: nothing drawn; when the ring has an
// entry (write index 0x93C2A1 not the read index 0x93C2A0), the state steps
// on and 0x939F60 is set.
extern "C" void __cdecl BattleWin_MessageWait(void) {
    if (At(at::kMsgTail)[0] == At(at::kMsgHead)[0]) return;
    unsigned char* const w = Rec();
    w[3] = static_cast<unsigned char>(w[3] + 1);
    At(at::kMsgOpen)[0] = 1;
}

// original 0x597DC0, message state 1: 0x939F60 set; the window slides down 8
// a frame until its y is 0x12, then the state steps on; then the message
// (a tail jump to BattleWin_DrawMessage).
extern "C" void __cdecl BattleWin_MessageSlideIn(void) {
    unsigned char* const w = Rec();
    At(at::kMsgOpen)[0] = 1;
    const std::uint16_t y = Word(w + 6);
    if (y == 0x12)
        ++w[3];
    else
        SetWord(w + 6, static_cast<std::uint16_t>(y + 8));
    g.draw_message();
}

// original 0x597DF0, message state 2: the message drawn; then, when the ring
// entry has flag bit 0 and a button went down this frame (Input_Pressed), the
// read index steps on and, when that empties the ring, the state becomes 3;
// then, for the entry now at the read index, when it has flag bit 1 and a
// timer (+1) other than 0xFF, the timer counts down, and at 0 the same step.
//
// As the original has it: the read index is re-read between the two tests,
// so a button that advances the ring makes the timer test look at the NEXT
// entry in the same frame; the record is re-read for each state store.
extern "C" void __cdecl BattleWin_MessageShow(void) {
    g.draw_message();
    unsigned i = At(at::kMsgHead)[0];
    if ((At(at::kMsgRing + 8 * i)[0] & 1) != 0 && Input_Pressed != 0) {
        if (g.msg_advance() != 0) Rec()[3] = 3;
    }
    i = At(at::kMsgHead)[0];
    unsigned char* const entry = At(at::kMsgRing + 8 * i);
    if ((entry[0] & 2) == 0) return;
    unsigned char timer = entry[1];
    if (timer == 0xFF) return;
    --timer;
    entry[1] = timer;
    if (timer != 0) return;
    if (g.msg_advance() != 0) Rec()[3] = 3;
}

// original 0x597E60, message state 3: the window slides up 8 a frame until
// its y is -0x16; there 0x939F60 is cleared and the state goes back to 0 (the
// record is kept, waiting for the next message). Every frame the box, and
// the ring entry BEFORE the read index (the one just shown) through
// Text_DrawAt at (+4 + 4, +6 + 3), colour 0, to its end.
extern "C" void __cdecl BattleWin_MessageSlideOut(void) {
    unsigned char* const w = Rec();
    const std::uint16_t y = Word(w + 6);
    if (y == 0xFFEA) {
        At(at::kMsgOpen)[0] = 0;
        w[3] = 0;
    } else {
        SetWord(w + 6, static_cast<std::uint16_t>(y - 8));
    }
    const unsigned char* r = Rec();
    g.message_box(Word(r + 4), Word(r + 6));
    const unsigned i = (At(at::kMsgHead)[0] - 1u) & 0xFu;
    const auto* const text = As<const unsigned char*>(static_cast<std::uint32_t>(Long(At(at::kMsgRing + 4 + 8 * i))));
    r = Rec();
    g.text_draw_at(static_cast<std::uint16_t>(Word(r + 4) + 4), static_cast<std::uint16_t>(Word(r + 6) + 3), 0, 0xFF,
                   text);
}

// ===========================================================================
// Record handler 4
// ===========================================================================

// original 0x597F60: handler 4 of Field_RunTaskRecords' nine - one of six by
// record byte +2, on its stack: 0x597FA0, 0x5984B0, 0, 0, 0x598570,
// 0x5986C0. The two zeros are stored from eax, not immediates: kinds 2 and 3
// call address 0 in the original (an access violation); ours aborts loudly
// there, as for a kind past the table.
extern "C" void __cdecl Window_Handler4Kinds(void) {
    const unsigned kind = Rec()[2];
    if (kind >= 6) bof3::Fatal("a record-handler-4 window's kind byte is %u; the original's stack table holds six", kind);
    if (g.result[kind] == nullptr)
        bof3::Fatal("a record-handler-4 window has kind %u, whose slot in the original's stack table is 0", kind);
    g.result[kind]();
}

// ===========================================================================

void WindowKinds_Inject() {
    if (bof3::WantsShadow("window_kinds")) window_kinds::SelfTest();
    BOF3_INJECT(Window_HpGaugeTrack);
    BOF3_INJECT(Window_HpGaugeDrain);
    BOF3_INJECT(Window_HpGaugeFill);
    BOF3_INJECT(BattleWin_BannerRun);
    BOF3_INJECT(BattleWin_BannerSlideIn);
    BOF3_INJECT(BattleWin_BannerSlideOut);
    BOF3_INJECT(BattleWin_MessageRun);
    BOF3_INJECT(BattleWin_MessageWait);
    BOF3_INJECT(BattleWin_MessageSlideIn);
    BOF3_INJECT(BattleWin_MessageShow);
    BOF3_INJECT(BattleWin_MessageSlideOut);
    BOF3_INJECT(Window_Handler4Kinds);
}
