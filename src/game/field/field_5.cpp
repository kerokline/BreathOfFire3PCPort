// Moved here by tools/move_funcs.py from event_ops.cpp (docs/source-layout-round.md).
#include "game/field/field_5.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_ops.h"
#include "game/event_ops_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace event_ops;

namespace event_ops {

using move_script::At;

using move_script::Long;

}  // namespace event_ops

namespace {

unsigned char* Object(int n) { return rv::Bytes(&Sprite_Objects[n]); }

unsigned char* Extra(int n) { return rv::Bytes(&Sprite_ObjectsExtra[n]); }

// Field_DirectionSteps' pair and the walk's unit pair for a direction - a
// whole byte into tables of eight, as the original indexes them.
std::uint32_t StepX(unsigned d) { return static_cast<std::uint32_t>(event_ops::Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u)); }

std::uint32_t StepZ(unsigned d) { return static_cast<std::uint32_t>(event_ops::Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u + 4)); }

// The cell step pair: two signed bytes per direction.
signed char CellDX(unsigned d) { return static_cast<signed char>(event_ops::At(::bof3::Img(event_ops::at::kCellDelta))[d * 2u]); }

signed char CellDZ(unsigned d) { return static_cast<signed char>(event_ops::At(::bof3::Img(event_ops::at::kCellDelta))[d * 2u + 1]); }

// The whole part of a 16.16 position, as the original pushes it: the high
// half of the dword, which the callee reads as a short.
short Whole(std::uint32_t v) { return static_cast<short>(v >> 16); }

unsigned short W0() { return at::kScratch.w_00; }

unsigned short W2() { return at::kScratch2; }

void SetW0(unsigned v) { at::kScratch.w_00 = v; }

void SetW2(unsigned v) { at::kScratch2 = v; }

// The leader turned to face a cell found by an even direction's probe.
unsigned char TurnFromEven(unsigned char a, unsigned char b, unsigned char to_ab, unsigned char other) {
    unsigned char* const sc = Sprite_Current;
    sc[8] = (sc[8] == a || sc[8] == b) ? to_ab : other;
    return 1;
}

}  // namespace

// original 0x530A50: the object the leader faces (PSX FUN_801B5148). Even
// directions try the two neighbouring odd ones first - turning to the one
// that answers - then their own; odd ones look (+0x70 + 2) steps ahead, and
// unless that cell is 0x30, one step (for a large sprite on a 0x30 cell the
// far point; for a small one, one step further on).
// As the original has it: the last try's direction and the odd one index
// Field_DirectionSteps whole.
extern "C" unsigned char __cdecl Field_FacingObject(void) {
    const unsigned char* sc = Sprite_Current;
    const unsigned char d = sc[8];
    if (!(d & 1)) {
        const unsigned left = (d - 1u) & 7;
        unsigned char r = g.object_ahead(static_cast<long>(StepX(left) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x)),
                                         static_cast<long>(StepZ(left) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z)), left);
        if (r != 0xFF) {
            Sprite_Current[8] = (Sprite_Current[8] - 1) & 7;
            return r;
        }
        sc = Sprite_Current;
        const unsigned right = (sc[8] + 1u) & 7;
        r = g.object_ahead(static_cast<long>(StepX(right) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x)),
                           static_cast<long>(StepZ(right) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z)), right);
        if (r != 0xFF) {
            Sprite_Current[8] = (Sprite_Current[8] + 1) & 7;
            return r;
        }
        sc = Sprite_Current;
        const unsigned ahead = sc[8];
        return g.object_ahead(static_cast<long>(StepX(ahead) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x)),
                              static_cast<long>(StepZ(ahead) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z)), ahead);
    }
    const std::uint32_t reach = sc[0x70] + 2u;
    std::uint32_t x = StepX(d) * reach + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x);
    std::uint32_t z = StepZ(d) * reach + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z);
    const unsigned char cell = g.byte_at(Whole(x), Whole(z));
    sc = Sprite_Current;
    if (cell != 0x30) {
        const unsigned e = sc[8];
        x = StepX(e) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x);
        z = StepZ(e) + static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z);
    } else if (sc[0x70] == 0) {
        const unsigned e = sc[8];
        x += StepX(e);
        z += StepZ(e);
    }
    return g.object_ahead(static_cast<long>(x), static_cast<long>(z), sc[8]);
}

// original 0x530BF0: the object at (x, z) the leader could reach (PSX
// FUN_801B53C8) - Sprite_ObjectAt with the leader's +0x70; 0xFF for none, for
// a kind-8 object, one whose word +0x88 is 0xFFFF, or one more than 0x100
// above or below. The third argument is not read.
// As the original has it: the index is compared SIGNED with 30, so an
// answer of 0x80 or more reads an object before Sprite_Objects.
extern "C" unsigned char __cdecl Field_ObjectInPath(long x, long z, unsigned) {
    const unsigned char r = g.object_at(x, z, Sprite_Current[0x70]);
    if (r == 0xFF) return 0xFF;
    const auto s = static_cast<signed char>(r);
    const unsigned char* const o = s < 30 ? Object(s) : Extra(s - 30);
    if (o[6] == 8 || rv::As<records::SpriteObject>(o)->trigger_word == 0xFFFF) return 0xFF;
    const int rise = static_cast<short>(rv::As<records::SpriteHead>(o)->height_whole) - static_cast<short>(rv::Current<records::SpriteHead>()->height_whole);
    if (std::abs(rise) > 0x100) return 0xFF;
    return r;
}

// original 0x530C90 (PSX FUN_801B548C): Field_CellAroundNear, or 0x531120
// (Capcom's) for a sprite with +0x70.
extern "C" unsigned char __cdecl Field_CellAround(unsigned cell) {
    return Sprite_Current[0x70] == 0 ? g.cell_around_near(cell) : g.cell_around_large(cell);
}

// original 0x530CC0: a cell of the argument's value next to the leader, for
// a sprite without +0x70 (PSX FUN_801B5D74). Its whole cell (the word 0x903850
// / 0x903852 left at the one found): between cells on both axes, none; on a
// whole cell, the cell ahead's z column then its x row - an even direction
// turns to the diagonal between (1 / 5, 7 / 3), an odd one must already be
// that diagonal - then for an odd direction the two turned 90 degrees
// (turning to it); between cells on x, the two cells of the row ahead (the
// diagonals 7 / 3 look one row back and one on, turning to 1 / 5); on z, the
// same by columns (1 / 5 turning to 7 / 3).
// As the original has it: a match on a row whose direction is odd and not
// the diagonal wanted goes on to the next cell rather than failing; the
// direction indexes the cell table whole.
extern "C" unsigned char __cdecl Field_CellAroundNear(unsigned cell_arg) {
    const unsigned char cell = static_cast<unsigned char>(cell_arg);
    const unsigned char* const sc = Sprite_Current;
    const unsigned short fx = rv::As<records::SpriteHead>(sc)->x_frac;
    if (fx != 0 && rv::As<records::SpriteHead>(sc)->z_frac != 0) return 0;
    const unsigned char d = sc[8];
    const unsigned short x = rv::As<records::SpriteHead>(sc)->x_whole;
    const auto ahead_x = static_cast<unsigned short>(x + CellDX(d));
    const auto ahead_z = static_cast<unsigned short>(rv::As<records::SpriteHead>(sc)->z_whole + CellDZ(d));
    if (fx == 0 && rv::As<records::SpriteHead>(sc)->z_frac == 0) {
        SetW0(x);
        SetW2(ahead_z);
        if (g.byte_at(static_cast<short>(rv::As<records::SpriteHead>(sc)->x_whole), static_cast<short>(ahead_z)) == cell) {
            const unsigned char* const s = Sprite_Current;
            SetW2(ahead_z);
            SetW0(rv::As<records::SpriteHead>(s)->x_whole);
            const unsigned char e = s[8];
            if (!(e & 1)) return TurnFromEven(0, 2, 1, 5);
            return e == 1 || e == 5;
        }
        if (g.byte_at(static_cast<short>(ahead_x), static_cast<short>(rv::Current<records::SpriteHead>()->z_whole)) == cell) {
            const unsigned char* const s = Sprite_Current;
            SetW0(ahead_x);
            SetW2(rv::As<records::SpriteHead>(s)->z_whole);
            const unsigned char e = s[8];
            if (!(e & 1)) return TurnFromEven(0, 6, 7, 3);
            return e == 7 || e == 3;
        }
        {
            const unsigned char* const s = Sprite_Current;
            if (!(s[8] & 1)) return 0;
            const unsigned char t = (s[8] + 2) & 7;
            const auto cx = static_cast<unsigned short>(rv::As<records::SpriteHead>(s)->x_whole + CellDX(t));
            SetW0(cx);
            const auto cz = static_cast<unsigned short>(rv::As<records::SpriteHead>(s)->z_whole + CellDZ(t));
            SetW2(cz);
            if (g.byte_at(static_cast<short>(cx), static_cast<short>(cz)) == cell) {
                Sprite_Current[8] = t;
                return 1;
            }
        }
        const unsigned char* const s = Sprite_Current;
        const unsigned char t = (s[8] - 2) & 7;
        const auto cx = static_cast<unsigned short>(rv::As<records::SpriteHead>(s)->x_whole + CellDX(t));
        SetW0(cx);
        const auto cz = static_cast<unsigned short>(rv::As<records::SpriteHead>(s)->z_whole + CellDZ(t));
        SetW2(cz);
        if (g.byte_at(static_cast<short>(cx), static_cast<short>(cz)) != cell) return 0;
        Sprite_Current[8] = t;
        return 1;
    }
    if (fx != 0) {   // between cells on x
        if (d == 7 || d == 3) {
            SetW0(x);
            SetW2(rv::As<records::SpriteHead>(sc)->z_whole - 1u);
            for (int i = 0;;) {
                if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                    Sprite_Current[8] = 1;
                    return 1;
                }
                SetW0(W0() + 1u);
                if (++i > 1) break;
            }
            const unsigned char* const s = Sprite_Current;
            SetW0(rv::As<records::SpriteHead>(s)->x_whole);
            SetW2(rv::As<records::SpriteHead>(s)->z_whole + 1u);
            for (int i = 0;;) {
                if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                    Sprite_Current[8] = 5;
                    return 1;
                }
                ++i;
                SetW0(W0() + 1u);
                if (i > 1) return 0;
            }
        }
        SetW2(ahead_z);
        SetW0(x);
        for (int i = 0;;) {
            if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                const unsigned char e = Sprite_Current[8];
                if (!(e & 1)) return TurnFromEven(0, 2, 1, 5);
                if (e == 1 || e == 5) return 1;
            }
            SetW0(W0() + 1u);
            if (++i > 1) return 0;
        }
    }
    // between cells on z
    if (d == 1 || d == 5) {
        SetW0(x - 1u);
        SetW2(rv::As<records::SpriteHead>(sc)->z_whole);
        for (int i = 0;;) {
            if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                Sprite_Current[8] = 7;
                return 1;
            }
            SetW2(W2() + 1u);
            if (++i > 1) break;
        }
        const unsigned char* const s = Sprite_Current;
        SetW0(rv::As<records::SpriteHead>(s)->x_whole + 1u);
        SetW2(rv::As<records::SpriteHead>(s)->z_whole);
        for (int i = 0;;) {
            if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
                Sprite_Current[8] = 3;
                return 1;
            }
            ++i;
            SetW2(W2() + 1u);
            if (i > 1) return 0;
        }
    }
    SetW0(ahead_x);
    SetW2(rv::As<records::SpriteHead>(sc)->z_whole);
    for (int i = 0;;) {
        if (g.byte_at(static_cast<short>(W0()), static_cast<short>(W2())) == cell) {
            const unsigned char e = Sprite_Current[8];
            if (!(e & 1)) return TurnFromEven(0, 6, 7, 3);
            if (e == 7 || e == 3) return 1;
        }
        SetW2(W2() + 1u);
        if (++i > 1) return 0;
    }
}

void Field5_Inject() {
    BOF3_INJECT(Field_FacingObject);
    BOF3_INJECT(Field_ObjectInPath);
    BOF3_INJECT(Field_CellAround);
    BOF3_INJECT(Field_CellAroundNear);
}
