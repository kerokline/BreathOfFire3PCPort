// Moved here by tools/move_funcs.py from event_script.cpp, field_o.cpp, map_field_objects.cpp, scena_se.cpp (docs/source-layout-round.md).
#include "game/field/event_script.h"

#include <cstdint>
#include <cstring>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/event_script.h"
#include "game/event_script_callees.h"
#include "game/faerie_ring.h"
#include "game/field/event_script_callees.h"
#include "game/field_o.h"
#include "game/field_o_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/map_field_objects.h"
#include "game/map_field_objects_callees.h"
#include "game/map_rim.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scena_se_callees.h"
#include "game/scenario_harness.h"
#include "game/wide_staging.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = event_script::at;

}  // namespace

namespace {

using namespace map_field_objects;

}  // namespace

namespace {

unsigned char* AtEventScript(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

void SetWord(unsigned char* p, unsigned v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

// The count and the bank word (see the top of the file).
short Count() {
    short v;
    std::memcpy(&v, AtEventScript(::bof3::Img(SharedScratch_addr)), sizeof v);
    return v;
}

void SetCount(unsigned v) { SharedScratch.w_00 = v; }

unsigned short Bank() {
    unsigned short v;
    std::memcpy(&v, AtEventScript(::bof3::Img(SharedScratch_addr)) + 2, sizeof v);
    return v;
}

void SetBank(unsigned v) { SharedScratch.w_02 = v; }

unsigned char* Object(int n) { return rv::Bytes(Sprite_Objects) + n * 0xA4; }

unsigned char* Extra(unsigned n) { return rv::Bytes(&Sprite_ObjectsExtra[n]); }

// A placement's x or z: the whole part from `whole`, and the half (0x8000) if
// `half` is non-zero - `neg / sbb / and 0x8000`.
std::uint32_t Coordinate(unsigned char half, unsigned char whole) {
    return (half ? 0x8000u : 0u) | static_cast<std::uint32_t>(whole) << 16;
}

// `cdq` / `idiv`: the original's signed division, the divide fault included.
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor) {
    std::int32_t quotient;
    __asm__ volatile("cltd\n\tidivl %2" : "=a"(quotient) : "a"(dividend), "r"(divisor) : "edx", "cc");
    return quotient;
}

// EventScript_If and IfNot, which differ only in which arm runs first:
// `taken` is the arm-runs answer. The condition gets the position of its
// operand (it may move it) and its index masked to five bits - 17..31 reach
// MoveScript_CounterOps, the E9 handlers and data words, which take other
// arguments; no shipped script has one (tools/event_scan.py). One byte past
// the operand, the arms: the depth starts at 1, FE at depth 0 ends the if and
// is stepped over; the else byte chooses the step each time round, so an FD
// swaps them once (a second FD only sets it again).
const unsigned char* If(const unsigned char* at, bool run_on_true) {
    unsigned char depth = 0, else_seen = 0;
    const unsigned char* q = at;
    const unsigned c = *q++;
    const bool answer = (event_script::g.conditions[c & 0x1F](&q) & 0xFF) != 0;
    ++depth;
    ++q;
    const bool run_first = answer == run_on_true;
    for (;;) {
        if (depth == 0 && *q == 0xFE) return q + 1;
        const bool run = (else_seen == 0) == run_first;
        q = run ? event_script::g.run_step(q, &else_seen, &depth) : event_script::g.skip_step(q, &else_seen, &depth);
    }
}

std::uint16_t g_placed_area = 0xFFFF;   // the area the last placement ran for

std::uint8_t g_levett_door = 0;          // 1 entered area 70 by the north-east door, 2 the south-west, 0 neither

void MountainPasses_AfterPlacement() {
    if (!g_mountain_passes) return;
    const std::uint16_t area = Game_AreaNumber;
    const std::uint16_t from = g_placed_area;
    g_placed_area = area;
    const unsigned px = static_cast<unsigned>(SharedScratch.d_10 >> 16) & 0xFFu;
    const unsigned pz = static_cast<unsigned>(static_cast<std::int32_t>(AreaEntry_PendingZ) >> 16) & 0xFFu;
    if (area == 70 && from == 16) {
        g_levett_door = static_cast<std::uint8_t>(px == 0x38 ? 1 : px == 0x16 ? 2 : 0);
        bof3::Log("DIV-0191    Mt. Levett entered at (%#x, %#x): door %u", px, pz, g_levett_door);
    } else if (area == 16 && from == 70) {
        const unsigned link = (px == 0x39 && pz == 0x38) ? 1u : (px == 0x36 && pz == 0x3B) ? 2u : 0u;
        if (link != 0 && g_levett_door != 0 && link != g_levett_door && !(MountainPasses_Bits & 1)) {
            MountainPasses_Bits = static_cast<unsigned char>(MountainPasses_Bits | 1);
            bof3::Log("DIV-0191    Mt. Levett crossed (door %u to link %u): the map is passable from now on", g_levett_door, link);
        }
        g_levett_door = 0;
    }
    if (area == 16 && (MountainPasses_Bits & 1)) {
        for (int i = 0; i < 30; ++i) {
            unsigned char* const o = Object(i);
            const auto* const head = rv::As<records::SpriteHead>(o);
            if (o[0] != 0 && head->x_whole == 0x38 && head->z_whole == 0x39) {
                o[0] = 0;
                bof3::Log("DIV-0191    Mt. Levett: object %d at (0x38, 0x39) dropped", i);
            }
        }
    }
    if (area == 87 && Cond_ByteFA >= 9) {
        unsigned char* const plane = const_cast<unsigned char*>(AreaMap_Bytes.Native());
        const unsigned width = AreaMap_Header[0];
        // 0x91, the passage kind: it walks (Field_CellKind class 0), has no
        // encounter (the encounter test wants a 0 cell), no plate, and the
        // field menu's camp test refuses it (its AreaMap_ByteAt & 0xF0 among
        // 0xA0 / 0x91, docs/menu_lists.md) - as the passage cells (0x34,
        // 0x1B) beside it already do. The owner's call, 2026-10-30 night.
        static constexpr unsigned char kCells[][2] = {{0x38, 0x1D}, {0x38, 0x1C}, {0x38, 0x1B}, {0x37, 0x1B}};
        for (const auto& c : kCells) plane[c[1] * width + c[0]] = 0x91;
        // The strip's corner heights (AreaMap_Corners, the mesh's and the
        // leader's alike) stay the ground's 0x10: raised to the hut's 0x16
        // they lifted the drawn tiles with the leader (the owner's eye,
        // 2026-10-30 night), so the leader stands a step below the painted
        // block there - Capcom's art over Capcom's mesh, left as it is.
        bof3::Log("DIV-0191    Mt. Zublo: four passage cells laid east of the hut (chapter %d)", Cond_ByteFA);
    }
}

// DIVERGENCE DIV-0204 (desert_faerie=1): a second faerie village entrance on
// the desert map (area 152, the map of Caer Xhan, the Container Yard and the
// Oasis). The village has no link anywhere: the Rhapala map's entrance is one
// cell of byte 0xAE, and confirm on it (Field_LeaderCellEvent 0x531950)
// starts tail kind 0x37, FieldTail_StoryWarp 0x56DF10 - message 0x20 or,
// holding the Faerie Tiara, 0x21 and its choice; the village's stage from
// story flags 0xF6..0xFF (area 175..185); the leader's cell and
// Game_AreaNumber kept as the return point (0x904148..0x904150), which the
// village's exit square (Area185_StepHook 0x429DC0) changes back to. Area
// 152's own text holds the same two prompts at 0x20 / 0x21 (the US disc's
// AREA152, read 2026-11-05), and its descriptor's choice handler 0 is the
// one area 87's is (0x41D270), so one 0xAE cell is all the entrance needs;
// the return comes back to this map by itself. The cell is written into the
// plane after the placement script, every time the map loads (the plane on
// disc never changes, as DIV-0191's cells). 0xAE walks (a place cell), has
// no encounter and refuses the camp and the Tiara, as Rhapala's does.
// Always there when on, as Rhapala's is (the owner, 2026-11-05).
// The cell: (0x27, 0x2C), the owner's spot west of the Oasis's link (0x30,
// 0x2B), sampled live with tools/cell_peek.py (2026-11-05): 0x00 open sand,
// no link of area 152's list on it.
constexpr unsigned kDesertFaerieX = 0x27, kDesertFaerieZ = 0x2C;

void DesertFaerie_AfterPlacement() {
    if (!g_desert_faerie) return;
    if (Game_AreaNumber != 152) {
        FaerieRing_Leave();   // the borrowed palette entries back to zero (faerie_ring.h)
        return;
    }
    unsigned char* const plane = const_cast<unsigned char*>(AreaMap_Bytes.Native());
    const unsigned width = AreaMap_Header[0];
    plane[kDesertFaerieZ * width + kDesertFaerieX] = 0xAE;
    bof3::Log("DIV-0204    the desert map's faerie village cell laid at (%#x, %#x)", kDesertFaerieX, kDesertFaerieZ);
    FaerieRing_Lay(kDesertFaerieX, kDesertFaerieZ);   // its drawn ring (faerie_ring.h)
}

// EventOp_1x and EventOp_2x, which differ in 2x's +0x18 / +0x1C and object
// +0x83. The object's own bytes are written by the count read again each
// time, the sprite's through Sprite_Current read again each time - the same
// object unless a callee moved one of them.
void Place(const unsigned char* op, bool two) {
    if (Count() >= 30) return;
    unsigned char* const o = Object(Count());
    Sprite_Current = o;
    Field_ActiveMember = o;
    SetBank(static_cast<unsigned>(op[1]) << 8 | op[2]);
    event_script::g.reset();
    event_script::g.set_bank(Bank());
    Sprite_Current[0] = 1;
    rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->x, Coordinate(op[6], op[7]));
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_x, static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->x));
    rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->z, Coordinate(op[8], op[9]));
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_z, static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->z));
    rv::As<records::SpriteObject>(Object(Count()))->best_score = 0;
    const long elevation = event_script::g.elevation(static_cast<long>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->x)),
                                       static_cast<long>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->z)));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(elevation));
    Sprite_Current[1] = op[0xA];
    Sprite_Current[2] = op[0xC];
    if (two) {
        rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->work_18, op[0x11]);
        rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->work_1c, op[0x12]);
    }
    Sprite_Current[8] = op[0] & 0xF;
    Sprite_Current[6] = op[0] >> 4;
    unsigned char* const n = Object(Count());
    rv::As<records::SpriteObject>(n)->half_w = op[4];
    rv::As<records::SpriteObject>(n)->half_d = op[5];
    rv::As<records::SpriteObject>(n)->trigger_word = static_cast<unsigned>(op[0xE]) << 8 | op[0xF];
    n[0x84] = op[0xB];
    if (two) n[0x83] = op[0x10];
    n[0xA0] = 0x7F;
    event_script::g.set_flags(op + 0xD);
    if (op[3] & 0x80) Sprite_Current[0] |= 0x20;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5C] = op[3] & 0xF;
    event_script::g.face();
    SetCount(static_cast<unsigned>(Count() + 1));
}

constexpr auto& AtFieldO = move_script::At;

using move_script::Long;

using U = std::uint32_t;

unsigned char* PtrAt(const unsigned char* cell) { return AtFieldO(static_cast<U>(Long(cell))); }

unsigned char* Sc() { return Sprite_Current; }

// The event ops' count and bank words (event_script.cpp's terms): the word at
// SharedScratch is the next Sprite_Objects entry a placement fills, the word
// after it the animation bank a placement passes on.
short CountFieldO() {
    short v;
    std::memcpy(&v, AtFieldO(::bof3::Img(SharedScratch_addr)), sizeof v);
    return v;
}

unsigned short BankFieldO() { return SharedScratch.w_02; }

void BumpCount() { SharedScratch.w_00 = static_cast<unsigned>(CountFieldO() + 1); }

unsigned char* ObjectFieldO(int n) { return rv::Bytes(&Sprite_Objects[n]); }

// A placement's x or z: the whole part and the half (0x8000) when `half` is not 0.
std::int32_t CoordinateFieldO(unsigned char half, unsigned char whole) {
    return static_cast<std::int32_t>((half ? 0x8000u : 0u) | static_cast<U>(whole) << 16);
}

// EventOp_3x / 4x / 7x / 6x / Ax's shared start: Sprite_Current and
// Field_ActiveMember the next Sprite_Objects entry.
unsigned char* Claim() {
    unsigned char* const o = ObjectFieldO(CountFieldO());
    Sprite_Current = o;
    Field_ActiveMember = o;
    return o;
}

// x, z into the sprite and into object +0x8C / +0x90 (+0x94 0), by the count
// read again each time, as EventOp_1x's.
void PlaceXZ(std::int32_t x, std::int32_t z) {
    rv::As<records::SpriteObject>(Sc())->x = x;
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()))->target_x, rv::As<records::SpriteObject>(Sc())->x);
    rv::As<records::SpriteObject>(Sc())->z = z;
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()))->target_z, rv::As<records::SpriteObject>(Sc())->z);
    rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()))->best_score = 0;
}

void Ground() {
    records::SpriteObject* const sc = rv::As<records::SpriteObject>(Sc());
    const long e = SH_CALL(AreaMap_Elevation)(sc->x, sc->z);
    rv::Store<unsigned>(rv::As<records::SpriteObject>(Sc())->height_whole, static_cast<U>(e));
}

void Colour(unsigned char v) {
    rv::As<records::SpriteObject>(Sc())->shade[0] = v;
    rv::As<records::SpriteObject>(Sc())->shade[2] = v;
    rv::As<records::SpriteObject>(Sc())->shade[1] = v;
}

}  // namespace

namespace map_field_objects {

namespace {

template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

}  // namespace

}  // namespace map_field_objects

namespace {

unsigned char* AtMapFieldObjects(map_field_objects::U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }

map_field_objects::U Byte(map_field_objects::U address) { return AtMapFieldObjects(address)[0]; }

map_field_objects::U DwordMapFieldObjects(const unsigned char* p) {
    map_field_objects::U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

void SetWordMapFieldObjects(unsigned char* p, map_field_objects::U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}

void SetDwordMapFieldObjects(unsigned char* p, map_field_objects::U v) { std::memcpy(p, &v, sizeof v); }

map_field_objects::U S16(map_field_objects::U v) { return static_cast<map_field_objects::U>(static_cast<std::int32_t>(static_cast<std::int16_t>(v))); }

U SpriteCurrent() { return DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Sprite_Current_addr))); }

unsigned char* Sprite() { return AtMapFieldObjects(SpriteCurrent()); }

// The placement count (SharedScratch, read afresh at every use as the
// original reads the word [0x903850]) and the bank word after it.
short CountScenaSe() { return static_cast<std::int16_t>(scena_se::at::kCount); }

unsigned char* ObjectScenaSe(int n) { return rv::Bytes(Sprite_Objects) + n * static_cast<int>(scena_se::at::kObjectStride); }

}  // namespace

// event_script's callee seam, moved here from event_script.cpp by tools/retire_shells.py
// (docs/source-layout-round.md 5): the group's lowest function lives in this file.
namespace event_script {

const Callees kOriginals = {
    EventScript_Op, EventScript_Control, EventScript_If, EventScript_IfNot, EventScript_Switch,
    EventScript_IfRunStep, EventScript_IfSkipStep, EventScript_SkipControl, EventScript_SkipIf,
    EventScript_SkipSwitch, EventScript_CaseRun, EventScript_CaseSkip,
    reinterpret_cast<const ::bof3::CodeSlot<Condition>*>(EventScript_Conditions),
    {EventOp_0x, EventOp_1x, EventOp_2x, EventOp_3x, EventOp_4x, EventOp_5x, EventOp_6x, EventOp_7x, EventOp_8x,
     EventOp_9x, EventOp_Ax, EventOp_Bx, EventOp_Cx, EventOp_Dx, EventOp_Ex, EventOp_0x},
    EventObj_Reset, Sprite_SetAnimationBank, AreaMap_Elevation, EventObj_SetFlags, EventObj_Face,
    PartyRecord_Clear, Sprite_InitFromEntry, Kind2_Place,
    EventScript_Run, Flags_Test,
};
Callees g = kOriginals;

}  // namespace event_script

// original 0x579740: an area's placement script (PSX FUN_801A4E58), from the
// area descriptor +0. As the original has it: the count is signed - a count
// the script left negative clears from that (negative) entry, before the
// array, up to 29; one of 30 or more clears nothing and is left as it is.
extern "C" void __cdecl Area_RunPlacement(const unsigned char* script) {
    SetCount(0);
    for (unsigned i = 0; i < 4; ++i) Extra(i)[0] = 0;
    SetBank(4);
    event_script::g.run(script);
    const short n = Count();
    if (n >= 30) return;
    SetCount(30);   // stored before the loop, as the original stores it
    for (int i = n; i < 30; ++i) Object(i)[0] = 0;
    MountainPasses_AfterPlacement();
    DesertFaerie_AfterPlacement();
}

// original 0x5797C0: runs an event script to its FF (PSX FUN_801A4F44).
// As the original has it: the flag bank starts as Cond_ByteFA sign-extended;
// every byte below F0 is an op and every other byte but FF a control op - a
// control op that does not advance (F2, F3, F5..F8, FB..FE) never ends.
extern "C" void __cdecl EventScript_Run(const unsigned char* script) {
    EventScript_FlagBank = static_cast<unsigned short>(static_cast<short>(Cond_ByteFA));
    const unsigned char* at = script;
    while (*at != 0xFF) at = *at < 0xF0 ? event_script::g.op(at) : event_script::g.control(at);
}

// original 0x579800: a control op, F0 and up (PSX FUN_801A5408). Anything but
// F0 F1 F4 F9 FA - and any byte below F0 - comes back where it was.
extern "C" const unsigned char* __cdecl EventScript_Control(const unsigned char* at) {
    switch (at[0]) {
    case 0xF0: return event_script::g.if_(at + 1);
    case 0xF1: return event_script::g.if_not(at + 1);
    case 0xF4: return event_script::g.switch_(at + 1);
    case 0xF9:
        EventScript_FlagBank = at[1];
        return at + 2;
    case 0xFA:
        EventScript_FlagBank = static_cast<unsigned short>(static_cast<short>(Cond_ByteFA));
        return at + 1;
    default: return at;
    }
}

// original 0x579890: F0 c x (PSX FUN_801A4FC8), `at` past the F0.
extern "C" const unsigned char* __cdecl EventScript_If(const unsigned char* at) { return If(at, true); }

// original 0x579950: F1 c x (PSX FUN_801A5160), `at` past the F1.
extern "C" const unsigned char* __cdecl EventScript_IfNot(const unsigned char* at) { return If(at, false); }

// original 0x579A10: one step of an arm that runs (PSX FUN_801A52F8). FE
// counts the depth down and stays; FD sets the else byte; FD and FF step one.
extern "C" const unsigned char* __cdecl EventScript_IfRunStep(const unsigned char* at, unsigned char* else_seen,
                                                             unsigned char* depth) {
    const unsigned char b = *at;
    if (b < 0xF0) return event_script::g.op(at);
    if (b < 0xFD) return event_script::g.control(at);
    if (b == 0xFE) {
        --*depth;
        return at;
    }
    if (b == 0xFD) *else_seen = 1;
    return at + 1;
}

// original 0x579A50: one step of an arm that is skipped (PSX FUN_801A537C):
// an op by its length, a control op through EventScript_SkipControl, FE and
// FD as EventScript_IfRunStep.
extern "C" const unsigned char* __cdecl EventScript_IfSkipStep(const unsigned char* at, unsigned char* else_seen,
                                                              unsigned char* depth) {
    const unsigned char b = *at;
    if (b < 0xF0) return at + EventScript_OpLengths[b >> 4];
    if (b < 0xFD) return event_script::g.skip_control(at);
    if (b == 0xFE) {
        --*depth;
        return at;
    }
    if (b == 0xFD) *else_seen = 1;
    return at + 1;
}

// original 0x579AA0: steps over a control op without running it (PSX
// FUN_801A54AC). The nested if and switch are skipped by Capcom's
// EventScript_SkipIf / SkipSwitch from ON their first byte; F2 F3 F5..F8 and
// FB..FF - and any byte below F0 - come back where they were.
extern "C" const unsigned char* __cdecl EventScript_SkipControl(const unsigned char* at) {
    switch (at[0]) {
    case 0xF0:
    case 0xF1: return event_script::g.skip_if(at);
    case 0xF4: return event_script::g.skip_switch(at);
    case 0xF9: return at + 2;
    case 0xFA: return at + 1;
    default: return at;
    }
}

// original 0x579B00: F4 c (PSX FUN_801A5520), `at` past the F4.
// As the original has it: the condition index is the byte SIGNED and not
// masked - the table read from -128 to 127 entries - where the if masks it;
// the loop takes F6 x (a case), F7 (the default) and F5 (the end), and any
// other byte there loops for ever. Once a case has run, later cases are not
// tested: EventScript_CaseSkip starts past their F6 x. A case's body - run
// or skipped - starts one byte past where the condition left the position
// (the conditions reached leave it on x).
extern "C" const unsigned char* __cdecl EventScript_Switch(const unsigned char* at) {
    bool open = true;
    const unsigned char* q = at;
    const auto c = static_cast<signed char>(*q++);
    for (;;) {
        const unsigned char b = *q;
        if (b == 0xF5) return q + 1;
        if (b == 0xF6) {
            ++q;
            bool ran = false;
            if (open && (event_script::g.conditions[c](&q) & 0xFF) != 0) {
                ++q;
                q = event_script::g.case_run(q);
                open = false;
                ran = true;
            }
            if (!ran) {
                ++q;
                q = event_script::g.case_skip(q);
            }
        }
        if (*q != 0xF7) continue;
        ++q;
        if (open) {
            q = event_script::g.case_run(q);
            open = false;
        } else {
            q = event_script::g.case_skip(q);
        }
    }
}

// original 0x579BA0: a case body that runs, to past its F8 (PSX
// FUN_801A5648). F6 steps 2 and F7 1 - further labels of the same body; F2
// F3 F5 and FB..FF do nothing and are met again for ever.
extern "C" const unsigned char* __cdecl EventScript_CaseRun(const unsigned char* at) {
    bool go = true;
    do {
        const unsigned char b = *at;
        if (b < 0xF0) {
            at = event_script::g.op(at);
            continue;
        }
        switch (b) {
        case 0xF0:
        case 0xF1:
        case 0xF4:
        case 0xF9:
        case 0xFA: at = event_script::g.control(at); break;
        case 0xF6: at += 2; break;
        case 0xF7: at += 1; break;
        case 0xF8:
            go = false;
            at += 1;
            break;
        default: break;
        }
    } while (go);
    return at;
}

// original 0x579C20: a case body skipped (PSX FUN_801A56F0) - to past its F8,
// or back AT the next F6 (the next case, which the switch then tests). F7
// steps 1; F2 F3 F5 and FB..FF are met again for ever.
extern "C" const unsigned char* __cdecl EventScript_CaseSkip(const unsigned char* at) {
    bool go = true;
    do {
        const unsigned char b = *at;
        if (b < 0xF0) {
            at += EventScript_OpLengths[b >> 4];
            continue;
        }
        switch (b) {
        case 0xF0:
        case 0xF1:
        case 0xF4:
        case 0xF9:
        case 0xFA: at = event_script::g.skip_control(at); break;
        case 0xF6: return at;
        case 0xF7: at += 1; break;
        case 0xF8:
            go = false;
            at += 1;
            break;
        default: break;
        }
    } while (go);
    return at;
}

// original 0x579CA0 (PSX 0x801A57A0): an F0 / F1 skipped (EventScript_SkipControl's F0 / F1,
// the event_script.cpp table's skip_if): `at` is ON the F0 / F1; three bytes
// on, then by EventScript_OpLengths and EventScript_SkipControl (for F0..FC)
// to the FE of its depth - an FD on the way is stepped over - and one past it.
// As the original has it: byte +1 is loaded and not used; an FF on the way,
// or an FD..FF that SkipControl leaves where it was, is never passed.
extern "C" const unsigned char* __cdecl EventScript_SkipIf(const unsigned char* at) {
    at += 3;
    while (at[0] != 0xFE) {
        const unsigned char b = at[0];
        if (b >= 0xF0) {
            if (b < 0xFD) at = SH_CALL(EventScript_SkipControl)(at);
            if (at[0] == 0xFD) ++at;
        } else {
            at += EventScript_OpLengths[b >> 4];
        }
    }
    return at + 1;
}

// original 0x579CF0 (PSX 0x801A584C): `at` is ON the F4 of a switch being
// skipped; from two on, to past its F5 - an op below F0 by
// EventScript_OpLengths[op >> 4], F6 two bytes, F7 and F8 one, F0 F1 F4 F9 FA
// through EventScript_SkipControl (whose pointer is taken as the new
// position), F5 the end, returned one past.
// As the original has it: F2, F3 and FB..FF match no case of its jump table
// and the loop reads the same byte for ever (read, not run: the fuzz keeps to
// the others); byte +1 is loaded and never used.
extern "C" const unsigned char* __cdecl EventScript_SkipSwitch(const unsigned char* at) {
    const volatile unsigned char* p = at + 2;
    for (;;) {
        const unsigned char c = *p;
        if (c < 0xF0) {
            p += Byte(map_field_objects::at::OpLengthsAt() + (c >> 4));
            continue;
        }
        switch (c) {
        case 0xF0:
        case 0xF1:
        case 0xF4:
        case 0xF9:
        case 0xFA: p = g.skip_control(const_cast<const unsigned char*>(p)); break;
        case 0xF5: return const_cast<const unsigned char*>(p) + 1;
        case 0xF6: p += 2; break;
        case 0xF7:
        case 0xF8: p += 1; break;
        default: break;   // F2, F3, FB..FF: the same byte again
        }
    }
}

// original 0x579D70: the pose or the facing at the end of a placement (PSX
// FUN_801A591C). Sprite_Current +0x4B = 0xFF; with +7 bit 3, +0x2A = bit 4 of
// +7 and Sprite_SetAnimation(+8 of Sprite_Current read again), else
// Sprite_FaceDirection(+8 of the object read the second time).
extern "C" void __cdecl EventObj_Face(void) {
    Sprite_Current[0x4B] = 0xFF;
    unsigned char* const c = Sprite_Current;
    const unsigned char flags = c[7];
    if (flags & 8) {
        c[0x2A] = static_cast<unsigned char>((flags >> 4) & 1);
        SH_CALL(Sprite_SetAnimation)(Sprite_Current[8]);
        return;
    }
    SH_CALL(Sprite_FaceDirection)(c[8]);
}

// original 0x579DB0: the current object's flags byte (PSX FUN_801A5998).
// As the original has it: flags[0] is read again for each test (it is
// stored into the object first); the speed index Field_ActiveMember +0x84 is
// a whole byte into Field_MoveSpeeds' six - 16 is divided by the byte there,
// and a zero there (indices 6, 7, 9..11, 13, and others past the table)
// raises the divide fault the original does (Idiv). Index 0 skips it.
extern "C" void __cdecl EventObj_SetFlags(const unsigned char* flags) {
    Sprite_Current[7] = flags[0];
    if (flags[0] & 1) Sprite_Current[0] |= 0x40;
    if (flags[0] & 4) Field_ActiveMember[0x80] &= 0xDF;
    Sprite_Current[0x48] = (flags[0] & 0x40) ? 0 : 1;
    const unsigned char speed = Field_ActiveMember[0x84];
    if (speed != 0) Sprite_Current[0xA] = static_cast<unsigned char>(Idiv(16, Field_MoveSpeeds[speed]));
}

// original 0x579E30: the current object's reset (PSX FUN_801A5AB0) - its
// sprite fields, then its script context through Field_ActiveMember.
extern "C" void __cdecl EventObj_Reset(void) {
    Sprite_Current[4] = 0;
    Sprite_Current[3] = 0;
    Sprite_Current[0x24] = 0;
    Sprite_Current[0xA] = 0;
    Sprite_Current[9] = 0;
    rv::Current<records::SpriteObject>()->scale_y = 0x10000;
    rv::Current<records::SpriteObject>()->scale_x = 0x10000;
    rv::Current<records::SpriteObject>()->angle[2] = 0;
    rv::Current<records::SpriteObject>()->angle[1] = 0;
    rv::Current<records::SpriteObject>()->angle[0] = 0;
    Field_ActiveMember[0x80] = 0;
    Field_ActiveMember[0x81] = 0;
    Field_ActiveMember[0x82] = 0;
    Field_ActiveMember[0x85] = 0;
    Field_ActiveMember[0x86] = 0xFF;
    Field_ActiveMember[0x87] = 0;
    SetWord(Field_ActiveMember + 0x8A, 0);
    SetWord(Field_ActiveMember + 0x9C, 0);
}

// original 0x579F00 (PSX 0x801A5BC8): AreaMap_Bytes[s16 z * width + s16 x] =
// value (the width the area block's byte 0). eax on the way out is the row
// offset with the value in its low byte (imul; mov al), returned whole.
extern "C" unsigned long __cdecl AreaMap_SetByte(unsigned long x, unsigned long z, unsigned value) {
    const U row = (DwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(AreaMap_Header_addr))) & 0xFFu) * S16(static_cast<U>(z));
    AtMapFieldObjects(S16(static_cast<U>(x)) + DwordMapFieldObjects(AtMapFieldObjects(map_field_objects::at::BytesAt())) + row)[0] = static_cast<unsigned char>(value);
    return (row & 0xFFFFFF00u) | (value & 0xFFu);
}

// original 0x579F30: one op below F0 (PSX FUN_801A5C00). The handler by the
// high nibble - 0 and F both reach EventOp_0x, the jump table's default -
// then the op's length by the high nibble of the byte read AGAIN, after the
// handler.
extern "C" const unsigned char* __cdecl EventScript_Op(const unsigned char* at) {
    event_script::g.handlers[at[0] >> 4](at);
    wide_staging::Placed(at);   // DIV-0122: a site's placement, its start moved out (wide only)
    return at + EventScript_OpLengths[at[0] >> 4];
}

// original 0x57A010: event op 0x and Fx, 17 bytes (PSX FUN_801A5D5C) - the
// jump table's default in EventScript_Op, and called directly by 19 sites
// (three of them chapter code). EventOp_1x's placement (event_script.cpp)
// with object +0x83 = op[0x10]: Sprite_Objects[count] becomes
// Sprite_Current and Field_ActiveMember, the bank word op[1] op[2], its
// fields and its script context's from the op, EventObj_SetFlags(op + 0xD),
// EventObj_Face, the count one on. Nothing when the count is 30 or more; a
// negative count (signed, unchecked) places before the table, as the
// original does. The count is read again after each call, Sprite_Current at
// every use, the op's bytes where the original reads them.
extern "C" void __cdecl EventOp_0x(const unsigned char* op) {
    if (CountScenaSe() >= static_cast<short>(scena_se::at::kObjectCount)) return;
    unsigned char* const o = ObjectScenaSe(CountScenaSe());
    Sprite_Current = o;
    Field_ActiveMember = o;
    scena_se::at::kBank = static_cast<unsigned>(op[1]) << 8 | op[2];
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(scena_se::at::kBank);
    Sprite_Current[0] = 1;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(Coordinate(op[6], op[7])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(ObjectScenaSe(CountScenaSe()))->target_x, rv::Current<records::SpriteObject>()->x);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(Coordinate(op[8], op[9])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(ObjectScenaSe(CountScenaSe()))->target_z, rv::Current<records::SpriteObject>()->z);
    rv::As<records::SpriteObject>(ObjectScenaSe(CountScenaSe()))->best_score = 0;
    const long ground = SH_CALL(AreaMap_Elevation)(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(ground));
    Sprite_Current[1] = op[0xA];
    Sprite_Current[2] = op[0xC];
    Sprite_Current[8] = static_cast<unsigned char>(op[0] & 0xF);
    Sprite_Current[6] = static_cast<unsigned char>(op[0] >> 4);
    unsigned char* const n = ObjectScenaSe(CountScenaSe());
    rv::As<records::SpriteObject>(n)->half_w = op[4];
    rv::As<records::SpriteObject>(n)->half_d = op[5];
    rv::As<records::SpriteObject>(n)->trigger_word = static_cast<unsigned>(op[0xE]) << 8 | op[0xF];
    n[0x83] = op[0x10];
    n[0x84] = op[0xB];
    n[0xA0] = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 0xD);
    if (op[3] & 0x80) Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x20);
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5C] = static_cast<unsigned char>(op[3] & 0xF);
    SH_CALL(EventObj_Face)();
    scena_se::at::kCount = static_cast<unsigned>(scena_se::at::kCount + 1);
}

// original 0x57A1E0: op 1x, 16 bytes - places Sprite_Objects[count] (PSX
// FUN_801A60C8). op[1]:op[2] the animation bank, op[3] bit 7 sets +0 bit
// 0x20 and its low nibble is +0x5C, op[4] / op[5] words +0x98 / +0x9A, op[6..9]
// x and z, op[0xA] +1, op[0xB] the speed index +0x84, op[0xC] +2, op[0xD] the
// flags (EventObj_SetFlags), op[0xE]:op[0xF] word +0x88; op[0]'s nibbles the
// direction +8 and the kind +6.
// As the original has it: nothing past the count's test of 30 checks it -
// a callee that moved the count moves the object's own writes with it.
extern "C" void __cdecl EventOp_1x(const unsigned char* op) { Place(op, false); }

// original 0x57A3A0 (PSX 0x801A6408): event op 8x (12 bytes) - an object
// placed. While the next object index (s16 kScratch +0) is below 0x1E: the
// object 0x7DEE80 + 0xA4 n becomes Sprite_Current and Field_ActiveMember; the
// animation bank (op[1] << 8 | op[2]) to the scratch word +2, read back after
// EventObj_Reset for Sprite_SetAnimationBank; byte +0 = 1; its position x =
// op[5] << 16 (+ 0x8000 when op[4]), z = op[7] << 16 (+ 0x8000 when op[6]), each
// copied to the object n's +0x8C / +0x90 (n read again each time), +0x94 = 0;
// +0x3E the AreaMap_Elevation there; +1 op[8], +2 op[10], +8 op[0] & 0xF, +6
// op[0] >> 4; n's +0x88 = 0xFFFF, +0x84 = op[9], +0xA0 = 0x7F; EventObj_SetFlags(op
// + 11); +0 |= 0x20 when op[3] bit 7; +0x5D, +0x5F, +0x5E = 0; +0x5C = op[3] &
// 0xF; +0x2A = bit 4 of +7; Sprite_SetAnimation(+8); then 0x10 stamped into
// AreaMap_Bytes at the object's cell (words +0x36, +0x3A), and the cells beside
// it that its half-cell position overlaps (+1 in x when word +0x34, in z when
// +0x38, both when both); the index + 1. Sprite_Current is read again for
// every access, as the original reads [0x937F88].
extern "C" void __cdecl EventOp_8x(const unsigned char* op) {
    const U first = kScratchX.w_00;
    if (static_cast<std::int16_t>(first) >= 0x1E) return;
    const U object = ::bof3::Img(Sprite_Objects_addr) + S16(first) * kObjectStride;
    SetDwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Sprite_Current_addr)), object);
    SetDwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Field_ActiveMember_addr)), object);
    kScratchBank = (static_cast<U>(op[1]) << 8) | op[2];
    g.obj_reset();
    g.set_bank(static_cast<std::uint32_t>(kScratchBank));
    rv::As<records::SpriteObject>(Sprite())->flags = 1;
    rv::Store<U>(rv::As<records::SpriteObject>(Sprite())->x, (static_cast<U>(op[5]) << 16) | (op[4] != 0 ? 0x8000u : 0u));
    SetDwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Sprite_Objects_addr) + 0x8c + static_cast<std::uint32_t>(static_cast<std::int16_t>(kScratchX.w_00)) * kObjectStride), static_cast<U>(rv::As<records::SpriteObject>(Sprite())->x));
    rv::Store<U>(rv::As<records::SpriteObject>(Sprite())->z, (static_cast<U>(op[7]) << 16) | (op[6] != 0 ? 0x8000u : 0u));
    {
        records::SpriteObject* const s = rv::As<records::SpriteObject>(Sprite());
        const U n = static_cast<std::uint32_t>(static_cast<std::int16_t>(kScratchX.w_00)) * kObjectStride;
        SetDwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Sprite_Objects_addr) + 0x90 + n), static_cast<U>(s->z));
        SetDwordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Sprite_Objects_addr) + 0x94 + n), 0);
        const long elevation = g.elevation(static_cast<long>(static_cast<U>(s->x)), static_cast<long>(static_cast<U>(s->z)));
        rv::Store<U>(rv::As<records::SpriteObject>(Sprite())->height_whole, static_cast<U>(elevation));
    }
    rv::As<records::SpriteObject>(Sprite())->state = op[8];
    Sprite()[2] = op[0xA];
    rv::As<records::SpriteObject>(Sprite())->direction = static_cast<unsigned char>(op[0] & 0xF);
    rv::As<records::SpriteObject>(Sprite())->type = static_cast<unsigned char>(op[0] >> 4);
    {
        const U n = static_cast<std::uint32_t>(static_cast<std::int16_t>(kScratchX.w_00)) * kObjectStride;
        SetWordMapFieldObjects(AtMapFieldObjects(::bof3::Img(Sprite_Objects_addr) + 0x88 + n), 0xFFFF);
        AtMapFieldObjects(::bof3::Img(Sprite_Objects_addr) + 0x84 + n)[0] = op[9];
        AtMapFieldObjects(::bof3::Img(Sprite_Objects_addr) + 0xa0 + n)[0] = 0x7F;
    }
    g.set_flags(op + 0xB);
    if (op[3] & 0x80) rv::As<records::SpriteObject>(Sprite())->flags = static_cast<unsigned char>(rv::As<records::SpriteObject>(Sprite())->flags | 0x20);
    rv::As<records::SpriteObject>(Sprite())->shade[0] = 0;
    rv::As<records::SpriteObject>(Sprite())->shade[2] = 0;
    rv::As<records::SpriteObject>(Sprite())->shade[1] = 0;
    rv::As<records::SpriteObject>(Sprite())->draw_mode = static_cast<unsigned char>(op[3] & 0xF);
    {
        records::SpriteObject* const s = rv::As<records::SpriteObject>(Sprite());
        s->flip = static_cast<unsigned char>((s->flags2 >> 4) & 1);
    }
    g.set_animation(rv::As<records::SpriteObject>(Sprite())->direction);
    {
        unsigned char* const s = Sprite();
        g.set_byte(static_cast<U>(rv::As<records::SpriteObject>(s)->x_whole), static_cast<U>(rv::As<records::SpriteObject>(s)->z_whole), 0x10);
    }
    unsigned char* s = Sprite();
    if (static_cast<U>(rv::As<records::SpriteObject>(s)->x_frac) != 0) {
        g.set_byte((static_cast<U>(rv::As<records::SpriteObject>(s)->x_whole) + 1u) & 0xFFFFu, static_cast<U>(rv::As<records::SpriteObject>(s)->z_whole), 0x10);
        s = Sprite();
    }
    if (static_cast<U>(rv::As<records::SpriteObject>(s)->z_frac) != 0) {
        g.set_byte(static_cast<U>(rv::As<records::SpriteObject>(s)->x_whole), (static_cast<U>(rv::As<records::SpriteObject>(s)->z_whole) + 1u) & 0xFFFFu, 0x10);
        s = Sprite();
    }
    if (static_cast<U>(rv::As<records::SpriteObject>(s)->x_frac) != 0 && static_cast<U>(rv::As<records::SpriteObject>(s)->z_frac) != 0)
        g.set_byte((static_cast<U>(rv::As<records::SpriteObject>(s)->x_whole) + 1u) & 0xFFFFu, (static_cast<U>(rv::As<records::SpriteObject>(s)->z_whole) + 1u) & 0xFFFFu, 0x10);
    SharedScratch.w_00 = static_cast<std::uint32_t>(SharedScratch.w_00) + 1u;
}

// original 0x57A5E0: op 2x, 19 bytes (PSX FUN_801A67F8) - EventOp_1x, and
// op[0x10] to object +0x83, op[0x11] / op[0x12] to +0x18 / +0x1C as dwords.
extern "C" void __cdecl EventOp_2x(const unsigned char* op) { Place(op, true); }

// original 0x57A7C0: op 3x, 18 bytes (EventScript_Op's handler 3) - EventOp_2x's
// placement with its own layout: op[1]:op[2] the bank, op[3] bit 7 +0 bit 5 and
// low nibble +0x5C, op[4] / op[5] words +0x98 / +0x9A, op[6..9] x and z,
// op[0xA] +1, op[0xB] +0x84, op[0xC] +2, op[0xD] the flags, op[0xE]:op[0xF]
// +0x88, op[0x10] +0x83, op[0x11] the dword +0x70; op[0]'s nibbles +6 / +8.
// As the original has it: nothing past the count's test of 30 checks it.
extern "C" void __cdecl EventOp_3x(const unsigned char* op) {
    if (CountFieldO() >= 30) return;
    Claim();
    SetBank(static_cast<unsigned>(op[1]) << 8 | op[2]);
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(BankFieldO());
    rv::As<records::SpriteObject>(Sc())->flags = 1;
    PlaceXZ(CoordinateFieldO(op[6], op[7]), CoordinateFieldO(op[8], op[9]));
    Ground();
    rv::As<records::SpriteObject>(Sc())->type = op[0] >> 4;
    rv::As<records::SpriteObject>(Sc())->direction = op[0] & 0xF;
    rv::As<records::SpriteObject>(Sc())->state = op[0xA];
    Sc()[2] = op[0xC];
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Sc())->reach, op[0x11]);
    records::SpriteObject* const n = rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()));
    n->half_w = op[4];
    n->half_d = op[5];
    n->trigger_word = static_cast<unsigned>(op[0xE]) << 8 | op[0xF];
    n->area_script = op[0x10];
    n->speed = op[0xB];
    n->handler = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 0xD);
    if (op[3] & 0x80) rv::As<records::SpriteObject>(Sc())->flags |= 0x20;
    Colour(0);
    rv::As<records::SpriteObject>(Sc())->draw_mode = op[3] & 0xF;
    SH_CALL(EventObj_Face)();
    BumpCount();
}

// original 0x57A990: op 4x, 17 bytes (handler 4) - op 3x's layout without
// +0x83 and +0x70: op[0x10] is object +0xA0 (not 0x7F).
extern "C" void __cdecl EventOp_4x(const unsigned char* op) {
    if (CountFieldO() >= 30) return;
    Claim();
    SetBank(static_cast<unsigned>(op[1]) << 8 | op[2]);
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(BankFieldO());
    rv::As<records::SpriteObject>(Sc())->flags = 1;
    PlaceXZ(CoordinateFieldO(op[6], op[7]), CoordinateFieldO(op[8], op[9]));
    Ground();
    rv::As<records::SpriteObject>(Sc())->state = op[0xA];
    Sc()[2] = op[0xC];
    rv::As<records::SpriteObject>(Sc())->direction = op[0] & 0xF;
    rv::As<records::SpriteObject>(Sc())->type = op[0] >> 4;
    records::SpriteObject* const n = rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()));
    n->half_w = op[4];
    n->half_d = op[5];
    n->trigger_word = static_cast<unsigned>(op[0xE]) << 8 | op[0xF];
    n->speed = op[0xB];
    n->handler = op[0x10];
    SH_CALL(EventObj_SetFlags)(op + 0xD);
    if (op[3] & 0x80) rv::As<records::SpriteObject>(Sc())->flags |= 0x20;
    Colour(0);
    rv::As<records::SpriteObject>(Sc())->draw_mode = op[3] & 0xF;
    SH_CALL(EventObj_Face)();
    BumpCount();
}

// original 0x57AB50: op 7x, 17 bytes (handler 7) - op[1]:op[2] the bank,
// op[3] / op[4] words +0x98 / +0x9A, op[5..8] x and z, op[9] +1, op[0xA]
// +0x84, op[0xB] +2, op[0xC] the flags, op[0xD]:op[0xE] +0x88, op[0xF] +0x83,
// op[0x10] +0x9E; +0x5C 2.
extern "C" void __cdecl EventOp_7x(const unsigned char* op) {
    if (CountFieldO() >= 30) return;
    Claim();
    SetBank(static_cast<unsigned>(op[1]) << 8 | op[2]);
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_SetAnimationBank)(BankFieldO());
    rv::As<records::SpriteObject>(Sc())->flags = 1;
    PlaceXZ(CoordinateFieldO(op[5], op[6]), CoordinateFieldO(op[7], op[8]));
    Ground();
    rv::As<records::SpriteObject>(Sc())->state = op[9];
    Sc()[2] = op[0xB];
    rv::As<records::SpriteObject>(Sc())->direction = op[0] & 0xF;
    rv::As<records::SpriteObject>(Sc())->type = op[0] >> 4;
    records::SpriteObject* const n = rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()));
    n->half_w = op[3];
    n->half_d = op[4];
    n->trigger_word = static_cast<unsigned>(op[0xD]) << 8 | op[0xE];
    n->area_script = op[0xF];
    n->speed = op[0xA];
    n->tag = op[0x10];
    n->handler = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 0xC);
    Colour(0);
    rv::As<records::SpriteObject>(Sc())->draw_mode = 2;
    SH_CALL(EventObj_Face)();
    BumpCount();
}

// original 0x57AD10 (PSX 0x801A75A8): op 6x, 16 bytes (handler 6; also the scene code's
// placers - Area141_*, Scena08_SpawnPair, Scena13_SpawnPair*, Scena15's, FC1's
// 0x46A600) - a party member's sprite: Field_MemberSprite(op[1], op[0xF]) in
// place of the bank; op[2] / op[3] +0x98 / +0x9A, op[4..7] x and z, op[8] +1,
// op[9] +0x84, op[0xA] +2, op[0xB] the flags, op[0xC]:op[0xD] +0x88, op[0xE]
// +0x83; +0x5C 2.
extern "C" void __cdecl EventOp_6x(const unsigned char* op) {
    if (CountFieldO() >= 30) return;
    Claim();
    SH_CALL(EventObj_Reset)();
    SH_CALL(Field_MemberSprite)(op[1], op[0xF]);
    rv::As<records::SpriteObject>(Sc())->flags = 1;
    PlaceXZ(CoordinateFieldO(op[4], op[5]), CoordinateFieldO(op[6], op[7]));
    Ground();
    rv::As<records::SpriteObject>(Sc())->state = op[8];
    Sc()[2] = op[0xA];
    rv::As<records::SpriteObject>(Sc())->direction = op[0] & 0xF;
    rv::As<records::SpriteObject>(Sc())->type = op[0] >> 4;
    records::SpriteObject* const n = rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()));
    n->half_w = op[2];
    n->half_d = op[3];
    n->trigger_word = static_cast<unsigned>(op[0xC]) << 8 | op[0xD];
    n->area_script = op[0xE];
    n->speed = op[9];
    n->handler = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 0xB);
    Colour(0);
    rv::As<records::SpriteObject>(Sc())->draw_mode = 2;
    SH_CALL(EventObj_Face)();
    BumpCount();
}

// original 0x57AEC0: op Ax, 14 bytes (handler 10) - EventOp_Bx's placement into
// Sprite_Objects[count]: the descriptor +8 entry op[9] of Game_AreaNumber's
// area through Sprite_InitFromEntry, party record 4 cleared, op[1..4] x and z,
// op[5] +1, op[6] +0x84, op[7] +2, op[8] the flags, op[0xA] +0x83,
// op[0xC]:op[0xD] +0x88; +0x70 the entry's byte 2 (through +0x54); +0x5C 2
// and the colour 0x80; and with +0x84 0 the area block's cell (and the ones
// right and below it, where x / z has a half) set to 0x10.
// As the original has it: the descriptor is Game_AreaNumber's unchecked.
extern "C" void __cdecl EventOp_Ax(const unsigned char* op) {
    if (CountFieldO() >= 30) return;
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    const unsigned char* const entry = PtrAt(descriptor + 8) + op[9] * 8u;
    records::SpriteObject* const o = rv::As<records::SpriteObject>(Claim());
    o->direction = op[0] & 0xF;
    SH_CALL(EventObj_Reset)();
    SH_CALL(Sprite_InitFromEntry)(const_cast<unsigned char*>(entry));
    SH_CALL(PartyRecord_Clear)(4);
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Sc())->x, CoordinateFieldO(op[1], op[2]));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Sc())->z, CoordinateFieldO(op[3], op[4]));
    Ground();
    rv::As<records::SpriteObject>(Sc())->state = op[5];
    Sc()[2] = op[7];
    rv::As<records::SpriteObject>(Sc())->type = op[0] >> 4;
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Sc())->work_20, 0);
    rv::As<records::SpriteObject>(Sc())->work_1c = 0;
    rv::As<records::SpriteObject>(Sc())->work_18 = 0;
    {
        unsigned char* const sc = Sc();
        sc[0x70] = PtrAt(sc + 0x54)[2];
    }
    records::SpriteObject* const n = rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()));
    n->best_score = 0;
    n->target_z = 0;
    n->target_x = 0;
    n->area_script = op[0xA];
    n->speed = op[6];
    n->trigger_word = static_cast<unsigned>(op[0xC]) << 8 | op[0xD];
    n->handler = 0x7F;
    SH_CALL(EventObj_SetFlags)(op + 8);
    Colour(0x80);
    rv::As<records::SpriteObject>(Sc())->draw_mode = 2;
    if (rv::As<records::SpriteObject>(ObjectFieldO(CountFieldO()))->speed == 0) {
        unsigned char* sc = Sc();
        SH_CALL(AreaMap_SetByte)(rv::As<records::SpriteObject>(sc)->x_whole, rv::As<records::SpriteObject>(sc)->z_whole, 0x10);
        sc = Sc();
        if (rv::As<records::SpriteObject>(sc)->x_frac != 0) {
            SH_CALL(AreaMap_SetByte)(static_cast<std::uint16_t>(rv::As<records::SpriteObject>(sc)->x_whole + 1), rv::As<records::SpriteObject>(sc)->z_whole, 0x10);
            sc = Sc();
        }
        if (rv::As<records::SpriteObject>(sc)->z_frac != 0) {
            SH_CALL(AreaMap_SetByte)(rv::As<records::SpriteObject>(sc)->x_whole, static_cast<std::uint16_t>(rv::As<records::SpriteObject>(sc)->z_whole + 1), 0x10);
            sc = Sc();
        }
        if (rv::As<records::SpriteObject>(sc)->x_frac != 0 && rv::As<records::SpriteObject>(sc)->z_frac != 0)
            SH_CALL(AreaMap_SetByte)(static_cast<std::uint16_t>(rv::As<records::SpriteObject>(sc)->x_whole + 1),
                                     static_cast<std::uint16_t>(rv::As<records::SpriteObject>(sc)->z_whole + 1), 0x10);
    }
    BumpCount();
}

void FieldEventScript_Inject() {
    BOF3_INJECT(Area_RunPlacement);
    BOF3_INJECT(EventScript_Run);
    BOF3_INJECT(EventScript_Control);
    BOF3_INJECT(EventScript_If);
    BOF3_INJECT(EventScript_IfNot);
    BOF3_INJECT(EventScript_IfRunStep);
    BOF3_INJECT(EventScript_IfSkipStep);
    BOF3_INJECT(EventScript_SkipControl);
    BOF3_INJECT(EventScript_Switch);
    BOF3_INJECT(EventScript_CaseRun);
    BOF3_INJECT(EventScript_CaseSkip);
    BOF3_INJECT(EventScript_SkipIf);
    BOF3_INJECT(EventScript_SkipSwitch);
    BOF3_INJECT(EventObj_Face);
    BOF3_INJECT(EventObj_SetFlags);
    BOF3_INJECT(EventObj_Reset);
    BOF3_INJECT(AreaMap_SetByte);
    BOF3_INJECT(EventScript_Op);
    BOF3_INJECT(EventOp_0x);
    BOF3_INJECT(EventOp_1x);
    BOF3_INJECT(EventOp_8x);
    BOF3_INJECT(EventOp_2x);
    BOF3_INJECT(EventOp_3x);
    BOF3_INJECT(EventOp_4x);
    BOF3_INJECT(EventOp_7x);
    BOF3_INJECT(EventOp_6x);
    BOF3_INJECT(EventOp_Ax);
}
