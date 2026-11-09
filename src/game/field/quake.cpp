// Moved here by tools/move_funcs.py from battle_e6.cpp, magic_s23.cpp, rest_3g.cpp (docs/source-layout-round.md).
#include "game/field/quake.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_e6_callees.h"
#include "game/boss_harness.h"
#include "game/cheats.h"
#include "game/draw_pool.h"
#include "game/field/quake_callees.h"
#include "game/magic_harness.h"
#include "game/magic_s23_callees.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/psp_code.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/rest_3g.h"
#include "game/rest_3g_callees.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = quake::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetWord;

using move_script::Word;

U LBattleE6(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

std::int32_t S8(unsigned char v) { return static_cast<std::int8_t>(v); }

std::int32_t S16BattleE6(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

U Addr(const void* p) { return Low32(p); }

float F(const unsigned char* p) {
    float f;
    std::memcpy(&f, p, sizeof f);
    return f;
}

void SetF(unsigned char* p, float f) { std::memcpy(p, &f, sizeof f); }

// A BMAGIC vertex's height (Quake_VertexLift 0x4CF4B0, R3G's): ax is read.
U CellHeight(std::int32_t x, std::int32_t z) { return BH_AT(U (__cdecl*)(std::int32_t, std::int32_t), at::kCellHeight.v)(x, z); }

// A PSX short vector and matrix as the BMAGIC cells build them on the stack.
struct SVec {
    std::int16_t x, y, z, pad;
};

struct Matrix {
    std::int16_t m[9];
    std::int16_t pad;
    std::int32_t t[3];
};

// A BMAGIC vertex from a record dword: x = x0 + 2 * its byte 3, z = z0 + 2 *
// its byte 2 (16 bits), y = the cell height there + its low word.
SVec CellVertex(U x0, U z0, const unsigned char* at) {
    const U d = LBattleE6(at);
    SVec v{};
    v.x = static_cast<std::int16_t>(x0 + 2 * static_cast<U>(S8(static_cast<unsigned char>(d >> 24))));
    v.y = static_cast<std::int16_t>(z0 + 2 * static_cast<U>(S8(static_cast<unsigned char>(d >> 16))));
    v.z = static_cast<std::int16_t>(CellHeight(v.x, v.y) + Word(at));
    return v;
}

// A spin vertex: three 10-bit fields, bit 9 extending the sign over the upper
// six bits (the first field keeps the dword's bits 30 and 31 when bit 9 is 0).
std::int16_t Field10(U v) {
    if (v & 0x200) v |= 0xFFFFFC00u;
    return static_cast<std::int16_t>(v);
}

using Pers4 = long (__cdecl*)(const SVec*, const SVec*, const SVec*, const SVec*, unsigned char*, unsigned char*, unsigned char*,
                              unsigned char*, long*, long*);

using Pers = long (__cdecl*)(const void*, void*, long*);

// The loop over a record's entries ends when the counter meets byte +2; one
// that never can runs past the record in the original (reading on until it
// faults). Ours stops first.
void CheckEntries(const char* who, const unsigned char* record, unsigned first, unsigned step) {
    const unsigned len = record[2];
    if (len == first) return;
    if (len < first || (len - first) % step != 0)
        bof3::Fatal("%s: record 0x%X's length byte %u is not %u + %u n - the original's loop runs past the record "
                    "(docs/battle_e6.md section 7)",
                    who, (unsigned)Addr(record), len, first, step);
}

namespace hat = magic_harness::at;

using magic_harness::EnemyOf;

using magic_harness::Mem;

using magic_harness::PartyOf;

using magic_harness::Pointer;

using move_script::SetLong;

unsigned char* Sc() { return Sprite_Current; }

void Bump(unsigned char& b, int by = 1) { b = static_cast<unsigned char>(b + by); }

short Get(std::uint32_t address) { return static_cast<short>(Word(Mem(address))); }

void Put(std::uint32_t address, int v) { SetWord(Mem(address), static_cast<unsigned>(v)); }

std::uint8_t Byte(std::uint32_t address) { return Mem(address)[0]; }

// 32-bit imul and shl, wrapping as the original's do.
std::int32_t Imul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}

unsigned char* Slot(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + index * hat::kTaskStride); }

int Sin(int angle) { return MH_CALL(Math_Sin)(angle); }

// A phase through a stack table the original builds, by +1: aborts past it.
void Dispatch(const char* who, const std::uint32_t* table, unsigned n) {
    const unsigned phase = Sc()[1];
    if (phase >= n) bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, n);
    magic_harness::Phase(table[phase])();
}

// The acting side's facing: the actor sprite's +8, turned by 2 when the
// target's bit 0x40 and the actor's side disagree (bit 0x40 and a party
// actor, or neither and an enemy).
unsigned char Facing() {
    const bool bit = (Mem(::bof3::Img(hat::kTarget))[0] & 0x40) != 0;
    const bool party = Mem(::bof3::Img(hat::kActor))[0] < 3;
    const unsigned char f = Pointer(::bof3::Img(MagicS23_ActorSprite_addr))[8];
    return bit == party ? static_cast<unsigned char>(f ^ 2) : f;
}

// Quake's walk over MapView_Cells: every non-zero cell names a run of
// AreaMap_Header dwords (the cell plus AreaMap_CellBase's low word; the
// dword before it holds the run's length in its high word), stepped by each
// dword's byte +2; `map` switches the top byte (the polygon code) of each.
// A run whose steps do not land on its end is not checked, as the original.
void Walk(void (*map)(unsigned char*)) {
    for (std::uint32_t cell = ::bof3::Img(MapView_Cells_addr); cell < ::bof3::Img(at::kMapCellsEnd); cell += 2) {
        const unsigned c = Word(Mem(cell));
        if (c == 0) continue;
        const std::uint32_t index = c + (static_cast<std::uint32_t>(Long(Mem(::bof3::Img(AreaMap_CellBase_addr)))) & 0xFFFF);
        const std::uint32_t n = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(AreaMap_Header_addr) - 4 + index * 4))) >> 16;
        std::uint32_t p = ::bof3::Img(AreaMap_Header_addr) + index * 4;
        const std::uint32_t end = p + n * 4 - 4;
        while (p != end) {
            map(Mem(p));
            p += 4u * Mem(p)[2];
        }
    }
}

void Recode(unsigned char* e, std::uint32_t from, std::uint32_t to, bool keep_low) {
    const std::uint32_t v = static_cast<std::uint32_t>(Long(e));
    if ((v & 0xFF000000u) != from) return;
    SetLong(e, static_cast<std::int32_t>(keep_low ? (v & 0xFFFFFFu) | to : v & 0xFFFFFFu));
}

// At the start: code 0 becomes 0x28, 0x21 0x29, 0x22 0x2A, 0x27 0x2B.
void Semi(unsigned char* e) {
    Recode(e, 0, 0x28000000u, true);
    Recode(e, 0x21000000u, 0x29000000u, true);
    Recode(e, 0x22000000u, 0x2A000000u, true);
    Recode(e, 0x27000000u, 0x2B000000u, true);
}

// At the end: 0x28 becomes 0, 0x29 0x21, 0x2A 0x22, 0x2B 0x27.
void Opaque(unsigned char* e) {
    Recode(e, 0x28000000u, 0, false);
    Recode(e, 0x29000000u, 0x21000000u, true);
    Recode(e, 0x2A000000u, 0x22000000u, true);
    Recode(e, 0x2B000000u, 0x27000000u, true);
}

long Ground(const unsigned char* at_position) {
    return MH_CALL(AreaMap_Elevation)(rv::As<records::SpriteHead>(at_position)->x, rv::As<records::SpriteHead>(at_position)->z);
}

// Quake_Heave's idiv by a Quake_Divisors byte: the original faults on 0.
int Divide(int n, unsigned d) {
    if (d == 0) bof3::Fatal("Quake_Heave: a divisor of 0 (facing byte %u reads past Quake_Divisors)", Byte(::bof3::Img(MagicS23_QuakeFacing_addr)));
    return n / static_cast<int>(d);
}

void AddByte(std::uint32_t address, int v) { Mem(address)[0] = static_cast<unsigned char>(Mem(address)[0] + v); }

}  // namespace

#define S23_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

std::int32_t I(U v) { return static_cast<std::int32_t>(v); }

std::int32_t S8Rest3g(unsigned char b) { return static_cast<signed char>(b); }

// sar on the 32 bits.
U SarRest3g(U v, unsigned n) { return static_cast<U>(I(v) >> n); }

}  // namespace

// original 0x4CE100: the kind-2 task (Magic_Rows row 69). Its phase +1
// through a four-entry stack table - Quake_Start, Quake_Rumble, Quake_Heave,
// Quake_End - unchecked in the original, ours aborts past it.
S23_EXPORT void __cdecl Quake_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::Quake_Start, bof3::addr::Quake_Rumble,
                                                 bof3::addr::Quake_Heave, bof3::addr::Quake_End};
    Dispatch("Quake_Task", kPhases, 4);
}

// original 0x4CE140: the facing to 0x6959CC; the block's first column / row
// the view's cell (0x905E66 / 0x905E62) plus Quake_FacingOffsets[facing]
// (signed bytes, the facing unbounded); +0xB and +9 0; how high each party
// member, each enemy still in (its +0x3E kept too), each Sprite_Objects /
// Sprite_ObjectsExtra object in use and each Effect_Objects object of kind
// 0x17 stands above the ground (AreaMap_Elevation); the map's polygon codes
// made semi-transparent (Walk, Semi); the lift table zeroed; the hover flag
// for event battles 0x1C, 0x23, 0x2F and 0x34 (a byte table of the
// original's jump); sound 0x100; on.
S23_EXPORT void __cdecl Quake_Start(void) {
    const unsigned char facing = Facing();
    Mem(::bof3::Img(MagicS23_QuakeFacing_addr))[0] = facing;
    {
        const unsigned f = Byte(::bof3::Img(MagicS23_QuakeFacing_addr));
        const int dx = static_cast<signed char>(Byte(::bof3::Img(Quake_FacingOffsets_addr) + 2 * f));
        const int dy = static_cast<signed char>(Byte(::bof3::Img(Quake_FacingOffsets_addr) + 2 * f + 1));
        Put(::bof3::Img(MagicS23_QuakeX_addr), dx + Word(Mem(::bof3::Img(at::kCameraCellX))));
        Put(::bof3::Img(MagicS23_QuakeY_addr), dy + Word(Mem(::bof3::Img(at::kCameraCellY))));
    }
    Sc()[0xB] = 0;
    Sc()[9] = 0;
    for (unsigned i = 0; i < 3; ++i) {
        if (PartyOf(static_cast<unsigned char>(i))[0] == 0) continue;
        const long ground = Ground(Slot(i));
        Put(::bof3::Img(at::kQuakePartyZ) + 2 * i, rv::As<records::PartyMember>(PartyOf(static_cast<unsigned char>(i)))->height_whole - static_cast<short>(ground));
    }
    for (unsigned i = 0; i < 8; ++i) {
        if (MH_CALL(Battle_ActorIsOut)(i + 3) != 0) continue;
        const unsigned char* const slot = Slot(3 + i);
        const records::BattleTask* const tslot = rv::AsConst<records::BattleTask>(slot);
        const std::int32_t x = tslot->x, z = tslot->z;
        const std::uint16_t height = rv::As<records::BattleEnemy>(EnemyOf(static_cast<unsigned char>(i + 3)))->height_whole;
        const long ground = MH_CALL(AreaMap_Elevation)(x, z);
        Put(::bof3::Img(at::kQuakeEnemyZ) + 2 * i, height);
        Put(::bof3::Img(at::kQuakeEnemyUp) + 2 * i, height - static_cast<short>(ground));
    }
    for (unsigned i = 0; i < 30; ++i) {
        const unsigned char* const o = Mem(::bof3::Img(Sprite_Objects_addr) + 0xA4 * i);
        if (o[0] == 0) continue;
        const long ground = Ground(o);
        Put(::bof3::Img(at::kQuakeSpriteZ) + 2 * i, rv::As<records::SpriteObject>(o)->height_whole - static_cast<short>(ground));
    }
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char* const o = Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0xA4 * i);
        if (o[0] == 0) continue;
        const long ground = Ground(o);
        Put(::bof3::Img(at::kQuakeExtraZ) + 2 * i, rv::As<records::SpriteObject>(o)->height_whole - static_cast<short>(ground));
    }
    for (unsigned i = 0; i < 20; ++i) {
        const unsigned char* const o = Mem(::bof3::Img(Effect_Objects_addr) + 0x80 * i);
        if (o[0] == 0 || o[5] != 0x17) continue;
        const long ground = Ground(o);
        Put(::bof3::Img(at::kQuakeEffectZ) + 2 * i, rv::As<records::EffectObject>(o)->height_whole - static_cast<short>(ground));
    }
    Walk(&Semi);
    std::memset(Mem(::bof3::Img(at::kQuakeLift)), 0, 0xFF);
    switch (Byte(::bof3::Img(EventBattle_Id_addr))) {
    case 0x1C: case 0x23: case 0x2F: case 0x34: Mem(::bof3::Img(MagicS23_QuakeHover_addr))[0] = 1; break;
    default: Mem(::bof3::Img(MagicS23_QuakeHover_addr))[0] = 0; break;
    }
    MH_CALL(Sound_PlayById)(0x100);
    Bump(Sc()[1]);
}

// original 0x4CE4A0: the rumble. +9 up; at 0x10 back to 0 and on; the view
// redrawn (MapView_Redraw 2) and the camera shifted +-(+9 / 2) by the frame's
// parity.
S23_EXPORT void __cdecl Quake_Rumble(void) {
    Bump(Sc()[9]);
    unsigned char* sc = Sc();
    if (sc[9] >= 0x10) {
        sc[9] = 0;
        Bump(Sc()[1]);
        sc = Sc();
    }
    const int odd = static_cast<int>(Frame_Counter & 1);
    MapView_Redraw = 2;
    Put(::bof3::Img(Camera_ShiftY_addr), (2 * odd - 1) * (sc[9] >> 1));
}

// original 0x4CE500: the heave. For each of 16 columns i the lift
// (sin((+9 + 2i) << 7) - sin(((+9 + 2i) << 7) - 0x80)) / 256 (0 at i = 0)
// and the fall (sin((+9 + 2i + 2) << 7) - sin((+9 + 2i + 1) << 7)) / 256 (0 at
// i = 15); for each of 14 rows j inside the area (bytes 0 / 1 of
// AreaMap_Header), the cell's four AreaMap_Corners bytes raised by lift and
// fall over the Quake_Divisors pair of j (the row by the facing's bit 1;
// across the map, i and j swap and so do the middle two corners), and the
// lift of the first corner kept in the lift table (the last in 0x695A2C).
// Then every actor and object put back on the ground with its kept height -
// the party when the target has bit 0x80, else the enemies still in (in the
// hover battles, an enemy above the ground keeps its own height) - the view
// redrawn, the camera shifted -8 / +8 by the frame's parity, +9 up; when the
// lift table is all 0 and +9 is past 0x5A, on.
S23_EXPORT void __cdecl Quake_Heave(void) {
    for (int i = 0; i < 16; ++i) {
        const int s0 = Sin(((Sc()[9] + 2 * i) * 128 - 0x80) & 0xFFF);
        const int s1 = Sin(((Sc()[9] + 2 * i) * 128) & 0xFFF);
        const int lift = (i != 0 ? 1 : 0) * ((s1 >> 8) - (s0 >> 8));
        const int s2 = Sin(((Sc()[9] + 2 * i + 2) * 128) & 0xFFF);
        const int s3 = Sin(((Sc()[9] + 2 * i + 1) * 128) & 0xFFF);
        const int fall = (i != 15 ? 1 : 0) * ((s2 >> 8) - (s3 >> 8));
        const bool across = (Byte(::bof3::Img(MagicS23_QuakeFacing_addr)) & 1) != 0;
        for (int j = 0; j < 14; ++j) {
            const std::uint32_t area = static_cast<std::uint32_t>(Long(Mem(::bof3::Img(AreaMap_Header_addr))));
            const int width = static_cast<int>(area & 0xFF), height = static_cast<int>((area >> 8) & 0xFF);
            const int first_row = Get(::bof3::Img(MagicS23_QuakeY_addr));
            const int row = first_row + (across ? j : i);
            if (row < 0 || row >= height) continue;
            const int col = Get(::bof3::Img(MagicS23_QuakeX_addr)) + (across ? i : j);
            if (col < 0 || col >= width) continue;
            const unsigned row_of_divisors = (Byte(::bof3::Img(MagicS23_QuakeFacing_addr)) >> 1) * 15u;
            const std::uint32_t corner =
                ::bof3::Img(AreaMap_Corners_addr) + 4u * static_cast<std::uint32_t>(Imul(static_cast<int>(Long(Mem(::bof3::Img(AreaMap_Header_addr))) & 0xFF), row) + col);
            const unsigned d0 = Byte(::bof3::Img(Quake_Divisors_addr) + row_of_divisors + static_cast<unsigned>(j));
            const int raised = Divide(lift, d0);
            AddByte(corner, raised);
            const unsigned d1 = Byte(::bof3::Img(Quake_Divisors_addr) + row_of_divisors + static_cast<unsigned>(j) + 1);
            AddByte(corner + (across ? 2 : 1), Divide(lift, d1));
            AddByte(corner + (across ? 1 : 2), Divide(fall, d0));
            AddByte(corner + 3, Divide(fall, d1));
            const std::uint32_t k = static_cast<std::uint32_t>(i * 15 + j);
            const unsigned char before = Byte(::bof3::Img(at::kQuakeLift) + k);
            Mem(::bof3::Img(magic_s23::at::kQuakeLast) + k)[0] = before;
            Mem(::bof3::Img(at::kQuakeLift) + k)[0] = static_cast<unsigned char>(before + raised);
        }
    }
    if (Mem(::bof3::Img(hat::kTarget))[0] & 0x80) {
        for (unsigned i = 0; i < 3; ++i) {
            if (PartyOf(static_cast<unsigned char>(i))[0] == 0) continue;
            const long ground = Ground(Slot(i));
            rv::Store<unsigned>(rv::As<records::PartyMember>(PartyOf(static_cast<unsigned char>(i)))->height_whole,
                                static_cast<unsigned>(static_cast<short>(ground) + Get(::bof3::Img(at::kQuakePartyZ) + 2 * i)));
        }
    } else {
        for (unsigned i = 0; i < 8; ++i) {
            if (MH_CALL(Battle_ActorIsOut)(i + 3) != 0) continue;
            const unsigned char* const slot = Slot(3 + i);
            if (Byte(::bof3::Img(MagicS23_QuakeHover_addr)) != 0) {
                const short kept = Get(::bof3::Img(at::kQuakeEnemyZ) + 2 * i);
                const short ground = static_cast<short>(Ground(slot));
                if (kept > ground) {
                    rv::Store<unsigned>(rv::As<records::BattleEnemy>(EnemyOf(static_cast<unsigned char>(i + 3)))->height_whole, static_cast<std::uint16_t>(kept));
                    continue;
                }
            }
            const long ground = Ground(slot);
            rv::Store<unsigned>(rv::As<records::BattleEnemy>(EnemyOf(static_cast<unsigned char>(i + 3)))->height_whole,
                                static_cast<unsigned>(static_cast<short>(ground) + Get(::bof3::Img(at::kQuakeEnemyUp) + 2 * i)));
        }
    }
    for (unsigned i = 0; i < 30; ++i) {
        unsigned char* const o = Mem(::bof3::Img(Sprite_Objects_addr) + 0xA4 * i);
        if (o[0] == 0) continue;
        const long ground = Ground(o);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(o)->height_whole, static_cast<unsigned>(static_cast<short>(ground) + Get(::bof3::Img(at::kQuakeSpriteZ) + 2 * i)));
    }
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const o = Mem(::bof3::Img(Sprite_ObjectsExtra_addr) + 0xA4 * i);
        if (o[0] == 0) continue;
        const long ground = Ground(o);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(o)->height_whole, static_cast<unsigned>(static_cast<short>(ground) + Get(::bof3::Img(at::kQuakeExtraZ) + 2 * i)));
    }
    for (unsigned i = 0; i < 20; ++i) {
        unsigned char* const o = Mem(::bof3::Img(Effect_Objects_addr) + 0x80 * i);
        if (o[0] == 0 || o[5] != 0x17) continue;
        const long ground = Ground(o);
        rv::Store<unsigned>(rv::As<records::EffectObject>(o)->height_whole, static_cast<unsigned>(static_cast<short>(ground) + Get(::bof3::Img(at::kQuakeEffectZ) + 2 * i)));
    }
    const int odd = static_cast<int>(Frame_Counter & 1);
    unsigned char* const sc = Sc();
    MapView_Redraw = 2;
    Put(::bof3::Img(Camera_ShiftY_addr), (odd << 4) - 8);
    Bump(sc[9]);
    for (std::uint32_t row = 0; row < 16; ++row)
        for (std::uint32_t j = 0; j < 14; ++j)
            if (Byte(::bof3::Img(at::kQuakeLift) + row * 15 + j) != 0) return;
    if (sc[9] > 0x5A) Bump(sc[1]);
}

// original 0x4CEA20: the end. The map's polygon codes put back (Walk,
// Opaque: a code that was 0x28 before the start becomes 0 too); the camera
// shift 0; the target's flag 0x40; the done flag; the task freed; the view
// redrawn.
S23_EXPORT void __cdecl Quake_End(void) {
    Walk(&Opaque);
    Put(::bof3::Img(Camera_ShiftY_addr), 0);
    MH_CALL(Battle_SetTargetFlag40)(Mem(::bof3::Img(hat::kTarget))[0]);
    Mem(::bof3::Img(hat::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(hat::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
    MapView_Redraw = 2;
}

// original 0x4CEB40 (slot 40): quads of five dwords after the record's head,
// the cell at x0 = (b1 - 0x80) << 7, z0 = (b0 - 0x80) << 7 (0x4CF4B0(x0 -
// 0x80, z0) first, unread): each one's first vertex projected
// (Gte_LoadVertex, Gte_Rtps, Gte_StoreScreenXY into the next packet +8); on
// screen (x between the bounds at 0x5C4208 / 0x5C4204, y between 0x5C4200 /
// 0x5C41FC, all exclusive): an FT4 with its three other vertices (Gte_Rtpt),
// the fifth dword its texture, committed to 7 / 4 (texture bit 14, by bit
// 30) or 6 / Draw_OtSlot, 0x48 bytes, then the three screen points and the
// depths.
extern "C" void __cdecl MapCell_DrawTexQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (BH_CALL(Area_TestCondition)(Word(record)) == 0) return;
    const U x0 = (b1 - 0x80) << 7, z0 = (b0 - 0x80) << 7;
    CellHeight(static_cast<std::int32_t>(x0 - 0x80), static_cast<std::int32_t>(z0));
    CheckEntries("MapCell_DrawTexQuads", record, 1, 5);
    const unsigned char* p = record + 0x14;
    for (U counter = 1; counter != record[2]; counter += 5, p += 0x14) {
        const SVec first = CellVertex(x0, z0, p - 0x10);
        BH_CALL(Gte_LoadVertex)(reinterpret_cast<const unsigned long*>(&first));
        BH_CALL(Gte_Rtps)();
        unsigned char* const prim = Gfx_PacketNext;
        BH_CALL(Gte_StoreScreenXY)(reinterpret_cast<unsigned long*>(prim + 8));
        // DIV-0041 (UC2): the x bounds moved out by the columns; .rdata's
        // floats stay Capcom's (their code reads them too).
        const float fill = static_cast<float>(Widescreen_Fill());
        if (!(F(prim + 8) > F(At(at::kScreenXLo)) - fill)) continue;
        if (!(F(prim + 8) < F(At(at::kScreenXHi)) + fill)) continue;
        if (!(F(prim + 0xC) > F(At(at::kScreenYLo)))) continue;
        if (!(F(prim + 0xC) < F(At(at::kScreenYHi)))) continue;
        BH_CALL(Gpu_SetPolyFT4)(prim);
        BH_CALL(Gpu_SetShadeTex)(prim, 0);
        SVec v[3];
        for (unsigned k = 0; k < 3; ++k) v[k] = CellVertex(x0, z0, p - 0xC + 4 * k);
        BH_CALL(Gte_LoadVertices3)(reinterpret_cast<const unsigned long*>(v));
        BH_CALL(Prim_SetTexture)(LBattleE6(p), prim, 1);
        BH_CALL(Gte_Rtpt)();
        const U texture = LBattleE6(p);
        unsigned slot;
        if (texture & 0x4000) slot = texture & 0x40000000 ? 7 : 4;
        else slot = texture & 0x40000000 ? 6 : Draw_OtSlot;
        BH_CALL(Gfx_CommitPrim)(slot, 0x48);
        BH_CALL(Gte_StoreScreenXY3)(reinterpret_cast<unsigned long*>(prim + 0x18), reinterpret_cast<unsigned long*>(prim + 0x28),
                                    reinterpret_cast<unsigned long*>(prim + 0x38));
        BH_CALL(Gte_PrimDepths4_10)(prim);
    }
}

// original 0x4CED60 (slot 41): entries of eight dwords after the record's
// head word, the cell as slot 40's: each a draw mode ((its fifth dword >> 24)
// & 3) << 5 | 0x95 committed (0xC to Draw_OtSlot), then a G4 of its four
// vertices (Gte_RotTransPers4 in the order 1, 2, 3, 0 and the depths), semi-
// transparent by the fifth dword's bit 31, the four colours from dwords 5..8,
// committed 0x44. Then draw mode 0x95 committed.
extern "C" void __cdecl MapCell_DrawShadedQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (BH_CALL(Area_TestCondition)(Word(record)) == 0) return;
    const U x0 = (b1 - 0x80) << 7, z0 = (b0 - 0x80) << 7;
    CellHeight(static_cast<std::int32_t>(x0 - 0x80), static_cast<std::int32_t>(z0));
    if (record[2] != 1) {
        CheckEntries("MapCell_DrawShadedQuads", record, 1, 8);
        const unsigned char* vertex = record + 4;
        const unsigned char* mode = record + 0x14;
        for (U counter = 1; counter != record[2]; counter += 8, mode += 0x20) {
            BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, (((LBattleE6(mode) >> 24) & 3) << 5) | 0x95, 0);
            BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
            unsigned char* const prim = Gfx_PacketNext;
            BH_CALL(Gpu_SetPolyG4)(prim);
            SVec v[4];
            for (unsigned k = 0; k < 4; ++k) v[k] = CellVertex(x0, z0, vertex + 4 * k);
            vertex += 0x10;
            long depth = 0, flag = 0;
            BH_AT(Pers4, bof3::addr::Gte_RotTransPers4)(&v[1], &v[2], &v[3], &v[0], prim + 0x18, prim + 0x28, prim + 0x38, prim + 8,
                                                        &depth, &flag);
            BH_CALL(Gte_StoreDepthF4)(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                                      reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
            BH_CALL(Gpu_SetSemiTrans)(prim, LBattleE6(vertex) >> 31);
            for (unsigned k = 0; k < 4; ++k) {
                const U colour = LBattleE6(vertex);
                prim[4 + 0x10 * k] = static_cast<unsigned char>(colour >> 16);
                prim[5 + 0x10 * k] = static_cast<unsigned char>(colour >> 8);
                prim[6 + 0x10 * k] = static_cast<unsigned char>(colour);
                vertex += 4;
            }
            BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x44);
        }
    }
    BH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0x95, 0);
    BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0xC);
}

// original 0x4CEFC0 (slot 42): a centre from dword 1 (x and z bytes at 64 a
// cell unit, doubled, and 0x4CF4B0 plus its low word) and a spin from dword 2
// (three 10-bit rates times Frame_Counter & 0xFFF, times 4); then entries of
// six dwords from dword 3: each under a pushed matrix, the centre rotated and
// translated into the matrix (Gte_RotTrans), its angles (the spin plus four
// times its three 10-bit fields, & 0xFFF) made a matrix times Camera_Matrix,
// set; an FT4 of its four sign-extended 10-bit vertices (the order 1, 2, 3,
// 0), its sixth dword the texture, committed 0x48 to Draw_OtSlot; popped.
extern "C" void __cdecl MapCell_DrawSpinQuads(const unsigned char* record, unsigned b1, unsigned b0) {
    if (BH_CALL(Area_TestCondition)(Word(record)) == 0) return;
    const U d1 = LBattleE6(record + 4);
    SVec centre{};
    centre.x = static_cast<std::int16_t>((static_cast<U>(S8(static_cast<unsigned char>(d1 >> 24))) + ((b1 - 0x80) << 6)) << 1);
    centre.y = static_cast<std::int16_t>((static_cast<U>(S8(static_cast<unsigned char>(d1 >> 16))) + ((b0 - 0x80) << 6)) << 1);
    centre.z = static_cast<std::int16_t>(CellHeight(centre.x, centre.y) + Word(record + 4));
    const U t = Frame_Counter & 0xFFF;
    const U d2 = LBattleE6(record + 8);
    const U spin[3] = {((d2 >> 20) & 0x3FF) * t * 4, ((d2 >> 10) & 0x3FF) * t * 4, (d2 & 0x3FF) * t * 4};
    if (record[2] == 3) return;
    CheckEntries("MapCell_DrawSpinQuads", record, 3, 6);
    const unsigned char* p = record + 0xC;
    U counter = 3;
    do {
        BH_CALL(Gte_PushMatrix)();
        const U e = LBattleE6(p);
        const std::int16_t angles[4] = {static_cast<std::int16_t>((spin[0] + ((e >> 20) & 0x3FF) * 4) & 0xFFF),
                                        static_cast<std::int16_t>((spin[1] + ((e >> 10) & 0x3FF) * 4) & 0xFFF),
                                        static_cast<std::int16_t>((spin[2] + (e & 0x3FF) * 4) & 0xFFF), 0};
        Matrix m{};
        BH_CALL(Gte_RotTrans)(&centre.x, reinterpret_cast<long*>(m.t));
        BH_CALL(Gte_RotMatrix)(angles, m.m);
        BH_CALL(Gte_MulMatrix0)(reinterpret_cast<const short*>(At(::bof3::Img(Camera_Matrix_addr))), m.m, m.m);
        BH_CALL(Gte_SetRotMatrix)(reinterpret_cast<const unsigned long*>(&m));
        BH_CALL(Gte_SetTransMatrix)(reinterpret_cast<const unsigned long*>(&m));
        unsigned char* const prim = Gfx_PacketNext;
        BH_CALL(Gpu_SetPolyFT4)(prim);
        BH_CALL(Gpu_SetShadeTex)(prim, 0);
        SVec q[4];
        for (unsigned k = 0; k < 4; ++k) {
            const U v = LBattleE6(p + 4 + 4 * k);
            q[k].x = Field10((v >> 20) & 0xFFFF);
            q[k].y = Field10((v >> 10) & 0x3FF);
            q[k].z = Field10(v & 0x3FF);
            q[k].pad = 0;
        }
        long depth = 0, flag = 0;
        BH_AT(Pers4, bof3::addr::Gte_RotTransPers4)(&q[1], &q[2], &q[3], &q[0], prim + 0x18, prim + 0x28, prim + 0x38, prim + 8,
                                                    &depth, &flag);
        BH_CALL(Gte_StoreDepthF4)(reinterpret_cast<float*>(prim + 0x20), reinterpret_cast<float*>(prim + 0x30),
                                  reinterpret_cast<float*>(prim + 0x40), reinterpret_cast<float*>(prim + 0x10));
        BH_CALL(Prim_SetTexture)(LBattleE6(p + 0x14), prim, 1);
        BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x48);
        counter += 6;
        p += 0x18;
        BH_CALL(Gte_PopMatrix)();
    } while (counter != record[2]);
}

// original 0x4CF270 (slot 43): a sprite standing on the ground: the point
// (x and z as slot 42's centre into Prim_VertexScratch, y minus half the
// AreaMap_Elevation there) projected (Gte_RotTransPers into MapView_ScreenXY,
// z its answer); on screen (x from the bound at 0x5C4210 to 0x5C420C, y from
// 0x5C4200 to 0x5C41FC, inclusive) and z not 0: the frame by Frame_Counter
// modulo the record's byte 8 against the thresholds from byte 0xA, the
// corners of that frame's dword (four s8 * 1125 / z around the point), an FT4
// with its texture dword, committed 0x48 to Draw_OtSlot. The original divides
// by byte 8 unchecked: ours aborts on 0.
extern "C" void __cdecl MapCell_DrawGroundSprite(const unsigned char* record, unsigned b1, unsigned b0) {
    if (BH_CALL(Area_TestCondition)(Word(record)) == 0) return;
    unsigned char* const point = At(::bof3::Img(Prim_VertexScratch_addr));
    SetWord(point, (static_cast<U>(S8(static_cast<unsigned char>(LBattleE6(record + 4) >> 24))) + ((b1 - 0x80) << 6)) << 1);
    SetWord(point + 2, (static_cast<U>(S8(static_cast<unsigned char>(LBattleE6(record + 4) >> 16))) + ((b0 - 0x80) << 6)) << 1);
    const long height = BH_CALL(AreaMap_Elevation)(static_cast<long>(static_cast<U>(S16BattleE6(Word(point)) + 0x4000) << 9),
                                                   static_cast<long>(static_cast<U>(S16BattleE6(Word(point + 2)) + 0x4000) << 9));
    SetWord(point + 4, static_cast<U>(-(S16BattleE6(static_cast<U>(height)) / 2)));
    long p = 0;
    unsigned char* const screen = At(::bof3::Img(MapView_ScreenXY_addr));
    const long z = BH_AT(Pers, bof3::addr::Gte_RotTransPers)(point, screen, &p);
    const float fill = static_cast<float>(Widescreen_Fill());   // DIV-0041 (UC2): x out by the columns
    if (!(F(screen) >= F(At(at::kSpriteXLo)) - fill)) return;
    if (F(screen) > F(At(at::kSpriteXHi)) + fill) return;
    if (!(F(screen + 4) >= F(At(at::kScreenYLo)))) return;
    if (F(screen + 4) > F(At(at::kScreenYHi))) return;
    if (z == 0) return;
    const unsigned frames = record[8];
    if (frames == 0)
        bof3::Fatal("MapCell_DrawGroundSprite: record 0x%X's frame count byte +8 is 0 - the original divides by it "
                    "(docs/battle_e6.md section 7)",
                    (unsigned)Addr(record));
    const auto tick = static_cast<std::int32_t>(Frame_Counter % frames);
    unsigned frame = 0;
    while (tick >= static_cast<std::int32_t>(record[0xA + frame])) ++frame;
    unsigned char* const prim = Gfx_PacketNext;
    const unsigned head = (record[9] + 5u) >> 2;
    BH_CALL(Gpu_SetPolyFT4)(prim);
    BH_CALL(Gpu_SetShadeTex)(prim, 0);
    BH_CALL(Gte_PrimDepthFlat4_10)(prim);
    const unsigned char* const corners = record + (head + 2 * frame + 2) * 4;
    const auto zz = static_cast<std::int32_t>(z);
    double x = static_cast<double>(F(screen)) - static_cast<double>(S8(corners[0]) * 1125 / zz);
    SetF(prim + 0x28, static_cast<float>(x));
    SetF(prim + 8, static_cast<float>(x));
    x += static_cast<double>(S8(static_cast<unsigned char>(LBattleE6(corners) >> 8)) * 1125 / zz);
    SetF(prim + 0x38, static_cast<float>(x));
    SetF(prim + 0x18, static_cast<float>(x));
    double y = static_cast<double>(F(screen + 4)) - static_cast<double>(S8(static_cast<unsigned char>(LBattleE6(corners) >> 16)) * 1125 / zz);
    SetF(prim + 0x1C, static_cast<float>(y));
    SetF(prim + 0xC, static_cast<float>(y));
    y += static_cast<double>(S8(static_cast<unsigned char>(LBattleE6(corners) >> 24)) * 1125 / zz);
    SetF(prim + 0x3C, static_cast<float>(y));
    SetF(prim + 0x2C, static_cast<float>(y));
    BH_CALL(Prim_SetTexture)(LBattleE6(record + (head + 2 * frame) * 4 + 0xC), prim, 1);
    BH_CALL(Gfx_CommitPrim)(Draw_OtSlot, 0x48);
}

// original 0x4CF4B0 (cdecl; callers read ax): the vertex (x, z), each (v +
// 0x4000) sar 6 less twice the heaved block's first column / row (0x695C2C /
// 0x695C2E) - with Quake's facing bit 0 the axes swap: a from x, b from z;
// without, a from z, b from x. Outside a 0..0x1F, b 0..0x1B: ax 0 (the upper
// half a's). Else by the parities of a and b, from the lift table 0x695A2C
// (index i = (a >> 1) * 15 + (b >> 1)): both even -(s8 [i+1] + s8 [i+0xF]) << 3;
// b odd a even -(s8 [i+0x10] + s8 [i+1]) << 3; b even a odd -(s8 [i+0x10] + s8
// [i+0xF]) << 3; both odd -s8 [i+0x10] << 4.
extern "C" unsigned __cdecl Quake_VertexLift(long x, long z) {
    const U fx = SarRest3g(static_cast<U>(x) + 0x4000u, 6) - (static_cast<std::uint32_t>(MagicS23_QuakeX) << 1);
    const U fz = SarRest3g(static_cast<U>(z) + 0x4000u, 6) - (static_cast<std::uint32_t>(MagicS23_QuakeY) << 1);
    U a, b;   // eax and esi
    if (MagicS23_QuakeFacing & 1) {
        b = fz;
        a = fx;
    } else {
        b = fx;
        a = fz;
    }
    if (I(a) >= 0x20 || I(b) >= 0x1C || I(a) < 0 || I(b) < 0) return a & 0xFFFF0000u;
    const U i = (SarRest3g(a, 1) * 15u) + SarRest3g(b, 1);
    auto t = [&](U offset) { return static_cast<U>(S8Rest3g(At(::bof3::Img(rest_3g::at::kQuakeLast.v) + offset + i)[0])); };
    if (((a | b) & 1) == 0) return (0u - (t(1) + t(0xF))) << 3;
    if ((b & 1) != 0 && (a & 1) == 0) return (0u - (t(0x10) + t(1))) << 3;
    if ((b & 1) == 0 && (a & 1) != 0) return (0u - (t(0x10) + t(0xF))) << 3;
    return (0u - t(0x10)) << 4;
}

void Quake_Inject() {
    BOF3_INJECT(Quake_Task);
    BOF3_INJECT(Quake_Start);
    BOF3_INJECT(Quake_Rumble);
    BOF3_INJECT(Quake_Heave);
    BOF3_INJECT(Quake_End);
    BOF3_INJECT(MapCell_DrawTexQuads);
    BOF3_INJECT(MapCell_DrawShadedQuads);
    BOF3_INJECT(MapCell_DrawSpinQuads);
    BOF3_INJECT(MapCell_DrawGroundSprite);
    BOF3_INJECT(Quake_VertexLift);
}
