// Moved here by tools/move_funcs.py from effect_6b.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_35.h"

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

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

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

// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }

// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: the height halved toward zero.
U Half(long height) { return static_cast<U>(static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2); }

// The quad's four corners projected into the POLY_FT4 at p, its depths set.
// The original also pushes a tenth pointer (a flag local) the projection does
// not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

// E6A's panel draw 0x50E1C0 (void, on Sprite_Current), raw until E6A merges.
void DrawE6A() { scenario_harness::Phase(at::kE6ADraw.v)(); }

void Draw35() { SH_CALL(EffectKind18Sub35_Draw)(); }

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

// Shut and on: the sound 0x201 unless a message is up, then +2 up (the record
// read again only when the sound was made - the same record either way unless
// the sound moves it).
void ShutOn() {
    unsigned char* s = S();
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(at::kSoundShut);
        s = S();
    }
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

}  // namespace

// original 0x50E750 (hidden in 0x50E1C0).
extern "C" void __cdecl EffectKind18Sub34_Run(void) {
    Dispatch("EffectKind18Sub34_Run", AddressOf(EffectKind18Sub34_States), EffectKind18Sub34_States_count);
}

// original 0x50E770 (sub-state 0): Place, the variant of 0x65EBEC (room 2),
// open 0x100.
extern "C" void __cdecl EffectKind18Sub34_Place(void) {
    Place("EffectKind18Sub34_Place", ::bof3::Img(at::kSub34Cells.v), 0, at::kRoom2, 0x100, &DrawE6A);
}

// original 0x50E890 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub34_WaitNear(void) { WaitNear(&DrawE6A); }

// original 0x50E970 (sub-state 2): as 0x33's.
extern "C" void __cdecl EffectKind18Sub34_Open(void) {
    if (Slide(0x20) >= 0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    DrawE6A();
}

// original 0x50E990 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub34_WaitFar(void) { WaitFar(&DrawE6A); }

// original 0x50EA60 (sub-state 4): as 0x33's.
extern "C" void __cdecl EffectKind18Sub34_Close(void) {
    if (Slide(0xFFE0u) <= 0) Shut();
    DrawE6A();
}

// original 0x50EAA0 (hidden in 0x50E1C0).
extern "C" void __cdecl EffectKind18Sub37_Run(void) {
    Dispatch("EffectKind18Sub37_Run", AddressOf(EffectKind18Sub37_States), EffectKind18Sub37_States_count);
}

// original 0x50EAC0 (sub-state 0): +8 = whether the spawn's z cell is 0; the
// variant's cell (0x65EC04, room 2); +0x3E = -0x80, the slide 0x100 (open),
// +2 up. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub37_Place(void) {
    unsigned char* const s = S();
    const U v = Variant("EffectKind18Sub37_Place", s, at::kSub37Cells.v, at::kRoom2);
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    SetWord(S() + 0x36, At(::bof3::Img(at::kSub37Cells.v) + 2 * v)[0]);
    SetWord(S() + 0x3A, At(::bof3::Img(at::kSub37Cells.v) + 2 * v + 1)[0]);
    SetWord(S() + 0x3E, 0xFF80);
    SetWord(S() + 0x30, 0x100);
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x50EB20 (sub-state 1, and EffectKind18Sub38_States[1]): once the
// count +9 has reached 8 (unsigned), +2 up; +9 up every frame. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub37_Hold(void) {
    unsigned char* s = S();
    if (s[9] >= 8) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        s = S();
    }
    s[9] = static_cast<unsigned char>(s[9] + 1);
}

// original 0x50EB40 (sub-state 2): the slide down 0x10; at 0 or below ShutOn
// (+2 up, to E6A's draw held). A tail jump to E6A's draw.
extern "C" void __cdecl EffectKind18Sub37_Close(void) {
    if (Slide(0xFFF0u) <= 0) ShutOn();
    DrawE6A();
}

// original 0x50EB80 (hidden in 0x50E1C0).
extern "C" void __cdecl EffectKind18Sub35_Run(void) {
    Dispatch("EffectKind18Sub35_Run", AddressOf(EffectKind18Sub35_States), EffectKind18Sub35_States_count);
}

// original 0x50EBA0 (sub-state 0): Place, the variant of 0x65EC1C (room 4),
// open -0x100.
extern "C" void __cdecl EffectKind18Sub35_Place(void) {
    Place("EffectKind18Sub35_Place", ::bof3::Img(at::kSub35Cells.v), 0, at::kRoom4, 0xFF00, &Draw35);
}

// original 0x50ECC0 (sub-state 1).
extern "C" void __cdecl EffectKind18Sub35_WaitNear(void) { WaitNear(&Draw35); }

// original 0x50EDA0 (sub-state 2): the slide down 0x20; at -0x100 or below +2
// up. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub35_Open(void) {
    if (Slide(0xFFE0u) <= -0x100) S()[2] = static_cast<unsigned char>(S()[2] + 1);
    Draw35();
}

// original 0x50EDC0 (sub-state 3).
extern "C" void __cdecl EffectKind18Sub35_WaitFar(void) { WaitFar(&Draw35); }

// original 0x50EE90 (sub-state 4): the slide up 0x20; at 0 or above Shut. A
// tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub35_Close(void) {
    if (Slide(0x20) >= 0) Shut();
    Draw35();
}

// original 0x50EED0 (sub-kind 0x35's draw; EffectKind18Sub38_States[3]; called
// or tail-jumped to by 0x35's states and 0x38's last): a draw mode (page 0x95)
// committed at slot 6 (0xC); one POLY_FT4: with +8 its corners lie across z
// at y (+0x3A << 7) - 0x3FC0 and run along x from (+0x36 << 7) - +0x30 -
// 0x3F40 to - 0x4040; without, across x at (+0x36 << 7) - 0x3FC0 and along z
// from (+0x3A << 7) + +0x30 - 0x3F40 to - 0x4040 (every word the low sixteen
// bits). Heights from the ground: corners 0 and 2 at vertex 0's point, 0x40 and
// 0x180 less half the ground, corners 1 and 3 at vertex 1's - four
// AreaMap_Elevation calls. Projected, the texture (+8 << 21) | 0x151010F,
// committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub35_Draw(void) {
    SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    unsigned char* const v = Vertices();
    {
        const unsigned char* const s = S();
        if (s[8] != 0) {
            const U z = (static_cast<U>(Word(s + 0x3A)) << 7) - 0x3FC0u;
            SetWord(v + 0x1A, z);
            SetWord(v + 0x12, z);
            SetWord(v + 0xA, z);
            SetWord(v + 2, z);
            const U x0 = (static_cast<U>(Word(s + 0x36)) << 7) - Word(s + 0x30) - 0x3F40u;
            SetWord(v + 0x10, x0);
            SetWord(v, x0);
            const U x1 = (static_cast<U>(Word(s + 0x36)) << 7) - Word(s + 0x30) - 0x4040u;
            SetWord(v + 0x18, x1);
            SetWord(v + 8, x1);
        } else {
            const U x = (static_cast<U>(Word(s + 0x36)) << 7) - 0x3FC0u;
            SetWord(v + 0x18, x);
            SetWord(v + 0x10, x);
            SetWord(v + 8, x);
            SetWord(v, x);
            const U z0 = (static_cast<U>(Word(s + 0x3A)) << 7) + Word(s + 0x30) - 0x3F40u;
            SetWord(v + 0x12, z0);
            SetWord(v + 2, z0);
            const U z1 = (static_cast<U>(Word(s + 0x3A)) << 7) + Word(s + 0x30) - 0x4040u;
            SetWord(v + 0x1A, z1);
            SetWord(v + 0xA, z1);
        }
    }
    const long x0 = Ground(v), z0 = Ground(v + 2);
    long h = SH_CALL(AreaMap_Elevation)(x0, z0);
    SetWord(v + 4, 0x40u - Half(h));
    h = SH_CALL(AreaMap_Elevation)(x0, z0);
    const long x1 = Ground(v + 8), z1 = Ground(v + 0xA);
    SetWord(v + 0x14, 0x180u - Half(h));
    h = SH_CALL(AreaMap_Elevation)(x1, z1);
    SetWord(v + 0xC, 0x40u - Half(h));
    h = SH_CALL(AreaMap_Elevation)(x1, z1);
    SetWord(v + 0x1C, 0x180u - Half(h));
    Project(p);
    SH_CALL(Prim_SetTexture)((static_cast<U>(S()[8]) << 21) | 0x151010Fu, p, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// original 0x50F110 (hidden in 0x50EED0).
extern "C" void __cdecl EffectKind18Sub38_Run(void) {
    Dispatch("EffectKind18Sub38_Run", AddressOf(EffectKind18Sub38_States), EffectKind18Sub38_States_count);
}

// original 0x50F130 (sub-state 0): +8 = whether the spawn's z cell is 0; the
// variant's cell (0x65EC38, room 4); +0x3E = -0x80. The leader at the cell's
// front - z first, whatever +8 says: the slide -0x100 and +2 up (to the hold);
// otherwise the slide 0 and +2 = 3 (the draw held). Nothing drawn.
extern "C" void __cdecl EffectKind18Sub38_Place(void) {
    unsigned char* s = S();
    const U v = Variant("EffectKind18Sub38_Place", s, at::kSub38Cells.v, at::kRoom4);
    s[8] = Word(s + 0x3A) == 0 ? 1 : 0;
    SetWord(S() + 0x36, At(::bof3::Img(at::kSub38Cells.v) + 2 * v)[0]);
    SetWord(S() + 0x3A, At(::bof3::Img(at::kSub38Cells.v) + 2 * v + 1)[0]);
    SetWord(S() + 0x3E, 0xFF80);
    s = S();
    if (LeaderAt(s, true, 0x8000, 0x10000)) {
        SetWord(s + 0x30, 0xFF00);
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
    } else {
        SetWord(s + 0x30, 0);
        S()[2] = 3;
    }
}

// original 0x50F1F0 (sub-state 2): the slide up 0x10; at 0 or above ShutOn
// (+2 up, to the draw held). A tail jump to 0x35's draw.
extern "C" void __cdecl EffectKind18Sub38_Close(void) {
    if (Slide(0x10) >= 0) ShutOn();
    Draw35();
}

void EffectKind18Sub35_Inject() {
    BOF3_INJECT(EffectKind18Sub34_Run);
    BOF3_INJECT(EffectKind18Sub34_Place);
    BOF3_INJECT(EffectKind18Sub34_WaitNear);
    BOF3_INJECT(EffectKind18Sub34_Open);
    BOF3_INJECT(EffectKind18Sub34_WaitFar);
    BOF3_INJECT(EffectKind18Sub34_Close);
    BOF3_INJECT(EffectKind18Sub37_Run);
    BOF3_INJECT(EffectKind18Sub37_Place);
    BOF3_INJECT(EffectKind18Sub37_Hold);
    BOF3_INJECT(EffectKind18Sub37_Close);
    BOF3_INJECT(EffectKind18Sub35_Run);
    BOF3_INJECT(EffectKind18Sub35_Place);
    BOF3_INJECT(EffectKind18Sub35_WaitNear);
    BOF3_INJECT(EffectKind18Sub35_Open);
    BOF3_INJECT(EffectKind18Sub35_WaitFar);
    BOF3_INJECT(EffectKind18Sub35_Close);
    BOF3_INJECT(EffectKind18Sub35_Draw);
    BOF3_INJECT(EffectKind18Sub38_Run);
    BOF3_INJECT(EffectKind18Sub38_Place);
    BOF3_INJECT(EffectKind18Sub38_Close);
}
