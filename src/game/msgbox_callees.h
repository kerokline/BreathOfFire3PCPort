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

#include "bof3/symbols.gen.h"
// Rebound 2026-09-28 (round eleven's cleanup, docs/round-11-cleanup.md item 2):
// every constant here whose target has a name in symbols.toml reads
// bof3::addr::<Name>. The values are unchanged - the fuzz keys on them.
#include <cstring>

namespace msgbox {

// --- The MsgBoxState block 0x7DEE40 (symbols.toml block MsgBoxState; the PSX
// twin of each byte is the same offset off 0x8014909C, established by the
// dispatcher pair 0x497AD0 / PSX 0x801508EC). -----------------------------
constexpr ::bof3::image::Va kState{0x7DEE40};         // u8, MsgBox_StateDispatch's index, 0..7
// kSubState: now ItemUse_SubState (symbols.toml). u8, every state's own dispatch index
// kEffectKind: now Msgbox_EffectKind (symbols.toml). u8, MsgBox_EffectTask's index, 0..5
// kEffectPhase: now Msgbox_EffectPhase (symbols.toml). u8, each effect's own index, 0..1
constexpr ::bof3::image::Va kFlags{0x7DEE44};         // u16: 1 substitution, 2 done, 4 hold the stepper,
                                                   // 8 the effect draw, 0x10 instant print, 0x40/0x80 kept
// kDelay: now Msgbox_Delay (symbols.toml). u8, frames until the next character
// kSubCount: now Msgbox_SubCount (symbols.toml). u8, characters left of a substitution (state 0's)
constexpr ::bof3::image::Va kMessage{0x7DEE48};       // u16, the message id last opened
// kEffectTimer: now Msgbox_EffectTimer (symbols.toml). u16, frames the effect has left, 0xFFFF forever
constexpr ::bof3::image::Va kBase{0x7DEE4C};          // const unsigned char *, the message the stepper draws
constexpr ::bof3::image::Va kAt{0x7DEE50};            // const unsigned char *, state 0's read position
constexpr ::bof3::image::Va kResume{0x7DEE54};        // const unsigned char *, where a substitution returns to
// kColorHigh: now ItemUse_ColorHigh (symbols.toml). u8, the colour byte's high nibble (the clip pair)
constexpr ::bof3::image::Va kStepCount{0x7DEE59};     // u8, characters MsgBox_Step draws this frame
// kOffsetX: now Msgbox_OffsetX (symbols.toml). s8, the effect's pen x offset
// kOffsetY: now Msgbox_OffsetY (symbols.toml). s8, the effect's pen y offset
// kPenY: now Msgbox_PenY (symbols.toml). u16, the stepper's pen y (MsgBox_PenX is 0x7DEE5C)
// kOriginY: now Msgbox_OriginY (symbols.toml). u16, the box origin y (MsgBox_LineX is 0x7DEE60)
// kChoiceId: now ItemUse_ChoiceId (symbols.toml). u8, the choice list's id (code 0x14's first byte)
// kChoiceKind: now MenuWindows_ListSet (symbols.toml). u8, its second byte's high nibble
constexpr ::bof3::image::Va kChoiceLast{0x7DEE66};    // u8, (low nibble - 1): the last row
// kCursor: now AreaW0c_ChoiceAnswer (symbols.toml). u8, the row the cursor is on
constexpr ::bof3::image::Va kEffectOff{0x7DEE68};     // u16, the effect's own offset
// kEffectWait: now Msgbox_EffectWait (symbols.toml). u16, the effect's second counter
// kPlacement: now Msgbox_Placement (symbols.toml). u8, the argument of a leading 0x0C

// --- Outside the block ----------------------------------------------------
// kWindows: now WindowRecords (symbols.toml). WindowRecords, stride 0x24; slot 1 is 0x803184
constexpr ::bof3::image::Va kWindow0State{0x803163};  // u8, window 0's state byte
// kWindow0X: now WindowRecords record 0 x (records::WindowRecord).
// kWindow0Y: now WindowRecords record 0 y (records::WindowRecord).
// kWindow0Rows: now Window0_ParamA (symbols.toml). u8, halved into the page-arrow's x
// kWindow0Kind: now Window0_Pass (symbols.toml). u8, set to 2 by MsgBox_Reset
// kWindow1Kind: now Window1_Kind (symbols.toml). u8, window 1's kind (1 a choice list, 2 a menu)
// kWindow1State: now Window1_State (symbols.toml). u8, window 1's state byte
constexpr ::bof3::image::Va kScriptPool{0x803580};    // MessagePools, the script side
constexpr ::bof3::image::Va kPoolSelector{0x803584};  // u32 inside the pool header: code 0x08's sub-pool
constexpr ::bof3::image::Va kTextRecords{0x904CDF};   // Text_Records 0x904CE0 minus one (the tail's inc esi)
constexpr ::bof3::image::Va kCharRecords{0x903A6F};   // CharacterRecords 0x903A70 minus one, stride 164
// kNameIndex: now ObjTrio[0].char_id (symbols.toml). u8, the member whose name control 0x03 substitutes
constexpr ::bof3::image::Va kNameMap{0x66972C};       // u8[], that member's record index
constexpr ::bof3::image::Va kSpeedTable{0x658E94};    // u8[]: +0 held-fast, +1 normal, by the text-speed byte
constexpr ::bof3::image::Va kEffectTable{0x658E98};   // 4-byte records: kind, s8 offset, u16 frames
// kTextSpeed: now ModeFlow_ConfigBytes (symbols.toml). s8, the player's message-speed option
constexpr ::bof3::image::Va kClipTable{0x658F00};     // u8[], by 0x7DEE58: the rows MsgBox_EffectDraw cuts off a glyph

// --- Callees by address (ours since, named by symbol) ---------------------
constexpr std::uint32_t kRetOnly = bof3::addr::BareRet;       // a bare ret: effect kind 0, the only one the attract reaches
constexpr std::uint32_t kEffectDraw = bof3::addr::MsgBox_EffectDraw;    // MsgBox_EffectDraw, ours since 2026-10-03 (the fuzz's stand-in key)
constexpr std::uint32_t kPageArrow = bof3::addr::MsgBox_DrawArrow;     // draws the "more" arrow every other 32 frames
constexpr std::uint32_t kChoiceCommit = bof3::addr::MsgBox_ChoiceCommit;  // state 4 sub 5: an indirect call through Area_Descriptors +0x34
constexpr std::uint32_t kMenuCommit = bof3::addr::MsgBox_MenuCommit;    // state 5 sub 3: the same, for a menu
constexpr std::uint32_t kAutoRepeat = bof3::addr::Input_AutoRepeat;    // the pad auto-repeat on Input_Held (docs/msgbox.md section 3)

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
    unsigned (__cdecl* get_clut)(int, int);                   // Gpu_GetClut (ours, psx_gpu.cpp)
};

// DIVERGENCE DIV-0070: 0 is Capcom's (a space commits a primitive with a stale
// glyph word and stale texture bytes), 1 ours (a space commits nothing).
// Off until MsgBox_Inject has run the self-test, which compares Capcom's.
extern unsigned char g_effect_space_skips;

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

// The same accessors for a VA held as a bof3::Va (common/image_base.h): the
// VA is placed here, through ::bof3::Img, and nowhere else.
inline unsigned char* At(::bof3::image::Va va) { return At(::bof3::Img(va)); }
inline unsigned char B(::bof3::image::Va va) { return B(::bof3::Img(va)); }
inline void SetB(::bof3::image::Va va, unsigned v) { SetB(::bof3::Img(va), v); }
inline std::uint16_t W(::bof3::image::Va va) { return W(::bof3::Img(va)); }
inline void SetW(::bof3::image::Va va, unsigned v) { SetW(::bof3::Img(va), v); }
inline std::uint32_t L(::bof3::image::Va va) { return L(::bof3::Img(va)); }
inline void SetL(::bof3::image::Va va, std::uint32_t v) { SetL(::bof3::Img(va), v); }
inline const unsigned char* Ptr(::bof3::image::Va va) { return Ptr(::bof3::Img(va)); }
inline void SetPtr(::bof3::image::Va va, const unsigned char* p) { SetPtr(::bof3::Img(va), p); }

// The start-up fuzz (msgbox_fuzz.cpp): clones all forty-four with every call
// re-aimed at a recording stand-in, every stack-built dispatch table's
// immediates rewritten and the two inline jump tables relocated, runs ours
// against the clones, and ends the process through bof3::Fatal on any
// difference.
void SelfTest();

}  // namespace msgbox
