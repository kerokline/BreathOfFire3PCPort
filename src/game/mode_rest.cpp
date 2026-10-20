// Group PM of the platform round (step 2, 2026-10-05; docs/mode-rest.md): game
// modes 3..6 - GameMode_Handlers' four entries no catalogue held - with the
// steps of modes 3 and 5 not already ours, and the jump 0x587C20 under mode 3.
// Each read with capstone to its last instruction (2026-10-05):
//
//   GameMode3_Run    0x495BB0  mode 3, the field menu: GameMode3_Steps by Game_Step
//   GameMode3_Enter  0x495BC0  its step 0
//   GameMode3_Leave  0x495C50  its step 2 (step 1 is Menu_Frame, ours)
//   GameMode4_Run    0x495E60  mode 4: the scripted field frame until the message bit
//   GameMode5_Run    0x495E90  mode 5, the battle: GameMode5_Steps by Game_Step
//   GameMode5_TurnSense .. GameMode5_Leave  0x495EA0 .. 0x496150  its steps 0..4, 6, 7
//                              (step 5 is Battle_Frame, ours)
//   GameMode6_Run    0x496230  mode 6: Look_PadControl or GameMode_LookEnd, a loading frame
//   (Sound_MusicPlaying 0x587C20, a jmp to Music_IsPlaying, is group PS's - sound_rest.cpp)
//
// Every call goes through the harness (SH_CALL / SH_AT), so the start-up fuzz
// can stand recorders in for ours as for the originals' copies; a memory cell
// the original reads after a call is read after it here. No divergence: each
// is a faithful replacement. Where the original jumps through a step table
// past its end, ours aborts with a message (docs/mode-rest.md section 6).
#include "game/mode_rest.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/mode_flow.h"
#include "game/mode_rest_callees.h"
#include "game/window_task.h"
#include "game/music_resume.h"
#include "game/sound_rest.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = mode_rest::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = scenario_harness::Handler;

U UL(U a) { return static_cast<U>(Long(At(a))); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
U AddressOf(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
void StepUp() { Game_Step = static_cast<unsigned short>(Game_Step + 1); }

// xor eax, eax; mov ax, [Game_Step]; jmp [table + eax * 4]: the table's
// `entries` steps read in place (the fuzz swaps the cells for recorders); a
// Fatal past them, where the original jumps through the dword after - for mode
// 3 the facing jitter's four bytes, for mode 5 the approach table's.
void StepDispatch(const char* who, U table, unsigned entries) {
    const unsigned step = Game_Step;
    if (step >= entries)
        bof3::Fatal("%s: Game_Step is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/mode-rest.md section 6)",
                    who, step, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(table + 4 * step)))();
}

// call File_LoadDone; test eax, eax; jne done; loop: Task_Sleep(1);
// File_LoadDone; test; je loop.
void WaitLoad() {
    if (SH_CALL(File_LoadDone)() != 0) return;
    do SH_CALL(Task_Sleep)(1);
    while (SH_CALL(File_LoadDone)() == 0);
}

// cmp word [0x7E0678], 0x6E; jne; call 0x587C20; test eax, eax; jne: the word
// first, the whole of eax.
bool MusicWordAndSilent() { return ModeRest_MusicWord == 0x6E && SH_CALL(Sound_MusicPlaying)() == 0; }

// The battle steps' tail: the field objects' frame and a loading frame.
void BattleFrames() {
    SH_CALL(Field_ObjectsFrame)();
    SH_CALL(Field_LoadingFrame)();
}

}  // namespace

// ===========================================================================
// Mode 3, the field menu (Field_Request 1, GameMode_Field)
// ===========================================================================

// original 0x495BB0 (GameMode_Handlers[3]; PSX 0x801985E4): jmp
// [GameMode3_Steps + Game_Step * 4], unchecked.
extern "C" void __cdecl GameMode3_Run(void) {
    StepDispatch("GameMode3_Run", AddressOf(GameMode3_Steps), GameMode3_Steps_count);
}

// original 0x495BC0 (GameMode3_Steps[0]; PSX 0x80198620, which loads 0x269):
// the menu's DAT 0x31E and its wait (a sleep a try); CLUT strip rows 1 and 2;
// Gfx_ClutStripDirty 1, the menu block's mode byte 0 and byte +4 3 - stored
// before Transition_Start(3) (pushed first); Menu_WaitTransition(1); with the
// word 0x7E0678 0x6E and the music not playing, Music_FadeOut(0x10); Game_Step
// up.
extern "C" void __cdecl GameMode3_Enter(void) {
    SH_CALL(LoadDatFile)(static_cast<int>(at::kMenuFile));
    WaitLoad();
    SH_CALL(Gfx_ClutStripCopyRow)(1);
    SH_CALL(Gfx_ClutStripCopyRow)(2);
    Gfx_ClutStripDirty = 1;
    BattleE1_Mode = 0;
    At(at::kMenuTimer)[0] = 3;
    SH_CALL(Transition_Start)(3);
    SH_CALL(Menu_WaitTransition)(1);
    if (MusicWordAndSilent()) SH_CALL(Music_FadeOut)(0x10);
    StepUp();
}

// original 0x495C50 (GameMode3_Steps[2]; PSX 0x801986EC): the menu's last
// frame and its fade out, then the field back:
//   - Menu_Frame; Transition_Start(2); Menu_WaitTransition(0); a sleep;
//   - 0x929F11 set: PartySet_Load(the list 0x904062..64, 0) and its wait,
//     0x929F11 0;
//   - the party set 0x90412C |= 0x80, Snd_LoadBankFile(its low seven bits +
//     0x2C2), not waited for;
//   - Field_InputFlags bit 4: DAT 0x12A and its wait, CommuSim_RollOffers;
//   - Transition_Start(3); Field_WaitTransition(1); the word 0x7E0678 0x6E and
//     the music not playing: Music_FadeIn(0x10);
//   - 0x905B60 set (cleared): with 0x904152 0 and (Field_InputFlags bit 0 or
//     area 0xBD), out - the leader's x, z and the area saved to 0x904148 /
//     0x90414C / 0x904150, 0x904152 1, the facing (0x656A80[Rand & 3] + the
//     leader's) & 0xF to 0x904153, Field_ChangeArea(0x937F82, 0x903860,
//     0x90384C, 0x905B88); otherwise home - 0x904152 0, story flag 0x77
//     cleared, Field_ChangeArea(the saved area, x, z, 4);
//   - else 0x905B61 set: Field_ChangeArea(it, 0x430000, 0x160000, 4), cleared;
//   - else Field_Request 0;
//   - Window_ResetAll; Game_Mode 2, Game_Step 0.
// As the original has it: the arguments with stale upper bytes (the list, the
// areas, the flags) are read by their callees as bytes and words (masked in
// the fuzz, scenario_harness.cpp's rows); the leader's facing, the pending
// area's cells and the jitter are read after Rand, the saved trip after
// Flags_Clear.
extern "C" void __cdecl GameMode3_Leave(void) {
    SH_CALL(Menu_Frame)();
    SH_CALL(Transition_Start)(2);
    SH_CALL(Menu_WaitTransition)(0);
    SH_CALL(Task_Sleep)(1);
    if (MenuLists_PartyChanged != 0) {
        const unsigned c = Cond_Flags[0xd4], b = Cond_Flags[0xd3], a = Cond_Flags[0xd2];
        SH_CALL(PartySet_Load)(a, b, c, 0);
        WaitLoad();
        MenuLists_PartyChanged = 0;
    }
    {
        const unsigned char set = static_cast<unsigned char>(AreaW0b_PartySet | 0x80);
        AreaW0b_PartySet = set;
        SH_CALL(Snd_LoadBankFile)((set & 0x7Fu) + at::kPartySetFiles);
    }
    if (Field_InputFlags & 0x10) {
        SH_CALL(LoadDatFile)(static_cast<int>(at::kCommuFile));
        WaitLoad();
        SH_CALL(CommuSim_RollOffers)();
    }
    SH_CALL(Transition_Start)(3);
    SH_CALL(Field_WaitTransition)(1);
    if (MusicWordAndSilent()) SH_CALL(Music_FadeIn)(0x10);
    if (MenuLists_CampChosen != 0) {
        const unsigned char out = AreaW3c_CampFlag;
        MenuLists_CampChosen = 0;
        if (out == 0 && ((Field_InputFlags & 1) != 0 || Game_AreaNumber == at::kTripAnyArea)) {
            const U x = static_cast<U>(ObjTrio[0].x), z = static_cast<U>(ObjTrio[0].z);
            const unsigned area = Game_AreaNumber;
            AreaW3c_CampFlag = 1;
            AreaW2d_ReturnPoint = x;
            AreaW4d_ReturnZ = z;
            AreaW4d_ReturnArea = area;
            const U r = static_cast<U>(SH_CALL(Rand)());
            AreaW3g_HealByte =
                static_cast<unsigned char>((At(at::kFacingJitter)[r & 3] + ObjTrio[0].direction) & 0xF);
            const unsigned place = AreaW0b_Place;
            const U px = static_cast<std::uint32_t>(at::kPendingX), pz = AreaEntry_PendingZ;
            SH_CALL(Field_ChangeArea)(place, I(px), I(pz), AreaEntry_PendingFlags);
        } else {
            AreaW3c_CampFlag = 0;
            SH_CALL(Flags_Clear)(At(Cond_Flags_addr + 0xa0), at::kTripFlag);
            const unsigned area = AreaW4d_ReturnArea;
            const U x = AreaW2d_ReturnPoint, z = AreaW4d_ReturnZ;
            SH_CALL(Field_ChangeArea)(area, I(x), I(z), 4);
        }
    } else if (ModeRest_AreaAsked != 0) {
        SH_CALL(Field_ChangeArea)(ModeRest_AreaAsked, I(at::kAreaAskedX), I(at::kAreaAskedZ), 4);
        ModeRest_AreaAsked = 0;
    } else {
        Field_Request = 0;
    }
    SH_CALL(Window_ResetAll)();
    Game_Mode = 2;
    Game_Step = 0;
}

// ===========================================================================
// Mode 4 (Field_Request 2: a message open)
// ===========================================================================

// original 0x495E60 (GameMode_Handlers[4]; PSX 0x801989D4): with the message
// cells' 0x7DEE44 bit 1, 0x905B82 = 0x10, Field_Request 0, Game_Mode 2; a tail
// jump to Field_FrameScripted either way.
extern "C" void __cdecl GameMode4_Run(void) {
    if (At(at::kMessageBits)[0] & 2) {
        EventOps_BlockByte = 0x10;
        Field_Request = 0;
        Game_Mode = 2;
    }
    SH_CALL(Field_FrameScripted)();
}

// ===========================================================================
// Mode 5, the battle (Field_Request 3)
// ===========================================================================

// original 0x495E90 (GameMode_Handlers[5]; PSX 0x80198A24): jmp
// [GameMode5_Steps + Game_Step * 4], unchecked.
extern "C" void __cdecl GameMode5_Run(void) {
    StepDispatch("GameMode5_Run", AddressOf(GameMode5_Steps), GameMode5_Steps_count);
}

// original 0x495EA0 (GameMode5_Steps[0]; PSX 0x80198A60):
// Encounter_PartyTurnSense, the two frames, Game_Step up.
extern "C" void __cdecl GameMode5_TurnSense(void) {
    SH_CALL(Encounter_PartyTurnSense)();
    BattleFrames();
    StepUp();
}

// original 0x495EC0 (GameMode5_Steps[1]; PSX 0x80198AA4): Encounter_PartyTurn
// (al), the two frames; once it answers non-zero: Port_DroppedCall(1); then by
// the flags 0x904AE5 and the dword 0x904AAC (both read once, after it):
//   - bit 0 clear: MoveScript_F3Divisor 0x40; the upper words of Field_Kind2Z
//     and _Kind2X stepped by the s8 pair 0x656AA4[formation byte * 2] (+1 to
//     z, +0 to x); MoveScript_FAWord = (the dword 0x939860 - MapView_Elevation)
//     sar 4;
//   - bit 4 clear: PartySet_Select(the party set's low seven bits, formation
//     bit 1 ? 2 : 1) and the formation read again;
// Game_Step up, Draw_OtSlot 4, Draw_SortOnX = formation bit 0, MapView_Redraw 2.
// As the original has it: the formation byte indexes the pairs unbounded
// (docs/mode-rest.md section 6); the elevation is read before the stores.
extern "C" void __cdecl GameMode5_Turn(void) {
    const unsigned char turned = SH_CALL(Encounter_PartyTurn)();
    BattleFrames();
    if (turned == 0) return;
    SH_CALL(Port_DroppedCall)(1);
    const unsigned char flags = EventBattle_Flags;
    U formation = UL(at::kFormation);
    if (!(flags & 1)) {
        const U i = (formation & 0xFF) * 2;
        const U elevation = static_cast<U>(MapView_Elevation);
        MoveScript_F3Divisor = 0x40;
        const unsigned dx = static_cast<std::uint16_t>(static_cast<signed char>(At(at::kApproach)[i]));
        const unsigned dz = static_cast<std::uint16_t>(static_cast<signed char>(At(at::kApproach)[i + 1]));
        SetWord(At(at::kKind2ZHigh), Word(At(at::kKind2ZHigh)) + dz);
        const U base = ModeRest_ElevationBase;
        SetWord(At(at::kKind2XHigh), Word(At(at::kKind2XHigh)) + dx);
        MoveScript_FAWord = static_cast<unsigned short>(static_cast<U>(I(base - elevation) >> 4));
    }
    if (!(flags & 0x10)) {
        const unsigned set = AreaW0b_PartySet & 0x7Fu;
        SH_CALL(PartySet_Select)(set, (formation & 2) ? 2u : 1u);
        formation = UL(at::kFormation);
    }
    StepUp();
    Draw_OtSlot = 4;
    Draw_SortOnX = static_cast<unsigned char>(formation & 1);
    MapView_Redraw = 2;
}

// original 0x495F90 (GameMode5_Steps[2]; PSX 0x80198C1C):
// Encounter_PartyToPlaces (al), the two frames; Game_Step up when File_LoadDone
// (asked first), that answer and Field_Kind2Hold 0.
extern "C" void __cdecl GameMode5_ToPlaces(void) {
    const unsigned char placed = SH_CALL(Encounter_PartyToPlaces)();
    BattleFrames();
    if (SH_CALL(File_LoadDone)() != 0 && placed != 0 && Field_Kind2Hold == 0) StepUp();
}

// original 0x495FD0 (GameMode5_Steps[3]; PSX 0x80198C94): Field_PartyLoad(1),
// Encounter_PartyAtPlaces, the two frames; Music_FadeOutStop(0xA) unless
// 0x904AE5 bit 6 or Music_Track 0xFF; the battle's file and track - an event
// battle's from its record (EventBattle_Records +3 indexing the word pairs
// 0x64DECC, the record read again after the load), else DAT 0xD2 and track
// 0x97 below chapter 8 (Cond_ByteFA, s8), DAT 0xD3 and 0x99 from it;
// Music_Play(track, 0xA); Window_ResetAll; Game_Step up; 0x904AE9 0, the word
// 0x904AA8 0, the bytes 0x904AA0..A4 0.
extern "C" void __cdecl GameMode5_Load(void) {
    SH_CALL(Field_PartyLoad)(1);
    SH_CALL(Encounter_PartyAtPlaces)();
    BattleFrames();
    // DIV-0092: the field's song and where it is heard, before the battle's
    // music takes over - our DLL's state, nothing written to the game's;
    // nothing remembered with the switch off (Capcom's rule, and the fuzz).
    music_resume::Remember();
    if (!(EventBattle_Flags & 0x40) && Music_Track != 0xFF) SH_CALL(Music_FadeOutStop)(0xA);
    unsigned track;
    const unsigned char event = EventBattle_Id;
    if (event != 0) {
        const unsigned asset = At(at::kEventAsset)[event * 4u];
        SH_CALL(LoadDatFile)(static_cast<int>(Word(At(at::kEventFile + asset * 4u))));
        const unsigned again = At(at::kEventAsset)[EventBattle_Id * 4u];
        track = Word(At(at::kEventTrack + again * 4u));
    } else if (Cond_ByteFA < static_cast<int>(at::kChapterSplit)) {
        SH_CALL(LoadDatFile)(static_cast<int>(at::kBattleFileA));
        track = at::kBattleTrackA;
    } else {
        SH_CALL(LoadDatFile)(static_cast<int>(at::kBattleFileB));
        track = at::kBattleTrackB;
    }
    SH_CALL(Music_Play)(track, 0xA);
    SH_CALL(Window_ResetAll)();
    StepUp();
    BattleMisc_BannerKinds = 0;
    SetWord(At(at::kBattleWord), 0);
    for (unsigned k = 0; k < 5; ++k) At(at::kBattleBytes)[k] = 0;
}

// original 0x4960D0 (GameMode5_Steps[4]; PSX 0x80198E54):
// Encounter_PartyScriptOnce (al), the two frames; when File_LoadDone (asked
// first) and that answer: MoveScript_F3Divisor 0, 0x904AE9 0, 0x904AA0..A4 0,
// Party_Count(1), Game_Step up, its al to 0x904AB0.
extern "C" void __cdecl GameMode5_Script(void) {
    const unsigned char done = SH_CALL(Encounter_PartyScriptOnce)();
    BattleFrames();
    if (SH_CALL(File_LoadDone)() == 0 || done == 0) return;
    MoveScript_F3Divisor = 0;
    BattleMisc_BannerKinds = 0;
    for (unsigned k = 0; k < 5; ++k) At(at::kBattleBytes)[k] = 0;
    const int count = SH_CALL(Party_Count)(1);
    StepUp();
    At(at::kMemberCount)[0] = static_cast<unsigned char>(count);
}

// original 0x496130 (GameMode5_Steps[6]; PSX 0x80198EFC): Party_PlaceAtSlots,
// Party_ScriptTicks, the two frames, Game_Step up.
extern "C" void __cdecl GameMode5_Place(void) {
    SH_CALL(Party_PlaceAtSlots)();
    SH_CALL(Party_ScriptTicks)();
    BattleFrames();
    StepUp();
}

// original 0x496150 (GameMode5_Steps[7]; PSX 0x80198F48): with Field_Kind2Hold
// 0 and File_LoadDone (in that order): Draw_OtSlot 6; Field_PartyLoad(0);
// Party_PlacesByList; Field_EdgeBits 0; unless 0x904AE5 bit 1,
// MapView_SetElevation(AreaMap_Elevation(Field_Kind2X, Field_Kind2Z)); the
// field's track again (Music_Play(Music_Track, 0xA)) unless 0x904AE5 bit 6,
// 0x904AE8 bit 3 or the track 0xFF; 0x904AE5 and the event battle 0;
// Field_ZoneCounterRoll(0); Field_AfterBattleTally. Then, either way,
// Party_ScriptTicks and the two frames; and after the first branch Game_Mode 2,
// Game_Step 0, Field_Request 0.
// As the original has it: 0x904AE5 is read twice, before and after the
// elevation's calls.
extern "C" void __cdecl GameMode5_Leave(void) {
    bool back = false;
    if (Field_Kind2Hold == 0 && SH_CALL(File_LoadDone)() != 0) {
        Draw_OtSlot = 6;
        SH_CALL(Field_PartyLoad)(0);
        SH_CALL(Party_PlacesByList)();
        const unsigned char flags = EventBattle_Flags;
        Field_EdgeBits = 0;
        if (!(flags & 2)) {
            const long z = Field_Kind2Z, x = Field_Kind2X;
            const long height = SH_CALL(AreaMap_Elevation)(x, z);
            SH_CALL(MapView_SetElevation)(static_cast<int>(height));
        }
        if (!(EventBattle_Flags & 0x40) && !(Battle_EndFlags & 8)) {
            unsigned char track = Music_Track;
            // DIV-0092: by now the fanfare's stopping fade (BattleEnd_ExitHook)
            // has left the track 0xFF, so Capcom's Music_Play(Music_Track) never
            // ran and the field came back silent; the PSX restarts the area's
            // song from its table. Ours plays the area's own track.
            // When that is the song the battle took over, it resumes where it
            // was heard (GameMode5_Load's Remember), as the PlayStation's does,
            // over music_resume's own fade.
            bool resume = false;
            if (track == 0xFF && mode_flow::g_area_track_fallback) {
                track = AreaW1f_MusicCurrent;
                // DIV-0092, second amendment (2026-10-11): after an event battle
                // (Field_StartEventBattle's byte, cleared below) the PlayStation
                // restarts the area's own song from its table - the owner heard
                // track 27 after the Nue fight on mednafen, where the current-track
                // byte held the script's 29. So: the zone's table track for the
                // leader's cell (what Area_PickMusic derives on an area change),
                // from its top, with Capcom's fade. Only a random encounter resumes.
                const unsigned char event = EventBattle_Id;
                if (event != 0) {
                    const unsigned char current = track;
                    track = Area_ZoneTrack(Area_ZoneAt(rv::Bytes(ObjTrio)[0x36], rv::Bytes(ObjTrio)[0x3a]));
                    AreaW1f_MusicCurrent = track;
                    bof3::Log("DIV-0092    event battle %u over: the zone's track %u from its top (the current byte held %u), no resume",
                              event, track, current);
                } else {
                    resume = track != 0xFF && music_resume::Arm(track);
                }
            }
            if (track != 0xFF) SH_CALL(Music_Play)(track, resume ? music_resume::kResumeFadeFrames : 0xA);
        }
        music_resume::Forget();  // a remembered position is used here or not at all
        EventBattle_Flags = 0;
        EventBattle_Id = 0;
        SH_CALL(Field_ZoneCounterRoll)(0);
        SH_CALL(Field_AfterBattleTally)();
        back = true;
    }
    SH_CALL(Party_ScriptTicks)();
    BattleFrames();
    if (back) {
        Game_Mode = 2;
        Game_Step = 0;
        Field_Request = 0;
    }
}

// ===========================================================================
// Mode 6 (Field_Request 4)
// ===========================================================================

// original 0x496230 (GameMode_Handlers[6]; PSX 0x80199170): Look_PadControl at
// Game_Step 0, GameMode_LookEnd at any other; a tail jump to
// Field_LoadingFrame.
extern "C" void __cdecl GameMode6_Run(void) {
    if (Game_Step == 0) SH_CALL(Look_PadControl)();
    else SH_CALL(GameMode_LookEnd)();
    SH_CALL(Field_LoadingFrame)();
}

// ===========================================================================
// 0x587C20: jmp Music_IsPlaying
// ===========================================================================

// Where the jump goes: Music_IsPlaying's address (Capcom's 0x5A7020, or the
// jump Inject put there to ours), or its recorder while the fuzz runs ours.
// Sound_MusicPlaying 0x587C20 (a five-byte jmp to Music_IsPlaying) is group PS's,
// src/game/sound_rest.cpp, taken the same night; GameMode3_Enter / _Leave call it
// through SH_CALL by name.

void ModeRest_Inject() {
    if (bof3::WantsShadow("mode_rest")) mode_rest::SelfTest();
    {
        // DIV-0092: armed here because ModeFlow_Inject (and its fuzz of
        // GameMode_Start) ran earlier in InjectAll; both fuzzes see Capcom's rule.
        const std::uint8_t was[] = {0, 0, 0, 0}, is[] = {1, 0, 0, 0};
        bof3::PatchBytes("AreaTrackFallback",
                         static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&mode_flow::g_area_track_fallback)), was, is, 4);
        bof3::Log("DIV-0092    the field's music after a battle, and after a load of a save without a track, is the area's own (on unless the line above says OFF)");
    }
    BOF3_INJECT(GameMode3_Run);
    BOF3_INJECT(GameMode3_Enter);
    BOF3_INJECT(GameMode3_Leave);
    BOF3_INJECT(GameMode4_Run);
    BOF3_INJECT(GameMode5_Run);
    BOF3_INJECT(GameMode5_TurnSense);
    BOF3_INJECT(GameMode5_Turn);
    BOF3_INJECT(GameMode5_ToPlaces);
    BOF3_INJECT(GameMode5_Load);
    BOF3_INJECT(GameMode5_Script);
    BOF3_INJECT(GameMode5_Place);
    BOF3_INJECT(GameMode5_Leave);
    BOF3_INJECT(GameMode6_Run);
}
