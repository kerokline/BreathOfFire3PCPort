// DIV-0095 off Windows (exe_icon.h; docs/linux-round.md, "B1 as run"): the
// window icon through SDL3, from the portable picture (exe_icon_picture.h).
// What exe_icon.cpp does with HICONs and WIC, this does with SDL surfaces:
//
//   * ExeIcon_Frame: once the sheet is in the VRAM shadow, the picture at the
//     five sizes of the .ico (16, 24, 32, 48, 256) as one surface and its
//     alternates, set on the game's window by SDL_SetWindowIcon - the window
//     system takes the size it shows (X11's _NET_WM_ICON and Wayland's
//     xdg-toplevel-icon take every size). Windows sets two HICONs at the
//     system's two icon sizes; there is no such pair to ask for here.
//   * ExeIcon_SelfTest: the same test as Windows' - the .ico written beside
//     the log, both it and the reference decoded, every pixel of the five
//     images compared - with SDL3's PNG encoder and decoder in WIC's place and
//     the .ico's directory and DIB entries read here, the way Windows' icon
//     decoder reads a 32-bit entry (its alpha when any pixel has one, else
//     the AND mask). Then the window icon's surfaces built, without a window.
//
// The window handle the game hands ExeIcon_Frame is Game_Hwnd's value, which
// on this system is the SDL window's id (game_window_other.cpp AfterCreate).
// Any failure is one log line in the game and a Fatal in the self-test, as
// on Windows. This file compiles to nothing on Windows.
#ifndef _WIN32

#include "hook/exe_icon.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "hook/exe_icon_picture.h"
#include "hook/log.h"
#include "os/os.h"

namespace bof3 {
namespace {

using namespace exe_icon;

// The main image's size and the order of the rest (the alternates).
constexpr int kWindowMain = 32;

// An s x s RGBA picture as a surface of its own (copied: the surface owns its
// pixels). Null with SDL's reason in `why`.
SDL_Surface* SurfaceOf(const Rgba& px, int s, std::string& why) {
    SDL_Surface* surface = SDL_CreateSurface(s, s, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        why = std::string("SDL_CreateSurface failed: ") + SDL_GetError();
        return nullptr;
    }
    auto* row = static_cast<std::uint8_t*>(surface->pixels);
    for (int y = 0; y < s; ++y, row += surface->pitch) std::memcpy(row, &px[static_cast<std::size_t>(y) * s * 4], static_cast<std::size_t>(s) * 4);
    return surface;
}

// The window icon's surface: kWindowMain px, the .ico's other sizes as its
// alternate images. Null with the reason.
SDL_Surface* WindowIcon(const Rgba& base, std::string& why) {
    SDL_Surface* main = SurfaceOf(Scale(base, kWindowMain), kWindowMain, why);
    if (!main) return nullptr;
    for (int s : kFileSizes) {
        if (s == kWindowMain) continue;
        SDL_Surface* alt = SurfaceOf(Scale(base, s), s, why);
        if (!alt) {
            SDL_DestroySurface(main);
            return nullptr;
        }
        const bool added = SDL_AddSurfaceAlternateImage(main, alt);
        SDL_DestroySurface(alt);   // the main surface holds its own reference
        if (!added) {
            why = std::string("SDL_AddSurfaceAlternateImage failed: ") + SDL_GetError();
            SDL_DestroySurface(main);
            return nullptr;
        }
    }
    return main;
}

// --- the self-test's files ---------------------------------------------------------

bool ReadWhole(const std::string& path, Bytes& out, std::string& why) {
    std::ifstream f(os::PathOfUtf8(path.c_str()), std::ios::binary);
    if (!f) {
        why = "cannot open " + path;
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    if (f.bad() || out.size() >= (64u << 20)) {
        why = "cannot read " + path;
        return false;
    }
    return true;
}

bool WriteWhole(const std::string& path, const Bytes& data, std::string& why) {
    std::ofstream f(os::PathOfUtf8(path.c_str()), std::ios::binary | std::ios::trunc);
    if (!f) {
        why = "cannot create " + path;
        return false;
    }
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    f.close();
    if (!f) {
        why = "cannot write " + path;
        return false;
    }
    return true;
}

// Beside the log: the ELF's own folder (log.cpp names the log after the module).
std::string OwnDir() {
    std::string self;
    if (!os::ExecutablePath(&self)) return std::string();
    const std::size_t slash = self.rfind('/');
    return slash == std::string::npos ? std::string(".") : self.substr(0, slash);
}

std::string EnvPath(const char* name) {
    char buf[4096];
    const std::uint32_t n = os::GetEnv(name, buf, sizeof buf);
    return n == 0 || n >= sizeof buf ? std::string() : std::string(buf, n);
}

// The 256 px entry as a PNG, through SDL3's encoder.
bool PngImage(const Rgba& px, int s, Bytes& out, std::string& why) {
    SDL_Surface* surface = SurfaceOf(px, s, why);
    if (!surface) return false;
    SDL_IOStream* io = SDL_IOFromDynamicMem();
    bool ok = io && SDL_SavePNG_IO(surface, io, false);
    if (!ok) why = std::string("the PNG encoder failed: ") + SDL_GetError();
    if (ok) {
        const Sint64 size = SDL_GetIOSize(io);
        const auto* bytes = static_cast<const std::uint8_t*>(
            SDL_GetPointerProperty(SDL_GetIOProperties(io), SDL_PROP_IOSTREAM_DYNAMIC_MEMORY_POINTER, nullptr));
        ok = size > 0 && bytes;
        if (ok) out.assign(bytes, bytes + size);
        else why = "the PNG encoder wrote nothing";
    }
    if (io) SDL_CloseIO(io);
    SDL_DestroySurface(surface);
    return ok;
}

std::uint32_t U32(const Bytes& b, std::size_t at) {
    return static_cast<std::uint32_t>(b[at]) | static_cast<std::uint32_t>(b[at + 1]) << 8 |
           static_cast<std::uint32_t>(b[at + 2]) << 16 | static_cast<std::uint32_t>(b[at + 3]) << 24;
}
unsigned U16(const Bytes& b, std::size_t at) { return b[at] | static_cast<unsigned>(b[at + 1]) << 8; }

// A PNG entry, decoded by SDL3 to BGRA (WIC's 32bpp BGRA: straight alpha).
bool DecodePng(const std::uint8_t* data, std::size_t n, Frame& f, std::string& why) {
    SDL_IOStream* io = SDL_IOFromConstMem(data, n);
    SDL_Surface* png = io ? SDL_LoadPNG_IO(io, true) : nullptr;
    SDL_Surface* bgra = png ? SDL_ConvertSurface(png, SDL_PIXELFORMAT_BGRA32) : nullptr;
    if (png) SDL_DestroySurface(png);
    if (!bgra) {
        why = std::string("the PNG decoder failed: ") + SDL_GetError();
        return false;
    }
    f.w = static_cast<unsigned>(bgra->w);
    f.h = static_cast<unsigned>(bgra->h);
    f.bgra.resize(static_cast<std::size_t>(f.w) * f.h * 4);
    const auto* row = static_cast<const std::uint8_t*>(bgra->pixels);
    for (unsigned y = 0; y < f.h; ++y, row += bgra->pitch) std::memcpy(&f.bgra[static_cast<std::size_t>(y) * f.w * 4], row, f.w * 4);
    SDL_DestroySurface(bgra);
    return true;
}

// A 32-bit DIB entry (BITMAPINFOHEADER, twice the height: the colour rows
// bottom-up, then the AND mask), as Windows' icon decoder gives it: the
// colour's own alpha when any pixel has one; when none has, opaque where the
// mask's bit is clear and transparent where it is set.
bool DecodeDib(const std::uint8_t* data, std::size_t n, unsigned w_entry, Frame& f, std::string& why) {
    const Bytes b(data, data + n);
    if (n < 40 || U32(b, 0) < 40) {
        why = "an .ico entry is neither a PNG nor a DIB";
        return false;
    }
    const std::uint32_t header = U32(b, 0), w = U32(b, 4), h2 = U32(b, 8);
    if (U16(b, 12) != 1 || U16(b, 14) != 32 || U32(b, 16) != 0 || w == 0 || h2 != 2 * w || (w_entry && w != w_entry)) {
        why = "an .ico entry's DIB is not 32 bits a pixel, square and uncompressed";
        return false;
    }
    const std::size_t colour = static_cast<std::size_t>(w) * w * 4, mask_row = ((w + 31) / 32) * 4;
    if (header + colour + mask_row * w > n) {
        why = "an .ico entry's DIB is shorter than its size says";
        return false;
    }
    f.w = f.h = w;
    f.bgra.resize(colour);
    bool any_alpha = false;
    for (std::uint32_t y = 0; y < w; ++y) {
        std::memcpy(&f.bgra[static_cast<std::size_t>(y) * w * 4], &b[header + static_cast<std::size_t>(w - 1 - y) * w * 4], w * 4);
        for (std::uint32_t x = 0; x < w && !any_alpha; ++x) any_alpha = f.bgra[(static_cast<std::size_t>(y) * w + x) * 4 + 3] != 0;
    }
    if (!any_alpha) {
        const std::size_t mask = header + colour;
        for (std::uint32_t y = 0; y < w; ++y)
            for (std::uint32_t x = 0; x < w; ++x) {
                const bool clear = !(b[mask + static_cast<std::size_t>(w - 1 - y) * mask_row + x / 8] & (0x80 >> (x % 8)));
                std::uint8_t* p = &f.bgra[(static_cast<std::size_t>(y) * w + x) * 4];
                if (clear) p[3] = 255;
                else p[0] = p[1] = p[2] = p[3] = 0;
            }
    }
    return true;
}

// Every image of an .ico file, in its directory's order.
bool DecodeIco(const std::string& path, std::vector<Frame>& frames, std::string& why) {
    Bytes b;
    if (!ReadWhole(path, b, why)) return false;
    if (b.size() < 6 || U16(b, 0) != 0 || U16(b, 2) != 1) {
        why = path + " is not an .ico file";
        return false;
    }
    const unsigned count = U16(b, 4);
    if (b.size() < 6 + 16u * count) {
        why = path + "'s directory is cut short";
        return false;
    }
    static const std::uint8_t kPng[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    for (unsigned i = 0; i < count; ++i) {
        const std::size_t e = 6 + 16u * i;
        const unsigned w_entry = b[e] ? b[e] : 256;
        const std::uint32_t size = U32(b, e + 8), at = U32(b, e + 12);
        if (at > b.size() || size > b.size() - at) {
            why = path + ": an image lies past the file's end";
            return false;
        }
        Frame f;
        const bool png = size >= 8 && std::memcmp(&b[at], kPng, 8) == 0;
        if (!(png ? DecodePng(&b[at], size, f, why) : DecodeDib(&b[at], size, w_entry, f, why))) {
            why = path + ": " + why;
            return false;
        }
        frames.push_back(std::move(f));
    }
    return true;
}

}  // namespace

void ExeIcon_Frame(void* hwnd) {
    Rgba base;
    if (!WatchFrame(base)) return;
    const auto id = static_cast<SDL_WindowID>(reinterpret_cast<std::uintptr_t>(hwnd));
    SDL_Window* window = SDL_GetWindowFromID(id);
    if (!window) {
        Log("DIV-0095    icon: the window icon was not set: no SDL window has the id %u (%s)", static_cast<unsigned>(id),
            SDL_GetError());
        return;
    }
    std::string why;
    SDL_Surface* icon = WindowIcon(base, why);
    if (!icon) {
        Log("DIV-0095    icon: the window icon was not set: %s", why.c_str());
        return;
    }
    const bool set = SDL_SetWindowIcon(window, icon);
    if (!set) why = SDL_GetError();
    SDL_DestroySurface(icon);   // the window keeps what it needs
    if (!set) {
        Log("DIV-0095    icon: the window icon was not set: SDL_SetWindowIcon failed: %s", why.c_str());
        return;
    }
    Log("DIV-0095    icon: the window's icon set (%d px, and 16, 24, 48 and 256 px as its alternates)", kWindowMain);
}

void ExeIcon_SelfTest() {
    const std::string vram_path = EnvPath("BOF3X_ICON_VRAM");
    if (vram_path.empty()) return;
    const std::string ref_path = EnvPath("BOF3X_ICON_REF");
    if (ref_path.empty()) Fatal("icon self-test: BOF3X_ICON_VRAM is set without BOF3X_ICON_REF (the expected .ico)");
    std::string why;
    Bytes dump;
    if (!ReadWhole(vram_path, dump, why)) Fatal("icon self-test: %s", why.c_str());
    const Rgba base = SelfTestPicture(dump, vram_path.c_str());

    IconImages images;
    for (int i = 0; i < kFileCount; ++i) {
        const int s = kFileSizes[i];
        images.size[i] = s;
        const Rgba px = Scale(base, s);
        if (s == 256) {
            if (!PngImage(px, s, images.data[i], why)) Fatal("icon self-test: %s", why.c_str());
        } else {
            images.data[i] = DibImage(px, s);
        }
    }
    const std::string ico = OwnDir() + "/bof3x.icon-test.ico";
    if (!WriteWhole(ico, IcoFile(images), why)) Fatal("icon self-test: %s", why.c_str());

    // Every pixel of the five images against the reference, both files
    // decoded here (the reference's entries are PNGs, ours DIBs and a PNG).
    std::vector<Frame> mine, ref;
    if (!DecodeIco(ico, mine, why) || !DecodeIco(ref_path, ref, why)) Fatal("icon self-test: %s", why.c_str());
    const unsigned total = CompareIco(mine, ref);
    Log("DIV-0095    icon self-test: %s rendered from %s - 16, 24, 32, 48 and 256 px, all %u pixels identical to %s",
        ico.c_str(), vram_path.c_str(), total, ref_path.c_str());

    // The window icon's path, without a window.
    SDL_Surface* icon = WindowIcon(base, why);
    if (!icon) Fatal("icon self-test: the window icon: %s", why.c_str());
    int images_in = 0;
    SDL_Surface** all = SDL_GetSurfaceImages(icon, &images_in);
    SDL_free(all);
    SDL_DestroySurface(icon);
    if (images_in != kFileCount)
        Fatal("icon self-test: the window icon holds %d images, not the %d sizes of the .ico", images_in, kFileCount);
    Log("DIV-0095    icon self-test: the window icon built at %d px with %d alternates (SDL surfaces)", kWindowMain,
        kFileCount - 1);
}

}  // namespace bof3

#endif  // !_WIN32
