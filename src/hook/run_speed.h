// BOF3X_SPEED's one exception (win_main.cpp, DIV-0048's tooling). Audio plays
// in wall time whatever the frame rate, and some scenes wait on it: the inn
// counts 150 frames and then waits for its jingle to end (InnNight_Wait),
// the scripts' op 8B waits on a voice. So from the frame a stream is started
// (Sound_LoadStream) until it has stopped, WinMain's loop runs at the
// ordinary period: the stream then lasts the frames it lasts at speed 1 and
// a recipe recorded at speed 1 stays in step. Nothing here touches game
// state, and at speed 1 the loop does not ask.
#pragma once

namespace bof3 {

void RunSpeed_StreamStarted();   // Sound_LoadStream, as it starts one
bool RunSpeed_StreamPlaying();   // the loop, once a frame while BOF3X_SPEED is above 1
void RunSpeed_StreamForget();    // the loop, when a hold outlasts any jingle (music took the buffer over)

}  // namespace bof3
