// One pad through SDL3, shared by the DLL (src/game/pad_read.cpp, DIV-0050)
// and the launcher (its Controls dialog's capture and its pad navigation):
// the first connected pad, re-opened on hot-plug, read as the positional
// inputs of input/bindings.h with the face-button layout applied, so that
// what the launcher captures is what the game reads.
#pragma once

#include <vector>

#include "input/bindings.h"

namespace bof3x::input {

// Lines for the caller's log; nullptr for none.
using PadLog = void (*)(const char* line);

// SDL_Init(GAMEPAD) with the background-events hint; false if SDL refused.
bool PadSdl_Start(Layout layout, PadLog log);
void PadSdl_Stop();
bool PadSdl_Started();

// Hot-plug and state refresh: call once per read, before the reads below.
void PadSdl_Poll();
bool PadSdl_Open();   // a pad is open

// Whether a map-space input is down: the face buttons are read by position,
// swapped in pairs under the Nintendo layout (or the pad's own labels under
// auto, decided when the pad opens).
bool PadSdl_InputDown(PadInput input);

// The PlayStation word for a map.
unsigned PadSdl_Word(const std::vector<PadBinding>& map);

// The first map-space input that is down, or -1: for a binding capture.
int PadSdl_FirstInputDown();

}  // namespace bof3x::input
