// The sound output through SDL3 (DIV-0197; docs/x64-build-design.md 6.3):
// objects of ours with DirectSound-shaped vtables - a device, its buffers,
// each buffer's notify facet - in the slots the game keeps DirectSound's
// pointers in, and one SDL3 device stream fed by a mixer of ours that sums
// every playing buffer at its frequency, volume and pan. The game's 54
// vtable calls (sound_callees.h Method<>) reach them unchanged, as the
// render shim's objects stand in front of Direct3D 11 (render_shim.cpp).
//
// Bound by one PatchBytes on the DirectSoundCreate import slot 0x5C4014
// (Imp_DirectSoundCreate): the code name `SoundSdl`, so BOF3X_OFF=SoundSdl
// (the launcher's off=) and, hosted, BOF3X_ORIGINAL=SoundSdl leave the slot
// on Windows' DirectSound for the owner's A/B listen. On the 64-bit build
// there is no DirectSound path (its objects take x64 descriptors and 64-bit
// out-pointers the game does not pass), so the switch is ignored there with
// a log line and the slot is bound to ours regardless.
#pragma once

namespace bof3 {

// Both hosts, once the import slot holds a function (hosted: the loader
// bound it before DllMain; bof3x.exe: after BindSlots): the switch and the
// slot. Runs the layer's self-test where the other hosted modules run theirs
// (BOF3X_SHADOW=sound_sdl, or the exe host's BOF3X_SELFTEST_ONLY).
void SoundSdl_Start();

// True when the slot was bound to our Create.
bool SoundSdl_On();

// True when `fn` is our Create: the WAV dump asks before it wraps the slot's
// function (wav_dump.cpp), as it checks for DSOUND.dll's own export.
bool SoundSdl_IsCreate(const void* fn);

}  // namespace bof3
