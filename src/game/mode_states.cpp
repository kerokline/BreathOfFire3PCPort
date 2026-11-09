// Top-level modes, the system choice and the field core: the look-around
// mode's two camera steps (0x496250, 0x496A00, 0x496AD0); the shop mode and
// its three steps (0x496290, 0x4962A0, 0x517300, 0x496390); the menu's
// transition wait and frame (0x496830, 0x5172F0); the message box's system
// choice (0x498A30); the 8 px UI string draw (0x516E70); the field's loading
// frame (0x517290); the field core's state 2 and its fade steps (0x525370,
// 0x5258B0, 0x5258D0, 0x525920); scenario chapter 1's frame, area entry and
// step hook (0x539AD0, 0x539B20, 0x53D830); and the window cursor draw
// (0x5960D0). docs/mode_states.md.
//
// Every call goes through mode_states::g (mode_states_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike. Everything here is a faithful replacement; the
// one index the original leaves unbounded into its own stack (the system
// choice, docs/known-defects.md D22) aborts instead where no original frame
// is left to reproduce (section 2 of the doc).
#include "game/mode_states.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/labels.h"
#include "game/mode_states_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace mode_states {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    {MsgBox_SysChoice80, MsgBox_SysChoice81, MsgBox_SysChoice82, MsgBox_SysChoice83, MsgBox_SysChoice84,
     MsgBox_SysChoice85, MsgBox_SysChoice86, MsgBox_SysChoice87, MsgBox_SysChoice88, MsgBox_SysChoice89,
     MsgBox_SysChoice8A, MsgBox_SysChoice8B, MsgBox_SysChoice8C, MsgBox_SysChoice8D, MsgBox_SysChoice8E,
     MsgBox_SysChoice8F},
    {0x4981F1, 0x4983F1},
    Look_Return, Field_LoadingFrame, Menu_Frame,
    Field_Frame, LoadDatFile, File_LoadDone, Task_Sleep, Gfx_ClutStripCopyRow, Sprite_FaceDirection,
    Snd_LoadBankFile, Fn<void (__cdecl*)()>(::bof3::Code(kAfterShop.v)), Text_EmitGlyph,
    AreaMap_Frame, Party_ExtraScreens, Party_UpdateScreens, Field_ObjectsScreen, Effect_RunObjects,
    MoveScript_TintFrame, Field_DrawFrame, Field_ModeDispatch, Fn<void (__cdecl*)()>(::bof3::Code(kMenuStates.v)),
    Field_RunTaskRecords, Fn<void (__cdecl*)()>(::bof3::Code(kShopStates.v)),
    Sprite_ShadeFadeBegin, Sprite_ShadeFadeStep,
    Flags_Test, Flags_Set, ScriptFlags_Set40, Music_LoadFile, Music_FadeOutStop, Sound_PlayEffect,
    Effect_FindFree, Scenario_CallA, AreaMap_SetupEntries, MoveCmd_TestFB, Field_ChangeArea,
    Menu_DrawHand,
};
Callees g = kOriginals;

}  // namespace mode_states

using namespace mode_states;

namespace {

// A table of code pointers in the exe's data, read as the original reads it:
// afresh, and indexed without a bound.
mode_states::Handler Entry(std::uint32_t table, int index) {
    return reinterpret_cast<mode_states::Handler>(static_cast<std::uintptr_t>(
        static_cast<std::uint32_t>(mode_states::Long(mode_states::At(table + 4u * static_cast<std::uint32_t>(index))))));
}

// The field's load wait: File_LoadDone asked first, then a frame's sleep and
// the question again until it answers non-zero.
void WaitLoad() {
    while (g.load_done() == 0) g.sleep(1);
}

// Scenario chapter 1's bytes (docs/field-modes.md section 2).
unsigned Area() { return Game_AreaNumber; }
unsigned char& Counter(unsigned i) { return At(::bof3::Img(at::kCounters) + i)[0]; }
void ClearCounters() { for (unsigned i = 0; i < 4; ++i) Counter(i) = 0; }
void SetStep5(unsigned v) { MoveScript_PartyRecords[0x65] = static_cast<unsigned char>(v); }
void SetRun(int v) { MoveScript_Var7 = static_cast<signed char>(v); }
// The flag bits, the dword 0x929ED0 read afresh at each call.
unsigned char* Bank() { return reinterpret_cast<unsigned char*>(Chapter_FlagRow); }
bool Flag(unsigned n) { return g.flags_test(Bank(), n) != 0; }
void SetFlag(unsigned n) { g.flags_set(Bank(), n); }
// Takes an effect slot and, when there is one, marks its record live as kind
// `kind`. As the original has it: the slot byte is stored first and the
// record indexed by the stored byte read back.
bool PlaceEffect(unsigned char kind) {
    const unsigned char slot = g.effect_find();
    at::kEffectSlot.b_00 = slot;
    if (slot == 0xFF) return false;
    unsigned char* const e = rv::Bytes(Effect_Objects) + ((static_cast<std::uint32_t>(at::kEffectSlot.d_00) & 0xFF) << 7);
    e[0] = 1;
    e[5] = kind;
    return true;
}

}  // namespace

// ===========================================================================
// The look-around mode (Game_Mode 6) and its camera
// ===========================================================================

// ===========================================================================
// The shop mode (Game_Mode 7)
// ===========================================================================

// original 0x517300 (no PSX twin paired; PSX 0x8019A398): the shop's step 1, once a frame -
// the field drawn without its objects' logic (AreaMap_Frame,
// Party_ExtraScreens, Party_UpdateScreens, Field_ObjectsScreen,
// Effect_RunObjects, Field_DrawFrame, Field_RunTaskRecords), then a tail
// jump into the shop overlay's state dispatch 0x57F500 (group DF's).
extern "C" __attribute__((disable_tail_calls)) void __cdecl Shop_Frame(void) {
    mode_states::g.areamap_frame();
    mode_states::g.extra_screens();
    mode_states::g.update_screens();
    mode_states::g.objects_screen();
    mode_states::g.run_effects();
    mode_states::g.draw_frame();
    mode_states::g.task_records();
    mode_states::g.shop_states();
}

// ===========================================================================
// The field's per-frame entries
// ===========================================================================

// original 0x517290 (PSX 0x8019A2B8): a field frame without the event
// script and without the objects' logic, for the frames a load waits -
// AreaMap_Frame, Party_ExtraScreens, Party_UpdateScreens,
// Field_ObjectsScreen, Effect_RunObjects, MoveScript_TintFrame, then a tail
// jump to Field_DrawFrame. Sixteen callers (the mode handlers' waits,
// Scenario_Start, 0x544B1D, 0x551C60, 0x56D645).
extern "C" __attribute__((disable_tail_calls)) void __cdecl Field_LoadingFrame(void) {
    mode_states::g.areamap_frame();
    mode_states::g.extra_screens();
    mode_states::g.update_screens();
    mode_states::g.objects_screen();
    mode_states::g.run_effects();
    mode_states::g.tint_frame();
    mode_states::g.draw_frame();
}

// original 0x5172F0 (PSX 0x8019A368): the field menu's frame (mode 3's
// step 1, GameMode_Handlers' neighbour 0x656A74 entry 1) - Field_ModeDispatch,
// the menu's state dispatch 0x589970 (group DH's), then a tail jump to
// Field_RunTaskRecords.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Menu_Frame(void) {
    mode_states::g.mode_dispatch();
    mode_states::g.menu_states();
    mode_states::g.task_records();
}

// ===========================================================================
// The message box's system choice
// ===========================================================================

// ===========================================================================
// The 8 px UI string draw
// ===========================================================================

// original 0x516E70 (PSX 0x8014FD78, the catalogue's pairing): the small
// UI font - up to `count` characters of `text` at (x, y), each an 8 x 8
// quad through Text_EmitGlyph (x, y, 8, 8 - v, u, v, clut) with u and v from
// the byte pair 0x65F5A8[colour >> 4] and the CLUT 0x7800 | (colour & 0xF),
// the glyph index to word +0x16 of the primitive at Gfx_PacketNext first.
// 0x01 is a newline (x back, y + 9), 0x20 a blank 8 wide, a byte with bit 7
// the first of a two-byte code ((b0 & 0x7F) << 8 | b1), any other byte glyph
// b - 0x26. Returns one past the NUL that ended the text, or one past the
// next byte when the count ran out. This is the draw the shipped Chinese
// menus' small text goes through (docs/dialogue-localisation.md sections 4
// and 9, DIV-0016's 8 x 8 set).
//
// As the original has it: colour and count are the arguments' low bytes; x
// and y are whole ints, the pen x reset to the argument; a newline and a
// blank use up the count like a glyph; the text byte after each character
// and the primitive pointer are read afresh after the glyph's call; a text
// that starts with its NUL returns text + 1 whatever the count; there is no
// glyph bound here (Text_DrawString's 0xA00 test is not in this draw).
extern "C" const unsigned char* __cdecl Text_DrawSmall(int x, int y, unsigned colour, unsigned count, const unsigned char* text) {
    const unsigned c = colour & 0xFF;
    const unsigned char* const uv = mode_states::At(::bof3::Img(at::kSmallUV) + (c >> 4) * 2);
    const unsigned u = uv[0], v = uv[1];
    const int clut = static_cast<int>(0x7800u | (c & 0xF));
    const unsigned char* p = text;
    unsigned char ch = p[0];
    if (ch == 0) return p + 1;
    int pen_x = x, pen_y = y;
    unsigned char left = static_cast<unsigned char>(count);
    for (;;) {
        if (left == 0) return p + 1;
        if (ch == 1) {
            pen_x = x;
            pen_y += 9;
        } else if (ch == 0x20) {
            pen_x += 8;
        } else {
            unsigned glyph;
            if ((ch & 0x80) != 0) {
                glyph = ((ch & 0x7Fu) << 8) + p[1];
                ++p;
            } else {
                glyph = (ch - 0x26u) & 0xFFFF;
                // DIVERGENCE DIV-0064: the status words under a language
                // overlay, from its 8 x 8 set.
                if (const unsigned small = Labels_SmallGlyph(p)) glyph = small;
            }
            rv::Store<unsigned>(rv::As<records::GpuGlyph>(Gfx_PacketNext)->glyph, glyph);
            mode_states::g.emit_glyph(pen_x, pen_y, 8, static_cast<int>(8 - v), static_cast<int>(u), static_cast<int>(v), clut);
            pen_x += 8;
        }
        ch = p[1];
        ++p;
        --left;
        if (ch == 0) return p + 1;
    }
}

// ===========================================================================
// The field core: state 2 and its fade
// ===========================================================================

// original 0x525370 (PSX 0x801B79B8): state 2 of both the members' state
// table 0x65F960 and the leader's 0x660918 - a tail jump through
// FieldCore_State2Steps 0x66011C on Sprite_Current +2 (9 entries:
// 0x525390, 0x5257A0, FieldCore_Fade, 0x525960, 0x525CA0, 0x5261E0,
// 0x526490, 0x526A90, 0x526B80). Unchecked, the byte read afresh.
extern "C" void __cdecl FieldCore_State2(void) { Entry(::bof3::Img(FieldCore_State2Steps_addr), Sprite_Current[2])(); }

// original 0x5258B0 (PSX 0x801B8264): sub-state 2 of state 2 - a tail jump
// through FieldCore_FadeSteps 0x660158 on Sprite_Current +3 (0
// FieldCore_FadeBegin, 1 FieldCore_FadeStep). Unchecked: 2 would run
// 0x525960's table, the words after.
extern "C" void __cdecl FieldCore_Fade(void) { Entry(::bof3::Img(FieldCore_FadeSteps_addr), Sprite_Current[3])(); }

// original 0x5258D0 (PSX 0x801B82A8): the fade's first frame -
// Sprite_ShadeFadeBegin, then the shade bytes +0x5D..+0x5F 0x80 and +0x5C 1,
// Field_State +0x137 = 7, +0 bit 6 cleared and +3 = 1, all through
// Sprite_Current read afresh at each store.
extern "C" void __cdecl FieldCore_FadeBegin(void) {
    g.shade_begin();
    Sprite_Current[0x5D] = 0x80;
    Sprite_Current[0x5E] = 0x80;
    Sprite_Current[0x5F] = 0x80;
    Sprite_Current[0x5C] = 1;
    Field_State[0x137] = 7;
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] & 0xBF);
    Sprite_Current[3] = 1;
}

// original 0x525920 (PSX 0x801B833C): each later frame of the fade -
// Sprite_ShadeFadeStep(8), and once it answers non-zero, Field_State +0x137
// = 0 and the object back to state 1, sub-states 0 (+1 = 1, +2 = +3 = 0).
extern "C" void __cdecl FieldCore_FadeStep(void) {
    if (g.shade_step(8) == 0) return;
    Field_State[0x137] = 0;
    Sprite_Current[1] = 1;
    Sprite_Current[2] = 0;
    Sprite_Current[3] = 0;
}

// ===========================================================================
// Scenario chapter 1
// ===========================================================================

// original 0x539AD0: slot 0 of chapter 1's vtable Scena01_Hooks 0x660D68
// (0x662C80's entry 1), Field_ModeDispatch's call - a tail jump through
// Scena01_States 0x660D7C on the s8 0x8034E2: 0 0x539AE0 (the start), 1
// Scena01_EnterArea, 2 0x53A2B0 (the run). Unchecked, as Scena16_Frame.
extern "C" void __cdecl Scena01_Frame(void) {
    Entry(::bof3::Img(Scena01_States_addr), static_cast<signed char>(MoveScript_PartyRecords[0x62]))();
}

namespace {

// Scena01_EnterArea's body: every way out of it ends with state 2, which the
// caller stores. Each area test reads Game_AreaNumber afresh, each counter
// test the counter byte afresh.
void Scena01_EnterAreaBody() {
    if (Area() == 0) {
        const unsigned c = Counter(2);
        if (c == 0) {
            if (!Flag(0x3A)) {
                g.music_load(0);
                WaitLoad();
                ClearCounters();
                Draw_PassFlags = 0;
                SetStep5(0);
                SetRun(0xD);
            }
        } else if (c == 1) {
            if (!Flag(0x3A)) {
                Camera_Distance = static_cast<short>(Camera_Distance + 0xA00);
                MapView_Redraw = 2;
            }
        } else {
            return;
        }
    }
    if (Area() == 5) {
        switch (Counter(2)) {
        case 0:
            if (!Flag(0x15)) {
                Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 7);
                Counter(0) = 0;
                Draw_PassFlags = 0;
                SetStep5(0);
                SetRun(0xC);
            }
            break;
        case 1:
            g.music_load(8);
            WaitLoad();
            break;
        case 2:
            Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags ^ 7);
            Draw_PassFlags = 0x1F;
            return;
        case 4:
            Counter(0) = 0;
            SetStep5(3);
            SetRun(0x10);
            return;
        case 5:
            Draw_PassFlags = 0x1F;
            return;
        default:   // 3, and 6 and up
            return;
        }
    }
    if (Area() == 7 && !Flag(0x19)) {
        g.set40();
        Counter(0) = 0;
        Draw_PassFlags = 0;
        SetStep5(0);
        SetRun(0xB);
    }
    if (Area() == 8) {
        const unsigned c = Counter(2);
        if (c == 2) {
            if (!Flag(6)) {
                Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 6);
                SetStep5(0);
                SetRun(6);
            }
        } else if (c == 3) {
            if (!Flag(5)) {
                g.music_stop(0xA);
                SetStep5(0xA);
                SetRun(6);
            }
        } else if (c == 4) {
            ClearCounters();
            SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), 0);
            g.music_stop(0xA);
            Draw_PassFlags = 0x1F;
        } else {
            return;
        }
    }
    if (Area() == 9 && !Flag(0) && Counter(2) == 0) {
        Draw_PassFlags = 0;
        g.set40();
        SetStep5(0);
        SetRun(1);
    }
    if (Area() == 0xA) {
        switch (Counter(2)) {
        case 0:
            if (!Flag(1)) {
                Draw_PassFlags = 0x1F;
                SetRun(2);
            }
            break;
        case 3:
        case 4:
            Field_ScriptFlags = static_cast<unsigned short>(Field_ScriptFlags | 2);
            SetRun(7);
            return;
        case 7:
            if (!Flag(1)) {
                PlaceEffect(0x12);
                SetRun(2);
            }
            break;
        case 8:
            Draw_PassFlags = 0x1F;
            return;
        default:   // 1, 2, 5, 6, and 9 and up
            return;
        }
    }
    if (Area() == 0xD) {
        if (Counter(2) == 1) Draw_PassFlags = 0x1F;
        if (Flag(0x3C) && !Flag(0x3E) && Cond_ByteFD == 1) {
            Draw_PassFlags = 0;
            g.set40();
            Counter(0) = 0;
            Counter(1) = 0;
            Counter(2) = 0;
            SetStep5(0);
            SetRun(0xE);
        }
    }
    if (Area() == 0xE) {
        if (Counter(2) != 0) return;
        if (!Flag(0x1B)) {
            ClearCounters();
            SetStep5(0);
            SetRun(0);
        } else if (!Flag(0x1C)) {
            unsigned char* const bank = Bank();
            ClearCounters();
            g.flags_set(bank, 0x1C);
            g.call_a(1);
            Camera_Distance = static_cast<short>(Camera_Distance + 0xB80);
            MapView_Redraw = 2;
        }
    }
    const unsigned area = Area();
    if (area == 0x11) {
        ClearCounters();
        SetStep5(0);
        SetRun(0);
        return;
    }
    if (area == 0x16) {
        if (!Flag(0xB)) {
            unsigned char* const bank = Bank();
            SetStep5(0);
            SetRun(9);
            g.flags_set(bank, 0xB);
        } else if (!Flag(0xC)) {
            SetStep5(3);
            SetRun(9);
        } else if (!Flag(0xD)) {
            g.set40();
            unsigned char* const bank = Bank();
            SetStep5(8);
            SetRun(9);
            g.flags_set(bank, 0xD);
        } else if (!Flag(0xF)) {
            SetStep5(9);
            SetRun(9);
        } else if (!Flag(0x10)) {
            SetStep5(0x11);
            SetRun(9);
        }
    }
    if (Area() == 0x17) {
        switch (Counter(2)) {
        case 0:
            if (!Flag(9)) {
                ClearCounters();
                SetStep5(0);
                SetRun(8);
            }
            break;
        case 1:
            if (!Flag(0xA)) {
                g.music_stop(0xA);
                SetStep5(0x13);
                SetRun(8);
            }
            break;
        case 2:
            if (!Flag(0xA)) {
                SetStep5(0x37);
                SetRun(8);
            }
            break;
        case 3:
            if (PlaceEffect(0x11)) g.call_a(2);
            break;
        case 4:
            if (!Flag(0x39)) {
                Field_StatusBits = static_cast<unsigned char>(Field_StatusBits & 0xFE);
                g.setup_entries();
                g.test_fb(0x49, 0xC);
                unsigned char* const bank = Bank();
                Draw_PassFlags = 0;
                g.flags_set(bank, 0x39);
                g.flags_set(Bank(), 0x3F);
            }
            break;
        case 5:
            Counter(0) = 0;
            return;
        case 7:
            SetStep5(0x37);
            SetRun(8);
            return;
        default:   // 6, and 8 and up
            return;
        }
    }
    const unsigned last = Area();
    if (last == 0x21 || last == 0x10) {
        ClearCounters();
        SetRun(0);
        SetStep5(0);
        SetWord(At(::bof3::Img(MoveScript_PartyRecords_addr) + 0x66), 0);
    }
}

}  // namespace

// original 0x539B20: chapter 1's state 1, once per area entered (Scena01_States
// entry 1; the catalogue's 416 bytes end at a case of its first jump table -
// the body runs to 0x53A24B, its three jump tables 0x53A24C / 0x53A264 /
// 0x53A288 to 0x53A2A7). By the area number and the script counter
// 0x90384A, and the scenario flags (Flags_Test on the bank 0x929ED0), it
// sets the scene the chapter's run state plays next - the run
// MoveScript_Var7 and its step 0x8034E5 - with the counters, the pass flags,
// the script flags, the music, the camera distance or an effect; then state
// 2. The areas it knows are 0, 5, 7, 8, 9, 0xA, 0xD, 0xE, 0x10, 0x11, 0x16,
// 0x17 and 0x21 (what each scene is, is not read).
//
// As the original has it: the flag bank's dword is read before the stores
// that precede a Flags_Set; area 0xD's three flag and Cond_ByteFD tests and
// area 0x17 case 4's calls run in the original's order; the effect slot is
// indexed by the stored byte read back; every exit, early or not, stores
// state 2.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Scena01_EnterArea(void) {
    Scena01_EnterAreaBody();
    MoveScript_PartyRecords[0x62] = 2;
}

// original 0x53D830: slot 2 of chapter 1's vtable, the step hook
// (Scenario_StepHook calls it with the leader's target x and z, 16.16): 1
// when the step starts a scene - a script flag set (ScriptFlags_Set40), the
// run and its step, or an area change - and 0 otherwise. By area: 0xA the
// cell x 0x69.8 at z 6 or 7; 7 three door cells; 8 the cell 0x54.8 at z
// 0x1C / 0x1D (with sound 0x202) or, with flag 6, the rectangle to area 8;
// 0x13 two rectangles; 0x17 a rectangle to area 0x17; 0x16 three by flags
// 0xC..0x12; 0x60 sets flag 0x18 over a strip and answers 0.
//
// As the original has it: every bound is a signed 32-bit compare of the
// argument, inclusive; the z tests of area 0xA and 8 are of the argument's
// high word alone; area 0x16's first rectangle changes the area without
// ScriptFlags_Set40 and stores counter 0 from the flag test's answer (0);
// each area test reads Game_AreaNumber afresh.
extern "C" unsigned char __cdecl Scena01_StepHook(int x, int z) {
    const unsigned zi = (static_cast<std::uint32_t>(z) >> 16) & 0xFFFF;
    if (Area() == 0xA && !Flag(0x1C) && x == 0x698000 && (zi == 6 || zi == 7)) {
        g.set40();
        SetStep5(0);
        SetRun(0x14);
        return 1;
    }
    if (Area() == 7 && !Flag(0x1B)) {
        const bool door = ((x == 0x10000 || x == 0x8000) && z >= 0x130000 && z <= 0x158000) ||
                          (x >= 0x150000 && x <= 0x168000 && (z == 0x10000 || z == 0x8000)) ||
                          (z >= 0x180000 && z <= 0x198000 && (x == 0x270000 || x == 0x278000));
        if (door) {
            g.set40();
            SetStep5(0xA);
            SetRun(0xB);
            return 1;
        }
    }
    if (Area() == 8) {
        if (!Flag(6)) {
            if (x == 0x548000 && (zi == 0x1C || zi == 0x1D)) {
                g.set40();
                g.sound(0x202);
                SetRun(0x15);
                return 1;
            }
        } else if (!Flag(5) && x >= 0x558000 && x <= 0x568000 && z >= 0x1C0000 && z <= 0x1D8000) {
            g.set40();
            Field_ScriptFlags = static_cast<unsigned short>((Field_ScriptFlags ^ 6) & 0xFF7F);
            g.change_area(8, 0x340000, 0x178000, 0x83);
            return 1;
        }
    }
    if (Area() == 0x13) {
        if (Flag(0x1B) && !Flag(2) && x >= 0x1A0000 && x <= 0x1C8000 && z >= 0x1A0000 && z <= 0x1D8000) {
            g.set40();
            Counter(0) = 0x1E;
            SetStep5(0);
            SetRun(3);
            return 1;
        }
        if (Flag(2) && !Flag(3) && x == 0x160000 && z >= 0xC0000 && z <= 0xD8000) {
            g.set40();
            SetStep5(0xA);
            Counter(0) = 0;
            SetRun(3);
            return 1;
        }
    }
    if (Area() == 0x17 && !Flag(0xA) && z >= 0xC8000 && z <= 0xD8000 && x >= 0x480000 && x <= 0x498000) {
        g.set40();
        Counter(0) = 0;
        Counter(2) = 7;
        g.change_area(0x17, 0x180000, 0x180000, 0x82);
        return 1;
    }
    if (Area() == 0x16 && Flag(0xC)) {
        if (!Flag(0xD)) {
            if (z >= 0x1F8000 && z <= 0x200000 && x >= 0x510000 && x <= 0x520000) {
                Counter(0) = 0;
                SetStep5(8);
                g.change_area(0x16, 0x58000, 0x140000, 0x80);
                return 1;
            }
        } else if (!Flag(0xF)) {
            if ((x == 0x28000 || x == 0x300000) && z >= 0x70000 && z <= 0x88000) {
                g.set40();
                Counter(0) = 0;
                SetStep5(0xB);
                g.change_area(0x16, 0x450000, 0xB8000, 0x81);
                return 1;
            }
        } else if (!Flag(0x12) && !Flag(0x10) && x == 0x3D0000 && z >= 0xA0000 && z <= 0xB8000) {
            g.set40();
            SetStep5(0x12);
            return 1;
        }
    }
    if (Area() == 0x60 && (x == 0x10000 || x == 0x18000) && z >= 0x110000 && z <= 0x128000) SetFlag(0x18);
    return 0;
}

// ===========================================================================
// The window cursor
// ===========================================================================

// original 0x5960D0 (no PSX twin paired; PSX 0x8015ABC8): the window kind 2's cursor, a tail
// jump from its state 2 (0x595C44) - Menu_DrawHand at the window 0x905B84's
// x + 4 and at its y plus the list set's first row and the row stride times
// the cursor's row: record 0x7DEE65 of the 6-byte table 0x66AE2C (+2 the
// first row, +4 the stride), the s8 row 0x7DEE67; the window's +4 / +6 are
// 12.4 words.
//
// As the original has it: the window words are shifted as signed 16 bits
// and the product and sums are 16-bit; the upper halves of both coordinates
// are the registers' leftovers in the original, and Menu_DrawHand reads the
// low words only, so ours passes them zero-extended.
extern "C" __attribute__((disable_tail_calls)) void __cdecl Window_DrawCursor(void) {
    const unsigned set = MenuWindows_ListSet;
    const unsigned char* const rows = mode_states::At(::bof3::Img(at::kCursorRows) + set * 6);
    const short row = AreaW0c_ChoiceAnswer;
    const unsigned char* const w = reinterpret_cast<const unsigned char*>(BattleWinStates_Current);
    const unsigned y = (mode_states::Word(rows + 4) * static_cast<unsigned>(row) + static_cast<unsigned>(static_cast<short>(mode_states::Word(w + 6)) >> 4) +
                        mode_states::Word(rows + 2)) & 0xFFFF;
    const unsigned x = (static_cast<unsigned>(static_cast<short>(mode_states::Word(w + 4)) >> 4) + 4) & 0xFFFF;
    mode_states::g.draw_hand(static_cast<int>(x), static_cast<int>(y), 0);
}

// ===========================================================================

void ModeStates_Inject() {
    if (bof3::WantsShadow("mode_states")) mode_states::SelfTest();
    BOF3_INJECT(Text_DrawSmall);
    BOF3_INJECT(Field_LoadingFrame);
    BOF3_INJECT(Menu_Frame);
    BOF3_INJECT(Shop_Frame);
    BOF3_INJECT(FieldCore_State2);
    BOF3_INJECT(FieldCore_Fade);
    BOF3_INJECT(FieldCore_FadeBegin);
    BOF3_INJECT(FieldCore_FadeStep);
    BOF3_INJECT(Scena01_Frame);
    BOF3_INJECT(Scena01_EnterArea);
    BOF3_INJECT(Scena01_StepHook);
    BOF3_INJECT(Window_DrawCursor);
}
