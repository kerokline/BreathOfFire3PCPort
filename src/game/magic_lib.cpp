// The effect library (queue group L of round nine), compiled into the exe at
// 0x4FAFF0..0x4FC32F after MAGIC226/227 and before MAGIC080: the helpers the
// BMAGIC overlays call - by 1 to 94 of them each - and the engine's Restore
// Form now and then. Nine of its 34 functions were ours before (FxDim_*,
// magic_fx_reached.cpp; BattleActor_*, battle_items.cpp); these are the other
// 25. The first function of the band, 0x4FAF90, is not the library's but
// MAGIC226/227's own pool allocator (docs/magic_lib.md section 1).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that the two popup
// tasks abort on a phase past their four-entry tables (the project's
// precedent, docs/magic_fx_reached.md section 3), and that where the original
// divides by zero (the CLUT helpers' kinds 5..7, the side's centre with every
// actor out) ours aborts with a message instead of the processor's exception.
#include "game/magic_lib.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// The ability path's target and result record (docs/battle_odds.md), which
// MagicFx_ApplyBuff points at the actor for one call and puts back.
constexpr std::uint32_t kResultTarget = 0x904B54;   // u8
constexpr std::uint32_t kResultRecord = 0x904B60;   // unsigned char *
constexpr std::uint32_t kStatsCopy = 0x939F80;      // 32 bytes: the actor's +0xA0 (party) / +0xB0 (enemy)
// The byte FxSize's and FxSizeB's second tables and the formation rows are
// indexed by (battle_items.cpp's kFxSizeSelect).
constexpr std::uint32_t kFormation = 0x904B89;
// The buff roll: 0x44FC10(stat), 1 in al when it was resisted - unnamed, the
// engine's (in no group: docs/magic_lib.md section 4).
constexpr std::uint32_t kBuffRoll = 0x44FC10;
using BuffRollFn = unsigned char (__cdecl*)(unsigned);

// The packet pools' end (Gfx_PacketPools + 0x10000 - 0x54, symbols.toml), per
// buffer, and the frame's depth nodes (Gfx_FrameNodes + 4): 0x38 of 0x30 bytes.
constexpr std::uint32_t kPoolEnd = 0x7F1BAC;
constexpr std::uint32_t kDepthCells = 0x8022C4;
constexpr int kDepths = 0x38;
// The strip's row 2 (Gfx_ClutStrip + 0x400) the CLUT helpers borrow, and the
// same row of the strip as loaded.
constexpr std::uint32_t kFxRow = 0x80F980;
constexpr std::uint32_t kFxRowSource = 0x80B980;
constexpr std::uint32_t kEnemyFxSizeB = 0x8C5653;   // + 0x8C * the enemy's type: FxSize's byte + 1

short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
std::int32_t I32(std::uint32_t v) { return static_cast<std::int32_t>(v); }
std::uint32_t U32(const unsigned char* p) { return static_cast<std::uint32_t>(Long(p)); }
void PutFloat(unsigned char* p, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(p, &f, sizeof f);
}
void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
std::uint32_t Addr(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

unsigned char* PartyRecord(unsigned i) { return Mem(at::kParty + i * at::kPartyStride); }
unsigned char* EnemyRecord(unsigned i) { return Mem(at::kEnemies + (i - 3u) * at::kEnemyStride); }
// An actor's record by the battle index, as the originals index it (0..2 the
// party, 3.. an enemy; unchecked above).
unsigned char* ActorRecord(unsigned i) { return i < 3 ? PartyRecord(i) : EnemyRecord(i); }

// --- the buff popup ----------------------------------------------------------

// Both popup tasks' tail: while the slot is live and past its first phase,
// the popup drawn with the icon at +4.
void PopupDrawIfShown() {
    const unsigned char* const t = Sprite_Current;
    if (t[0] != 0 && t[1] != 0) MH_CALL(BuffPopup_Draw)(t[4]);
}

// Both starts' end, the actor's position taken: its screen point, lifted 0x18
// and by +0xA; +0x10 (the rise's speed) -14; +9 0; +0xA 1; the phase on.
void PopupSettle() {
    MH_CALL(BattleActor_UpdateScreenXY)();
    SetWord(Sprite_Current + 0x2E, Word(Sprite_Current + 0x2E) - 0x18u);
    SetWord(Sprite_Current + 0x30, Word(Sprite_Current + 0x30) - Sprite_Current[0xA]);
    SetLong(Sprite_Current + 0x10, -14);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 1;
    Bump(Sprite_Current[1]);
}

void CopyPosition(const unsigned char* from) {
    SetLong(Sprite_Current + 0x34, Long(from + 0x34));
    SetLong(Sprite_Current + 0x38, Long(from + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(from + 0x3C));
}

// One of the popup's two quads: a POLY_FT4 at Gfx_PacketNext, 0x30 x +0xA at
// (+0x2E, +0x30) + d, texture page (0x380, 0x100), CLUT (0x10, 0x1FA), the
// icon's cell, shade `shade`; committed to slot 2.
void PopupQuad(const unsigned char* uv, int d, int w, unsigned char shade) {
    unsigned char* const p = Gfx_PacketNext;
    MH_CALL(Gpu_SetPolyFT4)(p);
    PutFloat(p + 0x08, S16(Sprite_Current + 0x2E) + d);
    PutFloat(p + 0x0C, S16(Sprite_Current + 0x30) + d);
    PutFloat(p + 0x18, S16(Sprite_Current + 0x2E) + w);
    PutFloat(p + 0x1C, S16(Sprite_Current + 0x30) + d);
    PutFloat(p + 0x28, S16(Sprite_Current + 0x2E) + d);
    PutFloat(p + 0x2C, S16(Sprite_Current + 0x30) + Sprite_Current[0xA] + d);
    PutFloat(p + 0x38, S16(Sprite_Current + 0x2E) + w);
    PutFloat(p + 0x3C, S16(Sprite_Current + 0x30) + Sprite_Current[0xA] + d);
    SetWord(p + 0x26, MH_CALL(Gpu_GetTPage)(0, 0, 0x380, 0x100));
    SetWord(p + 0x16, MH_CALL(Gpu_GetClut)(0x10, 0x1FA));
    p[0x14] = uv[0];
    p[0x15] = uv[2];
    p[0x24] = static_cast<unsigned char>(uv[0] + 0x30);
    p[0x25] = uv[2];
    p[0x34] = uv[0];
    p[0x35] = static_cast<unsigned char>(Sprite_Current[0xA] + uv[2]);
    p[0x44] = static_cast<unsigned char>(uv[0] + 0x30);
    p[0x45] = static_cast<unsigned char>(Sprite_Current[0xA] + uv[2]);
    p[4] = p[5] = p[6] = shade;
    MH_CALL(Gfx_CommitPrim)(2, 0x48);
}

// --- the sprite-CLUT helpers' cell -------------------------------------------

// The record's CLUT cell in Gfx_ClutStrip: +0x28 the kind (unchecked: 5..7
// divide by zero, 8.. read past the tables), +0x27 the index, split by the
// kind's divisor into a row and a column times the kind's multiplier (a byte
// each), the row 0x10 on when +0x24 has bit 2.
unsigned ClutCell(const unsigned char* s, const char* who) {
    const unsigned kind = s[0x28];
    const unsigned index = s[0x27];
    const unsigned divisor = SpriteClut_Divisors[kind];
    if (divisor == 0) bof3::Fatal("%s: sprite kind %u has divisor 0 - the original divides by zero", who, kind);
    auto row = static_cast<unsigned char>(index / divisor);
    const auto col = static_cast<unsigned char>((index % divisor) * SpriteClut_Mults[kind]);
    if (s[0x24] & 4) row = static_cast<unsigned char>(row + 0x10);
    return row * 0x100u + col;
}
unsigned char* StripWord(unsigned i) { return reinterpret_cast<unsigned char*>(Gfx_ClutStrip + i); }

// --- the steps toward a point ------------------------------------------------

// Sprite_Current moved by `speed` along in[] normalised: x and z by
// (n * speed) >> 3, the height word +0x3E by (n * speed) >> 11. Sprite_Current
// is read again after the normalisation (and for each field).
void StepAlong(const long* in, int speed) {
    short out[4] = {};
    MH_CALL(Gte_VectorNormalS)(in, out);
    const auto s = static_cast<std::uint32_t>(static_cast<short>(speed));
    SetLong(Sprite_Current + 0x34, I32(U32(Sprite_Current + 0x34) + static_cast<std::uint32_t>(
                                                                          I32(static_cast<std::uint32_t>(out[0]) * s) >> 3)));
    SetLong(Sprite_Current + 0x38, I32(U32(Sprite_Current + 0x38) + static_cast<std::uint32_t>(
                                                                          I32(static_cast<std::uint32_t>(out[1]) * s) >> 3)));
    SetWord(Sprite_Current + 0x3E,
            Word(Sprite_Current + 0x3E) + static_cast<unsigned>(I32(static_cast<std::uint32_t>(out[2]) * s) >> 11));
}

// The three near tests' shared first two: |a - b + size / 2| within size,
// unsigned, on x and z.
bool NearXZ(const unsigned char* sc, std::uint32_t x, std::uint32_t z, std::uint32_t size) {
    const std::uint32_t half = size >> 1;
    if (U32(sc + 0x34) - x + half > size) return false;
    return U32(sc + 0x38) - z + half <= size;
}

}  // namespace

#define MLIB_EXPORT extern "C" __attribute__((disable_tail_calls))

// original 0x4FB0A0: the buff popup's task (kind 1, parameter 0x48 - made by
// MagicFx_BuffPopup). Its phase +1 through a four-entry table the original
// builds on its stack - BuffPopup_Start, _Rise, _Fall, _End - then, with the
// slot still live (+0) and past phase 0 (+1, read after the phase), the draw
// of the icon at +4. The index is not checked by the original: 4..255 would
// call through its own stack; ours aborts.
MLIB_EXPORT void __cdecl BuffPopup_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::BuffPopup_Start, bof3::addr::BuffPopup_Rise,
                                                 bof3::addr::BuffPopup_Fall, bof3::addr::BuffPopup_End};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 4) bof3::Fatal("BuffPopup_Task: phase %u, past the four-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    PopupDrawIfShown();
}

// original 0x4FB100: +9 down (the delay the maker set to 1); at 0 the owner's
// (0x93B940, read again for each) position +0x34 / +0x38 / +0x3C to the task,
// then its screen point lifted (PopupSettle).
MLIB_EXPORT void __cdecl BuffPopup_Start(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    if (Sprite_Current[9] != 0) return;
    SetLong(Sprite_Current + 0x34, Long(Pointer(at::kOwner) + 0x34));
    SetLong(Sprite_Current + 0x38, Long(Pointer(at::kOwner) + 0x38));
    SetLong(Sprite_Current + 0x3C, Long(Pointer(at::kOwner) + 0x3C));
    PopupSettle();
}

// original 0x4FB190: the rise. +0xA (the quad's height) up to 8; the speed
// +0x10 += 2 and the screen y +0x30 += its low word; +9 up, at 12 the speed
// -6 and the phase on.
MLIB_EXPORT void __cdecl BuffPopup_Rise(void) {
    unsigned char* const t = Sprite_Current;
    if (t[0xA] < 8) Bump(t[0xA]);
    SetLong(t + 0x10, Long(t + 0x10) + 2);
    SetWord(t + 0x30, Word(t + 0x30) + Word(t + 0x10));
    Bump(t[9]);
    if (t[9] == 0xC) {
        SetLong(t + 0x10, -6);
        Bump(t[1]);
    }
}

// original 0x4FB1F0: the bounce. The speed +0x10 += 2, the screen y += its
// low word; +9 up, at 16 the phase on.
MLIB_EXPORT void __cdecl BuffPopup_Fall(void) {
    unsigned char* const t = Sprite_Current;
    SetLong(t + 0x10, Long(t + 0x10) + 2);
    SetWord(t + 0x30, Word(t + 0x30) + Word(t + 0x10));
    Bump(t[9]);
    if (t[9] == 0x10) Bump(t[1]);
}

// original 0x4FB230: the hold. +9 up; at 28 the owner's +0xB (its count of
// live children) down and the slot freed (a tail jmp).
MLIB_EXPORT void __cdecl BuffPopup_End(void) {
    Bump(Sprite_Current[9]);
    if (Sprite_Current[9] != 0x1C) return;
    unsigned char* const owner = Pointer(at::kOwner);
    owner[0xB] = static_cast<unsigned char>(owner[0xB] - 1);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4FB260: the other popup task (kind 1, parameter 2), placed by
// an actor index rather than the owner: its table is BuffPopupAt_Start and
// the popup's own rise, fall and end. As BuffPopup_Task otherwise.
MLIB_EXPORT void __cdecl BuffPopupAt_Task(void) {
    static constexpr std::uint32_t kPhases[4] = {bof3::addr::BuffPopupAt_Start, bof3::addr::BuffPopup_Rise,
                                                 bof3::addr::BuffPopup_Fall, bof3::addr::BuffPopup_End};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 4) bof3::Fatal("BuffPopupAt_Task: phase %u, past the four-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    PopupDrawIfShown();
}

// original 0x4FB2C0: +9 down; at 0 the position of the actor whose battle
// index is +0xB (0..2 a party record, else an enemy's, unchecked), then as
// BuffPopup_Start.
MLIB_EXPORT void __cdecl BuffPopupAt_Start(void) {
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] - 1);
    if (Sprite_Current[9] != 0) return;
    CopyPosition(ActorRecord(Sprite_Current[0xB]));
    PopupSettle();
}

// original 0x4FB3E0: the popup's draw. A draw mode (page 0x15, dither on) to
// slot 2, then two quads of the icon's cell (BuffPopup_IconUV[icon], u at +0
// and v at +2, the index unchecked): the shadow one pixel down and right and
// one wider, shade 1, then the icon, shade 0x80. Each quad's pointer is read
// once from Gfx_PacketNext after the commit before it.
MLIB_EXPORT void __cdecl BuffPopup_Draw(unsigned icon) {
    MH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 1, 0x15, 0);
    MH_CALL(Gfx_CommitPrim)(2, 0xC);
    const unsigned char* const uv = BuffPopup_IconUV + (icon & 0xFF) * 4;
    PopupQuad(uv, 1, 0x31, 1);
    PopupQuad(uv, 0, 0x30, 0x80);
}

// original 0x4FB6F0: the buff roll on one actor. The ability path's target
// byte (0x904B54) and result record (0x904B60) are set to the actor's - the
// party record's +0x124, an enemy's +0x104 - and its 32 bytes from +0xA0 /
// +0xB0 copied to 0x939F80; 0x44FC10(stat); both put back. 1 in al when the
// roll answered 0 (not resisted), else 0. The actor unchecked above 2.
MLIB_EXPORT unsigned char __cdecl MagicFx_ApplyBuff(unsigned stat, unsigned actor) {
    const unsigned char kept_target = Mem(kResultTarget)[0];
    const std::int32_t kept_record = Long(Mem(kResultRecord));
    const unsigned a = actor & 0xFF;
    Mem(kResultTarget)[0] = static_cast<unsigned char>(a);
    const unsigned char* stats;
    if (a < 3) {
        SetLong(Mem(kResultRecord), I32(Addr(PartyRecord(a) + 0x124)));
        stats = PartyRecord(a) + 0xA0;
    } else {
        SetLong(Mem(kResultRecord), I32(Addr(EnemyRecord(a) + 0x104)));
        stats = EnemyRecord(a) + 0xB0;
    }
    for (unsigned i = 0; i < 32; i += 4) SetLong(Mem(kStatsCopy + i), Long(stats + i));
    const unsigned char resisted = MH_AT(BuffRollFn, kBuffRoll)(stat);
    Mem(kResultTarget)[0] = kept_target;
    SetLong(Mem(kResultRecord), kept_record);
    return resisted == 0 ? 1 : 0;
}

// original 0x4FB790: the buff and its popup. The stat MagicFx_BuffStats[kind
// & 3] rolled (MagicFx_ApplyBuff) on `who` - an enemy (who + 3) when the
// target byte 0x904B44 has bit 6, else the party's `who`; then a popup task
// (kind 1, parameter 0x48, BuffPopup_Task) whose owner is Sprite_Current (read
// after the create), +4 the icon - `kind`, or 8 when resisted -, +9 1, +0xA 0.
// The slot is not tested (0xFF writes past the slots).
MLIB_EXPORT void __cdecl MagicFx_BuffPopup(unsigned kind, unsigned who) {
    const unsigned char target = Mem(at::kTarget)[0];
    const unsigned stat = MagicFx_BuffStats[kind & 3];
    const unsigned char applied =
        MH_CALL(MagicFx_ApplyBuff)(stat, (target & 0x40) ? static_cast<unsigned char>(who + 3) : who);
    const unsigned char icon = applied != 0 ? static_cast<unsigned char>(kind) : 8;
    const unsigned slot = MH_CALL(BattleTask_Create)(1, 0x48);
    unsigned char* const t = Mem(at::kTasks + (slot & 0xFF) * at::kTaskStride);
    SetLong(t + 0x80, I32(Addr(Sprite_Current)));
    t[4] = icon;
    t[9] = 1;
    t[0xA] = 0;
}

// original 0x4FB880: `count` primitives of `size` bytes from `prims` linked
// into one depth of the frame's nodes, the one of the largest key first. The
// depth: hi(z) - (lo(z) == 0) - (lo(x) == 0) + hi(x) - MapView_Origin's two
// words + bias + 2 (hi a dword's upper word signed, lo its lower). Past
// 0..0x37, or with the next primitive of `size` not fitting before the pool's
// end, the `count` primitives are given back (Gfx_PacketNext -= count *
// size). Else `count` times: the largest key (signed, the first of equals)
// is taken; a zero key links nothing; else it is zeroed and its primitive
// linked (Gpu_LinkPrim(the node's head, primitive)) and made the head, the
// node's address computed again after the link (Gfx_BufferIndex re-read).
MLIB_EXPORT void __cdecl MagicFx_LinkByDepth(unsigned x, unsigned z, int* keys, unsigned prims, unsigned count,
                                             unsigned size, unsigned bias) {
    const auto hi = [](std::uint32_t v) { return static_cast<int>(static_cast<short>(v >> 16)); };
    const auto lo_zero = [](std::uint32_t v) { return (v & 0xFFFF) == 0 ? 1 : 0; };
    const int depth = hi(z) - lo_zero(z) - lo_zero(x) - MapView_Origin[1] -
                      MapView_Origin[0] + hi(x) + static_cast<int>(bias & 0xFF) + 2;
    const unsigned n = count & 0xFF;
    const unsigned sz = size & 0xFF;
    unsigned char* const next_cell = reinterpret_cast<unsigned char*>(&Gfx_PacketNext);
    if (depth < 0 || depth >= kDepths ||
        kPoolEnd + (static_cast<std::uint32_t>(Gfx_BufferIndex) << 16) <= U32(next_cell) + sz) {
        SetLong(next_cell, I32(U32(next_cell) - n * sz));
        return;
    }
    const std::uint32_t row = static_cast<std::uint32_t>(depth) * 6;
    for (unsigned left = n; left != 0; --left) {
        unsigned best = 0;
        for (unsigned i = 0; i < n; ++i)
            if (keys[best] < keys[i]) best = i;
        if (keys[best] == 0) continue;
        const std::uint32_t prim = prims + sz * best;
        keys[best] = 0;
        unsigned char* const head = Mem(kDepthCells + (Gfx_BufferIndex + row) * 8);
        MH_CALL(Gpu_LinkPrim)(reinterpret_cast<unsigned long*>(static_cast<std::uintptr_t>(U32(head))), prim);
        SetLong(Mem(kDepthCells + (Gfx_BufferIndex + row) * 8), I32(prim));
    }
}

// original 0x4FB9F0: Sprite_Current one step of `speed` (a short) toward the
// sprite `to`: the difference to its x / z (sar 9 of the difference) and
// height (sar 1 of the difference of the words +0x3E), normalised
// (Gte_VectorNormalS), then StepAlong.
MLIB_EXPORT void __cdecl MagicFx_StepToward(const unsigned char* to, int speed) {
    const unsigned char* const sc = Sprite_Current;
    const long in[3] = {I32(U32(to + 0x34) - U32(sc + 0x34)) >> 9, I32(U32(to + 0x38) - U32(sc + 0x38)) >> 9,
                        (static_cast<int>(S16(to + 0x3E)) - S16(sc + 0x3E)) >> 1};
    StepAlong(in, speed);
}

// original 0x4FBA90: the same toward a point in the effects' space (x, z, y:
// a VECTOR by value; its fourth word is not read), the sprite's side
// (+0x34 sar 9) - 0x4000, (+0x38 sar 9) - 0x4000 and (s16 +0x3E) sar 1.
MLIB_EXPORT void __cdecl MagicFx_StepTowardPoint(unsigned x, unsigned z, unsigned y, unsigned pad, int speed) {
    (void)pad;
    const unsigned char* const sc = Sprite_Current;
    const long in[3] = {I32(x - static_cast<std::uint32_t>((Long(sc + 0x34) >> 9) - 0x4000)),
                        I32(z - static_cast<std::uint32_t>((Long(sc + 0x38) >> 9) - 0x4000)),
                        I32(y - static_cast<std::uint32_t>(S16(sc + 0x3E) >> 1))};
    StepAlong(in, speed);
}

// original 0x4FBB40: Sprite_Current moved by `radius` at the angle toward
// the sprite `to` (Math_Ratan2(dz, dx), as floats) plus `offset`, the angle
// masked to 12 bits: x += (cos * radius) >> 12, z += (sin * radius) >> 12. The
// angle is answered. Each field's slot pointer is read before the cosine /
// sine, the field after.
MLIB_EXPORT int __cdecl MagicFx_StepAround(const unsigned char* to, int radius, int offset) {
    const unsigned char* const sc = Sprite_Current;
    const int dx = I32(U32(to + 0x34) - U32(sc + 0x34));
    const int dz = I32(U32(to + 0x38) - U32(sc + 0x38));
    const int angle = static_cast<int>(static_cast<std::uint32_t>(MH_CALL(Math_Ratan2)(static_cast<float>(dz),
                                                                                       static_cast<float>(dx)) +
                                                                  static_cast<std::uint32_t>(offset)) &
                                       0xFFF);
    unsigned char* const px = Sprite_Current + 0x34;
    const std::uint32_t c = static_cast<std::uint32_t>(MH_CALL(Math_Cos)(angle));
    SetLong(px, I32(U32(px) + static_cast<std::uint32_t>(I32(c * static_cast<std::uint32_t>(radius)) >> 12)));
    unsigned char* const pz = Sprite_Current + 0x38;
    const std::uint32_t s = static_cast<std::uint32_t>(MH_CALL(Math_Sin)(angle));
    SetLong(pz, I32(U32(pz) + static_cast<std::uint32_t>(I32(s * static_cast<std::uint32_t>(radius)) >> 12)));
    return angle;
}

// original 0x4FBBD0: 1 when Sprite_Current is within the box of `size`
// around the sprite `other`: x and z (NearXZ), and ((size >> 9) - other's
// word +0x3E + ours) & 0xFFFF at most size >> 8; else 0.
MLIB_EXPORT int __cdecl MagicFx_NearSprite3D(const unsigned char* other, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    if (!NearXZ(sc, U32(other + 0x34), U32(other + 0x38), size)) return 0;
    const std::uint32_t h = ((size >> 9) - Word(other + 0x3E) + Word(sc + 0x3E)) & 0xFFFF;
    return h <= (size >> 8) ? 1 : 0;
}

// original 0x4FBC30: the same on x and z only.
MLIB_EXPORT int __cdecl MagicFx_NearSprite(const unsigned char* other, unsigned size) {
    return NearXZ(Sprite_Current, U32(other + 0x34), U32(other + 0x38), size) ? 1 : 0;
}

// original 0x4FBC70: MagicFx_NearSprite3D against a point: x, z, and the
// height the upper word of `yw` (a sprite's dword +0x3C passed whole).
MLIB_EXPORT int __cdecl MagicFx_NearPoint3D(unsigned x, unsigned z, unsigned yw, unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    if (!NearXZ(sc, x, z, size)) return 0;
    const std::uint32_t h = ((size >> 9) + Word(sc + 0x3E) - (yw >> 16)) & 0xFFFF;
    return h <= (size >> 8) ? 1 : 0;
}

// original 0x4FBCD0: MagicFx_NearSprite against a point.
MLIB_EXPORT int __cdecl MagicFx_NearPoint(unsigned x, unsigned z, unsigned size) {
    return NearXZ(Sprite_Current, x, z, size) ? 1 : 0;
}

// original 0x4FBE30: bit 15 (the semi-transparency bit) set on the record's
// CLUT (ClutCell) entries 1 .. count - 1, count SpriteClut_Counts[kind] * 16
// (the kind re-read each entry); Gfx_ClutStripDirty 1.
MLIB_EXPORT void __cdecl SpriteClut_SetStp(const unsigned char* s) {
    const unsigned cell = ClutCell(s, "SpriteClut_SetStp");
    for (unsigned i = 1; i < SpriteClut_Counts[s[0x28]] * 16u; ++i) StripWord(cell + i)[1] |= 0x80;
    Gfx_ClutStripDirty = 1;
}

// original 0x4FBED0: the record's CLUT entry 31 zeroed; Gfx_ClutStripDirty 1.
MLIB_EXPORT void __cdecl SpriteClut_ClearEntry31(const unsigned char* s) {
    const unsigned cell = ClutCell(s, "SpriteClut_ClearEntry31");
    Gfx_ClutStripDirty = 1;
    SetWord(StripWord(cell + 31), 0);
}

// original 0x4FBF50: the record's CLUT, count entries (as SpriteClut_SetStp),
// copied word by word upward to the strip's row 2 (0x80F980) - overlapping
// cells copy as the loop does, not as memmove; Gfx_ClutStripDirty 1; the
// kind's divisor * 2 answered.
MLIB_EXPORT unsigned __cdecl SpriteClut_CopyToFxRow(const unsigned char* s) {
    const unsigned cell = ClutCell(s, "SpriteClut_CopyToFxRow");
    for (unsigned i = 0; i < SpriteClut_Counts[s[0x28]] * 16u; ++i)
        SetWord(Mem(kFxRow + 2 * i), Word(StripWord(cell + i)));
    Gfx_ClutStripDirty = 1;
    return SpriteClut_Divisors[s[0x28]] * 2u;
}

// original 0x4FC000: the strip's row 2 (256 entries) put back from the strip
// as loaded (Gfx_ClutStripSource); Gfx_ClutStripDirty 1.
MLIB_EXPORT void __cdecl SpriteClut_RestoreFxRow(void) {
    for (unsigned i = 0; i < 0x200; i += 2) SetWord(Mem(kFxRow + i), Word(Mem(kFxRowSource + i)));
    Gfx_ClutStripDirty = 1;
}

// original 0x4FC0E0: Sprite_Current to the centre of the target's side: with
// both bits 6 and 7 of the target byte 0x904B44 (read once) nothing; bit 6 the
// eight enemies, else the three party members; of those not out
// (Battle_ActorIsOut, the record read after), the mean of x sar 9 (then shl
// 9), z the same, and the height word, each a signed division. With every
// one out the original divides by zero; ours aborts.
MLIB_EXPORT void __cdecl MagicFx_CenterOnSide(void) {
    const unsigned char target = Mem(at::kTarget)[0];
    if ((target & 0xC0) == 0xC0) return;
    std::uint32_t sx = 0, sz = 0, sy = 0;
    int n = 0;
    const bool enemies = (target & 0x40) != 0;
    for (unsigned i = 0; i < (enemies ? 8u : 3u); ++i) {
        const unsigned actor = enemies ? i + 3 : i;
        if (MH_CALL(Battle_ActorIsOut)(actor) != 0) continue;
        const unsigned char* const r = ActorRecord(actor);
        sx += static_cast<std::uint32_t>(Long(r + 0x34) >> 9);
        sz += static_cast<std::uint32_t>(Long(r + 0x38) >> 9);
        sy += static_cast<std::uint32_t>(static_cast<int>(S16(r + 0x3E)));
        ++n;
    }
    if (n == 0) bof3::Fatal("MagicFx_CenterOnSide: every actor of the side is out - the original divides by zero");
    SetLong(Sprite_Current + 0x34, I32(static_cast<std::uint32_t>(I32(sx) / n) << 9));
    SetLong(Sprite_Current + 0x38, I32(static_cast<std::uint32_t>(I32(sz) / n) << 9));
    SetWord(Sprite_Current + 0x3E, static_cast<unsigned>(I32(sy) / n));
}

// original 0x4FC260: BattleActor_FxSize's twin, one byte on: the acting actor
// (0x904B34) a party member - with its +0x134 bit 1 FxSizeB_AltTable[byte
// 0x904B89], else FxSizeB_Table[its +0x89] -, or an enemy, the byte at
// 0x8C5653 + 0x8C * its type (+0xF0). Answered in al. Its one caller
// (MAGIC009) stores it to +9, as FxSize's do.
MLIB_EXPORT unsigned char __cdecl BattleActor_FxSizeB(void) {
    const unsigned char i = Mem(at::kActor)[0];
    if (i < 3) {
        const unsigned char* const r = PartyRecord(i);
        if (r[0x134] & 2) return FxSizeB_AltTable[Mem(kFormation)[0]];
        return FxSizeB_Table[r[0x89]];
    }
    return Mem(kEnemyFxSizeB + EnemyRecord(i)[0xF0] * 0x8Cu)[0];
}

// original 0x4FC2D0: Sprite_Current's +0xC and +0x10 (dwords) from the pair
// of shorts MagicFx_FormationOffsets[(MagicFx_FormationRows[byte 0x904B89] *
// 4 + (+8 & 3)) * 2 ..], both indices unchecked.
MLIB_EXPORT void __cdecl MagicFx_FormationOffset(void) {
    const unsigned row = MagicFx_FormationRows[Mem(kFormation)[0]] * 4u;
    SetLong(Sprite_Current + 0xC, MagicFx_FormationOffsets[(row + (Sprite_Current[8] & 3u)) * 2]);
    SetLong(Sprite_Current + 0x10, MagicFx_FormationOffsets[(row + (Sprite_Current[8] & 3u)) * 2 + 1]);
}

void MagicLib_Inject() {
    if (bof3::WantsShadow("magic_lib")) magic_lib::SelfTest();
    BOF3_INJECT(BuffPopup_Task);
    BOF3_INJECT(BuffPopup_Start);
    BOF3_INJECT(BuffPopup_Rise);
    BOF3_INJECT(BuffPopup_Fall);
    BOF3_INJECT(BuffPopup_End);
    BOF3_INJECT(BuffPopupAt_Task);
    BOF3_INJECT(BuffPopupAt_Start);
    BOF3_INJECT(BuffPopup_Draw);
    BOF3_INJECT(MagicFx_ApplyBuff);
    BOF3_INJECT(MagicFx_BuffPopup);
    BOF3_INJECT(MagicFx_LinkByDepth);
    BOF3_INJECT(MagicFx_StepToward);
    BOF3_INJECT(MagicFx_StepTowardPoint);
    BOF3_INJECT(MagicFx_StepAround);
    BOF3_INJECT(MagicFx_NearSprite3D);
    BOF3_INJECT(MagicFx_NearSprite);
    BOF3_INJECT(MagicFx_NearPoint3D);
    BOF3_INJECT(MagicFx_NearPoint);
    BOF3_INJECT(SpriteClut_SetStp);
    BOF3_INJECT(SpriteClut_ClearEntry31);
    BOF3_INJECT(SpriteClut_CopyToFxRow);
    BOF3_INJECT(SpriteClut_RestoreFxRow);
    BOF3_INJECT(MagicFx_CenterOnSide);
    BOF3_INJECT(BattleActor_FxSizeB);
    BOF3_INJECT(MagicFx_FormationOffset);
}
