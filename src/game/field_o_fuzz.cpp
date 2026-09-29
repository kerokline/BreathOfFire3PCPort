// BOF3X_SHADOW=field_o: group FO's 41 functions through the scenario harness's
// field mode (scenario_harness.h, docs/scenario_harness.md section 7), once at
// start-up. docs/field_o.md section 4.
//
// The clone table is tools/band_rows.py --group FO --clones (2026-09-29,
// capstone, every jump internal but 0x577B50's, one of the six cases dropped),
// names given, plus the two starts no list had: 0x579CA0 EventScript_SkipIf
// and 0x57C3E0 EventCond_Counter0. Extents are the tool's, read from the code
// (six differ from the cut: see the doc). Shapes:
//
//   the eight menu panels          kCall, their words; a panel / set / summary a scratch pointer
//   MoveCmd_OpF9                   kCall (a, b), eax
//   MoveCmd_Op88 / Op87            kSprite: through 0x663AFC by Sprite_Current +4 (a DataTable)
//   their four states, MoveCmd_OpDB, ObjTrio_ClearBit40   kSprite / kState, no arguments
//   EventScript_SkipIf             kCall, the script cursor's op, eax (a pointer)
//   EventOp_3x .. _Ax              kScript
//   the thirteen conditions        kCursor, al
//   MoveCmd_OpE9 and its states    kCall, seven words (the object a sprite record), al
//
// MoveCmd_OpE9's table hands its states seven words: the seed stands a typed
// recorder per entry in 0x663B84 itself (a region, so it is put back), as
// scena_sc0_fuzz.cpp's ObjectEntry - a DataTable's handler recorder logs no
// arguments.
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/field_o.h"
#include "game/field_o_callees.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

#include <windows.h>

namespace field_o {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using sh::Arg;
using sh::ArgAt;
using sh::Shape;
using U = std::uint32_t;

// band_rows.py's clone table (2026-09-29), names given.
constexpr sh::CallSite kCalls5738A0[] = {{0x23, 0x57CF60}, {0x52, 0x516B30}, {0x68, 0x5B9380}, {0x7C, 0x517090}, {0x95, 0x516B30}, {0xAB, 0x5B9380}, {0xBF, 0x517090}, {0xD8, 0x516B30}, {0xF1, 0x5B9380}, {0x105, 0x517090}, {0x11E, 0x516B30}, {0x134, 0x5B9380}, {0x148, 0x517090}, {0x163, 0x497740}, {0x16D, 0x591940}, {0x174, 0x497740}, {0x186, 0x516B30}, {0x197, 0x57D910}, {0x1B3, 0x57D910}, {0x1CB, 0x57D910}};
constexpr sh::CallSite kCalls573BF0[] = {{0x20, 0x57CF60}, {0x6D, 0x516B30}, {0x89, 0x574A60}, {0xA1, 0x5B9380}, {0xB5, 0x5B9380}, {0xCC, 0x516F60}, {0xDA, 0x57D910}};
constexpr sh::CallSite kCalls573F70[] = {{0x32, 0x57CF60}, {0x64, 0x516E70}, {0x99, 0x5A7A50}, {0xA8, 0x5A7A00}, {0xC0, 0x5A7A50}, {0xCF, 0x5A7A00}, {0xFE, 0x5A75F0}, {0x21C, 0x461E50}, {0x286, 0x5A7A50}, {0x294, 0x5A7A00}, {0x2A9, 0x5A7A50}, {0x2B8, 0x5A7A00}, {0x3E2, 0x5A79E0}, {0x40E, 0x5744B0}, {0x42E, 0x57D910}, {0x451, 0x57D910}, {0x471, 0x57D910}};
constexpr sh::CallSite kCalls574400[] = {{0xF, 0x5A77C0}, {0x18, 0x461E50}, {0x86, 0x5A7710}, {0x8E, 0x5A7780}, {0x97, 0x461E50}};
constexpr sh::CallSite kCalls574EC0[] = {{0x26, 0x57CF60}, {0x4E, 0x516B30}, {0x64, 0x516B30}, {0x7A, 0x516B30}, {0x8D, 0x516B30}, {0xA0, 0x516B30}, {0xB6, 0x5B9380}, {0xCD, 0x517090}, {0xE3, 0x5B9380}, {0xF4, 0x517090}, {0x10A, 0x5B9380}, {0x11B, 0x517090}, {0x134, 0x5B9380}, {0x145, 0x517090}, {0x16D, 0x590AB0}, {0x1B6, 0x5A77C0}, {0x1BF, 0x461E50}, {0x1E2, 0x5A79E0}, {0x1F7, 0x5744B0}, {0x210, 0x5B9380}, {0x226, 0x517090}, {0x315, 0x57D360}, {0x325, 0x57D800}, {0x33C, 0x516B30}, {0x353, 0x57D360}, {0x35D, 0x57D800}, {0x374, 0x516B30}, {0x3AD, 0x57D360}, {0x3BD, 0x57D800}, {0x3D4, 0x516B30}, {0x3EB, 0x57D360}, {0x3F5, 0x57D800}, {0x40C, 0x516B30}, {0x439, 0x57D360}, {0x449, 0x57D800}, {0x460, 0x516B30}, {0x48C, 0x57D910}, {0x4A5, 0x57D860}, {0x4B3, 0x57D860}, {0x4D3, 0x57D860}, {0x4E1, 0x57D860}, {0x50E, 0x57D860}, {0x51D, 0x57D860}, {0x532, 0x57D860}, {0x540, 0x57D860}, {0x54F, 0x57D860}, {0x55A, 0x57D860}};
constexpr sh::CallSite kCalls575F50[] = {{0x2F, 0x57CF60}, {0x3E, 0x591E50}, {0x70, 0x57DA70}, {0x9C, 0x5918A0}, {0xD2, 0x591DB0}, {0x10D, 0x57DC90}, {0x13C, 0x57DC90}, {0x166, 0x57DC90}, {0x1C2, 0x516B30}, {0x1E8, 0x57D910}, {0x20E, 0x57D910}, {0x244, 0x57D860}, {0x26F, 0x57D860}, {0x295, 0x57D860}, {0x2BE, 0x57D860}, {0x2E0, 0x57D860}, {0x2FD, 0x57D860}, {0x31E, 0x57D860}, {0x335, 0x57D860}, {0x35E, 0x57D860}};
constexpr sh::CallSite kCalls5763F0[] = {{0x70, 0x57D9A0}, {0x87, 0x591720}, {0x154, 0x57CF60}, {0x16F, 0x57DF00}, {0x1FB, 0x57DBF0}, {0x21C, 0x57DBF0}, {0x23D, 0x57DBF0}, {0x297, 0x57CF60}, {0x2BE, 0x57CF60}, {0x2ED, 0x57D800}, {0x30B, 0x516B30}, {0x329, 0x5B9380}, {0x348, 0x517090}, {0x35E, 0x57D910}, {0x377, 0x57D910}, {0x39B, 0x57D860}, {0x3C6, 0x57D860}, {0x3EC, 0x57D860}, {0x415, 0x57D860}, {0x43B, 0x57D860}, {0x453, 0x57D860}, {0x47C, 0x57D860}, {0x4A2, 0x57D860}, {0x4CA, 0x57D860}, {0x4F0, 0x57D860}, {0x507, 0x57D860}, {0x51F, 0x57D860}, {0x549, 0x57DD10}};
constexpr sh::JumpTable kTables5763F0[] = {{0x19, 0x55C, 5}};
constexpr sh::CallSite kCalls576960[] = {{0x1F, 0x57CF60}, {0x39, 0x5B9380}, {0x4D, 0x517090}, {0x71, 0x5A77C0}, {0x7A, 0x461E50}, {0xAC, 0x573E50}, {0xF0, 0x516B30}, {0x105, 0x5B9380}, {0x119, 0x517090}, {0x134, 0x5B9380}, {0x14B, 0x517090}, {0x15C, 0x517090}, {0x16D, 0x517090}, {0x184, 0x574530}, {0x1AC, 0x516B30}, {0x1BE, 0x57D910}, {0x1DD, 0x57D860}};
constexpr sh::CallSite kCalls578FA0[] = {{0x5B, 0x5720C0}, {0x1DB, 0x5190A0}};
constexpr sh::CallSite kCalls5794F0[] = {{0xE, 0x454CC0}};
constexpr sh::CallSite kCalls579560[] = {{0x58, 0x454D60}};
constexpr sh::CallSite kCalls579610[] = {{0xE, 0x454CC0}};
constexpr sh::CallSite kCalls579690[] = {{0x58, 0x454D60}};
constexpr sh::CallSite kCalls579CA0[] = {{0x1C, 0x579AA0}};   // by hand: EventScript_SkipControl
constexpr sh::CallSite kCalls57A7C0[] = {{0x3F, 0x579E30}, {0x4B, 0x589590}, {0xDE, 0x5720C0}, {0x181, 0x579DB0}, {0x1C1, 0x579D70}};
constexpr sh::CallSite kCalls57A990[] = {{0x3F, 0x579E30}, {0x4B, 0x589590}, {0xDE, 0x5720C0}, {0x16D, 0x579DB0}, {0x1AC, 0x579D70}};
constexpr sh::CallSite kCalls57AB50[] = {{0x3F, 0x579E30}, {0x4B, 0x589590}, {0xDE, 0x5720C0}, {0x17D, 0x579DB0}, {0x1A8, 0x579D70}};
constexpr sh::CallSite kCalls57AD10[] = {{0x2C, 0x579E30}, {0x3D, 0x533BA0}, {0xD0, 0x5720C0}, {0x166, 0x579DB0}, {0x192, 0x579D70}};
constexpr sh::CallSite kCalls57AEC0[] = {{0x54, 0x579E30}, {0x5A, 0x57BA60}, {0x61, 0x57B100}, {0xB1, 0x5720C0}, {0x15D, 0x579DB0}, {0x1B6, 0x579F00}, {0x1D7, 0x579F00}, {0x1F8, 0x579F00}, {0x221, 0x579F00}};
constexpr sh::CallSite kCalls57C270[] = {{0xE, 0x57C140}};
constexpr sh::CallSite kCalls57C290[] = {{0x9, 0x5918E0}};
constexpr sh::CallSite kCalls57C2F0[] = {{0xE, 0x57C140}};
constexpr sh::CallSite kCalls57C920[] = {{0x94, 0x57C840}};
constexpr sh::CallSite kCalls57CA80[] = {{0x5D, 0x5720C0}, {0xB9, 0x589330}, {0xFC, 0x5197F0}, {0x109, 0x57CD40}, {0x119, 0x5197F0}};
constexpr sh::CallSite kCalls57CBB0[] = {{0x2A, 0x5725F0}, {0x4A, 0x5720C0}, {0x75, 0x5725F0}, {0x106, 0x5725F0}};
constexpr sh::CallSite kCalls57CCE0[] = {{0x2E, 0x5720C0}};

#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define FO_FN(name) reinterpret_cast<const void*>(&::name)
#define FO_C(a) kCalls##a, SH_N(kCalls##a)
constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;
constexpr Shape kC = Shape::kCall, kS = Shape::kSprite, kT = Shape::kScript, kK = Shape::kCursor, kSt = Shape::kState;
constexpr U kE9Args = ArgAt(0, Arg::kSprite);
const sh::Clone kClones[] = {
    {"Menu_DrawStatsPanel", 0x5738A0, 0x1D8, FO_C(5738A0), nullptr, 0, nullptr, 0, FO_FN(Menu_DrawStatsPanel), 0, false, kC},
    {"Menu_DrawExpPanel", 0x573BF0, 0xE7, FO_C(573BF0), nullptr, 0, nullptr, 0, FO_FN(Menu_DrawExpPanel), 0, false, kC},
    {"Menu_DrawIconWheel", 0x573F70, 0x481, FO_C(573F70), nullptr, 0, nullptr, 0, FO_FN(Menu_DrawIconWheel), 0, false, kC},
    {"Menu_DrawTile16", 0x574400, 0xA1, FO_C(574400), nullptr, 0, nullptr, 0, FO_FN(Menu_DrawTile16), 0, false, kC},
    {"Menu_DrawEquipCompare", 0x574EC0, 0x56A, FO_C(574EC0), nullptr, 0, nullptr, 0, FO_FN(Menu_DrawEquipCompare), 0, false, kC,
     ArgAt(3, Arg::kScratch) | ArgAt(5, Arg::kScratch)},
    {"Menu_DrawAbilityPanel", 0x575F50, 0x375, FO_C(575F50), nullptr, 0, nullptr, 0, FO_FN(Menu_DrawAbilityPanel), 0, false, kC,
     ArgAt(0, Arg::kScratch)},
    {"Menu_DrawItemPanel", 0x5763F0, 0x570, FO_C(5763F0), nullptr, 0, kTables5763F0, SH_N(kTables5763F0), FO_FN(Menu_DrawItemPanel), 0,
     false, kC, ArgAt(0, Arg::kScratch)},
    {"Menu_DrawSaveSlot", 0x576960, 0x1E9, FO_C(576960), nullptr, 0, nullptr, 0, FO_FN(Menu_DrawSaveSlot), 0, false, kC,
     ArgAt(3, Arg::kScratch)},
    {"MoveCmd_OpF9", 0x578FA0, 0x2F9, FO_C(578FA0), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_OpF9), kAll, false, kC},
    {"MoveCmd_Op88", 0x5794D0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(MoveCmd_Op88), 0, false, kS},
    {"MoveCmd_Op88Start", 0x5794F0, 0x6B, FO_C(5794F0), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_Op88Start), 0, false, kS},
    {"MoveCmd_Op88Fade", 0x579560, 0x84, FO_C(579560), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_Op88Fade), 0, false, kS},
    {"MoveCmd_Op87", 0x5795F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(MoveCmd_Op87), 0, false, kS},
    {"MoveCmd_Op87Start", 0x579610, 0x78, FO_C(579610), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_Op87Start), 0, false, kS},
    {"MoveCmd_Op87Fade", 0x579690, 0xA9, FO_C(579690), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_Op87Fade), 0, false, kS},
    {"EventScript_SkipIf", 0x579CA0, 0x48, FO_C(579CA0), nullptr, 0, nullptr, 0, FO_FN(EventScript_SkipIf), kAll, false, kC,
     ArgAt(0, Arg::kScript)},
    {"EventOp_3x", 0x57A7C0, 0x1D0, FO_C(57A7C0), nullptr, 0, nullptr, 0, FO_FN(EventOp_3x), 0, false, kT},
    {"EventOp_4x", 0x57A990, 0x1BB, FO_C(57A990), nullptr, 0, nullptr, 0, FO_FN(EventOp_4x), 0, false, kT},
    {"EventOp_7x", 0x57AB50, 0x1B7, FO_C(57AB50), nullptr, 0, nullptr, 0, FO_FN(EventOp_7x), 0, false, kT},
    {"EventOp_6x", 0x57AD10, 0x1A1, FO_C(57AD10), nullptr, 0, nullptr, 0, FO_FN(EventOp_6x), 0, false, kT},
    {"EventOp_Ax", 0x57AEC0, 0x234, FO_C(57AEC0), nullptr, 0, nullptr, 0, FO_FN(EventOp_Ax), 0, false, kT},
    {"EventCond_Area", 0x57C1A0, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_Area), 0xFF, false, kK},
    {"EventCond_Run", 0x57C1F0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_Run), 0xFF, false, kK},
    {"EventCond_Status1", 0x57C230, 0x16, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_Status1), 0xFF, false, kK},
    {"EventCond_LeaderId", 0x57C250, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_LeaderId), 0xFF, false, kK},
    {"EventCond_StoryFlag", 0x57C270, 0x17, FO_C(57C270), nullptr, 0, nullptr, 0, FO_FN(EventCond_StoryFlag), 0xFF, false, kK},
    {"EventCond_KeyItem", 0x57C290, 0x12, FO_C(57C290), nullptr, 0, nullptr, 0, FO_FN(EventCond_KeyItem), 0xFF, false, kK},
    {"EventCond_ChapterAtMost", 0x57C2B0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_ChapterAtMost), 0xFF, false, kK},
    {"EventCond_RecordBit0", 0x57C2D0, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_RecordBit0), 0xFF, false, kK},
    {"EventCond_Gene", 0x57C2F0, 0x17, FO_C(57C2F0), nullptr, 0, nullptr, 0, FO_FN(EventCond_Gene), 0xFF, false, kK},
    {"EventCond_Counter0", 0x57C3E0, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_Counter0), 0xFF, false, kK},
    {"EventCond_Counter1", 0x57C400, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_Counter1), 0xFF, false, kK},
    {"EventCond_Counter2", 0x57C420, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_Counter2), 0xFF, false, kK},
    {"EventCond_Counter3", 0x57C440, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(EventCond_Counter3), 0xFF, false, kK},
    {"ObjTrio_ClearBit40", 0x57C7E0, 0x2D, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(ObjTrio_ClearBit40), 0, false, kSt},
    {"MoveCmd_OpE9", 0x57C8E0, 0x39, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(MoveCmd_OpE9), 0xFF, false, kC, kE9Args},
    {"MoveCmd_OpE9Start", 0x57C920, 0x160, FO_C(57C920), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_OpE9Start), 0xFF, false, kC, kE9Args},
    {"MoveCmd_OpE9Arc", 0x57CA80, 0x126, FO_C(57CA80), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_OpE9Arc), 0xFF, false, kC, kE9Args},
    {"MoveCmd_OpE9Kind2", 0x57CBB0, 0x126, FO_C(57CBB0), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_OpE9Kind2), 0xFF, false, kC, kE9Args},
    {"MoveCmd_OpE9Fall", 0x57CCE0, 0x55, FO_C(57CCE0), nullptr, 0, nullptr, 0, FO_FN(MoveCmd_OpE9Fall), 0xFF, false, kC, kE9Args},
    {"MoveCmd_OpDB", 0x57CD40, 0x49, nullptr, 0, nullptr, 0, nullptr, 0, FO_FN(MoveCmd_OpDB), 0, false, kS},
};
#undef FO_C
#undef FO_FN

enum : unsigned {
    kStats, kExp, kWheel, kTile, kEquip, kAbility, kItems, kSlot, kF9, kOp88, kOp88Start, kOp88Fade, kOp87, kOp87Start,
    kOp87Fade, kSkipIf, kOp3, kOp4, kOp7, kOp6, kOpA, kCArea, kCRun, kCStatus, kCLeader, kCStory, kCKey, kCChapter,
    kCRecord, kCGene, kCCount0, kCCount1, kCCount2, kCCount3, kClear40, kE9, kE9Start, kE9Arc, kE9Kind2, kE9Fall, kOpDB,
    kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char* Mem(U a) { return sh::Mem(a); }
unsigned char* Rec(unsigned n) { return Mem(bof3::addr::CharacterRecords + (n & 7) * at::kRecordStride); }

// --- the stand-ins the group lists ------------------------------------------------

// Crt_sprintf: logs as many words as its format has conversions (the callers
// push one, two or none past the format - the rest is the caller's frame),
// then writes up to seven characters and a NUL where the caller's buffer is
// in the regions (the harness's FxSprintf).
int __cdecl Sprintf(char* dst, const char* format, U a, U b, U c) {
    unsigned n = 0;
    for (const char* p = format; *p; ++p)
        if (*p == '%' && p[1] != '%') ++n;
    sh::Record(0x5B9380, sh::HashBytes(format, static_cast<unsigned>(std::strlen(format))), n > 0 ? a : 0, n > 1 ? b : 0,
               n > 2 ? c : 0);
    sh::Stir();
    const unsigned len = sh::Noise() % 8;
    if (sh::InRegions(dst, 8)) {
        for (unsigned i = 0; i < len; ++i) dst[i] = static_cast<char>('0' + sh::Noise() % 43);
        dst[len] = 0;
    }
    return static_cast<int>(len);
}

// MoveCmd_OpE9's four states, typed: each logs its seven words at the widths
// the states read them (a, b, e, f bytes; c, d words) under its own address.
template <U kAddress> unsigned char __cdecl E9State(unsigned char* object, U a, U b, U c, U d, U e, U f) {
    sh::Record(kAddress, Key(object), (a & 0xFF) | (b & 0xFF) << 8 | (c & 0xFFFF) << 16, (d & 0xFFFF) | (e & 0xFF) << 16 | (f & 0xFF) << 24,
               Sprite_Current[4]);
    sh::Stir();
    return static_cast<unsigned char>(sh::Noise());
}

// Effects: the answers the functions read back.
U TextAnswer(const U*, U answer) { return Key(sh::Text() + (answer & 0xF0)); }
U SkipOne(const U* a, U) { return a[0] + 1; }   // EventScript_SkipControl: the byte after the control
U KindAnswer(const U*, U) { return sh::Noise() % 4; }   // MoveScript_ObjectKind: 0..3, 2 the kind-2 object
U ExpAnswer(const U*, U answer) { return sh::Noise() % 3 == 0 ? 0xFFFFFFFFu : answer; }   // Char_ExpForLevel: -1 past 99
U PreviewFill(const U* a, U answer) {   // Equip_PreviewSet: the four marks (1, 4, or other) and four words it writes
    auto* const marks = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[2]));
    auto* const values = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a[3]));
    static const unsigned char kMarks[] = {0, 1, 2, 4};
    for (unsigned i = 0; i < 4; ++i) marks[i] = kMarks[sh::Noise() % 4];
    sh::FillBytes(values, 8);
    return answer;
}
U EntryAnswer(const U* a, U answer) {   // Sprite_InitFromEntry: the sprite's +0x54 the entry (EventOp_Ax reads its byte 2 through it)
    unsigned char* const sprite = Sprite_Current;
    if (sh::InRegions(sprite + 0x54, 4)) SetLong(sprite + 0x54, static_cast<std::int32_t>(a[0]));
    return answer;
}

#define FO_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
const sh::Callee kCallees[] = {
    // the group's own, called directly (E8)
    {FO_OURS(MoveCmd_OpDB), 0, {}, sh::Answer::kPhase, 0, 0},
    // MoveCmd_OpE9's states, typed (the seed writes them into 0x663B84)
    {"MoveCmd_OpE9States[0]", 0x57C920, 0x57C920, 7, {kAll, kU8, kU8, kU16, kU16, kU8, kU8}, kG, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&E9State<0x57C920>)},
    {"MoveCmd_OpE9States[1]", 0x57CA80, 0x57CA80, 7, {kAll, kU8, kU8, kU16, kU16, kU8, kU8}, kG, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&E9State<0x57CA80>)},
    {"MoveCmd_OpE9States[2]", 0x57CBB0, 0x57CBB0, 7, {kAll, kU8, kU8, kU16, kU16, kU8, kU8}, kG, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&E9State<0x57CBB0>)},
    {"MoveCmd_OpE9States[3]", 0x57CCE0, 0x57CCE0, 7, {kAll, kU8, kU8, kU16, kU16, kU8, kU8}, kG, 0, 0, {}, nullptr,
     reinterpret_cast<const void*>(&E9State<0x57CCE0>)},
    // Capcom's, the format's conversions logged (FoSprintf above)
    {"Crt_sprintf", 0x5B9380, 0x5B9380, 5, {kAll, kAll, kAll, kAll, kAll}, kG, 0, 0, {}, nullptr, reinterpret_cast<const void*>(&Sprintf)},
    // Re-listed from the field-standard set at the widths the callee reads
    // (where Capcom pushes a whole register for a word or a byte; the reading
    // cited; battle_e7_fuzz.cpp re-lists the draw ones the same way):
    // menu_windows.cpp Menu_DrawBox: U16 x / y / w / h, the flags and colour bytes
    {FO_OURS(Menu_DrawBox), 6, {kU16, kU16, kU16, kU16, kU8, kU8}, kG, 0, 0},
    // Menu_DrawPiece: S16 x / y, Menu_PieceRect's id & 0xFF, the flag byte
    {FO_OURS(Menu_DrawPiece), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},
    {FO_OURS(Menu_DrawPieces), 4, {kU16, kU16, kAll, kU8}, kG, 0, 0},
    {FO_OURS(Menu_DrawIcon8), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},
    // msgbox.cpp Text_DrawAt: shorts; Text_DrawString's colour and count bytes; the text hashed
    {FO_OURS(Text_DrawAt), 5, {kU16, kU16, kU8, kU8, 0}, kG, 0, 0, {0, 0, 0, 0, 16}, nullptr, nullptr, true},
    {FO_OURS(Text_DrawSmall), 5, {kU16, kU16, kU8, kU8, 0}, kG, 0, 0, {0, 0, 0, 0, 16}, nullptr, nullptr, true},
    {FO_OURS(Text_DrawFont12), 4, {kU16, kU16, kU8, 0}, kG, 0, 0, {0, 0, 0, 16}, nullptr, nullptr, true},
    {FO_OURS(Text_DrawFont8), 4, {kU16, kU16, kU8, 0}, kG, 0, 0, {0, 0, 0, 16}, nullptr, nullptr, true},
    {FO_OURS(Text_CharCount), 1, {0}, sh::Answer::kByte, 0, 17, {16}, nullptr, nullptr, true},
    // menu_windows.cpp Menu_DrawScrollBar: the counts bytes, x / y S16
    {FO_OURS(Menu_DrawScrollBar), 7, {kAll, kU8, kU16, kU16, kU8, kU8, kU8}, kG, 0, 0, {16}, nullptr, nullptr, true},
    // map_field_objects.cpp Menu_DrawCell8: x & 0xFFFF, y & 0xFFFF, u << 3 and v << 3 bytes, the CLUT a word, the shade a byte
    {FO_OURS(Menu_DrawCell8), 6, {kU16, kU16, kU8, kU8, kU16, kU8}, kG, 0, 0},
    // menu_windows.cpp Menu_DrawBigIcon: U16 x / y, icon & 0xFF, the shade byte
    {FO_OURS(Menu_DrawBigIcon), 4, {kU16, kU16, kU8, kU8}, kG, 0, 0},
    // Menu_DrawExpBar: U16 x / y, the member and level through Char_ExpForLevel's bytes, exp whole
    {FO_OURS(Menu_DrawExpBar), 5, {kU16, kU16, kU8, kU8, kAll}, kG, 0, 0},
    // Menu_DrawItemRow: x, y through the S16 draws; colour, category, id, count and dim bytes
    {FO_OURS(Menu_DrawItemRow), 7, {kU16, kU16, kU8, kU8, kU8, kU8, kU8}, kG, 0, 0},
    // battle_window_draw.cpp Menu_DrawSkillRow: x, y through the S16 draws; colour, kind, cost, dim bytes; the name hashed
    {FO_OURS(Menu_DrawSkillRow), 7, {kU16, kU16, kU8, kU8, 0, kU8, kU8}, kG, 0, 0, {0, 0, 0, 0, 16, 0, 0}, nullptr, nullptr, true},
    // msg_pool.cpp Msg_SystemPtr: id & 0xFFFF; answers into the text buffer
    {FO_OURS(Msg_SystemPtr), 1, {kU16}, kG, 0, 0, {}, &TextAnswer},
    // char_stats.cpp TextRecord_Set: slot and length bytes, the text hashed
    {FO_OURS(TextRecord_Set), 3, {kU8, kU8, 0}, kG, 0, 0, {0, 0, 16}, nullptr, nullptr, true},
    // menu_windows.cpp Char_ExpForLevel: member & 0xFF, the level a byte; -1 a third of the time
    {FO_OURS(Char_ExpForLevel), 2, {kU8, kU8}, kG, 0, 0, {}, &ExpAnswer},
    // battle_window_draw.cpp: Char_AbilityList's three bytes (answers into the text buffer), Skill_CanUse's,
    // Skill_FlagIndex's, Skill_ApCost's
    {FO_OURS(Char_AbilityList), 3, {kU8, kU8, kU8}, kG, 0, 0, {}, &TextAnswer},
    {FO_OURS(Skill_CanUse), 3, {kU8, kU8, kU8}, sh::Answer::kFlag, 0, 0},
    {FO_OURS(Skill_FlagIndex), 1, {kU8}, sh::Answer::kFlag, 0, 0},
    {FO_OURS(Skill_ApCost), 3, {kU8, kU8, kU8}, sh::Answer::kFlag, 0, 0},
    // menu_windows.cpp Item_CanUse: all four bytes; char_stats.cpp Item_IconKind: bytes, a kind 2..7
    {FO_OURS(Item_CanUse), 4, {kU8, kU8, kU8, kU8}, sh::Answer::kFlag, 0, 0},
    {FO_OURS(Item_IconKind), 2, {kU8, kU8}, sh::Answer::kByte, 2, 7},
    // menu_windows.cpp Menu_ListScroll: self-contained, it writes the top, offset, moving and state
    // bytes the panel reads back - both sides call it for real
    {FO_OURS(Menu_ListScroll), 4, {kAll, kAll, kAll, kAll}, sh::Answer::kThrough, 0, 0},
    // char_stats.cpp Equip_PreviewSet: id & 0xFF, the six set bytes; it fills the caller's marks and values
    {FO_OURS(Equip_PreviewSet), 4, {kU8, kAll, 0, 0}, kG, 0, 0, {0, 6, 0, 0}, &PreviewFill, nullptr, true},
    // field_event.cpp Field_MemberSprite: member_arg & 0xFF, slot_arg & 0xFF
    {FO_OURS(Field_MemberSprite), 2, {kU8, kU8}, kG, 0, 0},
    // char_stats.cpp KeyItem_Has: the item a byte
    {FO_OURS(KeyItem_Has), 1, {kU8}, sh::Answer::kFlag, 0, 0},
    // map_field_objects.cpp AreaMap_SetByte: S16 x and z
    {FO_OURS(AreaMap_SetByte), 3, {kU16, kU16, kAll}, kG, 0, 0},
    // map_view.cpp MapView_SetElevation: only the low word reaches the u16 offset
    {FO_OURS(MapView_SetElevation), 1, {kU16}, kG, 0, 0},
    // sprite_pose.cpp Sprite_EnsureAnimation(unsigned char): the byte
    {FO_OURS(Sprite_EnsureAnimation), 1, {kU8}, sh::Answer::kFlag, 0, 0},
    // move_cmds.cpp MoveScript_ObjectKind: 0..3 exactly (the caller compares eax with 2)
    {FO_OURS(MoveScript_ObjectKind), 0, {}, kG, 0, 0, {}, &KindAnswer},
    // event_script.cpp EventScript_SkipControl: the position after the control byte
    {FO_OURS(EventScript_SkipControl), 1, {kAll}, kG, 0, 0, {}, &SkipOne},
    // event_script.cpp Sprite_InitFromEntry: sets the sprite's +0x54 to the entry (read back through)
    {FO_OURS(Sprite_InitFromEntry), 1, {kAll}, kG, 0, 0, {16}, &EntryAnswer, nullptr, true},
};
#undef FO_OURS

// Op 88 / 87's four states, read in place by MoveCmd_Op88 / Op87 (one table:
// op 88's bytes 2 and 3 reach op 87's pair).
const sh::DataTable kTables[] = {{at::kOp88States, 4}};

// Beyond field mode's standard regions (which hold ObjTrio, Sprite_Objects and
// Extra, Cond_Flags with the party list, the save block to 0x904160, the
// menu block, the style byte and the records' first 0x24 bytes, the text
// scratch, the packet cursor, Game_AreaNumber, the counters, DamageScratch,
// Sprite_Current, Frame_Counter, Sprite_Kind2, Field_Kind2X / Z):
const sh::Region kRegions[] = {
    {0x903A94, 0x903F90 - 0x903A94},   // CharacterRecords 0x903A70.. past the standard 0x24: the eight records
    {0x6BC740, 0x188},                 // the item panel's kept lists 0x6BC760 / 0x6BC7E0, the icon wheel's spots
    {Key(MoveScript_PartyRecords), 0x80},
    {Key(&Field_ActiveMember), 4},
    {at::kOpE9States, 0x10},           // MoveCmd_OpE9States: the typed recorders, seeded
    {0x904160, 0x400},                 // the inventory's id and count lists 0x904154..0x904554 past the standard save block
};

unsigned g_k;   // the clone being seeded, for Disturb
// BOF3X_FO_ONLY=<name>: one clone alone (for a failing function and the
// controls); g_real maps the run's index to the enum's.
unsigned g_real[kCount];
unsigned g_areas[200];
unsigned g_area_n;

// Areas whose Area_Descriptors entry and its +8 table are image data (the
// descriptors are read in place; EventOp_Ax takes an entry of the +8 table).
void FindAreas() {
    if (g_area_n) return;
    for (unsigned n = 0; n < 200; ++n) {
        const auto d = static_cast<U>(Long(Mem(Key(Area_Descriptors) + 4 * n)));
        if (d < 0x5C3000 || d >= 0x6D0000) continue;
        const auto t = static_cast<U>(Long(Mem(d + 8)));
        if (t < 0x5C3000 || t >= 0x6D0000) continue;
        g_areas[g_area_n++] = n;
    }
    if (!g_area_n) bof3::Fatal("field_o: no area descriptor with an image +8 table");
}

unsigned char Pick(std::initializer_list<unsigned> v) { return static_cast<unsigned char>(v.begin()[sh::Next() % v.size()]); }

// EventScript_SkipIf's script: F0 / F1, the condition and operand, then up to
// eight items - an op (byte below F0, its EventScript_OpLengths bytes), a
// control F0..FC (SkipControl's recorder steps one), an FD - and the FE; no FF.
void SkipScript(unsigned char* s) {
    s[0] = sh::Half() ? 0xF0 : 0xF1;
    s[1] = static_cast<unsigned char>(sh::Next());
    s[2] = static_cast<unsigned char>(sh::Next());
    unsigned at = 3;
    const unsigned items = sh::Next() % 9;
    for (unsigned i = 0; i < items; ++i) {
        switch (sh::Next() % 4) {
        case 0: s[at++] = static_cast<unsigned char>(0xF0 + sh::Next() % 0xD); break;
        case 1: s[at++] = 0xFD; break;
        default: {
            const unsigned char op = static_cast<unsigned char>(sh::Next() % 0xF0);
            const unsigned n = EventScript_OpLengths[op >> 4];
            s[at] = op;
            for (unsigned k = 1; k < n; ++k) s[at + k] = static_cast<unsigned char>(sh::Next());
            at += n;
            break;
        }
        }
    }
    s[at] = 0xFE;
}

void Seed(unsigned run_k) {
    const unsigned k = g_real[run_k];
    g_k = k;
    unsigned char* const sc = Sprite_Current;
    unsigned char* const op = sh::Script();
    // Field_ActiveMember: a sprite record most of the time, an extra record (a party slot) or Sprite_Current
    switch (sh::Next() % 4) {
    case 0: Field_ActiveMember = sc; break;
    case 1: Field_ActiveMember = Sprite_ObjectsExtra + (sh::Next() % 4) * 0xA4; break;
    default: Field_ActiveMember = sh::SpriteRecord(sh::Next()); break;
    }
    // the count word in 0..31 (30 and 31: the ops' early return)
    SetWord(Mem(bof3::addr::DamageScratch), sh::Often() ? sh::Next() % 30 : 30 + sh::Next() % 2);
    for (unsigned i = 0; i < 4; ++i) {
        unsigned char* const r = sh::SpriteRecord(i);
        r[0xA] = static_cast<unsigned char>(1 + sh::Next() % 0x10);   // the frames E9's states divide by
        SetLong(r + 0x54, static_cast<std::int32_t>(Key(sh::Text() + (sh::Next() & 0xF0))));   // EventOp_Ax reads through it
    }
    unsigned char* const panel = sh::Scratch(k == kEquip ? 5 : k == kSlot ? 3 : 0);
    switch (k) {
    case kStats:
        if (sh::Half()) Rec(0)[0x1F] = 0xFF, Rec(1)[0x1F] = 0xFF, Rec(2)[0x1F] = 0xFF, Rec(3)[0x1F] = 0xFF;
        break;
    case kExp:
        for (unsigned i = 0; i < 8; ++i) Rec(i)[0xA] = Pick({0, 1, 0x62, 0x63, 0xFF, sh::Next() & 0xFF});
        break;
    case kEquip:
        panel[0xA] = static_cast<unsigned char>(sh::Next() % 7);
        panel[0xB] = static_cast<unsigned char>(sh::Next() % 7);
        break;
    case kAbility:
        panel[0xB] = static_cast<unsigned char>(sh::Next() % 8);   // inside the title table's (and the next table's) pointers
        panel[0xC] = static_cast<unsigned char>(sh::Next() % 11);
        panel[0xD] = static_cast<unsigned char>(sh::Next() % 11);
        if (sh::Half()) SetWord(panel + 0x10, 0);
        panel[9] = Pick({0, 1, 2, 3, 0x10, 0x13, 0xF0, 0xF3, sh::Next() & 0xFF});
        for (unsigned i = 0; i < 0x200; ++i)
            if (sh::Next() % 3 == 0) sh::Text()[i] = 0;   // some ability ids 0
        break;
    case kItems:
        panel[9] = static_cast<unsigned char>(sh::Next() % 8);
        panel[0x10] = Pick({0, 0x10, 0x14, 0x18, 0xF0, 0xF4, 0xF8, sh::Next() & 0xFF});
        for (unsigned i = 0; i < 0x400; ++i)
            if (sh::Next() % 3 == 0) Mem(0x904160)[i] = 0;   // some ids 0
        break;
    case kSlot:
        for (unsigned i = 5; i < 8; ++i)
            if (sh::Half()) panel[i] = 0xFF;   // no party icon
        if (sh::Half()) panel[0x14] = 0;
        break;
    case kF9:
        Field_ActiveMember[0x84] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 6 : sh::Next());
        if (sh::Half()) SetLong(sc + 0x18, 0), SetLong(sc + 0x1C, 0);
        if (sh::Half()) sc[6] = 0x0A;
        if (sh::Half()) {   // the target within reach
            SetLong(Field_ActiveMember + 0x8C, Long(sc + 0x34) + static_cast<std::int32_t>(sh::Next() % 0x20000) - 0x10000);
            SetLong(Field_ActiveMember + 0x90, Long(sc + 0x38) + static_cast<std::int32_t>(sh::Next() % 0x20000) - 0x10000);
        }
        for (unsigned i = 0; i < 8; ++i)
            if (sh::Half()) MoveScript_PartyRecords[16 * i + 1 + (sh::Next() & 1)] = 0;
        break;
    case kOp88: sc[4] = static_cast<unsigned char>(sh::Next() % 4); break;
    case kOp87: sc[4] = static_cast<unsigned char>(sh::Next() % 2); break;
    case kOp88Fade:
    case kOp87Fade:
        for (unsigned c = 0x5D; c <= 0x5F; ++c) sc[c] = Pick({0x80, 0x84, 0xBC, 0xC0, 0x7C, 0x40, sh::Next() & 0xFF});
        if (sh::Half()) {   // all three one step from the end (or at it)
            const unsigned char v = k == kOp88Fade ? Pick({0x80, 0x84}) : Pick({0xC0, 0xBC});
            sc[0x5D] = sc[0x5E] = sc[0x5F] = v;
        }
        break;
    case kSkipIf: SkipScript(op); break;
    case kOp3: case kOp4: case kOp7: case kOp6: break;
    case kOpA: {
        FindAreas();
        SetWord(Mem(0x904EFC), g_areas[sh::Next() % g_area_n]);   // Game_AreaNumber
        if (sh::Half()) op[6] = 0;                                 // object +0x84 0: the area block's cells
        if (sh::Half()) op[1] = 0;
        if (sh::Half()) op[3] = 0;
        break;
    }
    case kCArea: if (sh::Half()) SetWord(Mem(0x904EFC), op[0]); break;
    case kCRun: if (sh::Half()) Mem(0x8034E4)[0] = op[0]; break;
    case kCStatus: if (sh::Half()) op[0] = 1; break;
    case kCLeader: if (sh::Half()) ObjTrio[0x89] = op[0]; break;
    case kCChapter: if (sh::Half()) op[0] = static_cast<unsigned char>(Mem(0x8034E0)[0] + sh::Next() % 3 - 1); break;
    case kCRecord: op[0] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : sh::Next()); break;
    case kCCount0: if (sh::Half()) op[0] = Mem(0x903848)[0]; break;
    case kCCount1: if (sh::Half()) op[0] = Mem(0x903849)[0]; break;
    case kCCount2: if (sh::Half()) op[0] = Mem(0x90384A)[0]; break;
    case kCCount3: if (sh::Half()) op[0] = Mem(0x90384B)[0]; break;
    case kE9:
        sc[4] = static_cast<unsigned char>(sh::Next() % 4);
        for (unsigned i = 0; i < 4; ++i)
            SetLong(Mem(at::kOpE9States + 4 * i), static_cast<std::int32_t>(Key(reinterpret_cast<const void*>(
                                                      i == 0 ? &E9State<0x57C920> : i == 1 ? &E9State<0x57CA80> : i == 2 ? &E9State<0x57CBB0> : &E9State<0x57CCE0>))));
        break;
    case kE9Start:
    case kE9Arc:
    case kE9Kind2:
    case kE9Fall:
        for (unsigned i = 0; i < 4; ++i) {
            unsigned char* const r = sh::SpriteRecord(i);
            // the object is the record's script context +0x80 (MoveCmd_OpE9's callers hand it
            // that: boss_sa_callees.h kFieldActors), its +4 the speed index +0x84: 1..5 (16 / speed
            // is a step's frames); Start also 0, 6, 7 (no speed)
            r[0x84] = k == kE9Start && sh::Next() % 4 == 0 ? Pick({0, 6, 7}) : static_cast<unsigned char>(1 + sh::Next() % 5);
            if (sh::Half()) r[0xA] = k == kE9Start ? r[0xA] : static_cast<unsigned char>(sh::Next() % 2);
            if (sh::Half()) r[0xB] = 0;
            if (sh::Half()) r[6] = 6;
            if (sh::Half()) SetLong(r + 0x14, sh::Half() ? 0 : static_cast<std::int32_t>(sh::Next() % 0x200));   // a small rise or none: the top within a step
        }
        break;
    default: break;
    }
}

void Args(unsigned run_k, U* a) {
    const unsigned k = g_real[run_k];
    switch (k) {
    case kStats:
    case kExp:
        a[2] = (a[2] & 0xFFFFFF00u) | (a[2] & 7);
        if (k == kExp) a[3] = sh::Half() ? 0 : a[3] | 1;
        break;
    case kWheel: if (sh::Often()) a[2] = (a[2] & 0xFFFFFF00u) | (sh::Next() % 6); break;
    case kEquip:
        a[0] = (a[0] & 0xFFFFFF00u) | (a[0] & 7);
        if (sh::Half()) a[4] &= 0xFFFFFF00u;
        break;
    case kSlot: if (sh::Next() % 4 == 0) a[3] = 0; break;
    case kE9:
    case kE9Start:
    case kE9Arc:
    case kE9Kind2:
    case kE9Fall:
        a[0] += 0x80;   // the record's script context (the sprite record the shape drew + 0x80)
        if (sh::Half()) a[4] = (a[4] & 0xFFFF0000u) | (0x10000u - sh::Next() % 0x400);   // d a small fall
        if (sh::Half()) a[5] = (a[5] & 0xFFFFFF00u) | 0xFF;   // e 0xFF: no animation
        if (sh::Half()) a[1] &= 0xFFFFFF07u, a[2] &= 0xFFFFFF07u;   // small steps
        if (sh::Next() % 4 == 0) a[1] &= 0xFFFFFF00u, a[2] &= 0xFFFFFF00u;   // no steps
        break;
    default: break;
    }
}

// The cells the functions read again after a call that the harness's
// disturbance does not move: Field_ActiveMember, the count word (kept inside
// the objects), a colour byte of Sprite_Current.
void Disturb(U h) {
    const unsigned v = (h >> 8) & 0xFF;
    switch ((h >> 16) % 3) {
    case 0: Field_ActiveMember = sh::SpriteRecord(v); break;
    case 1: SetWord(Mem(bof3::addr::DamageScratch), v % 30); break;
    default: {
        unsigned char* const s = Sprite_Current;
        if (sh::InRegions(s + 0x5D, 3)) s[0x5D + v % 3] = static_cast<unsigned char>(h >> 24);
        break;
    }
    }
}

// Under BOF3X_FO_ONLY: a fault's EIP and address in the log (the crash
// reporter is installed after the self-tests).
LONG CALLBACK Faulted(EXCEPTION_POINTERS* p) {
    if (p->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || p->ExceptionRecord->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO)
        bof3::Log("shadow      field_o: fault 0x%08X at eip 0x%08X (address 0x%08X)", (unsigned)p->ExceptionRecord->ExceptionCode,
                  (unsigned)p->ContextRecord->Eip, (unsigned)p->ExceptionRecord->ExceptionInformation[1]);
    return EXCEPTION_CONTINUE_SEARCH;
}

}  // namespace

void SelfTest() {
    static sh::Clone one[1];
    const sh::Clone* clones = kClones;
    unsigned n = kCount;
    for (unsigned k = 0; k < kCount; ++k) g_real[k] = k;
    char only[64];
    const DWORD got = GetEnvironmentVariableA("BOF3X_FO_ONLY", only, sizeof only);
    if (got > 0 && got < sizeof only) {
        unsigned k = 0;
        while (k < kCount && std::strcmp(kClones[k].name, only) != 0) ++k;
        if (k == kCount) bof3::Fatal("field_o: BOF3X_FO_ONLY names no clone: %s", only);
        one[0] = kClones[k];
        g_real[0] = k;
        clones = one;
        n = 1;
        bof3::Log("shadow      field_o: %s alone (BOF3X_FO_ONLY)", only);
        AddVectoredExceptionHandler(1, Faulted);
    }
    sh::Group g = {"field_o", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0], Seed, Disturb, 0};
    g.args = Args;
    g.field = true;
    char rounds[16];   // BOF3X_FO_ROUNDS=n: the rounds per function (a control's re-run)
    const DWORD rn = GetEnvironmentVariableA("BOF3X_FO_ROUNDS", rounds, sizeof rounds);
    if (rn > 0 && rn < sizeof rounds) g.rounds = static_cast<unsigned>(std::strtoul(rounds, nullptr, 10));
    sh::Run(g);
}

}  // namespace field_o
