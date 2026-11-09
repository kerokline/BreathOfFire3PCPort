// Moved here by tools/move_funcs.py from effect_5f.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_18_sub_42.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/draw_pool.h"
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

std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }

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

// The panel's four corners on the axis +8 does not pick: c = (w << 7) - 0x3FC0
// of the record's other cell word, into the four vertices' word `off`.
void Across(unsigned off, U c) {
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x18) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x10) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr) + 0x8) + off, c);
    SetWord(Vertex(::bof3::Img(Prim_VertexScratch_addr)) + off, c);
}

void Draw42() {
    unsigned char* const s = S();
    SH_CALL(EffectKind18Sub42_Draw)(s[8], S16(Word(s + 0x3E)));
}

}  // namespace

// original 0x509290 (hidden in 0x509030): jmp [EffectKind18Sub28_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub28_Run(void) {
    Dispatch("EffectKind18Sub28_Run", AddressOf(EffectKind18Sub28_States), EffectKind18Sub28_States_count);
}

// original 0x5092B0 (state 0): +0x3E -0x5C0, +0x30 0, +2 up; story flag 0x54
// clear: +2 = 5 (wait for it); set and the leader within the gate (along x):
// +0x30 -0x100, +2 = 3. The draw.
extern "C" void __cdecl EffectKind18Sub28_Start(void) {
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(S())->height_whole, 0xFA40);
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(S())->work_30, 0);
    ++S()[2];
    const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x54);
    if (set == 0) {
        S()[2] = 5;
        SH_CALL(EffectKind18Sub28_Draw)();
        return;
    }
    unsigned char* const s = S();
    if (Near(s, false)) {
        rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, 0xFF00);
        S()[2] = 3;
    }
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x509360 (state 1): the leader within the gate (along x): the sound, +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub28_WaitNear(void) {
    StepIfNear(false);
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x5093E0 (state 2): +0x30 down 0x20; at -0x100 or below +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub28_Open(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, Word(s + 0x30) - 0x20u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) <= -0x100) ++s[2];
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x509400 (state 3): the leader two cells or more away (along x): +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub28_WaitFar(void) {
    unsigned char* const s = S();
    if (Far(s, false)) ++s[2];
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x509460 (state 4): +0x30 up 0x20; at 0 or above the sound 0x201 and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub28_Close(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectWork>(s)->work_30, rv::As<records::EffectWork>(s)->work_30 + 0x20u);
    if (S16(rv::As<records::EffectWork>(s)->work_30) >= 0) Closed();
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x5094A0 (state 5): story flag 0x54 set: the sound 0x200 unless
// Field_Request, +2 = 2. The draw.
extern "C" void __cdecl EffectKind18Sub28_WaitFlag(void) {
    if (SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x54) != 0) {
        if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x200);
        S()[2] = 2;
    }
    SH_CALL(EffectKind18Sub28_Draw)();
}

// original 0x5094E0 (states 0..5 call or jump to it): a draw mode committed
// (Gfx_CommitPrim(6, 0xC)), one shaded POLY_FT4 across x at the cell, along z
// from the cell plus +0x30; corners 0x40 / 0x180 above half the ground's
// height; texture 0x1780119; committed with 0x48 bytes.
extern "C" void __cdecl EffectKind18Sub28_Draw(void) {
    DrawMode();
    SH_CALL(Gfx_CommitPrim)(6, 0xC);
    unsigned char* const p = Quad();
    unsigned char* const s = S();
    Across(0, (rv::As<records::EffectObject>(s)->x_whole << 7) - 0x3FC0u);
    const U a = (rv::As<records::EffectObject>(s)->z_whole << 7) + rv::As<records::EffectWork>(s)->work_30 - 0x3F40u;
    Prim_VertexScratch[2].y = a;
    Prim_VertexScratch[0].y = a;
    const U b = (rv::As<records::EffectObject>(s)->z_whole << 7) + rv::As<records::EffectWork>(s)->work_30 - 0x4040u;
    Prim_VertexScratch[3].y = b;
    Prim_VertexScratch[1].y = b;
    GroundFixed(0x40, 0x180);
    Project(p);
    SH_CALL(Prim_SetTexture)(0x1780119, p, 1);
    SH_CALL(Gfx_CommitPrim)(6, 0x48);
}

// original 0x509690 (hidden in 0x5094E0): jmp [EffectKind18Sub42_States + +2 * 4], unbounded.
extern "C" void __cdecl EffectKind18Sub42_Run(void) {
    Dispatch("EffectKind18Sub42_Run", AddressOf(EffectKind18Sub42_States), EffectKind18Sub42_States_count);
}

// original 0x5096B0 (state 0): the placement +0x36 (three) gives the cell, +8
// (the variant) and +0xB (the story flag); +8 clear: +0x3E 0, +2 up (wait for
// the flag); set: +0x3E 0x80, +2 = 4 (raised). The draw.
extern "C" void __cdecl EffectKind18Sub42_Start(void) {
    unsigned char* const s = S();
    const U i = Place("EffectKind18Sub42_Start", at::kSub42Places.v, at::kSub42PlaceCount);
    const U place = ::bof3::Img(at::kSub42Places.v) + 4 * i;
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->x_whole, B(place));
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->z_whole, B(place + 1));
    s[8] = At(place + 2)[0];
    s[0xB] = At(place + 3)[0];
    if (s[8] == 0) {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, 0);
        ++s[2];
    } else {
        rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, 0x80);
        s[2] = 4;
    }
    Draw42();
}

// original 0x509740 (state 1): the story flag +0xB differing from +8 (al ^ +8)
// and no party member within reach of the cell's middle: MoveCmd_TestFB at the
// cell + 1 (answer unread), +2 up, the sound 0x205 unless Field_Request, the
// draw. Otherwise nothing, not even the draw.
extern "C" void __cdecl EffectKind18Sub42_WaitFlag(void) {
    unsigned char* s = S();
    const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), s[0xB]);
    s = S();
    if (static_cast<unsigned char>(set ^ s[8]) == 0) return;
    const unsigned char who = SH_CALL(EffectKind18Sub42_MemberNear)(static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->x) | 0x8000u),
                                                                    static_cast<long>(static_cast<std::uint32_t>(rv::As<records::EffectObject>(s)->z) | 0x8000u));
    if (static_cast<signed char>(who) >= 0) return;
    s = S();
    SH_CALL(MoveCmd_TestFB)(static_cast<short>(rv::As<records::EffectObject>(s)->x_whole + 1), static_cast<short>(rv::As<records::EffectObject>(s)->z_whole + 1));
    ++S()[2];
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x205);
    Draw42();
}

// original 0x5097E0 (state 2): +0x3E up 0x10; at 0x80 or above
// MoveCmd_TestFB at the cell and MoveCmd_TestFC at the cell + 1 (answers
// unread), +2 up. The draw.
extern "C" void __cdecl EffectKind18Sub42_Raise(void) {
    unsigned char* s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, rv::As<records::EffectObject>(s)->height_whole + 0x10u);
    if (S16(rv::As<records::EffectObject>(s)->height_whole) >= 0x80) {
        SH_CALL(MoveCmd_TestFB)(static_cast<short>(rv::As<records::EffectObject>(s)->x_whole), static_cast<short>(rv::As<records::EffectObject>(s)->z_whole));
        s = S();
        SH_CALL(MoveCmd_TestFC)(static_cast<short>(rv::As<records::EffectObject>(s)->x_whole + 1), static_cast<short>(rv::As<records::EffectObject>(s)->z_whole + 1));
        ++S()[2];
    }
    Draw42();
}

// original 0x509850 (state 3; PSX twin 0x801F35B0): for the cell and its
// neighbour along the axis +8 picks (two), the map's cell word at
// AreaMap_Header + 2 * (x + width * z + 2 * the header's cell base) indexes a
// texture dword past the cells; the cell's draw item (MapView_ItemAt) keeps
// in its +0x7E (+8 set) or +0x8E word an item of its own, allocated
// (DrawItemPool_Alloc) and textured (Prim_SetTexture(that dword, the item, 2))
// when the word is 0. Then MapView_Redraw = 2, +2 up, the draw. The item array
// is draw_pool's (DIV-0062: the original's immediates are re-aimed at inject).
extern "C" void __cdecl EffectKind18Sub42_Mark(void) {
    unsigned char* s = S();
    for (U i = 0; i < 2; ++i) {
        const U b = s[8];
        const U flat = b == 0 ? 1u : 0u;
        const U x = static_cast<U>(S16(Word(s + 0x36))) + b + flat * i;
        const U z = static_cast<U>(S16(Word(s + 0x3A))) + flat + b * i;
        const U width = AreaMap_Header[0];
        const U cell = x + width * z + W(::bof3::Img(at::kMapCellBase.v)) * 2;
        const U word = Word(AreaMap_Header + cell * 2);
        const U half = static_cast<U>(static_cast<std::int32_t>(static_cast<std::uint32_t>(AreaMap_Header[1]) * width + 1) / 2);
        const U texture = ::bof3::Img(at::kMapTextures.v) + 4 * (half + W(::bof3::Img(at::kMapCellBase.v)) + word);
        const U item = static_cast<U>(SH_CALL(MapView_ItemAt)(static_cast<long>(x), static_cast<long>(z)));
        s = S();
        if (item >= draw_pool::Count())
            bof3::Fatal("EffectKind18Sub42_Mark: MapView_ItemAt answered %u, past the %u draw items - the original "
                        "reads and writes what follows them (docs/effect_5f.md section 6)",
                        (unsigned)item, draw_pool::Count());
        unsigned char* const keep = draw_pool::Items() + item * at::kDrawItemStride + (s[8] != 0 ? at::kItemHalfB : at::kItemHalfA);
        if (Word(keep) != 0) continue;
        const U got = SH_CALL(DrawItemPool_Alloc)();
        SetWord(keep, got);
        const U mine = got & 0xFFFFu;
        if (mine >= draw_pool::Count())
            bof3::Fatal("EffectKind18Sub42_Mark: DrawItemPool_Alloc answered %u, past the %u draw items "
                        "(docs/effect_5f.md section 6)",
                        (unsigned)mine, draw_pool::Count());
        SH_CALL(Prim_SetTexture)(UL(texture), draw_pool::Items() + mine * at::kDrawItemStride, 2);
        s = S();
    }
    MapView_Redraw = 2;
    ++s[2];
    Draw42();
}

// original 0x509980 (state 4): the story flag +0xB clear-ness ((al == 0)
// against +8, whole) differing: MoveCmd_TestFB at the cell and at the cell + 1
// (answers unread), +2 up, the sound 0x205 unless Field_Request, the draw.
// Otherwise nothing.
extern "C" void __cdecl EffectKind18Sub42_WaitFlagBack(void) {
    unsigned char* s = S();
    const unsigned char set = SH_CALL(Flags_Test)(At(::bof3::Img(Cond_Flags_addr) + 0xa0), s[0xB]);
    s = S();
    const U clear = set == 0 ? 1u : 0u;
    if (clear == s[8]) return;
    SH_CALL(MoveCmd_TestFB)(static_cast<short>(rv::As<records::EffectObject>(s)->x_whole), static_cast<short>(rv::As<records::EffectObject>(s)->z_whole));
    s = S();
    SH_CALL(MoveCmd_TestFB)(static_cast<short>(rv::As<records::EffectObject>(s)->x_whole + 1), static_cast<short>(rv::As<records::EffectObject>(s)->z_whole + 1));
    ++S()[2];
    if (Field_Request == 0) SH_CALL(Sound_PlayEffect)(0x205);
    Draw42();
}

// original 0x509A20 (state 5): +0x3E down 0x10; at 0 or below MoveCmd_TestFC
// at the cell + 1 (answer unread) and +2 = 1. The draw.
extern "C" void __cdecl EffectKind18Sub42_Lower(void) {
    unsigned char* const s = S();
    rv::Store<std::uint32_t>(rv::As<records::EffectObject>(s)->height_whole, rv::As<records::EffectObject>(s)->height_whole - 0x10u);
    if (S16(rv::As<records::EffectObject>(s)->height_whole) <= 0) {
        SH_CALL(MoveCmd_TestFC)(static_cast<short>(rv::As<records::EffectObject>(s)->x_whole + 1), static_cast<short>(rv::As<records::EffectObject>(s)->z_whole + 1));
        S()[2] = 1;
    }
    Draw42();
}

// original 0x509A70 (a catalog row of no group; the six states call it): the
// six pieces of `variant` (the record's +8, two), each a draw mode linked at
// its cell (the record's cell plus the piece's unsigned dx, dz; dy 1) and one
// shaded POLY_FT4 of the piece's shape (three: four vertices of three s8 -
// x and z in cells from the piece's cell, the height << 7 less `height` plus
// the variant's lift), texture ((the shape's page byte + 0x1200) << 16) + the
// piece's byte, linked at the cell with 0x48 bytes. Sprite_Current read again
// for each piece and each vertex.
extern "C" void __cdecl EffectKind18Sub42_Draw(unsigned variant, int height) {
    if (variant >= at::kSub42Variants)
        bof3::Fatal("EffectKind18Sub42_Draw: variant %u, past the two piece lists of 0x%X - the original reads the "
                    "tables after them (docs/effect_5f.md section 6)",
                    variant, at::kSub42Pieces.v);
    U piece = ::bof3::Img(at::kSub42Pieces.v) + 0x18 * variant;
    for (unsigned n = 0; n < at::kSub42PieceCount; ++n, piece += 4) {
        unsigned char* s = S();
        const U dx = B(piece), dz = B(piece + 1);
        const U zz = (static_cast<U>(S16(Word(s + 0x3A))) + dz) << 16;
        const U xx = (static_cast<U>(S16(Word(s + 0x36))) + dx) << 16;
        DrawMode();
        SH_CALL(MapView_LinkPrimAt)(xx, zz, 1, 0xC);
        unsigned char* const p = Quad();
        const U lift = W(::bof3::Img(at::kSub42Lift.v) + 2 * variant);
        const U shape = B(piece + 2);
        if (shape >= at::kSub42ShapeCount)
            bof3::Fatal("EffectKind18Sub42_Draw: shape %u, past the three of 0x%X (docs/effect_5f.md section 6)",
                        (unsigned)shape, at::kSub42Shapes.v);
        U point = ::bof3::Img(at::kSub42Shapes.v) + 12 * shape;
        for (unsigned j = 0; j < 4; ++j, point += 3) {
            s = S();
            unsigned char* const v = Vertex(::bof3::Img(Prim_VertexScratch_addr) + 8 * j);
            SetWord(v, ((static_cast<U>(S8(B(point))) + dx + Word(s + 0x36)) << 7) - 0x4040u);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->y, ((static_cast<U>(S8(B(point + 1))) + Word(s + 0x3A) + dz) << 7) - 0x4040u);
            rv::Store<std::uint32_t>(rv::As<records::SVector>(v)->z, (static_cast<U>(S8(B(point + 2))) << 7) - static_cast<U>(height) + lift);
        }
        Project(p);
        SH_CALL(Prim_SetTexture)(((B(::bof3::Img(at::kSub42ShapeTexture.v) + shape) + 0x1200u) << 16) + B(piece + 3), p, 1);
        SH_CALL(MapView_LinkPrimAt)(xx, zz, 1, 0x48);
    }
}

// original 0x509C00: the party member within reach of the point (x, z), 16.16:
// for each of the Field_MemberCount ObjTrio records in use, its point moved by
// its velocity (+0xC, +0x10) times +9, within ((+0x70 byte) + 3) << 15 on both
// axes. Answers the member in al, 0xFF for none.
extern "C" unsigned char __cdecl EffectKind18Sub42_MemberNear(long x, long z) {
    const U count = Field_MemberCount;
    for (U i = 0; i < count; ++i) {
        if (i >= at::kObjTrioCount)
            bof3::Fatal("EffectKind18Sub42_MemberNear: Field_MemberCount is %u, past the three ObjTrio records - the "
                        "original reads what follows them (docs/effect_5f.md section 6)",
                        (unsigned)count);
        const unsigned char* const o = rv::Bytes(&ObjTrio[i]);
        auto* const oo = rv::AsConst<records::PartyMember>(o);
        if (o[0] == 0) continue;
        const U t = o[9];
        const std::int32_t reach = static_cast<std::int32_t>((static_cast<U>(o[0x70]) + 3) << 15);
        const U px = static_cast<std::uint32_t>(oo->step_x) * t + static_cast<std::uint32_t>(oo->x);
        if (Abs(px - static_cast<U>(x)) >= reach) continue;
        const U pz = static_cast<std::uint32_t>(oo->step_z) * t + static_cast<std::uint32_t>(oo->z);
        if (Abs(pz - static_cast<U>(z)) < reach) return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

void EffectKind18Sub42_Inject() {
    BOF3_INJECT(EffectKind18Sub28_Run);
    BOF3_INJECT(EffectKind18Sub28_Start);
    BOF3_INJECT(EffectKind18Sub28_WaitNear);
    BOF3_INJECT(EffectKind18Sub28_Open);
    BOF3_INJECT(EffectKind18Sub28_WaitFar);
    BOF3_INJECT(EffectKind18Sub28_Close);
    BOF3_INJECT(EffectKind18Sub28_WaitFlag);
    BOF3_INJECT(EffectKind18Sub28_Draw);
    BOF3_INJECT(EffectKind18Sub42_Run);
    BOF3_INJECT(EffectKind18Sub42_Start);
    BOF3_INJECT(EffectKind18Sub42_WaitFlag);
    BOF3_INJECT(EffectKind18Sub42_Raise);
    BOF3_INJECT(EffectKind18Sub42_Mark);
    BOF3_INJECT(EffectKind18Sub42_WaitFlagBack);
    BOF3_INJECT(EffectKind18Sub42_Lower);
    BOF3_INJECT(EffectKind18Sub42_Draw);
    BOF3_INJECT(EffectKind18Sub42_MemberNear);
}
