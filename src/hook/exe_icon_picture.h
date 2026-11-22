// DIV-0095's picture, the part of the window icon that needs no system
// (docs/exe-icon.md; exe_icon.h says when and from what): the record read
// out of the copied .data, the sheet watched in the VRAM shadow, the 40 x 40
// picture rendered, scaled to any size exactly as tools/make_icon.py scales
// it, laid out as .ico images, and two .ico files' pixels compared. Split out
// of exe_icon.cpp in wave B (docs/linux-round.md, "B1 as run") so that the
// Windows icon (exe_icon.cpp: HICON, WIC) and the Linux one (exe_icon_sdl.cpp:
// SDL3) share every pixel; the code is exe_icon.cpp's, moved, not changed.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bof3::exe_icon {

constexpr int kVramW = 1024, kVramH = 512;   // halfwords (docs/asset-loading-path.md)
constexpr unsigned kIcon = 10;               // the whelp (the owner's choice, 2026-10-11)
constexpr int kFileSizes[] = {16, 24, 32, 48, 256};
constexpr int kFileCount = sizeof kFileSizes / sizeof kFileSizes[0];

struct Record {
    int u = 0, v = 0, clut_x = 0, clut_y = 0;
};

using Rgba = std::vector<std::uint8_t>;    // 4 bytes a pixel, rows top-down
using Bytes = std::vector<std::uint8_t>;

// The record of icon kIcon in the table, as the copied .data holds it. False
// when it would leave the page or VRAM (make_icon.py's end-of-table test).
bool ReadRecord(Record& r);

// The sheet and its CLUT are in the VRAM shadow `vram` (kVramW x kVramH).
bool SheetReady(const std::uint16_t* vram, const Record& r);

// The centre 40 x 40 of the 40 x 48 sprite, RGBA.
Rgba Render(const std::uint16_t* vram, const Record& r);

// make_icon.py's cmd_ico for any size s: s x s RGBA.
Rgba Scale(const Rgba& base, int s);

// Once a drawn frame (ExeIcon_Frame's watch): true exactly once, with the
// picture in `base`, when the sheet has read the same on two drawn frames
// running - then logs the line naming the frame. False before, and for ever
// after it was true or found the record unusable (logged once); nothing in
// the hosted build, which keeps Capcom's icon.
bool WatchFrame(Rgba& base);

// An icon image as a DIB, the .ico file's own form below 256 px: the header
// with twice the height, the BGRA rows bottom-up, then the AND mask.
Bytes DibImage(const Rgba& px, int s);

// The five images of kFileSizes, 256 px a PNG (made by the system's encoder,
// the caller's), the others DIBs.
struct IconImages {
    int size[kFileCount];
    Bytes data[kFileCount];
};
Bytes IcoFile(const IconImages& images);

// A decoded image of an .ico file, BGRA rows top-down (the decoders' output:
// WIC's 32bpp BGRA on Windows, SDL3's on Linux).
struct Frame {
    unsigned w = 0, h = 0;
    Bytes bgra;
};

// The self-test's picture: the 1 MB dump of the VRAM shadow (BOF3X_ICON_VRAM)
// rendered, or a Fatal naming `dump_name` when it is not one or does not
// hold the sheet.
Rgba SelfTestPicture(const Bytes& dump, const char* dump_name);

// Every pixel of the five images of `mine` against `ref`: a Fatal naming the
// first difference, else the pixel count compared.
unsigned CompareIco(const std::vector<Frame>& mine, const std::vector<Frame>& ref);

}  // namespace bof3::exe_icon
