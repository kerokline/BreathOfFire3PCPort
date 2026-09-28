// BOF3X_SHADOW=area_w2b: world 2's areas 85..88 through the area round's
// shared harness (area_harness.h), once at start-up: one area_harness::Run
// per area (Group::area its number, the real descriptor and tables in place,
// the world maps' .data state tables swapped through DataTable).
// docs/area_w2b.md section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA085..088 --clones,
// 2026-09-28), each read against the disassembly; areas 87's and 88's are
// area 45's rows (area_w1b_fuzz.cpp) at their addresses - a capstone compare
// of the pairs, area 88's place hook four bytes longer - and their groups are
// area 45's with each copy's tables.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2b.h"
#include "game/area_w2b_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w2b {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using ah::Mem;
using S = ah::Shape;
using at::WorldMapTables;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define AH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define W2B_C(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
#define W2B_P(name, base, size, shape) \
    {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0, false, shape}
// A clone answering in al.
#define W2B_A(name, base, size, calls, shape) \
    {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::name), 0xFF, false, shape}

constexpr U kActiveMember = 0x9035A4;     // Field_ActiveMember
constexpr U kScriptObject = 0x929E80;     // MoveScript_Object
constexpr U kEffects = 0x7E11E0;          // Effect_Objects: twenty records of 0x80

const ah::Callee kSet40 = {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kClear40 = {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0};

// A party record or one of the first four field objects.
unsigned char* SomeRecord(U h) { return (h & 1) ? ah::PartyOf(static_cast<unsigned char>(h >> 1)) : ah::TaskAt(h >> 1); }

// ===========================================================================
// Area 85
// ===========================================================================

// clones: tools/area_rows.py --unit AREA085 --clones
constexpr ah::CallSite kCalls40F720[] = {{0x1C, 0x573400}};
constexpr ah::CallSite kCalls40F780[] = {{0x1F, 0x533BA0}, {0x26, 0x5891F0}};
constexpr ah::CallSite kCalls40F850[] = {{0x0, 0x589810}, {0x4D, 0x587740}};
constexpr ah::CallSite kCalls40F8C0[] = {{0x0, 0x589810}, {0x4D, 0x587740}};
constexpr ah::CallSite kCalls40F930[] = {{0xE, 0x589590}, {0x15, 0x5891F0}};
constexpr ah::CallSite kCalls40F960[] = {{0x6, 0x454A80}, {0x17, 0x455290}, {0x1E, 0x5891F0}};
constexpr ah::CallSite kCalls40F990[] = {{0x6, 0x454A80}};
constexpr ah::CallSite kCalls40F9B0[] = {{0xE, 0x587740}};
constexpr ah::CallSite kCalls40F9D0[] = {{0x2, 0x40F9E0}};
const ah::Clone kClones85[] = {
    W2B_C(Area85_Kind2Walk, 0x40F720, 0x55, kCalls40F720, S::kHandler),
    W2B_C(Area85_FollowMember2, 0x40F780, 0xA8, kCalls40F780, S::kHandler),
    W2B_P(Area85_SetType7, 0x40F830, 0x17, S::kHandler),
    W2B_C(Area85_SpawnEffect47A, 0x40F850, 0x62, kCalls40F850, S::kHandler),
    W2B_C(Area85_SpawnEffect47B, 0x40F8C0, 0x62, kCalls40F8C0, S::kHandler),
    W2B_C(Area85_SetUpObject, 0x40F930, 0x2F, kCalls40F930, S::kHandler),
    W2B_C(Area85_StartSlotScript, 0x40F960, 0x27, kCalls40F960, S::kHandler),
    W2B_C(Area85_ReleaseSlots, 0x40F990, 0xD, kCalls40F990, S::kHandler),
    W2B_P(Area85_Raise18, 0x40F9A0, 0xB, S::kHandler),
    W2B_C(Area85_Sound201, 0x40F9B0, 0x15, kCalls40F9B0, S::kHandler),
    W2B_C(Area85_Init, 0x40F9D0, 0x9, kCalls40F9D0, S::kInit),
    W2B_P(Area85_ClutShift, 0x40F9E0, 0xA4, S::kCallee),
};
enum : unsigned { k85Walk, k85Follow, k85Type7, k85SpawnA, k85SpawnB, k85SetUp, k85Start, k85Release, k85Raise, k85Sound, k85Init, k85Shift };
static_assert(k85Shift + 1 == AH_COUNT(kClones85), "area 85's seeding indices");

const ah::Callee kCallees85[] = {
    {"MoveCmd_MoveKind2", bof3::addr::MoveCmd_MoveKind2, KeyOf(&::MoveCmd_MoveKind2), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    // a slot of the twenty or none (0xFF), garbage above
    {"Effect_FindFree", bof3::addr::Effect_FindFree, KeyOf(&::Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x13},
    {"0x454A80", kSlotsReleaseFor, kSlotsReleaseFor, 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {"0x455290", kSlotStart, kSlotStart, 2, {kAll, kAll}, ah::Answer::kByte, 0xFF, 0x07},
    {"Area85_ClutShift", 0x40F9E0, 0x40F9E0, 1, {kAll}, ah::Answer::kGarbage, 0, 0},
};

const ah::Region kRegions85[] = {
    {kScriptObject, 4},
    {kActiveMember, 4},
    {kEffects, 20 * 0x80},
    {at::kClutRow6, 0x200},
    {at::kClutRow6Live, 0x200},
};

void Seed85(unsigned k) {
    ah::SetPointer(kScriptObject, SomeRecord(ah::Next()));
    ah::SetPointer(kActiveMember, SomeRecord(ah::Next()));
    unsigned char* const o = Sprite_Current;
    switch (k) {
    case k85Walk:
        // the column's step count at its edges: 0x160000 and a half-cell either side
        if (ah::Often()) SetLong(o + 0x34, static_cast<std::int32_t>(0x160000 + AH_PICK(0, 0x7FFF, 0x8000, 0xFFFF8000u, 0xFFFF7FFFu, 0x7F8000, 0x800000, 0x80000000u)));
        break;
    case k85Follow:
        Mem(at::kScriptVar7)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(6, 6, 5, 7, 0x86) : ah::Next());
        Mem(at::kScriptStep)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0xC, 0xC, 0xB, 0xD, 0x8C) : ah::Next());
        for (unsigned m = 0; m < 3; ++m)
            if (ah::Often()) ah::PartyOf(static_cast<unsigned char>(m))[0x89] = static_cast<unsigned char>(ah::Half() ? 2 : AH_PICK(1, 3, 0x82, 0));
        if (ah::Half()) Field_MemberCount = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 4));
        break;
    case k85SpawnA:
    case k85SpawnB:
        if (ah::Half()) o[0xB] = static_cast<unsigned char>(ah::Next());
        break;
    case k85Sound:
        Field_Request = static_cast<unsigned char>(ah::Often() ? AH_PICK(5, 5, 4, 6, 0x85, 0) : ah::Next());
        break;
    case k85Shift:
    case k85Init:
        // CLUT words with channels at 0, 1, 8, 9, 0x1E, 0x1F and bit 15 either way
        for (U n = 0; n < 0x100; n += 1 + ah::Next() % 8) {
            static const U kC[] = {0, 1, 8, 9, 0x17, 0x18, 0x1E, 0x1F};
            const U w = kC[ah::Next() % 8] | kC[ah::Next() % 8] << 5 | kC[ah::Next() % 8] << 10 | (ah::Half() ? 0x8000 : 0);
            SetWord(Mem(at::kClutRow6 + n * 2), w);
        }
        break;
    default: break;
    }
}
// Area85_ClutShift's delta: the init's -8, each channel edge's step, and
// the 32-bit extremes (an effect record's dword).
void Args85(unsigned k, U* a) {
    if (k == k85Shift)
        a[0] = ah::Often() ? AH_PICK(0xFFFFFFF8u, 0, 1, 0xFFFFFFFFu, 8, 0x1E, 0x1F, 0x20, 0xFFFFFFE1u, 0xFFFFFFE0u, 0x7FFFFFFF, 0x80000000u) : ah::Next();
}
// Area 85's disturbance: MoveScript_Object (handler 0 reads it again after
// the walk), Field_ActiveMember.
void Disturb85(U h) {
    switch ((h >> 16) % 3) {
    case 0: ah::SetPointer(kScriptObject, SomeRecord(h >> 20)); break;
    case 1: ah::SetPointer(kActiveMember, SomeRecord(h >> 20)); break;
    default: break;
    }
}

// ===========================================================================
// Area 86
// ===========================================================================

// clones: tools/area_rows.py --unit AREA086 --clones
constexpr ah::CallSite kCalls40FA90[] = {{0x6E, 0x40FB70}, {0x9D, 0x57C140}, {0xBC, 0x57C160}, {0xC6, 0x587740}, {0xCD, 0x469FE0}};
constexpr ah::CallSite kCalls40FC10[] = {{0xF, 0x579F00}, {0x1A, 0x579F00}};
const ah::Clone kClones86[] = {
    W2B_A(Area86_SwitchHook, 0x40FA90, 0xDB, kCalls40FA90, S::kHook),
    {"Area86_MemberNear", 0x40FB70, 0x92, nullptr, 0, nullptr, 0, nullptr, 0, reinterpret_cast<const void*>(&::Area86_MemberNear), 0xFF, false, S::kCallee},
    W2B_C(Area86_Init, 0x40FC10, 0x23, kCalls40FC10, S::kInit),
    W2B_P(Area87_Init, 0x40FC40, 0x14, S::kInit),
};
enum : unsigned { k86Hook, k86Near, k86Init, k87Init };

// Area86_MemberNear's answer as the hook tests it (signed al): mostly none
// (0xFF, and the sign's edge 0x80, 0xFE), one call in twelve a member (0..2,
// 0x7F) - the hook asks up to nine cells, and every one must be clear for
// the switch to toggle - garbage above.
U NearAnswer(const U*, U answer) {
    static const U kNone[] = {0xFF, 0xFF, 0xFF, 0x80, 0xFE};
    static const U kMember[] = {0, 1, 2, 0x7F};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 12 == 0 ? kMember[(n >> 8) % 4] : kNone[(n >> 8) % 5]);
}
const ah::Callee kCallees86[] = {
    {"Area86_MemberNear", 0x40FB70, 0x40FB70, 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &NearAnswer, nullptr},
    // the index pushed with Flags_Test's answer above its byte
    {"0x57C160", kFlagsToggle, kFlagsToggle, 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"0x469FE0", kSpawnKind4, kSpawnKind4, 1, {kAll}, ah::Answer::kGarbage, 0, 0},
};
const ah::Region kRegions86[] = {{at::kPrevArea, 2}};

// Area86_MemberNear's cell (x, z), drawn by the seed (which plants the party
// records around it: an args hook's writes to memory are lost, the harness
// captures the state before the arguments) and handed over by the args.
U g_near_x, g_near_z;

void Seed86(unsigned k) {
    switch (k) {
    case k86Hook: {
        // the leader facing one switch's way two rounds in three
        const U r = at::kA86Switches + (ah::Next() % 3) * 4;
        if (ah::Often()) Mem(at::kLeaderDir)[0] = Mem(r + 2)[0];
        break;
    }
    case k86Near: {
        g_near_x = ah::Often() ? 0x1E + ah::Next() % 0x20 : ah::Next();
        g_near_z = ah::Often() ? 0x14 + ah::Next() % 0x10 : ah::Next();
        // each record's position a step ahead against the cell: inside, at
        // and past the reach ((+0x70) + 2) << 15, on either side
        for (unsigned m = 0; m < 3; ++m) {
            unsigned char* const r = ah::PartyOf(static_cast<unsigned char>(m));
            if (!ah::Often()) continue;
            r[0] = static_cast<unsigned char>(ah::Half() ? 1 : AH_PICK(0, 0x80, 0xFF));
            if (ah::Half()) r[0x70] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFE, 0xFF, 0x10));
            const U reach = (static_cast<U>(r[0x70]) + 2) << 15;
            const auto place = [reach](U cell) {
                const U edge = AH_PICK(0, 1, 2);
                const U off = edge == 0 ? reach - 1 : edge == 1 ? reach : reach + 1;
                const U d = ah::Half() ? off : ah::Next() % (reach + 1);
                return (cell << 16) + (ah::Half() ? d : 0u - d);
            };
            r[9] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0xFF));
            // one axis sometimes far off, so the x test and the z test each decide
            const U fx = ah::Often() ? 0 : 0x800000;
            const U fz = fx != 0 || ah::Often() ? 0 : 0x800000;
            SetLong(r + 0x34, static_cast<std::int32_t>(place(g_near_x) + fx - static_cast<U>(Long(r + 0xC)) * r[9]));
            SetLong(r + 0x38, static_cast<std::int32_t>(place(g_near_z) + fz - static_cast<U>(Long(r + 0x10)) * r[9]));
        }
        if (ah::Half()) Field_MemberCount = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 4, 0x80));
        break;
    }
    case k86Init:
        Cond_ByteFA = static_cast<signed char>(ah::Often() ? AH_PICK(0xC, 0xD, 0xE, 0, 0x80, 0x7F, 0xFF) : ah::Next());
        break;
    case k87Init:
        SetWord(Mem(at::kPrevArea), ah::Often() ? AH_PICK(0x3C, 0x3C, 0x3B, 0x3D, 0x13C, 0x803C) : ah::Next());
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 0x2000);
        break;
    default: break;
    }
}
// A cell hook argument: one of the three switches' (x, z) bytes (two rounds
// in three), bits above them, one off, or anything; MemberNear's the seed's.
void Args86(unsigned k, U* a) {
    if (k == k86Hook) {
        if (ah::Often()) {
            const U r = at::kA86Switches + (ah::Next() % 3) * 4;
            a[0] = Mem(r)[0] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
            a[1] = Mem(r + 1)[0] | (ah::Half() ? 0 : ah::Next() & 0xFFFFFF00u);
            if (ah::Half()) a[ah::Next() & 1] += ah::Half() ? 1 : 0xFFFFFFFFu;
        } else {
            a[0] = ah::Next();
            a[1] = ah::Next();
        }
    } else if (k == k86Near) {
        a[0] = g_near_x;
        a[1] = g_near_z;
    }
}
// Area 86's disturbance: the leader's facing (the hook reads it once, before
// any call) and Field_MemberCount (MemberNear reads it once).
void Disturb86(U h) {
    if ((h >> 16) % 2 == 0) Mem(at::kLeaderDir)[0] = static_cast<unsigned char>(h >> 8);
}

// ===========================================================================
// Areas 87 and 88: the world-map copies (area 45's group, area_w1b_fuzz.cpp)
// ===========================================================================

// --- the fuzz's own memory: a packet buffer, four map items, four names ------

constexpr unsigned kPacketBytes = 0x200, kItemBytes = 0x48;
alignas(16) unsigned char g_packets[kPacketBytes];
alignas(16) unsigned char g_items[4][kItemBytes];
alignas(16) unsigned char g_names[4][16];

bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }

void Advance(U size) {
    U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x50)) next = Key(g_packets) + (ah::Noise() % 4) * 4;
    Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
void Scribble(U at, unsigned n) {
    if (!InPackets(at, n)) return;
    unsigned char* const p = Mem(at);
    for (unsigned i = 0; i < n; i += 4) SetLong(p + i, static_cast<std::int32_t>(ah::Noise()));
}

// --- the effects: the callees that write what the caller reads after ---------

U CommitEffect(const U* a, U answer) { Advance(a[1] & 0xFF); return answer; }
U LinkEffect(const U* a, U answer) {
    if (ah::Noise() % 5) Advance(a[3] & 0xFF);
    return answer;
}
U ByteAtEffect(const U*, U answer) {
    static const U kCells[] = {0xA1, 0xA1, 0xA0, 0xAE, 0xA2, 0x9F, 0, 0x21};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 5 == 0 ? (n >> 8) & 0xFF : kCells[(n >> 4) % 8]);
}
U NameEffect(const U*, U answer) { return Key(g_names[(answer >> 4) % 4]); }
U ItemEffect(const U*, U answer) { return answer % 3 == 0 ? 0u : Key(g_items[(answer >> 4) % 4]); }
U PolyEffect(const U* a, U answer) {
    Scribble(a[0], 0x48);
    if (InPackets(a[0], 8)) Mem(a[0])[7] = 0x2C;
    return answer;
}
U SprtEffect(const U* a, U answer) { Scribble(a[0], 0x20); return answer; }
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U ShadeEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 1 : Mem(a[0])[7] & 0xFE);
    return answer;
}
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 2 : Mem(a[0])[7] & 0xFD);
    return answer;
}
U PersEffect(const U* a, U answer) {
    for (unsigned c = 4; c < 8; ++c) Scribble(a[c], 8);
    return answer;
}
U DepthEffect(const U* a, U answer) {
    for (U z = 0x10; z <= 0x40; z += 0x10) Scribble(a[0] + z, 4);
    return answer;
}
U TextureEffect(const U* a, U answer) {
    if (InPackets(a[1], 0x18)) Mem(a[1])[0x16] = static_cast<unsigned char>(a[0]);
    return answer;
}

const ah::Callee kPers4 = {"Gte_RotTransPers4", bof3::addr::Gte_RotTransPers4, KeyOf(&::Gte_RotTransPers4), 10,
                           {kAll, kAll, kAll, kAll, kAll, kAll, kAll, kAll, 0, 0}, ah::Answer::kGarbage, 0, 0,
                           {8, 8, 8, 8}, &PersEffect, nullptr};

// --- the clones --------------------------------------------------------------

// clones: tools/area_rows.py --unit AREA087 --clones
constexpr ah::CallSite kCalls40FC60[] = {{0x22, 0x57C7A0}, {0x3B, 0x57C7C0}, {0x4F, 0x536700}, {0x8E, 0x4976D0}, {0x113, 0x591680}, {0x154, 0x4976D0}};
constexpr ah::CallSite kCalls40FDE0[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls40FEC0[] = {{0xE, 0x589590}};
constexpr ah::CallSite kCalls40FF20[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls410070[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls4100C0[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls410120[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls410190[] = {{0x0, 0x4101A0}, {0x5, 0x410270}};
constexpr ah::CallSite kCalls4101C0[] = {{0x1C, 0x4101F0}};
constexpr ah::CallSite kCalls4101F0[] = {{0x1F, 0x4103D0}};
constexpr ah::CallSite kCalls410220[] = {{0x38, 0x4103D0}};
constexpr ah::CallSite kCalls410290[] = {{0x59, 0x410660}};
constexpr ah::CallSite kCalls410300[] = {{0x65, 0x410660}};
constexpr ah::CallSite kCalls410370[] = {{0x4E, 0x410660}};
constexpr ah::CallSite kCalls4103D0[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x4105A0}, {0x51, 0x536700}, {0x7B, 0x4105A0}, {0xDC, 0x4105A0}, {0xF8, 0x4105A0}, {0x15B, 0x4105A0}, {0x173, 0x531920}, {0x1A8, 0x4105A0}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls4105A0[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls410660[] = {{0x17, 0x4105A0}, {0x26, 0x4105A0}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls4106E0[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls410860[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls410920[] = {{0xB7, 0x5A75D0}, {0xBF, 0x5A77A0}, {0x199, 0x5A85F0}, {0x19F, 0x5A9290}, {0x1AF, 0x572A00}, {0x1BB, 0x461E50}, {0x21D, 0x572F70}, {0x236, 0x5A75D0}, {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};

const ah::Clone kClones87[] = {
    W2B_C(Area87_PlaceMessage, 0x40FC60, 0x174, kCalls40FC60, S::kState),
    W2B_C(Area87_PlateRun, 0x40FDE0, 0xD6, kCalls40FDE0, S::kState),
    W2B_C(Area87_PlateStart, 0x40FEC0, 0x51, kCalls40FEC0, S::kState),
    W2B_C(Area87_PlateShow, 0x40FF20, 0x142, kCalls40FF20, S::kState),
    W2B_C(Area87_PlateGrow, 0x410070, 0x41, kCalls410070, S::kState),
    W2B_C(Area87_PlateHold, 0x4100C0, 0x58, kCalls4100C0, S::kState),
    W2B_C(Area87_PlateShrink, 0x410120, 0x50, kCalls410120, S::kState),
    W2B_P(Area87_HudRun, 0x410170, 0x12, S::kState),
    W2B_C(Area87_HudFrame, 0x410190, 0xA, kCalls410190, S::kState),
    W2B_P(Area87_FrameStep, 0x4101A0, 0x12, S::kCallee),
    W2B_C(Area87_FrameSlideIn, 0x4101C0, 0x21, kCalls4101C0, S::kState),
    W2B_C(Area87_FrameHold, 0x4101F0, 0x28, kCalls4101F0, S::kState),
    W2B_C(Area87_FrameSlideOut, 0x410220, 0x41, kCalls410220, S::kState),
    W2B_P(Area87_BoxStep, 0x410270, 0x12, S::kCallee),
    W2B_C(Area87_BoxSlideIn, 0x410290, 0x62, kCalls410290, S::kState),
    W2B_C(Area87_BoxHold, 0x410300, 0x6E, kCalls410300, S::kState),
    W2B_C(Area87_BoxSlideOut, 0x410370, 0x57, kCalls410370, S::kState),
    W2B_C(Area87_DrawFrame, 0x4103D0, 0x1C5, kCalls4103D0, S::kCallee),
    W2B_C(Area87_DrawSprite, 0x4105A0, 0xBC, kCalls4105A0, S::kCallee),
    W2B_C(Area87_DrawHud, 0x410660, 0x58, kCalls410660, S::kCallee),
    W2B_P(Area87_Record8Run, 0x4106C0, 0x12, S::kState),
    W2B_C(Area87_Record8Place, 0x4106E0, 0x154, kCalls4106E0, S::kState),
    W2B_P(Area87_Record4Run, 0x410840, 0x12, S::kState),
    W2B_C(Area87_Record4MarkCell, 0x410860, 0xB5, kCalls410860, S::kState),
    W2B_C(Area87_DrawDrift, 0x410920, 0x462, kCalls410920, S::kState),
};

// clones: tools/area_rows.py --unit AREA088 --clones
constexpr ah::CallSite kCalls410D90[] = {{0x22, 0x57C7A0}, {0x3B, 0x57C7C0}, {0x4F, 0x536700}, {0x8E, 0x4976D0}, {0x117, 0x591680}, {0x158, 0x4976D0}};
constexpr ah::CallSite kCalls410F10[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls410FF0[] = {{0xE, 0x589590}};
constexpr ah::CallSite kCalls411050[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls4111A0[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls4111F0[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls411250[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls4112E0[] = {{0x0, 0x4112F0}, {0x5, 0x4113F0}};
constexpr ah::CallSite kCalls411340[] = {{0x1C, 0x411370}};
constexpr ah::CallSite kCalls411370[] = {{0x1F, 0x411550}};
constexpr ah::CallSite kCalls4113A0[] = {{0x38, 0x411550}};
constexpr ah::CallSite kCalls411410[] = {{0x59, 0x4117E0}};
constexpr ah::CallSite kCalls411480[] = {{0x65, 0x4117E0}};
constexpr ah::CallSite kCalls4114F0[] = {{0x4E, 0x4117E0}};
constexpr ah::CallSite kCalls411550[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x411720}, {0x51, 0x536700}, {0x7B, 0x411720}, {0xDC, 0x411720}, {0xF8, 0x411720}, {0x15B, 0x411720}, {0x173, 0x531920}, {0x1A8, 0x411720}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls411720[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls4117E0[] = {{0x17, 0x411720}, {0x26, 0x411720}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls411860[] = {{0x2, 0x589590}, {0x133, 0x5891F0}, {0x14F, 0x588F20}};
constexpr ah::CallSite kCalls4119E0[] = {{0x1C, 0x462A90}, {0x26, 0x589590}, {0x9B, 0x5891F0}, {0xB0, 0x589840}};
constexpr ah::CallSite kCalls411AA0[] = {{0xB7, 0x5A75D0}, {0xBF, 0x5A77A0}, {0x199, 0x5A85F0}, {0x19F, 0x5A9290}, {0x1AF, 0x572A00}, {0x1BB, 0x461E50}, {0x21D, 0x572F70}, {0x236, 0x5A75D0}, {0x23E, 0x5A77A0}, {0x246, 0x5A7780}, {0x430, 0x572FA0}};

const ah::Clone kClones88[] = {
    W2B_C(Area88_PlaceMessage, 0x410D90, 0x178, kCalls410D90, S::kState),
    W2B_C(Area88_PlateRun, 0x410F10, 0xD6, kCalls410F10, S::kState),
    W2B_C(Area88_PlateStart, 0x410FF0, 0x51, kCalls410FF0, S::kState),
    W2B_C(Area88_PlateShow, 0x411050, 0x142, kCalls411050, S::kState),
    W2B_C(Area88_PlateGrow, 0x4111A0, 0x41, kCalls4111A0, S::kState),
    W2B_C(Area88_PlateHold, 0x4111F0, 0x58, kCalls4111F0, S::kState),
    W2B_C(Area88_PlateShrink, 0x411250, 0x50, kCalls411250, S::kState),
    W2B_P(Area88_HudRun, 0x4112C0, 0x12, S::kState),
    W2B_C(Area88_HudFrame, 0x4112E0, 0xA, kCalls4112E0, S::kState),
    W2B_P(Area88_FrameStep, 0x4112F0, 0x12, S::kCallee),
    W2B_C(Area88_FrameSlideIn, 0x411340, 0x21, kCalls411340, S::kState),
    W2B_C(Area88_FrameHold, 0x411370, 0x28, kCalls411370, S::kState),
    W2B_C(Area88_FrameSlideOut, 0x4113A0, 0x41, kCalls4113A0, S::kState),
    W2B_P(Area88_BoxStep, 0x4113F0, 0x12, S::kCallee),
    W2B_C(Area88_BoxSlideIn, 0x411410, 0x62, kCalls411410, S::kState),
    W2B_C(Area88_BoxHold, 0x411480, 0x6E, kCalls411480, S::kState),
    W2B_C(Area88_BoxSlideOut, 0x4114F0, 0x57, kCalls4114F0, S::kState),
    W2B_C(Area88_DrawFrame, 0x411550, 0x1C5, kCalls411550, S::kCallee),
    W2B_C(Area88_DrawSprite, 0x411720, 0xBC, kCalls411720, S::kCallee),
    W2B_C(Area88_DrawHud, 0x4117E0, 0x58, kCalls4117E0, S::kCallee),
    W2B_P(Area88_Record8Run, 0x411840, 0x12, S::kState),
    W2B_C(Area88_Record8Place, 0x411860, 0x154, kCalls411860, S::kState),
    W2B_P(Area88_Record4Run, 0x4119C0, 0x12, S::kState),
    W2B_C(Area88_Record4MarkCell, 0x4119E0, 0xB5, kCalls4119E0, S::kState),
    W2B_C(Area88_DrawDrift, 0x411AA0, 0x462, kCalls411AA0, S::kState),
};
enum : unsigned {
    kPlaceMessage, kPlateRun, kPlateStart, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame,
    kFrameStep, kFrameSlideIn, kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame,
    kDrawSprite, kDrawHud, kRecord8Run, kRecord8Place, kRecord4Run, kRecord4MarkCell, kDrawDrift,
};
static_assert(kDrawDrift + 1 == AH_COUNT(kClones87) && kDrawDrift + 1 == AH_COUNT(kClones88), "the copies' seeding indices");

// --- the callees: the standard set's, with what the callers read after -------

#define W2B_WM_CALLEES(nn)                                                                                                        \
    {"Area" #nn "_FrameStep", at::kWm##nn.fn_frame_step, at::kWm##nn.fn_frame_step, 0, {}, ah::Answer::kPhase, 0, 0},          \
    {"Area" #nn "_BoxStep", at::kWm##nn.fn_box_step, at::kWm##nn.fn_box_step, 0, {}, ah::Answer::kPhase, 0, 0},                \
    {"Area" #nn "_DrawFrame", at::kWm##nn.fn_draw_frame, at::kWm##nn.fn_draw_frame, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0}, \
    {"Area" #nn "_DrawSprite", at::kWm##nn.fn_draw_sprite, at::kWm##nn.fn_draw_sprite, 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0}, \
    {"Area" #nn "_DrawHud", at::kWm##nn.fn_draw_hud, at::kWm##nn.fn_draw_hud, 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},     \
    kSet40, kClear40,                                                                                                             \
    {"Item_NamePtr", bof3::addr::Item_NamePtr, KeyOf(&::Item_NamePtr), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &NameEffect, nullptr}, \
    {"WorldMap_PinSprite", bof3::addr::WorldMap_PinSprite, KeyOf(&::WorldMap_PinSprite), 0, {}, ah::Answer::kGarbage, 0, 0},  \
    {"Effect_Release", bof3::addr::Effect_Release, KeyOf(&::Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},              \
    {"Gfx_CommitPrim", bof3::addr::Gfx_CommitPrim, KeyOf(&::Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect, nullptr}, \
    {"WorldMap_DrawNeedle", bof3::addr::WorldMap_DrawNeedle, KeyOf(&::WorldMap_DrawNeedle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0}, \
    {"WorldMap_RecordIndex", bof3::addr::WorldMap_RecordIndex, KeyOf(&::WorldMap_RecordIndex), 0, {}, ah::Answer::kGarbage, 0, 0}, \
    {"Prim_SetTexture", bof3::addr::Prim_SetTexture, KeyOf(&::Prim_SetTexture), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &TextureEffect, nullptr}, \
    {"AreaMap_ByteAt", bof3::addr::AreaMap_ByteAt, KeyOf(&::AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect, nullptr}, \
    {"Field_CellHasEvent", bof3::addr::Field_CellHasEvent, KeyOf(&::Field_CellHasEvent), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0}, \
    {"MapView_ItemHalfAt", bof3::addr::MapView_ItemHalfAt, KeyOf(&::MapView_ItemHalfAt), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ItemEffect, nullptr}, \
    {"MapView_LinkPrimAt", bof3::addr::MapView_LinkPrimAt, KeyOf(&::MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect, nullptr}, \
    {"Gpu_SetPolyFT4", bof3::addr::Gpu_SetPolyFT4, KeyOf(&::Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr}, \
    {"Gpu_SetShadeTex", bof3::addr::Gpu_SetShadeTex, KeyOf(&::Gpu_SetShadeTex), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ShadeEffect, nullptr}, \
    {"Gpu_SetSemiTrans", bof3::addr::Gpu_SetSemiTrans, KeyOf(&::Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect, nullptr}, \
    {"Gpu_SetSprt", bof3::addr::Gpu_SetSprt, KeyOf(&::Gpu_SetSprt), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SprtEffect, nullptr}, \
    {"Gpu_SetDrawMode", bof3::addr::Gpu_SetDrawMode, KeyOf(&::Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect, nullptr}, \
    {"Gte_PrimDepths4_10", bof3::addr::Gte_PrimDepths4_10, KeyOf(&::Gte_PrimDepths4_10), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DepthEffect, nullptr}, \
    kPers4

const ah::Callee kCallees87[] = {W2B_WM_CALLEES(87)};
const ah::Callee kCallees88[] = {W2B_WM_CALLEES(88)};
#undef W2B_WM_CALLEES

const ah::DataTable kTables87[] = {
    {at::kWm87.plate_states, 5}, {at::kWm87.hud_states, 2}, {at::kWm87.frame_states, 4},
    {at::kWm87.box_states, 4},   {at::kWm87.record8_states, 3}, {at::kWm87.record4_states, 2},
};
const ah::DataTable kTables88[] = {
    {at::kWm88.plate_states, 5}, {at::kWm88.hud_states, 2}, {at::kWm88.frame_states, 4},
    {at::kWm88.box_states, 4},   {at::kWm88.record8_states, 3}, {at::kWm88.record4_states, 2},
};

// --- the regions: the harness's plus the copy's tables (only the tables: the
// descriptor data between the cell records and the place rows is left alone)

enum : unsigned { kRegPackets = 3, kRegItems = 7, kRegNames = 8 };
#define W2B_WM_REGIONS(nn, text_bytes, cell_records)                                                     \
    {at::kMapMode, 1}, {0x7E0918, 1}, {0x7E0670, 4}, {0, kPacketBytes}, {0x9037A0, 0x20}, {0x66C7E8, 2}, \
    {at::kButtonMap0, 0x10}, {0, 4 * kItemBytes}, {0, sizeof g_names}, {at::kTextRecords, text_bytes},  \
    {at::kAreaText, 4}, {at::kFlag3A79, 1},                                                              \
    {at::kWm##nn.plate_anims, at::kWm##nn.plate_anims_end - at::kWm##nn.plate_anims},                   \
    {at::kWm##nn.cells, 4 * (cell_records)},                                                             \
    {at::kWm##nn.place_messages, at::kWm##nn.place_messages_end - at::kWm##nn.place_messages},          \
    {at::kWm##nn.name_sets, 3 * at::kWm##nn.set_stride},                                                 \
    {at::kWm##nn.directions, 0x18}, {at::kWm##nn.drift_u, 0xC}
// Area 87's cell region takes its zero record (the fourth); area 88's has
// none, its fourth record is the descriptor's data: its region stops at the
// third and its sentinel is planted there.
ah::Region g_regions87[] = {W2B_WM_REGIONS(87, 0x80, 4)};
ah::Region g_regions88[] = {W2B_WM_REGIONS(88, 0x160, 3)};
#undef W2B_WM_REGIONS

// The exe's own bytes of the tables the fuzz randomises, put back two rounds
// in three (so the seeds see the shipped values most of the time).
struct Saved {
    unsigned char anims[0x40], cells[0x10], messages[0x160], names[0x28], dirs[0x18], drift[0xC];
};
Saved g_saved87, g_saved88;

// The copy the running group is (set before each Run; the harness calls
// the seed and the disturbance synchronously).
const WorldMapTables* g_t = &at::kWm87;
Saved* g_saved = &g_saved87;

unsigned CellRecords() { return g_t == &at::kWm87 ? 4u : 3u; }

void SaveTables(const WorldMapTables& t, Saved& s) {
    std::memcpy(s.anims, Mem(t.plate_anims), t.plate_anims_end - t.plate_anims);
    std::memcpy(s.cells, Mem(t.cells), 0x10);
    std::memcpy(s.messages, Mem(t.place_messages), t.place_messages_end - t.place_messages);
    std::memcpy(s.names, Mem(t.name_sets), 3 * t.set_stride);
    std::memcpy(s.dirs, Mem(t.directions), sizeof s.dirs);
    std::memcpy(s.drift, Mem(t.drift_u), sizeof s.drift);
}

unsigned char* Obj() { return Sprite_Current; }

// The leader's cell words as bytes, and one cell record planted with them
// (the search has no bound): the last record (the sentinel) always, a
// random one when asked.
void PlantCell(bool random_place) {
    const WorldMapTables& t = *g_t;
    unsigned char* const cx = Mem(at::kLeaderCellWordX);
    unsigned char* const cz = Mem(at::kLeaderCellWordZ);
    cx[1] = 0;
    cz[1] = 0;
    const unsigned n = CellRecords();
    unsigned char* const last = Mem(t.cells + (n - 1) * 4);
    last[0] = cx[0];
    last[1] = cz[0];
    // the sentinel's id one of the sets', so a search with stale words (or
    // none again) lands on a record of another set
    last[3] = Mem(t.name_sets + ((cx[0] ^ cz[0]) % 3) * t.set_stride)[0];
    if (random_place) {
        unsigned char* const r = Mem(t.cells + (ah::Next() % n) * 4);
        r[0] = cx[0];
        r[1] = cz[0];
    }
}

void DisturbWm(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 12) {
    case 0: Mem(at::kMapMode)[0] = static_cast<unsigned char>(v % 4 == 0 ? v : v % 3); break;
    case 1: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ (v & 1 ? 4 : 0x1B)); break;
    case 2: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (v & 1 ? 0x100 : 0x4000)); break;
    case 3: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000); break;
    case 4: Mem(at::kMsgState)[0] = static_cast<unsigned char>(v % 3); break;
    case 5: Game_Mode = static_cast<unsigned short>(Game_Mode ^ 1); break;
    case 6: Cond_ByteFA = static_cast<signed char>(v % 16); break;
    case 7: SetWord(Mem(at::kPlace), v); break;
    case 8: Mem(at::kLeaderCellX + (v & 1) * 2)[0] = static_cast<unsigned char>(v); break;
    case 9: Mem(at::kPartySet)[0] = static_cast<unsigned char>(v & 1 ? 0xC : v); break;
    case 10: case 11: Mem(v & 1 ? at::kLeaderCellWordX : at::kLeaderCellWordZ)[0] = static_cast<unsigned char>(h >> 24); break;   // the settle re-plants its record
    default: break;
    }
}
void SettleWm() {
    unsigned char* const o = Obj();
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(false);
}

void SeedWm(unsigned k) {
    const WorldMapTables& t = *g_t;
    const Saved& s = *g_saved;
    unsigned char* const o = Obj();
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(t.plate_anims), s.anims, t.plate_anims_end - t.plate_anims);
    if (ah::Often()) std::memcpy(Mem(t.cells), s.cells, CellRecords() * 4);
    if (ah::Often()) std::memcpy(Mem(t.place_messages), s.messages, t.place_messages_end - t.place_messages);
    if (ah::Often()) std::memcpy(Mem(t.name_sets), s.names, 3 * t.set_stride);
    if (ah::Often()) std::memcpy(Mem(t.directions), s.dirs, sizeof s.dirs);
    if (ah::Often()) std::memcpy(Mem(t.drift_u), s.drift, sizeof s.drift);
    o[1] = static_cast<unsigned char>(o[1] % 5);
    PlantCell(true);
    const unsigned rows = (t.place_messages_end - t.place_messages) / 0x20;
    const unsigned anims = (t.plate_anims_end - t.plate_anims) / 4;
    const auto leave = [o] {
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2) : ah::Next());
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 2, 5, 1, 3));
        Field_ScriptFlags = static_cast<unsigned short>(ah::Often() ? Field_ScriptFlags & ~0x100u : Field_ScriptFlags | 0x100u);
    };
    switch (k) {
    case kPlaceMessage: {
        Mem(at::kMsgState)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        Cond_ByteFA = static_cast<signed char>(ah::Often() ? AH_PICK(1, 2, 15, 0, 0xFF, 0x80, 0x7F, 8) : ah::Next());
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 5, 3));
        // the place in one of the rows, or in none
        if (ah::Often()) SetWord(Mem(t.place_messages + (ah::Next() % rows) * 0x20), Word(Mem(at::kPlace)));
        // the found cell's id one of the three sets', or none
        unsigned char* cell = Mem(t.cells);
        while (!(cell[0] == Mem(at::kLeaderCellWordX)[0] && cell[1] == Mem(at::kLeaderCellWordZ)[0])) cell += 4;
        if (ah::Often()) cell[3] = Mem(t.name_sets + (ah::Next() % 3) * t.set_stride)[0];
        // items held or not, now and then a list ended early
        for (unsigned i = 0; i < 0x74; ++i)
            if (ah::Half()) Mem(at::kItemsHeld + i)[0] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Half())
            Mem(t.name_sets + 1 + (ah::Next() % 3) * t.set_stride + ah::Next() % (t.set_stride - 1))[0] = static_cast<unsigned char>(AH_PICK(0xFF, 0x16, 0x5E));
        break;
    }
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) Mem(at::kLeaderSteps)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
        SetWord(Mem(t.plate_anims + (ah::Next() % anims) * 4), Word(Mem(at::kPlace)));
        break;
    case kPlateGrow:
    case kPlateShrink:
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(1, 1, 2, 0, 0xFF, 0x80));
        if (ah::Half()) SetLong(o + 0x40, static_cast<std::int32_t>(AH_PICK(0, 0xE000, 0x10000, 0xFFFFE000u, 0x7FFFF000)));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 2, 0));
        break;
    case kPlateHold:
        Game_Mode = static_cast<unsigned short>(ah::Often() ? AH_PICK(0, 2, 1, 0x101) : ah::Next());
        if (ah::Often()) o[7] = static_cast<unsigned char>(ah::Half() ? 1 : AH_PICK(0, 2, 3, 4));
        if (ah::Often()) o[0xB] = o[7];
        if (ah::Often()) SetLong(o + 0x18, static_cast<std::int32_t>(Word(Mem(at::kPlace)) | (ah::Half() ? 0 : 0x10000u)));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 5, 2));
        break;
    case kHudRun: o[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFrameStep: o[2] = static_cast<unsigned char>(ah::Next() % 4); break;
    case kBoxStep: o[3] = static_cast<unsigned char>(ah::Next() % 4); break;
    case kRecord8Run: o[1] = static_cast<unsigned char>(ah::Next() % 3); break;
    case kRecord4Run: o[1] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFrameSlideIn:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0, 0xFFFF, 1, 0xFFF0, 0x7FF0, 0x7FFF, 0xFFD0));
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1) : ah::Next());
        break;
    case kFrameHold:
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
        break;
    case kFrameSlideOut:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0xFFE0, 0xFFE1, 0xFFDF, 0x8000, 0x800F, 0x8010, 0x10));
        Mem(at::kMapMode)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
        break;
    case kBoxSlideIn:
        if (ah::Often()) SetWord(o + 0x30, AH_PICK(0xD1, 0xD2, 0xD3, 0xF0, 0x800A, 0x8009, 0));
        leave();
        break;
    case kBoxHold:
        if (ah::Often()) o[0xB] = 0;
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(0x58, 0x59, 0x5A, 0xFF, 0, 0x7F));
        leave();
        break;
    case kBoxSlideOut:
        if (ah::Often()) SetWord(o + 0x30, AH_PICK(0xE5, 0xE6, 0xE7, 0xC8, 0x7FF6, 0x7FF5));
        leave();
        break;
    case kDrawFrame:
    case kDrawHud:
    case kDrawSprite:
        Draw_PassFlags = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 2, 8, 0x10, 0x1B, 4, 0x20, 0xE4) : ah::Next());
        // the button words with a bit of one entry's mask, or none
        if (ah::Often()) SetLong(Mem(at::kButtonMap0), static_cast<std::int32_t>(Word(Mem(t.buttons + (ah::Next() % 6) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) {
            // a word only the eighth entry answers, when it has such bits
            U others = 0;
            for (U e = 0; e < 7; ++e) others |= Word(Mem(t.buttons + e * 4));
            const U only = Word(Mem(t.buttons + 7 * 4)) & ~others;
            if (only != 0) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(only | (ah::Next() & 0xFFFF0000u)));
        } else if (ah::Often()) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(Word(Mem(t.buttons + (ah::Half() ? 6 + ah::Next() % 2 : ah::Next() % 8) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) SetLong(Mem(at::kButtonMap0), 0);
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1000u);
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x4000u);
        if (ah::Half()) Mem(at::kPartySet)[0] = static_cast<unsigned char>(AH_PICK(0xC, 0x8C, 0xB, 0xFF));
        break;
    case kRecord8Place:
        if (ah::Often()) o[8] = static_cast<unsigned char>(ah::Next() % 4);
        if (ah::Often()) o[6] = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0xFF, 1));
        break;
    case kRecord4MarkCell:
        Mem(at::kFlag3A79)[0] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 9, 8, 0x89) : ah::Next());
        if (ah::Often()) Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & ~1u);
        // the map's width so that any cell record lands in the block
        AreaMap_Header[0] = static_cast<unsigned char>(ah::Next() % 0x18);
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Next() % CellRecords());
        break;
    case kDrawDrift: {
        if (ah::Half()) o[2] = 0;
        if (ah::Often()) Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags | 4);
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 3, 2, 3, 0, 1, 4, 5) : ah::Next());
        const unsigned height = Mem(at::kMapHeight)[0];
        if (ah::Half()) SetWord(o + 0x3A, height + AH_PICK(8, 9, 7, 0, 0x7FFF));
        const auto near = [](unsigned char* cell, unsigned centre) {
            SetWord(cell, centre + AH_PICK(0, 25, 26, static_cast<U>(-25), static_cast<U>(-26), 1, 100));
        };
        if (ah::Often()) near(Mem(at::kLeaderCellX), Word(o + 0x36));
        if (ah::Often()) near(Mem(at::kLeaderCellZ), Word(o + 0x3A));
        if (ah::Half()) Frame_Counter = AH_PICK(0, 15, 16, 31, 0x2F, 0xFFFFFFFFu);
        break;
    }
    default: break;
    }
}

// The draws' arguments: x and y whole words (the dial's and the box's
// positions, and anything), the sprite's index a byte with bits above it.
void ArgsWm(unsigned k, U* a) {
    if (k == kDrawFrame || k == kDrawHud || k == kDrawSprite) {
        a[0] = ah::Often() ? AH_PICK(0x10, 0x5C, 0, 0x7FF0) : ah::Next();
        a[1] = ah::Often() ? AH_PICK(0xFFD0, 0xFFFFFFD0u, 0xC8, 0xF0, 0x10) | (ah::Half() ? ah::Next() & 0xFFFF0000u : 0) : ah::Next();
        if (k == kDrawSprite) a[2] = ah::Often() ? AH_PICK(0, 1, 2, 3, 4, 5, 0x15, 0x100, 0x1FF) : ah::Next();
    }
}

void RunWorldMap(int area, const WorldMapTables& t, Saved& saved, const ah::Clone* clones, unsigned n, const ah::Callee* callees,
                 unsigned n_callees, const ah::DataTable* tables, ah::Region* regions, unsigned n_regions) {
    regions[kRegPackets].at = Key(g_packets);
    regions[kRegItems].at = Key(g_items);
    regions[kRegNames].at = Key(g_names);
    g_t = &t;
    g_saved = &saved;
    ah::Group g{"area_w2b", clones, n, callees, n_callees, tables, 6, regions, n_regions, &SeedWm, &DisturbWm, 4000};
    g.settle = &SettleWm;
    g.phase_span = 5;
    g.args = &ArgsWm;
    g.area = area;
    ah::Run(g);
}

// BOF3X_AR2B_AREA=n runs area n's group alone (the controls script's
// shortcut); unset, every area runs.
bool Wants(int area) {
    const char* const only = std::getenv("BOF3X_AR2B_AREA");
    return only == nullptr || *only == 0 || std::atoi(only) == area;
}

}  // namespace

void SelfTest() {
    SaveTables(at::kWm87, g_saved87);
    SaveTables(at::kWm88, g_saved88);

    if (Wants(85)) {
        ah::Group g{"area_w2b", kClones85, AH_COUNT(kClones85), kCallees85, AH_COUNT(kCallees85), nullptr, 0,
                    kRegions85, AH_COUNT(kRegions85), &Seed85, &Disturb85, 4000};
        g.args = &Args85;
        g.area = 85;
        ah::Run(g);
    }
    if (Wants(86)) {
        ah::Group g{"area_w2b", kClones86, AH_COUNT(kClones86), kCallees86, AH_COUNT(kCallees86), nullptr, 0,
                    kRegions86, AH_COUNT(kRegions86), &Seed86, &Disturb86, 6000};
        g.args = &Args86;
        g.area = 86;
        ah::Run(g);
    }
    if (Wants(87))
        RunWorldMap(87, at::kWm87, g_saved87, kClones87, AH_COUNT(kClones87), kCallees87, AH_COUNT(kCallees87), kTables87,
                    g_regions87, AH_COUNT(g_regions87));
    if (Wants(88))
        RunWorldMap(88, at::kWm88, g_saved88, kClones88, AH_COUNT(kClones88), kCallees88, AH_COUNT(kCallees88), kTables88,
                    g_regions88, AH_COUNT(g_regions88));
}

}  // namespace area_w2b
