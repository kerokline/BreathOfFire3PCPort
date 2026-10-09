// DIVERGENCE DIV-0087 (docs/music-seq-engine.md): a BGM track the importer's
// cache holds - <BOF3X_CACHE>\base\bgm\NNN.DAT and its bank
// base\bgm\bank\NAME.DAT (docs/seq-format.md) - is synthesised from the
// PlayStation's sequence by psx::MusicSynth (src/audio/) instead of decoded
// from the PC's MP3. The seam is five points in sound.cpp (Music_LoadFile,
// Music_Start, Music_Decode, Music_SetVolume, Music_Pump's gate) and
// Music_Release; the ring, its notifications, the pump and the fades are the
// MP3 path's, untouched. A track the cache lacks plays its MP3 whatever the
// switch. Armed after every module's self-test (BOF3X_MUSIC, BOF3X_CACHE);
// until then, and with no cache, nothing here is reached.
#pragma once

#include <cstdint>

namespace music_seq {

// Music_LoadFile's seam, before the BGM names are formatted: when armed and
// the cache has base\bgm\NNN.DAT for `track`, reads it through the sound
// layer's file callees into a fresh Music_File (the old one freed, as the
// original does), sets Music_FileSize, Music_FileLoops (the song's loop flag)
// and Music_LoadedTrack, and returns true: the MP3 names are not tried.
// False, with nothing changed, otherwise.
bool LoadFile(unsigned track);

// Music_LoadFile's MP3 path is about to free `file` (the old Music_File): if
// it held the cache song, that song is no longer the one loaded.
void Forget(const void* file);

// Music_Start's seam: true when (file, size) is the cache song LoadFile put in
// Music_File - Music_Play handing it on. Sound_LoadStream's streams and every
// MP3 are not.
bool IsSong(const void* file, unsigned size);

// Music_Start for the cache song, where the MP3 path opens its decoder: the
// song's bank put into the synth unless it is the one there (kept while songs
// share it, as the PSX keeps the SEP's VAB resident), the synth started, and
// the stream marked as the synth's from here on (Active).
void Begin();

// The stream Music_Start last started is the synth's (until Music_Release or
// the next Music_Start of an MP3).
bool Active();

// Music_Decode for the synth: `size` bytes of 44,100 Hz 16-bit stereo. When a
// song that plays once has ended, the rest is zeros and Music_Finished is set,
// as the MP3 path does at its end of stream.
void Decode(unsigned char* dst, int size);

// Music_SetVolume for the synth's stream: the game's 0..127 as DirectSound
// hundredths of a decibel by libsnd's sequence-volume law - the volume
// truncated toward zero, below 1 taken as 1 (_SsVmSetSeqVol), above 127 as
// 127; amplitude (v / 127)^2, i.e. 4000 * log10(v / 127), rounded.
long Level(float volume);

// Music_Stop: the synth's song stopped (SsSepStop) with the buffer; the
// stream stays the synth's until it is released.
void Stop();

// Music_Release tearing the stream down: the synth stopped, the stream no
// longer the synth's. The bank stays loaded.
void Release();

// After every module's self-test: reads BOF3X_MUSIC (unset or seq: the cache's
// song when it has one; mp3: the PC's file always; anything else is fatal) and
// BOF3X_CACHE (unset: no cache; not a directory, or too long for the file
// layer: fatal; a directory without base\bgm: one log line, no cache).
void Arm();

// BOF3X_SHADOW=sound: the seam driven through Music_LoadFile, Music_Start,
// Music_Decode and Music_SetVolume on a synthetic song and bank made here, with
// the file layer, the heap and the DirectSound buffer stood in for; compared
// sample for sample against one MusicSynth::Render. Everything put back.
void SelfTest();

}  // namespace music_seq
