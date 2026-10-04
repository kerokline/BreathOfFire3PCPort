// Round twelve group FE1: the field engine's resident code 0x52D080..0x533BA0,
// taken with the scenario harness in field mode (scenario_harness.h, docs/
// scenario_harness.md section 7). docs/field_e1.md has each function, every
// caller, the tables and the fuzz.
//
//   FieldPanel_DrawHeader        0x52D080  two panel sprites (0, 1) at (x + 8, y) and (x + 0x108, y)
//   FieldPanel_DrawKindIcon      0x52D0C0  panel sprite 2 at (x, y), its CLUT and UV by a kind byte
//   FieldPanel_DrawKindRow       0x52D140  a kind's row: three sprites, its name, count and points
//   FieldPanel_DrawTotal         0x52D320  the total of the points, its rank (0x9045F4) and the rank's sprites
//   FieldPanel_DrawMessage       0x52D560  sprite 0xC and a script-pool message at (x, y)
//   FieldPanel_DrawShade         0x52D5C0  a half-transparent tile over the 320 x 240 screen
//   FieldPanel_DrawBox3          0x52D610  a window, its sprite frame and three script-pool lines
//   FieldPanel_DrawBox2          0x52D750  a wider window and two lines
//   Inventory_Holds38To4DAt99    0x52D880  al: 22 or more slots of item 0x38..0x4D held at 99
//   FieldPanel_DrawBlink         0x52D8C0  sprite 0x47 every other 8 frames while 0x939A28
//   Field_PathClear              0x52EC20  al: the way from the leader to (x, z) walkable, x then z
//   Field_FormActionState        0x52F4F0  leader state 6: the party set's handler (0x660A44) until done
//   PartyAction_ScriptEnd        0x52F5C0  a party action's step: the script once, done when it ends
//   Field_PassageTrigger         0x52F8F0  passage step 3: the object trigger on the passage's event
//   Field_JumpState              0x52F950  leader state 8: jmp Field_JumpSteps[+2]
//   Field_JumpBegin .. JumpIn3   0x52F970..0x52FB50  the jump's steps and sub-steps (FE2's helpers)
//   Field_ContentState           0x52FBB0  leader state 11: the content step (0x660A1C), the script tick
//   Field_ContentTake            0x52FBD0  a field object's content taken: zenny or an item, its flag
//   Field_ContentEnd             0x52FD90  the object after: closed back, removed or its cell marked
//   Field_AreaRunState           0x52FE90  leader state 12: area 104's or 121's run
//   Field_GiveZenny              0x5307C0  the zenny found: sound, "n" printed, message 5, Zenny_Add
//   Field_CellAroundLarge        0x531120  al: a cell of `code` around a sprite whose +0x34 / +0x38 are not whole
//   Field_CellAroundSide         0x531540  al: the two cells of `code` on one side, the facing turned to it
//   Field_GatewayExit            0x531820  al: the area's gateway exit, set pending
//   Party_PlaceAtSlots           0x532C10  the members to the slot positions, the camera to the leader
//   Party_ScriptTicks            0x532D10  each member's movement script, one tick
//   Party_PlacesByList           0x532D50  the members' places by the second list, then poses or a drop-in
//   Field_LeaderPlaceOffset      0x533690  the leader placed at (x, z) set off by the facing
//   Field_PendingJumpTurn        0x5338B0  pending jump 1: the member released by its facing (kind 2 / 3)
//   Field_PendingJumpKind4       0x5338F0  pending jump 2: released with kind 4
//   Field_PendingDrop            0x533900  pending jump 3: the next member dropped in from above
//   Field_PendingNext            0x5339A0  al: the next member of the pending jump made current
//   Field_PendingRelease         0x533A50  al: the member at 0x904EF2 released into a kind's state
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Sprite_Current and Field_State are read
// afresh at every use, as the original reads [0x937F88] / [0x905D98] (a callee
// may move either). Where the original would jump through a table entry that
// is not code, or walk its stack past a table, ours aborts with a message
// (docs/field_e1.md section 7).
#include "game/field_e1.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_e1_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = field_e1::at;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char& B(std::uint32_t a) { return *At(a); }
std::uint16_t W(std::uint32_t a) { return Word(At(a)); }
void SetW(std::uint32_t a, unsigned v) { SetWord(At(a), v); }
std::int32_t L(std::uint32_t a) { return Long(At(a)); }
std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Fs() { return Field_State; }
unsigned char* Member(unsigned i) { return ObjTrio + i * at::kObjStride; }
unsigned char* Object(unsigned i) { return Sprite_Objects + i * at::kSpriteStride; }
short S16(std::uint32_t v) { return static_cast<short>(v); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<std::uint32_t>(v);
    const std::uint32_t s = static_cast<std::uint32_t>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

// --- the callees nobody owns, and FE2's, by address -----------------------------
using Void0 = void (__cdecl*)();
using Byte0 = unsigned char (__cdecl*)();
void DrawMode(unsigned index, unsigned slot) { SH_AT(void (__cdecl*)(unsigned, unsigned), at::kDrawMode)(index, slot); }
unsigned char* DrawSprite(unsigned sprite, unsigned slot, int x, int y) {
    return SH_AT(unsigned char* (__cdecl*)(unsigned, unsigned, int, int), at::kDrawSprite)(sprite, slot, x, y);
}
unsigned KindPoints(unsigned kind, unsigned count) { return SH_AT(unsigned (__cdecl*)(unsigned, unsigned), at::kKindPoints)(kind, count); }
unsigned KindTotal() { return SH_AT(unsigned (__cdecl*)(), at::kKindTotal)(); }
void DrawQuad(int x, int y, unsigned height, unsigned which) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kDrawQuad)(x, y, height, which);
}
void Fe2(std::uint32_t address) { SH_AT(Void0, address)(); }
unsigned char Fe2Al(std::uint32_t address) { return SH_AT(Byte0, address)(); }

void Text(int x, int y, int colour, int count, std::uint32_t text) {
    SH_CALL(Text_DrawAt)(x, y, colour, count, At(text));
}
// A script-pool message: MessagePools + its u16 offset.
std::uint32_t PoolText(std::uint32_t offset_cell) { return at::kPools + W(offset_cell); }

// A .data dispatch table read in place: the index is a whole byte and
// unchecked, as the original's; where the entry is not code (past the run of
// code-pointer tables the entry lies in) the original jumps into data - ours
// aborts. While the fuzz runs, the entries are its recorders (outside .text).
using Handler = void (__cdecl*)();
Handler CodeAt(std::uint32_t table, unsigned index, const char* who) {
    const std::uint32_t cell = table + 4u * index;
    const auto entry = static_cast<std::uint32_t>(L(cell));
    if (!scenario_harness::g_active && (entry < 0x401000 || entry >= 0x5C3000))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X, not code - past its table (the original jumps there)", who, index,
                    (unsigned)entry, (unsigned)cell);
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(entry));
}

bool ScriptFlag8() { return ((B(at::kFlags2Lo) | B(at::kFlagsLo)) & 8) != 0; }

}  // namespace

// --- the panel draws (callers 0x466BE0..0x4670C0, 0x528A90..0x52A420: Capcom's) ----

// original 0x52D080: the draw mode (0, 1); panel sprites 0 at (x + 8, y) and
// 1 at (x + 0x108, y).
extern "C" void __cdecl FieldPanel_DrawHeader(int x, int y) {
    DrawMode(0, 1);
    DrawSprite(0, 1, x + 8, y);
    DrawSprite(1, 1, x + 0x108, y);
}

// original 0x52D0C0: the draw mode (1, 1), panel sprite 2 at (x, y); unless
// the kind is 0xFF, the primitive's CLUT word +0x16 = ((kind >> 4) + 0x1EB) <<
// 6 | (kind & 0xF), u +0x14 = (kind & 3) << 6, v +0x15 = (kind >> 2) * 0x28
// (a byte).
extern "C" void __cdecl FieldPanel_DrawKindIcon(int x, int y, unsigned kind) {
    DrawMode(1, 1);
    unsigned char* const p = DrawSprite(2, 1, x, y);
    const auto k = static_cast<unsigned char>(kind);
    if (k == 0xFF) return;
    SetWord(p + 0x16, ((static_cast<unsigned>(k >> 4) + 0x1EB) << 6) | (k & 0xFu));
    p[0x14] = static_cast<unsigned char>((k & 3u) << 6);
    p[0x15] = static_cast<unsigned char>((k >> 2) * 0x28u);
}

// original 0x52D140: a kind's row `row` - the draw mode (0, 1); sprites 3 at
// (0x68, 0x42 - row * 20), 4 at (0x20 - row * 25, 0x70), 5 at (0xE0 + row *
// 25, 0x70); the name into the text scratch 0x904BA0 (kind 0x16 its own
// 16 bytes, 0xFF eight '?' and a NUL, any other the 22-byte record's first 16)
// drawn at (0x70, 0x45 - row * 20); unless 0xFF, the count and the kind's
// points for it (0x52CE60) printed and drawn at (0x27 - row * 25, 0x73) and
// (0xE7 + row * 25, 0x73).
extern "C" void __cdecl FieldPanel_DrawKindRow(unsigned kind, unsigned count, unsigned row) {
    const auto k = static_cast<unsigned char>(kind);
    const int r = static_cast<unsigned char>(row);
    DrawMode(0, 1);
    DrawSprite(3, 1, 0x68, 0x42 - r * 20);
    DrawSprite(4, 1, 0x20 - r * 25, 0x70);
    DrawSprite(5, 1, 0xE0 + r * 25, 0x70);
    unsigned char* const name = At(scenario_harness::at::kTextBuffer);
    if (k == 0x16) {
        std::memcpy(name, At(at::kKindName16), 16);
    } else if (k == 0xFF) {
        std::memset(name, '?', 8);
        name[8] = 0;
    } else {
        std::memcpy(name, At(at::kKindNames + k * 22u), 16);
    }
    Text(0x70, 0x45 - r * 20, 0, 0xF, Key(name));
    if (k == 0xFF) return;
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(name), reinterpret_cast<const char*>(At(at::kCountFormat)),
                         static_cast<unsigned>(static_cast<unsigned char>(count)));
    Text(0x27 - r * 25, 0x73, 0, 3, Key(name));
    const unsigned points = KindPoints(kind, static_cast<unsigned char>(count)) & 0xFFFFu;
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(name), reinterpret_cast<const char*>(At(at::kCountFormat)), points);
    Text(r * 25 + 0xE7, 0x73, 0, 3, Key(name));
}

// original 0x52D320: the draw mode (3, 1); sprites 6 at (x, y) and 7 at
// (x + 0x58, y); the total 0x52CED0 against the thirteen thresholds the
// original builds on its stack - the rank is the first it is below, written
// to 0x9045F4 - and the rank's record (four bytes, also built on the stack):
// its sprite at (x + 0x1D, y + 0x1D), and sprite 0x48 at (x + b1 + 0x1D, y +
// 0x21) when b2, again at (x + b1 + 0x24, y + 0x21) when b3; the total printed
// at (x + 0x3D, y + 9).
// As the original has it: the last threshold is 0xFFFF and the compare is
// "below", so a total of 0xFFFF walks on past the thresholds into the frame;
// ours aborts there.
extern "C" void __cdecl FieldPanel_DrawTotal(int x, int y) {
    static const std::uint16_t kThresholds[13] = {100, 300, 500, 1000, 1500, 2000, 3000, 4000, 5000, 7000, 9000, 9500, 0xFFFF};
    static const unsigned char kRanks[13][4] = {
        {8, 0x38, 0, 0},   {8, 0x38, 1, 0},   {8, 0x38, 1, 1},   {9, 0x3A, 0, 0}, {9, 0x3A, 1, 0},
        {9, 0x3A, 1, 1},   {0xA, 0x49, 0, 0}, {0xA, 0x49, 1, 0}, {0xA, 0x49, 1, 1}, {0xB, 0x4E, 0, 0},
        {0xB, 0x4E, 1, 0}, {0xB, 0x4E, 1, 1}, {0x49, 0x28, 0, 0},
    };
    DrawMode(3, 1);
    DrawSprite(6, 1, x, y);
    DrawSprite(7, 1, x + 0x58, y);
    const auto total = static_cast<std::uint16_t>(KindTotal());
    unsigned rank = 0;
    while (total >= kThresholds[rank]) {
        if (++rank == 13)
            bof3::Fatal("FieldPanel_DrawTotal: a total of 0x%X is at or above every threshold (the original walks its stack past them)",
                        (unsigned)total);
    }
    B(at::kRank) = static_cast<unsigned char>(rank);
    const unsigned char* const r = kRanks[rank];
    DrawSprite(r[0], 1, x + 0x1D, y + 0x1D);
    if (r[2] != 0) {
        const int shifted = x + r[1];
        DrawSprite(0x48, 1, shifted + 0x1D, y + 0x21);
        if (r[3] != 0) DrawSprite(0x48, 1, shifted + 0x24, y + 0x21);
    }
    unsigned char* const text = At(scenario_harness::at::kTextBuffer);
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(text), reinterpret_cast<const char*>(At(at::kTotalFormat)),
                         static_cast<unsigned>(total));
    Text(x + 0x3D, y + 9, 0, 5, Key(text));
}

// original 0x52D560: the draw mode (3, 1), sprite 0xC at (x, y); unless the
// id's word is 0xFFFF, the script pool's message `id` at (x + 0xA, y + 8).
extern "C" void __cdecl FieldPanel_DrawMessage(int x, int y, unsigned id) {
    DrawMode(3, 1);
    DrawSprite(0xC, 1, x, y);
    const auto message = static_cast<std::uint16_t>(id);
    if (message == 0xFFFF) return;
    Text(x + 0xA, y + 8, 0, 0xFF, PoolText(at::kPools + message * 2u));
}

// original 0x52D5C0: the draw mode (4, 2); a tile at the packet cursor - its
// colour 0x20 0x20 0x20, (0, 0), 320.0 x 240.0 (floats) -, half-transparent
// (mode 1), committed to slot 2 (0x1C bytes).
extern "C" void __cdecl FieldPanel_DrawShade(void) {
    DrawMode(4, 2);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    p[6] = 0x20;
    p[5] = 0x20;
    p[4] = 0x20;
    SetLong(p + 8, std::bit_cast<std::int32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    SetLong(p + 0xC, 0);
    SetLong(p + 0x14, std::bit_cast<std::int32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetLong(p + 0x18, 0x43700000);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// The sprite frame of a panel window: its top row (0x25, 0x26 x `across`,
// 0x27, 0x28) at y, the two sides (0x468950) from y + 0x18 over `side`, the
// bottom row (0x30, 0x31 x `bottom`, 0x32) at y + `low`.
namespace {
void Frame(int x, int y, unsigned across, int top_end, int right, unsigned side, int low, unsigned bottom) {
    DrawMode(9, 1);
    DrawSprite(0x25, 1, x, y);
    for (unsigned i = 0; i < across; ++i) DrawSprite(0x26, 1, x + static_cast<int>(i) * 8 + 0x20, y);
    DrawSprite(0x27, 1, x + top_end, y);
    DrawSprite(0x28, 1, x + right, y);
    DrawQuad(x, y + 0x18, side, 0);
    DrawQuad(x + right, y + 0x18, side, 1);
    DrawSprite(0x30, 1, x, y + low);
    for (unsigned i = 0; i < bottom; ++i) DrawSprite(0x31, 1, x + static_cast<int>(i) * 8 + 8, y + low);
    DrawSprite(0x32, 1, x + right, y + low);
}
}  // namespace

// original 0x52D610: Menu_DrawBox(x, y + 1, 0x64, 0x38, 0x80, the style
// 0x903A5A); the frame (8 across, the sides 0x20 high, the bottom at + 0x38);
// the script pool's messages by the words 0x8035FA at (x + 0x28, y + 6),
// 0x8035FC at (x + 6, y + 0x19) and 0x8035FE at (x + 6, y + 0x29).
extern "C" void __cdecl FieldPanel_DrawBox3(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y + 1, 0x64, 0x38, 0x80, B(at::kStyle));
    Frame(x, y, 8, 0x48, 0x60, 0x20, 0x38, 0xB);
    Text(x + 0x28, y + 6, 0, 0xFF, PoolText(at::kPools + 0x7A));
    Text(x + 6, y + 0x19, 0, 0xFF, PoolText(at::kPools + 0x7C));
    Text(x + 6, y + 0x29, 0, 0xFF, PoolText(at::kPools + 0x7E));
}

// original 0x52D750: Menu_DrawBox(x, y + 1, 0xB3, 0x42, 0x80, the style); the
// frame (15 across, the sides 0x28 high, the bottom at + 0x40); the messages
// by 0x8035F8 at (x + 0x32, y + 6) and 0x803602 at (x + 6, y + 0x19).
extern "C" void __cdecl FieldPanel_DrawBox2(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y + 1, 0xB3, 0x42, 0x80, B(at::kStyle));
    Frame(x, y, 0xF, 0x98, 0xB0, 0x28, 0x40, 0x15);
    Text(x + 0x32, y + 6, 0, 0xFF, PoolText(at::kPools + 0x78));
    Text(x + 6, y + 0x19, 0, 0xFF, PoolText(at::kPools + 0x82));
}

// original 0x52D880: for each id 0x38..0x4D, the slots of the consumables'
// list (Inventory_IdLists[0], 128) holding it with the count 99 counted; al 1
// when there are 22 or more (a duplicate slot counts twice).
extern "C" unsigned char __cdecl Inventory_Holds38To4DAt99(void) {
    int n = 0;
    for (unsigned id = 0x38; id < 0x4E; ++id)
        for (unsigned i = 0; i < 0x80; ++i)
            if (B(at::kItemIds + i) == id && B(at::kItemCounts + i) == 0x63) ++n;
    return n >= 0x16 ? 1 : 0;
}

// original 0x52D8C0: while 0x939A28 and Frame_Counter bit 3, the draw mode
// (3, 1) and sprite 0x47 at (0x1A, 0x5C).
extern "C" void __cdecl FieldPanel_DrawBlink(void) {
    if (B(at::kBlink) == 0 || (Frame_Counter & 8) == 0) return;
    DrawMode(3, 1);
    DrawSprite(0x47, 1, 0x1A, 0x5C);
}

// --- the way, the leader's states ---------------------------------------------------

// original 0x52EC20: whether the way from the leader to (x, z) is open - from
// the leader's x (read again each step) in steps of half a unit toward x,
// |x - leader| >> 15 + 1 times, at z; then from the leader's z toward z at x.
// Each step: the ground (MapView_GroundAt) within 0x40 of the leader's height
// +0x3E, and not Field_WayBlocked(.., raised, the ground). al 1 open, 0 not.
// Called by Field_SwapGather and Field_SwapExchange (event_leader.cpp).
extern "C" unsigned char __cdecl Field_PathClear(long x, long z, unsigned raised) {
    const std::int32_t leader_z = L(at::kLeaderZ);
    const std::int32_t along_x = Abs32(static_cast<std::int32_t>(x) - L(at::kLeaderX)) >> 15;
    const std::int32_t along_z = Abs32(static_cast<std::int32_t>(z) - leader_z) >> 15;
    const short xh = S16(static_cast<std::uint32_t>(x) >> 16), zh = S16(static_cast<std::uint32_t>(z) >> 16);
    const short lxh = static_cast<short>(W(at::kLeaderX + 2)), lzh = static_cast<short>(W(at::kLeaderZ + 2));
    const int sx = xh > lxh ? 1 : xh == lxh ? 0 : -1;
    const int sz = zh > lzh ? 1 : zh == lzh ? 0 : -1;
    std::int32_t offset = 0;
    for (std::int32_t i = 0; i <= along_x; ++i) {
        const auto px = static_cast<long>(static_cast<std::uint32_t>(L(at::kLeaderX)) + static_cast<std::uint32_t>(offset));
        const long ground = SH_CALL(MapView_GroundAt)(px, z);
        if (Abs32(static_cast<short>(W(at::kLeaderHeight)) - S16(static_cast<std::uint32_t>(ground))) > 0x40) return 0;
        if (SH_CALL(Field_WayBlocked)(px, z, raised, ground)) return 0;
        offset = static_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + (static_cast<std::uint32_t>(sx) << 15));
    }
    offset = 0;
    for (std::int32_t i = 0; i <= along_z; ++i) {
        const auto pz = static_cast<long>(static_cast<std::uint32_t>(L(at::kLeaderZ)) + static_cast<std::uint32_t>(offset));
        const long ground = SH_CALL(MapView_GroundAt)(x, pz);
        if (Abs32(static_cast<short>(W(at::kLeaderHeight)) - S16(static_cast<std::uint32_t>(ground))) > 0x40) return 0;
        if (SH_CALL(Field_WayBlocked)(x, pz, raised, ground)) return 0;
        offset = static_cast<std::int32_t>(static_cast<std::uint32_t>(offset) + (static_cast<std::uint32_t>(sz) << 15));
    }
    return 1;
}

// original 0x52F4F0: leader state 6 - with Field_InputHeld not 0, Field_State
// +0x137 = 0; else the handler of 0x660A44 for the loaded party set (0x90412C
// & 0x7F; 19 entries, unchecked). Then, once +0x137 is 0: the pose +8, +9 =
// 0, member 0 cleared, state 1, +0xB = 0, Field_LeaderStand (a tail jump).
extern "C" void __cdecl Field_FormActionState(void) {
    if (Field_InputHeld != 0) {
        Fs()[0x137] = 0;
    } else {
        const unsigned set = B(at::kPartySet) & 0x7Fu;
        CodeAt(at::kFormActions, set, "Field_FormActionState")();
    }
    if (Fs()[0x137] != 0) return;
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
    Sc()[9] = 0;
    SH_CALL(Member_ClearState)(0);
    Sc()[1] = 1;
    Sc()[0xB] = 0;
    SH_CALL(Field_LeaderStand)();
}

// original 0x52F5C0: a party action's step (48 cells of the PartyAction run
// tables 0x65F998..): Sprite_ScriptTickOnce, and when it answers not 0,
// Field_State +0x137 = 0 (the action done).
extern "C" void __cdecl PartyAction_ScriptEnd(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() != 0) Fs()[0x137] = 0;
}

// original 0x52F8F0: passage step 3 (Field_PassageSteps[3]) - unless
// Field_Request is 2: an object record on the stack (0xA4 bytes, only +0x86
// and +0x88 written: Field_State +0x12A and the word +0x12C), handed to
// Field_ObjectTrigger unless +0x86 is 0xFF; step 2.
// As the original has it: the rest of the record is whatever the frame held
// (the trigger reads +0x89, the word's high byte, and hands the record on).
extern "C" void __cdecl Field_PassageTrigger(void) {
    if (Field_Request == 2) return;
    alignas(4) unsigned char object[0xA4];
    std::memset(object, 0, sizeof object);
    const unsigned char* const fs = Fs();
    object[0x86] = fs[0x12A];
    SetWord(object + 0x88, Word(fs + 0x12C));
    if (object[0x86] != 0xFF) SH_AT(void (__cdecl*)(unsigned char*), at::kObjectTrigger)(object);
    Sc()[2] = 2;
}

// original 0x52F950: leader state 8, the jump - jmp Field_JumpSteps[+2].
extern "C" void __cdecl Field_JumpState(void) { CodeAt(at::kJumpSteps, Sc()[2], "Field_JumpState")(); }

// original 0x52F970: jump step 0 - 0x535FC0 (the first pose), step 2.
extern "C" void __cdecl Field_JumpBegin(void) {
    Fe2(at::kJumpPose);
    Sc()[2] = 2;
}

// original 0x52F980: jump step 1 - jmp Field_JumpOutSteps[+3].
extern "C" void __cdecl Field_JumpOut(void) { CodeAt(at::kJumpOutSteps, Sc()[3], "Field_JumpOut")(); }

// original 0x52F9A0: jump-out 0 - 0x535FE0 (Field_JumpSetUp's caller); with
// +5 0 and neither script flag's bit 3, Field_JumpCamera; +3 = 1.
extern "C" void __cdecl Field_JumpOut0(void) {
    Fe2(at::kJumpSetUp);
    if (Sc()[5] == 0 && !ScriptFlag8()) SH_CALL(Field_JumpCamera)();
    Sc()[3] = 1;
}

// original 0x52F9E0 / 0x52FA00 / 0x52FA20: jump-out 1..3 - FE2's step, and
// when it answers al not 0, +3 one on.
extern "C" void __cdecl Field_JumpOut1(void) {
    if (Fe2Al(at::kJumpOut1)) Sc()[3] = 2;
}
extern "C" void __cdecl Field_JumpOut2(void) {
    if (Fe2Al(at::kJumpOut2)) Sc()[3] = 3;
}
extern "C" void __cdecl Field_JumpOut3(void) {
    if (Fe2Al(at::kJumpOut3)) Sc()[3] = 4;
}

// original 0x52FA40: jump-out 4 - 0x536170; when al: step 2, +3 = 0.
extern "C" void __cdecl Field_JumpOut4(void) {
    if (!Fe2Al(at::kJumpOut4)) return;
    Sc()[2] = 2;
    Sc()[3] = 0;
}

// original 0x52FA60: jump step 2 - by Field_InputHeld: bit 12 0x5364D0, else
// bit 14 0x536550, else any bit of the button map word 0x903580 0x5365D0; then
// unless a script flag's bit 3, MapView_SetElevation(+0x3E) and
// Field_JumpCamera (a tail jump).
extern "C" void __cdecl Field_JumpAir(void) {
    const std::uint16_t held = Field_InputHeld;
    if (held & 0x1000) Fe2(at::kJumpAirA);
    else if (held & 0x4000) Fe2(at::kJumpAirB);
    else if (W(at::kButtonMap) & held) Fe2(at::kJumpAirC);
    if (ScriptFlag8()) return;
    SH_CALL(MapView_SetElevation)(static_cast<short>(Word(Sc() + 0x3E)));
    SH_CALL(Field_JumpCamera)();
}

// original 0x52FAC0: jump step 3 - jmp Field_JumpInSteps[+3].
extern "C" void __cdecl Field_JumpIn(void) { CodeAt(at::kJumpInSteps, Sc()[3], "Field_JumpIn")(); }

// original 0x52FAE0: jump-in 0 - 0x536290, +3 = 1.
extern "C" void __cdecl Field_JumpIn0(void) {
    Fe2(at::kJumpIn0);
    Sc()[3] = 1;
}

// original 0x52FAF0: jump-in 1 - 0x5362D0; when al, +3 = 2.
extern "C" void __cdecl Field_JumpIn1(void) {
    if (Fe2Al(at::kJumpIn1)) Sc()[3] = 2;
}

// original 0x52FB10: jump-in 2 - 0x5363C0; when al: with +5 0 and neither
// script flag's bit 3, Field_JumpCamera; +3 = 3.
extern "C" void __cdecl Field_JumpIn2(void) {
    if (!Fe2Al(at::kJumpIn2)) return;
    if (Sc()[5] == 0 && !ScriptFlag8()) SH_CALL(Field_JumpCamera)();
    Sc()[3] = 3;
}

// original 0x52FB50: jump-in 3 - a tail jump to 0x536440.
extern "C" void __cdecl Field_JumpIn3(void) { Fe2(at::kJumpIn3); }

// original 0x52FBB0: leader state 11 - the step of Field_ContentSteps[+2],
// then Sprite_ScriptTick (a tail jump).
extern "C" void __cdecl Field_ContentState(void) {
    CodeAt(at::kContentSteps, Sc()[2], "Field_ContentState")();
    SH_CALL(Sprite_ScriptTick)();
}

// The field object Field_State +0x139 names (Sprite_Objects, the byte whole
// and unchecked), read again at every use as the original reads it.
namespace {
unsigned char* ContentObject() { return Object(Fs()[0x139]); }
}  // namespace

// original 0x52FBD0: content step 0 - the object's flag (+5, in the bank
// 0x9040CC) set: system message 1. Else by +0x18: 0xFF the zenny +0x19 * 40
// (Field_GiveZenny), the flag (unless 0xFF) and the count 0x904140; an item
// (category +0x18, id +0x19): its 16-byte name to Text_Records and
// Inventory_Add(.., 1): taken - the flag (unless 0xFF), sound 0x106, message
// 2, the count; no room - message 3. Then, except after message 1, unless the
// object's +0xB bit 0 its animation 1 (Sprite_Current lent to it). Field_Request
// 2, +2 one on.
// As the original has it: the category and id reach Item_NamePtr and
// Inventory_Add as dwords whose upper bytes are the frame's (both read the low
// byte only).
extern "C" void __cdecl Field_ContentTake(void) {
    if (SH_CALL(Flags_Test)(At(at::kObjectFlags), ContentObject()[5])) {
        SH_CALL(Msg_OpenSystem)(1);
    } else {
        const unsigned char* const o = ContentObject();
        const unsigned char item = o[0x19];
        const unsigned char category = o[0x18];
        if (category == 0xFF) {
            SH_CALL(Field_GiveZenny)(item * 40u);
            const unsigned char flag = ContentObject()[5];
            if (flag != 0xFF) SH_CALL(Flags_Set)(At(at::kObjectFlags), flag);
            SetLong(At(at::kContentCount), L(at::kContentCount) + 1);
        } else {
            const unsigned char* const name = SH_CALL(Item_NamePtr)(category, item);
            std::memcpy(At(bof3::addr::Text_Records), name, 16);
            if (SH_CALL(Inventory_Add)(category, item, 1)) {
                const unsigned char flag = ContentObject()[5];
                if (flag != 0xFF) SH_CALL(Flags_Set)(At(at::kObjectFlags), flag);
                SH_CALL(Sound_PlayEffect)(0x106);
                SH_CALL(Msg_OpenSystem)(2);
                SetLong(At(at::kContentCount), L(at::kContentCount) + 1);
            } else {
                SH_CALL(Msg_OpenSystem)(3);
            }
        }
        unsigned char* const object = ContentObject();
        if (!(object[0xB] & 1)) {
            unsigned char* const kept = Sprite_Current;
            Sprite_Current = object;
            SH_CALL(Sprite_SetAnimation)(1);
            Sprite_Current = kept;
        }
    }
    unsigned char* const sc = Sc();
    Field_Request = 2;
    sc[2] = static_cast<unsigned char>(sc[2] + 1);
}

// original 0x52FD90: content step 1 - once Field_Request is 0: after system
// message 3 (the message word 0x7DEE48), the object back to animation 0
// unless its +0xB bit 0; else bit 0 removes the object (+0 = 0), or with its
// +0x34 and +0x38 fractions 0 its cell (+0x36, +0x3A) becomes 0x10
// (AreaMap_SetByte). Then with Field_InputFlags bit 5 state 0xC, else the pose
// +8 and state 1; step 0.
extern "C" void __cdecl Field_ContentEnd(void) {
    if (Field_Request != 0) return;
    if (W(at::kMessageWord) == 3) {
        unsigned char* const object = ContentObject();
        if (!(object[0xB] & 1)) {
            unsigned char* const kept = Sprite_Current;
            Sprite_Current = object;
            SH_CALL(Sprite_SetAnimation)(0);
            Sprite_Current = kept;
        }
    } else {
        unsigned char* const object = ContentObject();
        if (object[0xB] & 1) {
            object[0] = 0;
        } else if (Word(object + 0x34) == 0 && Word(object + 0x38) == 0) {
            SH_CALL(AreaMap_SetByte)(Word(object + 0x36), Word(object + 0x3A), 0x10);
        }
    }
    if (Field_InputFlags & 0x20) {
        Sc()[1] = 0xC;
        Sc()[2] = 0;
        return;
    }
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8]);
    Sc()[1] = 1;
    Sc()[2] = 0;
}

// original 0x52FE90: leader state 12 - in area 0x68 Area104_LeaderRun, else
// Area121_LeaderRun (tail jumps).
extern "C" void __cdecl Field_AreaRunState(void) {
    if (Game_AreaNumber == 0x68) SH_CALL(Area104_LeaderRun)();
    else SH_CALL(Area121_LeaderRun)();
}

// original 0x5307C0 (PSX 0x801B6E50, the sibling's Field_GiveZenny): sound
// 0x106; the amount printed into Text_Records by Area08_MessageFormat; system
// message 5; Field_Request 2; Zenny_Add(amount, 0).
extern "C" void __cdecl Field_GiveZenny(unsigned amount) {
    SH_CALL(Sound_PlayEffect)(0x106);
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(At(bof3::addr::Text_Records)), reinterpret_cast<const char*>(At(at::kZennyFormat)),
                         amount);
    SH_CALL(Msg_OpenSystem)(5);
    Field_Request = 2;
    SH_CALL(Zenny_Add)(amount, 0);
}

// --- the cells around a sprite ------------------------------------------------------

namespace {
// The cell step of a direction (0x66971C, a whole byte unchecked): 1 reads as 2.
int DirStep(unsigned direction, unsigned which) {
    const auto v = static_cast<signed char>(B(at::kDirPairs + direction * 2u + which));
    return v == 1 ? 2 : v;
}
std::uint16_t CellX() { return W(at::kCellX); }
std::uint16_t CellZ() { return W(at::kCellZ); }
bool CellIs(unsigned char code) {
    return SH_CALL(AreaMap_ByteAt)(static_cast<short>(CellX()), static_cast<short>(CellZ())) == code;
}
// A match's new facing: an even facing 0 or 2 to 1, any other even to 5 (the
// runs along x); 0 or 6 to 7, any other to 3 (the runs along z).
unsigned char AlongX() {
    const unsigned char f = Sc()[8];
    Sc()[8] = f == 0 || f == 2 ? 1 : 5;
    return 1;
}
unsigned char AlongZ() {
    const unsigned char f = Sc()[8];
    Sc()[8] = f == 0 || f == 6 ? 7 : 3;
    return 1;
}
}  // namespace

// original 0x531120: the cells of `code` around Sprite_Current when its +0x34
// or +0x38 (the fractions) are not 0 - by which is not 0, rows of two or three
// cells ahead of it by its facing's step (0x66971C) are searched, the cell in
// 0x903850 / 0x903852; a match with an even facing turns it (AlongX /
// AlongZ), with the facing already along the row answers 1; a clean pass runs
// the other rows and, for the first shape, Field_CellAroundSide on the facings
// two to either side. al 1 found, 0 not. Called by Field_CellAround (a tail).
extern "C" unsigned char __cdecl Field_CellAroundLarge(unsigned code) {
    const unsigned char* const sc = Sc();
    const std::uint16_t fx = Word(sc + 0x34);
    if (fx == 0 && Word(sc + 0x38) == 0) return 0;
    const unsigned facing = sc[8];
    const std::uint16_t x = Word(sc + 0x36);
    const auto ahead_x = static_cast<std::uint16_t>(x + DirStep(facing, 0));
    const auto ahead_z = static_cast<std::uint16_t>(Word(sc + 0x3A) + DirStep(facing, 1));
    const auto c = static_cast<unsigned char>(code);
    if (fx != 0 && Word(sc + 0x38) != 0) {
        // both fractions: x .. x + 1 at the row ahead, then the column ahead
        SetW(at::kCellX, x);
        SetW(at::kCellZ, ahead_z);
        for (int i = 0; i <= 1; ++i) {
            if (CellIs(c)) {
                const unsigned char f = Sc()[8];
                if (!(f & 1)) return AlongX();
                if (f == 1 || f == 5) return 1;
            }
            SetW(at::kCellX, CellX() + 1);
        }
        SetW(at::kCellX, ahead_x);
        SetW(at::kCellZ, Word(Sc() + 0x3A));
        for (int i = 0; i <= 1; ++i) {
            if (CellIs(c)) {
                const unsigned char f = Sc()[8];
                if (!(f & 1)) return AlongZ();
                if (f == 7 || f == 3) return 1;
            }
            SetW(at::kCellZ, CellZ() + 1);
        }
        if (SH_CALL(Field_CellAroundSide)(static_cast<unsigned char>((Sc()[8] + 2) & 7), code)) return 1;
        if (SH_CALL(Field_CellAroundSide)(static_cast<unsigned char>((Sc()[8] - 2) & 7), code)) return 1;
        return 0;
    }
    if (fx != 0) {
        // x's fraction only: the column ahead, z - 1 .. z + 1
        SetW(at::kCellX, ahead_x);
        SetW(at::kCellZ, static_cast<std::uint16_t>(Word(sc + 0x3A) - 1));
        for (int i = 0; i <= 2; ++i) {
            if (CellIs(c)) {
                const unsigned char f = Sc()[8];
                if (!(f & 1)) return AlongZ();
                if (f == 7 || f == 3) return 1;
            }
            SetW(at::kCellZ, CellZ() + 1);
        }
        const unsigned char f = Sc()[8];
        if (f != 1 && f != 5) return 0;
        SetW(at::kCellX, static_cast<std::uint16_t>(Word(Sc() + 0x36) - 1));
        SetW(at::kCellZ, static_cast<std::uint16_t>(Word(Sc() + 0x3A) - 1));
        for (int i = 0; i <= 2; ++i) {
            if (CellIs(c)) {
                Sc()[8] = 7;
                return 1;
            }
            SetW(at::kCellZ, CellZ() + 1);
        }
        SetW(at::kCellX, static_cast<std::uint16_t>(Word(Sc() + 0x36) + 2));
        SetW(at::kCellZ, static_cast<std::uint16_t>(Word(Sc() + 0x3A) - 1));
        for (int i = 0; i <= 2; ++i) {
            if (CellIs(c)) {
                Sc()[8] = 3;
                return 1;
            }
            SetW(at::kCellZ, CellZ() + 1);
        }
        return 0;
    }
    // z's fraction only: the row ahead, x - 1 .. x + 1
    SetW(at::kCellX, static_cast<std::uint16_t>(x - 1));
    SetW(at::kCellZ, ahead_z);
    for (int i = 0; i <= 2; ++i) {
        if (CellIs(c)) {
            const unsigned char f = Sc()[8];
            if (!(f & 1)) return AlongX();
            if (f == 1 || f == 5) return 1;
        }
        SetW(at::kCellX, CellX() + 1);
    }
    const unsigned char f = Sc()[8];
    if (f != 7 && f != 3) return 0;
    SetW(at::kCellX, static_cast<std::uint16_t>(Word(Sc() + 0x36) - 1));
    SetW(at::kCellZ, static_cast<std::uint16_t>(Word(Sc() + 0x3A) - 1));
    for (int i = 0; i <= 2; ++i) {
        if (CellIs(c)) {
            Sc()[8] = 1;
            return 1;
        }
        SetW(at::kCellX, CellX() + 1);
    }
    SetW(at::kCellX, static_cast<std::uint16_t>(Word(Sc() + 0x36) - 1));
    SetW(at::kCellZ, static_cast<std::uint16_t>(Word(Sc() + 0x3A) + 2));
    for (int i = 0; i <= 2; ++i) {
        if (CellIs(c)) {
            Sc()[8] = 5;
            return 1;
        }
        SetW(at::kCellX, CellX() + 1);
    }
    return 0;
}

// original 0x531540: the two cells of `code` on the side `direction` of
// Sprite_Current (its whole x, z plus the direction's step): along x for
// directions 1 and 5, along z for 7 and 3, none for the rest; a match turns
// the facing +8 to `direction`. al 1 found. Called by Field_CellAroundLarge.
extern "C" unsigned char __cdecl Field_CellAroundSide(unsigned direction, unsigned code) {
    const auto d = static_cast<unsigned char>(direction);
    const auto c = static_cast<unsigned char>(code);
    const unsigned char* const sc = Sc();
    SetW(at::kCellX, static_cast<std::uint16_t>(Word(sc + 0x36) + DirStep(d, 0)));
    SetW(at::kCellZ, static_cast<std::uint16_t>(Word(sc + 0x3A) + DirStep(d, 1)));
    if (d == 1 || d == 5) {
        for (int i = 0; i <= 1; ++i) {
            if (CellIs(c)) {
                Sc()[8] = d;
                return 1;
            }
            SetW(at::kCellX, CellX() + 1);
        }
        return 0;
    }
    if (d == 7 || d == 3) {
        for (int i = 0; i <= 1; ++i) {
            if (CellIs(c)) {
                Sc()[8] = d;
                return 1;
            }
            SetW(at::kCellZ, CellZ() + 1);
        }
        return 0;
    }
    return 0;
}

// original 0x531820: the gateway exit - none (al 0) with Field_ScriptFlags
// bit 14, in party set 0xC, on a cell with an event (Field_CellHasEvent at the
// leader's whole x, z) or with Field_ScriptFlags2 bit 12; in area 0xBD the
// record of 0x660B08 by Cond_ByteFF == 0 (area, x, z bytes); elsewhere the
// area's record in the ten of 0x660AB8 (none: al 0). The exit's area to
// 0x937F82, kind 4 to 0x905B88, x << 16 to 0x903860, z << 16 to 0x90384C; al 1.
// Called by Field_LeaderCellEvent and FieldMenu_TopBarInput.
extern "C" unsigned char __cdecl Field_GatewayExit(void) {
    if (Field_ScriptFlags & 0x4000) return 0;
    if ((B(at::kPartySet) & 0x7F) == 0xC) return 0;
    if (Game_AreaNumber == 0xBD) {
        const std::uint32_t r = at::kGateway189 + (Cond_ByteFF == 0 ? 3u : 0u);
        B(at::kExitKind) = 4;
        SetW(at::kExitArea, B(r));
        SetLong(At(at::kExitX), static_cast<std::int32_t>(static_cast<std::uint32_t>(B(r + 1)) << 16));
        SetLong(At(at::kExitZ), static_cast<std::int32_t>(static_cast<std::uint32_t>(B(r + 2)) << 16));
        return 1;
    }
    if (SH_CALL(Field_CellHasEvent)(W(at::kLeaderX + 2), W(at::kLeaderZ + 2))) return 0;
    if (Field_ScriptFlags2 & 0x1000) return 0;
    const std::uint16_t area = Game_AreaNumber;
    for (std::uint32_t r = at::kGatewayExits; r < at::kGatewayExitsEnd; r += 8) {
        if (W(r) != area) continue;
        SetW(at::kExitArea, W(r + 2));
        B(at::kExitKind) = 4;
        SetLong(At(at::kExitZ), static_cast<std::int32_t>(static_cast<std::uint32_t>(W(r + 6)) << 16));
        SetLong(At(at::kExitX), static_cast<std::int32_t>(static_cast<std::uint32_t>(W(r + 4)) << 16));
        return 1;
    }
    return 0;
}

// --- the party's placements -----------------------------------------------------------

// original 0x532C10: each member's x, z from the slot positions 0x7E06E0; the
// leader - the member whose id +0x89 is the party list's first 0x904062, or
// the record after the last member when none is - unless 0x904AE5 bit 1:
// MoveScript_F3Divisor 0x20, Field_Kind2X / Z to its x, z, and
// MoveScript_FAWord the height change (AreaMap_Elevation there less
// MapView_Elevation, as a short) divided by the larger distance >> 13 as a
// byte, 0 when that byte is 0.
// As the original has it: with no member of that id, the record after the
// last member is read (with three members, past ObjTrio).
extern "C" void __cdecl Party_PlaceAtSlots(void) {
    const unsigned n = Field_MemberCount;
    for (unsigned i = 0; i < n; ++i) {
        SetLong(Member(i) + 0x34, L(at::kSlotPositions + 8 * i));
        SetLong(Member(i) + 0x38, L(at::kSlotPositions + 8 * i + 4));
    }
    unsigned k = 0;
    while (k < n && Member(k)[0x89] != B(at::kLeaderList)) ++k;
    if (B(at::kBattleBits) & 2) return;
    MoveScript_F3Divisor = 0x20;
    const std::int32_t x = Long(Member(k) + 0x34), z = Long(Member(k) + 0x38);
    const std::int32_t dx = Abs32(static_cast<std::int32_t>(static_cast<std::uint32_t>(Field_Kind2X) - static_cast<std::uint32_t>(x)));
    Field_Kind2X = x;
    const std::int32_t dz = Abs32(static_cast<std::int32_t>(static_cast<std::uint32_t>(Field_Kind2Z) - static_cast<std::uint32_t>(z)));
    Field_Kind2Z = z;
    const std::int32_t far = dx < dz ? dz : dx;
    const auto frames = static_cast<unsigned char>(far >> 13);
    const long elevation = SH_CALL(AreaMap_Elevation)(x, z);
    if (frames == 0) {
        MoveScript_FAWord = 0;
        return;
    }
    const short rise = S16(static_cast<std::uint32_t>(elevation) - static_cast<std::uint32_t>(MapView_Elevation));
    MoveScript_FAWord = static_cast<unsigned short>(rise / static_cast<int>(frames));
}

// original 0x532D10: each member in turn Field_State and Sprite_Current and
// Sprite_ScriptTick (the count read again after each).
extern "C" void __cdecl Party_ScriptTicks(void) {
    if (Field_MemberCount == 0) return;
    unsigned i = 0;
    do {
        Field_State = Member(i);
        Sprite_Current = Member(i);
        SH_CALL(Sprite_ScriptTick)();
    } while (++i < Field_MemberCount);
}

namespace {
// The slot in the second party list 0x904065 holding the id, among the first
// `n` (n when none holds it).
unsigned ListSlot(unsigned char id, unsigned n) {
    unsigned c = 0;
    while (c < n && B(at::kSecondList + c) != id) ++c;
    return c;
}
}  // namespace

// original 0x532D50: the members' x saved to 0x903850.. and each member given
// the x of the slot its id holds in the second list 0x904065; then z likewise,
// each member's ground (MapView_GroundAt) to +0x3E. Then with 0x904AE5 bit 7
// Party_DropIn(0x92BF18) and Field_MembersFrame; else each member made current
// and given the formation's pose (BattleFormation_Anims by 0x904AAC), +0x29 =
// 6, the ones after the leader +7 = 0 and +6 = +5 - 1, Field_MemberTimers and
// Sprite_SetAnimation(+8). The count is read again after each call.
// As the original has it: an id in none of the list's slots takes the saved
// word past the members' (0x903850 + 4 * count).
extern "C" void __cdecl Party_PlacesByList(void) {
    unsigned char count = Field_MemberCount;
    const unsigned n = count;
    if (n > 0) {
        for (unsigned i = 0; i < n; ++i) SetLong(At(at::kCellX + 4 * i), Long(Member(i) + 0x34));
        for (unsigned j = 0; j < n; ++j) SetLong(Member(j) + 0x34, L(at::kCellX + 4 * ListSlot(Member(j)[0x89], n)));
        for (unsigned i = 0; i < n; ++i) SetLong(At(at::kCellX + 4 * i), Long(Member(i) + 0x38));
        unsigned members = n;
        for (unsigned j = 0; j < members; ++j) {
            unsigned char* const m = Member(j);
            const std::int32_t z = L(at::kCellX + 4 * ListSlot(m[0x89], members));
            SetLong(m + 0x38, z);
            const long ground = SH_CALL(MapView_GroundAt)(Long(m + 0x34), z);
            count = Field_MemberCount;
            SetWord(m + 0x3E, static_cast<std::uint32_t>(ground));
            members = count;
        }
    }
    if (B(at::kBattleBits) & 0x80) {
        SH_CALL(Party_DropIn)(B(at::kDropEntry));
        SH_CALL(Field_MembersFrame)();
        return;
    }
    if (count == 0) return;
    unsigned i = 0;
    do {
        unsigned char* const m = Member(i);
        Field_State = m;
        Sprite_Current = m;
        m[8] = B(at::kFormationAnims + B(at::kFormation));
        Sc()[0x29] = 6;
        if (m != ObjTrio) {
            Sc()[7] = 0;
            Sc()[6] = static_cast<unsigned char>(Sc()[5] - 1);
        }
        SH_CALL(Field_MemberTimers)();
        SH_CALL(Sprite_SetAnimation)(Sc()[8]);
    } while (++i < Field_MemberCount);
}

// original 0x533690: the leader placed at (x, z) facing `direction` - alone,
// there; with others, set off by twice the step of 0x660B40 for direction >>
// 1 (thrice when its +0x70), and once more when any member after it has its
// +0x70. Called by Field_PartySetUp with Field_ScriptFlags bit 11.
extern "C" void __cdecl Field_LeaderPlaceOffset(long x, long z, unsigned direction) {
    const unsigned char n = Field_MemberCount;
    unsigned char* const leader = ObjTrio;
    if (n <= 1) {
        SetLong(leader + 0x34, static_cast<std::int32_t>(x));
        SetLong(leader + 0x38, static_cast<std::int32_t>(z));
        leader[8] = static_cast<unsigned char>(direction);
        return;
    }
    const std::uint32_t e = (direction & 0xFFu) >> 1;
    const auto ox = static_cast<std::uint32_t>(L(at::kPlaceOffsets + e * 8));
    const auto oz = static_cast<std::uint32_t>(L(at::kPlaceOffsets + e * 8 + 4));
    SetLong(leader + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(x) + ox * 2));
    SetLong(leader + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(z) + oz * 2));
    if (leader[0x70] != 0) {
        SetLong(leader + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(leader + 0x34)) + ox));
        SetLong(leader + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(leader + 0x38)) + oz));
    }
    leader[8] = static_cast<unsigned char>(direction);
    for (unsigned c = 1; c < n; ++c) {
        if (Member(c)[0x70] != 0) {
            SetLong(leader + 0x34, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(leader + 0x34)) + ox));
            SetLong(leader + 0x38, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(leader + 0x38)) + oz));
            return;
        }
    }
}

// --- the pending jump's members (Field_PendingJumps 1..3) ---------------------------

// original 0x5338B0: pending jump 1 - the member at 0x904EF2 released with
// kind 2 when its facing +8 is 5 or 3, else kind 3.
extern "C" void __cdecl Field_PendingJumpTurn(void) {
    const unsigned char f = Member(B(at::kPendingCount))[8];
    SH_CALL(Field_PendingRelease)(f == 5 || f == 3 ? 2 : 3);
}

// original 0x5338F0: pending jump 2 - released with kind 4.
extern "C" void __cdecl Field_PendingJumpKind4(void) { SH_CALL(Field_PendingRelease)(4); }

// original 0x533900: pending jump 3 - when Field_PendingNext makes a member
// current: its pose 0x3A (facing 3) or 0x3B, +0 bit 6 cleared, its height +0x3E
// the ground plus 0x7D0, Sprite_ClearSteps, +0x20 = -8, state 2 / 3 / 3, and
// Field_State +0x138 bit 1 cleared.
extern "C" void __cdecl Field_PendingDrop(void) {
    if (!SH_CALL(Field_PendingNext)()) return;
    SH_CALL(Sprite_EnsureAnimation)(Sc()[8] == 3 ? 0x3A : 0x3B);
    Sc()[0] = static_cast<unsigned char>(Sc()[0] & 0xBF);
    const long ground = SH_CALL(MapView_GroundAt)(Long(Sc() + 0x34), Long(Sc() + 0x38));
    SetWord(Sc() + 0x3E, static_cast<std::uint32_t>(ground) + 0x7D0);
    SH_CALL(Sprite_ClearSteps)();
    SetLong(Sc() + 0x20, -8);
    Sc()[1] = 2;
    Sc()[2] = 3;
    Sc()[3] = 3;
    Fs()[0x138] = static_cast<unsigned char>(Fs()[0x138] & 0xFD);
}

// original 0x5339A0: the next member of the pending jump - with 0x904EF2 at n
// not 0, only once member n - 1 is in state 1 (else al 0); at n 1 bit 3 of
// Field_ScriptFlags2 cleared and, with one member, bit 9 too and the jump over
// (0x904EF0 = 0, al 0); bit n (mod 32) cleared. Then member n current
// (Sprite_Current and Field_State), 0x904EF2 one on, and when that reaches
// the count (more than one member) bits 3, 4, 9 cleared and the jump over;
// al 1.
extern "C" unsigned char __cdecl Field_PendingNext(void) {
    const unsigned char n = B(at::kPendingCount);
    const unsigned char count = Field_MemberCount;
    if (n != 0) {
        if (Member(n - 1u)[1] != 1) return 0;
        if (n == 1) {
            Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF7);
            if (count == n) {
                Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFDFF);
                B(at::kPendingFlag) = 0;
                return 0;
            }
        }
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~(1u << (n & 31)));
    }
    const auto next = static_cast<unsigned char>(n + 1);
    B(at::kPendingCount) = next;
    Sprite_Current = Member(n);
    Field_State = Member(n);
    if (count != 1 && next == count) {
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFDE7);
        B(at::kPendingFlag) = 0;
    }
    return 1;
}

// original 0x533A50: the member at 0x904EF2 (n) released - unless its bit n
// of Field_ScriptFlags (read once) is set, its +0 bit 6 cleared, its state
// bytes +1..+4 the kind's record of 0x660B70 and +0x138 bit 1 cleared. With n
// not 0 it waits (al 0) until bit n or bit n - 1 is set or member n - 1 is in
// state 1, and at n 2 with bit 1 set until member 0 is in state 1; bit n of
// Field_ScriptFlags2 is cleared then. 0x904EF2 one on; at the count bit 4
// cleared, the jump over and al 2, else al 1.
// As the original has it: the shifts are by n (and n - 1) mod 32, so at n 0 the
// "bit n - 1" is bit 31 of a 16-bit word, never set.
extern "C" unsigned char __cdecl Field_PendingRelease(unsigned kind) {
    const unsigned char n = B(at::kPendingCount);
    const std::uint32_t flags = Field_ScriptFlags;
    const auto bit = [](unsigned s) { return 1u << (s & 31); };
    if (n != 0 && !(flags & bit(n)) && !(flags & bit(n - 1u)) && Member(n - 1u)[1] != 1) return 0;
    const std::uint32_t below = flags & bit(static_cast<unsigned>(n) - 1u);
    if (below != 0 && n == 2 && ObjTrio[1] != 1) return 0;
    if (n != 0) {
        if (!(flags & bit(n)) && below == 0 && Member(n - 1u)[1] != 1) return 0;
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~bit(n));
    }
    if (!(flags & bit(n))) {
        unsigned char* const m = Member(n);
        m[0] = static_cast<unsigned char>(m[0] & 0xBF);
        const std::uint32_t r = at::kPendingStates + (kind & 0xFFu) * 4;
        m[1] = B(r);
        m[2] = B(r + 1);
        m[3] = B(r + 2);
        m[4] = B(r + 3);
        m[0x138] = static_cast<unsigned char>(m[0x138] & 0xFD);
    }
    const auto next = static_cast<unsigned char>(n + 1);
    B(at::kPendingCount) = next;
    if (next == Field_MemberCount) {
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFEF);
        B(at::kPendingFlag) = 0;
        return 2;
    }
    return 1;
}

void FieldE1_Inject() {
    if (bof3::WantsShadow("field_e1")) field_e1::SelfTest();
    BOF3_INJECT(FieldPanel_DrawHeader);
    BOF3_INJECT(FieldPanel_DrawKindIcon);
    BOF3_INJECT(FieldPanel_DrawKindRow);
    BOF3_INJECT(FieldPanel_DrawTotal);
    BOF3_INJECT(FieldPanel_DrawMessage);
    BOF3_INJECT(FieldPanel_DrawShade);
    BOF3_INJECT(FieldPanel_DrawBox3);
    BOF3_INJECT(FieldPanel_DrawBox2);
    BOF3_INJECT(Inventory_Holds38To4DAt99);
    BOF3_INJECT(FieldPanel_DrawBlink);
    BOF3_INJECT(Field_PathClear);
    BOF3_INJECT(Field_FormActionState);
    BOF3_INJECT(PartyAction_ScriptEnd);
    BOF3_INJECT(Field_PassageTrigger);
    BOF3_INJECT(Field_JumpState);
    BOF3_INJECT(Field_JumpBegin);
    BOF3_INJECT(Field_JumpOut);
    BOF3_INJECT(Field_JumpOut0);
    BOF3_INJECT(Field_JumpOut1);
    BOF3_INJECT(Field_JumpOut2);
    BOF3_INJECT(Field_JumpOut3);
    BOF3_INJECT(Field_JumpOut4);
    BOF3_INJECT(Field_JumpAir);
    BOF3_INJECT(Field_JumpIn);
    BOF3_INJECT(Field_JumpIn0);
    BOF3_INJECT(Field_JumpIn1);
    BOF3_INJECT(Field_JumpIn2);
    BOF3_INJECT(Field_JumpIn3);
    BOF3_INJECT(Field_ContentState);
    BOF3_INJECT(Field_ContentTake);
    BOF3_INJECT(Field_ContentEnd);
    BOF3_INJECT(Field_AreaRunState);
    BOF3_INJECT(Field_GiveZenny);
    BOF3_INJECT(Field_CellAroundLarge);
    BOF3_INJECT(Field_CellAroundSide);
    BOF3_INJECT(Field_GatewayExit);
    BOF3_INJECT(Party_PlaceAtSlots);
    BOF3_INJECT(Party_ScriptTicks);
    BOF3_INJECT(Party_PlacesByList);
    BOF3_INJECT(Field_LeaderPlaceOffset);
    BOF3_INJECT(Field_PendingJumpTurn);
    BOF3_INJECT(Field_PendingJumpKind4);
    BOF3_INJECT(Field_PendingDrop);
    BOF3_INJECT(Field_PendingNext);
    BOF3_INJECT(Field_PendingRelease);
}
