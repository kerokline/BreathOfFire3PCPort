// The top-level task flow: the boot task WinMain starts (0x496B60) and the
// title's loader it hands over to (0x496C90); the field task 0x495800 with
// its mode-0 and mode-1 handlers, the play clock it ticks every frame, and
// the area-entry hub 0x594E60 under mode 1; the screen-transition task
// 0x495070 with its 21 kinds and the fade loops under them; and the small
// resets they share. docs/mode-flow.md.
//
// Every call goes through mode_flow::g (mode_flow_callees.h), so that the
// start-up fuzz can stand recorders in for them - for ours and for the
// originals' copies alike.
#include "game/mode_flow.h"

#include "game/area_data_reload.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/mode_flow_callees.h"
#include "game/widescreen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "input/sigma.h"
#include "hook/log.h"

namespace mode_flow {

std::uint32_t g_area_track_fallback = 0;  // DIV-0092; 1 through PatchBytes("AreaTrackFallback") in ModeRest_Inject

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::Word;

namespace {
template <typename T> T Fn(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }
}  // namespace

const Callees kOriginals = {
    Task_Sleep, Task_Exit, Task_Create, Task_ClearPrivate,
    Transition_FadeSub, Transition_FadeAdd, Transition_DrawTile, Window_ResetAll, Game_ClockTick,
    Transition_Start, Field_WaitTransition, Area_Enter, Area_EntryWalk, MoveScript_TintReset,
    Field_SlotRelease, Field_SlotsReleaseAll, Gfx_ClutStripRestore, File_LoadDone, Gfx_ClearRect,
    Gpu_GetTPage, Gpu_SetDrawMode, Gfx_CommitPrim, Gpu_SetTile, Gpu_SetSemiTrans,
    Field_PartyLoad, Field_ChangeArea, Fn<void (__cdecl*)(unsigned, unsigned)>(::bof3::Code(kEntryPoint.v)), Field_EncounterThresholdRoll,
    Field_Frame, Effect_ClearAll, Music_Play, LoadDatFile, Field_PartySetUp, Area_ZoneIdAt, Field_ViewReset,
    Gte_SetColorMatrix, Scenario_Start, Area_RunPlacement, Flags_Clear, Field_ModeDispatch, Field_PartyFirstFrame,
    MoveCmd_TestFB, Fn<unsigned char (__cdecl*)(unsigned)>(::bof3::Code(kPlaceParty.v)), Field_MembersFrame, Port_DroppedCall,
    Title_Task, Gfx_ClearImage,
    {Transition_Kind00, Transition_Kind01, Transition_Kind02, Transition_Kind03, Transition_Kind04,
     Transition_Kind05, Transition_Kind06, Transition_Kind07, Transition_Kind08, Transition_Kind09,
     Transition_Kind10, Transition_Kind11, Transition_Kind12, Transition_Kind13, Transition_Kind14,
     Transition_Kind15, Transition_Kind16, Transition_Kind17, Transition_Kind18, Transition_Kind19,
     Transition_Kind20},
};
Callees g = kOriginals;

}  // namespace mode_flow

using namespace mode_flow;

// ===========================================================================
// The screen transition (0x495040..0x495800)
// ===========================================================================

// ===========================================================================
// The field task (0x495800) and its modes 0 and 1
// ===========================================================================

// original 0x496870 (PSX 0x80199CAC): the play clock and two countdowns, once a frame from
// Field_Task (PSX 0x80199CAC, gap pairing - the same code). Unless
// Field_Request is 9: the clock's frames count to 30, seconds and minutes to
// 60, hours to 99, and at 99:59:59 it stops; then each countdown ticks.
//
// As the original has it: the stop is hours == 0x63 && minutes == 0x3B &&
// seconds == minutes, exactly (hours above 99 keep counting); the rollover
// tests are unsigned (>= 30, >= 60); at 99 hours a rollover of the minutes
// holds the clock at 99:59:59 rather than counting the hour.
// original 0x594E60: entering an area, once per area change from
// GameMode_Enter (PSX 0x801A0AA8, callers pairing). docs/mode-flow.md
// section 4 has it step by step.
//
// As the original has it: flag 0x80 of the entry flags sets or clears bit 15
// of Field_ScriptFlags2 at once and is kept (the original writes it over the
// flags' own argument slot) for the end; the area's DAT is loaded only when
// the area number differs AND Field_Request is 5; the flags go on whole to
// Field_PartySetUp; Area_Descriptors[Game_AreaNumber] is read afresh after
// every call; an area whose descriptor has no colour matrix at +0x38 gets
// the default one at 0x6BE090 and the light pair at 0x663B20; the scenario
// chapter steps by one with 0x10 skipped (to 0x11) when Field_StatusBits has
// bit 7; the descriptor's +0x40, when not null, is called with nothing.
extern "C" void __cdecl Area_Enter(unsigned area, int x, int z, unsigned flags) {
    const bool drop_in = (flags & 0x80) != 0;
    if (drop_in) Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 | 0x8000);
    else Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0x7FFF);
    mode_flow::g.effect_clear();
    mode_flow::g.tint_reset();
    const std::uint16_t previous = Game_AreaNumber;
    const unsigned char zone = Cond_ByteFD;
    const long kind2_x = Field_Kind2X, kind2_z = Field_Kind2Z;
    AreaW2c_LastArea = previous;
    AreaW1a_EntryZone = zone;
    mode_flow::SetLong(mode_flow::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x220), kind2_z);
    mode_flow::SetLong(mode_flow::At(::bof3::Img(MoveScript_TintRecords_addr) + 0x21c), kind2_x);
    const unsigned char track = AreaW1f_MusicCurrent;
    if (track != 0xFF) mode_flow::g.music_play(track, 8);
    else Music_Track = 0xFF;
    if (Game_AreaNumber != static_cast<std::uint16_t>(area) && Field_Request == 5) {
        Game_AreaNumber = static_cast<std::uint16_t>(area);
        mode_flow::g.load_dat(static_cast<int>((area & 0xFFFF) + 3));
        while (mode_flow::g.load_done() == 0) mode_flow::g.sleep(1);
        AreaDataReload_OnLoad(area & 0xFFFF);   // DIV-0182: the area's own .data as the disc holds it
        Field_InputFlags = Area_Descriptors[Game_AreaNumber][0x32];
        mode_flow::g.clut_restore();
        Gfx_ClutStripDirty = 1;
    }
    mode_flow::g.party_set_up(x, z, flags);
    Cond_ByteFD = mode_flow::g.zone_id(mode_flow::Word(mode_flow::At(::bof3::Img(at::kKind2XHigh))), mode_flow::Word(mode_flow::At(::bof3::Img(at::kKind2ZHigh))));
    mode_flow::g.view_reset();
    const auto colour = static_cast<std::uint32_t>(mode_flow::Long(Area_Descriptors[Game_AreaNumber] + 0x38));
    if (colour != 0) {
        mode_flow::g.colour_matrix(Fn<const unsigned long*>(colour));
        const unsigned char* const own = Fn<const unsigned char*>(
            static_cast<std::uint32_t>(mode_flow::Long(Area_Descriptors[Game_AreaNumber] + 0x38)));
        mode_flow::SetLong(mode_flow::At(::bof3::Img(0x903598)), mode_flow::Long(own + 0x20));   // Light_Angles
        mode_flow::SetLong(mode_flow::At(::bof3::Img(0x90359C)), mode_flow::Long(own + 0x24));
    } else {
        mode_flow::g.colour_matrix(Fn<const unsigned long*>(::bof3::Img(at::kDefaultColour)));
        mode_flow::SetLong(mode_flow::At(::bof3::Img(0x903598)), static_cast<std::int32_t>(ModeFlow_DefaultLight));
        mode_flow::SetLong(mode_flow::At(::bof3::Img(0x90359C)), mode_flow::Long(mode_flow::At(::bof3::Img(ModeFlow_DefaultLight_addr) + 4)));
    }
    if (Field_StatusBits & 0x80) {
        auto chapter = static_cast<unsigned char>(Cond_ByteFA + 1);
        Cond_ByteFA = static_cast<signed char>(chapter);
        if (chapter == 0x10) {
            chapter = 0x11;
            Cond_ByteFA = 0x11;
        }
        mode_flow::g.scenario_start(chapter);
    }
    mode_flow::g.run_placement(Fn<const unsigned char*>(static_cast<std::uint32_t>(mode_flow::Long(Area_Descriptors[Game_AreaNumber]))));
    const auto hook = static_cast<std::uint32_t>(mode_flow::Long(Area_Descriptors[Game_AreaNumber] + 0x40));
    if (hook != 0) Fn<mode_flow::Handler>(hook)();
    mode_flow::g.flags_clear(mode_flow::At(::bof3::Img(Cond_Flags_addr) + 0xa0), 0x1C);
    mode_flow::At(::bof3::Img(0x8034E2))[0] = 1;   // Field_StatusBits + 1
    mode_flow::g.mode_dispatch();
    mode_flow::g.first_frame();

    const std::uint16_t bits = Field_ScriptFlags2;
    if (bits & 0x8000) {
        if (drop_in) {
            EventLeader_PendingFlag = 0;
            mode_flow::g.place_party(flags & ~0x80u);
            Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xFFEF);
            mode_flow::g.members_frame();
        }
    } else if (!(Field_ScriptFlags & 0x800)) {
        bool walked = true;
        if (bits & 0x80) mode_flow::g.entry_walk(1, x, z);
        else if (bits & 0x100) mode_flow::g.entry_walk(2, x, z);
        else if (bits & 0x200) mode_flow::g.entry_walk(3, x, z);
        else if (Field_MemberCount > 1) {
            mode_flow::g.entry_walk(0, x, z);
            if (!(Field_ScriptFlags & 0x800) && !(Field_ScriptFlags2 & 0x8000))
                mode_flow::g.test_fb(static_cast<short>(mode_flow::Word(mode_flow::At(::bof3::Img(at::kKind2XHigh)))), static_cast<short>(mode_flow::Word(mode_flow::At(::bof3::Img(at::kKind2ZHigh)))));
        } else {
            walked = false;
        }
        if (walked) {
            const unsigned char members = Field_MemberCount;
            if (members > 1) {
                auto v = static_cast<std::uint16_t>(Field_ScriptFlags2 | 0x12);
                if (members == 3) v = static_cast<std::uint16_t>(v | 4);
                Field_ScriptFlags2 = static_cast<std::uint16_t>(v & 0xEE7F);
                return;
            }
        }
    }
    Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 & 0xEE7F);
}

// original 0x595160 (PSX 0x801A0F90, gap pairing): the party walks in from
// the area's edge - the walk on, its direction, a zero, the point (x, z);
// with a direction bit 3 of Field_ScriptFlags2 set; each of Field_MemberCount
// members' two walk bytes: the direction's pose from 0x66ADBC, and bit 1.
//
// As the original has it: the direction is the argument's low byte; the pose
// table is indexed by it unchecked (callers pass 0..3); the member count is
// read once, as a byte.
extern "C" void __cdecl Area_EntryWalk(unsigned direction, int x, int z) {
    const auto dir = static_cast<unsigned char>(direction);
    if (dir != 0) Field_ScriptFlags2 = static_cast<std::uint16_t>(Field_ScriptFlags2 | 8);
    EventLeader_PendingX = x;
    const unsigned members = Field_MemberCount;
    EventLeader_PendingFlag = 1;
    mode_flow::At(::bof3::Img(EventLeader_PendingFlag_addr))[1] = dir;
    mode_flow::At(::bof3::Img(EventLeader_PendingFlag_addr))[2] = 0;
    EventLeader_PendingZ = z;
    if (members == 0) return;
    const unsigned char pose = mode_flow::At(::bof3::Img(at::kWalkPoses))[dir];
    for (unsigned i = 0; i < members; ++i) {
        unsigned char* const m = mode_flow::At(::bof3::Img(ObjTrio_addr) + 0x137 + i * 0x14C);
        m[0] = pose;
        m[1] = static_cast<unsigned char>(m[1] | 2);
    }
}

// original 0x59E330 (PSX 0x8015990C, the JP executable): every window
// record's in-use, handler, kind, state and pass bytes (+0..+3, +0xF)
// zeroed, and the pass the walk starts from. No calls.
//
// As the original has it, and the JP disc: the other fields keep what the
// last window left, and one is read before any write - +0xD bit 0 (no
// frame), which Window_Grow / Window_Shrink read for kinds 1 and 2 and never
// set, so a list or battle window that set it leaves the next window
// frameless. The US executable's reset (PSX 0x80158E50) zeroes +0..+0x1F of
// every record - the bytes, the words +4 / +6 and the eight words +0x10..
// +0x1E - and leaves +0x20..+0x23, as here.
//
// DIVERGENCE DIV-0175 (known-defects D352): 1 through
// PatchBytes("WindowResetFull") in ModeFlow_Inject, after the self-test,
// which compared Capcom's five-field reset.
namespace mode_flow { std::uint8_t g_reset_full = 0; }

extern "C" void __cdecl Window_ResetAll(void) {
    for (unsigned i = 0; i < 22; ++i) {
        unsigned char* const r = mode_flow::At(::bof3::Img(WindowRecords_addr) + i * 0x24);
        if (mode_flow::g_reset_full) {
            std::memset(r, 0, 0x20);
            continue;
        }
        r[0] = 0;
        r[1] = 0;
        r[2] = 0;
        r[3] = 0;
        r[0xF] = 0;
    }
    BossSa_WindowPass = 0;
}

// ===========================================================================
// Boot (0x496B60, 0x496C90) and the small resets
// ===========================================================================

// original 0x454810: `mov eax, 1; ret`. Where the PSX polled its CD load
// (File_LoadDone 0x801636F0), the PC's loads are synchronous, so every wait
// loop's first question is answered "done".
extern "C" int __cdecl File_LoadDone(void) { return 1; }

// original 0x4549B0 (PSX 0x8014E178, the same loops): the last 0x800 bytes of
// Gfx_ClutStripSource - its rows 28..31 - zeroed, then all 32 rows of 0x200
// bytes copied to Gfx_ClutStrip. So the copy carries the zeros: the strip's
// rows 28..31 come out zero too. The PSX zeroes the same four rows first.
// Gfx_ClutStripDirty is not touched.
extern "C" void __cdecl Gfx_ClutStripRestore(void) {
    std::memset(mode_flow::At(::bof3::Img(Gfx_ClutStripSource_addr) + 0x3800), 0, 0x800);
    std::memcpy(Gfx_ClutStrip, Gfx_ClutStripSource, 0x4000);
}

// original 0x454A20 (PSX 0x8014E2E8): byte +0 of the 32 MoveScript_TintRecords
// zeroed, then 16 dwords from 0x7E06A0 set to -1.
extern "C" void __cdecl MoveScript_TintReset(void) {
    for (unsigned i = 0; i < 32; ++i) MoveScript_TintRecords[i * 12] = 0;
    for (unsigned i = 0; i < 16; ++i) mode_flow::SetLong(mode_flow::At(::bof3::Img(at::kTintMarks) + i * 4), -1);
}

// original 0x454A50 (PSX 0x8014E378): the Field_Slots record `slot` (its low
// byte) freed - byte +0 zeroed - if its bit 0 is set. As the original has it:
// the index is not checked (callers pass 0..7).
extern "C" void __cdecl Field_SlotRelease(unsigned slot) {
    unsigned char* const r = Field_Slots + (slot & 0xFF) * 16u;
    if (r[0] & 1) r[0] = 0;
}

// original 0x454AB0: Field_SlotRelease for each of the eight slots.
extern "C" void __cdecl Field_SlotsReleaseAll(void) {
    for (unsigned i = 0; i < 8; ++i) mode_flow::g.slot_release(i);
}

// ===========================================================================

void ModeFlow_Inject() {
    if (bof3::WantsShadow("mode_flow")) mode_flow::SelfTest();
    {
        // DIV-0175: after the self-test, which compared Capcom's five-field reset.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("WindowResetFull", Low32(&mode_flow::g_reset_full), &was, &is, 1);
        bof3::Log("DIV-0175    the window reset clears +0..+0x1F of every record, as the US disc (on unless the line above says OFF)");
    }
    BOF3_INJECT(Area_Enter);
    BOF3_INJECT(Area_EntryWalk);
    BOF3_INJECT(Window_ResetAll);
    BOF3_INJECT(File_LoadDone);
    BOF3_INJECT(Gfx_ClutStripRestore);
    BOF3_INJECT(MoveScript_TintReset);
    BOF3_INJECT(Field_SlotRelease);
    BOF3_INJECT(Field_SlotsReleaseAll);
}
