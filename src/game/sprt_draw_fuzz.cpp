// BOF3X_SHADOW=sprt_draw: a differential fuzz of the three Direct3D sprite
// handlers, once at start-up, on the vertex-block harness (d3d_fuzz.h).
// docs/sprt-draw.md section 4.
//
// Two sets of copies, both made from Capcom's own bytes before anything
// patches them (SprtDraw_Inject runs this before its BOF3_INJECTs):
//
//   - Capcom's handlers as they are. Ours with the far edge's base at
//     Capcom's 0x7CA9DC - DIV-0010 off - is compared with them byte for byte.
//   - Capcom's handlers with the two far-edge operands of each re-aimed at
//     g_far: exactly what ran as DIV-0010 from 2026-09-20 to 2026-09-23
//     (gfx_sprite_uv.cpp). Ours with the base at g_far - DIV-0010 on - is
//     compared with them byte for byte.
//
// And a third pass: ours with DIV-0010 on against Capcom's unchanged, where
// every byte must be equal except the four far texture coordinates - so the
// divergence is checked to touch those and nothing else.
//
// Every copy has each of its six calls re-aimed at a recording stand-in, the
// same stand-ins ours is put on through sprt_draw::g; the device is the
// harness's fake. Per round: random state (the primitive, the scales, the
// vertex block, the texture-coordinate table and the 0x800 bytes after it,
// the first 512 entries of g_far, Gfx_DrawTpage), boundaries seeded; the copy,
// then from the same state ours; the log of calls, the vertices DrawPrimitive
// was handed, every byte of state either could touch, the primitive and the
// return value compared. Under a control word picked per round from 0x027F
// (the game's, measured by group R), 0x007F and 0x037F.
//
// The stand-ins write the colour pair where the handler reads it, and now and
// then a byte of the primitive, the vertex block, the scales, Gfx_DrawTpage, a
// texture coordinate or a far-edge entry: so every read and store the handler
// makes is ordered against every call it makes.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/d3d_fuzz.h"
#include "game/sprt_draw_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sprt_draw {
namespace {

using d3d_fuzz::Next;
using d3d_fuzz::Pick;
using d3d_fuzz::Record;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void PutWord(unsigned char* p, U v) {
    const std::uint16_t w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
U GetLong(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

unsigned short GetControlWord() {
    unsigned short cw;
    __asm__ volatile("fnstcw %0" : "=m"(cw));
    return cw;
}
void SetControlWord(unsigned short cw) { __asm__ volatile("fldcw %0" : : "m"(cw)); }
const unsigned short kControlWords[] = {0x027F, 0x007F, 0x037F};

// --- the state -------------------------------------------------------------

// The vertex block, the 8 bytes after it (Capcom's far index 0 reads the
// second dword), the texture table and the 0x800 bytes after it (Capcom's far
// indexes 256..767 land there: D-NEW-D), the scales, Gfx_DrawTpage, and the
// first 512 entries of our table.
constexpr U kBlock = kVertices;
constexpr U kBlockBytes = 0x80 + 8 + 0x400 + 0x800;
constexpr U kFarSeeded = 512;
constexpr U kFarBytes = kFarSeeded * 4;
constexpr U kStateBytes = kBlockBytes + 8 + 4 + kFarBytes;

struct Region {
    U address, bytes;
};
Region g_regions[4];

struct State {
    unsigned char bytes[kStateBytes];
};
State g_saved, g_start, g_theirs_state, g_ours_state;

void InitRegions() {
    g_regions[0] = {kBlock, kBlockBytes};
    g_regions[1] = {kScaleY, 8};
    g_regions[2] = {kDrawTpage, 4};
    g_regions[3] = {Addr(g_far), kFarBytes};
}
void Capture(State& s) {
    U at = 0;
    for (const Region& r : g_regions) {
        std::memcpy(s.bytes + at, At(r.address), r.bytes);
        at += r.bytes;
    }
}
void Restore(const State& s) {
    U at = 0;
    for (const Region& r : g_regions) {
        std::memcpy(At(r.address), s.bytes + at, r.bytes);
        at += r.bytes;
    }
}
// The first differing byte outside the vertex block's four far coordinates
// when `skip_far`; 0 when none.
U FirstDifference(const State& a, const State& b, bool skip_far) {
    U at = 0;
    for (const Region& r : g_regions) {
        for (U i = 0; i < r.bytes; ++i) {
            if (skip_far && r.address == kBlock && i < 0x80) {
                const U k = i & ~3u;
                if (k == 0x38 || k == 0x78 || k == 0x5C || k == 0x7C) continue;
            }
            if (a.bytes[at + i] != b.bytes[at + i]) return r.address + i;
        }
        at += r.bytes;
    }
    return 0;
}

// --- the stand-ins -----------------------------------------------------------

constexpr U kPrimBytes = 0x1C;
U g_round;
unsigned char* g_prim;
const unsigned char kTexels[] = {0x00, 0x01, 0x07, 0x08, 0x0F, 0x10, 0x7F, 0x80, 0xF0, 0xF7, 0xF8, 0xFE, 0xFF};
constexpr U kNTexels = sizeof kTexels;

U Mix(U salt) {
    U h = (g_round * 0x9E3779B1u) ^ (salt * 0x85EBCA6Bu) ^ (d3d_fuzz::g_log->n * 0xC2B2AE35u);
    h ^= h >> 16;
    h *= 0x7FEB352Du;
    h ^= h >> 15;
    h *= 0x846CA68Bu;
    h ^= h >> 16;
    return h;
}

// A quarter of the time, one byte of what the handlers read or write. The
// primitive's bytes are the likeliest: u, v, w, h and the code are each read
// once before a call and (the code) once after.
void Disturb(U salt) {
    const U h = Mix(salt ^ 0x5BD1E995u);
    if (h % 4) return;
    const auto value = static_cast<unsigned char>(h >> 24);
    switch ((h >> 2) % 8) {
    case 0:
    case 1:
    case 2:
        if (g_prim) g_prim[(h >> 8) % kPrimBytes] = value;
        break;
    case 3: At(kVertices)[(h >> 8) % 0x80] = value; break;
    case 4: At(kScaleY)[(h >> 8) % 8] = value; break;
    case 5: At(kDrawTpage)[(h >> 8) % 2] = value; break;
    case 6: At(kTexCoords + kTexels[(h >> 8) % kNTexels] * 4u)[(h >> 16) % 4] = value; break;
    default: {
        // a far entry either side could read for a seeded texel: Capcom's
        // tc[j - 1] or g_far[j]
        const U j = kTexels[(h >> 8) % kNTexels] + ((h >> 12) % 3 == 0 ? 8u : (h >> 12) % 3 == 1 ? 16u : 0u);
        unsigned char* p = (h >> 20) % 2 ? At(kCapcomFarBase + j * 4) : reinterpret_cast<unsigned char*>(&g_far[j]);
        p[(h >> 16) % 4] = value;
        break;
    }
    }
}

void __cdecl StubPrimColor(unsigned r, unsigned gg, unsigned b, unsigned code, unsigned mode,
                           unsigned long* diffuse, unsigned long* specular) {
    Record(1, r, gg, b, code, mode, diffuse != nullptr, specular != nullptr);
    Disturb(1);
    *diffuse = Mix(2);
    if (specular) *specular = Mix(3);
}
long __cdecl StubBind(unsigned tpage, unsigned clut) {
    Record(2, tpage, clut);
    Disturb(4);
    return static_cast<long>(Mix(5));
}
void __cdecl StubRet(unsigned a) {
    Record(3, a);
    Disturb(6);
}
void __cdecl StubBlend(unsigned code, unsigned mode) {
    Record(4, code, mode);
    Disturb(7);
}
void __cdecl StubShade(unsigned mode) {
    Record(5, mode);
    Disturb(8);
}

const Callees kStandIns = {StubPrimColor, StubBind, StubRet, StubBlend, StubShade};

// --- the copies --------------------------------------------------------------

using DrawFn = long(__cdecl*)(const unsigned char*);
template <typename F> const void* P(F* f) { return reinterpret_cast<const void*>(f); }

// Offsets from the disassembly (docs/sprt-draw.md section 2): the six E8s of
// each body and the disp32s of its two far-edge `fld [reg*4 + disp32]`.
struct Handler {
    const char* name;
    U original, size;
    DrawFn ours;
    U calls[6];
    U far_disp[2];
    U far_operand;   // what the original's two far operands hold
    U far_extent;    // g_far + this is what the re-aimed copy's hold: 0 where the index is u + w, else w
    bool sized;      // w and h from the primitive
};

const Handler kHandlers[] = {
    {"D3d_DrawSprt", 0x5A2300, 0x211, D3d_DrawSprt, {0x38, 0x1BB, 0x1C2, 0x1C9, 0x1E1, 0x1E8}, {0xD2, 0x130},
     0x7CA9DC, 0, true},
    {"D3d_DrawSprt8", 0x5A2520, 0x1EB, D3d_DrawSprt8, {0x35, 0x198, 0x19F, 0x1A6, 0x1BE, 0x1C5}, {0xBD, 0x11D},
     0x7CA9FC, 8, false},
    {"D3d_DrawSprt16", 0x5A2710, 0x1EB, D3d_DrawSprt16, {0x35, 0x198, 0x19F, 0x1A6, 0x1BE, 0x1C5}, {0xBD, 0x11D},
     0x7CAA1C, 16, false},
};
constexpr unsigned kNHandlers = sizeof kHandlers / sizeof kHandlers[0];

DrawFn Clone(const Handler& h, bool reaim) {
    const void* stubs[6] = {P(StubPrimColor), P(StubBind), P(StubRet), P(StubRet), P(StubBlend), P(StubShade)};
    const U callees[6] = {0x59FBA0, 0x59FFE0, kRetOnly, kRetOnly, 0x59FCA0, 0x59FD80};
    bof3::CloneCall calls[6];
    for (int i = 0; i < 6; ++i) calls[i] = {h.calls[i], stubs[i], callees[i]};
    auto* code = static_cast<unsigned char*>(bof3::CloneOriginal(h.name, h.original, h.size, calls, 6));
    if (!code) bof3::Fatal("sprt_draw: CloneOriginal(%s) returned null", h.name);
    for (U at : h.far_disp) {
        // D9 04 <sib> disp32: fld dword [reg*4 + disp32]
        if (GetLong(code + at) != h.far_operand || code[at - 3] != 0xD9 || code[at - 2] != 0x04)
            bof3::Fatal("%s: no `fld [reg*4 + 0x%X]` at +0x%X", h.name, (unsigned)h.far_operand, (unsigned)at);
        if (reaim) PutLong(code + at, Addr(g_far + h.far_extent));
    }
    return reinterpret_cast<DrawFn>(code);
}

// --- seeds -------------------------------------------------------------------

const U kFloats[] = {
    0x00000000, 0x80000000, 0x3F800000, 0x3F000000, 0x43200000, 0xC2000000, 0x3C23D70A,  // 0 -0 1 .5 160 -32 .01
    0x7149F2CA, 0x7F7FFFFF, 0x00800000, 0x007FFFFF, 0x00000001, 0x7F800000, 0xFF800000,  // 1e30 max min-normal denormals inf
    0x7FC00000, 0x7FA00000, 0xFF800001, 0x3F7D70A4, 0x4B7FFFFF, 0x3EAAAAAB, 0xBF800000,  // qNaN sNaN sNaN .99 2^24-1 1/3 -1
    0x4B800000, 0x4B000001, 0x3F800001, 0xC6FFFE00,                                      // 2^24 2^23+1 1+ulp -32767
};
const U kScales[] = {0x40000000, 0x3F800000, 0x3FC00000, 0x40400000, 0x3F000000, 0xC0000000, 0x40100000,
                     0x0DA24260, 0x7F000000, 0x3F800001, 0x3F7FFFFF, 0x40000001};
const U kSizes[] = {0x0000, 0x0001, 0x0002, 0x0007, 0x0008, 0x0009, 0x000C, 0x000F, 0x0010, 0x0011, 0x0020, 0x0080,
                    0x00FF, 0x0100, 0x0101, 0x01FF, 0x7FFF, 0x8000, 0x8001, 0xFFFF};
const U kWords[] = {0x0000, 0x0001, 0x0020, 0x0040, 0x0060, 0x0080, 0x0100, 0x0180, 0x0400,
                    0x0800, 0x0C00, 0x7FFF, 0x8000, 0xFFFF, 0x0010, 0x001F};
const U kColours[] = {0x00, 0x01, 0x7F, 0x80, 0x81, 0xFF};
const U kCodes[] = {0x64, 0x65, 0x66, 0x67, 0x74, 0x75, 0x7C, 0x7F};

void RandomFloat(unsigned char* p) {
    PutLong(p, Next() % 3 ? Pick(kFloats, sizeof kFloats / 4) : Next());
}

// A texture table entry: seven in eight the game's value for its index, else
// any bits (NaNs among them, which fld / fstp quiet).
U TableValue(float game) {
    if (Next() % 8) {
        U v;
        std::memcpy(&v, &game, 4);
        return v;
    }
    return Next() % 2 ? Next() : Pick(kFloats, sizeof kFloats / 4);
}

void RandomCommon() {
    for (U a : {kScaleX, kScaleY}) {
        U v;
        switch (Next() % 5) {
        case 0: v = Next(); break;
        case 1: v = 0x3F800000u + (Next() % 0x2000000u); break;
        default: v = Pick(kScales, sizeof kScales / 4); break;
        }
        PutLong(At(a), v);
    }
    for (U i = 0; i < 0x88; ++i) At(kVertices)[i] = static_cast<unsigned char>(Next());
    // Capcom's table and what follows it: the game's values continued past
    // 255, so that an index off by one shows in any entry.
    for (U i = 0; i < 0x300; ++i)
        PutLong(At(kTexCoords + i * 4), TableValue((static_cast<float>(i) + 0.512f) / 256.0f));
    // Ours: its real values, sometimes disturbed; restored after the fuzz.
    for (U j = 0; j < kFarSeeded; ++j) {
        const float real = (static_cast<float>(j) + (0.012f - 1.0f / 30.0f)) / 256.0f;
        const U v = TableValue(real);
        std::memcpy(&g_far[j], &v, 4);
    }
    PutLong(At(kDrawTpage), Next() % 2 ? Next() : Pick(kWords, 16) | (Next() & 0xFFFF0000u));
}

void RandomPrim(const Handler& h, unsigned char* p) {
    for (U i = 0; i < kPrimBytes; ++i) p[i] = static_cast<unsigned char>(Next());
    for (U i = 4; i < 7; ++i)
        if (Next() % 2) p[i] = static_cast<unsigned char>(Pick(kColours, 6));
    if (Next() % 2) p[7] = static_cast<unsigned char>(Pick(kCodes, 8));
    for (U off : {8u, 0xCu, 0x10u})
        if (Next() % 4) RandomFloat(p + off);
    if (Next() % 4) p[0x14] = kTexels[Next() % kNTexels];
    if (Next() % 4) p[0x15] = kTexels[Next() % kNTexels];
    if (Next() % 2) PutWord(p + 0x16, Pick(kWords, 16));
    if (h.sized) {
        // Mostly so that u + w stays inside the seeded 768 entries; now and
        // then wide (D-NEW-D reaches unseeded memory, equal on both sides).
        for (U off : {0x18u, 0x1Au}) {
            const U texel = p[off == 0x18 ? 0x14 : 0x15];
            switch (Next() % 4) {
            case 0: PutWord(p + off, Pick(kSizes, sizeof kSizes / 4)); break;
            case 1: PutWord(p + off, (0x100 - texel + Next() % 5 - 2) & 0xFFFF); break;   // u + w near 256
            case 2: PutWord(p + off, Next() % (0x300 - texel)); break;
            default: break;   // random
            }
        }
        // u + w = 0: Capcom's far index is -1, the dword before the table.
        if (Next() % 32 == 0) {
            p[0x14] = 0;
            PutWord(p + 0x18, 0);
        }
        if (Next() % 32 == 0) {
            p[0x15] = 0;
            PutWord(p + 0x1A, 0);
        }
    }
}

d3d_fuzz::Log g_theirs, g_ours;

// DIV-0010 against Capcom's: every byte equal but the four far coordinates -
// the second and fourth vertices' tu, the third and fourth's tv.
bool SameButFar(const unsigned char* ours, const unsigned char* theirs) {
    for (U i = 0; i < 0x80; i += 4) {
        if (i == 0x38 || i == 0x78 || i == 0x5C || i == 0x7C) continue;
        if (std::memcmp(ours + i, theirs + i, 4) != 0) return false;
    }
    return true;
}

// 0: DIV-0010 off against Capcom's; 1: on, against the re-aimed copies;
// 2: on, against Capcom's, the far coordinates excepted.
const char* const kPassNames[] = {"Capcom's edges", "DIV-0010", "DIV-0010 against Capcom's"};

struct Cover {
    unsigned past_table, gap, wide, far_differs;
};

unsigned FuzzHandler(const Handler& h, DrawFn theirs, unsigned pass, unsigned rounds, Cover& cover) {
    unsigned bad = 0;
    unsigned char prim[kPrimBytes], prim_start[kPrimBytes], prim_theirs[kPrimBytes];
    const unsigned short saved_cw = GetControlWord();
    for (unsigned r = 0; r < rounds; ++r) {
        g_round = r + (h.original << 4) + (pass << 28);
        RandomCommon();
        RandomPrim(h, prim_start);
        const unsigned short cw = kControlWords[Next() % 3];
        Capture(g_start);

        const U u = prim_start[0x14];
        U w = h.sized ? (prim_start[0x18] | prim_start[0x19] << 8) : h.far_extent;
        if (u + w > 256) ++cover.past_table;
        if (u + w == 0) ++cover.gap;
        if (w >= 0x8000) ++cover.wide;

        std::memcpy(prim, prim_start, sizeof prim);
        g_prim = prim;
        g_theirs.Clear();
        d3d_fuzz::g_log = &g_theirs;
        SetControlWord(cw);
        const long ret_theirs = theirs(prim);
        SetControlWord(saved_cw);
        Capture(g_theirs_state);
        std::memcpy(prim_theirs, prim, sizeof prim);

        Restore(g_start);
        std::memcpy(prim, prim_start, sizeof prim);
        g_ours.Clear();
        d3d_fuzz::g_log = &g_ours;
        g_far_base = pass == 0 ? kCapcomFarBase : Addr(g_far);
        SetControlWord(cw);
        const long ret_ours = h.ours(prim);
        SetControlWord(saved_cw);
        g_far_base = kCapcomFarBase;
        d3d_fuzz::g_log = nullptr;
        g_prim = nullptr;
        Capture(g_ours_state);

        char why[200] = "";
        const d3d_fuzz::SnapCompare rule = pass == 2 ? SameButFar : nullptr;
        bool same = d3d_fuzz::SameLog(g_ours, g_theirs, why, rule);
        if (same && ret_ours != ret_theirs) {
            same = false;
            std::strcpy(why, "the return value");
        }
        if (same) {
            if (const U where = FirstDifference(g_ours_state, g_theirs_state, pass == 2)) {
                same = false;
                std::snprintf(why, sizeof why, "memory at 0x%X", (unsigned)where);
            }
        }
        if (same && std::memcmp(prim, prim_theirs, sizeof prim) != 0) {
            same = false;
            std::strcpy(why, "the primitive");
        }
        if (pass == 2 && g_theirs.n_snaps && g_ours.n_snaps &&
            std::memcmp(g_ours.snaps[0], g_theirs.snaps[0], d3d_fuzz::kSnapBytes) != 0)
            ++cover.far_differs;
        if (!same) {
            if (bad < 4)
                bof3::Log("shadow      sprt_draw MISMATCH: %s (%s) round %u (cw %04X, u %X w %X): %s", h.name,
                          kPassNames[pass], r, cw, (unsigned)u, (unsigned)w, why);
            ++bad;
        }
    }
    return bad;
}

}  // namespace

void SelfTest() {
    InitRegions();
    // The copies, before SprtDraw_Inject patches the originals: Capcom's, and
    // Capcom's with the far operands re-aimed as DIV-0010's copies had them.
    DrawFn capcom[kNHandlers], reaimed[kNHandlers];
    for (unsigned i = 0; i < kNHandlers; ++i) {
        capcom[i] = Clone(kHandlers[i], false);
        reaimed[i] = Clone(kHandlers[i], true);
    }

    Capture(g_saved);
    const Callees saved_callees = g;
    const U saved_base = g_far_base;
    g = kStandIns;
    d3d_fuzz::Seed(0x64747C64u);

    unsigned bad = 0;
    constexpr unsigned kRounds = 20000;
    Cover cover[kNHandlers] = {};
    {
        d3d_fuzz::DeviceSwap swap;
        for (unsigned i = 0; i < kNHandlers; ++i) {
            bad += FuzzHandler(kHandlers[i], capcom[i], 0, kRounds, cover[i]);
            bad += FuzzHandler(kHandlers[i], reaimed[i], 1, kRounds, cover[i]);
            bad += FuzzHandler(kHandlers[i], capcom[i], 2, kRounds, cover[i]);
        }
    }

    g = saved_callees;
    g_far_base = saved_base;
    Restore(g_saved);
    FillFarTable();   // the seeded entries back to the real ones, whatever the capture held

    for (unsigned i = 0; i < kNHandlers; ++i)
        bof3::Log("shadow      sprt_draw self-test: %s %u rounds x 3 (Capcom's edges exact, DIV-0010 exact against "
                  "the re-aimed copy, DIV-0010 against Capcom's but the far coordinates) under three control "
                  "words; of the %u: u + w past 256 %u, u + w = 0 %u, w >= 0x8000 %u; of the last %u, far "
                  "coordinates changed by DIV-0010 in %u",
                  kHandlers[i].name, kRounds, 3 * kRounds, cover[i].past_table, cover[i].gap, cover[i].wide,
                  kRounds, cover[i].far_differs);
    if (bad) bof3::Fatal("the Direct3D sprite handlers differ from the original in %u self-test rounds", bad);
}

}  // namespace sprt_draw
