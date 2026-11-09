// Moved here by tools/move_funcs.py from effect_5f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_27.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
#include "game/effect_5f.h"
#include "game/effect_5f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5f::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

U UL(U a) { return UL(At(a)); }

U W(U a) { return Word(At(a)); }

U B(U a) { return At(a)[0]; }

std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }

U AddressOf(const void* p) { return Low32(p); }

// cdq; xor; sub: |v| in 32 bits (0x80000000 stays itself, below every bound)
std::int32_t Abs(U v) {
    const U m = static_cast<U>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ m) - m);
}

// movsx eax, ax; cdq; sub eax, edx; sar eax, 1: half the height, toward zero
U Half(long e) { return static_cast<U>(S16(static_cast<U>(e)) / 2); }

unsigned char* Vertex(U a) { return At(a); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next sub-kind's table or data.
void Dispatch(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5f.md section 6)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * state)))();
}

// A placement index the start states read from +0x36 (movsx: a signed word),
// checked against its table's records.
U Place(const char* who, U table, unsigned count) {
    const std::int32_t i = S16(rv::As<records::EffectObject>(S())->x_whole);
    if (i < 0 || i >= static_cast<std::int32_t>(count))
        bof3::Fatal("%s: +0x36 is %d, outside the %u records of 0x%X - the original reads what lies beside them "
                    "(docs/effect_5f.md section 6)",
                    who, (int)i, count, (unsigned)table);
    return static_cast<U>(i);
}

// The leader against the record's cell (+0x36 x, +0x3A z, s16 cells; the
// leader's point 16.16). Along the axis +8 picks (z when set), the leader's
// distance from the cell's far edge's middle ((c + 1) << 16 | 0x8000) is at
// most `edge`; across it, the leader is within `cross` of the cell's or the
// next cell's start. The gate's test is (0x8000, 0x10000); "two cells away"
// is its negation at (0x20000, 0x20000).
bool Within(const unsigned char* s, bool z_axis, std::int32_t edge, std::int32_t cross) {
    const U lx = static_cast<std::uint32_t>(ObjTrio[0].x), lz = static_cast<std::uint32_t>(ObjTrio[0].z);
    const U x = static_cast<U>(S16(Word(s + 0x36))), z = static_cast<U>(S16(Word(s + 0x3A)));
    U along, across;
    if (z_axis) {
        along = lz - (((z + 1) << 16) | 0x8000u);
        across = lx - (x << 16);
    } else {
        along = lx - (((x + 1) << 16) | 0x8000u);
        across = lz - (z << 16);
    }
    return Abs(along) <= edge && (Abs(across) <= cross || Abs(across - 0x10000u) <= cross);
}

bool Near(const unsigned char* s, bool z_axis) { return Within(s, z_axis, 0x8000, 0x10000); }

bool Far(const unsigned char* s, bool z_axis) { return !Within(s, z_axis, 0x20000, 0x20000); }

// The leader within the gate: Sound_PlayEffect(0x200) unless Field_Request,
// then +2 up (Sprite_Current read again after the sound).
void StepIfNear(bool z_axis) {
    unsigned char* s = S();
    if (!Near(s, z_axis)) return;
    if (Field_Request == 0) {
        SH_CALL(Sound_PlayEffect)(0x200);
        s = S();
    }
    ++s[2];
}

// +2 back to 1 with Sound_PlayEffect(0x201) unless Field_Request.
void Closed() {
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x201);
    S()[2] = 1;
}

// The quad's projection, its depths: the four vertices of Prim_VertexScratch
// into the packet's four screen points (the original pushes a tenth word, a
// local, which Gte_RotTransPers4 does not read).
void Project(unsigned char* p) {
    long depth;
    SH_CALL(Gte_RotTransPers4)(reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr))), reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8)),
                               reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10)), reinterpret_cast<const short*>(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18)),
                               reinterpret_cast<float*>(p + 8), reinterpret_cast<float*>(p + 0x18),
                               reinterpret_cast<float*>(p + 0x28), reinterpret_cast<float*>(p + 0x38), &depth);
    SH_CALL(Gte_PrimDepths4_10)(p);
}

long Elevation(U x, U z) { return SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z)); }

U GroundOf(U word) { return static_cast<U>(S16(word) + 0x4000) << 9; }

// The ground under v0 and v1 (each vertex's +0 x and +2 z words as 16.16 map
// points: (w + 0x4000) << 9), four AreaMap_Elevation calls, the second pair's
// point read after the second call: v0 and v1 `low` less half the ground's
// height, v2 and v3 `high` less it (sub-kinds 0x27 and 0x28: 0x40 and 0x180).
void GroundFixed(U low, U high) {
    unsigned char* const v0 = Vertex(::bof3::Img(Prim_VertexScratch_addr));
    unsigned char* const v1 = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8);
    U gx = GroundOf(Word(v0)), gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v0)->y));
    long e = Elevation(gx, gz);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v0)->z, low - Half(e));
    e = Elevation(gx, gz);
    gz = GroundOf(static_cast<std::uint16_t>(rv::As<records::SVector>(v1)->y));
    gx = GroundOf(Word(v1));
    Prim_VertexScratch[2].z = high - Half(e);
    e = Elevation(gx, gz);
    rv::Store<std::uint32_t>(rv::As<records::SVector>(v1)->z, low - Half(e));
    e = Elevation(gx, gz);
    Prim_VertexScratch[3].z = high - Half(e);
}

// Gpu_SetDrawMode(cursor, 0, 0, 0x95, 0): every draw here opens with it.
void DrawMode() { SH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0); }

// The next packet as a shaded POLY_FT4.
unsigned char* Quad() {
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetPolyFT4)(p);
    SH_CALL(Gpu_SetShadeTex)(p, 0);
    return p;
}

void LinkAtRecord(int dy, unsigned size) {
    unsigned char* const s = S();
    auto* const so = rv::As<records::EffectObject>(s);
    SH_CALL(MapView_LinkPrimAt)(static_cast<std::uint32_t>(so->x), static_cast<std::uint32_t>(so->z), dy, size);
}

// The panel's four corners on the axis +8 does not pick: c = (w << 7) - 0x3FC0
// of the record's other cell word, into the four vertices' word `off`.
void Across(unsigned off, U c) {
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr)) + off, c);
}

}  // namespace

// original 0x508CC0 (hidden in E5E's 0x508BA0): jmp [EffectKind18Sub27_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub27_Run(void) {
    Dispatch("EffectKind18Sub27_Run", AddressOf(EffectKind18Sub27_States), EffectKind18Sub27_States_count);
}

// original 0x508CE0 (state 0, hidden in 0x508BA0): +8 = (+0x3A word is 0); the
// placement +0x36 (two) gives the cell (+0x36, +0x3A), +0x3E and +0x2E; +0x30
// 0, +2 up; the leader within the gate: +0x30 -0x100, +2 = 3 (open at once).
// The draw.
extern "C" void __cdecl EffectKind18Sub27_Start(void) {
    unsigned char* const s = S();
    s[8] = rv::As<records::EffectObject>(s)->z_whole == 0 ? 1 : 0;
    const U i = Place("EffectKind18Sub27_Start", at::kSub27Cell.v, at::kSub27Places);
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, B(::bof3::Img(at::kSub27Cell.v) + 2 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, B(::bof3::Img(at::kSub27Cell.v) + 2 * i + 1));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, W(::bof3::Img(at::kSub27Height.v) + 2 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_2e, W(::bof3::Img(at::kSub27Texture.v) + 2 * i));
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0);
    ++s[2];
    if (Near(s, s[8] != 0)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0xFF00);
        s[2] = 3;
    }
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x508E20 (state 1): the leader within the gate: the sound, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub27_WaitNear(void) {
    StepIfNear(S()[8] != 0);
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x508F00 (state 2): +0x30 down 0x20; at -0x100 or below +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub27_Open(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) - 0x20u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) <= -0x100) ++s[2];
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x508F20 (state 3): the leader two cells or more away: +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub27_WaitFar(void) {
    unsigned char* const s = S();
    if (Far(s, s[8] != 0)) ++s[2];
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x508FF0 (state 4): +0x30 up 0x20; at 0 or above the sound 0x201
// and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub27_Close(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + 0x20u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) >= 0) Closed();
    SH_CALL(EffectKind18Sub27_Draw)();
}

// original 0x509030 (states 0..4 call or jump to it): one shaded POLY_FT4, a
// draw mode linked at the record's point (dy -2) first. Along +8's axis the
// panel spans the cell's 0x80 less +0x30 (its slide), across it the cell's
// width; each corner 0x40 / 0x180 above half the ground's height; texture
// +0x2E (signed) | +8 << 21 | 0x1500000; linked at the point with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub27_Draw(void) {
    DrawMode();
    LinkAtRecord(-2, 0xC);
    unsigned char* const p = Quad();
    unsigned char* s = S();
    if (s[8] != 0) {
        Across(2, (rv::As<records::EffectObject>(s)->z_whole << 7) - 0x3FC0u);
        const U a = (rv::As<records::EffectObject>(s)->x_whole << 7) - rv::As<records::EffectWork>(s)->work_30 - 0x3F40u;
        Prim_VertexScratch[2].x = a;
        Prim_VertexScratch[0].x = a;
        const U b = (rv::As<records::EffectObject>(s)->x_whole << 7) - rv::As<records::EffectWork>(s)->work_30 - 0x4040u;
        Prim_VertexScratch[3].x = b;
        Prim_VertexScratch[1].x = b;
    } else {
        Across(0, (rv::As<records::EffectObject>(s)->x_whole << 7) - 0x3FC0u);
        const U a = (rv::As<records::EffectObject>(s)->z_whole << 7) + rv::As<records::EffectWork>(s)->work_30 - 0x3F40u;
        Prim_VertexScratch[2].y = a;
        Prim_VertexScratch[0].y = a;
        const U b = (rv::As<records::EffectObject>(s)->z_whole << 7) + rv::As<records::EffectWork>(s)->work_30 - 0x4040u;
        Prim_VertexScratch[3].y = b;
        Prim_VertexScratch[1].y = b;
    }
    GroundFixed(0x40, 0x180);
    Project(p);
    s = S();
    SH_CALL(Prim_SetTexture)((static_cast<U>(s[8]) << 21) | static_cast<U>(S16(rv::As<records::EffectWork>(s)->work_2e)) | 0x1500000u, p, 1);
    LinkAtRecord(-2, 0x48);
}

void EffectKind18Sub27_Inject() {
    BOF3_INJECT(EffectKind18Sub27_Run);
    BOF3_INJECT(EffectKind18Sub27_Start);
    BOF3_INJECT(EffectKind18Sub27_WaitNear);
    BOF3_INJECT(EffectKind18Sub27_Open);
    BOF3_INJECT(EffectKind18Sub27_WaitFar);
    BOF3_INJECT(EffectKind18Sub27_Close);
    BOF3_INJECT(EffectKind18Sub27_Draw);
}
