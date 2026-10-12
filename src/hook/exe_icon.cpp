// DIV-0095: bof3x.exe's icon, generated from the game's own data on the
// player's machine (docs/exe-icon.md; exe_icon.h says when and from what).
//
// The reads are tools/make_icon.py's, which is the specification of the
// picture: the record at Menu_DrawBigIcon's table + icon * 4 is (u, v,
// clut-x byte, clut-y byte); the sprite is 0x28 x 0x30 at 4 bits a pixel from
// texture page 0x1E (VRAM x 896, y 256), the CLUT sixteen halfwords at VRAM
// (clut-x & 0xF0, 0x1E0 + clut-y); a halfword 0x0000 is transparent, any
// other is 15-bit colour, its bit 15 (semi-transparency) ignored. The 48 rows
// are cropped to the centre 40.
//
// The sizes, as make_icon.py makes them: a whole multiple of 40 by nearest
// neighbour; above 40, the largest whole multiple letterboxed into the square;
// below 40, Pillow's Lanczos resampling, reproduced exactly (premultiplied
// alpha, the separable two-pass filter with its 22-bit integer coefficients,
// the floor division back to straight alpha) so the self-test can compare
// every pixel with the .ico make_icon.py wrote.
#include "hook/exe_icon.h"

#include <windows.h>

#include <objidl.h>
#include <wincodec.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "common/icon_stamp.h"
#include "common/sha256.h"
#include "game/menu_windows_callees.h"
#include "hook/host.h"
#include "hook/log.h"

namespace bof3 {
namespace {

// --- the picture -------------------------------------------------------------

constexpr int kVramW = 1024, kVramH = 512;   // halfwords (docs/asset-loading-path.md)
constexpr int kPageX = 896, kPageY = 256;    // texture page 0x1E: (0x1E & 0xF) * 64, (0x1E >> 4) * 256
constexpr int kIconW = 0x28, kIconH = 0x30;  // Menu_DrawBigIcon's SPRT
constexpr int kClutBaseY = 0x1E0;
constexpr int kSide = kIconW;                // the square: 40 x 40
constexpr int kCropTop = (kIconH - kIconW) / 2;   // the centre crop: rows 4..43 (the owner's choice, 2026-10-11)
constexpr unsigned kIcon = 10;               // the whelp (the owner's choice, 2026-10-11)
constexpr int kFileSizes[] = {16, 24, 32, 48, 256};
constexpr int kFileCount = sizeof kFileSizes / sizeof kFileSizes[0];

struct Record {
    int u = 0, v = 0, clut_x = 0, clut_y = 0;
};

// The record of icon kIcon in the table, as the copied .data holds it. False
// when it would leave the page or VRAM (make_icon.py's end-of-table test).
bool ReadRecord(Record& r) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(menu_windows::at::kIconRects + kIcon * 4));
    r = {p[0], p[1], p[2] & 0xF0, kClutBaseY + p[3]};
    return r.u + kIconW <= 256 && r.v + kIconH <= 256 && r.clut_y < kVramH;
}

std::uint16_t Cell(const std::uint16_t* vram, int x, int y) { return vram[y * kVramW + x]; }

// The texture region's halfwords: row y of the sprite covers ten halfwords
// (40 pixels at 4 a halfword) from x = 896 + u / 4. u is a multiple of 4 for
// every record in the table, but the read follows make_icon.py's per pixel.
std::uint16_t Texel(const std::uint16_t* vram, const Record& r, int x, int y) {
    const int tx = r.u + x;
    const std::uint16_t hw = Cell(vram, kPageX + tx / 4, kPageY + r.v + y);
    return static_cast<std::uint16_t>((hw >> ((tx & 3) * 4)) & 0xF);
}

// The bytes the picture is cut from, in a fixed order: the record's four
// values, the sixteen CLUT halfwords, the region's halfwords row by row.
std::vector<std::uint8_t> SheetBytes(const std::uint16_t* vram, const Record& r) {
    std::vector<std::uint8_t> out;
    const auto put16 = [&](std::uint16_t v) {
        out.push_back(static_cast<std::uint8_t>(v));
        out.push_back(static_cast<std::uint8_t>(v >> 8));
    };
    put16(static_cast<std::uint16_t>(r.u));
    put16(static_cast<std::uint16_t>(r.v));
    put16(static_cast<std::uint16_t>(r.clut_x));
    put16(static_cast<std::uint16_t>(r.clut_y));
    for (int i = 0; i < 16; ++i) put16(Cell(vram, r.clut_x + i, r.clut_y));
    const int x0 = kPageX + r.u / 4, x1 = kPageX + (r.u + kIconW + 3) / 4;
    for (int y = 0; y < kIconH; ++y)
        for (int x = x0; x < x1; ++x) put16(Cell(vram, x, kPageY + r.v + y));
    return out;
}

// The sheet and its CLUT are there: some CLUT entry is not zero (entry 0 is
// the transparent 0x0000 in the whelp's), and some halfword of the region is
// not zero.
bool SheetReady(const std::uint16_t* vram, const Record& r) {
    bool clut = false, region = false;
    for (int i = 0; i < 16; ++i) clut |= Cell(vram, r.clut_x + i, r.clut_y) != 0;
    if (!clut) return false;
    const int x0 = kPageX + r.u / 4, x1 = kPageX + (r.u + kIconW + 3) / 4;
    for (int y = 0; y < kIconH && !region; ++y)
        for (int x = x0; x < x1; ++x)
            if (Cell(vram, x, kPageY + r.v + y)) {
                region = true;
                break;
            }
    return region;
}

std::uint32_t Fnv1a(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t h = 2166136261u;
    for (std::uint8_t b : bytes) h = (h ^ b) * 16777619u;
    return h;
}

using Rgba = std::vector<std::uint8_t>;   // 4 bytes a pixel, rows top-down

// PSX 15-bit colour to RGBA, as make_icon.py's rgba().
void PutColour(std::uint8_t* px, std::uint16_t hw) {
    if (hw == 0) {
        px[0] = px[1] = px[2] = px[3] = 0;
        return;
    }
    const int r = hw & 31, g = (hw >> 5) & 31, b = (hw >> 10) & 31;
    px[0] = static_cast<std::uint8_t>((r << 3) | (r >> 2));
    px[1] = static_cast<std::uint8_t>((g << 3) | (g >> 2));
    px[2] = static_cast<std::uint8_t>((b << 3) | (b >> 2));
    px[3] = 255;
}

// The centre 40 x 40 of the 40 x 48 sprite.
Rgba Render(const std::uint16_t* vram, const Record& r) {
    std::uint16_t clut[16];
    for (int i = 0; i < 16; ++i) clut[i] = Cell(vram, r.clut_x + i, r.clut_y);
    Rgba out(kSide * kSide * 4);
    for (int y = 0; y < kSide; ++y)
        for (int x = 0; x < kSide; ++x) PutColour(&out[(y * kSide + x) * 4], clut[Texel(vram, r, x, kCropTop + y)]);
    return out;
}

// --- the sizes -----------------------------------------------------------------

Rgba Nearest(const Rgba& base, int k) {
    const int s = kSide * k;
    Rgba out(s * s * 4);
    for (int y = 0; y < s; ++y)
        for (int x = 0; x < s; ++x) std::memcpy(&out[(y * s + x) * 4], &base[((y / k) * kSide + x / k) * 4], 4);
    return out;
}

// Pillow's Lanczos (Resample.c): a = 3, the support widened by the scale.
double Sinc(double x) {
    if (x == 0.0) return 1.0;
    x = x * 3.14159265358979323846;
    return std::sin(x) / x;
}
double Lanczos(double x) { return (-3.0 <= x && x < 3.0) ? Sinc(x) * Sinc(x / 3) : 0.0; }

constexpr int kPrecisionBits = 32 - 8 - 2;

struct Taps {
    int first = 0;
    std::vector<int> k;
};

// precompute_coeffs + normalize_coeffs_8bpc, for in -> out with in > out.
std::vector<Taps> Coefficients(int in, int out) {
    const double scale = static_cast<double>(in) / out;
    const double filterscale = scale < 1.0 ? 1.0 : scale;
    const double support = 3.0 * filterscale;
    std::vector<Taps> taps(out);
    for (int xx = 0; xx < out; ++xx) {
        const double center = (xx + 0.5) * scale;
        const double ss = 1.0 / filterscale;
        int xmin = static_cast<int>(center - support + 0.5);
        if (xmin < 0) xmin = 0;
        int xmax = static_cast<int>(center + support + 0.5);
        if (xmax > in) xmax = in;
        xmax -= xmin;
        std::vector<double> w(xmax);
        double ww = 0.0;
        for (int x = 0; x < xmax; ++x) {
            w[x] = Lanczos((x + xmin - center + 0.5) * ss);
            ww += w[x];
        }
        taps[xx].first = xmin;
        for (int x = 0; x < xmax; ++x) {
            const double k = ww != 0.0 ? w[x] / ww : w[x];
            taps[xx].k.push_back(k < 0 ? static_cast<int>(-0.5 + k * (1 << kPrecisionBits))
                                       : static_cast<int>(0.5 + k * (1 << kPrecisionBits)));
        }
    }
    return taps;
}

std::uint8_t Clip8(int v) {
    if (v >= (1 << kPrecisionBits << 8)) return 255;
    if (v <= 0) return 0;
    return static_cast<std::uint8_t>(v >> kPrecisionBits);
}

// Pillow's RGBA -> RGBa: MULDIV255.
std::uint8_t Premultiply(unsigned c, unsigned a) {
    const unsigned t = c * a + 128;
    return static_cast<std::uint8_t>(((t >> 8) + t) >> 8);
}

// Image.resize(LANCZOS) of an RGBA image: to RGBa, horizontal then vertical,
// back to RGBA (alpha 0 and 255 kept as they are, else 255 * c / a floored
// and clipped).
Rgba Lanczos(const Rgba& base, int s) {
    const int n = kSide;
    Rgba pre(base.size());
    for (int i = 0; i < n * n; ++i) {
        const unsigned a = base[i * 4 + 3];
        for (int c = 0; c < 3; ++c) pre[i * 4 + c] = Premultiply(base[i * 4 + c], a);
        pre[i * 4 + 3] = static_cast<std::uint8_t>(a);
    }
    const std::vector<Taps> taps = Coefficients(n, s);
    Rgba mid(n * s * 4);
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < s; ++x)
            for (int c = 0; c < 4; ++c) {
                int acc = 1 << (kPrecisionBits - 1);
                for (std::size_t i = 0; i < taps[x].k.size(); ++i)
                    acc += pre[(y * n + taps[x].first + static_cast<int>(i)) * 4 + c] * taps[x].k[i];
                mid[(y * s + x) * 4 + c] = Clip8(acc);
            }
    Rgba out(s * s * 4);
    for (int y = 0; y < s; ++y)
        for (int x = 0; x < s; ++x)
            for (int c = 0; c < 4; ++c) {
                int acc = 1 << (kPrecisionBits - 1);
                for (std::size_t i = 0; i < taps[y].k.size(); ++i)
                    acc += mid[((taps[y].first + static_cast<int>(i)) * s + x) * 4 + c] * taps[y].k[i];
                out[(y * s + x) * 4 + c] = Clip8(acc);
            }
    for (int i = 0; i < s * s; ++i) {
        const unsigned a = out[i * 4 + 3];
        if (a == 0 || a == 255) continue;
        for (int c = 0; c < 3; ++c) {
            const unsigned v = 255 * out[i * 4 + c] / a;
            out[i * 4 + c] = static_cast<std::uint8_t>(v > 255 ? 255 : v);
        }
    }
    return out;
}

// make_icon.py's cmd_ico, for any size.
Rgba Scale(const Rgba& base, int s) {
    if (s % kSide == 0) return Nearest(base, s / kSide);
    if (s > kSide) {
        const int k = s / kSide, inner = kSide * k, off = (s - inner) / 2;
        const Rgba big = Nearest(base, k);
        Rgba out(s * s * 4, 0);
        for (int y = 0; y < inner; ++y) std::memcpy(&out[((off + y) * s + off) * 4], &big[y * inner * 4], inner * 4);
        return out;
    }
    return Lanczos(base, s);
}

// --- HICONs --------------------------------------------------------------------

HICON MakeIcon(const Rgba& px, int s, std::string& why) {
    BITMAPV5HEADER bi = {};
    bi.bV5Size = sizeof bi;
    bi.bV5Width = s;
    bi.bV5Height = -s;   // top-down
    bi.bV5Planes = 1;
    bi.bV5BitCount = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask = 0x00FF0000;
    bi.bV5GreenMask = 0x0000FF00;
    bi.bV5BlueMask = 0x000000FF;
    bi.bV5AlphaMask = 0xFF000000;
    void* bits = nullptr;
    HDC dc = GetDC(nullptr);
    HBITMAP colour = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bi), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!colour || !bits) {
        why = "CreateDIBSection failed (error " + std::to_string(GetLastError()) + ")";
        if (colour) DeleteObject(colour);
        return nullptr;
    }
    auto* dst = static_cast<std::uint8_t*>(bits);
    for (int i = 0; i < s * s; ++i) {
        dst[i * 4 + 0] = px[i * 4 + 2];
        dst[i * 4 + 1] = px[i * 4 + 1];
        dst[i * 4 + 2] = px[i * 4 + 0];
        dst[i * 4 + 3] = px[i * 4 + 3];
    }
    // The AND mask all zero: the colour bitmap's alpha decides.
    std::vector<std::uint8_t> zeros(((s + 15) / 16) * 2 * s, 0);
    HBITMAP mask = CreateBitmap(s, s, 1, 1, zeros.data());
    ICONINFO ii = {};
    ii.fIcon = TRUE;
    ii.hbmMask = mask;
    ii.hbmColor = colour;
    HICON icon = mask ? CreateIconIndirect(&ii) : nullptr;
    if (!icon) why = "CreateIconIndirect failed (error " + std::to_string(GetLastError()) + ")";
    if (mask) DeleteObject(mask);
    DeleteObject(colour);
    return icon;
}

// --- the .ico images -------------------------------------------------------------

using Bytes = std::vector<std::uint8_t>;

void Put16(Bytes& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void Put32(Bytes& b, std::uint32_t v) {
    Put16(b, v & 0xFFFF);
    Put16(b, v >> 16);
}

// An icon image as a DIB: the header with twice the height, the BGRA rows
// bottom-up, then the AND mask (set where alpha is 0), rows padded to 32 bits.
Bytes DibImage(const Rgba& px, int s) {
    const int mask_row = ((s + 31) / 32) * 4;
    Bytes b;
    Put32(b, 40);
    Put32(b, static_cast<std::uint32_t>(s));
    Put32(b, static_cast<std::uint32_t>(2 * s));
    Put16(b, 1);
    Put16(b, 32);
    Put32(b, BI_RGB);
    Put32(b, static_cast<std::uint32_t>(s * s * 4 + mask_row * s));
    for (int i = 0; i < 4; ++i) Put32(b, 0);
    for (int y = s - 1; y >= 0; --y)
        for (int x = 0; x < s; ++x) {
            const std::uint8_t* p = &px[(y * s + x) * 4];
            b.push_back(p[2]);
            b.push_back(p[1]);
            b.push_back(p[0]);
            b.push_back(p[3]);
        }
    for (int y = s - 1; y >= 0; --y) {
        Bytes row(mask_row, 0);
        for (int x = 0; x < s; ++x)
            if (px[(y * s + x) * 4 + 3] == 0) row[x / 8] |= static_cast<std::uint8_t>(0x80 >> (x % 8));
        b.insert(b.end(), row.begin(), row.end());
    }
    return b;
}

// COM on the calling thread, for WIC; released with the object.
struct Com {
    bool ok = false;
    bool uninit = false;
    IWICImagingFactory* wic = nullptr;
    std::string why;
    Com() {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        uninit = SUCCEEDED(hr);
        if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
            why = "CoInitializeEx failed (0x" + Hex(hr) + ")";
            return;
        }
        const HRESULT f = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, __uuidof(IWICImagingFactory),
                                           reinterpret_cast<void**>(&wic));
        if (FAILED(f)) {
            why = "no WIC imaging factory (0x" + Hex(f) + ")";
            return;
        }
        ok = true;
    }
    ~Com() {
        if (wic) wic->Release();
        if (uninit) CoUninitialize();
    }
    static std::string Hex(HRESULT hr) {
        char t[16];
        wsprintfA(t, "%08lX", static_cast<unsigned long>(hr));
        return t;
    }
};

// An icon image as a PNG, through WIC's encoder (the 256 px entry).
bool PngImage(Com& com, const Rgba& px, int s, Bytes& out, std::string& why) {
    std::vector<std::uint8_t> bgra(px.size());
    for (int i = 0; i < s * s; ++i) {
        bgra[i * 4 + 0] = px[i * 4 + 2];
        bgra[i * 4 + 1] = px[i * 4 + 1];
        bgra[i * 4 + 2] = px[i * 4 + 0];
        bgra[i * 4 + 3] = px[i * 4 + 3];
    }
    IStream* stream = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* props = nullptr;
    HRESULT hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
    const char* step = "CreateStreamOnHGlobal";
    if (SUCCEEDED(hr)) step = "CreateEncoder", hr = com.wic->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc);
    if (SUCCEEDED(hr)) step = "Initialize", hr = enc->Initialize(stream, WICBitmapEncoderNoCache);
    if (SUCCEEDED(hr)) step = "CreateNewFrame", hr = enc->CreateNewFrame(&frame, &props);
    if (SUCCEEDED(hr)) step = "frame Initialize", hr = frame->Initialize(props);
    if (SUCCEEDED(hr)) step = "SetSize", hr = frame->SetSize(s, s);
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    if (SUCCEEDED(hr)) step = "SetPixelFormat", hr = frame->SetPixelFormat(&format);
    if (SUCCEEDED(hr) && !IsEqualGUID(format, GUID_WICPixelFormat32bppBGRA)) step = "SetPixelFormat (not 32bpp BGRA)", hr = E_FAIL;
    if (SUCCEEDED(hr))
        step = "WritePixels", hr = frame->WritePixels(s, s * 4, static_cast<UINT>(bgra.size()), bgra.data());
    if (SUCCEEDED(hr)) step = "frame Commit", hr = frame->Commit();
    if (SUCCEEDED(hr)) step = "Commit", hr = enc->Commit();
    STATSTG st = {};
    if (SUCCEEDED(hr)) step = "Stat", hr = stream->Stat(&st, STATFLAG_NONAME);
    HGLOBAL mem = nullptr;
    if (SUCCEEDED(hr)) step = "GetHGlobalFromStream", hr = GetHGlobalFromStream(stream, &mem);
    if (SUCCEEDED(hr)) {
        const auto* p = static_cast<const std::uint8_t*>(GlobalLock(mem));
        if (p) {
            out.assign(p, p + static_cast<std::size_t>(st.cbSize.QuadPart));
            GlobalUnlock(mem);
        } else {
            step = "GlobalLock", hr = E_FAIL;
        }
    }
    if (props) props->Release();
    if (frame) frame->Release();
    if (enc) enc->Release();
    if (stream) stream->Release();
    if (FAILED(hr)) why = std::string("the PNG encoder's ") + step + " failed (0x" + Com::Hex(hr) + ")";
    return SUCCEEDED(hr);
}

struct IconImages {
    int size[kFileCount];
    Bytes data[kFileCount];
};

bool MakeImages(Com& com, const Rgba& base, IconImages& images, std::string& why) {
    for (int i = 0; i < kFileCount; ++i) {
        const int s = kFileSizes[i];
        images.size[i] = s;
        const Rgba px = Scale(base, s);
        if (s == 256) {
            if (!PngImage(com, px, s, images.data[i], why)) return false;
        } else {
            images.data[i] = DibImage(px, s);
        }
    }
    return true;
}

// --- files -----------------------------------------------------------------------

std::wstring ExePath() {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return n == 0 || n >= MAX_PATH ? std::wstring() : std::wstring(buf, n);
}

std::wstring DirOf(const std::wstring& path) { return path.substr(0, path.find_last_of(L"\\/")); }

// Beside the log: bof3x.dll's directory.
std::wstring DllDir() {
    HMODULE self = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&ExeIcon_Frame), &self);
    wchar_t buf[MAX_PATH];
    const DWORD n = GetModuleFileNameW(self, buf, MAX_PATH);
    return n == 0 || n >= MAX_PATH ? std::wstring() : DirOf(std::wstring(buf, n));
}

bool ReadWhole(const std::wstring& path, Bytes& out, std::string& why) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        why = "cannot open " + std::string(Utf8Path(path.c_str()).text) + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    LARGE_INTEGER size = {};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (64 << 20);
    if (ok) {
        out.resize(static_cast<std::size_t>(size.QuadPart));
        DWORD got = 0;
        ok = out.empty() || (ReadFile(f, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) && got == out.size());
    }
    CloseHandle(f);
    if (!ok) why = "cannot read " + std::string(Utf8Path(path.c_str()).text);
    return ok;
}

bool WriteWhole(const std::wstring& path, const Bytes& data, std::string& why) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) {
        why = "cannot create " + std::string(Utf8Path(path.c_str()).text) + " (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    DWORD wrote = 0;
    const bool ok = WriteFile(f, data.data(), static_cast<DWORD>(data.size()), &wrote, nullptr) && wrote == data.size();
    CloseHandle(f);
    if (!ok) why = "cannot write " + std::string(Utf8Path(path.c_str()).text);
    return ok;
}

bool FileExists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

Bytes IcoFile(const IconImages& images) {
    Bytes b;
    Put16(b, 0);
    Put16(b, 1);
    Put16(b, kFileCount);
    std::uint32_t at = 6 + 16 * kFileCount;
    for (int i = 0; i < kFileCount; ++i) {
        const int s = images.size[i];
        b.push_back(static_cast<std::uint8_t>(s >= 256 ? 0 : s));
        b.push_back(static_cast<std::uint8_t>(s >= 256 ? 0 : s));
        b.push_back(0);
        b.push_back(0);
        Put16(b, 1);
        Put16(b, 32);
        Put32(b, static_cast<std::uint32_t>(images.data[i].size()));
        Put32(b, at);
        at += static_cast<std::uint32_t>(images.data[i].size());
    }
    for (int i = 0; i < kFileCount; ++i) b.insert(b.end(), images.data[i].begin(), images.data[i].end());
    return b;
}

// The kRtGroupIcon resource: the same directory with a resource id in place
// of each offset.
Bytes GroupResource(const IconImages& images) {
    Bytes b;
    Put16(b, 0);
    Put16(b, 1);
    Put16(b, kFileCount);
    for (int i = 0; i < kFileCount; ++i) {
        const int s = images.size[i];
        b.push_back(static_cast<std::uint8_t>(s >= 256 ? 0 : s));
        b.push_back(static_cast<std::uint8_t>(s >= 256 ? 0 : s));
        b.push_back(0);
        b.push_back(0);
        Put16(b, 1);
        Put16(b, 32);
        Put32(b, static_cast<std::uint32_t>(images.data[i].size()));
        Put16(b, static_cast<unsigned>(i + 1));
    }
    return b;
}

// --- the executable's resources ------------------------------------------------------

constexpr wchar_t kGroupName[] = L"BOF3_ICON";
// The resource types, wide (windows.h's RT_* follow the build's ANSI default).
const LPCWSTR kRtIcon = MAKEINTRESOURCEW(3), kRtGroupIcon = MAKEINTRESOURCEW(14), kRtManifest = MAKEINTRESOURCEW(24);   // Game_WinMain's LoadIconA (tools/make_exe.py)

BOOL CALLBACK CollectLanguage(HMODULE, LPCWSTR, LPCWSTR, WORD lang, LONG_PTR to) {
    reinterpret_cast<std::vector<WORD>*>(to)->push_back(lang);
    return TRUE;
}

std::vector<WORD> Languages(HMODULE m, LPCWSTR type, LPCWSTR name) {
    std::vector<WORD> langs;
    EnumResourceLanguagesW(m, type, name, &CollectLanguage, reinterpret_cast<LONG_PTR>(&langs));
    return langs;
}

bool ResourceBytes(HMODULE m, LPCWSTR type, LPCWSTR name, Bytes& out) {
    HRSRC r = FindResourceW(m, name, type);
    if (!r) return false;
    HGLOBAL g = LoadResource(m, r);
    const DWORD n = SizeofResource(m, r);
    const auto* p = static_cast<const std::uint8_t*>(g ? LockResource(g) : nullptr);
    if (!p) return false;
    out.assign(p, p + n);
    return true;
}

struct DataFile {
    HMODULE m = nullptr;
    explicit DataFile(const std::wstring& path)
        : m(LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE)) {}
    ~DataFile() {
        if (m) FreeLibrary(m);
    }
};

// The headers and sections of a PE file in memory.
struct Pe {
    const IMAGE_NT_HEADERS32* nt = nullptr;
    const IMAGE_SECTION_HEADER* sec = nullptr;
    unsigned n = 0;
};

bool ParsePe(const Bytes& f, Pe& pe) {
    if (f.size() < sizeof(IMAGE_DOS_HEADER)) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(f.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
        static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > f.size())
        return false;
    pe.nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(f.data() + dos->e_lfanew);
    if (pe.nt->Signature != IMAGE_NT_SIGNATURE || pe.nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) return false;
    const std::size_t first = static_cast<std::size_t>(dos->e_lfanew) + offsetof(IMAGE_NT_HEADERS32, OptionalHeader) +
                              pe.nt->FileHeader.SizeOfOptionalHeader;
    pe.n = pe.nt->FileHeader.NumberOfSections;
    if (first + pe.n * sizeof(IMAGE_SECTION_HEADER) > f.size()) return false;
    pe.sec = reinterpret_cast<const IMAGE_SECTION_HEADER*>(f.data() + first);
    for (unsigned i = 0; i < pe.n; ++i)
        if (static_cast<std::size_t>(pe.sec[i].PointerToRawData) + pe.sec[i].SizeOfRawData > f.size()) return false;
    return true;
}

std::string SectionName(const IMAGE_SECTION_HEADER& s) { return std::string(reinterpret_cast<const char*>(s.Name), strnlen(reinterpret_cast<const char*>(s.Name), 8)); }

// What UpdateResource may change is the last section, .rsrc, and the header
// words that describe it; everything else - the identity DetectHost reads
// (the timestamp, .stub at 0x401000), the layout exe_layout.h checks, the
// entry, the imports - must be byte for byte the source's.
bool SameImage(const Bytes& a, const Bytes& b, std::string& why) {
    Pe pa, pb;
    if (!ParsePe(a, pa) || !ParsePe(b, pb)) {
        why = "not a PE file";
        return false;
    }
    if (std::memcmp(&pa.nt->FileHeader, &pb.nt->FileHeader, sizeof pa.nt->FileHeader) != 0) {
        why = "the file header changed";
        return false;
    }
    IMAGE_OPTIONAL_HEADER32 oa = pa.nt->OptionalHeader, ob = pb.nt->OptionalHeader;
    for (IMAGE_OPTIONAL_HEADER32* o : {&oa, &ob}) {
        o->CheckSum = 0;
        o->SizeOfImage = 0;
        o->SizeOfInitializedData = 0;
        o->DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = 0;
    }
    if (std::memcmp(&oa, &ob, sizeof oa) != 0) {
        why = "the optional header changed beyond .rsrc's size";
        return false;
    }
    const unsigned n = pa.n;
    if (n == 0 || SectionName(pa.sec[n - 1]) != ".rsrc" || SectionName(pb.sec[n - 1]) != ".rsrc") {
        why = ".rsrc is not the last section";
        return false;
    }
    for (unsigned i = 0; i + 1 < n; ++i) {
        if (std::memcmp(&pa.sec[i], &pb.sec[i], sizeof pa.sec[i]) != 0 ||
            std::memcmp(a.data() + pa.sec[i].PointerToRawData, b.data() + pb.sec[i].PointerToRawData, pa.sec[i].SizeOfRawData) != 0) {
            why = "section " + SectionName(pa.sec[i]) + " changed";
            return false;
        }
    }
    const IMAGE_SECTION_HEADER &ra = pa.sec[n - 1], &rb = pb.sec[n - 1];
    const DWORD align = pb.nt->OptionalHeader.SectionAlignment;
    if (ra.VirtualAddress != rb.VirtualAddress || ra.Characteristics != rb.Characteristics ||
        pb.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress != rb.VirtualAddress ||
        pb.nt->OptionalHeader.SizeOfImage != ((rb.VirtualAddress + rb.Misc.VirtualSize + align - 1) / align) * align) {
        why = ".rsrc moved or the image size does not match it";
        return false;
    }
    if (b.size() != static_cast<std::size_t>(rb.PointerToRawData) + rb.SizeOfRawData &&
        a.size() == static_cast<std::size_t>(ra.PointerToRawData) + ra.SizeOfRawData) {
        why = "bytes after .rsrc";
        return false;
    }
    return true;
}

// The new file's group and images are what we wrote, and its manifest the
// source's.
bool ResourcesAsWritten(const std::wstring& source, const std::wstring& written, const IconImages& images, std::string& why) {
    DataFile src(source), dst(written);
    if (!src.m || !dst.m) {
        why = "cannot load " + std::string(src.m ? "the written file" : "the source") + " as a data file (error " +
              std::to_string(GetLastError()) + ")";
        return false;
    }
    Bytes group;
    if (!ResourceBytes(dst.m, kRtGroupIcon, kGroupName, group) || group != GroupResource(images)) {
        why = "the written group icon is not the one written";
        return false;
    }
    for (int i = 0; i < kFileCount; ++i) {
        Bytes img;
        if (!ResourceBytes(dst.m, kRtIcon, MAKEINTRESOURCEW(i + 1), img) || img != images.data[i]) {
            why = "icon image " + std::to_string(i + 1) + " is not the one written";
            return false;
        }
    }
    Bytes ma, mb;
    if (!ResourceBytes(src.m, kRtManifest, MAKEINTRESOURCEW(1), ma) || !ResourceBytes(dst.m, kRtManifest, MAKEINTRESOURCEW(1), mb) ||
        ma != mb) {
        why = "the manifest did not survive";
        return false;
    }
    return true;
}

// `source` copied to `target` with its group icon BOF3_ICON and that group's
// images replaced by `images` (ids 1..5, language neutral); then checked.
bool WriteExe(const std::wstring& source, const std::wstring& target, const IconImages& images, std::string& why) {
    if (!CopyFileW(source.c_str(), target.c_str(), FALSE)) {
        why = "cannot copy bof3x.exe (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    // What the source holds now: the group's languages and the image ids it names.
    std::vector<WORD> group_langs;
    std::vector<std::pair<WORD, WORD>> old_images;   // (id, language)
    {
        DataFile src(source);
        if (!src.m) {
            why = "cannot load bof3x.exe as a data file (error " + std::to_string(GetLastError()) + ")";
            DeleteFileW(target.c_str());
            return false;
        }
        group_langs = Languages(src.m, kRtGroupIcon, kGroupName);
        Bytes group;
        if (group_langs.empty() || !ResourceBytes(src.m, kRtGroupIcon, kGroupName, group) || group.size() < 6) {
            why = "bof3x.exe has no group icon BOF3_ICON";
            DeleteFileW(target.c_str());
            return false;
        }
        const unsigned count = group[4] | (group[5] << 8);
        for (unsigned i = 0; i < count && 6 + i * 14 + 14 <= group.size(); ++i) {
            const WORD id = static_cast<WORD>(group[6 + i * 14 + 12] | (group[6 + i * 14 + 13] << 8));
            for (WORD lang : Languages(src.m, kRtIcon, MAKEINTRESOURCEW(id))) old_images.push_back({id, lang});
        }
    }
    const char* step = nullptr;   // the step that failed
    DWORD error = 0;
    const auto check = [&](BOOL done, const char* what) {
        if (!done && !step) {
            step = what;
            error = GetLastError();
        }
        return done != FALSE;
    };
    HANDLE h = BeginUpdateResourceW(target.c_str(), FALSE);
    bool ok = check(h != nullptr, "BeginUpdateResource");
    for (const auto& [id, lang] : old_images)
        ok = ok && check(UpdateResourceW(h, kRtIcon, MAKEINTRESOURCEW(id), lang, nullptr, 0), "removing an old image");
    for (WORD lang : group_langs)
        ok = ok && check(UpdateResourceW(h, kRtGroupIcon, kGroupName, lang, nullptr, 0), "removing the old group");
    const WORD neutral = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
    for (int i = 0; i < kFileCount; ++i)
        ok = ok && check(UpdateResourceW(h, kRtIcon, MAKEINTRESOURCEW(i + 1), neutral,
                                         const_cast<std::uint8_t*>(images.data[i].data()),
                                         static_cast<DWORD>(images.data[i].size())),
                         "adding an image");
    Bytes group = GroupResource(images);
    ok = ok && check(UpdateResourceW(h, kRtGroupIcon, kGroupName, neutral, group.data(), static_cast<DWORD>(group.size())),
                     "adding the group");
    if (h) ok = check(EndUpdateResourceW(h, ok ? FALSE : TRUE), "EndUpdateResource") && ok;
    if (!ok) {
        why = std::string(step) + " failed (error " + std::to_string(error) + ")";
        DeleteFileW(target.c_str());
        return false;
    }
    Bytes a, b;
    if (!ReadWhole(source, a, why) || !ReadWhole(target, b, why) || !SameImage(a, b, why) ||
        !ResourcesAsWritten(source, target, images, why)) {
        why = "the written file failed its check: " + why;
        DeleteFileW(target.c_str());
        return false;
    }
    return true;
}

// --- the worker: bof3x.exe.new and the stamp, once an install ---------------------------

struct Job {
    Rgba base;
    std::string sheet_sha;
};

std::string Sha(const std::wstring& path) {
    std::string hex, why;
    return bof3x::Sha256Hex(path.c_str(), hex, why) ? hex : std::string();
}

void WriteFileIcon(const Job& job) {
    const std::wstring exe = ExePath();
    if (exe.empty()) {
        Log("DIV-0095    icon: the file icon was not written: cannot read the executable's path");
        return;
    }
    const std::wstring dir = DirOf(exe), pending = exe + L".new", work = dir + L"\\bof3x.icon-work.exe",
                       stamp_path = dir + L"\\bof3x.icon";
    const std::string current = Sha(exe);
    if (current.empty()) {
        Log("DIV-0095    icon: the file icon was not written: cannot hash %s", Utf8Path(exe.c_str()).text);
        return;
    }
    bof3x::IconStamp stamp;
    if (bof3x::IconStampRead(stamp_path, stamp) && stamp.sheet == job.sheet_sha) {
        if (current == stamp.to) {
            Log("DIV-0095    icon: bof3x.exe already carries this icon (bof3x.icon, sheet sha256 %.16s...)", job.sheet_sha.c_str());
            return;
        }
        if (current == stamp.from && FileExists(pending) && Sha(pending) == stamp.to) {
            Log("DIV-0095    icon: bof3x.exe.new already holds this icon; the launcher installs it at its next start");
            return;
        }
    }
    DeleteFileW(work.c_str());
    Com com;
    IconImages images;
    std::string why;
    if (!com.ok) why = com.why;
    const bool ok = com.ok && MakeImages(com, job.base, images, why) && WriteExe(exe, work, images, why);
    if (!ok) {
        Log("DIV-0095    icon: the file icon was not written: %s", why.c_str());
        return;
    }
    const bof3x::IconStamp fresh{job.sheet_sha, current, Sha(work)};
    if (fresh.to.empty()) {
        DeleteFileW(work.c_str());
        Log("DIV-0095    icon: the file icon was not written: cannot hash the written file");
        return;
    }
    // The stamp first: a launcher that finds the new file finds its stamp.
    if (!bof3x::IconStampWrite(stamp_path, fresh, why)) {
        DeleteFileW(work.c_str());
        Log("DIV-0095    icon: the file icon was not written: %s", why.c_str());
        return;
    }
    if (!MoveFileExW(work.c_str(), pending.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        const DWORD error = GetLastError();
        DeleteFileW(work.c_str());
        Log("DIV-0095    icon: the file icon was not written: cannot move it to bof3x.exe.new (error %lu)", error);
        return;
    }
    Log("DIV-0095    icon: %s written - bof3x.exe with the icon at 16, 24, 32, 48 and 256 px (sheet sha256 %.16s..., "
        "to %.16s...); the launcher moves it into place at its next start",
        Utf8Path(pending.c_str()).text, job.sheet_sha.c_str(), fresh.to.c_str());
}

DWORD WINAPI Worker(void* p) {
    Job* job = static_cast<Job*>(p);
    WriteFileIcon(*job);
    delete job;
    return 0;
}

// --- the run-time state ---------------------------------------------------------------

enum class State { kUnstarted, kWatching, kDone };
State g_state = State::kUnstarted;
Record g_record;
bool g_have_last = false;
std::uint32_t g_last = 0;

void SetWindowIcons(HWND hwnd, const Rgba& base) {
    const int big = GetSystemMetrics(SM_CXICON), small = GetSystemMetrics(SM_CXSMICON);
    std::string why;
    HICON hbig = big > 0 ? MakeIcon(Scale(base, big), big, why) : nullptr;
    HICON hsmall = hbig && small > 0 ? MakeIcon(Scale(base, small), small, why) : nullptr;
    if (!hbig || !hsmall) {
        Log("DIV-0095    icon: the window icon was not set: %s", big > 0 && small > 0 ? why.c_str() : "no icon size from GetSystemMetrics");
        if (hbig) DestroyIcon(hbig);
        return;
    }
    // Kept for the process's life: the window and its class hold them.
    SendMessageA(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(hbig));
    SendMessageA(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(hsmall));
    SetClassLongPtrA(hwnd, GCLP_HICON, reinterpret_cast<LONG_PTR>(hbig));
    SetClassLongPtrA(hwnd, GCLP_HICONSM, reinterpret_cast<LONG_PTR>(hsmall));
    Log("DIV-0095    icon: the window's and its class's icons set (%d and %d px)", big, small);
}

// --- the self-test's comparison -------------------------------------------------------

struct Frame {
    UINT w = 0, h = 0;
    Bytes bgra;
};

bool DecodeIco(Com& com, const std::wstring& path, std::vector<Frame>& frames, std::string& why) {
    IWICBitmapDecoder* dec = nullptr;
    HRESULT hr = com.wic->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                              WICDecodeMetadataCacheOnDemand, &dec);
    UINT n = 0;
    if (SUCCEEDED(hr)) hr = dec->GetFrameCount(&n);
    for (UINT i = 0; i < n && SUCCEEDED(hr); ++i) {
        IWICBitmapFrameDecode* fr = nullptr;
        IWICFormatConverter* conv = nullptr;
        Frame f;
        hr = dec->GetFrame(i, &fr);
        if (SUCCEEDED(hr)) hr = fr->GetSize(&f.w, &f.h);
        if (SUCCEEDED(hr)) hr = com.wic->CreateFormatConverter(&conv);
        if (SUCCEEDED(hr))
            hr = conv->Initialize(fr, GUID_WICPixelFormat32bppBGRA,
                                                WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
        if (SUCCEEDED(hr)) {
            f.bgra.resize(static_cast<std::size_t>(f.w) * f.h * 4);
            hr = conv->CopyPixels(nullptr, f.w * 4, static_cast<UINT>(f.bgra.size()), f.bgra.data());
        }
        if (conv) conv->Release();
        if (fr) fr->Release();
        if (SUCCEEDED(hr)) frames.push_back(std::move(f));
    }
    if (dec) dec->Release();
    if (FAILED(hr)) why = "WIC cannot decode " + std::string(Utf8Path(path.c_str()).text) + " (0x" + Com::Hex(hr) + ")";
    return SUCCEEDED(hr);
}

const Frame* FrameOf(const std::vector<Frame>& frames, int s) {
    const Frame* found = nullptr;
    for (const Frame& f : frames)
        if (static_cast<int>(f.w) == s && static_cast<int>(f.h) == s) {
            if (found) return nullptr;   // two of one size: ambiguous
            found = &f;
        }
    return found;
}

std::wstring EnvPath(const wchar_t* name) {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH);
    return n == 0 || n >= MAX_PATH ? std::wstring() : std::wstring(buf, n);
}

}  // namespace

void ExeIcon_Frame(void* hwnd) {
    if (g_state == State::kDone) return;
    if (g_state == State::kUnstarted) {
        if (!IsExeHost()) {   // the hosted build keeps Capcom's icon
            g_state = State::kDone;
            return;
        }
        if (!ReadRecord(g_record)) {
            Log("DIV-0095    icon: icon %u's record (u %d, v %d, CLUT y %d) leaves the page or VRAM; no icon generated", kIcon,
                g_record.u, g_record.v, g_record.clut_y);
            g_state = State::kDone;
            return;
        }
        g_state = State::kWatching;
    }
    const std::uint16_t* vram = Gfx_VramShadow;
    if (!SheetReady(vram, g_record)) {
        g_have_last = false;
        return;
    }
    // The same bytes on two drawn frames running: an upload is complete.
    std::vector<std::uint8_t> sheet = SheetBytes(vram, g_record);
    const std::uint32_t h = Fnv1a(sheet);
    if (!g_have_last || h != g_last) {
        g_have_last = true;
        g_last = h;
        return;
    }
    g_state = State::kDone;
    const Rgba base = Render(vram, g_record);
    Log("DIV-0095    icon: icon %u's sheet and CLUT in the VRAM shadow at Frame_Counter %u (u %d, v %d, CLUT %d,%d)", kIcon,
        static_cast<unsigned>(Frame_Counter), g_record.u, g_record.v, g_record.clut_x, g_record.clut_y);
    SetWindowIcons(static_cast<HWND>(hwnd), base);
    auto* job = new Job{base, std::string()};
    if (!bof3x::Sha256HexBytes(sheet.data(), sheet.size(), job->sheet_sha)) {
        Log("DIV-0095    icon: the file icon was not written: cannot hash the sheet");
        delete job;
        return;
    }
    HANDLE t = CreateThread(nullptr, 0, &Worker, job, 0, nullptr);
    if (!t) {
        Log("DIV-0095    icon: the file icon was not written: CreateThread failed (error %lu)", GetLastError());
        delete job;
        return;
    }
    CloseHandle(t);
}

void ExeIcon_SelfTest() {
    const std::wstring vram_path = EnvPath(L"BOF3X_ICON_VRAM");
    if (vram_path.empty()) return;
    const std::wstring ref_path = EnvPath(L"BOF3X_ICON_REF");
    if (ref_path.empty()) Fatal("icon self-test: BOF3X_ICON_VRAM is set without BOF3X_ICON_REF (the expected .ico)");
    std::string why;
    Bytes dump;
    if (!ReadWhole(vram_path, dump, why)) Fatal("icon self-test: %s", why.c_str());
    if (dump.size() != static_cast<std::size_t>(kVramW) * kVramH * 2)
        Fatal("icon self-test: %s is %u bytes, not a 1 MB dump of the VRAM shadow", Utf8Path(vram_path.c_str()).text,
              static_cast<unsigned>(dump.size()));
    std::vector<std::uint16_t> vram(static_cast<std::size_t>(kVramW) * kVramH);
    std::memcpy(vram.data(), dump.data(), dump.size());

    Record r;
    if (!ReadRecord(r)) Fatal("icon self-test: icon %u's record leaves the page or VRAM", kIcon);
    if (!SheetReady(vram.data(), r))
        Fatal("icon self-test: the dump does not hold icon %u's sheet and CLUT (u %d, v %d, CLUT %d,%d)", kIcon, r.u, r.v,
              r.clut_x, r.clut_y);
    const Rgba base = Render(vram.data(), r);

    Com com;
    if (!com.ok) Fatal("icon self-test: %s", com.why.c_str());
    IconImages images;
    if (!MakeImages(com, base, images, why)) Fatal("icon self-test: %s", why.c_str());
    const std::wstring dir = DllDir();
    const std::wstring ico = dir + L"\\bof3x.icon-test.ico", exe_out = dir + L"\\bof3x.icon-test.exe";
    if (!WriteWhole(ico, IcoFile(images), why)) Fatal("icon self-test: %s", why.c_str());

    // Every pixel of the five images against the reference, both files
    // decoded by WIC (the reference's entries are PNGs, ours DIBs and a PNG).
    std::vector<Frame> mine, ref;
    if (!DecodeIco(com, ico, mine, why) || !DecodeIco(com, ref_path, ref, why)) Fatal("icon self-test: %s", why.c_str());
    if (mine.size() != static_cast<std::size_t>(kFileCount) || ref.size() != static_cast<std::size_t>(kFileCount))
        Fatal("icon self-test: %u images written, %u in the reference; %d expected", static_cast<unsigned>(mine.size()),
              static_cast<unsigned>(ref.size()), kFileCount);
    unsigned total = 0;
    for (int s : kFileSizes) {
        const Frame *a = FrameOf(mine, s), *b = FrameOf(ref, s);
        if (!a || !b) Fatal("icon self-test: no single %d px image in %s", s, a ? "the reference" : "ours");
        unsigned differ = 0, first = 0;
        for (std::size_t i = 0; i < a->bgra.size(); i += 4)
            if (std::memcmp(&a->bgra[i], &b->bgra[i], 4) != 0) {
                if (!differ) first = static_cast<unsigned>(i / 4);
                ++differ;
            }
        if (differ)
            Fatal("icon self-test: the %d px image differs from the reference in %u pixels, the first at %u,%u (ours "
                  "BGRA %02X%02X%02X%02X, the reference %02X%02X%02X%02X)",
                  s, differ, first % s, first / s, a->bgra[first * 4], a->bgra[first * 4 + 1], a->bgra[first * 4 + 2],
                  a->bgra[first * 4 + 3], b->bgra[first * 4], b->bgra[first * 4 + 1], b->bgra[first * 4 + 2],
                  b->bgra[first * 4 + 3]);
        total += static_cast<unsigned>(a->bgra.size() / 4);
    }
    Log("DIV-0095    icon self-test: %s rendered from %s - 16, 24, 32, 48 and 256 px, all %u pixels identical to %s",
        Utf8Path(ico.c_str()).text, Utf8Path(vram_path.c_str()).text, total, Utf8Path(ref_path.c_str()).text);

    // The resources: the same write as the run time's, into a test copy.
    const std::wstring exe = ExePath();
    if (exe.empty() || !WriteExe(exe, exe_out, images, why)) Fatal("icon self-test: the executable's resources: %s", why.c_str());
    {
        DataFile out(exe_out);
        HICON loaded = out.m ? static_cast<HICON>(LoadImageW(out.m, kGroupName, IMAGE_ICON, 32, 32, 0)) : nullptr;
        if (!loaded) Fatal("icon self-test: LoadImage of BOF3_ICON from %s failed (error %lu)", Utf8Path(exe_out.c_str()).text, GetLastError());
        DestroyIcon(loaded);
    }
    Log("DIV-0095    icon self-test: %s written from %s - .rsrc's group BOF3_ICON and its 5 images read back as "
        "written, the manifest kept, every other section and header word unchanged; LoadImage reads the group",
        Utf8Path(exe_out.c_str()).text, Utf8Path(exe.c_str()).text);

    // The window icons' path, without a window.
    for (int s : {GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXSMICON)}) {
        HICON icon = s > 0 ? MakeIcon(Scale(base, s), s, why) : nullptr;
        if (!icon) Fatal("icon self-test: the %d px window icon: %s", s, s > 0 ? why.c_str() : "no size from GetSystemMetrics");
        DestroyIcon(icon);
    }
    Log("DIV-0095    icon self-test: window icons built at %d and %d px", GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXSMICON));
}

}  // namespace bof3
