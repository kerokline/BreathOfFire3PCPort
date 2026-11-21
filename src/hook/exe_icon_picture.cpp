// DIV-0095's picture, portable (exe_icon_picture.h): moved out of
// exe_icon.cpp unchanged in wave B (docs/linux-round.md, "B1 as run").
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
#include "hook/exe_icon_picture.h"

#include <cmath>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/menu_windows_callees.h"
#include "hook/host.h"
#include "hook/log.h"

namespace bof3::exe_icon {
namespace {

// --- the picture -------------------------------------------------------------

constexpr int kPageX = 896, kPageY = 256;    // texture page 0x1E: (0x1E & 0xF) * 64, (0x1E >> 4) * 256
constexpr int kIconW = 0x28, kIconH = 0x30;  // Menu_DrawBigIcon's SPRT
constexpr int kClutBaseY = 0x1E0;
constexpr int kSide = kIconW;                // the square: 40 x 40
constexpr int kCropTop = (kIconH - kIconW) / 2;   // the centre crop: rows 4..43 (the owner's choice, 2026-10-11)

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

std::uint32_t Fnv1a(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t h = 2166136261u;
    for (std::uint8_t b : bytes) h = (h ^ b) * 16777619u;
    return h;
}

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

// --- the .ico images -------------------------------------------------------------

void Put16(Bytes& b, unsigned v) {
    b.push_back(static_cast<std::uint8_t>(v));
    b.push_back(static_cast<std::uint8_t>(v >> 8));
}
void Put32(Bytes& b, std::uint32_t v) {
    Put16(b, v & 0xFFFF);
    Put16(b, v >> 16);
}

constexpr std::uint32_t kBiRgb = 0;   // BI_RGB

const Frame* FrameOf(const std::vector<Frame>& frames, int s) {
    const Frame* found = nullptr;
    for (const Frame& f : frames)
        if (static_cast<int>(f.w) == s && static_cast<int>(f.h) == s) {
            if (found) return nullptr;   // two of one size: ambiguous
            found = &f;
        }
    return found;
}

// --- the run-time state ---------------------------------------------------------------

enum class State { kUnstarted, kWatching, kDone };
State g_state = State::kUnstarted;
Record g_record;
bool g_have_last = false;
std::uint32_t g_last = 0;

}  // namespace

// The record of icon kIcon in the table, as the copied .data holds it. False
// when it would leave the page or VRAM (make_icon.py's end-of-table test).
bool ReadRecord(Record& r) {
    const auto* p = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(::bof3::Img(menu_windows::at::kIconRects) + kIcon * 4));
    r = {p[0], p[1], p[2] & 0xF0, kClutBaseY + p[3]};
    return r.u + kIconW <= 256 && r.v + kIconH <= 256 && r.clut_y < kVramH;
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

// The centre 40 x 40 of the 40 x 48 sprite.
Rgba Render(const std::uint16_t* vram, const Record& r) {
    std::uint16_t clut[16];
    for (int i = 0; i < 16; ++i) clut[i] = Cell(vram, r.clut_x + i, r.clut_y);
    Rgba out(kSide * kSide * 4);
    for (int y = 0; y < kSide; ++y)
        for (int x = 0; x < kSide; ++x) PutColour(&out[(y * kSide + x) * 4], clut[Texel(vram, r, x, kCropTop + y)]);
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

bool WatchFrame(Rgba& base) {
    if (g_state == State::kDone) return false;
    if (g_state == State::kUnstarted) {
        if (!IsExeHost()) {   // the hosted build keeps Capcom's icon
            g_state = State::kDone;
            return false;
        }
        if (!ReadRecord(g_record)) {
            Log("DIV-0095    icon: icon %u's record (u %d, v %d, CLUT y %d) leaves the page or VRAM; no icon generated", kIcon,
                g_record.u, g_record.v, g_record.clut_y);
            g_state = State::kDone;
            return false;
        }
        g_state = State::kWatching;
    }
    const std::uint16_t* vram = Gfx_VramShadow;
    if (!SheetReady(vram, g_record)) {
        g_have_last = false;
        return false;
    }
    // The same bytes on two drawn frames running: an upload is complete.
    std::vector<std::uint8_t> sheet = SheetBytes(vram, g_record);
    const std::uint32_t h = Fnv1a(sheet);
    if (!g_have_last || h != g_last) {
        g_have_last = true;
        g_last = h;
        return false;
    }
    g_state = State::kDone;
    base = Render(vram, g_record);
    Log("DIV-0095    icon: icon %u's sheet and CLUT in the VRAM shadow at Frame_Counter %u (u %d, v %d, CLUT %d,%d)", kIcon,
        static_cast<unsigned>(Frame_Counter), g_record.u, g_record.v, g_record.clut_x, g_record.clut_y);
    return true;
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
    Put32(b, kBiRgb);
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

Rgba SelfTestPicture(const Bytes& dump, const char* dump_name) {
    if (dump.size() != static_cast<std::size_t>(kVramW) * kVramH * 2)
        Fatal("icon self-test: %s is %u bytes, not a 1 MB dump of the VRAM shadow", dump_name,
              static_cast<unsigned>(dump.size()));
    std::vector<std::uint16_t> vram(static_cast<std::size_t>(kVramW) * kVramH);
    std::memcpy(vram.data(), dump.data(), dump.size());

    Record r;
    if (!ReadRecord(r)) Fatal("icon self-test: icon %u's record leaves the page or VRAM", kIcon);
    if (!SheetReady(vram.data(), r))
        Fatal("icon self-test: the dump does not hold icon %u's sheet and CLUT (u %d, v %d, CLUT %d,%d)", kIcon, r.u, r.v,
              r.clut_x, r.clut_y);
    return Render(vram.data(), r);
}

unsigned CompareIco(const std::vector<Frame>& mine, const std::vector<Frame>& ref) {
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
    return total;
}

}  // namespace bof3::exe_icon
