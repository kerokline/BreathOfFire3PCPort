// BOF3X_SHADOW=mode_states: a differential fuzz of the twenty functions of
// mode_states.cpp against byte-copies of Capcom's, once at start-up
// (docs/mode_states.md section 4).
//
// Every call and tail jump out of a copy is re-aimed at a recording stand-in
// (bof3::CloneCall with `expected`); MsgBox_SystemChoice's sixteen stack
// immediates are re-aimed at recorders inside its copy (checked first);
// Scena01_EnterArea's three jump tables are relocated into its copy; the four
// .data tables the dispatchers jump through (GameMode_ShopSteps,
// FieldCore_State2Steps, FieldCore_FadeSteps, Scena01_States) are swapped for
// recorders. The system choice's id 0x90 - which calls its own return
// address - is run through copies of both message-box commits, one calling
// the choice's copy and one calling ours, so that the tail each resumes into
// is a copy's. One round: one function, random bytes in every region any of
// them touches, the pointers put back inside buffers of the fuzz's own, each
// branch's boundaries seeded; theirs, then from the same state ours; the
// regions, the buffers, the packet cursor, the result and the stand-ins' log
// compared. Everything is put back afterwards.
#include <cstddef>
#include <cstdint>
#include <cstring>

#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/mode_states_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace mode_states {
namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
void SetPtr(std::uint32_t address, const void* p) { SetLong(At(address), static_cast<std::int32_t>(Address(p))); }

// --- the fuzz's own buffers and the stand-ins' log ---------------------------

constexpr unsigned kLog = 96;
struct Entry { std::uint32_t what, a, b, c, d, e, f, h; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;
unsigned g_load_calls, g_load_limit, g_frame_calls, g_frame_limit;

constexpr unsigned kPool = 0x100, kText = 0x40, kFieldState = 0x140, kWindow = 0x10;
unsigned char g_prim[kPool];           // Gfx_PacketNext points in here
unsigned char g_text[kText];           // Text_DrawSmall's text
unsigned char g_fstate[2][kFieldState];  // Field_State points at one of these
unsigned char g_window[kWindow];       // the window record 0x905B84 points at

constexpr std::uint32_t kObjects = 0x7DEE80, kObjectSize = 0xA4;
constexpr unsigned kObjectCount = 16;
unsigned char* Object(unsigned i) { return At(kObjects + i * kObjectSize); }

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n * 0x2545F491u) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0,
            std::uint32_t e = 0, std::uint32_t f = 0, std::uint32_t h = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d, e, f, h};
    ++g_log_n;
}
// A pointer as something both sides agree on: an offset into one of the
// buffers, or the address itself.
std::uint32_t Id(const void* p) {
    const std::uint32_t at = Address(p);
    const auto in = [at](const void* b, std::uint32_t n) { return at >= Address(b) && at < Address(b) + n; };
    if (in(g_prim, sizeof g_prim)) return 0x10000u + (at - Address(g_prim));
    if (in(g_text, sizeof g_text + 1)) return 0x20000u + (at - Address(g_text));
    if (in(g_fstate, sizeof g_fstate)) return 0x30000u + (at - Address(g_fstate));
    return at;
}

// Every byte below is one some function reads again after a call: the step,
// the mode, the wait word, Sprite_Current, Field_State, the camera words, the
// area, the counters, the flag bank's dword, Cond_ByteFD, the packet cursor,
// the text after the cursor, the input flags, the script flags.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) return;
    const unsigned v = (h >> 12) & 0xFF;
    const unsigned w = h >> 20;
    switch ((h >> 4) % 16) {
    case 0: Game_Step = static_cast<unsigned short>(v % 3 == 0 ? h >> 16 : v % 4); break;
    case 1: MoveScript_WaitWordDA = static_cast<unsigned short>(v % 2 ? 0 : w); break;
    case 2: Sprite_Current = Object(w % kObjectCount); break;
    case 3: Field_State = g_fstate[v % 2]; break;
    case 4: {
        static const std::uint16_t kAngles[] = {0xFD56, 0x200, 0xFD55, 0xFD57, 0x1FF, 0x201, 0xFE39, 0x355, 0xAA, 0x6C0};
        SetWord(At(0x929EC8 + 4 * (w % 2)), kAngles[v % 10]);
        break;
    }
    case 5: {
        static const std::uint16_t kAreas[] = {0, 5, 7, 8, 9, 0xA, 0xD, 0xE, 0x10, 0x11, 0x13, 0x16, 0x17, 0x21, 0x60, 0x4};
        Game_AreaNumber = kAreas[v % 16];
        break;
    }
    case 6: At(at::kCounters + w % 4)[0] = static_cast<unsigned char>(v % 3 == 0 ? v : v % 10); break;
    case 7: SetLong(At(at::kFlagBank), static_cast<std::int32_t>(h * 0x9E3779B1u)); break;
    case 8: Cond_ByteFD = static_cast<unsigned char>(v % 2 ? 1 : v); break;
    case 9: Gfx_PacketNext = g_prim + (w % 0xC0); break;
    case 10: g_text[w % kText] = static_cast<unsigned char>(v % 3 == 0 ? 0 : v); break;
    case 11: Field_InputFlags = static_cast<unsigned char>(Field_InputFlags ^ 0x10); break;
    case 12: Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ (1u << (w % 16))); break;
    case 13: Game_Mode = static_cast<unsigned short>(w); break;
    case 14: Sprite_Current[1 + w % 3] = static_cast<unsigned char>(v); break;
    default: Field_Request = static_cast<unsigned char>(v); break;
    }
}

// --- the stand-ins ---------------------------------------------------------

template <unsigned N> void __cdecl StubVoid() { Record(N, Id(Sprite_Current)); Disturb(); }
// Look_Return brings the camera home half the time, one word or both, so
// that GameMode_LookEnd's test after it goes either way.
void __cdecl StubLookReturn() {
    Record(20);
    const std::uint32_t h = Hash();
    if (h % 2) SetWord(At(0x929EC8), 0xFD56);
    if ((h >> 1) % 3) SetWord(At(0x929ECC), 0x200);
    Disturb();
}
// Not done for the round's first g_load_limit asks, then done.
int __cdecl StubLoadDone() {
    Record(1);
    Disturb();
    return ++g_load_calls > g_load_limit ? 1 : 0;
}
void __cdecl StubSleep(int frames) { Record(2, static_cast<std::uint32_t>(frames)); Disturb(); }
void __cdecl StubLoadDat(int file) { Record(3, static_cast<std::uint32_t>(file)); Disturb(); }
void __cdecl StubClutRow(unsigned row) { Record(4, row); Disturb(); }
void __cdecl StubFace(unsigned char direction) { Record(5, direction, Id(Sprite_Current)); Disturb(); }
void __cdecl StubBank(unsigned file) { Record(6, file); Disturb(); }
// The menu's frame ends the transition wait after g_frame_limit frames.
void __cdecl StubMenuFrame() {
    Record(7);
    Disturb();
    if (++g_frame_calls > g_frame_limit) MoveScript_WaitWordDA = 0;
}
// Text_EmitGlyph uses the low words of h, u and v (the original pushes
// registers whose upper halves are left over); x, y, w and the CLUT whole.
void __cdecl StubEmit(int x, int y, int w, int h, int u, int v, int clut) {
    Record(8, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), static_cast<std::uint32_t>(w),
           static_cast<std::uint32_t>(h) & 0xFFFF, static_cast<std::uint32_t>(u) & 0xFFFF,
           static_cast<std::uint32_t>(v) & 0xFFFF, static_cast<std::uint32_t>(clut) | Word(Gfx_PacketNext + 0x16) << 16);
    Disturb();
}
unsigned char __cdecl StubShadeStep(unsigned step) {
    Record(9, step);
    Disturb();
    return static_cast<unsigned char>(Hash() % 3 == 0 ? 0 : Hash() >> 9);
}
// The answer from the hash, so both sides see the same one; the pointer is
// recorded, never dereferenced.
unsigned char __cdecl StubFlagsTest(const unsigned char* bits, unsigned index) {
    Record(10, Address(bits), index);
    const std::uint32_t h = Hash();
    Disturb();
    return static_cast<unsigned char>(h % 2 ? 0 : (h >> 8) % 3 == 0 ? 0x80 : 1 + (h >> 9) % 0xFF);
}
void __cdecl StubFlagsSet(unsigned char* bits, unsigned index) { Record(11, Address(bits), index); Disturb(); }
int __cdecl StubMusicLoad(unsigned track) { Record(12, track); Disturb(); return static_cast<int>(Hash()); }
void __cdecl StubMusicStop(int frames) { Record(13, static_cast<std::uint32_t>(frames)); Disturb(); }
void __cdecl StubSound(unsigned short id) { Record(14, id); Disturb(); }
// A slot of the first eight effect records, or none.
unsigned char __cdecl StubEffectFind() {
    Record(15);
    const std::uint32_t h = Hash();
    Disturb();
    return static_cast<unsigned char>(h % 4 == 0 ? 0xFF : (h >> 8) % 8);
}
void __cdecl StubCallA(unsigned n) { Record(16, n); Disturb(); }
unsigned char __cdecl StubTestFB(short x, short z) {
    Record(17, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(z));
    Disturb();
    return static_cast<unsigned char>(Hash());
}
void __cdecl StubChangeArea(unsigned area, int x, int z, unsigned flags) {
    Record(18, area, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(z), flags);
    Disturb();
}
// Menu_DrawHand reads the low words of x and y (docs/mode_states.md).
void __cdecl StubDrawHand(int x, int y, int unused) {
    Record(19, static_cast<std::uint32_t>(x) & 0xFFFF, static_cast<std::uint32_t>(y) & 0xFFFF, static_cast<std::uint32_t>(unused));
    Disturb();
}

// The table slots: which one ran.
template <unsigned N> void __cdecl Slot() { Record(40 + N, Id(Sprite_Current)); Disturb(); }
constexpr unsigned kSlotCount = 20;
constexpr Handler kSlotFns[kSlotCount] = {Slot<0>, Slot<1>, Slot<2>, Slot<3>, Slot<4>, Slot<5>, Slot<6>,
                                          Slot<7>, Slot<8>, Slot<9>, Slot<10>, Slot<11>, Slot<12>, Slot<13>,
                                          Slot<14>, Slot<15>, Slot<16>, Slot<17>, Slot<18>, Slot<19>};
// The system choice's sixteen handlers.
template <unsigned N> void __cdecl Choice() { Record(70 + N); Disturb(); }
constexpr Handler kChoiceFns[16] = {Choice<0>, Choice<1>, Choice<2>,  Choice<3>,  Choice<4>,  Choice<5>,
                                    Choice<6>, Choice<7>, Choice<8>,  Choice<9>,  Choice<10>, Choice<11>,
                                    Choice<12>, Choice<13>, Choice<14>, Choice<15>};

const Callees kStubs = {
    {Choice<0>, Choice<1>, Choice<2>, Choice<3>, Choice<4>, Choice<5>, Choice<6>, Choice<7>, Choice<8>, Choice<9>,
     Choice<10>, Choice<11>, Choice<12>, Choice<13>, Choice<14>, Choice<15>},
    {0, 0},
    StubLookReturn, StubVoid<21>, StubMenuFrame,
    StubVoid<22>, StubLoadDat, StubLoadDone, StubSleep, StubClutRow, StubFace, StubBank, StubVoid<23>, StubEmit,
    StubVoid<24>, StubVoid<25>, StubVoid<26>, StubVoid<27>, StubVoid<28>, StubVoid<29>, StubVoid<30>, StubVoid<31>,
    StubVoid<32>, StubVoid<33>, StubVoid<34>, StubVoid<35>, StubShadeStep,
    StubFlagsTest, StubFlagsSet, StubVoid<36>, StubMusicLoad, StubMusicStop, StubSound, StubEffectFind, StubCallA,
    StubVoid<37>, StubTestFB, StubChangeArea, StubDrawHand,
};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x496AD0: return f(kStubs.look_return);
    case 0x517290: return f(kStubs.loading_frame);
    case 0x5172F0: return f(kStubs.menu_frame);
    case 0x517200: return f(kStubs.field_frame);
    case 0x454590: return f(kStubs.load_dat);
    case 0x454810: return f(kStubs.load_done);
    case 0x5A9949: return f(kStubs.sleep);
    case 0x4549F0: return f(kStubs.clut_copy_row);
    case 0x57C4C0: return f(kStubs.face_direction);
    case 0x454770: return f(kStubs.bank_file);
    case kAfterShop: return f(kStubs.after_shop);
    case 0x516D50: return f(kStubs.emit_glyph);
    case 0x56E6C0: return f(kStubs.areamap_frame);
    case 0x57B780: return f(kStubs.extra_screens);
    case 0x531B60: return f(kStubs.update_screens);
    case 0x5173E0: return f(kStubs.objects_screen);
    case 0x494030: return f(kStubs.run_effects);
    case 0x454AD0: return f(kStubs.tint_frame);
    case 0x592F00: return f(kStubs.draw_frame);
    case 0x56D690: return f(kStubs.mode_dispatch);
    case kMenuStates: return f(kStubs.menu_states);
    case 0x59E230: return f(kStubs.task_records);
    case kShopStates: return f(kStubs.shop_states);
    case 0x534590: return f(kStubs.shade_begin);
    case 0x534790: return f(kStubs.shade_step);
    case 0x57C140: return f(kStubs.flags_test);
    case 0x57C0F0: return f(kStubs.flags_set);
    case 0x57C7C0: return f(kStubs.set40);
    case 0x587A20: return f(kStubs.music_load);
    case 0x587B40: return f(kStubs.music_stop);
    case 0x587740: return f(kStubs.sound);
    case 0x589810: return f(kStubs.effect_find);
    case 0x5341A0: return f(kStubs.call_a);
    case 0x571720: return f(kStubs.setup_entries);
    case 0x572650: return f(kStubs.test_fb);
    case 0x594E00: return f(kStubs.change_area);
    case 0x5905D0: return f(kStubs.draw_hand);
    default: bof3::Fatal("mode_states: no stand-in for a call to 0x%X", static_cast<unsigned>(target)); return nullptr;
    }
}

// --- the copies ------------------------------------------------------------

// Calls and tail jumps out, by capstone 2026-09-25 (the E8 / E9 byte's
// offset); every other jump stays inside.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; unsigned ret; };

constexpr Call kLookEndCalls[] = {{0x0, 0x496AD0}};
constexpr Call kOpenCalls[] = {{0x5C, 0x57C4C0}, {0x8A, 0x517200}, {0x96, 0x454590}, {0x9E, 0x454810}, {0xA7, 0x517290},
                               {0xAE, 0x5A9949}, {0xB6, 0x454810}, {0xC1, 0x4549F0}, {0xC8, 0x4549F0}, {0xD7, 0x517200}};
constexpr Call kCloseCalls[] = {{0x15, 0x454770}, {0x1D, 0x454810}, {0x26, 0x517290}, {0x2D, 0x5A9949},
                                {0x35, 0x454810}, {0x4C, 0x454590}, {0x54, 0x454810}, {0x5D, 0x517290},
                                {0x64, 0x5A9949}, {0x6C, 0x454810}, {0x75, 0x4560D0}, {0x90, 0x517200}};
constexpr Call kMenuWaitCalls[] = {{0x7, 0x5A9949}, {0x1D, 0x5172F0}, {0x30, 0x5A9949}};
constexpr Call kSmallCalls[] = {{0xBB, 0x516D50}};
constexpr Call kLoadingCalls[] = {{0x0, 0x56E6C0}, {0x5, 0x57B780}, {0xA, 0x531B60}, {0xF, 0x5173E0},
                                  {0x14, 0x494030}, {0x19, 0x454AD0}, {0x1E, 0x592F00}};
constexpr Call kMenuFrameCalls[] = {{0x0, 0x56D690}, {0x5, 0x589970}, {0xA, 0x59E230}};
constexpr Call kShopFrameCalls[] = {{0x0, 0x56E6C0}, {0x5, 0x57B780}, {0xA, 0x531B60}, {0xF, 0x5173E0},
                                    {0x14, 0x494030}, {0x19, 0x592F00}, {0x1E, 0x59E230}, {0x23, 0x57F500}};
constexpr Call kFadeBeginCalls[] = {{0x0, 0x534590}};
constexpr Call kFadeStepCalls[] = {{0x2, 0x534790}};
constexpr Call kEnterCalls[] = {
    {0x2A, 0x57C140},  {0x51, 0x57C140},  {0x5E, 0x587A20},  {0x66, 0x454810},  {0x71, 0x5A9949},  {0x79, 0x454810},
    {0xD7, 0x57C140},  {0x115, 0x57C140}, {0x121, 0x57C7C0}, {0x18C, 0x587B40}, {0x1A2, 0x587A20}, {0x1AA, 0x454810},
    {0x1B9, 0x5A9949}, {0x1C1, 0x454810}, {0x20D, 0x57C140}, {0x21B, 0x587B40}, {0x23C, 0x57C140}, {0x26D, 0x57C140},
    {0x287, 0x57C7C0}, {0x2C3, 0x57C140}, {0x300, 0x57C140}, {0x315, 0x57C140}, {0x330, 0x57C7C0}, {0x377, 0x57C140},
    {0x3D0, 0x57C140}, {0x3E0, 0x589810}, {0x42A, 0x57C140}, {0x457, 0x57C0F0}, {0x45E, 0x5341A0}, {0x4C2, 0x57C140},
    {0x4E3, 0x57C0F0}, {0x4F9, 0x57C140}, {0x517, 0x57C140}, {0x523, 0x57C7C0}, {0x53E, 0x57C0F0}, {0x551, 0x57C140},
    {0x56F, 0x57C140}, {0x5AE, 0x57C140}, {0x62E, 0x57C140}, {0x63C, 0x587B40}, {0x656, 0x57C140}, {0x672, 0x589810},
    {0x6A1, 0x5341A0}, {0x6B6, 0x57C140}, {0x6CD, 0x571720}, {0x6D6, 0x572650}, {0x6EA, 0x57C0F0}, {0x6F8, 0x57C0F0}};
constexpr Call kHookCalls[] = {
    {0x23, 0x57C140},  {0x43, 0x57C7C0},  {0x78, 0x57C140},  {0xA8, 0x57C7C0},  {0xE1, 0x57C7C0},  {0x11A, 0x57C7C0},
    {0x14A, 0x57C140}, {0x172, 0x57C7C0}, {0x17C, 0x587740}, {0x19A, 0x57C140}, {0x1C6, 0x57C7C0}, {0x1F4, 0x594E00},
    {0x21A, 0x57C140}, {0x22E, 0x57C140}, {0x25A, 0x57C7C0}, {0x284, 0x57C140}, {0x299, 0x57C140}, {0x2BD, 0x57C7C0},
    {0x2EE, 0x57C140}, {0x31A, 0x57C7C0}, {0x33E, 0x594E00}, {0x364, 0x57C140}, {0x37D, 0x57C140}, {0x3D6, 0x594E00},
    {0x3ED, 0x57C140}, {0x425, 0x57C7C0}, {0x449, 0x594E00}, {0x461, 0x57C140}, {0x476, 0x57C140}, {0x49A, 0x57C7C0},
    {0x4DF, 0x57C0F0}};
constexpr Call kCursorCalls[] = {{0x46, 0x5905D0}};

enum : unsigned {
    kLookEnd, kShop, kOpen, kClose, kMenuWait, kLookPad, kLookReturn, kChoice, kSmall, kLoading, kMenuFrame, kShopFrame,
    kState2, kFade, kFadeBegin, kFadeStep, kFrame01, kEnter, kHook, kCursor, kCount,
    kViaCommit = kCount,   // the system choice through the commits' copies (id 0x80..0x90)
    kKinds
};

#define MS_C(name, base, size, calls, ret) {name, base, size, calls, static_cast<int>(sizeof calls / sizeof calls[0]), ret}
#define MS_P(name, base, size, ret) {name, base, size, nullptr, 0, ret}
const Clone kClones[kCount] = {
    MS_C("GameMode_LookEnd", 0x496250, 0x32, kLookEndCalls, 0),
    MS_P("GameMode_Shop", 0x496290, 0xF, 0),
    MS_C("Shop_Open", 0x4962A0, 0xE5, kOpenCalls, 0),
    MS_C("Shop_Close", 0x496390, 0x95, kCloseCalls, 0),
    MS_C("Menu_WaitTransition", 0x496830, 0x3A, kMenuWaitCalls, 0),
    MS_P("Look_PadControl", 0x496A00, 0xC1, 0),
    MS_P("Look_Return", 0x496AD0, 0x88, 0),
    MS_P("MsgBox_SystemChoice", 0x498A30, 0x98, 0),
    MS_C("Text_DrawSmall", 0x516E70, 0xEE, kSmallCalls, 2),
    MS_C("Field_LoadingFrame", 0x517290, 0x23, kLoadingCalls, 0),
    MS_C("Menu_Frame", 0x5172F0, 0xF, kMenuFrameCalls, 0),
    MS_C("Shop_Frame", 0x517300, 0x28, kShopFrameCalls, 0),
    MS_P("FieldCore_State2", 0x525370, 0x12, 0),
    MS_P("FieldCore_Fade", 0x5258B0, 0x12, 0),
    MS_C("FieldCore_FadeBegin", 0x5258D0, 0x4F, kFadeBeginCalls, 0),
    MS_C("FieldCore_FadeStep", 0x525920, 0x39, kFadeStepCalls, 0),
    MS_P("Scena01_Frame", 0x539AD0, 0xE, 0),
    MS_C("Scena01_EnterArea", 0x539B20, 0x788, kEnterCalls, 0),
    MS_C("Scena01_StepHook", 0x53D830, 0x4EE, kHookCalls, 1),
    MS_C("Window_DrawCursor", 0x5960D0, 0x50, kCursorCalls, 0),
};
#undef MS_C
#undef MS_P
constexpr move_script::Table kEnterTables[3] = {{0xCA, 0x72C, 6}, {0x2B6, 0x744, 9}, {0x5A2, 0x768, 8}};
// The system choice's imm32s: +0xC, then +0x19 + 8k.
constexpr std::uint32_t kChoiceImm(unsigned i) { return i == 0 ? 0xC : 0x19 + 8 * (i - 1); }
constexpr std::uint32_t kChoiceHandlers[16] = {0x498AD0, 0x498B00, 0x498B30, 0x498B60, 0x498B90, 0x498BC0,
                                               0x498BE0, 0x498C00, 0x498C20, 0x498C40, 0x498C60, 0x498C80,
                                               0x498CA0, 0x498CC0, 0x498CE0, 0x498D00};
// The two commits are item_use.cpp's (injected before this fuzz runs, so
// not clonable): each is stood in for by a thunk of the fuzz's own - a call
// of the choice (the choice's copy on one side, ours on the other) and then
// a byte-copy of the commit's own tail, 0x4981F1..0x49822C and
// 0x4983F1..0x49840B (capstone 2026-09-25: absolute stores, one short jump
// inside, a ret; no relative transfer out). The call's return address is the
// tail's first byte, as in the original.
struct Tail { std::uint32_t at, size; };
constexpr Tail kTails[2] = {{0x4981F1, 0x3C}, {0x4983F1, 0x1B}};
void* Thunk(const void* choice, const Tail& tail) {
    void* const code = VirtualAlloc(nullptr, 5 + tail.size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!code) bof3::Fatal("mode_states: VirtualAlloc for a commit's thunk failed");
    auto* b = static_cast<std::uint8_t*>(code);
    b[0] = 0xE8;
    const std::int32_t rel = static_cast<std::int32_t>(Address(choice) - (Address(code) + 5));
    std::memcpy(b + 1, &rel, sizeof rel);
    std::memcpy(b + 5, At(tail.at), tail.size);
    if (b[5 + tail.size - 1] != 0xC3) bof3::Fatal("mode_states: the commit tail at 0x%X does not end in a ret", static_cast<unsigned>(tail.at));
    FlushInstructionCache(GetCurrentProcess(), code, 5 + tail.size);
    return code;
}

// --- the swapped .data tables ----------------------------------------------

struct Swap { std::uint32_t at, words, first_slot; std::uint32_t saved[9]; };
Swap g_swaps[] = {{at::kShopSteps, 3, 0, {}}, {at::kState2Steps, 9, 3, {}}, {at::kFadeSteps, 2, 12, {}},
                  {at::kScena01States, 3, 14, {}}};
void SwapTables() {
    for (Swap& s : g_swaps) {
        std::memcpy(s.saved, At(s.at), 4 * s.words);
        for (unsigned j = 0; j < s.words; ++j) SetPtr(s.at + 4 * j, reinterpret_cast<const void*>(kSlotFns[s.first_slot + j]));
    }
}
void RestoreTables() {
    for (const Swap& s : g_swaps) std::memcpy(At(s.at), s.saved, 4 * s.words);
}

// --- the state both passes start from --------------------------------------

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {0x7DEE40, 0x40},                   // the message box's bytes (choice id, set, row, sub-state, colour)
    {kObjects, kObjectSize * kObjectCount},
    {0x803160, 0x30},                   // 0x803163, 0x803187 (the commits' tails)
    {0x8034E0, 0x18},                   // status bits, chapter state, run, step, timer, Cond_ByteFD
    {0x7E0670, 4},                      // Gfx_PacketNext
    {0x7E0680, 8},                      // Light_AnglesCopy
    {0x7E0918, 1},                      // Draw_PassFlags
    {0x7E11E0, 0x400},                  // the first eight effect records
    {0x7E1BE8, 4},                      // Input_Held
    {0x903580, 0x12},                   // the button map
    {0x903840, 0x14},                   // Camera_Distance, the counters, the effect slot
    {0x9039A2, 2},                      // Field_ScriptFlags
    {at::kPartyCombo, 1},
    {0x904EFC, 2},                      // Game_AreaNumber
    {at::kWindow, 4},
    {0x905BA2, 1},                      // Field_InputFlags
    {0x905D98, 4},                      // Field_State
    {0x905E69, 1},                      // MapView_Redraw
    {0x929EC8, 0xC},                    // the camera angles and the flag bank's dword
    {0x929F00, 0x10},                   // the menu block, the shop's object
    {0x937F88, 4},                      // Sprite_Current
    {0x937F90, 1},                      // Gfx_ClutStripDirty
    {0x66C7D8, 0x14},                   // Field_Request, the turn byte, Game_Mode, Game_Step
    {0x66C810, 2},                      // MoveScript_WaitWordDA
    {at::kSmallUV, 0x20},               // constant data from here on - random here, put back after
    {at::kCursorRows, 0x30},
};
constexpr unsigned kRegionBytes = 0x40 + kObjectSize * kObjectCount + 0x30 + 0x18 + 4 + 8 + 1 + 0x400 + 4 + 0x12 + 0x14 + 2 +
                                  1 + 2 + 4 + 1 + 4 + 1 + 0xC + 0x10 + 4 + 1 + 0x14 + 2 + 0x20 + 0x30;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char prim[kPool];
    unsigned char text[kText];
    unsigned char fstate[2][kFieldState];
    unsigned char window[kWindow];
    std::uint32_t packet, sprite, field_state, result;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.prim, g_prim, sizeof g_prim);
    std::memcpy(s.text, g_text, sizeof g_text);
    std::memcpy(s.fstate, g_fstate, sizeof g_fstate);
    std::memcpy(s.window, g_window, sizeof g_window);
    s.packet = Id(Gfx_PacketNext);
    s.sprite = Id(Sprite_Current);
    s.field_state = Id(Field_State);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_prim, s.prim, sizeof g_prim);
    std::memcpy(g_text, s.text, sizeof g_text);
    std::memcpy(g_fstate, s.fstate, sizeof g_fstate);
    std::memcpy(g_window, s.window, sizeof g_window);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
    g_load_calls = 0;
    g_frame_calls = 0;
}
unsigned Byte(const State& s, std::uint32_t address) {
    unsigned at = 0;
    for (const Region& r : kRegions) {
        if (address >= r.at && address < r.at + r.size) return s.memory[at + (address - r.at)];
        at += r.size;
    }
    return 0x100;
}

std::uint32_t g_rng = 0x6A09E667u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
bool Half() { return (Next() & 1) != 0; }
unsigned g_place;   // the step hook's place this round
std::uint32_t Garbage(std::uint32_t low_bits, std::uint32_t value) { return (Next() & ~low_bits) | value; }

// Random bytes put back inside what the pointers may point at.
void Fix() {
    Sprite_Current = Object(Next() % kObjectCount);
    Field_State = g_fstate[Next() % 2];
    Gfx_PacketNext = g_prim + Next() % 0xC0;
    SetPtr(at::kWindow, g_window);
    g_text[kText - 1] = 0;
}

struct Args { std::uint32_t a[5]; };

// Each branch's boundaries, on top of the random bytes.
Args Seed(unsigned k) {
    Args args;
    for (std::uint32_t& v : args.a) v = Next();
    if (Often()) MoveScript_WaitWordDA = 0;
    switch (k) {
    case kLookEnd:
    case kLookReturn:
    case kLookPad: {
        static const std::uint16_t kYaws[] = {0xFD56, 0xFD55, 0xFD57, 0xFD6C, 0xFD40, 0xFE39, 0xFE38, 0xFE3A, 0x8000, 0x7FFF, 0};
        static const std::uint16_t kPitches[] = {0x200, 0x1FF, 0x201, 0x216, 0x1EA, 0x355, 0x354, 0x356, 0xAA, 0xA9, 0xAB, 0x8000, 0x7FFF};
        if (Often()) SetWord(At(0x929EC8), kYaws[Next() % 11]);
        if (Often()) SetWord(At(0x929ECC), kPitches[Next() % 13]);
        if (Half()) At(at::kLookTurn)[0] = 0;
        if (k == kLookPad) {
            if (Half()) Input_Held = static_cast<unsigned short>(Input_Held & ~Word(At(at::kLookButton)));
            else if (Half()) Input_Held = static_cast<unsigned short>(Input_Held | (Next() & 0xF000));
        }
        break;
    }
    case kShop: Game_Step = static_cast<unsigned short>(Next() % 3); break;
    case kOpen: {
        const unsigned pick = Next() % 4;
        At(at::kShopObject)[0] = static_cast<unsigned char>(pick == 0 ? 0x80 | Next() : Next() % kObjectCount);
        unsigned char* const o = Object(At(at::kShopObject)[0] % kObjectCount);
        if (Often()) SetLong(o + 0x18, Half() ? 9 : static_cast<std::int32_t>(Next() % 12));
        if (Half()) o[7] = static_cast<unsigned char>(o[7] & ~8u);
        break;
    }
    case kClose: break;
    case kMenuWait:
        args.a[0] = Garbage(0xFF, Half() ? 0 : Next() & 0xFF);
        break;
    case kChoice:
        At(at::kChoiceId)[0] = static_cast<unsigned char>(0x80 + Next() % 16);
        break;
    case kViaCommit:
        At(at::kChoiceId)[0] = static_cast<unsigned char>(0x80 + Next() % 17);
        if (Next() % 3 == 0) At(at::kChoiceId)[0] = 0x90;
        if (Half()) SetWord(At(0x7DEE48), 0xFFFF);
        break;
    case kSmall: {
        // Codes: the NUL, the newline, the blank, two-byte codes, glyphs.
        static const unsigned char kCodes[] = {0, 1, 0x20, 0x80, 0xFF, 0x81, 0x26, 0x27, 0x25, 0x7F, 2, 0x21, 0x41, 0xC3};
        for (unsigned i = 0; i < kText - 1; ++i)
            g_text[i] = Next() % 3 ? kCodes[Next() % 14] : static_cast<unsigned char>(Next());
        if (Next() % 8 == 0) g_text[0] = 0;
        for (unsigned i = 0; i < kText - 1; ++i) if (g_text[i] == 0 && Often()) g_text[i] = 0x41;
        g_text[20 + Next() % (kText - 21)] = 0;
        static const unsigned kCounts[] = {0, 1, 2, 3, 4, 7, 8, 0xFF, 0x100, 0x101};
        args.a[3] = Often() ? Garbage(0xFF, kCounts[Next() % 10] & 0xFF) : Next();
        args.a[4] = Address(g_text + (Next() % 4 == 0 ? Next() % 8 : 0));
        break;
    }
    case kState2: Sprite_Current[2] = static_cast<unsigned char>(Next() % 9); break;
    case kFade: Sprite_Current[3] = static_cast<unsigned char>(Next() % 2); break;
    case kFrame01: At(at::kScena01State)[0] = static_cast<unsigned char>(Next() % 3); break;
    case kEnter:
    case kHook: {
        static const std::uint16_t kAreas[] = {0, 5, 7, 8, 9, 0xA, 0xD, 0xE, 0x10, 0x11, 0x13, 0x16, 0x17, 0x21, 0x60, 0x4, 0x100};
        if (Often()) Game_AreaNumber = kAreas[Next() % 17];
        if (Often()) At(at::kCounters + 2)[0] = static_cast<unsigned char>(Next() % 10);
        if (Half()) Cond_ByteFD = 1;
        if (k == kHook) {
            // One of the hook's thirteen places, x and z each on a bound, one
            // either side of it, inside, or anything; its area most rounds.
            struct Place { std::uint16_t area; std::int32_t x0, x1, z0, z1, x2, z2; };
            static const Place kPlaces[] = {
                {0xA, 0x698000, 0x698000, 0x60000, 0x7FFFF, 0, 0},        // z's high word 6 or 7
                {7, 0x10000, 0x10000, 0x130000, 0x158000, 0x8000, 0},
                {7, 0x150000, 0x168000, 0x10000, 0x10000, 0, 0x8000},
                {7, 0x270000, 0x270000, 0x180000, 0x198000, 0x278000, 0},
                {8, 0x548000, 0x548000, 0x1C0000, 0x1DFFFF, 0, 0},        // z's high word 0x1C or 0x1D
                {8, 0x558000, 0x568000, 0x1C0000, 0x1D8000, 0, 0},
                {0x13, 0x1A0000, 0x1C8000, 0x1A0000, 0x1D8000, 0, 0},
                {0x13, 0x160000, 0x160000, 0xC0000, 0xD8000, 0, 0},
                {0x17, 0x480000, 0x498000, 0xC8000, 0xD8000, 0, 0},
                {0x16, 0x510000, 0x520000, 0x1F8000, 0x200000, 0, 0},
                {0x16, 0x28000, 0x28000, 0x70000, 0x88000, 0x300000, 0},
                {0x16, 0x3D0000, 0x3D0000, 0xA0000, 0xB8000, 0, 0},
                {0x60, 0x10000, 0x10000, 0x110000, 0x128000, 0x18000, 0},
            };
            const Place& p = kPlaces[Next() % 13];
            g_place = static_cast<unsigned>(&p - kPlaces);
            if (Often()) Game_AreaNumber = p.area;
            const auto pick = [](std::int32_t lo, std::int32_t hi, std::int32_t other) {
                switch (Next() % 8) {
                case 0: return lo - 1;
                case 1: return hi + 1;
                case 2: return static_cast<std::int32_t>(Next());
                case 3: return other ? other : lo;
                case 4: return lo + (hi - lo) / 2;
                case 5: return other ? other : hi;
                case 6: return hi;
                default: return lo;
                }
            };
            args.a[0] = static_cast<std::uint32_t>(pick(p.x0, p.x1, p.x2));
            args.a[1] = static_cast<std::uint32_t>(pick(p.z0, p.z1, p.z2));
        }
        break;
    }
    case kCursor:
        At(at::kCursorSet)[0] = static_cast<unsigned char>(Next() % 8);
        break;
    default:
        break;
    }
    return args;
}

// What the rounds reached, from the original's side.
struct Coverage {
    unsigned logged[100];
    unsigned hook_hits, hook_places[13], hook_areas[8], enter_areas[16], small_texts, small_returned_nul, commit_twice, open_back;
} g_cover;
void Cover(unsigned k, const State& in, const State& out) {
    for (unsigned i = 0; i < out.log_n && i < kLog; ++i)
        if (out.log[i].what < 100) ++g_cover.logged[out.log[i].what];
    switch (k) {
    case kHook:
        if (out.result & 0xFF) {
            ++g_cover.hook_hits;
            ++g_cover.hook_places[g_place];
            static const unsigned kAreas[] = {0xA, 7, 8, 0x13, 0x17, 0x16};
            const unsigned area = Byte(in, 0x904EFC) | Byte(in, 0x904EFD) << 8;
            for (unsigned j = 0; j < 6; ++j) if (area == kAreas[j]) ++g_cover.hook_areas[j];
        }
        break;
    case kEnter: {
        static const unsigned kAreas[] = {0, 5, 7, 8, 9, 0xA, 0xD, 0xE, 0x10, 0x11, 0x16, 0x17, 0x21};
        const unsigned area = Byte(in, 0x904EFC) | Byte(in, 0x904EFD) << 8;
        for (unsigned j = 0; j < 13; ++j) if (area == kAreas[j]) ++g_cover.enter_areas[j];
        break;
    }
    case kSmall: {
        unsigned glyphs = 0;
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) glyphs += out.log[i].what == 8;
        if (glyphs) ++g_cover.small_texts;
        break;
    }
    case kOpen:
        if ((Byte(out, 0x66C7E8) | Byte(out, 0x66C7E9) << 8) == 2 && out.log_n && out.log[out.log_n - 1].what == 22)
            ++g_cover.open_back;
        break;
    case kViaCommit:
        if (Byte(in, at::kChoiceId) == 0x90) ++g_cover.commit_twice;
        break;
    default:
        break;
    }
}

using Fn5 = std::uint32_t (__cdecl*)(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t);

}  // namespace

void SelfTest() {
    constexpr unsigned kPerKind = 1500;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes)
        bof3::Fatal("mode_states: the regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);

    void* clones[kCount];
    for (unsigned k = 0; k < kCount; ++k) {
        const Clone& c = kClones[k];
        static bof3::CloneCall calls[48];
        if (c.n_calls > 48) bof3::Fatal("mode_states: %s has %d calls", c.name, c.n_calls);
        for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target), c.calls[i].target};
        clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
    }
    for (const move_script::Table& t : kEnterTables) move_script::Relocate(clones[kEnter], kClones[kEnter].base, kClones[kEnter].size, t);
    {
        auto* code = static_cast<std::uint8_t*>(clones[kChoice]);
        for (unsigned i = 0; i < 16; ++i) {
            std::uint32_t had;
            std::memcpy(&had, code + kChoiceImm(i), sizeof had);
            if (had != kChoiceHandlers[i])
                bof3::Fatal("mode_states: MsgBox_SystemChoice +0x%X holds 0x%X, not the handler 0x%X",
                            static_cast<unsigned>(kChoiceImm(i)), static_cast<unsigned>(had), static_cast<unsigned>(kChoiceHandlers[i]));
            const std::uint32_t to = Address(reinterpret_cast<const void*>(kChoiceFns[i]));
            std::memcpy(code + kChoiceImm(i), &to, sizeof to);
        }
    }
    // The commits' copies: [side][commit], side 0 calling the choice's copy,
    // side 1 ours.
    void* commits[2][2];
    for (unsigned side = 0; side < 2; ++side)
        for (unsigned c = 0; c < 2; ++c)
            commits[side][c] = Thunk(side == 0 ? clones[kChoice] : reinterpret_cast<const void*>(&MsgBox_SystemChoice), kTails[c]);

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&GameMode_LookEnd), reinterpret_cast<const void*>(&GameMode_Shop),
        reinterpret_cast<const void*>(&Shop_Open), reinterpret_cast<const void*>(&Shop_Close),
        reinterpret_cast<const void*>(&Menu_WaitTransition), reinterpret_cast<const void*>(&Look_PadControl),
        reinterpret_cast<const void*>(&Look_Return), reinterpret_cast<const void*>(&MsgBox_SystemChoice),
        reinterpret_cast<const void*>(&Text_DrawSmall), reinterpret_cast<const void*>(&Field_LoadingFrame),
        reinterpret_cast<const void*>(&Menu_Frame), reinterpret_cast<const void*>(&Shop_Frame),
        reinterpret_cast<const void*>(&FieldCore_State2), reinterpret_cast<const void*>(&FieldCore_Fade),
        reinterpret_cast<const void*>(&FieldCore_FadeBegin), reinterpret_cast<const void*>(&FieldCore_FadeStep),
        reinterpret_cast<const void*>(&Scena01_Frame), reinterpret_cast<const void*>(&Scena01_EnterArea),
        reinterpret_cast<const void*>(&Scena01_StepHook), reinterpret_cast<const void*>(&Window_DrawCursor)};

    static State saved, input, their_out, our_out;
    unsigned char* const saved_packet = Gfx_PacketNext;
    Capture(saved);
    SwapTables();
    g = kStubs;
    g.tails[0] = Address(commits[1][0]) + 5;
    g.tails[1] = Address(commits[1][1]) + 5;

    // The chapter's two long functions get five times the rounds.
    unsigned schedule[kKinds + 8], n_schedule = 0;
    for (unsigned k = 0; k < kKinds; ++k) schedule[n_schedule++] = k;
    for (unsigned i = 0; i < 4; ++i) { schedule[n_schedule++] = kEnter; schedule[n_schedule++] = kHook; }
    unsigned bad = 0, calls = 0, rounds = 0, bad_per[kKinds] = {};
    for (unsigned round = 0; round < kPerKind * n_schedule; ++round) {
        const unsigned k = schedule[round % n_schedule];
        ++rounds;
        for (unsigned i = 0; i < kRegionBytes; i += 4) {
            const std::uint32_t v = Next();
            std::memcpy(input.memory + i, &v, kRegionBytes - i < 4 ? kRegionBytes - i : 4);
        }
        for (unsigned char& b : input.prim) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.text) b = static_cast<unsigned char>(Next());
        for (auto& f : input.fstate) for (unsigned char& b : f) b = static_cast<unsigned char>(Next());
        for (unsigned char& b : input.window) b = static_cast<unsigned char>(Next());
        Apply(input);
        Fix();
        g_seed = Next();
        g_load_limit = Next() % 4;
        g_frame_limit = Next() % 4;
        const Args args = Seed(k);
        Capture(input);

        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            State& out = pass ? our_out : their_out;
            const void* fn;
            if (k == kViaCommit) fn = commits[pass][round / n_schedule % 2];
            else fn = pass ? ours[k] : clones[k];
            const std::uint32_t r = reinterpret_cast<Fn5>(const_cast<void*>(fn))(args.a[0], args.a[1], args.a[2], args.a[3], args.a[4]);
            Capture(out);
            const unsigned ret = k < kCount ? kClones[k].ret : 0;
            out.result = ret == 0 ? 0u : ret == 1 ? (r & 0xFFu) : Id(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(r)));
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0) {
            ++bad_per[k];
            if (++bad <= 12) {
                unsigned first = 0;
                while (first < sizeof their_out &&
                       reinterpret_cast<const unsigned char*>(&their_out)[first] == reinterpret_cast<const unsigned char*>(&our_out)[first])
                    ++first;
                bof3::Log("shadow      mode_states self-test MISMATCH: round %u, %s, log %u / %u, result 0x%X / 0x%X, "
                          "first differing state byte %u",
                          round, k < kCount ? kClones[k].name : "the commits", their_out.log_n, our_out.log_n,
                          their_out.result, our_out.result, first);
            }
        }
    }
    g = kOriginals;
    RestoreTables();
    Apply(saved);
    Gfx_PacketNext = saved_packet;

    bof3::Log("shadow      mode_states self-test: %u rounds over %u functions and the commits (%u rounds of each, five times that for the chapter's two), %u calls to the "
              "stand-ins, %u MISMATCHES; the message box, objects, chapter bytes, camera, effects, input, counters, "
              "flags, area, menu block and task bytes, the two constant tables, the primitive pool, text, Field_State "
              "and window buffers, the three pointers, the result and the stand-ins' log compared",
              rounds, static_cast<unsigned>(kCount), kPerKind, calls, bad);
    for (unsigned k = 0; k < kKinds; ++k)
        if (bad_per[k]) bof3::Log("shadow      mode_states: %s mismatched in %u rounds", k < kCount ? kClones[k].name : "the commits", bad_per[k]);
    const Coverage& c = g_cover;
    unsigned slots = 0, choices = 0;
    for (unsigned i = 40; i < 40 + kSlotCount - 3; ++i) slots += c.logged[i] ? 1u : 0u;
    for (unsigned i = 70; i < 86; ++i) choices += c.logged[i] ? 1u : 0u;
    bof3::Log("shadow      mode_states coverage: table slots %u of 17, choices %u of 16, id 0x90 through a commit %u; "
              "step hook hits %u (places %u %u %u %u %u %u %u %u %u %u %u %u; areas 0xA %u, 7 %u, 8 %u, 0x13 %u, 0x17 %u, 0x16 %u), area changes %u, sounds %u, "
              "flags set %u; area entries 0 %u, 5 %u, 7 %u, 8 %u, 9 %u, 0xA %u, 0xD %u, 0xE %u, 0x10 %u, 0x11 %u, 0x16 %u, "
              "0x17 %u, 0x21 %u; effects found %u, music loads %u, stops %u, call A %u, test FB %u; small texts drawn %u "
              "(glyphs %u); shop back to the field %u, faced %u, loads %u, bank files %u; hands %u; fade steps %u",
              slots, choices, c.commit_twice, c.hook_hits, c.hook_places[0], c.hook_places[1], c.hook_places[2],
              c.hook_places[3], c.hook_places[4], c.hook_places[5], c.hook_places[6], c.hook_places[7], c.hook_places[8],
              c.hook_places[9], c.hook_places[10], c.hook_places[11], c.hook_areas[0], c.hook_areas[1], c.hook_areas[2],
              c.hook_areas[3], c.hook_areas[4], c.hook_areas[5], c.logged[18], c.logged[14], c.logged[11],
              c.enter_areas[0], c.enter_areas[1], c.enter_areas[2], c.enter_areas[3], c.enter_areas[4], c.enter_areas[5],
              c.enter_areas[6], c.enter_areas[7], c.enter_areas[8], c.enter_areas[9], c.enter_areas[10], c.enter_areas[11],
              c.enter_areas[12], c.logged[15], c.logged[12], c.logged[13], c.logged[16], c.logged[17], c.small_texts,
              c.logged[8], c.open_back, c.logged[5], c.logged[3], c.logged[6], c.logged[19], c.logged[9]);
    if (bad) bof3::Fatal("the top-level modes and the field core differ from the original in %u self-test rounds", bad);
}

}  // namespace mode_states
