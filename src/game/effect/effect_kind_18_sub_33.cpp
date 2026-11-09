// Moved here by tools/move_funcs.py from effect_6b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_33.h"

#include <bit>
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_6b.h"
#include "game/effect_6b_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_6b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

U AddressOf(const void* p) { return Low32(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6b.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(At(table + 4 * sub))))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its table has.
U Variant(const char* who, const unsigned char* s, U table, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its table 0x%X has room for - the original reads on "
                    "into the data after (docs/effect_6b.md section 7)",
                    who, (int)v, room, (unsigned)table);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx; cmp eax, n; jg: |d| as the original
// computes it - 0x80000000 stays itself, negative, and so counts as near.
std::int32_t Abs(U d) {
    const U m = static_cast<std::int32_t>(d) < 0 ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ m) - m);
}

// A point against a cell's centre on one axis: point - ((cell + 1) << 16 |
// 0x8000) (inc; shl 16; or dh, 0x80).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell + 1) << 16) | 0x8000u); }

// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }

// The tests every waiting state makes: |a| within `first`, then |b| or |b -
// 0x10000| within `second` (the cell or the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}

U LeaderX() { return static_cast<std::uint32_t>(ObjTrio[0].x); }

U LeaderZ() { return static_cast<std::uint32_t>(ObjTrio[0].z); }

// The leader at the record's cell: across the x axis first (the centre), then
// z - or, z_first, z first, then x.
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// E6A's panel draw 0x50E1C0 (void, on Sprite_Current), raw until E6A merges.
void DrawE6A() { scenario_harness::Phase(at::kE6ADraw.v)(); }

using Draw = void (*)();

// The place state sub-kinds 0x33, 0x34, 0x35 and 0x3B share (0x50E420): +8 =
// whether the spawn's z cell is 0; the variant's cell into +0x36 / +0x3A (and,
// with `heights`, its lift into +0x3E); the slide +0x30 0, +2 up. The leader
// already at the cell's front (z first when +8): the slide `open` and +2 = 3.
// Then the draw.
void Place(const char* who, U cells, U heights, unsigned room, U open, Draw draw) {
    unsigned char* s = S();
    const U v = Variant(who, s, cells, room);
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    SetWord(S() + 0x36, At(cells + 2 * v)[0]);
    SetWord(S() + 0x3A, At(cells + 2 * v + 1)[0]);
    if (heights != 0) SetWord(S() + 0x3E, Word(At(heights + 2 * v)));
    SetWord(S() + 0x30, 0);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
    s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        SetWord(s + 0x30, open);
        S()[2] = 3;
    }
    draw();
}

// The leader at the cell's front: sound 0x200 unless a message is up, +2 up
// (Sprite_Current read after the sound). The draw.
void WaitNear(Draw draw) {
    unsigned char* s = S();
    if (LeaderAt(s, s[8] != 0, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(at::kSoundOpen);
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    draw();
}

// Once the leader is two cells away (the front test at 0x20000 both ways
// fails), +2 up. The draw.
void WaitFar(Draw draw) {
    unsigned char* const s = S();
    if (!LeaderAt(s, s[8] != 0, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    draw();
}

// The slide +0x30 moved by `step` (a word add); answers it as a signed word.
std::int32_t Slide(U step) {
    SetWord(S() + 0x30, Word(S() + 0x30) + step);
    return S16(S() + 0x30);
}

// Shut: the sound 0x201 unless a message is up, then +2 = 1 (read after the
// sound).
void Shut() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(at::kSoundShut);
    S()[2] = 1;
}

}  // namespace

// original 0x50E400 (hidden in E6A's 0x50E1C0): jmp [EffectKind18Sub33_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub33_Run(void) {
    Dispatch("EffectKind18Sub33_Run", AddressOf(EffectKind18Sub33_States), EffectKind18Sub33_States_count);
}

// original 0x50E420 (sub-state 0): Place, the variant of 0x65EBD4 (room 2),
// open 0x100.
extern "C" void __cdecl EffectKind18Sub33_Place(void) {
    Place("EffectKind18Sub33_Place", ::bof3::Img(at::kSub33Cells.v), 0, at::kRoom2, 0x100, &DrawE6A);
}

// original 0x50E540 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub33_WaitNear(void) { WaitNear(&DrawE6A); }

// original 0x50E620 (sub-state 2): the slide up 0x20; at 0x100 +2 up. A tail
// jump to the draw.
extern "C" void __cdecl EffectKind18Sub33_Open(void) {
    if (Slide(0x20) >= 0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    DrawE6A();
}

// original 0x50E640 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub33_WaitFar(void) { WaitFar(&DrawE6A); }

// original 0x50E710 (sub-state 4): the slide down 0x20; at 0 or below Shut. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub33_Close(void) {
    if (Slide(0xFFE0u) <= 0) Shut();
    DrawE6A();
}

void EffectKind18Sub33_Inject() {
    BOF3_INJECT(EffectKind18Sub33_Run);
    BOF3_INJECT(EffectKind18Sub33_Place);
    BOF3_INJECT(EffectKind18Sub33_WaitNear);
    BOF3_INJECT(EffectKind18Sub33_Open);
    BOF3_INJECT(EffectKind18Sub33_WaitFar);
    BOF3_INJECT(EffectKind18Sub33_Close);
}
