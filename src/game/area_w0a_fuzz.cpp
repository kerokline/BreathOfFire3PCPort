// BOF3X_SHADOW=area_w0a: world 0's areas 0..5, 7, 8, 10, 12, 13 and 15
// through the area round's shared harness (area_harness.h), once at start-up:
// one Run per area (Group::area its number), the real descriptors and tables
// left in place, area 15's two state tables swapped for recorders.
// docs/area_w0a.md section 3.
//
// The clone rows are tools/area_rows.py --unit AREA<NNN> --clones (2026-09-27)
// with each row read against the disassembly, plus 0x401000 (area 0's choice
// 0), which the tool does not list (docs/area_w0a.md section 2). A shared body
// is cloned once, in the Run of the first area that reaches it.
#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a.h"
#include "game/area_w0a_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w0a {
namespace {

namespace ah = area_harness;
using ah::Shape;
using move_script::SetLong;
using move_script::SetWord;

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename F> const void* Ours(F f) { return reinterpret_cast<const void*>(f); }

constexpr std::uint32_t kSetByte = 0x579F00, kFindFree = 0x589810, kMove = 0x578C10, kPlace = 0x5734F0;

// ---- the clone rows, an area at a time ------------------------------------------

// area 0
constexpr ah::CallSite kCalls401070[] = {{0x1D, 0x57C4C0}};   // Sprite_FaceDirection
constexpr ah::CallSite kCalls4010A0[] = {{0x10, kMove}};
const ah::Clone kArea0[] = {
    {"Area00_ChoiceVars3And6", 0x401000, 0x32, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area00_ChoiceVars3And6), 0, false, Shape::kChoice},
    {"Area00_ChoiceVar3", 0x401040, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area00_ChoiceVar3), 0, false, Shape::kChoice},
    {"Area00_FaceAsActiveMember", 0x401070, 0x24, kCalls401070, 1, nullptr, 0, nullptr, 0, Ours(&::Area00_FaceAsActiveMember), 0, false, Shape::kHandler},
    {"Area00_MoveFacing", 0x4010A0, 0x22, kCalls4010A0, 1, nullptr, 0, nullptr, 0, Ours(&::Area00_MoveFacing), 0, false, Shape::kHandler},
};
// area 1
constexpr ah::CallSite kCalls401100[] = {{0x12, kFindFree}};
const ah::Clone kArea1[] = {
    {"Area01_ActiveMemberToVar6", 0x4010D0, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area01_ActiveMemberToVar6), 0, false, Shape::kHandler},
    {"Area01_SpawnEffectB", 0x401100, 0x4D, kCalls401100, 1, nullptr, 0, nullptr, 0, Ours(&::Area01_SpawnEffectB), 0, false, Shape::kHandler},
};
// area 2
constexpr ah::CallSite kCalls401150[] = {{0x37, 0x57C0F0}};   // Flags_Set
const ah::Clone kArea2[] = {
    {"Area02_ChoiceArmTail", 0x401150, 0x40, kCalls401150, 1, nullptr, 0, nullptr, 0, Ours(&::Area02_ChoiceArmTail), 0, false, Shape::kChoice},
};
// area 3 (and the gap 0x401230, object trigger 22, in its block)
constexpr ah::CallSite kCalls4011F0[] = {{0x9, kSetByte}, {0x17, kSetByte}, {0x25, kSetByte}, {0x33, kSetByte}};
constexpr ah::CallSite kCalls401230[] = {{0x0, 0x57C7C0}};   // ScriptFlags_Set40
const ah::Clone kArea3[] = {
    {"Area03_ChoiceMessage42", 0x401190, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area03_ChoiceMessage42), 0, false, Shape::kChoice},
    {"Area03_ChoiceMessage44", 0x4011C0, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area03_ChoiceMessage44), 0, false, Shape::kChoice},
    {"Area03_SetCells", 0x4011F0, 0x3C, kCalls4011F0, 4, nullptr, 0, nullptr, 0, Ours(&::Area03_SetCells), 0, false, Shape::kHandler},
    {"Area03_Trigger22", 0x401230, 0x16, kCalls401230, 1, nullptr, 0, nullptr, 0, Ours(&::Area03_Trigger22), 0xFF, false, Shape::kCallee},
};
// area 4
constexpr ah::CallSite kCalls401250[] = {{0x37, 0x57C0F0}};   // Flags_Set
const ah::Clone kArea4[] = {
    {"Area04_ChoiceArmTail", 0x401250, 0x40, kCalls401250, 1, nullptr, 0, nullptr, 0, Ours(&::Area04_ChoiceArmTail), 0, false, Shape::kChoice},
};
// area 5 (with 0x401670 / 0x4016A0, in area 8's block, which areas 5, 10 and 15 reach)
const ah::Clone kArea5[] = {
    {"Area05_ChoiceMessageA", 0x401290, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area05_ChoiceMessageA), 0, false, Shape::kChoice},
    {"Area05_ChoiceMessageB", 0x4012B0, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area05_ChoiceMessageB), 0, false, Shape::kChoice},
    {"Area05_ChoiceVar3At64", 0x401670, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area05_ChoiceVar3At64), 0, false, Shape::kChoice},
    {"Area05_ChoiceVar3AtC8", 0x4016A0, 0x28, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area05_ChoiceVar3AtC8), 0, false, Shape::kChoice},
};
// area 7 (with 0x4012D0..0x4012F0, in area 5's block, which areas 7 and 19 reach)
constexpr ah::CallSite kCalls4012D0[] = {{0x2, kPlace}};
constexpr ah::CallSite kCalls4012E0[] = {{0x2, kPlace}};
constexpr ah::CallSite kCalls4012F0[] = {{0x2, kPlace}};
constexpr ah::CallSite kCalls401300[] = {
    {0x6, kSetByte},   {0x11, kSetByte},  {0x1C, kSetByte},  {0x27, kSetByte},  {0x32, kSetByte},  {0x3D, kSetByte},
    {0x4B, kSetByte},  {0x56, kSetByte},  {0x61, kSetByte},  {0x6C, kSetByte},  {0x77, kSetByte},  {0x82, kSetByte},
    {0x90, kSetByte},  {0x9B, kSetByte},  {0xA6, kSetByte},  {0xB1, kSetByte},  {0xBC, kSetByte},  {0xC7, kSetByte},
    {0xD5, kSetByte},  {0xE0, kSetByte},  {0xEB, kSetByte},  {0xF6, kSetByte},  {0x101, kSetByte}, {0x10C, kSetByte},
    {0x11A, kSetByte}, {0x125, kSetByte}, {0x130, kSetByte}, {0x13B, kSetByte}, {0x146, kSetByte}, {0x151, kSetByte},
    {0x15F, kSetByte}, {0x16A, kSetByte}, {0x175, kSetByte}, {0x180, kSetByte}, {0x18B, kSetByte}, {0x196, kSetByte},
    {0x1A4, kSetByte}, {0x1AF, kSetByte}};
const ah::Clone kArea7[] = {
    {"Area07_PlaceKind2At1", 0x4012D0, 0x9, kCalls4012D0, 1, nullptr, 0, nullptr, 0, Ours(&::Area07_PlaceKind2At1), 0, false, Shape::kHandler},
    {"Area07_PlaceKind2At2", 0x4012E0, 0x9, kCalls4012E0, 1, nullptr, 0, nullptr, 0, Ours(&::Area07_PlaceKind2At2), 0, false, Shape::kHandler},
    {"Area07_PlaceKind2At3", 0x4012F0, 0x9, kCalls4012F0, 1, nullptr, 0, nullptr, 0, Ours(&::Area07_PlaceKind2At3), 0, false, Shape::kHandler},
    {"Area07_SetCells", 0x401300, 0x1B8, kCalls401300, 38, nullptr, 0, nullptr, 0, Ours(&::Area07_SetCells), 0, false, Shape::kHandler},
    {"Area07_CameraShiftYLess", 0x4014C0, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area07_CameraShiftYLess), 0, false, Shape::kHandler},
    {"Area07_CameraShiftYMore", 0x4014D0, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area07_CameraShiftYMore), 0, false, Shape::kHandler},
};
// area 8 (with 0x401640, which area 3 reaches too)
constexpr ah::CallSite kCalls401510[] = {{0x6, kSetByte}, {0x11, kSetByte}, {0x1C, kSetByte}};
constexpr ah::CallSite kCalls401540[] = {{0x13, 0x5B9380}, {0x1A, 0x4976D0}};   // Crt_sprintf, Msg_OpenScript
constexpr ah::CallSite kCalls401570[] = {{0x13, 0x5B9380}, {0x1A, 0x4976D0}, {0x2A, 0x587AE0}};   // .., Music_Play
constexpr ah::CallSite kCalls4015B0[] = {{0x0, kFindFree}};
constexpr ah::CallSite kCalls401640[] = {{0x6, kSetByte}, {0x11, kSetByte}, {0x1C, kSetByte}, {0x27, kSetByte}};
const ah::Clone kArea8[] = {
    {"Area08_ChoiceMessageVar3", 0x4014E0, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area08_ChoiceMessageVar3), 0, false, Shape::kChoice},
    {"Area08_SetCells", 0x401510, 0x25, kCalls401510, 3, nullptr, 0, nullptr, 0, Ours(&::Area08_SetCells), 0, false, Shape::kHandler},
    {"Area08_OpenMessage54", 0x401540, 0x2A, kCalls401540, 2, nullptr, 0, nullptr, 0, Ours(&::Area08_OpenMessage54), 0, false, Shape::kHandler},
    {"Area08_OpenMessage56", 0x401570, 0x33, kCalls401570, 3, nullptr, 0, nullptr, 0, Ours(&::Area08_OpenMessage56), 0, false, Shape::kHandler},
    {"Area08_SpawnEffect54", 0x4015B0, 0x4F, kCalls4015B0, 1, nullptr, 0, nullptr, 0, Ours(&::Area08_SpawnEffect54), 0, false, Shape::kHandler},
    {"Area08_CameraShiftXMore", 0x401600, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area08_CameraShiftXMore), 0, false, Shape::kHandler},
    {"Area08_CameraShiftXLess", 0x401610, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area08_CameraShiftXLess), 0, false, Shape::kHandler},
    {"Area08_Object6XMore", 0x401620, 0xB, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area08_Object6XMore), 0, false, Shape::kHandler},
    {"Area08_Object6XLess", 0x401630, 0xB, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area08_Object6XLess), 0, false, Shape::kHandler},
    {"Area08_ClearCells", 0x401640, 0x30, kCalls401640, 4, nullptr, 0, nullptr, 0, Ours(&::Area08_ClearCells), 0, false, Shape::kHandler},
};
// area 10
constexpr ah::CallSite kCalls401710[] = {{0x1, kFindFree}};
const ah::Clone kArea10[] = {
    {"Area10_ChoiceMessageA", 0x4016D0, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area10_ChoiceMessageA), 0, false, Shape::kChoice},
    {"Area10_ChoiceMessageB", 0x4016F0, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area10_ChoiceMessageB), 0, false, Shape::kChoice},
    {"Area10_SpawnEffect1C", 0x401710, 0x40, kCalls401710, 1, nullptr, 0, nullptr, 0, Ours(&::Area10_SpawnEffect1C), 0, false, Shape::kHandler},
};
// area 12
constexpr ah::CallSite kCalls401840[] = {{0x8, 0x57C140}};   // Flags_Test
const ah::Clone kArea12[] = {
    {"Area12_WaitFlagE", 0x401840, 0x20, kCalls401840, 1, nullptr, 0, nullptr, 0, Ours(&::Area12_WaitFlagE), 0, false, Shape::kHandler},
};
// area 13
constexpr ah::CallSite kCalls401890[] = {{0x6, kSetByte}, {0x11, kSetByte}, {0x1C, kSetByte}, {0x27, kSetByte}};
constexpr ah::CallSite kCalls4018E0[] = {{0x4B, kMove}};
constexpr ah::CallSite kCalls401940[] = {{0x45, kMove}};
const ah::Clone kArea13[] = {
    {"Area13_ChoiceMessageVar3", 0x401860, 0x2E, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area13_ChoiceMessageVar3), 0, false, Shape::kChoice},
    {"Area13_ClearCells", 0x401890, 0x30, kCalls401890, 4, nullptr, 0, nullptr, 0, Ours(&::Area13_ClearCells), 0, false, Shape::kHandler},
    {"Area13_SkipUnlessLeader8", 0x4018C0, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area13_SkipUnlessLeader8), 0, false, Shape::kHandler},
    {"Area13_WalkToX", 0x4018E0, 0x54, kCalls4018E0, 1, nullptr, 0, nullptr, 0, Ours(&::Area13_WalkToX), 0, false, Shape::kHandler},
    {"Area13_WalkToZ", 0x401940, 0x4E, kCalls401940, 1, nullptr, 0, nullptr, 0, Ours(&::Area13_WalkToZ), 0, false, Shape::kHandler},
    {"Area13_TurnDirection", 0x401990, 0x11, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area13_TurnDirection), 0, false, Shape::kHandler},
};
// area 15: the two dispatchers jump through Area15_StatesA / B (swapped for
// recorders); the three state handlers are cloned and called directly
constexpr ah::CallSite kCalls401A30[] = {{0xF, 0x5B93D2}, {0x32, 0x589330}};   // Rand, Sprite_EnsureAnimation
constexpr ah::CallSite kCalls401B10[] = {{0xF, 0x5B93D2}, {0x32, 0x589330}};
const ah::Clone kArea15[] = {
    {"Area15_SetZones", 0x4019B0, 0x45, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area15_SetZones), 0, false, Shape::kInit},
    {"Area15_PassFlags1F", 0x401A00, 0x8, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area15_PassFlags1F), 0, false, Shape::kHandler},
    {"Area15_RunStatesA", 0x401A10, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area15_RunStatesA), 0, false, Shape::kHandler},
    {"Area15_StateA0", 0x401A30, 0x61, kCalls401A30, 2, nullptr, 0, nullptr, 0, Ours(&::Area15_StateA0), 0, false, Shape::kState},
    {"Area15_StateCount", 0x401AA0, 0x45, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area15_StateCount), 0, false, Shape::kState},
    {"Area15_RunStatesB", 0x401AF0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, Ours(&::Area15_RunStatesB), 0, false, Shape::kHandler},
    {"Area15_StateB0", 0x401B10, 0x61, kCalls401B10, 2, nullptr, 0, nullptr, 0, Ours(&::Area15_StateB0), 0, false, Shape::kState},
};
const ah::DataTable kTables15[] = {{at::kArea15StatesA, at::kArea15States}, {at::kArea15StatesB, at::kArea15States}};

// ---- the callees the standard set lacks ------------------------------------------

constexpr std::uint32_t kAll = 0xFFFFFFFFu, kU8 = 0xFFu;

// Effect_FindFree answers a slot 0..19 or 0xFF (none), in al: the none a
// quarter of the time, and a slot the record table holds otherwise.
std::uint32_t FindFree(const std::uint32_t*, std::uint32_t answer) {
    const unsigned slot = (answer >> 8) % 4 == 0 ? 0xFFu : (answer >> 10) % at::kEffectCount;
    return (answer & 0xFFFFFF00u) | slot;
}

const ah::Callee kCallees[] = {
    {"MoveCmd_Move", kMove, kMove, 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {"Effect_FindFree", bof3::addr::Effect_FindFree, KeyOf(&::Effect_FindFree), 0, {}, ah::Answer::kByte, 0, 0x13, {}, &FindFree},
    {"Kind2_Place", bof3::addr::Kind2_Place, KeyOf(&::Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {"Crt_sprintf", 0x5B9380, KeyOf(&::Crt_sprintf), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
};

// ---- the state -------------------------------------------------------------------

constexpr std::uint32_t kActiveMember = 0x9035A4, kScriptObject = 0x929E80, kCameraShift = 0x903800,
                        kPassFlags = 0x7E0918;
std::uint32_t g_zones15, g_zones_other;   // area 15's zone list and another area's
unsigned g_other_area;
ah::Region g_regions[9];
unsigned g_region_n;

void BuildRegions() {
    g_zones15 = static_cast<std::uint32_t>(move_script::Long(ah::Mem(at::kZoneLists + 15 * 4)));
    g_zones_other = 0;
    for (unsigned a = 0; a < 200 && !g_zones_other; ++a) {
        const auto p = static_cast<std::uint32_t>(move_script::Long(ah::Mem(at::kZoneLists + a * 4)));
        if (a != 15 && p && (p + 0x18 <= g_zones15 || p >= g_zones15 + 0x18)) {
            g_zones_other = p;
            g_other_area = a;
        }
    }
    if (!g_zones15 || !g_zones_other) bof3::Fatal("area_w0a: no zone list for area 15 or another area");
    const ah::Region r[] = {
        {kActiveMember, 4},                                   // Field_ActiveMember
        {kScriptObject, 4},                                   // MoveScript_Object
        {at::kFlagBank, 4},                                   // the flag bank's pointer
        {kCameraShift, 4},                                    // Camera_ShiftX, Camera_ShiftY
        {at::kByte9398CF, 1},
        {kPassFlags, 1},                                      // Draw_PassFlags
        {at::kEffects, at::kEffectCount * at::kEffectStride}, // Effect_Objects
        {g_zones15, 0x18},                                    // area 15's zone records 0..2
        {g_zones_other, 0x18},
    };
    g_region_n = 0;
    for (const ah::Region& x : r) g_regions[g_region_n++] = x;
}

// A record the pointers the functions follow may name: one of the four party
// objects (Sprite_ObjectsExtra), a field object, or a party record.
unsigned char* Record(std::uint32_t h) {
    switch (h % 3) {
    case 0: return ah::Mem(ah::at::kObjectsExtra + (h >> 2) % 4 * ah::at::kObjectStride);
    case 1: return ah::Object(h >> 2);
    default: return ah::PartyOf(static_cast<unsigned char>(h >> 2));
    }
}

const ah::Clone* g_clones;

// ---- the seeds ---------------------------------------------------------------

void Seed(unsigned k) {
    // every round: the three pointers some function follows, inside the state
    ah::SetPointer(kActiveMember, Record(ah::Next()));
    ah::SetPointer(kScriptObject, Record(ah::Next()));
    ah::SetPointer(at::kFlagBank, ah::Mem(ah::at::kCondFlags + ah::Next() % 0x1C0));
    // the cursor row: the rows a handler tests, its neighbours, the s8 edges
    if (ah::Often()) ah::Mem(at::kCursor)[0] = static_cast<unsigned char>(AH_PICK(0, 1, 0, 1, 2, 3, 0xFF, 0x80, 0x7F));
    unsigned char* const cur = Sprite_Current;
    switch (g_clones[k].base) {
    case 0x4010D0: case 0x4015B0:
        // the difference the index divides: any pointer (none is followed)
        if (ah::Half()) move_script::SetLong(ah::Mem(kActiveMember), static_cast<std::int32_t>(ah::Next()));
        else if (ah::Half())
            ah::SetPointer(kActiveMember, ah::Mem(area_harness::at::kObjects + static_cast<std::uint32_t>(AH_PICK(0, 0xA3, 0xA4, 0xA5, 0x1478))));
        break;
    case 0x401100: {
        // variable 5 around its bound 6, Frame_Counter's low three bits
        if (ah::Often()) ah::Mem(at::kVar5)[0] = static_cast<unsigned char>(AH_PICK(5, 6, 7, 0, 0xFF, 0x80, 0x7F));
        if (ah::Half()) Frame_Counter &= ~7u;
        else if (ah::Half()) Frame_Counter = (Frame_Counter & ~7u) | static_cast<unsigned>(AH_PICK(1, 2, 4));
        break;
    }
    case 0x4018C0:
        if (ah::Half()) Field_State[0x89] = static_cast<unsigned char>(AH_PICK(8, 8, 9, 7, 0x88));
        break;
    case 0x4018E0: {
        // x around 0x1C8000 (the direction's sign, the steps' 0x8000 edges, the wrap)
        const std::uint32_t d = AH_PICK(0, 1, 0xFFFFFFFFu, 0x7FFF, 0x8000, 0x8001, 0xFFFF8001u, 0xFFFF8000u, 0xFFFF7FFFu,
                                        0x7F8000, 0x800000, 0x80000000u, 0x7FFFFFFFu, 0x80000001u, 0x10000, 0xFFFF0000u);
        if (ah::Often()) SetLong(cur + 0x34, static_cast<std::int32_t>(0x1C8000u - d));
        break;
    }
    case 0x401940: {
        const std::uint32_t d = AH_PICK(0, 1, 0xFFFFFFFFu, 0x7FFF, 0x8000, 0x8001, 0xFFFF8001u, 0xFFFF8000u, 0xFFFF7FFFu,
                                        0x7F8000, 0x800000, 0x80000000u, 0x7FFFFFFFu, 0x80000001u, 0x10000, 0xFFFF0000u);
        if (ah::Often()) SetLong(cur + 0x38, static_cast<std::int32_t>(0x20000u - d));
        break;
    }
    case 0x4019B0:
        // the area whose list is read: 15, or another with a list in the state
        if (ah::Half()) Game_AreaNumber = static_cast<unsigned short>(g_other_area);
        break;
    case 0x401A10: case 0x401AF0:
        // a state the two-entry table holds (past it the original jumps into data)
        cur[4] = static_cast<unsigned char>(ah::Next() % at::kArea15States);
        break;
    case 0x401A30: case 0x401B10:
        ah::SetRandHint(ah::Next() & 0xF);
        break;
    case 0x401AA0:
        if (ah::Often()) cur[0xA] = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0xFF, 0x80));
        if (ah::Half()) SetWord(ah::Mem(at::kTimer), AH_PICK(0, 0, 1, 0xFFFF, 0x100));
        break;
    default: break;
    }
}

// A cell of the group's own moved after a call, drawn only from the hash the
// harness gives: the pointers the functions read again after a call, the tail
// bytes, the slot byte, the timer, a byte of the effect records or the zone
// records, the camera shifts.
void Disturb(std::uint32_t h) {
    const std::uint32_t v = h >> 8;
    switch ((h >> 24) % 10) {
    case 0: ah::SetPointer(kActiveMember, Record(v)); break;
    case 1: ah::SetPointer(kScriptObject, Record(v)); break;
    case 2: ah::Mem(at::kTailKind)[0] = static_cast<unsigned char>(v); break;
    case 3: ah::Mem(at::kTailArg)[0] = static_cast<unsigned char>(v); break;
    case 4: ah::Mem(at::kEffectSlot)[0] = static_cast<unsigned char>(v); break;
    case 5: SetWord(ah::Mem(at::kTimer), v & 1 ? 0 : v >> 4); break;
    case 6: ah::Mem(at::kEffects + (v >> 4) % (at::kEffectCount * at::kEffectStride))[0] = static_cast<unsigned char>(v); break;
    case 7: ah::Mem(g_zones15 + (v >> 4) % 0x18)[0] = static_cast<unsigned char>(v); break;
    case 8: SetWord(ah::Mem(kCameraShift + (v & 2)), v >> 4); break;
    default: ah::Mem(at::kByte9398CF)[0] = static_cast<unsigned char>(v); break;
    }
}

// Area03_Trigger22's arguments, as 0x56E020 passes them: the object and the
// story flags (neither read).
void Args(unsigned k, std::uint32_t* a) {
    if (g_clones[k].base == 0x401230) {
        a[0] = Key(ah::Object(a[0]));
        a[1] = at::kStoryFlags;
    }
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables) {
    g_clones = clones;
    ah::Group g{"area_w0a", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                g_regions, g_region_n, &Seed, &Disturb, 6000};
    g.args = &Args;
    g.area = area;
    ah::Run(g);
}

#define AW_N(a) (sizeof a / sizeof a[0])

}  // namespace

void SelfTest() {
    BuildRegions();
    RunArea(0, kArea0, AW_N(kArea0), nullptr, 0);
    RunArea(1, kArea1, AW_N(kArea1), nullptr, 0);
    RunArea(2, kArea2, AW_N(kArea2), nullptr, 0);
    RunArea(3, kArea3, AW_N(kArea3), nullptr, 0);
    RunArea(4, kArea4, AW_N(kArea4), nullptr, 0);
    RunArea(5, kArea5, AW_N(kArea5), nullptr, 0);
    RunArea(7, kArea7, AW_N(kArea7), nullptr, 0);
    RunArea(8, kArea8, AW_N(kArea8), nullptr, 0);
    RunArea(10, kArea10, AW_N(kArea10), nullptr, 0);
    RunArea(12, kArea12, AW_N(kArea12), nullptr, 0);
    RunArea(13, kArea13, AW_N(kArea13), nullptr, 0);
    RunArea(15, kArea15, AW_N(kArea15), kTables15, AW_N(kTables15));
}

#undef AW_N

}  // namespace area_w0a
