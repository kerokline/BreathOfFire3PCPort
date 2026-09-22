// The top-level mode tasks. Task 1's title sequence - its body 0x4621C0 and
// the three things it runs every frame around its state handler (0x462420,
// 0x462600, 0x462740), with the sprite helper 0x462560 under them - and the
// field task's mode-2 handler 0x4959F0, which turns Field_Request into the
// next Game_Mode. docs/mode-tasks.md.
#include "game/mode_tasks.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// Two callees another group is taking over in parallel (the field frame
// 0x517200 is group A's, and group A's 0x517240 calls the event dispatcher
// 0x56D690 too); named in symbols.toml by whoever merges last, called here
// by address so that the two tables cannot bind one address twice.
constexpr std::uint32_t kEventDispatch = 0x56D690;   // call [[0x662C80 + s8 0x8034E0 * 4]], jmp 0x56D8B0
constexpr std::uint32_t kFieldFrame = 0x517200;      // the field's frame: objects, map, draw

// GameMode_Field's bytes, unnamed (docs/mode-tasks.md section 3 has the PSX
// twin of each).
constexpr std::uint32_t kBattleCount = 0x904134;     // dword, +1 entering mode 5 (PSX 0x80145028)
constexpr std::uint32_t kBattleBytes = 0x904AA0;     // three bytes zeroed entering mode 5 (PSX 0x801462DC)
constexpr std::uint32_t kMemberByte0 = 0x802D4B;     // one byte in each of three 0x14C-byte records,
constexpr std::uint32_t kMemberByte1 = 0x802E97;     // zeroed entering mode 5 (PSX 0x80145E97,
constexpr std::uint32_t kMemberByte2 = 0x802FE3;     // 0x80145FD7, 0x80146117 - 0x140 apart there)
constexpr std::uint32_t kFieldFlags = 0x905BA4;      // byte |= 0x40 on the area change (PSX 0x80146256)
constexpr std::uint32_t kMusicTrack = 0x904131;      // the track Music_Play last started, 0xFF none
constexpr std::uint32_t kAreaTrack = 0x904CD0;       // the track the next area wants (PSX 0x80143F1F)
constexpr std::uint32_t kAreaTransition = 0x904EE0; // Transition_Start's kind for the area change, 0xFF none
constexpr std::uint32_t kMenuBlock = 0x929F00;       // the menu's state block (docs/menu-screens.md)
constexpr std::uint32_t kMode11Bytes = 0x905DA0;     // five bytes set entering mode 11 (PSX 0x80146268)

// Every callee, through pointers so that the start-up fuzz can stand
// recording functions in for them - for the originals' copies and for ours
// alike. Four of them are ours (Title_CheckStart, Title_DrawBackdrop,
// Title_DrawLogo and Title_Sprite), called through here all the same so that
// each function is tested alone; Gpu_SetSprt, Gpu_SetSemiTrans,
// Gpu_SetDrawMode and Gfx_CommitPrim are other files'.
struct Callees {
    void (__cdecl* clear_private)();
    void (__cdecl* sleep)(int);
    void (__cdecl* check_start)();
    void (__cdecl* backdrop)();
    void (__cdecl* logo)();
    int (__cdecl* load_done)();
    void (__cdecl* transition)(unsigned char);
    void (__cdecl* fade_music)();
    void (__cdecl* effect)(unsigned short);
    void (__cdecl* draw_mode)(unsigned char*, int, int, unsigned, unsigned long);
    void (__cdecl* commit)(unsigned, unsigned);
    unsigned char* (__cdecl* sprite)(int, int, unsigned, unsigned, unsigned);
    void (__cdecl* set_sprt)(unsigned char*);
    void (__cdecl* semi_trans)(unsigned char*, unsigned);
    void (__cdecl* set_a)(unsigned char, unsigned char);
    void (__cdecl* set_b)(int, int, unsigned char, unsigned char);
    void (__cdecl* set_c)(int, int, unsigned char, unsigned char);
    void (__cdecl* event_dispatch)();
    void (__cdecl* field_frame)();
    void (__cdecl* wait_transition)(unsigned char);
    void (__cdecl* music_fade)(int);
    void (__cdecl* music_stop)(int);
};
const Callees kOriginals = {
    Task_ClearPrivate, Task_Sleep, Title_CheckStart, Title_DrawBackdrop, Title_DrawLogo, File_LoadDone,
    Transition_Start, Title_FadeMusic, Sound_PlayEffect, Gpu_SetDrawMode, Gfx_CommitPrim, Title_Sprite,
    Gpu_SetSprt, Gpu_SetSemiTrans, Title_DrawSetA, Title_DrawSetB, Title_DrawSetC,
    reinterpret_cast<void (__cdecl*)()>(kEventDispatch), reinterpret_cast<void (__cdecl*)()>(kFieldFrame),
    Field_WaitTransition, Music_FadeOut, Music_FadeOutStop,
};
Callees g = kOriginals;

// Task 0's state word, +0 of its record: 0 while no task 0 runs.
std::uint16_t TaskZeroState() { return Word(At(bof3::addr::Task_Records)); }

}  // namespace

// original 0x4621C0: task 1's body once 0x496C90 has loaded DAT 0x226 and
// jumped here (PSX GAME.EMI section 1, FUN_801D0C04). A coroutine: it runs
// on task 1's own stack and never returns - Task_Sleep yields to the
// scheduler and comes back a frame later, and state 7's handler ends the task
// from inside the call below (a tail jump to 0x5A99AD), leaving this frame
// behind on a stack the next Task_Create(1, ..) starts afresh.
//
// As the original has it: the state word indexes Title_States unchecked,
// read afresh after Title_CheckStart (which may have moved it).
extern "C" void __cdecl Title_Task(void) {
    Title_State = 0;
    Title_Step = 0;
    g.clear_private();
    for (;;) {
        g.sleep(1);
        g.check_start();
        reinterpret_cast<void (__cdecl*)()>(Title_States[Title_State])();
        g.backdrop();
        g.logo();
    }
}

// original 0x462420: Start on the title (PSX FUN_801D104C). Pressed while the
// backdrop still fades in or holds (states 0..2), it cuts the hold short:
// the timer to 1, state 2 - the next frame's state-2 handler ends it. Pressed
// once the logo is up (state 3 or later, the backdrop's fade done, the logo's
// state 2), it leaves for the title menu: state 7.
//
// As the original has it: the checks in this order, each a return -
// File_LoadDone (always 1 on the PC) is asked only once Start is down; task 0
// running (the demo) makes Start this function's no-op, the demo's own state
// handler reads it instead. The state test is "at most 2" (the PSX's "below
// 3"). The original pushes Transition_Start's 0 and leaves it on the stack
// under Sound_PlayEffect's 0x105 until one `add esp, 8` - a stray second
// argument Sound_PlayEffect never reads (it reads [esp + 4] only).
extern "C" void __cdecl Title_CheckStart(void) {
    if (Title_StartArmed == 0) return;
    if (!(Input_Pressed & 0x800)) return;
    if (g.load_done() == 0) return;
    if (MoveScript_WaitWordDA != 0) return;
    if (TaskZeroState() != 0) return;
    if (Title_State <= 2) {
        Title_Timer = 1;
        Title_State = 2;
        return;
    }
    if (Title_Fade != 0) return;
    if (Title_LogoState != 2) return;
    g.transition(0);
    g.fade_music();
    g.effect(0x105);
    Title_StartArmed = 0;
    Title_State = 7;
}

// original 0x462560: one title sprite (PSX FUN_801D1760): a SPRT at the
// packet cursor, entry `index` of Title_Sprites for its texture, committed to
// OT slot `slot`. Returns the primitive, for the caller to shade.
//
// As the original has it: x and y are taken as s16 and converted exactly to
// floats (the PC's primitive holds floats where the PSX's held s16); index
// and semi are taken as bytes, slot whole (Gfx_CommitPrim masks it itself);
// the clut word is entry +8 shifted left 6, into 16 bits. The primitive is
// the cursor as it was on entry, whatever the commit did to it. The original
// also writes its converted x and y back over its own argument slots - dead
// stores no caller reads (all five callers only pop them).
extern "C" unsigned char* __cdecl Title_Sprite(int x, int y, unsigned index, unsigned slot, unsigned semi) {
    unsigned char* const prim = Gfx_PacketNext;
    g.set_sprt(prim);
    g.semi_trans(prim, semi & 0xFF);
    prim[4] = 0x80;
    prim[5] = 0x80;
    prim[6] = 0x80;
    const float fx = static_cast<float>(static_cast<short>(x));
    const float fy = static_cast<float>(static_cast<short>(y));
    std::memcpy(prim + 8, &fx, sizeof fx);
    std::memcpy(prim + 0xC, &fy, sizeof fy);
    const unsigned char* const entry = Title_Sprites + (index & 0xFF) * 10u;
    prim[0x14] = entry[0];
    prim[0x15] = entry[2];
    SetWord(prim + 0x18, Word(entry + 4));
    SetWord(prim + 0x1A, Word(entry + 6));
    SetWord(prim + 0x16, static_cast<unsigned>(Word(entry + 8)) << 6);
    g.commit(slot, 0x1C);
    return prim;
}

// original 0x462600: the title's backdrop (PSX FUN_801D1880): four columns
// 255 wide, from texture pages 5, 7, 9 and 11, sliding left two pixels a
// frame while Title_Fade is set, coloured by Title_Bright.
//
// As the original has it:
//   - a column is skipped while scroll / 640 (s16, truncated) exceeds its
//     index - the scroll re-read for every column;
//   - the fade steps once per column DRAWN, not once per frame: up to four
//     steps a frame, fewer as columns slide out;
//   - fade 1 raises the brightness, and on reaching 0x80 (s16 compare) sets
//     0x80 and fade 2 - the column is still drawn, and the brightness kept in
//     a register is not clamped, only the stored word;
//   - fade 3 lowers it; at 0 or below (s16) both words go to 0 and the column
//     is not drawn - and every later column that frame then takes the "any
//     other fade" branch, which stores 0x80 into the brightness it just
//     zeroed before seeing fade 0 and skipping;
//   - the fade and the brightness are read back from memory after each
//     column is drawn; the colour bytes are the stored word's low byte.
extern "C" void __cdecl Title_DrawBackdrop(void) {
    unsigned char fade = Title_Fade;
    if (fade == 0) return;
    const auto scroll = static_cast<std::uint16_t>(Title_Scroll + 2);
    const auto left = static_cast<std::uint16_t>(0x140 - scroll);
    Title_Scroll = scroll;
    auto bright = static_cast<std::uint16_t>(Title_Bright);
    unsigned column = 0;
    for (unsigned page = 0x140; page < 0x340; page += 0x80, ++column) {
        if (static_cast<short>(Title_Scroll) / 640 > static_cast<int>(column)) continue;
        if (fade == 1) {
            ++bright;
            Title_Bright = bright;
            if (static_cast<short>(bright) >= 0x80) {
                Title_Bright = 0x80;
                Title_Fade = 2;
            }
        } else if (fade == 3) {
            --bright;
            Title_Bright = bright;
            if (static_cast<short>(bright) <= 0) {
                bright = 0;
                fade = 0;
                Title_Bright = 0;
                Title_Fade = 0;
                continue;
            }
        } else {
            bright = 0x80;
            Title_Bright = 0x80;
            if (fade == 0) continue;
        }
        g.draw_mode(Gfx_PacketNext, 0, 0, ((page & 0x3C0) | 0x2000) >> 6, 0);
        g.commit(2, 0xC);
        unsigned char* const prim = g.sprite(static_cast<int>(column * 255 + left), 0x18, column + 0xB, 2, 0);
        prim[4] = static_cast<unsigned char>(Title_Bright);
        prim[5] = static_cast<unsigned char>(Title_Bright);
        prim[6] = static_cast<unsigned char>(Title_Bright);
        bright = Title_Bright;
        fade = Title_Fade;
    }
}

// original 0x462740: the logo (PSX FUN_801D1A88). Title_LogoState 1 fades
// two shades in - A by 4 a frame, B by 2 - each clamped at 0x80 with its
// flag cleared, and state 2 once both are; the flags are the three sets'
// semi-transparency while fading.
//
// As the original has it: a state other than 0, 1 and 2 draws with both
// flags set and moves nothing; the clamps are s16 compares; each shade is
// passed as the byte read from memory after the previous set was drawn.
extern "C" void __cdecl Title_DrawLogo(void) {
    unsigned char flag_a = 1, flag_b = 1;
    const unsigned char state = Title_LogoState;
    if (state == 1) {
        Title_LogoShadeA = static_cast<std::uint16_t>(Title_LogoShadeA + 4);
        if (static_cast<short>(Title_LogoShadeA) >= 0x80) {
            flag_a = 0;
            Title_LogoShadeA = 0x80;
        }
        Title_LogoShadeB = static_cast<std::uint16_t>(Title_LogoShadeB + 2);
        if (static_cast<short>(Title_LogoShadeB) >= 0x80) {
            flag_b = 0;
            Title_LogoShadeB = 0x80;
        }
        if (flag_a == 0 && flag_b == 0) Title_LogoState = 2;
    } else if (state == 2) {
        flag_a = 0;
        flag_b = 0;
        Title_LogoShadeA = 0x80;
        Title_LogoShadeB = 0x80;
    } else if (state == 0) {
        return;
    }
    g.set_a(flag_b, static_cast<unsigned char>(Title_LogoShadeB));
    g.set_b(0x1A, 0x18, flag_b, static_cast<unsigned char>(Title_LogoShadeB));
    g.set_c(-6, 0x1C, flag_a, static_cast<unsigned char>(Title_LogoShadeA));
}

// original 0x4959F0: Game_Mode 2, the field (PSX GAME.EMI section 0,
// FUN_80198378): the event script and the field's frame, then whatever
// Field_Request asks for becomes the next mode.
//
// As the original has it: Field_Request is read (as a byte) only after the
// two calls, which may set it; 0 and anything above 9 change nothing. On the
// area change (5) the music track byte is read again after each call that
// may have changed it, and Transition_Start gets the area's kind as a byte
// (the original passes ecx with whatever its upper bytes held; the callee
// reads the byte). The PSX's case 5 also started the next area's music; the
// port fades instead (docs/mode-tasks.md section 3).
extern "C" void __cdecl GameMode_Field(void) {
    g.event_dispatch();
    g.field_frame();
    switch (Field_Request) {
    case 1:   // the menu
        g.transition(2);
        g.wait_transition(0);
        Game_Step = 0;
        Game_Mode = 3;
        return;
    case 2:
        Game_Mode = 4;
        return;
    case 3: {   // battle
        const std::int32_t count = Long(At(kBattleCount));
        Game_Mode = 5;
        At(kBattleBytes)[0] = 0;
        At(kBattleBytes)[1] = 0;
        At(kBattleBytes)[2] = 0;
        At(kMemberByte0)[0] = 0;
        At(kMemberByte1)[0] = 0;
        At(kMemberByte2)[0] = 0;
        Game_Step = 0;
        SetLong(At(kBattleCount), static_cast<std::int32_t>(static_cast<std::uint32_t>(count) + 1u));
        return;
    }
    case 4:
        Game_Mode = 6;
        return;
    case 5: {   // the area change
        unsigned char track = At(kMusicTrack)[0];
        At(kFieldFlags)[0] |= 0x40;
        if (track != 0xFF && At(kAreaTrack)[0] != track) {
            g.music_fade(0x10);
            track = At(kMusicTrack)[0];
        }
        const unsigned char kind = At(kAreaTransition)[0];
        if (kind != 0xFF) {
            g.transition(kind);
            g.wait_transition(0);
            track = At(kMusicTrack)[0];
        }
        if (track != 0xFF && At(kAreaTrack)[0] != track) g.music_stop(0xA);
        Game_Mode = 1;
        return;
    }
    case 6:
        At(kMenuBlock)[0] = 6;
        At(kMenuBlock)[1] = 0;
        At(kMenuBlock)[2] = 0;
        Game_Step = 0;
        At(kMenuBlock)[0xC] = 0xFF;
        Game_Mode = 7;
        return;
    case 7:
    case 8:
        Game_Mode = Field_Request == 7 ? 9 : 0xA;
        Game_Step = 0;
        At(kMenuBlock)[0] = 0;
        At(kMenuBlock)[1] = 0;
        At(kMenuBlock)[2] = 0;
        return;
    case 9:
        At(kMode11Bytes)[0] = 1;
        At(kMode11Bytes)[1] = 0;
        At(kMode11Bytes)[2] = 0;
        At(kMode11Bytes)[3] = 0;
        At(kMode11Bytes)[4] = 0;
        Game_Mode = 0xB;
        return;
    default:
        return;
    }
}

namespace {

// --- BOF3X_SHADOW=mode_tasks: a differential fuzz, once at start-up --------
// Six byte-copies, every call out re-aimed at the recorders below, and for
// the task body the state table swapped for eight recording handlers. One
// round: one of the six, random bytes in every region any of them touches,
// then each branch's boundaries seeded; theirs, then from the same state
// ours; the regions, the primitive buffers, the packet cursor, the return
// and the recorders' log compared. The task body never returns: the
// Task_Sleep recorder ends it by a long jump after one to six frames.

constexpr unsigned kLog = 48;
struct Entry { std::uint32_t what, a, b, c, d; };
Entry g_log[kLog];
unsigned g_log_n, g_seed, g_sleeps, g_sleep_limit;
std::uint32_t g_jump[6];

extern "C" __attribute__((naked, returns_twice)) int __cdecl JumpSave(std::uint32_t* buffer) {
    asm("movl 4(%esp), %eax\n\t"
        "movl %ebx, 0(%eax)\n\t"
        "movl %esi, 4(%eax)\n\t"
        "movl %edi, 8(%eax)\n\t"
        "movl %ebp, 12(%eax)\n\t"
        "leal 4(%esp), %ecx\n\t"
        "movl %ecx, 16(%eax)\n\t"
        "movl (%esp), %ecx\n\t"
        "movl %ecx, 20(%eax)\n\t"
        "xorl %eax, %eax\n\t"
        "ret");
}
extern "C" __attribute__((naked, noreturn)) void __cdecl JumpBack(std::uint32_t* buffer) {
    asm("movl 4(%esp), %eax\n\t"
        "movl 0(%eax), %ebx\n\t"
        "movl 4(%eax), %esi\n\t"
        "movl 8(%eax), %edi\n\t"
        "movl 12(%eax), %ebp\n\t"
        "movl 16(%eax), %esp\n\t"
        "movl 20(%eax), %ecx\n\t"
        "movl $1, %eax\n\t"
        "jmp *%ecx");
}
unsigned char g_prim[0x100], g_sprite_out[0x80];

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0, std::uint32_t c = 0, std::uint32_t d = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, a, b, c, d};
    ++g_log_n;
}
std::uint32_t Id(const void* p) {
    const auto at = reinterpret_cast<std::uintptr_t>(p);
    const auto prim = reinterpret_cast<std::uintptr_t>(g_prim), out = reinterpret_cast<std::uintptr_t>(g_sprite_out);
    if (at >= prim && at < prim + sizeof g_prim) return 0x1000 + static_cast<std::uint32_t>(at - prim);
    if (at >= out && at < out + sizeof g_sprite_out) return 0x2000 + static_cast<std::uint32_t>(at - out);
    return static_cast<std::uint32_t>(at);
}

// A callee may change what its caller reads again after it: the title's
// words (the state kept below 8, as the fuzz's table holds eight), and the
// field handler's request, music track and area bytes.
void DisturbTitle() {
    const std::uint32_t h = Hash();
    if (h % 5 == 0) Title_State = static_cast<std::uint16_t>((h >> 8) % 8);
    if (h % 7 == 0) Title_Fade = static_cast<unsigned char>((h >> 11) % 5);
    if (h % 6 == 0) Title_Bright = static_cast<std::uint16_t>(h % 3 ? 0x7E + (h >> 13) % 4 : h >> 16);
    if (h % 9 == 0) Title_Scroll = static_cast<std::uint16_t>(h >> 12);
    if (h % 4 == 0) Title_LogoShadeA = static_cast<std::uint16_t>(h >> 17);
    if (h % 3 == 0) Title_LogoShadeB = static_cast<std::uint16_t>(h >> 5);
}
void DisturbField() {
    const std::uint32_t h = Hash();
    if (h % 3 == 0) Field_Request = static_cast<unsigned char>(h % 7 ? (h >> 8) % 11 : h >> 8);
    if (h % 4 == 0) At(kMusicTrack)[0] = static_cast<unsigned char>(h % 8 ? h >> 16 : 0xFF);
    if (h % 5 == 0) At(kAreaTrack)[0] = static_cast<unsigned char>(h % 2 ? At(kMusicTrack)[0] : h >> 20);
    if (h % 6 == 0) At(kAreaTransition)[0] = static_cast<unsigned char>(h >> 24);
}

void __cdecl StubClearPrivate() { Record(1); DisturbTitle(); }
void __cdecl StubSleep(int frames) {
    Record(2, static_cast<std::uint32_t>(frames));
    if (++g_sleeps >= g_sleep_limit) JumpBack(g_jump);
    DisturbTitle();
}
void __cdecl StubCheckStart() { Record(3, Title_State); DisturbTitle(); }
void __cdecl StubBackdrop() { Record(4, Title_State); DisturbTitle(); }
void __cdecl StubLogo() { Record(5, Title_State); DisturbTitle(); }
int __cdecl StubLoadDone() { Record(6); return Hash() % 3 ? 1 : 0; }
void __cdecl StubTransition(unsigned char kind) { Record(7, kind); DisturbField(); }
void __cdecl StubFadeMusic() { Record(8); }
void __cdecl StubEffect(unsigned short id) { Record(9, id); }
// The state handlers: which one ran, at which state.
template <unsigned N> void __cdecl StubHandler() { Record(10 + N, Title_State); DisturbTitle(); }
void __cdecl StubDrawMode(unsigned char* prim, int dfe, int dtd, unsigned tpage, unsigned long tw) {
    Record(20, Id(prim), static_cast<std::uint32_t>(dfe), static_cast<std::uint32_t>(dtd), tpage ^ static_cast<std::uint32_t>(tw) << 16);
    std::memset(prim + 4, static_cast<int>(Hash() & 0xFF), 8);
    DisturbTitle();
}
// The commit moves the packet cursor, as the real one does - inside the
// buffer - so that a sprite that returned the cursor after it would show.
void __cdecl StubCommit(unsigned slot, unsigned size) {
    Record(21, slot, size, Id(Gfx_PacketNext));
    Gfx_PacketNext = g_prim + (static_cast<unsigned>(Gfx_PacketNext - g_prim) + (size & 0xFF)) % 0x80u;
    DisturbTitle();
}
// The callee reads x and y as s16 and index as a byte: that much is logged.
unsigned char* __cdecl StubSprite(int x, int y, unsigned index, unsigned slot, unsigned semi) {
    Record(22, static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y), index & 0xFF, slot ^ semi << 16);
    unsigned char* const out = g_sprite_out + (Hash() % 4) * 0x20;
    std::memset(out, static_cast<int>(Hash() >> 8 & 0xFF), 0x20);
    DisturbTitle();
    return out;
}
// Scribbles over the bytes the caller writes after it, so that a store
// moved before the call would show.
void __cdecl StubSetSprt(unsigned char* prim) {
    Record(23, Id(prim));
    for (unsigned i = 4; i < 0x1C; ++i) prim[i] = static_cast<unsigned char>(Hash() >> (i % 24));
}
void __cdecl StubSemiTrans(unsigned char* prim, unsigned abe) { Record(24, Id(prim), abe); prim[7] ^= 2; }
void __cdecl StubSetA(unsigned char flag, unsigned char shade) { Record(25, flag, shade); DisturbTitle(); }
void __cdecl StubSetB(int x, int y, unsigned char flag, unsigned char shade) {
    Record(26, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), flag, shade);
    DisturbTitle();
}
void __cdecl StubSetC(int x, int y, unsigned char flag, unsigned char shade) {
    Record(27, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), flag, shade);
    DisturbTitle();
}
void __cdecl StubEventDispatch() { Record(30, Field_Request); DisturbField(); }
void __cdecl StubFieldFrame() { Record(31, Field_Request); DisturbField(); }
void __cdecl StubWaitTransition(unsigned char run) { Record(32, run); DisturbField(); }
// The fade may end the track (0xFF) or leave the one the area wants: what
// decides the area change's second fade, read again after it.
void __cdecl StubMusicFade(int frames) {
    Record(33, static_cast<std::uint32_t>(frames));
    const std::uint32_t h = Hash();
    if (h % 3 == 0) At(kMusicTrack)[0] = 0xFF;
    else if (h % 3 == 1) At(kMusicTrack)[0] = At(kAreaTrack)[0];
}
void __cdecl StubMusicStop(int frames) { Record(34, static_cast<std::uint32_t>(frames)); DisturbField(); }

const Callees kStubs = {StubClearPrivate, StubSleep, StubCheckStart, StubBackdrop, StubLogo, StubLoadDone, StubTransition,
                        StubFadeMusic, StubEffect, StubDrawMode, StubCommit, StubSprite, StubSetSprt, StubSemiTrans,
                        StubSetA, StubSetB, StubSetC, StubEventDispatch, StubFieldFrame, StubWaitTransition,
                        StubMusicFade, StubMusicStop};
void* const kHandlers[8] = {reinterpret_cast<void*>(&StubHandler<0>), reinterpret_cast<void*>(&StubHandler<1>),
                            reinterpret_cast<void*>(&StubHandler<2>), reinterpret_cast<void*>(&StubHandler<3>),
                            reinterpret_cast<void*>(&StubHandler<4>), reinterpret_cast<void*>(&StubHandler<5>),
                            reinterpret_cast<void*>(&StubHandler<6>), reinterpret_cast<void*>(&StubHandler<7>)};

const void* StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<const void*>(p); };
    switch (target) {
    case 0x5A99F4: return f(&StubClearPrivate);
    case 0x5A9949: return f(&StubSleep);
    case 0x462420: return f(&StubCheckStart);
    case 0x462600: return f(&StubBackdrop);
    case 0x462740: return f(&StubLogo);
    case 0x454810: return f(&StubLoadDone);
    case 0x495040: return f(&StubTransition);
    case 0x4624B0: return f(&StubFadeMusic);
    case 0x587740: return f(&StubEffect);
    case 0x5A77C0: return f(&StubDrawMode);
    case 0x461E50: return f(&StubCommit);
    case 0x462560: return f(&StubSprite);
    case 0x5A7710: return f(&StubSetSprt);
    case 0x5A7780: return f(&StubSemiTrans);
    case 0x462820: return f(&StubSetA);
    case 0x462A00: return f(&StubSetB);
    case 0x462930: return f(&StubSetC);
    case 0x56D690: return f(&StubEventDispatch);
    case 0x517200: return f(&StubFieldFrame);
    case 0x4967F0: return f(&StubWaitTransition);
    case 0x587BE0: return f(&StubMusicFade);
    case 0x587B40: return f(&StubMusicStop);
    default: bof3::Fatal("mode_tasks: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The six copies and their calls out, by capstone 2026-09-22 (every jump
// stays inside; the task body's table call and the field handler's switch
// are absolute - the first reads Title_States, swapped below, the second a
// jump table relocated into the copy).
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
constexpr Call kTaskCalls[] = {{0xe, 0x5a99f4}, {0x15, 0x5a9949}, {0x1d, 0x462420}, {0x33, 0x462600}, {0x38, 0x462740}};
constexpr Call kCheckCalls[] = {{0x17, 0x454810}, {0x66, 0x495040}, {0x6b, 0x4624b0}, {0x75, 0x587740}};
constexpr Call kSpriteCalls[] = {{0x8, 0x5a7710}, {0x18, 0x5a7780}, {0x94, 0x461e50}};
constexpr Call kBackdropCalls[] = {{0xd0, 0x5a77c0}, {0xd9, 0x461e50}, {0xf3, 0x462560}};
constexpr Call kLogoCalls[] = {{0xa6, 0x462820}, {0xb7, 0x462a00}, {0xcc, 0x462930}};
constexpr Call kFieldCalls[] = {{0x1, 0x56d690}, {0x6, 0x517200}, {0x28, 0x495040}, {0x30, 0x4967f0},
                                {0xbd, 0x587be0}, {0xd6, 0x495040}, {0xdd, 0x4967f0}, {0xf8, 0x587b40}};
enum : unsigned { kTask, kCheck, kSprite, kBackdrop, kLogo, kField, kCount };
const Clone kClones[kCount] = {
    {"Title_Task", 0x4621C0, 0x3F, kTaskCalls, 5},
    {"Title_CheckStart", 0x462420, 0x8E, kCheckCalls, 4},
    {"Title_Sprite", 0x462560, 0xA0, kSpriteCalls, 3},
    {"Title_DrawBackdrop", 0x462600, 0x13B, kBackdropCalls, 3},
    {"Title_DrawLogo", 0x462740, 0xDA, kLogoCalls, 3},
    // 0x4959F0..0x495B85, two bytes of padding, the 9-entry table at +0x198.
    {"GameMode_Field", 0x4959F0, 0x1BC, kFieldCalls, 8},
};
constexpr move_script::Table kFieldTable = {0x22, 0x198, 9};

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {bof3::addr::Task_Records, 0x48},   // task 0's state, Field_Request, Game_Mode / Game_Step,
                                        // task 1's words, MoveScript_WaitWordDA
    {static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&Input_Pressed)), 2},
    {kMusicTrack - 1, 8},               // the track and the battle count
    {kBattleBytes, 4},
    {kMemberByte0, 1},
    {kMemberByte1, 1},
    {kMemberByte2, 1},
    {kAreaTrack, 1},
    {kAreaTransition, 1},
    {kFieldFlags, 1},
    {kMenuBlock, 0x10},
    {kMode11Bytes, 8},
};
constexpr unsigned kRegionBytes = 0x48 + 2 + 8 + 4 + 1 + 1 + 1 + 1 + 1 + 1 + 0x10 + 8;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char prim[sizeof g_prim], sprite_out[sizeof g_sprite_out];
    std::uint32_t packet;   // Gfx_PacketNext, as an Id
    std::uint32_t ret;      // Title_Sprite's, as an Id
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    std::memcpy(s.prim, g_prim, sizeof g_prim);
    std::memcpy(s.sprite_out, g_sprite_out, sizeof g_sprite_out);
    s.packet = Id(Gfx_PacketNext);
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    std::memcpy(g_prim, s.prim, sizeof g_prim);
    std::memcpy(g_sprite_out, s.sprite_out, sizeof g_sprite_out);
    Gfx_PacketNext = g_prim + (s.packet - 0x1000);
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
    g_sleeps = 0;
}

std::uint32_t g_rng = 0x3C6EF372u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 != 0; }
// A word near a boundary b: b - 3 .. b + 2.
std::uint16_t Near(unsigned b) { return static_cast<std::uint16_t>(b - 3 + Next() % 6); }

using VoidFn = void (__cdecl*)();
using SpriteFn = unsigned char* (__cdecl*)(int, int, unsigned, unsigned, unsigned);

// The task body runs until the Task_Sleep recorder's long jump. A setjmp of
// our own - ebx, esi, edi, ebp, esp and the return address - because clang's
// __builtin_setjmp let the loop's registers come back wrong here (the first
// build's rounds named the wrong function after the jump).
__attribute__((noinline)) void RunTask(VoidFn fn) {
    if (JumpSave(g_jump) == 0) fn();
}

// Each branch's boundaries, on top of the random bytes.
void Seed(unsigned k) {
    switch (k) {
    case kTask:
        Title_State = static_cast<std::uint16_t>(Next() % 8);
        break;
    case kCheck:
        if (Often()) Title_StartArmed = static_cast<unsigned char>(Next() % 4 ? 1 : Next());
        else Title_StartArmed = 0;
        if (Often()) Input_Pressed = static_cast<std::uint16_t>(Input_Pressed | 0x800);
        if (Often()) MoveScript_WaitWordDA = 0;
        if (Often()) SetWord(At(bof3::addr::Task_Records), 0);
        Title_State = static_cast<std::uint16_t>(Next() % 3 ? Next() % 8 : Next());
        if (Often()) Title_Fade = 0;
        if (Often()) Title_LogoState = 2;
        break;
    case kSprite:
        break;
    case kBackdrop: {
        switch (Next() % 6) {
        case 0: Title_Fade = 0; break;
        case 1: Title_Fade = 1; break;
        case 2: Title_Fade = 2; break;
        case 3: Title_Fade = 3; break;
        default: break;
        }
        // Around each column's threshold (640 j, before the + 2), negative too.
        const int j = static_cast<int>(Next() % 7) - 2;
        if (Often()) Title_Scroll = Near(static_cast<unsigned>(640 * j));
        static const std::uint16_t kBright[] = {0, 1, 2, 0x7E, 0x7F, 0x80, 0x81, 0x7FFF, 0x8000, 0xFFFF};
        if (Often()) Title_Bright = kBright[Next() % 10];
        break;
    }
    case kLogo: {
        Title_LogoState = static_cast<unsigned char>(Next() % 5 ? Next() % 4 : Next());
        static const std::uint16_t kShade[] = {0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F, 0x80, 0x7FFE, 0x7FFF, 0xFFFE};
        if (Often()) Title_LogoShadeA = kShade[Next() % 10];
        if (Often()) Title_LogoShadeB = kShade[Next() % 10];
        break;
    }
    case kField:
        Field_Request = static_cast<unsigned char>(Next() % 4 == 0 ? 5 : Next() % 5 ? Next() % 11 : Next());
        if (Often()) At(kMusicTrack)[0] = 0xFF;
        if (Often()) At(kAreaTrack)[0] = At(kMusicTrack)[0];
        if (Often()) At(kAreaTransition)[0] = 0xFF;
        break;
    default:
        break;
    }
}

// What the rounds reached, from the original's side: for the log line, so
// that a branch the seeds stopped reaching shows as a zero.
struct Coverage {
    unsigned cut, leave;               // Title_CheckStart: the hold cut short, left for the title menu
    unsigned faded_in, faded_out, relit;  // the backdrop: fade 1 -> 2, 3 -> 0, and 0x80 stored after 3 -> 0
    unsigned logo_done, logo_other;    // the logo: state 1 -> 2, a state above 2 drawn
    unsigned mode[12];                 // GameMode_Field: rounds that ended in each Game_Mode it sets
    unsigned frames[7];                // Title_Task: rounds by the frames it ran
} g_cover;
unsigned Byte(const State& s, std::uint32_t address) { return s.memory[address - bof3::addr::Task_Records]; }
unsigned Half(const State& s, std::uint32_t address) { return Byte(s, address) | Byte(s, address + 1) << 8; }
void Cover(unsigned k, const State& in, const State& out) {
    constexpr std::uint32_t aFade = 0x66C7F9, aLogo = 0x66C7FA, aTimer = 0x66C7FC, aBright = 0x66C800, aState = 0x66C808,
                            aMode = 0x66C7E8;
    switch (k) {
    case kTask: ++g_cover.frames[g_sleep_limit % 7]; break;
    case kCheck:
        for (unsigned i = 0; i < out.log_n && i < kLog; ++i) if (out.log[i].what == 9) ++g_cover.leave;
        if (Half(out, aTimer) == 1 && Half(out, aState) == 2 && (Half(in, aTimer) != 1 || Half(in, aState) != 2)) ++g_cover.cut;
        break;
    case kBackdrop:
        if (Byte(in, aFade) == 1 && Byte(out, aFade) == 2) ++g_cover.faded_in;
        if (Byte(in, aFade) == 3 && Byte(out, aFade) == 0) {
            ++g_cover.faded_out;
            if (Half(out, aBright) == 0x80) ++g_cover.relit;
        }
        break;
    case kLogo:
        if (Byte(in, aLogo) == 1 && Byte(out, aLogo) == 2) ++g_cover.logo_done;
        if (Byte(in, aLogo) > 2) ++g_cover.logo_other;
        break;
    case kField:
        if (Half(out, aMode) != Half(in, aMode) && Half(out, aMode) < 12) ++g_cover.mode[Half(out, aMode)];
        break;
    default: break;
    }
}

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 24000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("mode_tasks: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    unsigned char* const saved_packet = Gfx_PacketNext;
    Gfx_PacketNext = g_prim;
    Capture(saved);
    void* saved_table[8];
    std::memcpy(saved_table, Title_States, sizeof saved_table);
    std::memcpy(Title_States, kHandlers, sizeof kHandlers);
    g = kStubs;

    const void* const ours[kCount] = {
        reinterpret_cast<const void*>(&Title_Task), reinterpret_cast<const void*>(&Title_CheckStart),
        reinterpret_cast<const void*>(&Title_Sprite), reinterpret_cast<const void*>(&Title_DrawBackdrop),
        reinterpret_cast<const void*>(&Title_DrawLogo), reinterpret_cast<const void*>(&GameMode_Field)};
    unsigned bad = 0, calls = 0, per[kCount] = {};
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, packet); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.packet = 0x1000 + Next() % 0x40;
        g_seed = Next();
        g_sleep_limit = 1 + Next() % 6;
        const std::uint32_t args[5] = {Next(), Next(), Next() % 3 ? Next() % 17 : Next(), Next() % 2 ? Next() % 8 : Next(),
                                       Next() % 2 ? Next() % 2 : Next()};
        Apply(input);
        Seed(k);
        Capture(input);
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* const fn = pass ? ours[k] : theirs[k];
            std::uint32_t ret = 0;
            if (k == kTask) RunTask(reinterpret_cast<VoidFn>(const_cast<void*>(fn)));
            else if (k == kSprite)
                ret = Id(reinterpret_cast<SpriteFn>(const_cast<void*>(fn))(static_cast<int>(args[0]), static_cast<int>(args[1]),
                                                                           args[2], args[3], args[4]));
            else reinterpret_cast<VoidFn>(const_cast<void*>(fn))();
            State& out = pass ? our_out : their_out;
            Capture(out);
            out.ret = ret;
        }
        calls += their_out.log_n;
        Cover(k, input, their_out);
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12)
            bof3::Log("shadow      mode_tasks self-test MISMATCH: round %u, %s, log %u / %u", round, kClones[k].name,
                      their_out.log_n, our_out.log_n);
    }
    g = kOriginals;
    std::memcpy(Title_States, saved_table, sizeof saved_table);
    Apply(saved);
    Gfx_PacketNext = saved_packet;
    bof3::Log("shadow      mode_tasks self-test: %u rounds (%u per function), %u calls to the stand-ins, %u MISMATCHES; "
              "task 0 and 1's words, the request, mode, music and menu bytes, the primitives, the packet cursor, the "
              "return and the stand-ins' log compared",
              kRounds, per[0], calls, bad);
    const Coverage& c = g_cover;
    bof3::Log("shadow      mode_tasks coverage: start cut %u, left %u; backdrop faded in %u, out %u (relit %u); logo done %u, "
              "other %u; field modes 1:%u 3:%u 4:%u 5:%u 6:%u 7:%u 9:%u 10:%u 11:%u; task frames 1..6: %u %u %u %u %u %u",
              c.cut, c.leave, c.faded_in, c.faded_out, c.relit, c.logo_done, c.logo_other, c.mode[1], c.mode[3], c.mode[4],
              c.mode[5], c.mode[6], c.mode[7], c.mode[9], c.mode[10], c.mode[11], c.frames[1], c.frames[2], c.frames[3],
              c.frames[4], c.frames[5], c.frames[6]);
    if (bad) bof3::Fatal("the mode tasks differ from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void ModeTasks_Inject() {
    if (bof3::WantsShadow("mode_tasks")) {
        void* clones[kCount];
        for (unsigned k = 0; k < kCount; ++k) {
            const Clone& c = kClones[k];
            bof3::CloneCall calls[8];
            if (c.n_calls > 8) bof3::Fatal("mode_tasks: %s has %d calls", c.name, c.n_calls);
            for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target)};
            clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        }
        move_script::Relocate(clones[kField], kClones[kField].base, kClones[kField].size, kFieldTable);
        SelfTest(clones);
    }
    BOF3_INJECT(Title_Task);
    BOF3_INJECT(Title_CheckStart);
    BOF3_INJECT(Title_Sprite);
    BOF3_INJECT(Title_DrawBackdrop);
    BOF3_INJECT(Title_DrawLogo);
    BOF3_INJECT(GameMode_Field);
}
