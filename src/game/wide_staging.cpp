// The wide staging (wide_staging.h; docs/wide-staging.md): DIV-0122, DIV-0123 and DIV-0124
// of the ultrawide round, armed only when the view is wide.
#include "game/wide_staging.h"

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/band_census.h"
#include "game/move_script_bytes.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace wide_staging {

bool g_staging = false;
bool g_hide = false;

namespace {

using move_script::Long;
using move_script::Pos;
using move_script::SetLong;
using move_script::SetPos;
using move_script::SetWord;

std::uint32_t Address(const void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }
const unsigned char* Pointer(std::uint32_t a) { return reinterpret_cast<const unsigned char*>(static_cast<std::uintptr_t>(a)); }

// --- D: the site table -----------------------------------------------------
//
// One row per re-staged walk, keyed like a twin row: the area, the script
// (the descriptor's +0x10 or +0x18 table and the index; the address it holds
// on BOF3.exe, checked), the FNV-1a hash of the script's first `span` bytes
// (and of the placement op's bytes, for a start a placement op sets), and the
// ops by their offset in the script. No script byte is copied: the move's
// direction and steps are read from the script at run time, and the row only
// says how many steps to add, as a rule in the columns.
//
//   kIn         the start moves out: back along the walk's direction by
//               Steps() steps, where the placement op (`place`) or the
//               script's 0D (`at0d`) puts it; the walk at `move` gains those
//               steps in front, so it ends where it did. Only an object whose
//               start was moved has its walk changed.
//   kOut        the walk at `move` - the last before a hide or an end - gains
//               Steps() steps at its end: the point it hides at moves out.
//   kOutBefore  the same, as a walk of Steps() steps in the direction of the
//               move at `move`, walked before the op at `before` (an EB or E6
//               that follows a talk at the walk's end, so the talk stays put).
//
// Steps(c) = ceil((c + kHalfWidth - beyond) * 10 / pps10), capped by `top`
// when it is not 0: `beyond` is how far (px) the site's point already lies
// past the 320 view's edge (negative: inside it), `pps10` the screen px one
// step outward gains, x10, and `top` the steps that take the feet off the
// picture's top or bottom - measured from SS's camera by
// tools/wide_staging_check.py --measure, which also checks every row against
// the executable. 0 columns: 0 steps. `fast`: the added steps are walked at the
// fastest speed (kIn only: they lie out of view), because what follows the
// walk in the actor's own script is a wait for a clock value another script
// steps - the actor must not arrive later than the original's did by more
// than those few frames.
//
// EB-1's live census may add sites (docs/wide-staging.md section 7): a new
// row here, its fields from --measure, and the check run clean.
enum Shape : std::uint8_t { kIn, kOut, kOutBefore };
constexpr std::uint16_t kNone = 0xFFFF;
constexpr int kHalfWidth = 12;   // SS's half sprite (tools/staging_scan.py HALF_W)

struct Site {
    std::uint16_t area;
    std::uint8_t field;      // 0x10 (objects) or 0x18 (the party's scene scripts)
    std::uint8_t index;
    std::uint32_t script;    // what Area_Descriptors[area] + field [index] holds on BOF3.exe
    std::uint32_t hash;      // FNV-1a 32 of the script's first `span` bytes, then the placement op's
    std::uint16_t span;
    std::uint32_t place;     // the placement op that sets the start (kIn), else 0
    std::uint16_t at0d;      // the script's 0D that sets the start (kIn), else kNone
    std::uint16_t move;      // the move op: lengthened (kIn, kOut), or the direction (kOutBefore)
    std::uint16_t before;    // kOutBefore: the op the added walk comes before, else kNone
    Shape shape;
    std::int16_t beyond;
    std::uint16_t pps10;
    std::uint8_t top;
    bool fast;
};

// The 16:9-band rows of SS's table (analysis/ultrawide/staging_static.tsv)
// that the out shape fits, and one PD site SS's table leaves to the census;
// docs/wide-staging.md section 3 lists every one of the 36 rows and why the
// rest are not here. tools/wide_staging_check.py parses this table: one row a
// line, the comment last.
// clang-format off
const Site kSites[] = {
    // area, field, index, script, hash, span, place, at0d, move, before, shape, beyond, pps10, top, fast
    {0, 0x10, 5, 0x5DAAEC, 0x6EEFAD88, 0x0004, 0x005DA1FC, kNone, 0x003, kNone, kIn, 20, 74, 7, false},  // SS spawn pop, sx 340: shown, then walks south into view
    {0, 0x10, 6, 0x5DAB1C, 0xBD4799D4, 0x0008, 0x005DA20D, kNone, 0x007, kNone, kIn, 40, 83, 9, false},  // SS spawn pop, sx 360 (PSP changed: placed a cell farther)
    {19, 0x18, 3, 0x5E6890, 0x4FEBFC09, 0x0109, 0x00000000, 0x103, 0x108, kNone, kIn, 16, 144, 0, false},  // PD: the PSP's start 2 cells farther (SS: needs census, 16:9 if centred)
    {27, 0x10, 2, 0x5ECB30, 0x4997E8F8, 0x000D, 0x00000000, kNone, 0x00C, kNone, kOut, 46, 94, 8, false},  // SS parked, sx -46, then E6 (PSP changed: 2 steps more)
    {32, 0x10, 4, 0x5EEAEC, 0x86FDB743, 0x001A, 0x00000000, 0x010, 0x017, kNone, kIn, 46, 171, 0, false},  // SS spawn pop, sx -46 (PSP changed: placed a cell farther)
    {39, 0x10, 34, 0x5F31EC, 0x3AEC2712, 0x0005, 0x005F2AC8, kNone, 0x004, kNone, kIn, 46, 171, 0, false},  // SS parked, waits then enters, sx -46
    {39, 0x18, 14, 0x5F35F4, 0x41AFD7C7, 0x0114, 0x00000000, 0x10E, 0x113, kNone, kIn, 38, 134, 0, false},  // SS spawn pop, sx -38
    {43, 0x18, 3, 0x5F6680, 0x7C02C57B, 0x0011, 0x00000000, 0x008, 0x00E, kNone, kIn, 14, 168, 0, true},  // SS parked, waits then enters, sx 334
    {50, 0x18, 2, 0x5FC858, 0x7EDD120E, 0x000F, 0x00000000, 0x000, 0x00E, kNone, kIn, 27, 149, 0, true},  // SS entrance seen early, sx 347
    {60, 0x18, 2, 0x603D90, 0xA02E9306, 0x0032, 0x00000000, kNone, 0x02C, 0x031, kOutBefore, 31, 169, 3, false},  // SS parked and despawn pop, sx 351 (PSP changed: 6 steps more before EB)
    {66, 0x10, 1, 0x604A30, 0xBBCC151D, 0x0007, 0x006049BD, kNone, 0x004, kNone, kIn, 45, 173, 0, false},  // SS entrance seen early, sx 365
    {69, 0x10, 1, 0x607F98, 0x07A8CE6F, 0x0022, 0x00000000, kNone, 0x021, kNone, kOut, 12, 74, 27, false},  // SS despawn pop, sx 332
    {69, 0x10, 2, 0x607FBC, 0xC1A09022, 0x0005, 0x00607EEC, kNone, 0x004, kNone, kIn, 44, 73, 23, false},  // SS parked, waits then enters, sx -44
    {94, 0x10, 39, 0x6161EC, 0x6545C873, 0x0012, 0x006158CD, kNone, 0x00F, kNone, kIn, 34, 69, 17, false},  // SS entrance and parked, sx -34 (PSP changed: 2 cells farther)
    {120, 0x18, 15, 0x6234F0, 0x1187C1B3, 0x000B, 0x00000000, 0x000, 0x008, kNone, kIn, 14, 168, 0, false},  // SS entrance and parked, sx 334
    {175, 0x10, 6, 0x641C98, 0x48AD3D90, 0x0022, 0x00000000, kNone, 0x021, kNone, kOut, 33, 79, 10, false},  // SS despawn pop, sx 353
    {175, 0x10, 7, 0x641CBC, 0x327DA213, 0x002C, 0x00000000, kNone, 0x02B, kNone, kOut, 38, 72, 14, false},  // SS despawn pop, sx 358
    {175, 0x10, 7, 0x641CBC, 0xF305F83C, 0x0004, 0x006418BD, kNone, 0x003, kNone, kIn, 17, 71, 17, true},  // SS spawn pop, sx 337
};
// clang-format on
constexpr unsigned kSiteCount = sizeof kSites / sizeof kSites[0];

// --- E: case zero ----------------------------------------------------------
//
// Area 175, chapter 7 (Scena07_Scene2), the party's scene scripts +0x18[4]
// and [5]: the original walks the pair 24 steps west at clock 0x1B and parks
// them at screen x -40 until 0x29, in the 16:9 band. The PSP walks them 21
// west and 6 north, inside the 4:3 view, and on its later route adds 6 south
// before the east walk, which is 3 shorter, so the route ends where it did
// (docs/ultrawide-staging-psp.md section 2, rows 53 and 54; PD's opdiff).
// Written here as a change of the original's steps: the walk keeps its
// direction and loses 3 steps, and a walk of 6 steps north (direction 1)
// follows it, or one of 6 south (direction 5) comes before it.
struct CaseZeroScript {
    std::uint8_t index;
    std::uint32_t script;
    std::uint32_t hash;   // FNV-1a 32 of the first `span` bytes
    std::uint16_t span;
};
struct CaseZeroOp {
    std::uint8_t index;
    std::uint16_t at;     // an F2 walk in the script
    std::int8_t delta;    // its steps change by this
    std::uint8_t dir;     // the walk added: 1 north after a west walk, 5 south before the east walk
    bool after;           // the added walk follows the F2 (else it comes before)
};
// The added walk's steps, chosen at run time (CaseZeroSplit) so that:
//  - the rewritten op takes exactly the original's frames (the round's rule:
//    game timing stays Capcom's; EB-2's hash, ultrawide-census-3.md 3.2, found
//    the PSP's 21 + 6 moving dolphinBoss's recorded route): the 3 steps the
//    walk loses, 3 * f frames at the actor's own f = 0x10 / speed frames a
//    step, are spent on the added walk - `a` steps at the actor's speed and
//    `b` at the fastest (one frame a step), a * f + b = 3 * f;
//  - north, the pair end wholly above the picture's top row (census 3's
//    sliver: the PSP crops 12 rows, we keep 240): the steps needed are the
//    drawn rectangle's bottom row, read when the north walk starts
//    (band_census::ScreenRect, the same reading the census and the hide use),
//    over kCaseZeroRise10 / 10 px a step;
//  - south, the east walk's added steps are the last north walk's of the same
//    script, so the route ends where the original's does.
// a is taken as large as the north steps needed allow (the visible part at
// the actor's own pace), the rest fast, out of view.
constexpr unsigned kCaseZeroRise10 = 27;   // screen px a north step lifts the pair near the top, x10 (SS's camera,
                                           // the smallest over 20 steps from the wait point; wide_staging_check.py)
constexpr std::uint16_t kCaseZeroArea = 175;
// clang-format off
const CaseZeroScript kCaseZeroScripts[] = {
    {4, 0x641FEC, 0x6354AB60, 0x008E},
    {5, 0x6420A4, 0x32775E7B, 0x00A0},
};
const CaseZeroOp kCaseZero[] = {
    {4, 0x060, -3, 1, true},    // the first west walk (clock 0x1B): 21 west, then north off the top
    {4, 0x082, -3, 1, true},    // the second (0x29), the same
    {4, 0x08B, -3, 5, false},   // the east walk (0x33): south as far as the last north walk first, then 3 fewer east
    {5, 0x074, -3, 1, true},
    {5, 0x094, -3, 1, true},
    {5, 0x09D, -3, 5, false},
};
// clang-format on

// --- F: the hide's exceptions ----------------------------------------------
//
// Objects the hide must leave drawn, keyed like the sites: the area, 0x10 and
// the object's script index (+0x83), or 0 and the party member's slot. Empty
// to start (the brief); a row is added when the census or the owner finds an
// object the original's scene shows from beyond the 16:9 edge on purpose.
struct HideException {
    std::uint16_t area;
    std::uint8_t field;
    std::uint8_t index;
};
constexpr int kAttractChapter = 16;     // Cond_ByteFA of the attract demo (FieldScene below)
constexpr unsigned kNoWorldMap = 11;    // WorldMap_RecordIndex's "none of the eleven"
const HideException kHideExceptions[] = {
    {0xFFFF, 0, 0},   // the end of the list
};

// --- the rule ----------------------------------------------------------------

unsigned g_columns = 0;   // Widescreen_Live() at the arm

unsigned StepsFor(int beyond, unsigned pps10, unsigned top, unsigned c) {
    if (c == 0) return 0;
    const int need = static_cast<int>(c) + kHalfWidth - beyond;
    unsigned h = need <= 0 ? 0u : (static_cast<unsigned>(need) * 10u + pps10 - 1u) / pps10;
    if (top != 0 && top < h) h = top;
    return h;
}
unsigned Steps(const Site& s) { return StepsFor(s.beyond, s.pps10, s.top, g_columns); }

std::uint32_t Fnv(const unsigned char* p, unsigned n, std::uint32_t h = 0x811C9DC5u) {
    for (unsigned i = 0; i < n; ++i) h = (h ^ p[i]) * 0x01000193u;
    return h;
}

// A move op's direction and steps: 10..4F (one byte) or F2 d n.
bool MoveAt(const unsigned char* p, unsigned& dir, unsigned& steps, unsigned& length) {
    if (p[0] >= 0x10 && p[0] <= 0x4F) {
        dir = static_cast<unsigned>(p[0] - 0x10) >> 3;
        steps = p[0] & 7u;
        length = 1;
        return true;
    }
    if (p[0] == 0xF2) {
        dir = p[1];
        steps = p[2];
        length = 3;
        return true;
    }
    return false;
}

const unsigned char* ScriptOf(unsigned area, unsigned field, unsigned index) {
    const unsigned char* const descriptor = Area_Descriptors[area];
    std::uint32_t table;
    std::memcpy(&table, descriptor + field, sizeof table);
    std::uint32_t script;
    std::memcpy(&script, Pointer(table) + 4u * index, sizeof script);
    return Pointer(script);
}

// What armed: the rows whose bytes matched. Indexed like kSites / kCaseZero.
bool g_site_on[kSiteCount];
bool g_case_on[sizeof kCaseZero / sizeof kCaseZero[0]];
bool g_site_logged[kSiteCount];
bool g_case_logged[sizeof kCaseZero / sizeof kCaseZero[0]];
bool g_log_all = false;
bool g_quiet = false;     // the self-test runs the engine: nothing logged, nothing marked as logged
// The callees, through pointers so the self-test can stand recorders in.
void (__cdecl* g_move)(unsigned char*, unsigned char) = MoveCmd_Move;
int (__cdecl* g_kind)() = MoveScript_ObjectKind;
unsigned char g_fast_index = 0;   // Field_MoveSpeeds' fastest entry whose speed divides 0x10

// One step of direction `dir` at the context's speed, as MoveCmd_Move and
// Field_ObjectMotion take it: the velocity F9Steps * speed for 0x10 / speed
// frames (0x20 for directions 2 and 6) - half a cell on each axis that moves.
bool StepDisplacement(const unsigned char* context, unsigned dir, std::int32_t& dx, std::int32_t& dz) {
    const unsigned speed = Field_MoveSpeeds[context[4]];
    if (speed == 0) return false;
    const unsigned base = (dir == 2 || dir == 6) ? 0x20u : 0x10u;
    const std::int32_t frames = static_cast<std::int32_t>(base / speed);
    dx = static_cast<std::int32_t>(MoveCmd_F9Steps[dir * 2] * static_cast<long>(speed)) * frames;
    dz = static_cast<std::int32_t>(MoveCmd_F9Steps[dir * 2 + 1] * static_cast<long>(speed)) * frames;
    return true;
}

// --- the live state: one entry an object, while a rewritten walk runs ------

struct Element {
    unsigned char dir, steps;
    bool fast;
};
struct Plan {
    Element e[3];
    unsigned count;
    bool advance;          // the last element steps past the op; else the op runs after them
    unsigned length;       // the op's length
    int site, cz;          // which row (-1 none)
};

struct Live {
    unsigned char* context = nullptr;
    const unsigned char* script = nullptr;
    unsigned pos = 0, area = 0;
    bool marked = false;       // kIn: the start was moved; the walk at the site waits for it
    int site = -1;             // the kIn row the mark is for
    bool running = false;      // a plan in progress
    Plan plan{};
    unsigned next = 0;         // the next element
    std::int32_t x = 0, z = 0; // the sprite where the last element began
    bool restore = false;
    unsigned char speed = 0;   // the context's speed index before a fast element
    bool pending = false;      // the plan ended on a fast element: its speed goes back at the next step
    unsigned frame = 0;
};
constexpr unsigned kLive = 32;
Live g_live[kLive];

Live* Find(const unsigned char* context) {
    for (Live& l : g_live)
        if (l.context == context) return &l;
    return nullptr;
}
Live* Take(unsigned char* context) {
    if (Live* l = Find(context)) return l;
    Live* oldest = &g_live[0];
    for (Live& l : g_live) {
        if (!l.context) {
            l = Live{};
            l.context = context;
            return &l;
        }
        if (l.frame < oldest->frame) oldest = &l;
    }
    bof3::Log("DIV-0122    the live table is full: an entry from frame %u (context %08X) dropped", oldest->frame,
              Address(oldest->context));
    *oldest = Live{};
    oldest->context = context;
    return oldest;
}
void Drop(Live* l) { *l = Live{}; }

void Note(int site, int cz, unsigned steps) {
    if (g_quiet) return;
    if (site >= 0) {
        if (g_site_logged[site] && !g_log_all) return;
        g_site_logged[site] = true;
        const Site& s = kSites[site];
        bof3::Log("DIV-0122    site %d (area %u +0x%02X[%u] +0x%03X): %u steps at %u columns, frame %u", site, s.area,
                  s.field, s.index, s.move, steps, g_columns, Frame_Counter);
    } else if (cz >= 0) {
        if (g_case_logged[cz] && !g_log_all) return;
        g_case_logged[cz] = true;
        bof3::Log("DIV-0123    case zero, +0x18[%u] +0x%03X: the PSP's staging, frame %u", kCaseZero[cz].index,
                  kCaseZero[cz].at, Frame_Counter);
    }
}

// Case zero's added walk: the 3 lost steps' frames, `lost * f` with f the
// actor's frames a step, spent as `a` steps at its speed and `b` at the
// fastest whole speed (g_fast_index's, `ff` frames a step): a * f + b * ff =
// lost * f, with a + b >= need where it can be (else the most there is,
// a = 0). False when the actor's speed does not divide 0x10 (no whole
// frames): the walk is then the original's, logged.
constexpr unsigned kCaseZeroFallback = 6;   // the PSP's 6 when the rectangle cannot be read
unsigned g_cz_a[2], g_cz_b[2];
bool g_cz_have[2];
bool CaseZeroSplit(unsigned speed, unsigned lost, unsigned need, unsigned& a, unsigned& b) {
    const unsigned fast = g_fast_index ? Field_MoveSpeeds[g_fast_index] : 0;
    if (speed == 0 || 0x10u % speed != 0 || fast == 0 || 0x10u % fast != 0) {
        bof3::Log("DIV-0123    case zero: speed %u (fast %u) gives no whole frames; the walk is the original's", speed, fast);
        return false;
    }
    const unsigned f = 0x10u / speed, ff = 0x10u / fast;
    for (unsigned k = lost + 1; k-- > 0;) {   // a = lost .. 0
        if (((lost - k) * f) % ff != 0) continue;
        const unsigned bb = (lost - k) * f / ff;
        if (k + bb >= need || k == 0) {
            a = k;
            b = bb;
            return true;
        }
    }
    return false;
}

// The plan for the op at the context's position, or false.
bool PlanFor(const Live* l, const unsigned char* context, const unsigned char* script, unsigned pos, Plan& plan) {
    const unsigned area = Game_AreaNumber;
    const unsigned char* const p = script + pos;
    for (unsigned i = 0; i < kSiteCount; ++i) {
        if (!g_site_on[i]) continue;
        const Site& s = kSites[i];
        if (s.area != area || Pointer(s.script) != script) continue;
        const unsigned at = s.shape == kOutBefore ? s.before : s.move;
        if (at != pos) continue;
        if (s.shape == kIn && !(l && l->marked && l->site == static_cast<int>(i))) continue;
        unsigned dir, steps, length;
        if (!MoveAt(script + s.move, dir, steps, length)) return false;
        const unsigned add = Steps(s);
        if (add == 0) return false;
        plan = Plan{};
        plan.site = static_cast<int>(i);
        plan.cz = -1;
        plan.length = length;
        if (s.shape == kIn) {
            plan.e[0] = {static_cast<unsigned char>(dir), static_cast<unsigned char>(add), s.fast};
            plan.e[1] = {static_cast<unsigned char>(dir), static_cast<unsigned char>(steps), false};
            plan.count = 2;
            plan.advance = true;
        } else if (s.shape == kOut) {
            plan.e[0] = {static_cast<unsigned char>(dir), static_cast<unsigned char>(steps), false};
            plan.e[1] = {static_cast<unsigned char>(dir), static_cast<unsigned char>(add), false};
            plan.count = 2;
            plan.advance = true;
        } else {
            plan.e[0] = {static_cast<unsigned char>(dir), static_cast<unsigned char>(add), false};
            plan.count = 1;
            plan.advance = false;
        }
        (void)context;
        return true;
    }
    if (area == kCaseZeroArea) {
        for (unsigned i = 0; i < sizeof kCaseZero / sizeof kCaseZero[0]; ++i) {
            if (!g_case_on[i]) continue;
            const CaseZeroOp& c = kCaseZero[i];
            if (c.at != pos) continue;
            const unsigned char* own = nullptr;
            for (const CaseZeroScript& cs : kCaseZeroScripts)
                if (cs.index == c.index) own = Pointer(cs.script);
            if (own != script) continue;
            unsigned dir, steps, length;
            if (!MoveAt(p, dir, steps, length)) return false;
            const unsigned slot = c.index == kCaseZeroScripts[0].index ? 0u : 1u;
            unsigned a = 0, b = 0;
            if (c.after) {
                float left, right, top, bottom;
                const bool seen = band_census::ScreenRect(Sprite_Current, left, right, top, bottom);
                const unsigned need = !seen ? kCaseZeroFallback
                                    : bottom <= 0.0f ? 0u
                                    : static_cast<unsigned>((bottom * 10.0f + kCaseZeroRise10 - 1) / kCaseZeroRise10);
                if (!CaseZeroSplit(Field_MoveSpeeds[context[4]], static_cast<unsigned>(-c.delta), need, a, b)) return false;
                g_cz_a[slot] = a;
                g_cz_b[slot] = b;
                g_cz_have[slot] = true;
            } else {
                if (!g_cz_have[slot]) return false;   // no north walk to undo: the original's east walk
                a = g_cz_a[slot];
                b = g_cz_b[slot];
                g_cz_have[slot] = false;
            }
            plan = Plan{};
            plan.site = -1;
            plan.cz = static_cast<int>(i);
            plan.length = length;
            const Element walk = {static_cast<unsigned char>(dir), static_cast<unsigned char>(static_cast<int>(steps) + c.delta), false};
            const Element paced = {c.dir, static_cast<unsigned char>(a), false};
            const Element fast = {c.dir, static_cast<unsigned char>(b), true};
            // North: the walk, the paced steps, the fast ones (out of view last). South: the fast ones
            // first (out of view), the paced ones, the walk. Elements of no steps are left out: one would
            // cost the frame the step takes to come back.
            const Element order[3] = {c.after ? walk : fast, paced, c.after ? fast : walk};
            plan.count = 0;
            for (const Element& e : order)
                if (e.steps != 0) plan.e[plan.count++] = e;
            plan.advance = true;
            return true;
        }
    }
    return false;
}

}  // namespace

// --- D and E: the hooks ------------------------------------------------------

bool StepSite(unsigned char* context, const unsigned char* script, unsigned char& result) {
    const unsigned pos = Pos(context);
    Live* l = Find(context);
    if (l && l->pending) {
        // A plan that ended on fast steps: they have run, the speed goes back before the next op.
        if (l->area == Game_AreaNumber && context[4] == g_fast_index) context[4] = l->speed;
        Drop(l);
        l = nullptr;
    }
    if (l && l->running && (l->script != script || l->pos != pos || l->area != Game_AreaNumber)) {
        // The context moved on without us (a scene cut short, an area change): forget the plan.
        // Put the speed back only while it is still the fast one this entry set, in the same area: a context
        // placed afresh since (an area change) has its own.
        if (l->restore && l->area == Game_AreaNumber && context[4] == g_fast_index) context[4] = l->speed;
        Drop(l);
        l = nullptr;
    }
    if (l && l->running) {
        // Back at the op: the element issued last has run - unless its first
        // step was blocked and undone (Field_ObjectUpdate puts the position
        // back and runs the op again): the sprite has not moved from where it
        // began, so that element is issued again.
        const Element& last = l->plan.e[l->next - 1];
        if (last.steps != 0 && Long(Sprite_Current + 0x34) == l->x && Long(Sprite_Current + 0x38) == l->z) --l->next;
        if (l->restore) {
            context[4] = l->speed;
            l->restore = false;
        }
    } else {
        Plan plan;
        if (!PlanFor(l, context, script, pos, plan)) return false;
        if (g_kind() == 2) return false;   // the kind-2 object's path: never a site
        l = Take(context);
        l->marked = false;   // a kIn mark is spent by the walk it was for
        l->running = true;
        l->plan = plan;
        l->next = 0;
        l->script = script;
        l->pos = pos;
        l->area = Game_AreaNumber;
        Note(plan.site, plan.cz, plan.site >= 0 ? plan.e[kSites[plan.site].shape == kOut ? 1 : 0].steps : 0);
    }
    l->frame = Frame_Counter;
    if (l->next >= l->plan.count) {   // a walk before an op: done, the op runs as the original's
        Drop(l);
        return false;
    }
    const Element e = l->plan.e[l->next];
    ++l->next;
    l->x = Long(Sprite_Current + 0x34);
    l->z = Long(Sprite_Current + 0x38);
    if (e.fast && g_fast_index != 0) {
        l->speed = context[4];
        l->restore = true;
        context[4] = g_fast_index;
    }
    // As MoveScript_Step's own move ops: the direction is the step's result,
    // the steps go to +7, and a move with steps starts the timed move.
    result = e.dir;
    context[7] = e.steps;
    if (e.steps != 0) g_move(context, e.dir);
    if (l->next == l->plan.count && l->plan.advance) {
        SetPos(context, l->pos + l->plan.length);   // as the op would have left it (the step's own +1 included)
        if (e.fast && l->restore) {
            const unsigned char speed = l->speed;
            const unsigned area = l->area;
            Drop(l);
            l = Take(context);
            l->pending = true;
            l->speed = speed;
            l->area = area;
            l->frame = Frame_Counter;
        } else {
            Drop(l);
        }
    }
    return true;
}

void Placed0DSite(unsigned char* context, const unsigned char* script, unsigned at) {
    const unsigned area = Game_AreaNumber;
    for (unsigned i = 0; i < kSiteCount; ++i) {
        if (!g_site_on[i]) continue;
        const Site& s = kSites[i];
        if (s.shape != kIn || s.at0d != at || s.area != area || Pointer(s.script) != script) continue;
        unsigned dir, steps, length;
        if (!MoveAt(script + s.move, dir, steps, length)) return;
        std::int32_t dx, dz;
        if (!StepDisplacement(context, dir & 7u, dx, dz)) return;
        const auto k = static_cast<std::int32_t>(Steps(s));
        unsigned char* const sprite = Sprite_Current;
        SetLong(sprite + 0x34, Long(sprite + 0x34) - k * dx);
        SetLong(sprite + 0x38, Long(sprite + 0x38) - k * dz);
        Live* l = Take(context);
        if (l->restore && l->area == area && context[4] == g_fast_index) context[4] = l->speed;
        *l = Live{};
        l->context = context;
        l->marked = k != 0;
        l->site = static_cast<int>(i);
        l->frame = Frame_Counter;
        return;
    }
}

void PlacedSite(const unsigned char* op) {
    const std::uint32_t a = Address(op);
    const unsigned area = Game_AreaNumber;
    for (unsigned i = 0; i < kSiteCount; ++i) {
        if (!g_site_on[i]) continue;
        const Site& s = kSites[i];
        if (s.shape != kIn || s.place != a || s.area != area) continue;
        unsigned char* const sprite = Sprite_Current;
        const std::uint32_t at = Address(sprite), objects = Address(Sprite_Objects);
        // The handler leaves its object current, its home (+0x8C / +0x90) where it put it.
        if (at < objects || at >= objects + 30u * 0xA4u || (at - objects) % 0xA4u != 0 || sprite[0x83] != s.index ||
            Long(sprite + 0x8C) != Long(sprite + 0x34) || Long(sprite + 0x90) != Long(sprite + 0x38)) {
            bof3::Log("DIV-0122    site %u: the placement op at %08X did not leave its object current (%08X); not moved", i,
                      a, at);
            return;
        }
        const unsigned char* const script = ScriptOf(area, s.field, s.index);
        unsigned dir, steps, length;
        if (!MoveAt(script + s.move, dir, steps, length)) return;
        unsigned char* const context = sprite + 0x80;
        std::int32_t dx, dz;
        if (!StepDisplacement(context, dir & 7u, dx, dz)) return;
        const auto k = static_cast<std::int32_t>(Steps(s));
        SetLong(sprite + 0x34, Long(sprite + 0x34) - k * dx);
        SetLong(sprite + 0x38, Long(sprite + 0x38) - k * dz);
        SetWord(sprite + 0x3E, static_cast<unsigned>(AreaMap_Elevation(Long(sprite + 0x34), Long(sprite + 0x38))));
        Live* l = Take(context);
        // A placement sets the object afresh: its speed is the op's, never one a stale entry would put back.
        *l = Live{};
        l->context = context;
        l->marked = k != 0;
        l->site = static_cast<int>(i);
        l->frame = Frame_Counter;
        return;
    }
}

// --- F: the hide -------------------------------------------------------------

// A field scene, as the game itself tells the three apart (EB-1's census,
// docs/ultrawide-census-1.md 2.2: the scene mask is also set on the world map,
// 0C, and through the attract demo, 02):
//   - Game_Mode 2 is the field: the top-level mode GameMode_Field 0x4959F0
//     runs as (symbols.toml Game_Mode; 3 menu, 5 battle); and Game_Mode 4
//     is the field with a message open: GameMode_Field sets it on
//     Field_Request 2, and GameMode4_Run 0x495E60 (ours, mode_rest.cpp)
//     runs Field_FrameScripted every frame - the scripted field frame - until
//     the message cells' bit 1 (0x7DEE44) puts Game_Mode back to 2. Scenes
//     play through it (EB-2's census 3, ultrawide-census-3.md 4.1: 175 object
//     rows of areas 39 and 41's long scenes drawn in mode 4);
//   - the world map is an area WorldMap_RecordIndex 0x462A90 finds a record
//     for (0..10; 11 is "no world map", which 0x56DE30 dispatches its hooks
//     on - symbols.toml WorldMap_FieldHooks): areas 16, 33, 45, 65, 87, 88,
//     104, 115, 121, 151 and 152 (WorldMap_Records' byte +0x18, read from
//     the exe 2026-10-13);
//   - the attract demo is scenario chapter 16 (Cond_ByteFA 16): the title
//     starts it with Scenario_Start(0x10) (Title_StateStartDemo 0x462300,
//     docs/field-modes.md: "chapter 16 is the attract demo").
bool FieldScene(unsigned game_mode, int chapter, unsigned world_map) {
    return (game_mode == 2 || game_mode == 4) && chapter != kAttractChapter && world_map == kNoWorldMap;
}

bool HideDecision(float left, float right, unsigned scene_mask, bool field_scene, bool candidate, bool excepted) {
    if (!candidate || excepted || !field_scene) return false;
    if ((scene_mask & band_census::kSceneMaskBits) == 0) return false;
    // Wholly beyond the 16:9 view: the 16:9 picture's columns, by the rule
    // that gives them (53), not the live width's.
    const float c169 = static_cast<float>(widescreen_rule::ColumnsForAspect(16, 9));
    return right <= -c169 || left >= 320.0f + c169;
}

namespace {

// A field object, an extra object or a party member other than the leader
// (ObjTrio's first member, whose state the scene mask's 0x10 reads); and the
// exception list's key for it.
bool Candidate(const unsigned char* sprite, unsigned& field, unsigned& index) {
    const std::uint32_t a = Address(sprite);
    const std::uint32_t objects = Address(Sprite_Objects), extra = Address(Sprite_ObjectsExtra), trio = Address(ObjTrio);
    if (a >= objects && a < objects + 30u * 0xA4u && (a - objects) % 0xA4u == 0) {
        field = 0x10;
        index = sprite[0x83];
        return true;
    }
    if (a >= extra && a < extra + 4u * 0xA4u && (a - extra) % 0xA4u == 0) {
        field = 0x10;
        index = 0x100;   // no script of its own: no exception can name it
        return true;
    }
    if (a > trio && a < trio + 3u * 0x14Cu && (a - trio) % 0x14Cu == 0) {
        field = 0;
        index = (a - trio) / 0x14Cu;
        return true;
    }
    return false;
}

bool Excepted(unsigned area, unsigned field, unsigned index) {
    for (const HideException& e : kHideExceptions) {
        if (e.area == 0xFFFF) break;
        if (e.area == area && e.field == field && e.index == index) return true;
    }
    return false;
}

bool Decide(const unsigned char* sprite) {
    unsigned field = 0, index = 0;
    if (!Candidate(sprite, field, index)) return false;
    const unsigned mask = band_census::SceneMask();
    if ((mask & band_census::kSceneMaskBits) == 0) return false;
    if (!FieldScene(Game_Mode, Cond_ByteFA, WorldMap_RecordIndex() & 0xFFu)) return false;   // each caller takes the low byte
    float left, right, top, bottom;
    if (!band_census::ScreenRect(sprite, left, right, top, bottom)) return false;
    return HideDecision(left, right, mask, true, true, Excepted(Game_AreaNumber, field, index));
}

}  // namespace

bool HideSite(const unsigned char* sprite) { return Decide(sprite); }

bool WouldHide(const unsigned char* sprite) { return g_hide && Decide(sprite); }

// --- the self-test and the arm ---------------------------------------------

namespace {

unsigned g_bad = 0, g_checks = 0;
void Check(bool ok, const char* what, int a = 0, int b = 0) {
    ++g_checks;
    if (ok) return;
    if (++g_bad <= 20) bof3::Log("shadow      wide_staging self-test MISMATCH: %s (%d, %d)", what, a, b);
}

unsigned EnvUnsigned(const char* name, unsigned fallback, unsigned lo, unsigned hi) {
    char text[32];
    const DWORD n = GetEnvironmentVariableA(name, text, sizeof text);
    if (n == 0) return fallback;
    if (n >= sizeof text) bof3::Fatal("%s: too long", name);
    char* end = nullptr;
    const unsigned long v = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || v < lo || v > hi) bof3::Fatal("%s=%s: %u..%u", name, text, lo, hi);
    return static_cast<unsigned>(v);
}

// 6. The rewrite engine on the real rows and the real scripts, with a
//    stand-in script context and sprite, Game_AreaNumber set to the row's
//    area, and recorders standing in for MoveCmd_Move and MoveScript_ObjectKind
//    (put back after): what each Step issues, where it leaves the position,
//    the start a 0D moves, the fast speed and its restore, a blocked first
//    step issued again, a kIn walk without its moved start left alone, and no
//    steps at 0 columns.
struct MoveCall {
    unsigned char* context;
    unsigned dir, steps;
};
MoveCall g_calls[8];
unsigned g_call_count = 0;
int g_test_kind = 1;
void __cdecl RecordMove(unsigned char* context, unsigned char dir) {
    if (g_call_count < 8) g_calls[g_call_count] = {context, dir, context[7]};
    ++g_call_count;
}
int __cdecl RecordKind() { return g_test_kind; }

int RowOf(unsigned area, unsigned field, unsigned index, Shape shape) {
    for (unsigned i = 0; i < kSiteCount; ++i)
        if (kSites[i].area == area && kSites[i].field == field && kSites[i].index == index && kSites[i].shape == shape)
            return static_cast<int>(i);
    return -1;
}

void EngineTest() {
    const unsigned saved_area = Game_AreaNumber;
    unsigned char* const saved_sprite = Sprite_Current;
    auto* const saved_move = g_move;
    auto* const saved_kind = g_kind;
    const unsigned saved_columns = g_columns;
    const unsigned char saved_fast = g_fast_index;
    g_move = RecordMove;
    g_kind = RecordKind;
    g_test_kind = 1;
    g_fast_index = 5;
    g_quiet = true;
    for (bool& on : g_site_on) on = true;
    for (bool& on : g_case_on) on = true;
    alignas(4) static unsigned char sprite[0xA4];
    alignas(4) static unsigned char context[0x20];
    Sprite_Current = sprite;
    auto reset = [&](unsigned pos, unsigned speed_index) {
        std::memset(sprite, 0, sizeof sprite);
        std::memset(context, 0, sizeof context);
        SetPos(context, pos);
        context[4] = static_cast<unsigned char>(speed_index);
        g_call_count = 0;
        for (Live& l : g_live) l = Live{};
    };
    auto walk = [&](std::int32_t dx) { SetLong(sprite + 0x34, Long(sprite + 0x34) + dx); };
    unsigned char result = 0;
    g_columns = 53;

    // kOut: area 27 +0x10[2], the 7-step walk west then Steps() more, the position past the op after the second.
    {
        const int row = RowOf(27, 0x10, 2, kOut);
        const Site& s = kSites[row];
        const unsigned char* const script = Pointer(s.script);
        const unsigned add = Steps(s);
        Game_AreaNumber = static_cast<unsigned short>(s.area);
        reset(s.move, 2);
        Check(StepSite(context, script, result) && result == 7 && context[7] == 7 && Pos(context) == s.move,
              "kOut: the original's walk first", static_cast<int>(context[7]), static_cast<int>(Pos(context)));
        Check(StepSite(context, script, result) && context[7] == 7 && g_call_count == 2,
              "kOut: a blocked first step (the sprite unmoved) is issued again", static_cast<int>(context[7]));
        walk(-0x38000);
        Check(StepSite(context, script, result) && result == 7 && context[7] == add && Pos(context) == s.move + 1u,
              "kOut: then the added steps, and the position past the op", static_cast<int>(context[7]), static_cast<int>(add));
        Check(g_call_count == 3 && g_calls[2].context == context && g_calls[2].dir == 7 && add >= 1, "kOut: three moves");
        Check(!StepSite(context, script, result), "kOut: the next op is the original's");
        // 0 columns: nothing.
        g_columns = 0;
        reset(s.move, 2);
        Check(!StepSite(context, script, result) && g_call_count == 0, "0 columns: no rewrite");
        g_columns = 53;
        // The kind-2 object's path is never a site.
        g_test_kind = 2;
        reset(s.move, 2);
        Check(!StepSite(context, script, result) && g_call_count == 0, "kind 2: no rewrite");
        g_test_kind = 1;
    }
    // kIn by a 0D: area 32 +0x10[4]. Without the moved start the walk is the original's; the 0D moves it back
    // along the walk (direction 1, z - half a cell a step: so z + half a cell a step), then the walk gains the steps.
    {
        const int row = RowOf(32, 0x10, 4, kIn);
        const Site& s = kSites[row];
        const unsigned char* const script = Pointer(s.script);
        const unsigned add = Steps(s);
        Game_AreaNumber = static_cast<unsigned short>(s.area);
        reset(s.move, 1);
        Check(!StepSite(context, script, result) && g_call_count == 0, "kIn: no moved start, no rewrite");
        reset(s.at0d, 1);
        SetLong(sprite + 0x38, 0x1C0000);
        Placed0DSite(context, script, s.at0d);
        Check(Long(sprite + 0x38) == 0x1C0000 + static_cast<std::int32_t>(add) * 0x8000 && Long(sprite + 0x34) == 0,
              "kIn: the 0D's point moved back by the steps", Long(sprite + 0x38), static_cast<int>(add));
        SetPos(context, s.move);
        Check(StepSite(context, script, result) && result == 1 && context[7] == add && Pos(context) == s.move,
              "kIn: the added steps first", static_cast<int>(context[7]));
        SetLong(sprite + 0x38, 0x1C0000);
        Check(StepSite(context, script, result) && context[7] == 14 && Pos(context) == s.move + 3u,
              "kIn: then the original's 14, past the F2", static_cast<int>(context[7]), static_cast<int>(Pos(context)));
        Check(context[4] == 1, "kIn: the speed untouched (not a fast row)");
        SetPos(context, s.move);
        Check(!StepSite(context, script, result), "kIn: the mark is spent");
    }
    // A fast kIn row: area 43 +0x18[3]. The added steps at the fastest speed, the speed put back for the walk.
    {
        const int row = RowOf(43, 0x18, 3, kIn);
        const Site& s = kSites[row];
        const unsigned char* const script = Pointer(s.script);
        Game_AreaNumber = static_cast<unsigned short>(s.area);
        reset(s.at0d, 2);
        Placed0DSite(context, script, s.at0d);
        SetPos(context, s.move);
        Check(s.fast && StepSite(context, script, result) && context[4] == 5, "fast: the added steps at speed index 5",
              static_cast<int>(context[4]));
        walk(-0x10000);
        Check(StepSite(context, script, result) && context[4] == 2 && context[7] == 8 && Pos(context) == s.move + 3u,
              "fast: the speed put back for the original's 8", static_cast<int>(context[4]), static_cast<int>(context[7]));
    }
    // kOutBefore: area 60 +0x18[2], a walk north before the EB, then the EB runs as the original's.
    {
        const int row = RowOf(60, 0x18, 2, kOutBefore);
        const Site& s = kSites[row];
        const unsigned char* const script = Pointer(s.script);
        Game_AreaNumber = static_cast<unsigned short>(s.area);
        reset(s.before, 2);
        Check(StepSite(context, script, result) && result == 1 && context[7] == Steps(s) && Pos(context) == s.before,
              "kOutBefore: the walk, the position kept", static_cast<int>(context[7]));
        walk(0x8000);
        Check(!StepSite(context, script, result) && Pos(context) == s.before, "kOutBefore: then the EB, the original's");
    }
    // Case zero (DIV-0123): every rewritten walk takes exactly the original's frames (each element's steps times
    // 0x10 / its speed - MoveCmd_Move's frames a step, Field_ObjectUpdate's next step the frame after), at every
    // speed index 1..5 and with the pair's feet anywhere from row -10 to row 120 when the north walk starts; the
    // north walk ends with the drawn rectangle's bottom at or above row 0 whenever the frames allow it; the east
    // walk's south steps are the north walk's; the speed is the actor's again before its next op.
    {
        Game_AreaNumber = kCaseZeroArea;
        const unsigned char* const script = Pointer(kCaseZeroScripts[0].script);
        auto SetY = [&](float v) { std::memcpy(sprite + 0x78, &v, sizeof v); };
        auto GetY = [&]() { float v; std::memcpy(&v, sprite + 0x78, sizeof v); return v; };
        struct Run { unsigned frames, north, south, east, west; bool fast_seen; };
        auto run = [&](unsigned at, unsigned speed_index, float feet) {
            Run r{};
            reset(at, speed_index);
            SetY(feet - 1.0f);   // no frame table: the rectangle is the point, its bottom the point + 1
            for (int guard = 0; guard < 8; ++guard) {
                const unsigned before = Pos(context);
                if (!StepSite(context, script, result)) break;
                const unsigned speed = Field_MoveSpeeds[context[4]];
                if (context[4] == g_fast_index) r.fast_seen = true;
                r.frames += context[7] * (0x10u / speed);
                (result == 1 ? r.north : result == 5 ? r.south : result == 3 ? r.east : r.west) += context[7];
                // the walk's motion: the sprite moves (half a cell a step, a lift of kCaseZeroRise10 / 10 px north)
                walk(static_cast<std::int32_t>(context[7]) * 0x8000);
                if (result == 1) SetY(GetY() - context[7] * (kCaseZeroRise10 / 10.0f));
                if (Pos(context) != before) break;
            }
            return r;
        };
        for (unsigned speed_index = 1; speed_index <= 5; ++speed_index) {
            const unsigned f = 0x10u / Field_MoveSpeeds[speed_index];
            for (int feet = -10; feet <= 120; feet += 5) {
                g_cz_have[0] = false;
                const Run west = run(0x060, speed_index, static_cast<float>(feet));
                Check(west.frames == 24 * f, "case zero: the west walk's frames are the original's 24 steps'",
                      static_cast<int>(west.frames), static_cast<int>(24 * f));
                Check(west.west == 21, "case zero: 21 west", static_cast<int>(west.west));
                const unsigned need = feet <= 0 ? 0u : (static_cast<unsigned>(feet) * 10u + kCaseZeroRise10 - 1) / kCaseZeroRise10;
                const unsigned most = 3u * f;   // all three steps' frames at one frame a step
                Check(west.north >= (need < most ? need : most) && west.north >= 1 && west.north <= most,
                      "case zero: north enough to clear the top row when the frames allow", static_cast<int>(west.north),
                      static_cast<int>(need));
                if (need <= most) {
                    const float bottom = GetY() + 1.0f;
                    Check(bottom <= 0.0f || need == 0, "case zero: the rectangle's bottom at or above row 0", feet,
                          static_cast<int>(bottom));
                }
                // The speed is the actor's at its next op (the plan may end on fast steps).
                SetPos(context, 0x063);
                StepSite(context, script, result);
                Check(context[4] == speed_index, "case zero: the speed put back", static_cast<int>(context[4]));
                const unsigned north = west.north;
                const Run east = run(0x08B, speed_index, 0.0f);
                Check(east.frames == 18 * f, "case zero: the east walk's frames are the original's 18 steps'",
                      static_cast<int>(east.frames), static_cast<int>(18 * f));
                Check(east.south == north && east.east == 15, "case zero: south as far as north, then 15 east",
                      static_cast<int>(east.south), static_cast<int>(north));
            }
        }
        // No north walk recorded: the east walk is the original's.
        g_cz_have[0] = false;
        reset(0x08B, 2);
        Check(!StepSite(context, script, result), "case zero: no north walk, the original's east walk");
        // Another area: nothing.
        Game_AreaNumber = 174;
        reset(0x060, 2);
        Check(!StepSite(context, script, result), "case zero: only in area 175");
        g_cz_have[0] = g_cz_have[1] = false;
    }
    // A context that moved on (an area change mid-walk) drops its plan.
    {
        const int row = RowOf(27, 0x10, 2, kOut);
        const Site& s = kSites[row];
        Game_AreaNumber = static_cast<unsigned short>(s.area);
        reset(s.move, 2);
        StepSite(context, Pointer(s.script), result);
        SetPos(context, 0x002);
        Check(!StepSite(context, Pointer(s.script), result) && Find(context) == nullptr, "a stale plan is dropped");
    }
    for (Live& l : g_live) l = Live{};
    for (bool& on : g_site_on) on = false;
    for (bool& on : g_case_on) on = false;
    Game_AreaNumber = static_cast<unsigned short>(saved_area);
    Sprite_Current = saved_sprite;
    g_move = saved_move;
    g_kind = saved_kind;
    g_columns = saved_columns;
    g_fast_index = saved_fast;
    g_quiet = false;
}

void SelfTest() {
    g_bad = g_checks = 0;
    // 1. The rule: 0 columns, 0 steps; at any width the steps reach c + 12
    //    and one fewer does not; `top` caps them.
    for (int beyond = -60; beyond <= 140; beyond += 7)
        for (unsigned pps10 : {3u, 41u, 72u, 125u, 260u})
            for (unsigned c = 0; c <= 130; ++c) {
                const unsigned k = StepsFor(beyond, pps10, 0, c);
                if (c == 0) {
                    Check(k == 0, "0 columns move nothing", beyond, static_cast<int>(pps10));
                    continue;
                }
                const int need = static_cast<int>(c) + kHalfWidth;
                Check(beyond * 10 + static_cast<int>(k * pps10) >= need * 10, "the steps reach the edge", beyond, static_cast<int>(c));
                if (k) Check(beyond * 10 + static_cast<int>((k - 1) * pps10) < need * 10, "one step fewer would do", beyond, static_cast<int>(c));
                Check(StepsFor(beyond, pps10, 4, c) <= 4, "top caps", beyond, static_cast<int>(c));
                Check(StepsFor(beyond, pps10, 0, c + 1) >= k, "more columns, never fewer steps", beyond, static_cast<int>(c));
            }
    // 2. The table: every row's shape has what it needs; fast only on kIn.
    for (unsigned i = 0; i < kSiteCount; ++i) {
        const Site& s = kSites[i];
        Check(s.field == 0x10 || s.field == 0x18, "row field", static_cast<int>(i));
        Check(s.pps10 != 0, "row pps10", static_cast<int>(i));
        Check(s.shape != kIn || s.place != 0 || s.at0d != kNone, "kIn has a start", static_cast<int>(i));
        Check(s.shape == kIn || (s.place == 0 && s.at0d == kNone), "only kIn moves a start", static_cast<int>(i));
        Check(s.shape == kOutBefore ? s.before != kNone : s.before == kNone, "before", static_cast<int>(i));
        Check(!s.fast || s.shape == kIn, "fast only on kIn", static_cast<int>(i));
        Check(s.place == 0 || s.field == 0x10, "a placement op starts an object", static_cast<int>(i));
        Check(StepsFor(s.beyond, s.pps10, s.top, 0) == 0, "narrow: no steps", static_cast<int>(i));
    }
    // 3. Narrow applies no site: with nothing armed, every hook leaves the
    //    script context, the sprite and the result alone.
    {
        const bool was_staging = g_staging, was_hide = g_hide;
        g_staging = g_hide = false;
        unsigned char context[16] = {};
        unsigned char result = 0x5A;
        Check(!Step(context, Pointer(kSites[0].script), result) && result == 0x5A, "narrow: Step does nothing");
        Placed0D(context, Pointer(kSites[0].script), kSites[2].at0d);
        Placed(Pointer(kSites[0].place));
        unsigned char zero[16] = {};
        Check(std::memcmp(context, zero, sizeof context) == 0, "narrow: the hooks write nothing");
        Check(!Hide(Sprite_Objects), "narrow: nothing hidden");
        Check(!WouldHide(Sprite_Objects), "narrow: the census says not hidden");
        g_staging = was_staging;
        g_hide = was_hide;
    }
    // 4. The hide's decision at the band edges, the scene mask on and off,
    //    the leader (not a candidate) and an exception excluded.
    {
        const float c = 53.0f;
        struct Edge { float left, right; bool hide; };
        static const Edge kEdges[] = {
            {-80.0f, -53.0f, true},   {-80.0f, -52.9f, false},  {-70.0f, -54.0f, true},   {-60.0f, -40.0f, false},
            {-10.0f, 10.0f, false},   {100.0f, 120.0f, false},  {372.9f, 390.0f, false},  {373.0f, 390.0f, true},
            {380.0f, 400.0f, true},   {440.0f, 460.0f, true},   {-200.0f, -150.0f, true}, {300.0f, 380.0f, false},
        };
        for (const Edge& e : kEdges)
            for (unsigned mask = 0; mask < 0x20; ++mask)
                for (int fs = 0; fs < 2; ++fs)
                    for (int cand = 0; cand < 2; ++cand)
                        for (int exc = 0; exc < 2; ++exc) {
                            const bool want = e.hide && (mask & 0xF) != 0 && fs && cand && !exc;
                            Check(HideDecision(e.left, e.right, mask, fs != 0, cand != 0, exc != 0) == want,
                                  "hide decision", static_cast<int>(e.left), static_cast<int>(mask));
                        }
        // A sweep of whole x: hidden exactly when the sprite's 24 columns lie past -53 or 373.
        for (int x = -200; x <= 520; ++x) {
            const float l = static_cast<float>(x), r = l + 24.0f;
            const bool want = r <= -c || l >= 320.0f + c;
            Check(HideDecision(l, r, 1, true, true, false) == want, "hide sweep", x);
            Check(!HideDecision(l, r, 0x10, true, true, false), "the leader's state alone is no scene", x);
        }
        // The field scene, as the game tells it (EB-1's census: the mask is set on the world map and in the
        // demo; census 3: mode 4, the field with a message open). Every mode 0..15, chapter -1..20 and world-map
        // index 0..12: a field scene only for mode 2 or 4,
        // not chapter 16, index 11.
        for (unsigned mode = 0; mode < 16; ++mode)
            for (int chapter = -1; chapter <= 20; ++chapter)
                for (unsigned wm = 0; wm <= 12; ++wm)
                    Check(FieldScene(mode, chapter, wm) == ((mode == 2 || mode == 4) && chapter != 16 && wm == 11), "field scene",
                          static_cast<int>(mode), chapter * 100 + static_cast<int>(wm));
        // The census's cases, a sprite wholly past 373 under each: the world map (mask 0C, field mode, an
        // area with a world-map record) and the attract demo (mask 02, chapter 16) never hide; a town
        // scene in chapter 7 does.
        struct Case { unsigned mask, mode; int chapter; unsigned wm; bool hide; const char* what; };
        static const Case kCases[] = {
            {0x0C, 2, 7, 0, false, "the world map, area 16 (record 0)"},
            {0x0C, 2, 7, 2, false, "the world map, area 45 (record 2)"},
            {0x0C, 2, 3, 10, false, "the world map, area 152 (record 10)"},
            {0x02, 2, 16, 11, false, "the attract demo (chapter 16)"},
            {0x1E, 2, 16, 11, false, "the attract demo, every bit"},
            {0x02, 3, 7, 11, false, "the menu (mode 3)"},
            {0x02, 5, 7, 11, false, "battle (mode 5)"},
            {0x02, 2, 7, 11, true, "a field scene, chapter 7"},
            {0x02, 4, 3, 11, true, "a field scene with a message open (mode 4), chapter 3 (areas 39 and 41)"},
            {0x0C, 4, 3, 0, false, "mode 4 on the world map"},
            {0x02, 4, 16, 11, false, "mode 4 in the demo"},
            {0x02, 6, 3, 11, false, "mode 6 (Look_PadControl, not the field frame)"},
            {0x0C, 2, 2, 11, true, "a field area change in a scene (mask 0C off the world map)"},
            {0x00, 2, 7, 11, false, "free walking (mask 0)"},
        };
        for (const Case& k : kCases)
            Check(HideDecision(380.0f, 404.0f, k.mask, FieldScene(k.mode, k.chapter, k.wm), true, false) == k.hide,
                  k.what, static_cast<int>(k.mask), static_cast<int>(k.wm));
        // The world-map areas are the records' (WorldMap_RecordIndex reads them): 16 and 45, where the
        // census saw mask 0C, among them; a town (area 7) and case zero's area (175) not.
        {
            const unsigned short saved = Game_AreaNumber;
            for (unsigned area : {16u, 45u}) {
                Game_AreaNumber = static_cast<unsigned short>(area);
                Check((WorldMap_RecordIndex() & 0xFFu) < kNoWorldMap, "a world-map area has a record", static_cast<int>(area));
            }
            for (unsigned area : {7u, 175u}) {
                Game_AreaNumber = static_cast<unsigned short>(area);
                Check((WorldMap_RecordIndex() & 0xFFu) == kNoWorldMap, "a field area has none", static_cast<int>(area));
            }
            Game_AreaNumber = saved;
        }
        // The leader is never a candidate; the other two members and the objects are.
        unsigned field = 0, index = 0;
        Check(!Candidate(ObjTrio, field, index), "the leader is not hidden");
        Check(Candidate(ObjTrio + 0x14C, field, index) && field == 0 && index == 1, "member 1 is a candidate");
        Check(Candidate(ObjTrio + 2 * 0x14C, field, index) && index == 2, "member 2 is a candidate");
        Check(!Candidate(ObjTrio + 3 * 0x14C, field, index), "past the trio");
        Check(Candidate(Sprite_Objects + 29 * 0xA4, field, index) && field == 0x10, "object 29");
        Check(!Candidate(Sprite_Objects + 30 * 0xA4, field, index), "past object 29");
        Check(!Candidate(Sprite_Kind2, field, index), "the kind-2 object is not hidden");
        Check(!Excepted(0, 0x10, 0), "the exception list starts empty");
    }
    // 5. Case zero's table: each script's two west walks add north, its east walk south; 3 fewer steps a walk.
    {
        for (const CaseZeroScript& cs : kCaseZeroScripts) {
            int north = 0, south = 0, shorter = 0;
            for (const CaseZeroOp& c : kCaseZero) {
                if (c.index != cs.index) continue;
                north += c.dir == 1 && c.after;
                south += c.dir == 5 && !c.after;
                shorter += c.delta;
            }
            Check(north == 2 && south == 1 && shorter == -9, "case zero: two north, one south, 3 fewer a walk", north, shorter);
        }
        const unsigned char saved_fast_index = g_fast_index;
        g_fast_index = 5;
        unsigned a = 0, b = 0;
        for (unsigned need = 0; need <= 40; ++need) {
            Check(CaseZeroSplit(2, 3, need, a, b) && a * 8 + b == 24 && (a + b >= need || a == 0), "the split at speed 2",
                  static_cast<int>(need), static_cast<int>(a * 100 + b));
        }
        g_fast_index = saved_fast_index;
    }
    EngineTest();
    bof3::Log("shadow      wide_staging self-test: %u checks, %u mismatches", g_checks, g_bad);
}

void Arm() {
    g_columns = Widescreen_Live();
    if (g_columns == 0) return;
    g_log_all = EnvUnsigned("BOF3X_WIDE_STAGING_LOG", 0, 0, 1) != 0;
    // The fastest speed whose frames stay whole: a step is still half a cell.
    unsigned best = 0;
    for (unsigned i = 1; i < 16; ++i) {
        const unsigned s = Field_MoveSpeeds[i];
        if (s != 0 && 0x10u % s == 0 && s > best) {
            best = s;
            g_fast_index = static_cast<unsigned char>(i);
        }
    }
    const bool staging = EnvUnsigned("BOF3X_WIDE_STAGING", 1, 0, 1) != 0;
    const bool hide = EnvUnsigned("BOF3X_WIDE_HIDE", 1, 0, 1) != 0;
    unsigned on = 0, off = 0;
    if (staging) {
        for (unsigned i = 0; i < kSiteCount; ++i) {
            const Site& s = kSites[i];
            const unsigned char* const script = ScriptOf(s.area, s.field, s.index);
            std::uint32_t h = Fnv(script, s.span);
            if (s.place) h = Fnv(Pointer(s.place), EventScript_OpLengths[Pointer(s.place)[0] >> 4], h);
            unsigned dir, steps, length;
            const bool ok = Address(script) == s.script && h == s.hash && MoveAt(script + s.move, dir, steps, length) &&
                            steps != 0 && (dir & ~7u) == 0 && steps + Steps(s) <= 0xFF &&
                            (s.at0d == kNone || script[s.at0d] == 0x0D) &&
                            (s.before == kNone || script[s.before] == 0xEB || script[s.before] == 0xE6);
            g_site_on[i] = ok;
            if (ok) {
                ++on;
            } else {
                ++off;
                bof3::Log("DIV-0122    site %u (area %u +0x%02X[%u]) left original: the script is %08X, hash %08X - not the "
                          "bytes the row was measured on (a disc-built install?)",
                          i, s.area, s.field, s.index, Address(script), h);
            }
        }
        for (unsigned i = 0; i < sizeof kCaseZero / sizeof kCaseZero[0]; ++i) {
            const CaseZeroOp& c = kCaseZero[i];
            const CaseZeroScript* cs = nullptr;
            for (const CaseZeroScript& x : kCaseZeroScripts)
                if (x.index == c.index) cs = &x;
            const unsigned char* const script = ScriptOf(kCaseZeroArea, 0x18, c.index);
            unsigned dir, steps, length;
            const bool ok = cs && Address(script) == cs->script && Fnv(script, cs->span) == cs->hash &&
                            MoveAt(script + c.at, dir, steps, length) && script[c.at] == 0xF2 &&
                            static_cast<int>(steps) + c.delta >= 1;
            g_case_on[i] = ok;
            if (ok) {
                ++on;
            } else {
                ++off;
                bof3::Log("DIV-0123    case zero +0x18[%u] +0x%03X left original: not the bytes the row was read on", c.index,
                          c.at);
            }
        }
        g_staging = on != 0;
    }
    g_hide = hide && g_columns > widescreen_rule::ColumnsForAspect(16, 9);
    bof3::Log("DIV-0122/23 %s: %u of %u rows armed at %u columns (%u left original)%s",
              staging ? "the wide staging on" : "the wide staging off (BOF3X_WIDE_STAGING=0)", on,
              kSiteCount + static_cast<unsigned>(sizeof kCaseZero / sizeof kCaseZero[0]), g_columns, off,
              g_fast_index ? "" : "; no fast speed found");
    bof3::Log("DIV-0124    the 21:9 hide %s", g_hide ? "on: field objects wholly past the 16:9 view are not drawn in a scene"
                                                     : (hide ? "off: the view is not wider than 16:9" : "off (BOF3X_WIDE_HIDE=0)"));
}

}  // namespace

void Inject() {
    if (bof3::WantsShadow("wide_staging")) SelfTest();
    Arm();
}

}  // namespace wide_staging
