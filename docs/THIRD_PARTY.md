# Third-party code carried in this repository

**Status:** CURRENT (2026-09-24)

`CLAUDE.md` rule 5 keeps this list short: nothing copyleft, nothing whose
terms we could not grant on at a handoff ([`LICENSING.md`](LICENSING.md)
§4). Everything below is permissive and its notice is reproduced in full,
which is all its terms ask.

## 1. CRT-SatPixie (the `satpixie` look, DIV-0043)

`src/render/satpixie.cpp` is a port to HLSL of the RetroArch preset in
[Conkwer/satpixie-crt-shader](https://github.com/Conkwer/satpixie-crt-shader)
(`satpixie-crt.slangp` and its four `.slang` passes), a fork of Mattias
Gustavsson's "newpixie" CRT shader as adapted for slang by hunterk. The
repository offers it under the MIT licence or, alternatively, as public
domain (the Unlicense); we take it under MIT and keep this notice, taken
from the repository's `LICENSE` file on 2026-09-23:

```
------------------------------------------------------------------------------
This software is available under 2 licenses - you may choose the one you like.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2016 Mattias Gustavsson
Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this 
software, either in source code form or as a compiled binary, for any purpose, 
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this 
software dedicate any and all copyright interest in the software to the public 
domain. We make this dedication for the benefit of the public at large and to 
the detriment of our heirs and successors. We intend this dedication to be an 
overt act of relinquishment in perpetuity of all present and future rights to 
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
```

## 2. SDL3 (the pad, DIV-0050)

`CMakeLists.txt` fetches [libsdl-org/SDL](https://github.com/libsdl-org/SDL)
at the tag `release-3.4.16` at configure time and builds its joystick and
gamepad layers (no video, audio or render) as a static library into
`bof3x.dll` and, since the same day, into `bof3x-launcher.exe`;
`src/input/pad_sdl.cpp` is the one file that includes it.
Nothing of SDL is copied into this repository. SDL is offered under the zlib
licence, whose notice, taken from the repository's `LICENSE.txt` on
2026-09-24, is:

```
Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
```
