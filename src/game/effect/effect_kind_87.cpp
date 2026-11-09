// Moved here by tools/move_funcs.py from effect_4a.cpp, effect_4b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_87.h"

#include <bit>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/cheats.h"
#include "game/effect/effect_kind_87_callees.h"
#include "game/effect_4a.h"
#include "game/effect_4a_callees.h"
#include "game/effect_4b.h"
#include "game/effect_4b_callees.h"
#include "game/effect_gte.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/sprite_draw.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_kind_87::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table, a null or data.
void DispatchEffect4a(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4a.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(P(table + 4 * state))))();
}

using move_script::SetLong;

using move_script::Word;

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

// sar: an arithmetic right shift of the 32 bits as the original holds them.
U Sar(U v, unsigned n) { return static_cast<U>(static_cast<std::int32_t>(v) >> n); }

// The draw mode most of these put first: Gpu_GetTPage(0, abr, x, y) and
// Gpu_SetDrawMode at the cursor (read after the page) - its fifth argument is
// the first of the page's five pushes, a zero, left on the stack.
void DrawMode(unsigned abr, int x, int y, int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, abr, x, y) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
}

U PanePoint(unsigned n) { return ::bof3::Img(EffectKind30_Shards_addr) + 0xa8 + at::kPanePointStride * n; }

unsigned char* Pane(unsigned i) { return At(::bof3::Img(EffectKind30_Shards_addr) + at::kPaneStride * i); }

void SetPanePoints(unsigned i, unsigned p0, unsigned p1, unsigned p2, unsigned p3) {
    unsigned char* const r = Pane(i);
    SetUL(r + 0, PanePoint(p0));
    SetUL(r + 4, PanePoint(p1));
    SetUL(r + 8, PanePoint(p2));
    SetUL(r + 0xC, PanePoint(p3));
}

}  // namespace

// original 0x488F60 (Effect_KindHandlers[0x87], hidden in 0x487BF0): jmp
// [EffectKind87_States + +1 * 4].
extern "C" void __cdecl EffectKind87_Run(void) {
    DispatchEffect4a("EffectKind87_Run", AddressOf(EffectKind87_States), EffectKind87_States_count);
}

// original 0x488F80 (state 0): +6 the draw pass flags (Draw_PassFlags' low
// byte), which become 0x1B; E4B's set-up 0x489030; 0x676280 0; +1 up one.
extern "C" void __cdecl EffectKind87_Start(void) {
    S()[6] = Draw_PassFlags;
    Draw_PassFlags = 0x1B;
    SH_AT(Handler, at::kKind87Setup.v)();
    unsigned char* const s = S();
    Effect4b_SoundIndex = 0;
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x488FB0 (state 1): E4B's 0x489220 answered 0 (al): E4B's 0x4891F0,
// Sound_PlayEffect(0x203), +1 up one.
extern "C" void __cdecl EffectKind87_Step1(void) {
    const unsigned char going = SH_AT(unsigned char (__cdecl*)(), at::kKind87Step.v)();
    if (going != 0) return;
    SH_AT(Handler, at::kKind87Reset.v)();
    SH_CALL(Sound_PlayEffect)(0x203);
    S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x488FE0 (state 2): 0x489220 answered 0: +1 up one.
extern "C" void __cdecl EffectKind87_Step2(void) {
    const unsigned char going = SH_AT(unsigned char (__cdecl*)(), at::kKind87Step.v)();
    if (going == 0) S()[1] = static_cast<unsigned char>(S()[1] + 1);
}

// original 0x489000 (state 3): the draw pass flags' low byte put back from +6,
// +1 up one (to states 4..7, 0x492750 each, which step +1 on).
extern "C" void __cdecl EffectKind87_Restore(void) {
    unsigned char* const s = S();
    Draw_PassFlags = s[6];
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x489020 (state 8): Effect_Release, then the step word 0x8034E6 0.
extern "C" void __cdecl EffectKind87_End(void) {
    SH_CALL(Effect_Release)();
    SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), 0);
}

// original 0x489030 (E4A's 0x488F80 calls it): the six panes of 0x1C at
// EffectKind30_Shards - each live (+0x18 1) at case 0 (+0x17), its timer +0x19
// = 15 * i, grey 0xF0 (+0x14..+0x16), its slot +0x1B 3 or 1 by i's parity;
// their four corners and centre pointers into the fourteen projected points
// (0x92C028, 0xC each), their animations +0x1A 1, 1, 7, 5, 5, 3; and rows 8..13
// of kKind87Points made the midpoints of two others (EffectKind87_Midpoint).
extern "C" void __cdecl EffectKind87_Setup(void) {
    for (unsigned i = 0; i < at::kPaneCount; ++i) {
        unsigned char* const r = Pane(i);
        r[0x19] = static_cast<unsigned char>(i * 0xF);
        r[0x18] = 1;
        r[0x17] = 0;
        r[0x14] = r[0x15] = r[0x16] = 0xF0;
        r[0x1B] = (i & 1) == 0 ? 3 : 1;
    }
    SetPanePoints(0, 0, 1, 4, 5);
    SetUL(Pane(0) + 0x10, PanePoint(8));
    Pane(0)[0x1A] = 1;
    SH_CALL(EffectKind87_Midpoint)(0, 5, 8);
    SetPanePoints(1, 0, 1, 2, 3);
    SetUL(Pane(1) + 0x10, PanePoint(9));
    Pane(1)[0x1A] = 1;
    SH_CALL(EffectKind87_Midpoint)(0, 3, 9);
    // pane 2: +0 point 2, +4 point 0
    SetPanePoints(2, 2, 0, 6, 4);
    SetUL(Pane(2) + 0x10, PanePoint(10));
    Pane(2)[0x1A] = 7;
    SH_CALL(EffectKind87_Midpoint)(2, 4, 0xA);
    SetPanePoints(3, 2, 3, 6, 7);
    SetUL(Pane(3) + 0x10, PanePoint(11));
    SH_CALL(EffectKind87_Midpoint)(2, 7, 0xB);
    Pane(3)[0x1A] = 5;
    SetPanePoints(4, 4, 5, 6, 7);
    SetUL(Pane(4) + 0x10, PanePoint(12));
    Pane(4)[0x1A] = 5;
    SH_CALL(EffectKind87_Midpoint)(4, 7, 0xC);
    SetPanePoints(5, 3, 1, 7, 5);
    SH_CALL(EffectKind87_Midpoint)(3, 5, 0xD);
    Pane(5)[0x1A] = 3;
    SetUL(Pane(5) + 0x10, PanePoint(13));
}

// original 0x4891F0 (E4A's 0x488FB0 calls it): the six panes live at case 3
// (+0x17), their timer +0x19 0x1E, coloured (0, 0xF0, 0).
extern "C" void __cdecl EffectKind87_FadePanes(void) {
    for (unsigned i = 0; i < at::kPaneCount; ++i) {
        unsigned char* const r = Pane(i);
        r[0x18] = 1;
        r[0x19] = 0x1E;
        r[0x17] = 3;
        r[0x14] = 0;
        r[0x15] = 0xF0;
        r[0x16] = 0;
    }
}

// original 0x489220 (E4A's 0x488FB0 and 0x488FE0 call it): EffectGte_LoadMapCamera;
// the fourteen rows of kKind87Points projected to 0x92C028..; the camera
// again; each live pane stepped by its case through the switch at 0x489380
// (a case past 3 does nothing): 0 - its timer +0x19 down, at 0 a sound of
// kKind87Sounds by kSoundIndex (which steps 0, 1, 2), case up, timer 0xF; 1 -
// red and blue +0x14 / +0x16 down 0x10, at timer 0xA ObjTrio (made
// Sprite_Current for the call) given the pane's animation +0x1A, the timer
// down, at 0 the next case and 0xF; 2 / 3 - green +0x15 down 0x10 / 8, the
// timer down, at 0 the pane dead. Cases 1..3 draw it (EffectKind87_DrawPane).
// al 1 when any pane was live, else 0.
extern "C" unsigned char __cdecl EffectKind87_StepPanes(void) {
    unsigned char any = 0;
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kKind87PointCount; ++i)
        SH_CALL(EffectGte_ProjectPoint)(reinterpret_cast<const long*>(At(::bof3::Img(at::kKind87Points.v) + at::kKind87PointStride * i)),
                                        reinterpret_cast<float*>(At(PanePoint(i))));
    SH_CALL(EffectGte_LoadMapCamera)();
    for (unsigned i = 0; i < at::kPaneCount; ++i) {
        unsigned char* const r = Pane(i);
        if (r[0x18] == 0) continue;
        any = 1;
        switch (r[0x17]) {
        case 0:
            if (r[0x19] != 0) {
                r[0x19] = static_cast<unsigned char>(r[0x19] - 1);
                continue;
            }
            {
                const unsigned k = Effect4b_SoundIndex;
                if (k >= at::kKind87SoundCount)
                    bof3::Fatal("EffectKind87_StepPanes: the sound index 0x%X is %u, past kind 0x87's three sounds at 0x%X "
                                "- the original reads the word after (docs/effect_4b.md section 6)",
                                (unsigned)Effect4b_SoundIndex_addr, k, at::kKind87Sounds.v);
                SH_CALL(Sound_PlayEffect)(Word(At(::bof3::Img(at::kKind87Sounds.v) + 2 * k)));
            }
            {
                const unsigned char n = static_cast<unsigned char>(Effect4b_SoundIndex + 1);
                Effect4b_SoundIndex = n;
                if (n > 2) Effect4b_SoundIndex = 0;
            }
            r[0x19] = 0xF;
            r[0x17] = static_cast<unsigned char>(r[0x17] + 1);
            continue;
        case 1: {
            const unsigned char t = r[0x19];
            r[0x14] = static_cast<unsigned char>(r[0x14] + 0xF0);
            r[0x16] = static_cast<unsigned char>(r[0x16] + 0xF0);
            if (t == 0xA) {
                unsigned char* const saved = Sprite_Current;
                Sprite_Current = rv::Bytes(ObjTrio);
                SH_CALL(Sprite_SetAnimation)(r[0x1A]);
                Sprite_Current = saved;
            }
            r[0x19] = static_cast<unsigned char>(r[0x19] - 1);
            if (r[0x19] == 0) {
                r[0x19] = 0xF;
                r[0x17] = static_cast<unsigned char>(r[0x17] + 1);
            }
            break;
        }
        case 2:
            r[0x15] = static_cast<unsigned char>(r[0x15] + 0xF0);
            r[0x19] = static_cast<unsigned char>(r[0x19] - 1);
            if (r[0x19] == 0) r[0x18] = 0;
            break;
        case 3:
            r[0x15] = static_cast<unsigned char>(r[0x15] + 0xF8);
            r[0x19] = static_cast<unsigned char>(r[0x19] - 1);
            if (r[0x19] == 0) r[0x18] = 0;
            break;
        default: continue;
        }
        SH_CALL(EffectKind87_DrawPane)(r);
    }
    return any;
}

// original 0x489390 (a pane of 0x1C): a draw mode (page (0x380, 0x100), dtd 1)
// committed in the pane's slot +0x1B; then four POLY_G3s, semi-transparent,
// fanned from its centre (the point +0x10 names) over its corners +0 / +4,
// +4 / +0xC, +0xC / +8 and +8 / +0 (each point's three dwords copied), the
// centre its colour +0x14..+0x16 and the corners half of it, each committed
// (0x34) in the slot; the cursor read again after each.
extern "C" void __cdecl EffectKind87_DrawPane(unsigned char* pane) {
    DrawMode(1, 0x380, 0x100, 1);
    SH_CALL(Gfx_CommitPrim)(pane[0x1B], 0xC);
    static const unsigned kCorners[4][2] = {{0, 4}, {4, 0xC}, {0xC, 8}, {8, 0}};
    for (const auto& c : kCorners) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyG3)(p);
        SH_CALL(Gpu_SetSemiTrans)(p, 1);
        std::memcpy(p + 8, At(UL(pane + 0x10)), 12);
        std::memcpy(p + 0x18, At(UL(pane + c[0])), 12);
        std::memcpy(p + 0x28, At(UL(pane + c[1])), 12);
        p[4] = pane[0x14];
        p[5] = pane[0x15];
        p[6] = pane[0x16];
        p[0x24] = p[0x14] = static_cast<unsigned char>(pane[0x14] >> 1);
        p[0x25] = p[0x15] = static_cast<unsigned char>(pane[0x15] >> 1);
        p[0x26] = p[0x16] = static_cast<unsigned char>(pane[0x16] >> 1);
        SH_CALL(Gfx_CommitPrim)(pane[0x1B], 0x34);
    }
}

// original 0x489630 (a, b, c: bytes): row c of kKind87Points (three s32) the
// midpoint of rows a and b - (a + b) sar 1, each word as the 32 bits wrap. A
// row past the fourteen is the image's .data after the table: ours aborts.
extern "C" void __cdecl EffectKind87_Midpoint(unsigned a, unsigned b, unsigned c) {
    const unsigned ra = a & 0xFFu, rb = b & 0xFFu, rc = c & 0xFFu;
    if (ra >= at::kKind87PointCount || rb >= at::kKind87PointCount || rc >= at::kKind87PointCount)
        bof3::Fatal("EffectKind87_Midpoint: rows %u, %u, %u, past kKind87Points' fourteen at 0x%X - the original reads "
                    "and writes the .data after (docs/effect_4b.md section 6)",
                    ra, rb, rc, at::kKind87Points.v);
    unsigned char* const pa = At(::bof3::Img(at::kKind87Points.v) + at::kKind87PointStride * ra);
    unsigned char* const pb = At(::bof3::Img(at::kKind87Points.v) + at::kKind87PointStride * rb);
    unsigned char* const pc = At(::bof3::Img(at::kKind87Points.v) + at::kKind87PointStride * rc);
    for (unsigned w = 0; w < 12; w += 4) SetUL(pc + w, Sar(UL(pb + w) + UL(pa + w), 1));
}

void EffectKind87_Inject() {
    BOF3_INJECT(EffectKind87_Run);
    BOF3_INJECT(EffectKind87_Start);
    BOF3_INJECT(EffectKind87_Step1);
    BOF3_INJECT(EffectKind87_Step2);
    BOF3_INJECT(EffectKind87_Restore);
    BOF3_INJECT(EffectKind87_End);
    BOF3_INJECT(EffectKind87_Setup);
    BOF3_INJECT(EffectKind87_FadePanes);
    BOF3_INJECT(EffectKind87_StepPanes);
    BOF3_INJECT(EffectKind87_DrawPane);
    BOF3_INJECT(EffectKind87_Midpoint);
}
