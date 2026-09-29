// BOF3X_SHADOW=field_e1: group FE1's 45 functions through the scenario
// harness in field mode (scenario_harness.h, docs/scenario_harness.md section
// 7), once at start-up. docs/field_e1.md section 4.
//
// The clone rows are tools/band_rows.py's (--group FE1 --clones, 2026-09-29),
// each read against the disassembly to its last instruction (the tool's
// extents; the cut's sizes are padding, section 2 of the doc). The shapes: the
// leader's states and the jump's steps kSprite (Sprite_Current and Field_State
// an ObjTrio record, the dispatch byte seeded inside its table per function),
// the helpers with arguments or an answer kCall, the rest kState. The five
// .data tables the dispatchers go through are DataTables (their entries
// recorders while the fuzz runs).
//
// Callees re-listed here with the width the callee reads (the brief's "masks"
// note: Capcom pushes whole registers whose upper bytes ours cannot hold), each
// cited in docs/field_e1.md section 5; and the callees the standard set does not
// hold: the group's own functions called by E8 (by name) and FE2's (raw).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_e1.h"
#include "game/field_e1_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/log.h"

namespace field_e1 {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
unsigned char& B(U a) { return sh::Mem(a)[0]; }
unsigned char* Member(unsigned i) { return ObjTrio + (i % 3) * at::kObjStride; }
unsigned char* Object(unsigned i) { return Sprite_Objects + i * at::kSpriteStride; }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the clone table (band_rows.py --clones, 2026-09-29) --------------------------
constexpr sh::CallSite kCalls52D080[] = {{0x6, 0x52CF60}, {0x1C, 0x52CFE0}, {0x2D, 0x52CFE0}};
constexpr sh::CallSite kCalls52D0C0[] = {{0x4, 0x52CF60}, {0x17, 0x52CFE0}};
constexpr sh::CallSite kCalls52D140[] = {{0x27, 0x52CF60}, {0x95, 0x52CFE0}, {0x156, 0x516B30}, {0x178, 0x5B9380}, {0x196, 0x516B30}, {0x1A3, 0x52CE60}, {0x1B8, 0x5B9380}, {0x1CF, 0x516B30}};
constexpr sh::CallSite kCalls52D320[] = {{0x155, 0x52CF60}, {0x168, 0x52CFE0}, {0x176, 0x52CFE0}, {0x17E, 0x52CED0}, {0x1B2, 0x52CFE0}, {0x1DD, 0x52CFE0}, {0x1FF, 0x52CFE0}, {0x218, 0x5B9380}, {0x22D, 0x516B30}};
constexpr sh::CallSite kCalls52D560[] = {{0x6, 0x52CF60}, {0x19, 0x52CFE0}, {0x50, 0x516B30}};
constexpr sh::CallSite kCalls52D5C0[] = {{0x5, 0x52CF60}, {0x11, 0x5A7740}, {0x3A, 0x5A7780}, {0x43, 0x461E50}};
constexpr sh::CallSite kCalls52D610[] = {{0x20, 0x57CF60}, {0x29, 0x52CF60}, {0x34, 0x52CFE0}, {0x48, 0x52CFE0}, {0x5F, 0x52CFE0}, {0x6D, 0x52CFE0}, {0x7B, 0x468950}, {0x86, 0x468950}, {0x97, 0x52CFE0}, {0xAB, 0x52CFE0}, {0xC2, 0x52CFE0}, {0xE6, 0x516B30}, {0x10D, 0x516B30}, {0x12E, 0x516B30}};
constexpr sh::CallSite kCalls52D750[] = {{0x23, 0x57CF60}, {0x2C, 0x52CF60}, {0x37, 0x52CFE0}, {0x4B, 0x52CFE0}, {0x65, 0x52CFE0}, {0x76, 0x52CFE0}, {0x84, 0x468950}, {0x8F, 0x468950}, {0xA0, 0x52CFE0}, {0xB4, 0x52CFE0}, {0xCE, 0x52CFE0}, {0xF5, 0x516B30}, {0x119, 0x516B30}};
constexpr sh::CallSite kCalls52D8C0[] = {{0x16, 0x52CF60}, {0x23, 0x52CFE0}};
constexpr sh::CallSite kCalls52EC20[] = {{0x8E, 0x572570}, {0xBE, 0x535610}, {0x102, 0x572570}, {0x12E, 0x535610}};
constexpr sh::CallSite kCalls52F4F0[] = {{0x41, 0x589330}, {0x52, 0x536730}, {0x6D, 0x52DA70}};
constexpr sh::CallSite kCalls52F5C0[] = {{0x0, 0x589410}};
constexpr sh::CallSite kCalls52F8F0[] = {{0x3A, 0x56D6B0}};
constexpr sh::CallSite kCalls52F970[] = {{0x0, 0x535FC0}};
constexpr sh::CallSite kCalls52F9A0[] = {{0x0, 0x535FE0}, {0x24, 0x534710}};
constexpr sh::CallSite kCalls52F9E0[] = {{0x0, 0x536050}};
constexpr sh::CallSite kCalls52FA00[] = {{0x0, 0x5360C0}};
constexpr sh::CallSite kCalls52FA20[] = {{0x0, 0x536130}};
constexpr sh::CallSite kCalls52FA40[] = {{0x0, 0x536170}};
constexpr sh::CallSite kCalls52FA60[] = {{0xA, 0x5364D0}, {0x16, 0x536550}, {0x26, 0x5365D0}, {0x47, 0x5725F0}, {0x4F, 0x534710}};
constexpr sh::CallSite kCalls52FAE0[] = {{0x0, 0x536290}};
constexpr sh::CallSite kCalls52FAF0[] = {{0x0, 0x5362D0}};
constexpr sh::CallSite kCalls52FB10[] = {{0x0, 0x5363C0}, {0x28, 0x534710}};
constexpr sh::CallSite kCalls52FB50[] = {{0x0, 0x536440}};
constexpr sh::CallSite kCalls52FBB0[] = {{0x12, 0x5893A0}};
constexpr sh::CallSite kCalls52FBD0[] = {{0x25, 0x57C140}, {0x33, 0x497710}, {0x81, 0x5307C0}, {0xAE, 0x57C0F0}, {0xCC, 0x591680}, {0xFA, 0x590BB0}, {0x12C, 0x57C0F0}, {0x139, 0x587740}, {0x140, 0x497710}, {0x157, 0x497710}, {0x192, 0x5891F0}};
constexpr sh::CallSite kCalls52FD90[] = {{0x4B, 0x5891F0}, {0xAB, 0x579F00}, {0xDA, 0x589330}};
constexpr sh::CallSite kCalls52FE90[] = {{0xA, 0x415020}, {0xF, 0x41B9D0}};
constexpr sh::CallSite kCalls5307C0[] = {{0x6, 0x587740}, {0x1A, 0x5B9380}, {0x21, 0x497710}, {0x30, 0x591BE0}};
constexpr sh::CallSite kCalls531120[] = {{0x86, 0x536700}, {0xEC, 0x536700}, {0x12D, 0x531540}, {0x170, 0x531540}, {0x1B4, 0x536700}, {0x240, 0x536700}, {0x29E, 0x536700}, {0x2F8, 0x536700}, {0x38B, 0x536700}, {0x3E8, 0x536700}};
constexpr sh::CallSite kCalls531540[] = {{0x8B, 0x536700}, {0xD9, 0x536700}};
constexpr sh::CallSite kCalls531820[] = {{0x3A, 0x531920}};
constexpr sh::CallSite kCalls532C10[] = {{0xB9, 0x5720C0}};
constexpr sh::CallSite kCalls532D10[] = {{0x1E, 0x5893A0}};
constexpr sh::CallSite kCalls532D50[] = {{0xAF, 0x572570}, {0x134, 0x534EC0}, {0x143, 0x5891F0}, {0x169, 0x531F90}, {0x171, 0x517350}};
constexpr sh::CallSite kCalls5338B0[] = {{0x21, 0x533A50}, {0x2C, 0x533A50}};
constexpr sh::CallSite kCalls5338F0[] = {{0x2, 0x533A50}};
constexpr sh::CallSite kCalls533900[] = {{0x0, 0x5339A0}, {0x1E, 0x589330}, {0x3F, 0x572570}, {0x56, 0x536650}};
#define FE1_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define FE1_FN(name) reinterpret_cast<const void*>(&::name)
#define FE1_ROW(name, base, size, calls) #name, base, size, calls, FE1_N(calls), nullptr, 0, nullptr, 0, FE1_FN(name)
#define FE1_LEAF(name, base, size) #name, base, size, nullptr, 0, nullptr, 0, nullptr, 0, FE1_FN(name)
constexpr sh::Shape kSp = sh::Shape::kSprite, kCa = sh::Shape::kCall, kSt = sh::Shape::kState;
const sh::Clone kClones[] = {
    {FE1_ROW(FieldPanel_DrawHeader, 0x52D080, 0x38, kCalls52D080), 0, false, kCa},
    {FE1_ROW(FieldPanel_DrawKindIcon, 0x52D0C0, 0x7B, kCalls52D0C0), 0, false, kCa},
    {FE1_ROW(FieldPanel_DrawKindRow, 0x52D140, 0x1DD, kCalls52D140), 0, false, kCa},
    {FE1_ROW(FieldPanel_DrawTotal, 0x52D320, 0x23D, kCalls52D320), 0, false, kCa},
    {FE1_ROW(FieldPanel_DrawMessage, 0x52D560, 0x5B, kCalls52D560), 0, false, kCa},
    {FE1_ROW(FieldPanel_DrawShade, 0x52D5C0, 0x4D, kCalls52D5C0), 0, false, kSt},
    {FE1_ROW(FieldPanel_DrawBox3, 0x52D610, 0x13B, kCalls52D610), 0, false, kCa},
    {FE1_ROW(FieldPanel_DrawBox2, 0x52D750, 0x126, kCalls52D750), 0, false, kCa},
    {FE1_LEAF(Inventory_Holds38To4DAt99, 0x52D880, 0x39), 0xFF, false, kCa},
    {FE1_ROW(FieldPanel_DrawBlink, 0x52D8C0, 0x2C, kCalls52D8C0), 0, false, kSt},
    {FE1_ROW(Field_PathClear, 0x52EC20, 0x15D, kCalls52EC20), 0xFF, false, kCa},
    {FE1_ROW(Field_FormActionState, 0x52F4F0, 0x73, kCalls52F4F0), 0, false, kSp},
    {FE1_ROW(PartyAction_ScriptEnd, 0x52F5C0, 0x16, kCalls52F5C0), 0, false, kSp},
    {FE1_ROW(Field_PassageTrigger, 0x52F8F0, 0x53, kCalls52F8F0), 0, false, kSp},
    {FE1_LEAF(Field_JumpState, 0x52F950, 0x12), 0, false, kSp},
    {FE1_ROW(Field_JumpBegin, 0x52F970, 0xF, kCalls52F970), 0, false, kSp},
    {FE1_LEAF(Field_JumpOut, 0x52F980, 0x12), 0, false, kSp},
    {FE1_ROW(Field_JumpOut0, 0x52F9A0, 0x39, kCalls52F9A0), 0, false, kSp},
    {FE1_ROW(Field_JumpOut1, 0x52F9E0, 0x13, kCalls52F9E0), 0, false, kSp},
    {FE1_ROW(Field_JumpOut2, 0x52FA00, 0x13, kCalls52FA00), 0, false, kSp},
    {FE1_ROW(Field_JumpOut3, 0x52FA20, 0x13, kCalls52FA20), 0, false, kSp},
    {FE1_ROW(Field_JumpOut4, 0x52FA40, 0x1D, kCalls52FA40), 0, false, kSp},
    {FE1_ROW(Field_JumpAir, 0x52FA60, 0x55, kCalls52FA60), 0, false, kSp},
    {FE1_LEAF(Field_JumpIn, 0x52FAC0, 0x12), 0, false, kSp},
    {FE1_ROW(Field_JumpIn0, 0x52FAE0, 0xF, kCalls52FAE0), 0, false, kSp},
    {FE1_ROW(Field_JumpIn1, 0x52FAF0, 0x13, kCalls52FAF0), 0, false, kSp},
    {FE1_ROW(Field_JumpIn2, 0x52FB10, 0x38, kCalls52FB10), 0, false, kSp},
    {FE1_ROW(Field_JumpIn3, 0x52FB50, 0x5, kCalls52FB50), 0, false, kSp},
    {FE1_ROW(Field_ContentState, 0x52FBB0, 0x17, kCalls52FBB0), 0, false, kSp},
    {FE1_ROW(Field_ContentTake, 0x52FBD0, 0x1B4, kCalls52FBD0), 0, false, kSp},
    {FE1_ROW(Field_ContentEnd, 0x52FD90, 0xF7, kCalls52FD90), 0, false, kSp},
    {FE1_ROW(Field_AreaRunState, 0x52FE90, 0x14, kCalls52FE90), 0, false, kSp},
    {FE1_ROW(Field_GiveZenny, 0x5307C0, 0x3A, kCalls5307C0), 0, false, kCa},
    {FE1_ROW(Field_CellAroundLarge, 0x531120, 0x420, kCalls531120), 0xFF, false, kCa},
    {FE1_ROW(Field_CellAroundSide, 0x531540, 0x116, kCalls531540), 0xFF, false, kCa},
    {FE1_ROW(Field_GatewayExit, 0x531820, 0xF9, kCalls531820), 0xFF, false, kCa},
    {FE1_ROW(Party_PlaceAtSlots, 0x532C10, 0xF6, kCalls532C10), 0, false, kSt},
    {FE1_ROW(Party_ScriptTicks, 0x532D10, 0x38, kCalls532D10), 0, false, kSt},
    {FE1_ROW(Party_PlacesByList, 0x532D50, 0x17A, kCalls532D50), 0, false, kSt},
    {FE1_LEAF(Field_LeaderPlaceOffset, 0x533690, 0xCF), 0, false, kCa},
    {FE1_ROW(Field_PendingJumpTurn, 0x5338B0, 0x33, kCalls5338B0), 0, false, kSt},
    {FE1_ROW(Field_PendingJumpKind4, 0x5338F0, 0x9, kCalls5338F0), 0, false, kSt},
    {FE1_ROW(Field_PendingDrop, 0x533900, 0x92, kCalls533900), 0, false, kSt},
    {FE1_LEAF(Field_PendingNext, 0x5339A0, 0xA3), 0xFF, false, kCa},
    {FE1_LEAF(Field_PendingRelease, 0x533A50, 0x150), 0xFF, false, kCa},
};
#undef FE1_ROW
#undef FE1_LEAF
enum : unsigned {
    kHeader, kKindIcon, kKindRow, kTotal, kMessage, kShade, kBox3, kBox2, kHolds, kBlink, kPath, kFormAction, kScriptEnd,
    kPassage, kJumpState, kJumpBegin, kJumpOut, kJumpOut0, kJumpOut1, kJumpOut2, kJumpOut3, kJumpOut4, kJumpAir, kJumpIn,
    kJumpIn0, kJumpIn1, kJumpIn2, kJumpIn3, kContentState, kContentTake, kContentEnd, kAreaRun, kGiveZenny, kAroundLarge,
    kAroundSide, kGateway, kPlaceSlots, kScriptTicks, kPlacesByList, kPlaceOffset, kPendingTurn, kPendingKind4, kPendingDrop,
    kPendingNext, kPendingRelease, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

// The .data tables the dispatchers read in place: to the next table's start.
const sh::DataTable kTables[] = {
    {at::kJumpSteps, 4}, {at::kJumpOutSteps, 5}, {at::kJumpInSteps, 4}, {at::kContentSteps, 2}, {at::kFormActions, 19},
};

// --- the stand-ins ------------------------------------------------------------------

// The cell code the round's AreaMap_ByteAt answers a third of the time (the
// argument the cell searches look for; set by Args, the same on both passes).
U g_code;

// 0x52CFE0: the primitive at the packet cursor, the cursor moved past its
// 0x1C bytes while the buffer has room (the real one commits it) - its callers
// write through the answer.
U FxSprite(const U*, U) {
    unsigned char* const p = sh::Pointer(sh::at::kPacketNext);
    if (p >= sh::Packets() && p + 0x1C + 0x40 <= sh::Packets() + 0x800) sh::SetPointer(sh::at::kPacketNext, p + 0x1C);
    return Key(p);
}
// 0x52CED0: the total, drawn near the thresholds FieldPanel_DrawTotal
// compares (never 0xFFFF: there the original walks its stack, section 7).
U FxTotal(const U*, U answer) {
    static const std::uint16_t kT[] = {100, 300, 500, 1000, 1500, 2000, 3000, 4000, 5000, 7000, 9000, 9500};
    const U n = sh::Noise();
    std::uint16_t v = n % 3 == 0 ? static_cast<std::uint16_t>(answer) : static_cast<std::uint16_t>(kT[(n >> 4) % 12] + (n >> 8) % 3 - 1);
    if (v == 0xFFFF) v = 0xFFFE;
    return (answer & 0xFFFF0000u) | v;
}
// Crt_sprintf (3 words: the 4th the standard lists is the caller's frame):
// up to seven letters and a NUL where the buffer is in the regions.
U FxSprintf(const U* a, U) {
    auto* const dst = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    if (!sh::InRegions(dst, 8)) return 0;
    const unsigned n = sh::Noise() % 8;
    for (unsigned i = 0; i < n; ++i) dst[i] = static_cast<unsigned char>('0' + sh::Noise() % 43);
    dst[n] = 0;
    return n;
}
// Item_NamePtr: a name in the harness's text buffer (Field_ContentTake copies
// 16 bytes from it).
U FxName(const U*, U answer) { return Key(sh::Text() + (answer & 0xF0)); }
// Field_ObjectTrigger (FE2's): the record's pointer is the caller's frame; what
// it hands on is +0x86 and the word +0x88.
U FxTrigger(const U* a, U answer) {
    const auto* const p = reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a[0]));
    sh::NoteBytes(p + 0x86, 4);
    return answer;
}
// MapView_GroundAt: two times in three within 0x48 of the leader's height, so
// Field_PathClear's steps pass its 0x40 test as often as not.
U FxGround(const U*, U answer) {
    const U n = sh::Noise();
    if (n % 3 == 0) return answer;
    const auto h = static_cast<std::uint16_t>(Word(sh::Mem(at::kLeaderHeight)) + (n >> 4) % 0x91 - 0x48);
    return (answer & 0xFFFF0000u) | h;
}
// Field_WayBlocked: open (al 0) seven times in eight.
U FxWay(const U*, U answer) { return sh::Noise() % 8 ? answer & 0xFFFFFF00u : answer | 1; }
// AreaMap_ByteAt: the searched code a third of the time.
U FxByteAt(const U*, U answer) { return sh::Noise() % 3 == 0 ? (answer & 0xFFFFFF00u) | (g_code & 0xFF) : answer; }
// Field_PendingNext (the group's own, called by Field_PendingDrop): when it
// answers al not 0 a member is made Sprite_Current and Field_State, as the
// real one does.
U FxNext(const U*, U answer) {
    if (answer & 0xFF) {
        unsigned char* const m = Member(sh::Noise());
        Sprite_Current = m;
        Field_State = m;
    }
    return answer;
}

#define FE1_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage, kF = sh::Answer::kFlag;
const sh::Callee kCallees[] = {
    // the engine's panel sprite (0x52CFE0): reads the sprite as a byte (+0x34
    // and eax, 0xFF), the slot whole (Gfx_CommitPrim's), x and y as words
    // (+0x1F / +0x11 movsx)
    {"0x52CFE0", at::kDrawSprite, at::kDrawSprite, 4, {kU8, kAll, kU16, kU16}, kG, 0, 0, {}, &FxSprite, nullptr, true},
    {"0x52CED0", at::kKindTotal, at::kKindTotal, 0, {}, kG, 0, 0, {}, &FxTotal, nullptr, true},
    // standard entries re-listed with the width the callee reads
    {FE1_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, kAll}, kG, 0, 0, {0, 0, 0, 0, 16}, nullptr, nullptr, true},
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 3, {kAll, 0, kAll}, kG, 0, 0, {0, 16, 0}, &FxSprintf, nullptr, true},
    {FE1_OURS(Menu_DrawBox), 6, {kAll, kAll, kAll, kAll, kAll, kU8}, kG, 0, 0, {}, nullptr, nullptr, true},
    {FE1_OURS(Sprite_EnsureAnimation), 1, {kU8}, kF, 0, 0, {}, nullptr, nullptr, true},
    {FE1_OURS(Item_NamePtr), 2, {kU8, kU8}, kG, 0, 0, {}, &FxName, nullptr, true},
    {FE1_OURS(Inventory_Add), 3, {kU8, kU8, kU8}, kF, 0, 0, {}, nullptr, nullptr, true},
    {FE1_OURS(AreaMap_SetByte), 3, {kU16, kU16, kU8}, kG, 0, 0, {}, nullptr, nullptr, true},
    {FE1_OURS(MapView_SetElevation), 1, {kU16}, kG, 0, 0, {}, nullptr, nullptr, true},
    {FE1_OURS(Field_CellHasEvent), 2, {kU16, kU16}, kF, 0, 0, {}, nullptr, nullptr, true},
    {FE1_OURS(MapView_GroundAt), 2, {kAll, kAll}, kG, 0, 0, {}, &FxGround, nullptr, true},
    {FE1_OURS(Field_WayBlocked), 4, {kAll, kAll, kAll, kAll}, kF, 0, 0, {}, &FxWay, nullptr, true},
    {FE1_OURS(AreaMap_ByteAt), 2, {kU16, kU16}, kF, 0, 0, {}, &FxByteAt, nullptr, true},
    // the group's own, called by E8
    {FE1_OURS(Field_GiveZenny), 1, {kAll}, kG, 0, 0},
    {FE1_OURS(Field_CellAroundSide), 2, {kU8, kU8}, kF, 0, 0},
    {FE1_OURS(Field_PendingNext), 0, {}, kF, 0, 0, {}, &FxNext},
    {FE1_OURS(Field_PendingRelease), 1, {kU8}, kF, 0, 0},
    // FE2's (round twelve wave two), raw until it merges
    {"Field_ObjectTrigger", at::kObjectTrigger, at::kObjectTrigger, 1, {0}, kG, 0, 0, {}, &FxTrigger},
    {"0x535FC0", at::kJumpPose, at::kJumpPose, 0, {}, kG, 0, 0},
    {"0x535FE0", at::kJumpSetUp, at::kJumpSetUp, 0, {}, kG, 0, 0},
    {"0x536050", at::kJumpOut1, at::kJumpOut1, 0, {}, kF, 0, 0},
    {"0x5360C0", at::kJumpOut2, at::kJumpOut2, 0, {}, kF, 0, 0},
    {"0x536130", at::kJumpOut3, at::kJumpOut3, 0, {}, kF, 0, 0},
    {"0x536170", at::kJumpOut4, at::kJumpOut4, 0, {}, kF, 0, 0},
    {"0x5364D0", at::kJumpAirA, at::kJumpAirA, 0, {}, kG, 0, 0},
    {"0x536550", at::kJumpAirB, at::kJumpAirB, 0, {}, kG, 0, 0},
    {"0x5365D0", at::kJumpAirC, at::kJumpAirC, 0, {}, kG, 0, 0},
    {"0x536290", at::kJumpIn0, at::kJumpIn0, 0, {}, kG, 0, 0},
    {"0x5362D0", at::kJumpIn1, at::kJumpIn1, 0, {}, kF, 0, 0},
    {"0x5363C0", at::kJumpIn2, at::kJumpIn2, 0, {}, kF, 0, 0},
    {"0x536440", at::kJumpIn3, at::kJumpIn3, 0, {}, kG, 0, 0},
};
#undef FE1_OURS

// --- the state --------------------------------------------------------------------

// Beyond field mode's standard regions.
const sh::Region kRegions[] = {
    {0x904160, 0x280},     // the inventory's id and count lists past 0x904160 (Inventory_Holds38To4DAt99)
    {0x7DEE20, 0x60},      // the message cells: the message word 0x7DEE48 (Field_ContentEnd)
    {0x903860, 0x10},      // the exit's x 0x903860 (Field_GatewayExit)
    {0x937F80, 8},         // the exit's area 0x937F82
    {0x7E06E0, 0x20},      // the slot positions (Party_PlaceAtSlots)
    {0x904AA0, 0x50},      // the formation 0x904AAC and the bits 0x904AE5
    {0x92BF18, 4},         // Party_DropIn's entry
    {0x904EF0, 4},         // the pending jump's flag and member
    {0x904CE0, 0x40},      // Text_Records' first two (the zenny printed, a content's name)
    {0x939A28, 4},         // the blink byte
    {0x7E1BE0, 8},         // Cond_ByteFF 0x7E1BE2 (area 0xBD's gateway)
};

// What the functions read again after a call, moved by the harness's
// disturbance (case 14) from its hash alone.
void Disturb(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const fs = Field_State;
    switch ((h >> 16) % 12) {
    case 0: if (sh::InRegions(fs, 0x14C)) fs[0x137] = static_cast<unsigned char>(v & 1); break;
    case 1: if (sh::InRegions(fs, 0x14C)) fs[0x139] = static_cast<unsigned char>(v % 30); break;
    case 2: if (sh::InRegions(fs, 0x14C)) { unsigned char* const o = Object(fs[0x139] % 30); o[5] = static_cast<unsigned char>(v); o[0xB] ^= 1; } break;
    case 3: Field_MemberCount = static_cast<unsigned char>(v % 4); break;
    case 4: B(at::kPendingCount) = static_cast<unsigned char>(v % 3); break;
    case 5: SetLong(sh::Mem(at::kLeaderX + (v & 1) * 4), Long(sh::Mem(at::kLeaderX + (v & 1) * 4)) + static_cast<std::int32_t>(h >> 24) * 0x800 - 0x40000); break;
    case 6: SetWord(sh::Mem(at::kLeaderHeight), Word(sh::Mem(at::kLeaderHeight)) + (v & 0x3F) - 0x20); break;
    case 7: Sprite_Current = Member(v); break;
    case 8: Field_State = Member(v); break;
    case 9: if (sh::InRegions(sc, 0x10)) sc[8] = static_cast<unsigned char>(v & 7); break;
    case 10: SetWord(sh::Mem(at::kMessageWord), v & 1 ? 3u : h >> 20); break;
    case 11: Member(v)[1] = static_cast<unsigned char>(v & 2 ? 1 : v >> 2); break;
    default: break;
    }
}

// --- the seed -----------------------------------------------------------------------

bool IsLeaderState(unsigned k) {
    return k == kFormAction || k == kScriptEnd || k == kPassage || (k >= kJumpState && k <= kAreaRun) || k == kPendingDrop;
}

void Seed(unsigned k) {
    Field_MemberCount = static_cast<unsigned char>(sh::Often() ? 1 + sh::Next() % 3 : sh::Next() % 4);
    B(at::kPendingCount) = static_cast<unsigned char>(sh::Next() % 3);
    if (IsLeaderState(k) || sh::Half()) {
        unsigned char* const m = sh::Often() ? ObjTrio : Member(sh::Next());
        Sprite_Current = m;
        Field_State = sh::Often() ? m : Member(sh::Next());
    }
    unsigned char* const sc = Sprite_Current;
    unsigned char* const fs = Field_State;
    sc[8] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : sh::Next() % 12);
    B(at::kPartySet) = static_cast<unsigned char>(sh::Next() % 19 | (sh::Half() ? 0x80 : 0));
    fs[0x139] = static_cast<unsigned char>(sh::Next() % 30);
    switch (k) {
    case kTotal: break;
    case kHolds: {
        // the 22 ids at 99 in the first slots, then one taken away, doubled or moved
        for (unsigned i = 0; i < 0x80; ++i) B(at::kItemIds + i) = static_cast<unsigned char>(sh::Next() % 0x60);
        if (sh::Often()) {
            for (unsigned i = 0; i < 22; ++i) {
                const unsigned slot = sh::Half() ? i : sh::Next() % 0x80;
                B(at::kItemIds + slot) = static_cast<unsigned char>(0x38 + i);
                B(at::kItemCounts + slot) = 0x63;
            }
            const unsigned j = sh::Next() % 0x80;
            switch (sh::Next() % 4) {
            case 0: B(at::kItemCounts + j) = static_cast<unsigned char>(PickOf(0x62, 0x64, 0)); break;
            case 1: B(at::kItemIds + j) = static_cast<unsigned char>(PickOf(0x37, 0x4E, 0x38 + sh::Next() % 22)); break;
            default: break;
            }
        }
        break;
    }
    case kBlink:
        B(at::kBlink) = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        break;
    case kPath:
        SetWord(ObjTrio + 0x3E, sh::Next() % 0x400);
        break;
    case kFormAction:
        Field_InputHeld = static_cast<unsigned short>(sh::Half() ? 0 : sh::Next());
        fs[0x137] = static_cast<unsigned char>(sh::Next() & 1);
        break;
    case kPassage:
        Field_Request = static_cast<unsigned char>(PickOf(0, 1, 2, 2, sh::Next()));
        fs[0x12A] = static_cast<unsigned char>(sh::Half() ? 0xFF : sh::Next());
        break;
    case kJumpState: sc[2] = static_cast<unsigned char>(sh::Next() % 4); break;
    case kJumpOut: sc[3] = static_cast<unsigned char>(sh::Next() % 5); break;
    case kJumpIn: sc[3] = static_cast<unsigned char>(sh::Next() % 4); break;
    case kContentState: sc[2] = static_cast<unsigned char>(sh::Next() % 2); break;
    case kJumpOut0:
    case kJumpIn2:
        sc[5] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        B(at::kFlags2Lo) = static_cast<unsigned char>(B(at::kFlags2Lo) & (sh::Often() ? ~8u : 0xFFu));
        B(at::kFlagsLo) = static_cast<unsigned char>(B(at::kFlagsLo) & (sh::Often() ? ~8u : 0xFFu));
        break;
    case kJumpAir: {
        const U held = PickOf(0x1000, 0x4000, 0x5000, 0, 1u << (sh::Next() % 16), sh::Next());
        Field_InputHeld = static_cast<unsigned short>(held);
        SetWord(sh::Mem(at::kButtonMap), sh::Half() ? held & 0xAFFF : sh::Next());
        B(at::kFlags2Lo) = static_cast<unsigned char>(B(at::kFlags2Lo) & (sh::Often() ? ~8u : 0xFFu));
        B(at::kFlagsLo) = static_cast<unsigned char>(B(at::kFlagsLo) & (sh::Often() ? ~8u : 0xFFu));
        break;
    }
    case kContentTake:
    case kContentEnd: {
        unsigned char* const o = Object(fs[0x139]);
        o[5] = static_cast<unsigned char>(sh::Half() ? 0xFF : sh::Next());
        o[0x18] = static_cast<unsigned char>(sh::Half() ? 0xFF : sh::Next() % 6);
        o[0xB] = static_cast<unsigned char>(sh::Next());
        if (sh::Half()) SetWord(o + 0x34, 0);
        if (sh::Half()) SetWord(o + 0x38, 0);
        Field_Request = static_cast<unsigned char>(sh::Often() ? 0 : sh::Next());
        SetWord(sh::Mem(at::kMessageWord), sh::Half() ? 3u : sh::Next() % 8);
        Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ (sh::Half() ? 0x20 : 0));
        break;
    }
    case kAreaRun: Game_AreaNumber = static_cast<unsigned short>(sh::Half() ? 0x68 : PickOf(0x79, 0x67, 0x69, sh::Next())); break;
    case kAroundLarge:
    case kAroundSide:
        SetWord(sc + 0x34, sh::Half() ? 0 : sh::Next());
        SetWord(sc + 0x38, sh::Half() ? 0 : sh::Next());
        break;
    case kGateway: {
        Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags & (sh::Often() ? 0xBFFF : 0xFFFF));
        Field_ScriptFlags2 = static_cast<unsigned short>(Field_ScriptFlags2 & (sh::Often() ? 0xEFFF : 0xFFFF));
        if (sh::Next() % 6 == 0) B(at::kPartySet) = static_cast<unsigned char>(0xC | (sh::Half() ? 0x80 : 0));
        const U pick = sh::Next() % 4;
        if (pick == 0) Game_AreaNumber = 0xBD;
        else if (pick != 3) Game_AreaNumber = Word(sh::Mem(at::kGatewayExits + 8 * (sh::Next() % 10)));
        Cond_ByteFF = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        break;
    }
    case kPlaceSlots: {
        B(at::kLeaderList) = Member(sh::Next())[0x89];
        B(at::kBattleBits) = static_cast<unsigned char>(sh::Next());
        const unsigned m = sh::Next() % 3;
        const std::int32_t x = Long(sh::Mem(at::kSlotPositions + 8 * m)), z = Long(sh::Mem(at::kSlotPositions + 8 * m + 4));
        if (sh::Often()) {
            Field_Kind2X = x + static_cast<std::int32_t>(sh::Next() % 0x40000) - 0x20000;
            Field_Kind2Z = z + static_cast<std::int32_t>(sh::Next() % 0x40000) - 0x20000;
        }
        break;
    }
    case kPlacesByList:
        for (unsigned i = 0; i < 3; ++i) B(at::kSecondList + i) = Member(sh::Often() ? i + sh::Next() % 2 : sh::Next())[0x89];
        B(at::kBattleBits) = static_cast<unsigned char>(sh::Next());
        break;
    case kPlaceOffset:
        for (unsigned i = 0; i < 3; ++i) Member(i)[0x70] = static_cast<unsigned char>(sh::Half() ? 0 : sh::Next());
        break;
    case kPendingTurn:
    case kPendingKind4:
    case kPendingNext:
    case kPendingRelease:
    case kPendingDrop: {
        const unsigned n = Field_MemberCount;
        B(at::kPendingCount) = static_cast<unsigned char>(n > 1 ? sh::Next() % n : sh::Next() % 2);
        for (unsigned i = 0; i < 3; ++i) {
            Member(i)[1] = static_cast<unsigned char>(sh::Often() ? 1 : sh::Next() % 4);
            Member(i)[8] = static_cast<unsigned char>(PickOf(5, 3, sh::Next() % 8));
        }
        break;
    }
    default: break;
    }
}

// The arguments each function reads (garbage above the bytes it masks).
void Args(unsigned k, U* a) {
    g_code = sh::Next() & 0xFF;
    switch (k) {
    case kKindIcon: a[2] = (a[2] & 0xFFFFFF00u) | PickOf(0xFF, 0x16, sh::Next() % 0x40, sh::Next() & 0xFF); break;
    case kKindRow:
        a[0] = (a[0] & 0xFFFFFF00u) | PickOf(0xFF, 0x16, sh::Next() % 0x20, sh::Next() & 0xFF);
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Often() ? sh::Next() % 4 : sh::Next() & 0xFF);
        break;
    case kMessage: a[2] = sh::Half() ? (a[2] | 0xFFFF) : (a[2] & 0xFFFF0000u) | (sh::Next() % 0x200); break;
    case kPath: {
        const U lx = static_cast<U>(Long(sh::Mem(at::kLeaderX))), lz = static_cast<U>(Long(sh::Mem(at::kLeaderZ)));
        a[0] = sh::Half() ? lx + sh::Next() % 0x60000 - 0x30000 : PickOf(lx, lx + 0x8000, lx - 0x8000, lx + 0x7FFF);
        a[1] = sh::Half() ? lz + sh::Next() % 0x60000 - 0x30000 : PickOf(lz, lz + 0x8000, lz - 0x8000, lz + 0x10000);
        break;
    }
    case kGiveZenny: a[0] = sh::Often() ? sh::Next() % 10000 : a[0]; break;
    case kAroundLarge: a[0] = (a[0] & 0xFFFFFF00u) | g_code; break;
    case kAroundSide:
        a[0] = (a[0] & 0xFFFFFF00u) | PickOf(1, 5, 7, 3, sh::Next() % 8, sh::Next() & 0xFF);
        a[1] = (a[1] & 0xFFFFFF00u) | g_code;
        break;
    case kPlaceOffset: a[2] = (a[2] & 0xFFFFFF00u) | (sh::Often() ? sh::Next() % 8 : sh::Next() & 0xFF); break;
    case kPendingRelease: a[0] = (a[0] & 0xFFFFFF00u) | (sh::Often() ? 2 + sh::Next() % 3 : sh::Next() % 5); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    sh::Group g = {"field_e1", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
                   kTables, sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   &Seed, &Disturb, 6000};
    g.args = &Args;
    g.field = true;
    sh::Run(g);
}

}  // namespace field_e1
