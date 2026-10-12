// The launcher's music player (docs/music-player.md): the game's songs from
// whichever sources the install has - the PlayStation disc's sequences through
// the importer's cache, the PC port's MP3s, and (once its import exists) the
// PSP disc's - one song at a time, looping without end. Nothing of the game
// runs; the songs are played by our own sequencer and decoder.
#pragma once

#include <windows.h>

#include <string>

namespace bof3x {

// Modal over `owner`. `game_dir` holds the PC's BGM\; `cache` is the
// importer's cache folder (its base\bgm\ the disc's songs, and a PSP disc's in
// opt\psp-music\bgm\ beside a PlayStation base), empty for none. `data_root`
// is where the game finds layers (ConfigCacheDataRoot), and `opt` the
// settings' opt= as read. The battle-chord box records the player's choice in
// `chord_choice` (-1 untouched, 0 off, 1 on); the settings dialog makes opt=
// from the line as read and every choice at once, on Play (SuitesOptEdit), so
// a box ticked and unticked again writes the line back byte for byte.
void MusicPlayerRun(HWND owner, const std::wstring& game_dir, const std::wstring& cache,
                    const std::wstring& data_root, const std::string* opt, int* chord_choice);

}  // namespace bof3x
