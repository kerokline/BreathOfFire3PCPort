Droid Sans in the bof3x launcher
================================

What this folder holds
----------------------
LICENSE-Apache-2.0.txt   the Apache License, Version 2.0, the licence of the
                         font, as the licence's own text (taken from a Debian
                         system's /usr/share/common-licenses/Apache-2.0;
                         sha256 cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30)
README-bof3x.txt         this file, written by the bof3x project

The font file itself is not in this folder and not in this repository.

Where the font comes from
-------------------------
The launcher draws its text with DroidSans.ttf, by Steve Matteson. The build
takes the file from Dear ImGui's own tree, which it fetches at configure time
(CMakeLists.txt, ocornut/imgui at the tag v1.92.9b, misc/fonts/DroidSans.ttf;
sha256 4e2371bc0e4cf6983342e150412f140da79d674c9be0b56458401f581072ecd3), and
writes it, unchanged, into the launcher's program as bytes (tools/embed_file.py).
Dear ImGui's docs/FONTS.md gives the font as "DroidSans.ttf, by Steve
Matteson, Apache License 2.0".

The font's own notices (its name table, as read from that file)
----------------------------------------------------------------
Copyright:  Digitized data copyright (c) 2007, Google Corporation.
Trademark:  Droid is a trademark of Google and may be registered in certain
            jurisdictions.
Maker:      Ascender Corporation (http://www.ascendercorp.com/)
Licence:    Licensed under the Apache License, Version 2.0
            http://www.apache.org/licenses/LICENSE-2.0
Version:    Version 1.00 build 113

What was changed
----------------
Nothing. The font file is used as its authors released it. The launcher
shows the letters only; it does not give the font to other programs.

A release of bof3x carries this folder's licence text in its licenses\droid-sans\
folder (the Apache License 2.0, section 4(a)). docs/THIRD_PARTY.md section 6
says more.
