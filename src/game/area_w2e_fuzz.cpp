// BOF3X_SHADOW=area_w2e: world 2's areas 104..106 through the area round's
// shared harness (area_harness.h), once at start-up: area 104 in two Runs (its
// world-map copy, then its own code), areas 105 and 106 one each - every Group
// setting its area number, all under the one shadow name. docs/area_w2e.md
// section "The fuzz".
//
// The clone rows are tools/area_rows.py's (--unit AREA104..106 --clones,
// 2026-09-28), each read against the disassembly (every start, extent, call
// site and the three in-function jump tables agree); the shapes are the root
// each function hangs from (docs/area_w2e.md section 1). Area 104's world-map
// rows are area 87's (area_w2b_fuzz.cpp) at area 104's addresses, and its
// group is area 87's over area 104's tables. The group's own functions called
// directly are recorders here like any other callee, so each is fuzzed alone;
// area 104's six .data state tables are swapped for recorders (DataTable).
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w2e.h"
#include "game/area_w2e_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w2e {
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
unsigned char& B(U address) { return *Mem(address); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define AH_COUNT(a) static_cast<unsigned>(sizeof a / sizeof a[0])
#define OURS(f) reinterpret_cast<const void*>(&::f)
// A clone with its calls; one without; one answering in al.
#define W2E_C(name, base, size, calls, shape) {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, OURS(name), 0, false, shape}
#define W2E_P(name, base, size, shape) {#name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, OURS(name), 0, false, shape}
#define W2E_A(name, base, size, calls, shape) {#name, base, size, calls, AH_N(calls), nullptr, 0, nullptr, 0, OURS(name), 0xFF, false, shape}
#define W2E_T(name, base, size, calls, tables, shape, mask) {#name, base, size, calls, AH_N(calls), nullptr, 0, tables, AH_N(tables), OURS(name), mask, false, shape}

// clones: tools/area_rows.py --unit AREA104..106 --clones (verbatim)
constexpr ah::CallSite kCalls4146C0[] = {{0x12, 0x5918E0}, {0x30, 0x416020}};
constexpr ah::CallSite kCalls414700[] = {{0x18, 0x57C7A0}, {0x33, 0x57C7C0}, {0x3A, 0x4976D0}};
constexpr ah::CallSite kCalls414760[] = {{0x55, 0x536700}, {0x6E, 0x536700}, {0x88, 0x536700}};
constexpr ah::CallSite kCalls414840[] = {{0x70, 0x5891F0}, {0xB2, 0x5891F0}, {0xF0, 0x5891F0}, {0x12F, 0x5891F0}};
constexpr ah::CallSite kCalls414990[] = {{0x0, 0x4112A0}, {0x3C, 0x5890E0}};
constexpr ah::CallSite kCalls4149E0[] = {{0x0, 0x4112A0}, {0x53, 0x5890E0}};
constexpr ah::CallSite kCalls414A40[] = {{0x0, 0x4112A0}, {0x38, 0x589840}, {0x4B, 0x5890E0}};
constexpr ah::CallSite kCalls414AB0[] = {{0x0, 0x414AC0}, {0x5, 0x414B90}};
constexpr ah::CallSite kCalls414AE0[] = {{0x1C, 0x414B10}};
constexpr ah::CallSite kCalls414B10[] = {{0x1F, 0x414D30}};
constexpr ah::CallSite kCalls414B40[] = {{0x38, 0x414D30}};
constexpr ah::CallSite kCalls414BF0[] = {{0x59, 0x414FC0}};
constexpr ah::CallSite kCalls414C60[] = {{0x65, 0x414FC0}};
constexpr ah::CallSite kCalls414CD0[] = {{0x4E, 0x414FC0}};
constexpr ah::CallSite kCalls414D30[] = {{0x22, 0x5A77C0}, {0x2B, 0x461E50}, {0x3C, 0x414F00}, {0x51, 0x536700}, {0x7B, 0x414F00}, {0xDC, 0x414F00}, {0xF8, 0x414F00}, {0x15B, 0x414F00}, {0x173, 0x531920}, {0x1A8, 0x414F00}, {0x1B8, 0x408530}};
constexpr ah::CallSite kCalls414F00[] = {{0x13, 0x5A77C0}, {0x1C, 0x461E50}, {0x28, 0x5A7710}, {0x3F, 0x5A7780}, {0xB1, 0x461E50}};
constexpr ah::CallSite kCalls414FC0[] = {{0x17, 0x414F00}, {0x26, 0x414F00}, {0x4D, 0x516B30}};
constexpr ah::CallSite kCalls415020[] = {{0x12, 0x4156C0}};
constexpr ah::CallSite kCalls415040[] = {{0x3, 0x415460}, {0x77, 0x531950}, {0x84, 0x41C0A0}, {0x91, 0x530920}, {0x9E, 0x41C0E0}, {0xB4, 0x41C110}, {0xEE, 0x415640}, {0x17F, 0x52E160}, {0x1D5, 0x531DF0}, {0x1F5, 0x4152B0}, {0x1FE, 0x415680}, {0x239, 0x5345E0}, {0x23E, 0x535F50}, {0x24B, 0x52E140}};
constexpr ah::CallSite kCalls4152B0[] = {{0x41, 0x531CF0}, {0xA4, 0x4153F0}, {0xFB, 0x4153F0}, {0x107, 0x5345E0}, {0x114, 0x52E140}};
constexpr ah::CallSite kCalls4153F0[] = {{0x16, 0x41BE10}, {0x29, 0x52E160}, {0x36, 0x531CF0}};
constexpr ah::CallSite kCalls415460[] = {{0x1A, 0x531CF0}, {0x35, 0x4153F0}, {0x41, 0x5345E0}, {0x4E, 0x52E140}};
constexpr ah::CallSite kCalls4154E0[] = {{0x23, 0x52E140}, {0x30, 0x5350C0}, {0x35, 0x534A00}, {0x52, 0x536700}, {0x79, 0x594E00}, {0x9F, 0x536700}, {0xBA, 0x5951D0}, {0xE0, 0x415640}, {0xF2, 0x56D750}, {0x108, 0x534F10}, {0x13A, 0x415040}};
constexpr ah::CallSite kCalls4157A0[] = {{0x26, 0x57BA60}, {0x53, 0x415860}, {0x64, 0x5918E0}, {0x73, 0x589810}};
constexpr ah::CallSite kCalls415860[] = {{0x2D, 0x57B830}, {0x68, 0x41C350}, {0x71, 0x415940}, {0x83, 0x415940}, {0xA2, 0x41C350}, {0xAE, 0x415940}, {0xC0, 0x41C350}, {0xC9, 0x415940}, {0xD2, 0x415940}};
constexpr ah::CallSite kCalls415940[] = {{0x1D, 0x5A75D0}, {0xB9, 0x461E50}};
constexpr ah::CallSite kCalls415A10[] = {{0x14, 0x415A70}, {0x33, 0x415A70}, {0x4E, 0x415860}};
constexpr ah::CallSite kCalls415A70[] = {{0xB5, 0x415B40}};
constexpr ah::CallSite kCalls415B40[] = {{0x3E, 0x415A10}, {0x43, 0x415860}};
constexpr ah::CallSite kCalls415B90[] = {{0x37, 0x41C5B0}};
constexpr ah::CallSite kCalls415BE0[] = {{0x27, 0x4976D0}, {0x61, 0x594E00}, {0x94, 0x57C140}, {0xA7, 0x57C0F0}, {0xAE, 0x4976D0}, {0xBA, 0x4976D0}, {0xEB, 0x589810}, {0x13A, 0x587B40}, {0x146, 0x587AE0}};
constexpr ah::JumpTable kTables415BE0[] = {{0x14, 0x15C, 4}};
constexpr ah::CallSite kCalls415D50[] = {{0x12, 0x589840}, {0x2B, 0x57CF60}, {0x3D, 0x57D420}, {0x4E, 0x5B93D2}, {0xA5, 0x594E00}, {0xB6, 0x589840}, {0x113, 0x5B9380}, {0x14B, 0x516F60}, {0x153, 0x4161F0}, {0x180, 0x5A7A50}, {0x1A0, 0x5A7A50}, {0x1C5, 0x5A77C0}, {0x1CE, 0x461E50}, {0x210, 0x5A7570}, {0x218, 0x5A7780}, {0x22C, 0x5A7A50}, {0x245, 0x5A7A00}, {0x268, 0x5A7A50}, {0x281, 0x5A7A00}, {0x2B0, 0x461E50}};
constexpr ah::CallSite kCalls416020[] = {{0x2A, 0x4160E0}, {0x36, 0x4160E0}};
constexpr ah::CallSite kCalls4160E0[] = {{0xA, 0x536700}};
constexpr ah::JumpTable kTables4160E0[] = {{0x29, 0x3C, 4}};
constexpr ah::CallSite kCalls4161F0[] = {{0xB, 0x5A75D0}, {0x13, 0x5A7780}, {0x81, 0x461E50}, {0x95, 0x5A7570}, {0x9D, 0x5A7780}, {0x109, 0x461E50}};
constexpr ah::CallSite kCalls416340[] = {{0x0, 0x57C7C0}};
constexpr ah::CallSite kCalls416360[] = {{0x18, 0x57C7A0}, {0x2E, 0x4976D0}};
constexpr ah::CallSite kCalls4163B0[] = {{0x30, 0x57C7C0}};
constexpr ah::CallSite kCalls416400[] = {{0x1A, 0x57C0F0}, {0x32, 0x57C110}};
constexpr ah::CallSite kCalls416480[] = {{0x7, 0x57C140}, {0x33, 0x57C140}, {0x73, 0x57C140}};
constexpr ah::CallSite kCalls416550[] = {{0x9, 0x57C140}, {0x1A, 0x57C140}, {0x6C, 0x57C140}};
constexpr ah::CallSite kCalls416600[] = {{0x23, 0x531F90}, {0x3B, 0x57C0F0}, {0x47, 0x531F90}, {0x61, 0x57C110}, {0x6D, 0x531F90}, {0x87, 0x57C0F0}, {0x93, 0x57C110}, {0x9F, 0x531F90}, {0xB9, 0x57C110}, {0xC5, 0x57C0F0}, {0xD1, 0x531F90}, {0xEB, 0x57C0F0}, {0xF7, 0x57C0F0}, {0x103, 0x531F90}, {0x11D, 0x57C110}, {0x129, 0x57C110}};
constexpr ah::JumpTable kTables416600[] = {{0x1C, 0x134, 7}};
constexpr ah::CallSite kCalls416770[] = {{0x14, 0x57C140}, {0x27, 0x57C140}, {0x55, 0x4168C0}, {0x7E, 0x4168C0}, {0x90, 0x57C140}, {0xC2, 0x57C140}, {0xD0, 0x4168C0}, {0xDD, 0x4168C0}, {0xEF, 0x57C140}, {0x115, 0x4168C0}, {0x136, 0x4168C0}};
constexpr ah::CallSite kCalls4168C0[] = {{0x0, 0x57C7C0}};

// Which area and function the round is running (set by the seeds; read by the
// louder stand-ins and the settle).
int g_area = 0;
unsigned g_k = 0;
unsigned g_case06 = 0;   // area 106's step hook: the cell case the seed planted flags for

// ===========================================================================
// The fuzz's own memory: a packet buffer, the stand-ins' effects
// ===========================================================================

constexpr unsigned kPacketBytes = 0x400;
alignas(16) unsigned char g_packets[kPacketBytes];

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

U CommitEffect(const U* a, U answer) { Advance(a[1] & 0xFF); return answer; }
U PolyEffect(const U* a, U answer) {
    Scribble(a[0], 0x48);
    if (InPackets(a[0], 8)) Mem(a[0])[7] = 0x2C;
    return answer;
}
U F3Effect(const U* a, U answer) {
    Scribble(a[0], 0x2C);
    if (InPackets(a[0], 8)) Mem(a[0])[7] = 0x20;
    return answer;
}
U SprtEffect(const U* a, U answer) { Scribble(a[0], 0x20); return answer; }
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? Mem(a[0])[7] | 2 : Mem(a[0])[7] & 0xFD);
    return answer;
}

// The map's cells the band tests: the plate's 0xA1 / 0xA0 / 0xAE, the step's
// exit 0xAF and link 0xC0, the minimap's 0x00 / 0x10 / 0x40 / 0x50 and their
// neighbours, now and then anything.
U ByteAtEffect(const U*, U answer) {
    static const U kCells[] = {0xA1, 0xA0, 0xAE, 0xAF, 0xC0, 0xC1, 0xBF, 0x00, 0x10, 0x40, 0x50, 0x0F, 0x41, 0x51, 0xB0, 0x9F};
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 6 == 0 ? (n >> 8) & 0xFF : kCells[(n >> 4) % 16]);
}

// The functions whose effect answers need it (the seed sets g_k).
enum : unsigned {
    kInit, kPlaceMessage,
    kLeaderRun, kLeaderIdle, kObjectAhead, kTurnToFree, kStartOnObject, kLeaderStep, kStopMotion, kPoseByCharge,
    kLeaderCharge, kKind5CRun, kKind5CStart, kKind5CFollow, kDrawGauge, kKind5CTurn, kKind5CTurnStep, kKind5CSpin,
    kKind5CRise, kTail40, kCountdown, kBuildMinimap, kMinimapShade, kDrawPanel, kTrigger36,
};

// Effect_FindFree: a slot of the twenty, or none a quarter of the time -
// never none for Area104_Kind5CStart, which does not test it (the original
// writes far past the records; ours aborts).
U FindFreeAnswer(const U*, U answer) {
    const U n = ah::Noise();
    if (g_area == 104 && g_k != kKind5CStart && n % 4 == 0) return (answer & 0xFFFFFF00u) | 0xFF;
    return (answer & 0xFFFFFF00u) | ((n >> 4) % at::kEffectCount);
}
// Sprite_ObjectAt: none (0xFF), a field object 0..0x1D, one of the four
// extra records 0x1E..0x21 - but not none with the charge button held when
// Area104_ObjectAhead121 runs (it indexes the extra records by it unchecked;
// ours aborts).
U ObjectAtAnswer(const U*, U answer) {
    const U n = ah::Noise();
    U v;
    switch (n % 4) {
    case 0: v = 0xFF; break;
    case 1: v = 0x1E + (n >> 4) % 4; break;
    default: v = (n >> 4) % 0x1E; break;
    }
    const bool held = (Word(Mem(at::kButtonCharge)) & Word(Mem(at::kInputHeld))) != 0;
    if (v == 0xFF && held && g_k == kObjectAhead) v = 0x21;
    return (answer & 0xFFFFFF00u) | v;
}
// Area104_PoseByCharge's stand-in writes what its caller reads after it:
// Field_State +0x128 3 or 4 (or anything).
U PoseEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (Field_State != nullptr && (n & 3) != 0) Field_State[0x128] = static_cast<unsigned char>((n & 4) ? 4 : (n & 8) ? 3 : n >> 8);
    return answer;
}
// Louder than the real callees on purpose: after these the callers read
// Sprite_Current (and its +8, +9, +2) again - the stand-in moves the running
// object half the time, from Noise only.
unsigned char* SomeRecord(U h) { return (h & 1) ? ah::PartyOf(static_cast<unsigned char>(h >> 1)) : ah::TaskAt(h >> 1); }
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if ((n & 1) != 0) Sprite_Current = SomeRecord(n >> 4);
    if ((n & 6) == 2) Sprite_Current[8] = static_cast<unsigned char>(n >> 12);
    return answer;
}
// A test that ends its caller when it answers: 0 five calls in six (so the
// callers' later paths run), else a non-zero byte; garbage above either way.
U MostlyNo(const U*, U answer) {
    const U n = ah::Noise();
    return (answer & 0xFFFFFF00u) | (n % 6 != 0 ? 0u : ((n >> 8) | 1u) & 0xFF);
}
// Field_LeaderStepTarget: 0 (free) half the time, 1 and 0xFF (the two the
// idle stops on) and another byte a sixth each; the running object moved as
// MovesCurrent does.
U StepTargetAnswer(const U* a, U answer) {
    const U n = ah::Noise();
    static const U kV[] = {0, 0, 0, 1, 0xFF, 2};
    answer = (answer & 0xFFFFFF00u) | (kV[n % 6] == 2 ? ((n >> 8) | 2u) & 0xFF : kV[n % 6]);
    return MovesCurrent(a, answer);
}
// Field_LeaderPushObjects: 0 or a non-zero byte, half and half; the running
// object moved as MovesCurrent does.
U HalfNo(const U* a, U answer) {
    const U n = ah::Noise();
    return MovesCurrent(a, (answer & 0xFFFFFF00u) | ((n & 1) != 0 ? 0u : ((n >> 8) | 1u) & 0xFF));
}
// Flags_Test on the story flags: three calls in four the bit as it stands
// (a whole eax of 0 or 1, as the real one), else the recorder's own answer -
// so that a seed can plant the flags a path wants.
U FlagsAnswer(const U* a, U answer) {
    const U n = ah::Noise();
    if (a[0] != at::kStoryFlags || n % 4 == 0) return answer;
    return (Mem(at::kStoryFlags + (a[1] & 0xFF) / 8)[0] >> (a[1] & 7)) & 1;
}
// The kind 0x5C turn test's stand-in: answers 0 two calls in three, and
// moves the facings both callers read after it.
U TurnStepEffect(const U*, U answer) {
    const U n = ah::Noise();
    if ((n & 3) == 1) B(at::kLeaderDir) = static_cast<unsigned char>((n >> 8) & 7);
    if ((n & 0x30) == 0x10) Sprite_Current[8] = static_cast<unsigned char>((n >> 12) & 7);
    return n % 3 == 0 ? answer : answer & 0xFFFFFF00u;
}

const ah::Callee kSet40 = {"ScriptFlags_Set40", bof3::addr::ScriptFlags_Set40, KeyOf(&::ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0};
const ah::Callee kClear40 = {"ScriptFlags_Clear40", bof3::addr::ScriptFlags_Clear40, KeyOf(&::ScriptFlags_Clear40), 0, {}, ah::Answer::kGarbage, 0, 0};
#define W2E_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
#define W2E_THEIRS(name) #name, KeyOf(name), KeyOf(name)
#define W2E_RAW(name, address) name, address, address

const ah::Callee kCallees[] = {
    kSet40, kClear40,
    {W2E_OURS(Gfx_CommitPrim), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &CommitEffect, nullptr},
    {W2E_OURS(Gpu_SetPolyFT4), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &PolyEffect, nullptr},
    {W2E_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect, nullptr},
    {W2E_OURS(Gpu_SetSprt), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SprtEffect, nullptr},
    {W2E_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect, nullptr},
    {W2E_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, ah::Answer::kGarbage, 0, 0, {}, &ByteAtEffect, nullptr},
    // the cell words pushed with stale bits above them
    {W2E_OURS(Field_CellHasEvent), 2, {kU16, kU16}, ah::Answer::kFlag, 0, 0},
    {W2E_OURS(WorldMap_PinSprite), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W2E_OURS(WorldMap_DrawNeedle), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W2E_OURS(Effect_Release), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W2E_OURS(Effect_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &FindFreeAnswer, nullptr},
    {W2E_OURS(Sprite_ObjectAt), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &ObjectAtAnswer, nullptr},
    {W2E_OURS(Text_DrawFont12), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W2E_OURS(Scenario_ArriveHook), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &MostlyNo, nullptr},
    {W2E_OURS(Field_LeaderCellEvent), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MostlyNo, nullptr},
    {W2E_OURS(Field_LeaderTalkTest), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MostlyNo, nullptr},
    // the x and z words pushed with stale bits above the byte it reads
    {W2E_OURS(Area_LinkAt), 2, {kU8, kU8}, ah::Answer::kFlag, 0, 0},
    {W2E_OURS(KeyItem_Has), 1, {kAll}, ah::Answer::kFlag, 0, 0},
    // the colour byte pushed with stale bits above it
    {W2E_OURS(Menu_DrawBox), 6, {kAll, kAll, kAll, kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2E_OURS(Menu_DrawOutline), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W2E_THEIRS(Crt_sprintf), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    // the area word and the flag byte pushed with stale bits above them (it
    // reads a u16 and a byte, window_task.cpp)
    {W2E_OURS(Field_ChangeArea), 4, {kU16, kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2E_OURS(Field_LeaderStepTarget), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &StepTargetAnswer, nullptr},
    {W2E_OURS(Flags_Test), 2, {kAll, kU8}, ah::Answer::kBool, 0, 0, {}, &FlagsAnswer, nullptr},
    {W2E_OURS(Field_LeaderPushObjects), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &HalfNo, nullptr},
    // area 121's (AR3B's band this wave), by raw address
    {W2E_RAW("0x41BE10", kTurnToward121), 3, {kAll, kAll, kU8}, ah::Answer::kByte, 0, 7, {}, &MovesCurrent, nullptr},
    {W2E_RAW("0x41C0A0", kMenuButton121), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MostlyNo, nullptr},
    {W2E_RAW("0x41C0E0", kHoldButton121), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MostlyNo, nullptr},
    {W2E_RAW("0x41C110", kTurnKeys121), 0, {}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent, nullptr},
    {W2E_RAW("0x41C350", kGaugeFrame121), 3, {kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W2E_RAW("0x41C5B0", kKind5CState4), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent, nullptr},
    // an engine GPU setter nobody owns (POLY_F3)
    {W2E_RAW("0x5A7570", kSetPolyF3), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &F3Effect, nullptr},
    // the group's own, called directly
    {W2E_RAW("Area104_BuildMinimap", 0x416020), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_LeaderIdle", 0x415040), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_ObjectAhead121", 0x4152B0), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_TurnToFree", 0x4153F0), 3, {kAll, kAll, kU8}, ah::Answer::kFlag, 0, 0, {}, &MovesCurrent, nullptr},
    {W2E_RAW("Area104_StartOnObject121", 0x415460), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MostlyNo, nullptr},
    {W2E_RAW("Area104_StopMotion", 0x415640), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent, nullptr},
    {W2E_RAW("Area104_PoseByCharge", 0x415680), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &PoseEffect, nullptr},
    {W2E_RAW("Area104_LeaderCharge", 0x4156C0), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_Kind5CFollow", 0x415860), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_DrawGauge", 0x415940), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2E_RAW("Area104_Kind5CTurn", 0x415A10), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_Kind5CTurnStep", 0x415A70), 2, {kU8, kU8}, ah::Answer::kGarbage, 0, 0, {}, &TurnStepEffect, nullptr},
    {W2E_RAW("Area104_Kind5CSpin", 0x415B40), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_DrawPanel", 0x4161F0), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_MinimapShade", 0x4160E0), 2, {kU16, kU16}, ah::Answer::kByte, 0, 3},
    {W2E_RAW("Area106_ArmTail36", 0x4168C0), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    // the world-map copy's own
    {W2E_RAW("Area104_FrameStep", 0x414AC0), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_BoxStep", 0x414B90), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_FrameHold", 0x414B10), 0, {}, ah::Answer::kPhase, 0, 0},
    {W2E_RAW("Area104_DrawFrame", 0x414D30), 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},
    {W2E_RAW("Area104_DrawSprite", 0x414F00), 3, {kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W2E_RAW("Area104_DrawHud", 0x414FC0), 2, {kAll, kU16}, ah::Answer::kGarbage, 0, 0},
};
#undef W2E_OURS
#undef W2E_THEIRS
#undef W2E_RAW

// ===========================================================================
// Area 104's world-map copy (area 87's group, area_w2b_fuzz.cpp)
// ===========================================================================

const ah::Clone kClonesWm[] = {
    W2E_C(Area104_PlateRun, 0x414760, 0xD6, kCalls414760, S::kState),
    W2E_C(Area104_PlateShow, 0x414840, 0x142, kCalls414840, S::kState),
    W2E_C(Area104_PlateGrow, 0x414990, 0x41, kCalls414990, S::kState),
    W2E_C(Area104_PlateHold, 0x4149E0, 0x58, kCalls4149E0, S::kState),
    W2E_C(Area104_PlateShrink, 0x414A40, 0x50, kCalls414A40, S::kState),
    W2E_P(Area104_HudRun, 0x414A90, 0x12, S::kState),
    W2E_C(Area104_HudFrame, 0x414AB0, 0xA, kCalls414AB0, S::kState),
    W2E_P(Area104_FrameStep, 0x414AC0, 0x12, S::kCallee),
    W2E_C(Area104_FrameSlideIn, 0x414AE0, 0x21, kCalls414AE0, S::kState),
    W2E_C(Area104_FrameHold, 0x414B10, 0x28, kCalls414B10, S::kState),
    W2E_C(Area104_FrameSlideOut, 0x414B40, 0x41, kCalls414B40, S::kState),
    W2E_P(Area104_BoxStep, 0x414B90, 0x12, S::kCallee),
    W2E_C(Area104_BoxSlideIn, 0x414BF0, 0x62, kCalls414BF0, S::kState),
    W2E_C(Area104_BoxHold, 0x414C60, 0x6E, kCalls414C60, S::kState),
    W2E_C(Area104_BoxSlideOut, 0x414CD0, 0x57, kCalls414CD0, S::kState),
    W2E_C(Area104_DrawFrame, 0x414D30, 0x1C5, kCalls414D30, S::kCallee),
    W2E_C(Area104_DrawSprite, 0x414F00, 0xBC, kCalls414F00, S::kCallee),
    W2E_C(Area104_DrawHud, 0x414FC0, 0x58, kCalls414FC0, S::kCallee),
};
enum : unsigned {
    kPlateRun, kPlateShow, kPlateGrow, kPlateHold, kPlateShrink, kHudRun, kHudFrame, kFrameStep, kFrameSlideIn,
    kFrameHold, kFrameSlideOut, kBoxStep, kBoxSlideIn, kBoxHold, kBoxSlideOut, kDrawFrame, kDrawSprite, kDrawHud,
};
static_assert(kDrawHud + 1 == AH_COUNT(kClonesWm), "the world map's seeding indices");

const ah::DataTable kTablesWm[] = {
    {at::kWm104.plate_states, 5}, {at::kWm104.hud_states, 2}, {at::kWm104.frame_states, 4}, {at::kWm104.box_states, 4},
};

// The fuzz's own literal table addresses (read off the exe, docs/area_w2e.md
// section 2): the seed plants and the regions come from these, never from the
// kWm104 ours reads, so a wrong constant in ours shows.
constexpr U kFzPlateAnims = 0x61B4F0;   // one entry, then the zero record
constexpr U kFzButtons = 0x61BC10;

enum : unsigned { kRegPackets = 3 };
ah::Region g_regions_wm[] = {
    {at::kMapMode, 1}, {0x7E0918, 1}, {0x7E0670, 4}, {0, kPacketBytes}, {0x9037A0, 0x20}, {0x66C7E8, 2},
    {at::kButtonMap0, 0x10}, {at::kAreaText, 4}, {kFzPlateAnims, 8},
};
unsigned char g_saved_anims[8];

void DisturbWm(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 10) {
    case 0: B(at::kMapMode) = static_cast<unsigned char>(v % 4 == 0 ? v : v % 3); break;
    case 1: Draw_PassFlags = static_cast<unsigned char>(Draw_PassFlags ^ (v & 1 ? 4 : 0x1B)); break;
    case 2: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (v & 1 ? 0x100 : 0x4000)); break;
    case 3: Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000); break;
    case 4: B(at::kTailState) = static_cast<unsigned char>(v % 3); break;
    case 5: Game_Mode = static_cast<unsigned short>(Game_Mode ^ 1); break;
    case 6: SetWord(Mem(at::kPlace), v & 1 ? 0x65 : v); break;
    case 7: B(at::kLeaderCellX + (v & 1) * 2) = static_cast<unsigned char>(v); break;
    case 8: B(at::kPartySet) = static_cast<unsigned char>(v & 1 ? 0xC : v); break;
    default: break;
    }
}
// The plate's +1 kept inside its five states after a disturbance; the place,
// whatever it became, planted in the plate table's zero record (the search
// has no bound).
void SettleWm() {
    unsigned char* const o = Sprite_Current;
    if (o[1] >= 5) o[1] = static_cast<unsigned char>(o[1] % 5);
    SetWord(Mem(kFzPlateAnims + 4), Word(Mem(at::kPlace)));
}

void SeedWm(unsigned k) {
    g_area = 104;
    g_k = 0x100 + k;
    unsigned char* const o = Sprite_Current;
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
    if (ah::Often()) std::memcpy(Mem(kFzPlateAnims), g_saved_anims, sizeof g_saved_anims);
    o[1] = static_cast<unsigned char>(o[1] % 5);
    // the place the shipped entry's (0x65) half the time; the zero record
    // (the search's end) planted with the place
    if (ah::Half()) SetWord(Mem(at::kPlace), 0x65);
    SetWord(Mem(kFzPlateAnims + 4), Word(Mem(at::kPlace)));
    const auto leave = [o] {
        B(at::kMapMode) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2) : ah::Next());
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next() | 1);
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(0, 2, 5, 1, 3));
        Field_ScriptFlags = static_cast<unsigned short>(ah::Often() ? Field_ScriptFlags & ~0x100u : Field_ScriptFlags | 0x100u);
    };
    switch (k) {
    case kPlateRun:
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 ^ 0x1000);
        if (ah::Half()) B(at::kLeaderSteps) = static_cast<unsigned char>(AH_PICK(0, 1, 0xFF, 0x80));
        break;
    case kPlateShow:
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 1, 1, 2, 3, 4, 0, 5) : ah::Next());
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
    case kFrameSlideIn:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0, 0xFFFF, 1, 0xFFF0, 0x7FF0, 0x7FFF, 0xFFD0));
        B(at::kMapMode) = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1) : ah::Next());
        break;
    case kFrameHold:
        B(at::kMapMode) = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
        break;
    case kFrameSlideOut:
        if (ah::Often()) SetWord(o + 0x2E, AH_PICK(0xFFE0, 0xFFE1, 0xFFDF, 0x8000, 0x800F, 0x8010, 0x10));
        B(at::kMapMode) = static_cast<unsigned char>(ah::Often() ? AH_PICK(2, 0, 1, 0x82) : ah::Next());
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
        if (ah::Often()) SetLong(Mem(at::kButtonMap0), static_cast<std::int32_t>(Word(Mem(kFzButtons + (ah::Next() % 6) * 4)) | (ah::Next() & 0xFFFF0000u)));
        if (ah::Half()) {
            // a word only the seventh or eighth entry answers (in area 104 the
            // leader table's first two dwords), when it has such bits
            U others = 0;
            for (U e = 0; e < 6; ++e) others |= Word(Mem(kFzButtons + e * 4));
            const U e = 6 + ah::Next() % 2;
            const U only = Word(Mem(kFzButtons + e * 4)) & ~others;
            if (only != 0) SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(only | (ah::Next() & 0xFFFF0000u)));
        } else if (ah::Often()) {
            SetLong(Mem(at::kButtonMap6), static_cast<std::int32_t>(Word(Mem(kFzButtons + (ah::Next() % 8) * 4)) | (ah::Next() & 0xFFFF0000u)));
        }
        if (ah::Half()) SetLong(Mem(at::kButtonMap0), 0);
        if (ah::Half()) Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & ~0x1000u);
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x4000u);
        if (ah::Half()) B(at::kPartySet) = static_cast<unsigned char>(AH_PICK(0xC, 0x8C, 0xB, 0xFF));
        break;
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

// ===========================================================================
// Area 104's own code
// ===========================================================================

const ah::Clone kClones104[] = {
    W2E_C(Area104_Init, 0x4146C0, 0x3D, kCalls4146C0, S::kInit),
    W2E_C(Area104_PlaceMessage, 0x414700, 0x56, kCalls414700, S::kState),
    W2E_C(Area104_LeaderRun, 0x415020, 0x17, kCalls415020, S::kState),
    W2E_C(Area104_LeaderIdle, 0x415040, 0x26B, kCalls415040, S::kState),
    W2E_C(Area104_ObjectAhead121, 0x4152B0, 0x133, kCalls4152B0, S::kCallee),
    W2E_A(Area104_TurnToFree, 0x4153F0, 0x69, kCalls4153F0, S::kCallee),
    W2E_A(Area104_StartOnObject121, 0x415460, 0x71, kCalls415460, S::kCallee),
    W2E_C(Area104_LeaderStep, 0x4154E0, 0x157, kCalls4154E0, S::kState),
    W2E_P(Area104_StopMotion, 0x415640, 0x36, S::kCallee),
    W2E_P(Area104_PoseByCharge, 0x415680, 0x3A, S::kCallee),
    W2E_P(Area104_LeaderCharge, 0x4156C0, 0xB8, S::kCallee),
    W2E_P(Area104_Kind5CRun, 0x415780, 0x12, S::kState),
    W2E_C(Area104_Kind5CStart, 0x4157A0, 0xB1, kCalls4157A0, S::kState),
    W2E_C(Area104_Kind5CFollow, 0x415860, 0xDB, kCalls415860, S::kCallee),
    W2E_C(Area104_DrawGauge, 0x415940, 0xC4, kCalls415940, S::kCallee),
    W2E_C(Area104_Kind5CTurn, 0x415A10, 0x54, kCalls415A10, S::kState),
    W2E_A(Area104_Kind5CTurnStep, 0x415A70, 0xCB, kCalls415A70, S::kCallee),
    W2E_C(Area104_Kind5CSpin, 0x415B40, 0x48, kCalls415B40, S::kState),
    W2E_C(Area104_Kind5CRise, 0x415B90, 0x4B, kCalls415B90, S::kState),
    W2E_T(Area104_Tail40, 0x415BE0, 0x16C, kCalls415BE0, kTables415BE0, S::kTail, 0),
    W2E_C(Area104_Kind6ACountdown, 0x415D50, 0x2CC, kCalls415D50, S::kState),
    W2E_C(Area104_BuildMinimap, 0x416020, 0xB8, kCalls416020, S::kCallee),
    W2E_T(Area104_MinimapShade, 0x4160E0, 0x10D, kCalls4160E0, kTables4160E0, S::kCallee, 0xFF),
    W2E_C(Area104_DrawPanel, 0x4161F0, 0x14E, kCalls4161F0, S::kCallee),
    W2E_A(Area104_Trigger36, 0x416340, 0x1D, kCalls416340, S::kCallee),
};
static_assert(kTrigger36 + 1 == AH_COUNT(kClones104), "area 104's seeding indices");

const ah::DataTable kTables104[] = {{at::kA104LeaderStates, at::kA104LeaderStateCount}, {at::kA104Kind5CStates, at::kA104Kind5CStateCount}};

// Beyond the field frame: the effect records, the minimap's image, upload
// queue and CLUT words, the cells the band writes outside the harness's
// regions, Input_Held, the actor states the idle reads, two pointer cells.
const ah::Region kRegions104[] = {
    {at::kEffects, at::kEffectCount * at::kEffectStride},
    {at::kUnpack, at::kUnpackBytes},
    {at::kUploadCount, 8},   // with Field_ActiveMember 0x9035A4 after it
    {at::kUploadX, 0x50},
    {at::kUploadRecord, at::kUploadSlots * 4},
    {at::kClutWords, 8},
    {at::kInputHeld, 4},
    {at::kActorStates, 4 * at::kActorStride},
    {at::kCell904EE0, 1},
    {at::kCell937F98, 1},
    {at::kFAWord, 2},
    {at::kCondFE, 1},
    {at::kMenuColour, 1},
    {at::kButtonMap0, 0x10},
    {at::kPrevArea, 2},
    {at::kScriptObject, 4},
    {0x7E0670, 4},
    {0, kPacketBytes},
    {0x7E0918, 1},
};
enum : unsigned { kReg104Packets = 17 };
ah::Region g_regions104[AH_COUNT(kRegions104)];

// A 16.16 word with the high word `high` and any low word.
U At16(U high, U low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }

void Common(int area, unsigned k) {
    g_area = area;
    g_k = k;
    ah::SetPointer(at::kActiveMember, SomeRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, SomeRecord(ah::Next()));
    Gfx_PacketNext = g_packets + (ah::Next() % 4) * 4;
}

void Seed104(unsigned k) {
    Common(104, k);
    unsigned char* const o = Sprite_Current;
    const auto button = [] {
        // the charge button word held or not in Input_Held's low half
        SetWord(Mem(at::kButtonCharge), ah::Often() ? AH_PICK(0x20, 0x40, 0x10, 0x8000) : ah::Next());
        const U held = ah::Half() ? Word(Mem(at::kButtonCharge)) | (ah::Next() & 0xFFFF0000u) : (ah::Next() & ~static_cast<U>(Word(Mem(at::kButtonCharge))));
        SetLong(Mem(at::kInputHeld), static_cast<std::int32_t>(held));
    };
    switch (k) {
    case kInit:
        if (ah::Half()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x140u);
        B(at::kUploadCount) = static_cast<unsigned char>(ah::Next() % at::kUploadSlots);
        break;
    case kPlaceMessage:
        B(at::kTailState) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 5, 3));
        break;
    case kLeaderRun:
        o[2] = static_cast<unsigned char>(ah::Next() % at::kA104LeaderStateCount);
        break;
    case kLeaderIdle:
        // each early test at its value or beside it
        if (ah::Often()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x100u);
        if (ah::Often()) B(at::kScriptFlags2) = static_cast<unsigned char>(B(at::kScriptFlags2) & ~0x40u);
        if (ah::Next() % 6 != 0) Field_Request = 0;
        if (ah::Next() % 6 != 0) o[0xA] = 0;
        if (ah::Half()) SetWord(Mem(at::kFieldInputHeld), 0);
        if (ah::Half()) Field_State[0x148] = static_cast<unsigned char>(ah::Next() % 4);
        for (unsigned a = 0; a < 4; ++a)
            if (ah::Half()) B(at::kActorStates + a * at::kActorStride) = static_cast<unsigned char>(ah::Next() ^ (ah::Half() ? 0x20 : 0));
        button();
        o[8] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Next());
        if (ah::Often()) o[0xB] = static_cast<unsigned char>(AH_PICK(0x40, 0x3F, 0, 0x41, 0xC0, 0x10));
        break;
    case kObjectAhead:
    case kStartOnObject:
        Game_AreaNumber = static_cast<unsigned short>(ah::Often() ? 0x79 : AH_PICK(0x68, 0x179, 0x78, 0x7A));
        button();
        o[8] = static_cast<unsigned char>(ah::Often() ? ah::Next() % 8 : ah::Next());
        // the objects' facings on the leader's, now and then
        for (unsigned n = 0; n < 0x22; ++n) {
            unsigned char* const r = n < 0x1E ? ah::Object(n) : Mem(at::kObjectsExtra + (n - 0x1E) * at::kObjectStride);
            if (ah::Half()) r[8] = static_cast<unsigned char>((o[8] & 7) | (ah::Next() & 0xF8));
        }
        break;
    case kTurnToFree:
        break;
    case kLeaderStep:
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(0, 0, 0, 0, 1, 2, 0xFF));
        if (ah::Often()) B(at::kFieldInputFlags) = static_cast<unsigned char>(ah::Next() | 1);
        if (ah::Often()) o[8] = static_cast<unsigned char>(AH_PICK(2, 6, 0, 1, 3, 4, 5, 7, 0x82));
        if (ah::Often()) Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & ~0x100u);
        if (ah::Often()) B(at::kScriptFlags2) = static_cast<unsigned char>(B(at::kScriptFlags2) & ~0x40u);
        if (ah::Half()) SetWord(Mem(at::kButtonCharge), 0);
        if (ah::Half()) SetWord(Mem(at::kButtonMap0), 0);
        if (ah::Half()) SetWord(Mem(at::kFieldInputHeld), ah::Half() ? 0 : AH_PICK(0x1000, 0x8000, 0x0800));
        break;
    case kPoseByCharge:
    case kLeaderCharge:
        button();
        o[0xA] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 1, 2, 0x1E) : ah::Next());
        o[0xB] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 0x20, 0x21, 0x30, 0x31, 0x3F, 0x40, 0x41, 0x7F, 0x80, 0xFF, 0x42) : ah::Next());
        o[2] = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next());
        break;
    case kKind5CRun:
        o[1] = static_cast<unsigned char>(ah::Next() % at::kA104Kind5CStateCount);
        break;
    case kKind5CStart:
        Game_AreaNumber = static_cast<unsigned short>(ah::Often() ? 0x68 : AH_PICK(0x69, 0x79, 0x67, 0x10));
        break;
    case kKind5CFollow:
        Draw_PassFlags = static_cast<unsigned char>(ah::Often() ? AH_PICK(1, 2, 8, 0x10, 0x1B, 4, 0x20, 0xE4) : ah::Next());
        B(at::kStatusBits) = static_cast<unsigned char>(ah::Half() ? B(at::kStatusBits) & ~0x40u : B(at::kStatusBits) | 0x40u);
        if (ah::Often()) B(at::kLeaderA) = static_cast<unsigned char>(ah::Half() ? 0 : ah::Next());
        break;
    case kDrawGauge:
        break;
    case kKind5CTurn:
        if (ah::Often()) o[8] = static_cast<unsigned char>(ah::Next() % 8);
        if (ah::Often()) B(at::kLeaderDir) = ah::Half() ? o[8] : static_cast<unsigned char>(ah::Next() % 8);
        break;
    case kKind5CTurnStep:
        if (ah::Often()) o[8] = static_cast<unsigned char>(ah::Next() % 8);
        break;
    case kKind5CSpin:
        if (ah::Often()) o[9] = static_cast<unsigned char>(AH_PICK(1, 1, 2, 0, 8, 0xFF));
        if (ah::Often()) SetLong(o + 0x14, static_cast<std::int32_t>(AH_PICK(0x40, 0xFFFFFFC0u, 0, 0x1000)));
        break;
    case kKind5CRise:
        if (ah::Often()) o[0xA] = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0x18, 0xFF));
        break;
    case kTail40:
        B(at::kTailState) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 2, 3, 0, 1, 2, 3, 4, 0xFF, 0x80) : ah::Next());
        if (ah::Often()) Field_Request = static_cast<unsigned char>(ah::Half() ? 0 : AH_PICK(1, 2, 5, 0x80));
        break;
    case kCountdown:
        Sprite_Current = Mem(at::kEffects + (ah::Next() % at::kEffectCount) * at::kEffectStride);
        Field_Request = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 0, 5, 1, 2) : ah::Next());
        if (ah::Often()) B(at::kLeaderState) = static_cast<unsigned char>(ah::Half() ? 0xC : AH_PICK(0xB, 0xD, 0x8C, 0));
        Sprite_Current[0xA] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 2, 3, 0x1D, 0x1E, 0xFF, 0x80) : ah::Next());
        Sprite_Current[9] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 9, 10, 11, 0x19, 0xFF) : ah::Next());
        if (ah::Half()) Frame_Counter = AH_PICK(0, 4, 3, 7, 8, 0xE, 0xF, 0xFFFFFFFFu);
        break;
    case kBuildMinimap:
        B(at::kUploadCount) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 18, 19) : ah::Next() % at::kUploadSlots);
        break;
    case kDrawPanel:
        if (ah::Half()) Frame_Counter = AH_PICK(0, 8, 0x10, 0x18, 7, 9, 0xFFFFFFF8u, 0xFFFFFFFFu);
        break;
    default: break;
    }
}
// The (x, z, object) of Area104_TurnToFree: an object byte, none (0xFF), with
// bits above it; the gauge's (width, bar): bar 0 or 1 with bits above it; the
// turn test's (from, to) facings; the minimap cell; the trigger's (object,
// 0x904030).
void Args104(unsigned k, U* a) {
    switch (k) {
    case kTurnToFree:
        a[2] = (ah::Often() ? AH_PICK(0xFF, 0, 5, 0x1D, 0x1E, 0x21) : ah::Next()) | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0);
        break;
    case kDrawGauge:
        a[0] = ah::Often() ? AH_PICK(0x40, 0, 1, 0x3F, 0xFF, 0x140) : ah::Next();
        a[1] = (ah::Next() & 1) | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0);
        break;
    case kKind5CTurnStep: {
        const U from = ah::Next() % 8;
        U to;
        switch (ah::Next() % 5) {
        case 0: to = from; break;
        case 1: to = (from + 1) & 7; break;
        case 2: to = (from + 7) & 7; break;
        case 3: to = (from + 2 + ah::Next() % 5) & 7; break;
        default: to = ah::Next(); break;
        }
        a[0] = from | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0);
        a[1] = to | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0);
        if (ah::Next() % 8 == 0) a[0] = ah::Next();
        break;
    }
    case kMinimapShade:
        a[0] = ah::Often() ? 5 + ah::Next() % 0x50 : ah::Next();
        a[1] = ah::Often() ? 0xD + ah::Next() % 0x45 : ah::Next();
        break;
    case kTrigger36:
        a[0] = Key(ah::Object(a[0]));
        a[1] = at::kStoryFlags;
        break;
    default: break;
    }
}
// Area 104's disturbance (from h only): the tail state, the charge bytes,
// the facings, the button words.
void Disturb104(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 7) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 5 : v); break;
    case 1: B(at::kLeaderDir) = static_cast<unsigned char>(v & 7); break;
    case 2: if (Sprite_Current != nullptr) Sprite_Current[0xB] = static_cast<unsigned char>(h & 0x100 ? 0x40 : v); break;
    case 3: SetWord(Mem(at::kInputHeld), h & 0x100 ? Word(Mem(at::kButtonCharge)) : 0); break;
    case 4: ah::SetPointer(at::kActiveMember, SomeRecord(h >> 16)); break;
    case 5: ah::SetPointer(at::kScriptObject, SomeRecord(h >> 16)); break;
    default: B(at::kUploadCount) = static_cast<unsigned char>(v % at::kUploadSlots); break;
    }
}
// Every call's aftermath kept inside what ours can index: the leader state
// byte inside its table, the upload count inside the queue.
void Settle104() {
    if (g_k == kLeaderRun && Sprite_Current[2] >= at::kA104LeaderStateCount) Sprite_Current[2] = static_cast<unsigned char>(Sprite_Current[2] & 1);
    if (g_k == kKind5CRun && Sprite_Current[1] >= at::kA104Kind5CStateCount) Sprite_Current[1] = static_cast<unsigned char>(Sprite_Current[1] % 5);
    if (B(at::kUploadCount) >= at::kUploadSlots) B(at::kUploadCount) = static_cast<unsigned char>(B(at::kUploadCount) % at::kUploadSlots);
}

// ===========================================================================
// Areas 105 and 106
// ===========================================================================

const ah::Clone kClones105[] = {
    W2E_C(Area105_Tail61, 0x416360, 0x45, kCalls416360, S::kTail),
    W2E_A(Area105_StepHook, 0x4163B0, 0x49, kCalls4163B0, S::kHook),
    W2E_C(Area105_Init, 0x416400, 0x3B, kCalls416400, S::kInit),
};
enum : unsigned { k105Tail, k105Step, k105Init };

const ah::Clone kClones106[] = {
    W2E_P(Area106_Handler0, 0x416440, 0x36, S::kHandler),
    W2E_C(Area106_PlaceByFlags, 0x416480, 0xCF, kCalls416480, S::kHandler),
    W2E_C(Area106_PlaceByPair, 0x416550, 0xAF, kCalls416550, S::kHandler),
    W2E_T(Area106_Tail36, 0x416600, 0x161, kCalls416600, kTables416600, S::kTail, 0),
    W2E_A(Area106_StepHook, 0x416770, 0x144, kCalls416770, S::kHook),
    W2E_C(Area106_ArmTail36, 0x4168C0, 0x16, kCalls4168C0, S::kCallee),
};
enum : unsigned { k106Handler0, k106Flags, k106Pair, k106Tail, k106Step, k106Arm };

const ah::Region kRegions10x[] = {
    {at::kPrevArea, 2}, {at::kCondFE, 1}, {at::kActiveMember, 4}, {at::kScriptObject, 4},
};

void Seed105(unsigned k) {
    Common(105, k);
    switch (k) {
    case k105Tail:
        B(at::kTailState) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 0, 1, 1, 2, 0xFF, 0x80) : ah::Next());
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 1, 3, 0x82));
        break;
    case k105Step:
        if (ah::Often()) B(at::kCondFA) = static_cast<unsigned char>(AH_PICK(9, 9, 0xA, 0xB, 0, 0x80, 0xFF, 0x7F));
        if (ah::Often()) B(at::kCondFD) = static_cast<unsigned char>(ah::Half() ? 0 : AH_PICK(1, 0x80, 0xFF));
        if (ah::Often()) B(at::kLeaderDir) = static_cast<unsigned char>(ah::Next() % 8);
        break;
    case k105Init:
        SetWord(Mem(at::kPrevArea), ah::Often() ? AH_PICK(0x57, 0x57, 0x56, 0x58, 0x157, 0x8057) : ah::Next());
        B(at::kCondFD) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 1, 0, 1, 2, 0xFF, 0x81) : ah::Next());
        break;
    default: break;
    }
}
// The step hook's (x, z): z's high word on and beside 0x1D (a signed test),
// its sign's edges, anything.
void Args105(unsigned k, U* a) {
    if (k != k105Step || !ah::Often()) return;
    a[1] = At16(AH_PICK(0x1D, 0x1E, 0x1C, 0x20, 0x8000, 0x7FFF, 0xFFFF, 0), a[1]);
}

void Seed106(unsigned k) {
    Common(106, k);
    switch (k) {
    case k106Handler0:
        if (ah::Often()) B(at::kLeader89) = static_cast<unsigned char>(ah::Next() % 0x20);
        break;
    case k106Tail:
        B(at::kTailState) = static_cast<unsigned char>(ah::Often() ? AH_PICK(0, 2, 0xA, 0xC, 0xE, 0x10, 1, 3, 4, 8, 0xB, 0x11, 0xFF, 0x80) : ah::Next());
        break;
    case k106Step: {
        B(at::kCondFD) = static_cast<unsigned char>(ah::Often() ? 0 : AH_PICK(1, 0x80));
        // the case: 0 (0x4F, not 0x51), 1 (0x4F and 0x51), 2 / 3 (0x52, with
        // 0x53 or not), 4 (0x53 only), 5 (none of them)
        g_case06 = ah::Next() % 6;
        static const unsigned char kFlags[6][4] = {{1, 0, 0, 0}, {1, 1, 0, 0}, {0, 0, 1, 1}, {0, 0, 1, 0}, {0, 0, 0, 1}, {0, 0, 0, 0}};
        static const unsigned kIds[4] = {0x4F, 0x51, 0x52, 0x53};
        for (unsigned i = 0; i < 4; ++i) {
            if (!ah::Often()) continue;
            unsigned char& byte = Mem(at::kStoryFlags + kIds[i] / 8)[0];
            const auto bit = static_cast<unsigned char>(1u << (kIds[i] & 7));
            byte = static_cast<unsigned char>(kFlags[g_case06][i] ? byte | bit : byte & ~bit);
        }
        break;
    }
    default: break;
    }
}
// A cell on or beside each of the five the step hook tests: the exact word
// (or one low bit off) in one axis, the high word in or beside its range in
// the other.
void Args106(unsigned k, U* a) {
    if (k == k106Arm) {
        a[0] = (ah::Often() ? AH_PICK(0, 2, 0xA, 0xC, 0xE, 0x10) : ah::Next()) | (ah::Half() ? ah::Next() & 0xFFFFFF00u : 0);
        return;
    }
    if (k != k106Step || !ah::Often()) return;
    const auto near = [](U lo, U n) {
        switch (ah::Next() % 5) {
        case 0: case 1: return lo + ah::Next() % n;
        case 2: return ah::Half() ? lo - 1 : lo + n;
        case 3: return (lo + ah::Next() % n) | 0x100u;
        default: return ah::Next();
        }
    };
    const auto exact = [](U v) { return ah::Often() ? v : AH_PICK(1, 0x8000, 0x10000, 0xFFFFFFFFu) + v; };
    // the seed's case's cell two rounds in three, another's otherwise
    static const unsigned kCell[6] = {0, 1, 2, 2, 3, 4};
    switch (ah::Often() ? kCell[g_case06] : ah::Next() % 5) {
    case 0: a[1] = exact(0x248000); a[0] = At16(near(0x11, 3), a[0]); break;
    case 1: a[1] = exact(0xA8000); a[0] = At16(near(0x12, 2), a[0]); break;
    case 2: a[1] = exact(0x258000); a[0] = At16(near(0x1A, 2), a[0]); break;
    case 3: a[0] = exact(0x268000); a[1] = At16(near(0x24, 2), a[1]); break;
    default: a[0] = exact(0x188000); a[1] = At16(near(0x15, 2), a[1]); break;
    }
}
void Disturb10x(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 4) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 0x12 : v); break;
    case 1: B(at::kCondFD) = static_cast<unsigned char>(h & 0x100 ? v & 1 : v); break;
    case 2: ah::SetPointer(at::kActiveMember, SomeRecord(h >> 16)); break;
    default: ah::SetPointer(at::kScriptObject, SomeRecord(h >> 16)); break;
    }
}

// BOF3X_AR2E_AREA=n runs area n's groups alone (the controls script's
// shortcut; 1040 is area 104's world-map copy alone); unset, every area runs.
bool Wants(int area) {
    const char* const only = std::getenv("BOF3X_AR2E_AREA");
    return only == nullptr || *only == 0 || std::atoi(only) == area;
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    std::memcpy(g_saved_anims, Mem(kFzPlateAnims), sizeof g_saved_anims);
    if (Wants(104) || Wants(1040)) {
        g_regions_wm[kRegPackets].at = Key(g_packets);
        ah::Group g{"area_w2e", kClonesWm, AH_COUNT(kClonesWm), kCallees, AH_COUNT(kCallees), kTablesWm, AH_COUNT(kTablesWm),
                    g_regions_wm, AH_COUNT(g_regions_wm), &SeedWm, &DisturbWm, 4000};
        g.settle = &SettleWm;
        g.phase_span = 5;
        g.args = &ArgsWm;
        g.area = 104;
        ah::Run(g);
    }
    if (Wants(104)) {
        std::memcpy(g_regions104, kRegions104, sizeof kRegions104);
        g_regions104[kReg104Packets].at = Key(g_packets);
        ah::Group g{"area_w2e", kClones104, AH_COUNT(kClones104), kCallees, AH_COUNT(kCallees), kTables104, AH_COUNT(kTables104),
                    g_regions104, AH_COUNT(g_regions104), &Seed104, &Disturb104, kRounds};
        g.settle = &Settle104;
        g.args = &Args104;
        g.area = 104;
        ah::Run(g);
    }
    if (Wants(105)) {
        ah::Group g{"area_w2e", kClones105, AH_COUNT(kClones105), kCallees, AH_COUNT(kCallees), nullptr, 0,
                    kRegions10x, AH_COUNT(kRegions10x), &Seed105, &Disturb10x, kRounds};
        g.args = &Args105;
        g.area = 105;
        ah::Run(g);
    }
    if (Wants(106)) {
        ah::Group g{"area_w2e", kClones106, AH_COUNT(kClones106), kCallees, AH_COUNT(kCallees), nullptr, 0,
                    kRegions10x, AH_COUNT(kRegions10x), &Seed106, &Disturb10x, kRounds};
        g.args = &Args106;
        g.area = 106;
        ah::Run(g);
    }
}

}  // namespace area_w2e
