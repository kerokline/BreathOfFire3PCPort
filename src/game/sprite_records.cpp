#include "game/sprite_records.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

// A 3D object's quads, for the draw-order pass: the object's matrix through
// the GTE, a light direction in its frame, then per quad a textured POLY_FT4
// of 0x48 bytes and a 12-byte record the pass sorts by depth
// (docs/sprite-draw-order.md section 12). Everything these call is ours.
//
// Runs on a 16 KB coroutine stack (docs/SCAFFOLDING.md section 3): three
// MATRIXes of locals at most.

namespace {

template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}
template <class T> void Put(unsigned char* p, unsigned offset, T v) { std::memcpy(p + offset, &v, sizeof v); }

// Gfx_PacketPools + 0x10000 - 0x54: the last byte a primitive may reach in
// buffer 0; buffer 1's is 0x10000 higher. As in draw_emit.cpp.
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;
constexpr unsigned kPrimBytes = 0x48, kQuadBytes = 0x28, kRecordBytes = 12;

const unsigned char* ObjectData() { return Get<const unsigned char*>(Sprite_Current, 0x54); }

}  // namespace

// original 0x57BFF0: the current object's matrix. The position - x and y in
// 1/512ths less 0x4000, z the negated half of the s16 at +0x3E, toward zero -
// through the current GTE matrix into the translation; the low words of
// +0x64, +0x68, +0x6C as the rotation's angles.
extern "C" short* __cdecl Sprite_ObjectMatrix(short* matrix) {
    const unsigned char* object = Sprite_Current;
    short position[4], angles[4];
    position[0] = static_cast<short>((Get<long>(object, 0x34) >> 9) - 0x4000);
    position[1] = static_cast<short>((Get<long>(object, 0x38) >> 9) - 0x4000);
    position[2] = static_cast<short>(-(Get<short>(object, 0x3E) / 2));
    angles[0] = Get<short>(object, 0x64);
    angles[1] = Get<short>(object, 0x68);
    angles[2] = Get<short>(object, 0x6C);
    Gte_RotTrans(position, reinterpret_cast<long*>(matrix + 10));
    return Gte_RotMatrix(angles, matrix);
}

// original 0x57BED0: the light's direction in the object's frame, to out[0..2]
// - the first row of the light matrix. (0, 0, Light_Angles[3]) turned by the
// light's angles, then by the object's rotation "transposed" - IN PLACE, and
// Gte_TransposeMatrix in place is not a transpose: the upper triangle is
// lost. Kept, as the original has it; Gte_NormalColor as shipped throws the
// lit colour away, so nothing on screen shows it.
extern "C" void __cdecl Light_ObjectDirection(short* out, const long* angles) {
    Light_AnglesCopy[0] = Light_Angles[0];
    Light_AnglesCopy[1] = Light_Angles[1];
    Light_AnglesCopy[2] = Light_Angles[2];
    const unsigned long zero[3] = {0, 0, 0};
    Gte_PushMatrix();
    alignas(4) short turn[16];
    Gte_RotMatrix(Light_AnglesCopy, turn);
    Gte_TransMatrix(reinterpret_cast<unsigned long*>(turn), zero);
    Gte_SetRotMatrix(reinterpret_cast<const unsigned long*>(turn));
    Gte_SetTransMatrix(reinterpret_cast<const unsigned long*>(turn));
    const short reach[4] = {0, 0, Light_Angles[3], 0};
    long light[3];
    Gte_RotTrans(reach, light);
    const short object_angles[4] = {static_cast<short>(angles[0]), static_cast<short>(angles[1]),
                                    static_cast<short>(angles[2]), 0};
    Gte_RotMatrix(object_angles, turn);
    Gte_TransposeMatrix(turn, turn);
    Gte_ApplyMatrixLV(turn, light, light);
    out[0] = static_cast<short>(light[0]);
    out[1] = static_cast<short>(light[1]);
    out[2] = static_cast<short>(light[2]);
    Gte_PopMatrix();
}

// original 0x57BAE0: the current object's quads as primitives and records.
// Returns the count's low byte; the one caller keeps al only.
//
// Kept from the original, deliberately:
//   - the count is the s8 at byte 0 of the object's +0x54 data widened to a
//     u16, so a negative one is some 65,000 quads - which the room check
//     then refuses;
//   - when the packet pool has no room the function returns 0 with the GTE
//     matrix still PUSHED and the matrices it loaded still loaded;
//   - a record's depth is the largest of the four depths / 4, kept as a u16
//     and compared zero-extended against the whole long, so a negative depth
//     never raises it and one past 0xFFFF is stored truncated;
//   - the vertices' fourth words in Prim_VertexScratch are left as they were
//     and go to Gte_LoadVertex with them.
extern "C" unsigned __cdecl Sprite_AddDrawRecords(unsigned long* record) {
    Gte_PushMatrix();
    const auto count = static_cast<std::uint16_t>(static_cast<std::int8_t>(ObjectData()[0]));
    alignas(4) short matrix[16];
    Sprite_ObjectMatrix(matrix);
    Gte_SetRotMatrix(reinterpret_cast<const unsigned long*>(matrix));
    Gte_SetTransMatrix(reinterpret_cast<const unsigned long*>(matrix));
    if (Sprite_Current[0x48] == 0) {
        const long s = Get<long>(Sprite_Current, 0x40);
        const long scale[3] = {s, s, s};
        Gte_ScaleMatrix(matrix, scale);
    }
    alignas(4) short view[16];
    std::memcpy(view, matrix, sizeof view);
    Light_ObjectDirection(reinterpret_cast<short*>(Light_Matrix), reinterpret_cast<const long*>(Sprite_Current + 0x64));
    Camera_LoadMatrix(view);
    Gte_SetMatrix2(Light_Matrix);

    const unsigned char flags = ObjectData()[3];
    const unsigned mode = flags & 0x40 ? flags : 2u;
    const unsigned char* quad = Get<const unsigned char*>(Sprite_Current, 0x50);
    const unsigned tpage = Gpu_GetTPage(0, mode & 3, 0x2C0, 0x100);
    unsigned char* const start = Gfx_PacketNext;
    const std::uint32_t bytes = static_cast<std::uint32_t>(count) * kPrimBytes;
    const std::uint32_t limit = (static_cast<std::uint32_t>(Gfx_BufferIndex) << 16) + kPoolLimit;
    if (limit < static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(start)) + bytes) return 0;

    const unsigned semi_trans = (mode >> 6) & 1;
    unsigned char* prim = start;
    auto* rec = reinterpret_cast<unsigned char*>(record);
    for (unsigned n = count; n != 0; --n) {
        Gpu_SetPolyFT4(prim);
        short* vertices = Prim_VertexScratch;
        for (unsigned i = 0; i < 12; ++i) vertices[i / 3 * 4 + i % 3] = Get<short>(quad, 2 + 2 * i);
        const long normal[3] = {Get<short>(quad, 0x1A), Get<short>(quad, 0x1C), Get<short>(quad, 0x1E)};
        for (unsigned k = 0; k < 4; ++k) Put<std::uint16_t>(prim, 0x14 + 0x10 * k, Get<std::uint16_t>(quad, 0x20 + 2 * k));
        for (unsigned k = 0; k < 4; ++k) {
            Gte_LoadVertex(reinterpret_cast<const unsigned long*>(vertices + 4 * k));
            Gte_Rtps();
            Gte_StoreScreenXY(reinterpret_cast<unsigned long*>(prim + 8 + 0x10 * k));
            long depth;
            Gte_StoreDepthQuarter(&depth);
            Gte_StoreDepthF(reinterpret_cast<float*>(prim + 0x10 + 0x10 * k));
            if (k == 0) {
                Put<std::uint16_t>(rec, 4, static_cast<std::uint16_t>(depth));
                Put<std::uint16_t>(rec, 6, static_cast<std::uint16_t>(depth));
            } else if (static_cast<long>(Get<std::uint16_t>(rec, 4)) < depth) {
                Put<std::uint16_t>(rec, 4, static_cast<std::uint16_t>(depth));
            }
        }
        Put<unsigned char*>(rec, 0, prim);
        Put<unsigned char*>(rec, 8, Sprite_Current);
        prim[4] = Sprite_Current[0x5D];
        prim[5] = Sprite_Current[0x5E];
        prim[6] = Sprite_Current[0x5F];
        if (ObjectData()[3] & 0x80) {
            Gte_VectorNormalS(normal, Prim_VertexScratch);
            const unsigned char color[4] = {Sprite_Current[0x5D], Sprite_Current[0x5E], Sprite_Current[0x5F], prim[7]};
            Gte_NormalColor(Prim_VertexScratch, color, prim + 4);
        }
        Put<std::uint16_t>(prim, 0x26, static_cast<std::uint16_t>(tpage));
        const int clut_x = static_cast<int>(static_cast<unsigned>(static_cast<int>(Get<short>(quad, 0))) << 4);
        Put<std::uint16_t>(prim, 0x16, static_cast<std::uint16_t>(Gpu_GetClut(clut_x, 0x1E3)));
        Gpu_SetSemiTrans(prim, semi_trans);
        prim += kPrimBytes;
        rec += kRecordBytes;
        quad += kQuadBytes;
    }
    Gfx_PacketNext = start + bytes;
    Gte_PopMatrix();
    return count & 0xFFu;
}

namespace {

// --- BOF3X_SHADOW=sprite_records: a differential fuzz, once at start-up -------
// Byte-copies of the three originals, their calls left where the originals
// called: this module injects BEFORE every module that owns one of their
// ~25 callees, so at fuzz time theirs runs Capcom's whole call tree and ours
// runs ours. One round: a fake object with its quads and data, the GTE's
// globals, the light and scratch globals, the camera's matrix and a window of
// the packet pool randomised; theirs, then from the same state ours; all of
// it and the result's low byte compared.

constexpr unsigned short kGameControlWord = 0x027F;   // measured: psx_gte_float.cpp
unsigned short GetControlWord() {
    unsigned short cw;
    __asm__("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__("fldcw %0" : : "m"(cw)); }

constexpr unsigned kMaxQuads = 8;
struct Scratch {
    alignas(4) unsigned char object[0x80];
    alignas(4) unsigned char quads[kMaxQuads * kQuadBytes];
    alignas(4) unsigned char data[4];
    alignas(4) unsigned char records[kMaxQuads * kRecordBytes];
};

struct Region { unsigned char* at; unsigned size; };
constexpr unsigned kRegions = 11, kStateBytes = 0x1000;

void Capture(const Region* r, unsigned char* out) {
    for (unsigned i = 0; i < kRegions; out += r[i].size, ++i) std::memcpy(out, r[i].at, r[i].size);
}
void Apply(const Region* r, const unsigned char* in) {
    for (unsigned i = 0; i < kRegions; in += r[i].size, ++i) std::memcpy(r[i].at, in, r[i].size);
}

using Fn = unsigned (__cdecl*)(unsigned long*);

void SelfTest(Fn theirs) {
    constexpr unsigned kRounds = 12000;
    static Scratch scratch;
    static unsigned char saved[kStateBytes], input[kStateBytes], their_out[kStateBytes], our_out[kStateBytes];
    auto* gte = reinterpret_cast<unsigned char*>(&Gte_RampFar);   // 0x7DE428 .. Gte_Depth, 0x7DE7A8
    const unsigned gte_size = static_cast<unsigned>(reinterpret_cast<unsigned char*>(&Gte_Depth) + 4 - gte);
    auto* pools = reinterpret_cast<unsigned char*>(Gfx_PacketPools);
    const unsigned short saved_word = GetControlWord();

    std::uint32_t rng = 0x57BAE021u;
    auto next = [&rng] { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    auto number = [&next]() -> long {
        static const long kEdge[] = {0, 1, -1, 2, 0x7FFFFFFF, static_cast<long>(0x80000000u), 0x1000, 500, 1000};
        switch (next() % 8) {
            case 0: return static_cast<long>(next());
            case 1: return kEdge[next() % 9];
            case 2: return static_cast<long>(next() % 0x2000000u) - 0x1000000;
            default: return static_cast<long>(next() % 0x4000u) - 0x1000;
        }
    };
    unsigned bad = 0, no_room = 0, empty = 0, negative = 0, lit = 0, unscaled = 0, quads = 0, padding = 0, scenes = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        // The packet window: mostly anywhere with room, one round in six at
        // the room check's edge, one byte either side of it.
        const unsigned buffer = next() % 2;
        std::int8_t count = static_cast<std::int8_t>(next() % (kMaxQuads + 1));
        if (next() % 16 == 0) count = static_cast<std::int8_t>(-1 - static_cast<int>(next() % 128));
        const unsigned bytes = count < 0 ? 0 : static_cast<unsigned>(count) * kPrimBytes;
        const unsigned pool = next() % 2;
        unsigned offset = next() % (0x10000u - 0x54u - kMaxQuads * kPrimBytes);
        if (next() % 6 == 0) offset = 0x10000u - 0x54u - bytes + (next() % 3) - 1;
        unsigned char* packet = pools + pool * 0x10000u + offset;
        const unsigned window = static_cast<unsigned>(pools + 0x20000u - packet) < 0x300u
                                    ? static_cast<unsigned>(pools + 0x20000u - packet) : 0x300u;
        const Region regions[kRegions] = {
            {reinterpret_cast<unsigned char*>(&scratch), sizeof scratch},
            {gte, gte_size},
            {reinterpret_cast<unsigned char*>(Light_Matrix), 0x20},
            {reinterpret_cast<unsigned char*>(Prim_VertexScratch), 0x20},
            {reinterpret_cast<unsigned char*>(Light_AnglesCopy), 6},
            {reinterpret_cast<unsigned char*>(Light_Angles), 8},
            {reinterpret_cast<unsigned char*>(Camera_Matrix), 18},
            {reinterpret_cast<unsigned char*>(&Gfx_PacketNext), 4},
            {reinterpret_cast<unsigned char*>(&Gfx_BufferIndex), 1},
            {reinterpret_cast<unsigned char*>(&Sprite_Current), 4},
            {packet, window},
        };
        if (round == 0) Capture(regions, saved);   // the fixed regions; the window is put back below

        static unsigned char window_saved[0x300];
        std::memcpy(window_saved, packet, window);
        unsigned total = 0;
        for (const Region& r : regions) total += r.size;
        if (total > kStateBytes) bof3::Fatal("sprite_records self-test: state of 0x%X bytes", total);

        // Everything random, then the fields that must be meaningful.
        Capture(regions, input);
        for (unsigned i = 0; i < total; ++i) input[i] = static_cast<unsigned char>(next());
        Apply(regions, input);
        if (next() % 2 == 0)
            for (unsigned i = 0; i + 1 < sizeof scratch.quads; i += 2) {
                const short v = static_cast<short>(static_cast<int>(next() % 0x2001) - 0x1000);
                std::memcpy(scratch.quads + i, &v, 2);
            }
        Put<unsigned char*>(scratch.object, 0x50, scratch.quads);
        Put<unsigned char*>(scratch.object, 0x54, scratch.data);
        if (next() % 3 == 0) scratch.object[0x48] = 0;
        scratch.data[0] = static_cast<unsigned char>(count);
        Sprite_Current = scratch.object;
        Gfx_PacketNext = packet;
        Gfx_BufferIndex = static_cast<unsigned char>(buffer);
        static const long kDepth[] = {0, 1, 17, 18, 19, 20, -1};
        Gte_MatrixDepth = next() % 4 == 0 ? kDepth[next() % 7] : static_cast<long>(next() % 18);
        Gte_NearZ = number(); Gte_ProjDistance = number();
        Gte_OffsetX = number(); Gte_OffsetY = number();
        Gte_RampNear = number(); Gte_RampFar = number();
        Gte_RampNearScale = number(); Gte_RampFarScale = number();
        // Half the rounds a scene, not noise: rotations of rotation size, a
        // translation and a position near the view, a small near plane - so
        // that depths land around 0..0xFFFF, where a quad's four can straddle
        // the u16 the record keeps and the sign of the comparison shows.
        if (next() % 2 == 0) {
            ++scenes;
            auto small = [&next](unsigned span) { return static_cast<long>(next() % (2 * span + 1)) - static_cast<long>(span); };
            auto* rotation = reinterpret_cast<short*>(Gte_Matrix);
            for (unsigned i = 0; i < 9; ++i) rotation[i] = static_cast<short>(small(0x1000));
            Gte_Matrix[5] = static_cast<unsigned long>(small(0x4000));
            Gte_Matrix[6] = static_cast<unsigned long>(small(0x4000));
            Gte_Matrix[7] = static_cast<unsigned long>(small(0x20000));
            for (unsigned i = 0; i < 9; ++i) Camera_Matrix[i] = static_cast<short>(small(0x1000));
            Gte_NearZ = small(0x100);
            Put<long>(scratch.object, 0x34, (0x4000 + small(0x400)) << 9);
            Put<long>(scratch.object, 0x38, (0x4000 + small(0x400)) << 9);
            Put<short>(scratch.object, 0x3E, static_cast<short>(small(0x4000)));
            Put<long>(scratch.object, 0x40, 0x1000 + small(0x800));
        }
        Capture(regions, input);

        if (count == 0) ++empty;
        if (count < 0) ++negative;
        if (scratch.object[0x48] != 0) ++unscaled;
        if (count > 0 && (scratch.data[3] & 0x80)) ++lit;
        const bool room = count >= 0 && (static_cast<std::uint32_t>(buffer) << 16) + kPoolLimit >=
                                            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(packet)) + bytes;
        if (!room) ++no_room; else quads += static_cast<unsigned>(count);

        unsigned result[2];
        for (int pass = 0; pass < 2; ++pass) {
            Apply(regions, input);
            if (pass == 0) SetControlWord(kGameControlWord);
            result[pass] = (pass ? Fn(&Sprite_AddDrawRecords) : theirs)(reinterpret_cast<unsigned long*>(scratch.records));
            if (pass == 0) SetControlWord(saved_word);
            Capture(regions, pass ? our_out : their_out);
        }
        // DIV-0021: a MATRIX's padding word is stale stack from the original
        // product and zero from ours, and Gte_SetRotMatrix and Gte_PushMatrix
        // carry it into Gte_Matrix and the stack's slots (and, from a push at
        // depth -1, into Gte_Matrix2). Theirs is taken there; counted apart.
        {
            unsigned char* our_gte = our_out + sizeof scratch;
            const unsigned char* their_gte = their_out + sizeof scratch;
            auto pad = [&](const void* matrix) {
                const unsigned at = static_cast<unsigned>(static_cast<const unsigned char*>(matrix) - gte) + 0x12;
                if (std::memcmp(our_gte + at, their_gte + at, 2) != 0) ++padding;
                std::memcpy(our_gte + at, their_gte + at, 2);
            };
            pad(Gte_Matrix);
            pad(Gte_Matrix2);
            for (unsigned slot = 0; slot < 20; ++slot) pad(Gte_MatrixStack + slot * 8);
        }
        const bool same = std::memcmp(their_out, our_out, total) == 0 && (result[0] & 0xFF) == (result[1] & 0xFF);
        if (!same && ++bad <= 8) {
            unsigned at = 0;
            while (at < total && their_out[at] == our_out[at]) ++at;
            unsigned region = 0, base = 0;
            while (region < kRegions && at >= base + regions[region].size) base += regions[region++].size;
            bof3::Log("shadow      sprite_records self-test MISMATCH: round %u, count %d, room %d, first difference "
                      "in region %u at +0x%X, results %02X / %02X", round, count, room, region, at - base,
                      result[0] & 0xFF, result[1] & 0xFF);
        }
        std::memcpy(packet, window_saved, window);
    }
    // The fixed regions as they were before round 0 (the window was put back each round).
    const Region fixed[kRegions] = {
        {reinterpret_cast<unsigned char*>(&scratch), sizeof scratch},
        {gte, gte_size},
        {reinterpret_cast<unsigned char*>(Light_Matrix), 0x20},
        {reinterpret_cast<unsigned char*>(Prim_VertexScratch), 0x20},
        {reinterpret_cast<unsigned char*>(Light_AnglesCopy), 6},
        {reinterpret_cast<unsigned char*>(Light_Angles), 8},
        {reinterpret_cast<unsigned char*>(Camera_Matrix), 18},
        {reinterpret_cast<unsigned char*>(&Gfx_PacketNext), 4},
        {reinterpret_cast<unsigned char*>(&Gfx_BufferIndex), 1},
        {reinterpret_cast<unsigned char*>(&Sprite_Current), 4},
        {pools, 0},
    };
    Apply(fixed, saved);
    SetControlWord(saved_word);
    bof3::Log("shadow      sprite_records self-test: %u rounds under control word %04X (%u quads drawn, %u with no "
              "room, %u empty, %u negative counts, %u lit, %u unscaled, %u scenes), %u MISMATCHES; the object and its data, the "
              "records, the GTE's globals, the light and scratch globals, the camera, the packet window and the "
              "result compared; a matrix padding word differed %u times (DIV-0021, not counted)", kRounds,
              kGameControlWord, quads, no_room, empty, negative, lit, unscaled, scenes, bad, padding);
    if (bad) bof3::Fatal("Sprite_AddDrawRecords differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void SpriteRecords_Inject() {
    // Called BEFORE every module that owns a callee of these three: the
    // clones' calls go where the originals' went, which must still be
    // Capcom's code while the fuzz runs.
    if (bof3::WantsShadow("sprite_records")) {
        // Offsets of every call: capstone over each function, 2026-09-21. No
        // jump leaves any of them.
        static const bof3::CloneCall kObjectCalls[] = {{0x68, nullptr}, {0x73, nullptr}};
        static const bof3::CloneCall kLightCalls[] = {{0x3E, nullptr}, {0x4D, nullptr}, {0x5C, nullptr}, {0x66, nullptr},
                                                      {0x70, nullptr}, {0x9A, nullptr}, {0xC7, nullptr}, {0xD6, nullptr},
                                                      {0xEA, nullptr}, {0x113, nullptr}};
        const void* object = bof3::CloneOriginal("Sprite_ObjectMatrix", bof3::addr::Sprite_ObjectMatrix, 0x80,
                                                 kObjectCalls, 2);
        const void* light = bof3::CloneOriginal("Light_ObjectDirection", bof3::addr::Light_ObjectDirection, 0x11D,
                                                kLightCalls, 10);
        // 0x21 and 0x87 are the calls to the two helpers: to their clones.
        const bof3::CloneCall calls[] = {
            {0x7, nullptr},   {0x21, object},   {0x2B, nullptr},  {0x35, nullptr},  {0x62, nullptr},  {0x87, light},
            {0x91, nullptr},  {0x9B, nullptr},  {0xDB, nullptr},  {0x146, nullptr}, {0x208, nullptr}, {0x20D, nullptr},
            {0x216, nullptr}, {0x220, nullptr}, {0x229, nullptr}, {0x23F, nullptr}, {0x244, nullptr}, {0x24D, nullptr},
            {0x257, nullptr}, {0x260, nullptr}, {0x27F, nullptr}, {0x284, nullptr}, {0x28D, nullptr}, {0x297, nullptr},
            {0x2A0, nullptr}, {0x2BF, nullptr}, {0x2C4, nullptr}, {0x2CD, nullptr}, {0x2D7, nullptr}, {0x2E0, nullptr},
            {0x343, nullptr}, {0x374, nullptr}, {0x391, nullptr}, {0x3A0, nullptr}, {0x3D0, nullptr}};
        const auto theirs = reinterpret_cast<Fn>(bof3::CloneOriginal(
            "Sprite_AddDrawRecords", bof3::addr::Sprite_AddDrawRecords, 0x3E1, calls, sizeof calls / sizeof calls[0]));
        SelfTest(theirs);
    }
    BOF3_INJECT(Sprite_ObjectMatrix);
    BOF3_INJECT(Light_ObjectDirection);
    BOF3_INJECT(Sprite_AddDrawRecords);
}
