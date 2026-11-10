// A field object's update around its movement script: originals 0x517BF0
// (the update), 0x518980 (the timed move's motion), 0x5197F0 (the party
// record's velocity) and 0x518D10 (settling the direction).
// docs/movement-script.md section 1b.
#include "game/field_objects.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

// Sprite_Current, as rv::Current<records::SpriteObject>(), may be a Sprite_Objects
// record or another family whose first 0x80 bytes are a sprite object's (an
// ObjTrio member, the kind-2 object), so only that part is named.

std::uint32_t Address(const volatile void* p) { return Low32(p); }

const field_objects::CalleesFieldObjects kOriginalsFieldObjects = {
    MoveScript_Step, Sprite_SetPoseWait, Field_ObjectBlocked, MoveCmd_Move, Field_ObjectSettle,
    Field_ObjectMotion, AreaMap_Elevation, Party_ApplyRecord, Party_MoveMember,
    Sprite_ObjectByHandle, MoveCmd_AttachOffset, Field_ObjectUpdate, Gte_PushMatrix, Gte_RotMatrix,
    Gte_TransMatrix, Gte_SetRotMatrix, Gte_SetTransMatrix, Gte_RotTrans, Gte_PopMatrix,
};
}  // namespace
namespace {
// As MoveScript_GroupD's: Field_ActiveMember's party slot, as a byte.
unsigned char PartySlot() {
    const auto distance = static_cast<std::int32_t>(Low32(Field_ActiveMember.Native()) - Address(rv::Bytes(Sprite_ObjectsExtra)));
    return static_cast<unsigned char>(distance / 0xA4);
}

}  // namespace

namespace {

// --- BOF3X_SHADOW=field_objects: a differential fuzz, once at start-up ------
// Four byte-copies, their calls re-aimed at the recorders below - the update's
// calls of the motion and the settle included, so each function is tested
// alone. One round: one of the four, a random object and sprite (or
// Sprite_Kind2), a party slot 0..7, the party records, scratch byte 3 and
// Field_Request; for the update, a speed with steps left, and a step result
// of 0xFF or the sprite's own direction often; theirs, then from the same
// state ours; all of it, the result of nothing (all return void) and the
// recorders' log compared.

constexpr unsigned kBuf = 0x100, kLog = 64;
unsigned char g_object[kBuf], g_sprite[kBuf], g_descriptor[0x44];
const unsigned char* g_scripts[256];
short g_vectors[256 * 3];   // the descriptor's +0x0C offsets
long g_offset[4];
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
    if (p == g_object) return 1;
    if (p == g_object + 0x80) return 2;
    return Low32(p);
}
// A callee may change what the caller reads again after it.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) g_object[0x80] = static_cast<unsigned char>(h >> 8);
    if (h % 5 == 0) rv::Current<records::SpriteObject>()->direction = static_cast<unsigned char>(h >> 16);
    if (h % 7 == 0) rv::Current<records::SpriteObject>()->step_frames = static_cast<unsigned char>(h >> 24) % 3;
}

unsigned char __cdecl StubStep(unsigned char* context, const unsigned char* script) {
    Record(1, Id(context), Id(script));
    const std::uint32_t h = Hash();
    // A step moves the script position (context +0xA = object +0x8A) and may
    // count a loop (+2 = object +0x82) - which a blocked move puts back. A
    // stand-in that left them alone hid a control dropping the restore.
    SetPos(context, Pos(context) + 1 + (h >> 4) % 7);
    if (h % 2) context[2] = static_cast<unsigned char>(h >> 9);
    // The step may begin a timed move (steps to +0x87) or a wait (+0x81).
    if (h % 3 == 0) g_object[0x87] = static_cast<unsigned char>(h >> 8) % 4;
    if (h % 5 == 0) g_object[0x81] = static_cast<unsigned char>(h >> 12);
    if (h % 7 == 0) g_object[0x80] = static_cast<unsigned char>(h >> 16);
    switch (h >> 28) {
    case 0: case 1: case 2: return 0xFF;
    case 3: case 4: case 5: return rv::Current<records::SpriteObject>()->direction;
    default: return static_cast<unsigned char>(h >> 20);
    }
}
void __cdecl StubPoseWait(unsigned char* c) { Record(2, Id(c)); Disturb(); }
unsigned char __cdecl StubBlocked(unsigned char* o) { Record(3, Id(o)); Disturb(); return static_cast<unsigned char>(Hash() % 2 ? Hash() | 1 : 0); }
void __cdecl StubMove(unsigned char* c, unsigned char d) { Record(4, Id(c), d); Disturb(); }
void __cdecl StubSettle(unsigned char* o, unsigned char d) { Record(5, Id(o), d); }
void __cdecl StubMotion(unsigned char* o) { Record(6, Id(o)); }
long __cdecl StubElevation(long x, long z) { Record(7, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z)); return static_cast<long>(Hash()); }
void __cdecl StubPartyApply(unsigned char* c) { Record(8, Id(c)); Disturb(); }
void __cdecl StubPartyMove(unsigned char* o, unsigned char d) { Record(9, Id(o), d); Disturb(); }
// The attach pair's: the handle's object number (often one of the 30, or a
// party object from 0x1E), the offset, the update, and the GTE - whose matrix
// is a local on both sides, so the recorders log its contents, not where it is.
unsigned char __cdecl StubByHandle(unsigned char h) {
    Record(10, h);
    const std::uint32_t k = Hash();
    return static_cast<unsigned char>(k % 4 ? k % 0x1E : 0x1E + (k >> 8) % 8);
}
void __cdecl StubAttachOffset(long* out, unsigned char i) {
    Record(11, i);
    for (unsigned j = 0; j < 3; ++j) out[j] = static_cast<long>(Hash() * (j + 3));
    Disturb();
}
void __cdecl StubUpdate(unsigned char* o) { Record(12, Id(o)); Disturb(); }
std::uint32_t Sum(const void* p, unsigned n) {
    std::uint32_t h = 0x811C9DC5u;
    for (unsigned i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 0x01000193u;
    return h;
}
void __cdecl StubPush() { Record(13); }
void __cdecl StubPop() { Record(14); }
short* __cdecl StubRotMatrix(const short* a, short* m) {
    Record(15, static_cast<std::uint16_t>(a[0]), static_cast<std::uint16_t>(a[1]) << 16 | static_cast<std::uint16_t>(a[2]));
    for (unsigned i = 0; i < 16; ++i) m[i] = static_cast<short>(Hash() >> i);
    return m;
}
void __cdecl StubTransMatrix(unsigned long* m, const unsigned long* v) {
    Record(16, static_cast<std::uint32_t>(v[0]), static_cast<std::uint32_t>(v[1]), static_cast<std::uint32_t>(v[2]));
    for (unsigned i = 0; i < 3; ++i) m[5 + i] = v[i];
}
void __cdecl StubSetRot(const unsigned long* m) { Record(17, Sum(m, 20)); }
void __cdecl StubSetTrans(const unsigned long* m) { Record(18, Sum(m + 5, 12)); }
void __cdecl StubRotTrans(const short* v, long* out) {
    Record(19, static_cast<std::uint16_t>(v[0]), static_cast<std::uint16_t>(v[1]) << 16 | static_cast<std::uint16_t>(v[2]));
    for (unsigned j = 0; j < 3; ++j) out[j] = static_cast<long>(Hash() * (j + 7));
}
const field_objects::CalleesFieldObjects kStubs = {StubStep, StubPoseWait, StubBlocked, StubMove, StubSettle, StubMotion, StubElevation, StubPartyApply, StubPartyMove,
                        StubByHandle, StubAttachOffset, StubUpdate, StubPush, StubRotMatrix, StubTransMatrix, StubSetRot, StubSetTrans,
                        StubRotTrans, StubPop};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x576B50: return f(&StubStep);
    case 0x518B00: return f(&StubPoseWait);
    case 0x519670: return f(&StubBlocked);
    case 0x578C10: return f(&StubMove);
    case 0x518D10: return f(&StubSettle);
    case 0x518980: return f(&StubMotion);
    case 0x5720C0: return f(&StubElevation);
    case 0x5197F0: return f(&StubPartyApply);
    case 0x5190A0: return f(&StubPartyMove);
    case 0x57C0A0: return f(&StubByHandle);
    case 0x578EB0: return f(&StubAttachOffset);
    case 0x517BF0: return f(&StubUpdate);
    case 0x5A7B90: return f(&StubPush);
    case 0x5A8060: return f(&StubRotMatrix);
    case 0x5A8100: return f(&StubTransMatrix);
    case 0x5A8DE0: return f(&StubSetRot);
    case 0x5A8E00: return f(&StubSetTrans);
    case 0x5A8200: return f(&StubRotTrans);
    case 0x5A7BC0: return f(&StubPop);
    default: bof3::Fatal("field_objects: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The four copies and their calls out, by capstone 2026-09-22; every jump stays inside.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
constexpr Call kUpdateCalls[] = {{0x8a, 0x576b50}, {0x9f, 0x518b00}, {0x108, 0x519670}, {0x1eb, 0x519670},
                                 {0x261, 0x578c10}, {0x283, 0x518d10}, {0x28c, 0x518980}};
constexpr Call kMotionCalls[] = {{0x74, 0x5720c0}, {0xb7, 0x518b00}, {0x106, 0x5197f0}};
constexpr Call kSettleCalls[] = {{0x37, 0x5190a0}};
constexpr Call kFollowCalls[] = {{0xc, 0x57c0a0}, {0xc0, 0x578eb0}, {0xfd, 0x517bf0}};
constexpr Call kOffsetCalls[] = {{0x3, 0x5a7b90}, {0x32, 0x5a8060}, {0x4f, 0x5a8100}, {0x59, 0x5a8de0},
                                 {0x63, 0x5a8e00}, {0xbb, 0x5a8200}, {0xe2, 0x5a7bc0}};
const Clone kClones[] = {
    {"Field_ObjectUpdate", 0x517BF0, 0x29C, kUpdateCalls, 7},
    {"Field_ObjectMotion", 0x518980, 0x136, kMotionCalls, 3},
    {"Party_ApplyRecord", 0x5197F0, 0x93, nullptr, 0},
    {"Field_ObjectSettle", 0x518D10, 0xBE, kSettleCalls, 1},
    {"Field_ObjectFollow", 0x5192A0, 0x10F, kFollowCalls, 3},
    {"MoveCmd_AttachOffset", 0x578EB0, 0xEB, kOffsetCalls, 7},
};
constexpr unsigned kCount = 6;

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(rv::Bytes(&Sprite_Kind2)), sizeof(records::SpriteObject)},
    {Address(rv::Bytes(Sprite_ObjectsExtra)), 8 * 0xA4},
    {Address(MoveScript_PartyRecords), MoveScript_PartyRecords_count},
    {::bof3::Img(SharedScratch_addr), 4},
    {Address(&Field_Request), 1},
    {Address(rv::Bytes(Sprite_Objects)), (Sprite_Objects_count * sizeof(records::SpriteObject))}, // what a handle names
};
constexpr unsigned kRegionBytes = sizeof(records::SpriteObject) + 8 * 0xA4 + MoveScript_PartyRecords_count + 4 + 1 + (Sprite_Objects_count * sizeof(records::SpriteObject));

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char object[kBuf], sprite[kBuf];
    long offset[4];   // MoveCmd_AttachOffset's out
    unsigned char* current;
    unsigned char* active;
    std::uint16_t area;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.object, g_object, kBuf);
    std::memcpy(s.sprite, g_sprite, kBuf);
    std::memcpy(s.offset, g_offset, sizeof s.offset);
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
    std::memcpy(g_offset, s.offset, sizeof g_offset);
    Sprite_Current = s.current;
    Field_ActiveMember = s.active;
    Game_AreaNumber = s.area;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x68E31DA4u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }

using ObjectFn = void (__cdecl*)(unsigned char*);
using SettleFn = void (__cdecl*)(unsigned char*, unsigned char);
using OffsetFn = void (__cdecl*)(long*, unsigned char);

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 20000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("field_objects: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    Capture(saved);
    const unsigned char* const* const scripts = g_scripts;
    std::memcpy(g_descriptor + 0x10, &scripts, sizeof scripts);
    for (unsigned i = 0; i < 256; ++i) g_scripts[i] = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(0x10000u + i * 0x40u));
    const short* const vectors = g_vectors;
    std::memcpy(g_descriptor + 0xC, &vectors, sizeof vectors);
    field_objects::g = kStubs;

    unsigned bad = 0, calls = 0, per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.current = Next() % 5 ? g_sprite : rv::Bytes(&Sprite_Kind2);
        const unsigned slot = Next() % 8;
        input.active = At(Address(rv::Bytes(Sprite_ObjectsExtra)) + slot * 0xA4 + (slot < 7 ? Next() % 3 : 0));
        input.area = static_cast<std::uint16_t>(Next() % Area_Descriptors_count);
        g_seed = Next();
        for (auto& v : g_vectors) v = static_cast<short>(Next());
        Apply(input);
        // The boundaries: a timed move running or not, arriving or not; a speed
        // (never 0 with steps left); the sprite's type byte 0x0A; bit 7 of
        // Sprite_Current[7]; E4's flag; the entry direction repeated.
        records::SpriteObject* const sprite = rv::Current<records::SpriteObject>();
        sprite->step_frames = static_cast<unsigned char>(Next() % 2 ? 0 : Next() % 3);
        g_object[0x87] = static_cast<unsigned char>(Next() % 2 ? 0 : Next() % 4);
        g_object[0x84] = static_cast<unsigned char>(g_object[0x87] ? 1 + Next() % 5 : Next() % 6);
        g_object[0x81] = static_cast<unsigned char>(Next() % 2 ? 0 : Next());
        if (Often()) sprite->type = 0x0A;
        if (Often()) sprite->flags2 &= 0x7F;
        if (Often()) rv::Bytes(sprite)[2] = static_cast<unsigned char>(Next() % 5);
        if (Often()) sprite->script_pos = Next() % 16;
        if (Often()) Scratch()[3] = 0xFF;
        if (Often()) Field_Request = 2;
        const unsigned char direction = Often() ? static_cast<unsigned char>(sprite->direction | (sprite->step_frames ? 8 : 0)) : static_cast<unsigned char>(Next());
        if (k == 2 && Often()) {   // the party record's counts, one or both running
            unsigned char* const r = MoveScript_PartyRecords + (PartySlot() & 7) * 16;
            r[1] = static_cast<unsigned char>(Next() % 3);
            r[2] = static_cast<unsigned char>(Next() % 3);
        }
        Capture(input);
        unsigned char* const saved_descriptor = Area_Descriptors[input.area];
        Area_Descriptors[input.area] = g_descriptor;
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* fn = pass ? nullptr : theirs[k];
            if (pass) {
                const void* const ours[kCount] = {reinterpret_cast<const void*>(&Field_ObjectUpdate), reinterpret_cast<const void*>(&Field_ObjectMotion),
                                                  reinterpret_cast<const void*>(&Party_ApplyRecord), reinterpret_cast<const void*>(&Field_ObjectSettle),
                                                  reinterpret_cast<const void*>(&Field_ObjectFollow), reinterpret_cast<const void*>(&MoveCmd_AttachOffset)};
                fn = ours[k];
            }
            if (k == 3) reinterpret_cast<SettleFn>(const_cast<void*>(fn))(g_object, direction);
            else if (k == 5) reinterpret_cast<OffsetFn>(const_cast<void*>(fn))(g_offset, direction);
            else reinterpret_cast<ObjectFn>(const_cast<void*>(fn))(k == 2 ? g_object + 0x80 : g_object);
            Capture(pass ? our_out : their_out);
        }
        Area_Descriptors[input.area] = saved_descriptor;
        calls += their_out.log_n;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      field_objects self-test MISMATCH: round %u, %s, log %u / %u", round, kClones[k].name,
                      their_out.log_n, our_out.log_n);
    }
    field_objects::g = kOriginalsFieldObjects;
    Apply(saved);
    bof3::Log("shadow      field_objects self-test: %u rounds (%u per function), %u calls to the stand-ins, %u MISMATCHES; "
              "the object, sprite, Sprite_Kind2, party objects and records, scratch bytes, Field_Request and the stand-ins' "
              "log compared", kRounds, per[0], calls, bad);
    if (bad) bof3::Fatal("the field object update differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void FieldObjects_Inject() {
    if (bof3::WantsShadow("field_objects")) {
        void* clones[kCount];
        for (unsigned k = 0; k < kCount; ++k) {
            const Clone& c = kClones[k];
            bof3::CloneCall calls[8];
            if (c.n_calls > 8) bof3::Fatal("field_objects: %s has %d calls", c.name, c.n_calls);
            for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
            clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        }
        SelfTest(clones);
    }
}
