// Moved here by tools/move_funcs.py from field_c3.cpp, member_sprites.cpp, mode_states.cpp, rest_1f.cpp (docs/source-layout-round.md).
#include "game/field/field_core.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/field/field_core_callees.h"
#include "game/field_c3.h"
#include "game/field_c3_callees.h"
#include "game/labels.h"
#include "game/member_sprites.h"
#include "game/member_sprites_callees.h"
#include "game/mode_states.h"
#include "game/mode_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_0a.h"
#include "game/rest_1f.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_core::at;

}  // namespace

namespace {

using namespace move_script;

}  // namespace

using namespace mode_states;

namespace {

using U = std::uint32_t;

// move_script::At is named in full here (step B0, analysis/tu_renames.tsv)
using move_script::Long;

using move_script::SetLong;

using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }

unsigned char* F() { return Field_State; }

// Sprite_Current, as rv::Current<records::SpriteHead>(), may be a Sprite_Objects
// record or another family whose first 0x80 bytes are a sprite object's (an
// ObjTrio member - the FieldCore_* states are entry 2 of the members' and the
// leader's state tables, docs/mode_states.md -, the kind-2 object), so only
// the shared head is named (records.h SpriteHead; stage C2).
// Field_State is an ObjTrio record.
unsigned char& B(U address) { return move_script::At(address)[0]; }

U ULong(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

void SetULong(unsigned char* p, U v) { move_script::SetLong(p, static_cast<std::int32_t>(v)); }

short SWord(const unsigned char* p) { return static_cast<short>(move_script::Word(p)); }

template <typename T> U AddressOf(T* p) { return Low32(p); }

// A word's low byte, stored alone as the original's byte `or` does.
unsigned char& LowByte(unsigned short& w) { return *reinterpret_cast<unsigned char*>(&w); }

// jmp / call [table + 4 * Sprite_Current[at]]: the table's `entries` handlers,
// read in place (the fuzz swaps the cells for recorders); a Fatal past them,
// where the original jumps through the dword after - the next table's entry
// (docs/field_c3.md section 6).
void Run(const char* who, unsigned long* table, unsigned entries, unsigned at) {
    const unsigned state = Sprite_Current[at];
    if (state >= entries)
        bof3::Fatal("%s: state byte +%u is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/field_c3.md section 6)",
                    who, at, state, entries, (unsigned)AddressOf(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[state]))();
}

// Sprite_EnsureAnimation with the byte it compares and hands on (the callers
// push whole registers; Sprite_SetAnimationAt reads the animation as a byte).
void Animate(unsigned animation) { SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(animation)); }

unsigned char ScriptTick() { return SH_CALL(Sprite_ScriptTick)(); }

unsigned char StepTick() { return SH_CALL(Field_LeaderStepTick)(); }

long GroundAt(U x, U z) { return SH_CALL(MapView_GroundAt)(static_cast<long>(x), static_cast<long>(z)); }

long SlopeAt(U x, U z, unsigned direction) {
    return SH_CALL(MapView_SlopeAt)(static_cast<long>(x), static_cast<long>(z), direction);
}

// AreaMap_Slope's "sloped" byte, which the callers read straight after MapView_SlopeAt.
bool Sloped() { return field_c3::at::kSloped != 0; }

// The state-1 reset the steps end with: +1 = 1, +2 = +3 = 0 (each through
// Sprite_Current read again, as the originals store).
void BackToState1() {
    rv::Current<records::SpriteHead>()->state = 1;
    S()[2] = 0;
    S()[3] = 0;
}

// The shade fade the steps begin with +5 set: Sprite_ShadeFadeBegin, the three
// shade bytes 0x80 and +0x5C 1, +0 bit 6 cleared, then `state` at +at.
void ShadeBegin(unsigned at, unsigned char state) {
    SH_CALL(Sprite_ShadeFadeBegin)();
    rv::Current<records::SpriteHead>()->shade[0] = 0x80;
    rv::Current<records::SpriteHead>()->shade[1] = 0x80;
    rv::Current<records::SpriteHead>()->shade[2] = 0x80;
    rv::Current<records::SpriteHead>()->draw_mode = 1;
    rv::Current<records::SpriteHead>()->flags = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->flags & 0xBF);
    S()[at] = state;
}

// The jump table's step pair (8 bytes a direction) for direction d, unchecked
// (a byte: the image's table read past its eight, as the original's).
U JumpStepX(unsigned d) { return ULong(move_script::At(::bof3::Img(MoveCmd_F9Steps_addr) + d * 8)); }

U JumpStepZ(unsigned d) { return ULong(move_script::At(::bof3::Img(MoveCmd_F9Steps_addr) + 4 + d * 8)); }

U DirStepX(unsigned d) { return ULong(move_script::At(AddressOf(Field_DirectionSteps) + d * 8)); }

U DirStepZ(unsigned d) { return ULong(move_script::At(AddressOf(Field_DirectionSteps) + 4 + d * 8)); }

// FieldCore_DownSteps[1..4]: +4 = next when the FE2 step answers al.
void DownWait(U callee, unsigned char next) {
    if (SH_WORD(unsigned char (__cdecl*)(), callee)() != 0) S()[4] = next;
}

// MapView_SlopeAt's answer steep (above 0x40, a signed word) with the sloped byte set.
bool Steep(U x, U z, unsigned direction) {
    const long slope = SlopeAt(x, z, direction);
    return Sloped() && static_cast<short>(slope) > 0x40;
}

using member_sprites::g;

std::uint32_t Address(const volatile void* p) { return Low32(p); }

// The member records' views (game/record_views.h): Sprite_Current
// and Field_State are ObjTrio records here.
unsigned char& Cell(unsigned i) { return move_script::At(::bof3::Img(member_sprites::at::kCells.v))[i & 0xFF]; }

// `cdq; sub eax, edx; sar eax, 1`: a signed halving that rounds toward zero.
std::int32_t Half(std::uint32_t v) {
    const std::int32_t s = static_cast<std::int32_t>(v);
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(s) + (s < 0 ? 1u : 0u)) >> 1;
}

// A 16-bit value as the upper half of a 16.16 long.
std::uint32_t High(unsigned v) { return (v & 0xFFFF) << 16; }

}  // namespace

namespace mode_states {

using move_script::At;

using move_script::Long;

}  // namespace mode_states

namespace {

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
mode_states::Handler Entry(std::uint32_t table, int index) {
    return reinterpret_cast<mode_states::Handler>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(mode_states::Long(mode_states::At(table + 4u * static_cast<std::uint32_t>(index))))));
}

using move_script::At;

// kSteps: now Field_DirectionSteps (symbols.toml). Field_DirectionSteps: 8 rows of two longs (x, z)
constexpr ::bof3::image::Va kCellOffsets{0x66971C};   // two signed bytes a direction: the cell ahead

constexpr ::bof3::image::Va kCells{SharedScratch_addr}; // SharedScratch: cell 0 the sloped flag, 1..7 the kinds, 8..0xB the classes

unsigned char* Sc() { return Sprite_Current; }

unsigned char& CellRest1f(unsigned i) { return move_script::At(::bof3::Img(kCells.v))[i & 0xFF]; }

// `cdq; sub eax, edx; sar eax, 1`: a signed halving that rounds toward zero.
long HalfRest1f(U v) {
    const auto s = static_cast<std::int32_t>(v);
    return static_cast<long>(static_cast<std::int32_t>(static_cast<U>(s) + (s < 0 ? 1u : 0u)) >> 1);
}

std::uint16_t W16(unsigned v) { return static_cast<std::uint16_t>(v); }

// Field_CellAheadRaised's diagonal side tests (0x527AAF and 0x527C30 in the
// original): the class of three cells into cell 8 and three single cells into
// 9..0xB; then the answers as the original orders them. `turn_pair` and
// `turn_a2` are the two Field_CellPairTurn directions, `turn_a2_other` the
// one used when cell 0xB is 0xA2; `second` the cell the 0x20 class checks.
struct DiagSide {
    unsigned c0, c1, c2;        // the three cells of the class into cell 8
    unsigned second;            // cell 2 or 4: a 0x2_ there makes 0x20 a stop
    unsigned turn, turn_a2;     // Field_CellPairTurn's `to` for 0x10 / 0xA2 (cell 0xB not 0xA2), and for 0xA2 (cell 0xB 0xA2)
    unsigned to_first, to_second;   // the facing set when the first / second slope probe answers
};

unsigned char DiagEdge(const DiagSide& d, bool on_x, std::uint16_t ax, std::uint16_t az) {
    CellRest1f(8) = SH_CALL(Field_CellClass5)(d.c0, d.c1, d.c2, 0, 0);
    CellRest1f(9) = SH_CALL(Field_CellClass)(d.c0, 0, 0);
    CellRest1f(10) = SH_CALL(Field_CellClass)(d.c2, 0, 0);
    const unsigned char last = SH_CALL(Field_CellClass)(d.c1, 0, 0);
    CellRest1f(0xB) = last;
    unsigned char k = CellRest1f(8);
    if (k == 0xB0) return 6;
    if (k == 0x70) return 7;
    bool pair = k == 0x10;
    if (k == 0x20) {
        if ((CellRest1f(d.second) & 0xF0) == 0x20) return 0;
        pair = true;
    }
    if (pair) {
        if (last == 0x10) return 0;
        if (SH_CALL(Field_CellPairTurn)(9, 0xA, d.turn, 0x10) != 0) return 0;
        if (CellRest1f(0xB) == 0xA2) {
            if (SH_CALL(Field_CellPairTurn)(9, 0xA, d.turn_a2, 0xA2) != 0) return 0;
        } else {
            if (SH_CALL(Field_CellPairTurn)(9, 0xA, d.turn, 0xA2) != 0) return 0;
        }
        k = CellRest1f(8);
    }
    if ((k & 0xF0) == 0xA0) return SH_CALL(Field_CellSlope)(8);
    if (on_x) {
        // the z side: between (ax, ax) and the sprite's zs and zs + 1, then zs - 1, in direction 1
        std::uint16_t zs = rv::As<records::PartyMember>(Sc())->z_whole;
        if (SH_CALL(Field_SlopeBetween)(ax, ax, zs, W16(zs + 1u), 1) != 0) {
            Sc()[8] = static_cast<unsigned char>(d.to_first);
            return 1;
        }
        zs = rv::As<records::PartyMember>(Sc())->z_whole;
        if (SH_CALL(Field_SlopeBetween)(ax, ax, zs, W16(zs - 1u), 1) == 0) return 3;
        Sc()[8] = static_cast<unsigned char>(d.to_second);
        return 1;
    }
    std::uint16_t xs = rv::As<records::PartyMember>(Sc())->x_whole;
    if (SH_CALL(Field_SlopeBetween)(xs, W16(xs + 1u), az, az, 3) != 0) {
        Sc()[8] = static_cast<unsigned char>(d.to_first);
        return 1;
    }
    xs = rv::As<records::PartyMember>(Sc())->x_whole;
    if (SH_CALL(Field_SlopeBetween)(xs, W16(xs - 1u), az, az, 3) == 0) return 3;
    Sc()[8] = static_cast<unsigned char>(d.to_second);
    return 1;
}

}  // namespace

// original 0x525370 (PSX 0x801B79B8): state 2 of both the members' state
// table 0x65F960 and the leader's 0x660918 - a tail jump through
// FieldCore_State2Steps 0x66011C on Sprite_Current +2 (9 entries:
// 0x525390, 0x5257A0, FieldCore_Fade, 0x525960, 0x525CA0, 0x5261E0,
// 0x526490, 0x526A90, 0x526B80). Unchecked, the byte read afresh.
extern "C" void __cdecl FieldCore_State2(void) { Entry(::bof3::Img(FieldCore_State2Steps_addr), Sprite_Current[2])(); }

// original 0x525390 (PSX twin 0x801B79FC): FieldCore_State2Steps[0].
extern "C" void __cdecl FieldCore_ScriptMove(void) {
    F()[0x138] = static_cast<unsigned char>(F()[0x138] | 4);
    Run("FieldCore_ScriptMove", FieldCore_ScriptMoveSteps, FieldCore_ScriptMoveSteps_count, 3);
    records::SpriteHead* const s = rv::Current<records::SpriteHead>();
    if (rv::Bytes(s)[7] & 0x20) return;
    if (s->flags & 0x10)
        SH_CALL(Sprite_ScriptTick)();
    else
        SH_CALL(Sprite_ScriptTickOnce)();
}

// original 0x5253E0 (PSX twin 0x801B7BC0): FieldCore_ScriptMoveSteps[0].
extern "C" void __cdecl FieldCore_ScriptMoveAlign(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    const U x = static_cast<U>(s->x);
    if (((x & 0x7FFF) != 0 || (static_cast<U>(s->z) & 0x7FFF) != 0) && (s->draw_flags & 0x20) == 0) {
        s->x = (x + 0x4800) & 0xFFFF8000u;
        s = rv::Current<records::SpriteHead>();
        s->z = (static_cast<U>(s->z) + 0x4800) & 0xFFFF8000u;
        s = rv::Current<records::SpriteHead>();
        const long slope = SlopeAt(static_cast<U>(s->x), static_cast<U>(s->z), s->direction);
        rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, static_cast<unsigned>(slope));
        Animate(rv::Current<records::SpriteHead>()->direction + 8u);
        s = rv::Current<records::SpriteHead>();
        if ((static_cast<U>(s->x) & 0x7FFF) == 0 && (static_cast<U>(s->z) & 0x7FFF) == 0) Animate(s->direction);
        return;
    }
    SH_CALL(Sprite_ClearSteps)();
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
    SH_CALL(FieldCore_ScriptMoveNext)();
}

// original 0x5254A0: FieldCore_ScriptMoveSteps[1], and the tail jumps of
// FieldCore_ScriptMoveAlign and FieldCore_ScriptMoveStep.
extern "C" void __cdecl FieldCore_ScriptMoveNext(void) {
    unsigned char* f = Field_State;   // held in ecx by the original
    if (f[0x125] != 0) {
        f[0x125] = static_cast<unsigned char>(f[0x125] - 1);
        return;
    }
    const unsigned char step = SH_CALL(MoveScript_Step)(f + 0x124, move_script::At(ULong(f + 0x130)));
    unsigned char animation = step;   // the byte the original keeps at [esp + 8]
    if (step == 0xFF) return;
    f = Field_State;   // esi, from here to the end
    const unsigned char flags = f[0x124];
    if (flags & 8) {
        f[0x124] = static_cast<unsigned char>(flags & 0xF7);
        if ((Field_StatusBits & 0x40) == 0) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xFEFF);
        Animate(rv::Current<records::SpriteHead>()->direction);
        F()[0x138] = static_cast<unsigned char>(F()[0x138] & 0xFB);
        SH_CALL(Field_MemberTimers)();
        rv::Current<records::SpriteHead>()->state = 1;
        S()[2] = 0;
        S()[3] = 0;
        S()[4] = 0;
        rv::Bytes(rv::Current<records::SpriteHead>())[7] = 0;   // a party member here: +7, +6 raw (C2 audit frs F1)
        rv::Bytes(rv::Current<records::SpriteHead>())[6] = 0;
        return;
    }
    records::SpriteHead* s = rv::Current<records::SpriteHead>();   // edx
    if (flags & 0x20) {
        const unsigned char bits = rv::Bytes(s)[7];
        if ((bits & 4) == 0) {
            if ((bits & 8) == 0) {
                s->direction = step;
                Animate(rv::Current<records::SpriteHead>()->direction);
                s = rv::Current<records::SpriteHead>();
            }
            rv::Bytes(s)[3] = 3;
            return;
        }
    }
    if (flags & 2) {
        s->direction = step;
        SH_CALL(Sprite_ShadeFadeBegin)();
        rv::Current<records::SpriteHead>()->shade[0] = 0xC0;
        rv::Current<records::SpriteHead>()->shade[1] = 0xC0;
        rv::Current<records::SpriteHead>()->shade[2] = 0xC0;
        rv::Current<records::SpriteHead>()->draw_mode = 1;
        Animate(rv::Current<records::SpriteHead>()->direction);
        S()[3] = 4;
        return;
    }
    if (flags & 4) {
        s->direction = step;
        SH_CALL(Sprite_ShadeFadeBegin)();
        rv::Current<records::SpriteHead>()->draw_mode = 1;
        Animate(rv::Current<records::SpriteHead>()->direction);
        S()[3] = 5;
        return;
    }
    if (s->step_frames != 0) {
        if (f[0x12B] != 0) {
            f[0x12B] = static_cast<unsigned char>(f[0x12B] - 1);
            s = rv::Current<records::SpriteHead>();
        }
        s->direction = step;
        S()[3] = 2;
        animation = static_cast<unsigned char>(step + 8);
        rv::Current<records::SpriteHead>()->step_frames = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->step_frames - 1);
        StepTick();
    } else {
        s->direction = step;
        S()[3] = 1;
    }
    if ((F()[0x124] & 0x40) == 0) Animate(animation);
}

// original 0x5256A0 (PSX twin 0x801B7F14): FieldCore_ScriptMoveSteps[2].
extern "C" void __cdecl FieldCore_ScriptMoveStep(void) {
    records::SpriteHead* const s = rv::Current<records::SpriteHead>();   // held in ecx
    if (s->step_frames != 0) {
        s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
        StepTick();
        return;
    }
    unsigned char* const f = Field_State;
    if (f[0x12B] == 0) {
        SH_CALL(FieldCore_ScriptMoveNext)();
        return;
    }
    const U rise = static_cast<U>(s->step_y);
    f[0x12B] = static_cast<unsigned char>(f[0x12B] - 1);
    SH_CALL(Field_JumpStart)();
    if (F()[0x124] & 0x80) rv::Current<records::SpriteHead>()->step_y = rise;
    rv::Current<records::SpriteHead>()->step_frames = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->step_frames - 1);
    StepTick();
}

// original 0x525710 (PSX twin 0x801B7FD4): FieldCore_ScriptMoveSteps[3].
extern "C" void __cdecl FieldCore_ScriptMoveWait(void) {
    if (Field_Request == 2) return;
    F()[0x124] = static_cast<unsigned char>(F()[0x124] & 0xDF);
    S()[3] = 0;
}

// original 0x525740 (PSX twin 0x801B801C): FieldCore_ScriptMoveSteps[4].
extern "C" void __cdecl FieldCore_ScriptMoveShadeLower(void) {
    if (SH_CALL(Sprite_ShadeLower)(4) == 0) return;
    F()[0x124] = static_cast<unsigned char>(F()[0x124] & 0xFD);
    S()[3] = 1;
}

// original 0x525770 (PSX twin 0x801B8074): FieldCore_ScriptMoveSteps[5].
extern "C" void __cdecl FieldCore_ScriptMoveShadeFade(void) {
    if (SH_CALL(Sprite_ShadeFadeStep)(4) == 0) return;
    F()[0x124] = static_cast<unsigned char>(F()[0x124] & 0xFB);
    S()[3] = 1;
}

// original 0x5257A0 (PSX twin 0x801B80CC): FieldCore_State2Steps[1].
extern "C" void __cdecl FieldCore_Attached(void) {
    if (rv::Current<records::SpriteHead>()->step_frames == 0) {
        rv::Current<records::SpriteHead>()->draw_flags = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->draw_flags | 0x20);
        // Sprite_ObjectsExtra[+0x18] (unchecked, as the original), each field
        // through +0x18 read again
        static const unsigned kFields[] = {0x34, 0x38, 0x3C, 0x64, 0x68, 0x6C};
        for (const unsigned field : kFields) {
            records::SpriteHead* const s = rv::Current<records::SpriteHead>();
            SetULong(rv::Bytes(s) + field, ULong(move_script::At(::bof3::Img(Sprite_ObjectsExtra_addr) + 0x34 + (field - 0x34) + static_cast<U>(s->work_18) * 0xA4u)));
        }
        long offset[3];
        SH_CALL(MoveCmd_AttachOffset)(offset, S()[0x1C]);
        rv::Current<records::SpriteHead>()->x = static_cast<U>(rv::Current<records::SpriteHead>()->x) + static_cast<U>(offset[0]);
        rv::Current<records::SpriteHead>()->z = static_cast<U>(rv::Current<records::SpriteHead>()->z) + static_cast<U>(offset[1]);
        rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, rv::As<records::SpriteHead>(S())->height_whole + static_cast<unsigned>(offset[2]));
    }
    SH_CALL(FieldCore_ScriptMove)();
}

// original 0x5258B0 (PSX 0x801B8264): sub-state 2 of state 2 - a tail jump
// through FieldCore_FadeSteps 0x660158 on Sprite_Current +3 (0
// FieldCore_FadeBegin, 1 FieldCore_FadeStep). Unchecked: 2 would run
// 0x525960's table, the words after.
extern "C" void __cdecl FieldCore_Fade(void) { Entry(::bof3::Img(FieldCore_FadeSteps_addr), Sprite_Current[3])(); }

// original 0x5258D0 (PSX 0x801B82A8): the fade's first frame -
// Sprite_ShadeFadeBegin, then the shade bytes +0x5D..+0x5F 0x80 and +0x5C 1,
// Field_State +0x137 = 7, +0 bit 6 cleared and +3 = 1, all through
// Sprite_Current read afresh at each store.
extern "C" void __cdecl FieldCore_FadeBegin(void) {
    mode_states::g.shade_begin();
    Sprite_Current[0x5D] = 0x80;
    Sprite_Current[0x5E] = 0x80;
    Sprite_Current[0x5F] = 0x80;
    Sprite_Current[0x5C] = 1;
    Field_State[0x137] = 7;
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    Sprite_Current[3] = 1;
}

// original 0x525920 (PSX 0x801B833C): each later frame of the fade -
// Sprite_ShadeFadeStep(8), and once it answers non-zero, Field_State +0x137
// = 0 and the object back to state 1, sub-states 0 (+1 = 1, +2 = +3 = 0).
extern "C" void __cdecl FieldCore_FadeStep(void) {
    if (mode_states::g.shade_step(8) == 0) return;
    Field_State[0x137] = 0;
    Sprite_Current[1] = 1;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// original 0x525960 (PSX twin 0x801B83A8): FieldCore_State2Steps[3].
extern "C" void __cdecl FieldCore_Hop(void) { Run("FieldCore_Hop", FieldCore_HopSteps, FieldCore_HopSteps_count, 3); }

// original 0x525980 (PSX twin 0x801B83EC): FieldCore_HopSteps[0].
extern "C" void __cdecl FieldCore_HopBegin(void) {
    Animate(rv::Current<records::SpriteHead>()->direction == 3 ? 0x38 : 0x39);
    F()[0x128] = 3;
    S()[3] = 1;
}

// original 0x5259D0 (PSX twin 0x801B8444): FieldCore_HopSteps[1].
extern "C" void __cdecl FieldCore_HopLaunch(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    const unsigned pace = F()[0x128];
    const U speed = Field_MoveSpeeds[pace];   // a byte index: the image's six read past, as the original's
    if (speed == 0)
        bof3::Fatal("FieldCore_HopLaunch: speed 0 (Field_MoveSpeeds[%u]) - the original divides 0x20 by it and faults "
                    "(docs/field_c3.md section 6)",
                    pace);
    rv::Current<records::SpriteHead>()->step_frames = static_cast<unsigned char>(0x20u / speed);
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    U n = (s->reach & 0xFF) + 1;
    s->step_x = JumpStepX(s->direction) * n * speed;
    s = rv::Current<records::SpriteHead>();
    n = (s->reach & 0xFF) + 1;
    s->step_z = JumpStepZ(s->direction) * n * speed;
    s = rv::Current<records::SpriteHead>();
    s->step_y = ((s->reach & 0xFF) + 1) << 6;
    s = rv::Current<records::SpriteHead>();
    SetULong(rv::Bytes(s) + 0x20, (0xFFFFFFFFu - (s->reach & 0xFF)) << 3);
    s = rv::Current<records::SpriteHead>();
    if (rv::Bytes(s)[5] == 0) {
        Field_Kind2X = static_cast<long>(static_cast<U>(s->step_x) * s->step_frames + static_cast<U>(s->x));
        Field_Kind2Z = static_cast<long>(static_cast<U>(s->step_z) * s->step_frames + static_cast<U>(s->z));
        MoveScript_F3Divisor = static_cast<short>(speed << 3);
    }
    Animate(s->direction == 3 ? 0x3A : 0x3B);
    SH_CALL(Sound_PlayEffect)(0x104);
    rv::Current<records::SpriteHead>()->step_frames = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->step_frames - 1);
    s = rv::Current<records::SpriteHead>();
    s->step_y = static_cast<U>(s->step_y) + ULong(rv::Bytes(s) + 0x20);
    StepTick();
    S()[3] = 2;
}

// original 0x525B20 (PSX twin 0x801B8650): FieldCore_HopSteps[2].
extern "C" void __cdecl FieldCore_HopRise(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if (s->step_frames == 0) {
        s->step_x = 0;
        rv::Current<records::SpriteHead>()->step_z = 0;
        S()[3] = 3;
    } else {
        s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
    }
    s = rv::Current<records::SpriteHead>();
    s->step_y = static_cast<U>(s->step_y) + ULong(rv::Bytes(s) + 0x20);
    StepTick();
    s = rv::Current<records::SpriteHead>();
    if (rv::Bytes(s)[5] == 0) SH_CALL(MapView_SetElevation)(SWord(rv::Bytes(s) + 0x3E));
    ScriptTick();
}

// original 0x525B90 (PSX twin 0x801B86EC): FieldCore_HopSteps[3].
extern "C" void __cdecl FieldCore_HopFall(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    s->step_y = static_cast<U>(s->step_y) + ULong(rv::Bytes(s) + 0x20);
    StepTick();
    s = rv::Current<records::SpriteHead>();
    const long ground = GroundAt(static_cast<U>(s->x), static_cast<U>(s->z));
    s = rv::Current<records::SpriteHead>();   // ecx
    if (static_cast<short>(ground) > SWord(rv::Bytes(s) + 0x3E)) {
        s->height_whole = static_cast<unsigned>(ground);
        s = rv::Current<records::SpriteHead>();
        if (s->step_y < -0x80)
            Animate(static_cast<unsigned char>((s->direction >> 1) + 0x34));
        else
            Animate(s->direction == 3 ? 0x38 : 0x39);
        rv::Current<records::SpriteHead>()->step_y = 0;
        SetULong(S() + 0x20, 0);
        S()[3] = 4;
        s = rv::Current<records::SpriteHead>();
    }
    if (rv::Bytes(s)[5] == 0 && ((static_cast<unsigned>(Field_ScriptFlags2) | Field_ScriptFlags) & 8) == 0)
        SH_CALL(MapView_SetElevation)(SWord(rv::Bytes(s) + 0x3E));
    ScriptTick();
}

// original 0x525C50 (PSX twin 0x801B8814): FieldCore_HopSteps[4].
extern "C" void __cdecl FieldCore_HopLand(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return;
    if (SH_CALL(Field_TileD0)() != 0) return;
    Animate(rv::Current<records::SpriteHead>()->direction);
    F()[0x137] = 0;
    BackToState1();
}

// original 0x525CA0: FieldCore_State2Steps[4].
extern "C" void __cdecl FieldCore_Vertical(void) {
    F()[0x137] = 3;
    Run("FieldCore_Vertical", FieldCore_VerticalSteps, FieldCore_VerticalSteps_count, 3);
}

// original 0x525CC0: FieldCore_VerticalSteps[0].
extern "C" void __cdecl FieldCore_Up(void) { Run("FieldCore_Up", FieldCore_UpSteps, FieldCore_UpSteps_count, 4); }

// original 0x525CE0: FieldCore_UpSteps[0].
extern "C" void __cdecl FieldCore_UpBegin(void) {
    SH_AT(void (__cdecl*)(), field_c3::at::kUpFace.v)();
    S()[4] = static_cast<unsigned char>(S()[4] + 1);
}

// original 0x525CF0: FieldCore_UpSteps[1].
extern "C" void __cdecl FieldCore_UpOut(void) {
    rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, rv::As<records::SpriteHead>(S())->height_whole + 0x10u);
    records::SpriteHead* const s = rv::Current<records::SpriteHead>();
    const long ground = GroundAt(static_cast<U>(s->x), static_cast<U>(s->z));
    if (static_cast<int>(SWord(S() + 0x3E)) > static_cast<int>(static_cast<short>(ground)) + 0x200) Field_Request = 5;
    ScriptTick();
}

// original 0x525D40: FieldCore_UpSteps[2].
extern "C" void __cdecl FieldCore_UpArrive(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    const long ground = GroundAt(static_cast<U>(s->x), static_cast<U>(s->z));
    rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, static_cast<unsigned>(ground) - 0x200u);
    s = rv::Current<records::SpriteHead>();
    s->x = static_cast<U>(s->x) + ((0u - JumpStepX(s->direction) * ((s->reach & 0xFF) + 1)) << 5);
    s = rv::Current<records::SpriteHead>();
    s->z = static_cast<U>(s->z) + ((0u - JumpStepZ(s->direction) * ((s->reach & 0xFF) + 1)) << 5);
    Animate(rv::Current<records::SpriteHead>()->direction == 7 ? 0x3C : 0x3D);
    s = rv::Current<records::SpriteHead>();
    if (rv::Bytes(s)[5] != 0) {
        ShadeBegin(4, 3);
        return;
    }
    rv::Bytes(s)[4] = 4;
}

// original 0x525E30: FieldCore_UpSteps[4].
extern "C" void __cdecl FieldCore_UpIn(void) {
    rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, rv::As<records::SpriteHead>(S())->height_whole + 0x10u);
    const long ground = GroundAt(EventLeader_PendingX, EventLeader_PendingZ);
    const int stop = static_cast<int>(static_cast<short>(ground)) -
                     static_cast<int>(SWord(move_script::At(::bof3::Img(field_c3::at::kTargetOffsets) + rv::As<records::PartyMember>(F())->char_id * 2u)));
    if (static_cast<int>(SWord(S() + 0x3E)) >= stop) {
        SH_AT(void (__cdecl*)(), field_c3::at::kUpLanded.v)();
        S()[4] = static_cast<unsigned char>(S()[4] + 1);
        return;
    }
    ScriptTick();
}

// original 0x525E90: FieldCore_UpSteps[5].
extern "C" void __cdecl FieldCore_UpWait5(void) {
    if (SH_AT(unsigned char (__cdecl*)(), field_c3::at::kUpWait5.v)() != 0) S()[4] = static_cast<unsigned char>(S()[4] + 1);
}

// original 0x525EB0: FieldCore_UpSteps[6].
extern "C" void __cdecl FieldCore_UpWait6(void) {
    if (SH_AT(unsigned char (__cdecl*)(), field_c3::at::kUpWait6.v)() != 0) S()[4] = static_cast<unsigned char>(S()[4] + 1);
}

// original 0x525ED0: FieldCore_UpSteps[7].
extern "C" void __cdecl FieldCore_UpEnd(void) {
    if (SH_AT(unsigned char (__cdecl*)(), field_c3::at::kUpWait7.v)() == 0) return;
    if (S()[5] == 0) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF7);
}

// original 0x525EF0: FieldCore_VerticalSteps[1].
extern "C" void __cdecl FieldCore_Down(void) { Run("FieldCore_Down", FieldCore_DownSteps, FieldCore_DownSteps_count, 4); }

// original 0x525F10: FieldCore_DownSteps[0].
extern "C" void __cdecl FieldCore_DownBegin(void) {
    SH_AT(void (__cdecl*)(), field_c3::at::kDownJumpSetUp.v)();
    unsigned char* const s = S();
    LowByte(Field_ScriptFlags2) = static_cast<unsigned char>(LowByte(Field_ScriptFlags2) | 8);   // or byte ptr [0x905BA4], 8
    s[4] = 1;
}

// originals 0x525F30, 0x525F50, 0x525F70, 0x525F90: FieldCore_DownSteps[1..4].
extern "C" void __cdecl FieldCore_DownWait1(void) { DownWait(field_c3::at::kDownWait1.v, 2); }

extern "C" void __cdecl FieldCore_DownWait2(void) { DownWait(field_c3::at::kDownWait2.v, 3); }

extern "C" void __cdecl FieldCore_DownWait3(void) { DownWait(field_c3::at::kDownWait3.v, 4); }

extern "C" void __cdecl FieldCore_DownWait4(void) { DownWait(field_c3::at::kDownWait4.v, 5); }

// original 0x525FB0: FieldCore_DownSteps[5].
extern "C" void __cdecl FieldCore_DownOut(void) {
    rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, rv::As<records::SpriteHead>(S())->height_whole - 0x10u);
    const long ground = GroundAt(EventLeader_PendingX, EventLeader_PendingZ);
    if (static_cast<int>(SWord(S() + 0x3E)) < static_cast<int>(static_cast<short>(ground)) - 0x200) Field_Request = 5;
    ScriptTick();
}

// original 0x526000: FieldCore_DownSteps[6].
extern "C" void __cdecl FieldCore_DownArrive(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    const long ground = GroundAt(static_cast<U>(s->x), static_cast<U>(s->z));
    rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, static_cast<unsigned>(ground) + 0x200u);
    Animate(rv::Current<records::SpriteHead>()->direction == 3 ? 0x3C : 0x3D);
    s = rv::Current<records::SpriteHead>();
    if (rv::Bytes(s)[5] != 0) {
        ShadeBegin(4, 7);
        return;
    }
    rv::Bytes(s)[4] = 8;
}

// original 0x5260A0: FieldCore_DownSteps[8].
extern "C" void __cdecl FieldCore_DownIn(void) {
    rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, rv::As<records::SpriteHead>(S())->height_whole - 0x10u);
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    const long ground = GroundAt(static_cast<U>(s->x), static_cast<U>(s->z));
    s = rv::Current<records::SpriteHead>();
    if (SWord(rv::Bytes(s) + 0x3E) < static_cast<short>(ground)) {
        const long again = GroundAt(static_cast<U>(s->x), static_cast<U>(s->z));
        rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, static_cast<unsigned>(again));
        rv::Current<records::SpriteHead>()->step_frames = 2;
        S()[0xA] = 0;
        Animate(rv::Current<records::SpriteHead>()->direction == 3 ? 7 : 1);
        S()[4] = 9;
    }
    ScriptTick();
}

// original 0x526120: FieldCore_DownSteps[9].
extern "C" void __cdecl FieldCore_DownLand(void) {
    rv::Current<records::SpriteHead>()->step_frames = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->step_frames - 1);
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if (s->step_frames != 0) return;
    rv::Bytes(s)[0xA] = static_cast<unsigned char>(rv::Bytes(s)[0xA] + 1);
    s = rv::Current<records::SpriteHead>();
    Animate(s->direction == 3 ? static_cast<unsigned char>(7 - rv::Bytes(s)[0xA]) : static_cast<unsigned char>(rv::Bytes(s)[0xA] + 1));
    s = rv::Current<records::SpriteHead>();
    if (rv::Bytes(s)[0xA] != 4) {
        s->step_frames = 2;
        return;
    }
    if (rv::Bytes(s)[5] == 0) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF7);
    s->step_frames = 0;
    S()[0xA] = 0;
    BackToState1();
    S()[4] = 0;
}

// original 0x5261C0: FieldCore_UpSteps[3] and FieldCore_DownSteps[7].
extern "C" void __cdecl FieldCore_VerticalShade(void) {
    if (SH_CALL(Sprite_ShadeFadeStep)(8) != 0) S()[4] = static_cast<unsigned char>(S()[4] + 1);
}

// original 0x5261E0: FieldCore_State2Steps[5].
extern "C" void __cdecl FieldCore_JumpExit(void) {
    F()[0x137] = 2;
    Run("FieldCore_JumpExit", FieldCore_JumpExitSteps, FieldCore_JumpExitSteps_count, 3);
}

// original 0x526200: FieldCore_JumpExitSteps[0].
extern "C" void __cdecl FieldCore_JumpExitBegin(void) {
    records::SpriteHead* const s = rv::Current<records::SpriteHead>();
    LowByte(Field_ScriptFlags2) = static_cast<unsigned char>(LowByte(Field_ScriptFlags2) | 8);   // or byte ptr [0x905BA4], 8
    Animate(s->direction + 8u);
    rv::Current<records::SpriteHead>()->step_frames = 0;
    S()[0xA] = 0;
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// original 0x526240: FieldCore_JumpExitSteps[1].
extern "C" void __cdecl FieldCore_JumpExitOut(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if (s->step_frames == 0) {
        rv::Bytes(s)[0xA] = static_cast<unsigned char>(rv::Bytes(s)[0xA] + 1);
        if (S()[0xA] == 3) Field_Request = 5;
        SH_CALL(Field_JumpStart)();
        s = rv::Current<records::SpriteHead>();
    }
    s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
    StepTick();
    ScriptTick();
}

// original 0x526280: FieldCore_JumpExitSteps[2].
extern "C" void __cdecl FieldCore_JumpExitArrive(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if (rv::Bytes(s)[0x70] != 0) {
        s->x = static_cast<U>(s->x) + ((0u - JumpStepX(s->direction) * 3u) << 5);
        s = rv::Current<records::SpriteHead>();
        s->z = static_cast<U>(s->z) + ((0u - JumpStepZ(s->direction) * 3u) << 5);
        S()[0xA] = 7;
    } else {
        s->x = static_cast<U>(s->x) + ((0u - JumpStepX(s->direction)) << 6);
        s = rv::Current<records::SpriteHead>();
        s->z = static_cast<U>(s->z) + ((0u - JumpStepZ(s->direction)) << 6);
        S()[0xA] = 5;
    }
    rv::Current<records::SpriteHead>()->step_frames = 0;
    s = rv::Current<records::SpriteHead>();
    const long ground = GroundAt(static_cast<U>(s->x), static_cast<U>(s->z));
    rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, static_cast<unsigned>(ground));
    F()[0x128] = 2;
    Animate(rv::Current<records::SpriteHead>()->direction + 8u);
    s = rv::Current<records::SpriteHead>();
    if (rv::Bytes(s)[5] != 0) {
        ShadeBegin(3, 3);
        return;
    }
    rv::Bytes(s)[3] = 4;
}

// original 0x5263C0: FieldCore_JumpExitSteps[3].
extern "C" void __cdecl FieldCore_JumpExitShade(void) {
    if (SH_CALL(Sprite_ShadeFadeStep)(8) != 0) S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// original 0x5263E0: FieldCore_JumpExitSteps[4].
extern "C" void __cdecl FieldCore_JumpExitIn(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if (s->step_frames == 0) {
        rv::Bytes(s)[0xA] = static_cast<unsigned char>(rv::Bytes(s)[0xA] - 1);
        s = rv::Current<records::SpriteHead>();
        if (rv::Bytes(s)[0xA] == 0) {
            if (rv::Bytes(s)[5] == 0) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF7);
            F()[0x128] = 3;
            rv::Current<records::SpriteHead>()->step_x = 0;
            rv::Current<records::SpriteHead>()->step_z = 0;
            F()[0x137] = 0;
            BackToState1();
            rv::Current<records::SpriteHead>()->step_frames = 0;
            S()[0xA] = 0;
            Animate(rv::Current<records::SpriteHead>()->direction);
            return;
        }
        SH_CALL(Field_JumpStart)();
        s = rv::Current<records::SpriteHead>();
    }
    s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
    StepTick();
    ScriptTick();
}

// original 0x526490: FieldCore_State2Steps[6].
extern "C" void __cdecl FieldCore_TileD0(void) {
    F()[0x137] = 5;
    Run("FieldCore_TileD0", FieldCore_TileD0Steps, FieldCore_TileD0Steps_count, 3);
    ScriptTick();
}

// original 0x5264C0: FieldCore_TileD0Steps[0].
extern "C" void __cdecl FieldCore_TileD0Begin(void) {
    rv::Current<records::SpriteHead>()->step_frames = 0;
    records::SpriteHead* const s = rv::Current<records::SpriteHead>();
    rv::Bytes(s)[0xB] = s->direction;
    F()[0x128] = 4;
    S()[3] = 1;
}

// original 0x5264F0: FieldCore_TileD0Steps[1].
extern "C" void __cdecl FieldCore_TileD0Move(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if (s->step_frames == 0) {
        if (SH_CALL(AreaMap_CellsNone)(s->x, s->z, s->reach & 0xFF, 0xD0, 0) != 0) {
            // off the cells: the pace back to 3, the facing kept, state 1
            F()[0x128] = 3;
            s = rv::Current<records::SpriteHead>();
            s->direction = rv::Bytes(s)[0xB];
            Animate(rv::Current<records::SpriteHead>()->direction);
            F()[0x137] = 0;
            BackToState1();
            return;
        }
        SH_CALL(FieldCore_TileD0Exit)();
        if (SH_CALL(FieldCore_TileD0Slope)() != 0) {
            Animate(rv::Current<records::SpriteHead>()->direction);
            S()[2] = 3;
            S()[3] = 0;
            return;
        }
        if (SH_CALL(Field_CellAhead)() == 0) {
            rv::Current<records::SpriteHead>()->direction = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->direction + 1);
            SH_CALL(Field_CellAhead)();
        }
        switch (ULong(reinterpret_cast<const unsigned char*>(&Input_Held)) & 0xF000) {
        case 0x1000: S()[0xB] = 0; break;
        case 0x2000: S()[0xB] = 2; break;
        case 0x3000: S()[0xB] = 1; break;
        case 0x4000: S()[0xB] = 4; break;
        case 0x6000: S()[0xB] = 3; break;
        case 0x8000: S()[0xB] = 6; break;
        case 0x9000: S()[0xB] = 7; break;
        case 0xC000: S()[0xB] = 5; break;
        default: break;
        }
        Animate(S()[0xB] + 8u);
        SH_CALL(Field_JumpStart)();
        s = rv::Current<records::SpriteHead>();
    }
    s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
    StepTick();
    ScriptTick();
}

// original 0x5266B0: called by FieldCore_TileD0Move - the way off the cells.
extern "C" void __cdecl FieldCore_TileD0Exit(void) {
    for (unsigned i = 0; i < 8; ++i) {
        records::SpriteHead* const s = rv::Current<records::SpriteHead>();
        const unsigned d = (rv::Bytes(s)[8] + i) & 7u;
        const U n = (s->reach & 0xFF) + 1;
        const U x = DirStepX(d) * n + static_cast<U>(s->x);
        const U z = DirStepZ(d) * n + static_cast<U>(s->z);
        if (SH_CALL(Field_WayBlocked)(static_cast<long>(x), static_cast<long>(z), rv::Bytes(s)[0x70], SWord(rv::Bytes(s) + 0x3E)) != 0) continue;
        if (SH_CALL(AreaMap_CellsNone)(static_cast<long>(x), static_cast<long>(z), rv::Current<records::SpriteHead>()->reach & 0xFF, 0xD0, 0) != 0) {
            rv::Current<records::SpriteHead>()->direction = static_cast<unsigned char>(d);
            return;
        }
    }
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    unsigned char best_direction = s->direction;
    short best = SWord(rv::Bytes(s) + 0x3E);
    for (unsigned i = 0; i < 4; ++i) {
        if (i != 0) s = rv::Current<records::SpriteHead>();
        const unsigned d = i * 2 + 1;
        const U x = DirStepX(d) + static_cast<U>(s->x);
        const U z = DirStepZ(d) + static_cast<U>(s->z);
        short ground = 0;
        if (SH_CALL(FieldCore_TileD0Probe)(static_cast<long>(x), static_cast<long>(z), &ground, 1) != 0 && best > ground) {
            best = ground;
            best_direction = static_cast<unsigned char>(d);
        }
    }
    rv::Current<records::SpriteHead>()->direction = best_direction;
}

// original 0x526820: called by FieldCore_TileD0Exit - the ground at (x, z), or
// al 0 for a steep slope above the object.
extern "C" unsigned char __cdecl FieldCore_TileD0Probe(long x, long z, short* ground, unsigned direction) {
    const long slope = SlopeAt(static_cast<U>(x), static_cast<U>(z), direction);
    *ground = static_cast<short>(slope);
    if (Sloped() && static_cast<short>(slope) > 0x40) {
        const long height = GroundAt(static_cast<U>(x), static_cast<U>(z));
        if (SWord(S() + 0x3E) < static_cast<short>(height)) return 0;
    }
    *ground = static_cast<short>(GroundAt(static_cast<U>(x), static_cast<U>(z)));
    return 1;
}

// original 0x526880: called by FieldCore_TileD0Move - a steep edge ahead.
extern "C" unsigned char __cdecl FieldCore_TileD0Slope(void) {
    records::SpriteHead* const s = rv::Current<records::SpriteHead>();
    const unsigned char direction = s->direction;
    if (rv::Bytes(s)[0x70] == 0) {
        if (direction == 3) {
            if (s->x_frac != 0) return 0;
            U z = static_cast<U>(s->z) & 0xFFFF0000u;
            const U x = static_cast<U>(s->x) + 0x8000;
            if (!Steep(x, z, 3)) return 0;
            if (rv::As<records::SpriteHead>(S())->z_frac == 0) return 1;
            z += 0x8000;
            return Steep(x, z, 3) ? 1 : 0;
        }
        if (direction == 5) {
            if (s->z_frac != 0) return 0;
            const U z = static_cast<U>(s->z) + 0x8000;
            U x = static_cast<U>(s->x) & 0xFFFF0000u;
            if (!Steep(x, z, 5)) return 0;
            if (rv::As<records::SpriteHead>(S())->x_frac == 0) return 1;
            x += 0x8000;
            return Steep(x, z, 5) ? 1 : 0;
        }
        return 0;
    }
    if (direction == 3) {
        if (s->x_frac == 0) return 0;
        U z = static_cast<U>(s->z) & 0xFFFF0000u;
        const U x = static_cast<U>(s->x) + 0x10000;
        if (!Steep(x, z, 3)) return 0;
        z += 0x8000;
        if (!Steep(x, z, 3)) return 0;
        if (rv::As<records::SpriteHead>(S())->z_frac != 0) return 1;
        z -= 0x10000;
        return Steep(x, z, 3) ? 1 : 0;
    }
    if (direction == 5) {
        if (s->z_frac == 0) return 0;
        const U z = static_cast<U>(s->z) + 0x10000;
        U x = static_cast<U>(s->x) & 0xFFFF0000u;
        if (!Steep(x, z, 5)) return 0;
        x += 0x8000;
        if (!Steep(x, z, 5)) return 0;
        if (rv::As<records::SpriteHead>(S())->x_frac != 0) return 1;
        x -= 0x10000;
        return Steep(x, z, 3) ? 1 : 0;   // direction 3 here, as the original pushes
    }
    return 0;
}

// original 0x526A90: FieldCore_State2Steps[7].
extern "C" void __cdecl FieldCore_Fall(void) {
    F()[0x137] = 6;
    Run("FieldCore_Fall", FieldCore_FallSteps, FieldCore_FallSteps_count, 3);
}

// original 0x526AB0: FieldCore_FallSteps[0].
extern "C" void __cdecl FieldCore_FallBegin(void) {
    SH_CALL(Sound_PlayEffect)(0x107);
    unsigned char* const s = S();
    SH_CALL(Area_LinkAt)(rv::As<records::SpriteHead>(s)->x_whole, rv::As<records::SpriteHead>(s)->z_whole);
    rv::Current<records::SpriteHead>()->step_y = 0;
    Animate(rv::Current<records::SpriteHead>()->direction);
    S()[3] = 1;
}

// original 0x526B00: FieldCore_FallSteps[1].
extern "C" void __cdecl FieldCore_FallSpin(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    s->step_y = static_cast<U>(s->step_y) - 8;
    s = rv::Current<records::SpriteHead>();
    s->height_whole = s->height_whole + move_script::Word(rv::Bytes(s) + 0x14);
    s = rv::Current<records::SpriteHead>();
    s->direction = static_cast<unsigned char>((s->direction + 1) & 7);
    Animate(rv::Current<records::SpriteHead>()->direction);
    s = rv::Current<records::SpriteHead>();
    const long ground = GroundAt(static_cast<U>(s->x), static_cast<U>(s->z));
    if (static_cast<int>(static_cast<short>(ground)) - static_cast<int>(SWord(S() + 0x3E)) > 0x100 && Field_Request != 5) {
        B(::bof3::Img(field_c3::at::kScriptFlags2High)) = static_cast<unsigned char>(B(::bof3::Img(field_c3::at::kScriptFlags2High)) | 2);
        Field_Request = 5;
    }
}

// original 0x526B80: FieldCore_State2Steps[8].
extern "C" void __cdecl FieldCore_Recoil(void) { Run("FieldCore_Recoil", FieldCore_RecoilSteps, FieldCore_RecoilSteps_count, 3); }

// original 0x526BA0 (PSX twin 0x801BA208): FieldCore_RecoilSteps[0].
extern "C" void __cdecl FieldCore_RecoilBegin(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if (s->draw_mode != 0) {
        SH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + rv::Bytes(s)[5] * 64u)), 0);
        rv::Current<records::SpriteHead>()->flags = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->flags & 0xDF);
        rv::Current<records::SpriteHead>()->draw_mode = 0;
        rv::Current<records::SpriteHead>()->shade[0] = 0;
        rv::Current<records::SpriteHead>()->shade[1] = 0;
        rv::Current<records::SpriteHead>()->shade[2] = 0;
        s = rv::Current<records::SpriteHead>();
    }
    s->flags = static_cast<unsigned char>(s->flags & 0xBF);
    s = rv::Current<records::SpriteHead>();
    const U x = static_cast<U>(s->x);
    if ((x & 0x7FFF) != 0 || (static_cast<U>(s->z) & 0x7FFF) != 0) {
        s->x = (x + 0x4800) & 0xFFFF8000u;
        s = rv::Current<records::SpriteHead>();
        s->z = (static_cast<U>(s->z) + 0x4800) & 0xFFFF8000u;
        s = rv::Current<records::SpriteHead>();
        const long slope = SlopeAt(static_cast<U>(s->x), static_cast<U>(s->z), s->direction);
        rv::Store<unsigned>(rv::As<records::SpriteHead>(S())->height_whole, static_cast<unsigned>(slope));
        s = rv::Current<records::SpriteHead>();
        if (rv::Bytes(s)[5] == 0) {
            Field_Kind2X = s->x;
            Field_Kind2Z = s->z;
            SH_CALL(Field_ViewReset)();
            s = rv::Current<records::SpriteHead>();
        }
    }
    const unsigned char facing = s->direction;
    s->direction = static_cast<unsigned char>(facing ^ 4);
    const unsigned char ahead = SH_CALL(Field_CellAhead)();
    if (ahead != 1 && ahead != 3) {
        SH_CALL(Sprite_ClearSteps)();
    } else if (SH_CALL(Field_LeaderPushObjects)() != 0) {
        rv::Current<records::SpriteHead>()->step_frames = 0;
        SH_CALL(Sprite_ClearSteps)();
    } else {
        SH_CALL(Field_JumpStart)();
        rv::Current<records::SpriteHead>()->step_frames = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->step_frames - 1);
        SH_CALL(Sprite_ApplyVelocity)();
    }
    SH_AT(void (__cdecl*)(unsigned char), field_c3::at::kRecoilFace.v)(S()[0xB]);
    rv::Current<records::SpriteHead>()->direction = facing;
    S()[0xA] = 8;
    Animate(rv::Current<records::SpriteHead>()->direction);
    SH_CALL(Sound_PlayEffect)(0x108);
    S()[3] = static_cast<unsigned char>(S()[3] + 1);
}

// original 0x526D30 (PSX twin 0x801BA470): FieldCore_RecoilSteps[1].
extern "C" void __cdecl FieldCore_RecoilBlink(void) {
    records::SpriteHead* s = rv::Current<records::SpriteHead>();
    if (s->step_frames != 0) {
        SH_CALL(Sprite_ApplyVelocity)();
        rv::Current<records::SpriteHead>()->step_frames = static_cast<unsigned char>(rv::Current<records::SpriteHead>()->step_frames - 1);
        s = rv::Current<records::SpriteHead>();
        if (s->step_frames == 0) {
            SH_CALL(Sprite_ClearSteps)();
            s = rv::Current<records::SpriteHead>();
        }
    }
    if (rv::Bytes(s)[0xA] != 0) {
        rv::Bytes(s)[0xA] = static_cast<unsigned char>(rv::Bytes(s)[0xA] - 1);
        s = rv::Current<records::SpriteHead>();
        if (rv::Bytes(s)[0xA] & 1)
            s->flags = static_cast<unsigned char>(s->flags & 0xBF);
        else
            s->flags = static_cast<unsigned char>(s->flags | 0x40);
        return;
    }
    s->flags = static_cast<unsigned char>(s->flags & 0xBF);
    BackToState1();
}

// original 0x526DB0 (PSX 0x801BA590): the cell ahead, by Field_CellAheadRaised for a raised
// sprite (+0x70) and Field_CellAheadFlat otherwise (tail jumps). Also called by
// the leader (0x52E160) and 0x526530 / 0x526C80.
extern "C" unsigned char __cdecl Field_CellAhead(void) {
    if (Sprite_Current[0x70] != 0) return member_sprites::g.cell_ahead_raised();
    return member_sprites::g.cell_ahead_flat();
}

// original 0x526DD0 (PSX: the flat half of 0x801BA590's pair): what the cell
// ahead of a sprite on the ground means for a step - 1 go on (turned along a
// slope or a wall), 3 nothing in the way that this knows, 0 stop, 6 or 7 a
// state change (Field_CellSlope's 2, 4 or 5 on a slope), after turning the
// sprite (+8) where the cells say.
//
// As the original has it: mid-cell on both axes (the 16.16 fractions both
// non-zero) is 1 at once; the classes are kept in cells 8..10 and read back
// from memory where the original reads memory; the half-cell points are the
// original's rounding-toward-zero halvings of 16.16 values that may wrap.
extern "C" unsigned char __cdecl Field_CellAheadFlat(void) {
    if (Field_ScriptFlags & 0x400) return 1;
    const records::PartyMember* c = rv::Current<records::PartyMember>();
    const unsigned char facing = c->direction;
    if (c->x_frac != 0 && c->z_frac != 0) return 1;
    const unsigned char* const offsets = move_script::At(::bof3::Img(member_sprites::at::kCellOffsets.v)) + facing * 2u;
    const std::uint16_t ahead_z = static_cast<std::uint16_t>(static_cast<signed char>(offsets[1]) + c->z_whole);
    const std::uint16_t ahead_x = static_cast<std::uint16_t>(static_cast<signed char>(offsets[0]) + c->x_whole);
    member_sprites::g.read_cells(ahead_x, ahead_z);
    c = rv::Current<records::PartyMember>();
    const unsigned char dir = c->direction;
    if (dir & 1) {
        // Diagonal.
        const std::uint16_t fx = c->x_frac;
        if (fx == 0 && c->z_frac == 0) {
            const unsigned char k = member_sprites::g.cell_class(1, 0, 0);
            Cell(8) = k;
            if (k == 0xB0) return 6;
            if (k == 0x10) return 0;
            if ((k & 0xF0) != 0xA0) return 3;
            return member_sprites::g.cell_slope(8);
        }
        if (fx != 0 && c->z_frac == 0) {
            if (dir & 2) return 3;
            const unsigned char k = member_sprites::g.cell_class(2, 4, 0);
            Cell(8) = k;
            if (k == 0xB0) return 6;
            if (k == 0x70) return 7;
            if (k == 0x20) return 0;
            if (k == 0x10) {
                if (member_sprites::g.cell_pair_turn(2, 4, 3, 0xFF)) return 0;
                if (member_sprites::g.cell_pair_turn(2, 4, 3, 0x70)) return 0;
                if (member_sprites::g.cell_pair_turn(2, 4, 7, 0xA0)) return 0;
                if (member_sprites::g.cell_pair_turn(2, 4, 7, 0xA1)) return 0;
                if (member_sprites::g.cell_pair_turn(2, 4, 7, 0xA2)) return 0;
                return member_sprites::g.cell_pair_turn(2, 4, 7, 0xA3) == 0 ? 1 : 0;
            }
            if ((k & 0xF0) == 0xA0) return member_sprites::g.cell_slope(8);
            const unsigned char* const s = Sprite_Current;
            const long ax = Half(static_cast<std::uint32_t>(static_cast<short>(rv::As<records::PartyMember>(s)->x_whole)) * 0x20000u + 0x10000u);
            const long az = static_cast<long>(High(ahead_z));
            const long r = member_sprites::g.object_at(ax, az, 1);
            if (Cell(0) == 0) return 3;
            if (static_cast<short>(r) <= 0x40) return 3;
            rv::Current<records::PartyMember>()->direction = 7;
            return 1;
        }
        // z mid-cell (or both whole with x whole and z not): the z side.
        if (!(dir & 2)) return 3;
        const unsigned char k = member_sprites::g.cell_class(3, 5, 0);
        Cell(8) = k;
        if (k == 0xB0) return 6;
        if (k == 0x70) return 7;
        if (k == 0x20) return 0;
        if (k == 0x10) {
            if (member_sprites::g.cell_pair_turn(3, 5, 5, 0xFF)) return 0;
            if (member_sprites::g.cell_pair_turn(3, 5, 5, 0x70)) return 0;
            if (member_sprites::g.cell_pair_turn(3, 5, 1, 0xA0)) return 0;
            if (member_sprites::g.cell_pair_turn(3, 5, 1, 0xA1)) return 0;
            if (member_sprites::g.cell_pair_turn(3, 5, 1, 0xA2)) return 0;
            return member_sprites::g.cell_pair_turn(3, 5, 1, 0xA3) != 0 ? 0 : 3;
        }
        if ((k & 0xF0) == 0xA0) return member_sprites::g.cell_slope(8);
        const unsigned char* const s = Sprite_Current;
        const long az = Half(static_cast<std::uint32_t>(static_cast<short>(rv::As<records::PartyMember>(s)->z_whole)) * 0x20000u + 0x10000u);
        const long ax = static_cast<long>(High(ahead_x));
        const long r = member_sprites::g.object_at(ax, az, 1);
        if (Cell(0) == 0) return 3;
        if (static_cast<short>(r) <= 0x40) return 3;
        rv::Current<records::PartyMember>()->direction = 1;
        return 1;
    }

    // Straight.
    const std::uint16_t fx = c->x_frac;
    if (fx == 0 && c->z_frac == 0) {
        // On a cell's corner: ahead, and either side.
        Cell(8) = member_sprites::g.cell_class(1, 2, 3);
        Cell(9) = member_sprites::g.cell_class(2, 0, 0);
        const unsigned char third = member_sprites::g.cell_class(3, 0, 0);
        Cell(10) = third;
        const unsigned char first = Cell(8);
        if (first == 0xB0) return 6;
        if (first == 0x10 || first == 0x20) {
            if (Cell(9) == 0x10) {
                if (third == 0x10) return 0;
                member_sprites::g.turn_unless(2, 4, 3);
            } else if (third == 0x10) {
                member_sprites::g.turn_unless(4, 6, 5);
            } else {
                records::PartyMember* const s = rv::Current<records::PartyMember>();
                s->direction = static_cast<unsigned char>((s->direction + 1) & 7);
            }
        }
        records::PartyMember* const s = rv::Current<records::PartyMember>();
        if (!(Field_InputFlags & 1)) {
            const unsigned char side_x = Cell(9) & 0xF0;
            if (side_x == 0xA0 && (s->direction == 1 || s->direction == 5)) return member_sprites::g.cell_slope(9);
            const unsigned char side_z = Cell(10) & 0xF0;
            if (side_z == 0xA0 && (s->direction == 3 || s->direction == 7)) return member_sprites::g.cell_slope(0xA);
            if (side_x == 0xA0) {
                s->direction = facing;
                member_sprites::g.turn_unless(4, 6, 5);
                return member_sprites::g.cell_slope(9);
            }
            if (side_z == 0xA0) {
                s->direction = facing;
                member_sprites::g.turn_unless(2, 4, 3);
                return member_sprites::g.cell_slope(0xA);
            }
        }
        if (s->direction & 1) return 3;
        {
            const long ax = Half((static_cast<std::uint32_t>(static_cast<short>(s->x_whole)) +
                                  static_cast<std::uint32_t>(static_cast<short>(ahead_x))) << 16);
            const long r = member_sprites::g.object_at(ax, static_cast<long>(High(ahead_z)), 3);
            if (Cell(0) != 0 && static_cast<short>(r) > 0x40) {
                member_sprites::g.turn_unless(4, 6, 5);
                return 1;
            }
        }
        const records::PartyMember* const t = rv::Current<records::PartyMember>();
        if (t->direction & 1) return 3;
        const long az = Half((static_cast<std::uint32_t>(static_cast<short>(t->z_whole)) +
                              static_cast<std::uint32_t>(static_cast<short>(ahead_z))) << 16);
        const long r = member_sprites::g.object_at(static_cast<long>(High(ahead_x)), az, 1);
        if (Cell(0) == 0) return 3;
        if (static_cast<short>(r) <= 0x40) return 3;
        member_sprites::g.turn_unless(2, 4, 3);
        return 1;
    }
    if (fx != 0 && c->z_frac == 0) {
        // On an x edge: ahead on x.
        unsigned char k = member_sprites::g.cell_class(2, 4, 0);
        Cell(8) = k;
        if (k == 0x70) {
            member_sprites::g.turn_unless(4, 6, 5);
            return 7;
        }
        if (k == 0x10 || k == 0x20) {
            member_sprites::g.turn_unless(2, 4, 3);
            k = Cell(8);
        }
        if ((k & 0xF0) != 0xA0) return 3;
        if (!(rv::Current<records::PartyMember>()->direction & 1)) member_sprites::g.turn_unless(4, 6, 5);
        return member_sprites::g.cell_slope(8);
    }
    // On a z edge: ahead on z.
    unsigned char k = member_sprites::g.cell_class(3, 5, 0);
    Cell(8) = k;
    if (k == 0x70) {
        member_sprites::g.turn_unless(2, 4, 3);
        return 7;
    }
    if (k == 0xB0) return 6;
    if (k == 0x10 || k == 0x20) {
        member_sprites::g.turn_unless(4, 6, 5);
        k = Cell(8);
    }
    if ((k & 0xF0) != 0xA0) return 3;
    if (!(rv::Current<records::PartyMember>()->direction & 1)) member_sprites::g.turn_unless(2, 4, 3);
    return member_sprites::g.cell_slope(8);
}

// original 0x527470 (PSX 0x801BA5D8's helper): the class of up to three cells
// (indices a, b, c into the cells, the list ending at the first 0) - 0xB0 all
// 0xB0 (or no cell), 0x10 one of them 0x70 (with two, both 0x70 is 0x70) or
// 0xFF, a slope 0xA_ by the pairs' rules, 0x20 two different 0x2_, else 0.
//
// As the original has it: a list of one takes a 0xA_ cell as it is; with
// three, only the first two positions start the 0x2_ search and the search
// ends after the first 0x2_ found; the facing's bit 0 is read once.
extern "C" unsigned char __cdecl Field_CellClass(unsigned a, unsigned b, unsigned c) {
    unsigned char k[3] = {static_cast<unsigned char>(a), static_cast<unsigned char>(b), static_cast<unsigned char>(c)};
    int n = 0;
    for (int i = 0; i < 3 && k[i] != 0; ++i) ++n;
    for (int i = 0; i < n; ++i) k[i] = Cell(k[i]);
    unsigned char first = k[0], second = k[1];
    if (n == 2 && first == 0x70 && second == first) return 0x70;
    for (int i = 0; i < n; ++i)
        if (k[i] == 0x70) {
            k[i] = 0xFF;
            second = k[1];
            first = k[0];
        }
    for (int i = 0; i < n; ++i)
        if (k[i] == 0xFF) return 0x10;
    {
        int i = 0;
        while (i < n && k[i] == 0xB0) ++i;
        if (i == n) return 0xB0;
    }
    if (n & 1) {
        const unsigned char odd = rv::Current<records::PartyMember>()->direction & 1;
        for (int i = 0; i < n; ++i) {
            if (!odd && k[i] == 0xA2) return 0x10;
            if (k[i] == 0xA3) return 0x10;
            if (k[i] == 0xA0) return 0x10;
        }
        if (n == 1) return (first & 0xF0) == 0xA0 ? first : 0;
    } else if ((first & 0xF0) == 0xA0 || (second & 0xF0) == 0xA0) {
        const unsigned char high = first & 0xF0;
        if (first == 0xA2) {
            if (second == first) return (rv::Current<records::PartyMember>()->direction & 1) ? 0xA2 : 0x10;
        } else if (first == 0xA3) {
            if (second == first) return 0xA3;
        }
        if (high != 0xA0) return 0x10;
        return (second & 0xF0) == high ? 0xA1 : 0x10;
    } else if (n <= 1) {
        return 0;
    }
    for (int i = 0; i < n - 1; ++i) {
        if ((k[i] & 0xF0) != 0x20) continue;
        for (int j = 0; j < n; ++j)
            if ((k[j] & 0xF0) == 0x20 && k[i] != k[j]) return 0x20;
        i = n;
    }
    return 0;
}

// original 0x527640 (0x766 bytes): what the cell ahead of a raised sprite
// (+0x70 set) means for a step - Field_CellAhead tail-jumps here, as it does
// to Field_CellAheadFlat for a grounded one. 1 at once when
// Field_ScriptFlags has bit 10, or when the sprite stands on a cell's corner
// (both 16.16 fractions 0: the opposite of the flat half's test). Otherwise
// the cell ahead from Field_CellOffsets' two signed bytes for +8 (an offset
// of 1 counts 2; the cell stepped from is the sprite's own, one on where the
// offset is 1), Field_ReadCellsRaised; then by the facing (re-read) and the
// fractions the classes into cells 8..0xB and the answer - 0 stop, 1 go on
// (turned), 3 nothing known in the way, 6 / 7 a state change, or
// Field_CellSlope's - each step as docs/rest_1f.md section 1.3 lists it.
extern "C" unsigned char __cdecl Field_CellAheadRaised(void) {
    if (Field_ScriptFlags & 0x400) return 1;
    const unsigned char* c = Sc();
    const unsigned char facing = c[8];
    if (rv::As<records::PartyMember>(c)->x_frac == 0 && rv::As<records::PartyMember>(c)->z_frac == 0) return 1;
    const unsigned char* const offsets = move_script::At(::bof3::Img(kCellOffsets.v)) + static_cast<unsigned>(facing) * 2u;
    const auto ox = static_cast<signed char>(offsets[0]);
    const auto oz = static_cast<signed char>(offsets[1]);
    const std::uint16_t dx = ox == 1 ? 2 : static_cast<std::uint16_t>(static_cast<short>(ox));
    const std::uint16_t dz = oz == 1 ? 2 : static_cast<std::uint16_t>(static_cast<short>(oz));
    const std::uint16_t xs = rv::As<records::PartyMember>(c)->x_whole, zs = rv::As<records::PartyMember>(c)->z_whole;
    const std::uint16_t ax = W16(xs + dx), az = W16(zs + dz);
    SH_CALL(Field_ReadCellsRaised)(ax, az, W16(xs + (ox == 1 ? 1u : 0u)), W16(zs + (oz == 1 ? 1u : 0u)));
    c = Sc();
    const unsigned char dir = c[8];
    if ((dir & 1) == 0) {
        // Straight.
        if (rv::As<records::PartyMember>(c)->x_frac != 0) {
            if (rv::As<records::PartyMember>(c)->z_frac != 0) {
                // Mid-cell on both axes: ahead and the two sides.
                CellRest1f(8) = SH_CALL(Field_CellClass5)(1, 2, 3, 4, 5);
                CellRest1f(9) = SH_CALL(Field_CellClass5)(4, 5, 0, 0, 0);
                const unsigned char side_z = SH_CALL(Field_CellClass5)(2, 3, 0, 0, 0);
                CellRest1f(10) = side_z;
                if (CellRest1f(8) == 0xB0) return 6;
                if (CellRest1f(9) == 0x70) {
                    SH_CALL(Field_TurnUnless)(4, 6, 5);
                    return 7;
                }
                if (side_z == 0x70) {
                    SH_CALL(Field_TurnUnless)(2, 4, 3);
                    return 7;
                }
                if (SH_CALL(Field_CornerTurn)() != 0) return 0;
                const unsigned char high9 = CellRest1f(9) & 0xF0;
                unsigned char* const s = Sc();
                if (high9 == 0xA0 && (s[8] == 1 || s[8] == 5)) return SH_CALL(Field_CellSlope)(9);
                const unsigned char high10 = CellRest1f(10) & 0xF0;
                if (high10 == 0xA0 && (s[8] == 3 || s[8] == 7)) return SH_CALL(Field_CellSlope)(0xA);
                if (high9 == 0xA0) {
                    s[8] = facing;
                    SH_CALL(Field_TurnUnless)(4, 6, 5);
                    return SH_CALL(Field_CellSlope)(9);
                }
                if (high10 == 0xA0) {
                    s[8] = facing;
                    SH_CALL(Field_TurnUnless)(2, 4, 3);
                    return SH_CALL(Field_CellSlope)(0xA);
                }
                SH_CALL(Field_RaisedEdgeTurns)(ax, az);
                return 3;
            }
            // x mid-cell, z on a cell edge: ahead on x.
            unsigned char k = SH_CALL(Field_CellClass5)(3, 2, 6, 0, 0);
            CellRest1f(8) = k;
            if (k == 0xB0) return 6;
            if (k == 0x10 || k == 0x20) {
                SH_CALL(Field_TurnUnless)(4, 6, 5);
                k = CellRest1f(8);
            }
            if ((k & 0xF0) == 0xA0) {
                if ((Sc()[8] & 1) == 0) SH_CALL(Field_TurnUnless)(2, 4, 3);
                return SH_CALL(Field_CellSlope)(8);
            }
            if (Sc()[8] & 1) return 3;
            std::uint16_t z0 = rv::As<records::PartyMember>(Sc())->z_whole;
            if (SH_CALL(Field_SlopeBetween)(ax, ax, z0, W16(z0 + 1u), 1) != 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
            if (Sc()[8] & 1) return 3;
            z0 = rv::As<records::PartyMember>(Sc())->z_whole;
            if (SH_CALL(Field_SlopeBetween)(ax, ax, z0, W16(z0 - 1u), 1) != 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
            return 3;
        }
        // x on a cell edge, z mid-cell: ahead on z.
        unsigned char k = SH_CALL(Field_CellClass5)(5, 4, 7, 0, 0);
        CellRest1f(8) = k;
        if (k == 0xB0) return 6;
        if (k == 0x10 || k == 0x20) {
            SH_CALL(Field_TurnUnless)(2, 4, 3);
            k = CellRest1f(8);
        }
        const unsigned char now = Sc()[8];
        if ((k & 0xF0) == 0xA0) {
            if ((now & 1) == 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
            return SH_CALL(Field_CellSlope)(8);
        }
        if (now & 1) return 3;
        std::uint16_t x0 = rv::As<records::PartyMember>(Sc())->x_whole;
        if (SH_CALL(Field_SlopeBetween)(x0, W16(x0 + 1u), az, az, 3) != 0) SH_CALL(Field_TurnUnless)(2, 4, 3);
        if (Sc()[8] & 1) return 3;
        x0 = rv::As<records::PartyMember>(Sc())->x_whole;
        if (SH_CALL(Field_SlopeBetween)(x0, W16(x0 - 1u), az, az, 3) == 0) return 3;
        SH_CALL(Field_TurnUnless)(2, 4, 3);
        return 3;
    }

    // Diagonal.
    if (rv::As<records::PartyMember>(c)->x_frac != 0) {
        if (rv::As<records::PartyMember>(c)->z_frac != 0) {
            const unsigned char k = (dir & 2) ? SH_CALL(Field_CellClass5)(2, 3, 0, 0, 0) : SH_CALL(Field_CellClass5)(4, 5, 0, 0, 0);
            CellRest1f(8) = k;
            if (k == 0xB0) return 6;
            if (k == 0x70) return 7;
            if (k == 0x10 || k == 0x20) return 0;
            if ((k & 0xF0) == 0xA0) return SH_CALL(Field_CellSlope)(8);
            const unsigned char* const s = Sc();
            unsigned char r;
            if (s[8] & 2) {
                const std::uint16_t z0 = rv::As<records::PartyMember>(s)->z_whole;
                r = SH_CALL(Field_SlopeBetween)(ax, ax, z0, W16(z0 + 1u), 1);
            } else {
                const std::uint16_t x0 = rv::As<records::PartyMember>(s)->x_whole;
                r = SH_CALL(Field_SlopeBetween)(x0, W16(x0 + 1u), az, az, 3);
            }
            return r != 0 ? 0 : 3;
        }
        // x mid-cell, z on an edge: only facings with bit 1 (3, 7) look.
        if ((dir & 2) == 0) return 3;
        static const DiagSide kXSide = {3, 2, 6, 2, 1, 5, 1, 5};
        return DiagEdge(kXSide, true, ax, az);
    }
    // x on an edge, z mid-cell: only facings without bit 1 (1, 5) look.
    if (dir & 2) return 3;
    static const DiagSide kZSide = {5, 4, 7, 4, 7, 3, 7, 3};
    return DiagEdge(kZSide, false, ax, az);
}

// original 0x527DB0 (0x23B bytes): the class of up to five cells (indices a..e
// into the cells, the list ending at the first 0), Field_CellClass's rules
// for a raised sprite: two 0x70 alone are 0x70; any 0x70 counts as 0xFF; any
// 0xFF is 0x10; all 0xB0 (or no cell) 0xB0. Two cells: 0xA0 in either, or
// 0xA2 in either with an even facing, 0x10; 0xA3 with 0xA3 0xA3, with any
// other 0x10; a 0xA_ in either, the first when both are equal, else 0x10.
// Three: 0xA3 anywhere 0x10, 0xA2 anywhere with an even facing 0x10; a 0xA_
// anywhere: the first and third read 0xA1 for 0xA0 (the third in the list
// itself), the answer the first when all three agree, else 0x10. One, four or
// five: any 0xA_ is 0x10. Otherwise 0x20 when a 0x2_ cell differs from a
// later 0x2_ (only the first 0x2_ is compared), else 0.
extern "C" unsigned char __cdecl Field_CellClass5(unsigned a, unsigned b, unsigned c, unsigned d, unsigned e) {
    unsigned char k[5] = {static_cast<unsigned char>(a), static_cast<unsigned char>(b), static_cast<unsigned char>(c),
                          static_cast<unsigned char>(d), static_cast<unsigned char>(e)};
    unsigned n = 0;
    while (n < 5 && k[n] != 0) ++n;
    for (unsigned i = 0; i < n; ++i) k[i] = CellRest1f(k[i]);
    if (n == 2 && k[0] == 0x70 && k[1] == 0x70) return 0x70;
    for (unsigned i = 0; i < n; ++i)
        if (k[i] == 0x70) k[i] = 0xFF;
    for (unsigned i = 0; i < n; ++i)
        if (k[i] == 0xFF) return 0x10;
    {
        unsigned i = 0;
        while (i < n && k[i] == 0xB0) ++i;
        if (i == n) return 0xB0;
    }
    if (n == 2) {
        const unsigned char k0 = k[0], k1 = k[1];
        if (k0 == 0xA0 || k1 == 0xA0) return 0x10;
        if ((k0 == 0xA2 || k1 == 0xA2) && (Sc()[8] & 1) == 0) return 0x10;
        if (k0 == 0xA3) return k1 == 0xA3 ? 0xA3 : 0x10;
        if ((k0 & 0xF0) == 0xA0 || (k1 & 0xF0) == 0xA0) return k0 == k1 ? k0 : 0x10;
    } else if (n == 3) {
        const unsigned char k0 = k[0], k1 = k[1], k2 = k[2];
        if (k0 == 0xA3 || k1 == 0xA3 || k2 == 0xA3) return 0x10;
        if ((k0 == 0xA2 || k1 == 0xA2 || k2 == 0xA2) && (Sc()[8] & 1) == 0) return 0x10;
        if ((k0 & 0xF0) == 0xA0 || (k1 & 0xF0) == 0xA0 || (k2 & 0xF0) == 0xA0) {
            const unsigned char first = k0 == 0xA0 ? 0xA1 : k0;
            if (k2 == 0xA0) k[2] = 0xA1;
            return first == k1 && first == k[2] ? first : 0x10;
        }
    } else {
        for (unsigned i = 0; i < n; ++i)
            if ((k[i] & 0xF0) == 0xA0) return 0x10;
    }
    for (unsigned i = 0; i < n; ++i) {
        if ((k[i] & 0xF0) != 0x20) continue;
        for (unsigned j = i + 1; j < n; ++j)
            if ((k[j] & 0xF0) == 0x20 && k[i] != k[j]) return 0x20;
        return 0;
    }
    return 0;
}

// original 0x527FF0 (0x74 bytes): with cell 8 a 0x10 or 0x20 class - cells 9
// and 10 both 0x10 / 0x20: 1; only cell 9: Field_TurnUnless(2, 4, 3); only cell
// 10: Field_TurnUnless(4, 6, 5); neither: +8 one eighth on (& 7). 0 but for
// the first case; 0 when cell 8 is neither.
extern "C" unsigned char __cdecl Field_CornerTurn(void) {
    const unsigned char c8 = CellRest1f(8);
    if (c8 != 0x10 && c8 != 0x20) return 0;
    const unsigned char c9 = CellRest1f(9), c10 = CellRest1f(10);
    const bool side9 = c9 == 0x10 || c9 == 0x20, side10 = c10 == 0x10 || c10 == 0x20;
    if (side9 && side10) return 1;
    if (side9) {
        SH_CALL(Field_TurnUnless)(2, 4, 3);
        return 0;
    }
    if (side10) {
        SH_CALL(Field_TurnUnless)(4, 6, 5);
        return 0;
    }
    unsigned char* const s = Sc();
    s[8] = static_cast<unsigned char>((s[8] + 1) & 7);
    return 0;
}

// original 0x528070: a slope cell's answer by its low nibble - 2 for 1 or 0xF,
// 5 for 2, else 4.
extern "C" unsigned char __cdecl Field_CellSlope(unsigned cell) {
    const unsigned low = Cell(cell) & 0xF;
    if (low == 1 || low == 0xF) return 2;
    if (low == 2) return 5;
    return 4;
}

// original 0x5280A0 (0x4F bytes): MapView_SlopeAt at the point half way
// between two cells - x the sum of the words x0 and x1 (sign-extended) as
// 16.16, halved toward zero, z likewise - in `direction` (passed whole); 1
// when the sloped flag (cell 0) is set and the answer's low word, signed, is
// above 0x40, else 0.
extern "C" unsigned char __cdecl Field_SlopeBetween(unsigned x0, unsigned x1, unsigned z0, unsigned z1, unsigned direction) {
    const long x = HalfRest1f(static_cast<U>(static_cast<int>(static_cast<short>(x0)) + static_cast<int>(static_cast<short>(x1))) << 16);
    const long z = HalfRest1f(static_cast<U>(static_cast<int>(static_cast<short>(z0)) + static_cast<int>(static_cast<short>(z1))) << 16);
    const long r = SH_CALL(MapView_SlopeAt)(x, z, direction);
    if (CellRest1f(0) == 0) return 0;
    return static_cast<short>(r) > 0x40 ? 1 : 0;
}

// original 0x5280F0: a straight-facing sprite turned to `to` when it faces a
// or b, else to the opposite of `to`; a diagonal facing is left alone.
extern "C" void __cdecl Field_TurnUnless(unsigned a, unsigned b, unsigned to) {
    records::PartyMember* const c = rv::Current<records::PartyMember>();
    const unsigned char facing = c->direction;
    if (facing & 1) return;
    if (facing == static_cast<unsigned char>(a) || facing == static_cast<unsigned char>(b))
        c->direction = static_cast<unsigned char>(to);
    else
        c->direction = static_cast<unsigned char>(to ^ 4);
}

// original 0x528120: 1 when cells a and b both hold `value`; when only a
// does, the sprite turned to `to`, when only b does, to its opposite; 0.
extern "C" unsigned char __cdecl Field_CellPairTurn(unsigned a, unsigned b, unsigned to, unsigned value) {
    const unsigned char v = static_cast<unsigned char>(value);
    if (Cell(a) == v) {
        if (Cell(b) == v) return 1;
        rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>(to);
        return 0;
    }
    if (Cell(b) == v) rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>(to ^ 4);
    return 0;
}

// original 0x528190 (0x122 bytes): a straight-facing raised sprite on a
// cell's corner turned along a slope - in order, each only while +8 (re-read)
// is even: Field_SlopeBetween(x, x, zs, zs + 1, 1) turns it (4, 6, 5); then
// between (x, x) and (zs, z) for facings without bit 2, (z, zs + 1) with it,
// in direction 1, turns it (4, 6, 5); then (xs, xs + 1, z, z, 3) turns it (2,
// 4, 3); then (xs, x, z, z, 3) for facing 0 or 6, else (xs + 1, x, z, z, 3),
// turns it (2, 4, 3). xs / zs the sprite's cell words +0x36 / +0x3A.
extern "C" void __cdecl Field_RaisedEdgeTurns(unsigned x, unsigned z) {
    if ((Sc()[8] & 1) == 0) {
        const std::uint16_t zs = rv::As<records::PartyMember>(Sc())->z_whole;
        if (SH_CALL(Field_SlopeBetween)(x, x, zs, W16(zs + 1u), 1) != 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
    }
    {
        const unsigned char* const s = Sc();
        const unsigned char facing = s[8];
        if ((facing & 1) == 0) {
            unsigned char r;
            if ((facing & 4) == 0) r = SH_CALL(Field_SlopeBetween)(x, x, rv::As<records::PartyMember>(s)->z_whole, z, 1);
            else r = SH_CALL(Field_SlopeBetween)(x, x, z, W16(rv::As<records::PartyMember>(s)->z_whole + 1u), 1);
            if (r != 0) SH_CALL(Field_TurnUnless)(4, 6, 5);
        }
    }
    {
        const unsigned char* const s = Sc();
        if ((s[8] & 1) == 0) {
            const std::uint16_t xs = rv::As<records::PartyMember>(s)->x_whole;
            if (SH_CALL(Field_SlopeBetween)(xs, W16(xs + 1u), z, z, 3) != 0) SH_CALL(Field_TurnUnless)(2, 4, 3);
        }
    }
    const unsigned char* const s = Sc();
    const unsigned char facing = s[8];
    if (facing & 1) return;
    unsigned char r;
    if (facing == 0 || facing == 6) r = SH_CALL(Field_SlopeBetween)(rv::As<records::PartyMember>(s)->x_whole, x, z, z, 3);
    else r = SH_CALL(Field_SlopeBetween)(W16(rv::As<records::PartyMember>(s)->x_whole + 1u), x, z, z, 3);
    if (r != 0) SH_CALL(Field_TurnUnless)(2, 4, 3);
}

// original 0x5282C0: cells 1..5 - the kind of the cell (x, z) ahead, then of
// the two cells beside the way there, then of the two one further on - each
// as Field_CellKind of it from the sprite's own cell.
extern "C" void __cdecl Field_ReadCells(unsigned x, unsigned z) {
    const unsigned char* c = Sprite_Current;
    Cell(1) = member_sprites::g.cell_kind(x, z, rv::As<records::PartyMember>(c)->x_whole, rv::As<records::PartyMember>(c)->z_whole);
    c = Sprite_Current;
    Cell(2) = member_sprites::g.cell_kind(rv::As<records::PartyMember>(c)->x_whole, z, rv::As<records::PartyMember>(c)->x_whole, rv::As<records::PartyMember>(c)->z_whole);
    c = Sprite_Current;
    Cell(3) = member_sprites::g.cell_kind(x, rv::As<records::PartyMember>(c)->z_whole, rv::As<records::PartyMember>(c)->x_whole, rv::As<records::PartyMember>(c)->z_whole);
    c = Sprite_Current;
    {
        const unsigned x1 = static_cast<std::uint16_t>(rv::As<records::PartyMember>(c)->x_whole + 1);
        Cell(4) = member_sprites::g.cell_kind(x1, z, x1, rv::As<records::PartyMember>(c)->z_whole);
    }
    c = Sprite_Current;
    {
        const unsigned z1 = static_cast<std::uint16_t>(rv::As<records::PartyMember>(c)->z_whole + 1);
        Cell(5) = member_sprites::g.cell_kind(x, z1, rv::As<records::PartyMember>(c)->x_whole, z1);
    }
}

// original 0x528370: the kind of the map cell (x, z) seen from (x0, z0) -
// the map byte, with 0xFF for a wall, 0xB0 a way out the sprite faces, 0 or
// 0xA1 some slopes and steps - by the byte's high nibble, the byte of the
// cell stepped from, and Field_ObjectAhead at the point between the two (or
// at the cell, by the sprite's fractions and +0x70).
//
// As the original has it: the arguments are 16-bit; the midpoint is a 16.16
// sum halved toward zero; 0x80 is a wall only for a member (+5 non-zero);
// the byte 0x11 asks the object at 0x905DA0 its +0xB bit 0 when that is the
// sprite, Field_State +0x138 bit 0 otherwise.
extern "C" unsigned char __cdecl Field_CellKind(unsigned x, unsigned z, unsigned x0, unsigned z0) {
    const std::uint32_t X1 = High(x), Z1 = High(z), X0 = High(x0), Z0 = High(z0);
    const unsigned char* const c = Sprite_Current;
    const bool whole_x = rv::As<records::PartyMember>(c)->x_frac == 0, whole_z = rv::As<records::PartyMember>(c)->z_frac == 0, ground = c[0x70] == 0;
    const long px = whole_x == ground ? Half(X1 + X0) : static_cast<long>(X1);
    const long pz = whole_z == ground ? Half(Z1 + Z0) : static_cast<long>(Z1);
    const unsigned char to = member_sprites::g.byte_at(static_cast<short>(x), static_cast<short>(z));
    const unsigned char from = member_sprites::g.byte_at(static_cast<short>(x0), static_cast<short>(z0));
    switch (to & 0xF0) {
    case 0x80:
        if (rv::Current<records::PartyMember>()->actor != 0) return 0xFF;
        [[fallthrough]];
    case 0x00:
    case 0x60:
    case 0x90:
    case 0xB0:
    case 0xC0:
    case 0xD0:
    case 0xE0:
        if (member_sprites::g.cell_facing(from)) return 0xB0;
        if (member_sprites::g.object_ahead(px, pz)) return 0xFF;
        return to;
    case 0x10:
        if (to == 0x11) {
            const unsigned char* const s = Sprite_Current;
            if (Address(s) == ::bof3::Img(FieldEvent_Object905DA0_addr)) return (s[0xB] & 1) ? 0 : 0xFF;
            return (Field_State[0x138] & 1) ? 0 : 0xFF;
        }
        [[fallthrough]];
    case 0x30:
    case 0x40:
    case 0x50:
        if (to != 0x52) return 0xFF;
        if (member_sprites::g.object_ahead(px, pz)) return 0xFF;
        return 0;
    case 0x70:
        if (from == 0x70 && member_sprites::g.object_ahead(px, pz)) return from;
        return member_sprites::g.object_ahead(px, pz) ? 0xFF : 0;
    case 0x20:
        if (member_sprites::g.object_ahead(px, pz)) return 0xFF;
        if ((from & 0xF0) == 0x20 && ((from ^ to) & 0xF1) != 0) return 0xFF;
        return to & 0xF1;
    case 0xA0:
        if (from == 0xC0) {
            if (Field_InputFlags & 1) {
                if (to == 0xA0) return 0;
                if (to == 0xAE) return 0;
            }
            if (to == 0xAF) return 0xA1;
            if (rv::Current<records::PartyMember>()->actor != 0) return 0xFF;
            return to;
        }
        if (to == 0xA2 || to == 0xA3) return 0xFF;
        return member_sprites::g.object_ahead(px, pz) ? 0xFF : 0;
    default:   // 0xF0
        return 0xFF;
    }
}

// original 0x528730: 1 when the cell byte is a way out (0xB_) the sprite
// faces - low nibble 0..5 its facing, 8 facing 3 or 5.
extern "C" unsigned char __cdecl Field_CellFacing(unsigned cell) {
    const unsigned char v = static_cast<unsigned char>(cell);
    if ((v & 0xF0) != 0xB0) return 0;
    const unsigned char low = v & 0xF;
    if (low < 6) return rv::Current<records::PartyMember>()->direction == low ? 1 : 0;
    if (low == 8) return (rv::Current<records::PartyMember>()->direction == 3 || rv::Current<records::PartyMember>()->direction == 5) ? 1 : 0;
    return 0;
}

// original 0x528770: 1 when 0x5725C0 finds an object at (x, z) in the
// sprite's facing that stands more than 0x40 high.
extern "C" unsigned char __cdecl Field_ObjectAhead(long x, long z) {
    const long r = member_sprites::g.object_at(x, z, rv::Current<records::PartyMember>()->direction);
    if (Cell(0) == 0) return 0;
    return static_cast<short>(r) > 0x40 ? 1 : 0;
}

// original 0x5287B0 (0xC3 bytes): cells 1..7 - Field_CellKind of the cell (x,
// z) seen from (x0, z0); then of (x, zs), (x, zs + 1) and (x, zs - 1) from
// (x0, the same z) and of (xs, z), (xs + 1, z), (xs - 1, z) from (the same x,
// z0), zs / xs the sprite's own cell words +0x3A / +0x36 (16 bits each,
// Sprite_Current re-read before each): cells 1, 2, 3, 4, 5, 6, 7 in the order
// (x, z), (x, zs), (x, zs + 1), (xs, z), (xs + 1, z), (x, zs - 1), (xs - 1, z).
extern "C" void __cdecl Field_ReadCellsRaised(unsigned x, unsigned z, unsigned x0, unsigned z0) {
    CellRest1f(1) = SH_CALL(Field_CellKind)(x, z, x0, z0);
    std::uint16_t zs = rv::As<records::PartyMember>(Sc())->z_whole;
    CellRest1f(2) = SH_CALL(Field_CellKind)(x, zs, x0, zs);
    zs = W16(rv::As<records::PartyMember>(Sc())->z_whole + 1);
    CellRest1f(3) = SH_CALL(Field_CellKind)(x, zs, x0, zs);
    std::uint16_t xs = rv::As<records::PartyMember>(Sc())->x_whole;
    CellRest1f(4) = SH_CALL(Field_CellKind)(xs, z, xs, z0);
    xs = W16(rv::As<records::PartyMember>(Sc())->x_whole + 1);
    CellRest1f(5) = SH_CALL(Field_CellKind)(xs, z, xs, z0);
    zs = W16(rv::As<records::PartyMember>(Sc())->z_whole - 1);
    CellRest1f(6) = SH_CALL(Field_CellKind)(x, zs, x0, zs);
    xs = W16(rv::As<records::PartyMember>(Sc())->x_whole - 1);
    CellRest1f(7) = SH_CALL(Field_CellKind)(xs, z, xs, z0);
}

void FieldCore_Inject() {
    BOF3_INJECT(FieldCore_State2);
    BOF3_INJECT(FieldCore_ScriptMove);
    BOF3_INJECT(FieldCore_ScriptMoveAlign);
    BOF3_INJECT(FieldCore_ScriptMoveNext);
    BOF3_INJECT(FieldCore_ScriptMoveStep);
    BOF3_INJECT(FieldCore_ScriptMoveWait);
    BOF3_INJECT(FieldCore_ScriptMoveShadeLower);
    BOF3_INJECT(FieldCore_ScriptMoveShadeFade);
    BOF3_INJECT(FieldCore_Attached);
    BOF3_INJECT(FieldCore_Fade);
    BOF3_INJECT(FieldCore_FadeBegin);
    BOF3_INJECT(FieldCore_FadeStep);
    BOF3_INJECT(FieldCore_Hop);
    BOF3_INJECT(FieldCore_HopBegin);
    BOF3_INJECT(FieldCore_HopLaunch);
    BOF3_INJECT(FieldCore_HopRise);
    BOF3_INJECT(FieldCore_HopFall);
    BOF3_INJECT(FieldCore_HopLand);
    BOF3_INJECT(FieldCore_Vertical);
    BOF3_INJECT(FieldCore_Up);
    BOF3_INJECT(FieldCore_UpBegin);
    BOF3_INJECT(FieldCore_UpOut);
    BOF3_INJECT(FieldCore_UpArrive);
    BOF3_INJECT(FieldCore_UpIn);
    BOF3_INJECT(FieldCore_UpWait5);
    BOF3_INJECT(FieldCore_UpWait6);
    BOF3_INJECT(FieldCore_UpEnd);
    BOF3_INJECT(FieldCore_Down);
    BOF3_INJECT(FieldCore_DownBegin);
    BOF3_INJECT(FieldCore_DownWait1);
    BOF3_INJECT(FieldCore_DownWait2);
    BOF3_INJECT(FieldCore_DownWait3);
    BOF3_INJECT(FieldCore_DownWait4);
    BOF3_INJECT(FieldCore_DownOut);
    BOF3_INJECT(FieldCore_DownArrive);
    BOF3_INJECT(FieldCore_DownIn);
    BOF3_INJECT(FieldCore_DownLand);
    BOF3_INJECT(FieldCore_VerticalShade);
    BOF3_INJECT(FieldCore_JumpExit);
    BOF3_INJECT(FieldCore_JumpExitBegin);
    BOF3_INJECT(FieldCore_JumpExitOut);
    BOF3_INJECT(FieldCore_JumpExitArrive);
    BOF3_INJECT(FieldCore_JumpExitShade);
    BOF3_INJECT(FieldCore_JumpExitIn);
    BOF3_INJECT(FieldCore_TileD0);
    BOF3_INJECT(FieldCore_TileD0Begin);
    BOF3_INJECT(FieldCore_TileD0Move);
    BOF3_INJECT(FieldCore_TileD0Exit);
    BOF3_INJECT(FieldCore_TileD0Probe);
    BOF3_INJECT(FieldCore_TileD0Slope);
    BOF3_INJECT(FieldCore_Fall);
    BOF3_INJECT(FieldCore_FallBegin);
    BOF3_INJECT(FieldCore_FallSpin);
    BOF3_INJECT(FieldCore_Recoil);
    BOF3_INJECT(FieldCore_RecoilBegin);
    BOF3_INJECT(FieldCore_RecoilBlink);
    BOF3_INJECT(Field_CellAhead);
    BOF3_INJECT(Field_CellAheadFlat);
    BOF3_INJECT(Field_CellClass);
    BOF3_INJECT(Field_CellAheadRaised);
    BOF3_INJECT(Field_CellClass5);
    BOF3_INJECT(Field_CornerTurn);
    BOF3_INJECT(Field_CellSlope);
    BOF3_INJECT(Field_SlopeBetween);
    BOF3_INJECT(Field_TurnUnless);
    BOF3_INJECT(Field_CellPairTurn);
    BOF3_INJECT(Field_RaisedEdgeTurns);
    BOF3_INJECT(Field_ReadCells);
    BOF3_INJECT(Field_CellKind);
    BOF3_INJECT(Field_CellFacing);
    BOF3_INJECT(Field_ObjectAhead);
    BOF3_INJECT(Field_ReadCellsRaised);
}
