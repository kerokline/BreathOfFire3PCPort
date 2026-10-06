// BOF3X_SHADOW=rest_2a: group R2A's 22 functions through the scenario harness
// (scenario_harness.h, used unchanged) in field mode, once at start-up.
// docs/rest_2a.md section 4. BOF3X_R2A_ONLY=<name> runs the clones whose name
// contains it (the controls' speed-up).
//
// The clone rows are tools/band_rows.py --group R2A --clones --harness scenario
// (2026-10-04), each extent read again to its last instruction (capstone); the
// cut's sizes are padding past them, or (0x5375E0, 0x537990, 0x537B50) ran on
// over a start no list had. Shapes: the state handlers and the dispatchers
// kSprite (void, on Sprite_Current), LinkedObject_EffectRise kSprite answering
// al, the record helpers, Area_ObjectHandler and the two spawns kCall. The
// dispatchers' stack tables are the clones' immediates (re-aimed at handler
// recorders); Area_ObjectHandler's two tables - Area_ObjectFallbacks and the
// area table it reaches for area 74 (Area74_Handlers) - are DataTables. Every
// callee the group's code calls is re-listed here (registered before the
// standard rows: the group's listing stands) where the standard row is not
// what the group needs.
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/rest_2a.h"
#include "game/rest_2a_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace rest_2a {
namespace {

namespace sh = scenario_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
template <typename... T> U PickOf(T... v) {
    const U values[] = {static_cast<U>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// --- the clone table (band_rows.py --group R2A --clones --harness scenario, 2026-10-04) ---
constexpr sh::CallSite kCalls5372E0[] = {{0x54, 0x57B830}, {0xE9, 0x588F20}};
constexpr sh::Imm kImms5375A0[] = {{0xF, 0x5375E0}, {0x17, 0x537760}, {0x22, 0x5377B0}, {0x2A, 0x537800}, {0x32, 0x537850}};
constexpr sh::CallSite kCalls5375E0[] = {{0x3, 0x5B93D2},   {0x37, 0x57C4C0}, {0x7D, 0x537DE0},  {0x95, 0x537DE0},
                                         {0x9E, 0x591BE0},  {0xC1, 0x537ED0}, {0xD9, 0x537DE0},  {0xE2, 0x591BE0},
                                         {0x105, 0x537ED0}, {0x129, 0x537DE0}, {0x140, 0x537DE0}};
constexpr sh::JumpTable kTables5375E0[] = {{0x77, 0x158, 8}};
constexpr sh::CallSite kCalls537760[] = {{0x23, 0x5B9380}, {0x2A, 0x497710}};
constexpr sh::CallSite kCalls5377B0[] = {{0x23, 0x5B9380}, {0x2A, 0x497710}};
constexpr sh::CallSite kCalls537800[] = {{0x20, 0x57C4C0}};
constexpr sh::CallSite kCalls537850[] = {{0x18, 0x57C4C0}};
constexpr sh::Imm kImms5378A0[] = {{0xF, 0x5378D0}, {0x17, 0x537800}};
constexpr sh::CallSite kCalls5378D0[] = {{0x2, 0x5B93D2}, {0x31, 0x57C4C0}, {0x42, 0x537DE0}, {0x5E, 0x537DE0}};
constexpr sh::Imm kImms537950[] = {{0xF, 0x537990}, {0x17, 0x537760}, {0x22, 0x5377B0}, {0x2A, 0x537800}, {0x32, 0x537850}};
constexpr sh::CallSite kCalls537990[] = {{0x3, 0x5B93D2},   {0x37, 0x57C4C0}, {0x7D, 0x537DE0},  {0x95, 0x537DE0},
                                         {0x9E, 0x591BE0},  {0xC1, 0x537ED0}, {0xD9, 0x537DE0},  {0xE2, 0x591BE0},
                                         {0x105, 0x537ED0}, {0x129, 0x537DE0}, {0x140, 0x537DE0}};
constexpr sh::JumpTable kTables537990[] = {{0x77, 0x158, 8}};
constexpr sh::Imm kImms537B10[] = {{0xA, 0x537CE0}, {0x23, 0x537B50}, {0x2B, 0x537D50}, {0x33, 0x537D70}};
constexpr sh::CallSite kCalls537B50[] = {{0x3, 0x5B93D2},   {0x37, 0x57C4C0}, {0x7D, 0x537DE0},  {0x96, 0x537DE0},
                                         {0x9F, 0x591BE0},  {0xC2, 0x537ED0}, {0xD8, 0x537DE0},  {0xE1, 0x591BE0},
                                         {0x104, 0x537ED0}, {0x126, 0x537DE0}, {0x13B, 0x537DE0}};
constexpr sh::JumpTable kTables537B50[] = {{0x77, 0x170, 8}};
constexpr sh::CallSite kCalls537CE0[] = {{0x3F, 0x5B9380}, {0x46, 0x497710}};
constexpr sh::CallSite kCalls537D70[] = {{0x17, 0x57C4C0}};
constexpr sh::CallSite kCalls537DE0[] = {{0x0, 0x589810}, {0x65, 0x537EA0}};
constexpr sh::CallSite kCalls537ED0[] = {{0x0, 0x589810}};

#define R2A_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define R2A_REF(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kSp = sh::Shape::kSprite, kCa = sh::Shape::kCall;
// Answers: Char_LoseAp's eax whole (what it took; Char_LoseHp's twin, whose
// callers read ax), LinkedObject_EffectRise's al (its caller movsx's al);
// everything else is void (Char_GainHp's eax is a record offset nobody reads).
const sh::Clone kAll[] = {
    {"Mode11_ListedSpriteScreens", 0x5372E0, 0x103, kCalls5372E0, R2A_N(kCalls5372E0), nullptr, 0, nullptr, 0,
     R2A_REF(Mode11_ListedSpriteScreens), 0, false, kSp},
    {"Char_GainHp", 0x5373F0, 0x89, nullptr, 0, nullptr, 0, nullptr, 0, R2A_REF(Char_GainHp), 0, false, kCa},
    {"Char_LoseAp", 0x537500, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, R2A_REF(Char_LoseAp), 0xFFFFFFFFu, false, kCa},
    {"Area_ObjectHandler", 0x537540, 0x3A, nullptr, 0, nullptr, 0, nullptr, 0, R2A_REF(Area_ObjectHandler), 0, false, kCa},
    {"LinkedObjectA_Run", 0x5375A0, 0x3E, nullptr, 0, kImms5375A0, R2A_N(kImms5375A0), nullptr, 0, R2A_REF(LinkedObjectA_Run),
     0, false, kSp},
    {"LinkedObjectA_Roll", 0x5375E0, 0x178, kCalls5375E0, R2A_N(kCalls5375E0), nullptr, 0, kTables5375E0, R2A_N(kTables5375E0),
     R2A_REF(LinkedObjectA_Roll), 0, false, kSp},
    {"LinkedObject_Show2", 0x537760, 0x44, kCalls537760, R2A_N(kCalls537760), nullptr, 0, nullptr, 0, R2A_REF(LinkedObject_Show2),
     0, false, kSp},
    {"LinkedObject_Show5", 0x5377B0, 0x44, kCalls5377B0, R2A_N(kCalls5377B0), nullptr, 0, nullptr, 0, R2A_REF(LinkedObject_Show5),
     0, false, kSp},
    {"LinkedObject_EndAfterEffect", 0x537800, 0x4F, kCalls537800, R2A_N(kCalls537800), nullptr, 0, nullptr, 0,
     R2A_REF(LinkedObject_EndAfterEffect), 0, false, kSp},
    {"LinkedObject_EndAfterMessage", 0x537850, 0x48, kCalls537850, R2A_N(kCalls537850), nullptr, 0, nullptr, 0,
     R2A_REF(LinkedObject_EndAfterMessage), 0, false, kSp},
    {"LinkedObjectB_Run", 0x5378A0, 0x26, nullptr, 0, kImms5378A0, R2A_N(kImms5378A0), nullptr, 0, R2A_REF(LinkedObjectB_Run),
     0, false, kSp},
    {"LinkedObjectB_Roll", 0x5378D0, 0x77, kCalls5378D0, R2A_N(kCalls5378D0), nullptr, 0, nullptr, 0, R2A_REF(LinkedObjectB_Roll),
     0, false, kSp},
    {"LinkedObjectC_Run", 0x537950, 0x3E, nullptr, 0, kImms537950, R2A_N(kImms537950), nullptr, 0, R2A_REF(LinkedObjectC_Run),
     0, false, kSp},
    {"LinkedObjectC_Roll", 0x537990, 0x178, kCalls537990, R2A_N(kCalls537990), nullptr, 0, kTables537990, R2A_N(kTables537990),
     R2A_REF(LinkedObjectC_Roll), 0, false, kSp},
    {"LinkedObjectD_Run", 0x537B10, 0x3F, nullptr, 0, kImms537B10, R2A_N(kImms537B10), nullptr, 0, R2A_REF(LinkedObjectD_Run),
     0, false, kSp},
    {"LinkedObjectD_Roll", 0x537B50, 0x190, kCalls537B50, R2A_N(kCalls537B50), nullptr, 0, kTables537B50, R2A_N(kTables537B50),
     R2A_REF(LinkedObjectD_Roll), 0, false, kSp},
    {"LinkedObjectD_ShowAmount", 0x537CE0, 0x67, kCalls537CE0, R2A_N(kCalls537CE0), nullptr, 0, nullptr, 0,
     R2A_REF(LinkedObjectD_ShowAmount), 0, false, kSp},
    {"LinkedObjectD_WaitMessage", 0x537D50, 0x13, nullptr, 0, nullptr, 0, nullptr, 0, R2A_REF(LinkedObjectD_WaitMessage), 0,
     false, kSp},
    {"LinkedObjectD_ColourStep", 0x537D70, 0x64, kCalls537D70, R2A_N(kCalls537D70), nullptr, 0, nullptr, 0,
     R2A_REF(LinkedObjectD_ColourStep), 0, false, kSp},
    {"LinkedObject_SpawnEffect19", 0x537DE0, 0xB6, kCalls537DE0, R2A_N(kCalls537DE0), nullptr, 0, nullptr, 0,
     R2A_REF(LinkedObject_SpawnEffect19), 0, false, kCa},
    {"LinkedObject_EffectRise", 0x537EA0, 0x30, nullptr, 0, nullptr, 0, nullptr, 0, R2A_REF(LinkedObject_EffectRise), 0xFFu, false,
     kSp},
    {"LinkedObject_SpawnEffect32", 0x537ED0, 0x4C, kCalls537ED0, R2A_N(kCalls537ED0), nullptr, 0, nullptr, 0,
     R2A_REF(LinkedObject_SpawnEffect32), 0, false, kCa},
};
#undef R2A_REF
#undef R2A_N
constexpr unsigned kCount = sizeof kAll / sizeof kAll[0];
static_assert(kCount == 22, "the cut's 19 rows for R2A and the three starts no list had");

// The dispatchers' stack tables: their counts, for the state byte +4.
unsigned StatesOf(U base) {
    switch (base) {
    case 0x5375A0:
    case 0x537950: return 5;
    case 0x5378A0: return 2;
    case 0x537B10: return 6;
    default: return 0;
    }
}

// --- the stand-ins' answers (Noise() and the state only: both passes the same) ----------

U WithAl(U answer, U al) { return (answer & 0xFFFFFF00u) | (al & 0xFF); }

// Effect_FindFree: none (0xFF) a quarter of the time, else a record 0..19 -
// what the real one answers (the spawns write the record unchecked).
U FxRecordOrNone(const U*, U answer) {
    const U n = sh::Noise();
    return WithAl(answer, n % 4 == 0 ? 0xFF : (n >> 4) % 20);
}
// Sprite_UpdateScreen / _A draw Sprite_Current: which record it is, logged.
U FxOnCurrent(const U*, U answer) {
    sh::Note(Key(Sprite_Current));
    return answer;
}

#define R2A_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr U kW = 0xFFFFFFFFu, kU8 = 0xFFu;
const sh::Callee kCallees[] = {
    {R2A_OURS(Effect_FindFree), 0, {}, kG, 0, 0, {}, &FxRecordOrNone},
    {R2A_OURS(Sprite_UpdateScreen), 0, {}, kG, 0, 0, {}, &FxOnCurrent},
    {R2A_OURS(Sprite_UpdateScreenA), 0, {}, kG, 0, 0, {}, &FxOnCurrent},
    // Sprite_FaceDirection reads its argument's byte; the rolls push eax with
    // Rand's upper bytes, the ends edx / eax with whatever was there
    {R2A_OURS(Sprite_FaceDirection), 1, {kU8}, kG, 0, 0, {}, &FxOnCurrent},
    {"Rand", 0x5B93D2, KeyOf(&::Rand), 0, {}, sh::Answer::kRand, 0, 0},   // Capcom's CRT rand, not ours
    // the group's own, called by E8: the spawns read the argument's byte (the
    // rolls push the member's index as a whole dword, `mov cl, [esp + 4]`)
    {R2A_OURS(LinkedObject_SpawnEffect19), 1, {kU8}, kG, 0, 0, {}, &FxOnCurrent},
    {R2A_OURS(LinkedObject_SpawnEffect32), 1, {kU8}, kG, 0, 0, {}, &FxOnCurrent},
    {R2A_OURS(LinkedObject_EffectRise), 0, {}, kG, 0, 0},
    {R2A_OURS(Zenny_Add), 2, {kW, kW}, kG, 0, 0},
};
#undef R2A_OURS

// Beyond field mode's standard regions: the active member's pointer cell, the
// bank list (its count, its records - 17, the 16 a seed counts and the one
// past - and the pointer to them), Draw_OtSlot's dword, Text_Records' first 16
// bytes (Crt_sprintf's destination) and CharacterRecords past what the style
// cells hold (0x903A94 up to Cond_Flags: records 0..7).
const sh::Region kRegions[] = {  // (dynamic initialisation: the first is a symbol's address)
    {Key(&Field_ActiveMember), 4},
    {at::kBankCount, 4 + 17 * 8},
    {at::kBankRecords, 4},
    {0x92BF18, 4},
    {bof3::addr::Text_Records, 0x10},
    {0x903A94, 0x903F90 - 0x903A94},
};

// --- the seed ----------------------------------------------------------------------------

unsigned char* Records() { return sh::Mem(at::kBankRecordsAt); }

// The bank list: up to 16 records, each a bank word that is often one of
// Mode11_ScreenBanks (read in place) and a +7 byte.
void SeedBanks() {
    sh::Mem(at::kBankCount)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 4, 8, 16, sh::Next() % 17));
    sh::SetPointer(at::kBankRecords, Records());
    unsigned char* const r = Records();
    for (unsigned i = 0; i < 17; ++i) {
        const U bank = sh::Often() ? Mode11_ScreenBanks[sh::Next() % (Mode11_ScreenBanks_count - 1)] : PickOf(0xFFFF, 0, sh::Next());
        SetWord(r + i * 8, bank);
        r[i * 8 + 7] = static_cast<unsigned char>(PickOf(i, i, sh::Next() % 17, sh::Next()));
    }
}

// One sprite record's bytes the group reads: +0 bit 0 (live), the type +6
// (9 and 0xA, their tests), +7 bit 3, the direction +8, the effect index +0xB
// (0..19, or Effect_FindFree's 0xFF), +0x24 bit 4, the bank byte word +0x2C
// (one of the list's +7 bytes, often), the colour bytes +0x5D..+0x5F, the
// member's +0x80 / +0xA0.
void SeedSprite(unsigned char* s) {
    s[0] = static_cast<unsigned char>(sh::Half() ? s[0] | 1 : s[0] & 0xFE);
    s[6] = static_cast<unsigned char>(PickOf(9, 0xA, 1, 4, sh::Next()));
    s[7] = static_cast<unsigned char>(PickOf(0, 8, sh::Next()));
    s[8] = static_cast<unsigned char>(sh::Often() ? sh::Next() % 8 : sh::Next());
    s[0xB] = static_cast<unsigned char>(sh::Next() % 6 == 0 ? 0xFF : sh::Next() % 20);
    s[0x24] = static_cast<unsigned char>(PickOf(0, 0x10, sh::Next()));
    SetWord(s + 0x2C, sh::Often() ? Records()[(sh::Next() % 17) * 8 + 7] : PickOf(0x100, sh::Next()));
    s[0x5D] = static_cast<unsigned char>(PickOf(0, 0xB0, 0xF0, 0x10, sh::Next()));
    s[0xA0] = static_cast<unsigned char>((sh::Half() ? 0x7F : sh::Next() % 7) | (sh::Half() ? 0x80 : 0));
}

// The character records: HP +0x18 against its maximum +0x20 (below, at,
// above, a quarter of it), AP +0x1A at 0, 1 and random.
void SeedRecord(unsigned char* r) {
    const U max = PickOf(0, 1, 4, 100, 999, 0x7FFF, 0xFFFF, sh::Next() & 0xFFFF);
    SetWord(r + 0x20, max);
    SetWord(r + 0x18, PickOf(max, max - 1, max + 1, max / 4, max / 4 + 1, 0, 1, sh::Next()));
    SetWord(r + 0x1A, PickOf(0, 1, 2, 0xFFFF, sh::Next() & 0xFF, sh::Next()));
}

unsigned g_clone;   // the round's clone's index in kAll (set by Seed, read by Disturb)

void Seed(unsigned k) {
    g_clone = k;
    const sh::Clone& c = kAll[k];
    SeedBanks();
    for (unsigned i = 0; i < 30; ++i) {
        unsigned char* const s = Sprite_Objects + i * 0xA4;
        if (i < 4 || sh::Next() % 4 == 0) SeedSprite(s);
    }
    for (unsigned i = 0; i < 8; ++i) SeedRecord(sh::Mem(bof3::addr::CharacterRecords + i * at::kRecordStride));
    Field_State[0x148] = static_cast<unsigned char>(sh::Next() % 8);
    // the active member: a sprite record (Sprite_Current itself a third of the
    // time); Area_ObjectHandler's area 74 (its table is a DataTable here)
    Field_ActiveMember = sh::Often() ? sh::SpriteRecord(sh::Next()) : Sprite_Current;
    Game_AreaNumber = 74;
    Field_Request = static_cast<unsigned char>(PickOf(2, 2, 0, 3, sh::Next()));
    Draw_OtSlot = static_cast<unsigned char>(PickOf(4, 4, 6, sh::Next() % 8));
    // each sprite record's effect record in use or not, about half and half
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned e = sh::SpriteRecord(i)[0xB];
        if (e < 20) Effect_Objects[e * 0x80] = static_cast<unsigned char>(PickOf(0, 1, sh::Next()));
    }
    unsigned char* const s = Sprite_Current;
    if (const unsigned n = StatesOf(c.base)) s[4] = static_cast<unsigned char>(sh::Next() % n);
    switch (c.base) {
    case 0x5375E0:
    case 0x537990:
    case 0x537B50:
    case 0x5378D0:   // the rolls: Rand & 0xF at the switch's edges
        sh::SetRandHint(PickOf(3, 4, 6, 7, 8, 0xF, 0x10));
        break;
    case 0x537CE0:   // states 1..3 reach it; 0 reads the table's first byte
        s[4] = static_cast<unsigned char>(sh::Next() % 4);
        break;
    default: break;
    }
}

// The record helpers' member byte below MoveScript_EffectState's 24 (past it
// both read the .data after it unchecked), the amount at the HP / AP edges;
// Area_ObjectHandler's (short) n inside Area_ObjectFallbacks' 11 under random
// upper bytes; the spawns' sub-kind any dword.
void Args(unsigned k, U* a) {
    switch (kAll[k].base) {
    case 0x5373F0:
    case 0x537500:
        a[0] = sh::Half() ? PickOf(0, 1, 2, 0xFFFF, 0x10000, 0xFFFFFFFFu, 0x8000) : a[0];
        a[1] = (a[1] & 0xFFFFFF00u) | (sh::Next() % 24);
        break;
    case 0x537540: a[0] = (a[0] & 0xFFFF0000u) | (sh::Next() % 11); break;
    default: break;
    }
}

// What the group's functions read again after a call, moved by the group's
// case of the harness's disturbance (from its hash only): Sprite_Current's
// +7, +8, +0xB (0..19 only: the spawn re-reads it through the old sprite and
// writes the record), +0x34..+0x3C, +0x5D..+0x5F; the active member pointer
// and its +0xA0; an effect record's +0; Field_Request; Draw_OtSlot; a bank
// record's +7.
void Disturb(U h) {
    const U v = h >> 8;
    unsigned char* const s = Sprite_Current;
    switch (sh::DisturbCase(h, 11)) {
    case 0: s[7] = static_cast<unsigned char>(v & 1 ? 8 : v >> 1); break;
    case 1: s[8] = static_cast<unsigned char>(v >> 1); break;
    case 2: s[0xB] = static_cast<unsigned char>((v >> 1) % 20); break;
    case 3: SetLong(s + 0x34 + 4 * (v % 3), static_cast<std::int32_t>(v << 5)); break;
    case 4: s[0x5D + v % 3] = static_cast<unsigned char>(v >> 2); break;
    case 5: Field_ActiveMember = sh::SpriteRecord(v); break;
    case 6: Field_ActiveMember[0xA0] = static_cast<unsigned char>(Field_ActiveMember[0xA0] ^ 0x80); break;
    case 7: Effect_Objects[((v >> 1) % 20) * 0x80] = static_cast<unsigned char>(v & 1); break;
    case 8: Field_Request = static_cast<unsigned char>(v & 1 ? 2 : v >> 1); break;
    case 9: Draw_OtSlot = static_cast<unsigned char>(v & 1 ? 4 : v >> 1); break;
    case 10: Records()[(v % 17) * 8 + 7] = static_cast<unsigned char>(v >> 5); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    // Area 74's handler table, as Area_ObjectHandler reaches it: must be the
    // one symbols.toml names (its 7 entries become recorders)
    const auto area74 = static_cast<U>(Long(Area_Descriptors[74] + 0x3C));
    if (area74 != Key(Area74_Handlers))
        bof3::Fatal("rest_2a: area 74's descriptor +0x3C is 0x%X, not Area74_Handlers 0x%X", (unsigned)area74,
                    (unsigned)Key(Area74_Handlers));
    const sh::DataTable kTables[] = {
        {Key(Area_ObjectFallbacks), Area_ObjectFallbacks_count},
        {Key(Area74_Handlers), Area74_Handlers_count},
    };
    // BOF3X_R2A_ONLY: the clones whose name contains it (a control's run)
    static sh::Clone chosen[kCount];
    static unsigned index[kCount];
    const char* const only = std::getenv("BOF3X_R2A_ONLY");
    unsigned n = 0;
    for (unsigned k = 0; k < kCount; ++k)
        if (!only || !*only || std::strstr(kAll[k].name, only)) {
            index[n] = k;
            chosen[n++] = kAll[k];
        }
    if (n == 0) bof3::Fatal("rest_2a: BOF3X_R2A_ONLY=%s names no clone", only);
    static unsigned* s_index = index;
    sh::Group g = {"rest_2a", chosen, n, kCallees, sizeof kCallees / sizeof kCallees[0], kTables,
                   sizeof kTables / sizeof kTables[0], kRegions, sizeof kRegions / sizeof kRegions[0],
                   [](unsigned k) { Seed(s_index[k]); }, &Disturb, 6000};
    g.args = [](unsigned k, U* a) { Args(s_index[k], a); };
    g.field = true;
    sh::Run(g);
}

}  // namespace rest_2a
