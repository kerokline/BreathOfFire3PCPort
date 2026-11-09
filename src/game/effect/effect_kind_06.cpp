// Moved here by tools/move_funcs.py from effect_1b.cpp, field_c1.cpp, frame_callees.cpp, rest_3e.cpp, scena_sx2.cpp, worldmap_area.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_06.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/config_text.h"
#include "game/effect/effect_kind_06_callees.h"
#include "game/effect_1b.h"
#include "game/effect_1b_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/field_c1.h"
#include "game/field_c1_callees.h"
#include "game/fishing_text.h"
#include "game/frame_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_3e.h"
#include "game/rest_3e_callees.h"
#include "game/scena_sx2.h"
#include "game/scena_sx2_callees.h"
#include "game/scenario_harness.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "game/worldmap_area.h"
#include "game/worldmap_area_callees.h"
#include "hook/detour.h"
#include "hook/exe_image.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = effect_kind_06::at;

}  // namespace

namespace {

using namespace move_script;

}  // namespace

using namespace worldmap_area;

namespace {

unsigned char* Sc() { return Sprite_Current; }

using move_script::At;

using move_script::Long;

using move_script::Word;

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }

U AddressOfFieldC1(const void* p) { return Low32(p); }

U L(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

std::int32_t S32(U v) { return static_cast<std::int32_t>(v); }

short S16(U v) { return static_cast<short>(v & 0xFFFFu); }

// A state table's entry, read in place (the fuzz swaps the entries for
// recorders); past the table's code the original jumps to what is not code.
Handler Entry(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = L(move_script::At(cell));
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}

// A Sprite_Objects record the original only reads, by an unchecked byte: read
// in place wherever it lands, as the original reads it.
unsigned char* SpriteRead(unsigned i) { return rv::Bytes(&Sprite_Objects[i]); }

void Tick() {
    SH_CALL(Sprite_ScriptTick)();
    SH_CALL(Sprite_UpdateScreenSlot)();
}

using Fn = void (__cdecl*)();

Fn HandlerFrameCallees(unsigned long entry) { return reinterpret_cast<Fn>(static_cast<std::uintptr_t>(entry)); }

// `cdq` / `idiv`: the original's signed division, faults included - a zero
// divisor raises the same #DE at the same point of the same computation,
// which a C++ `/` does not promise (as kind2_object.cpp's).
struct QuotRem { std::int32_t quot, rem; };

QuotRem Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quot, rem;
    __asm__ volatile("cltd\n\tidivl %%ecx" : "=a"(quot), "=&d"(rem) : "a"(dividend), "c"(divisor) : "cc");
    return {quot, rem};
}

unsigned char* S() { return Sprite_Current; }

U ULRest3e(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

U ULRest3e(U a) { return ULRest3e(move_script::At(a)); }

U AddressOfRest3e(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - the next table.
void Dispatch(const char* who, const void* table, unsigned entries) {
    const unsigned state = S()[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_3e.md section 7)",
                    who, state, entries, (unsigned)::bof3::VaOf(AddressOfRest3e(table)));
    reinterpret_cast<scenario_harness::Handler>(static_cast<std::uintptr_t>(ULRest3e(static_cast<const unsigned char*>(table) + 4 * state)))();
}

constexpr auto& AtScenaSx2 = move_script::At;

}  // namespace

namespace worldmap_area {

using move_script::At;

using move_script::Long;

using move_script::Word;

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace worldmap_area

namespace {

// A handler read from a .data table in place, as the originals' `call` /
// `jmp [index * 4 + table]` do: the index is not checked.
Handler EntryWorldmapArea(U address) { return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(worldmap_area::Long(worldmap_area::At(address))))); }

}  // namespace

// original 0x469BB0 (Effect_KindHandlers 0x06): EffectKind06_States 0x653EC8
// by +1 - _Start, _Tick, _End.
extern "C" void __cdecl EffectKind06_Run(void) { EntryWorldmapArea(::bof3::Img(EffectKind06_States_addr) + rv::As<records::EffectObject>(Cur())->state * 4u)(); }

// original 0x469BD0 (EffectKind06_States 0 and 0x653EF4's entry 0; PSX
// 0x8019B404): Sprite_SetAnimationBank(0x18); +0x48 = 0; the word +0x2E +=
// the word +0xC, +0x30 -= the word +0x10; +0x24 = 0, +0x2A = 0, +0x29 = 2.
// Variant +6 == 3: byte +0 |= 0x20, +0x5C = 1, +0x5F / +0x5E / +0x5D = 0xF8,
// +9 = 0x14; any other: +0x5C and the three = 0, +9 = 0x10. Then
// Sprite_SetAnimation(EffectKind06_Anims[+6] - 0 0 2 1 3 4, unchecked) and
// +1 = 1.
extern "C" void __cdecl EffectKind06_Start(void) {
    g.set_bank(0x18);
    Cur()[0x48] = 0;
    rv::As<records::EffectObject>(Cur())->screen_x = static_cast<std::uint16_t>(rv::As<records::EffectObject>(Cur())->screen_x) + worldmap_area::Word(Cur() + 0xC);
    rv::As<records::EffectObject>(Cur())->screen_y = static_cast<std::uint16_t>(rv::As<records::EffectObject>(Cur())->screen_y) - worldmap_area::Word(Cur() + 0x10);
    Cur()[0x24] = 0;
    Cur()[0x2A] = 0;
    Cur()[0x29] = 2;
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    if (o->param == 3) {
        o->flags = static_cast<unsigned char>(o->flags | 0x20);
        Cur()[0x5C] = 1;
        rv::As<records::EffectObject>(Cur())->shade[2] = 0xF8;
        rv::As<records::EffectObject>(Cur())->shade[1] = 0xF8;
        rv::As<records::EffectObject>(Cur())->shade[0] = 0xF8;
        rv::As<records::EffectObject>(Cur())->timer = 0x14;
    } else {
        rv::Bytes(o)[0x5C] = 0;
        rv::As<records::EffectObject>(Cur())->shade[2] = 0;
        rv::As<records::EffectObject>(Cur())->shade[1] = 0;
        rv::As<records::EffectObject>(Cur())->shade[0] = 0;
        rv::As<records::EffectObject>(Cur())->timer = 0x10;
    }
    g.set_animation(worldmap_area::At(::bof3::Img(EffectKind06_Anims_addr) + rv::As<records::EffectObject>(Cur())->param)[0]);
    rv::As<records::EffectObject>(Cur())->state = 1;
}

// original 0x469CB0 (EffectKind06_States 1; PSX 0x8019B544): EffectKind06_Ticks
// 0x653EDC by the variant +6 - _Blink for 0, 1, 2 and 4, 0x469D40 for 3,
// 0x469D10 for 5 (EffectKind06_Fade, EffectKind06_PlayOnce: field_c1.cpp).
extern "C" void __cdecl EffectKind06_Tick(void) { EntryWorldmapArea(::bof3::Img(EffectKind06_Ticks_addr) + rv::As<records::EffectObject>(Cur())->param * 4u)(); }

// original 0x469CD0 (PSX 0x8019B588): +9 - 1; at 0 +1 + 1 and nothing more.
// Otherwise Sprite_ScriptTick, and - only while bit 2 of +9 is set, so it
// blinks - a tail jump to Sprite_QueueOverlay when the kind +5 is 6, else to
// Sprite_UpdateScreen. Sprite_ScriptTick's answer is not read.
extern "C" void __cdecl EffectKind06_Blink(void) {
    rv::As<records::EffectObject>(Cur())->timer = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->timer - 1);
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (o->timer == 0) {
        o->state = static_cast<unsigned char>(o->state + 1);
        return;
    }
    g.script_tick();
    o = rv::As<records::EffectObject>(Cur());
    if ((o->timer & 4) == 0) return;
    if (o->kind == 6) g.queue_overlay();
    else g.update_screen();
}

// original 0x469D10 (EffectKind06_Ticks[5], EffectKind19_Ticks[5]; PSX 0x8019B638): the
// script stepped once; +1 up when it ended (al not 0); then a tail jump to
// Sprite_QueueOverlay for the kind +5 == 6, else Sprite_UpdateScreen.
extern "C" void __cdecl EffectKind06_PlayOnce(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)()) rv::As<records::EffectObject>(Cur())->state = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->state + 1);
    if (rv::As<records::EffectObject>(Cur())->kind == 6) SH_CALL(Sprite_QueueOverlay)();
    else SH_CALL(Sprite_UpdateScreen)();
}

// original 0x469D40 (EffectKind06_Ticks[3], EffectKind19_Ticks[3]; PSX 0x8019B6B4): +9 down;
// at 0 +1 up and nothing more. Else the tint bytes +0x5D, +0x5E, +0x5F each
// plus 0xFA (six down, in 8 bits), Sprite_ScriptTick, then by the kind +5 ==
// 6: on odd +9 the word +0x30 up, tail Sprite_QueueOverlay; else on odd +9
// the dword +0x10 down, tail Sprite_UpdateScreen (+5 and +9 read after the
// tick).
extern "C" void __cdecl EffectKind06_Fade(void) {
    rv::As<records::EffectObject>(Cur())->timer = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->timer - 1);
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (o->timer == 0) {
        o->state = static_cast<unsigned char>(o->state + 1);
        return;
    }
    o->shade[0] = static_cast<unsigned char>(o->shade[0] + 0xFA);
    rv::As<records::EffectObject>(Cur())->shade[1] = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->shade[1] + 0xFA);
    rv::As<records::EffectObject>(Cur())->shade[2] = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->shade[2] + 0xFA);
    SH_CALL(Sprite_ScriptTick)();
    o = rv::As<records::EffectObject>(Cur());
    const unsigned char kind = o->kind, left = o->timer;
    if (kind == 6) {
        if (left & 1) o->screen_y = static_cast<std::uint16_t>(o->screen_y) + 1u;
        SH_CALL(Sprite_QueueOverlay)();
    } else {
        if (left & 1) o->step_z = static_cast<U>(o->step_z) - 1u;
        SH_CALL(Sprite_UpdateScreen)();
    }
}

// original 0x469DB0 (EffectKind06_States 2; PSX 0x8019B7DC): +0x5F, +0x5E,
// +0x5D = 0, then a tail jump to Effect_Release.
extern "C" void __cdecl EffectKind06_End(void) {
    rv::As<records::EffectObject>(Cur())->shade[2] = 0;
    rv::As<records::EffectObject>(Cur())->shade[1] = 0;
    rv::As<records::EffectObject>(Cur())->shade[0] = 0;
    g.effect_release();
}

// original 0x469DE0 (Effect_KindHandlers[0x19]; PSX 0x8019B80C): a tail jump through
// EffectKind19_States 0x653EF4 by +1 - EffectKind06_Start, EffectKind19_Tick,
// EffectKind06_End.
extern "C" void __cdecl EffectKind19_Run(void) { Entry(AddressOfFieldC1(EffectKind19_States), rv::As<records::EffectObject>(Cur())->state, "EffectKind19_Run")(); }

// original 0x469E00 (EffectKind19_States[1]; PSX 0x8019B850): EffectKind19_Ticks 0x653F00 by
// the variant +6 (kind 6's ticks again: _Blink for 0, 1, 2, 4, _Fade for 3,
// _PlayOnce for 5), then the words +0x2E += +0xC and +0x30 -= +0x10 - kind
// 6's tick with a drift.
extern "C" void __cdecl EffectKind19_Tick(void) {
    Entry(AddressOfFieldC1(EffectKind19_Ticks), rv::As<records::EffectObject>(Cur())->param, "EffectKind19_Tick")();
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    o->screen_x = static_cast<std::uint16_t>(o->screen_x) + move_script::Word(rv::Bytes(o) + 0xC);
    o = rv::As<records::EffectObject>(Cur());
    o->screen_y = static_cast<std::uint16_t>(o->screen_y) - move_script::Word(rv::Bytes(o) + 0x10);
}

// original 0x469E30: effect kind 0x13, the camera turn (PSX FUN_8019BC3C):
// its state +1 through CameraTurn_States, unbounded - states 3 and up are the
// next kind's - then MapView_Redraw 2, whatever the state did to it.
extern "C" void __cdecl Effect_CameraTurn(void) {
    HandlerFrameCallees(CameraTurn_States[Sprite_Current[1]])();
    MapView_Redraw = 2;
}

// original 0x469E50: the turn's first frame (PSX FUN_8019B90C, term for
// term). The three angles to 16.16, and a step for each toward the dwords
// +0x64 / +0x68 / +0x6C over +9 frames: ((target - angle) << 16, in 32 bits)
// / frames, signed and truncating. A +9 of 0 divides by zero, as on both
// platforms (the spawners at 0x406B56 / 0x406BE3 give 16). MapView_Redraw
// takes the frame count - which Effect_CameraTurn overwrites with 2 on return.
extern "C" void __cdecl CameraTurn_Start(void) {
    const std::int32_t x = Camera_Angles[0], z = Camera_Angles[2];
    unsigned char* const object = Sprite_Current;
    const std::int32_t y = Camera_Angles[1];
    CameraTurn_Angles[0] = static_cast<long>(static_cast<std::uint32_t>(x) << 16);
    CameraTurn_Angles[1] = static_cast<long>(static_cast<std::uint32_t>(y) << 16);
    CameraTurn_Angles[2] = static_cast<long>(static_cast<std::uint32_t>(z) << 16);
    const auto step = [object](unsigned at, std::int32_t angle) {
        const std::uint32_t diff = static_cast<std::uint32_t>(move_script::Long(object + at)) - static_cast<std::uint32_t>(angle);
        return static_cast<long>(Idiv(static_cast<std::int32_t>(diff << 16), object[9]).quot);
    };
    CameraTurn_Steps[0] = step(0x64, x);
    CameraTurn_Steps[1] = step(0x68, y);
    CameraTurn_Steps[2] = step(0x6C, z);
    const unsigned char bits = Field_StatusBits;
    MapView_Redraw = object[9];
    Field_StatusBits = static_cast<unsigned char>(bits | 0x20);
    object[1] = 1;
}

// original 0x469EF0: each later frame (PSX FUN_8019BA4C). With frames left,
// each 16.16 angle takes its step (wrapping in 32 bits) and its high half goes
// to Camera_Angles; with none, the state is 2.
extern "C" void __cdecl CameraTurn_Step(void) {
    unsigned char* const object = Sprite_Current;
    if (object[9] == 0) {
        object[1] = 2;
        return;
    }
    const std::uint32_t x = static_cast<std::uint32_t>(CameraTurn_Angles[0]) + static_cast<std::uint32_t>(CameraTurn_Steps[0]);
    const std::uint32_t y = static_cast<std::uint32_t>(CameraTurn_Angles[1]) + static_cast<std::uint32_t>(CameraTurn_Steps[1]);
    const std::uint32_t z = static_cast<std::uint32_t>(CameraTurn_Angles[2]) + static_cast<std::uint32_t>(CameraTurn_Steps[2]);
    CameraTurn_Angles[0] = static_cast<long>(x);
    CameraTurn_Angles[1] = static_cast<long>(y);
    CameraTurn_Angles[2] = static_cast<long>(z);
    Camera_Angles[0] = static_cast<short>(x >> 16);
    Camera_Angles[1] = static_cast<short>(y >> 16);
    Camera_Angles[2] = static_cast<short>(z >> 16);
    object[9] = static_cast<unsigned char>(object[9] - 1);
}

// original 0x469F70: the last (PSX FUN_8019BB04). The angles exactly to the
// targets' low words, the turning bit cleared, and the object released - a
// tail jmp in the original.
extern "C" void __cdecl CameraTurn_End(void) {
    unsigned char* const object = Sprite_Current;
    Camera_Angles[0] = static_cast<short>(move_script::Word(object + 0x64));
    Camera_Angles[1] = static_cast<short>(move_script::Word(object + 0x68));
    Camera_Angles[2] = static_cast<short>(move_script::Word(object + 0x6C));
    Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & 0xDF);
    frame_callees::g.release();
}

// original 0x469FB0 (PSX 0x8019BB64; Effect_KindHandlers[4]; Effect_HoldFlag1C spawns it with
// +9 its frames): +9 down while not 0; at 0 Flags_Clear(0x904030, 0x1C) and a
// tail jump to Effect_Release.
extern "C" void __cdecl EffectKind04_HoldTick(void) {
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    const unsigned char left = o->timer;
    if (left == 0) {
        SH_CALL(Flags_Clear)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C);
        SH_CALL(Effect_Release)();
        return;
    }
    o->timer = static_cast<unsigned char>(left - 1);
}

// original 0x469FE0 (PSX 0x8019BBB8): Effect_FindFree; with a record (al not 0xFF), its +0 1
// (in use), +5 4 (the kind: Effect_KindHandlers[4] = 0x469FB0 EffectKind04_HoldTick, which counts
// +9 down a frame and at 0 clears story flag 0x1C and releases the record),
// +9 the argument's low byte (the frames); then Flags_Set(0x904030, 0x1C).
// No record: nothing, the flag not set either. Areas 49, 77, 86, 112, 117,
// 118 and Area109_SwitchHook 0x4FEEB0 call it, each with 0xF; Area49_CellHook
// answers 0 while flag 0x1C is set (a switch's cool-down). PSX 0x8019BBB8
// (pairs_propagated "gap74").
extern "C" void __cdecl Effect_HoldFlag1C(unsigned frames) {
    const unsigned i = SH_CALL(Effect_FindFree)();
    if (i == 0xFF) return;
    unsigned char* const e = rv::Bytes(&Effect_Objects[i]);
    e[0] = 1;
    e[5] = 4;
    e[9] = static_cast<unsigned char>(frames);
    SH_CALL(Flags_Set)(AtScenaSx2(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C);
}

// original 0x46A020 (Effect_KindHandlers[0x31]; PSX 0x8019BC3C): CameraZoom_States 0x653F24
// by +1, then MapView_Redraw = 2 (as Effect_CameraTurn's).
extern "C" void __cdecl EffectKind31_Run(void) {
    Entry(AddressOfFieldC1(CameraZoom_States), rv::As<records::EffectObject>(Cur())->state, "EffectKind31_Run")();
    MapView_Redraw = 2;
}

// original 0x46A040 (CameraZoom_States[0]; PSX 0x8019BC8C): CameraTurn_Start, then the
// distance's step +0x10 = ((dword +0xC - s16 Camera_Distance) << 16) / +9
// (idiv by the unsigned byte; a +9 of 0 divides by zero - CameraTurn_Start
// divides by it first; ours aborts).
extern "C" void __cdecl CameraZoom_Start(void) {
    SH_CALL(CameraTurn_Start)();
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    const U num = (static_cast<U>(o->step_x) - static_cast<U>(static_cast<int>(Camera_Distance))) << 16;
    const unsigned frames = o->timer;
    if (frames == 0) bof3::Fatal("CameraZoom_Start: +9 is 0 - the original divides by zero");
    o->step_z = S32(num) / static_cast<std::int32_t>(frames);
}

// original 0x46A070 (CameraZoom_States[1]; PSX 0x8019BCFC): CameraTurn_Step, then
// Camera_Distance = ((s16 distance << 16) + +0x10) >> 16 (arithmetic), held
// at the target +0xC once past it - below it for a falling step, above it for
// a rising one (the new distance sign-extended against the dword).
extern "C" void __cdecl CameraZoom_Step(void) {
    SH_CALL(CameraTurn_Step)();
    records::EffectObject* const o = rv::As<records::EffectObject>(Cur());
    const U sum = (static_cast<U>(static_cast<int>(Camera_Distance)) << 16) + static_cast<U>(o->step_z);
    const short now = static_cast<short>(S32(sum) >> 16);
    Camera_Distance = now;
    const std::int32_t target = S32(static_cast<U>(o->step_x));
    if (S32(static_cast<U>(o->step_z)) < 0) {
        if (now < target) Camera_Distance = static_cast<short>(target);
    } else {
        if (now > target) Camera_Distance = static_cast<short>(target);
    }
}

// original 0x46A0C0 (CameraZoom_States[2]; PSX 0x8019BD78): Camera_Distance = the word +0xC,
// then a tail jump to CameraTurn_End.
extern "C" void __cdecl CameraZoom_End(void) {
    Camera_Distance = static_cast<short>(move_script::Word(Cur() + 0xC));
    SH_CALL(CameraTurn_End)();
}

// original 0x46A0E0 (Effect_KindHandlers[0x32]; PSX 0x8019BDAC): a tail jump through
// EffectKind32_States 0x653F30 by +1.
extern "C" void __cdecl EffectKind32_Run(void) { Entry(AddressOfFieldC1(EffectKind32_States), rv::As<records::EffectObject>(Cur())->state, "EffectKind32_Run")(); }

// original 0x46A100 (EffectKind32_States[0]; PSX 0x8019BDF0): +8 = the leader's facing
// (ObjTrio +8) & 7; Sprite_SetAnimationBank(0x18); +0x48, +0x24, +0x2A = 0;
// Sprite_SetAnimation(8); the position +0x34 / +0x38 / +0x3C that of
// Sprite_Objects[+6] (the byte unchecked, read in place); the tint bytes
// +0x5C..+0x5F 0; MoveCmd_Move of a stack object whose byte +0 is 0 and +4
// is 3 (the only bytes it reads: speed 3, no bit 7), in the direction +8;
// then EffectKind32_Arc.
extern "C" void __cdecl EffectKind32_Throw(void) {
    Cur()[8] = static_cast<unsigned char>(ObjTrio[0].direction & 7);
    SH_CALL(Sprite_SetAnimationBank)(0x18);
    Cur()[0x48] = 0;
    Cur()[0x24] = 0;
    Cur()[0x2A] = 0;
    SH_CALL(Sprite_SetAnimation)(8);
    rv::As<records::EffectObject>(Cur())->x = rv::As<records::SpriteObject>(SpriteRead(rv::As<records::EffectObject>(Cur())->param))->x;
    rv::As<records::EffectObject>(Cur())->z = rv::As<records::SpriteObject>(SpriteRead(rv::As<records::EffectObject>(Cur())->param))->z;
    rv::As<records::EffectObject>(Cur())->height = rv::As<records::SpriteObject>(SpriteRead(rv::As<records::EffectObject>(Cur())->param))->height;
    Cur()[0x5C] = 0;
    rv::As<records::EffectObject>(Cur())->shade[2] = 0;
    rv::As<records::EffectObject>(Cur())->shade[1] = 0;
    rv::As<records::EffectObject>(Cur())->shade[0] = 0;
    unsigned char mover[8] = {0, 0, 0, 0, 3, 0, 0, 0};
    SH_CALL(MoveCmd_Move)(mover, Cur()[8]);
    SH_CALL(EffectKind32_Arc)();
}

// original 0x46A1E0 (EffectKind32_States[1]; also called by _Throw; PSX 0x8019BF24): the
// ground AreaMap_Elevation(+0x34, +0x38); at or above the height word +0x3E
// (signed), or with +0 bit 7, or in state 0, a hop - the rise +0x14 and fall
// +0x20 from EffectKind32_Hops[+2] (the byte unchecked, read in place), +1
// up. Then the position +0x34 += +0xC, +0x38 += +0x10, +0x3C += +0x14 and
// +0x14 -= +0x20; Sprite_ScriptTick, tail Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind32_Arc(void) {
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(o->x, o->z)));
    o = rv::As<records::EffectObject>(Cur());
    if (ground >= static_cast<short>(o->height_whole) || (o->flags & 0x80) || o->state == 0) {
        o->step_y = L(move_script::At(AddressOfFieldC1(EffectKind32_Hops) + 8u * o->step));
        o = rv::As<records::EffectObject>(Cur());
        o->work_20 = L(move_script::At(AddressOfFieldC1(EffectKind32_Hops) + 4 + 8u * o->step));
        o = rv::As<records::EffectObject>(Cur());
        o->state = static_cast<unsigned char>(o->state + 1);
        o = rv::As<records::EffectObject>(Cur());
    }
    o->x = static_cast<U>(o->x) + static_cast<U>(o->step_x);
    o = rv::As<records::EffectObject>(Cur());
    o->z = static_cast<U>(o->z) + static_cast<U>(o->step_z);
    o = rv::As<records::EffectObject>(Cur());
    o->height = static_cast<U>(o->height) + static_cast<U>(o->step_y);
    o = rv::As<records::EffectObject>(Cur());
    o->step_y = static_cast<U>(o->step_y) - static_cast<U>(o->work_20);
    Tick();
}

// original 0x46A290 (EffectKind32_States[2]; PSX 0x8019C02C): the ground under +0x34 / +0x38;
// unless it lies within 0x80 below the height word +0x3E (ground <= height <
// ground + 0x80) with +0 bit 7 clear, +1 up and Sound_PlayEffect(0x106). Then
// +0x3C += +0x14, +0x14 -= +0x20 (no step across), Sprite_ScriptTick,
// Sprite_UpdateScreenSlot.
extern "C" void __cdecl EffectKind32_Settle(void) {
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    const short ground = S16(static_cast<U>(SH_CALL(AreaMap_Elevation)(o->x, o->z)));
    o = rv::As<records::EffectObject>(Cur());
    const short height = static_cast<short>(o->height_whole);
    if (ground + 0x80 <= height || ground > height || (o->flags & 0x80)) {
        o->state = static_cast<unsigned char>(o->state + 1);
        SH_CALL(Sound_PlayEffect)(0x106);
        o = rv::As<records::EffectObject>(Cur());
    }
    o->height = static_cast<U>(o->height) + static_cast<U>(o->step_y);
    o = rv::As<records::EffectObject>(Cur());
    o->step_y = static_cast<U>(o->step_y) - static_cast<U>(o->work_20);
    Tick();
}

// original 0x46A310 (PSX 0x8019C0F4; 49 cells of the effect kinds' state tables, among them
// EffectKind32_States[3], EffectKind37_States[2], EffectKind18_States'):
// a tail jump to Effect_Release.
extern "C" void __cdecl Effect_StateRelease(void) { SH_CALL(Effect_Release)(); }

// original 0x46A320 (hidden in 0x46A1E0's catalog extent; EffectKind32_Arc
// ends at 0x46A287): jmp [EffectKind37_States + +1 * 4], unbounded.
extern "C" void __cdecl EffectKind37_Run(void) { Dispatch("EffectKind37_Run", EffectKind37_States, EffectKind37_States_count); }

// original 0x46A340 (EffectKind37_States[0]): Sprite_SetAnimationBank(the word
// +0x2C); +0x48 = 1, +0x24 = 0, +0x2A = +6 >> 7; Sprite_SetAnimation(+6 &
// 0x7F); +1 = 1.
extern "C" void __cdecl EffectKind37_Start(void) {
    SH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(rv::As<records::EffectObject>(Cur())->column));
    Cur()[0x48] = 1;
    Cur()[0x24] = 0;
    Cur()[0x2A] = static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->param >> 7);
    SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(rv::As<records::EffectObject>(Cur())->param & 0x7F));
    rv::As<records::EffectObject>(Cur())->state = 1;
}

// original 0x46A390 (EffectKind37_States[1]): Sprite_ScriptTickOnce; when it
// ended a pass, +9 down and at 0 +1 = 2. Also +1 = 2 when +7 is not 0 and
// equals the frame word +0x58. Tail Sprite_UpdateScreen.
extern "C" void __cdecl EffectKind37_Play(void) {
    const unsigned char ended = SH_CALL(Sprite_ScriptTickOnce)();
    records::EffectObject* o = rv::As<records::EffectObject>(Cur());
    if (ended) {
        o->timer = static_cast<unsigned char>(o->timer - 1);
        o = rv::As<records::EffectObject>(Cur());
        if (o->timer == 0) {
            o->state = 2;
            o = rv::As<records::EffectObject>(Cur());
        }
    }
    if (rv::Bytes(o)[7] != 0 && static_cast<U>(rv::Bytes(o)[7]) == o->script_pos) o->state = 2;
    SH_CALL(Sprite_UpdateScreen)();
}

// original 0x46A3E0 (Effect_KindHandlers[0x92]): unless +0 has bit 6, the
// sprite object +0xB (of Sprite_Objects' 30) moves by the record's words +0x2E
// / +0x30 and takes its +0x29; once that object's +0 bit 0 is clear,
// Effect_Release.
extern "C" void __cdecl EffectKind92_Follow() {
    unsigned char* const s = Sc();
    const unsigned n = s[0xB];
    if (n >= effect_1b::at::kSpriteCount)
        bof3::Fatal("EffectKind92_Follow: sprite object %u past the %u (the original writes past Sprite_Objects)", n,
                    effect_1b::at::kSpriteCount);
    unsigned char* const o = rv::Bytes(Sprite_Objects) + effect_1b::at::kSpriteStride * n;
    auto* const oo = rv::As<records::SpriteObject>(o);
    if ((s[0] & 0x40) == 0) {
        oo->screen_x = static_cast<std::uint16_t>(oo->screen_x) + rv::As<records::EffectWork>(s)->work_2e;
        oo->screen_y = static_cast<std::uint16_t>(oo->screen_y) + rv::As<records::EffectWork>(s)->work_30;
        o[0x29] = s[0x29];
    }
    if ((o[0] & 1) == 0) SH_CALL(Effect_Release)();
}

void EffectKind06_InjectFromEffect1b() {
    BOF3_INJECT(EffectKind92_Follow);
}

void EffectKind06_InjectFromFieldC1() {
    BOF3_INJECT(EffectKind06_PlayOnce);
    BOF3_INJECT(EffectKind06_Fade);
    BOF3_INJECT(EffectKind19_Run);
    BOF3_INJECT(EffectKind19_Tick);
    BOF3_INJECT(EffectKind04_HoldTick);
    BOF3_INJECT(EffectKind31_Run);
    BOF3_INJECT(CameraZoom_Start);
    BOF3_INJECT(CameraZoom_Step);
    BOF3_INJECT(CameraZoom_End);
    BOF3_INJECT(EffectKind32_Run);
    BOF3_INJECT(EffectKind32_Throw);
    BOF3_INJECT(EffectKind32_Arc);
    BOF3_INJECT(EffectKind32_Settle);
    BOF3_INJECT(Effect_StateRelease);
    BOF3_INJECT(EffectKind37_Start);
    BOF3_INJECT(EffectKind37_Play);
}

void EffectKind06_InjectFromFrameCallees() {
    BOF3_INJECT(Effect_CameraTurn);
    BOF3_INJECT(CameraTurn_Start);
    BOF3_INJECT(CameraTurn_Step);
    BOF3_INJECT(CameraTurn_End);
}

void EffectKind06_InjectFromRest3e() {
    BOF3_INJECT(EffectKind37_Run);
}

void EffectKind06_InjectFromScenaSx2() {
    BOF3_INJECT(Effect_HoldFlag1C);
}

void EffectKind06_InjectFromWorldmapArea() {
    BOF3_INJECT(EffectKind06_Run);
    BOF3_INJECT(EffectKind06_Start);
    BOF3_INJECT(EffectKind06_Tick);
    BOF3_INJECT(EffectKind06_Blink);
    BOF3_INJECT(EffectKind06_End);
}
