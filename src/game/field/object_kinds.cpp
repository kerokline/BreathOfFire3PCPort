// The field objects' kind handlers - the table at 0x65F5F8, indexed by the
// sprite's pose byte +1, less entries 4 and 7 (Field_ObjectUpdate and
// Field_ObjectFollow, field_objects.cpp) - with the pace table 0x65F5DC they
// call, the field object loops' three other per-object calls, and the small
// helpers only these use. docs/object-kinds.md.
//
// Every handler is called with the object, which in the loops is also
// Sprite_Current: the sprite fields ([1] pose, [2] pace, [3] pose to go back
// to, [4] sub-state, [6] type, [7] flags, [8] direction, [9] a timed move's
// frames, [0xA] a countdown) and the object fields (+0x80 the script context's
// flags, +0x81 a wait, +0x84 a speed index, +0x85 a target direction, +0x87
// steps) are one block there. The originals reload Sprite_Current after every
// call, and so does this code, by naming it each time.
#include "game/object_kinds.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return Low32(p); }

// Data these read that symbols.toml does not name.
constexpr std::uint32_t kPaceTable = 0x65F5DC;      // 7 handlers, by Sprite_Current[2], no bound
// kSteps: now Field_DirectionSteps (symbols.toml). per direction: x, z step (16.16)
// kCounter: now Field_StepCount (symbols.toml). byte: a counter the loop copies...
constexpr std::uint32_t kCounterSeen = 0x8034E8;    // ...here after each pass
constexpr std::uint32_t kBlockFlags = 0x8034E1;     // bit 6: blocks the idle turn
// kEncounter: now BattleE1_Mode (symbols.toml). bytes +0, +1 and +0xC: a type-5 touch
// kTalker: now AreaW1a_FocusObject (symbols.toml). dword: Field_ActiveMember of the last talk
constexpr std::uint32_t kLeaderDirection = 0x802D48;  // ObjTrio +8
constexpr std::uint32_t kLinkArgument = 0x802DC9;   // ObjTrio +0x89

object_kinds::CalleesObjectKinds OriginalsObjectKinds() {
    return {reinterpret_cast<const ::bof3::CodeSlot<object_kinds::PaceFn>*>(static_cast<std::uintptr_t>(::bof3::Img(kPaceTable))),
            Field_Pace0,
            Rand,
            Field_ObjectRandomTurn,
            Field_ObjectOpenDirection,
            Field_ObjectInHome,
            Field_ObjectTurn,
            Field_ObjectSettle,
            Field_ObjectMotion,
            Field_ObjectFleeDirection,
            Field_ObjectChaseDirection,
            Field_ObjectBestDirection,
            Field_ObjectBlockedAhead,
            MoveCmd_Move,
            Sprite_SetTint,
            Tint_Release,
            Area_ObjectHandler,
            Field_ObjectTrigger,
            EventScript_Run,
            reinterpret_cast<void (__cdecl*)(unsigned short)>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Msg_OpenScript))),
            reinterpret_cast<void (__cdecl*)(unsigned)>(static_cast<std::uintptr_t>(::bof3::Code(bof3::addr::Msg_OpenSystem)))};
}
}  // namespace (object_kinds::gObjectKinds is beside its first reader, in field/field_3.cpp)
namespace {
// The field object, rv::Current<records::SpriteObject>(): the handlers'
// argument, which the loops also make Sprite_Current.

}  // namespace

// --- the handler table 0x65F5F8 ------------------------------------------------

// --- the pace table 0x65F5DC ---------------------------------------------------

// --- helpers only the kinds use ------------------------------------------------

// --- the loops' other calls ----------------------------------------------------

namespace {

// --- BOF3X_SHADOW=object_kinds: a differential fuzz, once at start-up --------
// Nineteen byte-copies (the two fades are one), every relative call re-aimed at
// a recorder, every `call [reg*4 + 0x65F5DC]` at a table of 256 recording pace
// entries, the fades' two jump tables moved into the copy. One round: one
// function, a random object / sprite / member (the sprite often the object,
// as in the loops), the globals they touch, boundaries seeded; theirs, then
// from the same state ours; the result byte, all of it and the recorders' log
// compared.

constexpr unsigned kBuf = 0x100, kLog = 48;
unsigned char g_object[kBuf], g_sprite[kBuf], g_member[kBuf], g_descriptor[0x44];
const unsigned char* g_scripts[0x1000];   // the descriptor's +4 table
unsigned char* g_current_object;
struct Entry { std::uint32_t what, a, b, c; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const unsigned char* const bufs[3] = {g_object, g_sprite, g_member};
    for (unsigned i = 0; i < 3; ++i) {
        if (p == bufs[i]) return 1 + i;
        if (p == bufs[i] + 0x80) return 4 + i;
    }
    return Address(p);
}
// A callee may change what the caller reads after it: the fields the real
// callees write (steps, frames, pose, direction, countdown, flags), the talk
// word, Field_Request, and - rarely - which object and member are current.
void Disturb() {
    const std::uint32_t h = Hash();
    unsigned char* const o = g_current_object;
    if (h % 4 == 0) o[0x80] = static_cast<unsigned char>(h >> 8);
    if (h % 5 == 0) Sprite_Current[9] = static_cast<unsigned char>((h >> 12) % 3);
    if (h % 6 == 0) Sprite_Current[8] = static_cast<unsigned char>(h >> 16);
    if (h % 7 == 0) o[0x87] = static_cast<unsigned char>((h >> 20) % 3);
    if (h % 9 == 0) Sprite_Current[1] = static_cast<unsigned char>(h % 2 ? 0x0A : h >> 24);
    if (h % 11 == 0) Sprite_Current[0xA] = static_cast<unsigned char>((h >> 8) % 3);
    if (h % 13 == 0) o[0x89] ^= 0x80;   // the talk word's bit 15
    if (h % 17 == 0) Field_Request = 2;
    if (h % 19 == 0) o[0x81] = static_cast<unsigned char>(h >> 4);
    if (h % 23 == 0) Field_ActiveMember = (h >> 9) % 2 ? g_member : g_object;
    if (h % 29 == 0) Sprite_Current = (h >> 9) % 2 ? g_sprite : g_object;
    if (h % 31 == 0) Sprite_Current[3] = static_cast<unsigned char>((h >> 5) % 2 ? 4 : h >> 16);
}
unsigned char Direction() {   // 0xFF a quarter of the time, else any byte
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h % 4 == 0 ? 0xFF : h >> 8);
}

template <int K>
unsigned char __cdecl StubPace(unsigned char* o, unsigned speed, unsigned flags) {
    Record(40 + K, Id(o), speed & 0xFF, flags & 0xFF);
    Disturb();
    return static_cast<unsigned char>(Hash() % 3 ? 0 : Hash() >> 7 | 1);
}
::bof3::CodeSlot<object_kinds::PaceFn> g_pace_stubs[256];

unsigned char __cdecl StubPace0(unsigned char* o, unsigned speed, unsigned flags) { Record(1, Id(o), speed & 0xFF, flags & 0xFF); Disturb(); return static_cast<unsigned char>(Hash()); }
int __cdecl StubRand() { Record(2); return static_cast<int>(Hash()); }
unsigned char __cdecl StubRandomTurn(unsigned char* o) { Record(3, Id(o)); Disturb(); return static_cast<unsigned char>(Hash()); }
unsigned char __cdecl StubOpenDirection(unsigned char* o) { Record(4, Id(o)); Disturb(); return Direction(); }
unsigned char __cdecl StubInHome(unsigned char* o) { Record(5, Id(o)); return static_cast<unsigned char>(Hash() % 2 ? Hash() | 1 : 0); }
void __cdecl StubTurn(unsigned char* o) { Record(6, Id(o)); Disturb(); }
void __cdecl StubSettle(unsigned char* o, unsigned d) { Record(7, Id(o), d & 0xFF); }
void __cdecl StubMotion(unsigned char* o) { Record(8, Id(o)); }
unsigned char __cdecl StubFlee(unsigned char* o) { Record(9, Id(o)); Disturb(); return Direction(); }
unsigned char __cdecl StubChase(unsigned char* o) { Record(10, Id(o)); Disturb(); return Direction(); }
unsigned char __cdecl StubBest(unsigned char* o, unsigned toward) { Record(11, Id(o), toward & 0xFF); Disturb(); return Direction(); }
unsigned char __cdecl StubBlockedAhead(unsigned char* o) { Record(12, Id(o), Sprite_Current[8]); Disturb(); return static_cast<unsigned char>(Hash() % 2 ? Hash() | 1 : 0); }
void __cdecl StubMove(unsigned char* c, unsigned d) { Record(13, Id(c), d & 0xFF); Disturb(); }
unsigned char __cdecl StubSetTint(unsigned char* s, unsigned r, unsigned gr, unsigned b, unsigned a) {
    Record(14, Id(s), (r & 0xFF) | (gr & 0xFF) << 8 | (b & 0xFF) << 16, a & 0xFF);
    return static_cast<unsigned char>(Hash());
}
void __cdecl StubTintRelease(unsigned i) { Record(15, i & 0xFF); Disturb(); }
void __cdecl StubAreaHandler(unsigned n) { Record(16, n & 0xFFFF); }
void __cdecl StubTrigger(unsigned char* o) { Record(17, Id(o)); Disturb(); }
void __cdecl StubEventScript(const unsigned char* p) { Record(18, Address(p)); Disturb(); }
void __cdecl StubMsgScript(unsigned id) { Record(19, id & 0xFFFF); }
void __cdecl StubMsgSystem(unsigned id) { Record(20, id); }

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
object_kinds::CalleesObjectKinds Stubs() {
    object_kinds::CalleesObjectKinds s{};
    s.pace = g_pace_stubs;
    s.pace0 = StubPace0;
    s.rand = StubRand;
    s.random_turn = StubRandomTurn;
    s.open_direction = StubOpenDirection;
    s.in_home = StubInHome;
    s.turn = StubTurn;
    s.settle = As<decltype(s.settle)>(&StubSettle);
    s.motion = StubMotion;
    s.flee = StubFlee;
    s.chase = StubChase;
    s.best = As<decltype(s.best)>(&StubBest);
    s.blocked_ahead = StubBlockedAhead;
    s.move = As<decltype(s.move)>(&StubMove);
    s.set_tint = As<decltype(s.set_tint)>(&StubSetTint);
    s.tint_release = As<decltype(s.tint_release)>(&StubTintRelease);
    s.area_handler = As<decltype(s.area_handler)>(&StubAreaHandler);
    s.trigger = StubTrigger;
    s.event_script = StubEventScript;
    s.msg_script = As<decltype(s.msg_script)>(&StubMsgScript);
    s.msg_system = StubMsgSystem;
    return s;
}

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5187C0: return f(&StubPace0);
    case 0x5B93D2: return f(&StubRand);
    case 0x518DD0: return f(&StubRandomTurn);
    case 0x518CA0: return f(&StubOpenDirection);
    case 0x518000: return f(&StubInHome);
    case 0x5196E0: return f(&StubTurn);
    case 0x518D10: return f(&StubSettle);
    case 0x518980: return f(&StubMotion);
    case 0x518E20: return f(&StubFlee);
    case 0x518ED0: return f(&StubChase);
    case 0x518F80: return f(&StubBest);
    case 0x518080: return f(&StubBlockedAhead);
    case 0x578C10: return f(&StubMove);
    case 0x454CC0: return f(&StubSetTint);
    case 0x454D60: return f(&StubTintRelease);
    case 0x537540: return f(&StubAreaHandler);
    case 0x56D6B0: return f(&StubTrigger);
    case 0x5797C0: return f(&StubEventScript);
    case 0x4976D0: return f(&StubMsgScript);
    case 0x497710: return f(&StubMsgSystem);
    default: bof3::Fatal("object_kinds: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The copies and their calls out, by capstone 2026-09-22 (every jump stays
// inside); `pace` lists the disp32 of each `call [reg*4 + 0x65F5DC]`.
struct Call { std::uint32_t offset, target; };
struct Clone {
    const char* name;
    std::uint32_t base, size;
    Call calls[7];
    int n_calls;
    std::uint32_t pace[3];
    int n_pace;
};
constexpr ::bof3::image::CodeVa kFadeBase{0x5193B0};
constexpr std::uint32_t kFadeSize = 0x249, kFadeInOffset = 0x130;
const Clone kClones[] = {
    {"Field_ObjectWander", 0x517640, 0x14F,
     {{0x7b, 0x518dd0}, {0x90, 0x5b93d2}, {0xc1, 0x518ca0}, {0x112, 0x5196e0}, {0x139, 0x518d10}, {0x142, 0x518980}}, 6, {0x21, 0x6f, 0xab}, 3},
    {"Field_ObjectFlee", 0x517790, 0x16A,
     {{0x82, 0x518e20}, {0x98, 0x5b93d2}, {0xba, 0x518f80}, {0x121, 0x5196e0}, {0x155, 0x518d10}, {0x15e, 0x518980}}, 6, {0x20, 0x72, 0xb3}, 3},
    {"Field_ObjectChase", 0x517900, 0x16A,
     {{0x82, 0x518ed0}, {0x98, 0x5b93d2}, {0xba, 0x518f80}, {0x121, 0x5196e0}, {0x155, 0x518d10}, {0x15e, 0x518980}}, 6, {0x20, 0x72, 0xb3}, 3},
    {"Field_ObjectWanderHome", 0x517A70, 0x173,
     {{0x7a, 0x518dd0}, {0x8f, 0x5b93d2}, {0xc0, 0x518ca0}, {0xdd, 0x518000}, {0x129, 0x5196e0}, {0x15e, 0x518d10}, {0x167, 0x518980}}, 7, {0x20, 0x6e, 0xaa}, 3},
    {"Field_ObjectWait", 0x518AC0, 0x31, {}, 0, {}, 0},
    {"Field_ObjectStill", 0x517FE0, 0x17, {}, 0, {}, 0},
    {"Field_ObjectFadeOut", kFadeBase.v, kFadeSize, {{0x2e, 0x454cc0}, {0xf4, 0x454d60}, {0x15e, 0x454cc0}, {0x217, 0x454d60}}, 4, {}, 0},
    {"Field_ObjectFadeIn", 0, 0, {}, 0, {}, 0},   // FadeOut's copy + 0x130
    {"Field_ObjectTurn", 0x5196E0, 0x103, {{0x4d, 0x518080}, {0x84, 0x578c10}}, 2, {}, 0},
    {"Field_ObjectLinked", 0x519600, 0x6F, {{0x35, 0x537540}, {0x68, 0x537540}}, 2, {}, 0},
    {"Field_ObjectIdleLong", 0x518B40, 0x15A, {{0x52, 0x5187c0}, {0x8f, 0x518080}, {0xad, 0x518000}, {0x12e, 0x578c10}}, 4, {}, 0},
    {"Field_ObjectIdle", 0x517F30, 0xA7, {{0x1c, 0x56d6b0}}, 1, {}, 0},
    {"Field_ObjectTalk", 0x517E90, 0x95, {{0x3f, 0x5797c0}, {0x56, 0x4976d0}, {0x63, 0x497710}}, 3, {}, 0},
    {"Field_Pace0", 0x5187C0, 0x32, {}, 0, {}, 0},
    {"Field_Pace4", 0x518800, 0x71, {}, 0, {}, 0},
    {"Field_Pace5", 0x518880, 0x71, {}, 0, {}, 0},
    {"Field_Pace6", 0x518900, 0x71, {}, 0, {}, 0},
    {"Field_ObjectOpenDirection", 0x518CA0, 0x6A, {{0x18, 0x518080}}, 1, {}, 0},
    {"Field_ObjectRandomTurn", 0x518DD0, 0x44, {{0x5, 0x5b93d2}}, 1, {}, 0},
    {"Field_ObjectInHome", 0x518000, 0x71, {}, 0, {}, 0},
};
constexpr unsigned kCount = sizeof kClones / sizeof kClones[0];
enum : unsigned {
    kWander, kFlee, kChase, kWanderHome, kWait, kStill, kFadeOut, kFadeIn, kTurn, kLinked, kIdleLong, kIdle, kTalk,
    kPace0, kPace4, kPace5, kPace6, kOpenDirection, kRandomTurn, kInHome
};
static_assert(kInHome + 1 == kCount, "one enumerator per clone");

// The fades' jump tables: 0x65F654 (FadeOut, 2 entries, then FadeIn's) and
// 0x65F65C (FadeIn, 2): four entries, moved into the copy.
std::uint32_t g_fade_table[4];

void Patch32(void* copy, std::uint32_t at, std::uint32_t expected, std::uint32_t value, const char* name) {
    auto* code = static_cast<std::uint8_t*>(copy);
    std::uint32_t old;
    std::memcpy(&old, code + at, sizeof old);
    if (old != expected) bof3::Fatal("object_kinds: %s +0x%X holds 0x%X, not 0x%X", name, (unsigned)at, (unsigned)old, (unsigned)expected);
    std::memcpy(code + at, &value, sizeof value);
}

// What the functions touch outside the three buffers. The first region runs
// from below Sprite_Objects (Field_ObjectIdle's object may sit anywhere in or
// just below it) through MoveScript_TintRecords.
struct Region { std::uint32_t at, size; };
constexpr std::uint32_t kObjectsLow = 0x7DEE80 - 0x200;
const Region kRegions[] = {
    {::bof3::Img(kObjectsLow), 0x7E1300 - kObjectsLow},
    {Address(&Field_Request), 1},
    {Address(&Game_Mode), 2},
    {::bof3::Img(BattleE1_Mode_addr), 0x10},
    {::bof3::Img(0x8034E0), 0x10},   // kBlockFlags, kCounterSeen
    {Address(&Field_ScriptFlags), 2},
    {::bof3::Img(AreaW1a_FocusObject_addr), 4},
    {::bof3::Img(Field_StepCount_addr), 1},
    {Address(rv::Bytes(ObjTrio)), 0x100}, // kLeaderDirection, kLinkArgument
};
constexpr unsigned kRegionBytes = (0x7E1300 - kObjectsLow) + 1 + 2 + 0x10 + 0x10 + 2 + 4 + 1 + 0x100;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char object[kBuf], sprite[kBuf], member[kBuf];
    unsigned char* current;
    unsigned char* active;
    std::uint16_t area;
    std::uint32_t result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s, std::uint32_t result) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.object, g_object, kBuf);
    std::memcpy(s.sprite, g_sprite, kBuf);
    std::memcpy(s.member, g_member, kBuf);
    s.current = Sprite_Current;
    s.active = Field_ActiveMember;
    s.area = Game_AreaNumber;
    s.result = result;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_object, s.object, kBuf);
    std::memcpy(g_sprite, s.sprite, kBuf);
    std::memcpy(g_member, s.member, kBuf);
    Sprite_Current = s.current;
    Field_ActiveMember = s.active;
    Game_AreaNumber = s.area;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x3C6EF372u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }
unsigned char Pick(std::initializer_list<unsigned char> values) { return values.begin()[Next() % values.size()]; }

using ObjectFn = void (__cdecl*)(unsigned char*);
using ByteFn = std::uint32_t (__cdecl*)(unsigned char*);   // al is the result; the rest of eax is noise
using PaceCallFn = std::uint32_t (__cdecl*)(unsigned char*, unsigned, unsigned);
using VoidFn = void (__cdecl*)();

const void* Ours(unsigned k) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (k) {
    case kWander: return f(&Field_ObjectWander);
    case kFlee: return f(&Field_ObjectFlee);
    case kChase: return f(&Field_ObjectChase);
    case kWanderHome: return f(&Field_ObjectWanderHome);
    case kWait: return f(&Field_ObjectWait);
    case kStill: return f(&Field_ObjectStill);
    case kFadeOut: return f(&Field_ObjectFadeOut);
    case kFadeIn: return f(&Field_ObjectFadeIn);
    case kTurn: return f(&Field_ObjectTurn);
    case kLinked: return f(&Field_ObjectLinked);
    case kIdleLong: return f(&Field_ObjectIdleLong);
    case kIdle: return f(&Field_ObjectIdle);
    case kTalk: return f(&Field_ObjectTalk);
    case kPace0: return f(&Field_Pace0);
    case kPace4: return f(&Field_Pace4);
    case kPace5: return f(&Field_Pace5);
    case kPace6: return f(&Field_Pace6);
    case kOpenDirection: return f(&Field_ObjectOpenDirection);
    case kRandomTurn: return f(&Field_ObjectRandomTurn);
    default: return f(&Field_ObjectInHome);
    }
}

// Sets the round's input: random bytes everywhere, then the boundaries each
// branch turns on. Returns the object argument.
unsigned char* Seed(unsigned k, State& input, unsigned& speed, unsigned& flags) {
    auto* bytes = reinterpret_cast<unsigned char*>(&input);
    for (unsigned i = 0; i < offsetof(State, current); ++i) bytes[i] = static_cast<unsigned char>(Next());
    input.current = Next() % 2 ? g_object : g_sprite;   // the loops' own case half the time
    const unsigned m = Next() % 3;
    input.active = m == 0 ? g_object : m == 1 ? g_member : g_sprite;
    input.area = static_cast<std::uint16_t>(Next() % Area_Descriptors_count);
    g_seed = Next();
    Apply(input);

    unsigned char* object = g_object;
    if (k == kIdle && Often()) object = At(::bof3::Img(kObjectsLow) + Next() % (0x200 + 29 * 0xA4));   // in, and below, Sprite_Objects
    g_current_object = object;
    unsigned char* const s = Sprite_Current;
    unsigned char* const member = Field_ActiveMember;
    // Timed move, steps, speed (0..5 mostly), wait.
    s[9] = static_cast<unsigned char>(Next() % 2 ? 0 : Next() % 3);
    object[0x87] = Pick({0, 0, 0, 1, 2, 0xF8, 0xF9, 0xFA, 0xFB, static_cast<unsigned char>(Next())});
    object[0x84] = static_cast<unsigned char>(Next() % 4 ? Next() % 6 : Next());
    object[0x81] = static_cast<unsigned char>(Next() % 2 ? 0 : Next() % 3 ? 1 : Next());
    object[0x86] = static_cast<unsigned char>(Next() % 2 ? 0xFF : Next());
    // Poses, type, flags, sub-state.
    if (Often()) s[1] = Pick({3, 4, 6, 0x0A});
    if (Often()) s[3] = Pick({3, 4, 6, 0x0A});
    if (Often()) s[6] = Pick({1, 3, 4, 5, 8, 0x0A});
    if (Often()) s[7] &= static_cast<unsigned char>(Next());
    if (Often()) s[0] ^= 0x40;
    s[4] = static_cast<unsigned char>(Next() % (k == kFadeOut ? 4 : 2));
    // Directions: the target equal to the facing, half a turn off (the turn's
    // boundary), bit 3 set; the countdown at 0.
    if (Often()) s[8] &= 7;
    if (Often()) object[0x85] = s[8];
    else if (Often()) object[0x85] = static_cast<unsigned char>((s[8] + Pick({3, 4, 5})) & 7);
    if (Often()) s[0xA] = 0;
    // The member's area handler index and tint record, its bytes near the
    // fades' ends and signs.
    if (Often()) member[0xA0] = Pick({0x7F, 0xFF, 0x7E, 0});
    unsigned char* const record = MoveScript_TintRecords + member[0x9F] * 12u;
    if (Often()) {
        const unsigned char near = Pick({0, 1, 2, 3, 0x1D, 0x1E, 0x1F, 0x20, 0x7F, 0x80, 0xFE, 0xFF});
        for (unsigned i = 2; i < 5; ++i) record[i] = Often() ? near : Pick({0, 1, 3, 0x1E, 0x1F, 0x80, 0xFF, 0xFE});
    }
    // Globals.
    if (Often()) Field_Request = 2;
    if (Often()) Field_StatusBits &= 0xBF;
    if (Often()) At(::bof3::Img(Field_StepCount_addr))[0] = At(::bof3::Img(Field_StepCountPrev_addr))[0];
    // The talk word: none, a script, a system message, a script message.
    const unsigned talk = Next() % 4;
    SetWord(object + 0x88, talk == 0 ? 0xFFFFu : talk == 1 ? (0x8000u | Next()) : talk == 2 ? ((Next() & 0x5FFFu) | 0x2000u) : Next() & 0x5FFFu);
    // The home box: the next step's high words at the edge, just in, just out.
    if (k == kInHome && Next() % 4) {
        const unsigned char* const step = At(::bof3::Img(Field_DirectionSteps_addr)) + (object[8] & 7) * 8u;
        for (unsigned axis = 0; axis < 2; ++axis) {
            const auto home = static_cast<std::int16_t>(Next() % 4 ? Next() % 64 : Next());
            const auto half = static_cast<std::int16_t>(Next() % 8 ? Next() % 8 : Next());
            SetWord(object + 0x8E + axis * 4, static_cast<std::uint16_t>(home));
            SetWord(object + 0x98 + axis * 2, static_cast<std::uint16_t>(half));
            const int side = Next() % 2 ? 1 : -1;
            const auto hi = static_cast<std::uint32_t>(home + side * half + static_cast<int>(Next() % 3) - 1);
            const std::uint32_t target = hi << 16 | (Next() & 0xFFFF);
            SetLong(object + 0x34 + axis * 4, static_cast<std::int32_t>(target - static_cast<std::uint32_t>(Long(step + axis * 4))));
        }
    }
    // A pace entry's arguments: bytes with noise above them; a speed never 0
    // where it would divide.
    flags = (Next() & ~0xFFu) | (Next() % 4);
    speed = (Next() & ~0xFFu) | (1 + Next() % 32);
    if (Next() % 8 == 0) speed = (speed & ~0xFFu) | (1 + Next() % 255);
    if (!(flags & 2) && Next() % 4 == 0) speed &= ~0xFFu;
    return object;
}

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 30000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("object_kinds: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    unsigned char* const saved_object = g_current_object;
    Capture(saved, 0);
    const unsigned char* const* const scripts = g_scripts;
    std::memcpy(g_descriptor + 4, &scripts, sizeof scripts);
    for (unsigned i = 0; i < 0x1000; ++i) g_scripts[i] = At(0x20000u + i * 0x10u);
    object_kinds::gObjectKinds = Stubs();

    unsigned bad = 0, calls = 0, per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        unsigned speed = 0, flags = 0;
        unsigned char* const object = Seed(k, input, speed, flags);
        Capture(input, 0);
        unsigned char* const saved_descriptor = Area_Descriptors[input.area];
        Area_Descriptors[input.area] = g_descriptor;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            g_current_object = object;
            const void* const fn = pass ? Ours(k) : theirs[k];
            std::uint32_t result = 0;
            if (k == kLinked) {
                reinterpret_cast<VoidFn>(const_cast<void*>(fn))();
            } else if (k >= kPace0 && k <= kPace6) {
                result = reinterpret_cast<PaceCallFn>(const_cast<void*>(fn))(object, speed, flags) & 0xFF;
            } else if (k == kTalk) {
                result = reinterpret_cast<ByteFn>(const_cast<void*>(fn))(object + 0x80) & 0xFF;
            } else if (k >= kOpenDirection) {
                result = reinterpret_cast<ByteFn>(const_cast<void*>(fn))(object) & 0xFF;
            } else {
                reinterpret_cast<ObjectFn>(const_cast<void*>(fn))(object);
            }
            Capture(pass ? our_out : their_out, result);
        }
        Area_Descriptors[input.area] = saved_descriptor;
        calls += their_out.log_n;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      object_kinds self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X", round, kClones[k].name,
                      their_out.log_n, our_out.log_n, (unsigned)their_out.result, (unsigned)our_out.result);
    }
    object_kinds::gObjectKinds = OriginalsObjectKinds();
    Apply(saved);
    g_current_object = saved_object;
    bof3::Log("shadow      object_kinds self-test: %u rounds (%u per function, %u functions), %u calls to the stand-ins, %u MISMATCHES; "
              "the object, sprite, member, Sprite_Objects to MoveScript_TintRecords, the globals, the result byte and the stand-ins' "
              "log compared", kRounds, per[0], kCount, calls, bad);
    if (bad) bof3::Fatal("the object kind handlers differ from the original in %u of %u self-test rounds", bad, kRounds);
}

template <int... K>
void FillPaceStubs(std::integer_sequence<int, K...>) {
    const object_kinds::PaceFn eight[] = {&StubPace<K>...};
    for (unsigned i = 0; i < 256; ++i) g_pace_stubs[i] = eight[i % 8];
}

}  // namespace

void ObjectKinds_Inject() {
    if (bof3::WantsShadow("object_kinds")) {
        FillPaceStubs(std::make_integer_sequence<int, 8>{});
        void* clones[kCount];
        for (unsigned k = 0; k < kCount; ++k) {
            const Clone& c = kClones[k];
            if (k == kFadeIn) {
                clones[k] = static_cast<std::uint8_t*>(clones[kFadeOut]) + kFadeInOffset;
                continue;
            }
            bof3::CloneCall calls[7];
            for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
            clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
            for (int i = 0; i < c.n_pace; ++i) Patch32(clones[k], c.pace[i], kPaceTable, Address(g_pace_stubs), c.name);
        }
        // The fades: the four case addresses of 0x65F654 / 0x65F65C, moved.
        const std::uint32_t moved = Address(clones[kFadeOut]) - kFadeBase.v;
        for (unsigned i = 0; i < 4; ++i) {
            std::uint32_t target;
            std::memcpy(&target, At(::bof3::Img(Field_ObjectFadeOutSteps_addr) + 4 * i), sizeof target);
            if (target < kFadeBase.v || target >= kFadeBase.v + kFadeSize) bof3::Fatal("object_kinds: fade case %u is 0x%X", i, (unsigned)target);
            g_fade_table[i] = target + moved;
        }
        Patch32(clones[kFadeOut], 0xE, 0x65F654, Address(&g_fade_table[0]), "Field_ObjectFadeOut");
        Patch32(clones[kFadeOut], 0x13E, 0x65F65C, Address(&g_fade_table[2]), "Field_ObjectFadeIn");
        SelfTest(clones);
    }
}
