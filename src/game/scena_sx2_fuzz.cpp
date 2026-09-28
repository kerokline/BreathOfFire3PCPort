// BOF3X_SHADOW=scena_sx2: group SX2's thirteen functions through the
// scenario harness (scenario_harness.h), once at start-up. docs/scena_sx2.md
// section 4.
//
// The clone table is magic_rows.descend / clone_sites over the thirteen
// (2026-09-28, capstone, every jump internal, no jump table, nothing
// refused), names given; each extent is the function's last ret, read
// whole. Each function's call shape (none is a vtable slot, a hook or a state
// handler; every one is a direct cdecl callee):
//
//   Effect_HoldFlag1C        1 word (a byte used)
//   Party_PlaceInFormation   3 words: x, z, the slot (a byte)
//   AreaMap_SetHeight        3 words: x, z (s16s), the value (a byte)
//   Flags_Toggle             2 words: the bits, the index (a byte)
//   Camera_TurnStep          2 words: the angle (a word), the step (s8); answers in al
//   Camera_TurnFBToDegrees   2 words: the angle (s16), the step (s8); answers in al
//   Camera_TurnStepFB        as Camera_TurnStep
//   Member_SetState2_8       2 words, a byte of each
//   Sound_StopChannels       none
//   Sound_SetCueVolume       2 words: the cue (a word), the level (whole)
//   KeyItem_Remove           1 word (a byte used); answers in al
//   AbilityList_ForType      3 words, a byte of each; answers a pointer in eax
//   Gpu_SetSprt16            1 word: the primitive
//
// None reads the chapter bytes or the flag row, so the chapter is 0.
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scena_sx2.h"
#include "game/scena_sx2_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace scena_sx2 {
namespace {

namespace sh = scenario_harness;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// magic_rows.descend / clone_sites over the thirteen, 2026-09-28, names given.
constexpr sh::CallSite kCalls469FE0[] = {{0x0, 0x589810}, {0x33, 0x57C0F0}};
constexpr sh::CallSite kCalls532FD0[] = {{0x12B, 0x572570}};
constexpr sh::CallSite kCalls57C600[] = {{0x3E, 0x57C650}};
constexpr sh::CallSite kCalls587860[] = {{0xD, 0x5A6C30}};
constexpr sh::CallSite kCalls587890[] = {{0x50, 0x5A6C60}};
#define SH_N(a) static_cast<int>(sizeof a / sizeof a[0])
#define SX2_FN(name) reinterpret_cast<const void*>(&::name)
constexpr sh::Shape kE = sh::Shape::kEntry;
const sh::Clone kClones[] = {
    {"Effect_HoldFlag1C", 0x469FE0, 0x3C, kCalls469FE0, SH_N(kCalls469FE0), nullptr, 0, nullptr, 0, SX2_FN(Effect_HoldFlag1C), 0, false, kE},
    {"Party_PlaceInFormation", 0x532FD0, 0x140, kCalls532FD0, SH_N(kCalls532FD0), nullptr, 0, nullptr, 0, SX2_FN(Party_PlaceInFormation), 0, false, kE},
    {"AreaMap_SetHeight", 0x572620, 0x2F, nullptr, 0, nullptr, 0, nullptr, 0, SX2_FN(AreaMap_SetHeight), 0, false, kE},
    {"Flags_Toggle", 0x57C160, 0x1F, nullptr, 0, nullptr, 0, nullptr, 0, SX2_FN(Flags_Toggle), 0, false, kE},
    {"Camera_TurnStep", 0x57C5A0, 0x5E, nullptr, 0, nullptr, 0, nullptr, 0, SX2_FN(Camera_TurnStep), 0xFF, false, kE},
    {"Camera_TurnFBToDegrees", 0x57C600, 0x47, kCalls57C600, SH_N(kCalls57C600), nullptr, 0, nullptr, 0, SX2_FN(Camera_TurnFBToDegrees), 0xFF, false, kE},
    {"Camera_TurnStepFB", 0x57C650, 0x5E, nullptr, 0, nullptr, 0, nullptr, 0, SX2_FN(Camera_TurnStepFB), 0xFF, false, kE},
    {"Member_SetState2_8", 0x57C8A0, 0x35, nullptr, 0, nullptr, 0, nullptr, 0, SX2_FN(Member_SetState2_8), 0, false, kE},
    {"Sound_StopChannels", 0x587860, 0x28, kCalls587860, SH_N(kCalls587860), nullptr, 0, nullptr, 0, SX2_FN(Sound_StopChannels), 0, false, kE},
    {"Sound_SetCueVolume", 0x587890, 0x63, kCalls587890, SH_N(kCalls587890), nullptr, 0, nullptr, 0, SX2_FN(Sound_SetCueVolume), 0, false, kE},
    {"KeyItem_Remove", 0x591920, 0x20, nullptr, 0, nullptr, 0, nullptr, 0, SX2_FN(KeyItem_Remove), 0xFF, false, kE},
    {"AbilityList_ForType", 0x591EC0, 0x64, nullptr, 0, nullptr, 0, nullptr, 0, SX2_FN(AbilityList_ForType), 0xFFFFFFFFu, false, kE},
    {"Gpu_SetSprt16", 0x5A7730, 0x10, nullptr, 0, nullptr, 0, nullptr, 0, SX2_FN(Gpu_SetSprt16), 0, false, kE},
};
#undef SX2_FN
#undef SH_N

enum : unsigned {
    kHold, kPlace, kHeight, kToggle, kTurn, kTurnFBDeg, kTurnFB, kMember, kStopAll, kVolume, kKeyRemove, kListFor,
    kSprt16, kCount
};
static_assert(kCount == sizeof kClones / sizeof kClones[0], "one enum entry a clone, in order");

std::uint32_t Key(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
template <typename F> std::uint32_t KeyOf(F f) { return Key(reinterpret_cast<const void*>(f)); }
#define SX2_OURS(name) #name, ::bof3::addr::name, KeyOf(&::name)
constexpr sh::Answer kG = sh::Answer::kGarbage;
constexpr std::uint32_t kAll = 0xFFFFFFFFu;

unsigned char* Mem(std::uint32_t a) { return sh::Mem(a); }

// SH_PICK over values computed each call (SH_PICK's list is a static, fixed
// at its first use).
template <typename... T> std::uint32_t PickOf(T... v) {
    const std::uint32_t values[] = {static_cast<std::uint32_t>(v)...};
    return sh::Pick(values, sizeof...(v));
}

// The named cells whose symbols.gen.h names are macros (no bof3::addr entry to qualify).
constexpr std::uint32_t kSoundChannels = 0x6BC8C8;   // Sound_Channels
constexpr std::uint32_t kSoundBanks = 0x6BC928;      // Sound_Banks
constexpr std::uint32_t kAreaHeader = 0x8CB580;      // AreaMap_Header
constexpr std::uint32_t kHeightBase = 0x8CB5AA;      // AreaMap_HeightBase

// Gpu_SetSprt16's primitive: a buffer of the fuzz's own.
alignas(16) unsigned char g_prim[0x20];

// The height layer the fuzz keeps its writes inside (AreaMap_Header on).
constexpr std::uint32_t kLayerSize = 0x4000;

// The region the seed plants Sound_Banks' six records in.
constexpr std::uint32_t kBanksSize = 6 * at::kBankStride;

// The formation bytes whose first offset is 0 and those whose is not, found
// in the image's BattleFormation_Offsets at the first seed (both branches of
// Party_PlaceInFormation from the seed).
unsigned char g_zero[256], g_other[256];
unsigned g_n_zero, g_n_other;

// --- the moves --------------------------------------------------------------------
//
// What the functions read again after a call and a caller could have moved:
// Sprite_Current (Party_PlaceInFormation after MapView_GroundAt), a later
// Sound_Channels dword (Sound_StopChannels' walk), a cue's voice dwords
// (Sound_SetCueVolume's four), the formation byte. The harness's disturbance
// reaches a group cell about one call in 24, so the group's callees also move
// one after each call (Stir, from the recorders' stream).
void Move(std::uint32_t h) {
    const unsigned v = (h >> 8) & 0xFF;
    const std::uint32_t w = h >> 16;
    switch (h % 6) {
    case 0: Sprite_Current = sh::SpriteRecord(v); break;
    case 1: Sound_Channels[v % 23] = (w & 1) ? 0 : w; break;
    case 2: {
        // a voice dword of cue 0..7 of bank 1..6
        unsigned char* const cue = Mem(kSoundBanks + (v % 6) * at::kBankStride + ((w >> 4) % 8) * 16 + 4 * (w & 3));
        SetLong(cue, static_cast<std::int32_t>((w & 0x100) ? 0 : (w >> 9) % 64));
        break;
    }
    case 3: Mem(at::kFormation)[0] = static_cast<unsigned char>(v); break;
    default: break;
    }
}

std::uint32_t Stir(const std::uint32_t*, std::uint32_t answer) {
    Move(sh::Noise());
    return answer;
}

const sh::Callee kCallees[] = {
    // ours, called by name
    {SX2_OURS(MapView_GroundAt), 2, {kAll, kAll}, kG, 0, 0, {}, &Stir},
    {SX2_OURS(SndBuf_Stop), 1, {kAll}, kG, 0, 0, {}, &Stir},
    {SX2_OURS(Camera_TurnStepFB), 2, {kAll, kAll}, sh::Answer::kFlag, 0, 0},
    // nobody's, by address
    {"0x5A6C60", at::kSndBufVolume, at::kSndBufVolume, 2, {kAll, kAll}, kG, 0, 0, {}, &Stir},
};
#undef SX2_OURS

// --- the state -----------------------------------------------------------------------

// Beyond the harness's 22 (which hold Cond_Flags with the story flags
// 0x904030 and 0x904060; Field_MemberCount, Camera_Angles, Cond_AngleFB;
// Sprite_Current; ObjTrio; Sprite_Objects; Effect_Objects).
const sh::Region kRegions[] = {
    {bof3::addr::CharacterRecords, 8 * at::kRecordStride},   // AbilityList_ForType answers into it
    {at::kKeyItems, 0x20},
    {0x904AA0, 0xB0},                   // the battle bytes: the formation 0x904AAC
    {at::kLightAngle0, 8},
    {at::kLightCopy0, 8},
    {kAreaHeader, kLayerSize},
    {kSoundChannels, at::kChannelsEnd - kSoundChannels},
    {kSoundBanks, kBanksSize},
    {Key(g_prim), sizeof g_prim},
};

void Disturb(std::uint32_t h) { Move(h); }

// --- the seed ------------------------------------------------------------------------

void Formations() {
    if (g_n_zero + g_n_other != 0) return;
    const unsigned char* const pairs = move_script::At(bof3::addr::BattleFormation_Offsets);
    for (unsigned f = 0; f < 256; ++f) {
        if (pairs[f * 2] == 0)
            g_zero[g_n_zero++] = static_cast<unsigned char>(f);
        else
            g_other[g_n_other++] = static_cast<unsigned char>(f);
    }
}

// Inputs chosen by the seed for Args (the state is captured before the
// arguments, so what the arguments must match is planted here).
std::uint32_t g_a[3];

void Seed(unsigned k) {
    switch (k) {
    case kPlace: {
        Formations();
        Field_MemberCount = static_cast<unsigned char>(PickOf(0, 1, 2, 2, 3, 3, 4, sh::Next()));
        Mem(at::kLeaderSlot)[0] = static_cast<unsigned char>(PickOf(0, 1, 2, 3, 0xFF, 0xFC, sh::Next() % 8));
        const bool zero = g_n_zero != 0 && (g_n_other == 0 || sh::Half());
        Mem(at::kFormation)[0] = zero ? g_zero[sh::Next() % g_n_zero] : g_other[sh::Next() % g_n_other];
        g_a[0] = PickOf(0, sh::Next(), sh::Next() % 0x800000, 0x7FFFFFFF);
        g_a[1] = PickOf(0, sh::Next(), sh::Next() % 0x800000, 0x80000000u);
        break;
    }
    case kHeight: {
        // the write kept inside the layer region: width * z + base * 4 + x in 0..kLayerSize
        const unsigned width = sh::Next() % 0x41;
        const unsigned base = sh::Next() % 0x300;
        AreaMap_Header[0] = static_cast<unsigned char>(width);
        SetWord(Mem(kHeightBase), base);
        // z an s16 either side of 0 (a negative row still inside the layer when the base is past it)
        int z = static_cast<int>(sh::Next() % 0x61) - 0x20;
        if (z < 0 && static_cast<int>(width) * -z > static_cast<int>(base) * 4) z = -z;
        const int lo = -(static_cast<int>(width) * z + static_cast<int>(base) * 4);
        const int hi = static_cast<int>(kLayerSize) - 1 + lo;
        int x = PickOf(0, 1, sh::Next() % 0x100);
        if (sh::Next() % 4 == 0) x = sh::Half() ? lo : lo + static_cast<int>(sh::Next() % 0x100);
        if (x > hi) x = hi;
        g_a[0] = static_cast<std::uint32_t>(x);
        g_a[1] = static_cast<std::uint32_t>(z);
        break;
    }
    case kTurn:
    case kTurnFB: {
        // the angle word near the target, a step either way
        const std::uint32_t cell = k == kTurn ? at::kAngle0 : at::kAngleFB;
        const std::uint32_t target = PickOf(sh::Next() % 0x1000, sh::Next() % 0x1000, 0, 0xFFF, sh::Next());
        const int step = static_cast<int>(PickOf(0, 1, 2, 22, 0x7F, 0x80, 0xFF, 0xFE, 0xEA, sh::Next()) & 0xFF);
        const int s = static_cast<signed char>(step);
        const int near = static_cast<int>(target & 0xFFF) - s * static_cast<int>(PickOf(0, 1, 2, sh::Next() % 5)) +
                         static_cast<int>(PickOf(0, 0, 1, 0xFFFFFFFFu, sh::Next() % 9 - 4));
        const std::uint32_t upper = sh::Half() ? 0 : (sh::Next() & 0xF000);
        SetWord(Mem(cell), (static_cast<std::uint32_t>(near) & 0xFFF) | upper);
        g_a[0] = target;
        g_a[1] = static_cast<std::uint32_t>(step);
        break;
    }
    case kStopAll:
        for (unsigned i = 0; i < 23; ++i)
            if (sh::Half()) Sound_Channels[i] = 0;
        break;
    case kVolume: {
        // cues 0..23 of banks 1..6: each voice dword 0 or a voice 0..63 (now and then any byte, past
        // the bank's 64 voices; garbage above now and then)
        for (unsigned b = 0; b < 6; ++b)
            for (unsigned c = 0; c < 24; ++c)
                for (unsigned v = 0; v < 4; ++v) {
                    unsigned char* const d = Mem(kSoundBanks + b * at::kBankStride + c * 16 + 4 * v);
                    const std::uint32_t voice = (sh::Next() % 8 == 0 ? sh::Next() % 0x100 : sh::Next() % 64) |
                                                (sh::Next() % 8 == 0 ? sh::Next() & 0xFFFFFF00u : 0);
                    SetLong(d, static_cast<std::int32_t>(sh::Half() ? 0 : voice));
                }
        break;
    }
    case kKeyRemove: {
        // the item at a slot (no earlier slot holds it) two times in three
        unsigned char* const list = Mem(at::kKeyItems);
        auto item = static_cast<unsigned char>(PickOf(0, 0xB, 1 + sh::Next() % 0xFF, 1 + sh::Next() % 0xFF));
        if (sh::Often()) {
            const unsigned slot = PickOf(0, 0x1F, sh::Next() % 0x20);
            for (unsigned i = 0; i < slot; ++i)
                if (list[i] == item) list[i] = static_cast<unsigned char>(item ^ 0x80);
            list[slot] = item;
        } else {
            for (unsigned i = 0; i < 0x20; ++i)
                if (list[i] == item) list[i] = static_cast<unsigned char>(item ^ 0x40);
        }
        g_a[0] = item;
        break;
    }
    default: break;
    }
}

// the arguments each function reads (garbage above the bytes it masks)
void Args(unsigned k, std::uint32_t* a) {
    const std::uint32_t hi = a[9] & 0xFFFFFF00u;
    switch (k) {
    case kHold: break;   // the frames any byte
    case kPlace:
        a[0] = g_a[0];
        a[1] = g_a[1];
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Often() ? sh::Next() % 3 : sh::Next() % 5);
        break;
    case kHeight:
        a[0] = (a[0] & 0xFFFF0000u) | (g_a[0] & 0xFFFF);
        a[1] = (a[1] & 0xFFFF0000u) | (g_a[1] & 0xFFFF);
        break;
    case kToggle:
        a[0] = sh::Half() ? at::kStoryFlags : sh::at::kCondFlags + 8 * (sh::Next() % 29);
        break;
    case kTurn:
    case kTurnFB:
        a[0] = sh::Often() ? g_a[0] : (a[0] & 0xFFFF0000u) | (g_a[0] & 0xFFFF);
        a[1] = hi | g_a[1];
        break;
    case kTurnFBDeg:
        a[0] = (a[0] & 0xFFFF0000u) | (PickOf(sh::Next() % 721 - 360, sh::Next() % 1441 - 720, 0, 90, 0xFFD8, sh::Next()) & 0xFFFF);
        a[1] = hi | (PickOf(0, 2, 0xFE, 11, 12, 0xF5, 0x7F, 0x80, sh::Next()) & 0xFF);
        break;
    case kMember: a[0] = hi | (sh::Next() % 3); break;
    case kVolume:
        a[0] = (a[0] & 0xFFFF0000u) | (static_cast<std::uint32_t>(PickOf(1 + sh::Next() % 6, 1 + sh::Next() % 6, 1 + sh::Next() % 6, 0, 7)) << 8) |
               (sh::Often() ? sh::Next() % 24 : sh::Next() % 0x100) | (sh::Next() & 0xF000);
        break;
    case kKeyRemove: a[0] = hi | g_a[0]; break;
    case kListFor:
        a[2] = (a[2] & 0xFFFFFF00u) | (sh::Half() ? 0 : 1 + sh::Next() % 0xFF);
        break;
    case kSprt16: a[0] = Key(g_prim); break;
    default: break;
    }
}

}  // namespace

void SelfTest() {
    sh::Group group = {
        "scena_sx2", kClones, sizeof kClones / sizeof kClones[0], kCallees, sizeof kCallees / sizeof kCallees[0],
        nullptr, 0, kRegions, sizeof kRegions / sizeof kRegions[0], &Seed, &Disturb, 3000, nullptr, 0, &Args,
    };
    group.chapter = 0;   // none of the thirteen reads the chapter bytes or the flag row
    sh::Run(group);
}

}  // namespace scena_sx2
