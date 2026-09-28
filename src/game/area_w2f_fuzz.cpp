// BOF3X_SHADOW=area_w2f: world 2's areas 108 and 110..113 through the area
// round's shared harness (area_harness.h), once at start-up - one
// area_harness::Run per area, each Group setting its own area number, all
// under the one shadow name. docs/area_w2f.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA108 and
// AREA110..113 (2026-09-28), each row read against the disassembly (every
// start, extent, call site and the five in-function jump tables agree); the
// shapes are the root table each function hangs from (docs/area_w2f.md
// section 1). Area 110's init opens with Capcom's own jmp over eleven nops,
// which bof3::CloneOriginal refuses as an entry that looks patched: its clone
// is the body the jmp reaches, 0x10 bytes on. The group's own callees are
// recorders here like any other callee, so each function is fuzzed alone;
// area 108's fade states and area 112's effect states are swapped for
// recorders (DataTable).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2f.h"
#include "game/area_w2f_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w2f {
namespace {

namespace ah = area_harness;
using S = ah::Shape;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define W2F_CLONE(name, base, size, calls, n, tables, nt, ret, shape) \
    {#name, base, size, calls, n, nullptr, 0, tables, nt, reinterpret_cast<const void*>(&::name), ret, false, S::shape}

// ---- area 108 ----
constexpr ah::CallSite kCalls4168E0[] = {{0x32, 0x57C0F0}};
constexpr ah::CallSite kCalls4169A0[] = {{0xE8, 0x531F90}, {0xF4, 0x57C140}, {0x127, 0x57C140}, {0x135, 0x417510}, {0x165, 0x57C140}, {0x260, 0x531F90},
                                         {0x267, 0x417510}, {0x2C6, 0x531F90}, {0x33D, 0x531F90}, {0x349, 0x57C140}, {0x386, 0x57C7C0}};
constexpr ah::JumpTable kTables4169A0[] = {{0x93, 0x3AC, 5}};
constexpr ah::CallSite kCalls416E60[] = {{0x7, 0x57C140}, {0x36, 0x57C140}};
constexpr ah::CallSite kCalls416EC0[] = {{0x2B, 0x57C0F0}, {0x34, 0x572650}};
constexpr ah::CallSite kCalls416F30[] = {{0xE, 0x454CC0}};
constexpr ah::CallSite kCalls416FB0[] = {{0x55, 0x454D60}};
constexpr ah::CallSite kCalls417060[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls417090[] = {{0x0, 0x589810}, {0x4C, 0x5720C0}};
constexpr ah::CallSite kCalls417100[] = {{0xB, 0x417510}};
constexpr ah::CallSite kCalls417120[] = {{0x46, 0x57C140}, {0x65, 0x57C160}, {0x6F, 0x587740}, {0x77, 0x589810}, {0x92, 0x57C0F0}};
constexpr ah::CallSite kCalls417210[] = {{0x78, 0x5734F0}, {0x11C, 0x5720C0}, {0x122, 0x5725F0}, {0x136, 0x57C7A0},
                                         {0x14C, 0x589810}, {0x189, 0x5720C0}, {0x277, 0x57C0F0}, {0x280, 0x572650}};
constexpr ah::JumpTable kTables417210[] = {{0x1A, 0x2CC, 10}};
constexpr ah::CallSite kCalls417510[] = {{0x21, 0x572620}, {0x2A, 0x579F00}, {0x48, 0x572620}, {0x51, 0x579F00}};
// ---- area 110 ----
// The init's first instruction is a jmp over eleven nops to its body
// (0x417590), which bof3::CloneOriginal refuses as an entry that looks
// patched (E9); the clone is the body, 0x10 bytes on, its call sites 0x10
// less than the tool's rows (0x12, 0x4D, 0x98).
constexpr ah::CallSite kCalls417590[] = {{0x2, 0x5B93D2}, {0x3D, 0x5B93D2}, {0x88, 0x5720C0}};
// ---- area 111 ----
constexpr ah::CallSite kCalls417670[] = {{0x12, 0x57C7C0}};
constexpr ah::CallSite kCalls4176B0[] = {{0x28, 0x4181E0}, {0xBF, 0x4181E0}, {0xC9, 0x587740}};
constexpr ah::CallSite kCalls417790[] = {{0xB2, 0x5792A0}};
constexpr ah::CallSite kCalls4178F0[] = {{0x54, 0x4181E0}, {0xE9, 0x587740}};
constexpr ah::CallSite kCalls4179F0[] = {{0xFC, 0x5792A0}};
constexpr ah::CallSite kCalls417B30[] = {{0xA6, 0x573400}};
constexpr ah::CallSite kCalls417C10[] = {{0x6A, 0x5720C0}};
constexpr ah::CallSite kCalls417CF0[] = {{0x22, 0x531F90}, {0x59, 0x57C7A0}, {0x65, 0x572650}, {0xD6, 0x572650}, {0xE7, 0x579F00},
                                         {0xF8, 0x579F00}, {0x109, 0x579F00}, {0x11A, 0x579F00}, {0x124, 0x587740}, {0x130, 0x57C0F0}};
constexpr ah::JumpTable kTables417CF0[] = {{0x1C, 0x1A8, 8}};
constexpr ah::CallSite kCalls417ED0[] = {{0x10, 0x57C110}, {0x69, 0x418130}, {0xA4, 0x418130}, {0xE0, 0x418130}, {0x11C, 0x418130}, {0x157, 0x418130},
                                         {0x192, 0x418130}, {0x1CE, 0x418130}, {0x20A, 0x418130}, {0x23B, 0x57C140}, {0x247, 0x57C7C0}};
constexpr ah::CallSite kCalls418130[] = {{0xA3, 0x57C7C0}};
constexpr ah::CallSite kCalls4181E0[] = {{0x1E, 0x579F00}, {0x2A, 0x579F00}, {0x3D, 0x579F00}, {0x45, 0x579F00}};
constexpr ah::CallSite kCalls418240[] = {{0x10, 0x57C110}, {0x36, 0x4181E0}, {0x65, 0x57C140}, {0x7D, 0x579F00}, {0x8B, 0x579F00}, {0x99, 0x579F00},
                                         {0xAE, 0x579F00}, {0xBF, 0x579F00}, {0xD0, 0x579F00}, {0xE1, 0x579F00}, {0xF8, 0x579F00}, {0x103, 0x579F00}};
// ---- area 112 ----
constexpr ah::CallSite kCalls418350[] = {{0x8, 0x57C140}, {0x1F, 0x57C140}, {0x53, 0x594E00}, {0x6D, 0x594E00}, {0x8A, 0x594E00}, {0xA7, 0x594E00}};
constexpr ah::CallSite kCalls418400[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls418490[] = {{0x18, 0x57C0F0}, {0x3B, 0x57C110}, {0x47, 0x57C0F0}, {0x55, 0x57C0F0}, {0x63, 0x57C0F0},
                                         {0x6F, 0x57C0F0}, {0x88, 0x57C110}, {0x94, 0x57C110}, {0xA3, 0x57C140}, {0xBF, 0x57C110}};
constexpr ah::JumpTable kTables418490[] = {{0x30, 0xC8, 4}};
constexpr ah::CallSite kCalls418590[] = {{0x19, 0x57C110}, {0x25, 0x57C110}, {0x31, 0x57C110}, {0x3D, 0x57C110}, {0x49, 0x57C110},
                                         {0x55, 0x57C110}, {0x61, 0x57C110}, {0x6D, 0x57C110}, {0x7A, 0x587740}};
constexpr ah::CallSite kCalls418620[] = {{0x10, 0x57C140}, {0x23, 0x57C140}, {0x63, 0x531F90}};
constexpr ah::CallSite kCalls4186A0[] = {{0x43, 0x57C140}, {0x69, 0x57C140}, {0x7B, 0x4187C0}, {0x84, 0x57C7C0}, {0xCC, 0x57C110},
                                         {0xE4, 0x57C0F0}, {0xFA, 0x57C0F0}, {0x104, 0x587740}, {0x10B, 0x469FE0}};
constexpr ah::CallSite kCalls418880[] = {{0x22, 0x4220D0}};
// ---- area 113 ----
constexpr ah::CallSite kCalls4188F0[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls418910[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls418930[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls418960[] = {{0x1F, 0x418A40}, {0x47, 0x587740}, {0x4F, 0x57C7A0}, {0x6A, 0x587B80},
                                         {0x71, 0x587910}, {0x81, 0x587A00}, {0x93, 0x587B90}, {0x98, 0x57C7A0}};
constexpr ah::JumpTable kTables418960[] = {{0x1B, 0xBC, 6}};
constexpr ah::CallSite kCalls418A40[] = {{0x2B, 0x5918E0}, {0x3D, 0x591900}, {0x51, 0x57C140}, {0x65, 0x590BB0}, {0x74, 0x57C0F0}, {0x88, 0x57C140},
                                         {0x9C, 0x590BB0}, {0xAB, 0x57C0F0}, {0xC9, 0x57C140}, {0xE1, 0x590BB0}, {0xF0, 0x57C0F0}, {0x106, 0x57C140},
                                         {0x11A, 0x590BB0}, {0x129, 0x57C0F0}, {0x142, 0x57C140}, {0x156, 0x590BB0}, {0x165, 0x57C0F0}, {0x17E, 0x4976D0}};

const ah::Clone kClones108[] = {
    W2F_CLONE(Area108_ChoiceArmTail5, 0x4168E0, 0x42, kCalls4168E0, AH_N(kCalls4168E0), nullptr, 0, 0, kChoice),
    W2F_CLONE(Area108_ChoiceArmTailA, 0x416930, 0x28, nullptr, 0, nullptr, 0, 0, kChoice),
    W2F_CLONE(Area108_ClearCounter1Bits0, 0x416960, 0x8, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_ClearCounter1Bits2, 0x416970, 0x8, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_ClearCounter1Bits4, 0x416980, 0x8, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_ClearCounter1Bits6, 0x416990, 0x8, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_PlaceScene, 0x4169A0, 0x3C0, kCalls4169A0, AH_N(kCalls4169A0), kTables4169A0, AH_N(kTables4169A0), 0, kHandler),
    W2F_CLONE(Area108_PlaceAtCellA, 0x416D60, 0x71, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_PlaceAtCellB, 0x416DE0, 0x71, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_ScriptByFlags1E, 0x416E60, 0x5E, kCalls416E60, AH_N(kCalls416E60), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_FlagIfEffectState5, 0x416EC0, 0x46, kCalls416EC0, AH_N(kCalls416EC0), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_FadeRun, 0x416F10, 0x12, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_FadeBegin, 0x416F30, 0x7A, kCalls416F30, AH_N(kCalls416F30), nullptr, 0, 0, kState),
    W2F_CLONE(Area108_FadeStep, 0x416FB0, 0xA8, kCalls416FB0, AH_N(kCalls416FB0), nullptr, 0, 0, kState),
    W2F_CLONE(Area108_FindEffectSlot, 0x417060, 0x26, kCalls417060, AH_N(kCalls417060), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_SpawnEffect51, 0x417090, 0x6B, kCalls417090, AH_N(kCalls417090), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_CellsIfRequest5, 0x417100, 0x20, kCalls417100, AH_N(kCalls417100), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area108_CellHook, 0x417120, 0xE1, kCalls417120, AH_N(kCalls417120), nullptr, 0, 0xFF, kHook),
    W2F_CLONE(Area108_TailPlace, 0x417210, 0x2F4, kCalls417210, AH_N(kCalls417210), kTables417210, AH_N(kTables417210), 0, kTail),
    W2F_CLONE(Area108_SetCells, 0x417510, 0x64, kCalls417510, AH_N(kCalls417510), nullptr, 0, 0, kCallee),
};
enum : unsigned {
    k108Arm5, k108ArmA, k108Bits0, k108Bits2, k108Bits4, k108Bits6, k108Scene, k108CellA, k108CellB, k108Script, k108Effect5,
    k108FadeRun, k108FadeBegin, k108FadeStep, k108FindSlot, k108Spawn51, k108Request5, k108Hook, k108Tail, k108SetCells
};
static_assert(k108SetCells + 1 == sizeof kClones108 / sizeof kClones108[0], "area 108's seeding indices");
const ah::Clone kClones110[] = {
    W2F_CLONE(Area110_PlaceRandomObject, 0x417590, 0xBB, kCalls417590, AH_N(kCalls417590), nullptr, 0, 0, kInit),
};
const ah::Clone kClones111[] = {
    W2F_CLONE(Area111_ChoiceMessage2, 0x417650, 0x17, nullptr, 0, nullptr, 0, 0, kChoice),
    W2F_CLONE(Area111_ChoiceStartVar7, 0x417670, 0x26, kCalls417670, AH_N(kCalls417670), nullptr, 0, 0, kChoice),
    W2F_CLONE(Area111_CopyExtra6C, 0x4176A0, 0xB, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_SlideMark, 0x4176B0, 0xD7, kCalls4176B0, AH_N(kCalls4176B0), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_AttachAtLeaderCell, 0x417790, 0xF8, kCalls417790, AH_N(kCalls417790), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_Counter3Bit, 0x417890, 0x13, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_WaitMemberBit, 0x4178B0, 0x3D, nullptr, 0, nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_PlaceMemberInGrid, 0x4178F0, 0xF9, kCalls4178F0, AH_N(kCalls4178F0), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_AttachAhead, 0x4179F0, 0x13E, kCalls4179F0, AH_N(kCalls4179F0), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_SlideAhead, 0x417B30, 0xD9, kCalls417B30, AH_N(kCalls417B30), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_StepToExtra, 0x417C10, 0xD3, kCalls417C10, AH_N(kCalls417C10), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area111_TailGate, 0x417CF0, 0x1E0, kCalls417CF0, AH_N(kCalls417CF0), kTables417CF0, AH_N(kTables417CF0), 0, kTail),
    W2F_CLONE(Area111_ArriveHook, 0x417ED0, 0x25F, kCalls417ED0, AH_N(kCalls417ED0), nullptr, 0, 0xFF, kHook),
    W2F_CLONE(Area111_ArmTailAtLeaderCell, 0x418130, 0xAC, kCalls418130, AH_N(kCalls418130), nullptr, 0, 0, kCallee),
    W2F_CLONE(Area111_MarkCells, 0x4181E0, 0x52, kCalls4181E0, AH_N(kCalls4181E0), nullptr, 0, 0, kCallee),
    W2F_CLONE(Area111_Init, 0x418240, 0x10C, kCalls418240, AH_N(kCalls418240), nullptr, 0, 0, kInit),
};
enum : unsigned {
    k111Message2, k111StartVar7, k111Copy6C, k111SlideMark, k111AttachLeader, k111Counter3, k111WaitBit, k111PlaceMember, k111AttachAhead,
    k111SlideAhead, k111StepExtra, k111Tail, k111Arrive, k111ArmTail, k111MarkCells, k111Init
};
static_assert(k111Init + 1 == sizeof kClones111 / sizeof kClones111[0], "area 111's seeding indices");
const ah::Clone kClones112[] = {
    W2F_CLONE(Area112_ChangeAreaByFlags, 0x418350, 0xB0, kCalls418350, AH_N(kCalls418350), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area112_TalkByMember, 0x418400, 0x8B, kCalls418400, AH_N(kCalls418400), nullptr, 0, 0, kHandler),
    W2F_CLONE(Area112_ChoiceFlags34, 0x418490, 0xD8, kCalls418490, AH_N(kCalls418490), kTables418490, AH_N(kTables418490), 0, kChoice),
    W2F_CLONE(Area112_ChoiceMessage13, 0x418570, 0x17, nullptr, 0, nullptr, 0, 0, kChoice),
    W2F_CLONE(Area112_ChoiceClearFlags, 0x418590, 0x81, kCalls418590, AH_N(kCalls418590), nullptr, 0, 0, kChoice),
    W2F_CLONE(Area112_StepHook, 0x418620, 0x71, kCalls418620, AH_N(kCalls418620), nullptr, 0, 0xFF, kHook),
    W2F_CLONE(Area112_CellHook, 0x4186A0, 0x119, kCalls4186A0, AH_N(kCalls4186A0), nullptr, 0, 0xFF, kHook),
    W2F_CLONE(Area112_MemberNearBoxes, 0x4187C0, 0x9D, nullptr, 0, nullptr, 0, 0xFF, kCallee),
    W2F_CLONE(Area112_EffectRun, 0x418860, 0x12, nullptr, 0, nullptr, 0, 0, kCallee),
    W2F_CLONE(Area112_EffectRing, 0x418880, 0x2B, kCalls418880, AH_N(kCalls418880), nullptr, 0, 0, kState),
};
enum : unsigned { k112Change, k112Talk, k112Flags34, k112Message13, k112ClearFlags, k112Step, k112CellHook, k112NearBoxes, k112EffRun, k112EffRing };
static_assert(k112EffRing + 1 == sizeof kClones112 / sizeof kClones112[0], "area 112's seeding indices");
const ah::Clone kClones113[] = {
    W2F_CLONE(Area113_ChoiceAsk, 0x4188B0, 0x3F, nullptr, 0, nullptr, 0, 0, kChoice),
    W2F_CLONE(Area113_Trigger27, 0x4188F0, 0x16, kCalls4188F0, AH_N(kCalls4188F0), nullptr, 0, 0xFF, kCallee),
    W2F_CLONE(Area113_Trigger39, 0x418910, 0x1D, kCalls418910, AH_N(kCalls418910), nullptr, 0, 0xFF, kCallee),
    W2F_CLONE(Area113_Trigger63, 0x418930, 0x23, kCalls418930, AH_N(kCalls418930), nullptr, 0, 0xFF, kCallee),
    W2F_CLONE(Area113_TailReward, 0x418960, 0xE0, kCalls418960, AH_N(kCalls418960), kTables418960, AH_N(kTables418960), 0, kTail),
    W2F_CLONE(Area113_Reward, 0x418A40, 0x192, kCalls418A40, AH_N(kCalls418A40), nullptr, 0, 0xFF, kCallee),
};
enum : unsigned { k113Ask, k113Trig27, k113Trig39, k113Trig63, k113Tail, k113Reward };
static_assert(k113Reward + 1 == sizeof kClones113 / sizeof kClones113[0], "area 113's seeding indices");
#undef W2F_CLONE
#undef AH_N

const ah::DataTable kTables108[] = {{at::kArea108FadeStates, at::kArea108FadeStateCount}};
const ah::DataTable kTables112[] = {{at::kArea112EffectStates, at::kArea112EffectStateCount}};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }
unsigned char* EffectRecord(unsigned slot) { return ah::Mem(at::kEffectObjects + slot % at::kEffectCount * at::kEffectStride); }
unsigned char* ExtraRecord(unsigned k) { return ah::Mem(at::kSpriteObjectsExtra + k % at::kExtraCount * at::kExtraStride); }
unsigned char* Grid() { return ah::Mem(at::kArea111Grid); }

// Area 108's model records: the fuzz's own buffer, which the round points
// Area108_Model's pointer into (the image's points at the area's .data,
// which no region may hold whole: 127 records of 0x28 bytes). A region.
constexpr unsigned kPointBytes = 0x1440;
unsigned char g_points[kPointBytes];

// A record Field_ActiveMember may name: one of the four extra objects (most
// often: area 111 divides its distance from Sprite_ObjectsExtra), a field
// object, a party record, or the running object itself.
unsigned char* MemberRecord(U v) {
    switch (v % 6) {
    case 0: return ah::Object(v >> 3);
    case 1: return ah::PartyOf(static_cast<unsigned char>(v >> 3));
    case 2: return Sprite_Current;
    default: return ExtraRecord(v >> 3);
    }
}
unsigned char* ScriptRecord(U v) { return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1)); }

// ---- the stand-ins the group lists ----

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only half the time, from
// Noise): after these calls the callers read a cell again - the running
// object after the tint, the effect search, the tests, the elevation and
// the grid marks (areas 108, 111); the active member after Party_DropIn and
// Area108_SetCells (area 108's scene) and after the tint; the script object
// and its position after MoveCmd_Attach and MoveCmd_MoveKind2 (area 111); the
// tail state after Kind2_Place (area 108's tail); the answer byte after
// Flags_Set (area 112's choice); the focus object after ScriptFlags_Clear40
// (area 113's tail). The harness's own disturbance reaches a group cell
// about one call in 24.
void MoveCurrent(U n) { Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3); }
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n);
    return answer;
}
U MovesMember(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kActiveMember, MemberRecord(n >> 4));
    if (n & 2) MoveCurrent(n >> 2);
    return answer;
}
U MovesScriptObject(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 8));
    if (n & 2) MoveCurrent(n >> 4);
    if (n & 4) SetWord(ah::Pointer(at::kScriptObject) + 0xA, n >> 16);
    return answer;
}
U MovesTail(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kTailState) = static_cast<unsigned char>(n >> 8);
    if (n & 2) B(at::kCounter3) = static_cast<unsigned char>(n >> 16);
    return answer;
}
U MovesAnswer(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kChoiceAnswer) = static_cast<unsigned char>(n % 3 == 0 ? n >> 8 : (n >> 8) % 6);
    return answer;
}
U MovesFocus(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) ah::SetPointer(at::kFocusObject, ah::Object(n >> 8));
    return answer;
}
// The elevation answers anything and moves the running object and the
// script object (area 111's handler 8 reads both after it).
U ElevationEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n);
    if (n & 2) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 8));
    return answer;
}

#define W2F_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W2F_THEIRS(name) #name, KeyOf(name), KeyOf(name)
const ah::Callee kCallees[] = {
    // the map: words x, z and a value byte, as the originals pass them
    {W2F_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2F_OURS(AreaMap_SetHeight), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2F_OURS(AreaMap_Elevation), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ElevationEffect},
    // a byte: area 108's scene pushes a dword whose upper bytes are its stack
    {W2F_OURS(Party_DropIn), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesMember},
    {W2F_OURS(Flags_Toggle), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2F_OURS(Flags_Set), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesAnswer},
    // the point three dwords on the caller's stack: its bytes, not its address
    {"RingAt_4220D0", at::kRingAt, at::kRingAt, 1, {0}, ah::Answer::kGarbage, 0, 0, {12}},
    // slots inside the group's four effect records, or none
    {W2F_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &MovesCurrent},
    {W2F_OURS(Sprite_SetTint), 5, {kAll, kU8, kU8, kU8, kU8}, ah::Answer::kFlag, 0, 0, {}, &MovesMember},
    {W2F_OURS(Tint_Release), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W2F_OURS(Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesTail},
    {W2F_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W2F_OURS(ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesFocus},
    {W2F_OURS(MoveCmd_TestFB), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent},
    {W2F_OURS(MoveCmd_Attach), 5, {kAll, kU8, kU8, kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W2F_OURS(MoveCmd_MoveKind2), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesScriptObject},
    {W2F_OURS(Effect_HoldFlag1C), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W2F_OURS(KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    {W2F_OURS(KeyItem_Add), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    {W2F_OURS(Sound_LoadStream), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W2F_OURS(Sound_StreamDone), 0, {}, ah::Answer::kFlag, 0, 0},
    {W2F_THEIRS(Sound_StopMusic), 0, {}, ah::Answer::kGarbage, 0, 0},
    // the group's own, called directly: arguments by what each reads (the
    // grid cell's low words; area 111's handler 1 passes dwords whose upper
    // halves are the original's registers)
    {W2F_OURS(Area108_SetCells), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesMember},
    {W2F_OURS(Area111_MarkCells), 3, {kU16, kU16, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W2F_OURS(Area111_ArmTailAtLeaderCell), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W2F_OURS(Area112_MemberNearBoxes), 0, {}, ah::Answer::kFlag, 0, 0},
    {W2F_OURS(Area113_Reward), 0, {}, ah::Answer::kGarbage, 0, 0},
};
#undef W2F_OURS
#undef W2F_THEIRS

// Beyond the field frame: the twenty effect records, Sprite_Kind2, the active
// member, script object and focus object pointers, MoveScript_PartyRecords
// records 0..1, Field_Kind2Hold, Field_MoveSpeeds[3..4], area 108's model
// record and the fuzz's point buffer, MoveScript_EffectState, the mark, the
// byte 0x9045F4, area 110's cells and weights, area 111's grid (and the four
// bytes after it), its grid start and cell bytes.
ah::Region g_regions[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kSpriteKind2, 0xA4},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFocusObject, 4},
    {at::kPartyRecords, 0x20},
    {at::kKind2Hold, 1},
    {at::kMoveSpeed3, 2},
    {at::kArea108Model, 8},
    {0, kPointBytes},   // g_points (set at start-up)
    {0x66972C, 0x18},   // MoveScript_EffectState
    {at::kAnswerMark, 1},
    {at::kByte9045F4, 1},
    {at::kArea110Cells, 16},
    {at::kArea110Weights, 8},
    {at::kArea111Grid, 0x20},
    {at::kArea111GridStart, at::kArea111GridBytes},
    {at::kArea111CellBytes, 49},
};
constexpr unsigned kPointRegion = 9;

// The image's tables the seeds put back after the random fill (area 110's
// weights two times in three: random weights reach the "none" path).
unsigned char g_weights110[8];
unsigned char g_cells110[16];

// Every round: the pointers the areas follow put back inside the regions.
void Common() {
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
    ah::SetPointer(at::kFocusObject, ah::Object(ah::Next()));
    ah::SetPointer(at::kArea108ModelPoints, g_points + (ah::Next() % 4) * 4);
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 8) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 0x19 : v); break;
    case 1: B(at::kCounter3) = static_cast<unsigned char>(h & 0x100 ? v % 5 : v); break;
    case 2: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 3: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 4: ah::SetPointer(at::kFocusObject, ah::Object(h >> 16)); break;
    case 5: SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? v % 3 : v); break;
    case 6: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 6); break;
    default: B(at::kKind2Hold) = static_cast<unsigned char>(h & 0x100 ? 0 : v); break;
    }
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (tested signed by areas 108 and 112), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 4, 5, 0xFF, 0x80, 0x81, 0x7F));
}
// A 16.16 dword with the high word `high` and any low word.
U At16(U high, U low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }
// A value on or beside v: v, v - 1, v + 1, v with a high bit.
U Beside(U v, U high) {
    switch (ah::Next() % 5) {
    case 0: case 1: return v;
    case 2: return v - 1;
    case 3: return v + 1;
    default: return v | high;
    }
}

// ---- area 108 ----

// The members' high words on and beside the five strips' bounds.
void SeedMemberStrips() {
    for (unsigned m = 1; m < 3; ++m) {
        unsigned char* const r = ah::PartyOf(static_cast<unsigned char>(m));
        if (ah::Often())
            SetWord(r + 0x36, AH_PICK(0x27, 0x28, 0x38, 0x39, 0x21, 0x22, 0x32, 0x33, 0x34, 0x35, 0x1B, 0x1C, 0x1D, 0x1E, 0x8000, 0x7FFF, 0x133));
        if (ah::Often())
            SetWord(r + 0x3A, AH_PICK(0x52, 0x53, 0x54, 0x55, 0x3A, 0x3B, 0x40, 0x41, 0x42, 0x43, 0x8000, 0x7FFF, 0x153));
    }
    if (ah::Next() % 12 == 0) Field_MemberCount = static_cast<unsigned char>(AH_PICK(0, 9, 10));
}
void Seed108(unsigned k) {
    Common();
    switch (k) {
    case k108Arm5: case k108ArmA: SeedAnswer(); break;
    case k108Scene: {
        if (ah::Often()) B(at::kLeaderMember) = static_cast<unsigned char>(ah::Often() ? 2 : AH_PICK(1, 3, 0x82));
        if (ah::Often()) {
            // one of the five places, or one field off it
            const unsigned p = ah::Next() % at::kArea108PlaceCount;
            const unsigned char* const e = ah::Mem(at::kArea108Places + p * 3);
            U x = e[0], z = e[1], d = e[2];
            switch (ah::Next() % 6) {
            case 0: x = Beside(x, 0x100); break;
            case 1: z = Beside(z, 0x100); break;
            case 2: d = (d + 1) & 7; break;
            default: break;
            }
            SetWord(ExtraRecord(1) + 0x36, x);
            SetWord(ExtraRecord(1) + 0x3A, z);
            B(at::kLeaderPose) = static_cast<unsigned char>((ah::Half() ? 0 : ah::Next() & 0xF8) | (d & 7));
        }
        SeedMemberStrips();
        break;
    }
    case k108CellA: case k108CellB:
        if (ah::Often()) ExtraRecord(1)[0x83] = static_cast<unsigned char>(AH_PICK(1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0xA));
        break;
    case k108Effect5: {
        const auto index = static_cast<unsigned char>(ah::Often() ? ah::Next() % 0x18 : ah::Next());
        B(at::kLeaderMember) = index;
        if (index < 0x18 && ah::Half()) B(0x66972C + index) = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0x85));
        break;
    }
    case k108FadeRun: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea108FadeStateCount); break;
    case k108FadeStep: {
        const bool all = ah::Half();
        for (unsigned off = 0x5D; off <= 0x5F; ++off)
            if (all || ah::Often()) Sprite_Current[off] = static_cast<unsigned char>(all ? AH_PICK(0xCF, 0xD0, 0xD0, 0xD0) : AH_PICK(0xCF, 0xD0, 0xD1, 0x7F, 0x80, 0xFF, 0));
        break;
    }
    case k108Request5:
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0x85));
        break;
    case k108Hook:
        // the cells' direction is 7 in the image; the pose compared whole
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(7, 7, 7, 6, 0, 0x17, 0x87));
        break;
    case k108Tail: {
        B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 4, 5, 6, 9, 0, 1, 2, 3, 4, 5, 9, 0xA, 0xFF, 0x80, 7));
        if (ah::Often()) SetWord(ah::Mem(at::kTailTimer), AH_PICK(1, 1, 2, 0, 0x101));
        if (ah::Often()) B(at::kKind2Hold) = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next());
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(AH_PICK(2, 4, 1, 3, 5, 0x82));
        if (ah::Often()) ExtraRecord(1)[0x83] = static_cast<unsigned char>(AH_PICK(9, 8, 0xA, 1, 0x89));
        if (ah::Often()) B(at::kArea108Model) = static_cast<unsigned char>(AH_PICK(0x21, 0, 1, 2, 5, 0x7F, 0x80, 0xFF));
        break;
    }
    default: break;
    }
}
// The cell hook's (x, z): one of its four cells (bytes, with anything above
// them) or beside one, its direction the leader's (a random high nibble a
// quarter of the time); the helper's switch a byte with anything above.
void Args108(unsigned k, U* a) {
    if (k == k108Hook) {
        if (!ah::Often()) return;
        const unsigned i = ah::Next() % at::kArea108HookCellCount;
        const unsigned char* const e = ah::Mem(at::kArea108HookCells + i * 4);
        a[0] = (a[0] & 0xFFFFFF00u) | (ah::Next() % 5 == 0 ? static_cast<unsigned char>(e[0] + 1) : e[0]);
        a[1] = (a[1] & 0xFFFFFF00u) | (ah::Next() % 5 == 0 ? static_cast<unsigned char>(e[1] - 1) : e[1]);
        return;
    }
    if (k == k108SetCells && ah::Often()) a[0] = (a[0] & 0xFFFFFF00u) | AH_PICK(0, 1, 0x80);
}

// ---- area 110 ----
void Seed110(unsigned) {
    Common();
    // the image's weights (they sum to 0x40, so a roll always falls below
    // one), small weights (0..9 each: the walk reaches the eighth weight and
    // runs past it, "none"), or the random fill
    switch (ah::Next() % 3) {
    case 0: std::memcpy(ah::Mem(at::kArea110Weights), g_weights110, sizeof g_weights110); break;
    case 1:
        for (unsigned i = 0; i < 8; ++i) B(at::kArea110Weights + i) = static_cast<unsigned char>(ah::Next() % 10);
        break;
    default: break;
    }
    if (ah::Often()) std::memcpy(ah::Mem(at::kArea110Cells), g_cells110, sizeof g_cells110);
}

// ---- area 111 ----

// A grid of nibbles, each 0 with probability `zero` in 8, else one of the
// values the handlers test (1..4, 0xF) or anything.
void SeedGrid(unsigned zero) {
    for (unsigned i = 0; i < at::kArea111GridBytes; ++i) {
        unsigned char b = 0;
        for (unsigned half = 0; half < 2; ++half) {
            const U n = ah::Next();
            unsigned nib = 0;
            if (n % 8 >= zero) nib = (n >> 3) % 3 == 0 ? (n >> 5) & 0xF : AH_PICK(1, 2, 3, 4, 0xF, 5);
            b = static_cast<unsigned char>(b << 4 | nib);
        }
        Grid()[i] = b;
    }
}
// A grid cell's high word: base + 2 * cell + (0 or 1), cell in lo..hi.
U CellWord(U base, int lo, int hi) {
    const int cell = lo + static_cast<int>(ah::Next() % static_cast<U>(hi - lo + 1));
    return static_cast<U>(static_cast<int>(base) + 2 * cell + static_cast<int>(ah::Next() & 1));
}
// Sprite_Current at a grid cell (lo..hi each way), facing any direction.
void SeedCurrentCell(int lo, int hi) {
    SetWord(Sprite_Current + 0x36, CellWord(0xB, lo, hi));
    SetWord(Sprite_Current + 0x3A, CellWord(0xB1, lo, hi));
    if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(ah::Next() % 8 | (ah::Half() ? 0 : ah::Next() & 0xF8));
}
// The leader at a grid cell.
void SeedLeaderCell(int lo, int hi) {
    SetWord(ah::Mem(at::kLeaderXHigh), CellWord(0xB, lo, hi));
    SetWord(ah::Mem(at::kLeaderZHigh), CellWord(0xB1, lo, hi));
}
// The grid cell's nibble set to `nib`: the byte (col >> 1) + row * 4, low for
// an odd column (the originals' halving toward 0 of a cell >= 0).
void SetNibble(int col, int row, unsigned nib) {
    unsigned char& b = Grid()[(col >> 1) + row * 4];
    b = static_cast<unsigned char>(col & 1 ? (b & 0xF0) | nib : (b & 0x0F) | nib << 4);
}
int LeaderCol() { return (static_cast<std::int16_t>(Word(ah::Mem(at::kLeaderXHigh))) - 0xB) / 2; }
int LeaderRow() { return (static_cast<std::int16_t>(Word(ah::Mem(at::kLeaderZHigh))) - 0xB1) / 2; }

void Seed111(unsigned k) {
    Common();
    switch (k) {
    case k111Message2: case k111StartVar7: SeedAnswer(); break;
    case k111SlideMark:
        SeedGrid(5);
        SeedCurrentCell(-1, 7);
        break;
    case k111AttachLeader:
        SeedGrid(2);
        SeedLeaderCell(ah::Often() ? 0 : -2, ah::Often() ? 6 : 8);
        break;
    case k111Counter3: SetLong(Sprite_Current + 0x18, AH_PICK(0, 1, 2, 3, 4, 7, 8, 31, 32, 33, 0x80000003u)); break;
    case k111WaitBit: {
        const unsigned idx = ah::Next() % 4;
        if (ah::Often()) ah::SetPointer(at::kActiveMember, ExtraRecord(idx));
        if (ah::Often()) B(at::kCounter3) = static_cast<unsigned char>(ah::Often() ? 0x10u << idx : AH_PICK(0x10, 0x20, 0x40, 0x80, 0, 8, 1));
        break;
    }
    case k111PlaceMember:
        // the object's cell inside the grid (the write's row and byte), its x
        // word down to 0xA (-1 halves to 0), the grid holding the member keys
        SeedGrid(3);
        SetWord(Sprite_Current + 0x36, AH_PICK(0xA, 0xB, 0xC) + 2 * (ah::Next() % 7));
        SetWord(Sprite_Current + 0x3A, AH_PICK(0xB0, 0xB1, 0xB2) + 2 * (ah::Next() % 7));
        if (ah::Often()) ah::SetPointer(at::kActiveMember, ExtraRecord(ah::Next()));
        break;
    case k111AttachAhead: {
        SeedGrid(2);
        const unsigned extra = ah::Often() ? ah::Next() % 4 : AH_PICK(4, 5, 0xFF);
        Sprite_Current[0x18] = static_cast<unsigned char>(extra);
        unsigned char* const e = ah::Mem(at::kSpriteObjectsExtra + extra * at::kExtraStride);
        if (extra < 4) {
            SetWord(e + 0x36, CellWord(0xB, -1, 7));
            SetWord(e + 0x3A, CellWord(0xB1, -1, 7));
        }
        if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(ah::Next() % 8 | (ah::Half() ? 0 : ah::Next() & 0xF8));
        break;
    }
    case k111SlideAhead:
        SeedGrid(3);
        SeedCurrentCell(-1, 7);
        break;
    case k111StepExtra:
        SetLong(Sprite_Current + 0x18, ah::Often() ? ah::Next() % 4 : AH_PICK(4, 5, 0x10));
        if (ah::Often()) ah::Pointer(at::kScriptObject)[4] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next());
        break;
    case k111Tail:
        B(at::kTailState) = static_cast<unsigned char>(
            AH_PICK(1, 3, 5, 7, 0xA, 0x14, 0x15, 0x16, 0x17, 0x18, 0xA, 0x15, 0x16, 0x17, 0x18, 0x16, 0, 2, 0x19, 0xFF, 0x80));
        if (ah::Often()) SetWord(ah::Mem(at::kTailTimer), AH_PICK(1, 1, 2, 0, 0x101));
        if (ah::Often()) B(at::kKind2Hold) = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next());
        break;
    case k111Arrive:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 1, 1, 0, 2, 5, 0x81));
        if (ah::Often()) Sprite_Current[8] = static_cast<unsigned char>(ah::Next() % 8 | (ah::Half() ? 0 : ah::Next() & 0xF8));
        break;
    case k111ArmTail: {
        // the leader inside the grid on a block (1..4) - a cell of 0 or 5..0xF
        // makes the original write an extra record's +8 outside the regions
        SeedLeaderCell(0, 6);
        SetNibble(LeaderCol(), LeaderRow(), 1 + ah::Next() % 4);
        break;
    }
    case k111Init:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(1, 1, 0, 5, 2, 0x81));
        break;
    default: break;
    }
}
// The arrive hook's (x, z) in half cells (>> 15): on and beside the edges
// and gates each way, the final cell's; the helpers' arguments.
void Args111(unsigned k, U* a) {
    if (k == k111Arrive) {
        // one of the eight edges (the coordinate on it, or one beside; the
        // other inside its span, or one past either end), the final cell's
        // high words, or anything
        struct EdgeCase {
            bool on_x;
            U at, lo, span;
        };
        static const EdgeCase kEdges[] = {{true, 0x16, 0x162, 0x1B}, {true, 0x30, 0x162, 0x1B}, {false, 0x162, 0x16, 0x1B},
                                          {false, 0x17C, 0x16, 0x1B}, {true, 0x20, 0x16E, 3},     {true, 0x26, 0x16E, 3},
                                          {false, 0x16C, 0x22, 3},    {false, 0x172, 0x22, 3}};
        const unsigned pick = ah::Next() % 11;
        if (pick < 8) {
            const EdgeCase& e = kEdges[pick];
            const U on = ah::Next() % 4 == 0 ? e.at + (ah::Half() ? 1 : static_cast<U>(-1)) : e.at;
            const unsigned r = ah::Next() % 6;
            const U other = r == 0 ? e.lo - 1 : r == 1 ? e.lo + e.span : e.lo + ah::Next() % e.span;
            a[0] = (e.on_x ? on : other) << 15 | (ah::Next() & 0x7FFF);
            a[1] = (e.on_x ? other : on) << 15 | (ah::Next() & 0x7FFF);
        } else if (pick < 10) {
            a[0] = At16(AH_PICK(0x11, 0x12, 0x11, 0x12, 0x10, 0x13, 0x111), a[0]);
            a[1] = At16(AH_PICK(0xB7, 0xB8, 0xB7, 0xB8, 0xB6, 0xB9, 0x1B7), a[1]);
        }
        return;
    }
    if (k == k111ArmTail) {
        a[0] = (a[0] & 0xFFFFFF00u) | AH_PICK(1, 3, 5, 7, 0, 0xFF);
        return;
    }
    if (k == k111MarkCells) {
        if (ah::Often()) a[0] = static_cast<U>(static_cast<int>(ah::Next() % 9) - 1);
        if (ah::Often()) a[1] = static_cast<U>(static_cast<int>(ah::Next() % 9) - 1);
    }
}

// ---- area 112 ----

// Each member's next position near one of the four boxes (the high word the
// centre, the half size either side of it, or anything), its frames 0..2.
void SeedNearBoxes() {
    for (unsigned m = 0; m < 3; ++m) {
        unsigned char* const r = ah::PartyOf(static_cast<unsigned char>(m));
        if (!ah::Often()) continue;
        const unsigned char* const box = ah::Mem(at::kArea112Boxes + (ah::Next() % at::kArea112BoxCount) * 4);
        r[9] = static_cast<unsigned char>(ah::Next() % 3);
        SetLong(r + 0xC, static_cast<std::int32_t>(ah::Next() % 5 == 0 ? ah::Next() : (ah::Next() & 0x1FFFF) - 0x10000));
        SetLong(r + 0x10, static_cast<std::int32_t>(ah::Next() % 5 == 0 ? ah::Next() : (ah::Next() & 0x1FFFF) - 0x10000));
        const int dx = static_cast<int>(AH_PICK(0, 1, 2, 0xFF, 0xFE)) - (ah::Half() ? box[2] : 0);
        const int dz = static_cast<int>(AH_PICK(0, 1, 2, 0xFF, 0xFE)) - (ah::Half() ? box[3] : 0);
        SetLong(r + 0x34, static_cast<std::int32_t>(At16(static_cast<U>(box[0] + static_cast<signed char>(dx)), ah::Next())));
        SetLong(r + 0x38, static_cast<std::int32_t>(At16(static_cast<U>(box[1] + static_cast<signed char>(dz)), ah::Next())));
    }
}
void Seed112(unsigned k) {
    Common();
    switch (k) {
    case k112Talk:
        for (unsigned m = 0; m < 3; ++m)
            if (ah::Often()) ah::PartyOf(static_cast<unsigned char>(m))[0x89] = static_cast<unsigned char>(AH_PICK(5, 8, 4, 2, 3, 6, 0x85));
        if (ah::Next() % 10 == 0) Field_MemberCount = 0;
        break;
    case k112Flags34: case k112Message13: case k112ClearFlags: SeedAnswer(); break;
    case k112Step:
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(3, 3, 3, 2, 4, 0x83));
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(0, 7, 6, 0, 7, 6, 1, 5, 8, 0x80, 3));
        break;
    case k112CellHook:
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(3, 0, 1, 2, 4, 7, 0x83));
        break;
    case k112NearBoxes:
        SeedNearBoxes();
        if (ah::Next() % 10 == 0) Field_MemberCount = static_cast<unsigned char>(AH_PICK(0, 4));
        break;
    case k112EffRun:
        Sprite_Current = EffectRecord(ah::Next() % 4);
        Sprite_Current[1] = static_cast<unsigned char>(ah::Next() % at::kArea112EffectStateCount);
        break;
    case k112EffRing: Sprite_Current = EffectRecord(ah::Next() % 4); break;
    default: break;
    }
}
// The step hook's (x, z): each high word on or beside its three; the cell
// hook's one of its four cells (bytes, anything above) or beside one.
void Args112(unsigned k, U* a) {
    if (k == k112Step) {
        if (!ah::Often()) return;
        a[0] = At16(AH_PICK(0xB, 0xC, 0xD, 0xA, 0xE, 0x10B, 0xB), a[0]);
        a[1] = At16(AH_PICK(0x57, 0x58, 0x59, 0x56, 0x5A, 0x157, 0x57), a[1]);
        return;
    }
    if (k == k112CellHook) {
        if (!ah::Often()) return;
        const unsigned i = ah::Next() % at::kArea112HookCellCount;
        const unsigned char* const e = ah::Mem(at::kArea112HookCells + i * 4);
        a[0] = (a[0] & 0xFFFFFF00u) | (ah::Next() % 5 == 0 ? static_cast<unsigned char>(e[0] + 1) : e[0]);
        a[1] = (a[1] & 0xFFFFFF00u) | (ah::Next() % 5 == 0 ? static_cast<unsigned char>(e[1] - 1) : e[1]);
    }
}

// ---- area 113 ----
void Seed113(unsigned k) {
    Common();
    switch (k) {
    case k113Ask:
        SeedAnswer();
        if (ah::Often()) B(at::kByte9045F4) = static_cast<unsigned char>(AH_PICK(5, 6, 7, 0, 0xFF, 0x86));
        break;
    case k113Tail:
        B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 5, 0xA, 0xB, 0, 1, 5, 0xA, 0xB, 2, 4, 6, 9, 0xC, 0xFF, 0x80));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 0, 3, 0x82, 1));
        break;
    case k113Reward:
        if (ah::Often()) B(at::kByte9045F4) = static_cast<unsigned char>(AH_PICK(0xA, 0xB, 0xC, 0xA, 0xB, 0xC, 9, 0xD, 0));
        break;
    default: break;
    }
}
// An object trigger is called (a field object, 0x904030).
void Args113(unsigned k, U* a) {
    if (k == k113Trig27 || k == k113Trig39 || k == k113Trig63) {
        a[0] = Key(ah::Object(a[0]));
        a[1] = at::kStoryFlags;
    }
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables, void (*seed)(unsigned),
             void (*args)(unsigned, U*), unsigned rounds) {
    ah::Group g{"area_w2f", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                g_regions, sizeof g_regions / sizeof g_regions[0], seed, &Disturb, rounds};
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    g_regions[kPointRegion].at = Key(g_points);
    std::memcpy(g_weights110, ah::Mem(at::kArea110Weights), sizeof g_weights110);
    std::memcpy(g_cells110, ah::Mem(at::kArea110Cells), sizeof g_cells110);
    constexpr unsigned kRounds = 8000;
    RunArea(108, kClones108, sizeof kClones108 / sizeof kClones108[0], kTables108, sizeof kTables108 / sizeof kTables108[0], &Seed108, &Args108,
            kRounds);
    RunArea(110, kClones110, sizeof kClones110 / sizeof kClones110[0], nullptr, 0, &Seed110, nullptr, kRounds);
    RunArea(111, kClones111, sizeof kClones111 / sizeof kClones111[0], nullptr, 0, &Seed111, &Args111, kRounds);
    RunArea(112, kClones112, sizeof kClones112 / sizeof kClones112[0], kTables112, sizeof kTables112 / sizeof kTables112[0], &Seed112, &Args112,
            kRounds);
    RunArea(113, kClones113, sizeof kClones113 / sizeof kClones113[0], nullptr, 0, &Seed113, &Args113, kRounds);
}

}  // namespace area_w2f
