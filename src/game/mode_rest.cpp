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
#include "game/mode_rest_callees.h"
#include "game/sound_rest.h"
#include "game/move_script_bytes.h"
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
void SetUL(U a, U v) { SetLong(At(a), static_cast<std::int32_t>(v)); }
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
bool MusicWordAndSilent() { return Word(At(at::kMusicWord)) == 0x6E && SH_CALL(Sound_MusicPlaying)() == 0; }

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
    At(at::kMenuMode)[0] = 0;
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
    if (At(at::kPartyChanged)[0] != 0) {
        const unsigned c = At(at::kPartyList)[2], b = At(at::kPartyList)[1], a = At(at::kPartyList)[0];
        SH_CALL(PartySet_Load)(a, b, c, 0);
        WaitLoad();
        At(at::kPartyChanged)[0] = 0;
    }
    {
        const unsigned char set = static_cast<unsigned char>(At(at::kPartySet)[0] | 0x80);
        At(at::kPartySet)[0] = set;
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
    if (At(at::kTripAsked)[0] != 0) {
        const unsigned char out = At(at::kTripOut)[0];
        At(at::kTripAsked)[0] = 0;
        if (out == 0 && ((Field_InputFlags & 1) != 0 || Word(At(at::kAreaWord)) == at::kTripAnyArea)) {
            const U x = UL(at::kLeaderX), z = UL(at::kLeaderZ);
            const unsigned area = Word(At(at::kAreaWord));
            At(at::kTripOut)[0] = 1;
            SetUL(at::kTripX, x);
            SetUL(at::kTripZ, z);
            SetWord(At(at::kTripArea), area);
            const U r = static_cast<U>(SH_CALL(Rand)());
            At(at::kTripFacing)[0] =
                static_cast<unsigned char>((At(at::kFacingJitter)[r & 3] + At(at::kLeaderFacing)[0]) & 0xF);
            const unsigned place = Word(At(at::kPendingPlace));
            const U px = UL(at::kPendingX), pz = UL(at::kPendingZ);
            SH_CALL(Field_ChangeArea)(place, I(px), I(pz), At(at::kPendingFlags)[0]);
        } else {
            At(at::kTripOut)[0] = 0;
            SH_CALL(Flags_Clear)(At(at::kStoryFlags), at::kTripFlag);
            const unsigned area = Word(At(at::kTripArea));
            const U x = UL(at::kTripX), z = UL(at::kTripZ);
            SH_CALL(Field_ChangeArea)(area, I(x), I(z), 4);
        }
    } else if (At(at::kAreaAsked)[0] != 0) {
        SH_CALL(Field_ChangeArea)(At(at::kAreaAsked)[0], I(at::kAreaAskedX), I(at::kAreaAskedZ), 4);
        At(at::kAreaAsked)[0] = 0;
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
        At(at::kCameraTurnByte)[0] = 0x10;
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
    const unsigned char flags = At(at::kBattleFlags)[0];
    U formation = UL(at::kFormation);
    if (!(flags & 1)) {
        const U i = (formation & 0xFF) * 2;
        const U elevation = static_cast<U>(MapView_Elevation);
        MoveScript_F3Divisor = 0x40;
        const unsigned dx = static_cast<std::uint16_t>(static_cast<signed char>(At(at::kApproach)[i]));
        const unsigned dz = static_cast<std::uint16_t>(static_cast<signed char>(At(at::kApproach)[i + 1]));
        SetWord(At(at::kKind2ZHigh), Word(At(at::kKind2ZHigh)) + dz);
        const U base = UL(at::kElevationBase);
        SetWord(At(at::kKind2XHigh), Word(At(at::kKind2XHigh)) + dx);
        MoveScript_FAWord = static_cast<unsigned short>(static_cast<U>(I(base - elevation) >> 4));
    }
    if (!(flags & 0x10)) {
        const unsigned set = At(at::kPartySet)[0] & 0x7Fu;
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
    if (!(At(at::kBattleFlags)[0] & 0x40) && Music_Track != 0xFF) SH_CALL(Music_FadeOutStop)(0xA);
    unsigned track;
    const unsigned char event = At(at::kEventBattle)[0];
    if (event != 0) {
        const unsigned asset = At(at::kEventAsset)[event * 4u];
        SH_CALL(LoadDatFile)(static_cast<int>(Word(At(at::kEventFile + asset * 4u))));
        const unsigned again = At(at::kEventAsset)[At(at::kEventBattle)[0] * 4u];
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
    At(at::kBattleFlags3)[0] = 0;
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
    At(at::kBattleFlags3)[0] = 0;
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
        const unsigned char flags = At(at::kBattleFlags)[0];
        Field_EdgeBits = 0;
        if (!(flags & 2)) {
            const long z = Field_Kind2Z, x = Field_Kind2X;
            const long height = SH_CALL(AreaMap_Elevation)(x, z);
            SH_CALL(MapView_SetElevation)(static_cast<int>(height));
        }
        if (!(At(at::kBattleFlags)[0] & 0x40) && !(At(at::kBattleFlags2)[0] & 8)) {
            const unsigned char track = Music_Track;
            if (track != 0xFF) SH_CALL(Music_Play)(track, 0xA);
        }
        At(at::kBattleFlags)[0] = 0;
        At(at::kEventBattle)[0] = 0;
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
