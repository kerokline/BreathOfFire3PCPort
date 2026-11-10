// Moved here by tools/move_funcs.py from effect_6c.cpp, effect_6d.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_5b.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect/effect_kind_18_sub_5b_callees.h"
#include "game/effect_6c_callees.h"
#include "game/effect_6d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_kind_18_sub_5b::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

using Handler = scenario_harness::Handler;

unsigned char* S() { return Sprite_Current; }

U ULEffect6c(const unsigned char* p) { return static_cast<U>(Long(p)); }

U ULEffect6c(U a) { return ULEffect6c(At(a)); }

void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }

void SetUL(U a, U v) { SetUL(At(a), v); }

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

U AddressOf(const void* p) { return Low32(p); }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}

float F(U a) { return F(At(a)); }

double D(U a) { return static_cast<double>(F(a)); }

void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

void SetD(unsigned char* p, double d) { SetF(p, static_cast<float>(d)); }

void SetD(U a, double d) { SetD(At(a), d); }

// fild dword: a whole number, exact in double.
double Fi(U v) { return static_cast<double>(I(v)); }

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U Ftol(double v) {
    if (!(v > -9.2233720368547758e18 && v < 9.2233720368547758e18)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

unsigned char* Vertices() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }

void SetV(unsigned offset, U v) { SetWord(Vertices() + offset, v); }

const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6c.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect6c(At(table + 4 * sub))))();
}

#define E6C_TABLE(name) AddressOf(name), name##_count

// A draw item by index: the pool's (DIV-0062), a Fatal past its count where the
// original reads past the array.
unsigned char* Item(const char* who, U item) {
    if (item >= draw_pool::Count())
        bof3::Fatal("%s: draw item %u, past the pool's %u - the original reads past DrawItems (docs/effect_6c.md "
                    "section 7)",
                    who, (unsigned)item, draw_pool::Count());
    return draw_pool::Items() + item * at::kDrawItemStride;
}

// The dword record of the map cell (x, z) at `run` records into its run, as
// the original indexes it: the cell's word at AreaMap_Header + (x + width * z
// + 2 * base) * 2, plus base, plus (height * width + 1) / 2 - every byte and
// word re-read where the original re-reads them.
unsigned char* CellRecord(U x, U z, U run) {
    const U header = ULEffect6c(::bof3::Img(at::kAreaHeader.v));
    const U width = header & 0xFF;
    const U base = Word(At(::bof3::Img(AreaMap_Header_addr) + 0x2));
    const U word = Word(At(::bof3::Img(AreaMap_Header_addr) + 2 * (x + width * z + 2 * base)));
    const U height = (header >> 8) & 0xFF;
    const U half = (height * width + 1) / 2;   // cdq; sub eax, edx; sar 1 - never negative here
    return At(::bof3::Img(AreaMap_Header_addr) + 4 * (base + word + half + run));
}

// Sub-kind 0x5A's cycles: +9 up; every sixth frame +9 wraps to 0 at 0x18 and
// the move (rows[+9 / 6], y).
void Cycle5A(U rows, int y) {
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    unsigned char* const s = S();
    const U n = s[9];
    if (n % 6 != 0) return;
    s[9] = static_cast<unsigned char>(n < 0x18 ? n : 0);
    SH_CALL(EffectKind18Sub5A_Move)(*At(rows + S()[9] / 6), y);
}

// The three parts, the offsets set before each as the states set them.
void SetOffset(U x_bits, U y_bits) {
    SetUL(::bof3::Img(at::kOffsetX.v), x_bits);
    SetUL(::bof3::Img(at::kOffsetY.v), y_bits);
}

void Part(int part) { SH_CALL(EffectKind18Sub5B_DrawPart)(part); }

// float(-(+9 << shift)) / float(+9 << shift) as fild / fstp store them.
void OffsetX(bool negative, unsigned shift) {
    const U v = static_cast<U>(S()[9]) << shift;
    SetD(::bof3::Img(at::kOffsetX.v), Fi(negative ? 0u - v : v));
}

// Sub-states 2 and 4: the lift 0; part 0 at -(+9 << 3), part 1 at +9 << 3 (read
// again), part 2 at 0; +9 up; past `end` +9 0 and +2 up.
void Slide5B(unsigned end) {
    SetUL(::bof3::Img(at::kOffsetY.v), 0);
    OffsetX(true, 3);
    Part(0);
    OffsetX(false, 3);
    Part(1);
    SetUL(::bof3::Img(at::kOffsetX.v), 0);
    Part(2);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= end) return;
    S()[9] = 0;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

std::int32_t S16(const unsigned char* p) { return static_cast<std::int16_t>(Word(p)); }

std::int32_t S16(U a) { return S16(At(a)); }

U S8U(unsigned char b) { return static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(b))); }

const short* Vertex(unsigned i) { return reinterpret_cast<const short*>(Vertices() + 8 * i); }

// The float bits of an int (fild; fstp dword).
U FloatBits(std::int32_t v) {
    const float f = static_cast<float>(v);
    U b;
    std::memcpy(&b, &f, sizeof b);
    return b;
}

long double Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}

// The CRT's _ftol 0x5B9550 on the value x87 holds: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000; the callers keep eax's low word.
U FtolEffect6d(long double v) {
    if (!(v > -9.2233720368547758e18L && v < 9.2233720368547758e18L)) return 0;
    return static_cast<U>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax
// * 4]: the table's `entries` handlers read in place (the fuzz swaps the cells
// for recorders); a Fatal past them, where the original jumps through the dword
// after - another table or data.
void DispatchEffect6d(const char* who, U table, unsigned entries) {
    const unsigned sub = Sprite_Current[2];
    if (sub >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_6d.md section 7)",
                    who, sub, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * sub)))();
}

// The variant the spawn's x cell (+0x36, movsx) names, checked against the
// room its tables have.
U Variant(const char* who, const unsigned char* s, unsigned room) {
    const std::int32_t v = S16(s + 0x36);
    if (v < 0 || v >= static_cast<std::int32_t>(room))
        bof3::Fatal("%s: the variant +0x36 is %d, past the %u its tables have room for - the original reads on into "
                    "the next table (docs/effect_6d.md section 7)",
                    who, (int)v, room);
    return static_cast<U>(v);
}

// cdq; xor eax, edx; sub eax, edx: |d| as the original computes it -
// 0x80000000 stays itself, negative.
std::int32_t Abs(U d) {
    const U m = I(d) < 0 ? 0xFFFFFFFFu : 0u;
    return I((d ^ m) - m);
}

// A point against a cell's centre on one axis: point - ((cell + 1) << 16 |
// 0x8000) (inc; shl 16; or dh, 0x80).
U FromCentre(U point, std::int32_t cell) { return point - ((static_cast<U>(cell + 1) << 16) | 0x8000u); }

// ... and against its edge: point - (cell << 16).
U FromEdge(U point, std::int32_t cell) { return point - (static_cast<U>(cell) << 16); }

// |a| within `first`, then |b| or |b - 0x10000| within `second` (the cell or
// the one past it).
bool Within(U a, std::int32_t first, U b, std::int32_t second) {
    if (Abs(a) > first) return false;
    if (Abs(b) <= second) return true;
    return Abs(b - 0x10000u) <= second;
}

U LeaderX() { return static_cast<std::uint32_t>(ObjTrio[0].x); }

U LeaderZ() { return static_cast<std::uint32_t>(ObjTrio[0].z); }

// The leader at the record's cell, z across the centre first (sub-kind 0x5E,
// and 0x5D with +8 set), or x first (0x5D without).
bool LeaderAt(const unsigned char* s, bool z_first, std::int32_t first, std::int32_t second) {
    const std::int32_t cx = S16(s + 0x36), cz = S16(s + 0x3A);
    if (z_first) return Within(FromCentre(LeaderZ(), cz), first, FromEdge(LeaderX(), cx), second);
    return Within(FromCentre(LeaderX(), cx), first, FromEdge(LeaderZ(), cz), second);
}

// A vertex's ground point: (s16 + 0x4000) << 9, the 16.16 the map reads.
long Ground(const unsigned char* word) { return static_cast<long>(static_cast<U>(S16(word) + 0x4000) << 9); }

// movsx eax, ax; cdq; sub eax, edx; sar 1: half a height, toward zero.
U Half(long height) { return static_cast<U>(static_cast<std::int16_t>(static_cast<U>(height) & 0xFFFFu) / 2); }

// The quad's four corners projected into the POLY_FT4 / G4 at p, its depths
// set. The original also pushes a tenth pointer (a flag local) the projection
// does not read.
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(Vertex(0), Vertex(1), Vertex(2), Vertex(3), reinterpret_cast<float*>(p + 8),
                               reinterpret_cast<float*>(p + 0x18), reinterpret_cast<float*>(p + 0x28),
                               reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

// The draw mode the panels put first: Gpu_SetDrawMode(prim, 0, 0, 0x95, 0).
void PanelMode() { SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0); }

void Link(int dy, unsigned size) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), dy, size);
}

// A slide of +0x30 by `by`; answers +0x30 (signed, read again).
std::int32_t Slide(U by) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + by);
    return S16(S() + 0x30);
}

// Closed: the sound unless a message is up (Field_Request), +2 = 1 (read after
// the sound).
void Shut() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundShut));
    S()[2] = 1;
}

void StepUp() { S()[2] = static_cast<unsigned char>(S()[2] + 1); }

}  // namespace

// The pair as every state but the first draws it: side 0 at `dy0`, side 1 at 1.
static void Sub5CPair(int dy0) {
    SH_CALL(EffectKind18Sub5C_DrawPanel)(0, dy0);
    SH_CALL(EffectKind18Sub5C_DrawPanel)(1, 1);
}

// The opening states' dy for side 0: -1, or -2 once +0x30 reaches 0x80.
static int Sub5CDy() { return S16(S() + 0x30) >= 0x80 ? -2 : -1; }

// original 0x513860 (EffectKind18_States[0x5A], hidden in 0x513410): jmp
// [EffectKind18Sub5A_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5A_Run(void) {
    Dispatch("EffectKind18Sub5A_Run", E6C_TABLE(EffectKind18Sub5A_States));
}

// original 0x513880 (sub-state 0): +9 0; story flag 0x80 set: +2 = 5 (the
// second cycle); else +2 up.
extern "C" void __cdecl EffectKind18Sub5A_Start(void) {
    S()[9] = 0;
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x80) != 0)
        S()[2] = 5;
    else
        S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5138C0 (sub-state 1): once Cond_ByteFE is 2, +2 up.
extern "C" void __cdecl EffectKind18Sub5A_Wait(void) {
    if (Cond_ByteFE == 2) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5138E0 (sub-state 2): the map rows swept by +9
// (EffectKind18Sub5A_SetTiles(+9 >> 3, +9 >> 2 & 1)); +9 up; at 0xD0 +2 up.
extern "C" void __cdecl EffectKind18Sub5A_Animate(void) {
    const U n = S()[9];
    SH_CALL(EffectKind18Sub5A_SetTiles)(I(n >> 3), I((n >> 2) & 1));
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] >= 0xD0) S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x513920 (sub-state 3): story flag 0x80 set; +9 0; the move (0xC0,
// 0); the first records of cells (0x11, 0x61) and (0x12, 0x61) given 0xE6 and
// 0xE7 and their items textured (2) - DIV-0062's site 0x5139C2; +2 up.
extern "C" void __cdecl EffectKind18Sub5A_Set(void) {
    SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x80);
    S()[9] = 0;
    SH_CALL(EffectKind18Sub5A_Move)(0xC0, 0);
    for (U e = 0; I(e) < 2; ++e) {
        unsigned char* const record = CellRecord(e + 0x11, 0x61, 0);
        SetUL(record, (ULEffect6c(record) & 0xFFFFFF00u) | (e + 0xE6));
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(I(e + 0x11), 0x61));
        if (item != 0) SH_CALL(Prim_SetTexture)(ULEffect6c(record), Item("EffectKind18Sub5A_Set", item), 2);
    }
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x5139F0 (sub-state 4): the cycle through kSlide5A, y 0xC0.
extern "C" void __cdecl EffectKind18Sub5A_Cycle(void) { Cycle5A(::bof3::Img(at::kSlide5A.v), 0xC0); }

// original 0x513A60 (sub-state 5): the cycle through kSlide5B, y 0.
extern "C" void __cdecl EffectKind18Sub5A_CycleOpen(void) { Cycle5A(::bof3::Img(at::kSlide5B.v), 0); }

// original 0x513AD0 (called by EffectKind18Sub5A_Animate): map row z = step +
// 0x47, the bytes of two six-byte stack tables picked by step % 3 + 3 * bank:
// the second record of cell (0xD, z) gets the first's byte (5, 4, 3 / 0xB, 0xA,
// 9) and, where MapView_ItemAt answers an item, Prim_SetTexture(it, the item
// its link word names, 2) - DIV-0062's sites 0x513B9B / 0x513BAC; then the
// first record of cells (0xE..0x15, z) gets (x - 0xB) | the second's byte (0x30,
// 0x40, 0x50 / 0xC0, 0xD0, 0xE0) and its item textured (2) - the site 0x513C41.
extern "C" void __cdecl EffectKind18Sub5A_SetTiles(int step_word, int bank_word) {
    static const unsigned char kFirst[6] = {5, 4, 3, 0xB, 0xA, 9};
    static const unsigned char kSecond[6] = {0x30, 0x40, 0x50, 0xC0, 0xD0, 0xE0};
    const U step = static_cast<U>(step_word), bank = static_cast<U>(bank_word);
    const U z = step + 0x47;
    const U pick = static_cast<U>(I(step) % 3) + bank * 3;
    if (pick >= 6)
        bof3::Fatal("EffectKind18Sub5A_SetTiles: (step %d, bank %d) picks %d, past the six bytes of its stack tables - "
                    "the original reads its own frame (docs/effect_6c.md section 7)",
                    I(step), I(bank), I(pick));
    {
        unsigned char* const record = CellRecord(0xD, z, 1);
        SetUL(record, (ULEffect6c(record) & 0xFFFFFF00u) | kFirst[pick]);
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(0xD, I(z)));
        if (item != 0) {
            const U link = Word(Item("EffectKind18Sub5A_SetTiles", item) + at::kItemLink) & 0xFFF;
            SH_CALL(Prim_SetTexture)(ULEffect6c(record), Item("EffectKind18Sub5A_SetTiles", link), 2);
        }
    }
    const U mark = kSecond[pick];
    for (U e = 0; I(e) < 8; ++e) {
        unsigned char* const record = CellRecord(e + 0xE, z, 0);
        SetUL(record, (ULEffect6c(record) & 0xFFFFFF00u) | (e + 3) | mark);
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(I(e + 0xE), I(z)));
        if (item != 0) SH_CALL(Prim_SetTexture)(ULEffect6c(record), Item("EffectKind18Sub5A_SetTiles", item), 2);
    }
}

// original 0x513C70 (called by sub-kind 0x5A's three moving states): a
// texture-page move of the rect (0x240, x + 0x100, 0x58, 0x30) to (0x240, y +
// 0x100), committed at slot 2 (0x18). The rect is a local the original builds
// on its stack.
extern "C" void __cdecl EffectKind18Sub5A_Move(int x_word, int y_word) {
    unsigned char rect[8];
    SetWord(rect, 0x240);
    SetWord(rect + 2, static_cast<U>(x_word) + 0x100u);
    SetWord(rect + 4, 0x58);
    SetWord(rect + 6, 0x30);
    SH_CALL(Gpu_SetDrawMove)(Gfx_PacketNext, rect, 0x240, static_cast<U>(y_word) + 0x100u);
    SH_CALL(Gfx_CommitPrim)(2, 0x18);
}

// original 0x513CD0 (EffectKind18_States[0x5B], hidden in 0x513C70; the
// catalog's Table EffectKind18_States row, in no group's list): jmp
// [EffectKind18Sub5B_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5B_Run(void) {
    Dispatch("EffectKind18Sub5B_Run", E6C_TABLE(EffectKind18Sub5B_States));
}

// original 0x513CF0 (sub-state 0): story flag 0x81 set: +2 = 6 (opened
// already); else Cond_ByteFE 3 and +2 up.
extern "C" void __cdecl EffectKind18Sub5B_Start(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x81) != 0) {
        S()[2] = 6;
        return;
    }
    unsigned char* const s = S();
    Cond_ByteFE = 3;
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x513D20 (sub-state 1): once Cond_ByteFE is 2: the offsets 0, the
// three parts drawn, sound 0x205, +2 up.
extern "C" void __cdecl EffectKind18Sub5B_Wait(void) {
    if (Cond_ByteFE != 2) return;
    SetOffset(0, 0);
    Part(0);
    Part(1);
    Part(2);
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundOpen5B));
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x513D70 (sub-state 2): the slide, +9 past 8 ends it.
extern "C" void __cdecl EffectKind18Sub5B_SlideOut(void) { Slide5B(8); }

// original 0x513E00 (sub-state 3): parts 0 and 1 at -64 / 64, 2 at 0, the lift
// 0; +9 up; past 0x1E sound 0x206, +9 8, +2 up.
extern "C" void __cdecl EffectKind18Sub5B_Hold(void) {
    SetOffset(0xC2800000u, 0);   // -64.0f
    Part(0);
    SetUL(::bof3::Img(at::kOffsetX.v), 0x42800000u);   // 64.0f
    Part(1);
    SetUL(::bof3::Img(at::kOffsetX.v), 0);
    Part(2);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= 0x1E) return;
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundStep5B));
    S()[9] = 8;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x513E80 (sub-state 4): the slide, +9 past 0x30 ends it.
extern "C" void __cdecl EffectKind18Sub5B_SlideWide(void) { Slide5B(0x30); }

// original 0x513F10 (sub-state 5): parts 0 and 1 at -384 / 384, part 2 at 0
// lifted +9 << 3; +9 up; past 0x30 story flag 0x81 set, MoveCmd_TestFB(0x30,
// 0x71), the record released.
extern "C" void __cdecl EffectKind18Sub5B_Lift(void) {
    SetOffset(0xC3C00000u, 0);   // -384.0f
    Part(0);
    SetUL(::bof3::Img(at::kOffsetX.v), 0x43C00000u);   // 384.0f
    Part(1);
    SetUL(::bof3::Img(at::kOffsetX.v), 0);
    SetD(::bof3::Img(at::kOffsetY.v), Fi(static_cast<U>(S()[9]) << 3));
    Part(2);
    S()[9] = static_cast<unsigned char>(S()[9] + 1);
    if (S()[9] <= 0x30) return;
    SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x81);
    SH_CALL(MoveCmd_TestFB)(0x30, 0x71);
    SH_CALL(Effect_Release)();
}

// original 0x513FB0 (sub-state 6): once Cond_ByteFE is 2: story flag 0x81
// cleared, sound 0x206, the parts at -384 / 384 / 0 lifted 384, +9 0x60, +2 up.
extern "C" void __cdecl EffectKind18Sub5B_Reopen(void) {
    if (Cond_ByteFE != 2) return;
    SH_CALL(Flags_Clear)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x81);
    SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundStep5B));
    SetOffset(0xC3C00000u, 0);   // -384.0f
    Part(0);
    SetUL(::bof3::Img(at::kOffsetX.v), 0x43C00000u);   // 384.0f
    Part(1);
    SetOffset(0, 0x43C00000u);
    Part(2);
    S()[9] = 0x60;
    S()[2] = static_cast<unsigned char>(S()[2] + 1);
}

// original 0x514030 (sub-state 7): part 0 at -(+9 << 2), part 1 at +9 << 2,
// part 2 at 0 lifted 384; +9 down to 0.
extern "C" void __cdecl EffectKind18Sub5B_Close(void) {
    SetUL(::bof3::Img(at::kOffsetY.v), 0);
    OffsetX(true, 2);
    Part(0);
    OffsetX(false, 2);
    Part(1);
    SetOffset(0, 0x43C00000u);
    Part(2);
    unsigned char* const s = S();
    if (s[9] != 0) s[9] = static_cast<unsigned char>(s[9] - 1);
}

// original 0x5140C0 (called three times by each of sub-kind 0x5B's drawing
// states): the pieces kParts5B[part] (first, end) of kPieces5B: each a POLY_FT4
// at its cell (bytes 0, 1: ((c << 7) - 0x4040) into MapView_ScreenXY as
// floats), its four corners (bytes 2..13: x << 6 + the offset 0x903828 + the
// screen x, z << 6 + the screen y, (6 - y) << 6 + the lift 0x90382C, each
// truncated), projected, textured by its dword +0x10 (1), linked at ((x << 16)
// + 0x903828 * kPieceScale, z << 16) (dy 0, 0x48).
extern "C" void __cdecl EffectKind18Sub5B_DrawPart(int part_word) {
    const U part = static_cast<U>(part_word);
    if (part >= at::kParts5BCount)
        bof3::Fatal("EffectKind18Sub5B_DrawPart: part %u, past the %u ranges of 0x%X - the original reads on "
                    "(docs/effect_6c.md section 7)",
                    (unsigned)part, at::kParts5BCount, at::kParts5B.v);
    for (U j = *At(::bof3::Img(at::kParts5B.v) + 2 * part); I(j) < I(*At(::bof3::Img(at::kParts5B.v) + 2 * part + 1)); ++j) {
        const unsigned char* const r = At(::bof3::Img(at::kPieces5B.v) + at::kPieceStride * j);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        SetD(AddressOf(MapView_ScreenXY), Fi((static_cast<U>(r[0]) << 7) - 0x4040u));
        SetD(::bof3::Img(at::kScreenY.v), Fi((static_cast<U>(r[1]) << 7) - 0x4040u));
        for (unsigned v = 0; v < 4; ++v) {
            const unsigned char* const c = r + 2 + 3 * v;
            SetV(8 * v, Ftol((Fi(static_cast<U>(c[0]) << 6) + D(::bof3::Img(at::kOffsetX.v))) + D(AddressOf(MapView_ScreenXY))));
            SetV(8 * v + 2, Ftol(Fi(static_cast<U>(c[1]) << 6) + D(::bof3::Img(at::kScreenY.v))));
            SetV(8 * v + 4, Ftol(Fi((6u - c[2]) << 6) + D(::bof3::Img(at::kOffsetY.v))));
        }
        long depth;
        SH_CALL(Gte_RotTransPers4)(Vx(0), Vx(1), Vx(2), Vx(3), Fp(p + 8), Fp(p + 0x18), Fp(p + 0x28), Fp(p + 0x38), &depth);
        SH_CALL(Gte_PrimDepths4_10)(p);
        SH_CALL(Prim_SetTexture)(ULEffect6c(r + 0x10), p, 1);
        const U x = Ftol(Fi(static_cast<U>(r[0]) << 16) + D(::bof3::Img(at::kOffsetX.v)) * D(at::kPieceScale));
        SH_CALL(MapView_LinkPrimAt)(x, static_cast<U>(r[1]) << 16, 0, 0x48);
    }
}

// original 0x514270 (hidden in E6C's 0x5140C0): jmp [EffectKind18Sub5C_States +
// +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5C_Run(void) {
    DispatchEffect6d("EffectKind18Sub5C_Run", AddressOf(EffectKind18Sub5C_States), EffectKind18Sub5C_States_count);
}

// original 0x514290 (sub-state 0): the variant v the spawn's x cell names: +0xB
// = v, the cell (0x65F388) into +0x36 / +0x3A, +8 the variant's story flag
// (0x65F38C), set (Flags_Set(Cond_Flags + 0xA0, +8)); +0x30 0, +2 up. The pair.
extern "C" void __cdecl EffectKind18Sub5C_Place(void) {
    unsigned char* const s = S();
    const U v = Variant("EffectKind18Sub5C_Place", s, at::kSub5CVariants);
    s[0xB] = static_cast<unsigned char>(v);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, At(::bof3::Img(at::kSub5CCells.v) + 2 * v)[0]);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, At(::bof3::Img(at::kSub5CCells.v) + 2 * v + 1)[0]);
    s[8] = At(::bof3::Img(at::kSub5CFlags.v) + v)[0];
    SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), s[8]);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    StepUp();
    Sub5CPair(1);
}

// original 0x514310 (sub-state 1): Cond_ByteFE 1 is taken (0) and opens; or the
// leader within half a cell of the cell's centre along z and within two cells
// of the cell past it along x. Opening: the flag +8 cleared
// (Flags_Clear(Cond_Flags + 0xA0, +8)), sound 0x200 unless a message is up,
// +2 up (read after the calls), the pair. Otherwise nothing, not even drawn.
extern "C" void __cdecl EffectKind18Sub5C_WaitNear(void) {
    bool forced = false;
    if (Cond_ByteFE == 1) {
        forced = true;
        Cond_ByteFE = 0;
    }
    unsigned char* const s = S();
    const bool near = Abs(FromCentre(LeaderZ(), S16(s + 0x3A))) <= 0x8000 &&
                      Abs(FromEdge(LeaderX(), S16(s + 0x36)) - 0x10000u) <= 0x20000;
    if (!near && !forced) return;
    SH_CALL(Flags_Clear)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), s[8]);
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundOpen));
    StepUp();
    Sub5CPair(1);
}

// original 0x5143C0 (sub-state 2): the slide up 0x10; at 0xC0 +2 up. The pair,
// side 0 at -1 (-2 from 0x80).
extern "C" void __cdecl EffectKind18Sub5C_Open(void) {
    if (Slide(0x10) >= 0xC0) StepUp();
    Sub5CPair(Sub5CDy());
}

// original 0x514410 (sub-state 3): Cond_ByteFE 1 is taken (0) and +2 goes up;
// then, the leader more than four cells off the cell's edge along z or three
// off the cell past it along x, +2 up again. Nothing drawn.
extern "C" void __cdecl EffectKind18Sub5C_WaitFar(void) {
    if (Cond_ByteFE == 1) {
        Cond_ByteFE = 0;
        StepUp();
    }
    unsigned char* const s = S();
    if (Abs(FromEdge(LeaderZ(), S16(s + 0x3A))) > 0x40000 ||
        Abs(FromEdge(LeaderX(), S16(s + 0x36)) - 0x10000u) > 0x30000)
        s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x514470 (sub-state 4): the slide down 0x10; at 0 or below the flag
// +8 set again, sound 0x201 unless a message is up, +2 = 1. The pair, side 0
// at -1 (-2 from 0x80).
extern "C" void __cdecl EffectKind18Sub5C_Close(void) {
    if (Slide(0xFFF0u) <= 0) {
        SH_CALL(Flags_Set)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), S()[8]);
        Shut();
    }
    Sub5CPair(Sub5CDy());
}

// original 0x5144F0 (cdecl (side, dy); sub-kind 0x5C's draw, twice a state):
// x (+0x36 << 7) - the slide +0x30 times the side's sign (0x65F390) + side *
// 0xC0 - 0x4040 and y (+0x3A << 7) - 0x4040 as floats into MapView_ScreenXY;
// three textured quads, each: a draw mode (page 0x95) linked at the record's
// point with dy (0xC), its four corners the vertex table's (x, y) plus the two
// floats through _ftol and its z, projected, the texture the side's and the
// variant's (+0xB) of 0x65F3DC, linked with dy (0x48).
extern "C" void __cdecl EffectKind18Sub5C_DrawPanel(unsigned side, int dy) {
    {
        unsigned char* const s = S();
        if (side >= at::kSub5CSidesUsed)
            bof3::Fatal("EffectKind18Sub5C_DrawPanel: side %u, past the two its tables hold - the original reads on into "
                        "the next variant's textures (docs/effect_6d.md section 7)",
                        side);
        const U sign = S8U(At(::bof3::Img(at::kSub5CSides.v))[side]);
        const U x = (static_cast<U>(S16(s + 0x36)) << 7) - sign * static_cast<U>(S16(s + 0x30)) + side * 0xC0u - 0x4040u;
        SetUL(At(AddressOf(MapView_ScreenXY)), FloatBits(I(x)));
        const U y = (static_cast<U>(S16(s + 0x3A)) << 7) - 0x4040u;
        SetUL(At(AddressOf(MapView_ScreenXY) + 4), FloatBits(I(y)));
    }
    for (U i = 0; i < at::kSub5CQuads; ++i) {
        PanelMode();
        Link(dy, 0xC);
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        for (U v = 0; v < 4; ++v) {
            const unsigned char* const t = At(::bof3::Img(at::kSub5CVertices.v) + 6 * (4 * i + v));
            unsigned char* const out = Vertices() + 8 * v;
            SetWord(out, FtolEffect6d(static_cast<long double>(S16(t)) + Fl(AddressOf(MapView_ScreenXY))));
            rv::Store<std::uint32_t>(rv::As<records::SVector>(out)->y, FtolEffect6d(static_cast<long double>(S16(t + 2)) + Fl(AddressOf(MapView_ScreenXY) + 4)));
            rv::Store<std::uint32_t>(rv::As<records::SVector>(out)->z, Word(t + 4));
        }
        Project(p);
        const U variant = S()[0xB];
        if (variant >= at::kSub5CVariants)
            bof3::Fatal("EffectKind18Sub5C_DrawPanel: +0xB is %u, past the two variants of 0x%X - the original reads on "
                        "into EffectKind18Sub5E_States (docs/effect_6d.md section 7)",
                        (unsigned)variant, at::kSub5CTextures.v);
        SH_CALL(Prim_SetTexture)(UL(::bof3::Img(at::kSub5CTextures.v) + 4 * (3 * side + 6 * variant + i)), p, 1);
        Link(dy, 0x48);
    }
}

// original 0x514690 (hidden in 0x5144F0): jmp [EffectKind18Sub5E_States + +2 *
// 4], unbounded.
extern "C" void __cdecl EffectKind18Sub5E_Run(void) {
    DispatchEffect6d("EffectKind18Sub5E_Run", AddressOf(EffectKind18Sub5E_States), EffectKind18Sub5E_States_count);
}

// original 0x5146B0 (sub-state 0): +0x3E 0xFF80, +0x30 0, +2 up; the leader
// already at the cell (within half a cell of its centre along z, on it or the
// next along x): +0x30 0xC0, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub5E_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, 0xFF80u);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    StepUp();
    unsigned char* const s = S();
    if (LeaderAt(s, true, 0x8000, 0x10000)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0xC0);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x514740 (sub-state 1): the leader at the cell: sound 0x200 unless a
// message is up, +2 up (read after the sound). The draw.
extern "C" void __cdecl EffectKind18Sub5E_WaitNear(void) {
    unsigned char* s = S();
    if (LeaderAt(s, true, 0x8000, 0x10000)) {
        if (Field_Request == 0) {
            SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(at::kSoundOpen));
            s = S();
        }
        s[2] = static_cast<unsigned char>(s[2] + 1);
    }
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x5147C0 (sub-state 2): the slide up 0x10; at 0xC0 +2 up. A tail jump
// to the draw.
extern "C" void __cdecl EffectKind18Sub5E_Open(void) {
    if (Slide(0x10) >= 0xC0) StepUp();
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x5147E0 (sub-state 3): the leader two cells off (the same test at
// 0x20000 both ways fails): +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub5E_WaitFar(void) {
    unsigned char* const s = S();
    if (!LeaderAt(s, true, 0x20000, 0x20000)) s[2] = static_cast<unsigned char>(s[2] + 1);
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x514840 (sub-state 4): the slide down 0x10; at 0 or below sound
// 0x201 unless a message is up and +2 = 1. A tail jump to the draw.
extern "C" void __cdecl EffectKind18Sub5E_Close(void) {
    if (Slide(0xFFF0u) <= 0) Shut();
    SH_CALL(EffectKind18Sub5E_Draw)();
}

// original 0x514880 (sub-kind 0x5E's draw: called by its states 0, 1, 3, tail-
// jumped to by 2 and 4): the corners' y (+0x3A << 7) - 0x3FC0; a draw mode
// (page 0x95) committed at slot 6 (0xC); two textured quads, i 0 and 1: x
// (+0x36 << 7) - 0x3F40 / - 0x4040 plus the slide +0x30 times the side's sign
// (0x65F420); each corner's height from the ground under it (the top two
// -ground / 2, the bottom two 0x180 below), projected, the texture (0x11D - i)
// | 0x1790000, committed at slot 6 (0x48).
extern "C" void __cdecl EffectKind18Sub5E_Draw(void) {
    unsigned char* const v = Vertices();
    unsigned char* const mode = Gfx_PacketNext;
    const U y = (static_cast<U>(rv::As<records::EffectObject>(S())->z_whole) << 7) - 0x3FC0u;
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].y, y);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].y, y);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].y, y);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, y);
    SH_CALL(Gpu_SetDrawMode)(mode, 0, 0, 0x95, 0);
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    for (U i = 0; i < 2; ++i) {
        unsigned char* const p = Gfx_PacketNext;
        SH_CALL(Gpu_SetPolyFT4)(p);
        SH_CALL(Gpu_SetShadeTex)(p, 0);
        unsigned char* const s = S();
        const U sign = S8U(At(::bof3::Img(at::kSub5ESides.v))[i]);
        const long y0 = Ground(v + 2);
        const U slid = rv::As<records::EffectWork>(s)->work_30 * sign + (static_cast<U>(rv::As<records::EffectObject>(s)->x_whole) << 7);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].x, slid - 0x3F40u);
        SetWord(v, slid - 0x3F40u);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].x, slid - 0x4040u);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].x, slid - 0x4040u);
        const long x0 = Ground(v);
        long h = SH_CALL(AreaMap_Elevation)(x0, y0);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, 0u - Half(h));
        h = SH_CALL(AreaMap_Elevation)(x0, y0);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[2].z, 0x180u - Half(h));
        const long y1 = Ground(v + 0xA), x1 = Ground(v + 8);
        h = SH_CALL(AreaMap_Elevation)(x1, y1);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[1].z, 0u - Half(h));
        h = SH_CALL(AreaMap_Elevation)(x1, y1);
        rv::Store<std::uint32_t>(rv::As<records::SVector>(v)[3].z, 0x180u - Half(h));
        Project(p);
        SH_CALL(Prim_SetTexture)((0x11Du - i) | 0x1790000u, p, 1);
        SH_CALL(Gfx_CommitPrim)(6, 0x48);
    }
}

void EffectKind18Sub5b_Inject() {
    BOF3_INJECT(EffectKind18Sub5A_Run);
    BOF3_INJECT(EffectKind18Sub5A_Start);
    BOF3_INJECT(EffectKind18Sub5A_Wait);
    BOF3_INJECT(EffectKind18Sub5A_Animate);
    BOF3_INJECT(EffectKind18Sub5A_Set);
    BOF3_INJECT(EffectKind18Sub5A_Cycle);
    BOF3_INJECT(EffectKind18Sub5A_CycleOpen);
    BOF3_INJECT(EffectKind18Sub5A_SetTiles);
    BOF3_INJECT(EffectKind18Sub5A_Move);
    BOF3_INJECT(EffectKind18Sub5B_Run);
    BOF3_INJECT(EffectKind18Sub5B_Start);
    BOF3_INJECT(EffectKind18Sub5B_Wait);
    BOF3_INJECT(EffectKind18Sub5B_SlideOut);
    BOF3_INJECT(EffectKind18Sub5B_Hold);
    BOF3_INJECT(EffectKind18Sub5B_SlideWide);
    BOF3_INJECT(EffectKind18Sub5B_Lift);
    BOF3_INJECT(EffectKind18Sub5B_Reopen);
    BOF3_INJECT(EffectKind18Sub5B_Close);
    BOF3_INJECT(EffectKind18Sub5B_DrawPart);
    BOF3_INJECT(EffectKind18Sub5C_Run);
    BOF3_INJECT(EffectKind18Sub5C_Place);
    BOF3_INJECT(EffectKind18Sub5C_WaitNear);
    BOF3_INJECT(EffectKind18Sub5C_Open);
    BOF3_INJECT(EffectKind18Sub5C_WaitFar);
    BOF3_INJECT(EffectKind18Sub5C_Close);
    BOF3_INJECT(EffectKind18Sub5C_DrawPanel);
    BOF3_INJECT(EffectKind18Sub5E_Run);
    BOF3_INJECT(EffectKind18Sub5E_Start);
    BOF3_INJECT(EffectKind18Sub5E_WaitNear);
    BOF3_INJECT(EffectKind18Sub5E_Open);
    BOF3_INJECT(EffectKind18Sub5E_WaitFar);
    BOF3_INJECT(EffectKind18Sub5E_Close);
    BOF3_INJECT(EffectKind18Sub5E_Draw);
}
