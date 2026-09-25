// BOF3X_SHADOW=shop_states: a differential fuzz of the shop overlay's first
// table, once at start-up. docs/shop_states.md section 4.
//
// Twenty-five byte-copies, every call out re-aimed at a recording stand-in;
// the five .data dispatch tables' 38 entries (ShopMode_States' 11, then
// Inn_Steps .. FieldSave_States' 27, one block) swapped for recorders and put
// back. One round: one function, random bytes over every region any of them
// touches - the state block 0x929EC0..0x929F0F, the eight character records,
// the sprite objects and Sprite_Current, the inn's bytes, the zenny, the pad
// and its button maps, the save cursor - then the object index and the
// dispatch indices put back inside their arrays and that function's branch
// boundaries seeded; theirs, then from the same state ours; the regions and
// the stand-ins' log compared.
//
// The stand-ins are loud: each logs its arguments with a hash of the watched
// cells as they were at the call (so a store moved across a call shows), and
// two calls in three move one of the cells some caller reads again after it.
// Where the original passes a register whose upper bits it never set (the
// title box's colour, the slots' and the choices' x), the log keeps what the
// callee uses: the low byte of the colour, the low word of x and y.
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/shop_states_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace shop_states {
namespace {

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
unsigned char& B(U address) { return *At(address); }
U Long(U address) {
    U v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
void PutLong(U address, U v) { std::memcpy(At(address), &v, sizeof v); }
void PutWord(U address, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

// --- the random source and the stand-ins' log ------------------------------

std::uint32_t g_rng = 0x5A17C3E9u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }

constexpr unsigned kLog = 24;
struct Entry { U what, a, b, c, d, watch; };
Entry g_log[kLog];
unsigned g_log_n;
std::uint32_t g_seed;   // the stand-ins' own stream: the same on both passes

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}

// The cells a store or a re-read could be moved across a call on.
const U kWatched[] = {
    at::kState, at::kStep, at::kSub, at::kCount, at::kAnswer, at::kObject, at::kSavedObject, at::kClear0A,
    at::kShopNumber, at::kChoice, at::kCancelled, at::kSaveBack, at::kInnFlag, at::kColour, at::kMsgFlags,
    at::kFade, at::kPressed, at::kPressed + 1, at::kConfirm, at::kConfirm + 1, at::kCancel, at::kCancel + 1,
    at::kZenny, at::kZenny + 1, at::kSlot, at::kSlotTop, at::kRepeatLatch, at::kRepeatTimer, at::kGameStep,
    at::kScriptFlags + 1,
};
constexpr unsigned kNWatched = sizeof kWatched / sizeof kWatched[0];
std::uint32_t Watch() {
    std::uint32_t h = 0x811C9DC5u;
    for (U a : kWatched) h = (h ^ B(a)) * 0x01000193u;
    for (U a : {at::kSpriteCurrent, at::kZenny, at::kSlot, at::kSlotTop}) h = (h ^ Long(a)) * 0x01000193u;
    return h;
}
void Record(U what, U a = 0, U b = 0, U c = 0, U d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, Watch()};
    ++g_log_n;
}

// Two calls in three: one watched cell moved; once in 17, Sprite_Current
// repointed at another object.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) return;
    if (h % 17 == 0) {
        PutLong(at::kSpriteCurrent, at::kSpriteObjects + ((h >> 8) % at::kSpriteCount) * at::kSpriteStride);
        return;
    }
    const U where = kWatched[(h >> 4) % kNWatched];
    B(where) = static_cast<unsigned char>(h >> 12);
}

// --- the stand-ins ---------------------------------------------------------

U Low16(int v) { return static_cast<std::uint16_t>(v); }

void __cdecl StubTitleBox(int x, int y, int w, int h, int colour) {
    Record(1, Low16(x) | Low16(y) << 16, static_cast<U>(w), static_cast<U>(h), static_cast<U>(colour) & 0xFF);
    Disturb();
}
void __cdecl StubMoneyBox(int x, int y, int unused, unsigned value) {
    Record(2, static_cast<U>(x), static_cast<U>(y), static_cast<U>(unused), value);
    Disturb();
}
const unsigned char* __cdecl StubDrawText(int x, int y, int colour, int count, const unsigned char* text) {
    Record(3, static_cast<U>(x) | static_cast<U>(y) << 16, static_cast<U>(colour), static_cast<U>(count), Addr(text));
    Disturb();
    return text + 1;
}
// A pointer that names the id, never dereferenced by the stand-ins.
const unsigned char* __cdecl StubSystemPtr(unsigned id) {
    Record(4, id);
    Disturb();
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(0x5E0000u + (id & 0xFFFF) * 4u));
}
void __cdecl StubOpenSystem(unsigned id) {
    Record(5, id);
    Disturb();
}
void __cdecl StubPlay(unsigned short id) {
    Record(6, id);
    Disturb();
}
void __cdecl StubStream(unsigned id) {
    Record(7, id);
    Disturb();
}
int __cdecl StubStreamDone() {
    const std::uint32_t h = Hash();
    const int r = h % 3 == 0 ? 0 : static_cast<int>(h % 5 == 0 ? 0x100u : h >> 20);
    Record(8, static_cast<U>(r));
    Disturb();
    return r;
}
void __cdecl StubSlots(int x, int y, unsigned highlight) {
    Record(9, Low16(x) | Low16(y) << 16, highlight);
    Disturb();
}
void __cdecl StubReadSummaries() {
    Record(10);
    Disturb();
}
// Input_AutoRepeat: an answer with up, down, both or neither, and noise.
unsigned __cdecl StubAutoRepeat(unsigned pressed) {
    const std::uint32_t h = Hash();
    static const unsigned kBits[] = {0, 0x1000, 0x4000, 0x5000, 0x1000, 0x4000};
    const unsigned r = kBits[h % 6] | ((h >> 8) & 0xAFFFu) | ((h >> 3) & 0xFFFF0000u);
    Record(11, pressed, r);
    Disturb();
    return r;
}
// Menu_YesNo: an answer in al half the time.
unsigned char __cdecl StubYesNo() {
    const std::uint32_t h = Hash();
    const auto r = static_cast<unsigned char>(h % 2 == 0 ? 0 : (h % 7 == 0 ? 0x80 : 1 + (h >> 9) % 2));
    Record(12, r);
    Disturb();
    return r;
}
void __cdecl StubTransition(unsigned char kind) {
    Record(13, kind);
    Disturb();
}
void __cdecl StubFace(unsigned char direction) {
    Record(14, direction);
    Disturb();
}
void __cdecl StubResetWindows() {
    Record(15);
    Disturb();
}
// Crt_sprintf: the value logged and a mark left in the buffer.
int __cdecl StubSprintf(char* dst, const char* fmt, int value) {
    Record(16, Addr(dst), Addr(fmt), static_cast<U>(value));
    dst[0] = static_cast<char>(value);
    dst[1] = 0;
    Disturb();
    return 1;
}
void __cdecl StubTextRecord(unsigned slot, unsigned length, const unsigned char* text) {
    Record(17, slot, length, Addr(text));
    Disturb();
}
void __cdecl StubChoices(int x, int y) {
    Record(18, Low16(x), Low16(y));
    Disturb();
}
void __cdecl StubBlack() {
    Record(19);
    Disturb();
}
void __cdecl StubRestore(unsigned full) {
    Record(20, full & 0xFF);
    Disturb();
}

const Callees kStubs = {
    StubTitleBox, StubMoneyBox, StubDrawText, StubSystemPtr, StubOpenSystem,
    StubPlay, StubStream, StubStreamDone,
    StubSlots, StubReadSummaries, StubAutoRepeat, StubYesNo,
    StubTransition, StubFace, StubResetWindows,
    reinterpret_cast<int (__cdecl*)(char*, const char*, ...)>(StubSprintf), StubTextRecord, StubChoices, StubBlack,
    StubRestore,
};

// The dispatch tables' recorders: 11 for ShopMode_States, then the 27 of the
// inn's four tables, which lie end to end.
template <unsigned N> void __cdecl StubTarget() {
    Record(0x100 + N);
    Disturb();
}
using Target = void (__cdecl*)();
template <unsigned... N> struct Targets {
    static constexpr Target kAll[sizeof...(N)] = {&StubTarget<N>...};
};
constexpr unsigned kModeEntries = 11;
constexpr unsigned kInnEntries = (at::kTablesEnd - at::kInnSteps) / 4;   // 27
const Target* TargetTable() {
    return Targets<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
                   27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37>::kAll;
}

// --- the copies ------------------------------------------------------------

const void* StubFor(U target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case bof3::addr::Menu_DrawTitleBox:    return f(&StubTitleBox);
    case bof3::addr::Menu_DrawMoneyBox:    return f(&StubMoneyBox);
    case bof3::addr::Text_DrawAt:          return f(&StubDrawText);
    case bof3::addr::Msg_SystemPtr:        return f(&StubSystemPtr);
    case bof3::addr::Msg_OpenSystem:       return f(&StubOpenSystem);
    case bof3::addr::Sound_PlayEffect:     return f(&StubPlay);
    case bof3::addr::Sound_LoadStream:     return f(&StubStream);
    case bof3::addr::Sound_StreamDone:     return f(&StubStreamDone);
    case bof3::addr::SaveMenu_DrawSlots:   return f(&StubSlots);
    case bof3::addr::Save_ReadSummaries:   return f(&StubReadSummaries);
    case bof3::addr::Input_AutoRepeat:     return f(&StubAutoRepeat);
    case bof3::addr::Menu_YesNo:           return f(&StubYesNo);
    case bof3::addr::Transition_Start:     return f(&StubTransition);
    case bof3::addr::Sprite_FaceDirection: return f(&StubFace);
    case bof3::addr::Window_ResetAll:      return f(&StubResetWindows);
    case kCrtSprintf:                      return f(&StubSprintf);
    case bof3::addr::TextRecord_Set:       return f(&StubTextRecord);
    case bof3::addr::SaveMenu_DrawChoices: return f(&StubChoices);
    case bof3::addr::Menu_DrawBlackScreen: return f(&StubBlack);
    case bof3::addr::Party_RestoreAll:     return f(&StubRestore);
    default: bof3::Fatal("shop_states: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

struct Call { U offset, target; };
struct Clone {
    const char* name;
    U base, size;
    const Call* calls;
    int n_calls;
};

namespace a = bof3::addr;
constexpr Call kEnd[] = {{0x39, a::Sprite_FaceDirection}, {0x68, a::Window_ResetAll}};
constexpr Call kTitleIn[] = {{0x25, a::Menu_DrawTitleBox}, {0x5E, a::Msg_OpenSystem}};
constexpr Call kGreeting[] = {{0x11, a::Menu_DrawTitleBox}, {0x39, kCrtSprintf}, {0x47, a::TextRecord_Set},
                              {0x51, a::Msg_SystemPtr},     {0x62, a::Text_DrawAt}, {0x74, a::Menu_DrawMoneyBox},
                              {0x9F, a::Sound_PlayEffect}};
constexpr Call kChoicesIn[] = {{0x18, a::SaveMenu_DrawChoices}, {0x2F, a::Menu_DrawTitleBox}, {0x45, a::Msg_SystemPtr},
                               {0x56, a::Text_DrawAt}, {0x67, a::Menu_DrawMoneyBox}};
constexpr Call kChoose[] = {{0x04, a::SaveMenu_DrawChoices}, {0x1A, a::Menu_DrawTitleBox}, {0x30, a::Msg_SystemPtr},
                            {0x41, a::Text_DrawAt},          {0x53, a::Menu_DrawMoneyBox}, {0x9A, a::Sound_PlayEffect},
                            {0xF9, a::Sound_PlayEffect},     {0x131, a::Sound_PlayEffect}, {0x14B, a::Sound_PlayEffect},
                            {0x16E, a::Sound_PlayEffect}};
constexpr Call kChoicesOut[] = {{0x14, a::SaveMenu_DrawChoices}, {0x2B, a::Menu_DrawTitleBox},
                                {0x49, a::Menu_DrawMoneyBox}};
constexpr Call kMessage[] = {{0x11, a::Menu_DrawTitleBox}, {0x27, a::Msg_SystemPtr}, {0x38, a::Text_DrawAt},
                             {0x5E, a::Transition_Start}};
constexpr Call kFadeOut[] = {{0x0A, a::Menu_DrawBlackScreen}, {0x30, a::Menu_DrawTitleBox}, {0x3A, a::Msg_SystemPtr},
                             {0x4B, a::Text_DrawAt}};
constexpr Call kJingle[] = {{0x00, a::Menu_DrawBlackScreen}, {0x07, a::Sound_LoadStream}};
constexpr Call kWait[] = {{0x00, a::Menu_DrawBlackScreen}, {0x19, a::Sound_StreamDone}, {0x32, a::Transition_Start}};
constexpr Call kRestore[] = {{0x11, a::Menu_DrawTitleBox}, {0x25, a::Party_RestoreAll}, {0x62, a::Msg_OpenSystem}};
constexpr Call kAskSave[] = {{0x11, a::Menu_DrawTitleBox}, {0x34, a::Msg_SystemPtr}, {0x45, a::Text_DrawAt},
                             {0x4D, a::Menu_YesNo}};
constexpr Call kLeave[] = {{0x11, a::Menu_DrawTitleBox}};
constexpr Call kSaveBegin[] = {{0x00, a::Save_ReadSummaries}};
constexpr Call kSlotsIn[] = {{0x0E, a::Sound_PlayEffect}, {0x27, a::Menu_DrawTitleBox}, {0x46, a::SaveMenu_DrawSlots}};
constexpr Call kSaveChoose[] = {{0x12, a::Menu_DrawTitleBox}, {0x1C, a::Msg_SystemPtr},    {0x2D, a::Text_DrawAt},
                                {0x38, a::SaveMenu_DrawSlots}, {0x4B, a::Input_AutoRepeat}, {0xB4, a::Sound_PlayEffect},
                                {0xD0, a::Sound_PlayEffect},   {0xFB, a::Sound_PlayEffect}};
constexpr Call kSlotsOut[] = {{0x11, a::Menu_DrawTitleBox}, {0x2C, a::SaveMenu_DrawSlots}};
constexpr Call kSaveEnd[] = {{0x11, a::Menu_DrawTitleBox}, {0x6E, a::Msg_OpenSystem}};

enum : unsigned {
    kDispatch, kBegin, kModeEnd, kInnDispatch, kInnBegin, kPromptDispatch, kTitleIn2, kGreeting2, kChoicesIn2,
    kChoose2, kChoicesOut2, kNightDispatch, kMessage2, kFadeOut2, kJingle2, kWait2, kRestore2, kAskSave2, kLeave2,
    kSaveDispatch, kSaveBegin2, kSlotsIn2, kSaveChoose2, kSlotsOut2, kSaveEnd2, kCount
};

#define SS_C(name, base, size, calls) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0])}
#define SS_P(name, base, size) {name, base, size, nullptr, 0}
const Clone kClones[kCount] = {
    SS_P("ShopMode_Dispatch", 0x57F500, 0x11),
    SS_P("ShopMode_Begin", 0x57F520, 0xA4),
    SS_C("ShopMode_End", 0x57F5D0, 0x75, kEnd),
    SS_P("Inn_Dispatch", 0x57F650, 0xE),
    SS_P("Inn_Begin", 0x57F660, 0x78),
    SS_P("InnPrompt_Dispatch", 0x57F6E0, 0xE),
    SS_C("InnPrompt_TitleIn", 0x57F6F0, 0x65, kTitleIn),
    SS_C("InnPrompt_Greeting", 0x57F760, 0xA6, kGreeting),
    SS_C("InnPrompt_ChoicesIn", 0x57F810, 0x84, kChoicesIn),
    SS_C("InnPrompt_Choose", 0x57F8A0, 0x191, kChoose),
    SS_C("InnPrompt_ChoicesOut", 0x57FAB0, 0xAA, kChoicesOut),
    SS_P("InnNight_Dispatch", 0x57FB60, 0xE),
    SS_C("InnNight_Message", 0x57FB70, 0x65, kMessage),
    SS_C("InnNight_FadeOut", 0x57FBE0, 0x54, kFadeOut),
    SS_C("InnNight_Jingle", 0x57FC40, 0x23, kJingle),
    SS_C("InnNight_Wait", 0x57FC70, 0x39, kWait),
    SS_C("InnNight_Restore", 0x57FCB0, 0x69, kRestore),
    SS_C("InnNight_AskSave", 0x57FD20, 0x5D, kAskSave),
    SS_C("InnNight_Leave", 0x57FD80, 0x4D, kLeave),
    SS_P("FieldSave_Dispatch", 0x57FDD0, 0xE),
    SS_C("FieldSave_Begin", 0x57FDE0, 0x1F, kSaveBegin),
    SS_C("FieldSave_SlotsIn", 0x57FE00, 0x63, kSlotsIn),
    SS_C("FieldSave_Choose", 0x57FE70, 0x10C, kSaveChoose),
    SS_C("FieldSave_SlotsOut", 0x580150, 0x4B, kSlotsOut),
    SS_C("FieldSave_End", 0x5801A0, 0x89, kSaveEnd),
};
#undef SS_C
#undef SS_P

// --- the state both passes start from --------------------------------------

struct Region { U at, size; };
const Region kRegions[] = {
    {0x929EC0, 0x50},                                  // 0x929EC2/3 and the state block to 0x929F0F
    {at::kShopRecord, 4},
    {at::kActorStates, at::kActors * at::kActorStride},
    {at::kSpriteObjects, at::kSpriteCount * at::kSpriteStride},
    {at::kSpriteCurrent, 4},
    {at::kScriptFlags, 2},
    {at::kGameStep, 2},
    {at::kInputFlags, 1},
    {at::kArea, 2},
    {at::kColour, 1},
    {at::kMsgFlags, 4},
    {at::kText, 0x20},
    {at::kZenny, 4},
    {at::kSaveBack, 4},                                // 0x6BC880..3: the save flag, the cursor, cancelled
    {at::kPressed, 4},
    {at::kConfirm, 4},                                 // the confirm and cancel maps
    {at::kFade, 2},
    {at::kRepeatLatch, 2},
    {at::kRepeatTimer, 2},
    {at::kSlot, 4},
    {at::kSlotTop, 4},
    {at::kInnFlag, 1},
};
constexpr unsigned kRegionBytes = 0x50 + 4 + 8 * 0xA4 + 30 * 0xA4 + 4 + 2 + 2 + 1 + 2 + 1 + 4 + 0x20 + 4 + 4 + 4 + 4 +
                                  2 + 2 + 2 + 4 + 4 + 1;

struct State {
    unsigned char memory[kRegionBytes];
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

// The object byte: none, a save point, one of the 30 objects, or another
// negative.
void SeedObject() {
    static const unsigned char kObjects[] = {0xFF, 0xFE, 0, 29, 0x80, 0xFD};
    B(at::kObject) = Often() ? static_cast<unsigned char>(Next() % 30) : kObjects[Next() % 6];
    if (Half()) B(at::kObject) = Half() ? 0xFF : 0xFE;
}
void SeedCount(unsigned low, unsigned high) {
    static const unsigned char kEdges[] = {0, 1, 2, 0xFF, 0x80};
    B(at::kCount) = Often() ? static_cast<unsigned char>(low + Next() % (high - low + 1)) : kEdges[Next() % 5];
}
// A pad word with up, down or neither, and the confirm and cancel maps
// sometimes met.
void SeedPad() {
    static const std::uint16_t kDirs[] = {0, 0x1000, 0x4000, 0x5000};
    std::uint16_t pad = static_cast<std::uint16_t>((Next() & 0xAFFF) | kDirs[Next() % 4]);
    if (Next() % 4 == 0) pad = kDirs[Next() % 4];
    const std::uint16_t confirm = static_cast<std::uint16_t>(1u << (Next() % 16));
    const std::uint16_t cancel = static_cast<std::uint16_t>(1u << (Next() % 16));
    PutWord(at::kConfirm, confirm);
    PutWord(at::kCancel, cancel);
    switch (Next() % 5) {
    case 0: pad = static_cast<std::uint16_t>(pad | confirm); break;
    case 1: pad = static_cast<std::uint16_t>((pad | cancel) & ~confirm); break;
    case 2: pad = static_cast<std::uint16_t>(pad & ~(confirm | cancel)); break;
    default: break;
    }
    PutWord(at::kPressed, pad);
}
void SeedCursor() {
    static const unsigned char kCursors[] = {0, 1, 2, 3, 0x7F, 0x80, 0xFF};
    B(at::kChoice) = Often() ? static_cast<unsigned char>(Next() % 3) : kCursors[Next() % 7];
}

void Fix() {
    // The object byte in range or negative; Sprite_Current on an object.
    const auto object = static_cast<signed char>(B(at::kObject));
    if (object >= 30) B(at::kObject) = static_cast<unsigned char>(Next() % 30);
    PutLong(at::kSpriteCurrent, at::kSpriteObjects + (Next() % at::kSpriteCount) * at::kSpriteStride);
}

void Seed(unsigned k) {
    switch (k) {
    case kDispatch:
        B(at::kState) = static_cast<unsigned char>(Next() % kModeEntries);
        break;
    case kInnDispatch:
        // mostly Inn_Steps' five; sometimes on into the tables after it
        B(at::kStep) = static_cast<unsigned char>(Often() ? Next() % 5 : Next() % kInnEntries);
        break;
    case kPromptDispatch:
        B(at::kSub) = static_cast<unsigned char>(Often() ? Next() % 6 : Next() % (kInnEntries - 5));
        break;
    case kNightDispatch:
        B(at::kSub) = static_cast<unsigned char>(Often() ? Next() % 7 : Next() % (kInnEntries - 11));
        break;
    case kSaveDispatch:
        B(at::kSub) = static_cast<unsigned char>(Next() % 9);
        break;
    case kBegin:
    case kModeEnd: {
        SeedObject();
        if (Half()) B(at::kObject) = static_cast<unsigned char>(Next() % 30);
        // the character records at the quarter-HP edge
        for (U n = 0; n < at::kActors; ++n) {
            const U r = at::kActorStates + n * at::kActorStride;
            const std::uint16_t max = static_cast<std::uint16_t>(Next());
            PutWord(r + 0x10, max);
            if (Often()) PutWord(r + 8, static_cast<std::uint16_t>((max >> 2) + static_cast<int>(Next() % 3) - 1));
        }
        if (k == kModeEnd && B(at::kObject) < 30) {
            unsigned char* const o = At(at::kSpriteObjects + B(at::kObject) * at::kSpriteStride);
            o[7] = static_cast<unsigned char>(Half() ? o[7] & ~8u : o[7] | 8u);
        }
        break;
    }
    case kInnBegin: {
        SeedObject();
        static const std::uint16_t kAreas[] = {0xBC, 0x85, 0xC1, 0xBD, 0x84, 0};
        PutWord(at::kArea, Often() ? kAreas[Next() % 6] : static_cast<std::uint16_t>(Next()));
        B(at::kInputFlags) = Half() ? 0x40 : static_cast<unsigned char>(Next());
        break;
    }
    case kTitleIn2:
    case kChoicesIn2:
    case kMessage2:
    case kSlotsIn2:
        SeedObject();
        SeedCount(1, 5);
        break;
    case kChoicesOut2:
    case kSlotsOut2:
        SeedObject();
        SeedCount(3, 6);
        SeedCursor();
        B(at::kCancelled) = Half() ? 0 : static_cast<unsigned char>(Next());
        break;
    case kGreeting2:
    case kAskSave2:
        SeedObject();
        B(at::kMsgFlags) = static_cast<unsigned char>(Half() ? (Next() | 2) : (Next() & ~2u));
        break;
    case kChoose2: {
        SeedObject();
        SeedCursor();
        SeedPad();
        const U price = B(at::kShopNumber) * 10u;
        if (Often()) PutLong(at::kZenny, price + static_cast<U>(static_cast<int>(Next() % 3) - 1));
        if (Next() % 8 == 0) B(at::kObject) = 0;
        break;
    }
    case kFadeOut2:
    case kRestore2:
        SeedObject();
        PutWord(at::kFade, Half() ? 0 : static_cast<std::uint16_t>(Next()));
        break;
    case kWait2:
        SeedCount(0, 2);
        break;
    case kLeave2:
        B(at::kAnswer) = Half() ? 0 : static_cast<unsigned char>(Next());
        break;
    case kSaveChoose2: {
        SeedPad();
        static const U kSlots[] = {0, 1, 2, 3, 14, 15, 16, 0xFFFFFFFFu, 0x100, 0x80000000u};
        const U slot = Often() ? Next() % 16 : kSlots[Next() % 10];
        PutLong(at::kSlot, slot);
        static const int kTops[] = {0, -1, -2, -3, 1};
        PutLong(at::kSlotTop, Often() ? slot + static_cast<U>(kTops[Next() % 5]) : Next());
        break;
    }
    case kSaveEnd2:
        SeedObject();
        SeedCursor();
        B(at::kInnFlag) = Half() ? 0 : static_cast<unsigned char>(Next());
        B(at::kSaveBack) = Half() ? 0 : static_cast<unsigned char>(Next());
        break;
    default:
        break;
    }
}

// What the rounds reached, from the original's side, for the log line.
struct Coverage {
    unsigned targets[38];
    unsigned forced, paid, short_of, other_choice, cancelled, moved, top_moved, yes, done, flagged;
} g_cover;
unsigned Logged(const State& s, U what) {
    unsigned n = 0;
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i)
        if (s.log[i].what == what) ++n;
    return n;
}
bool LoggedArg(const State& s, U what, U arg) {
    for (unsigned i = 0; i < s.log_n && i < kLog; ++i)
        if (s.log[i].what == what && s.log[i].a == arg) return true;
    return false;
}
void Cover(unsigned k, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what >= 0x100 && out.log[i].what < 0x100 + 38) ++g_cover.targets[out.log[i].what - 0x100];
    switch (k) {
    case kInnBegin:
        // the region starts at 0x929EC0: the step 0x929F01 at +0x41
        if (out.memory[0x41] == 3) ++g_cover.forced;
        break;
    case kChoose2:
        if (LoggedArg(out, 6, 0x107)) ++g_cover.short_of;
        else if (LoggedArg(out, 6, 0x104)) ++g_cover.paid;
        if (LoggedArg(out, 6, 0x106)) ++g_cover.cancelled;
        if (LoggedArg(out, 6, 0x101)) ++g_cover.moved;
        break;
    case kSaveChoose2:
        if (LoggedArg(out, 6, 0x101)) ++g_cover.top_moved;
        break;
    case kAskSave2:
        if (Logged(out, 12) && out.log_n) ++g_cover.yes;
        break;
    case kWait2:
        if (Logged(out, 13)) ++g_cover.done;
        break;
    case kSaveEnd2:
        if (Logged(out, 5)) ++g_cover.flagged;
        break;
    default:
        break;
    }
}

}  // namespace

void SelfTest() {
    constexpr unsigned kRounds = 25 * 1600;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("shop_states: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        bof3::CloneCall calls[12];
        if (c.n_calls > 12) bof3::Fatal("shop_states: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&ShopMode_Dispatch),    reinterpret_cast<const void*>(&ShopMode_Begin),
        reinterpret_cast<const void*>(&ShopMode_End),         reinterpret_cast<const void*>(&Inn_Dispatch),
        reinterpret_cast<const void*>(&Inn_Begin),            reinterpret_cast<const void*>(&InnPrompt_Dispatch),
        reinterpret_cast<const void*>(&InnPrompt_TitleIn),    reinterpret_cast<const void*>(&InnPrompt_Greeting),
        reinterpret_cast<const void*>(&InnPrompt_ChoicesIn),  reinterpret_cast<const void*>(&InnPrompt_Choose),
        reinterpret_cast<const void*>(&InnPrompt_ChoicesOut), reinterpret_cast<const void*>(&InnNight_Dispatch),
        reinterpret_cast<const void*>(&InnNight_Message),     reinterpret_cast<const void*>(&InnNight_FadeOut),
        reinterpret_cast<const void*>(&InnNight_Jingle),      reinterpret_cast<const void*>(&InnNight_Wait),
        reinterpret_cast<const void*>(&InnNight_Restore),     reinterpret_cast<const void*>(&InnNight_AskSave),
        reinterpret_cast<const void*>(&InnNight_Leave),       reinterpret_cast<const void*>(&FieldSave_Dispatch),
        reinterpret_cast<const void*>(&FieldSave_Begin),      reinterpret_cast<const void*>(&FieldSave_SlotsIn),
        reinterpret_cast<const void*>(&FieldSave_Choose),     reinterpret_cast<const void*>(&FieldSave_SlotsOut),
        reinterpret_cast<const void*>(&FieldSave_End)};

    static State saved, input, their_out, our_out;
    Capture(saved);
    // The five tables, filled with recorders meanwhile.
    U mode_table[kModeEntries], inn_table[kInnEntries];
    std::memcpy(mode_table, At(at::kModeStates), sizeof mode_table);
    std::memcpy(inn_table, At(at::kInnSteps), sizeof inn_table);
    for (unsigned i = 0; i < kModeEntries; ++i) PutLong(at::kModeStates + i * 4, Addr(reinterpret_cast<const void*>(TargetTable()[i])));
    for (unsigned i = 0; i < kInnEntries; ++i)
        PutLong(at::kInnSteps + i * 4, Addr(reinterpret_cast<const void*>(TargetTable()[kModeEntries + i])));
    g = kStubs;

    unsigned bad = 0, calls = 0, per[kCount] = {}, bad_per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        for (unsigned i = 0; i < kRegionBytes; ++i) input.memory[i] = static_cast<unsigned char>(Next());
        std::memset(input.log, 0, sizeof input.log);
        input.log_n = 0;
        Apply(input);
        Fix();
        Seed(k);
        g_seed = Next();
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* const fn = pass ? ours[k] : clones[k];
            reinterpret_cast<void (__cdecl*)()>(const_cast<void*>(fn))();
            Capture(out);
        }
        calls += their_out.log_n;
        Cover(k, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12)
                bof3::Log("shadow      shop_states self-test MISMATCH: round %u, %s, log %u / %u", round, kClones[k].name,
                          their_out.log_n, our_out.log_n);
        }
    }
    g = kOriginals;
    std::memcpy(At(at::kModeStates), mode_table, sizeof mode_table);
    std::memcpy(At(at::kInnSteps), inn_table, sizeof inn_table);
    Apply(saved);

    bof3::Log("shadow      shop_states self-test: %u rounds over %u functions (%u each), %u calls to the stand-ins, "
              "%u MISMATCHES; the state block, the character records, the sprite objects and Sprite_Current, the "
              "inn's bytes, the zenny, the pad and its maps, the save cursor and the stand-ins' log compared",
              kRounds, static_cast<unsigned>(kCount), per[0], calls, bad);
    if (bad)
        for (unsigned k = 0; k < kCount; ++k)
            if (bad_per[k]) bof3::Log("shadow      shop_states: %s mismatched in %u rounds", kClones[k].name, bad_per[k]);
    const Coverage& c = g_cover;
    unsigned reached = 0;
    for (unsigned i = 0; i < 38; ++i) reached += c.targets[i] != 0;
    bof3::Log("shadow      shop_states coverage: table entries reached %u of 38; to the save menu at once %u; inn paid %u, "
              "short of zenny %u, cancelled %u, cursor moved %u; save slot moved %u; asked to save %u; night over %u; "
              "message 0xD0 %u",
              reached, c.forced, c.paid, c.short_of, c.cancelled, c.moved, c.top_moved, c.yes, c.done, c.flagged);
    if (bad) bof3::Fatal("the shop overlay's first table differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace shop_states
