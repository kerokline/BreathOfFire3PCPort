// DIVERGENCE DIV-0091 (docs/mp3-decoder.md): the MP3 decoder behind the music
// seam is minimp3 (third_party/minimp3, CC0) instead of the library Capcom
// linked (0x5AB7E0..0x5B8DA0, 200 functions, lineage unknown). The seam is the
// eight decoder entries sound.cpp's Music_OpenDecoder, Music_Decode,
// music_loops' LoopFrame and Music_Release reach through sound::Callees -
// Mp3_Create, Mp3_MemoryIo, Mp3_SetIo, Mp3_Open, Mp3_Start, Mp3_Decode,
// Mp3_Seek, Mp3_Destroy - and ours answer them with the same shape:
//
// - stdcall, as Capcom's; 0 is success from Create, Open, Start, Seek;
// - Open's name is the one Music_OpenDecoder formats, "%X@%X" (size, address
//   of the file in memory), with mode -1; the decoder reads the file there;
// - Decode writes one frame of 44,100 Hz 16-bit stereo PCM at `out` and
//   stores its byte count where every caller reads it, [[decoder + 0x14] + 8];
//   at the end of the stream it returns 0xFFFFFDFE (sound_callees.h's
//   kEndOfStream) and stores what Capcom's stores there, as the self-test
//   measured it;
// - Seek(0) rewinds to the file's first frame with the decoder's state reset,
//   so the samples after it are the first pass's, bit for bit (the rule of
//   docs/mp3-decoder-choice.md section 3: never skip frames with pcm == NULL).
//   The game asks for no other position; any other is fatal.
//
// Memory is the toolchain's heap, not the game's (Crt_malloc / calloc), which
// is what frees Crt_malloc and Crt_free to be rebound (docs/crt-rest.md
// section 2.6).
//
// DIVERGENCE DIV-0215: Open of a file that begins with the XA clip magic
// (media/xa_clip.h: a PlayStation disc's jingle as the importer keeps it)
// plays it through media::XaClip, not minimp3: Decode hands out frames of
// 1,152 stereo samples at 44,100 Hz (every one whole: the last is filled out
// with silence, so a looping stream's rewind meets whole frames as an MP3's
// does), stores their bytes where an MP3 frame's are stored, and returns the
// end of the stream when none is left; Seek(0) rewinds. Capcom's decoder
// cannot read a clip, so under BOF3X_MP3=original a clip is a Fatal
// (RefuseClipUnlessOurs, called by Sound_LoadStream, the one place a kind-0
// file is loaded), never a wrong decode. Music_OpenDecoder has no such guard:
// it must not read its file's bytes, since the sound fuzz hands it pointers
// that are not files.
//
// Armed after every module's self-test (BOF3X_MP3: unset or minimp3 for ours,
// original for Capcom's as the A/B); until then the callee table holds
// Capcom's, which the sound fuzz compares against.
#pragma once

#include <cstdint>

namespace mp3dec {

int __stdcall Create(void** decoder);
int __stdcall MemoryIo(void* io);
int __stdcall SetIo(void* decoder, void* io);
int __stdcall Open(void* decoder, const char* name, int mode);
int __stdcall Start(void* decoder);
int __stdcall Decode(void* decoder, void* out);
int __stdcall Seek(void* decoder, int position);
int __stdcall Destroy(void* decoder);

// The frames of an MP3 file in memory by a walk of its MPEG-1 layer III
// headers, nothing decoded (the self-test's independent count): the music's
// resume folds a looping track's position by it (docs/mp3-decoder.md 6).
// For an XA clip: ceil(its output samples / 1152), the frames Decode hands out.
std::uint32_t CountFrames(const void* file, std::uint32_t size);

// True when the file begins with the XA clip magic.
bool IsClip(const void* file, std::uint32_t size);

// Fatal when the file is an XA clip and Capcom's decoder is the one in use
// (BOF3X_MP3=original, or before Arm): `what` names the file for the message.
// Reads the file's first bytes, so only for a caller that holds a real file.
void RefuseClipUnlessOurs(const void* file, std::uint32_t size, const char* what);

// True once Arm has put ours into the callee table (BOF3X_MP3 not `original`).
bool Active();

// After every module's self-test: BOF3X_MP3 read, the callee table's eight
// decoder entries set to ours unless it says `original`.
void Arm();

// At WinMain's start, with the runtime's heap under it (Capcom's decoder needs
// it, and no start-up self-test has it): with BOF3X_MP3_COMPARE=<path>, every
// MP3 of BGM\ and SND\ decoded through both decoders, the comparison written
// there as a TSV (docs/mp3-decoder.md section 2), and the process ended.
void CompareAtStart();

// BOF3X_SHADOW=sound: BGM\001.DAT decoded whole through ours, driven as
// Music_OpenDecoder and Music_Decode drive the seam: as many frames as an
// independent walk of the file's headers counts, every frame 1,152 stereo
// samples, the store at the end of the stream, the replay after Seek(0)
// identical over the whole file. Capcom's decoder cannot run this early
// (CompareAtStart); nothing is touched.
void SelfTest();

}  // namespace mp3dec
