#include "game/event_script.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/event_script_callees.h"
#include "game/faerie_ring.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/wide_staging.h"
#include "hook/detour.h"
#include "hook/log.h"

// The event script interpreter and the flag helpers - see docs/event-script.md
// for each function, its PSX twin and the fuzz.
//
// Terms. "The count" is the word at SharedScratch (0x903850; the PSX
// scratchpad's 0x1F800000): the next Sprite_Objects entry a placement fills.
// The word after it (0x903852, PSX 0x1F800002) holds the animation bank a
// placement passes on. Both are general scratch words, used all over the exe
// (2,114 and 712 references) - here they mean only this. "SC" is
// Sprite_Current, read afresh at every use as the original reads [0x937F88]
// (a stand-in may move it); "FAM" is Field_ActiveMember, whose +0x80.. is the
// object's script context.

namespace {

// using event_script::g;   (B6: every use written event_script::g, so the moves copy no using-declaration)

unsigned char* AtEventScript(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

// Data these read that symbols.toml does not name.
// kBlockFlags: now Field_StatusBits (symbols.toml). bit 6: ScriptFlags_Set40 / Clear40 (object_kinds.cpp)

unsigned char* Extra(unsigned n) { return rv::Bytes(&Sprite_ObjectsExtra[n]); }

// The record views (game/record_views.h): rv::Current<T>() reads
// Sprite_Current afresh at each use, as the original does.
// rv::As<records::SpriteObject>(p) views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record: Object(n) is a Sprite_Objects record, Extra(n) a
// Sprite_ObjectsExtra one.

// A placement's x or z: the whole part from `whole`, and the half (0x8000) if
// `half` is non-zero - `neg / sbb / and 0x8000`.
std::uint32_t Coordinate(unsigned char half, unsigned char whole) {
    return (half ? 0x8000u : 0u) | static_cast<std::uint32_t>(whole) << 16;
}

}  // namespace

// --- the interpreter -------------------------------------------------------------

// --- the placements ---------------------------------------------------------------------

// DIVERGENCE DIV-0191 (switch MountainPasses): two mountains the world map
// never lets the party walk over once the way through is open.
//
// Mt. Zublo on the Rhapala map (area 87): the volcano's two doors are the
// place cells (0x37, 0x1D) (from the south road through the link cell
// (0x37, 0x1E), the choice-table link) and (0x36, 0x1B) (its link cell
// (0x35, 0x1B) -> area 195); the Urkan map is reached from (0x36, 0x1B)
// westward over the passage cell (0x34, 0x1B) 0x91, the link (0x33, 0x1B)
// -> area 88 and the stored exit (0x33, 0x1A) 0xAF. Between the doors is
// 0x10 ground, which Field_CellKind refuses, and a step from a link cell
// onto a place cell loads the area (docs/mountain-passes.md). The volcano's
// own road is blocked only while the chapter byte is 6..8 (area 195's
// placement script places a third object at (0x28, 0x3B) for those chapters
// and never after). So from chapter 9 on, four cells east of the hut are
// made road - the drawn walkway: the platform cell (0x38, 0x1D), the strip
// (0x38, 0x1C), (0x38, 0x1B) up its east side and the ledge cell (0x37,
// 0x1B) beside the upper door ((0x38, 0x1E), in front of the platform's
// wall, is ground: the owner's eye), so the route is the south road (0x37,
// 0x1F) -> the lower door's link cell (0x37, 0x1E), as the stairs already
// go -> the platform -> the strip -> (0x37, 0x1B) -> the upper door
// (0x36, 0x1B) -> the link (0x35, 0x1B) -> the passage (0x34, 0x1B) -> the
// link (0x33, 0x1B) -> the stored exit (0x33, 0x1A) -> the Urkan map, and
// the same back. A door reached from a plain cell, or the stored exit
// stepped onto from one, acts on the LAST place any link cell armed (the
// plate looks it up; confirm and the stored exit change area to it), which
// is what faulted the first build and would warp the second; so the step
// hook (event_ops.cpp, MountainPasses_StepArm) arms the right link whenever
// a step targets one of those cells, on both maps.
//
// Mt. Levett on the Yraall map (area 16): the mountain is a band of place
// cells (0x38, 0x38), (0x37, 0x39), (0x38, 0x39), (0x36, 0x3A), (0x37, 0x3A)
// between the link cells (0x39, 0x38) and (0x36, 0x3B), both -> area 70.
// Place to place passes, but area 16's placement script ends with an
// unconditional hidden object at (0x38, 0x39) (op 1x at 0x5E5DCC, flags
// 0x41: in use, not drawn), and an object in the cell ahead refuses the
// step: the "invisible wall" the owner hits. Area 70's own script places
// nothing conditional and no flag of Capcom's records a crossing (the whole
// flag block was watched unchanged through one), so bit 0 of
// MountainPasses_Bits (block +0x1040, ours) is set when the party enters
// area 70 by one door and comes out by the other - arrival cells from the
// links: the north-east link lands at area 70's (0x38, 0x2F), the
// south-west one at (0x16, 0x3A); area 70's exits land on the link cells
// themselves. With the bit set the hidden object is dropped after the
// placement runs, and the band walks.
//
// Both run here, after the placement script and before the descriptor's
// init: 0x594E60 sets Game_AreaNumber, sets the cell plane (0x56F670) and
// then calls Area_RunPlacement, so the plane and the area are current, and
// the pending arrival dwords are the link's. Armed by PatchBytes after the
// self-test (the fuzz compares the placement runner with Capcom's). Nothing
// here under BOF3X_ORIGINAL=MountainPasses: no cell, no object, no bit.
unsigned char g_mountain_passes = 0;
bool g_desert_faerie = false;   // DIV-0204, set by DesertFaerie_Arm

// original 0x57B130 (PSX 0x801A7CA0): op Bx, 14 bytes - places Sprite_ObjectsExtra[op[0xB]]
// (PSX FUN_801A7CA0). op[1..4] x and z, op[5] +1, op[6] the speed index,
// op[7] +2, op[8] the flags, op[9] the descriptor +8 entry, op[0xA] object
// +0x83, op[0xC]:op[0xD] word +0x88; op[0]'s nibbles the direction and the
// kind plus one.
// As the original has it: op[0xB] is not checked against the four extra
// objects (the shipped scripts use 0..3); the descriptor is Game_AreaNumber's
// with no check either; +0x70 takes byte 2 of whatever +0x54 holds after
// Sprite_InitFromEntry (`mov byte ptr [eax + 0x70], dl` at 0x57B245) - the
// PSX stores it as a word (US GAME.EMI#0 0x801A6C98, JP 0x801A7CA0).
//
// DIVERGENCE DIV-0164 (known-defects D341): 1 through
// PatchBytes("SpriteReachWord") in EventScript_Inject, after the self-test.
// The byte is then stored zero-extended over the whole dword +0x70, as the
// PSX does, so +0x71..+0x73 are 0. EventObj_Reset does not clear them.
namespace event_script { std::uint8_t g_reach_word = 0; }

// --- the flags --------------------------------------------------------------------------

// DIV-0191: armed from inject_all.cpp after every module's self-test (the
// placement runner's, the step hook's and the plate copies' fuzzes all
// compare with Capcom's unarmed code).
void MountainPasses_Arm() {
    const std::uint8_t was = 0, is = 1;
    bof3::PatchBytes("MountainPasses", Low32(&g_mountain_passes), &was, &is, 1);
    bof3::Log("DIV-0191    Mt. Zublo (chapter 9 on) and Mt. Levett (once crossed) passable on the world map: %s",
              g_mountain_passes ? "on" : "OFF (BOF3X_ORIGINAL=MountainPasses)");
}

// DIV-0204: BOF3X_DESERT_FAERIE = 0 (the original's map, the default) or 1;
// read after every module's self-test, which compared the placement runner
// without the cell.
void DesertFaerie_Arm() {
    char v[8] = "0";
    const DWORD n = GetEnvironmentVariableA("BOF3X_DESERT_FAERIE", v, sizeof v);
    if (n >= sizeof v || (std::strcmp(v, "0") != 0 && std::strcmp(v, "1") != 0))
        bof3::Fatal("BOF3X_DESERT_FAERIE is \"%s\": 0 or 1", n >= sizeof v ? "(too long)" : v);
    g_desert_faerie = v[0] == '1';
    if (g_desert_faerie && !bof3::IsEnabled(bof3::addr::Area_RunPlacement))
        bof3::Fatal("BOF3X_DESERT_FAERIE=1 needs Area_RunPlacement ours, and it is disabled");
    if (g_desert_faerie) bof3::Log("DIV-0204    a second faerie village entrance on the desert map (BOF3X_DESERT_FAERIE)");
}

void EventScript_Inject() {
    if (bof3::WantsShadow("event_script")) event_script::SelfTest();
    {
        // DIV-0164: after the self-test, which compared Capcom's byte store.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("SpriteReachWord", Low32(&event_script::g_reach_word), &was, &is, 1);
        bof3::Log("DIV-0164    op Bx stores the extra object's reach byte as a whole dword +0x70 (on unless the line above says OFF)");
    }
}
