// Moved here by tools/move_funcs.py from effect_5b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_0c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5b.h"
#include "game/effect_5b_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using I = std::int32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

unsigned char& B(U a) { return *At(a); }

U AddressOf(const void* p) { return Low32(p); }

using Handler = scenario_harness::Handler;

// cdq; sub eax, edx; sar eax, 1 on a whole dword
I Halve(I v) { return static_cast<I>(static_cast<U>(v) - static_cast<U>(v >> 31)) >> 1; }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next table, a data dword or a null.
void DispatchEffect5b(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// One of the image's byte arrays by an index the record supplies (unchecked by
// the original, which reads the next array or table past its extent).
unsigned Byte(const char* who, U array, unsigned extent, U index) {
    if (index >= extent)
        bof3::Fatal("%s: index %d past the %u bytes of 0x%X - the original reads what follows "
                    "(docs/effect_5b.md section 7)",
                    who, static_cast<int>(index), extent, (unsigned)::bof3::VaOf(array));
    return B(array + index);
}

unsigned char* Prim() { return Gfx_PacketNext; }

unsigned char* Flags() { return At(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// A VRAM move of an 8 x 0x78 column: the rect at (frame + 1) * 8 + `from`,
// 0x160, onto (`to`, 0x160); committed to slot 6.
void MoveColumn(U from, U to) {
    unsigned char rect[8];
    SetWord(rect, from);
    SetWord(rect + 2, 0x160);
    SetWord(rect + 4, 8);
    SetWord(rect + 6, 0x78);
    SH_CALL(Gpu_SetDrawMove)(Prim(), rect, to, 0x160);
    SH_CALL(Gfx_CommitPrim)(6, 0x18);
}

// +9 up one; when past `last`, +9 0 (when `clear`) and +2 to `next` (or up one
// when next is negative).
void Step(unsigned last, bool clear, int next) {
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] <= last) return;
    if (clear) S()[9] = 0;
    if (next < 0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    else S()[2] = static_cast<unsigned char>(next);
}

unsigned Sub0CFlag(const char* who) {
    return Byte(who, ::bof3::Img(at::kSub0CFlags.v), at::kSub0CFlagCount, static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(S())->x_whole))));
}

}  // namespace

// original 0x4FFBB0 (EffectKind18_States[12], hidden in 0x4FFA90): jmp
// [EffectKind18Sub0C_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub0C_Run(void) {
    DispatchEffect5b("EffectKind18Sub0C_Run", AddressOf(EffectKind18Sub0C_States), EffectKind18Sub0C_States_count);
}

// original 0x4FFBD0 (state 0): +9 0; the column's story flag (_Flags[+0x36])
// set: frame 3 and +2 3; clear: frame 0 and +2 1.
extern "C" void __cdecl EffectKind18Sub0C_Start(void) {
    S()[9] = 0;
    if (SH_CALL(Flags_Test)(Flags(), Sub0CFlag("EffectKind18Sub0C_Start"))) {
        SH_CALL(EffectKind18Sub0C_MoveFrame)(3);
        S()[2] = 3;
        return;
    }
    SH_CALL(EffectKind18Sub0C_MoveFrame)(0);
    S()[2] = 1;
}

// original 0x4FFC30 (state 1): the column's flag set: sound 0x201, +9 0, +2 up.
extern "C" void __cdecl EffectKind18Sub0C_Wait(void) {
    if (!SH_CALL(Flags_Test)(Flags(), Sub0CFlag("EffectKind18Sub0C_Wait"))) return;
    SH_CALL(Sound_PlayEffect)(0x201);
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x4FFC70 (state 2): the frame _OpenFrames[+9 >> 1]; +9 up, past 4:
// +9 0 and +2 up.
extern "C" void __cdecl EffectKind18Sub0C_Open(void) {
    SH_CALL(EffectKind18Sub0C_MoveFrame)(
        Byte("EffectKind18Sub0C_Open", ::bof3::Img(at::kSub0COpenFrames.v), at::kSub0DArray, S()[9] >> 1));
    Step(4, true, -1);
}

// original 0x4FFCC0 (state 3): the flag clear: +9 0 and +2 up; +9 up, past 0x40:
// +9 0 and, on an odd Rand, +2 5.
extern "C" void __cdecl EffectKind18Sub0C_Idle(void) {
    if (!SH_CALL(Flags_Test)(Flags(), Sub0CFlag("EffectKind18Sub0C_Idle"))) {
        S()[9] = 0;
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    }
    unsigned char* const s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    if (S()[9] <= 0x40) return;
    S()[9] = 0;
    if (SH_CALL(Rand)() & 1) S()[2] = 5;
}

// original 0x4FFD30 (state 4): the frame _ShutFrames[+9 / 3]; +9 up, past 3: +2 1.
extern "C" void __cdecl EffectKind18Sub0C_Shut(void) {
    SH_CALL(EffectKind18Sub0C_MoveFrame)(
        Byte("EffectKind18Sub0C_Shut", ::bof3::Img(at::kSub0CShutFrames.v), at::kSub0DArray, S()[9] / 3u));
    Step(3, false, 1);
}

// original 0x4FFD80 (state 5): the frame _FlashFrames[+9 >> 1]; +9 up, past 6:
// +9 0 and +2 3.
extern "C" void __cdecl EffectKind18Sub0C_Flash(void) {
    SH_CALL(EffectKind18Sub0C_MoveFrame)(
        Byte("EffectKind18Sub0C_Flash", ::bof3::Img(at::kSub0CFlashFrames.v), at::kSub0DArray, S()[9] >> 1));
    Step(6, true, 3);
}

// original 0x4FFDD0 (frame): Gpu_SetDrawMove of the rect ((frame + 1) * 8 + 0x1C0,
// 0x160, 8, 0x78) onto (_Columns[+0x36] + 0x1C0, 0x160), committed to slot 6.
extern "C" void __cdecl EffectKind18Sub0C_MoveFrame(unsigned frame) {
    const U offset = static_cast<U>(Halve(static_cast<I>((frame + 1) << 4)));
    const U to = Byte("EffectKind18Sub0C_MoveFrame", ::bof3::Img(at::kSub0CColumns.v), at::kSub0CColumnCount,
                      static_cast<U>(static_cast<std::int32_t>(static_cast<std::int16_t>(rv::As<records::EffectObject>(S())->x_whole)))) + 0x1C0u;
    MoveColumn(offset + 0x1C0, to);
}

void EffectKind18Sub0c_Inject() {
    BOF3_INJECT(EffectKind18Sub0C_Run);
    BOF3_INJECT(EffectKind18Sub0C_Start);
    BOF3_INJECT(EffectKind18Sub0C_Wait);
    BOF3_INJECT(EffectKind18Sub0C_Open);
    BOF3_INJECT(EffectKind18Sub0C_Idle);
    BOF3_INJECT(EffectKind18Sub0C_Shut);
    BOF3_INJECT(EffectKind18Sub0C_Flash);
    BOF3_INJECT(EffectKind18Sub0C_MoveFrame);
}
