// Moved here by tools/move_funcs.py from effect_1c.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_20.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1c::at;

}  // namespace

namespace {

using move_script::At;

using move_script::Long;

using move_script::Word;

using U = std::uint32_t;

using Handler = void (__cdecl*)();

unsigned char* Cur() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U Key(const void* p) { return Low32(p); }

U AddressOf(const void* p) { return Key(p); }

// jmp / call [table + 4 * index]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original goes through the dword after - the next kind's table or data.
Handler Entry(U table, unsigned index, unsigned entries, const char* who) {
    if (index >= entries)
        bof3::Fatal("%s: index %u past the %u entries of the table at 0x%X (the original jumps through 0x%X)", who,
                    index, entries, (unsigned)table, (unsigned)(table + 4u * index));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4u * index))));
}

// The draw mode of the debris, the model and kind 0x20's fan: the page at
// (0x380, 0x100), then committed at slot 1.
void DebrisMode(int dtd) {
    const unsigned tpage = SH_CALL(Gpu_GetTPage)(0, 1, 0x380, 0x100) & 0xFFFFu;
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, dtd, tpage, 0);
    SH_CALL(Gfx_CommitPrim)(1, 0xC);
}

}  // namespace

// The timer word +0x2C down 1; at 0, `reload` and +1 up.
void Tick(unsigned reload) {
    unsigned char* s = Cur();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->column, rv::As<records::EffectObject>(s)->column - 1u);
    s = Cur();
    if (rv::As<records::EffectObject>(s)->column != 0) return;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->column, reload);
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46EF70: a CALL through EffectKind20_States 0x654268 by +1 (seven
// entries), then - +1 (read again) not 0 - a tail jump to EffectKind20_Draw.
extern "C" void __cdecl EffectKind20_Run(void) {
    Entry(AddressOf(EffectKind20_States), Cur()[1], 7, "EffectKind20_Run")();
    if (Cur()[1] != 0) SH_CALL(EffectKind20_Draw)();
}

// original 0x46EFA0 (EffectKind20_States[0]): the timer word +0x2C 0x20; the
// dwords +0x64 (width), +0x68 (height), +0x6C (angle) 0; +1 up.
extern "C" void __cdecl EffectKind20_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->column, 0x20);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[0], 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[1], 0);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[2], 0);
    unsigned char* const s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46EFE0 (EffectKind20_States[1]): +0x64 and +0x68 up 4; Tick(0x10).
extern "C" void __cdecl EffectKind20_Grow(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[0], rv::As<records::EffectObject>(Cur())->angle[0] + 4);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[1], rv::As<records::EffectObject>(Cur())->angle[1] + 4);
    Tick(0x10);
}

// original 0x46F030 (EffectKind20_States[2]): Tick(0xC).
extern "C" void __cdecl EffectKind20_Wait(void) { Tick(0xC); }

// original 0x46F060 (EffectKind20_States[3]): the height +0x3C up 0x40 (<<
// 16), +0x64 down 6, +0x68 up 6; Tick(0x18).
extern "C" void __cdecl EffectKind20_Rise(void) {
    rv::Current<records::EffectObject>()->height = static_cast<std::uint32_t>(rv::Current<records::EffectObject>()->height) + 0x400000;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[0], rv::As<records::EffectObject>(Cur())->angle[0] - 6);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[1], rv::As<records::EffectObject>(Cur())->angle[1] + 6);
    Tick(0x18);
}

// original 0x46F0C0 (EffectKind20_States[4]): +0x64 up 3, +0x68 down 3;
// Tick(0x10).
extern "C" void __cdecl EffectKind20_Shrink(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[0], rv::As<records::EffectObject>(Cur())->angle[0] + 3);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[1], rv::As<records::EffectObject>(Cur())->angle[1] - 3);
    Tick(0x10);
}

// original 0x46F100 (EffectKind20_States[5]): the timer down 1; at 0: the
// point (+0x34, +0x38, +0x3C) projected and again with its z (+0x38) one unit
// (0x10000) further (EffectGte_LoadMapCamera first); +0x6C the angle
// Math_Ratan2(dy, dx) of the second screen point less the first (each a float
// difference at the x87's 53 bits, stored as a float) + 0x400; the timer 0x1E,
// the fall speed +0x10 0, +1 up.
extern "C" void __cdecl EffectKind20_Aim(void) {
    unsigned char* s = Cur();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->column, rv::As<records::EffectObject>(s)->column - 1u);
    if (rv::As<records::EffectObject>(Cur())->column != 0) return;
    SH_CALL(EffectGte_LoadMapCamera)();
    s = Cur();
    long point[3] = {rv::As<records::EffectObject>(s)->x, rv::As<records::EffectObject>(s)->z, rv::As<records::EffectObject>(s)->height};
    float near[3], far[3];
    SH_CALL(EffectGte_ProjectPoint)(point, near);
    point[1] = static_cast<long>(static_cast<U>(point[1]) + 0x10000u);
    SH_CALL(EffectGte_ProjectPoint)(point, far);
    const auto dx = static_cast<float>(static_cast<double>(far[0]) - static_cast<double>(near[0]));
    const auto dy = static_cast<float>(static_cast<double>(far[1]) - static_cast<double>(near[1]));
    const int angle = SH_CALL(Math_Ratan2)(dy, dx);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[2], static_cast<U>(angle) + 0x400u);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->column, 0x1E);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(Cur())->work_10, 0);
    s = Cur();
    s[1] = static_cast<unsigned char>(s[1] + 1);
}

// original 0x46F1C0 (EffectKind20_States[6]): the fall speed +0x10 up 0x2000
// and added to z +0x38; +0x64 down 6, not below 8; +0x68 up 6; the timer down
// 1 - at 0, Effect_Release (a tail jump).
extern "C" void __cdecl EffectKind20_Fall(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(Cur())->work_10, rv::As<records::EffectWork>(Cur())->work_10 + 0x2000);
    unsigned char* s = Cur();
    rv::As<records::EffectObject>(s)->z = static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) + rv::As<records::EffectWork>(s)->work_10;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[0], rv::As<records::EffectObject>(Cur())->angle[0] - 6);
    s = Cur();
    if (static_cast<std::int32_t>(rv::As<records::EffectObject>(s)->angle[0]) < 8) rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->angle[0], 8);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(Cur())->angle[1], rv::As<records::EffectObject>(Cur())->angle[1] + 6);
    s = Cur();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->column, rv::As<records::EffectObject>(s)->column - 1u);
    if (rv::As<records::EffectObject>(Cur())->column == 0) SH_CALL(Effect_Release)();
}

// original 0x46F230 (reached only by EffectKind20_Run's tail jump; the code no
// list has in 0x46F1C0's span): a draw mode (DebrisMode, dtd 1) committed;
// E4F's 0x493090(the point +0x34 / +0x38 / +0x3C copied, the words +0x64,
// +0x68, +0x6C, 0xC0, 0) - each word pushed with the high half the register
// held, as the original's: +0x38's, +0x3C's and +0x38's high words.
extern "C" void __cdecl EffectKind20_Draw(void) {
    DebrisMode(1);
    const unsigned char* const s = Cur();
    auto* const so = rv::AsConst<records::EffectObject>(s);
    const long point[3] = {so->x, so->z, so->height};
    const U width = (static_cast<std::uint32_t>(so->z) & 0xFFFF0000u) | Word(s + 0x64);
    const U height = (static_cast<std::uint32_t>(so->height) & 0xFFFF0000u) | Word(s + 0x68);
    const U angle = (static_cast<std::uint32_t>(so->z) & 0xFFFF0000u) | Word(s + 0x6C);
    SH_AT(void (__cdecl*)(const long*, U, U, U, U, U), at::kCone.v)(point, width, height, angle, 0xC0, 0);
}

void EffectKind20_Inject() {
    BOF3_INJECT(EffectKind20_Run);
    BOF3_INJECT(EffectKind20_Start);
    BOF3_INJECT(EffectKind20_Grow);
    BOF3_INJECT(EffectKind20_Wait);
    BOF3_INJECT(EffectKind20_Rise);
    BOF3_INJECT(EffectKind20_Shrink);
    BOF3_INJECT(EffectKind20_Aim);
    BOF3_INJECT(EffectKind20_Fall);
    BOF3_INJECT(EffectKind20_Draw);
}
