// BOF3X_SHADOW=area_w4c: world 4's areas 173 and 174 through the area round's
// shared harness (area_harness.h), once at start-up - four area_harness::Runs:
// area 173, area 174, area 175 (the six choices areas 175..185 share, whose
// bodies lie in the band) and area 198 (the two handlers it shares with area
// 174, read through the descriptor by Game_AreaNumber). docs/area_w4c.md
// section 3.
//
// The clone table is tools/area_rows.py --clones's rows for AREA173 and
// AREA174 (2026-09-28), each row read against the disassembly (every start,
// extent, call site, the tail's jump table and the stack table's two
// immediates agree; the "code immediate 0x460000" the tool notes in the arrive
// hook is a compare constant, not a pointer); the shapes are the root table
// each function hangs from (docs/area_w4c.md section 1). The group's own
// callee (Area174_SetPose) is a recorder here like any other callee, so each
// function is fuzzed alone.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4c.h"
#include "game/area_w4c_callees.h"
#include "game/move_script_bytes.h"
#include "hook/log.h"

namespace area_w4c {
namespace {

namespace ah = area_harness;
using U = std::uint32_t;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

#define AH_N(a) static_cast<int>(sizeof a / sizeof a[0])

// --- area 173 ---
constexpr ah::CallSite kCalls428450[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls4284E0[] = {{0x75, 0x4976D0}};
constexpr ah::CallSite kCalls4285F0[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls428660[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls4286D0[] = {{0x21, 0x531F90}, {0x62, 0x594E00}, {0x71, 0x57C0F0}, {0xD1, 0x57C110}};
constexpr ah::JumpTable kTables4286D0[] = {{0x1B, 0xE8, 6}};
constexpr ah::CallSite kCalls4287E0[] = {{0x3B, 0x57C7C0}};
constexpr ah::CallSite kCalls428840[] = {{0x7, 0x57C140}};
// --- area 174 ---
constexpr ah::CallSite kCalls428870[] = {{0x6, 0x454A80}, {0x17, 0x455290}, {0x28, 0x5891F0}};
constexpr ah::CallSite kCalls4288D0[] = {{0x2, 0x589810}};
constexpr ah::CallSite kCalls428960[] = {{0x1, 0x589810}};
constexpr ah::CallSite kCalls428AB0[] = {{0x40, 0x589200}};
constexpr ah::CallSite kCalls428B10[] = {{0x41, 0x589200}};
constexpr ah::CallSite kCalls428B80[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls428C10[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls428D50[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls428DE0[] = {{0x68, 0x428F50}};
constexpr ah::CallSite kCalls428E70[] = {{0x68, 0x428F50}};
constexpr ah::CallSite kCalls428F00[] = {{0x3D, 0x428F50}};
constexpr ah::CallSite kCalls428F50[] = {{0x29, 0x5891F0}};
constexpr ah::Imm kImms428F90[] = {{0xF, 0x428FC0}, {0x17, 0x429080}};
constexpr ah::CallSite kCalls4290E0[] = {{0xF, 0x454A80}};
constexpr ah::CallSite kCalls429100[] = {{0x16, 0x5366A0}};
constexpr ah::CallSite kCalls429120[] = {{0x0, 0x589810}};
constexpr ah::CallSite kCalls4291B0[] = {{0x0, 0x454810}};

#define W4C_CLONE(name, base, size, calls, n, imms, ni, tables, nt, ret, shape) \
    {#name, base, size, calls, n, imms, ni, tables, nt, reinterpret_cast<const void*>(&::name), ret, false, ah::Shape::shape}

// Every clone of the group, in one table; each Run takes a slice.
const ah::Clone kClones[] = {
    // area 173 (Run 1)
    W4C_CLONE(Area173_MessageByMemberA, 0x428450, 0x8B, kCalls428450, AH_N(kCalls428450), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area173_MessageByMemberB, 0x4284E0, 0x8B, kCalls4284E0, AH_N(kCalls4284E0), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area173_PlaceObject, 0x428570, 0x56, nullptr, 0, nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area173_ScriptOnIfMember2, 0x4285D0, 0x18, nullptr, 0, nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area173_Effect9DSub0, 0x4285F0, 0x67, kCalls4285F0, AH_N(kCalls4285F0), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area173_Effect9DSub1, 0x428660, 0x67, kCalls428660, AH_N(kCalls428660), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area173_Tail38, 0x4286D0, 0x10D, kCalls4286D0, AH_N(kCalls4286D0), nullptr, 0, kTables4286D0, AH_N(kTables4286D0), 0, kTail),
    W4C_CLONE(Area173_ArriveHook, 0x4287E0, 0x54, kCalls4287E0, AH_N(kCalls4287E0), nullptr, 0, nullptr, 0, 0xFF, kHook),
    W4C_CLONE(Area173_Init, 0x428840, 0x22, kCalls428840, AH_N(kCalls428840), nullptr, 0, nullptr, 0, 0, kInit),
    // area 174 (Run 2)
    W4C_CLONE(Area174_RestartSlotScript, 0x428870, 0x31, kCalls428870, AH_N(kCalls428870), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_WaitWhileRequest5, 0x4288B0, 0x15, nullptr, 0, nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_Effect9DSub0, 0x4288D0, 0x8E, kCalls4288D0, AH_N(kCalls4288D0), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_Effect9DSub1, 0x428960, 0x74, kCalls428960, AH_N(kCalls428960), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_EffectState3, 0x4289E0, 0x33, nullptr, 0, nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_SinkAndBrighten, 0x428A20, 0x8D, nullptr, 0, nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_ScriptAnimationAt, 0x428AB0, 0x5D, kCalls428AB0, AH_N(kCalls428AB0), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_ScriptAnimationOn2, 0x428B10, 0x6A, kCalls428B10, AH_N(kCalls428B10), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_EffectA2State0, 0x428B80, 0x8F, kCalls428B80, AH_N(kCalls428B80), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_EffectA1, 0x428C10, 0x81, kCalls428C10, AH_N(kCalls428C10), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_StepByScript, 0x428CA0, 0xA5, nullptr, 0, nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_EffectA2State2, 0x428D50, 0x8F, kCalls428D50, AH_N(kCalls428D50), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_TurnRightToScript, 0x428DE0, 0x8A, kCalls428DE0, AH_N(kCalls428DE0), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_TurnLeftToScript, 0x428E70, 0x8A, kCalls428E70, AH_N(kCalls428E70), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_FaceAwayFromLeader, 0x428F00, 0x50, kCalls428F00, AH_N(kCalls428F00), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_SetPose, 0x428F50, 0x3E, kCalls428F50, AH_N(kCalls428F50), nullptr, 0, nullptr, 0, 0, kCallee),
    W4C_CLONE(Area174_FadeRun, 0x428F90, 0x26, nullptr, 0, kImms428F90, AH_N(kImms428F90), nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_FadeTintUp, 0x428FC0, 0xBA, nullptr, 0, nullptr, 0, nullptr, 0, 0, kState),
    W4C_CLONE(Area174_FadeOut, 0x429080, 0x54, nullptr, 0, nullptr, 0, nullptr, 0, 0, kState),
    W4C_CLONE(Area174_ReleaseOnRequest5, 0x4290E0, 0x16, kCalls4290E0, AH_N(kCalls4290E0), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_LoadPalette, 0x429100, 0x1F, kCalls429100, AH_N(kCalls429100), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_EffectA3, 0x429120, 0x81, kCalls429120, AH_N(kCalls429120), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_WaitLoad, 0x4291B0, 0x15, kCalls4291B0, AH_N(kCalls4291B0), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_ChoiceByteE5, 0x4291D0, 0x1C, nullptr, 0, nullptr, 0, nullptr, 0, 0, kChoice),
    // areas 175..185's choices (Run 3)
    W4C_CLONE(Area175_ChoiceMessage62, 0x4291F0, 0x17, nullptr, 0, nullptr, 0, nullptr, 0, 0, kChoice),
    W4C_CLONE(Area175_ChoiceMessage70, 0x429210, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, 0, kChoice),
    W4C_CLONE(Area175_ChoiceMessage7E, 0x429230, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, 0, kChoice),
    W4C_CLONE(Area175_ChoiceMessage8A, 0x429250, 0x1A, nullptr, 0, nullptr, 0, nullptr, 0, 0, kChoice),
    W4C_CLONE(Area175_ChoiceStore3C, 0x429270, 0x14, nullptr, 0, nullptr, 0, nullptr, 0, 0, kChoice),
    W4C_CLONE(Area175_ChoiceByte3E, 0x429290, 0x2B, nullptr, 0, nullptr, 0, nullptr, 0, 0, kChoice),
    // area 198's handlers 1 and 2 (Run 4): the same two bodies again
    W4C_CLONE(Area174_ScriptAnimationAt, 0x428AB0, 0x5D, kCalls428AB0, AH_N(kCalls428AB0), nullptr, 0, nullptr, 0, 0, kHandler),
    W4C_CLONE(Area174_ScriptAnimationOn2, 0x428B10, 0x6A, kCalls428B10, AH_N(kCalls428B10), nullptr, 0, nullptr, 0, 0, kHandler),
};
// The index of each clone in kClones; a Run's seed gets its slice's index,
// so each Run adds its slice's first index (g_base).
enum : unsigned {
    kMsgA, kMsgB, kPlace, kOnIf2, kFx9D0, kFx9D1, kTail, kArrive, kInit,
    kRestart, kWait5, kFx9D0Keep, kFx9D1b, kFxState3, kSink, kAnimAt, kAnimOn2, kFxA2s0, kFxA1, kStep, kFxA2s2,
    kTurnRight, kTurnLeft, kFaceAway, kSetPose, kFadeRun, kFadeTint, kFadeOut, kRelease5, kPalette, kFxA3, kWaitLoad,
    kChoiceE5,
    kC62, kC70, kC7E, kC8A, kC3C, kC3E,
    kAnimAt198, kAnimOn2198,
    kCloneCount
};
static_assert(kCloneCount == sizeof kClones / sizeof kClones[0], "AR4C's seeding indices");
constexpr unsigned kFirst173 = kMsgA, kCount173 = kRestart - kMsgA;
constexpr unsigned kFirst174 = kRestart, kCount174 = kC62 - kRestart;
constexpr unsigned kFirst175 = kC62, kCount175 = kAnimAt198 - kC62;
constexpr unsigned kFirst198 = kAnimAt198, kCount198 = kCloneCount - kAnimAt198;
#undef W4C_CLONE
#undef AH_N

U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> U KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
unsigned char& B(U address) { return *ah::Mem(address); }

// The running object as the harness puts it: a party record or one of the
// first four field objects.
unsigned char* RunningRecord(U v) { return v & 1 ? ah::PartyOf(static_cast<unsigned char>(v >> 1)) : ah::Object((v >> 1) % 4); }
// The script object: a field object, a party record, or the running object
// itself (as the movement script often has it).
unsigned char* ScriptRecord(U v) {
    switch (v % 3) {
    case 0: return Sprite_Current;
    case 1: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    default: return ah::Object(v >> 2);
    }
}
// Field_ActiveMember inside the regions: a field object, one of the four
// extra objects (the party's, symbols.toml), or a party record.
unsigned char* MemberRecord(U v) {
    switch (v % 3) {
    case 0: return ah::Object(v >> 2);
    case 1: return ah::Mem(at::kSpriteObjectsExtra + ((v >> 2) % 4) * at::kObjectStride);
    default: return ah::PartyOf(static_cast<unsigned char>(v >> 2));
    }
}

// --- the effects: the callees that move what the caller reads after, and the
// answers the recorders cannot give (Noise only) ------------------------------

constexpr U kAll = 0xFFFFFFFFu, kU8 = 0xFFu, kU16 = 0xFFFFu;

// Louder than the real callees, each only half the time: the running object
// and the script object after a call its caller reads them again after.
U MovesCurrent(const U*, U answer) {
    const U n = ah::Noise();
    if (n & 1) Sprite_Current = RunningRecord(n >> 8);
    if (n & 2) ah::SetPointer(at::kScriptObject, ScriptRecord(n >> 12));
    return answer;
}
// Effect_FindFree: a slot of the 20 records or none (a third of the time);
// also moves the running object (the 0xA1 / 0xA2 / 0xA3 handlers read it
// after) and Field_ActiveMember (the 0x9D handlers read it after).
U EffectSlotEffect(const U* a, U answer) {
    MovesCurrent(a, answer);
    const U n = ah::Noise();
    if (n & 0x10000) ah::SetPointer(at::kActiveMember, MemberRecord(n >> 17));
    return (answer & 0xFFFFFF00u) | (n % 3 == 0 ? 0xFFu : (n >> 4) % at::kEffectCount);
}

#define W4C_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
const ah::Callee kCallees[] = {
    // the group's own, called directly (kGarbage with an effect: the harness's
    // kPhase recorder returns before the effect)
    {W4C_OURS(Area174_SetPose), 2, {kAll, kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    // engine callees nobody owns (raw)
    {"0x454A80", kSlotsReleaseFor, kSlotsReleaseFor, 1, {kAll}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {"0x455290", kSlotStart, kSlotStart, 2, {kAll, kAll}, ah::Answer::kByte, 0xFF, 0x07, {}, &MovesCurrent},
    // named, beyond the standard set
    {W4C_OURS(Effect_FindFree), 0, {}, ah::Answer::kGarbage, 0, 0, {}, &EffectSlotEffect},
    {W4C_OURS(ScriptFlags_Set40), 0, {}, ah::Answer::kGarbage, 0, 0},
    {W4C_OURS(File_LoadDone), 0, {}, ah::Answer::kFlag, 0, 0},
    // standard ones listed again: the caller reads the running object or the
    // script object after them
    {W4C_OURS(Sprite_SetAnimation), 1, {kU8}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
    {W4C_OURS(Sprite_SetAnimationAt), 2, {kU8, kU16}, ah::Answer::kGarbage, 0, 0, {}, &MovesCurrent},
};
#undef W4C_OURS

// Beyond the field frame: MoveScript_TintRecords .. Sprite_Kind2 .. the 20
// effect records (one block, 0x7E0700..0x7E1BE0), the script object pointer,
// Field_ActiveMember, area 175's two bytes.
const ah::Region kRegions[] = {
    {at::kTintRecords, at::kEffectObjects + at::kEffectCount * at::kEffectStride - at::kTintRecords},
    {at::kScriptObject, 4},
    {at::kActiveMember, 4},
    {at::kChoiceByte3C, 4},
};

// The slice the running Run seeds (its first index in kClones) and its area.
unsigned g_base;
unsigned g_scripts;   // the running area's script count (the descriptor's +0x10)

// Every round: the script object and Field_ActiveMember inside the regions.
void Common() {
    ah::SetPointer(at::kScriptObject, ScriptRecord(ah::Next()));
    ah::SetPointer(at::kActiveMember, MemberRecord(ah::Next()));
}

// The group's cells, moved by the harness's disturbance about one call in
// 24 - drawn only from h.
void Disturb(U h) {
    const auto v = static_cast<unsigned char>(h >> 20);
    switch ((h >> 8) % 6) {
    case 0: B(at::kTailState) = static_cast<unsigned char>(h & 0x100 ? v % 13 : v); break;
    case 1: B(at::kCounter3) = v; break;
    case 2: ah::SetPointer(at::kScriptObject, h & 0x10000 ? ah::Object(h >> 17) : ah::PartyOf(static_cast<unsigned char>(h >> 17))); break;
    case 3: ah::SetPointer(at::kActiveMember, MemberRecord(h >> 12)); break;
    case 4: SetWord(ah::Mem(at::kTailTimer), h & 0x100 ? 1 : v); break;
    default: Cond_ByteFD = static_cast<unsigned char>(h & 0x100 ? v % 3 : v); break;
    }
}

// A choice answer: 0, not 0, the sign edges, anything.
void SeedAnswer() { B(at::kChoiceAnswer) = static_cast<unsigned char>(AH_PICK(0, 0, 1, 2, 0xFF, 0x80, 0x7F, 0x10)); }

// The script object's +3 inside the running area's script table (past it the
// original reads a dword of what follows as a script pointer and reads through
// it), and its +0xA small half the time.
void SeedScript() {
    MoveScript_Object[3] = static_cast<unsigned char>(ah::Next() % g_scripts);
    SetWord(MoveScript_Object + 0xA, ah::Half() ? ah::Next() % 0x40 : ah::Next());
}
// The running script's bytes as the handlers read them (the image's).
const unsigned char* Script() {
    const unsigned char* const desc = ah::Descriptor(Game_AreaNumber);
    const U table = static_cast<U>(Long(desc + at::kDescScripts));
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(ah::Mem(table + MoveScript_Object[3] * 4u)))));
}

// Values the seeds hand to the args hook (the args hook cannot write memory).
U g_x, g_z, g_poses, g_direction;

void SeedTail() {
    const auto state = static_cast<unsigned char>(AH_PICK(0, 1, 1, 0xA, 0xA, 0xB, 0xB, 0xC, 0xC, 2, 5, 9, 0xD, 0x7F, 0x80, 0xFF, 0xF4));
    B(at::kTailState) = state;
    const bool on = ah::Often();
    switch (state) {
    case 1:
        B(at::kCounter3) = static_cast<unsigned char>(on ? 0x18 : AH_PICK(0x17, 0x19, 0x98, 0));
        Cond_ByteFD = static_cast<unsigned char>(AH_PICK(2, 2, 0, 1, 3, 0x82));
        break;
    case 0xA: B(at::kCounter3) = static_cast<unsigned char>(on ? 0x20 : AH_PICK(0x1F, 0x21, 0xA0, 0)); break;
    case 0xB: SetWord(ah::Mem(at::kTailTimer), on ? 1 : AH_PICK(0, 2, 0x101, 0x8001, 0xFFFF)); break;
    case 0xC: B(at::kCounter3) = static_cast<unsigned char>(on ? 0 : AH_PICK(1, 0x80, 0xFF, 0x24)); break;
    default: break;
    }
}

void SeedArrive() {
    const auto zone = static_cast<unsigned char>(AH_PICK(1, 1, 2, 2, 0, 3, 0x81, 0x82));
    Cond_ByteFD = zone;
    const U exact = zone == 2 ? 0x600000 : 0x460000;
    g_z = ah::Often() ? (ah::Often() ? exact : exact ^ 0x1C0000)
                      : exact + AH_PICK(1, 0xFFFFFFFFu, 0x10000, 0xFFFF0000u, 0x80000000u, 0x100);
    if (!ah::Often()) g_z = ah::Next();
    const U base = zone == 2 ? 0x30 : 0x11;
    const U high = ah::Often() ? base + AH_PICK(0, 1, 2, 0xFFFFFFFFu, 3, 4) : AH_PICK(0x1011, 0xFFFF, 0x8011, 0x3030, 0x11, 0x31);
    g_x = (high << 16) | (ah::Half() ? ah::Next() & 0xFFFF : 0);
}

void Seed(unsigned slice_k) {
    const unsigned k = g_base + slice_k;
    Common();
    unsigned char* const cur = Sprite_Current;
    switch (k) {
    case kMsgA: case kMsgB: {
        static const U kIds[] = {5, 8, 4, 2, 5, 8, 4, 2, 0, 1, 3, 6, 9, 0x85};
        for (unsigned m = 0; m < 3; ++m)
            if (ah::Often()) B(at::kParty89 + m * at::kPartyStride) = static_cast<unsigned char>(ah::Pick(kIds, sizeof kIds / sizeof kIds[0]));
        if (!ah::Often()) Field_MemberCount = static_cast<unsigned char>(AH_PICK(0, 1, 2, 3, 4, 5));
        break;
    }
    case kPlace: case kOnIf2: Field_State[0x89] = static_cast<unsigned char>(AH_PICK(2, 2, 1, 3, 0x82, 0)); break;
    case kFx9D0: case kFx9D1: case kFx9D0Keep: case kFx9D1b:
        // any pointer half the time (the division by 0xA4 of a difference of
        // either sign; it is not read through)
        if (ah::Half()) {
            const U d = ah::Half() ? 0u - (ah::Next() % 0x40000) : ah::Next() % 0x40000;
            SetLong(ah::Mem(at::kActiveMember), static_cast<std::int32_t>(at::kSpriteObjects + d));
        }
        break;
    case kTail: SeedTail(); break;
    case kArrive: SeedArrive(); break;
    case kWait5: case kRelease5: Field_Request = static_cast<unsigned char>(AH_PICK(5, 5, 4, 6, 0, 0x85)); break;
    case kFxState3: cur[0xB] = static_cast<unsigned char>(ah::Next() % at::kEffectCount); break;
    case kSink:
        SetWord(cur + 0x3E, ah::Often() ? AH_PICK(0xA14, 0xA14, 0xA13, 0xA15, 0xA00, 0x14) : ah::Next());
        for (unsigned i = 0x5D; i <= 0x5F; ++i)
            if (ah::Often()) cur[i] = static_cast<unsigned char>(AH_PICK(0xBF, 0xC0, 0x80, 0x7F, 0xBE, 0xC1, 0, 0xFF, 0x40));
        break;
    case kAnimAt: case kAnimAt198: SeedScript(); break;
    case kAnimOn2: case kAnimOn2198:
        SeedScript();
        SetWord(cur + 0x58, ah::Often() ? AH_PICK(2, 2, 1, 3, 0x102, 0) : ah::Next());
        break;
    case kStep: {
        // Field_State's script (+0x130) inside the area block; the byte it
        // reads beside the limit byte 0x903848
        const U offset = ah::Half() ? ah::Next() % 0x800 : ah::Next() & 0xFFFF;
        SetWord(MoveScript_Object + 0xA, offset);
        const U script = 0x8CB580 + ah::Next() % 0x400;
        SetLong(Field_State + 0x130, static_cast<std::int32_t>(script));
        if (script + offset + 2 < 0x8CB580 + 0x2000) {
            const auto b = static_cast<unsigned char>(ah::Next());
            B(script + offset + 2) = b;
            B(at::kStepLimit) = static_cast<unsigned char>(b + AH_PICK(0xFF, 0, 1, 0x80, 0x7F));
        }
        break;
    }
    case kTurnRight: case kTurnLeft: {
        SeedScript();
        const unsigned char* const s = Script();
        const unsigned char want = s[Word(MoveScript_Object + 0xA) + 2];
        cur[8] = static_cast<unsigned char>(ah::Often() ? want + AH_PICK(0, 0, 1, 0xFF, 7, 8) : ah::Next());
        break;
    }
    case kFaceAway: SeedScript(); break;
    case kSetPose: {
        static const U kPoses[] = {0x641700, 0x641710, 0x641720};
        g_poses = ah::Often() ? ah::Pick(kPoses, 3) : 0x8CB580 + ah::Next() % 0x1E00;
        g_direction = ah::Half() ? ah::Next() % 8 : ah::Next();
        break;
    }
    case kFadeRun: cur[4] = static_cast<unsigned char>(ah::Next() % 2); break;
    case kFadeTint: {
        unsigned char* const member = Field_ActiveMember;
        member[0x9F] = static_cast<unsigned char>(ah::Half() ? ah::Next() % 32 : ah::Next());
        unsigned char* const tint = ah::Mem(at::kTintRecords + member[0x9F] * at::kTintStride);
        if (ah::Often()) tint[2] = static_cast<unsigned char>(AH_PICK(0x1C, 0x1D, 0x1E, 0x1F, 0x7F, 0x80, 0xFF, 0));
        break;
    }
    case kFadeOut: cur[0x5D] = static_cast<unsigned char>(ah::Often() ? AH_PICK(0x80, 0x84, 0x7C, 0x81, 0) : ah::Next()); break;
    case kChoiceE5: case kC62: case kC70: case kC7E: case kC8A: case kC3C: case kC3E: SeedAnswer(); break;
    default: break;
    }
}

void Args(unsigned slice_k, U* a) {
    switch (g_base + slice_k) {
    case kArrive:
        a[0] = g_x;
        a[1] = g_z;
        break;
    case kSetPose:
        a[0] = g_poses;
        a[1] = g_direction;
        break;
    default: break;
    }
}

void RunSlice(unsigned first, unsigned count, int area, unsigned scripts, unsigned rounds) {
    g_base = first;
    g_scripts = scripts;
    ah::Group g{"area_w4c", kClones + first, count, kCallees, sizeof kCallees / sizeof kCallees[0], nullptr, 0,
                kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, rounds};
    g.args = &Args;
    g.area = area;
    ah::Run(g);
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 6000;
    RunSlice(kFirst173, kCount173, 173, 1, kRounds);
    RunSlice(kFirst174, kCount174, 174, at::kArea174ScriptCount, kRounds);
    RunSlice(kFirst175, kCount175, 175, 1, kRounds);
    RunSlice(kFirst198, kCount198, 198, at::kArea198ScriptCount, kRounds);
}

}  // namespace area_w4c
