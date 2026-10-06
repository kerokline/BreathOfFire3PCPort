// Group PM of the platform round (step 2): the four game modes the catalogue
// never held - GameMode_Handlers entries 3..6 (0x495BB0, 0x495E60, 0x495E90,
// 0x496230) and the steps of modes 3 and 5 that were not ours, 0x495BB0..
// 0x496226 - and the five-byte jump 0x587C20 only mode 3 calls. Fourteen
// functions. docs/mode-rest.md.
//
// What they are, by the code:
//   - mode 3, the field menu (Field_Request 1): GameMode3_Steps - its entry
//     (the menu's file, the CLUT rows, the transition), Menu_Frame (ours
//     already), and its way back (the party set reloaded, the two-way trip or
//     an area asked by number, else back to the field);
//   - mode 4 (Field_Request 2, a message open): the field's scripted frame
//     until the message cells' bit 1;
//   - mode 5, the battle (Field_Request 3): GameMode5_Steps - the party turned
//     and placed, the battle's file and music, Battle_Frame (ours already),
//     and the way back to the field;
//   - mode 6 (Field_Request 4): Look_PadControl at step 0, GameMode_LookEnd
//     after (both ours already), and a loading frame;
//   (Sound_MusicPlaying 0x587C20, the jump to Music_IsPlaying the mode-3 steps call, is group PS's: sound_rest.h.)
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// group's inject and its fuzz.
#pragma once

void ModeRest_Inject();

namespace mode_rest {
// BOF3X_SHADOW=mode_rest: the start-up fuzz, mode_rest_fuzz.cpp. Clones the
// fourteen originals before ModeRest_Inject patches them.
void SelfTest();
}  // namespace mode_rest
