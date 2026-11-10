// The movement script's group handlers: originals 0x578B00 (0x60), 0x5786C0
// (0x80), 0x578A00 (0x90), 0x578010 (0xC0), 0x577BD0 (0xD0), 0x577420 (0xE0)
// and the flow pass's 0x577280 (0xB0). docs/movement-script.md section 1.
#include "game/move_groups.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound_sp.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

// Where a generated data name lives (its name is a macro, so bof3::addr:: cannot be used with it).
std::uint32_t Address(const volatile void* p) { return Low32(p); }

const move_groups::CalleesMoveGroups kOriginalsMoveGroups = {
    MoveScript_ObjectKind, Sprite_FaceDirection, Field_ObjectTalk, MoveCmd_OpE7, MoveCmd_OpE9,
    Sprite_SetTint, Sprite_ReleaseTint, Sprite_SetAnimation, Sprite_SetAnimationBank, AreaMap_Elevation,
    MapView_SetElevation, Sprite_ScriptPeek, Sound_PlayById, Sound_PlayEffect, Music_Play, Music_FadeOutStop,
    Music_FadeIn, Music_FadeOut, Port_DroppedCall, MoveCmd_OpDB, MoveCmd_TestFB, MoveCmd_TestFC, Effect_Spawn,
    Effect_SpawnAt, MoveCmd_HandlePosition, MoveCmd_Op87, MoveCmd_Op88, Sound_LoadStream, Sound_StreamDone,
};
}  // namespace
namespace {

// Ops D4..D6: the party slot Field_ActiveMember is, as a byte - the original
// divides the pointer's distance from Sprite_ObjectsExtra by 0xA4, truncating.
unsigned char PartySlot() {
    const auto distance = static_cast<std::int32_t>(Low32(Field_ActiveMember.Native()) - Address(rv::Bytes(Sprite_ObjectsExtra)));
    return static_cast<unsigned char>(distance / 0xA4);
}
unsigned char* PartyRecord(int slot) { return MoveScript_PartyRecords + slot * 16; }

// Sprite_Current may be a Sprite_Objects record or another family whose
// first 0x80 bytes are a sprite object's (an ObjTrio member, the kind-2
// object), so only that part is named: rv::Current<records::SpriteHead>()
// (C2 audit frs F3; typed-data-round.md 3, "C2 audit as run").

}  // namespace

namespace {

// --- BOF3X_SHADOW=move_groups: a differential fuzz, once at start-up --------
// Seven byte-copies, one per handler, their jump tables moved into the copy
// and every call re-aimed at the recorders below; the area's handler table
// and +0x18 array are swapped for recorders and a table of the fuzz's. One
// round: a group, an op in (or just past) its range with random operands, and
// random state - the context, a sprite, the field state, a party object, the
// tint and party records, ObjTrio 0, Sprite_Kind2 and the globals the groups
// touch - with the boundaries each op tests seeded; theirs, then from the same
// state ours; all of it, the result and the recorders' log compared.

constexpr unsigned kBuf = 0x100, kScript = 0x10010, kLog = 128;
unsigned char g_object[kBuf], g_sprite[kBuf], g_field[0x200], g_script[kScript];
unsigned char g_descriptor[0x44];
unsigned char* g_scripts18[256];
long g_handle_buffer[4];
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {   // a pointer as the log keeps it
    const auto a = reinterpret_cast<std::uintptr_t>(p);
    if (p == g_sprite) return 1;
    if (p == g_object) return 2;
    if (a == Address(rv::Bytes(&Sprite_Kind2))) return 3;
    return static_cast<std::uint32_t>(a);
}
// A callee may move the position or turn the sprite: the handler must read them again.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) SetPos(g_object, Pos(g_object) + (h >> 8) % 5);
    if (h % 5 == 0) Sprite_Current[8] = static_cast<unsigned char>(h >> 16);
}
unsigned char Byte() { return static_cast<unsigned char>(Hash() >> 7); }

int __cdecl StubKind() { Record(1); return static_cast<int>(Hash() % 4); }
void __cdecl StubFace(unsigned char d) { Record(2, d); Disturb(); }
unsigned char __cdecl StubTurnTarget(unsigned char* o) { Record(3, Id(o)); Disturb(); return static_cast<unsigned char>(Hash()); }
void __cdecl StubOpE7(unsigned char n) { Record(4, n); }
unsigned char __cdecl StubOpE9(unsigned char* o, signed char a, signed char b, unsigned short c, unsigned short d,
                               unsigned char e, unsigned char f) {
    Record(5, Id(o), static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b) << 8 | e << 16 | static_cast<std::uint32_t>(f) << 24, c, d);
    Disturb();
    return Hash() % 2 ? Byte() | 1 : 0;
}
unsigned char __cdecl StubTint(unsigned char* s, unsigned char r, unsigned char gg, unsigned char b, unsigned char a) {
    Record(6, Id(s), r | gg << 8 | b << 16 | static_cast<std::uint32_t>(a) << 24);
    Disturb();
    return Byte();
}
void __cdecl StubTintRelease(unsigned char* s) { Record(7, Id(s)); }
void __cdecl StubSetAnimation(unsigned char a) { Record(8, a); Disturb(); }
unsigned char __cdecl StubAnimationBank(unsigned short b) { Record(9, b); Disturb(); return 0; }   // op CB ignores the result
long __cdecl StubElevation(long x, long z) { Record(10, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z)); return static_cast<long>(Hash()); }
void __cdecl StubSetElevation(int v) { Record(11, static_cast<std::uint32_t>(v)); }
unsigned short __cdecl StubPeek() {
    Record(12);
    const std::uint32_t h = Hash();
    const unsigned char operand = g_script[Pos(g_object) + 3u];
    switch (h % 4) {
    case 0: return Word(Sprite_Current + 0x58);
    case 1: return operand;
    case 2: SetWord(Sprite_Current + 0x58, operand); return operand;
    default: return static_cast<unsigned short>(h >> 8);
    }
}
void __cdecl StubPlayById(unsigned short id) { Record(13, id); }
void __cdecl StubSound(unsigned short id) { Record(14, id); }
void __cdecl StubMusicPlay(unsigned int t, int u) { Record(15, t & 0xFF, static_cast<std::uint32_t>(u)); }
void __cdecl StubFadeOutStop(int n) { Record(16, static_cast<std::uint32_t>(n)); }
void __cdecl StubFadeIn(int n) { Record(17, static_cast<std::uint32_t>(n) & 0xFF); }
void __cdecl StubFadeOut(int n) { Record(18, static_cast<std::uint32_t>(n) & 0xFF); }
void __cdecl StubOpB8(unsigned char b) { Record(19, b); }
void __cdecl StubOpDB() { Record(20); }
unsigned char __cdecl StubTestFB(short x, short z) { Record(21, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z)); return Byte(); }
unsigned char __cdecl StubTestFC(short x, short z) { Record(22, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z)); return Byte(); }
unsigned char __cdecl StubEffectSpawn(unsigned char k, signed char a, signed char b, short x, short z) {
    Record(23, k | static_cast<std::uint8_t>(a) << 8 | static_cast<std::uint8_t>(b) << 16, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z));
    Disturb();
    return Byte();
}
unsigned char __cdecl StubEffectSpawnAt(unsigned char k, signed char a, signed char b, long x, long y, long z) {
    Record(24, k | static_cast<std::uint8_t>(a) << 8 | static_cast<std::uint8_t>(b) << 16, static_cast<std::uint32_t>(x),
           static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(z));
    Disturb();
    return Byte();
}
// Fills the caller's buffer, or returns a buffer of its own: the handler must use what it returns.
long* __cdecl StubHandlePosition(long* out, unsigned char h, unsigned char b) {
    Record(25, h, b);
    long* const to = Hash() % 2 ? out : g_handle_buffer;
    for (unsigned i = 0; i < 4; ++i) to[i] = static_cast<long>(Hash() * (i + 1));
    return to;
}
void __cdecl StubOp87() { Record(26); }
void __cdecl StubOp88() { Record(27); }
void __cdecl StubLoadStream(unsigned id) { Record(28, id); }
int __cdecl StubStreamDone() { Record(29); return Hash() % 2 ? static_cast<int>(Hash() | 0x100u) : 0; }
template <unsigned I>
void __cdecl StubArea() {
    Record(40, I);
    Sprite_Current[8] = Byte();
}
template <std::size_t... I>
constexpr std::array<void*, sizeof...(I)> AreaStubs(std::index_sequence<I...>) {
    return {{reinterpret_cast<void*>(&StubArea<I>)...}};
}
const auto kAreaStubs = AreaStubs(std::make_index_sequence<256>());

const move_groups::CalleesMoveGroups kStubs = {
    StubKind, StubFace, StubTurnTarget, StubOpE7, StubOpE9, StubTint, StubTintRelease, StubSetAnimation,
    StubAnimationBank, StubElevation, StubSetElevation, StubPeek, StubPlayById, StubSound, StubMusicPlay,
    StubFadeOutStop, StubFadeIn, StubFadeOut, StubOpB8, StubOpDB, StubTestFB, StubTestFC, StubEffectSpawn,
    StubEffectSpawnAt, StubHandlePosition, StubOp87, StubOp88, StubLoadStream, StubStreamDone,
};

// Where each copy's calls went, by original address, and what they go to now.
const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x57C840: return f(&StubKind);
    case 0x57C4C0: return f(&StubFace);
    case 0x517E90: return f(&StubTurnTarget);
    case 0x518B20: return f(&StubOpE7);
    case 0x57C8E0: return f(&StubOpE9);
    case 0x454CC0: return f(&StubTint);
    case 0x454DC0: return f(&StubTintRelease);
    case 0x5891F0: return f(&StubSetAnimation);
    case 0x589590: return f(&StubAnimationBank);
    case 0x5720C0: return f(&StubElevation);
    case 0x5725F0: return f(&StubSetElevation);
    case 0x57CDC0: return f(&StubPeek);
    case 0x587900: return f(&StubPlayById);
    case 0x587740: return f(&StubSound);
    case 0x587AE0: return f(&StubMusicPlay);
    case 0x587B40: return f(&StubFadeOutStop);
    case 0x587BA0: return f(&StubFadeIn);
    case 0x587BE0: return f(&StubFadeOut);
    case 0x4DF820: return f(&StubOpB8);
    case 0x57CD40: return f(&StubOpDB);
    case 0x572650: return f(&StubTestFB);
    case 0x572790: return f(&StubTestFC);
    case 0x57CE10: return f(&StubEffectSpawn);
    case 0x57CE80: return f(&StubEffectSpawnAt);
    case 0x578DC0: return f(&StubHandlePosition);
    case 0x5795F0: return f(&StubOp87);
    case 0x5794D0: return f(&StubOp88);
    case 0x587910: return f(&StubLoadStream);
    case 0x587A00: return f(&StubStreamDone);
    default: bof3::Fatal("move_groups: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The seven copies: range, jump table, and every relative call out of the
// range as (E8 offset, original target) - by capstone, 2026-09-22.
struct Call { std::uint32_t offset, target; };
struct Group {
    const char* name;
    std::uint32_t base, size;
    Table table;
    const Call* calls;
    int n_calls;
    unsigned char first_op, last_op;
};
constexpr Call kCallsB[] = {{0x31, 0x587740}, {0x4a, 0x587740}, {0x64, 0x587740}, {0x7e, 0x587740}, {0x94, 0x587ae0}, {0xa4, 0x587b40},
                            {0xb8, 0x4df820}, {0xc9, 0x587b40}, {0xde, 0x587ba0}, {0xf3, 0x587be0}, {0x117, 0x587900}, {0x135, 0x587900},
                            {0x14c, 0x587ae0}};
constexpr Call kCallsE[] = {{0x42, 0x57c4c0}, {0x58, 0x517e90}, {0xa8, 0x57c4c0}, {0xd8, 0x517e90}, {0x164, 0x518b20}, {0x199, 0x57c8e0},
                            {0x1a9, 0x57c840}, {0x234, 0x57c840}, {0x2a3, 0x454cc0}};
constexpr Call kCallsD[] = {{0x4b, 0x5720c0}, {0x6a, 0x57cdc0}, {0xa2, 0x587900}, {0x350, 0x57cd40}, {0x362, 0x5720c0}, {0x383, 0x572650},
                            {0x3a2, 0x572790}};
constexpr Call kCallsC[] = {{0x32, 0x57c840}, {0x63, 0x454cc0}, {0xa8, 0x454cc0}, {0xc9, 0x57c840}, {0x1af, 0x57c840}, {0x4fd, 0x57c840},
                            {0x511, 0x57c4c0}, {0x52d, 0x5891f0}, {0x57c, 0x589590}, {0x58c, 0x5891f0}, {0x5dc, 0x454dc0}, {0x61d, 0x57c840},
                            {0x631, 0x57c4c0}, {0x642, 0x5891f0}};
constexpr Call kCalls8[] = {{0x8c, 0x578dc0}, {0x10c, 0x5720c0}, {0x112, 0x5725f0}, {0x150, 0x5720c0}, {0x16d, 0x5720c0}, {0x1cc, 0x57c840},
                            {0x235, 0x5795f0}, {0x246, 0x5794d0}, {0x260, 0x587910}, {0x28c, 0x587a00}, {0x2e4, 0x57ce80}};
constexpr Call kCalls9[] = {{0x5b, 0x57ce10}, {0x96, 0x57ce80}};
constexpr Call kCalls6[] = {{0xdf, 0x5891f0}};
#define BOF3_CALLS(a) a, static_cast<int>(sizeof a / sizeof a[0])
const Group kGroups[] = {
    {"MoveScript_GroupB", 0x577280, 0x198, {0x27, 0x15c, 15}, BOF3_CALLS(kCallsB), 0xB0, 0xBF},
    {"MoveScript_GroupE", 0x577420, 0x334, {0x2e, 0x2f4, 16}, BOF3_CALLS(kCallsE), 0xE0, 0xEF},
    {"MoveScript_GroupD", 0x577BD0, 0x438, {0x2f, 0x3f8, 16}, BOF3_CALLS(kCallsD), 0xD0, 0xDF},
    {"MoveScript_GroupC", 0x578010, 0x6a4, {0x2e, 0x668, 15}, BOF3_CALLS(kCallsC), 0xC0, 0xCF},
    {"MoveScript_Group8", 0x5786C0, 0x33c, {0x2f, 0x308, 13}, BOF3_CALLS(kCalls8), 0x80, 0x8F},
    {"MoveScript_Group9", 0x578A00, 0xf8, {0x3c, 0xd8, 4}, BOF3_CALLS(kCalls9), 0x90, 0x9F},
    {"MoveScript_Group6", 0x578B00, 0x110, {0x25, 0xf4, 7}, BOF3_CALLS(kCalls6), 0x60, 0x6F},
};
#undef BOF3_CALLS
constexpr unsigned kGroupCount = sizeof kGroups / sizeof kGroups[0];

using GroupFn = unsigned char (__cdecl*)(unsigned char*, const unsigned char*, unsigned char);
using GroupBFn = void (__cdecl*)(unsigned char*, const unsigned char*);
const void* const kOurs[kGroupCount] = {
    reinterpret_cast<const void*>(&MoveScript_GroupB), reinterpret_cast<const void*>(&MoveScript_GroupE),
    reinterpret_cast<const void*>(&MoveScript_GroupD), reinterpret_cast<const void*>(&MoveScript_GroupC),
    reinterpret_cast<const void*>(&MoveScript_Group8), reinterpret_cast<const void*>(&MoveScript_Group9),
    reinterpret_cast<const void*>(&MoveScript_Group6),
};

// Game memory both sides may touch, saved before the fuzz and put back after;
// randomised every round and compared after both passes.
struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(rv::Bytes(&Sprite_Kind2)), sizeof(records::SpriteObject)},
    {Address(rv::Bytes(ObjTrio)), 0x40},
    {Address(rv::Bytes(Sprite_ObjectsExtra)), 8 * 0xA4}, // the party objects and what a slot up to 7 reaches
    {Address(MoveScript_TintRecords), MoveScript_TintRecords_count},
    {Address(MoveScript_PartyRecords), MoveScript_PartyRecords_count},
    {::bof3::Img(SharedScratch_addr) - 8, 12},
    {Address(&Field_Kind2Z), 8},
    {Address(&Field_ScriptFlags), 2},
    {Address(&MoveScript_F3Divisor), 2},
    {Address(&MoveScript_FAWord), 2},
    {Address(&Field_Request), 1},
    {Address(&Transition_Busy), 2},
    {Address(&Field_Kind2Hold), 1},
    {Address(&MapView_Elevation), 4},
};
constexpr unsigned kRegionBytes = sizeof(records::SpriteObject) + 0x40 + 8 * 0xA4 + MoveScript_TintRecords_count +
                                  MoveScript_PartyRecords_count + 12 + 8 + 2 + 2 + 2 + 1 + 2 + 1 + 4;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char object[kBuf], sprite[kBuf], field[0x200];
    unsigned char* current;
    unsigned char* active;
    std::uint16_t area;
    Entry log[kLog];
    unsigned log_n;
    unsigned char result;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.object, g_object, kBuf);
    std::memcpy(s.sprite, g_sprite, kBuf);
    std::memcpy(s.field, g_field, sizeof s.field);
    s.current = Sprite_Current;
    s.active = Field_ActiveMember;
    s.area = Game_AreaNumber;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_object, s.object, kBuf);
    std::memcpy(g_sprite, s.sprite, kBuf);
    std::memcpy(g_field, s.field, sizeof s.field);
    Sprite_Current = s.current;
    Field_ActiveMember = s.active;
    Game_AreaNumber = s.area;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }

// The boundaries each op tests, seeded a third of the time.
void Seed(unsigned char op, unsigned char* p) {
    unsigned char* const sprite = g_sprite;
    switch (op) {
    case 0xC8: if (Often()) p[1] = sprite[8]; break;
    case 0xCE: if (Often()) sprite[8] = static_cast<unsigned char>((sprite[8] & 0xF8) | ((ObjTrio[0].direction ^ 4) & 7)); break;
    case 0xD4: if (Often()) p[7] = 0; break;
    case 0xD5: case 0xD6:
        if (Often()) {
            unsigned char* const r = PartyRecord(PartySlot() & 7);
            r[1] = static_cast<unsigned char>(0 - r[2]);
        }
        break;
    case 0xD9: if (Often()) Field_Request = 2; break;
    case 0xD1: case 0xDF: if (Often()) Field_Request = 3; break;
    case 0xDA: if (Often()) Transition_Busy = 0; break;
    case 0xEA: if (Often()) sprite[6] = 0x0A; break;
    case 0x9F: if (Often()) sprite[0xB] = static_cast<unsigned char>(Next() % 2); break;
    case 0x8C: if (Often()) p[1] = 0x94; break;
    case 0x85:
        if (Next() % 2) { Field_Kind2X = Long(rv::Bytes(ObjTrio) + 0x34); Field_Kind2Z = Long(rv::Bytes(ObjTrio) + 0x38); }
        else if (Often()) {
            Field_Kind2X = Long(rv::Bytes(ObjTrio) + 0x34) + static_cast<long>(Next() % 0x10000) - 0x8000;
            Field_Kind2Z = Long(rv::Bytes(ObjTrio) + 0x38) + static_cast<long>(Next() % 0x10000) - 0x8000;
        }
        if (Often()) Field_Kind2Hold = 0;
        break;
    default: break;
    }
}

void SelfTest(void* const (&theirs)[kGroupCount]) {
    constexpr unsigned kRounds = 30000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("move_groups: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    Capture(saved);
    unsigned char* const saved_field = Field_State;
    Field_State = g_field;
    const void* const area_table = kAreaStubs.data();
    std::memcpy(g_descriptor + 0x3C, &area_table, sizeof area_table);
    unsigned char** const scripts = g_scripts18;
    std::memcpy(g_descriptor + 0x18, &scripts, sizeof scripts);
    move_groups::g = kStubs;

    unsigned bad = 0, calls = 0, per_group[kGroupCount] = {}, ops[256] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kGroupCount;
        const Group& grp = kGroups[k];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        for (auto& s : g_scripts18) s = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(Next()));
        const unsigned start = Next() % 0xFF00;
        for (unsigned i = 0; i < 16; ++i) g_script[start + i] = static_cast<unsigned char>(Next());
        const unsigned span = grp.last_op - grp.first_op + 1u;
        const auto op = static_cast<unsigned char>(Next() % 16 ? grp.first_op + Next() % span : Next());
        g_script[start] = op;
        ++ops[op];
        ++per_group[k];
        SetPos(input.object, start);
        input.current = Next() % 5 ? g_sprite : rv::Bytes(&Sprite_Kind2);
        // The party slot D4..D6 compute: every one of 0..7, so that D4's clamp
        // (above 4) and D5's bound (4 and up) are met; a few bytes into the
        // object sometimes, since the original divides, truncating. A uniform
        // pointer across the four objects met slot 4 once in ~650 rounds and
        // never above it - a control moving D4's clamp went unseen.
        {
            const unsigned slot = Next() % 8;
            input.active = At(Address(rv::Bytes(Sprite_ObjectsExtra)) + slot * 0xA4 + (slot < 7 ? Next() % 3 : 0));
        }
        input.area = static_cast<std::uint16_t>(Next() % Area_Descriptors_count);
        const auto value = static_cast<unsigned char>(Next());
        g_seed = Next();
        // Seeding writes through the globals: apply, seed, and take that as the input.
        Apply(input);
        Seed(op, g_script + start);
        Capture(input);
        unsigned char* const saved_descriptor = Area_Descriptors[input.area];
        Area_Descriptors[input.area] = g_descriptor;

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* const fn = pass ? kOurs[k] : theirs[k];
            unsigned char r = 0;
            if (k == 0) reinterpret_cast<GroupBFn>(const_cast<void*>(fn))(g_object, g_script);
            else r = reinterpret_cast<GroupFn>(const_cast<void*>(fn))(g_object, g_script, value);
            State& out = pass ? our_out : their_out;
            Capture(out);
            out.result = r;
        }
        Area_Descriptors[input.area] = saved_descriptor;
        calls += their_out.log_n;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      move_groups self-test MISMATCH: round %u, %s op %02X, result %02X / %02X, position %04X / %04X, "
                      "log %u / %u", round, grp.name, op, their_out.result, our_out.result, Pos(their_out.object),
                      Pos(our_out.object), their_out.log_n, our_out.log_n);
    }
    move_groups::g = kOriginalsMoveGroups;
    Field_State = saved_field;
    Apply(saved);
    unsigned covered = 0, listed = 0;
    for (const Group& grp : kGroups)
        for (unsigned op = grp.first_op; op <= grp.last_op; ++op, ++listed) covered += ops[op] != 0;
    bof3::Log("shadow      move_groups self-test: %u rounds (%u per group), %u of %u ops of the seven groups generated, "
              "%u calls to the stand-ins, %u MISMATCHES; the context, sprite, field state, party objects, tint and party "
              "records, ObjTrio 0, Sprite_Kind2, eight globals, the result and the stand-ins' log compared",
              kRounds, per_group[0], covered, listed, calls, bad);
    if (bad) bof3::Fatal("the movement script's group handlers differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void MoveGroups_Inject() {
    if (bof3::WantsShadow("move_groups")) {
        void* clones[kGroupCount];
        for (unsigned k = 0; k < kGroupCount; ++k) {
            const Group& grp = kGroups[k];
            bof3::CloneCall calls[16];
            if (grp.n_calls > 16) bof3::Fatal("move_groups: %s has %d calls", grp.name, grp.n_calls);
            for (int i = 0; i < grp.n_calls; ++i) calls[i] = {grp.calls[i].offset, StubFor(grp.calls[i].target), grp.calls[i].target};
            clones[k] = bof3::CloneOriginal(grp.name, grp.base, grp.size, calls, grp.n_calls);
            Relocate(clones[k], grp.base, grp.size, grp.table);
        }
        SelfTest(clones);
    }
}
