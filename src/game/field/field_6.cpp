// Moved here by tools/move_funcs.py from area_entry.cpp, event_leader.cpp, event_objs.cpp, event_ops.cpp, field_blocked.cpp, field_e1.cpp, field_e2.cpp, field_event.cpp, field_input.cpp, field_modes.cpp, frame_callees.cpp, inventory_ops.cpp, scena_sx.cpp, scena_sx2.cpp, sprite_clut.cpp, sprite_find.cpp (docs/source-layout-round.md).
#include "game/field/field_6.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>
#include <utility>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/area_entry_callees.h"
#include "game/event_leader.h"
#include "game/event_leader_callees.h"
#include "game/event_objs_callees.h"
#include "game/event_ops_callees.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/field/field_6_callees.h"
#include "game/field_blocked.h"
#include "game/field_e1_callees.h"
#include "game/field_e2.h"
#include "game/field_e2_callees.h"
#include "game/field_event.h"
#include "game/field_event_callees.h"
#include "game/field_input.h"
#include "game/field_modes_callees.h"
#include "game/frame_callees.h"
#include "game/inventory_ops.h"
#include "game/inventory_ops_callees.h"
#include "game/lang_layout.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_sx2_callees.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "game/sprite_clut.h"
#include "game/sprite_find.h"
#include "game/text_advance.h"
#include "game/widescreen.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"
#undef far  // <windows.h>'s empty macro: a moved body names far

namespace {

namespace at = field_6::at;

}  // namespace

namespace {

using namespace move_script;

using namespace area_entry;

}  // namespace

using namespace event_leader;

namespace {

using namespace event_objs;

}  // namespace

using namespace event_ops;

namespace {

using namespace field_modes;

}  // namespace

using namespace inventory_ops;

namespace {

std::uint32_t Address(const volatile void* p) { return Low32(p); }

unsigned char* Ptr(const unsigned char* at) { return move_script::At(static_cast<std::uint32_t>(move_script::Long(at))); }

unsigned char* Descriptor() { return Area_Descriptors[Game_AreaNumber]; }

unsigned char* Record(unsigned slot) { return rv::Bytes(ObjTrio) + slot * kRecord; }

}  // namespace

namespace event_leader {

using move_script::At;

using move_script::Word;

}  // namespace event_leader

namespace {

unsigned char* Member(unsigned i) { return rv::Bytes(&ObjTrio[i]); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative dword stays negative).
std::int32_t Abs(std::uint32_t v) {
    const std::uint32_t s = static_cast<std::uint32_t>(static_cast<std::int32_t>(v) >> 31);
    return static_cast<std::int32_t>((v ^ s) - s);
}

unsigned char* AtEventObjs(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

std::uint16_t WordEventObjs(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void SetWordEventObjs(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

std::int32_t LongEventObjs(const unsigned char* p) {
    std::int32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record.
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

// SharedScratch's first byte: 0x5725C0 leaves 1 there on a slope.
unsigned char ScratchByte() { return SharedScratch.b_00; }

// The low byte of an actor record's state word (Field_ActorStates + n * 0xA4),
// n a whole byte as the original's.
unsigned char& ActorState(unsigned n) { return AtEventObjs(::bof3::Img(0x903A80u) + n * kRecordSize)[0]; }

// The landing point of a jump: the step times the frames, from the position.
long LandingX(const unsigned char* s) { return Add(Mul(rv::As<records::SpriteObject>(s)->step_x, s[9]), rv::As<records::SpriteObject>(s)->x); }

long LandingZ(const unsigned char* s) { return Add(Mul(rv::As<records::SpriteObject>(s)->step_z, s[9]), rv::As<records::SpriteObject>(s)->z); }

// Field_TileD0 and Field_TileA4.
unsigned char TileState(unsigned code, unsigned char state) {
    if (Field_ScriptFlags & 0x400) return 0;
    const unsigned char* const s = Sprite_Current;
    if (!event_objs::g.cells_all(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z, static_cast<std::uint32_t>(LongEventObjs(s + 0x70)) & 0xFF, code, 0))
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
    return event_objs::g.tile_turn(code, value, turn) != 0;
}

// AreaMap_CellsAll4 / AreaMap_CellsNone4: the four cells of the 2 x 2
// footprint at (x >> 16, z >> 16), in the original's call order.
void Cells(long x, long z, unsigned char c[4]) {
    const auto cx = static_cast<short>(static_cast<std::uint32_t>(x) >> 16);
    const auto cz = static_cast<short>(static_cast<std::uint32_t>(z) >> 16);
    const auto cx1 = static_cast<short>(cx + 1), cz1 = static_cast<short>(cz + 1);
    c[0] = event_objs::g.byte_at(cx, cz);
    c[1] = event_objs::g.byte_at(cx1, cz);
    c[2] = event_objs::g.byte_at(cx, cz1);
    c[3] = event_objs::g.byte_at(cx1, cz1);
}

}  // namespace

namespace event_ops {

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

}  // namespace event_ops

namespace {

unsigned char& Byte(std::uint32_t a) { return *event_ops::At(a); }

// Field_ScriptFlags' two bytes and Field_ScriptFlags2's, as the original
// tests them (`test ah, n` on a dword read, `test byte ptr`).
constexpr std::uint32_t kFlagsLo = 0x9039A2, kFlagsHi = 0x9039A3, kFlags2Lo = 0x905BA4, kFlags2Hi = 0x905BA5;

unsigned char* Object(int n) { return rv::Bytes(&Sprite_Objects[n]); }

unsigned char* Extra(int n) { return rv::Bytes(&Sprite_ObjectsExtra[n]); }

// Field_DirectionSteps' pair and the walk's unit pair for a direction - a
// whole byte into tables of eight, as the original indexes them.
std::uint32_t StepX(unsigned d) { return static_cast<std::uint32_t>(event_ops::Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u)); }

std::uint32_t StepZ(unsigned d) { return static_cast<std::uint32_t>(event_ops::Long(reinterpret_cast<const unsigned char*>(Field_DirectionSteps) + d * 8u + 4)); }

constexpr unsigned kObjectSize = 0xA4, kObjects = 30, kExtra = 4;   // Sprite_Objects, Sprite_ObjectsExtra

constexpr unsigned kMemberSize = 0x14C;                             // ObjTrio's records

// Only al of a byte result is meaningful: the originals (and the recorders
// standing in for them in the fuzz) leave the rest of eax undefined.
unsigned char Al(std::uint32_t eax) { return static_cast<unsigned char>(eax); }

// Sprite_ObjectAt and Party_MemberAt: the first object of `count` from `first`
// (`size` apart) that is not Sprite_Current (re-read for each), is in use
// (byte +0) and - for the sprite lists - not skipped by +7 bit 0x80, and for
// which Sprite_PointInReach says (x, y, h) is within `margin` of it. The count
// is re-read after each object; `count_at` null means a fixed count.
unsigned char FirstInReach(unsigned char* first, unsigned size, unsigned count, const unsigned char* count_at, bool skip_flag7,
                           long x, long y, long h, unsigned margin, unsigned char base) {
    for (unsigned i = 0; i < (count_at ? *count_at : count); ++i) {
        const unsigned char* const object = first + i * size;
        if (object == Sprite_Current) continue;
        if (object[0] == 0) continue;
        if (skip_flag7 && (object[7] & 0x80)) continue;
        if (Al(field_blocked::gFieldBlocked.reach(x, y, h, margin, object))) return static_cast<unsigned char>(base + i);
    }
    return 0xFF;
}

// move_script::At: named in full below (source-layout B0, a clash on At)
// move_script::Long: named in full below (source-layout B0, a clash on Long)
using move_script::SetLong;

unsigned char& BFieldE1(std::uint32_t a) { return *move_script::At(a); }

std::uint16_t W(std::uint32_t a) { return move_script::Word(move_script::At(a)); }

std::int32_t L(std::uint32_t a) { return move_script::Long(move_script::At(a)); }

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Fs() { return Field_State; }

unsigned char* MemberFieldE1(unsigned i) { return rv::Bytes(ObjTrio) + i * field_e1::at::kObjStride; }

// The record views (game/record_views.h): in the leader states Sprite_Current
// is an ObjTrio record, and Field_State always is.
short S16(std::uint32_t v) { return static_cast<short>(v); }

// `cdq / xor / sub`: the absolute value with the original's wrap (the most
// negative stays negative).
std::int32_t Abs32(std::int32_t v) {
    const auto u = static_cast<std::uint32_t>(v);
    const std::uint32_t s = static_cast<std::uint32_t>(v >> 31);
    return static_cast<std::int32_t>((u ^ s) - s);
}

// The cell step of a direction (0x66971C, a whole byte unchecked): 1 reads as 2.
int DirStep(unsigned direction, unsigned which) {
    const auto v = static_cast<signed char>(BFieldE1(::bof3::Img(field_e1::at::kDirPairs) + direction * 2u + which));
    return v == 1 ? 2 : v;
}

std::uint16_t CellX() { return field_e1::at::kCellX; }

std::uint16_t CellZ() { return field_e1::at::kCellZ; }

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

// The slot in the second party list 0x904065 holding the id, among the first
// `n` (n when none holds it).
unsigned ListSlot(unsigned char id, unsigned n) {
    unsigned c = 0;
    while (c < n && BFieldE1(::bof3::Img(field_e1::at::kSecondList) + c) != id) ++c;
    return c;
}

// move_script::SetWord: named in full below (source-layout B0, a clash on SetWord)
// move_script::Word: named in full below (source-layout B0, a clash on Word)
using U = std::uint32_t;

unsigned char& BFieldE2(U a) { return *move_script::At(a); }

U LFieldE2(U a) { return static_cast<U>(move_script::Long(move_script::At(a))); }

void SetL(U a, U v) { move_script::SetLong(move_script::At(a), static_cast<std::int32_t>(v)); }

std::uint16_t WFieldE2(U a) { return move_script::Word(move_script::At(a)); }

void SetW(U a, unsigned v) { move_script::SetWord(move_script::At(a), v); }

// The record views (game/record_views.h): the leader helpers run on the leader,
// an ObjTrio record, and Field_State is one.
U LongAt(const unsigned char* p) { return static_cast<U>(move_script::Long(p)); }

int S16FieldE2(unsigned v) { return static_cast<short>(v); }

// Sprite_EnsureAnimation of a byte (the callee compares al and hands on dl;
// the originals push whatever register held it).
void Ensure(unsigned animation) { SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(animation)); }

// A member slot's CLUT word and palette (Sprite_Current +5).
U ClutWord(const unsigned char* s) { return ::bof3::Img(Gfx_ClutStrip_addr) + 0x1e3e + (static_cast<U>(s[5]) << 6); }

unsigned short* Palette(const unsigned char* s) {
    return reinterpret_cast<unsigned short*>(static_cast<std::uintptr_t>(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + (static_cast<U>(s[5]) << 6)));
}

// The pace's reach: Field_DirectionSteps[+8] times (+0x70 + 2), from +0x34 / +0x38.
void Ahead(const unsigned char* s, std::int32_t& x, std::int32_t& z) {
    const U dir = s[8], k = static_cast<U>(s[0x70]) + 2u;
    x = static_cast<std::int32_t>(LFieldE2(::bof3::Img(field_e2::at::kDirectionSteps) + dir * 8u) * k + LongAt(s + 0x34));
    z = static_cast<std::int32_t>(LFieldE2(::bof3::Img(field_e2::at::kDirectionSteps) + 4u + dir * 8u) * k + LongAt(s + 0x38));
}

// The view follows the leader when +5 is 0 and bit 3 of the script flags'
// low bytes is clear; then every fourth frame one tick of its script.
void FollowAndTick(unsigned char* s) {
    if (s[5] == 0 && ((BFieldE2(::bof3::Img(field_e2::at::kScriptFlags2)) | BFieldE2(::bof3::Img(field_e2::at::kScriptFlagsLow))) & 8) == 0)
        SH_CALL(MapView_SetElevation)(static_cast<short>(rv::As<records::SpriteHead>(s)->height_whole));
    if ((Frame_Counter & 3) == 0) SH_CALL(Sprite_ScriptTickOnce)();
}

unsigned char Cell(U x, U z) { return SH_CALL(AreaMap_ByteAt)(static_cast<short>(x), static_cast<short>(z)); }

unsigned char Masked(unsigned char c, U mask) { return (mask & 0xFF) != 0 ? static_cast<unsigned char>(c & 0xF0) : c; }

// AreaMap_CellsNoneWide's filter: a 0x2n cell made 0x2n & 0x21.
unsigned char Doors(unsigned char c) { return (c & 0xF0) == 0x20 ? static_cast<unsigned char>(c & 0x21) : c; }

void HopSetUp() {
    Field_State[0x128] = static_cast<unsigned char>(Sprite_Current[0x70] + 2);
    SH_CALL(Field_JumpSetUp)();
    rv::Store<U>(rv::Current<records::PartyMember>()->step_y, 0);
    records::PartyMember* s = rv::Current<records::PartyMember>();
    s->step_frames = static_cast<unsigned char>(static_cast<unsigned char>(static_cast<unsigned char>(rv::Bytes(s)[0x70] + 1) * s->step_frames) << 1);
    s = rv::Current<records::PartyMember>();
    rv::Bytes(s)[0xA] = static_cast<unsigned char>(s->step_frames >> 1);
    Ensure(static_cast<unsigned char>(rv::Current<records::PartyMember>()->direction + 8));
    SH_CALL(Sprite_ApplyVelocity)();
}

using HandlerFieldEvent = field_event::Handler;

constexpr unsigned kMemberBytes = 0x14C, kActorBytes = 0xA4;

// The record views (game/record_views.h). The leader's states and the party's
// set-up run on an ObjTrio record (Member above, Field_State always); the
// Sprite_* helpers on any object, whose first 0x80 bytes are a sprite's.
unsigned char* Actor(unsigned a) { return move_script::At(::bof3::Img(CharacterRecords_addr)) + a * kActorBytes; }

void Dispatch(const std::uint32_t* table, unsigned index) {
    reinterpret_cast<HandlerFieldEvent>(static_cast<std::uintptr_t>(table[index]))();
}

// `rep movsd` of 0x29 dwords: an actor record into a member's +0x80.
void CopyActor(unsigned char* to, const unsigned char* from) { std::memcpy(to, from, kActorBytes); }

constexpr unsigned kStateSize = 0xA4;

bool HasState20(unsigned member) {
    return (Field_ActorStates[(rv::Bytes(ObjTrio) + 0x148)[member * kMemberSize] * kStateSize] & 0x20) != 0;
}

constexpr unsigned kMemberBytesFrameCallees = 0x14C;

using i32 = std::int32_t;

using u32 = std::uint32_t;

unsigned char* PosX(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMemberPos.v) + 8 * m); }

unsigned char* PosZ(u32 m) { return move_script::At(::bof3::Img(inventory_ops::at::kMemberPos.v) + 8 * m + 4); }

unsigned char* Obj(u32 i) { return rv::Bytes(ObjTrio) + inventory_ops::at::kObjStride * i; }

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does;
// rv::As<records::PartyMember>(p) is given party members (ObjTrio,
// Field_State), rv::As<records::EffectObject>(p) effect objects
// (Effect_Objects).
unsigned char& ByteInventoryOps(u32 address) { return *move_script::At(address); }

// A shift of a value the original holds as a 32-bit register.
i32 Shl(i32 v, unsigned n) { return static_cast<i32>(static_cast<u32>(v) << n); }

i32 AddInventoryOps(i32 a, i32 b) { return static_cast<i32>(static_cast<u32>(a) + static_cast<u32>(b)); }

// CharacterRecords' record n (stride 0xA4), unchecked.
unsigned char* RecordScenaSx(unsigned n) { return move_script::At(::bof3::Img(CharacterRecords_addr) + n * scena_sx::at::kRecordStride); }

// A member id's record: CharacterRecords[Character_IndexById[id]], both
// indexes a whole byte, unchecked (the original reads them the same way).
unsigned char* MemberRecord(unsigned char id) { return RecordScenaSx(Character_IndexById[id]); }

unsigned char* ObjScenaSx(unsigned n) { return rv::Bytes(&ObjTrio[n]); }

// Sprite_EnsureAnimation with the whole dword the original pushes (the
// callee hands it on to Sprite_SetAnimation), as scena_sc0.cpp's.
void EnsureAnimation(std::uint32_t eax) {
    const auto callee = reinterpret_cast<std::uintptr_t>(SH_CALL(Sprite_EnsureAnimation));
    reinterpret_cast<unsigned char (__cdecl*)(std::uint32_t)>(callee)(eax);
}

constexpr auto& AtScenaSx2 = move_script::At;

constexpr auto& LongScenaSx2 = move_script::Long;

std::uint32_t UScenaSx2(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// A signed byte over 4 the way the original divides (cdq / and 3 / add / sar
// 2): C's truncating division.
std::int32_t Quarter(unsigned char b) { return static_cast<signed char>(b) / 4; }

constexpr unsigned kPending = 0x138;   // Field_State: bit 3, "a CLUT wants putting back"

constexpr unsigned kClut = 0x27;       // sprite object: which CLUT is its

constexpr unsigned kX = 0x34;         // s32

constexpr unsigned kY = 0x38;         // s32

constexpr unsigned kZ = 0x3E;         // s16

template <class T> T GetSpriteFind(const unsigned char* at, unsigned offset) {
    // A pointer-typed field is a 32-bit word of the image on every width.
    if constexpr (std::is_pointer_v<T>) {
        std::uint32_t w;
        std::memcpy(&w, at + offset, sizeof w);
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(w));
    } else {
        T v;
        std::memcpy(&v, at + offset, sizeof v);
        return v;
    }
}

// |a - b| as the original computes it: a 32-bit difference that may wrap, and
// an absolute value that leaves 0x80000000 negative.
std::int32_t WrappingDistance(std::int32_t a, std::int32_t b) {
    const std::uint32_t d = static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b);
    const std::uint32_t sign = d & 0x80000000u ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ sign) - sign);
}

}  // namespace

// area_entry's callee seam, moved here from area_entry.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace area_entry {

const Callees kOriginals = {
    Flags_Test,         Area_ClassifyPending, Area_PickMusic,     Kind2_Place,       Member_ClearState, ScriptContext_Reset,
    Sprite_LoadPalette, Field_MemberSprite,   Party_SetUpMembers, Party_SwapMembers, ObjTrio_SwapFields,
};
Callees g = kOriginals;

}  // namespace area_entry

// event_objs's callee seam, moved here from event_objs.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace event_objs {
namespace {

template <typename F>
F Fn(std::uint32_t address) { return reinterpret_cast<F>(static_cast<std::uintptr_t>(address)); }

}  // namespace

const Callees kOriginals = {
    Field_JumpSetUp,
    Field_JumpCamera,
    Field_JumpStart,
    Fn<long (__cdecl*)(long, long, unsigned)>(::bof3::Code(kSlopeAt.v)),
    MapView_GroundAt,
    Sprite_ShadeRaise,
    Sprite_LoadPalette,
    AreaMap_CellsAll,
    AreaMap_CellsAll4,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(::bof3::Code(kCellsAllWide.v)),
    AreaMap_CellsNone,
    AreaMap_CellsNone4,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(::bof3::Code(kCellsNoneWide.v)),
    AreaMap_ByteAt,
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kFloorHurt.v)),
    Fn<void (__cdecl*)(unsigned)>(::bof3::Code(kFlash.v)),
    Party_Count,
    Fn<unsigned (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kHpLose.v)),
    Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kHpGain.v)),
    Effect_FindFree,
    Field_TileTurn,
    Fn<unsigned char (__cdecl*)(long, long, unsigned, unsigned)>(::bof3::Code(kTurnProbe.v)),
    Actor_EquipCount,
};
Callees g = kOriginals;

}  // namespace event_objs

// field_modes's callee seam, moved here from field_modes.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace field_modes {

const Callees kOriginals = {
    Scenario_CallA,
    reinterpret_cast<void (__cdecl*)(int, int, int, int)>(::bof3::Code(kChangeArea.v)),
    reinterpret_cast<void (__cdecl*)(int)>(::bof3::Code(kStartMusic.v)),
    File_LoadDone,
    Task_Sleep,
    reinterpret_cast<void (__cdecl*)()>(::bof3::Code(kTaskEnd.v)),
    ObjTrio_SetBit40,
    Flags_Test,
    Flags_Set,
    ScriptFlags_Set40,
    MapView_SetElevation,
    Kind2_Place,
    reinterpret_cast<unsigned char (__cdecl*)()>(::bof3::Code(kEffectAlloc.v)),
    reinterpret_cast<void (__cdecl*)(int)>(::bof3::Code(kPlaceParty.v)),
    Sound_PlayEffect,
    Music_FadeOutStop,
    Transition_Start,
    Music_Play,
    Text_DrawAt,
    reinterpret_cast<void (__cdecl*)(unsigned short)>(::bof3::Code(kOpenScript.v)),
    reinterpret_cast<void (__cdecl*)()>(::bof3::Code(kViewShift.v)),
    ClutStrip_FadeTo,
    ClutStrip_Restore,
    Scena16_Area1F,
    Scena16_Area04,
    Scena16_Area02,
    Field_ModeTail,
};
Callees g = kOriginals;

}  // namespace field_modes

// original 0x531120: the cells of `code` around Sprite_Current when its +0x34
// or +0x38 (the fractions) are not 0 - by which is not 0, rows of two or three
// cells ahead of it by its facing's step (0x66971C) are searched, the cell in
// 0x903850 / 0x903852; a match with an even facing turns it (AlongX /
// AlongZ), with the facing already along the row answers 1; a clean pass runs
// the other rows and, for the first shape, Field_CellAroundSide on the facings
// two to either side. al 1 found, 0 not. Called by Field_CellAround (a tail).
extern "C" unsigned char __cdecl Field_CellAroundLarge(unsigned code) {
    const unsigned char* const sc = Sc();
    const std::uint16_t fx = rv::As<records::SpriteHead>(sc)->x_frac;
    if (fx == 0 && rv::As<records::SpriteHead>(sc)->z_frac == 0) return 0;
    const unsigned facing = sc[8];
    const std::uint16_t x = rv::As<records::SpriteHead>(sc)->x_whole;
    const auto ahead_x = static_cast<std::uint16_t>(x + DirStep(facing, 0));
    const auto ahead_z = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(sc)->z_whole + DirStep(facing, 1));
    const auto c = static_cast<unsigned char>(code);
    if (fx != 0 && rv::As<records::SpriteHead>(sc)->z_frac != 0) {
        // both fractions: x .. x + 1 at the row ahead, then the column ahead
        field_e1::at::kCellX = x;
        field_e1::at::kCellZ = ahead_z;
        for (int i = 0; i <= 1; ++i) {
            if (CellIs(c)) {
                const unsigned char f = Sc()[8];
                if (!(f & 1)) return AlongX();
                if (f == 1 || f == 5) return 1;
            }
            field_e1::at::kCellX = CellX() + 1;
        }
        field_e1::at::kCellX = ahead_x;
        field_e1::at::kCellZ = rv::As<records::SpriteHead>(Sc())->z_whole;
        for (int i = 0; i <= 1; ++i) {
            if (CellIs(c)) {
                const unsigned char f = Sc()[8];
                if (!(f & 1)) return AlongZ();
                if (f == 7 || f == 3) return 1;
            }
            field_e1::at::kCellZ = CellZ() + 1;
        }
        if (SH_CALL(Field_CellAroundSide)(static_cast<unsigned char>((Sc()[8] + 2) & 7), code)) return 1;
        if (SH_CALL(Field_CellAroundSide)(static_cast<unsigned char>((Sc()[8] - 2) & 7), code)) return 1;
        return 0;
    }
    if (fx != 0) {
        // x's fraction only: the column ahead, z - 1 .. z + 1
        field_e1::at::kCellX = ahead_x;
        field_e1::at::kCellZ = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(sc)->z_whole - 1);
        for (int i = 0; i <= 2; ++i) {
            if (CellIs(c)) {
                const unsigned char f = Sc()[8];
                if (!(f & 1)) return AlongZ();
                if (f == 7 || f == 3) return 1;
            }
            field_e1::at::kCellZ = CellZ() + 1;
        }
        const unsigned char f = Sc()[8];
        if (f != 1 && f != 5) return 0;
        field_e1::at::kCellX = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->x_whole - 1);
        field_e1::at::kCellZ = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->z_whole - 1);
        for (int i = 0; i <= 2; ++i) {
            if (CellIs(c)) {
                Sc()[8] = 7;
                return 1;
            }
            field_e1::at::kCellZ = CellZ() + 1;
        }
        field_e1::at::kCellX = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->x_whole + 2);
        field_e1::at::kCellZ = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->z_whole - 1);
        for (int i = 0; i <= 2; ++i) {
            if (CellIs(c)) {
                Sc()[8] = 3;
                return 1;
            }
            field_e1::at::kCellZ = CellZ() + 1;
        }
        return 0;
    }
    // z's fraction only: the row ahead, x - 1 .. x + 1
    field_e1::at::kCellX = static_cast<std::uint16_t>(x - 1);
    field_e1::at::kCellZ = ahead_z;
    for (int i = 0; i <= 2; ++i) {
        if (CellIs(c)) {
            const unsigned char f = Sc()[8];
            if (!(f & 1)) return AlongX();
            if (f == 1 || f == 5) return 1;
        }
        field_e1::at::kCellX = CellX() + 1;
    }
    const unsigned char f = Sc()[8];
    if (f != 7 && f != 3) return 0;
    field_e1::at::kCellX = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->x_whole - 1);
    field_e1::at::kCellZ = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->z_whole - 1);
    for (int i = 0; i <= 2; ++i) {
        if (CellIs(c)) {
            Sc()[8] = 1;
            return 1;
        }
        field_e1::at::kCellX = CellX() + 1;
    }
    field_e1::at::kCellX = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->x_whole - 1);
    field_e1::at::kCellZ = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->z_whole + 2);
    for (int i = 0; i <= 2; ++i) {
        if (CellIs(c)) {
            Sc()[8] = 5;
            return 1;
        }
        field_e1::at::kCellX = CellX() + 1;
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
    field_e1::at::kCellX = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(sc)->x_whole + DirStep(d, 0));
    field_e1::at::kCellZ = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(sc)->z_whole + DirStep(d, 1));
    if (d == 1 || d == 5) {
        for (int i = 0; i <= 1; ++i) {
            if (CellIs(c)) {
                Sc()[8] = d;
                return 1;
            }
            field_e1::at::kCellX = CellX() + 1;
        }
        return 0;
    }
    if (d == 7 || d == 3) {
        for (int i = 0; i <= 1; ++i) {
            if (CellIs(c)) {
                Sc()[8] = d;
                return 1;
            }
            field_e1::at::kCellZ = CellZ() + 1;
        }
        return 0;
    }
    return 0;
}

// original 0x531660: talking to object n (PSX FUN_801B70D4) - 0..29
// Sprite_Objects, 30 and up Sprite_ObjectsExtra. Not of kind 9: its script
// context's +0x80 bit 5 (the "touched" bit Field_ObjectIdle 0x517F30 acts on), the pose,
// sub-state 0, Field_ScriptFlags bit 8. Of kind 9: unless on
// foot without bit 5 of Field_InputFlags, the facing pose (by 0x660A38[+0x89])
// unless bit 5, then Field_State +0x139 = n, state 0xB sub-state 0.
// As the original has it: n is not bounded above - an n past 33 is an object
// past the extra four.
extern "C" void __cdecl Field_LeaderTalkTo(unsigned index) {
    const unsigned char n = static_cast<unsigned char>(index);
    unsigned char* const o = n < 30 ? Object(n) : Extra(n - 30);
    if (o[6] != 9) {
        o[0x80] |= 0x20;
        event_ops::g.leader_animation(Sprite_Current[8]);
        Sprite_Current[2] = 0;
        Byte(::bof3::Img(kFlagsHi)) |= 1;
        return;
    }
    const unsigned char flags = Field_InputFlags;
    if ((flags & 1) && !(flags & 0x20)) return;
    if (!(flags & 0x20)) {
        if (event_ops::At(::bof3::Img(event_ops::at::kFacingPose))[Field_State[0x89]])
            event_ops::g.ensure_animation(static_cast<unsigned char>((Sprite_Current[8] >> 1) + 0x34));
        else
            event_ops::g.ensure_animation(Sprite_Current[8]);
    }
    Field_State[0x139] = n;
    Sprite_Current[1] = 0xB;
    Sprite_Current[2] = 0;
}

// original 0x5317F0: the encounter's area - the word after Game_AreaNumber's
// in the ten pairs at 0x660A90, to 0x937F82; nothing when it is not listed.
// Called only by Field_LeaderWalk.
extern "C" void __cdecl Field_EncounterArea(void) {
    const unsigned short area = Game_AreaNumber;
    for (std::uint32_t p = ::bof3::Img(Field_EncounterAreas_addr); p < ::bof3::Img(event_leader::at::kEncounterAreasEnd); p += 4) {
        if (event_leader::Word(event_leader::At(p)) == area) {
            AreaW0b_Place = event_leader::Word(event_leader::At(p + 2));
            return;
        }
    }
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
    if ((AreaW0b_PartySet & 0x7F) == 0xC) return 0;
    if (Game_AreaNumber == 0xBD) {
        const std::uint32_t r = ::bof3::Img(field_e1::at::kGateway189) + (Cond_ByteFF == 0 ? 3u : 0u);
        AreaEntry_PendingFlags = 4;
        AreaW0b_Place = BFieldE1(r);
        field_e1::at::kExitX = static_cast<std::int32_t>(static_cast<std::uint32_t>(BFieldE1(r + 1)) << 16);
        move_script::SetLong(move_script::At(::bof3::Img(AreaEntry_PendingZ_addr)), static_cast<std::int32_t>(static_cast<std::uint32_t>(BFieldE1(r + 2)) << 16));
        return 1;
    }
    if (SH_CALL(Field_CellHasEvent)(ObjTrio[0].x_whole, ObjTrio[0].z_whole)) return 0;
    if (Field_ScriptFlags2 & 0x1000) return 0;
    const std::uint16_t area = Game_AreaNumber;
    for (std::uint32_t r = ::bof3::Img(field_e1::at::kGatewayExits); r < ::bof3::Img(field_e1::at::kGatewayExitsEnd); r += 8) {
        if (W(r) != area) continue;
        AreaW0b_Place = W(r + 2);
        AreaEntry_PendingFlags = 4;
        AreaEntry_PendingZ = static_cast<std::int32_t>(static_cast<std::uint32_t>(W(r + 6)) << 16);
        move_script::SetLong(move_script::At(::bof3::Img(field_e1::at::kExitX_addr)), static_cast<std::int32_t>(static_cast<std::uint32_t>(W(r + 4)) << 16));
        return 1;
    }
    return 0;
}

// original 0x531920: whether the cell (x, z) has an event - its code
// (AreaMap_ByteAt) in the 0xFF-ended list at 0x660B10. Called by the world
// map's frame (eleven sites) and Field_LeaderCellEvent.
extern "C" unsigned char __cdecl Field_CellHasEvent(long x, long z) {
    const unsigned char code = event_leader::g.byte_at(static_cast<short>(x), static_cast<short>(z));
    for (const unsigned char* p = event_leader::At(::bof3::Img(Field_EventCells_addr)); *p != 0xFF; ++p)
        if (code == *p) return 1;
    return 0;
}

// original 0x531950: the cell the leader stands on, on foot (PSX
// FUN_801B766C). Confirm on 0xAE: 0x9039F3 = 0x37, sub-state 0; on a 0xAx
// cell (or under bit 12 of Field_ScriptFlags2): the pose unless bit 5, the
// cell's exit (0x531AF0), bit 12 cleared, Field_ChangeArea. The pad's 0x100
// cycles the byte 0x9045FA 0..2 (answer 0). The pad's 0x800 with an exit from
// 0x531820: the pose unless bit 5, the return point 0x904148 (x, z, area, a
// counter byte), bit 12 cleared, Field_ChangeArea. The button 0x90358C on
// 0xA0 / 0xA1: 0x9039F3 = 0x1B, the pose unless bit 5, sub-state 0.
extern "C" unsigned char __cdecl Field_LeaderCellEvent(void) {
    const unsigned char cell = event_ops::g.byte_at(static_cast<short>(rv::Current<records::SpriteHead>()->x_whole),
                                         static_cast<short>(rv::Current<records::SpriteHead>()->z_whole));
    const unsigned char flags = Field_InputFlags;
    if (!(flags & 1)) return 0;
    const unsigned pressed = Input_Pressed;
    if (event_ops::Word(event_ops::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f000)) & pressed) {
        if (cell == 0xAE) {
            unsigned char* const sc = Sprite_Current;
            AreaW0a_TailKind = 0x37;
            sc[2] = 0;
            return 1;
        }
        if ((cell & 0xF0) != 0xA0 && !(Byte(::bof3::Img(kFlags2Hi)) & 0x10)) return 0;
        if (!(flags & 0x20)) event_ops::g.leader_animation(Sprite_Current[8]);
        event_ops::g.exit_from_cell();
        const unsigned kind = AreaEntry_PendingFlags;
        const long z = static_cast<std::int32_t>(AreaEntry_PendingZ);
        const long x = event_ops::at::kExitX;
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xEFFF);
        event_ops::g.change_area(AreaW0b_Place, x, z, kind);
        return 1;
    }
    if (pressed & 0x100) {
        const auto cycle = static_cast<unsigned char>(AreaW0b_MapMode + 1);
        AreaW0b_MapMode = cycle;
        if (cycle > 2) AreaW0b_MapMode = 0;
        return 0;
    }
    if (pressed & 0x800) {
        if (!event_ops::g.exit_gateway()) return 0;
        if (!(Field_InputFlags & 0x20)) event_ops::g.leader_animation(Sprite_Current[8]);
        const unsigned char* const sc = Sprite_Current;
        unsigned char* const back = event_ops::At(::bof3::Img(AreaW2d_ReturnPoint_addr));
        event_ops::SetLong(back, rv::As<records::SpriteObject>(sc)->x);
        const auto count = static_cast<unsigned char>(back[0xA] + 1);
        const long z = rv::As<records::SpriteObject>(sc)->z;
        const unsigned short area = Game_AreaNumber;
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xEFFF);
        back[0xA] = count;
        const unsigned kind = AreaEntry_PendingFlags;
        event_ops::SetLong(back + 4, z);
        const long exit_z = static_cast<std::int32_t>(AreaEntry_PendingZ);
        event_ops::SetWord(back + 8, area);
        const long exit_x = event_ops::at::kExitX;
        event_ops::g.change_area(AreaW0b_Place, exit_x, exit_z, kind);
        return 1;
    }
    if (!(event_ops::Word(event_ops::At(::bof3::Img(Gfx_UnpackScratch_addr) + 0x1f00c)) & pressed)) return 0;
    if (cell != 0xA1 && cell != 0xA0) return 0;
    AreaW0a_TailKind = 0x1B;
    if (!(flags & 0x20)) event_ops::g.leader_animation(Sprite_Current[8]);
    Sprite_Current[2] = 0;
    return 1;
}

// original 0x531AF0: the exit of the cell the leader stands on - a 0xA0 cell's
// record in the area's exit list (WorldMap_ExitRecords) by the whole x and z: its area to
// 0x937F82, its kind to 0x905B88, and the fixed entry point (0xE8000,
// 0x410000). Called only by Field_LeaderCellEvent.
// As the original has it: the list has no end test - a cell with no record
// walks on.
extern "C" void __cdecl Field_ExitFromCell(void) {
    {
        unsigned char* const sc = Sprite_Current;
        if (event_leader::g.byte_at(static_cast<short>(rv::As<records::SpriteHead>(sc)->x_whole), static_cast<short>(rv::As<records::SpriteHead>(sc)->z_whole)) != 0xA0) return;
    }
    const unsigned char* r = event_leader::g.area_exits();
    const unsigned char* const sc = Sprite_Current;
    const unsigned short x = rv::As<records::SpriteHead>(sc)->x_whole;
    while (!(x == r[0] && rv::As<records::SpriteHead>(sc)->z_whole == r[1])) r += 4;
    AreaW0b_Place = r[2];
    event_leader::at::kExitX = 0xE8000;
    AreaEntry_PendingZ = 0x410000;
    AreaEntry_PendingFlags = r[3];
}

// original 0x531B60: the party members' screen updates (PSX FUN_801BEAB0,
// term for term). Field_MemberCount read once; from the last member down,
// each without bit 6 of +0 becomes Sprite_Current and is updated - through
// Sprite_UpdateScreenSlot when Field_Request (read each time) is 3. A count
// above 3 walks on past ObjTrio's three at the same stride, as the original.
extern "C" void __cdecl Party_UpdateScreens(void) {
    const int last = static_cast<int>(Field_MemberCount) - 1;
    if (last < 0) return;
    unsigned char* member = rv::Bytes(&ObjTrio[last]);
    for (int left = last + 1; left != 0; --left, member -= kMemberBytesFrameCallees) {
        if (member[0] & 0x40) continue;
        Sprite_Current = member;
        if (Field_Request == 3) frame_callees::g.update_screen_slot();
        else frame_callees::g.update_screen();
    }
}

// original 0x531BB0 (PSX 0x801BEB74): entries before the first 0xFF of the
// party list `slot` (0x904062 + slot * 3), at most 3. As the original has it:
// slot is unchecked.
extern "C" int __cdecl Party_Count(unsigned slot) {
    const unsigned char* const list = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2) + (slot & 0xFF) * 3u;
    int n = 0;
    while (list[n] != 0xFF)
        if (++n >= 3) break;
    return n;
}

// original 0x531BD0 (PSX 0x801BEBBC). The field's copy of the held buttons - with bits 12-13
// and 14-15 changing places while a member's state byte has bit 0x20. Which
// members count is Field_InputFlags bit 0: set, any of them; clear, the first,
// whatever the count says.
//
// As the original has it: the exchanged word also loses bits 9 and 10, and
// with bit 0 set and a count of 0 nothing is exchanged.
extern "C" void __cdecl Field_CopyInput(void) {
    bool exchange = false;
    if (Field_InputFlags & 1) {
        for (unsigned i = 0; i < Field_MemberCount; ++i)
            if (HasState20(i)) exchange = true;
    } else {
        exchange = HasState20(0);
    }
    const std::uint32_t held = Input_Held;
    if (!exchange) {
        Field_InputHeld = static_cast<unsigned short>(held);
        return;
    }
    Field_InputHeld = static_cast<unsigned short>(((held & 0xF000u) << 2) | ((held >> 2) & 0x3000u) | (held & 0x9FFu));
}

// original 0x531C70. Whether the point (x, y, z) is within the object's reach -
// byte +0x70, widened by `margin` and 2 - of where the object is about to be:
// its position less dword +0xC (and +0x10) times byte +9.
//
// As the original has it: z is compared inclusively and x and y exclusively;
// everything wraps in 32 bits, and an absolute difference of 0x80000000 counts
// as within any reach.
extern "C" unsigned char __cdecl Sprite_PointInReach(int x, int y, short z, int margin, const unsigned char* object) {
    const std::uint32_t reach = static_cast<std::uint32_t>(object[0x70]) + static_cast<std::uint32_t>(margin) + 2u;
    const int dz = GetSpriteFind<std::int16_t>(object, kZ) - z;
    if ((dz < 0 ? -dz : dz) > static_cast<std::int32_t>(reach << 7)) return 0;
    const std::uint32_t steps = object[0x09];
    const std::int32_t limit = static_cast<std::int32_t>(reach << 15);
    const std::uint32_t ax = static_cast<std::uint32_t>(x) - GetSpriteFind<std::uint32_t>(object, 0x0C) * steps;
    if (WrappingDistance(static_cast<std::int32_t>(ax), GetSpriteFind<std::int32_t>(object, kX)) >= limit) return 0;
    const std::uint32_t ay = static_cast<std::uint32_t>(y) - GetSpriteFind<std::uint32_t>(object, 0x10) * steps;
    if (WrappingDistance(static_cast<std::int32_t>(ay), GetSpriteFind<std::int32_t>(object, kY)) >= limit) return 0;
    return 1;
}

// original 0x531CF0 (PSX 0x801BEE0C): the sprite object at (x, y) - within
// `margin` by Sprite_PointInReach at the probe height - as its index in
// Sprite_Objects, or 0x1E + its index in Sprite_ObjectsExtra; 0xFF for none.
// Skips Sprite_Current, objects not in use, and those with +7 bit 0x80. The
// original stores the height over its own y argument slot (the calls take y
// from a register); cdecl gives the caller no claim on that slot and no
// caller reads it back (docs/field-blocked.md section 2), so ours does not.
extern "C" unsigned char __cdecl Sprite_ObjectAt(long x, long y, unsigned margin) {
    const long h = field_blocked::gFieldBlocked.probe(x, y);
    const unsigned char found = FirstInReach(rv::Bytes(Sprite_Objects), kObjectSize, kObjects, nullptr, true, x, y, h, margin, 0);
    if (found != 0xFF) return found;
    return FirstInReach(rv::Bytes(Sprite_ObjectsExtra), kObjectSize, kExtra, nullptr, true, x, y, h, margin, 0x1E);
}

// original 0x531DB0 (PSX 0x801BECF0): the height to test a point at - the
// ground's there, unless it is more than 0x100 above or below Sprite_Current's
// own (s16 +0x3E, read after the call), then Sprite_Current's. 16 bits,
// zero-extended as the original's eax is.
extern "C" long __cdecl Field_ProbeHeight(long x, long y) {
    const long ground = field_blocked::gFieldBlocked.elevation(x, y);
    const auto own = static_cast<short>(rv::Current<records::SpriteHead>()->height_whole);
    const auto there = static_cast<short>(ground);
    int distance = own - there;
    if (distance < 0) distance = -distance;
    return static_cast<long>(static_cast<std::uint16_t>(distance > 0x100 ? own : there));
}

// original 0x531DF0: the objects the leader walks into (PSX FUN_801BEF00) -
// one Field_DirectionSteps ahead (two under bit 12 of Field_ScriptFlags), at
// the ground's height there unless that is more than 0x100 from the leader's;
// every object in use without +7 bit 7 in reach (Sprite_PointInReach, the
// leader's +0x70) has its word +0x9C counted up. If any: pace 3, +9 = 0.
// As the original has it: the direction indexes Field_DirectionSteps whole.
extern "C" unsigned char __cdecl Field_LeaderPushObjects(void) {
    unsigned char hit = 0;
    const unsigned char* const sc = Sprite_Current;
    const unsigned d = sc[8];
    const std::uint32_t dx = StepX(d), dz = StepZ(d);
    std::uint32_t x = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->x) + dx;
    std::uint32_t z = static_cast<std::uint32_t>(rv::As<records::SpriteObject>(sc)->z) + dz;
    if (Byte(::bof3::Img(kFlagsHi)) & 0x10) {
        x += dx;
        z += dz;
    }
    auto height = static_cast<short>(event_ops::g.ground_at(static_cast<long>(x), static_cast<long>(z)));
    const auto own = static_cast<short>(rv::Current<records::SpriteHead>()->height_whole);
    if (std::abs(static_cast<int>(own) - height) > 0x100) height = own;
    for (int i = 0; i < 30; ++i) {
        unsigned char* const o = Object(i);
        if (o[0] == 0 || (o[7] & 0x80)) continue;
        if (event_ops::g.point_in_reach(static_cast<int>(x), static_cast<int>(z), height, Sprite_Current[0x70], o)) {
            rv::As<records::SpriteObject>(o)->counter = rv::As<records::SpriteObject>(o)->counter + 1u;
            hit = 1;
        }
    }
    for (int i = 0; i < 4; ++i) {
        unsigned char* const o = Extra(i);
        if (o[0] == 0 || (o[7] & 0x80)) continue;
        if (event_ops::g.point_in_reach(static_cast<int>(x), static_cast<int>(z), height, Sprite_Current[0x70], o)) {
            rv::As<records::SpriteObject>(o)->counter = rv::As<records::SpriteObject>(o)->counter + 1u;
            hit = 1;
        }
    }
    if (hit) {
        Field_State[0x128] = 3;
        Sprite_Current[9] = 0;
    }
    return hit;
}

// original 0x531F10 (PSX 0x801BF0B8): the same over the party's records -
// Field_MemberCount of ObjTrio's, re-read after each - without the +7 test.
extern "C" unsigned char __cdecl Party_MemberAt(long x, long y, unsigned margin) {
    const long h = field_blocked::gFieldBlocked.probe(x, y);
    return FirstInReach(rv::Bytes(ObjTrio), kMemberSize, 0, &Field_MemberCount, false, x, y, h, margin, 0);
}

// original 0x531F90 (PSX 0x801BF1A8, call-anchored; read, the same): the
// party placed from entry `entry` of the area's placement table
// (descriptor +0x14). The entry's first byte: N in its low nibble, bit 5
// "set bit 3 of Field_ScriptFlags", bits 4-5 both "then place the kind-2
// object", and a kind in bits 6-7:
//   0     exactly N members (else 1): N wanted members follow - a member
//         number, or 0x80 for "any one not yet taken". For each, the
//         lowest-slot record holding it (ObjTrio +0x89) or the first free
//         one; a wanted member nobody holds is 1. The party list 0x904062
//         is rewritten in the wanted order and the records put in it by
//         Party_SwapMembers, a selection sort; then the members set up from
//         the N bytes after the wanted list.
//   0x80  at least N members (else 1), set up from the N bytes after the
//         header, nothing re-ordered.
//   0x40, 0xC0  no test, set up likewise.
// Then Kind2_Place of the byte after the set-up list, when bits 4-5 are both
// set; Field_ScriptFlags |= 0x100, Field_ScriptFlags2 |= 0x8000; 0.
//
// As the original has it: Party_SetUpMembers gets the member count as it
// is after the swaps (every kind joins at its load, 0x53214D - the fuzz's
// first run caught a misreading of that join). The scratch lists are
// four bytes apart and N is not limited, as on the PSX. The first byte is
// also written over the argument's low byte (no caller reads the slot
// after the call: every one of the 393 sites releases it first - a scan,
// docs/area-entry.md section 2), which ours keeps in a local. The result:
// 1 on a failed kind-0 or kind-0x80 test with the placement table's
// address in the upper 24 bits (1 alone when a wanted member is missing), 0
// otherwise - or, after Kind2_Place, that callee's leftover eax with al
// cleared, which ours does not reproduce (0).
extern "C" unsigned __cdecl Party_DropIn(unsigned entry_arg) {
    const unsigned entry = entry_arg & 0xFFu;
    const auto table = static_cast<std::uint32_t>(move_script::Long(Descriptor() + 0x14));
    const unsigned char* list = Ptr(move_script::At(table) + entry * 4);
    const unsigned char header = list[0];
    const unsigned n = header & 0xFu;
    ++list;
    if ((header & 0xC0) == 0) {
        if (Field_MemberCount != n) return (table & 0xFFFFFF00u) | 1u;
        for (unsigned i = 0; i < n; ++i) move_script::At(::bof3::Img(kPickFree.v))[i] = Cond_Flags[0xd2 + i];
        for (unsigned i = 0; i < n; ++i) {
            const unsigned char wanted = list[i];
            const unsigned members = Field_MemberCount;
            unsigned j = 0;
            if (wanted != 0x80) {
                while (j < members && Record(j)[kRecMember] != wanted) ++j;
                if (j == members) return 1;
            } else {
                while (j < members && move_script::At(::bof3::Img(kPickFree.v))[j] == 0xFF) ++j;
                if (j == members) continue;
            }
            move_script::At(::bof3::Img(kPickColumn.v))[i] = Record(j)[kRecColumn];
            move_script::At(::bof3::Img(kPickSlot.v))[i] = static_cast<unsigned char>(j);
            move_script::At(::bof3::Img(kPickFree.v))[j] = 0xFF;
        }
        for (unsigned i = 0; i < n; ++i) Cond_Flags[0xd2 + i] = Record(move_script::At(::bof3::Img(kPickSlot.v))[i])[kRecMember];
        for (unsigned s = 0; static_cast<int>(s) < static_cast<int>(n) - 1; ++s) {
            const unsigned char there = move_script::At(::bof3::Img(kPickSlot.v))[s];
            if (s == there) continue;
            area_entry::g.swap_members(s, there, 0);
            for (unsigned t = s; t < n; ++t) {
                if (move_script::At(::bof3::Img(kPickSlot.v))[t] == s) {
                    move_script::At(::bof3::Img(kPickSlot.v))[t] = move_script::At(::bof3::Img(kPickSlot.v))[s];
                    break;
                }
            }
        }
        list += n;
    } else if ((header & 0xC0) == 0x80 && Field_MemberCount < n) {
        return (table & 0xFFFFFF00u) | 1u;
    }
    area_entry::g.set_up(Field_MemberCount, list);
    list += n;
    // Byte-wide, as the original: bit 3 and bit 8 of Field_ScriptFlags, bit
    // 15 of Field_ScriptFlags2.
    if (header & 0x20) move_script::At(Address(&Field_ScriptFlags))[0] |= 8;
    if ((header & 0x30) == 0x30) area_entry::g.kind2_place(list[0]);
    move_script::At(Address(&Field_ScriptFlags) + 1)[0] |= 1;
    move_script::At(Address(&Field_ScriptFlags2) + 1)[0] |= 0x80;
    return 0;
}

// original 0x5321C0 (PSX 0x801BF56C, callers pairing; read, the same):
// the first `count` party records set up for the area: each made
// Sprite_Current, bit 5 of +0 cleared, +0x5D..+0x5F 0 and +0x5C 2,
// Member_ClearState(slot), +1 2, +7 and +9 0, its script pointer +0x130 from
// the area descriptor's +0x18 table at the list's byte, its script context
// +0x124 reset, its palette (+5) loaded into palette 0 of the 64-byte block;
// then the low three bits of Field_ScriptFlags2 cleared.
//
// As the original has it: the record pointer is kept across the calls, but
// Sprite_Current is re-read after Member_ClearState and after
// ScriptContext_Reset; the descriptor is looked up afresh for each record;
// the count is a byte and not limited to the three records ObjTrio holds.
extern "C" void __cdecl Party_SetUpMembers(unsigned count_arg, const unsigned char* list) {
    const unsigned count = count_arg & 0xFFu;
    unsigned char* record = rv::Bytes(ObjTrio);
    for (unsigned i = 0; i < count; ++i, record += kRecord) {
        Sprite_Current = record;
        record[0] &= 0xDF;
        Sprite_Current[0x5D] = 0;
        Sprite_Current[0x5E] = 0;
        Sprite_Current[0x5F] = 0;
        Sprite_Current[0x5C] = 2;
        area_entry::g.member_clear(i);
        Sprite_Current[1] = 2;
        Sprite_Current[7] = 0;
        Sprite_Current[9] = 0;
        move_script::SetLong(record + kRecScript, move_script::Long(Ptr(Descriptor() + 0x18) + list[i] * 4u));
        area_entry::g.context_reset(record + kRecContext);
        area_entry::g.load_palette(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(kPalettes.v) + Sprite_Current[kRecPalette] * 64u)), 0);
    }
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF8);
}

// original 0x5322B0 (PSX 0x801BF708): a script context back to its start, at
// speed index 3.
extern "C" void __cdecl ScriptContext_Reset(unsigned char* context) {
    move_script::SetWord(context + 0xA, 0);
    context[7] = 0;
    context[2] = 0;
    context[5] = 0;
    context[0] = 0;
    context[1] = 0;
    context[3] = 0;
    context[4] = 3;
}

// original 0x5322D0 (PSX 0x801BF730, call-anchored; read, the same): party
// records a and b trade places - ObjTrio_SwapFields(a, b, keep), then for a
// and then b: the record made Field_State and Sprite_Current,
// Field_MemberSprite(its member, its slot), its character block +0x80
// (0xA4 bytes) copied from the character record Character_IndexById
// gives for its member, and +0x148 set to that table's byte again.
//
// As the original has it: the second lookup reads +0x89 AFTER the copy,
// which has just overwritten it with the character record's +9 - so +0x148
// is the table's byte for that, not for the member (the PSX's +0x13C the
// same). Field_State is re-read after Field_MemberSprite and after the copy;
// the member is passed with a stale upper half (Field_MemberSprite masks).
extern "C" void __cdecl Party_SwapMembers(unsigned a, unsigned b, unsigned keep) {
    area_entry::g.swap_fields(a, b, keep);
    const unsigned slots[2] = {a, b};
    for (const unsigned slot : slots) {
        unsigned char* const record = Record(slot & 0xFFu);
        Field_State = record;
        Sprite_Current = record;
        area_entry::g.member_sprite(record[kRecMember], slot);
        unsigned char* state = Field_State;
        const unsigned char character = Character_IndexById[state[kRecMember]];
        const unsigned char* const from = move_script::At(::bof3::Img(CharacterRecords_addr) + character * kCharacterBytes);
        for (unsigned k = 0; k < kCharacterBytes; k += 4) move_script::SetLong(state + kRecCharacter + k, move_script::Long(from + k));
        state = Field_State;
        state[kRecCharacterIndex] = Character_IndexById[state[kRecMember]];
    }
}

// original 0x5323E0 (PSX 0x801BF918, call-anchored; read, the same): the
// fields of party records a and b that go with the member rather than the
// slot, exchanged: the member +0x89; unless `keep`, also +0x08, +0x29,
// +0x138 and the position +0x34 / +0x38 / +0x3C; always the three dwords
// +0x0C..+0x14 and the script context's first two bytes +0x124 / +0x125;
// then +0x4B of both 0xFF.
//
// As the original has it: a, b and keep are bytes; nothing limits a and b to
// the three records.
extern "C" void __cdecl ObjTrio_SwapFields(unsigned a, unsigned b, unsigned keep) {
    unsigned char* const ra = Record(a & 0xFFu);
    unsigned char* const rb = Record(b & 0xFFu);
    const auto byte = [&](unsigned at) {
        const unsigned char va = ra[at], vb = rb[at];
        ra[at] = vb;
        rb[at] = va;
    };
    const auto dword = [&](unsigned at) {
        const std::int32_t va = move_script::Long(ra + at), vb = move_script::Long(rb + at);
        move_script::SetLong(ra + at, vb);
        move_script::SetLong(rb + at, va);
    };
    byte(0x89);
    if ((keep & 0xFFu) == 0) {
        byte(0x08);
        byte(0x29);
        byte(0x138);
        dword(0x34);
        dword(0x38);
        dword(0x3C);
    }
    dword(0x0C);
    dword(0x10);
    dword(0x14);
    byte(0x125);
    byte(0x124);
    ra[0x4B] = 0xFF;
    rb[0x4B] = 0xFF;
}

// original 0x532550 (PSX 0x801BFBA8): the intro's initiative roll. ObjTrio's
// first +0xB is 0xFF; for five slots (ObjTrio holds three: slots at or past
// Field_MemberCount, read per slot, never read theirs) r = Rand & 0x7F,
// redrawn above 100. A member notices when its record's byte +0x38 (by the
// first list and 0x66972C) is r or more: an effect (Effect_FindFree; none
// free counts as noticed) of kind 6 at the member's words +0x2E / +0x30, the
// two bytes 0x660B24 of the member id +0x89 at +0xC / +0x10, its index in
// ObjTrio's first +0xB. A slot past the party counts when r <= 50. All five:
// 0x904AE4 = 1; none: 2; else 0.
extern "C" void __cdecl Encounter_RollInitiative(void) {
    unsigned char count = 0;
    Obj(0)[0xB] = 0xFF;
    for (int i = 0; i < 5; ++i) {
        unsigned char r;
        do r = static_cast<unsigned char>(inventory_ops::g.rand() & 0x7F);
        while (r > 0x64);
        unsigned char* const obj = Obj(static_cast<u32>(i));
        if (i >= static_cast<int>(Field_MemberCount)) {
            if (r <= 0x32) ++count;
            continue;
        }
        const u32 record = Character_IndexById[Cond_Flags[0xd2 + i]];
        if (move_script::At(::bof3::Img(CharacterRecords_addr) + record * inventory_ops::at::kRecordSize)[0x38] < r) continue;
        const unsigned char e = inventory_ops::g.effect_free();
        if (e != 0xFF) {
            unsigned char* const fx = rv::Bytes(Effect_Objects) + (static_cast<u32>(e) << 7);
            fx[0] = 1;
            fx[5] = 6;
            fx[6] = 1;
            rv::As<records::EffectObject>(fx)->screen_x = static_cast<std::uint16_t>(rv::As<records::PartyMember>(obj)->screen_x);
            rv::As<records::EffectObject>(fx)->screen_y = static_cast<std::uint16_t>(rv::As<records::PartyMember>(obj)->screen_y);
            const u32 id = obj[0x89];
            Obj(0)[0xB] = e;
            rv::As<records::EffectObject>(fx)->step_x = move_script::At(::bof3::Img(inventory_ops::at::kAlertOffsets.v))[2 * id];
            rv::As<records::EffectObject>(fx)->step_z = move_script::At(::bof3::Img(inventory_ops::at::kAlertOffsets.v))[2 * id + 1];
        }
        ++count;
    }
    BattlePhases_Initiative = count == 5 ? 1 : count != 0 ? 0 : 2;
}

// original 0x532660 (PSX 0x801BFDC8): each member's +0xB the way it turns
// (Sprite_TurnSense) to the direction 0x660B3C gives the battle's facing;
// the count and the facing read per member, the sprite after the call.
extern "C" void __cdecl Encounter_PartyTurnSense(void) {
    for (u32 i = 0; i < Field_MemberCount; ++i) {
        Sprite_Current = Obj(i);
        const unsigned char s = inventory_ops::g.turn_sense(move_script::At(::bof3::Img(inventory_ops::at::kFaceTable.v))[ByteInventoryOps(::bof3::Img(inventory_ops::at::kBattleFacing.v))]);
        Sprite_Current[0xB] = s;
    }
}

// original 0x5326B0 (PSX 0x801BFE54): one frame of the party's turn. While
// any member's +0xB is set (the count held for the scan): each member
// (Field_State and Sprite_Current it) - one whose record's state word has
// bit 14 is given its animation +0x4B through 0x5891C0 and +0xB 0, with no
// script tick; else, facing the direction already, +0xB non-zero gives the
// animation of that direction and +0xB 0; not facing it, the direction
// steps by +0xB (& 7) and the new one's animation is set; then the script
// ticks. Answers 0. When none is set: bit 2 of 0x904AE5 answers 1 at once;
// else each member's 32 CLUT words (row +5) saved three CLUTs on,
// Sprite_ShadeFadeBegin, +0xB 0, shades +0x5D..+0x5F 0xC0, +0x5C 1; 1.
extern "C" unsigned char __cdecl Encounter_PartyTurn(void) {
    u32 n = Field_MemberCount;
    u32 i = 0;
    if (static_cast<i32>(n) > 0)
        while (i < n && Obj(i)[0xB] == 0) ++i;
    if (i < n) {
        for (u32 j = 0; static_cast<i32>(j) < static_cast<i32>(n); ++j, n = Field_MemberCount) {
            unsigned char* const obj = Obj(j);
            Field_State = obj;
            Sprite_Current = obj;
            const u32 record = obj[0x148];
            if (move_script::Word(move_script::At(::bof3::Img(CharacterRecords_addr) + record * inventory_ops::at::kRecordSize + 0x10)) & 0x4000) {
                inventory_ops::g.set_anim_from(obj[0x4B], 0, move_script::At(::bof3::Img(inventory_ops::at::kAnimBuffer.v)), 0xA00);
                Sprite_Current[0xB] = 0;
                continue;
            }
            const unsigned char target = move_script::At(::bof3::Img(inventory_ops::at::kFaceTable.v))[ByteInventoryOps(::bof3::Img(inventory_ops::at::kBattleFacing.v))];
            const unsigned char d = obj[8], turn = obj[0xB];
            if (d == target) {
                if (turn != 0) {
                    inventory_ops::g.set_anim_from(d, 0, move_script::At(::bof3::Img(inventory_ops::at::kAnimBuffer.v)), 0xA00);
                    Sprite_Current[0xB] = 0;
                }
            } else {
                obj[8] = static_cast<unsigned char>((d + turn) & 7);
                inventory_ops::g.set_anim_from(Sprite_Current[8], 0, move_script::At(::bof3::Img(inventory_ops::at::kAnimBuffer.v)), 0xA00);
            }
            inventory_ops::g.script_tick();
        }
        return 0;
    }
    if (EventBattle_Flags & 4) return 1;
    for (u32 j = 0; static_cast<i32>(j) < static_cast<i32>(n); ++j, n = Field_MemberCount) {
        unsigned char* const obj = Obj(j);
        Field_State = obj;
        Sprite_Current = obj;
        for (u32 w = 0; w < 32; ++w) {
            const u32 o = (static_cast<u32>(obj[5]) << 5) + w;
            move_script::SetWord(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1ec0 + 2 * o), move_script::Word(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + 2 * o)));
        }
        inventory_ops::g.shade_begin();
        Sprite_Current[0xB] = 0;
        Sprite_Current[0x5D] = 0xC0;
        Sprite_Current[0x5E] = 0xC0;
        Sprite_Current[0x5F] = 0xC0;
        Sprite_Current[0x5C] = 1;
    }
    return 1;
}

// original 0x532860 (PSX 0x801C0114): one frame of the party's walk to its
// spots. Bit 2 of 0x904AE5: 1 at once. Else the centre is Field_Kind2X / Z
// plus the facing's cell step (0x660B1C); then each member (Field_State and
// Sprite_Current it): +0xB 0 - when Sprite_ShadeLower(8) is done, it is put
// at the spot of its place in the second list (0x904065, by Field_State's
// id +0x89; not found, the fourth spot), Field_State's +0x138 bit 0 cleared,
// +0x29 Draw_OtSlot, its height the ground's (MapView_GroundAt), +0x48 0,
// +0xB 1; +0xB 1 - when Sprite_ShadeRaise(8) is done, its 32 saved CLUT
// words back into Gfx_ClutStrip, Gfx_ClutStripDirty, bit 5 of +0 cleared,
// +0xB, the shades and +0x5C 0, then +0xB 2; then the script ticks. Answers
// whether every member's +0xB is 2 (1 for no members), the count the last
// one read.
extern "C" unsigned char __cdecl Encounter_PartyToPlaces(void) {
    if (EventBattle_Flags & 4) return 1;
    const u32 f = ByteInventoryOps(::bof3::Img(inventory_ops::at::kBattleFacing.v));
    const auto* const step = reinterpret_cast<const signed char*>(move_script::At(::bof3::Img(inventory_ops::at::kSideSteps.v) + 2 * f));
    BossSi_CentreX = AddInventoryOps(Shl(step[0], 16), Field_Kind2X);
    BossSi_CentreZ = AddInventoryOps(Shl(step[1], 16), Field_Kind2Z);
    u32 n = Field_MemberCount;
    if (n != 0) {
        for (u32 i = 0; i < n; ++i, n = Field_MemberCount) {
            unsigned char* const obj = Obj(i);
            Field_State = obj;
            Sprite_Current = obj;
            const unsigned char state = obj[0xB];
            if (state == 0) {
                if (inventory_ops::g.shade_lower(8)) {
                    unsigned char* const fs = Field_State;
                    const unsigned char id = fs[0x89];
                    u32 k = 0;
                    while (k < 3 && Cond_Flags[0xd5 + k] != id) ++k;
                    fs[0x138] = static_cast<unsigned char>(fs[0x138] & 0xFE);
                    Sprite_Current[0x29] = Draw_OtSlot;
                    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, move_script::Long(PosX(k)));
                    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, move_script::Long(PosZ(k)));
                    const long ground = inventory_ops::g.ground_at(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
                    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(ground));
                    Sprite_Current[0x48] = 0;
                    Sprite_Current[0xB] = 1;
                }
            } else if (state == 1) {
                if (inventory_ops::g.shade_raise(8)) {
                    unsigned char* const sc = Sprite_Current;
                    for (u32 w = 0; w < 32; ++w) {
                        const u32 o = (static_cast<u32>(sc[5]) << 5) + w;
                        move_script::SetWord(move_script::At(::bof3::Img(Gfx_ClutStrip_addr) + 0x1e00 + 2 * o), move_script::Word(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1ec0 + 2 * o)));
                    }
                    Gfx_ClutStripDirty = 1;
                    sc[0] = static_cast<unsigned char>(sc[0] & 0xDF);
                    Sprite_Current[0xB] = 0;
                    Sprite_Current[0x5D] = 0;
                    Sprite_Current[0x5E] = 0;
                    Sprite_Current[0x5F] = 0;
                    Sprite_Current[0x5C] = 0;
                    Sprite_Current[0xB] = 2;
                }
            }
            inventory_ops::g.script_tick();
        }
    }
    if (static_cast<i32>(n) <= 0) return 1;
    for (u32 i = 0; i < n; ++i)
        if (Obj(i)[0xB] != 2) return 0;
    return 1;
}

// original 0x532A70 (the PSX twin not paired): each member (Field_State and
// Sprite_Current it, the count read per member) at its placed spot - x
// through the held object, z through the sprite - on the ground
// (MapView_GroundAt), +0xB 0, and the animation the battle's facing (+0x1C
// when bit 6 of its record's byte +0x11 is set, the record Field_State's
// +0x148). The original first searches the first party list for the
// member's id +0x89 and never uses what it finds: a read, not reproduced.
extern "C" void __cdecl Encounter_PartyAtPlaces(void) {
    for (u32 i = 0; i < Field_MemberCount; ++i) {
        unsigned char* const obj = Obj(i);
        Field_State = obj;
        Sprite_Current = obj;
        rv::As<records::PartyMember>(obj)->x = move_script::Long(PosX(i));
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, move_script::Long(PosZ(i)));
        const long ground = inventory_ops::g.ground_at(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
        rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(ground));
        Sprite_Current[0xB] = 0;
        const u32 record = Field_State[0x148];
        if (move_script::At(::bof3::Img(CharacterRecords_addr) + record * inventory_ops::at::kRecordSize + 0x11)[0] & 0x40)
            inventory_ops::g.set_animation(static_cast<unsigned char>(ByteInventoryOps(::bof3::Img(inventory_ops::at::kBattleFacing.v)) + 0x1C));
        else
            inventory_ops::g.set_animation(ByteInventoryOps(::bof3::Img(inventory_ops::at::kBattleFacing.v)));
    }
}

// original 0x532B60 (the PSX twin not paired): bit 4 of 0x904AE5 answers 1
// at once; else each member (Field_State and Sprite_Current it) whose +0xB is
// 0 gets +0xB 1 when Sprite_ScriptTickOnce answers non-zero. Answers whether
// every member's +0xB is 1 (1 for no members), the count the last one read.
// Like 0x532A70 it first searches the first party list for nothing.
extern "C" unsigned char __cdecl Encounter_PartyScriptOnce(void) {
    if (EventBattle_Flags & 0x10) return 1;
    u32 n = Field_MemberCount;
    if (n != 0) {
        for (u32 i = 0; i < n; ++i, n = Field_MemberCount) {
            unsigned char* const obj = Obj(i);
            Field_State = obj;
            Sprite_Current = obj;
            if (obj[0xB] == 0 && inventory_ops::g.script_once()) Sprite_Current[0xB] = 1;
        }
    }
    if (static_cast<i32>(n) <= 0) return 1;
    for (u32 i = 0; i < n; ++i)
        if (Obj(i)[0xB] != 1) return 0;
    return 1;
}

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
        rv::Store<std::int32_t>(rv::As<records::PartyMember>(MemberFieldE1(i))->x, L(::bof3::Img(field_e1::at::kSlotPositions) + 8 * i));
        rv::As<records::PartyMember>(MemberFieldE1(i))->z = L(::bof3::Img(field_e1::at::kSlotPositions) + 8 * i + 4);
    }
    unsigned k = 0;
    while (k < n && rv::As<records::PartyMember>(MemberFieldE1(k))->char_id != Cond_Flags[0xd2]) ++k;
    if (EventBattle_Flags & 2) return;
    MoveScript_F3Divisor = 0x20;
    const std::int32_t x = rv::As<records::PartyMember>(MemberFieldE1(k))->x, z = rv::As<records::PartyMember>(MemberFieldE1(k))->z;
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
        Field_State = MemberFieldE1(i);
        Sprite_Current = MemberFieldE1(i);
        SH_CALL(Sprite_ScriptTick)();
    } while (++i < Field_MemberCount);
}

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
        for (unsigned i = 0; i < n; ++i) move_script::SetLong(move_script::At(::bof3::Img(field_e1::at::kCellX_addr) + 4 * i), rv::As<records::PartyMember>(MemberFieldE1(i))->x);
        for (unsigned j = 0; j < n; ++j) rv::As<records::PartyMember>(MemberFieldE1(j))->x = L(::bof3::Img(field_e1::at::kCellX_addr) + 4 * ListSlot(rv::As<records::PartyMember>(MemberFieldE1(j))->char_id, n));
        for (unsigned i = 0; i < n; ++i) move_script::SetLong(move_script::At(::bof3::Img(field_e1::at::kCellX_addr) + 4 * i), rv::As<records::PartyMember>(MemberFieldE1(i))->z);
        unsigned members = n;
        for (unsigned j = 0; j < members; ++j) {
            records::PartyMember* const m = rv::As<records::PartyMember>(MemberFieldE1(j));
            const std::int32_t z = L(::bof3::Img(field_e1::at::kCellX_addr) + 4 * ListSlot(m->char_id, members));
            m->z = z;
            const long ground = SH_CALL(MapView_GroundAt)(m->x, z);
            count = Field_MemberCount;
            m->height_whole = static_cast<std::uint32_t>(ground);
            members = count;
        }
    }
    if (EventBattle_Flags & 0x80) {
        SH_CALL(Party_DropIn)(BossH_LeaderPick);
        SH_CALL(Field_MembersFrame)();
        return;
    }
    if (count == 0) return;
    unsigned i = 0;
    do {
        records::PartyMember* const m = rv::As<records::PartyMember>(MemberFieldE1(i));
        Field_State = rv::Bytes(m);
        Sprite_Current = rv::Bytes(m);
        m->direction = BFieldE1(::bof3::Img(field_e1::at::kFormationAnims) + BFieldE1(::bof3::Img(field_e1::at::kFormation)));
        rv::As<records::PartyMember>(Sc())->ot_slot = 6;
        if (rv::Bytes(m) != rv::Bytes(ObjTrio)) {
            Sc()[7] = 0;
            Sc()[6] = static_cast<unsigned char>(rv::As<records::PartyMember>(Sc())->actor - 1);
        }
        SH_CALL(Field_MemberTimers)();
        SH_CALL(Sprite_SetAnimation)(rv::As<records::PartyMember>(Sc())->direction);
    } while (++i < Field_MemberCount);
}

// original 0x532ED0: the members placed for an event battle. The x and z
// (16.16) to 0x903780 / 0x903784, the formation byte 0x904AAC from
// EventBattle_Records[n] +1 (n the third argument's low byte, unchecked).
// Then for each member i below Field_MemberCount (read again each time):
// Field_State and Sprite_Current its ObjTrio record, its slot j the index in
// the second list of the record's +0x89 (3 when absent); 0x532FD0(x, z, j) -
// the slot passed in the argument's dword, whose upper bytes stay the
// caller's; Sprite_Current's +0x34 / +0x38 to the slot's pair at 0x7E06E0,
// +0x48 0, +8 from BattleFormation_Anims[formation] and
// Sprite_EnsureAnimation of it. No PSX twin paired.
extern "C" void __cdecl Party_PlaceForBattle(std::int32_t x, std::int32_t z, unsigned formation_arg) {
    BossSi_CentreX = x;
    const unsigned char formation = move_script::At(::bof3::Img(bof3::addr::EventBattle_Records))[(formation_arg & 0xFF) * 4 + 1];
    BossSi_CentreZ = z;
    move_script::At(::bof3::Img(scena_sx::at::kFormation))[0] = formation;
    if (Field_MemberCount == 0) return;
    std::uint32_t slot_arg = formation_arg;
    for (unsigned i = 0; static_cast<int>(i) < Field_MemberCount; ++i) {
        unsigned char* const o = ObjScenaSx(i);
        Field_State = o;
        Sprite_Current = o;
        const unsigned char id = o[0x89];
        unsigned char j = 0;
        for (;;) {
            slot_arg = (slot_arg & 0xFFFFFF00u) | j;
            if (Cond_Flags[0xd5 + j] == id) break;
            ++j;
            slot_arg = (slot_arg & 0xFFFFFF00u) | j;
            if (j >= 3) break;
        }
        SH_AT(void (__cdecl*)(std::int32_t, std::int32_t, std::uint32_t), scena_sx::at::kFormationPlace.v)(
            static_cast<std::int32_t>(BossSi_CentreX), static_cast<std::int32_t>(BossSi_CentreZ), slot_arg);
        unsigned char* const s = Sprite_Current;
        unsigned char* const pos = move_script::At(::bof3::Img(scena_sx::at::kSlotPositions) + (slot_arg & 0xFF) * 8);
        move_script::SetLong(pos, rv::As<records::SpriteObject>(s)->x);
        move_script::SetLong(pos + 4, rv::As<records::SpriteObject>(s)->z);
        s[0x48] = 0;
        s[8] = move_script::At(::bof3::Img(bof3::addr::BattleFormation_Anims))[move_script::At(::bof3::Img(scena_sx::at::kFormation))[0]];
        const auto p = Low32(s);
        EnsureAnimation((p & 0xFFFFFF00u) | s[8]);
    }
}

// original 0x532FD0: Sprite_Current placed at (x, z) plus the event battle's
// formation offset for a slot, then grounded (Party_PlaceForBattle's one
// callee). The group g is the byte 0x904060, plus 1 at two members, plus 4 at
// three (bytes, wrapping; Field_MemberCount read once); the row is
// Formation_SlotVectors[g * 3 + slot] (slot the third argument's low byte;
// both unchecked), a pair of dwords (dx, dz). The formation byte f (0x904AAC)
// names a pair of s8 at BattleFormation_Offsets[2 f]: with a not 0, +0x34 =
// dx * (a / 4) + x and +0x38 = dz * (a / 4) + z; else +0x34 = x - dz * (b /
// 4) and +0x38 = dx * (b / 4) + z - the vector turned a quarter. Products
// wrap at 32 bits; f and the pair are read again for z. Then +0x3E =
// MapView_GroundAt(+0x34, +0x38)'s word (Sprite_Current read again after the
// call). No PSX twin paired.
extern "C" void __cdecl Party_PlaceInFormation(int x, int z, unsigned slot) {
    const unsigned char count = Field_MemberCount;
    const unsigned char lead = Cond_Flags[0xd0];
    unsigned char g = lead;
    if (count == 2)
        g = static_cast<unsigned char>(lead + 1);
    else if (count == 3)
        g = static_cast<unsigned char>(lead + 4);
    const unsigned char* const pairs = AtScenaSx2(::bof3::Img(bof3::addr::BattleFormation_Offsets));
    const unsigned char* const row = AtScenaSx2(::bof3::Img(bof3::addr::Formation_SlotVectors) + (g * 3u + (slot & 0xFF)) * 8);
    const unsigned f = AtScenaSx2(::bof3::Img(scena_sx2::at::kFormation))[0];
    const unsigned char a = pairs[f * 2];
    std::uint32_t zd;
    if (a != 0) {
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(UScenaSx2(LongScenaSx2(row)) * UScenaSx2(Quarter(a)) + UScenaSx2(x)));
        const unsigned f2 = AtScenaSx2(::bof3::Img(scena_sx2::at::kFormation))[0];
        zd = UScenaSx2(Quarter(pairs[f2 * 2])) * UScenaSx2(LongScenaSx2(row + 4));
    } else {
        rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(UScenaSx2(x) - UScenaSx2(Quarter(pairs[f * 2 + 1])) * UScenaSx2(LongScenaSx2(row + 4))));
        const unsigned f2 = AtScenaSx2(::bof3::Img(scena_sx2::at::kFormation))[0];
        zd = UScenaSx2(Quarter(pairs[f2 * 2 + 1])) * UScenaSx2(LongScenaSx2(row));
    }
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(zd + UScenaSx2(z)));
    unsigned char* const s = Sprite_Current;
    const long ground = SH_CALL(MapView_GroundAt)(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<std::uint32_t>(ground));
}

// original 0x533110 (PSX 0x801C0F94): the area set-up's party placement,
// called once per area by 0x594E60 with the entry point and its flags.
//
// As the original has it: `flags` is handed on whole to Field_PartyPosition /
// 0x533690, as the original pushes it; the area number is read once for its
// two tests at the start and again for 0xBD at the end of the leader-alone
// branch; the actor record's byte +9 (which member the leader is) is read
// again for each use; Field_MemberCount bounds the member-by-member loop and
// is read again at each turn; Party_Count's count is stored first as the
// member count, and the loop's first test is on that count, not re-read.
extern "C" void __cdecl Field_PartySetUp(long x, long z, unsigned flags) {
    Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & 0xEFFF);
    bool whole_party = (Field_ScriptFlags2 & 0x8000) != 0;
    if (!whole_party) {
        const unsigned char input = Field_InputFlags;
        const unsigned short area = Game_AreaNumber;
        if (!(input & 9) && area != 0xBD && area != 0xBB) {
            if (!(input & 0x40))
                whole_party = true;
            else if (field_event::g.flags_test(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x77))
                whole_party = true;
        }
    }
    if (!whole_party) {
        // The leader alone.
        const unsigned count = static_cast<unsigned char>(field_event::g.party_count(0));
        const unsigned char* const list = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2);
        for (unsigned i = 0; i < count; ++i) {
            unsigned char* const word = Field_ActorStates + Character_IndexById[list[i]] * kActorBytes;
            move_script::SetWord(word, move_script::Word(word) & 0xFFDF);
        }
        for (unsigned i = 0; i < count; ++i) {
            if (Field_ActorStates[Character_IndexById[list[i]] * kActorBytes] & 0x80) {
                rv::Bytes(ObjTrio)[0x124] = 10;
                break;
            }
        }
        const unsigned char input = Field_InputFlags;
        ObjTrio[0].actor = 0;
        Sprite_Current = rv::Bytes(ObjTrio);
        CopyActor(rv::Bytes(ObjTrio) + 0x80, Actor(0));
        const unsigned char bits = rv::Bytes(ObjTrio)[0x138];
        Field_MemberCount = 1;
        ObjTrio[0].char_index = 0;
        rv::Bytes(ObjTrio)[0x138] = static_cast<unsigned char>(bits & 0xFE);
        ObjTrio[0].scale_mode = 0;
        if (input & 1) {
            const unsigned char who = Actor(0)[9];
            if (who == 0)
                field_event::g.set_bank(0x52);
            else if (who == 9)
                field_event::g.set_bank(0xCB);
            else if (who == 7)
                field_event::g.set_bank(0x27A);
            Sprite_Current[0x24] = 0;
            *(reinterpret_cast<unsigned char*>(&Field_ScriptFlags) + 1) |= 0x10;
        } else if (input & 8) {
            field_event::g.set_bank(Actor(0)[9] == 0 ? 0x47 : 0x1FD);
            Sprite_Current[0x2A] = 0;
            Sprite_Current[0x24] = 0;
        } else if (input & 0x40) {
            field_event::g.member_sprite(Actor(0)[9], 0);
        } else if (Game_AreaNumber == 0xBD) {
            ObjTrio[0].key_class = 0;
            ObjTrio[0].reach = 0;
        }
    } else {
        // The whole party.
        const auto count = static_cast<unsigned char>(field_event::g.party_count(0));
        Field_MemberCount = count;
        if (!(Field_ScriptFlags2 & 0x8000)) {
            field_event::g.party_load(0);
        } else if (count != 0) {
            for (unsigned i = 0;;) {
                records::PartyMember* const m = rv::As<records::PartyMember>(Member(i));
                Sprite_Current = rv::Bytes(m);
                m->flags = static_cast<unsigned char>(m->flags | 1);
                rv::Bytes(m)[0x138] = static_cast<unsigned char>(rv::Bytes(m)[0x138] & 0xFE);
                rv::Current<records::PartyMember>()->x = 0;
                rv::Current<records::PartyMember>()->z = 0;
                rv::Current<records::PartyMember>()->height = 0;
                const unsigned char id = Cond_Flags[0xd2 + i];
                const unsigned char actor = Character_IndexById[id];
                m->char_index = actor;
                CopyActor(rv::Bytes(m) + 0x80, Actor(actor));
                if (!(Sprite_Current[0x24] & 1)) {
                    field_event::g.member_sprite(id, i);
                } else {
                    field_event::g.member_sprite(id, i);
                    field_event::g.release_tint(Sprite_Current);
                    Sprite_Current[0x27] = static_cast<unsigned char>(rv::Current<records::PartyMember>()->actor + 0x78);
                    field_event::g.load_palette(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + rv::Current<records::PartyMember>()->actor * 64u)), 0);
                }
                ++i;
                if (i >= Field_MemberCount) break;
            }
        }
    }
    // Where the kind-2 object and the camera start.
    if (Field_InputFlags & 8) {
        ObjTrio[0].z = 0x410000;
        ObjTrio[0].x = 0xF0000;
        Field_Kind2X = 0xF0000;
        Field_Kind2Z = 0x3C0000;
        return;
    }
    if (flags & 0x80) {
        Field_Kind2X = x;
        Field_Kind2Z = z;
        return;
    }
    if (Field_ScriptFlags & 0x800)
        field_event::g.position_alt(x, z, flags);
    else
        field_event::g.position(x, z, flags);
    const long leader_x = ObjTrio[0].x, leader_z = ObjTrio[0].z;
    Field_Kind2X = leader_x;
    Field_Kind2Z = leader_z;
}

// original 0x5334A0 (PSX 0x801C15A4): the placed party's first frame - each
// member's frame once (the leader's through Field_LeaderFrame), unless
// Field_ScriptFlags2 bit 0x8000 holds them, which clears their bit 0x40.
//
// As the original has it: the +0x29 loop runs to the member count read before
// it; the frame loop's bound is read again at each turn, and its mask is
// 1 << i with x86's shift (the count mod 32).
extern "C" void __cdecl Field_PartyFirstFrame(void) {
    if (Field_ScriptFlags2 & 0x8000) {
        const unsigned count = Field_MemberCount;
        for (unsigned i = 0; i < count; ++i) Member(i)[0] = static_cast<unsigned char>(Member(i)[0] & 0xBF);
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xBFFF);
        return;
    }
    field_event::g.clear_active();
    const unsigned char input = Field_InputFlags;
    const unsigned char count = Field_MemberCount;
    if (input & 8) {
        ObjTrio[0].state = 9;
    } else {
        for (unsigned i = 0; i < count; ++i) Member(i)[0x29] = 6;
    }
    if (count != 0) {
        for (unsigned i = 0;;) {
            if (!(Field_ScriptFlags & (1u << (i & 31)))) {
                records::PartyMember* const m = rv::As<records::PartyMember>(Member(i));
                const unsigned char slot = m->actor;
                Field_State = rv::Bytes(m);
                Sprite_Current = rv::Bytes(m);
                if (slot == 0)
                    field_event::g.leader_frame();
                else
                    field_event::g.member_frame();
            }
            ++i;
            if (i >= Field_MemberCount) break;
        }
    }
    Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xBFFF);
}

// original 0x533580 (PSX 0x801C1750): each member at (x, z) facing
// `direction` - or, with its +0x70 byte non-zero, at the offset 0x660B40
// gives for direction / 2 (subtracted with Field_ScriptFlags2 bit 0x4000) -
// grounded; bit 0x40 set on members 1 and 2, cleared on the leader.
//
// As the original has it: only the low byte of direction is used, the offset
// index is unchecked (directions above 7 read past the four pairs), the
// arithmetic wraps, and the ground is read with the position just stored.
extern "C" void __cdecl Field_PartyPosition(long x, long z, unsigned direction) {
    if (Field_MemberCount == 0) return;
    const auto d = static_cast<unsigned char>(direction);
    for (unsigned i = 0;;) {
        records::PartyMember* const m = rv::As<records::PartyMember>(Member(i));
        if (rv::Bytes(m)[0x70] == 0) {
            m->x = x;
            m->z = z;
        } else {
            const unsigned char* const offset = move_script::At(::bof3::Img(Field_PlaceOffsets_addr)) + (d >> 1) * 8u;
            const auto ux = static_cast<std::uint32_t>(x), uz = static_cast<std::uint32_t>(z);
            if (Field_ScriptFlags2 & 0x4000) {
                m->x = static_cast<std::int32_t>(ux - static_cast<std::uint32_t>(move_script::Long(offset)));
                m->z = static_cast<std::int32_t>(uz - static_cast<std::uint32_t>(move_script::Long(offset + 4)));
            } else {
                m->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(offset)) + ux);
                m->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(offset + 4)) + uz);
            }
        }
        m->direction = d;
        const long ground = field_event::g.ground_at(m->x, m->z);
        m->height_whole = static_cast<std::uint16_t>(ground);
        if (i != 0)
            m->flags = static_cast<unsigned char>(m->flags | 0x40);
        else
            ObjTrio[0].flags = static_cast<unsigned char>(ObjTrio[0].flags & 0xBF);
        ++i;
        if (i >= Field_MemberCount) break;
    }
}

// original 0x533690 (PSX 0x801C1904): the leader placed at (x, z) facing `direction` - alone,
// there; with others, set off by twice the step of 0x660B40 for direction >>
// 1 (thrice when its +0x70), and once more when any member after it has its
// +0x70. Called by Field_PartySetUp with Field_ScriptFlags bit 11.
extern "C" void __cdecl Field_LeaderPlaceOffset(long x, long z, unsigned direction) {
    const unsigned char n = Field_MemberCount;
    records::PartyMember* const leader = ObjTrio;
    if (n <= 1) {
        leader->x = static_cast<std::int32_t>(x);
        leader->z = static_cast<std::int32_t>(z);
        leader->direction = static_cast<unsigned char>(direction);
        return;
    }
    const std::uint32_t e = (direction & 0xFFu) >> 1;
    const auto ox = static_cast<std::uint32_t>(L(::bof3::Img(field_e1::at::kPlaceOffsets) + e * 8));
    const auto oz = static_cast<std::uint32_t>(L(::bof3::Img(field_e1::at::kPlaceOffsets) + e * 8 + 4));
    leader->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(x) + ox * 2);
    leader->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(z) + oz * 2);
    if (rv::Bytes(leader)[0x70] != 0) {
        leader->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(leader->x) + ox);
        leader->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(leader->z) + oz);
    }
    leader->direction = static_cast<unsigned char>(direction);
    for (unsigned c = 1; c < n; ++c) {
        if (MemberFieldE1(c)[0x70] != 0) {
            leader->x = static_cast<std::int32_t>(static_cast<std::uint32_t>(leader->x) + ox);
            leader->z = static_cast<std::int32_t>(static_cast<std::uint32_t>(leader->z) + oz);
            return;
        }
    }
}

// original 0x533760 (PSX 0x801C1A60): a pending jump - while the byte
// 0x904EF0 is set, through the four-entry table 0x660B60 by the byte after
// it. As the original has it: the index is unchecked and the call is a tail
// jump whose eax no caller keeps (Field_MembersFrame reads al only after
// loading it).
extern "C" void __cdecl Field_PendingJump(void) {
    if (EventLeader_PendingFlag == 0) return;
    Dispatch(field_event::g.pending_jumps, *move_script::At(::bof3::Img(EventLeader_PendingFlag_addr) + 1));
}

// original 0x533780 (PSX 0x801C1AAC): the pending jump's entry 0 (0x660B60, Field_PendingJump
// jumps to it while 0x904EF0 is set; Area_Enter 0x533710 sets it) - the party
// walking in. First frame (0x904EF2 = 0): the leader's +0 bit 6 and state
// cleared, +0x138 bit 1 cleared, the count 1. Later, once the leader is a
// whole unit or more from the stored point (0x904EF4 / 0x904EF8) on either
// axis: each member after the leader whose bit (1 << the count) is clear in
// Field_ScriptFlags is released - +0 bit 6, state 2 sub-state 2, +3 / +4 0,
// +0x138 bit 1, and its bit cleared in Field_ScriptFlags2 - counting one per
// member. When the count reaches the member count: MoveCmd_TestFC at the
// point's whole parts, Field_ScriptFlags2 bit 4 cleared, 0x904EF0 = 0.
// As the original has it: the bit tested is the running count's (from 1, so
// member i's own while the walk-in runs once), shifted mod 32, and cleared in
// the 16-bit Field_ScriptFlags2 only.
extern "C" void __cdecl Field_PendingWalkIn(void) {
    unsigned char n = EventLeader_PendingCount;
    if (n == 0) {
        ObjTrio[0].flags = static_cast<unsigned char>(ObjTrio[0].flags & 0xBF);
        event_leader::g.clear_state(0);
        const unsigned char f = rv::Bytes(ObjTrio)[0x138];
        n = EventLeader_PendingCount;
        rv::Bytes(ObjTrio)[0x138] = static_cast<unsigned char>(f & 0xFD);
        n = static_cast<unsigned char>(n + 1);
        EventLeader_PendingCount = n;
    } else {
        const std::int32_t dx = Abs(static_cast<std::uint32_t>(ObjTrio[0].x) - EventLeader_PendingX);
        if (dx < 0x10000) {
            const std::int32_t dz = Abs(static_cast<std::uint32_t>(ObjTrio[0].z) - EventLeader_PendingZ);
            if (dz < 0x10000) return;
        }
        const unsigned count = Field_MemberCount;
        if (static_cast<int>(count) > 1) {
            unsigned short flags2 = Field_ScriptFlags2;
            const unsigned flags = Field_ScriptFlags;
            unsigned left = count - 1;
            unsigned char* m = Member(1);
            do {
                const std::uint32_t bit = 1u << (n & 31u);
                if (!(flags & bit)) {
                    m[0] = static_cast<unsigned char>(m[0] & 0xBF);
                    m[1] = 2;
                    m[2] = 2;
                    m[3] = 0;
                    m[4] = 0;
                    m[0x138] = static_cast<unsigned char>(m[0x138] & 0xFD);
                    flags2 = static_cast<unsigned short>(flags2 & ~bit);
                }
                n = static_cast<unsigned char>(n + 1);
                m += event_leader::at::kMemberStride;
            } while (--left != 0);
            Field_ScriptFlags2 = flags2;
            EventLeader_PendingCount = n;
        }
    }
    if (n == Field_MemberCount) {
        event_leader::g.test_fc(static_cast<short>(event_leader::Word(event_leader::At(::bof3::Img(EventLeader_PendingX_addr) + 2))), static_cast<short>(event_leader::Word(event_leader::At(::bof3::Img(EventLeader_PendingZ_addr) + 2))));
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFEF);
        EventLeader_PendingFlag = 0;
    }
}

// original 0x5338B0 (PSX 0x801C1CD4): pending jump 1 - the member at 0x904EF2 released with
// kind 2 when its facing +8 is 5 or 3, else kind 3.
extern "C" void __cdecl Field_PendingJumpTurn(void) {
    const unsigned char f = MemberFieldE1(EventLeader_PendingCount)[8];
    SH_CALL(Field_PendingRelease)(f == 5 || f == 3 ? 2 : 3);
}

// original 0x5338F0 (PSX 0x801C1D2C): pending jump 2 - released with kind 4.
extern "C" void __cdecl Field_PendingJumpKind4(void) { SH_CALL(Field_PendingRelease)(4); }

// original 0x533900 (PSX 0x801C1D4C): pending jump 3 - when Field_PendingNext makes a member
// current: its pose 0x3A (facing 3) or 0x3B, +0 bit 6 cleared, its height +0x3E
// the ground plus 0x7D0, Sprite_ClearSteps, +0x20 = -8, state 2 / 3 / 3, and
// Field_State +0x138 bit 1 cleared.
extern "C" void __cdecl Field_PendingDrop(void) {
    if (!SH_CALL(Field_PendingNext)()) return;
    SH_CALL(Sprite_EnsureAnimation)(rv::As<records::PartyMember>(Sc())->direction == 3 ? 0x3A : 0x3B);
    rv::As<records::PartyMember>(Sc())->flags = static_cast<unsigned char>(rv::As<records::PartyMember>(Sc())->flags & 0xBF);
    const long ground = SH_CALL(MapView_GroundAt)(rv::As<records::PartyMember>(Sc())->x, rv::As<records::PartyMember>(Sc())->z);
    rv::Store<unsigned>(rv::As<records::PartyMember>(Sc())->height_whole, static_cast<std::uint32_t>(ground) + 0x7D0);
    SH_CALL(Sprite_ClearSteps)();
    rv::Store<std::int32_t>(rv::As<records::PartyMember>(Sc())->work_20, -8);
    rv::As<records::PartyMember>(Sc())->state = 2;
    rv::As<records::PartyMember>(Sc())->step = 3;
    rv::As<records::PartyMember>(Sc())->state3 = 3;
    Fs()[0x138] = static_cast<unsigned char>(Fs()[0x138] & 0xFD);
}

// original 0x5339A0 (PSX 0x801C1E40): the next member of the pending jump - with 0x904EF2 at n
// not 0, only once member n - 1 is in state 1 (else al 0); at n 1 bit 3 of
// Field_ScriptFlags2 cleared and, with one member, bit 9 too and the jump over
// (0x904EF0 = 0, al 0); bit n (mod 32) cleared. Then member n current
// (Sprite_Current and Field_State), 0x904EF2 one on, and when that reaches
// the count (more than one member) bits 3, 4, 9 cleared and the jump over;
// al 1.
extern "C" unsigned char __cdecl Field_PendingNext(void) {
    const unsigned char n = EventLeader_PendingCount;
    const unsigned char count = Field_MemberCount;
    if (n != 0) {
        if (MemberFieldE1(n - 1u)[1] != 1) return 0;
        if (n == 1) {
            Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFF7);
            if (count == n) {
                Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFDFF);
                EventLeader_PendingFlag = 0;
                return 0;
            }
        }
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~(1u << (n & 31)));
    }
    const auto next = static_cast<unsigned char>(n + 1);
    EventLeader_PendingCount = next;
    Sprite_Current = MemberFieldE1(n);
    Field_State = MemberFieldE1(n);
    if (count != 1 && next == count) {
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFDE7);
        EventLeader_PendingFlag = 0;
    }
    return 1;
}

// original 0x533A50 (PSX 0x801C1F94): the member at 0x904EF2 (n) released - unless its bit n
// of Field_ScriptFlags (read once) is set, its +0 bit 6 cleared, its state
// bytes +1..+4 the kind's record of 0x660B70 and +0x138 bit 1 cleared. With n
// not 0 it waits (al 0) until bit n or bit n - 1 is set or member n - 1 is in
// state 1, and at n 2 with bit 1 set until member 0 is in state 1; bit n of
// Field_ScriptFlags2 is cleared then. 0x904EF2 one on; at the count bit 4
// cleared, the jump over and al 2, else al 1.
// As the original has it: the shifts are by n (and n - 1) mod 32, so at n 0 the
// "bit n - 1" is bit 31 of a 16-bit word, never set.
extern "C" unsigned char __cdecl Field_PendingRelease(unsigned kind) {
    const unsigned char n = EventLeader_PendingCount;
    const std::uint32_t flags = Field_ScriptFlags;
    const auto bit = [](unsigned s) { return 1u << (s & 31); };
    if (n != 0 && !(flags & bit(n)) && !(flags & bit(n - 1u)) && MemberFieldE1(n - 1u)[1] != 1) return 0;
    const std::uint32_t below = flags & bit(static_cast<unsigned>(n) - 1u);
    if (below != 0 && n == 2 && ObjTrio[0].state != 1) return 0;
    if (n != 0) {
        if (!(flags & bit(n)) && below == 0 && MemberFieldE1(n - 1u)[1] != 1) return 0;
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~bit(n));
    }
    if (!(flags & bit(n))) {
        records::PartyMember* const m = rv::As<records::PartyMember>(MemberFieldE1(n));
        m->flags = static_cast<unsigned char>(m->flags & 0xBF);
        const std::uint32_t r = ::bof3::Img(field_e1::at::kPendingStates) + (kind & 0xFFu) * 4;
        m->state = BFieldE1(r);
        m->step = BFieldE1(r + 1);
        m->state3 = BFieldE1(r + 2);
        m->state4 = BFieldE1(r + 3);
        rv::Bytes(m)[0x138] = static_cast<unsigned char>(rv::Bytes(m)[0x138] & 0xFD);
    }
    const auto next = static_cast<unsigned char>(n + 1);
    EventLeader_PendingCount = next;
    if (next == Field_MemberCount) {
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & 0xFFEF);
        EventLeader_PendingFlag = 0;
        return 2;
    }
    return 1;
}

// original 0x533BA0 (PSX 0x801C224C): Sprite_Current set up as party member
// `member` in slot `slot` - its column in the loaded party set, its bank, its
// draw fields, and its palette loaded into the slot's 64 bytes.
//
// As the original has it: the column is 3 when the member is not in the set
// row (and +0x4C is then read from the bank table's fourth dword); the row is
// (0x90412C & 0x7F) * 3, unchecked against the table's 19 rows; +0x26 is the
// low byte of slot * 0x50; +0x70 keeps its upper 24 bits and gets
// (+0x2B != 0) as its low byte, both read back from the object.
extern "C" void __cdecl Field_MemberSprite(unsigned member_arg, unsigned slot_arg) {
    const unsigned member = member_arg & 0xFF, slot = slot_arg & 0xFF;
    const unsigned char* const row = move_script::At(::bof3::Img(MoveScript_EffectArg_addr) + 0xc) + (AreaW0b_PartySet & 0x7Fu) * 3u;
    unsigned char column = 0;
    while (row[column] != member)
        if (++column >= 3) break;
    field_event::g.release_tint(Sprite_Current);
    move_script::SetWord(Sprite_Current + 0x2C, column);
    {
        unsigned char* const c = Sprite_Current;
        const unsigned index = move_script::Word(c + 0x2C);
        move_script::SetLong(c + 0x4C, static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(move_script::At(::bof3::Img(field_event::at::kBankTable)) + index * 4u)) + ::bof3::Img(field_event::at::kBankTable)));
    }
    Sprite_Current[0x24] = 1;
    Sprite_Current[0x27] = static_cast<unsigned char>(slot + 0x78);
    Sprite_Current[0x28] = 2;
    Sprite_Current[0x25] = 5;
    Sprite_Current[0x26] = static_cast<unsigned char>(slot * 0x50);
    Sprite_Current[0x2B] = move_script::At(::bof3::Img(Character_IndexById_addr) + 0xc)[member];
    {
        unsigned char* const c = Sprite_Current;
        move_script::SetLong(c + 0x70, static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(c + 0x70)) & 0xFFFFFF00u));
    }
    {
        unsigned char* const c = Sprite_Current;
        move_script::SetLong(c + 0x70, static_cast<std::int32_t>(static_cast<std::uint32_t>(move_script::Long(c + 0x70)) | (c[0x2B] != 0 ? 1u : 0u)));
    }
    Sprite_Current[0x29] = Draw_OtSlot;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5C] = 0;
    field_event::g.load_palette(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + slot * 64u)), 0);
}

// original 0x533CE0 (PSX 0x801C23F8): Field_MemberCount members from the
// party list `slot` - each on, its sprite, scale 1.0, its actor record, and a
// tint (slot non-zero, actor bit 0x80) or its timers (slot 0).
//
// As the original has it: Party_ClearAll is skipped when Field_Request is 6
// (a member joining); Field_MemberCount is read again at every turn of the
// loop and bounds nothing else - more than 3 walks past ObjTrio; the tint's
// -4 and -8 are pushed as dwords, and Sprite_SetTint reads them as bytes.
extern "C" void __cdecl Field_PartyLoad(unsigned slot_arg) {
    const unsigned slot = slot_arg & 0xFF;
    if (Field_Request != 6) field_event::g.clear_all();
    if (Field_MemberCount == 0) return;
    const unsigned char* const list = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2) + slot * 3u;
    for (unsigned i = 0;;) {
        records::PartyMember* const m = rv::As<records::PartyMember>(Member(i));
        Field_State = rv::Bytes(m);
        Sprite_Current = rv::Bytes(m);
        m->flags = 1;
        rv::Current<records::PartyMember>()->actor = static_cast<unsigned char>(i);
        field_event::g.member_sprite(list[i], i);
        Sprite_Current[0x48] = 0;
        rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->scale_x, 0x10000);
        rv::Store<std::int32_t>(rv::Current<records::PartyMember>()->scale_y, 0x10000);
        const unsigned char actor = Character_IndexById[list[i]];
        m->char_index = actor;
        CopyActor(rv::Bytes(m) + 0x80, Actor(actor));
        rv::Bytes(m)[0x138] = static_cast<unsigned char>(rv::Bytes(m)[0x138] & 0xFE);
        if (slot != 0) {
            if (Field_ActorStates[m->char_index * kActorBytes] & 0x80) field_event::g.set_tint(Sprite_Current, 0xFC, 0xF8, 0, 0);
        } else {
            field_event::g.member_timers();
        }
        ++i;
        if (i >= Field_MemberCount) break;
    }
}

// original 0x533E00: for each member i below Field_MemberCount (read again
// each time): Sprite_Current its ObjTrio record, Sprite_ReleaseTint(the
// record), Sprite_LoadPalette(0x80D380 + 0x40 i, 0). Called by 63 of the
// chapter call tables' entries (most as a tail jmp) and group SE's
// Scena08_PartyJoin784. No PSX twin paired.
extern "C" void __cdecl Party_ReloadPalettes(void) {
    if (Field_MemberCount == 0) return;
    for (unsigned i = 0; static_cast<int>(i) < Field_MemberCount; ++i) {
        unsigned char* const o = ObjScenaSx(i);
        Sprite_Current = o;
        SH_CALL(Sprite_ReleaseTint)(o);
        SH_CALL(Sprite_LoadPalette)(reinterpret_cast<unsigned short*>(move_script::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + 0x40 * i)), 0);
    }
}

// original 0x533E50: each of the eight CharacterRecords with +0xB bit 0 (the
// joined bit): +0x1E 0, Char_RecalcStats(record), then +0x1C = +0x2E, HP +0x18
// = +0x20, AP +0x1A = +0x22, the state word +0x10 0 - read after the call.
// Then, for each member i below Field_MemberCount (read once, after the
// eight), the member's record (Character_IndexById of the first list's id,
// unchecked) copied whole (0xA4 bytes) over ObjTrio record i +0x80. No PSX
// twin paired; Party_RestoreAll (0x580630, the inn's) is its larger cousin.
extern "C" void __cdecl Party_HealJoined(void) {
    for (unsigned r = 0; r < 8; ++r) {
        unsigned char* const rec = RecordScenaSx(r);
        if (!(rec[0xB] & 1)) continue;
        rec[0x1E] = 0;
        SH_CALL(Char_RecalcStats)(rec);
        rec[0x1C] = rec[0x2E];
        rv::Store<unsigned>(rv::As<records::CharRecord>(rec)->hp, rv::As<records::CharRecord>(rec)->stats.max_hp);
        rv::Store<unsigned>(rv::As<records::CharRecord>(rec)->ap, rv::As<records::CharRecord>(rec)->stats.max_ap);
        rv::Store<unsigned>(rv::As<records::CharRecord>(rec)->status, 0);
    }
    const int n = Field_MemberCount;
    for (int i = 0; i < n; ++i)
        std::memmove(ObjScenaSx(static_cast<unsigned>(i)) + 0x80, MemberRecord(Cond_Flags[0xd2 + i]), scena_sx::at::kRecordStride);
}

// original 0x533EF0 (PSX 0x801C2874): member `member` joins the party as
// member Field_MemberCount - 0, or 1 (and Field_Request 6) when there are
// three already.
//
// As the original has it: the actor record's +0xB bit 0 is set before the
// test, so even a refused join marks it; the member's slot and the count are
// read again after each call; the actor index is read again for the copy and
// the +0x148 store; `member` is not checked against Character_IndexById's
// 24 entries.
extern "C" unsigned char __cdecl Party_Join(unsigned member_arg) {
    const auto member = static_cast<unsigned char>(member_arg);
    field_event::g.join_reset();
    const unsigned char* const actor_of = Character_IndexById + member;
    unsigned char* const joined = Actor(*actor_of) + 0xB;
    *joined = static_cast<unsigned char>(*joined | 1);
    const unsigned char n = Field_MemberCount;
    if (n >= 3) {
        Field_Request = 6;
        return 1;
    }
    Cond_Flags[0xd2 + n] = member;
    Cond_Flags[0xd2 + n + 3] = member;
    Member(n)[0] = 1;
    field_event::g.clear_state(n);
    unsigned char* const slot = Member(Field_MemberCount);
    Sprite_Current = slot;
    slot[0x24] = 1;
    field_event::g.member_sprite(member, Field_MemberCount);
    const unsigned char actor = *actor_of;
    const unsigned char count = Field_MemberCount;
    CopyActor(Member(count) + 0x80, Actor(actor));
    Sprite_Current[5] = count;
    Sprite_Current[0x4B] = 0xFF;
    const unsigned char k = Field_MemberCount;
    Field_MemberCount = static_cast<unsigned char>(k + 1);
    (rv::Bytes(ObjTrio) + 0x148)[k * kMemberBytes] = *actor_of;
    return 0;
}

// original 0x534010 (PSX 0x801C2838).
extern "C" void __cdecl Party_JoinReset(void) {
    move_script::SetLong(move_script::At(::bof3::Img(field_event::at::kJoinState)), 0);
    move_script::SetLong(move_script::At(::bof3::Img(field_event::at::kJoinState) + 4), 0);
    *move_script::At(::bof3::Img(field_event::at::kJoinState) + 8) = 0;
}

// original 0x534030: a member out of the party (the argument's low byte, an
// id). Party_JoinReset; the id's slot in the second list (the count when it
// is absent, unchecked) becomes 0xFF and moves to the end below the count;
// the id's slot in the first list becomes 0xFF, and each 0xFF below the count
// - 1 (read again each time) takes the next slot's id, the next ObjTrio record
// copied over it (0x14C bytes; Sprite_Current it) and renumbered: +5 its slot,
// +0x26 the slot x 0x50 (a byte), +0x27 0x78 + the slot, +0x4B 0xFF, +0x148
// Character_IndexById[+0x89]. Then ObjTrio record count - 1 +0 cleared,
// Member_ClearState(count - 1), and with the count read again: three -
// 0x904060 cleared when 3 or more, two - cleared; the count one less. No PSX
// twin paired.
extern "C" void __cdecl Party_Remove(unsigned id_arg) {
    SH_CALL(Party_JoinReset)();
    const auto id = static_cast<unsigned char>(id_arg);
    unsigned char* const first = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd2);
    unsigned char* const second = move_script::At(::bof3::Img(Cond_Flags_addr) + 0xd5);
    const int count = Field_MemberCount;
    int k = 0;
    while (k < count && second[k] != id) ++k;
    const int last = count - 1;
    second[k] = 0xFF;
    for (; k < last; ++k) {
        if (second[k] != 0xFF) continue;
        second[k] = second[k + 1];
        second[k + 1] = 0xFF;
    }
    k = 0;
    while (k < count && first[k] != id) ++k;
    first[k] = 0xFF;
    if (last > 0) {
        for (int s = 0; s < Field_MemberCount - 1; ++s) {
            if (first[s] != 0xFF) continue;
            first[s] = first[s + 1];
            unsigned char* const o = ObjScenaSx(static_cast<unsigned>(s));
            Sprite_Current = o;
            first[s + 1] = 0xFF;
            std::memmove(o, o + scena_sx::at::kObjStride, scena_sx::at::kObjStride);
            o[5] = static_cast<unsigned char>(s);
            Sprite_Current[0x26] = static_cast<unsigned char>(s * 0x50);
            Sprite_Current[0x27] = static_cast<unsigned char>(s + 0x78);
            Sprite_Current[0x4B] = 0xFF;
            o[0x148] = Character_IndexById[o[0x89]];
        }
    }
    const unsigned char n = Field_MemberCount;
    const auto below = static_cast<unsigned char>(n - 1);
    ObjTrio[static_cast<int>(n) - 1].flags = 0;
    SH_CALL(Member_ClearState)(below);
    const unsigned char now = Field_MemberCount;
    if (now == 3) {
        if (Cond_Flags[0xd0] >= 3) Cond_Flags[0xd0] = 0;
    } else if (now == 2) {
        Cond_Flags[0xd0] = 0;
    }
    Field_MemberCount = static_cast<unsigned char>(now - 1);
}

// original 0x5341A0 (PSX 0x801C2DE8): entry (n & 0xFF) of the current chapter's call table
// (0x660B84[chapter], PSX Scenario_CallA 0x801C2DE8 through 0x801CDC4C).
// Kept a tail jump: the entries take the caller's arguments where they lie,
// and 153 call sites push anything from one argument to several (cleaned up
// later, some of them together), so no C++ signature forwards them all. None
// of the 153 reads eax after the call (capstone, to the first branch or
// write of eax); the jump passes the entry's through regardless.
// A pair by width (docs/x64-asm-policy.md 8 unit 3): the i686 text as it
// was, and the same tail jump for the x86-64 register arguments.
extern "C" __attribute__((naked)) void __cdecl Scenario_CallA(unsigned) {
#if defined(__i386__)
    // edx holds the image's move first (Bof3_ImageMove, src/hook/host.cpp;
    // task_switch_i686.h says why the VAs are displacements); eax, ecx and
    // edx are the three the original writes.
    asm("movl " BOF3_SYM(Bof3_ImageMove) ", %%edx\n\t"
        "movsbl %c0(%%edx), %%eax\n\t"     // Cond_ByteFA, the chapter
        "movl 4(%%esp), %%ecx\n\t"
        "movl %c1(%%edx,%%eax,4), %%edx\n\t"   // field_modes::kCallATables
        "andl $0xFF, %%ecx\n\t"
        "jmp *(%%edx,%%ecx,4)"
        :
        : "i"(0x8034E0), "i"(0x660B84));
#elif defined(__x86_64__)
    // The same for the Win64 arguments: n in ecx, the rest in rdx, r8, r9 and
    // the caller's stack, forwarded untouched by a tail jump; the chapter
    // sign-extended to 64 bits (a negative one reads below the table, as the
    // i686 wrap does); the table word is a 32-bit code place, loaded into
    // eax and jumped through; r10 is the scratch (volatile, not an
    // argument). The entry sees ecx = n & 0xFF, as the i686 entry does.
    asm("movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%r10\n\t"
        "movsbq %c0(%%r10), %%rax\n\t"          // Cond_ByteFA, the chapter
        "andl $0xFF, %%ecx\n\t"
        "movl %c1(%%r10,%%rax,4), %%r10d\n\t"   // the chapter's table, a 32-bit place
        "movl (%%r10,%%rcx,4), %%eax\n\t"       // the entry's code word
        "jmp *%%rax"
        :
        : "i"(0x8034E0), "i"(0x660B84));
#else
#error "Scenario_CallA: no tail jump for this architecture"
#endif
}

// original 0x5341C0 (PSX 0x801C2E34): entry (n & 0xFF) of the current chapter's call table B
// (Scenario_CallBTables 0x660BD4[Cond_ByteFA], PSX 0x801C2E34), reached with
// the caller's arguments in place. Scenario_CallA's shape (field_modes.cpp):
// a tail jump, because the entries take anything from nothing to several
// words where the callers left them. Unchecked, as the original - chapter 0's
// table pointer is 0 (docs/field_e2.md section 6).
// A pair by width (docs/x64-asm-policy.md 8 unit 3): the i686 text as it
// was, and the same tail jump for the x86-64 register arguments.
extern "C" __attribute__((naked)) void __cdecl Scenario_CallB(unsigned) {
#if defined(__i386__)
    // edx holds the image's move first (Bof3_ImageMove, src/hook/host.cpp;
    // task_switch_i686.h says why the VAs are displacements); eax, ecx and
    // edx are the three the original writes.
    asm("movl " BOF3_SYM(Bof3_ImageMove) ", %%edx\n\t"
        "movsbl %c0(%%edx), %%eax\n\t"     // Cond_ByteFA, the chapter
        "movl 4(%%esp), %%ecx\n\t"
        "movl %c1(%%edx,%%eax,4), %%edx\n\t"   // Scenario_CallBTables
        "andl $0xFF, %%ecx\n\t"
        "jmp *(%%edx,%%ecx,4)"
        :
        : "i"(0x8034E0), "i"(0x660BD4));
#elif defined(__x86_64__)
    // The same for the Win64 arguments: n in ecx, the rest in rdx, r8, r9 and
    // the caller's stack, forwarded untouched by a tail jump; the chapter
    // sign-extended to 64 bits (a negative one reads below the table, as the
    // i686 wrap does); the table word is a 32-bit code place, loaded into
    // eax and jumped through; r10 is the scratch (volatile, not an
    // argument). The entry sees ecx = n & 0xFF, as the i686 entry does.
    asm("movslq " BOF3_SYM(Bof3_ImageMove) "(%%rip), %%r10\n\t"
        "movsbq %c0(%%r10), %%rax\n\t"          // Cond_ByteFA, the chapter
        "andl $0xFF, %%ecx\n\t"
        "movl %c1(%%r10,%%rax,4), %%r10d\n\t"   // the chapter's table, a 32-bit place
        "movl (%%r10,%%rcx,4), %%eax\n\t"       // the entry's code word
        "jmp *%%rax"
        :
        : "i"(0x8034E0), "i"(0x660BD4));
#else
#error "Scenario_CallB: no tail jump for this architecture"
#endif
}

// original 0x5341E0: the tallies GameMode_Field takes on its return from a
// battle (0x4961F2, after Field_EncounterThresholdRoll). Bit 4 of 0x904654 set and
// of 0x904657 clear: the byte 0x9045FB up by one below 0x1E. From chapter 8
// with 0x9046CB not 0: the first clear story flag n of 0xF6..0xFF (none: on
// to the counts); the in-use pairs at 0x9046D0 naming record 0xA and those
// naming 0xB counted (bytes); with the 0xB count - the 0xA count for n 0xF6,
// 0xF9, 0xFC - not 0, the dword 0x9046B0 up by one, and at the threshold
// max(5, t[n - 0xF6] - bias) (t 20 20 20 40 20 20 60 20 20 20 on the stack;
// the bias 0x9046CD for those three flags, 0x9046CE for the rest; unsigned
// compare) story flag n set and the dword 0. Then Records_CountUnpaired(1,
// 0x9046C0) below 7 at 0x9046CC, (0, 0x9046C4) below 10 at 0x9046CB, and
// Records_CountUnpairedOfKind(5), (0xB). Last, story flag 0xF5 set when clear
// with 0x9046CC at 4 or more. No PSX twin paired.
extern "C" void __cdecl Field_AfterBattleTally(void) {
    static const signed char kThresholds[10] = {0x14, 0x14, 0x14, 0x28, 0x14, 0x14, 0x3C, 0x14, 0x14, 0x14};
    if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(field_e2::at::kTallyBits)), 4) != 0 && SH_CALL(Flags_Test)(move_script::At(::bof3::Img(field_e2::at::kTallyBits) + 3), 4) == 0) {
        const unsigned char v = AreaW1c_Byte9045FB;
        if (v < 0x1E) AreaW1c_Byte9045FB = static_cast<unsigned char>(v + 1);
    }
    if (Cond_ByteFA >= 8 && FieldE2_LevelA != 0) {
        U flag = 0xF6;
        bool found = false;
        for (; flag <= 0xFF; ++flag)
            if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), flag) == 0) {
                found = true;
                break;
            }
        if (found) {
            unsigned char a = 0, b = 0;
            for (U p = ::bof3::Img(Community_Records_addr) + 1u; p < ::bof3::Img(field_e2::at::kPairsEnd) + 1u; p += 8) {
                if (BFieldE2(p - 1) == 0) continue;
                if (BFieldE2(p) == 0xA) ++a;
                else if (BFieldE2(p) == 0xB) ++b;
            }
            const bool first = flag == 0xF6 || flag == 0xF9 || flag == 0xFC;
            if (first) b = a;
            if (b != 0) {
                Rest4a_ClockA = Rest4a_ClockA + 1u;
                int need = kThresholds[flag - 0xF6] - static_cast<int>(BFieldE2(first ? ::bof3::Img(FieldE2_BiasA_addr) : ::bof3::Img(FieldE2_BiasB_addr)));
                if (need < 5) need = 5;
                if (Rest4a_ClockA >= static_cast<U>(need)) {
                    SH_CALL(Flags_Set)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), flag);
                    Rest4a_ClockA = 0;
                }
            }
        }
        if (Rest4a_LevelLit < 7) SH_CALL(Records_CountUnpaired)(1, reinterpret_cast<U*>(move_script::At(::bof3::Img(Rest4a_StampLevelLit_addr))));
        if (FieldE2_LevelA < 0xA) SH_CALL(Records_CountUnpaired)(0, reinterpret_cast<U*>(move_script::At(::bof3::Img(Rest4a_StampLevelDark_addr))));
        SH_CALL(Records_CountUnpairedOfKind)(5);
        SH_CALL(Records_CountUnpairedOfKind)(0xB);
    }
    if (SH_CALL(Flags_Test)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xF5) == 0 && Rest4a_LevelLit >= 4)
        SH_CALL(Flags_Set)(move_script::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0xF5);
}

// original 0x5343C0: with no pair of the sixty at 0x9046D0 (+0 in use, +1 the
// record, 1-based) naming any of the eight records at 0x9048B0 whose kind +0
// is 4 and key +1 is `key` (a byte), *counter + 1. One count over all the
// matching records: a single pair naming any of them spares the counter.
extern "C" void __cdecl Records_CountUnpaired(unsigned key, U* counter) {
    unsigned char named = 0;
    for (U r = 0; r < 8; ++r) {
        if (BFieldE2(::bof3::Img(field_e2::at::kRecords) + 8u * r) != 4) continue;
        if (BFieldE2(::bof3::Img(field_e2::at::kRecords) + 1u + 8u * r) != static_cast<unsigned char>(key)) continue;
        for (U p = ::bof3::Img(Community_Records_addr) + 1u; p < ::bof3::Img(field_e2::at::kPairsEnd) + 1u; p += 8)
            if (BFieldE2(p - 1) != 0 && BFieldE2(p) == r + 1u) ++named;
    }
    if (named == 0) ++*counter;
}

// original 0x534420: for each of the eight records at 0x9048B0 whose kind +0
// is `kind` (a byte) and whose +3 is not 0, its tally dword +4 + 1 unless a
// pair of the sixty at 0x9046D0 names it.
extern "C" void __cdecl Records_CountUnpairedOfKind(unsigned kind) {
    for (U r = 0; r < 8; ++r) {
        unsigned char named = 0;
        if (BFieldE2(::bof3::Img(field_e2::at::kRecords) + 8u * r) != static_cast<unsigned char>(kind)) continue;
        if (BFieldE2(::bof3::Img(field_e2::at::kRecords) + 3u + 8u * r) == 0) continue;
        for (U p = ::bof3::Img(Community_Records_addr) + 1u; p < ::bof3::Img(field_e2::at::kPairsEnd) + 1u; p += 8)
            if (BFieldE2(p - 1) != 0 && BFieldE2(p) == r + 1u) ++named;
        if (named == 0) SetL(::bof3::Img(field_e2::at::kRecords) + 4u + 8u * r, LFieldE2(::bof3::Img(field_e2::at::kRecords) + 4u + 8u * r) + 1u);
    }
}

// original 0x534480 (PSX 0x801C32D0): Field_LeaderStates[14] and the members' 0x65F960[14]:
// unless Field_Request is 9, state +1 = 1. Else the facing +8 toward the
// mode-11 object (0x905DA0's +0x34 / +0x38) by the signs of dx, dz: (-,-) 0,
// (+,+) 4, (-,+) 6, (-,0) 7, (0,+) 5, (0,-) 1, (+,-) 2, (+,0) 3, (0,0) kept;
// then Sprite_EnsureAnimation(+8). PSX twin 0x801C32D0 (table-anchored, a
// hypothesis).
extern "C" void __cdecl Field_FaceMode11Object(void) {
    if (Field_Request != 9) {
        Sprite_Current[1] = 1;
        return;
    }
    unsigned char* const s = Sprite_Current;
    const auto dx = static_cast<std::int32_t>(LFieldE2(::bof3::Img(field_e2::at::kObject) + 0x34) - LongAt(s + 0x34));
    const auto dz = static_cast<std::int32_t>(LFieldE2(::bof3::Img(field_e2::at::kObject) + 0x38) - LongAt(s + 0x38));
    int facing = -1;
    if (dx < 0) facing = dz < 0 ? 0 : dz > 0 ? 6 : 7;
    else if (dx > 0) facing = dz > 0 ? 4 : dz < 0 ? 2 : 3;
    else if (dz > 0) facing = 5;
    else if (dz < 0) facing = 1;
    if (facing >= 0) s[8] = static_cast<unsigned char>(facing);
    Ensure(Sprite_Current[8]);
}

// original 0x534590 (PSX 0x801C3430): bit 15 - the PlayStation's semi-
// transparency bit - on words 0..30 of the object's CLUT in row 15 of
// Gfx_ClutStrip, +0 bit 0x20, the strip dirty, word 31 zero. As the
// original: SC is held for the loop and the flag, and +5 is read again for
// each word; the last store reads SC and +5 afresh.
extern "C" void __cdecl Sprite_ShadeFadeBegin(void) {
    unsigned char* const s = Sprite_Current;
    for (unsigned i = 0; i < 31; ++i) AtEventObjs(::bof3::Img(Gfx_ClutStrip_addr) + 0x1e00 + (s[5] * 32u + i) * 2 + 1)[0] |= 0x80;
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    const unsigned clut = Sprite_Current[5];
    Gfx_ClutStripDirty = 1;
    SetWordEventObjs(AtEventObjs(::bof3::Img(Gfx_ClutStrip_addr) + 0x1e00 + 0x3E + clut * 64), 0);
}

// original 0x5345E0 (PSX 0x801C34C8): the set-up, then - for object 0 (+5),
// with bit 3 of neither flag word's low byte - the camera's target. The
// original reaches Field_JumpCamera by a tail jmp.
extern "C" void __cdecl Field_JumpStart(void) {
    event_objs::g.jump_setup();
    if (Sprite_Current[5] != 0) return;
    const auto flags = static_cast<unsigned char>(static_cast<unsigned char>(Field_ScriptFlags2) |
                                                  static_cast<unsigned char>(Field_ScriptFlags));
    if (flags & 8) return;
    event_objs::g.jump_camera();
}

// original 0x534610 (PSX 0x801C3530): a jump's frames (+9), its step per
// frame (+0xC, +0x10) and its rise per frame (+0x14) towards the ground at the
// landing point. Kept: both divisions are idiv and fault on 0 - a speed of 0
// (Field_MoveSpeeds' index a whole byte into a table of six, zeros after) or
// frames that the speed has brought to 0 (a speed above 0x20). The ground is
// 0x5725C0's answer unless that leaves SharedScratch's byte non-zero - a
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
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_x, Mul(LongEventObjs(AtEventObjs(::bof3::Img(MoveCmd_F9Steps_addr) + Sprite_Current[8] * 8u)), static_cast<std::int32_t>(speed)));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->step_z, Mul(LongEventObjs(AtEventObjs(::bof3::Img(MoveCmd_F9Steps_addr) + Sprite_Current[8] * 8u + 4)), static_cast<std::int32_t>(speed)));
    const unsigned char* const s = Sprite_Current;
    const long x = LandingX(s), z = LandingZ(s);
    long ground = event_objs::g.slope_at(x, z, s[8]);
    if (ScratchByte() != 0) ground = event_objs::g.ground_at(x, z);
    unsigned char* const t = Sprite_Current;
    rv::As<records::SpriteObject>(t)->step_y = Idiv(static_cast<short>(ground) - static_cast<short>(rv::As<records::SpriteHead>(t)->height_whole), t[9]);
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
    MoveScript_FAWord = WordEventObjs(s + 0x14);
}

// original 0x534790 (PSX 0x801C37D8): one step of the fade; at its end the
// palette comes back from row 15 of Gfx_ClutStripSource and the shade bytes
// and the semi-transparency go. al only (callers test al).
extern "C" unsigned char __cdecl Sprite_ShadeFadeStep(unsigned step) {
    if (!event_objs::g.shade_raise(step)) return 0;
    event_objs::g.load_palette(reinterpret_cast<unsigned short*>(AtEventObjs(::bof3::Img(Gfx_ClutStripSource_addr) + 0x1e00 + Sprite_Current[5] * 64u)), 0);
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

// original 0x534880 (PSX 0x801C3998): Sprite_ShadeRaise's mirror - each of
// Sprite_Current's shade bytes +0x5D..+0x5F that is not 0x80 less the step
// (both signed bytes, a 16-bit difference), 0x80 when that is below -128, else
// its low byte (so a difference above 127 wraps, and 0x80 can come of it).
// Answers whether all three are 0x80.
extern "C" unsigned char __cdecl Sprite_ShadeLower(unsigned step) {
    const auto s = static_cast<std::int16_t>(static_cast<signed char>(step));
    for (unsigned at = 0x5D; at <= 0x5F; ++at) {
        const unsigned char c = Sprite_Current[at];
        if (c == 0x80) continue;
        const auto v = static_cast<std::int16_t>(static_cast<std::int16_t>(static_cast<signed char>(c)) - s);
        Sprite_Current[at] = v < -128 ? 0x80 : static_cast<unsigned char>(v);
    }
    const unsigned char* const sc = Sprite_Current;
    return sc[0x5D] == 0x80 && sc[0x5E] == 0x80 && sc[0x5F] == 0x80;
}

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
        if (event_objs::g.cells_all(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z, static_cast<std::uint32_t>(LongEventObjs(s + 0x70)) & 0xFF, 0x80 + n, 0))
            break;
    }
    event_objs::g.floor_hurt(n);
    event_objs::g.flash(kKind[n]);
}

// original 0x534C20 (PSX 0x801C3E2C): the floor's damage of kind k (0..8; Field_FloorDamage's
// cell code - 0x80). A 27-byte table on the stack, three bytes a kind:
// Char_LoseHp(t0, Field_State +0x89) - HP by 4, 8, 16, 32, 2 for kinds 0..4 -
// and 0x537500(t1, +0x89) - AP by 4, 8, 32 for kinds 6..8 -, then the flags
// t2: bit 7 Field_State +0x124 = 10, bit 5 +0x125 = 40 (kinds 4 and 5 carry
// 0x80 and 0x20). The actor Field_State +0x148 names: with HP (+0x18) at most
// a quarter of +0x20, its state word +0x10 and Field_State +0x90 |= 0x2000;
// its state word |= t2; Actor_EquipCount(actor, 3, 8) not 0: its state word
// &= ~0x20 and Field_State +0x125 = 0. Both HP calls push stale upper bytes
// (Char_LoseHp and 0x537500 read a word and a byte). A kind above 8 reads
// past the stack table: ours aborts. PSX twin 0x801C3E2C (gap20, a
// hypothesis).
extern "C" void __cdecl Field_FloorHurt(unsigned kind_arg) {
    static const unsigned char kTable[27] = {4, 0, 0,  8, 0, 0,  0x10, 0, 0,  0x20, 0, 0,  2, 0, 0x80,
                                             0, 0, 0x20,  0, 4, 0,  0, 8, 0,  0, 0x20, 0};
    const U kind = kind_arg & 0xFF;
    if (kind > 8) bof3::Fatal("Field_FloorHurt: kind %u past the nine the stack table holds", (unsigned)kind);
    const unsigned char* const t = kTable + 3u * kind;
    SH_CALL(Char_LoseHp)(t[0], rv::As<records::PartyMember>(Field_State)->char_id);
    SH_AT(void (__cdecl*)(unsigned, unsigned), field_e2::at::kHpLoss2.v)(t[1], rv::As<records::PartyMember>(Field_State)->char_id);
    const unsigned char flags = t[2];
    if (flags & 0x80) Field_State[0x124] = 0xA;
    if (flags & 0x20) Field_State[0x125] = 0x28;
    records::PartyMember* fs = rv::As<records::PartyMember>(Field_State);
    U actor = ::bof3::Img(CharacterRecords_addr) + fs->char_index * field_e2::at::kRecordStride;
    if (rv::As<records::CharRecord>(move_script::At(actor))->hp <= (rv::As<records::CharRecord>(move_script::At(actor))->stats.max_hp >> 2)) {
        rv::As<records::CharRecord>(move_script::At(actor))->status = rv::As<records::CharRecord>(move_script::At(actor))->status | 0x2000u;
        fs->status = fs->status | 0x2000u;
        fs = rv::As<records::PartyMember>(Field_State);
    }
    const U id = fs->char_index;
    actor = ::bof3::Img(CharacterRecords_addr) + id * field_e2::at::kRecordStride;
    SetW(actor + 0x10, WFieldE2(actor + 0x10) | flags);
    // the member byte as the original pushes it: edx of `lea edx, [id + id * 4]` with dl the byte
    if (SH_CALL(Actor_EquipCount)(((id * 5u) & 0xFFFFFF00u) | id, 3, 8) == 0) return;
    unsigned char* const f = Field_State;
    const U a = ::bof3::Img(CharacterRecords_addr) + f[0x148] * field_e2::at::kRecordStride;
    rv::As<records::CharRecord>(move_script::At(a))->status = rv::As<records::CharRecord>(move_script::At(a))->status & 0xFFDFu;
    f[0x125] = 0;
}

// original 0x534DB0 (PSX 0x801C4084): Sprite_Current's CLUT row (+0x27) one colour: words 1..31
// of Gfx_ClutStrip row +0x27 = colour c of {0x0010, 0x4010, 0x0210, 0x4000}
// (c the argument's low byte; the original keeps the four on its stack and
// reads its own frame for c of 4 or more - ours aborts), Gfx_ClutStripDirty
// 1, Sound_PlayEffect(0x108), then Field_State +0x138 |= 8 (read after the
// call). PSX twin 0x801C4084 (pairs_propagated "call-anchored").
extern "C" void __cdecl Sprite_FlashClut(unsigned colour_arg) {
    static const std::uint16_t kColours[4] = {0x0010, 0x4010, 0x0210, 0x4000};
    const unsigned c = colour_arg & 0xFF;
    if (c >= 4)
        bof3::Fatal("Sprite_FlashClut: colour %u past the four (the original reads its own stack frame)", c);
    const unsigned row = Sprite_Current[0x27];
    for (unsigned i = 1; i < 0x20; ++i) Gfx_ClutStrip[row * 0x20 + i] = kColours[c];
    Gfx_ClutStripDirty = 1;
    SH_CALL(Sound_PlayEffect)(0x108);
    Field_State[0x138] = static_cast<unsigned char>(Field_State[0x138] | 8);
}

// original 0x534E50 (PSX 0x801C414C). If the field state says one is wanted: puts the current
// sprite object's CLUT back as it was loaded - entries 1 to 31, entry 0 is
// left - marks the strip for upload, and clears the request.
extern "C" void __cdecl Sprite_RestoreClut(void) {
    unsigned char* const state = Field_State;
    if (!(state[kPending] & 8)) return;
    for (unsigned n = 1; n < 0x20; ++n) {
        const unsigned at = Sprite_Current[kClut] * 32u + n;
        Gfx_ClutStrip[at] = Gfx_ClutStripSource[at];
    }
    Gfx_ClutStripDirty = 1;
    state[kPending] &= 0xF7;
}

// original 0x534EC0 (PSX 0x801C41FC): two of Field_State's timers from its
// actor record's bits 0x80 and 0x20. As the original has it: the record is
// indexed by Field_State +0x148 directly, a whole byte; Field_State is read
// again only after the first store.
extern "C" void __cdecl Field_MemberTimers(void) {
    records::PartyMember* state = rv::As<records::PartyMember>(Field_State);
    if (Field_ActorStates[state->char_index * kActorBytes] & 0x80) {
        rv::Bytes(state)[0x124] = 10;
        state = rv::As<records::PartyMember>(Field_State);
    }
    if (Field_ActorStates[state->char_index * kActorBytes] & 0x20) rv::Bytes(state)[0x125] = 0x28;
}

// original 0x534F10 (PSX 0x801C4294): while a member's actor state has bit
// 0x80 - every listed member with Field_InputFlags bit 0, else the leader -
// Field_State +0x124 counts down; at 0 it is 10 again, the flash, 1 HP from
// each such member (or from Field_State +0x89's), and an effect object of
// kind 0x41 at the leader if any HP was taken. Field_State is held from after
// Party_Count until the countdown, as the original's esi; read afresh after.
extern "C" void __cdecl Field_Bit80Tick(void) {
    const auto count = static_cast<unsigned char>(event_objs::g.party_count(0));
    unsigned char* const f = Field_State;
    unsigned char hit = 0;
    if (Field_InputFlags & 1) {
        for (unsigned i = 0; i < count; ++i)
            if (ActorState(Character_IndexById[Cond_Flags[0xd2 + i]]) & 0x80) hit = 1;
        if (!hit) return;
    } else if (!(ActorState(f[0x148]) & 0x80)) {
        return;
    }
    f[0x124] = static_cast<unsigned char>(f[0x124] - 1);
    hit = 0;
    if (Field_State[0x124] != 0) return;
    Field_State[0x124] = 10;
    event_objs::g.flash(1);
    if (Field_InputFlags & 1) {
        for (unsigned i = 0; i < count; ++i) {
            const unsigned char member = Cond_Flags[0xd2 + i];
            if ((ActorState(Character_IndexById[member]) & 0x80) && (event_objs::g.hp_lose(1, member) & 0xFFFF) != 0) hit = 1;
        }
        if (!hit) return;
    } else if ((event_objs::g.hp_lose(1, Field_State[0x89]) & 0xFFFF) == 0) {
        return;
    }
    const unsigned char e = event_objs::g.effect_free();
    if (e == 0xFF) return;
    const unsigned char* const s = Sprite_Current;
    unsigned char* const o = rv::Bytes(&Effect_Objects[e]);
    o[0] = 1;
    o[5] = 0x41;
    o[8] = s[8];
    rv::Store<unsigned>(rv::As<records::SpriteHead>(o)->x_whole, static_cast<std::uint16_t>(rv::As<records::SpriteObject>(s)->screen_x));
    rv::Store<unsigned>(rv::As<records::SpriteHead>(o)->z_whole, static_cast<std::uint16_t>(rv::As<records::SpriteObject>(s)->screen_y));
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
    SetWordEventObjs(state, WordEventObjs(state) & 0xFFDF);
}

// original 0x535120 (PSX 0x801C4788).
extern "C" unsigned char __cdecl Field_Tile89(unsigned turn) { return TileTurn(0x89, 2, turn); }

// original 0x535150 (PSX 0x801C4644): on a cell of `code`, state 2 / 8 with
// +0xB = value; with `turn`, the object first faces the first of the eight
// directions - from the opposite of its own, round - that 0x535610 does not
// refuse, or the eighth tried if it refuses all. The code goes on as the
// whole dword; value and turn are read as bytes.
extern "C" unsigned char __cdecl Field_TileTurn(unsigned code, unsigned value, unsigned turn) {
    {
        const unsigned char* const s = Sprite_Current;
        if (event_objs::g.cells_none(rv::As<records::SpriteObject>(s)->x, rv::As<records::SpriteObject>(s)->z, static_cast<std::uint32_t>(LongEventObjs(s + 0x70)) & 0xFF, code, 0))
            return 0;
    }
    if (static_cast<unsigned char>(turn) != 0) {
        unsigned char direction;
        for (unsigned i = 0;;) {
            const unsigned char* const s = Sprite_Current;
            direction = static_cast<unsigned char>(((s[8] ^ 0xFC) + i) & 7);
            const long x = Add(rv::As<records::SpriteObject>(s)->x, Mul(Field_DirectionSteps[direction * 2], 2));
            const long z = Add(rv::As<records::SpriteObject>(s)->z, Mul(Field_DirectionSteps[direction * 2 + 1], 2));
            if (!event_objs::g.turn_probe(x, z, 0, static_cast<std::uint16_t>(rv::As<records::SpriteHead>(s)->height_whole))) break;
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

// original 0x535240 (PSX 0x801C47D4).
extern "C" unsigned char __cdecl Field_Tile8A(unsigned turn) { return TileTurn(0x8A, 1, turn); }

// original 0x535270 (PSX 0x801C4820): one HP (0x5373F0) for each of three
// equipment tests the leader passes. Field_State read afresh for each use.
extern "C" void __cdecl Field_EquipTick(void) {
    if (event_objs::g.equip_count(Field_State[0x148], 3, 0x16)) event_objs::g.hp_gain(1, Field_State[0x89]);
    if (event_objs::g.equip_count(Field_State[0x148], 3, 0x17)) event_objs::g.hp_gain(1, Field_State[0x89]);
    if (event_objs::g.equip_count(Field_State[0x148], 2, 0x1F)) event_objs::g.hp_gain(1, Field_State[0x89]);
}

// original 0x535310 (PSX 0x801C48EC): how many of a record's equipment bytes
// of `kind` are `value`. Nothing bounds the member (a whole byte into
// Character_IndexById's 24) or the record it names.
extern "C" unsigned char __cdecl Actor_EquipCount(unsigned member, unsigned kind, unsigned value) {
    const unsigned char* const r = AtEventObjs(::bof3::Img(CharacterRecords_addr) + Character_IndexById[member & 0xFF] * kRecordSize);
    const auto v = static_cast<unsigned char>(value);
    switch (kind & 0xFF) {
    case 1: return r[0x12] == v;
    case 2: return static_cast<unsigned char>((r[0x13] == v) + (r[0x14] == v) + (r[0x15] == v));
    case 3: return static_cast<unsigned char>((r[0x16] == v) + (r[0x17] == v));
    default: return 0;
    }
}

// original 0x535390 (PSX 0x801C49D0): the 2 x 2 footprint when `wide` - the
// whole dword - is 0, else 0x535490's. The callee's eax is the answer.
extern "C" unsigned char __cdecl AreaMap_CellsAll(long x, long z, unsigned wide, unsigned code, unsigned mask) {
    return wide == 0 ? event_objs::g.cells_all4(x, z, code, mask) : event_objs::g.cells_all_wide(x, z, code, mask);
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

// original 0x535490: AreaMap_CellsAll's other footprint (its `wide` not 0):
// al 1 when every cell under the 16.16 point equals `code` (a byte) - the
// four at (x, z) .. (x + 1, z + 1), and with no x fraction the two at x - 1,
// with no z fraction the two at z - 1, with neither (x - 1, z - 1) - each and
// 0xF0 when `mask` is set; else 0. PSX twin 0x801C4B70 by the body (psx_pair
// has it and AreaMap_CellsAll4 swapped; a hypothesis).
extern "C" unsigned char __cdecl AreaMap_CellsAllWide(long x, long z, unsigned code, unsigned mask) {
    const U xi = static_cast<U>(x) >> 16, zi = static_cast<U>(z) >> 16;
    const auto c = static_cast<unsigned char>(code);
    unsigned char b[4] = {Cell(xi, zi), Cell(xi + 1, zi), Cell(xi, zi + 1), Cell(xi + 1, zi + 1)};
    for (unsigned char& v : b) v = Masked(v, mask);
    if (b[0] != c || b[1] != c || b[2] != c || b[3] != c) return 0;
    if ((x & 0xFFFF) == 0) {
        const unsigned char p = Masked(Cell(xi - 1, zi), mask), q = Masked(Cell(xi - 1, zi + 1), mask);
        if (p != c || q != c) return 0;
    }
    if ((z & 0xFFFF) == 0) {
        const unsigned char p = Masked(Cell(xi, zi - 1), mask), q = Masked(Cell(xi + 1, zi - 1), mask);
        if (p != c || q != c) return 0;
    }
    if ((x & 0xFFFF) == 0 && (z & 0xFFFF) == 0 && Masked(Cell(xi - 1, zi - 1), mask) != c) return 0;
    return 1;
}

// original 0x535610 (PSX 0x801C4E38): whether the way to (x, z) is blocked for a sprite of
// ground height `ground` - 0x535830 (a raised sprite's) when the byte
// `raised` is set, else Field_WayBlocked4. Their al.
extern "C" unsigned char __cdecl Field_WayBlocked(long x, long z, unsigned raised, long ground) {
    if (raised & 0xFF) return event_leader::g.way_blocked_wide(x, z, ground);
    return event_leader::g.way_blocked4(x, z, ground);
}

// original 0x535640: the 2 x 2 footprint's - blocking cells (Field_CellsBlock);
// a slope above 0x40 (MapView_SlopeAt's word, with its "slope" byte 0x903850)
// to the right when x has a fraction (direction 3), below when z has one (5),
// diagonally when both (4); the ground more than 0xC0 from `ground` (words);
// or an object there (Sprite_ObjectAt, with Sprite_Current's height +0x3E
// set to `ground` for the call and put back into Sprite_Current read again).
extern "C" unsigned char __cdecl Field_WayBlocked4(long x, long z, long ground) {
    if (event_leader::g.cells_block(x, z, 0)) return 1;
    const bool fx = (x & 0xFFFF) != 0, fz = (z & 0xFFFF) != 0;
    if (fx && static_cast<short>(event_leader::g.slope_at(x, z, 3)) > 0x40 && event_leader::at::kScratch != 0) return 1;
    if (fz && static_cast<short>(event_leader::g.slope_at(x, z, 5)) > 0x40 && event_leader::at::kScratch != 0) return 1;
    if (fx && fz && static_cast<short>(event_leader::g.slope_at(x, z, 4)) > 0x40 && event_leader::at::kScratch != 0) return 1;
    const long here = event_leader::g.ground_at(x, z);
    const int rise = static_cast<short>(here) - static_cast<short>(ground);
    if (std::abs(rise) > 0xC0) return 1;
    unsigned char* const sc = Sprite_Current;
    const unsigned short height = static_cast<std::uint16_t>(rv::As<records::SpriteHead>(sc)->height_whole);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(sc)->height_whole, static_cast<unsigned>(ground));
    const unsigned char object = event_leader::g.object_at(x, z, 0);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, height);
    return object != 0xFF;
}

// original 0x535730: whether the footprint at (x, z) (`wide` as
// AreaMap_CellsNone takes it) touches a blocking cell - any of the codes at
// 0x660C24 with the low nibble dropped; 0x10; 0x11 unless Field_State +0x138
// bit 0; 0x20 and 0x21 both; 0xC0 and 0xA0 (nibble dropped) both; 0xA2.
extern "C" unsigned char __cdecl Field_CellsBlock(long x, long z, unsigned wide) {
    for (const unsigned char* p = event_leader::At(::bof3::Img(Field_BlockingCells_addr)); *p != 0xFF; ++p)
        if (!event_leader::g.cells_none(x, z, wide, *p, 1)) return 1;
    if (!event_leader::g.cells_none(x, z, wide, 0x10, 0)) return 1;
    if (!(Field_State[0x138] & 1) && !event_leader::g.cells_none(x, z, wide, 0x11, 0)) return 1;
    if (!event_leader::g.cells_none(x, z, wide, 0x20, 0) && !event_leader::g.cells_none(x, z, wide, 0x21, 0)) return 1;
    if (!event_leader::g.cells_none(x, z, wide, 0xC0, 0) && !event_leader::g.cells_none(x, z, wide, 0xA0, 1)) return 1;
    return event_leader::g.cells_none(x, z, wide, 0xA2, 0) == 0 ? 1 : 0;
}

// original 0x535830: Field_WayBlocked for a raised sprite: al 1 when
// Field_CellsBlock(x, z, 1) answers; or when a slope probe MapView_SlopeAt
// answers above 0x40 (s16) with SharedScratch's byte 0x903850 set after it -
// the points and directions, with xi / zi the whole cells: (xi.8, zi.8) 4,
// (xi.8, zi.0) 3, (xi.0, zi.8) 5; with no x fraction (xi-1.8, zi.0) 7,
// (xi-1.8, zi+1.0) 7, (xi.8, zi+1.0) 7, (xi-1.0, zi.8) 1, (xi+1.0, zi.8) 1;
// with no z fraction (xi.0, zi-1.8) 1, (xi+1.0, zi-1.8) 1, (xi+1.0, zi.8) 1,
// (xi.8, zi-1.0) 3, (xi.8, zi+1.0) 3; with neither (xi-1.8, zi-1.8) 0; or
// when MapView_GroundAt(x, z) is more than 0xC0 from `ground` (s16s); else
// Sprite_ObjectAt(x, z, 1) with Sprite_Current's height +0x3E set to `ground`
// for the call (put back after, through Sprite_Current read again) - al 1
// unless it answers 0xFF. The probe points are built as the original builds
// them, word by word into two dwords. PSX twin 0x801C5038 (call, a
// hypothesis).
extern "C" unsigned char __cdecl Field_WayBlockedWide(long x, long z, unsigned ground) {
    if (SH_CALL(Field_CellsBlock)(x, z, 1) != 0) return 1;
    const U xi = static_cast<U>(x) >> 16, zi = static_cast<U>(z) >> 16;
    // The probe's two dwords: y (lo, hi) and x (lo, hi), words as the stack holds them.
    struct { std::uint16_t ylo, yhi, xlo, xhi; } q;
    auto probe = [&](unsigned direction) {
        const U px = q.xlo | static_cast<U>(q.xhi) << 16, py = q.ylo | static_cast<U>(q.yhi) << 16;
        const long slope = SH_CALL(MapView_SlopeAt)(static_cast<long>(px), static_cast<long>(py), direction);
        return S16FieldE2(static_cast<U>(slope)) > 0x40 && field_e2::at::kEffectSlot != 0;
    };
    q.yhi = static_cast<std::uint16_t>(zi);
    q.ylo = 0x8000;
    q.xhi = static_cast<std::uint16_t>(xi);
    q.xlo = 0x8000;
    if (probe(4)) return 1;
    q.ylo = 0;
    if (probe(3)) return 1;
    q.ylo = 0x8000;
    q.xlo = 0;
    if (probe(5)) return 1;
    if ((x & 0xFFFF) == 0) {
        q.ylo = 0;
        q.yhi = static_cast<std::uint16_t>(zi);
        q.xhi = static_cast<std::uint16_t>(xi - 1);
        q.xlo = 0x8000;
        if (probe(7)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi + 1);
        if (probe(7)) return 1;
        q.xhi = static_cast<std::uint16_t>(xi);
        if (probe(7)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi);
        q.ylo = 0x8000;
        q.xhi = static_cast<std::uint16_t>(xi - 1);
        q.xlo = 0;
        if (probe(1)) return 1;
        q.xhi = static_cast<std::uint16_t>(xi + 1);
        if (probe(1)) return 1;
    }
    if ((z & 0xFFFF) == 0) {
        q.ylo = 0x8000;
        q.yhi = static_cast<std::uint16_t>(zi - 1);
        q.xlo = 0;
        q.xhi = static_cast<std::uint16_t>(xi);
        if (probe(1)) return 1;
        q.xhi = static_cast<std::uint16_t>(xi + 1);
        if (probe(1)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi);
        if (probe(1)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi - 1);
        q.ylo = 0;
        q.xhi = static_cast<std::uint16_t>(xi);
        q.xlo = 0x8000;
        if (probe(3)) return 1;
        q.yhi = static_cast<std::uint16_t>(zi + 1);
        if (probe(3)) return 1;
    }
    if ((x & 0xFFFF) == 0 && (z & 0xFFFF) == 0) {
        q.yhi = static_cast<std::uint16_t>(zi - 1);
        q.ylo = 0x8000;
        q.xhi = static_cast<std::uint16_t>(xi - 1);
        q.xlo = 0x8000;
        if (probe(0)) return 1;
    }
    const long g = SH_CALL(MapView_GroundAt)(x, z);
    int d = S16FieldE2(static_cast<U>(g)) - S16FieldE2(ground);
    if (d < 0) d = -d;
    if (d > 0xC0) return 1;
    unsigned char* const s = Sprite_Current;
    const std::uint16_t kept = rv::As<records::SpriteHead>(s)->height_whole;
    rv::Store<unsigned>(rv::As<records::SpriteHead>(s)->height_whole, ground);
    const unsigned char hit = SH_CALL(Sprite_ObjectAt)(x, z, 1);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, kept);
    return hit != 0xFF ? 1 : 0;
}

// original 0x535C50 (PSX 0x801C5724): the same over the "none" tests.
extern "C" unsigned char __cdecl AreaMap_CellsNone(long x, long z, unsigned wide, unsigned code, unsigned mask) {
    return wide == 0 ? event_objs::g.cells_none4(x, z, code, mask) : event_objs::g.cells_none_wide(x, z, code, mask);
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

// original 0x535D60: AreaMap_CellsNone's other footprint: al 0 when any cell
// under the point equals `code` - the same cells as AreaMap_CellsAllWide,
// each 0x2n made 0x2n & 0x21 first (not the corner (x - 1, z - 1)), then and
// 0xF0 with `mask`; else 1. PSX twin 0x801C58F4 by the body (a hypothesis).
extern "C" unsigned char __cdecl AreaMap_CellsNoneWide(long x, long z, unsigned code, unsigned mask) {
    const U xi = static_cast<U>(x) >> 16, zi = static_cast<U>(z) >> 16;
    const auto c = static_cast<unsigned char>(code);
    unsigned char b[4] = {Cell(xi, zi), Cell(xi + 1, zi), Cell(xi, zi + 1), Cell(xi + 1, zi + 1)};
    for (unsigned char& v : b) v = Masked(Doors(v), mask);
    if (b[0] == c || b[1] == c || b[2] == c || b[3] == c) return 0;
    if ((x & 0xFFFF) == 0) {
        const unsigned char p = Masked(Doors(Cell(xi - 1, zi)), mask);
        const unsigned char q = Masked(Doors(Cell(xi - 1, zi + 1)), mask);
        if (p == c || q == c) return 0;
    }
    if ((z & 0xFFFF) == 0) {
        const unsigned char p = Masked(Doors(Cell(xi, zi - 1)), mask);
        const unsigned char q = Masked(Doors(Cell(xi + 1, zi - 1)), mask);
        if (p == c || q == c) return 0;
    }
    if ((x & 0xFFFF) == 0 && (z & 0xFFFF) == 0 && Masked(Cell(xi - 1, zi - 1), mask) == c) return 0;
    return 1;
}

// original 0x535F50 (PSX 0x801C5C40): if the ground at the landing point (on
// flat ground only - a slope leaves it) is 0x80 or more above the object, one
// speed slower and the jump set up again.
extern "C" void __cdecl Field_JumpCheckHeight(void) {
    const unsigned char* const s = Sprite_Current;
    const long x = LandingX(s), z = LandingZ(s);
    event_objs::g.slope_at(x, z, s[8]);
    if (ScratchByte() != 0) return;
    const long ground = event_objs::g.ground_at(x, z);
    if (static_cast<short>(ground) - static_cast<short>(rv::Current<records::SpriteHead>()->height_whole) < 0x80) return;
    Field_State[0x128] = static_cast<unsigned char>(Field_State[0x128] - 1);
    event_objs::g.jump_start();
}

// original 0x535FC0 (PSX 0x801C5D14): Sprite_EnsureAnimation(0x3C) facing 7, else (0x3D).
extern "C" void __cdecl Leader_Pose3C(void) { Ensure(rv::Current<records::PartyMember>()->direction == 7 ? 0x3C : 0x3D); }

// original 0x535FE0 (PSX 0x801C5D50): the hop's set-up (HopSetUp), +9 - 1.
extern "C" void __cdecl Leader_HopStart(void) {
    HopSetUp();
    rv::Current<records::PartyMember>()->step_frames = static_cast<unsigned char>(rv::Current<records::PartyMember>()->step_frames - 1);
}

// original 0x536050 (PSX 0x801C5E0C): the hop in flight. +9 at 0: Sprite_EnsureAnimation(+8)
// (the record read on entry), +0xA = 4, Sprite_ScriptTick, al 1. Else +9 - 1,
// Sprite_ApplyVelocity, and at +9 == +0xA (half way) the member slot's CLUT
// word 0x8113BE + +5 * 0x40 = 0 with Gfx_ClutStripDirty; Sprite_ScriptTick,
// al 0.
extern "C" unsigned char __cdecl Leader_HopFlight(void) {
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    if (s->step_frames == 0) {
        Ensure(s->direction);
        Sprite_Current[0xA] = 4;
        SH_CALL(Sprite_ScriptTick)();
        return 1;
    }
    s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
    SH_CALL(Sprite_ApplyVelocity)();
    unsigned char* const t = Sprite_Current;
    if (t[9] == t[0xA]) {
        Gfx_ClutStripDirty = 1;
        SetW(ClutWord(t), 0);
    }
    SH_CALL(Sprite_ScriptTick)();
    return 0;
}

// original 0x5360C0 (PSX 0x801C5EBC): +0xA - 1; at 0: Sprite_EnsureAnimation(0x37 facing 3,
// else 0x34), the facing +8 ^= 4, al 1. Else Sprite_EnsureAnimation(7 - +0xA
// facing 3, else +0xA + 1), +9 = 1, al 0.
extern "C" unsigned char __cdecl Leader_HopPose(void) {
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    const records::PartyMember* const s = rv::Current<records::PartyMember>();
    const unsigned char left = rv::Bytes(s)[0xA];
    if (left == 0) {
        Ensure(s->direction == 3 ? 0x37 : 0x34);
        rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>(rv::Current<records::PartyMember>()->direction ^ 4);
        return 1;
    }
    Ensure(s->direction == 3 ? static_cast<unsigned char>(7 - left) : static_cast<unsigned char>(left + 1));
    rv::Current<records::PartyMember>()->step_frames = 1;
    return 0;
}

// original 0x536130 (PSX 0x801C5F80): nothing (al 0) until Sprite_ScriptTickOnce answers; then
// Sprite_EnsureAnimation(0x3E facing 7, else 0x3F), al 1.
extern "C" unsigned char __cdecl Leader_Pose3E(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return 0;
    Ensure(rv::Current<records::PartyMember>()->direction == 7 ? 0x3E : 0x3F);
    return 1;
}

// original 0x536170 (PSX 0x801C5FD8): a step up. The point ahead (Field_DirectionSteps[+8] *
// (+0x70 + 2) from +0x34 / +0x38) and the ground there (MapView_GroundAt)
// less the actor's offset (the s16 0x66978C[Field_State +0x89], the actor as
// read on entry): with the height +0x3E (as read on entry, s16) above it, the
// height - the rise 0x6697A4[actor] into the cell read on entry, al 0; else
// the ground again (MapView_GroundAt a second time) less the offset of
// Field_State +0x89 read again into +0x3E of Sprite_Current read again,
// Sprite_EnsureAnimation(0x3C facing 7, else 0x3D), al 1. Then the view and
// the script tick (FollowAndTick).
extern "C" unsigned char __cdecl Leader_StepUp(void) {
    unsigned char* const s = Sprite_Current;
    std::int32_t x, z;
    Ahead(s, x, z);
    const std::uint16_t height = rv::As<records::PartyMember>(s)->height_whole;
    const U actor = rv::As<records::PartyMember>(Field_State)->char_id;
    unsigned char result = 0;
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    if (S16FieldE2(height) > S16FieldE2(static_cast<U>(ground)) - S16FieldE2(WFieldE2(::bof3::Img(field_e2::at::kPaceHeights) + actor * 2u))) {
        rv::Store<unsigned>(rv::As<records::PartyMember>(s)->height_whole, static_cast<unsigned>(height) - BFieldE2(::bof3::Img(field_e2::at::kPaceRises) + actor));
    } else {
        const long again = SH_CALL(MapView_GroundAt)(x, z);
        const std::uint16_t offset = WFieldE2(::bof3::Img(field_e2::at::kPaceHeights) + rv::As<records::PartyMember>(Field_State)->char_id * 2u);
        rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, static_cast<U>(again) - offset);
        Ensure(rv::Current<records::PartyMember>()->direction == 7 ? 0x3C : 0x3D);
        result = 1;
    }
    FollowAndTick(Sprite_Current);
    return result;
}

// original 0x536290 (PSX 0x801C61A8): the member slot's CLUT word 0x8113BE + +5 * 0x40 = 0 with
// Gfx_ClutStripDirty, then Sprite_EnsureAnimation(0x37 facing 7, else 0x34).
extern "C" void __cdecl Leader_Pose34(void) {
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    Gfx_ClutStripDirty = 1;
    SetW(ClutWord(rv::Bytes(s)), 0);
    Ensure(s->direction == 7 ? 0x37 : 0x34);
}

// original 0x5362D0 (PSX 0x801C6204): a step down. The point ahead as Leader_StepUp's and the
// ground there: with the height +0x3E (as read on entry) below it (s16), the
// height + the rise 0x6697A4[Field_State +0x89 read after the call] into the
// cell read on entry, al 0; else the ground again into +0x3E of
// Sprite_Current read again, Sprite_EnsureAnimation(0x3E facing 7, else
// 0x3F), al 1. Then FollowAndTick.
extern "C" unsigned char __cdecl Leader_StepDown(void) {
    unsigned char* const s = Sprite_Current;
    std::int32_t x, z;
    Ahead(s, x, z);
    const std::uint16_t height = rv::As<records::PartyMember>(s)->height_whole;
    unsigned char result = 0;
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    if (S16FieldE2(height) < S16FieldE2(static_cast<U>(ground))) {
        rv::Store<unsigned>(rv::As<records::PartyMember>(s)->height_whole, BFieldE2(::bof3::Img(field_e2::at::kPaceRises) + rv::As<records::PartyMember>(Field_State)->char_id) + static_cast<unsigned>(height));
    } else {
        const long again = SH_CALL(MapView_GroundAt)(x, z);
        rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, static_cast<U>(again));
        Ensure(rv::Current<records::PartyMember>()->direction == 7 ? 0x3E : 0x3F);
        result = 1;
    }
    FollowAndTick(Sprite_Current);
    return result;
}

// original 0x5363C0 (PSX 0x801C6394): nothing (al 0) until Sprite_ScriptTickOnce answers; then
// the hop's set-up, +9 - 1 and al 1.
extern "C" unsigned char __cdecl Leader_HopStartAfterTick(void) {
    if (SH_CALL(Sprite_ScriptTickOnce)() == 0) return 0;
    HopSetUp();
    rv::Current<records::PartyMember>()->step_frames = static_cast<unsigned char>(rv::Current<records::PartyMember>()->step_frames - 1);
    return 1;
}

// original 0x536440 (PSX 0x801C646C): the hop's fall. +9 (the record read on entry) at 0:
// Sprite_EnsureAnimation(+8), Sprite_ClearSteps, Field_State +0x137 = 0, the
// states +1 = 1, +2 = +3 = +4 = 0, al 1. Else +9 - 1, Sprite_ApplyVelocity,
// at +9 == +0xA the member's palette (Sprite_LoadPalette(0x80D380 + +5 *
// 0x40, 0)), Sprite_ScriptTick, al 0.
extern "C" unsigned char __cdecl Leader_HopFall(void) {
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    if (s->step_frames == 0) {
        Ensure(s->direction);
        SH_CALL(Sprite_ClearSteps)();
        Field_State[0x137] = 0;
        rv::Current<records::PartyMember>()->state = 1;
        rv::Current<records::PartyMember>()->step = 0;
        rv::Current<records::PartyMember>()->state3 = 0;
        rv::Current<records::PartyMember>()->state4 = 0;
        return 1;
    }
    s->step_frames = static_cast<unsigned char>(s->step_frames - 1);
    SH_CALL(Sprite_ApplyVelocity)();
    const unsigned char* const t = Sprite_Current;
    if (t[9] == t[0xA]) SH_CALL(Sprite_LoadPalette)(Palette(t), 0);
    SH_CALL(Sprite_ScriptTick)();
    return 0;
}

// original 0x5364D0 (PSX 0x801C6550): the height +0x3E up 0x10; the ground ahead (as
// Leader_StepUp's point) less the offset of Field_State +0x89: with the
// height at or above it (s16), the state +2 = 3. Then a tail jump to
// Sprite_ScriptTick, whose al it answers.
extern "C" unsigned char __cdecl Leader_Rise(void) {
    rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, rv::Current<records::PartyMember>()->height_whole + 0x10u);
    std::int32_t x, z;
    Ahead(Sprite_Current, x, z);
    const long ground = SH_CALL(MapView_GroundAt)(x, z);
    const int floor = S16FieldE2(static_cast<U>(ground)) - S16FieldE2(WFieldE2(::bof3::Img(field_e2::at::kPaceHeights) + rv::As<records::PartyMember>(Field_State)->char_id * 2u));
    records::PartyMember* const s = rv::Current<records::PartyMember>();
    if (S16FieldE2(s->height_whole) >= floor) s->step = 3;
    return SH_CALL(Sprite_ScriptTick)();
}

// original 0x536550 (PSX 0x801C6618): the height +0x3E down 0x10; with the ground under
// (+0x34, +0x38) above it (s16): +0x3E = the ground, Sprite_SetAnimation(+8),
// the member's palette, Field_State +0x137 = 0, the states +1 = 1, +2 = 0.
// Then a tail jump to Sprite_ScriptTick, whose al it answers.
extern "C" unsigned char __cdecl Leader_Sink(void) {
    rv::Store<unsigned>(rv::Current<records::PartyMember>()->height_whole, rv::Current<records::PartyMember>()->height_whole - 0x10u);
    unsigned char* s = Sprite_Current;
    const long ground = SH_CALL(MapView_GroundAt)(static_cast<long>(LongAt(s + 0x34)), static_cast<long>(LongAt(s + 0x38)));
    s = Sprite_Current;
    if (S16FieldE2(static_cast<U>(ground)) > S16FieldE2(rv::As<records::PartyMember>(s)->height_whole)) {
        rv::Store<unsigned>(rv::As<records::PartyMember>(s)->height_whole, static_cast<U>(ground));
        SH_CALL(Sprite_SetAnimation)(rv::Current<records::PartyMember>()->direction);
        SH_CALL(Sprite_LoadPalette)(Palette(Sprite_Current), 0);
        Field_State[0x137] = 0;
        rv::Current<records::PartyMember>()->state = 1;
        rv::Current<records::PartyMember>()->step = 0;
    }
    return SH_CALL(Sprite_ScriptTick)();
}

// original 0x5365D0 (PSX 0x801C66DC): the facing +8 ^= 4, Sprite_EnsureAnimation(0x3A facing
// 3, else 0x3B), the member's palette, Sprite_ClearSteps, +0x20 = -8, the
// states +1 = 2, +2 = 3, +3 = 3.
extern "C" void __cdecl Leader_TurnBack(void) {
    rv::Current<records::PartyMember>()->direction = static_cast<unsigned char>(rv::Current<records::PartyMember>()->direction ^ 4);
    Ensure(rv::Current<records::PartyMember>()->direction == 3 ? 0x3A : 0x3B);
    SH_CALL(Sprite_LoadPalette)(Palette(Sprite_Current), 0);
    SH_CALL(Sprite_ClearSteps)();
    rv::Store<U>(rv::Current<records::PartyMember>()->work_20, 0xFFFFFFF8u);
    rv::Current<records::PartyMember>()->state = 2;
    rv::Current<records::PartyMember>()->step = 3;
    rv::Current<records::PartyMember>()->state3 = 3;
}

// original 0x536650 (PSX 0x801C6790): Sprite_Current's dwords +0xC, +0x10, +0x14 cleared. The
// original returns with eax 0, which none of the 14 call sites reads (checked
// 2026-09-22); ours does not promise it.
extern "C" void __cdecl Sprite_ClearSteps(void) {
    rv::Current<records::SpriteObject>()->step_x = 0;
    rv::Current<records::SpriteObject>()->step_z = 0;
    rv::Current<records::SpriteObject>()->step_y = 0;
}

// original 0x536670 (PSX 0x801C67AC): one frame of the step - +0x34 / +0x38
// by +0xC / +0x10, the height word +0x3E by the low word of +0x14.
extern "C" void __cdecl Sprite_ApplyVelocity(void) {
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, Add(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->step_x));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, Add(rv::Current<records::SpriteObject>()->z, rv::Current<records::SpriteObject>()->step_z));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, rv::Current<records::SpriteHead>()->height_whole + WordEventObjs(Sprite_Current + 0x14));
}

void Field6_Inject() {
    BOF3_INJECT(Field_CellAroundLarge);
    BOF3_INJECT(Field_CellAroundSide);
    BOF3_INJECT(Field_LeaderTalkTo);
    BOF3_INJECT(Field_EncounterArea);
    BOF3_INJECT(Field_GatewayExit);
    BOF3_INJECT(Field_CellHasEvent);
    BOF3_INJECT(Field_LeaderCellEvent);
    BOF3_INJECT(Field_ExitFromCell);
    BOF3_INJECT(Party_UpdateScreens);
    BOF3_INJECT(Party_Count);
    BOF3_INJECT(Field_CopyInput);
    BOF3_INJECT(Sprite_PointInReach);
    BOF3_INJECT(Sprite_ObjectAt);
    BOF3_INJECT(Field_ProbeHeight);
    BOF3_INJECT(Field_LeaderPushObjects);
    BOF3_INJECT(Party_MemberAt);
    BOF3_INJECT(Party_DropIn);
    BOF3_INJECT(Party_SetUpMembers);
    BOF3_INJECT(ScriptContext_Reset);
    BOF3_INJECT(Party_SwapMembers);
    BOF3_INJECT(ObjTrio_SwapFields);
    BOF3_INJECT(Encounter_RollInitiative);
    BOF3_INJECT(Encounter_PartyTurnSense);
    BOF3_INJECT(Encounter_PartyTurn);
    BOF3_INJECT(Encounter_PartyToPlaces);
    BOF3_INJECT(Encounter_PartyAtPlaces);
    BOF3_INJECT(Encounter_PartyScriptOnce);
    BOF3_INJECT(Party_PlaceAtSlots);
    BOF3_INJECT(Party_ScriptTicks);
    BOF3_INJECT(Party_PlacesByList);
    BOF3_INJECT(Party_PlaceForBattle);
    BOF3_INJECT(Party_PlaceInFormation);
    BOF3_INJECT(Field_PartySetUp);
    BOF3_INJECT(Field_PartyFirstFrame);
    BOF3_INJECT(Field_PartyPosition);
    BOF3_INJECT(Field_LeaderPlaceOffset);
    BOF3_INJECT(Field_PendingJump);
    BOF3_INJECT(Field_PendingWalkIn);
    BOF3_INJECT(Field_PendingJumpTurn);
    BOF3_INJECT(Field_PendingJumpKind4);
    BOF3_INJECT(Field_PendingDrop);
    BOF3_INJECT(Field_PendingNext);
    BOF3_INJECT(Field_PendingRelease);
    BOF3_INJECT(Field_MemberSprite);
    BOF3_INJECT(Field_PartyLoad);
    BOF3_INJECT(Party_ReloadPalettes);
    BOF3_INJECT(Party_HealJoined);
    BOF3_INJECT(Party_Join);
    BOF3_INJECT(Party_JoinReset);
    BOF3_INJECT(Party_Remove);
    BOF3_INJECT(Scenario_CallA);
    BOF3_INJECT(Scenario_CallB);
    BOF3_INJECT(Field_AfterBattleTally);
    BOF3_INJECT(Records_CountUnpaired);
    BOF3_INJECT(Records_CountUnpairedOfKind);
    BOF3_INJECT(Field_FaceMode11Object);
    BOF3_INJECT(Sprite_ShadeFadeBegin);
    BOF3_INJECT(Field_JumpStart);
    BOF3_INJECT(Field_JumpSetUp);
    BOF3_INJECT(Field_JumpCamera);
    BOF3_INJECT(Sprite_ShadeFadeStep);
    BOF3_INJECT(Sprite_ShadeRaise);
    BOF3_INJECT(Sprite_ShadeLower);
    BOF3_INJECT(Field_TileD0);
    BOF3_INJECT(Field_TileA4);
    BOF3_INJECT(Field_FloorDamage);
    BOF3_INJECT(Field_FloorHurt);
    BOF3_INJECT(Sprite_FlashClut);
    BOF3_INJECT(Sprite_RestoreClut);
    BOF3_INJECT(Field_MemberTimers);
    BOF3_INJECT(Field_Bit80Tick);
    BOF3_INJECT(Field_Bit20Tick);
    BOF3_INJECT(Field_Tile89);
    BOF3_INJECT(Field_TileTurn);
    BOF3_INJECT(Field_Tile8A);
    BOF3_INJECT(Field_EquipTick);
    BOF3_INJECT(Actor_EquipCount);
    BOF3_INJECT(AreaMap_CellsAll);
    BOF3_INJECT(AreaMap_CellsAll4);
    BOF3_INJECT(AreaMap_CellsAllWide);
    BOF3_INJECT(Field_WayBlocked);
    BOF3_INJECT(Field_WayBlocked4);
    BOF3_INJECT(Field_CellsBlock);
    BOF3_INJECT(Field_WayBlockedWide);
    BOF3_INJECT(AreaMap_CellsNone);
    BOF3_INJECT(AreaMap_CellsNone4);
    BOF3_INJECT(AreaMap_CellsNoneWide);
    BOF3_INJECT(Field_JumpCheckHeight);
    BOF3_INJECT(Leader_Pose3C);
    BOF3_INJECT(Leader_HopStart);
    BOF3_INJECT(Leader_HopFlight);
    BOF3_INJECT(Leader_HopPose);
    BOF3_INJECT(Leader_Pose3E);
    BOF3_INJECT(Leader_StepUp);
    BOF3_INJECT(Leader_Pose34);
    BOF3_INJECT(Leader_StepDown);
    BOF3_INJECT(Leader_HopStartAfterTick);
    BOF3_INJECT(Leader_HopFall);
    BOF3_INJECT(Leader_Rise);
    BOF3_INJECT(Leader_Sink);
    BOF3_INJECT(Leader_TurnBack);
    BOF3_INJECT(Sprite_ClearSteps);
    BOF3_INJECT(Sprite_ApplyVelocity);
}
