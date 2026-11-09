// Moved here by tools/move_funcs.py from magic_s19.cpp (docs/source-layout-round.md).
#include "game/magic/shield_spark.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/magic_s19.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

#define MS19_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// The crystals' pool: 24 slots of a task's shape, flag bit 0 of byte 0 live.
constexpr ::bof3::image::Va kPool{0x68FA78};

constexpr std::uint32_t kPoolStride = 0x84;

constexpr unsigned kPoolCount = 0x18;

constexpr ::bof3::image::Va kActorKind{0x904B35};   // u8: 4 is the one kind Shield_Kind leaves alone

constexpr ::bof3::image::Va kAbility{0x904B80};     // u16: the id of the action being cast

// The GTE's input vectors (four SVECTORs) and the effects' scratch words.
// kV0: now Prim_VertexScratch (symbols.toml).
// kV1: now Prim_VertexScratch[1] (symbols.toml).
// kV2: now Prim_VertexScratch[2] (symbols.toml).
// kV3: now Prim_VertexScratch[3] (symbols.toml).
constexpr ::bof3::image::Va kRadius{SharedScratch_addr}; // s16 (dword for MAGIC086's draws)

constexpr ::bof3::image::Va kHeight{0x903852};   // s16

constexpr ::bof3::image::Va kAngle{0x903854};    // s16 (dword: MAGIC086's inner radius)

constexpr ::bof3::image::Va kShade{0x903856};    // u16

// kBlue: now Scratch_Swap (symbols.toml).
constexpr ::bof3::image::Va kRed{0x903858};  // u16 (0x90385C a dword: MAGIC086's lift)

constexpr ::bof3::image::Va kGreen{0x90385A};

constexpr ::bof3::image::Va kFace{0x90385E};     // u16

using Handler = void (__cdecl*)();

using DiscFn = void (__cdecl*)(unsigned);

using LinkSortedFn = void (__cdecl*)(unsigned long, unsigned long, long*, unsigned char*, unsigned, unsigned, unsigned);

void Bump(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Drop(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

unsigned char* Pool(unsigned i) { return Mem(::bof3::Img(kPool.v) + i * kPoolStride); }

unsigned char* Task(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + (slot & 0xFF) * at::kTaskStride); }

unsigned char* Tint(unsigned i) { return Mem(::bof3::Img(MoveScript_TintRecords_addr) + 12 * i); }

unsigned char* Enemy(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }

unsigned char* Member(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

// A .data handler table's cell, read in place.
Handler Cell(std::uint32_t table, unsigned index) {
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

std::int16_t S16(std::uint32_t a) { return static_cast<std::int16_t>(Word(Mem(a))); }

void Put16(std::uint32_t a, std::uint32_t v) { SetWord(Mem(a), v & 0xFFFF); }

// imul then sar, as the original's 32-bit registers wrap.
std::int32_t MulSar(std::int32_t a, std::int32_t b, int n) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> n;
}

std::int32_t ShlSar(std::int32_t a, int l, int r) { return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) << l) >> r; }

std::int32_t Add(std::int32_t a, std::int32_t b) {
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b));
}

const short* Vec(std::uint32_t a) { return reinterpret_cast<const short*>(Mem(a)); }

float* At(unsigned char* prim, unsigned off) { return reinterpret_cast<float*>(prim + off); }

unsigned char* Prim() { return Gfx_PacketNext; }

void DrawModeAndCommit(unsigned tpage) {
    MH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, tpage, 0);
    MH_CALL(Gfx_CommitPrim)(5, 0xC);
}

// The sin / cos helpers at radius r: (f(angle) * r) >> 12, as a word.
std::int32_t Sin(std::int32_t angle) { return MH_CALL(Math_Sin)(angle); }

std::int32_t Cos(std::int32_t angle) { return MH_CALL(Math_Cos)(angle); }

// One aura task for a side member not out: kind 1 parameter 0x22, owned by
// the effect, +3 its index on the side, +4 the effect's kind, +0xB its
// order, +9 its delay, the member's position; the effect's +0xB counts it.
// The writes are in the original's order (the slot may be the effect's own).
void SpawnAura(unsigned index, const unsigned char* record, unsigned char& order, unsigned char& delay) {
    const unsigned char slot = MH_CALL(BattleTask_Create)(1, 0x22);
    const std::int32_t x = rv::As<records::SpriteHead>(record)->x;
    unsigned char* const t = Task(slot);
    records::BattleTask* const tt = rv::As<records::BattleTask>(t);
    unsigned char* const cur = Sprite_Current;
    tt->owner = static_cast<std::int32_t>(Low32(cur));
    t[1] = 0;
    t[3] = static_cast<unsigned char>(index);
    t[4] = cur[4];
    t[0xB] = order;
    t[9] = delay;
    tt->x = x;
    tt->z = rv::As<records::SpriteHead>(record)->z;
    rv::Store(tt->height, rv::As<records::SpriteHead>(record)->height);
    Bump(order);
    delay = static_cast<unsigned char>(delay + 0x10);
    Bump(cur[0xB]);
}

// The side member an aura stands on: an enemy record by +3 when the target
// byte has 0x40, else a party record.
unsigned char* AuraMember(bool enemies, unsigned index) { return enemies ? Enemy(index) : Member(index); }

// Every crystal phase after the first: its screen point, the MAGIC082 disc
// under it (radius `radius`), and the crystal under the actor's matrix.
void CrystalFrame(unsigned radius) {
    MH_AT(DiscFn, bof3::addr::MagicFx_DrawDiscRadius)(radius);
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(ShieldSpark_DrawCrystal)();
    MH_CALL(Gte_PopMatrix)();
}

// +0x14 += +0x20, then the word +0x3E (the height) += the word +0x14.
void Lift() {
    unsigned char* const cur = Sprite_Current;
    rv::Store(rv::As<records::BattleTask>(cur)->step_y, Add(rv::As<records::BattleTask>(cur)->step_y, rv::As<records::BattleTask>(cur)->work_20));
    unsigned char* const now = Sprite_Current;
    rv::Store<unsigned>(rv::As<records::BattleTask>(now)->height_whole, rv::As<records::BattleTask>(now)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(now)->step_y));
}

// One face of the crystal: a POLY_G3 from the apex (0, 0, height) to the
// points at angles (step & 0x1F) << 7 and ((step + 8) & 0x1F) << 7, radius
// 0x903850; its colour from `colours` by (face + 4 * kind) * 3; its depth
// (Gte_RotAverage3) into depths[face]. Returns the Sprite_Current it read last.
unsigned char* CrystalFace(std::uint32_t colours, std::int32_t step, unsigned char* cur, std::int16_t height, long* depths) {
    const std::int16_t face = static_cast<std::int16_t>((step - static_cast<std::int32_t>(cur[9])) / 8);
    Put16(::bof3::Img(kFace.v), static_cast<std::uint16_t>(face));
    const std::int32_t row = (face + static_cast<std::int32_t>(cur[4]) * 4) * 3;
    Put16(::bof3::Img(kRed.v), Mem(colours + row)[0]);
    Put16(::bof3::Img(kGreen.v), Mem(colours + row + 1)[0]);
    Put16(::bof3::Img(Prim_VertexScratch_addr), 0);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
    Put16(::bof3::Img(Scratch_Swap_addr), Mem(colours + row + 2)[0]);
    const std::uint32_t a0 = (static_cast<std::uint32_t>(step) & 0x1F) << 7;
    Put16(::bof3::Img(kAngle.v), a0);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, static_cast<std::uint16_t>(height));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, static_cast<std::uint32_t>(MulSar(Sin(static_cast<std::int16_t>(a0)), S16(::bof3::Img(kRadius.v)), 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, static_cast<std::uint32_t>(MulSar(Cos(S16(::bof3::Img(kAngle.v))), S16(::bof3::Img(kRadius.v)), 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
    const std::uint32_t a1 = (static_cast<std::uint32_t>(step + 8) & 0x1F) << 7;
    Put16(::bof3::Img(kAngle.v), a1);
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, static_cast<std::uint32_t>(MulSar(Sin(static_cast<std::int16_t>(a1)), S16(::bof3::Img(kRadius.v)), 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, static_cast<std::uint32_t>(MulSar(Cos(S16(::bof3::Img(kAngle.v))), S16(::bof3::Img(kRadius.v)), 12)));
    unsigned char* const prim = Prim();
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
    MH_CALL(Gpu_SetPolyG3)(prim);
    MH_CALL(Gpu_SetSemiTrans)(prim, 0);
    long p;
    const long depth =
        MH_CALL(Gte_RotAverage3)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), At(prim, 8), At(prim, 0x18), At(prim, 0x28), &p);
    const std::int32_t slot = (step - static_cast<std::int32_t>(Sprite_Current[9])) / 8;
    if (slot < 0 || slot > 3)
        bof3::Fatal("ShieldSpark_DrawCrystal: face %d, past the four-entry depth array", static_cast<int>(slot));
    depths[slot] = depth;
    for (unsigned v = 0; v < 3; ++v) {
        prim[4 + 0x10 * v] = Mem(::bof3::Img(kRed.v))[0];
        prim[5 + 0x10 * v] = Mem(::bof3::Img(kGreen.v))[0];
        prim[6 + 0x10 * v] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
    }
    Gfx_PacketNext = Gfx_PacketNext + 0x34;
    return Sprite_Current;
}

}  // namespace

// original 0x4C1470: the kind-2 task (Magic_Rows row 94). Its phase +1
// through a two-entry table the original builds on its stack - Shield_Start
// and 0x4E5200 (MAGIC131's: the done flag and free once +0xB is 0) - then
// every live crystal of the pool run: Sprite_Current and the owner (0x93B940)
// set to the slot and its +0x80, ShieldSpark_Dispatch, both put back to
// what they were after the phase. The index is not checked by the original;
// ours aborts past the table.
MS19_EXPORT void __cdecl Shield_Task(void) {
    static constexpr std::uint32_t kPhases[2] = {bof3::addr::Shield_Start, bof3::addr::MagicFx_EndWhenChildrenDone};
    const unsigned phase = Sprite_Current[1];
    if (phase >= 2) bof3::Fatal("Shield_Task: phase %u, past the two-entry table", phase);
    magic_harness::Phase(kPhases[phase])();
    unsigned char* const cur = Sprite_Current;
    const std::int32_t owner = Long(Mem(::bof3::Img(BattleE2_Owner_addr)));
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const slot = Pool(i);
        records::BattleTask* const tslot = rv::As<records::BattleTask>(slot);
        if (!(slot[0] & 1)) continue;
        Sprite_Current = slot;
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), static_cast<std::int32_t>(tslot->owner));
        MH_CALL(ShieldSpark_Dispatch)();
        SetLong(Mem(::bof3::Img(BattleE2_Owner_addr)), owner);
        Sprite_Current = cur;
    }
}

// original 0x4C14F0: Shield_Task's entry 0. The crystal pool's bytes 0..2
// cleared; +4 the kind (Shield_Kind); +0xB 0; the phase on. Then an aura for
// every member of the target side (0x904B44 bit 0x40: the eight enemies, else
// the three party members) that Battle_ActorIsOut says is not out, delays 1,
// 0x11, 0x21, ...; the CLUT strip's rows at 0x812980 / 0x8129A0 (sixteen
// words each) from the copies 0x4000 below, Gfx_ClutStripDirty set; sound
// 0x100.
MS19_EXPORT void __cdecl Shield_Start(void) {
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const slot = Pool(i);
        slot[0] = 0;
        slot[1] = 0;
        slot[2] = 0;
    }
    const unsigned char kind = MH_CALL(Shield_Kind)();
    Sprite_Current[4] = kind;
    Sprite_Current[0xB] = 0;
    Bump(Sprite_Current[1]);
    unsigned char order = 0;
    unsigned char delay = 1;
    if (Mem(::bof3::Img(at::kTarget))[0] & 0x40) {
        for (unsigned i = 0; i < 8; ++i)
            if (!MH_CALL(Battle_ActorIsOut)(i + 3)) SpawnAura(i, Enemy(i), order, delay);
    } else {
        for (unsigned i = 0; i < 3; ++i)
            if (!MH_CALL(Battle_ActorIsOut)(i)) SpawnAura(i, Member(i), order, delay);
    }
    for (unsigned n = 0; n < 16; ++n) {
        SetWord(Mem(::bof3::Img(0x812980) + 2 * n), Word(Mem(::bof3::Img(0x80E980) + 2 * n)));
        SetWord(Mem(::bof3::Img(0x8129A0) + 2 * n), Word(Mem(::bof3::Img(0x80E9A0) + 2 * n)));
    }
    Gfx_ClutStripDirty = 1;
    MH_CALL(Sound_PlayById)(0x100);
}

// original 0x4C1710: the effect's kind, always 0 here. On the way, unless the
// actor kind byte 0x904B35 is 4, an action id 0x136 at 0x904B80 becomes 0x53
// (this overlay's own ability id). MAGIC082's twin (0x4C04F0, S18's) maps
// three ids to 0x52 / 0x54 / 0x55 and kinds 0 / 1 / 2.
MS19_EXPORT unsigned char __cdecl Shield_Kind(void) {
    if (Mem(::bof3::Img(kActorKind.v))[0] != 4 && Word(Mem(::bof3::Img(kAbility.v))) == 0x136) SetWord(Mem(::bof3::Img(kAbility.v)), 0x53);
    return 0;
}

// original 0x4C1730: kind 1 parameter 0x22 - jmp [ShieldAura_Types + 4 * +1].
MS19_EXPORT void __cdecl ShieldAura_Dispatch(void) { Cell(::bof3::Img(ShieldAura_Types_addr), Sprite_Current[1])(); }

// original 0x4C1750: ShieldAura_Types[0]. The phase +2 through
// ShieldAura_Phases; then, while the slot is live (byte 0) and past its first
// phase, the halo and the disc under the actor's matrix.
MS19_EXPORT void __cdecl ShieldAura_Task(void) {
    Cell(::bof3::Img(ShieldAura_Phases_addr), Sprite_Current[2])();
    const unsigned char* const cur = Sprite_Current;
    if (cur[0] == 0 || cur[2] == 0) return;
    MH_CALL(MagicFx_PushActorMatrix)();
    MH_CALL(ShieldAura_DrawHalo)();
    MH_CALL(ShieldAura_DrawDisc)();
    MH_CALL(Gte_PopMatrix)();
}

// original 0x4C1790: ShieldAura_Phases[0]. +9 down; at 0 the member's tint
// released and a new one set (0, 0, 0, 1), its record kept in +0xA; +9 0;
// the phase on.
MS19_EXPORT void __cdecl ShieldAura_Wait(void) {
    Drop(Sprite_Current[9]);
    const unsigned char* const cur = Sprite_Current;
    if (cur[9] != 0) return;
    unsigned char* const member = AuraMember((Mem(::bof3::Img(at::kTarget))[0] & 0x40) != 0, cur[3]);
    MH_CALL(Sprite_ReleaseTint)(member);
    const unsigned char tint = MH_CALL(Sprite_SetTint)(member, 0, 0, 0, 1);
    Sprite_Current[0xA] = tint;
    Sprite_Current[9] = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4C1820: ShieldAura_Phases[1]. The tint record's three levels up
// by one while its first is not 8 (only the first is tested); +9 up; at 0x10
// three crystals from the pool - +3 the aura's old +0xB plus 0, 0x10, 0x20
// (their angles), +0xB 0, 10, 20, +9 delays 1, 9, 17, +0xA 1, +4 the kind,
// owned by the aura, which counts them in its +0xB (cleared first) - and the
// phase on. A full pool skips a crystal.
MS19_EXPORT void __cdecl ShieldAura_Rise(void) {
    unsigned char* const cur = Sprite_Current;
    if (Tint(cur[0xA])[2] != 8) {
        Bump(Tint(cur[0xA])[2]);
        Bump(Tint(cur[0xA])[3]);
        Bump(Tint(cur[0xA])[4]);
    }
    Bump(cur[9]);
    unsigned char* const now = Sprite_Current;
    if (now[9] != 0x10) return;
    unsigned char angle = now[0xB];
    now[0xB] = 0;
    unsigned char order = 0;
    for (unsigned char delay = 1; delay < 0x19; delay = static_cast<unsigned char>(delay + 8)) {
        const unsigned char slot = MH_CALL(ShieldSpark_Alloc)();
        if (slot != 0xFF) {
            unsigned char* const e = Pool(slot);
            records::BattleTask* const te = rv::As<records::BattleTask>(e);
            unsigned char* const aura = Sprite_Current;
            te->owner = static_cast<std::int32_t>(Low32(aura));
            e[1] = 0;
            e[2] = 0;
            e[3] = angle;
            e[4] = aura[4];
            e[0xB] = order;
            e[9] = delay;
            e[0xA] = 1;
            Bump(aura[0xB]);
        }
        angle = static_cast<unsigned char>(angle + 0x10);
        order = static_cast<unsigned char>(order + 0xA);
    }
    Bump(Sprite_Current[2]);
}

// original 0x4C1940: ShieldAura_Phases[3] (between: 0x4EBA30, MAGIC151's,
// waits for +0xB - the crystals - to reach 0). The tint's three levels down
// while its first is not 0; +9 down; at 0 the member's tint released, the
// member flashed, 0x4FB790(kind, side index), +0xB up and the phase on
// (ShieldAura_Phases[4], 0x4D1AA0, MAGIC105's: the owner's count down and
// free once +0xB is 0 again).
MS19_EXPORT void __cdecl ShieldAura_Fade(void) {
    const bool enemies = (Mem(::bof3::Img(at::kTarget))[0] & 0x40) != 0;
    unsigned char* const cur = Sprite_Current;
    const unsigned char index = cur[3];
    const unsigned char actor = static_cast<unsigned char>(enemies ? index + 3 : index);
    unsigned char* const member = AuraMember(enemies, index);
    if (Tint(cur[0xA])[2] != 0) {
        Drop(Tint(cur[0xA])[2]);
        Drop(Tint(cur[0xA])[3]);
        Drop(Tint(cur[0xA])[4]);
    }
    Drop(cur[9]);
    if (Sprite_Current[9] != 0) return;
    MH_CALL(Sprite_ReleaseTint)(member);
    MH_CALL(BattleActor_Flash)(actor);
    const unsigned char* const now = Sprite_Current;
    MH_CALL(MagicFx_BuffPopup)(now[4], now[3]);
    Bump(Sprite_Current[0xB]);
    Bump(Sprite_Current[2]);
}

// original 0x4C1A20: the aura's disc - sixteen POLY_G3s about the actor's
// origin, radius 128 (Math_Sin / Math_Cos by 0x100 steps), semi-transparent,
// the centre grey (+9 * 5) and the rim ShieldAura_DiscColours[kind] * +9,
// between draw modes 0x35 and 0x15 on OT slot 5.
MS19_EXPORT void __cdecl ShieldAura_DrawDisc(void) {
    DrawModeAndCommit(0x35);
    const unsigned char* const cur = Sprite_Current;
    Put16(::bof3::Img(kShade.v), cur[9] * 5u);
    Put16(::bof3::Img(kRed.v), Mem(::bof3::Img(ShieldAura_DiscColours_addr))[cur[4] * 3u] * static_cast<std::uint32_t>(cur[9]));
    Put16(::bof3::Img(kGreen.v), Mem(::bof3::Img(ShieldAura_DiscColours_addr) + 1)[cur[4] * 3u] * static_cast<std::uint32_t>(cur[9]));
    Put16(::bof3::Img(Scratch_Swap_addr), Mem(::bof3::Img(ShieldAura_DiscColours_addr) + 2)[cur[4] * 3u] * static_cast<std::uint32_t>(cur[9]));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, static_cast<std::uint32_t>(ShlSar(Sin(0), 7, 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, static_cast<std::uint32_t>(ShlSar(Cos(0), 7, 12)));
    for (std::int32_t a = 0x100; a < 0x1100; a += 0x100) {
        Put16(::bof3::Img(Prim_VertexScratch_addr), 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x10)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, static_cast<std::uint32_t>(ShlSar(Sin(a), 7, 12)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, static_cast<std::uint32_t>(ShlSar(Cos(a), 7, 12)));
        unsigned char* const prim = Prim();
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        MH_CALL(Gpu_SetPolyG3)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        long depth;
        MH_CALL(Gte_RotTransPers3)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), At(prim, 8), At(prim, 0x18), At(prim, 0x28), &depth);
        MH_CALL(Gte_PrimDepths3_10B)(prim);
        prim[4] = Mem(::bof3::Img(kShade.v))[0];
        prim[5] = Mem(::bof3::Img(kShade.v))[0];
        prim[6] = Mem(::bof3::Img(kShade.v))[0];
        prim[0x14] = Mem(::bof3::Img(kRed.v))[0];
        prim[0x15] = Mem(::bof3::Img(kGreen.v))[0];
        prim[0x16] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        prim[0x24] = Mem(::bof3::Img(kRed.v))[0];
        prim[0x25] = Mem(::bof3::Img(kGreen.v))[0];
        prim[0x26] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        MH_CALL(Gfx_CommitPrim)(5, 0x34);
    }
    DrawModeAndCommit(0x15);
}

// original 0x4C1C20: the aura's halo - sixteen POLY_G4s of the annulus
// between radius 128 and 256, the inner edge ShieldAura_HaloColours[kind] *
// +9, the outer black (1, 1, 1: nothing under additive blending).
MS19_EXPORT void __cdecl ShieldAura_DrawHalo(void) {
    DrawModeAndCommit(0x35);
    const unsigned char* const cur = Sprite_Current;
    Put16(::bof3::Img(kRed.v), Mem(::bof3::Img(ShieldAura_HaloColours_addr))[cur[4] * 3u] * static_cast<std::uint32_t>(cur[9]));
    Put16(::bof3::Img(kGreen.v), Mem(::bof3::Img(ShieldAura_HaloColours_addr) + 1)[cur[4] * 3u] * static_cast<std::uint32_t>(cur[9]));
    Put16(::bof3::Img(Scratch_Swap_addr), Mem(::bof3::Img(ShieldAura_HaloColours_addr) + 2)[cur[4] * 3u] * static_cast<std::uint32_t>(cur[9]));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, static_cast<std::uint32_t>(ShlSar(Sin(0), 7, 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, static_cast<std::uint32_t>(ShlSar(Cos(0), 7, 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, static_cast<std::uint32_t>(ShlSar(Sin(0), 8, 12)));
    Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, static_cast<std::uint32_t>(ShlSar(Cos(0), 8, 12)));
    for (std::int32_t a = 0x100; a < 0x1100; a += 0x100) {
        Put16(::bof3::Img(Prim_VertexScratch_addr), Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x8)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 2, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x18)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 2, Word(Mem(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8, static_cast<std::uint32_t>(ShlSar(Sin(a), 7, 12)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 2, static_cast<std::uint32_t>(ShlSar(Cos(a), 7, 12)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18, static_cast<std::uint32_t>(ShlSar(Sin(a), 8, 12)));
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 2, static_cast<std::uint32_t>(ShlSar(Cos(a), 8, 12)));
        unsigned char* const prim = Prim();
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x18 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x10 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 0x8 + 4, 0);
        Put16(::bof3::Img(Prim_VertexScratch_addr) + 4, 0);
        MH_CALL(Gpu_SetPolyG4)(prim);
        MH_CALL(Gpu_SetSemiTrans)(prim, 1);
        long depth;
        MH_CALL(Gte_RotTransPers4)(Vec(::bof3::Img(Prim_VertexScratch_addr)), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x8), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x10), Vec(::bof3::Img(Prim_VertexScratch_addr) + 0x18), At(prim, 8), At(prim, 0x18), At(prim, 0x28),
                                   At(prim, 0x38), &depth);
        MH_CALL(Gte_PrimDepths4_10B)(prim);
        prim[4] = Mem(::bof3::Img(kRed.v))[0];
        prim[5] = Mem(::bof3::Img(kGreen.v))[0];
        prim[6] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        prim[0x14] = Mem(::bof3::Img(kRed.v))[0];
        prim[0x15] = Mem(::bof3::Img(kGreen.v))[0];
        prim[0x16] = Mem(::bof3::Img(Scratch_Swap_addr))[0];
        prim[0x24] = 1;
        prim[0x25] = 1;
        prim[0x26] = 1;
        prim[0x34] = 1;
        prim[0x35] = 1;
        prim[0x36] = 1;
        MH_CALL(Gfx_CommitPrim)(5, 0x44);
    }
    DrawModeAndCommit(0x15);
}

// original 0x4C1E70: a crystal - jmp [ShieldSpark_Types + 4 * +1].
MS19_EXPORT void __cdecl ShieldSpark_Dispatch(void) { Cell(::bof3::Img(ShieldSpark_Types_addr), Sprite_Current[1])(); }

// original 0x4C1E90: ShieldSpark_Types[0] - jmp [ShieldSpark_Phases + 4 * +2].
MS19_EXPORT void __cdecl ShieldSpark_Task(void) { Cell(::bof3::Img(ShieldSpark_Phases_addr), Sprite_Current[2])(); }

// original 0x4C1EB0: ShieldSpark_Phases[0]. +9 down; at 0 the crystal placed
// on the aura's circle - the angle (+0xB & 0x1F) << 7 (kept in 0x903854), x
// and z the owner's plus (Math_Sin / Math_Cos * 176) >> 3, y the owner's -
// with +0x14 0x40 (its lift), +0x20 -4 (the lift's step), +9 0, +0xA 0xC
// (frames), +0xC 1, +0x10 0; the phase on.
MS19_EXPORT void __cdecl ShieldSpark_Place(void) {
    Drop(Sprite_Current[9]);
    const unsigned char* const cur = Sprite_Current;
    if (cur[9] != 0) return;
    const std::uint32_t angle = (cur[0xB] & 0x1Fu) << 7;
    Put16(::bof3::Img(kAngle.v), angle);
    const std::int32_t s = Sin(static_cast<std::int16_t>(angle));
    rv::Store(rv::Current<records::BattleTask>()->x, Add(MulSar(s, 176, 3), rv::As<records::SpriteHead>(Owner())->x));
    const std::int32_t c = Cos(S16(::bof3::Img(kAngle.v)));
    rv::Store(rv::Current<records::BattleTask>()->z, Add(MulSar(c, 176, 3), rv::As<records::SpriteHead>(Owner())->z));
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    rv::Store(rv::Current<records::BattleTask>()->step_y, 0x40);
    rv::Store(rv::Current<records::BattleTask>()->work_20, -4);
    Sprite_Current[9] = 0;
    Sprite_Current[0xA] = 0xC;
    rv::Current<records::BattleTask>()->step_x = 1;
    rv::Current<records::BattleTask>()->step_z = 0;
    Bump(Sprite_Current[2]);
}

// original 0x4C1FA0: ShieldSpark_Phases[1]. The screen point; the disc (radius
// 0x18 + Rand() & 3) and the crystal; every other frame (Frame_Counter bit 0)
// +9 (its spin) up; the lift; +0xA down, and at 0 +0x14 -16, +0x20 4, +0xA 8,
// the phase on.
MS19_EXPORT void __cdecl ShieldSpark_Rise(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    CrystalFrame((static_cast<std::uint32_t>(MH_CALL(Rand)()) & 3) + 0x18);
    if (Frame_Counter & 1) Bump(Sprite_Current[9]);
    Lift();
    Drop(Sprite_Current[0xA]);
    unsigned char* const cur = Sprite_Current;
    if (cur[0xA] != 0) return;
    rv::Store(rv::As<records::BattleTask>(cur)->step_y, -16);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 4);
    Sprite_Current[0xA] = 8;
    Bump(Sprite_Current[2]);
}

// original 0x4C2040: ShieldSpark_Phases[2]. As the rise, and at 0 +0xA
// becomes 0x1C - +0xB (the crystals wait in turn); the phase on.
MS19_EXPORT void __cdecl ShieldSpark_Fall(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    CrystalFrame((static_cast<std::uint32_t>(MH_CALL(Rand)()) & 3) + 0x18);
    if (Frame_Counter & 1) Bump(Sprite_Current[9]);
    Lift();
    Drop(Sprite_Current[0xA]);
    unsigned char* const cur = Sprite_Current;
    if (cur[0xA] != 0) return;
    cur[0xA] = static_cast<unsigned char>(0x1C - cur[0xB]);
    Bump(Sprite_Current[2]);
}

// original 0x4C20D0: ShieldSpark_Phases[3]. The screen point; the disc -
// radius 0x40 + Rand() & 3 once the spin (+0xC) is 4 and +0xA is below 5,
// else 0x18 + Rand() & 3 - and the crystal. Below a spin of 4 (signed) +0x10
// counts and every eighth frame the spin goes up; +9 += the spin. At a spin of
// 4, +0xA down, and at 0: unless +3 has a bit of 0xF0, sound 0x101 (+3 odd)
// or 0x102; +0x10, +0x14 and +0x20 8; the phase on.
MS19_EXPORT void __cdecl ShieldSpark_Spin(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    const unsigned char* const cur = Sprite_Current;
    const records::BattleTask* const tcur = rv::AsConst<records::BattleTask>(cur);
    const bool wide = tcur->step_x == 4 && cur[0xA] < 5;
    const std::uint32_t r = static_cast<std::uint32_t>(MH_CALL(Rand)());
    CrystalFrame((r & 3) + (wide ? 0x40u : 0x18u));
    unsigned char* now = Sprite_Current;
    if (rv::As<records::BattleTask>(now)->step_x < 4) {
        rv::Store(rv::As<records::BattleTask>(now)->step_z, Add(rv::As<records::BattleTask>(now)->step_z, 1));
        now = Sprite_Current;
        if ((now[0x10] & 7) == 0) {
            rv::Store(rv::As<records::BattleTask>(now)->step_x, Add(rv::As<records::BattleTask>(now)->step_x, 1));
            now = Sprite_Current;
        }
    }
    now[9] = static_cast<unsigned char>(now[9] + now[0xC]);
    now = Sprite_Current;
    if (rv::As<records::BattleTask>(now)->step_x != 4) return;
    Drop(now[0xA]);
    now = Sprite_Current;
    if (now[0xA] != 0) return;
    const unsigned char side = now[3];
    if (!(side & 0xF0)) MH_CALL(Sound_PlayById)(side & 1 ? 0x101 : 0x102);
    rv::Current<records::BattleTask>()->step_z = 8;
    rv::Store(rv::Current<records::BattleTask>()->step_y, 8);
    rv::Store(rv::Current<records::BattleTask>()->work_20, 8);
    Bump(Sprite_Current[2]);
}

// original 0x4C21C0: ShieldSpark_Phases[4]. The screen point; the disc (0x20 +
// Rand() & 3) and the crystal; +0x10 += 4; +9 += the spin; +0xB += 2, the
// angle (+0xB & 0x1F) << 7 in 0x903854, the radius +0xA * +0x10 (16 bits) +
// 0xB0 in 0x903850; x and z the owner's plus (Math_Sin / Math_Cos * radius) >>
// 3: an outward spiral. +0xA up; at 0x10 the owner's count +0xB down and the
// slot freed (0x4F6290, a tail jump).
MS19_EXPORT void __cdecl ShieldSpark_Orbit(void) {
    MH_CALL(BattleActor_UpdateScreenXY)();
    CrystalFrame((static_cast<std::uint32_t>(MH_CALL(Rand)()) & 3) + 0x20);
    rv::Store(rv::Current<records::BattleTask>()->step_z, Add(rv::Current<records::BattleTask>()->step_z, 4));
    Sprite_Current[9] = static_cast<unsigned char>(Sprite_Current[9] + Sprite_Current[0xC]);
    Sprite_Current[0xB] = static_cast<unsigned char>(Sprite_Current[0xB] + 2);
    const unsigned char* const cur = Sprite_Current;
    const std::uint32_t angle = (cur[0xB] & 0x1Fu) << 7;
    Put16(::bof3::Img(kAngle.v), angle);
    Put16(::bof3::Img(kRadius.v), static_cast<std::uint32_t>(cur[0xA]) * Word(cur + 0x10) + 0xB0);
    const std::int32_t s = Sin(static_cast<std::int16_t>(angle));
    rv::Store(rv::Current<records::BattleTask>()->x, Add(MulSar(s, S16(::bof3::Img(kRadius.v)), 3), rv::As<records::SpriteHead>(Owner())->x));
    const std::int32_t c = Cos(S16(::bof3::Img(kAngle.v)));
    rv::Store(rv::Current<records::BattleTask>()->z, Add(MulSar(c, S16(::bof3::Img(kRadius.v)), 3), rv::As<records::SpriteHead>(Owner())->z));
    Bump(Sprite_Current[0xA]);
    if (Sprite_Current[0xA] != 0x10) return;
    Drop(Owner()[0xB]);
    magic_harness::Phase(bof3::addr::MagicFx_FreeCurrentRecord)();
}

// original 0x4C22D0: the crystal - two four-sided pyramids back to back, the
// upper apex at height 0x50 and the lower at -0x50, radius 0x30, turned by +9;
// eight semi-opaque POLY_G3s coloured by ShieldSpark_TopColours /
// _BottomColours[kind][face], each pyramid's four linked by their depths
// (0x4FB880) at the crystal's map cell, after a draw mode 0x15 linked there.
// The loop bound (+9 + 0x20) and the face are read again from Sprite_Current
// every face, as the original reads them.
MS19_EXPORT void __cdecl ShieldSpark_DrawCrystal(void) {
    MH_CALL(Gpu_SetDrawMode)(Prim(), 0, 1, 0x15, 0);
    {
        const unsigned char* const cur = Sprite_Current;
        const records::BattleTask* const tcur = rv::AsConst<records::BattleTask>(cur);
        MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tcur->x),
                                    static_cast<unsigned long>(tcur->z), 0, 0xC);
    }
    unsigned char* cur = Sprite_Current;
    unsigned char* const first = Prim();
    long depths[4];
    for (std::int32_t step = cur[9]; step < static_cast<std::int32_t>(cur[9]) + 0x20; step += 8) {
        Put16(::bof3::Img(kHeight.v), 0x50);
        Put16(::bof3::Img(kRadius.v), 0x30);
        cur = CrystalFace(::bof3::Img(ShieldSpark_TopColours_addr), step, cur, 0x50, depths);
    }
    MH_AT(LinkSortedFn, bof3::addr::MagicFx_LinkByDepth)(static_cast<unsigned long>(Long(cur + 0x34)),
                                                         static_cast<unsigned long>(Long(cur + 0x38)), depths, first, 4,
                                                         0x34, 0);
    cur = Sprite_Current;
    unsigned char* const second = Prim();
    for (std::int32_t step = cur[9]; step < static_cast<std::int32_t>(cur[9]) + 0x20; step += 8)
        cur = CrystalFace(::bof3::Img(ShieldSpark_BottomColours_addr), step, cur, static_cast<std::int16_t>(-S16(::bof3::Img(kHeight.v))), depths);
    MH_AT(LinkSortedFn, bof3::addr::MagicFx_LinkByDepth)(static_cast<unsigned long>(Long(cur + 0x34)),
                                                         static_cast<unsigned long>(Long(cur + 0x38)), depths, second,
                                                         4, 0x34, 0);
}

// original 0x4C2790: a free crystal slot - the first of the 24 whose byte 0
// has bit 0 clear, the bit set, its index; 0xFF when none is.
MS19_EXPORT unsigned char __cdecl ShieldSpark_Alloc(void) {
    for (unsigned i = 0; i < kPoolCount; ++i) {
        unsigned char* const slot = Pool(i);
        if (slot[0] & 1) continue;
        slot[0] = static_cast<unsigned char>(slot[0] | 1);
        return static_cast<unsigned char>(i);
    }
    return 0xFF;
}

void ShieldSpark_Inject() {
    BOF3_INJECT(Shield_Task);
    BOF3_INJECT(Shield_Start);
    BOF3_INJECT(Shield_Kind);
    BOF3_INJECT(ShieldAura_Dispatch);
    BOF3_INJECT(ShieldAura_Task);
    BOF3_INJECT(ShieldAura_Wait);
    BOF3_INJECT(ShieldAura_Rise);
    BOF3_INJECT(ShieldAura_Fade);
    BOF3_INJECT(ShieldAura_DrawDisc);
    BOF3_INJECT(ShieldAura_DrawHalo);
    BOF3_INJECT(ShieldSpark_Dispatch);
    BOF3_INJECT(ShieldSpark_Task);
    BOF3_INJECT(ShieldSpark_Place);
    BOF3_INJECT(ShieldSpark_Rise);
    BOF3_INJECT(ShieldSpark_Fall);
    BOF3_INJECT(ShieldSpark_Spin);
    BOF3_INJECT(ShieldSpark_Orbit);
    BOF3_INJECT(ShieldSpark_DrawCrystal);
    BOF3_INJECT(ShieldSpark_Alloc);
}
