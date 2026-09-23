// The PSX library's display calls as the port implements them over DirectDraw
// and Direct3D - PutDispEnv, PutDrawEnv, SetDefDispEnv, SetDefDrawEnv, the
// present under them, the back-buffer clear and the background colour - and
// the sound bank load Snd_LoadBank. docs/display-env.md.
#pragma once

void DisplayEnv_Inject();
