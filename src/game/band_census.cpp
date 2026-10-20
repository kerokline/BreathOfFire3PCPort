// The band census, BOF3X_BANDLOG (docs/band-census.md): which sprites the
// wide picture shows that the original's 320 columns did not. An instrument:
// it reads, and writes its own file only.
#include "game/band_census.h"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#include "bof3/symbols.gen.h"
#include "game/record_views.h"
#include "game/wide_staging.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/input_script.h"
#include "hook/log.h"

namespace band_census {

bool g_on = false;

namespace {

// DIV-0041's 16:9 picture adds 53 columns a side. The census calls a sprite
// "16:9" when it touches -53..0 or 320..373 - what the 16:9 mode shows - and
// "21:9 only" when it lies wholly beyond those. A definition of the class the
// census reports, not a width the game is drawn at: the wide picture's own
// width is Widescreen_Live()'s, read at Arm().
constexpr float kBand169 = 53.0f;
constexpr float kViewW = 320.0f, kViewH = 240.0f;

constexpr unsigned kObjects = 30, kExtra = 4, kMembers = 3;
constexpr unsigned kObjectBytes = 0xA4, kMemberBytes = 0x14C;
// BOF3.exe's image (ImageBase 0x400000, SizeOfImage 0x53F000): a script
// pointer read through the area descriptor is followed only inside it.
constexpr std::uint32_t kImageLo = 0x400000, kImageHi = 0x93F000;

template <class T> T Get(const unsigned char* p, unsigned offset) {
    T v;
    std::memcpy(&v, p + offset, sizeof v);
    return v;
}
std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
const unsigned char* Pointer(std::uint32_t a) { return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a)); }
bool InImage(std::uint32_t a) { return a >= kImageLo && a < kImageHi; }

// --- what one sprite on the draw list is, this frame -------------------------

enum Who : int { kObj, kExt, kParty, kKind2, kOther };
const char* const kWhoName[] = {"obj", "ext", "party", "kind2", "other"};

// The movement script's state (docs/movement-script.md sections 1, 1b),
// read for the 30 field objects only - the ones Field_ObjectUpdate 0x517BF0
// steps through the area descriptor's +0x10 scripts.
enum Script : int { kNone, kRun, kWait, kEnd, kUnread };
const char* const kScriptName[] = {"none", "run", "wait", "end", "unread"};

// The scene mask: what says the player does not have the leader. Each bit is
// a test the game itself makes before the leader may move:
//   1  the leader's frame held: (Field_ScriptFlags | Field_ScriptFlags2) bit 0
//      - Field_MembersFrame 0x517350 skips member 0 (field_frame.cpp);
//   2  Field_ScriptFlags bit 0x100 - Field_LeaderStand 0x52DA70 returns
//      first thing (field_event.cpp); set by script ops F1 / 80
//      (ScriptFlags_Set40), cleared by F0 / 81 / E5;
//   4  Field_ScriptFlags2 bit 0x40 - Field_LeaderStand's second return;
//   8  Field_Request == 5 - its third;
//   0x10  the leader's state (ObjTrio +1) is not 1, Field_LeaderControl -
//      a menu, an encounter, a passage, a swap, an area's own state: kept
//      for the table, not part of "scene".
constexpr unsigned kSceneBits = 0xF;

struct Sample {
    unsigned frame, recipe;
    unsigned area, mode;
    int chapter;
    int who, index;
    unsigned type;
    std::uint32_t sprite;          // +0x54, the frame table: names the sprite's bank
    float left, right, top, bottom;
    std::int32_t wx, wz;           // +0x34 / +0x38, the map position
    unsigned wy;                   // +0x3E, the height
    int script;
    unsigned mask;
    bool hidden;                   // DIV-0124's hide leaves it undrawn this frame (wide_staging::WouldHide)
};

// --- rows ---------------------------------------------------------------------

enum Parked : int { kMoving, kStill, kParked };
const char* const kParkedName[] = {"no", "still", "yes"};

struct Row {
    unsigned area, mode;
    int chapter, who, index;
    unsigned type;
    std::uint32_t sprite;
    unsigned first, last, seen, rfirst, rlast;
    float xlo, xhi, ylo, yhi;
    int cls, parked;
    bool scene;
    unsigned mask;
    int script;
    bool hidden;
};

struct Track {
    bool have = false, open = false;
    std::int32_t wx = 0, wz = 0;
    unsigned wy = 0, moved = 0, seen = 0;
    Row row{};
};

unsigned g_columns = 0;
unsigned g_park = 30;    // BOF3X_BANDLOG_PARK: frames without a move before an object counts as parked
// BOF3X_BANDLOG_SPAN: a row is closed after this many frames and a new one
// opened (tools/band_census.py joins them again). The run tools end the game
// with taskkill /F (attract_run.py kill_game): no exit handler runs, so a row
// still open then is lost - at most this many frames of it.
unsigned g_span = 60;
constexpr unsigned kGap = 2;   // a row (and stillness) survives one frame unseen
unsigned g_frame = ~0u;
std::unordered_map<std::uint32_t, Track> g_tracks;
FILE* g_out = nullptr;
unsigned g_rows = 0;

using Emit = void (*)(const Row&);
void WriteRow(const Row& r);
Emit g_emit = WriteRow;

const char* SideName(int cls) { return cls == kLeft169 || cls == kLeft219 ? "L" : "R"; }
const char* BandName(int cls) { return cls == kLeft169 || cls == kRight169 ? "16:9" : "21:9"; }

void WriteRow(const Row& r) {
    if (!g_out) return;
    std::fprintf(g_out, "%u\t%d\t%u\t%s\t%d\t%u\t%08X\t%u\t%u\t%u\t%u\t%u\t%.1f\t%.1f\t%.1f\t%.1f\t%s\t%s\t%s\t%u\t%02X\t%s\t%s\n",
                 r.area, r.chapter, r.mode, kWhoName[r.who], r.index, r.type, r.sprite, r.first, r.last, r.seen, r.rfirst,
                 r.rlast, r.xlo, r.xhi, r.ylo, r.yhi, SideName(r.cls), BandName(r.cls), kParkedName[r.parked],
                 r.scene ? 1u : 0u, r.mask, kScriptName[r.script], r.hidden ? "yes" : "no");
    std::fflush(g_out);
}

void Close(Track& t) {
    if (!t.open) return;
    t.open = false;
    ++g_rows;
    g_emit(t.row);
}

void Sweep(unsigned frame) {
    for (auto& kv : g_tracks)
        if (kv.second.open && frame > kv.second.row.last + kGap) Close(kv.second);
}

void CloseAll() {
    for (auto& kv : g_tracks) Close(kv.second);
}

bool SameRow(const Row& r, const Sample& s, int cls, int parked, bool scene) {
    return r.area == s.area && r.mode == s.mode && r.chapter == s.chapter && r.who == s.who && r.index == s.index &&
           r.type == s.type && r.sprite == s.sprite && r.cls == cls && r.parked == parked && r.scene == scene &&
           r.hidden == s.hidden;
}

void Process(std::uint32_t key, const Sample& s) {
    if (s.frame != g_frame) {
        g_frame = s.frame;
        Sweep(s.frame);
    }
    Track& t = g_tracks[key];
    // Stillness: the map position and height unchanged since `moved`, the
    // object seen on every frame between (or with one frame's gap).
    if (!t.have || s.frame > t.seen + kGap || s.wx != t.wx || s.wz != t.wz || s.wy != t.wy) t.moved = s.frame;
    t.have = true;
    t.wx = s.wx;
    t.wz = s.wz;
    t.wy = s.wy;
    t.seen = s.frame;

    const int cls = Classify(s.left, s.right, s.top, s.bottom, g_columns);
    if (cls == kNotShown || cls == kInside) {
        Close(t);
        return;
    }
    int parked = kMoving;
    if (s.frame - t.moved >= g_park) {
        if (s.script == kWait || s.script == kEnd) parked = kParked;
        else if (s.script != kRun) parked = kStill;   // no movement script read for this object
    }
    const bool scene = (s.mask & kSceneBits) != 0;
    if (t.open && (!SameRow(t.row, s, cls, parked, scene) || s.frame > t.row.last + kGap ||
                   s.frame - t.row.first >= g_span))
        Close(t);
    Row& r = t.row;
    if (!t.open) {
        t.open = true;
        r.area = s.area;
        r.mode = s.mode;
        r.chapter = s.chapter;
        r.who = s.who;
        r.index = s.index;
        r.type = s.type;
        r.sprite = s.sprite;
        r.first = r.last = s.frame;
        r.seen = 1;
        r.rfirst = r.rlast = s.recipe;
        r.xlo = s.left;
        r.xhi = s.right;
        r.ylo = s.top;
        r.yhi = s.bottom;
        r.cls = cls;
        r.parked = parked;
        r.scene = scene;
        r.mask = s.mask;
        r.script = s.script;
        r.hidden = s.hidden;
        return;
    }
    if (s.frame != r.last) ++r.seen;
    r.last = s.frame;
    r.rlast = s.recipe;
    if (s.left < r.xlo) r.xlo = s.left;
    if (s.right > r.xhi) r.xhi = s.right;
    if (s.top < r.ylo) r.ylo = s.top;
    if (s.bottom > r.yhi) r.yhi = s.bottom;
    r.mask |= s.mask;
    r.script = s.script;
}

// --- reading a sprite ---------------------------------------------------------

// The rectangle Sprite_Draw 0x5935B0 (ours, sprite_draw.cpp) and the
// Direct3D end draw for this sprite, in the game's 320 x 240 units; false for
// a sprite Sprite_Draw draws nothing of. The cells are the frame's pieces
// (+0x54's data + u16 +0x5A: a count, then 5 bytes a piece - size index,
// signed x and y, u, v), each SpriteCell_Sizes' w x h as SpriteCell_Add
// packs it into a byte and DrawCells 0x5A32B0 (tex_cells.cpp) unpacks it; the
// extent always holds the origin. D3d_DrawCellSprite 0x5A2EB0 (d3d_draw.cpp)
// puts it at the screen point +0x74 / +0x78 times the scale, mirrored about
// the point when +0x2A sets the mode's 0x400. The scale is Sprite_Draw's:
// for +0x48 == 1 without +0x24 bit 7, 0x4650000 / the depth +0x60 with the
// low byte cleared, in 16.16; else +0x40 / +0x44 when +0x48 is set; else 1.
bool Rect(const unsigned char* s, float& left, float& right, float& top, float& bottom) {
    if (s[0] & 0x40) return false;
    float sx = 1.0f, sy = 1.0f;
    if (!(s[0x24] & 0x80) && s[0x48] == 1) {
        const long depth = Get<long>(s, 0x60);
        if (depth == 0) return false;
        sx = sy = static_cast<float>(static_cast<double>((0x4650000L / depth) & ~0xFFL) * (1.0 / 65536));
    } else if (s[0x48] != 0) {
        sx = static_cast<float>(static_cast<double>(Get<long>(s, 0x40)) * (1.0 / 65536));
        sy = static_cast<float>(static_cast<double>(Get<long>(s, 0x44)) * (1.0 / 65536));
    }
    const float x = Get<float>(s, 0x74), y = Get<float>(s, 0x78);
    int l = 0, r = 0, t = 0, b = 0;
    const std::uint32_t table = Get<std::uint32_t>(s, 0x54);
    if (table != 0) {
        const unsigned char* frame = Pointer(table) + Get<std::uint16_t>(s, 0x5A);
        const unsigned count = frame[0];
        if (count == 0) return false;
        const unsigned char* piece = frame + 1;
        for (unsigned i = 0; i < count; ++i, piece += 5) {
            const unsigned size = (piece[0] & 0xFu) * 2;
            const unsigned w0 = SpriteCell_Sizes[size], h0 = SpriteCell_Sizes[size + 1];
            const unsigned packed = (((h0 & 0xF8u) << 1) | ((w0 & 0xFFu) >> 3)) & 0xFFu;
            const int w = static_cast<int>((packed & 0xFu) << 3), h = static_cast<int>((packed >> 1) & 0x78u);
            const int px = static_cast<signed char>(piece[1]), py = static_cast<signed char>(piece[2]);
            if (px < l) l = px;
            if (px + w > r) r = px + w;
            if (py < t) t = py;
            if (py + h > b) b = py + h;
        }
    }
    if (s[0x2A] != 0) {   // mirrored: x - extent * scale
        left = x - static_cast<float>(r) * sx;
        right = x - static_cast<float>(l) * sx;
    } else {
        left = x + static_cast<float>(l) * sx;
        right = x + static_cast<float>(r) * sx;
    }
    top = y + static_cast<float>(t) * sy;
    bottom = y + static_cast<float>(b) * sy;
    if (r == l) right = left + 1.0f;   // no frame read: the point itself
    if (b == t) bottom = top + 1.0f;
    return true;
}

// The op at the script position: at an end, at a wait, or running. Read from
// the handlers (move_script.cpp, move_groups.cpp): FF steps the position back
// onto itself (the end); EF, D8 and E6 end the script and the position has
// gone past them (so the byte before is one of them); 86 hands the sprite a
// +0x18 script and stays on itself. Waits step back onto themselves while
// their test holds: FD (Sprite_ScriptTickOnce), D9 (Field_Request 2), DA
// (Transition_Busy), E9 and F9 (their tests), and an A0..AF counter op
// the flow pass stopped on.
int ScriptAt(unsigned char op, unsigned char before) {
    if (op == 0xFF || op == 0x86) return kEnd;
    if (before == 0xEF || before == 0xD8 || before == 0xE6) return kEnd;
    if (op == 0xFD || op == 0xD9 || op == 0xDA || op == 0xE9 || op == 0xF9 || (op >= 0xA0 && op <= 0xAF)) return kWait;
    return kRun;
}

int ScriptOf(const unsigned char* o, unsigned area) {
    if (o[9] != 0 || o[0x87] != 0) return kWait;   // a timed move or its wait (Field_ObjectMotion 0x518980)
    if (o[0x84] == 0) return kEnd;                 // speed 0: Field_ObjectUpdate never steps it
    if (area >= Area_Descriptors_count) return kUnread;
    const std::uint32_t descriptor = Address(Area_Descriptors[area]);
    if (!InImage(descriptor)) return kUnread;
    const std::uint32_t scripts = Get<std::uint32_t>(Pointer(descriptor), 0x10);
    if (!InImage(scripts)) return kUnread;
    const std::uint32_t base = Get<std::uint32_t>(Pointer(scripts), o[0x83] * 4u);
    if (!InImage(base)) return kUnread;
    const unsigned pos = Get<std::uint16_t>(o, 0x8A);
    if (!InImage(base + pos)) return kUnread;
    return ScriptAt(Pointer(base)[pos], pos ? Pointer(base)[pos - 1] : 0);
}

void Identify(const unsigned char* s, int& who, int& index) {
    const std::uint32_t a = Address(s);
    const std::uint32_t objects = Address(Sprite_Objects), extra = Address(Sprite_ObjectsExtra), trio = Address(ObjTrio);
    who = kOther;
    index = -1;
    if (a >= objects && a < objects + kObjects * kObjectBytes && (a - objects) % kObjectBytes == 0) {
        who = kObj;
        index = static_cast<int>((a - objects) / kObjectBytes);
    } else if (a >= extra && a < extra + kExtra * kObjectBytes && (a - extra) % kObjectBytes == 0) {
        who = kExt;
        index = static_cast<int>(kObjects + (a - extra) / kObjectBytes);   // Sprite_ObjectByHandle's numbering
    } else if (a >= trio && a < trio + kMembers * kMemberBytes && (a - trio) % kMemberBytes == 0) {
        who = kParty;
        index = static_cast<int>((a - trio) / kMemberBytes);
    } else if (a == Address(rv::Bytes(&Sprite_Kind2))) {
        who = kKind2;
        index = 0;
    }
}

}  // namespace

bool ScreenRect(const unsigned char* sprite, float& left, float& right, float& top, float& bottom) {
    return Rect(sprite, left, right, top, bottom);
}

unsigned SceneMask() {
    return (static_cast<unsigned char>(Field_ScriptFlags | Field_ScriptFlags2) & 1u) |
           ((Field_ScriptFlags & 0x100) ? 2u : 0u) | ((Field_ScriptFlags2 & 0x40) ? 4u : 0u) |
           (Field_Request == 5 ? 8u : 0u) | (ObjTrio[1] != 1 ? 0x10u : 0u);
}

Class Classify(float left, float right, float top, float bottom, unsigned columns) {
    if (!(right > left) || !(bottom > top)) return kNotShown;   // NaNs included
    const float c = static_cast<float>(columns);
    if (bottom <= 0.0f || top >= kViewH) return kNotShown;
    if (right <= -c || left >= kViewW + c) return kNotShown;
    if (right > 0.0f && left < kViewW) return kInside;
    if (right <= 0.0f) return right > -kBand169 ? kLeft169 : kLeft219;
    return left < kViewW + kBand169 ? kRight169 : kRight219;
}

void Record(const unsigned char* s) {
    Sample x;
    if (!Rect(s, x.left, x.right, x.top, x.bottom)) return;
    x.frame = Frame_Counter;
    x.recipe = bof3::InputScript_Frame();
    x.area = Game_AreaNumber;
    x.mode = Game_Mode;
    x.chapter = Cond_ByteFA;
    Identify(s, x.who, x.index);
    x.type = s[6];
    x.sprite = Get<std::uint32_t>(s, 0x54);
    x.wx = Get<std::int32_t>(s, 0x34);
    x.wz = Get<std::int32_t>(s, 0x38);
    x.wy = Get<std::uint16_t>(s, 0x3E);
    x.script = x.who == kObj ? ScriptOf(s, x.area) : kNone;
    x.mask = SceneMask();
    x.hidden = wide_staging::WouldHide(s);
    Process(Address(s), x);
}

namespace {

// --- BOF3X_SHADOW=band_census: the self-test ---------------------------------

std::vector<Row> g_test_rows;
void TestEmit(const Row& r) { g_test_rows.push_back(r); }

unsigned g_bad = 0;
void Check(bool ok, const char* what, int a = 0, int b = 0) {
    if (ok) return;
    if (++g_bad <= 20) bof3::Log("shadow      band_census self-test MISMATCH: %s (%d, %d)", what, a, b);
}

Sample Make(unsigned frame, float x, float y, int script, unsigned mask) {
    Sample s{};
    s.frame = frame;
    s.recipe = frame + 7;
    s.area = 12;
    s.mode = 3;
    s.chapter = 2;
    s.who = kObj;
    s.index = 4;
    s.type = 1;
    s.sprite = 0x00612340;
    s.left = x;
    s.right = x + 16.0f;
    s.top = y;
    s.bottom = y + 32.0f;
    s.wx = 0x100000;
    s.wz = 0x200000;
    s.wy = 0;
    s.script = script;
    s.mask = mask;
    return s;
}

void SelfTest() {
    g_bad = 0;
    unsigned checks = 0;
    // 1. The band edges: a one-column rectangle at x, 100 rows down, under
    //    the narrow view, 16:9 (53) and 21:9 (120 - passed in, UW's).
    struct Edge { float x; Class at0, at53, at120; };
    static const Edge kEdges[] = {
        {-121, kNotShown, kNotShown, kNotShown}, {-120, kNotShown, kNotShown, kLeft219},
        {-54, kNotShown, kNotShown, kLeft219},   {-53, kNotShown, kLeft169, kLeft169},
        {-1, kNotShown, kLeft169, kLeft169},     {0, kInside, kInside, kInside},
        {319, kInside, kInside, kInside},        {320, kNotShown, kRight169, kRight169},
        {372, kNotShown, kRight169, kRight169},  {373, kNotShown, kNotShown, kRight219},
        {439, kNotShown, kNotShown, kRight219},  {440, kNotShown, kNotShown, kNotShown},
    };
    for (const Edge& e : kEdges) {
        const Class want[3] = {e.at0, e.at53, e.at120};
        const unsigned widths[3] = {0, 53, 120};
        for (int i = 0; i < 3; ++i) {
            const Class got = Classify(e.x, e.x + 1.0f, 100.0f, 101.0f, widths[i]);
            Check(got == want[i], "band edge", static_cast<int>(e.x) * 1000 + static_cast<int>(widths[i]), got);
            ++checks;
        }
    }
    // Rectangles: one that straddles a band's edge counts where it touches;
    // one that touches 0..320 at all is inside; rows above or below the
    // picture are not shown; a NaN is not a rectangle.
    struct Box { float l, r, t, b; unsigned c; Class want; };
    const Box kBoxes[] = {
        {-60, -40, 50, 80, 53, kLeft169},  {-60, -53, 50, 80, 53, kNotShown}, {-60, -53, 50, 80, 120, kLeft219},
        {-10, 5, 50, 80, 53, kInside},     {315, 330, 50, 80, 120, kInside},   {360, 380, 50, 80, 120, kRight169},
        {373, 380, 50, 80, 120, kRight219}, {-30, -10, -40, 0, 53, kNotShown}, {-30, -10, 239, 260, 53, kLeft169},
        {-30, -10, 240, 260, 53, kNotShown}, {std::nanf(""), 0, 0, 10, 120, kNotShown},
    };
    for (const Box& b : kBoxes) {
        const Class got = Classify(b.l, b.r, b.t, b.b, b.c);
        Check(got == b.want, "box", static_cast<int>(b.l), got);
        ++checks;
    }
    // A sweep: every whole x from -200 to 520 at each width 0..130 agrees
    // with the classes' definitions written another way.
    for (unsigned c = 0; c <= 130; ++c)
        for (int x = -200; x <= 520; ++x) {
            const Class got = Classify(static_cast<float>(x), static_cast<float>(x) + 1.0f, 10.0f, 11.0f, c);
            const int ci = static_cast<int>(c);
            Class want;
            if (x < -ci || x >= 320 + ci) want = kNotShown;
            else if (x >= 0 && x < 320) want = kInside;
            else if (x < 0) want = x >= -53 ? kLeft169 : kLeft219;
            else want = x < 373 ? kRight169 : kRight219;
            Check(got == want, "sweep", x, ci);
            ++checks;
        }

    // 2. The rectangle from a stand-in sprite: two pieces, scaled, mirrored.
    {
        unsigned char sprite[kObjectBytes] = {};
        // count 2; piece 0 size index 0 at (-8, -24); piece 1 size index 1 at (4, -8)
        static const unsigned char kFrame[] = {2, 0x00, 0xF8, 0xE8, 0, 0, 0x01, 0x04, 0xF8, 0, 0};
        const std::uint32_t table = Address(kFrame);
        std::memcpy(sprite + 0x54, &table, 4);
        const float px = -40.0f, py = 120.0f;
        std::memcpy(sprite + 0x74, &px, 4);
        std::memcpy(sprite + 0x78, &py, 4);
        auto size = [](unsigned i, int& w, int& h) {
            const unsigned w0 = SpriteCell_Sizes[i * 2], h0 = SpriteCell_Sizes[i * 2 + 1];
            const unsigned packed = (((h0 & 0xF8u) << 1) | ((w0 & 0xFFu) >> 3)) & 0xFFu;
            w = static_cast<int>((packed & 0xFu) << 3);
            h = static_cast<int>((packed >> 1) & 0x78u);
        };
        int w0, h0, w1, h1;
        size(0, w0, h0);
        size(1, w1, h1);
        const int l = -8 < 0 ? -8 : 0, r = (-8 + w0 > 4 + w1 ? -8 + w0 : 4 + w1) > 0 ? (-8 + w0 > 4 + w1 ? -8 + w0 : 4 + w1) : 0;
        const int t = -24, b = (-24 + h0 > -8 + h1 ? -24 + h0 : -8 + h1) > 0 ? (-24 + h0 > -8 + h1 ? -24 + h0 : -8 + h1) : 0;
        const auto about = [](float a, double b) { return std::fabs(static_cast<double>(a) - b) < 1e-3; };
        float L, R, T, B;
        bool ok = Rect(sprite, L, R, T, B);
        Check(ok && about(L, px + l) && about(R, px + r) && about(T, py + t) && about(B, py + b), "rect", static_cast<int>(L),
              static_cast<int>(R));
        sprite[0x2A] = 1;   // mirrored about the point
        ok = Rect(sprite, L, R, T, B);
        Check(ok && about(L, px - r) && about(R, px - l), "rect mirrored", static_cast<int>(L), static_cast<int>(R));
        sprite[0x2A] = 0;
        sprite[0x48] = 1;   // scaled by the depth: 0x4650000 / 2250 = 0x8000, half size
        const long depth = 2250;
        std::memcpy(sprite + 0x60, &depth, 4);
        ok = Rect(sprite, L, R, T, B);
        Check(ok && about(L, px + l * 0.5) && about(R, px + r * 0.5) && about(B, py + b * 0.5), "rect scaled",
              static_cast<int>(L * 100), static_cast<int>(R * 100));
        const long zero = 0;
        std::memcpy(sprite + 0x60, &zero, 4);
        Check(!Rect(sprite, L, R, T, B), "depth 0 draws nothing");
        sprite[0x48] = 0;
        sprite[0] = 0x40;
        Check(!Rect(sprite, L, R, T, B), "flag bit 6 draws nothing");
        checks += 5;
    }

    // 3. The script's state from the op at the position.
    {
        struct Op { unsigned char op, before; int want; };
        static const Op kOps[] = {{0xFF, 0x10, kEnd}, {0x86, 0, kEnd},   {0x00, 0xEF, kEnd},  {0x00, 0xD8, kEnd},
                                  {0x00, 0xE6, kEnd}, {0xFD, 0, kWait},  {0xD9, 0, kWait},    {0xDA, 0, kWait},
                                  {0xE9, 0, kWait},   {0xF9, 0, kWait},  {0xA1, 0, kWait},    {0xAF, 0, kWait},
                                  {0x12, 0, kRun},    {0xF2, 0xFF, kRun}, {0xB0, 0, kRun},    {0xDF, 0, kRun}};
        for (const Op& o : kOps) {
            Check(ScriptAt(o.op, o.before) == o.want, "script op", o.op, o.before);
            ++checks;
        }
    }

    // 4. Rows: the merge, parked, the scene, the span, the gap, a move inside.
    const unsigned saved_columns = g_columns, saved_park = g_park, saved_span = g_span, saved_frame = g_frame;
    const Emit saved_emit = g_emit;
    std::unordered_map<std::uint32_t, Track> saved_tracks;
    saved_tracks.swap(g_tracks);
    const unsigned saved_rows = g_rows;
    g_emit = TestEmit;
    g_columns = 120;
    g_park = 30;
    g_span = 300;
    g_frame = ~0u;
    g_test_rows.clear();
    // Object A: still at -80 (21:9 only, left) from frame 1 to 100, script at
    // its end, the leader's control taken (mask 2) until frame 60 and given
    // back after; a row until it has been still for 30 frames, then a parked
    // row, cut where the scene ends.
    for (unsigned f = 1; f <= 100; ++f) Process(1, Make(f, -80.0f, 60.0f, kEnd, f <= 60 ? 2u : 0u));
    // Then it walks right, out of the 21:9-only columns into the 16:9 band.
    for (unsigned f = 101; f <= 120; ++f) {
        Sample s = Make(f, -80.0f + 3.0f * (f - 100), 60.0f, kRun, 2);
        s.wx += static_cast<std::int32_t>(f);
        Process(1, s);
    }
    // Object B: parked at 400 (21:9 only, right) for 700 frames, no scene:
    // the span cuts it into rows the tool joins again.
    for (unsigned f = 1; f <= 700; ++f) {
        Sample s = Make(f, 400.0f, 100.0f, kWait, 0);
        s.index = 5;
        Process(2, s);
        if (f % 100 == 0) Process(2, s);   // updated twice in a frame: one frame seen
    }
    // Object C: a party member (no script read) still at 330 from 1 to 50,
    // unseen for 5 frames, then back: two rows, and the stillness restarts.
    for (unsigned f = 1; f <= 60; ++f) {
        if (f > 40 && f <= 45) continue;
        Sample s = Make(f, 330.0f, 100.0f, kNone, 1);
        s.who = kParty;
        s.index = 1;
        Process(3, s);
    }
    {
        // At 701 the sweep closes every row last seen before 699 (A's, B's,
        // C's); CloseAll is what the exit does with whatever is still open.
        Process(4, Make(701, 100.0f, 100.0f, kRun, 0));   // inside: no row
        CloseAll();
    }
    struct Want { std::uint32_t who_index; unsigned first, last, seen; int cls, parked; bool scene; };
    // who_index: who * 100 + index
    static const Want kWant[] = {
        {kObj * 100 + 4, 1, 30, 30, kLeft219, kMoving, true},
        {kObj * 100 + 4, 31, 60, 30, kLeft219, kParked, true},
        {kObj * 100 + 4, 61, 100, 40, kLeft219, kParked, false},
        {kObj * 100 + 4, 101, 103, 3, kLeft219, kMoving, true},     // right edge -61, -58, -55: beyond -53
        {kObj * 100 + 4, 104, 120, 17, kLeft169, kMoving, true},    // -52 .. -4: in the 16:9 band
        {kParty * 100 + 1, 1, 30, 30, kRight169, kMoving, true},
        {kParty * 100 + 1, 31, 40, 10, kRight169, kStill, true},
        {kParty * 100 + 1, 46, 60, 15, kRight169, kMoving, true},
        {kObj * 100 + 5, 1, 30, 30, kRight219, kMoving, false},
        {kObj * 100 + 5, 31, 330, 300, kRight219, kParked, false},
        {kObj * 100 + 5, 331, 630, 300, kRight219, kParked, false},
        {kObj * 100 + 5, 631, 700, 70, kRight219, kParked, false},
    };
    constexpr unsigned kWantN = sizeof kWant / sizeof kWant[0];
    Check(g_test_rows.size() == kWantN, "row count", static_cast<int>(g_test_rows.size()), static_cast<int>(kWantN));
    for (const Want& w : kWant) {
        bool found = false;
        for (const Row& r : g_test_rows)
            if (static_cast<std::uint32_t>(r.who * 100 + r.index) == w.who_index && r.first == w.first) {
                found = true;
                Check(r.last == w.last && r.seen == w.seen && r.cls == w.cls && r.parked == w.parked && r.scene == w.scene,
                      "row", static_cast<int>(w.who_index * 1000 + w.first), static_cast<int>(r.last));
                Check(r.rfirst == r.first + 7 && r.rlast == r.last + 7, "row recipe frames", static_cast<int>(r.rfirst),
                      static_cast<int>(r.rlast));
            }
        Check(found, "row missing", static_cast<int>(w.who_index), static_cast<int>(w.first));
        ++checks;
    }
    g_test_rows.clear();
    g_tracks.swap(saved_tracks);
    g_columns = saved_columns;
    g_park = saved_park;
    g_span = saved_span;
    g_frame = saved_frame;
    g_emit = saved_emit;
    g_rows = saved_rows;

    // 5. Off is a no-op: the hook records nothing and touches no state.
    {
        unsigned char sprite[kObjectBytes] = {};
        const bool was = g_on;
        g_on = false;
        Seen(sprite);
        Check(g_tracks.empty() && g_rows == saved_rows, "off records nothing", static_cast<int>(g_tracks.size()));
        g_on = was;
        ++checks;
    }
    bof3::Log("shadow      band_census self-test: %u checks (the band edges at 0, 53 and 120 columns, a sweep of x "
              "-200..520 at 0..130 columns, the drawn rectangle, the script's ops, %u rows merged), %u MISMATCHES",
              checks, kWantN, g_bad);
    if (g_bad) bof3::Fatal("band_census: %u self-test mismatches", g_bad);
}

unsigned EnvUnsigned(const char* name, unsigned fallback, unsigned lo, unsigned hi) {
    char text[16];
    const DWORD n = GetEnvironmentVariableA(name, text, sizeof text);
    if (n == 0) return fallback;
    char* end = nullptr;
    const unsigned long v = n < sizeof text ? std::strtoul(text, &end, 10) : 0;
    if (n >= sizeof text || end == text || *end != '\0' || v < lo || v > hi)
        bof3::Fatal("%s must be %u..%u", name, lo, hi);
    return static_cast<unsigned>(v);
}

void AtExit() {
    if (!g_out) return;
    CloseAll();
    std::fprintf(g_out, "# end: %u rows\n", g_rows);
    std::fclose(g_out);
    g_out = nullptr;
}

}  // namespace

void Inject() {
    if (bof3::WantsShadow("band_census")) SelfTest();
    char path[MAX_PATH];
    const DWORD n = GetEnvironmentVariableA("BOF3X_BANDLOG", path, sizeof path);
    if (n == 0) return;
    if (n >= sizeof path) bof3::Fatal("BOF3X_BANDLOG is longer than %u bytes", static_cast<unsigned>(sizeof path));
    g_park = EnvUnsigned("BOF3X_BANDLOG_PARK", 30, 1, 100000);
    g_span = EnvUnsigned("BOF3X_BANDLOG_SPAN", 60, 1, 1000000);
    if (EnvUnsigned("BOF3X_BANDLOG_VOID", 0, 0, 1) != 0)
        bof3::Fatal("BOF3X_BANDLOG_VOID: the void channel is not built (docs/band-census.md section 5)");
    g_columns = Widescreen_Live();
    g_out = std::fopen(path, "w");
    if (!g_out) bof3::Fatal("BOF3X_BANDLOG: cannot open %s for writing", path);
    std::fprintf(g_out, "# bandlog 1\tcolumns %u\tpark %u\tspan %u\n", g_columns, g_park, g_span);
    std::fprintf(g_out, "# area\tchapter\tmode\twho\tindex\ttype\tsprite\tfirst\tlast\tseen\trfirst\trlast\txlo\txhi\tylo\t"
                        "yhi\tside\tband\tparked\tscene\tmask\tscript\thidden\n");
    std::fflush(g_out);
    std::atexit(AtExit);
    g_on = true;
    bof3::Log("bandlog     BOF3X_BANDLOG on: %s, %u columns a side%s, parked after %u still frames, rows cut at %u frames",
              path, g_columns, g_columns ? "" : " (the view is narrow: nothing lies outside 320 and in the picture)", g_park,
              g_span);
}

}  // namespace band_census
