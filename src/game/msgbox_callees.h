// Internal to msgbox.cpp and msgbox_fuzz.cpp: the addresses the message box
// touches that have no name in symbols.toml, and every call the forty-four
// functions make - through pointers, so that the start-up fuzz can stand
// recording functions in for them, for the originals' copies and for ours
// alike. Most of the callees are ours in this module; through the pointers
// each function is still tested alone.
//
// The six state / effect dispatchers build their tables on the stack out of
// `mov dword [esp+k], imm32` immediates, so a copy of one carries absolute
// targets: the fuzz rewrites those immediates in the copy the way
// text_draw.cpp relocates a jump table (docs/msgbox.md section 4).
#pragma once

#include <cstdint>
#include <cstring>

namespace msgbox {

// --- The MsgBoxState block 0x7DEE40 (symbols.toml block MsgBoxState; the PSX
// twin of each byte is the same offset off 0x8014909C, established by the
// dispatcher pair 0x497AD0 / PSX 0x801508EC). -----------------------------
constexpr std::uint32_t kState = 0x7DEE40;         // u8, MsgBox_StateDispatch's index, 0..7
constexpr std::uint32_t kSubState = 0x7DEE41;      // u8, every state's own dispatch index
constexpr std::uint32_t kEffectKind = 0x7DEE42;    // u8, MsgBox_EffectTask's index, 0..5
constexpr std::uint32_t kEffectPhase = 0x7DEE43;   // u8, each effect's own index, 0..1
constexpr std::uint32_t kFlags = 0x7DEE44;         // u16: 1 substitution, 2 done, 4 hold the stepper,
                                                   // 8 the effect draw, 0x10 instant print, 0x40/0x80 kept
constexpr std::uint32_t kDelay = 0x7DEE46;         // u8, frames until the next character
constexpr std::uint32_t kSubCount = 0x7DEE47;      // u8, characters left of a substitution (state 0's)
constexpr std::uint32_t kMessage = 0x7DEE48;       // u16, the message id last opened
constexpr std::uint32_t kEffectTimer = 0x7DEE4A;   // u16, frames the effect has left, 0xFFFF forever
constexpr std::uint32_t kBase = 0x7DEE4C;          // const unsigned char *, the message the stepper draws
constexpr std::uint32_t kAt = 0x7DEE50;            // const unsigned char *, state 0's read position
constexpr std::uint32_t kResume = 0x7DEE54;        // const unsigned char *, where a substitution returns to
constexpr std::uint32_t kColorHigh = 0x7DEE58;     // u8, the colour byte's high nibble (the clip pair)
constexpr std::uint32_t kStepCount = 0x7DEE59;     // u8, characters MsgBox_Step draws this frame
constexpr std::uint32_t kOffsetX = 0x7DEE5A;       // s8, the effect's pen x offset
constexpr std::uint32_t kOffsetY = 0x7DEE5B;       // s8, the effect's pen y offset
constexpr std::uint32_t kPenY = 0x7DEE5E;          // u16, the stepper's pen y (MsgBox_PenX is 0x7DEE5C)
constexpr std::uint32_t kOriginY = 0x7DEE62;       // u16, the box origin y (MsgBox_LineX is 0x7DEE60)
constexpr std::uint32_t kChoiceId = 0x7DEE64;      // u8, the choice list's id (code 0x14's first byte)
constexpr std::uint32_t kChoiceKind = 0x7DEE65;    // u8, its second byte's high nibble
constexpr std::uint32_t kChoiceLast = 0x7DEE66;    // u8, (low nibble - 1): the last row
constexpr std::uint32_t kCursor = 0x7DEE67;        // u8, the row the cursor is on
constexpr std::uint32_t kEffectOff = 0x7DEE68;     // u16, the effect's own offset
constexpr std::uint32_t kEffectWait = 0x7DEE6A;    // u16, the effect's second counter
constexpr std::uint32_t kPlacement = 0x7DEE6E;     // u8, the argument of a leading 0x0C

// --- Outside the block ----------------------------------------------------
constexpr std::uint32_t kWindows = 0x803160;       // WindowRecords, stride 0x24; slot 1 is 0x803184
constexpr std::uint32_t kWindow0State = 0x803163;  // u8, window 0's state byte
constexpr std::uint32_t kWindow0X = 0x803164;      // s16 / s16, window 0's position
constexpr std::uint32_t kWindow0Y = 0x803166;
constexpr std::uint32_t kWindow0Rows = 0x80316A;   // u8, halved into the page-arrow's x
constexpr std::uint32_t kWindow0Kind = 0x80316F;   // u8, set to 2 by MsgBox_Reset
constexpr std::uint32_t kWindow1Kind = 0x803186;   // u8, window 1's kind (1 a choice list, 2 a menu)
constexpr std::uint32_t kWindow1State = 0x803187;  // u8, window 1's state byte
constexpr std::uint32_t kScriptPool = 0x803580;    // MessagePools, the script side
constexpr std::uint32_t kPoolSelector = 0x803584;  // u32 inside the pool header: code 0x08's sub-pool
constexpr std::uint32_t kTextRecords = 0x904CDF;   // Text_Records 0x904CE0 minus one (the tail's inc esi)
constexpr std::uint32_t kCharRecords = 0x903A6F;   // CharacterRecords 0x903A70 minus one, stride 164
constexpr std::uint32_t kNameIndex = 0x802DC9;     // u8, the member whose name control 0x03 substitutes
constexpr std::uint32_t kNameMap = 0x66972C;       // u8[], that member's record index
constexpr std::uint32_t kSpeedTable = 0x658E94;    // u8[]: +0 held-fast, +1 normal, by the text-speed byte
constexpr std::uint32_t kEffectTable = 0x658E98;   // 4-byte records: kind, s8 offset, u16 frames
constexpr std::uint32_t kTextSpeed = 0x903A58;     // s8, the player's message-speed option

// --- Callees that are not ours and have no name in symbols.toml -----------
constexpr std::uint32_t kRetOnly = 0x437CC0;       // a bare ret: effect kind 0, the only one the attract reaches
constexpr std::uint32_t kEffectDraw = 0x4987E0;    // the stepper's own glyph draw under flag 8 (unread; still 12 px)
constexpr std::uint32_t kPageArrow = 0x498D20;     // draws the "more" arrow every other 32 frames
constexpr std::uint32_t kChoiceCommit = 0x4981C0;  // state 4 sub 5: an indirect call through Area_Descriptors +0x34
constexpr std::uint32_t kMenuCommit = 0x4983C0;    // state 5 sub 3: the same, for a menu
constexpr std::uint32_t kAutoRepeat = 0x461EB0;    // the pad auto-repeat on Input_Held (docs/msgbox.md section 3)

struct Callees {
    void (__cdecl* state[8])();     // MsgBox_StateDispatch's eight, 0x497B30..0x498470
    void (__cdecl* sub2[2])();      // MsgBox_State2's
    void (__cdecl* sub3[2])();      // MsgBox_State3's
    void (__cdecl* sub4[8])();      // MsgBox_State4's
    void (__cdecl* sub5[6])();      // MsgBox_State5's
    void (__cdecl* sub7[2])();      // MsgBox_State7's
    void (__cdecl* effect[6])();    // MsgBox_EffectTask's six
    void (__cdecl* shake[2])();     // MsgBox_EffectShake's
    void (__cdecl* grow[2])();      // MsgBox_EffectGrow's
    void (__cdecl* rise[2])();      // MsgBox_EffectRise's
    void (__cdecl* reset)();                                  // MsgBox_Reset (ours)
    unsigned (__cdecl* window_alloc)(unsigned, unsigned);     // Window_Alloc (ours)
    void (__cdecl* dispatch)();                               // MsgBox_StateDispatch (ours)
    void (__cdecl* effect_task)();                            // MsgBox_EffectTask (ours)
    void (__cdecl* step)();                                   // MsgBox_Step (ours)
    void (__cdecl* reopen)();                                 // MsgBox_Reopen (ours)
    void (__cdecl* sound)(unsigned short);                    // Sound_PlayEffect
    // Text_DrawAt in the original; MsgBox_DrawChar in ours - DIV-0006, see msgbox.cpp.
    const unsigned char* (__cdecl* draw_char)(int, int, int, int, const unsigned char*);
    void (__cdecl* effect_draw)(unsigned, const unsigned char*);
    void (__cdecl* page_arrow)(int, int);
    unsigned (__cdecl* repeat)(unsigned);
    const unsigned char* (__cdecl* draw_string)(unsigned, unsigned, const unsigned char*);  // Text_DrawString (ours)
    void (__cdecl* set_code6c)(unsigned char*);               // Gpu_SetCode6C
    void (__cdecl* set_semitrans)(unsigned char*, unsigned);  // Gpu_SetSemiTrans
    void (__cdecl* commit)(unsigned, unsigned);               // Gfx_CommitPrim
};

extern const Callees kOriginals;
extern Callees g;

// Byte and word access at an absolute address, as the original's `mov byte
// ptr [imm32]`: every store is the original's width, so the arithmetic wraps
// where the original's does.
inline unsigned char* At(std::uint32_t address) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address));
}
inline unsigned char B(std::uint32_t address) { return *At(address); }
inline void SetB(std::uint32_t address, unsigned v) { *At(address) = static_cast<unsigned char>(v); }
inline std::uint16_t W(std::uint32_t address) {
    std::uint16_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
inline void SetW(std::uint32_t address, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(At(address), &w, sizeof w);
}
inline std::uint32_t L(std::uint32_t address) {
    std::uint32_t v;
    std::memcpy(&v, At(address), sizeof v);
    return v;
}
inline void SetL(std::uint32_t address, std::uint32_t v) { std::memcpy(At(address), &v, sizeof v); }
inline const unsigned char* Ptr(std::uint32_t address) {
    return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(L(address)));
}
inline void SetPtr(std::uint32_t address, const unsigned char* p) {
    SetL(address, static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)));
}

// The start-up fuzz (msgbox_fuzz.cpp): clones all forty-four with every call
// re-aimed at a recording stand-in, every stack-built dispatch table's
// immediates rewritten and the two inline jump tables relocated, runs ours
// against the clones, and ends the process through bof3::Fatal on any
// difference.
void SelfTest();

}  // namespace msgbox
