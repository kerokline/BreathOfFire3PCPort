// BOF3X_SHADOW=area_w4d: world 4's areas 175..187 through the area round's
// shared harness (area_harness.h), once at start-up - one area_harness::Run
// per area, each Group setting its own area number, all under the one shadow
// name. docs/area_w4d.md section 3.
//
// The clone tables are tools/area_rows.py --clones's rows for AREA175..187
// (2026-09-28), each row read against the disassembly (every start, extent,
// call site and the twelve in-function jump tables agree); the shapes are the
// root table each function hangs from (docs/area_w4d.md section 1). Areas
// 175..185's shared bodies run under the area whose block holds them; each
// area's own copy of choice 27 under that area. The group's own callee (area
// 186's start) is a recorder here like any other callee, so each function is
// fuzzed alone; area 175's glide states are swapped for recorders
// (DataTable).
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4d.h"
#include "game/area_w4d_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w4d {
namespace {

namespace ah = area_harness;
using S = ah::Shape;
using U = std::uint32_t;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define OURS(f) reinterpret_cast<const void*>(&::f)

// ---- the clone rows ----
// Choice 27's eleven copies: the jmp's disp32 at +0xF, its five-entry table at +0x48.
constexpr ah::JumpTable kTablesChoice27[] = {{0xF, 0x48, 5}};
// ---- area 175 ----
constexpr ah::CallSite kCalls4293C0[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls4294F0[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls429540[] = {{0x35, 0x4976D0}};
constexpr ah::CallSite kCalls4295B0[] = {{0x24, 0x5891F0}};
constexpr ah::CallSite kCalls4295E0[] = {{0x29, 0x57C0F0}, {0x30, 0x531F90}};
// ---- area 178 ----
constexpr ah::CallSite kCalls429810[] = {{0x5, 0x587900}};
// ---- area 179 ----
constexpr ah::CallSite kCalls4298A0[] = {{0x9, 0x455450}, {0x23, 0x587740}};
// ---- area 185 ----
constexpr ah::CallSite kCalls429DC0[] = {{0x2F, 0x587740}, {0x4B, 0x594E00}};
constexpr ah::CallSite kCalls429E20[] = {{0x2C, 0x5A77C0}, {0x42, 0x572FA0}, {0x5B, 0x5A7750}, {0x63, 0x5A7780}, {0xA4, 0x5A7A00},
                                         {0xD9, 0x5B9550}, {0xEE, 0x5A7A50}, {0x112, 0x5B9550}, {0x121, 0x5A7A00}, {0x14A, 0x5A8250},
                                         {0x153, 0x5A9110}, {0x169, 0x572FA0}, {0x1B6, 0x5A77C0}, {0x1CC, 0x572FA0}};
// ---- area 186 ----
constexpr ah::CallSite kCalls42A000[] = {{0x6C, 0x56FCA0}, {0x104, 0x56FCA0}, {0x147, 0x594E00}};
constexpr ah::JumpTable kTables42A000[] = {{0x1B, 0x15C, 5}};
constexpr ah::CallSite kCalls42A180[] = {{0x7, 0x57C140}, {0x1E, 0x42A1C0}, {0x2D, 0x42A1C0}};
constexpr ah::CallSite kCalls42A1C0[] = {{0x5B, 0x5734F0}};
// ---- area 187 ----
constexpr ah::CallSite kCalls42A290[] = {{0x1B, 0x41D430}, {0x2E, 0x57C110}, {0x4A, 0x594E00}, {0x5C, 0x41D430}, {0x61, 0x57C7C0}, {0x68, 0x4976D0}};

#define CHOICE27(area, base) \
    {"Area" #area "_ChoiceMessageByAnswer", base, 0x5C, nullptr, 0, nullptr, 0, kTablesChoice27, AH_N(kTablesChoice27), \
     OURS(Area##area##_ChoiceMessageByAnswer), 0, false, S::kChoice}

const ah::Clone kClones175[] = {
    CHOICE27(175, 0x4292C0),
    {"Area175_ClampXToKind2", 0x429320, 0x16, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area175_ClampXToKind2), 0, false, S::kHandler},
    {"Area175_SlideOrPlace", 0x429340, 0x79, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area175_SlideOrPlace), 0, false, S::kHandler},
    {"Area175_SpawnEffect1F", 0x4293C0, 0x7F, kCalls4293C0, AH_N(kCalls4293C0), nullptr, 0, nullptr, 0, OURS(Area175_SpawnEffect1F), 0, false, S::kHandler},
    {"Area175_GlideRun", 0x429440, 0x12, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area175_GlideRun), 0, false, S::kHandler},
    {"Area175_GlideBegin20", 0x429460, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area175_GlideBegin20), 0, false, S::kState},
    {"Area175_GlideStep", 0x429480, 0x62, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area175_GlideStep), 0, false, S::kState},
    {"Area175_SpawnEffect39", 0x4294F0, 0x44, kCalls4294F0, AH_N(kCalls4294F0), nullptr, 0, nullptr, 0, OURS(Area175_SpawnEffect39), 0, false, S::kHandler},
    {"Area175_OpenScriptMessage", 0x429540, 0x67, kCalls429540, AH_N(kCalls429540), nullptr, 0, nullptr, 0, OURS(Area175_OpenScriptMessage), 0, false,
     S::kHandler},
    {"Area175_PoseAnimA", 0x4295B0, 0x2B, kCalls4295B0, AH_N(kCalls4295B0), nullptr, 0, nullptr, 0, OURS(Area175_PoseAnimA), 0, false, S::kHandler},
    {"Area175_Trigger64", 0x4295E0, 0x42, kCalls4295E0, AH_N(kCalls4295E0), nullptr, 0, nullptr, 0, OURS(Area175_Trigger64), 0xFF, false, S::kCallee},
    {"Area175_ChoiceAskZenny6F", 0x429630, 0x4D, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area175_ChoiceAskZenny6F), 0, false, S::kChoice},
    {"Area175_ChoiceMessage9F", 0x429680, 0x19, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area175_ChoiceMessage9F), 0, false, S::kChoice},
};
enum : unsigned { k175Choice27, k175Clamp, k175Slide, k175Spawn1F, k175GlideRun, k175GlideBegin, k175GlideStep, k175Spawn39, k175Script, k175Pose,
                  k175Trigger, k175Zenny, k175Message9F };
const ah::Clone kClones176[] = {
    CHOICE27(176, 0x4296A0),
    {"Area176_ChoiceCount3E", 0x429700, 0x30, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area176_ChoiceCount3E), 0, false, S::kChoice},
};
const ah::Clone kClones177[] = {
    CHOICE27(177, 0x429730),
    {"Area177_ChoiceMessageF9", 0x429790, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area177_ChoiceMessageF9), 0, false, S::kChoice},
};
const ah::Clone kClones178[] = {
    CHOICE27(178, 0x4297B0),
    {"Area178_PlaySound201", 0x429810, 0xC, kCalls429810, AH_N(kCalls429810), nullptr, 0, nullptr, 0, OURS(Area178_PlaySound201), 0, false, S::kHandler},
    {"Area178_ChoiceMessageEF", 0x429820, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area178_ChoiceMessageEF), 0, false, S::kChoice},
};
const ah::Clone kClones179[] = {
    CHOICE27(179, 0x429840),
    {"Area179_Init", 0x4298A0, 0x2A, kCalls4298A0, AH_N(kCalls4298A0), nullptr, 0, nullptr, 0, OURS(Area179_Init), 0, false, S::kInit},
    {"Area179_CellHook", 0x4298D0, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area179_CellHook), 0xFF, false, S::kHook},
    {"Area179_ChoiceVar8", 0x429910, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area179_ChoiceVar8), 0, false, S::kChoice},
};
enum : unsigned { k179Choice27, k179Init, k179Cell, k179Var8 };
const ah::Clone kClones180[] = {
    CHOICE27(180, 0x429930),
    {"Area180_ChoiceStoreAnswer", 0x429990, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area180_ChoiceStoreAnswer), 0, false, S::kChoice},
};
const ah::Clone kClones181[] = {
    CHOICE27(181, 0x4299B0),
    {"Area181_ChoiceCounter0", 0x429A10, 0x30, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area181_ChoiceCounter0), 0, false, S::kChoice},
};
const ah::Clone kClones182[] = {
    CHOICE27(182, 0x429A40),
};
const ah::Clone kClones183[] = {
    CHOICE27(183, 0x429AA0),
    {"Area183_ChoiceMessage78", 0x429B00, 0x38, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area183_ChoiceMessage78), 0, false, S::kChoice},
};
const ah::Clone kClones184[] = {
    CHOICE27(184, 0x429B40),
    {"Area184_ChoiceMessage5A", 0x429BA0, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area184_ChoiceMessage5A), 0, false, S::kChoice},
    {"Area184_ChoiceTailState1", 0x429BD0, 0x25, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area184_ChoiceTailState1), 0, false, S::kChoice},
    {"Area184_ChoiceMessage7A", 0x429C00, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area184_ChoiceMessage7A), 0, false, S::kChoice},
    {"Area184_ChoiceMessage77", 0x429C30, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area184_ChoiceMessage77), 0, false, S::kChoice},
    {"Area184_ChoiceAskZenny7F", 0x429C60, 0x48, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area184_ChoiceAskZenny7F), 0, false, S::kChoice},
    {"Area184_ChoiceMessage89", 0x429CB0, 0x24, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area184_ChoiceMessage89), 0, false, S::kChoice},
    {"Area184_ChoiceMessageF7", 0x429CE0, 0x29, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area184_ChoiceMessageF7), 0, false, S::kChoice},
};
enum : unsigned { k184Choice27, k184M5A, k184Tail1, k184M7A, k184M77, k184Zenny, k184M89, k184MF7 };
const ah::Clone kClones185[] = {
    CHOICE27(185, 0x429D10),
    {"Area185_ChoiceSetFacility0", 0x429D70, 0x41, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area185_ChoiceSetFacility0), 0, false, S::kChoice},
    {"Area185_StepHook", 0x429DC0, 0x60, kCalls429DC0, AH_N(kCalls429DC0), nullptr, 0, nullptr, 0, OURS(Area185_StepHook), 0xFF, false, S::kHook},
    {"Area185_DrawTileField", 0x429E20, 0x1DC, kCalls429E20, AH_N(kCalls429E20), nullptr, 0, nullptr, 0, OURS(Area185_DrawTileField), 0, false, S::kState},
};
enum : unsigned { k185Choice27, k185Facility, k185Step, k185Draw };
const ah::Clone kClones186[] = {
    {"Area186_TailShift", 0x42A000, 0x17C, kCalls42A000, AH_N(kCalls42A000), nullptr, 0, kTables42A000, AH_N(kTables42A000), OURS(Area186_TailShift), 0,
     false, S::kTail},
    {"Area186_Init", 0x42A180, 0x34, kCalls42A180, AH_N(kCalls42A180), nullptr, 0, nullptr, 0, OURS(Area186_Init), 0, false, S::kInit},
    {"Area186_Start", 0x42A1C0, 0x7B, kCalls42A1C0, AH_N(kCalls42A1C0), nullptr, 0, nullptr, 0, OURS(Area186_Start), 0, false, S::kCallee},
};
enum : unsigned { k186Tail, k186Init, k186Start };
const ah::Clone kClones187[] = {
    {"Area187_ChoiceFocusPair", 0x42A240, 0x3C, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area187_ChoiceFocusPair), 0, false, S::kChoice},
    {"Area187_Trigger14", 0x42A280, 0xA, nullptr, 0, nullptr, 0, nullptr, 0, OURS(Area187_Trigger14), 0xFF, false, S::kCallee},
    {"Area187_TailMessage2", 0x42A290, 0x84, kCalls42A290, AH_N(kCalls42A290), nullptr, 0, nullptr, 0, OURS(Area187_TailMessage2), 0, false, S::kTail},
};
enum : unsigned { k187Choice, k187Trigger, k187Tail };
#undef CHOICE27
#undef AH_N
#undef OURS

const ah::DataTable kTables175[] = {{at::kArea175GlideStates, at::kArea175GlideStateCount}};

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }
unsigned char* EffectRecord(unsigned slot) { return ah::Mem(at::kEffectObjects + slot % at::kEffectCount * at::kEffectStride); }

// ---- the fuzz's own memory (its region placed at SelfTest) ----

// The packet buffer Gfx_PacketNext points into while the fuzz runs (area
// 185's draw builds 258 primitives at the pointer, each linked in turn).
constexpr unsigned kPacketBytes = 0x1000;
constexpr unsigned kPacketMargin = 0x80;
alignas(16) unsigned char g_packets[kPacketBytes];
void SetPacketNext(U v) { Gfx_PacketNext = g_packets + kPacketMargin + (v % 32) * 4; }
bool InPackets(U p, unsigned n) { return p >= Key(g_packets) && p + n <= Key(g_packets) + kPacketBytes; }

// Which area and function the round is running (set by the seeds).
int g_area = 0;
unsigned g_k = 0;
bool Running(int area, unsigned k) { return g_area == area && g_k == k; }

// ---- pointers the areas follow ----

// A record the active member pointer may name: one of the four party objects
// (Sprite_ObjectsExtra), a field object, a party record, or the running
// object itself.
unsigned char* MemberRecord(U v) {
    switch (v % 4) {
    case 0: return ah::Mem(at::kSpriteObjectsExtra + (v >> 2) % 4 * 0xA4);
    case 1: return ah::Object(v >> 2);
    case 2: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return Sprite_Current;
    }
}
unsigned char* ScriptRecord(U v) { return v & 1 ? ah::Object(v >> 1) : ah::PartyOf(static_cast<unsigned char>(v >> 1)); }
// The focus object: a field object, or one of Sprite_ObjectsExtra's four.
unsigned char* FocusRecord(U v) { return v % 5 == 0 ? ah::Mem(at::kSpriteObjectsExtra + (v >> 3) % 4 * 0xA4) : ah::Object(v >> 3); }

// Area 175's script message indexes the descriptor's +0x10 table by the
// script object's +3 unchecked, as the engine does: the fuzz keeps it inside
// the fourteen scripts after every move of the pointer or the byte.
void Settle() {
    if (!Running(175, k175Script)) return;
    unsigned char* const object = ah::Pointer(at::kScriptObject);
    object[3] = static_cast<unsigned char>(object[3] % at::kArea175ScriptCount);
}

// ---- the stand-ins the group lists ----

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, on purpose (each only part of the time, from
// Noise): the callers read cells again after these calls. The harness's own
// disturbance reaches a group cell about one call in 24.
void MoveCurrent(U n) { Sprite_Current = n & 0x100 ? ah::PartyOf(static_cast<unsigned char>(n >> 9)) : ah::Object((n >> 9) & 3); }
// Effect_FindFree: area 175's two spawns read Sprite_Current again after it
// (and the one's miss steps the active member).
U FindFreeEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) MoveCurrent(n);
    if (n & 2) ah::SetPointer(at::kActiveMember, MemberRecord(n >> 12));
    return answer;
}
// Msg_OpenScript: area 175's script message reads the script object again
// after it, area 187's tail its state.
U OpenScriptEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) {
        ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 8));
        Settle();
    }
    if (n & 2) B(at::kTailState) = static_cast<unsigned char>(n & 4 ? (n >> 16) % 3 : n >> 16);
    if (n & 8) Field_Request = static_cast<unsigned char>(n & 0x10 ? 2 : n >> 24);
    return answer;
}
// ScriptFlags_Set40: area 187's tail reads its state after it (and after
// the message).
U Set40Effect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) B(at::kTailState) = static_cast<unsigned char>(n >> 8);
    return answer;
}
// The sound, the disarm and the flag clear: the step hook and area 187's tail
// read the return point after them.
U MovesReturnPoint(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 3 == 0) {
        static const U kCells[] = {at::kReturnX, at::kReturnZ, at::kReturnArea};
        move_script::SetWord(ah::Mem(kCells[(n >> 4) % 3] + ((n >> 6) & 2)), n >> 16);
    }
    return answer;
}
// The engine's field reset: area 179's init reads Cond_ByteFD after it.
U FieldResetEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Cond_ByteFD = static_cast<unsigned char>(n & 2 ? 0 : n >> 8);
    return answer;
}
// MapView_FillCells: tail kind 39 counts its timer down after it.
U FillCellsEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) move_script::SetWord(ah::Mem(at::kTailTimer), n & 2 ? 1 : n >> 16);
    return answer;
}
// The draw's stand-ins. The link logs the primitive at Gfx_PacketNext (every
// primitive of the draw is built at the pointer, so without the log only the
// last would be compared), then moves it on by its size (four calls in five),
// kept in the buffer; the setters write the primitive's bytes.
void Advance(U size) {
    size &= 0xFF;
    if (InPackets(Key(Gfx_PacketNext), size)) ah::NoteBytes(Gfx_PacketNext, size);
    const U next = Key(Gfx_PacketNext) + size;
    if (!InPackets(next, 0x80)) SetPacketNext(ah::Noise());
    else Gfx_PacketNext = reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(next));
}
U LinkEffect(const U* a, U answer) {
    const U n = ah::Noise();
    if (n % 5) Advance(a[3]);
    if (n % 7 == 0) MoveCurrent(n >> 4);
    return answer;
}
void Scribble(U p, unsigned n) {
    if (!InPackets(p, n)) return;
    ah::FillBytes(ah::Mem(p), n);
}
U DrawModeEffect(const U* a, U answer) { Scribble(a[0], 0xC); return answer; }
U Tile1Effect(const U* a, U answer) { Scribble(a[0], 0x14); return answer; }
U SemiEffect(const U* a, U answer) {
    if (InPackets(a[0], 8)) ah::Mem(a[0])[7] = static_cast<unsigned char>(a[1] ? ah::Mem(a[0])[7] | 2 : ah::Mem(a[0])[7] & 0xFD);
    return answer;
}
// A float the screen could hold: a small whole or half number.
void SomeFloat(U cell, U n) {
    const float f = static_cast<float>(static_cast<std::int32_t>((n >> 4) % 2400) - 1200) + ((n >> 16) & 1 ? 0.5f : 0.0f);
    std::memcpy(ah::Mem(cell), &f, sizeof f);
}
// Math_Sin moves the frame counter (the draw reads it once, after the first
// sine) and now and then Sprite_Current; Math_Cos the counter (which the draw
// must not read again) and the screen floats (which it reads at each fadd).
U SinEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 4 == 0) Frame_Counter = n >> 8;
    if (n % 9 == 1) MoveCurrent(n >> 3);
    return answer;
}
U CosEffect(const U*, U answer) {
    const U n = ah::Noise();
    if (n % 4 == 0) Frame_Counter = n >> 8;
    if (n % 5 == 1) SomeFloat(at::kScreenXY + ((n >> 3) & 4), n >> 5);
    return answer;
}
// Gte_RotTransPers writes the screen point into the tile (two words of x, y
// at +8 on the PC's primitives, eight bytes); Gte_StoreDepthF a float at
// +0x10.
U PersEffect(const U* a, U answer) {
    if (InPackets(a[1], 8)) ah::FillBytes(ah::Mem(a[1]), 8);
    return answer;
}
U DepthEffect(const U* a, U answer) {
    if (InPackets(a[0], 4)) SomeFloat(a[0], ah::Noise());
    return answer;
}

#define W4D_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
const ah::Callee kCallees[] = {
    // slots inside the group's four effect records, or none
    {W4D_OURS(Effect_FindFree), 0, {}, ah::Answer::kByte, 0xFF, 0x03, {}, &FindFreeEffect},
    {W4D_OURS(Msg_OpenScript), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &OpenScriptEffect},
    {W4D_OURS(Sprite_SetAnimation), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W4D_OURS(Flags_Set), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0},
    {W4D_OURS(Flags_Clear), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesReturnPoint},
    {W4D_OURS(Flags_Test), 2, {kAll, kAll}, ah::Answer::kFlag, 0, 0},
    {W4D_OURS(Party_DropIn), 1, {kAll}, ah::Answer::kGarbage, 0, 0},
    {W4D_OURS(Sound_PlayById), 1, {kU16}, ah::Answer::kGarbage, 0, 0},
    {W4D_OURS(Sound_PlayEffect), 1, {kU16}, ah::Answer::kGarbage, 0, 0, {}, &MovesReturnPoint},
    // the area as a word, the flags as a byte (window_task.cpp reads no more)
    {W4D_OURS(Field_ChangeArea), 4, {kU16, kAll, kAll, kU8}, ah::Answer::kGarbage, 0, 0},
    {W4D_OURS(MapView_FillCells), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &FillCellsEffect},
    {W4D_OURS(Kind2_Place), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
    {W4D_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &Set40Effect},
    {W4D_OURS(Area131_DisarmTail), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &MovesReturnPoint},
    // Capcom's, unnamed: the engine's field reset
    {"FieldReset_455450", at::kFieldReset, at::kFieldReset, 0, {}, ah::Answer::kGarbage, 0, 0, {}, &FieldResetEffect},
    // the draw: the vertex by its six bytes, the tile by its address in the
    // buffer, the depth local not at all; the CRT's _ftol the originals'
    // own (both copies keep it)
    {W4D_OURS(Gpu_SetDrawMode), 5, {kAll, kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &DrawModeEffect},
    {W4D_OURS(MapView_LinkPrimAt), 4, {kAll, kAll, kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &LinkEffect},
    {W4D_OURS(Gpu_SetTile1), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &Tile1Effect},
    {W4D_OURS(Gpu_SetSemiTrans), 2, {kAll, kAll}, ah::Answer::kGarbage, 0, 0, {}, &SemiEffect},
    {W4D_OURS(Math_Sin), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &SinEffect},
    {W4D_OURS(Math_Cos), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &CosEffect},
    {W4D_OURS(Gte_RotTransPers), 3, {kAll, kAll, 0}, ah::Answer::kGarbage, 0, 0, {6}, &PersEffect},
    {W4D_OURS(Gte_StoreDepthF), 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &DepthEffect},
    {"Ftol_5B9550", 0x5B9550, 0x5B9550, 0, {}, ah::Answer::kThrough, 0, 0},
    // the group's own, called directly: area 186's start (its byte)
    {W4D_OURS(Area186_Start), 1, {kU8}, ah::Answer::kGarbage, 0, 0},
};
#undef W4D_OURS

// Beyond the field frame: the twenty effect records, the active member,
// script object and focus pointers, the facility's six bytes, the hook's arm
// byte, the vertex scratch, the screen floats, the packet pointer and the
// fuzz's buffer, the map focus and origin, Sprite_Kind2's z, the camera
// angles.
ah::Region g_regions[] = {
    {at::kEffectObjects, at::kEffectCount * at::kEffectStride},
    {at::kActiveMember, 4},
    {at::kScriptObject, 4},
    {at::kFocusObject, 4},
    {at::kFacility, at::kFacilityBytes},
    {at::kHookArmed, 1},
    {at::kVertex, 6},
    {at::kScreenXY, 8},
    {at::kPacketNext, 4},
    {0, kPacketBytes},   // g_packets, placed at SelfTest
    {at::kFocusZ, 4},
    {at::kOriginY - 2, 4},
    {at::kKind2Z, 4},
    {at::kCameraAngles, 6},
};

// Every round: the pointers the areas follow put back inside the regions.
void Common(int area, unsigned k) {
    g_area = area;
    g_k = k;
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
    ah::SetPointer(at::kFocusObject, FocusRecord(ah::Next()));
    SetPacketNext(ah::Next());
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h (area_harness.h: a group disturb never draws Next).
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 11) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 3 : v); break;
    case 1: move_script::SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? v % 3 : v); break;
    case 2: B(at::kCounter0) = v; break;
    case 3: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 16)); break;
    case 4: ah::SetPointer(at::kScriptObject, ScriptRecord(h >> 16)); break;
    case 5: ah::SetPointer(at::kFocusObject, FocusRecord(h >> 16)); break;
    case 6: B(at::kFacility + (h >> 12) % at::kFacilityBytes) = v; break;
    case 7: B(at::kHookArmed) = static_cast<unsigned char>(h & 0x100 ? v & 1 : v); break;
    case 8: {
        static const U kFocus[] = {0x4400, 0x6C00, 0x6200, 0x4E00};
        move_script::SetLong(ah::Mem(at::kFocusZ), static_cast<std::int32_t>(h & 0x100 ? kFocus[(h >> 12) % 4] : h >> 12));
        break;
    }
    case 9: Field_Request = static_cast<unsigned char>(h & 0x100 ? 2 : v); break;
    default: B(at::kChoiceAnswer) = static_cast<unsigned char>(v % 6); break;
    }
    Settle();
}

// A choice answer: each value a handler tests, its neighbours, a negative
// byte (read signed by choice 27 and area 187's), anything.
void SeedAnswer() {
    if (ah::Often()) B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 4, 5, 0, 1, 0xFF, 0x80, 0x81, 0x7F));
}
// Party_Zenny at 500 and beside, and far off.
void SeedZenny() {
    if (ah::Often()) move_script::SetLong(ah::Mem(at::kZenny), static_cast<std::int32_t>(AH_PICK(0x1F4, 0x1F3, 0x1F5, 0, 0xFFFFFFFFu, 0x800001F3u, 0x100001F3u)));
}
// A 16.16 word with the high word `high` and any low word.
U At16(U high, U low) { return (high & 0xFFFF) << 16 | (low & 0xFFFF); }
// A high word on or beside [lo, lo + 4): each inside, one either side, a high
// byte above it (the compares are 16-bit), anything.
U Around4(U lo) {
    switch (ah::Next() % 5) {
    case 0: case 1: return lo + ah::Next() % 4;
    case 2: return ah::Half() ? lo - 1 : lo + 4;
    case 3: return (lo + ah::Next() % 4) | 0x100u;
    default: return ah::Next();
    }
}

// ---- area 175 ----
void Seed175(unsigned k) {
    Common(175, k);
    switch (k) {
    case k175Choice27: case k175Message9F: SeedAnswer(); break;
    case k175Zenny: SeedAnswer(); SeedZenny(); break;
    case k175Clamp:
        if (ah::Often()) Field_Kind2X = static_cast<long>(AH_PICK(0x2E0000, 0x2E0001, 0x2DFFFF, 0, 0xFFFF0000u, 0x80000000u, 0x7FFFFFFF));
        break;
    case k175Slide:
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 1, 3, 0x82, 0));
        break;
    case k175GlideRun: Sprite_Current[4] = static_cast<unsigned char>(ah::Next() % at::kArea175GlideStateCount); break;
    case k175GlideStep:
        if (ah::Often()) Sprite_Current[0xA] = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 0x10, 0x11, 0x21, 0x80, 0xFF));
        break;
    case k175Script: {
        // the script [object +3] one of the fourteen, its position inside the
        // script half the time (any word one time in sixteen)
        unsigned char* const object = ah::Pointer(at::kScriptObject);
        object[3] = static_cast<unsigned char>(ah::Next() % at::kArea175ScriptCount);
        if (ah::Next() % 16 != 0) move_script::SetWord(object + 0xA, ah::Half() ? ah::Next() % 0x30 : ah::Next() % 0x100);
        break;
    }
    default: break;
    }
}
// An object trigger is called (a field object, 0x904030).
void ArgsTrigger(unsigned, U* a) {
    a[0] = Key(ah::Object(a[0]));
    a[1] = at::kStoryFlags;
}
void Args175(unsigned k, U* a) {
    if (k == k175Trigger) ArgsTrigger(k, a);
}

// ---- areas 176..184: the shared choices, each area's choice 27 ----
void SeedChoices(int area, unsigned k) {
    Common(area, k);
    SeedAnswer();
    if (area == 184 && k == k184Zenny) SeedZenny();
}
void Seed176(unsigned k) { SeedChoices(176, k); }
void Seed177(unsigned k) { SeedChoices(177, k); }
void Seed178(unsigned k) { SeedChoices(178, k); }
void Seed180(unsigned k) { SeedChoices(180, k); }
void Seed181(unsigned k) { SeedChoices(181, k); }
void Seed182(unsigned k) { SeedChoices(182, k); }
void Seed183(unsigned k) { SeedChoices(183, k); }
void Seed184(unsigned k) { SeedChoices(184, k); }

// ---- area 179: the init, the cell hook ----
void Seed179(unsigned k) {
    Common(179, k);
    switch (k) {
    case k179Choice27: case k179Var8: SeedAnswer(); break;
    case k179Init:
        if (ah::Often()) Cond_ByteFA = static_cast<signed char>(AH_PICK(7, 8, 7, 8, 0x7F, 0x80, 0xFF, 0, 6, 9));
        if (ah::Often()) Cond_ByteFD = static_cast<unsigned char>(AH_PICK(0, 0, 1, 0x80));
        break;
    case k179Cell:
        if (ah::Often()) B(at::kLeaderPose) = static_cast<unsigned char>(AH_PICK(1, 1, 0, 2, 0x81, 0x11));
        if (ah::Often()) Cond_ByteFA = static_cast<signed char>(AH_PICK(7, 8, 0x7F, 0x80, 0xFF));
        break;
    default: break;
    }
}
// The cell hook's (x, z) cell words: 0x3F and 0xE, one either side, a high
// half or byte above (the compares are 16-bit), anything.
void Args179(unsigned k, U* a) {
    if (k != k179Cell || !ah::Often()) return;
    a[0] = AH_PICK(0x3F, 0x3F, 0x3F, 0x3E, 0x40, 0x1003F, 0x13F, 0xFFFF003Fu);
    a[1] = AH_PICK(0xE, 0xE, 0xE, 0xD, 0xF, 0x1000E, 0x10E, 0xFFFF000Eu);
}

// ---- area 185: the facility choice, the step hook, the draw ----
void Seed185(unsigned k) {
    Common(185, k);
    switch (k) {
    case k185Choice27: case k185Facility: SeedAnswer(); break;
    case k185Step:
        if (ah::Often()) B(at::kHookArmed) = static_cast<unsigned char>(AH_PICK(0, 1, 1, 0x80, 2));
        break;
    case k185Draw:
        Sprite_Current = EffectRecord(ah::Next() % 4);
        break;
    default: break;
    }
}
void Args185(unsigned k, U* a) {
    if (k != k185Step || !ah::Often()) return;
    a[0] = At16(Around4(0x41), a[0]);
    a[1] = At16(Around4(0x14), a[1]);
}

// ---- area 186: tail kind 39, the init, the start ----
void Seed186(unsigned k) {
    Common(186, k);
    switch (k) {
    case k186Tail: {
        static const U kStates[] = {0, 1, 0xA, 0xB, 0, 1, 0xA, 0xB, 2, 5, 9, 0xC, 0xFF, 0x80, 0x7F};
        B(at::kTailState) = static_cast<unsigned char>(ah::Pick(kStates, sizeof kStates / sizeof kStates[0]));
        if (ah::Often()) move_script::SetLong(ah::Mem(at::kFocusZ), static_cast<std::int32_t>(AH_PICK(0x4400, 0x6C00, 0x4400, 0x6C00, 0x4401, 0x6BFF, 0x10004400, 0x6200)));
        if (ah::Often()) move_script::SetWord(ah::Mem(at::kTailTimer), AH_PICK(1, 1, 2, 0, 0x101, 0xFFFF));
        break;
    }
    case k186Init:
        if (ah::Often()) B(at::kKind2Mode) = static_cast<unsigned char>(AH_PICK(1, 2, 1, 2, 0, 3, 0x81, 0x82));
        break;
    default: break;
    }
}
void Args186(unsigned k, U* a) {
    if (k == k186Start && ah::Often()) a[0] = AH_PICK(0, 0xA, 0, 0xA, 1, 0xFF, 0x100, 0xA0A, 0x80);
}

// ---- area 187: the choice, trigger 14, tail kind 29 ----
void Seed187(unsigned k) {
    Common(187, k);
    switch (k) {
    case k187Choice: SeedAnswer(); break;
    case k187Tail:
        if (ah::Often()) B(at::kTailState) = static_cast<unsigned char>(AH_PICK(0, 1, 2, 0, 1, 2, 3, 0xFF, 0x80));
        if (ah::Often()) Field_Request = static_cast<unsigned char>(AH_PICK(2, 2, 0, 1, 3, 0x82));
        break;
    default: break;
    }
}
void Args187(unsigned k, U* a) {
    if (k == k187Trigger) ArgsTrigger(k, a);
}

void RunArea(int area, const ah::Clone* clones, unsigned n, const ah::DataTable* tables, unsigned n_tables, void (*seed)(unsigned),
             void (*args)(unsigned, U*), unsigned rounds) {
    ah::Group g{"area_w4d", clones, n, kCallees, sizeof kCallees / sizeof kCallees[0], tables, n_tables,
                g_regions, sizeof g_regions / sizeof g_regions[0], seed, &Disturb, rounds};
    g.settle = &Settle;
    g.args = args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    for (auto& r : g_regions)
        if (r.at == 0) r.at = Key(g_packets);
#define RUN(area, tables, n_tables, args) \
    RunArea(area, kClones##area, sizeof kClones##area / sizeof kClones##area[0], tables, n_tables, &Seed##area, args, kRounds)
    RUN(175, kTables175, 1, &Args175);
    RUN(176, nullptr, 0, nullptr);
    RUN(177, nullptr, 0, nullptr);
    RUN(178, nullptr, 0, nullptr);
    RUN(179, nullptr, 0, &Args179);
    RUN(180, nullptr, 0, nullptr);
    RUN(181, nullptr, 0, nullptr);
    RUN(182, nullptr, 0, nullptr);
    RUN(183, nullptr, 0, nullptr);
    RUN(184, nullptr, 0, nullptr);
    RUN(185, nullptr, 0, &Args185);
    RUN(186, nullptr, 0, &Args186);
    RUN(187, nullptr, 0, &Args187);
#undef RUN
}

}  // namespace area_w4d
