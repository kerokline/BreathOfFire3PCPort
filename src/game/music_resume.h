// DIVERGENCE DIV-0092 (its second part, 2026-10-10 night): after a battle the
// field's song resumes where it was when the battle took it over, as on the
// PlayStation (the owner's copy), instead of starting again from the top.
//
// The position is our DLL's state alone - never game memory, so neither the
// state hash nor a save file sees it. It is taken in GameMode5_Load before the
// battle's music starts (Remember) and used once, by GameMode5_Leave's
// DIV-0092 fallback when that plays the same track (Arm, then Music_Play,
// whose Music_Start fast-forwards the new stream there). Behind DIV-0092's
// switch, mode_flow::g_area_track_fallback: with it off (Capcom's rule, and
// every start-up fuzz, which runs before it is armed) nothing is remembered.
//
// The heard position is the stream's sample under DirectSound's play cursor:
// the ring is two halves of 0x12000 bytes (18,432 samples, 0.418 s), and the
// stream sample each half starts with is kept as Music_CreateBuffer and
// Music_Pump write it, so the sample heard is that half's first plus the
// cursor's offset in it - the 0.42 to 0.84 s the decoder runs ahead of the
// ear left out exactly. docs/music-seq-engine.md section 8 (the sequenced
// path's fast-forward) and docs/mp3-decoder.md section 6 (the MP3's).
#pragma once

namespace music_resume {

// The fade-in of a resumed song, in logic frames (Music_Play's argument).
// 10 at first; the owner heard it on 2026-10-11 ("sounds great") and asked
// for half a second, so the resumed song does not seem to start so close to
// the battle music's end: 30.
constexpr int kResumeFadeFrames = 30;

// GameMode5_Load, before the battle's Music_FadeOutStop / Music_Play: the
// track playing (Music_Track, when it is the BGM file Music_Play started) and
// the sample heard, folded into the song's first pass and loop; nothing (any
// earlier position forgotten) when no track plays or the switch is off.
void Remember();

// GameMode5_Leave's fallback, about to Music_Play `track`: true when the
// remembered position is for `track`; it is then pending for the Music_Start
// that follows. The remembered position is cleared either way.
bool Arm(unsigned track);

// After that Music_Play, and wherever a remembered position is to go unused:
// nothing remembered or pending.
void Forget();

// The self-tests' way in: `sample` of `track` pending for the next
// Music_Start, as Arm leaves it. DIV-0169's resume (sound_in.cpp) uses it too.
void Pend(unsigned track, unsigned sample);

// DIV-0169 (sound_in.h): the song heard now, whatever DIV-0092's switch says -
// Remember's reading without storing it. True, with Music_Track and the
// sample folded into the song's first pass and loop, when the stream is the
// BGM file Music_Play started for that track and its buffer plays (a
// once-only track that has ended does not); false otherwise. Reads only.
bool Heard(unsigned* track, unsigned* sample);

// DIV-0177 (sound_in.h): as Heard, but a halted ring counts too - the stream
// is still the BGM file Music_Play started for Music_Track, stopped
// (Music_Halt) or playing, its position kept; *playing says which. Reads only.
bool Kept(unsigned* track, unsigned* sample, bool* playing);

// BOF3X_SHADOW=sound: the ring's heard position and Remember's gate; the MP3
// path's resume against the stream from the top (a stand-in decoder, a
// looping track's fold, a measured loop's fold, a once-only track past its
// end). The sequenced path's is music_seq::SelfTest's.
void SelfTest();

}  // namespace music_resume
