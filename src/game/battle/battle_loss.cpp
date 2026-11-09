// Moved here by tools/move_funcs.py from battle_e1.cpp (docs/source-layout-round.md).
#include "game/battle/battle_loss.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e1.h"
#include "game/battle_e1_callees.h"
#include "game/boss_harness.h"
#include "game/labels.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = battle_e1::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

unsigned char& BBattleE1(U address) { return move_script::At(address)[0]; }

U LBattleE1(U address) { return static_cast<U>(move_script::Long(move_script::At(address))); }

unsigned char* Packet() { return Gfx_PacketNext; }

unsigned char* Member(U index) { return move_script::At(::bof3::Img(ObjTrio_addr) + index * battle_e1::at::kMemberSize); }

// fild dword / fstp dword: the int to a float, nearest.
void PutFloat(unsigned char* p, std::int32_t v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}

void Rgb(unsigned char* p, unsigned at_, unsigned char v) { p[at_] = p[at_ + 1] = p[at_ + 2] = v; }

// jmp [table + 4 * index]: the entry as read (the fuzz swaps the table's
// cells for its recorders), a Fatal past `entries` (the original jumps
// through the dword after: docs/battle_e1.md section 7). The jmp leaves the
// caller's stack word and the entry's eax in place: ours hands the word on
// and answers the entry's eax (round 11 doc section 5.1).
using Entry = unsigned long (__cdecl*)(unsigned long);

unsigned long Dispatch(const char* who, U table, unsigned index, unsigned entries, unsigned long through) {
    if (index >= entries)
        bof3::Fatal("%s: the step byte is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/battle_e1.md section 7)",
                    who, index, entries, (unsigned)table);
    return reinterpret_cast<Entry>(static_cast<std::uintptr_t>(LBattleE1(table + 4 * index)))(through);
}

// A texture page and draw mode for the next primitives: Gpu_GetTPage(tp, abr,
// x, y), Gpu_SetDrawMode(Gfx_PacketNext, 0, 0, page & 0xFFFF, 0) - the fifth
// word is the GetTPage push the caller's `add esp, 0x10` leaves -, then
// Gfx_CommitPrim(slot, 0xC).
void DrawMode(unsigned tp, unsigned abr, int x, int y, unsigned slot) {
    const unsigned page = BH_CALL(Gpu_GetTPage)(tp, abr, x, y);
    BH_CALL(Gpu_SetDrawMode)(Packet(), 0, 0, page & 0xFFFF, 0);
    BH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// The loss screen's sprites: CLUT (0, 0x1FA), shade, x / y floats (as bit
// patterns), u, v, w, h - the fields stored before Gpu_SetSprt, as the
// original does - then Gfx_CommitPrim(2, 0x1C).
void LossSprite(unsigned char shade, U xf, U yf, unsigned char u, unsigned char v, unsigned w, unsigned h) {
    unsigned char* const p = Packet();
    auto* const sprt = rv::As<records::GpuSprt>(p);  // a SPRT: Gpu_SetSprt below starts it
    const unsigned clut = BH_CALL(Gpu_GetClut)(0, 0x1FA);
    rv::Store<unsigned>(sprt->clut, clut);
    Rgb(p, 4, shade);
    rv::Store<float>(sprt->x, std::bit_cast<float>(static_cast<std::int32_t>(xf)));
    rv::Store<float>(sprt->y, std::bit_cast<float>(static_cast<std::int32_t>(yf)));
    p[0x14] = u;
    p[0x15] = v;
    rv::Store<unsigned>(sprt->w, w);
    rv::Store<unsigned>(sprt->h, h);
    BH_CALL(Gpu_SetSprt)(p);
    BH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// A CLUT run of 0x100 words greyed: each colour's three 5-bit channels
// averaged (the sum / 3, rounded down: the original's multiply by 0x55555556)
// into all three, bit 15 kept.
void GreyClut(U at_) {
    for (unsigned k = 0; k < 0x100; ++k) {
        unsigned char* const w = At(at_ + 2 * k);
        const unsigned c = Word(w);
        const unsigned q = (((c >> 10) & 0x1F) + ((c >> 5) & 0x1F) + (c & 0x1F)) / 3;
        SetWord(w, (q << 10) | (q << 5) | q | (c & 0x8000));
    }
}

void LossMember(U index) {
    unsigned char* const m = Member(index);
    auto* const member_rec = rv::As<records::PartyMember>(m);
    Sprite_Current = m;
    BH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(member_rec->direction + 0x1C));
    const auto flags = static_cast<unsigned char>(m[0x24] | 0x80);
    member_rec->ot_slot = 1;
    m[0x24] = flags;
    const U row = ::bof3::Img(BattleLoss_Steps_addr) + 24 * static_cast<U>(BBattleE1(::bof3::Img(at::kPartyCount.v))) + 8 * index;
    member_rec->screen_x = Word(At(row));
    member_rec->screen_y = Word(At(row + 4));
    BH_CALL(Sprite_ReleaseTint)(m);
}

}  // namespace

// original 0x432430 (0x64AF70[2]): by 0x904AA3 through BattleLoss_Steps
// 0x64AFD8 (5).
extern "C" unsigned long __cdecl BattleLoss_Dispatch(unsigned long through) {
    return Dispatch("BattleLoss_Dispatch", ::bof3::Img(BattleLoss_Steps_addr), BattleE4_Step3, 5, through);
}

// original 0x432440 (BattleLoss_Steps[0]): Transition_Start(0x13),
// Gfx_ClutStripCopyRow(0x1A), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_FadeOut(void) {
    BH_CALL(Transition_Start)(0x13);
    BH_CALL(Gfx_ClutStripCopyRow)(0x1A);
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x432460 (BattleLoss_Steps[1]): once no transition runs, window
// 0's +0 = 0; members 0, 1, 2 reset (LossMember) while the count (re-read)
// is at least 1, 2, exactly 3; then 0x494E70() (the enemies' state bytes),
// BattleTask_ClearAll, Transition_Start(0xE), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_ResetParty(void) {
    if (Transition_Busy != 0) return;
    const unsigned char n = BBattleE1(::bof3::Img(at::kPartyCount.v));
    WindowRecords[0].in_use = 0;
    if (n >= 1) {
        LossMember(0);
        if (BBattleE1(::bof3::Img(at::kPartyCount.v)) >= 2) {
            LossMember(1);
            if (BBattleE1(::bof3::Img(at::kPartyCount.v)) == 3) LossMember(2);
        }
    }
    BH_AT(void (__cdecl*)(), at::kEnemiesClear.v)();
    BH_CALL(BattleTask_ClearAll)();
    BH_CALL(Transition_Start)(0xE);
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x4325F0 (BattleLoss_Steps[2]): the black screen and the panels
// (shade 0x80); once no transition runs, the bar 0x904B70 = 0x2C and
// 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_Show(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    if (Transition_Busy != 0) return;
    BattleResult_TickStep = 0x2C;
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x432630 (BattleLoss_Steps[3]): the black screen, the panels, the
// caption; the bar up by 4 while below 0xF4, drawn; on any held button both
// CLUT runs 0x812980 / 0x811380 greyed, Gfx_ClutStripDirty = 1,
// Transition_Start(0xD), 0x904AA3 + 1.
extern "C" void __cdecl BattleLoss_BarGrow(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    BH_CALL(BattleLoss_DrawCaption)(0x80);
    unsigned width = BattleResult_TickStep;
    if (width < 0xF4) {
        width += 4;
        BattleResult_TickStep = width;
    }
    BH_CALL(BattleLoss_DrawBar)(width);
    if (Input_Held == 0) return;
    GreyClut(::bof3::Img(Gfx_ClutStrip_addr) + 0x3400);
    GreyClut(::bof3::Img(Gfx_ClutStrip_addr) + 0x1e00);
    Gfx_ClutStripDirty = 1;
    BH_CALL(Transition_Start)(0xD);
    BattleE4_Step3 = static_cast<unsigned char>(BattleE4_Step3 + 1);
}

// original 0x432750 (BattleLoss_Steps[4]): the black screen, the panels, the
// caption; once no transition runs, the three members' tints released,
// Window_ResetAll, 0x904AE9 / 0x904AE5 / the fight byte / the phase and its
// four step bytes 0, 0x494E70(), BattleTask_ClearAll, Sound_StopChannels,
// Task_Restart(Boot_Task) (which does not return in the game).
extern "C" void __cdecl BattleLoss_Restart(void) {
    BH_CALL(BattleLoss_DrawBlack)();
    BH_CALL(BattleLoss_DrawPanels)(0x80);
    BH_CALL(BattleLoss_DrawCaption)(0x80);
    if (Transition_Busy != 0) return;
    BH_CALL(Sprite_ReleaseTint)(Member(0));
    BH_CALL(Sprite_ReleaseTint)(Member(1));
    BH_CALL(Sprite_ReleaseTint)(Member(2));
    BH_CALL(Window_ResetAll)();
    BattleMisc_BannerKinds = 0;
    EventBattle_Flags = 0;
    EventBattle_Id = 0;
    BBattleE1(::bof3::Img(0x904AA0)) = 0;
    BattleActions_Step = 0;
    BattleE4_Step2 = 0;
    BattleE4_Step3 = 0;
    BattleE4_Step4 = 0;
    BH_AT(void (__cdecl*)(), at::kEnemiesClear.v)();
    BH_CALL(BattleTask_ClearAll)();
    BH_CALL(Sound_StopChannels)();
    BH_CALL(Task_Restart)(reinterpret_cast<void*>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Boot_Task))));
}

// original 0x4327F0: three SPRTs of page (0x340, 0x100) and CLUT (0, 0x1FA)
// in the shade byte: 0x100 x 0xA0 from (0, 0x60) at (16, 60); 0x20 x 0x60
// from (0xC0, 0) at (272, 60); 0x20 x 0x40 from (0xE0, 0) at (272, 156).
extern "C" void __cdecl BattleLoss_DrawPanels(unsigned shade) {
    DrawMode(1, 0, 0x340, 0x100, 2);
    const auto s = static_cast<unsigned char>(shade);
    LossSprite(s, 0x41800000u, 0x42700000u, 0, 0x60, 0x100, 0xA0);
    LossSprite(s, 0x43880000u, 0x42700000u, 0xC0, 0, 0x20, 0x60);
    LossSprite(s, 0x43880000u, 0x431C0000u, 0xE0, 0, 0x20, 0x40);
}

// original 0x432930: page (0x3C0, 0), then a black TILE over (0, 0) ..
// (320, 240), Gfx_CommitPrim(2, 0x1C).
//
// DIV-0041: under a wide picture the black covers it whole, (-53, 0) 426 x
// 240, as the fade tile and the save menu's black do - the owner's shot,
// 2026-09-30: the battle field showing in both bands beside GAME OVER.
// Widescreen_Fill is 0 until every self-test has run (widescreen.h).
extern "C" void __cdecl BattleLoss_DrawBlack(void) {
    DrawMode(0, 0, 0x3C0, 0, 2);
    unsigned char* const p = Packet();
    auto* const tile = rv::As<records::GpuTile>(p);  // a TILE: Gpu_SetTile
    BH_CALL(Gpu_SetTile)(p);
    rv::Store<float>(tile->x, Widescreen_FillX());
    rv::Store<float>(tile->y, std::bit_cast<float>(0));
    rv::Store<float>(tile->w, Widescreen_FillWidth());   // 0x43A00000 narrow
    rv::Store<float>(tile->h, std::bit_cast<float>(0x43700000));
    Rgb(p, 4, 0);
    BH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// original 0x4329A0: page (0x340, 0x100), one SPRT 0xA8 x 0x18 from (0, 0)
// at (76, 48) in the shade byte, CLUT (0, 0x1FA).
extern "C" void __cdecl BattleLoss_DrawCaption(unsigned shade) {
    DrawMode(1, 0, 0x340, 0x100, 2);
    LossSprite(static_cast<unsigned char>(shade), 0x42980000u, 0x42400000u, 0, 0, 0xA8, 0x18);
}

// original 0x432A30: the bar of the low word w: page (0x3C0, 0), a black
// TILE at (w + 0x20, 48) of 168 x 24 (Gfx_CommitPrim(1, 0x1C)); page
// (0x3C0, 0) with abr 2, a semi-transparent POLY_G4 (w, 48) .. (w + 0x22,
// 72), black on the left, white on the right (Gfx_CommitPrim(1, 0x44)).
extern "C" void __cdecl BattleLoss_DrawBar(unsigned width) {
    const auto w = static_cast<std::int32_t>(width & 0xFFFF);
    DrawMode(0, 0, 0x3C0, 0, 1);
    unsigned char* p = Packet();
    BH_CALL(Gpu_SetTile)(p);
    SetLong(p + 0xC, 0x42400000);
    SetLong(p + 0x14, 0x43280000);
    SetLong(p + 0x18, 0x41C00000);
    PutFloat(p + 8, w + 0x20);
    Rgb(p, 4, 0);
    BH_CALL(Gfx_CommitPrim)(1, 0x1C);
    DrawMode(0, 2, 0x3C0, 0, 1);
    p = Packet();
    BH_CALL(Gpu_SetPolyG4)(p);
    Rgb(p, 4, 0);
    PutFloat(p + 8, w);
    Rgb(p, 0x14, 0xFF);
    Rgb(p, 0x24, 0);
    Rgb(p, 0x34, 0xFF);
    SetLong(p + 0xC, 0x42400000);
    PutFloat(p + 0x18, w + 0x22);
    PutFloat(p + 0x28, w);
    SetLong(p + 0x1C, 0x42400000);
    SetLong(p + 0x2C, 0x42900000);
    PutFloat(p + 0x38, w + 0x22);
    SetLong(p + 0x3C, 0x42900000);
    BH_CALL(Gpu_SetSemiTrans)(p, 1);
    BH_CALL(Gfx_CommitPrim)(1, 0x44);
}

void BattleLoss_Inject() {
    BOF3_INJECT(BattleLoss_Dispatch);
    BOF3_INJECT(BattleLoss_FadeOut);
    BOF3_INJECT(BattleLoss_ResetParty);
    BOF3_INJECT(BattleLoss_Show);
    BOF3_INJECT(BattleLoss_BarGrow);
    BOF3_INJECT(BattleLoss_Restart);
    BOF3_INJECT(BattleLoss_DrawPanels);
    BOF3_INJECT(BattleLoss_DrawBlack);
    BOF3_INJECT(BattleLoss_DrawCaption);
    BOF3_INJECT(BattleLoss_DrawBar);
}
