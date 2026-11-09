// The battle windows (the seventh round's group BC). docs/battle_windows.md.
//
// Twenty-two functions the owner's combat route reaches: the party row's
// status bars, the command cross and its icons, the command label banner,
// the three plain boxes, the enemy and member target banners - the handlers
// the window task 0x596A90 calls - with the window task's small helpers and
// the battle party's per-frame state step (0x441100 .. 0x4412B0). Each was
// read to its last instruction (2026-09-23) against its BATTLE.EMI twin where
// analysis/pairs_propagated.json has one (docs/battle_windows.md, section 1).
//
// Every call goes through battle_windows::g (battle_windows_callees.h), so
// that the start-up fuzz can stand recorders in for them - for ours and for
// the originals' copies alike. Where the original reads memory after a call,
// ours reads it after the same call.
//
// Upper halves. The originals push many values built with 8- and 16-bit
// operations, whose upper bits are whatever the register or the stack slot
// held; every callee here reads only the low 16 bits of a coordinate and the
// low byte of a colour, size or flag (each callee's read is in the fuzz's
// stand-ins), so ours passes the value with its upper bits as C++ computes
// them. The originals also park values in their own argument slots (the
// status bars' x in the first, a colour byte in the second); unobservable to
// a caller, ours uses locals.
#include "game/battle_windows.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_windows_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/text_pairs.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_windows {

using move_script::At;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Gpu_GetTPage,
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetPolyFT4,
    Text_DrawAt,
    Text_DrawFont8,
    Crt_sprintf,
    Menu_DrawIcon,
    Sprite_UpdateScreen,
    reinterpret_cast<unsigned (__cdecl*)()>(reinterpret_cast<void*>(&Sprite_ScriptTick)),
    reinterpret_cast<unsigned (__cdecl*)()>(reinterpret_cast<void*>(&Sprite_ScriptTickOnce)),
    reinterpret_cast<unsigned (__cdecl*)(unsigned)>(reinterpret_cast<void*>(&Sprite_EnsureAnimation)),
    Fn<unsigned (__cdecl*)(unsigned, std::uint32_t, unsigned)>(::bof3::Code(kPoseFrom.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(::bof3::Code(kDrawValue.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(::bof3::Code(kDrawEdge.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned)>(::bof3::Code(kDrawTile.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kDrawTileRgb.v)),
    Fn<void (__cdecl*)(int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kDrawBar.v)),
    Fn<void (__cdecl*)(int, int, unsigned)>(::bof3::Code(kDrawDigit.v)),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kLinePlain.v)),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kLineSemi0.v)),
    Fn<void (__cdecl*)(int, int, int, int, unsigned, unsigned, unsigned)>(::bof3::Code(kLineSemi1.v)),
    Fn<unsigned (__cdecl*)(unsigned)>(::bof3::Code(kEnemyNameShown.v)),
    Fn<const unsigned char* (__cdecl*)(int, int, unsigned, unsigned, const unsigned char*)>(::bof3::Code(kTinyFont.v)),
    reinterpret_cast<const std::uint32_t*>(static_cast<std::uintptr_t>(::bof3::Img(BattleObj_StateTable_addr))),
    {at::kKindHandlers[0], at::kKindHandlers[1], at::kKindHandlers[2]},
    BattleObj_RunState,
    BattleWin_DrawCommandIcon,
    BattleWin_DrawMessageBox,
};
Callees g = kOriginals;

}  // namespace battle_windows

using namespace battle_windows;

namespace {

using move_script::Long;
using move_script::SetWord;

unsigned char* PtrBattleWindows(std::uint32_t at) { return battle_windows::At(static_cast<std::uint32_t>(move_script::Long(battle_windows::At(at)))); }
unsigned char* CurrentBattleWindows() { return PtrBattleWindows(::bof3::Img(battle_windows::at::kCurrent.v)); }

// A party member's battle object (ObjTrio, stride 0x14C) and its words.
unsigned char* MemberBattleWindows(unsigned i) { return battle_windows::At(::bof3::Img(ObjTrio_addr) + battle_windows::at::kObjectStride * i); }

}  // namespace

// ===========================================================================
// The battle party's per-frame state step (the battle frame Battle_Frame 0x42E370 calls
// the first two; the state handlers of 0x64DFE0 the rest).

// ===========================================================================
// The window task's handlers

// ===========================================================================
// The window task's small helpers

// original 0x5979E0: the current window record's +0xF set and its state +3
// stepped on.
extern "C" void __cdecl Window_FlagAndAdvance() {
    CurrentBattleWindows()[0xF] = 1;
    ++rv::As<records::WindowRecord>(CurrentBattleWindows())->state;
}

// original 0x597A00: the current window record's position +4 / +6 put back
// from +0x10 / +0x12, +0xF cleared, its state +3 stepped back.
extern "C" void __cdecl Window_RestoreAndBack() {
    unsigned char* w = CurrentBattleWindows();
    records::WindowRecord* win = rv::As<records::WindowRecord>(w);
    win->x = rv::As<records::BattleWindow>(w)->home_x;
    w = CurrentBattleWindows();
    win = rv::As<records::WindowRecord>(w);
    win->y = rv::As<records::BattleWindow>(w)->home_y;
    CurrentBattleWindows()[0xF] = 0;
    --rv::As<records::WindowRecord>(CurrentBattleWindows())->state;
}

// original 0x597A30: a window kind's handler by the byte its eighth argument
// points at, from three on its stack (0x597A80, 0x597BD0, 0x597C10), handed
// all eight arguments; the answer is the handler's. Past 2 the original
// indexes its frame without a bound: 3 calls its own return address, 4..11
// its arguments as pointers, 12 and up the caller's frame - ours reads the
// same slots (`frame` is our EBP = entry ESP - 4; the table's dword i sits at
// entry ESP - 0xC + 4 i). docs/battle_windows.md, section 4.
extern "C" __attribute__((disable_tail_calls, noinline)) unsigned __cdecl Window_DispatchKind(
    std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, std::uint32_t a4, std::uint32_t a5,
    std::uint32_t a6, std::uint32_t a7) {
    const auto* const frame = static_cast<const unsigned char*>(__builtin_frame_address(0));
    const unsigned i = battle_windows::At(a7)[0];
    std::uint32_t handler;
    if (i < 3) {
        handler = battle_windows::g.kind_handlers[i];
    } else {
        std::memcpy(&handler, frame - 8 + 4 * i, sizeof handler);
    }
    using Handler = unsigned (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t,
                                        std::uint32_t, std::uint32_t, std::uint32_t);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(::bof3::CodePlace(handler)))(a0, a1, a2, a3, a4, a5, a6, a7);
}

// original 0x597ED0: the battle message - the current window's box
// (BattleWin_DrawMessageBox at +4, +6), then the ring's entry 0x93C2A0
// through Text_DrawAt at (+4 + 4, +6 + 3), to its end. The record and the
// index are read after the box.
extern "C" void __cdecl BattleWin_DrawMessage() {
    const records::WindowRecord* win = rv::As<records::WindowRecord>(CurrentBattleWindows());
    battle_windows::g.message_box(win->x, win->y);
    const unsigned index = BattleWindows_MsgHead;
    const auto* const text = battle_windows::At(static_cast<std::uint32_t>(move_script::Long(battle_windows::At(::bof3::Img(at::kMsgRing.v) + 8 * index))));
    win = rv::As<records::WindowRecord>(CurrentBattleWindows());
    battle_windows::g.text_draw_at(static_cast<std::uint16_t>(win->x + 4), static_cast<std::uint16_t>(win->y + 3), 0, 0xFF,
                   text);
}

// original 0x597F20: the battle message ring's read index 0x93C2A0 stepped
// on (mod 16); 1 when it has reached the write index 0x93C2A1.
extern "C" unsigned char __cdecl BattleMsg_Advance() {
    const auto head = static_cast<unsigned char>((BattleWindows_MsgHead + 1) & 0xF);
    const unsigned char tail = BattleActorCopies_QueueWrite;
    BattleWindows_MsgHead = head;
    return tail == head ? 1 : 0;
}

// original 0x597F40: the characters of a string, a byte of 0x80 or more
// taking the next with it; a byte count.
extern "C" unsigned char __cdecl Text_GlyphCount(const unsigned char* text) {
    unsigned char n = 0;
    const unsigned char* p = text;
    unsigned char c = *p;
    while (c != 0) {
        if (c & 0x80) {
            if (TextPair_At(p)) ++n;      // DIV-0057: a pair is two characters wide
            ++p;
        }
        c = p[1];
        ++p;
        ++n;
    }
    return n;
}

// ===========================================================================

void BattleWindows_Inject() {
    g = kOriginals;
    if (bof3::WantsShadow("battle_windows")) SelfTest();

    BOF3_INJECT(Window_FlagAndAdvance);
    BOF3_INJECT(Window_RestoreAndBack);
    BOF3_INJECT(Window_DispatchKind);
    BOF3_INJECT(BattleWin_DrawMessage);
    BOF3_INJECT(BattleMsg_Advance);
    BOF3_INJECT(Text_GlyphCount);
}
