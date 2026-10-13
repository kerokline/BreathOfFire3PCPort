// The wide staging (wide_staging.h; docs/wide-staging.md): DIV D, E and F of
// the ultrawide round, armed only when the view is wide.
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
    std::uint8_t dir;     // the walk added
    std::uint8_t steps;
    bool after;           // the added walk follows the F2 (else it comes before)
};
constexpr std::uint16_t kCaseZeroArea = 175;
// clang-format off
const CaseZeroScript kCaseZeroScripts[] = {
    {4, 0x641FEC, 0x6354AB60, 0x008E},
    {5, 0x6420A4, 0x32775E7B, 0x00A0},
};
const CaseZeroOp kCaseZero[] = {
    {4, 0x060, -3, 1, 6, true},    // the first west walk (clock 0x1B): 21 west, then 6 north
    {4, 0x082, -3, 1, 6, true},    // the second (0x29), the same
    {4, 0x08B, -3, 5, 6, false},   // the east walk (0x33): 6 south first, then 3 fewer east
    {5, 0x074, -3, 1, 6, true},
    {5, 0x094, -3, 1, 6, true},
    {5, 0x09D, -3, 5, 6, false},
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
    Element e[2];
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
    bof3::Log("DIV-D       the live table is full: an entry from frame %u (context %08X) dropped", oldest->frame,
              Address(oldest->context));
    *oldest = Live{};
    oldest->context = context;
    return oldest;
}
void Drop(Live* l) { *l = Live{}; }

void Note(int site, int cz, unsigned steps) {
    if (site >= 0) {
        if (g_site_logged[site] && !g_log_all) return;
        g_site_logged[site] = true;
        const Site& s = kSites[site];
        bof3::Log("DIV-D       site %d (area %u +0x%02X[%u] +0x%03X): %u steps at %u columns, frame %u", site, s.area,
                  s.field, s.index, s.move, steps, g_columns, Frame_Counter);
    } else if (cz >= 0) {
        if (g_case_logged[cz] && !g_log_all) return;
        g_case_logged[cz] = true;
        bof3::Log("DIV-E       case zero, +0x18[%u] +0x%03X: the PSP's staging, frame %u", kCaseZero[cz].index,
                  kCaseZero[cz].at, Frame_Counter);
    }
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
            plan = Plan{};
            plan.site = -1;
            plan.cz = static_cast<int>(i);
            plan.length = length;
            const Element walk = {static_cast<unsigned char>(dir), static_cast<unsigned char>(static_cast<int>(steps) + c.delta), false};
            const Element added = {c.dir, c.steps, false};
            plan.e[0] = c.after ? walk : added;
            plan.e[1] = c.after ? added : walk;
            plan.count = 2;
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
    if (l && l->running && (l->script != script || l->pos != pos || l->area != Game_AreaNumber)) {
        // The context moved on without us (a scene cut short, an area change): forget the plan.
        if (l->restore) context[4] = l->speed;
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
        if (MoveScript_ObjectKind() == 2) return false;   // the kind-2 object's path: never a site
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
    if (e.steps != 0) MoveCmd_Move(context, e.dir);
    if (l->next == l->plan.count && l->plan.advance) {
        SetPos(context, l->pos + l->plan.length);   // as the op would have left it (the step's own +1 included)
        Drop(l);
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
        if (l->restore) context[4] = l->speed;
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
            bof3::Log("DIV-D       site %u: the placement op at %08X did not leave its object current (%08X); not moved", i,
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
        if (l->restore) context[4] = l->speed;
        *l = Live{};
        l->context = context;
        l->marked = k != 0;
        l->site = static_cast<int>(i);
        l->frame = Frame_Counter;
        return;
    }
}

// --- F: the hide -------------------------------------------------------------

bool HideDecision(float left, float right, unsigned scene_mask, bool candidate, bool excepted) {
    if (!candidate || excepted) return false;
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
    float left, right, top, bottom;
    if (!band_census::ScreenRect(sprite, left, right, top, bottom)) return false;
    return HideDecision(left, right, mask, true, Excepted(Game_AreaNumber, field, index));
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
                for (int cand = 0; cand < 2; ++cand)
                    for (int exc = 0; exc < 2; ++exc) {
                        const bool want = e.hide && (mask & 0xF) != 0 && cand && !exc;
                        Check(HideDecision(e.left, e.right, mask, cand != 0, exc != 0) == want, "hide decision",
                              static_cast<int>(e.left), static_cast<int>(mask));
                    }
        // A sweep of whole x: hidden exactly when the sprite's 24 columns lie past -53 or 373.
        for (int x = -200; x <= 520; ++x) {
            const float l = static_cast<float>(x), r = l + 24.0f;
            const bool want = r <= -c || l >= 320.0f + c;
            Check(HideDecision(l, r, 1, true, false) == want, "hide sweep", x);
            Check(!HideDecision(l, r, 0x10, true, false), "the leader's state alone is no scene", x);
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
    // 5. Case zero's rewrite: 3 fewer steps, never below 1, and the added
    //    walk's displacement undone on the later route (6 north, 6 south).
    {
        // Per script: the two west walks each gain 6 north and the east walk
        // 6 south first - the second route's end 6 north of the original's
        // before the east walk, and every walk still 3 steps or more.
        for (const CaseZeroScript& cs : kCaseZeroScripts) {
            int north = 0, shorter = 0;
            for (const CaseZeroOp& c : kCaseZero) {
                if (c.index != cs.index) continue;
                north += c.dir == 1 ? c.steps : c.dir == 5 ? -static_cast<int>(c.steps) : 0;
                shorter += c.delta;
            }
            Check(north == 6 && shorter == -9, "case zero: 6 north twice, 6 south once, 3 fewer steps a walk", north, shorter);
        }
    }
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
                bof3::Log("DIV-D       site %u (area %u +0x%02X[%u]) left original: the script is %08X, hash %08X - not the "
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
                bof3::Log("DIV-E       case zero +0x18[%u] +0x%03X left original: not the bytes the row was read on", c.index,
                          c.at);
            }
        }
        g_staging = on != 0;
    }
    g_hide = hide && g_columns > widescreen_rule::ColumnsForAspect(16, 9);
    bof3::Log("DIV-D/E     %s: %u of %u rows armed at %u columns (%u left original)%s",
              staging ? "the wide staging on" : "the wide staging off (BOF3X_WIDE_STAGING=0)", on,
              kSiteCount + static_cast<unsigned>(sizeof kCaseZero / sizeof kCaseZero[0]), g_columns, off,
              g_fast_index ? "" : "; no fast speed found");
    bof3::Log("DIV-F       the 21:9 hide %s", g_hide ? "on: field objects wholly past the 16:9 view are not drawn in a scene"
                                                     : (hide ? "off: the view is not wider than 16:9" : "off (BOF3X_WIDE_HIDE=0)"));
}

}  // namespace

void Inject() {
    if (bof3::WantsShadow("wide_staging")) SelfTest();
    Arm();
}

}  // namespace wide_staging
