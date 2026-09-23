// The event script's object ops - see docs/event-objs.md for each function,
// its PSX twin (GAME.EMI section 0) and the fuzz (event_objs_fuzz.cpp).
//
// "SC" is Sprite_Current, read afresh at every use where the original reads
// [0x937F88] again (a callee may move it), and held where the original holds
// it in a register. Every callee is reached through event_objs::g, so that
// the start-up fuzz can stand recorders in for them.
#include "game/event_objs.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_objs_callees.h"
#include "hook/detour.h"

namespace event_objs {

namespace {
template <typename F>
F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Field_JumpSetUp,
    Field_JumpCamera,
    Field_JumpStart,
    Fn<long (__cdecl*)(long, long, unsigned)>(kSlopeAt),
    MapView_GroundAt,
    Sprite_ShadeRaise,
    Sprite_LoadPalette,
    AreaMap_CellsAll,
    AreaMap_CellsAll4,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(kCellsAllWide),
    AreaMap_CellsNone,
    AreaMap_CellsNone4,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(kCellsNoneWide),
    AreaMap_ByteAt,
    Fn<void (__cdecl*)(unsigned)>(kFloorHurt),
    Fn<void (__cdecl*)(unsigned)>(kFlash),
    Party_Count,
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(kHpLose),
    Fn<void (__cdecl*)(unsigned, unsigned)>(kHpGain),
    Effect_FindFree,
    Field_TileTurn,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(kTurnProbe),
    Actor_EquipCount,
};
Callees g = kOriginals;

}  // namespace event_objs

namespace {

using namespace event_objs;

unsigned char* At(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint16_t Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetWord(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
std::int32_t Long(const unsigned char* p) {
    std::int32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void SetLong(unsigned char* p, std::int32_t v) { std::memcpy(p, &v, sizeof v); }
std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}
std::int32_t Mul(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b));
}
// `cdq` / `idiv`: the original's signed division, the divide fault included.
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quotient;
    __asm__ volatile("cltd\n\tidivl %2" : "=a"(quotient) : "a"(dividend), "r"(divisor) : "edx", "cc");
    return quotient;
}
// DamageScratch's first byte: 0x5725C0 leaves 1 there on a slope.
unsigned char ScratchByte() { return At(bof3::addr::DamageScratch)[0]; }
// The low byte of an actor record's state word (Field_ActorStates + n * 0xA4),
// n a whole byte as the original's.
unsigned char& ActorState(unsigned n) { return At(0x903A80u + n * kRecordSize)[0]; }
// The landing point of a jump: the step times the frames, from the position.
long LandingX(const unsigned char* s) { return Add(Mul(Long(s + 0xC), s[9]), Long(s + 0x34)); }
long LandingZ(const unsigned char* s) { return Add(Mul(Long(s + 0x10), s[9]), Long(s + 0x38)); }

// Field_TileD0 and Field_TileA4.
unsigned char TileState(unsigned code, unsigned char state) {
    if (Field_ScriptFlags & 0x400) return 0;
    const unsigned char* const s = Sprite_Current;
    if (!g.cells_all(Long(s + 0x34), Long(s + 0x38), static_cast<std::uint32_t>(Long(s + 0x70)) & 0xFF, code, 0))
        return 0;
    Sprite_Current[1] = 2;
    Sprite_Current[2] = state;
    Sprite_Current[3] = 0;
    Sprite_Current[4] = 0;
    return 1;
}

// Field_Tile89 and Field_Tile8A.
unsigned char TileTurn(unsigned code, unsigned value, unsigned turn) {
    if (Field_ScriptFlags & 0x400) return 0;
    return g.tile_turn(code, value, turn) != 0;
}

// AreaMap_CellsAll4 / AreaMap_CellsNone4: the four cells of the 2 x 2
// footprint at (x >> 16, z >> 16), in the original's call order.
void Cells(long x, long z, unsigned char c[4]) {
    const auto cx = static_cast<short>(static_cast<std::uint32_t>(x) >> 16);
    const auto cz = static_cast<short>(static_cast<std::uint32_t>(z) >> 16);
    const auto cx1 = static_cast<short>(cx + 1), cz1 = static_cast<short>(cz + 1);
    c[0] = g.byte_at(cx, cz);
    c[1] = g.byte_at(cx1, cz);
    c[2] = g.byte_at(cx, cz1);
    c[3] = g.byte_at(cx1, cz1);
}

}  // namespace

// --- the shade fade -----------------------------------------------------------

// original 0x534590 (PSX 0x801C3430): bit 15 - the PlayStation's semi-
// transparency bit - on words 0..30 of the object's CLUT in row 15 of
// Gfx_ClutStrip, +0 bit 0x20, the strip dirty, word 31 zero. As the
// original: SC is held for the loop and the flag, and +5 is read again for
// each word; the last store reads SC and +5 afresh.
extern "C" void __cdecl Sprite_ShadeFadeBegin(void) {
    unsigned char* const s = Sprite_Current;
    for (unsigned i = 0; i < 31; ++i) At(kShadeClut + (s[5] * 32u + i) * 2 + 1)[0] |= 0x80;
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    const unsigned clut = Sprite_Current[5];
    Gfx_ClutStripDirty = 1;
    SetWord(At(kShadeClut + 0x3E + clut * 64), 0);
}

// original 0x534790 (PSX 0x801C37D8): one step of the fade; at its end the
// palette comes back from row 15 of Gfx_ClutStripSource and the shade bytes
// and the semi-transparency go. al only (callers test al).
extern "C" unsigned char __cdecl Sprite_ShadeFadeStep(unsigned step) {
    if (!g.shade_raise(step)) return 0;
    g.load_palette(reinterpret_cast<unsigned short*>(At(kShadeSource + Sprite_Current[5] * 64u)), 0);
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xDF);
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5C] = 0;
    return 1;
}

// original 0x534800 (PSX 0x801C388C): each non-zero shade byte +0x5D..+0x5F
// gets the step's low byte added and, if the sum is then above -64 as a
// signed byte, becomes 0xC0 - so a step can only ever end at 0xC0, from
// below it (0x80.. up) or at once from anything above. A zero byte stays 0,
// and then the answer is never 1. al 1 when all three are 0xC0.
extern "C" unsigned char __cdecl Sprite_ShadeRaise(unsigned step) {
    const auto d = static_cast<unsigned char>(step);
    for (unsigned at = 0x5D; at <= 0x5F; ++at) {
        const unsigned char v = Sprite_Current[at];
        if (v == 0) continue;
        Sprite_Current[at] = static_cast<unsigned char>(v + d);
        if (static_cast<signed char>(Sprite_Current[at]) > -64) Sprite_Current[at] = 0xC0;
    }
    const unsigned char* const s = Sprite_Current;
    return s[0x5D] == 0xC0 && s[0x5E] == 0xC0 && s[0x5F] == 0xC0;
}

// --- the jump -------------------------------------------------------------------

// original 0x5345E0 (PSX 0x801C34C8): the set-up, then - for object 0 (+5),
// with bit 3 of neither flag word's low byte - the camera's target. The
// original reaches Field_JumpCamera by a tail jmp.
extern "C" void __cdecl Field_JumpStart(void) {
    g.jump_setup();
    if (Sprite_Current[5] != 0) return;
    const auto flags = static_cast<unsigned char>(static_cast<unsigned char>(Field_ScriptFlags2) |
                                                  static_cast<unsigned char>(Field_ScriptFlags));
    if (flags & 8) return;
    g.jump_camera();
}

// original 0x534610 (PSX 0x801C3530): a jump's frames (+9), its step per
// frame (+0xC, +0x10) and its rise per frame (+0x14) towards the ground at the
// landing point. Kept: both divisions are idiv and fault on 0 - a speed of 0
// (Field_MoveSpeeds' index a whole byte into a table of six, zeros after) or
// frames that the speed has brought to 0 (a speed above 0x20). The ground is
// 0x5725C0's answer unless that leaves DamageScratch's byte non-zero - a
// slope - and then MapView_GroundAt's.
extern "C" void __cdecl Field_JumpSetUp(void) {
    const unsigned speed = Field_MoveSpeeds[Field_State[0x128]];
    {
        unsigned char* const s = Sprite_Current;
        const unsigned char direction = s[8];
        s[9] = direction == 2 || direction == 6 ? 0x20 : 0x10;
    }
    Sprite_Current[9] = static_cast<unsigned char>(Idiv(Sprite_Current[9], static_cast<std::int32_t>(speed)));
    if (Field_ScriptFlags & 0x1000) Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] << 1);
    SetLong(Sprite_Current + 0xC, Mul(Long(At(kJumpSteps + Sprite_Current[8] * 8u)), static_cast<std::int32_t>(speed)));
    SetLong(Sprite_Current + 0x10, Mul(Long(At(kJumpSteps + Sprite_Current[8] * 8u + 4)), static_cast<std::int32_t>(speed)));
    const unsigned char* const s = Sprite_Current;
    const long x = LandingX(s), z = LandingZ(s);
    long ground = g.slope_at(x, z, s[8]);
    if (ScratchByte() != 0) ground = g.ground_at(x, z);
    unsigned char* const t = Sprite_Current;
    SetLong(t + 0x14, Idiv(static_cast<short>(ground) - static_cast<short>(Word(t + 0x3E)), t[9]));
}

// original 0x534710 (PSX 0x801C3708): what the camera follows during the
// jump - MoveScript_F3Divisor (4 or 8 by the direction, times the speed, both
// stored), the landing point in Field_Kind2X / Z, the rise in
// MoveScript_FAWord. SC held throughout, as the original's eax.
extern "C" void __cdecl Field_JumpCamera(void) {
    const unsigned char* const s = Sprite_Current;
    const unsigned char direction = s[8];
    const short divisor = direction == 2 || direction == 6 ? 4 : 8;
    MoveScript_F3Divisor = divisor;
    const unsigned char* const f = Field_State;
    MoveScript_F3Divisor = static_cast<short>(Field_MoveSpeeds[f[0x128]] * divisor);
    Field_Kind2X = LandingX(s);
    Field_Kind2Z = LandingZ(s);
    MoveScript_FAWord = Word(s + 0x14);
}

// original 0x535F50 (PSX 0x801C5C40): if the ground at the landing point (on
// flat ground only - a slope leaves it) is 0x80 or more above the object, one
// speed slower and the jump set up again.
extern "C" void __cdecl Field_JumpCheckHeight(void) {
    const unsigned char* const s = Sprite_Current;
    const long x = LandingX(s), z = LandingZ(s);
    g.slope_at(x, z, s[8]);
    if (ScratchByte() != 0) return;
    const long ground = g.ground_at(x, z);
    if (static_cast<short>(ground) - static_cast<short>(Word(Sprite_Current + 0x3E)) < 0x80) return;
    Field_State[0x128] = static_cast<unsigned char>(Field_State[0x128] - 1);
    g.jump_start();
}

// original 0x536670 (PSX 0x801C67AC): one frame of the step - +0x34 / +0x38
// by +0xC / +0x10, the height word +0x3E by the low word of +0x14.
extern "C" void __cdecl Sprite_ApplyVelocity(void) {
    SetLong(Sprite_Current + 0x34, Add(Long(Sprite_Current + 0x34), Long(Sprite_Current + 0xC)));
    SetLong(Sprite_Current + 0x38, Add(Long(Sprite_Current + 0x38), Long(Sprite_Current + 0x10)));
    SetWord(Sprite_Current + 0x3E, Word(Sprite_Current + 0x3E) + Word(Sprite_Current + 0x14));
}

// --- the tiles under the leader -------------------------------------------------

// original 0x534920 (PSX 0x801C3AA4): cells of code 0xD0 - state 2 / 6.
extern "C" unsigned char __cdecl Field_TileD0(void) { return TileState(0xD0, 6); }

// original 0x534990 (PSX 0x801C3B44): cells of code 0xA4 - state 2 / 7.
extern "C" unsigned char __cdecl Field_TileA4(void) { return TileState(0xA4, 7); }

// original 0x534A00 (PSX 0x801C3BE4): the first of the codes 0x80..0x88 the
// footprint is all of, and its damage (0x534C20) and flash (0x534DB0) by
// kind. The original pushes both arguments with stale upper bytes, which both
// callees mask.
extern "C" void __cdecl Field_FloorDamage(void) {
    static constexpr unsigned char kKind[9] = {0, 0, 0, 0, 1, 2, 3, 3, 3};
    if (Field_ScriptFlags & 0x400) return;
    unsigned n = 0;
    for (;; ++n) {
        if (n == 9) return;
        const unsigned char* const s = Sprite_Current;
        if (g.cells_all(Long(s + 0x34), Long(s + 0x38), static_cast<std::uint32_t>(Long(s + 0x70)) & 0xFF, 0x80 + n, 0))
            break;
    }
    g.floor_hurt(n);
    g.flash(kKind[n]);
}

// original 0x535120 (PSX 0x801C4788).
extern "C" unsigned char __cdecl Field_Tile89(unsigned turn) { return TileTurn(0x89, 2, turn); }

// original 0x535240 (PSX 0x801C47D4).
extern "C" unsigned char __cdecl Field_Tile8A(unsigned turn) { return TileTurn(0x8A, 1, turn); }

// original 0x535150 (PSX 0x801C4644): on a cell of `code`, state 2 / 8 with
// +0xB = value; with `turn`, the object first faces the first of the eight
// directions - from the opposite of its own, round - that 0x535610 does not
// refuse, or the eighth tried if it refuses all. The code goes on as the
// whole dword; value and turn are read as bytes.
extern "C" unsigned char __cdecl Field_TileTurn(unsigned code, unsigned value, unsigned turn) {
    {
        const unsigned char* const s = Sprite_Current;
        if (g.cells_none(Long(s + 0x34), Long(s + 0x38), static_cast<std::uint32_t>(Long(s + 0x70)) & 0xFF, code, 0))
            return 0;
    }
    if (static_cast<unsigned char>(turn) != 0) {
        unsigned char direction;
        for (unsigned i = 0;;) {
            const unsigned char* const s = Sprite_Current;
            direction = static_cast<unsigned char>(((s[8] ^ 0xFC) + i) & 7);
            const long x = Add(Long(s + 0x34), Mul(Field_DirectionSteps[direction * 2], 2));
            const long z = Add(Long(s + 0x38), Mul(Field_DirectionSteps[direction * 2 + 1], 2));
            if (!g.turn_probe(x, z, 0, Word(s + 0x3E))) break;
            if (++i >= 8) break;
        }
        Sprite_Current[8] = static_cast<unsigned char>(direction ^ 4);
    }
    Sprite_Current[1] = 2;
    Sprite_Current[2] = 8;
    Sprite_Current[3] = 0;
    Sprite_Current[4] = 0;
    Sprite_Current[0xB] = static_cast<unsigned char>(value);
    return 1;
}

// --- the cells ----------------------------------------------------------------

// original 0x535390 (PSX 0x801C49D0): the 2 x 2 footprint when `wide` - the
// whole dword - is 0, else 0x535490's. The callee's eax is the answer.
extern "C" unsigned char __cdecl AreaMap_CellsAll(long x, long z, unsigned wide, unsigned code, unsigned mask) {
    return wide == 0 ? g.cells_all4(x, z, code, mask) : g.cells_all_wide(x, z, code, mask);
}

// original 0x535C50 (PSX 0x801C5724): the same over the "none" tests.
extern "C" unsigned char __cdecl AreaMap_CellsNone(long x, long z, unsigned wide, unsigned code, unsigned mask) {
    return wide == 0 ? g.cells_none4(x, z, code, mask) : g.cells_none_wide(x, z, code, mask);
}

// original 0x5353E0 (PSX 0x801C4A10): every cell the footprint covers is the
// code - the cell right of it only if x has a fraction, below only if z has
// one, the diagonal only if both. With the mask byte, the cells' low nibbles
// are dropped first (the code's are not).
extern "C" unsigned char __cdecl AreaMap_CellsAll4(long x, long z, unsigned code, unsigned mask) {
    unsigned char c[4];
    Cells(x, z, c);
    if (static_cast<unsigned char>(mask) != 0)
        for (unsigned char& v : c) v = static_cast<unsigned char>(v & 0xF0);
    const auto k = static_cast<unsigned char>(code);
    const bool fx = (x & 0xFFFF) != 0, fz = (z & 0xFFFF) != 0;
    if (c[0] != k) return 0;
    if (fx && c[1] != k) return 0;
    if (fz && c[2] != k) return 0;
    if (fx && fz && c[3] != k) return 0;
    return 1;
}

// original 0x535CA0 (PSX 0x801C5764): no cell the footprint covers is the
// code. Each cell 0x20..0x2F is first cut to its bits 0x21, then the mask.
extern "C" unsigned char __cdecl AreaMap_CellsNone4(long x, long z, unsigned code, unsigned mask) {
    unsigned char c[4];
    Cells(x, z, c);
    for (unsigned char& v : c)
        if ((v & 0xF0) == 0x20) v = static_cast<unsigned char>(v & 0x21);
    if (static_cast<unsigned char>(mask) != 0)
        for (unsigned char& v : c) v = static_cast<unsigned char>(v & 0xF0);
    const auto k = static_cast<unsigned char>(code);
    const bool fx = (x & 0xFFFF) != 0, fz = (z & 0xFFFF) != 0;
    if (c[0] == k) return 0;
    if (fx && c[1] == k) return 0;
    if (fz && c[2] == k) return 0;
    if (fx && fz && c[3] == k) return 0;
    return 1;
}

// --- the actor timers and the equipment --------------------------------------

// original 0x534F10 (PSX 0x801C4294): while a member's actor state has bit
// 0x80 - every listed member with Field_InputFlags bit 0, else the leader -
// Field_State +0x124 counts down; at 0 it is 10 again, the flash, 1 HP from
// each such member (or from Field_State +0x89's), and an effect object of
// kind 0x41 at the leader if any HP was taken. Field_State is held from after
// Party_Count until the countdown, as the original's esi; read afresh after.
extern "C" void __cdecl Field_Bit80Tick(void) {
    const auto count = static_cast<unsigned char>(g.party_count(0));
    unsigned char* const f = Field_State;
    unsigned char hit = 0;
    if (Field_InputFlags & 1) {
        for (unsigned i = 0; i < count; ++i)
            if (ActorState(MoveScript_EffectState[At(kPartyList)[i]]) & 0x80) hit = 1;
        if (!hit) return;
    } else if (!(ActorState(f[0x148]) & 0x80)) {
        return;
    }
    f[0x124] = static_cast<unsigned char>(f[0x124] - 1);
    hit = 0;
    if (Field_State[0x124] != 0) return;
    Field_State[0x124] = 10;
    g.flash(1);
    if (Field_InputFlags & 1) {
        for (unsigned i = 0; i < count; ++i) {
            const unsigned char member = At(kPartyList)[i];
            if ((ActorState(MoveScript_EffectState[member]) & 0x80) && (g.hp_lose(1, member) & 0xFFFF) != 0) hit = 1;
        }
        if (!hit) return;
    } else if ((g.hp_lose(1, Field_State[0x89]) & 0xFFFF) == 0) {
        return;
    }
    const unsigned char e = g.effect_free();
    if (e == 0xFF) return;
    const unsigned char* const s = Sprite_Current;
    unsigned char* const o = Effect_Objects + e * 0x80u;
    o[0] = 1;
    o[5] = 0x41;
    o[8] = s[8];
    SetWord(o + 0x36, Word(s + 0x2E));
    SetWord(o + 0x3A, Word(s + 0x30));
    o[0x27] = 0;
    o[6] = 1;
    o[0xB] = s[5];
}

// original 0x5350C0 (PSX 0x801C4590): while the leader's actor state has bit
// 0x20, Field_State +0x125 counts down, and at 0 the bit goes.
extern "C" void __cdecl Field_Bit20Tick(void) {
    unsigned char* const f = Field_State;
    if (!(ActorState(f[0x148]) & 0x20)) return;
    f[0x125] = static_cast<unsigned char>(f[0x125] - 1);
    if (Field_State[0x125] != 0) return;
    unsigned char* const state = &ActorState(Field_State[0x148]);
    SetWord(state, Word(state) & 0xFFDF);
}

// original 0x535270 (PSX 0x801C4820): one HP (0x5373F0) for each of three
// equipment tests the leader passes. Field_State read afresh for each use.
extern "C" void __cdecl Field_EquipTick(void) {
    if (g.equip_count(Field_State[0x148], 3, 0x16)) g.hp_gain(1, Field_State[0x89]);
    if (g.equip_count(Field_State[0x148], 3, 0x17)) g.hp_gain(1, Field_State[0x89]);
    if (g.equip_count(Field_State[0x148], 2, 0x1F)) g.hp_gain(1, Field_State[0x89]);
}

// original 0x535310 (PSX 0x801C48EC): how many of a record's equipment bytes
// of `kind` are `value`. Nothing bounds the member (a whole byte into
// MoveScript_EffectState's 24) or the record it names.
extern "C" unsigned char __cdecl Actor_EquipCount(unsigned member, unsigned kind, unsigned value) {
    const unsigned char* const r = At(bof3::addr::CharacterRecords + MoveScript_EffectState[member & 0xFF] * kRecordSize);
    const auto v = static_cast<unsigned char>(value);
    switch (kind & 0xFF) {
    case 1: return r[0x12] == v;
    case 2: return static_cast<unsigned char>((r[0x13] == v) + (r[0x14] == v) + (r[0x15] == v));
    case 3: return static_cast<unsigned char>((r[0x16] == v) + (r[0x17] == v));
    default: return 0;
    }
}

void EventObjs_Inject() {
    if (bof3::WantsShadow("event_objs")) event_objs::SelfTest();
    BOF3_INJECT(Sprite_ShadeFadeBegin);
    BOF3_INJECT(Field_JumpStart);
    BOF3_INJECT(Field_JumpSetUp);
    BOF3_INJECT(Field_JumpCamera);
    BOF3_INJECT(Sprite_ShadeFadeStep);
    BOF3_INJECT(Sprite_ShadeRaise);
    BOF3_INJECT(Field_TileD0);
    BOF3_INJECT(Field_TileA4);
    BOF3_INJECT(Field_FloorDamage);
    BOF3_INJECT(Field_Bit80Tick);
    BOF3_INJECT(Field_Bit20Tick);
    BOF3_INJECT(Field_Tile89);
    BOF3_INJECT(Field_TileTurn);
    BOF3_INJECT(Field_Tile8A);
    BOF3_INJECT(Field_EquipTick);
    BOF3_INJECT(Actor_EquipCount);
    BOF3_INJECT(AreaMap_CellsAll);
    BOF3_INJECT(AreaMap_CellsAll4);
    BOF3_INJECT(AreaMap_CellsNone);
    BOF3_INJECT(AreaMap_CellsNone4);
    BOF3_INJECT(Field_JumpCheckHeight);
    BOF3_INJECT(Sprite_ApplyVelocity);
}
