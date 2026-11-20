# Third-party code carried in this repository

**Status:** CURRENT (2026-11-20)

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
(Since then the build takes more of SDL: its audio for the sound output,
DIV-0201; its video for the game's window, the Linux round; and since
2026-11-20 its 2D renderer and file dialog, for the drawn launcher of
section 6. `CMakeLists.txt` says which parts and why.)
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

## 3. minimp3 (the MP3 decoder, DIV-0091)

`third_party/minimp3/minimp3.h` is lieff's minimp3, verbatim from
[lieff/minimp3](https://github.com/lieff/minimp3) at commit `ea99364f`
(sha256 of the header `29735314aa1814ca4b089a3605202c1f57af67b3413603dfb246402eff413fd7`),
dedicated to the public domain under CC0 1.0 - the choice is in
[`mp3-decoder-choice.md`](mp3-decoder-choice.md), its use in
[`mp3-decoder.md`](mp3-decoder.md). It is included once, from
`src/audio/mp3.cpp` in the DLL, and once more from
`src/launcher/ui/jukebox_play.cpp` in the launcher (the Jukebox,
[`music-player.md`](music-player.md); `src/launcher/music_player.cpp`
until the Win32 dialogs went at stage P6 of
[`launcher-portable.md`](launcher-portable.md), 2026-11-20). CC0 asks for nothing; the dedication is reproduced
here all the same, from the repository's `LICENSE` file as fetched
2026-10-10 (also at `third_party/minimp3/LICENSE`):

```
CC0 1.0 Universal

Statement of Purpose

The laws of most jurisdictions throughout the world automatically confer
exclusive Copyright and Related Rights (defined below) upon the creator and
subsequent owner(s) (each and all, an "owner") of an original work of
authorship and/or a database (each, a "Work").

Certain owners wish to permanently relinquish those rights to a Work for the
purpose of contributing to a commons of creative, cultural and scientific
works ("Commons") that the public can reliably and without fear of later
claims of infringement build upon, modify, incorporate in other works, reuse
and redistribute as freely as possible in any form whatsoever and for any
purposes, including without limitation commercial purposes. These owners may
contribute to the Commons to promote the ideal of a free culture and the
further production of creative, cultural and scientific works, or to gain
reputation or greater distribution for their Work in part through the use and
efforts of others.

For these and/or other purposes and motivations, and without any expectation
of additional consideration or compensation, the person associating CC0 with a
Work (the "Affirmer"), to the extent that he or she is an owner of Copyright
and Related Rights in the Work, voluntarily elects to apply CC0 to the Work
and publicly distribute the Work under its terms, with knowledge of his or her
Copyright and Related Rights in the Work and the meaning and intended legal
effect of CC0 on those rights.

1. Copyright and Related Rights. A Work made available under CC0 may be
protected by copyright and related or neighboring rights ("Copyright and
Related Rights"). Copyright and Related Rights include, but are not limited
to, the following:

  i. the right to reproduce, adapt, distribute, perform, display, communicate,
  and translate a Work;

  ii. moral rights retained by the original author(s) and/or performer(s);

  iii. publicity and privacy rights pertaining to a person's image or likeness
  depicted in a Work;

  iv. rights protecting against unfair competition in regards to a Work,
  subject to the limitations in paragraph 4(a), below;

  v. rights protecting the extraction, dissemination, use and reuse of data in
  a Work;

  vi. database rights (such as those arising under Directive 96/9/EC of the
  European Parliament and of the Council of 11 March 1996 on the legal
  protection of databases, and under any national implementation thereof,
  including any amended or successor version of such directive); and

  vii. other similar, equivalent or corresponding rights throughout the world
  based on applicable law or treaty, and any national implementations thereof.

2. Waiver. To the greatest extent permitted by, but not in contravention of,
applicable law, Affirmer hereby overtly, fully, permanently, irrevocably and
unconditionally waives, abandons, and surrenders all of Affirmer's Copyright
and Related Rights and associated claims and causes of action, whether now
known or unknown (including existing as well as future claims and causes of
action), in the Work (i) in all territories worldwide, (ii) for the maximum
duration provided by applicable law or treaty (including future time
extensions), (iii) in any current or future medium and for any number of
copies, and (iv) for any purpose whatsoever, including without limitation
commercial, advertising or promotional purposes (the "Waiver"). Affirmer makes
the Waiver for the benefit of each member of the public at large and to the
detriment of Affirmer's heirs and successors, fully intending that such Waiver
shall not be subject to revocation, rescission, cancellation, termination, or
any other legal or equitable action to disrupt the quiet enjoyment of the Work
by the public as contemplated by Affirmer's express Statement of Purpose.

3. Public License Fallback. Should any part of the Waiver for any reason be
judged legally invalid or ineffective under applicable law, then the Waiver
shall be preserved to the maximum extent permitted taking into account
Affirmer's express Statement of Purpose. In addition, to the extent the Waiver
is so judged Affirmer hereby grants to each affected person a royalty-free,
non transferable, non sublicensable, non exclusive, irrevocable and
unconditional license to exercise Affirmer's Copyright and Related Rights in
the Work (i) in all territories worldwide, (ii) for the maximum duration
provided by applicable law or treaty (including future time extensions), (iii)
in any current or future medium and for any number of copies, and (iv) for any
purpose whatsoever, including without limitation commercial, advertising or
promotional purposes (the "License"). The License shall be deemed effective as
of the date CC0 was applied by Affirmer to the Work. Should any part of the
License for any reason be judged legally invalid or ineffective under
applicable law, such partial invalidity or ineffectiveness shall not
invalidate the remainder of the License, and in such case Affirmer hereby
affirms that he or she will not (i) exercise any of his or her remaining
Copyright and Related Rights in the Work or (ii) assert any associated claims
and causes of action with respect to the Work, in either case contrary to
Affirmer's express Statement of Purpose.

4. Limitations and Disclaimers.

  a. No trademark or patent rights held by Affirmer are waived, abandoned,
  surrendered, licensed or otherwise affected by this document.

  b. Affirmer offers the Work as-is and makes no representations or warranties
  of any kind concerning the Work, express, implied, statutory or otherwise,
  including without limitation warranties of title, merchantability, fitness
  for a particular purpose, non infringement, or the absence of latent or
  other defects, accuracy, or the present or absence of errors, whether or not
  discoverable, all to the greatest extent permissible under applicable law.

  c. Affirmer disclaims responsibility for clearing rights of other persons
  that may apply to the Work or any use thereof, including without limitation
  any person's Copyright and Related Rights in the Work. Further, Affirmer
  disclaims responsibility for obtaining any necessary consents, permissions
  or other rights required for any use of the Work.

  d. Affirmer understands and acknowledges that Creative Commons is not a
  party to this document and has no duty or obligation with respect to this
  CC0 or use of the Work.

For more information, please see
<http://creativecommons.org/publicdomain/zero/1.0/>
```

## 4. xdelta3 (the DJW stage of `tools/vcdiff.py`; read, not carried)

`tools/vcdiff.py` reads a romhack's `.xdelta` (docs/romhack-mods.md). The
VCDIFF container is RFC 3284. The secondary compressor xdelta3 calls DJW has no
specification but its source, so its decoder here was written from a reading of
`xdelta3/xdelta3-djw.h` in <https://github.com/jmacd/xdelta> (Copyright 2016
Joshua MacDonald, Apache License 2.0), 2026-11-10. No line of that file is in
this repository: the Python is this project's own, and what it shares with the
source is the format - the bit order, the field widths, and the 21-entry
starting order of the code-length list (`DJW_CLEN_MTF`). Listed here so the
origin of that format knowledge is on record; no notice is owed for it.

## 5. bof3-player's psx-formats and psx-machine (the PSX movie and XA decoders, DIV-0214, DIV-0215)

`src/media` (the STR movie, the XA ADPCM decoder, the XA clip reader;
[`psx-media-round.md`](psx-media-round.md)) ports two parts of
[bof3-player](https://github.com/raulcorreia7/bof3-player) at `da74b60`
(2026-11-09), the work of frost (raulcorreia7), who also made the port, from
Rust to C++: the MDEC's dequantisation, inverse DCT and
colour conversion (`psx-machine/src/mdec.rs`) and the XA ADPCM sound-group
decode (`psx-formats/src/xa.rs`, `adpcm.rs`). The STR frame assembly, the BS
v2 bitstream reader and the resampler are this project's own, from psx-spx.
bof3-player is MIT; its notice, from its `LICENSE`:

```
MIT License

Copyright (c) 2026 Raúl Correia and BOF3 contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## 6. Dear ImGui and Droid Sans (the drawn launcher, `bof3x-launcher` since stage P6)

`CMakeLists.txt` fetches [ocornut/imgui](https://github.com/ocornut/imgui) at
the tag `v1.92.9b` (commit `f1cc2ae1`) at configure time, as it fetches SDL3,
and compiles its four core files, its `std::string` helper and its two SDL3
backends into a static library that only the drawn launcher links
([`launcher-portable.md`](launcher-portable.md), stage P1; the owner's call
of 2026-11-19). `src/launcher/ui/ui_imgui.cpp` is the one file that includes
it. Nothing of Dear ImGui is copied into this repository. Since the cutover
(stage P6, 2026-11-20) the drawn launcher is the player's `bof3x-launcher`,
and the release ships it: `tools/make_release.py` copies the fetched tree's
own `LICENSE.txt` into the zip as `licenses\dear-imgui\LICENSE.txt`. Dear
ImGui is MIT; its notice, from the tag's `LICENSE.txt`:

```
The MIT License (MIT)

Copyright (c) 2014-2026 Omar Cornut

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

**Droid Sans.** The drawn launcher's text font is `misc/fonts/DroidSans.ttf`
from the same fetched tree, built into the executable as a byte array at
build time (`tools/embed_file.py`); the file is not copied into this
repository. Dear ImGui's `docs/FONTS.md` gives it as "DroidSans.ttf, by Steve
Matteson, Apache License 2.0". **Paid at stage P6 (2026-11-20):** the
Apache License 2.0's text (its section 4(a) asks for a copy of the licence
with a redistribution) is in
[`third_party/droid-sans/`](../third_party/droid-sans), taken from the
licence's own text as a Debian system holds it
(`/usr/share/common-licenses/Apache-2.0`, sha256 `cfc7749b...bc523d30`, in
full in the folder's `README-bof3x.txt`), and the release carries it with
that README as `licenses\droid-sans\`. The font's own notices, read from
the name table of the file the build embeds (sha256 `4e2371bc...401f581072ecd3`):
copyright "Digitized data copyright (c) 2007, Google Corporation."; trademark
"Droid is a trademark of Google and may be registered in certain
jurisdictions."; maker Ascender Corporation; licence "Licensed under the
Apache License, Version 2.0" with the licence's URL; version "Version 1.00
build 113". The README repeats them. The fetched tree holds no NOTICE file
for the font, so section 4(d) asks nothing more. Which font ships, for the text and for the
banner, was the owner's call ([`launcher-portable.md`](launcher-portable.md)
section 7, call 2): Droid Sans stays the text font (the owner, 2026-11-20),
and the banner's font is section 7's. It holds no Japanese or
Chinese: the pages that show such text (the Jukebox's titles, a save's
names) need a font that does before stage P5.

## 7. TeX Gyre Pagella (the drawn launcher's banner)

[`third_party/tex-gyre-pagella/`](../third_party/tex-gyre-pagella) holds
`texgyrepagella-regular.otf`, version 2.609 (31 March 2026), byte for byte
from the OpenType archive on [GUST's page of the
font](https://www.gust.org.pl/projects/e-foundry/tex-gyre/pagella) (sha256
`5dcecb4f...538e100f`, in full in the folder's `README-bof3x.txt`), with the
family's manifest, the GUST Font License and the LPPL 1.3c text beside it.
The drawn launcher draws its banner with it, in place of the Win32 launcher's
Palatino Linotype, which is Windows' font and cannot ship (the owner's
choice, 2026-11-20; [`launcher-portable.md`](launcher-portable.md) section
9). The build writes the file into `bof3x-launcher` (until stage P6,
`bof3x-launcher-ui`) as a byte array (`tools/embed_file.py`).

The font is offered under the GUST Font License, whose text is:

```
% This is version 1.0, dated 22 June 2009, of the GUST Font License.
% (GUST is the Polish TeX Users Group, https://www.gust.org.pl)
%
% For the most recent version of this license see
% https://www.gust.org.pl/fonts/licenses/GUST-FONT-LICENSE.txt
% or
% https://tug.org/fonts/licenses/GUST-FONT-LICENSE.txt
%
% This work may be distributed and/or modified under the conditions
% of the LaTeX Project Public License, either version 1.3c of this
% license or (at your option) any later version.
% 
% Please also observe the following clause:
% 1) it is requested, but not legally required, that derived works be
%    distributed only after changing the names of the fonts comprising this
%    work and given in an accompanying "manifest", and that the
%    files comprising the Work, as listed in the manifest, also be given
%    new names. Any exceptions to this request are also given in the
%    manifest.
%    
%    We recommend the manifest be given in a separate file named
%    MANIFEST-<fontid>.txt, where <fontid> is some unique identification
%    of the font family. If a separate "readme" file accompanies the Work, 
%    we recommend a name of the form README-<fontid>.txt.
%
% The latest version of the LaTeX Project Public License is in
% https://www.latex-project.org/lppl.txt and version 1.3c or later
% is part of all distributions of LaTeX version 2006/05/20 or later.
```

**What the licence asks, as this project reads it** (a reading, not advice;
the LPPL 1.3c's text is in the folder):

- The LPPL's clause 2 counts the distribution of a part of the Work as a
  modification. The Work is the whole family that the manifest lists, and
  this repository and the launcher carry one file of it. So both carry a
  Derived Work, under clause 6.
- Clause 6 asks, for that: a prominent notice of what was changed (6b), no
  suggestion that the authors support the copy (6c), and the whole
  unchanged Work or where to get it (6d). `README-bof3x.txt` in the folder
  says the three. Clause 6a (a changed component names itself as changed
  when it is used with its Base Interpreter) is about use with TeX; the
  file is unchanged and keeps its name.
- The GUST licence's own clause asks, and does not require, that a derived
  work's fonts take new names. The file is unchanged, so it keeps its name.
- Clause 11: the licence puts no condition on works that are not the Work,
  or on putting them together with it. It asks nothing of the launcher's
  own code. That is why it is here under `CLAUDE.md` rule 5: it is not a
  copyleft over the program, and what it asks travels with the font file
  as a notice does.

**Paid at stage P6 (2026-11-20):** the folder's four text files (the
licence, the LPPL text, the manifest, the README) go into every release zip
as `licenses\tex-gyre-pagella\` (`tools/make_release.py`), since the
release distributes the font inside the program.
