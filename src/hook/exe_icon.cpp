// Windows-only by design (docs/linux-round.md, LB1): it reads the icon resource out of the PE image.
// DIV-0095: the game window's icon, generated from the game's own data on the
// player's machine (docs/exe-icon.md; exe_icon.h says when and from what).
// The file's icon is ours, from the build (data/icon/bof3x.ico); since
// 2026-10-15 nothing here writes an executable.
//
// The picture itself - the record, the sheet, the render, the sizes, the
// .ico images and the comparison - is portable: exe_icon_picture.cpp, split
// out in wave B (docs/linux-round.md, "B1 as run") so that the Linux icon
// (exe_icon_sdl.cpp) shares it. What is left here is Windows': HICONs, WIC's
// PNG encoder and decoder, the files, the window's messages.
#include "hook/exe_icon.h"
#include "hook/exe_icon_picture.h"

#include <windows.h>

#include <objidl.h>
#include <wincodec.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "bof3/symbols.gen.h"
#include "hook/host.h"
#include "hook/log.h"

namespace bof3 {
namespace {

using namespace exe_icon;

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

std::wstring EnvPath(const wchar_t* name) {
    wchar_t buf[MAX_PATH];
    const DWORD n = GetEnvironmentVariableW(name, buf, MAX_PATH);
    return n == 0 || n >= MAX_PATH ? std::wstring() : std::wstring(buf, n);
}

}  // namespace

void ExeIcon_Frame(void* hwnd) {
    Rgba base;
    if (!WatchFrame(base)) return;
    // The window's icon only: bof3x.exe's file carries its own from the build
    // (data/icon/bof3x.ico, tools/make_exe.py) and is never rewritten
    // (2026-10-15: Defender quarantined the launcher over the rewrite chain).
    SetWindowIcons(static_cast<HWND>(hwnd), base);
}

void ExeIcon_SelfTest() {
    const std::wstring vram_path = EnvPath(L"BOF3X_ICON_VRAM");
    if (vram_path.empty()) return;
    const std::wstring ref_path = EnvPath(L"BOF3X_ICON_REF");
    if (ref_path.empty()) Fatal("icon self-test: BOF3X_ICON_VRAM is set without BOF3X_ICON_REF (the expected .ico)");
    std::string why;
    Bytes dump;
    if (!ReadWhole(vram_path, dump, why)) Fatal("icon self-test: %s", why.c_str());
    const Rgba base = SelfTestPicture(dump, Utf8Path(vram_path.c_str()).text);

    Com com;
    if (!com.ok) Fatal("icon self-test: %s", com.why.c_str());
    IconImages images;
    if (!MakeImages(com, base, images, why)) Fatal("icon self-test: %s", why.c_str());
    const std::wstring dir = DllDir();
    const std::wstring ico = dir + L"\\bof3x.icon-test.ico";
    if (!WriteWhole(ico, IcoFile(images), why)) Fatal("icon self-test: %s", why.c_str());

    // Every pixel of the five images against the reference, both files
    // decoded by WIC (the reference's entries are PNGs, ours DIBs and a PNG).
    std::vector<Frame> mine, ref;
    if (!DecodeIco(com, ico, mine, why) || !DecodeIco(com, ref_path, ref, why)) Fatal("icon self-test: %s", why.c_str());
    const unsigned total = CompareIco(mine, ref);
    Log("DIV-0095    icon self-test: %s rendered from %s - 16, 24, 32, 48 and 256 px, all %u pixels identical to %s",
        Utf8Path(ico.c_str()).text, Utf8Path(vram_path.c_str()).text, total, Utf8Path(ref_path.c_str()).text);

    // The window icons' path, without a window.
    for (int s : {GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXSMICON)}) {
        HICON icon = s > 0 ? MakeIcon(Scale(base, s), s, why) : nullptr;
        if (!icon) Fatal("icon self-test: the %d px window icon: %s", s, s > 0 ? why.c_str() : "no size from GetSystemMetrics");
        DestroyIcon(icon);
    }
    Log("DIV-0095    icon self-test: window icons built at %d and %d px", GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXSMICON));
}

}  // namespace bof3
