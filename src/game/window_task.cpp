// The window/task layer: Field_RunTaskRecords' per-frame walk of the 22
// window records, the first of its nine handlers (the window kinds 0, 1 and
// 2), the five states each of them runs, the frame, outline and line an open
// window draws, and the record free. With the area-change funnel above it in
// the image - Field_ChangeArea, the pending area's classification, the zone
// lookup and the music the new area wants. docs/window-task.md.
#include "game/window_task.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/window_task_callees.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace window_task {

using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

namespace {

template <typename T> T As(std::uint32_t address) { return reinterpret_cast<T>(static_cast<std::uintptr_t>(address)); }

void PutFloat(unsigned char* at, float v) { std::memcpy(at, &v, sizeof v); }
float Flt(int v) { return static_cast<float>(v); }

// The record the layer is running, re-read from 0x905B84 at every point the
// original re-reads it - after every call, and in most of these functions
// after every store too (the port's compiler never kept it in a register
// across one). Volatile so that ours cannot cache it either.
unsigned char* Rec() {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(
        *reinterpret_cast<volatile std::uint32_t*>(static_cast<std::uintptr_t>(at::kCurrent))));
}

}  // namespace

const Callees kOriginals = {
    Area_ClassifyPending,
    Area_PickMusic,
    Area_ZoneAtIn,
    Window_DrawFrame,
    Window_DrawOutline,
    Window_DrawLine,
    Window_FreeCurrent,
    Flags_Test,
    As<unsigned char(__cdecl*)()>(kMsgBoxFrameTask),
    Gpu_SetDrawMode,
    Gfx_CommitPrim,
    Gpu_SetTile,
    Gpu_SetPolyGT4,
    Gpu_SetLineF4,
    Gpu_SetLineF2,
    Gpu_SetSemiTrans,
    Gpu_GetTPage,
    Gpu_GetClut,
    As<Handler>(kListSetUp),
    As<Handler>(kListDraw),
    As<Handler>(kListCursorDraw),
    As<Handler>(kSetDraw),
    As<Handler>(kSetCursorDraw),
    {As<Handler>(kRecordHandlers[0]), As<Handler>(kRecordHandlers[1]), As<Handler>(kRecordHandlers[2]),
     As<Handler>(kRecordHandlers[3]), As<Handler>(kRecordHandlers[4]), As<Handler>(kRecordHandlers[5]),
     As<Handler>(kRecordHandlers[6]), As<Handler>(kRecordHandlers[7]), As<Handler>(kRecordHandlers[8])},
    {As<Handler>(bof3::addr::Window_Kind0States), As<Handler>(bof3::addr::Window_Kind1States),
     As<Handler>(bof3::addr::Window_Kind2States)},
    {As<Handler>(bof3::addr::Window_Kind0Open), As<Handler>(bof3::addr::Window_Grow),
     As<Handler>(bof3::addr::Window_Kind0Frame), As<Handler>(bof3::addr::Window_Shrink),
     As<Handler>(bof3::addr::Window_Kind0Close)},
    {As<Handler>(bof3::addr::Window_Kind1Open), As<Handler>(bof3::addr::Window_Grow),
     As<Handler>(bof3::addr::Window_Kind1Frame), As<Handler>(bof3::addr::Window_Shrink),
     As<Handler>(bof3::addr::Window_FreeState)},
    {As<Handler>(bof3::addr::Window_Kind2Open), As<Handler>(bof3::addr::Window_Grow),
     As<Handler>(bof3::addr::Window_Kind2Frame), As<Handler>(bof3::addr::Window_Shrink),
     As<Handler>(bof3::addr::Window_FreeState)},
};
Callees g = kOriginals;

}  // namespace window_task

using namespace window_task;

// ===========================================================================
// The area-change funnel (0x594E00..0x595450)
// ===========================================================================

// original 0x594E00: the single funnel every area change goes through (PSX
// Field_ChangeArea 0x801A0A30, the sibling's docs/loader_records/AREA.md
// section 4). Stashes the four arguments, classifies the pending area, picks
// the track the new area wants, and asks the field for mode 5.
//
// As the original has it: the area is stored as a word and read back out of
// memory for Area_PickMusic (not kept in a register); x and z are arithmetic
// shifts of the whole 32-bit argument, so their integer parts reach the zone
// lookup, which then keeps only the low byte of each; the flags argument is
// stored as a byte. Where the PSX keeps all five cells in one block at
// 0x80143F10..1F, the port scattered them.
extern "C" void __cdecl Field_ChangeArea(unsigned area, int x, int z, unsigned flags) {
    SetWord(At(at::kPendingArea), static_cast<std::uint16_t>(area));
    SetLong(At(at::kPendingX), x);
    SetLong(At(at::kPendingZ), z);
    At(at::kPendingFlags)[0] = static_cast<unsigned char>(flags);
    g.classify();
    g.pick_music(static_cast<unsigned>(x >> 16), static_cast<unsigned>(z >> 16), Word(At(at::kPendingArea)));
    Field_Request = 5;
}

// original 0x595350 (PSX 0x801A12F0): 0x937F98 is 0xB when the pending area
// is one of the eleven at 0x66ADC0 - 0x10, 0x21, 0x2D, 0x41, 0x57, 0x58,
// 0x68, 0x73, 0x79, 0x97, 0x98 - and 1 when it is not. What reads it is not
// established (docs/window-task.md section 2).
//
// As the original has it: the "not found" store is under `cmp ecx, 0xB`, a
// test the loop can only leave true - eleven entries, the counter at eleven -
// so it is unconditional in effect. The scan stops at the FIRST match; every
// entry is distinct, so that is unobservable.
extern "C" void __cdecl Area_ClassifyPending(void) {
    const std::uint16_t pending = Word(At(at::kPendingArea));
    unsigned n = 0;
    for (std::uint32_t p = at::kAreaKinds; p < at::kAreaKindsEnd; p += 2, ++n) {
        if (Word(At(p)) == pending) {
            At(at::kPendingKind)[0] = 0xB;
            return;
        }
    }
    if (n == 0xB) At(at::kPendingKind)[0] = 1;
}

// original 0x595390 (PSX 0x801A1350): the zone byte +5 at (x, z) in the
// current area. Its one caller is 0x594E60, the area change's mode-5 step,
// which is still Capcom's.
//
// As the original has it: the area is read as a word with the upper half of
// the register left stale, which the lookup masks away; the result is `al`
// alone - the upper 24 bits of eax are the zone record's address, which no
// caller reads.
extern "C" unsigned char __cdecl Area_ZoneIdAt(unsigned x, unsigned z) {
    return g.zone_at(x, z, Game_AreaNumber)[5];
}

// original 0x5953B0 (PSX 0x801A1388): the track the new area wants, into
// 0x904CD0, which the mode-5 step hands to Music_Play unless it is 0xFF
// (docs/mode-tasks.md section 3).
//
// With bit 0x80 of Field_ScriptFlags' low byte set the current track is kept
// (0x904131 copied over). Otherwise the zone record at (x, z) decides: byte
// +7 zero means byte +6 IS the track; otherwise +6 names one of the 8-byte
// records at 0x669A48 - a pointer to `count` three-byte entries (track, flag
// row, flag bit) and the count - and the track is the first entry whose flag
// is set, or the byte that follows the list when none is.
//
// As the original has it: the count is stashed into the caller's third
// argument slot as a byte and read back as a dword, then masked to 8 bits, so
// the stale bytes above it never reach the loop; there is no bound on the set
// index or on the flag row.
extern "C" void __cdecl Area_PickMusic(unsigned x, unsigned z, unsigned area) {
    if ((Field_ScriptFlags & 0x80u) != 0) {
        At(at::kAreaTrack)[0] = At(at::kMusicTrack)[0];
        return;
    }
    const unsigned char* const zone = g.zone_at(x, z, area);
    if (zone[7] == 0) {
        At(at::kAreaTrack)[0] = zone[6];
        return;
    }
    const std::uint32_t set = at::kMusicSets + static_cast<std::uint32_t>(zone[6]) * 8u;
    const unsigned count = At(set)[4];
    const unsigned char* entry;
    std::memcpy(&entry, At(set), sizeof entry);
    for (unsigned i = 0; i < count; ++i, entry += 3)
        if (g.flags_test(Cond_Flags + entry[1] * 8u, entry[2]) != 0) break;
    At(at::kAreaTrack)[0] = entry[0];
}

// original 0x595450 (PSX 0x801A1498): the first of area `area`'s 8-byte zone
// records whose box [r0, r2] x [r1, r3] holds (x, z) - Area_ZoneAt 0x52FFD0
// with the area as an argument instead of Game_AreaNumber, instruction for
// instruction otherwise.
//
// As the original has it: there is no end test - a list with no record that
// matches loops for ever. x and z are masked to a byte and the area to a
// word; the comparisons are 16-bit signed on zero-extended bytes, the same as
// unsigned.
//
// NOT the PSX Window_Task that symbols.toml guessed at here until 2026-09-22:
// pe_funcs.py had merged 0x595450..0x595C48 into one 0x7F9-byte "function"
// (the fourteen between are all pointer-reached), and the 613 instructions
// that guess rested on are those fourteen.
extern "C" const unsigned char* __cdecl Area_ZoneAtIn(unsigned x_arg, unsigned z_arg, unsigned area) {
    const unsigned x = x_arg & 0xFFu, z = z_arg & 0xFFu;
    const unsigned char* record;
    std::memcpy(&record, At(at::kZoneLists) + (area & 0xFFFFu) * 4u, sizeof record);
    for (;; record += 8) {
        if (x < record[0] || z < record[1] || x > record[2] || z > record[3]) continue;
        return record;
    }
}

// ===========================================================================
// The window records (0x5954B0..0x596150, 0x59E230, 0x59E310)
// ===========================================================================

// original 0x59E230 (PSX 0x8015973C): three passes over the 22 window records
// at 0x803160, starting from the pass byte at 0x802D20. A record with byte 0
// non-zero whose byte +0xF is the running pass becomes the current record
// (0x905B84) and runs handler byte +1 of a nine-entry table the function
// builds on its own stack.
//
// As the original has it: the starting pass is read once into a register, so
// a handler that changes 0x802D20 (Window_Kind0Close does, to 0) only affects
// the NEXT frame; a starting pass above 2 runs nothing; the record walk is a
// register too, so a handler that repoints 0x905B84 does not move it. There
// is no bound on byte +1: index 9 of the original's stack table is its own
// return address. That cannot be reproduced, so ours aborts loudly
// (CLAUDE.md rule 4) - Window_Alloc is the only thing that sets byte +1.
extern "C" void __cdecl Field_RunTaskRecords(void) {
    unsigned pass = At(at::kPass)[0];
    if (pass > 2) return;
    do {
        for (std::uint32_t a = at::kRecords; a < at::kRecordsEnd; a += 0x24) {
            unsigned char* const record = At(a);
            if (record[0] == 0) continue;
            if (record[0xF] != pass) continue;
            SetLong(At(at::kCurrent), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(record)));
            const unsigned handler = record[1];
            if (handler >= 9)
                bof3::Fatal("window record %u names handler %u; the original's stack table holds nine",
                            static_cast<unsigned>((a - at::kRecords) / 0x24), handler);
            g.records[handler]();
        }
        ++pass;
    } while (pass <= 2);
}

// original 0x59E310 (PSX 0x801598DC): frees the current record - byte 0, the
// kind byte +2 and the state byte +3 to zero. Byte +1, the record handler
// Field_RunTaskRecords dispatches on, is left as it was. The pointer is
// re-read for each of the three stores.
extern "C" void __cdecl Window_FreeCurrent(void) {
    Rec()[0] = 0;
    Rec()[2] = 0;
    Rec()[3] = 0;
}

// original 0x5954B0: handler 0 of Field_RunTaskRecords' table - the window
// records proper. Byte +2 picks one of three kinds, through a three-entry
// table built on the stack. No bound: ours aborts loudly (rule 4), since
// index 3 of the original's table is the slot its own `add esp, 0xC` frees.
extern "C" void __cdecl Window_Run(void) {
    const unsigned kind = Rec()[2];
    if (kind >= 3) bof3::Fatal("a window record's kind byte is %u; the original's stack table holds three", kind);
    g.kinds[kind]();
}

namespace {

// The three kind dispatchers are one function three times over: byte +3 of
// the record through a five-entry table built on the stack, the same `sub
// esp, 0x14` / `call [esp + eax * 4]` in each. No bound on the state either.
void RunState(const Handler (&states)[5], const char* which) {
    const unsigned state = Rec()[3];
    if (state >= 5) bof3::Fatal("a %s window's state byte is %u; the original's stack table holds five", which, state);
    states[state]();
}

}  // namespace

// original 0x5954E0: kind 0 - the message box. Its states are the open, the
// grow, the frame, the shrink and the close.
extern "C" void __cdecl Window_Kind0States(void) { RunState(g.kind0, "kind 0"); }

// original 0x595AA0: kind 1 - the choice list drawn at fixed screen
// coordinates. States 1 and 3 are kind 0's grow and shrink.
extern "C" void __cdecl Window_Kind1States(void) { RunState(g.kind1, "kind 1"); }

// original 0x595B40: kind 2 - the list drawn relative to its own window.
// States 1, 3 and 4 are shared as well.
extern "C" void __cdecl Window_Kind2States(void) { RunState(g.kind2, "kind 2"); }

// original 0x595520: kind 0's state 0. Takes the two high bits off the 0x0C
// placement code into the record's flag byte +0xD - bit 7 means "do not draw
// the frame" (flag 1), bit 6 means "the narrow box" (flag 2) - then sets the
// box's half-size, its centre from the placement table, a zero current size,
// and a step of a fifth of the half-size in each axis, so the box opens over
// five frames. Then the state moves on.
//
// As the original has it: the placement byte is written back with the bit
// cleared and read again for each table lookup; the centre is
// (table x + 0x3C) << 4 for the narrow box and (table x + 0x64) << 4 for the
// wide one, and (table y + 0x1B) << 4 for both; the halving is an ordinary
// signed divide by five of the half-size shifted up four. The record pointer
// is re-read before every store.
extern "C" void __cdecl Window_Kind0Open(void) {
    Rec()[0xD] = 0;
    unsigned code = At(at::kPlacement)[0];
    if ((code & 0x80) != 0) {
        At(at::kPlacement)[0] = static_cast<unsigned char>(code & 0x7F);
        Rec()[0xD] |= 1;
    }
    code = At(at::kPlacement)[0];
    unsigned y;
    if ((code & 0x40) != 0) {
        At(at::kPlacement)[0] = static_cast<unsigned char>(code & 0xBF);
        Rec()[0xD] |= 2;
        Rec()[0xA] = 0x78;
        Rec()[0xB] = 0x36;
        SetWord(Rec() + 4, (At(at::kBoxAltXY)[At(at::kPlacement)[0] * 2u] + 0x3Cu) << 4);
        y = At(at::kBoxAltXY)[At(at::kPlacement)[0] * 2u + 1u];
    } else {
        Rec()[0xA] = 0xC8;
        Rec()[0xB] = 0x36;
        SetWord(Rec() + 4, (At(at::kBoxXY)[At(at::kPlacement)[0] * 2u] + 0x64u) << 4);
        y = At(at::kBoxXY)[At(at::kPlacement)[0] * 2u + 1u];
    }
    SetWord(Rec() + 6, (y + 0x1Bu) << 4);
    SetWord(Rec() + 0x10, 0);
    SetWord(Rec() + 0x12, 0);
    unsigned char* r = Rec();
    SetWord(r + 0x14, static_cast<unsigned>((static_cast<int>(r[0xA]) << 4) / 5));
    r = Rec();
    SetWord(r + 0x16, static_cast<unsigned>((static_cast<int>(r[0xB]) << 4) / 5));
    ++Rec()[3];
}

// original 0x595B80: kind 2's state 0. A fixed 0x47 x 0x20 box at the
// placement table's (x + 0x23, y + 0x10) << 4 - the same arithmetic
// MsgBox_PlacementTable's note records - opening from zero at steps of 0xE3
// and 0x66. It touches neither the flag byte +0xD nor the placement code's
// two high bits.
extern "C" void __cdecl Window_Kind2Open(void) {
    SetWord(Rec() + 4, (Word(At(at::kPlaceXY + At(at::kPlacement)[0] * 4u)) + 0x23u) << 4);
    SetWord(Rec() + 6, (Word(At(at::kPlaceXY + At(at::kPlacement)[0] * 4u + 2u)) + 0x10u) << 4);
    SetWord(Rec() + 0x10, 0);
    SetWord(Rec() + 0x12, 0);
    SetWord(Rec() + 0x14, 0xE3);
    SetWord(Rec() + 0x16, 0x66);
    Rec()[0xA] = 0x47;
    Rec()[0xB] = 0x20;
    ++Rec()[3];
}

// original 0x595AE0: kind 1's state 0 - the unread set-up at 0x596330, then
// the state on.
extern "C" void __cdecl Window_Kind1Open(void) {
    g.list_set_up();
    ++Rec()[3];
}

namespace {

// The half-size each axis settles at while the box opens, << 4. Kind 0 takes
// the placement table (narrow or wide by flag 2), kind 1 the record's own
// +0x18 / +0x1A, kind 2 and anything above MsgBox_PlacementTable.
unsigned SettledX(const unsigned char* r) {
    switch (r[2]) {
    case 0:
        return ((r[0xD] & 2) != 0 ? At(at::kBoxAltXY)[At(at::kPlacement)[0] * 2u]
                                  : At(at::kBoxXY)[At(at::kPlacement)[0] * 2u]) << 4;
    case 1:
        return Word(r + 0x18) << 4;
    default:
        return Word(At(at::kPlaceXY + At(at::kPlacement)[0] * 4u)) << 4;
    }
}
unsigned SettledY(const unsigned char* r) {
    switch (r[2]) {
    case 0:
        return ((r[0xD] & 2) != 0 ? At(at::kBoxAltXY)[At(at::kPlacement)[0] * 2u + 1u]
                                  : At(at::kBoxXY)[At(at::kPlacement)[0] * 2u + 1u]) << 4;
    case 1:
        return Word(r + 0x1A) << 4;
    default:
        return Word(At(at::kPlaceXY + At(at::kPlacement)[0] * 4u + 2u)) << 4;
    }
}

// The centre's drift while an axis grows or shrinks: half the step, truncated
// toward zero, added on the shrink and subtracted on the grow, so that the
// box stays centred on its settled position.
int HalfStep(const unsigned char* r, unsigned offset) {
    return static_cast<int>(static_cast<std::int16_t>(Word(r + offset))) / 2;
}

// The four arguments every draw in this family is called with: the centre and
// the current size, each an arithmetic shift of the 12.4 word. Window_DrawFrame
// and Window_DrawOutline keep only the low 16 bits of the first two and the
// low byte of the last two, so what the original leaves in the upper halves
// of the registers it pushes never reaches either.
int RecX(const unsigned char* r) { return static_cast<std::int16_t>(Word(r + 4)) >> 4; }
int RecY(const unsigned char* r) { return static_cast<std::int16_t>(Word(r + 6)) >> 4; }
int RecW(const unsigned char* r) { return static_cast<std::int16_t>(Word(r + 0x10)) >> 4; }
int RecH(const unsigned char* r) { return static_cast<std::int16_t>(Word(r + 0x12)) >> 4; }

}  // namespace

// original 0x595670: state 1 of all three kinds - the box growing. Each axis
// grows by its step and its centre moves back half a step; when the size
// passes the settled half-size the centre snaps to the settled place and the
// size to the settled size. With flag 1 clear the outline is drawn at the
// size this frame reached; when BOTH axes settled this frame the state moves
// on - and then without a draw, since the outline is drawn before the test.
//
// As the original has it: the size is compared as `(s16)size >> 4 > (u8)
// half-size`, a 16-bit arithmetic shift against a zero-extended byte, so a
// size that has gone negative never settles; the "settled" counter is a byte
// that state 1 raises to 1 on the x axis and increments on the y, so a state
// that settles only y counts one, not two.
extern "C" void __cdecl Window_Grow(void) {
    unsigned settled = 0;
    unsigned char* r = Rec();
    SetWord(r + 0x10, Word(r + 0x10) + Word(r + 0x14));
    r = Rec();
    SetWord(r + 4, Word(r + 4) - static_cast<unsigned>(HalfStep(r, 0x14)));
    r = Rec();
    if (RecW(r) > r[0xA]) {
        SetWord(r + 4, SettledX(r));
        r = Rec();
        settled = 1;
        SetWord(r + 0x10, r[0xA] << 4);
        r = Rec();
    }
    SetWord(r + 0x12, Word(r + 0x12) + Word(r + 0x16));
    r = Rec();
    SetWord(r + 6, Word(r + 6) - static_cast<unsigned>(HalfStep(r, 0x16)));
    r = Rec();
    if (RecH(r) > r[0xB]) {
        SetWord(r + 6, SettledY(r));
        r = Rec();
        SetWord(r + 0x12, r[0xB] << 4);
        r = Rec();
        ++settled;
    }
    if ((r[0xD] & 1) == 0) {
        g.draw_outline(RecX(r), RecY(r), RecW(r), RecH(r));
        r = Rec();
    }
    if (settled == 2) ++r[3];
}

// original 0x595860: state 3 of all three kinds - the box shrinking, the
// mirror of the grow. Each axis loses its step and its centre moves forward
// half a step; when the size goes negative the centre snaps to the settled
// place PLUS half the box's own half-size - the box closes onto its own
// middle, not onto its left edge - and the size to zero. When both axes are
// done the state moves on and nothing more happens this frame; otherwise the
// outline is drawn (flag 1 clear) and, for kind 0 alone, the message box's
// frame task runs as a tail jump.
//
// As the original has it: the "gone negative" test is `(s16)(size & 0xFFF0)
// < 0`, and the mask cannot change the sign bit, so it is exactly `size < 0`;
// the half-size is halved as an unsigned byte shift, not the signed halving
// the centre drift uses; kind 1 takes its settled place from the 8-byte
// records at 0x66ADFC by the list set 0x7DEE65, where the grow takes it from
// the record's own +0x18 / +0x1A.
extern "C" void __cdecl Window_Shrink(void) {
    unsigned settled = 0;
    unsigned char* r = Rec();
    SetWord(r + 0x10, Word(r + 0x10) - Word(r + 0x14));
    r = Rec();
    SetWord(r + 4, Word(r + 4) + static_cast<unsigned>(HalfStep(r, 0x14)));
    r = Rec();
    if (static_cast<std::int16_t>(Word(r + 0x10)) < 0) {
        const unsigned half = r[0xA] >> 1;
        unsigned place;
        switch (r[2]) {
        case 0:
            place = ((r[0xD] & 2) != 0 ? At(at::kBoxAltXY)[At(at::kPlacement)[0] * 2u]
                                       : At(at::kBoxXY)[At(at::kPlacement)[0] * 2u]) + half;
            break;
        case 1:
            place = Word(At(at::kSetXY + At(at::kListSet)[0] * 8u)) + half;
            break;
        default:
            place = Word(At(at::kPlaceXY + At(at::kPlacement)[0] * 4u)) + half;
            break;
        }
        SetWord(r + 4, place << 4);
        r = Rec();
        settled = 1;
        SetWord(r + 0x10, 0);
        r = Rec();
    }
    SetWord(r + 0x12, Word(r + 0x12) - Word(r + 0x16));
    r = Rec();
    SetWord(r + 6, Word(r + 6) + static_cast<unsigned>(HalfStep(r, 0x16)));
    r = Rec();
    if (static_cast<std::int16_t>(Word(r + 0x12)) < 0) {
        const unsigned half = r[0xB] >> 1;
        unsigned place;
        switch (r[2]) {
        case 0:
            place = ((r[0xD] & 2) != 0 ? At(at::kBoxAltXY)[At(at::kPlacement)[0] * 2u + 1u]
                                       : At(at::kBoxXY)[At(at::kPlacement)[0] * 2u + 1u]) + half;
            break;
        case 1:
            place = Word(At(at::kSetXY + At(at::kListSet)[0] * 8u + 2u)) + half;
            break;
        default:
            place = Word(At(at::kPlaceXY + At(at::kPlacement)[0] * 4u + 2u)) + half;
            break;
        }
        SetWord(r + 6, place << 4);
        r = Rec();
        ++settled;
        SetWord(r + 0x12, 0);
        r = Rec();
    }
    if (settled == 2) {
        ++r[3];
        return;
    }
    if ((r[0xD] & 1) == 0) {
        g.draw_outline(RecX(r), RecY(r), RecW(r), RecH(r));
        r = Rec();
    }
    if (r[2] != 0) return;
    g.msgbox_frame();
}

// original 0x595820: kind 0's state 2 - the open message box. The frame,
// unless flag 1 is set, then the message box's frame task as a tail jump.
// The task's return value becomes this function's; nothing reads it (its
// caller is the kind dispatcher's `call [esp + eax * 4]`).
extern "C" void __cdecl Window_Kind0Frame(void) {
    unsigned char* const r = Rec();
    if ((r[0xD] & 1) == 0) g.draw_frame(RecX(r), RecY(r), RecW(r), RecH(r));
    g.msgbox_frame();
}

// original 0x595AF0: kind 1's state 2 - the frame unconditionally (flag 1 is
// not consulted here), the unread list draw at 0x596090, then the unread
// cursor draw at 0x596120 as a tail jump.
extern "C" void __cdecl Window_Kind1Frame(void) {
    unsigned char* const r = Rec();
    g.draw_frame(RecX(r), RecY(r), RecW(r), RecH(r));
    g.list_draw();
    g.list_cursor();
}

// original 0x595C10: kind 2's state 2, the same shape with the two
// window-relative draws 0x596020 and 0x5960D0.
extern "C" void __cdecl Window_Kind2Frame(void) {
    unsigned char* const r = Rec();
    g.draw_frame(RecX(r), RecY(r), RecW(r), RecH(r));
    g.set_draw();
    g.set_cursor();
}

// original 0x595A80: kind 0's state 4 - once the message box's frame task
// answers 0, the record pass byte 0x802D20 goes back to 0 and the record is
// freed (a tail jump). While it answers non-zero nothing happens.
//
// As the original has it: the byte stored to 0x802D20 is the task's own
// zero return, so it is always 0 on that path.
extern "C" void __cdecl Window_Kind0Close(void) {
    if (g.msgbox_frame() != 0) return;
    At(at::kPass)[0] = 0;
    g.free_current();
}

// original 0x595B30: kinds 1 and 2's state 4 - the record free alone, as a
// five-byte tail jump.
extern "C" void __cdecl Window_FreeState(void) { g.free_current(); }

// ===========================================================================
// What an open window draws (0x595C50, 0x595F60, 0x596150)
// ===========================================================================

// original 0x595C50 (PSX Window_DrawFrame 0x8015A58C - the function the
// sibling's docs/FURIGANA.md measures a box's width from). Six primitives:
//
//   1. a black TILE over (x, y) of (w + 1, h + 1), semi-transparent, under
//      texture page 0xF;
//   2. an 8-byte primitive holding the words 0, 0xF0, 0x10, 0x10, whose
//      ADDRESS is then passed as the next draw mode's texture window;
//   3. a POLY_GT4 of the same corners, white at every vertex, textured
//      (u, v) (0, 0)..(w, h) from page (0, 0, 0x3C0, 0) and the CLUT at
//      ((s8) 0x903A5A * 32 + 0x10, 0x1E1) - the window colour;
//   4. a second 8-byte primitive, words 0, 0, 0x100, 0x100, likewise the
//      texture window of a draw mode under page 5;
//   5. a LINE_F4 in 0xC8C8C8 round (x - 1, y - 1)..(x + w + 1, y + h + 1) -
//      three of the rectangle's four sides, the fourth drawn as a line below;
//   6. five LINE_F2s: the missing left side in 0xC8, then a 0x8C border two
//      pixels out on each of the four sides.
//
// As the original has it: x and y are the low 16 bits of their arguments
// sign-extended, w and h the low byte alone, so the stale upper halves its
// callers push never reach anything; the vertex floats are `fild` of those
// 32-bit sums stored as singles; the packet cursor is read afresh for each
// draw mode and each primitive; the two 8-byte primitives advance the cursor
// by hand, not through Gfx_CommitPrim.
extern "C" void __cdecl Window_DrawFrame(int x_arg, int y_arg, int w_arg, int h_arg) {
    const int x = static_cast<std::int16_t>(x_arg), y = static_cast<std::int16_t>(y_arg);
    const int w = static_cast<int>(static_cast<unsigned>(w_arg) & 0xFFu);
    const int h = static_cast<int>(static_cast<unsigned>(h_arg) & 0xFFu);

    // 1. the black tile
    g.draw_mode(Gfx_PacketNext, 0, 0, 0xF, 0);
    g.commit(1, 0xC);
    unsigned char* const tile = Gfx_PacketNext;
    g.set_tile(tile);
    const float fx = Flt(x), fy = Flt(y);
    PutFloat(tile + 8, fx);
    PutFloat(tile + 0xC, fy);
    PutFloat(tile + 0x14, Flt(w + 1));
    PutFloat(tile + 0x18, Flt(h + 1));
    tile[4] = 0;
    tile[5] = 0;
    tile[6] = 0;
    g.set_semi(tile, 1);
    g.commit(1, 0x1C);

    // 2. the texture window of the quad's draw mode
    unsigned char* const window1 = Gfx_PacketNext;
    Gfx_PacketNext = window1 + 8;
    SetWord(window1 + 0, 0);
    SetWord(window1 + 2, 0xF0);
    SetWord(window1 + 6, 0x10);
    SetWord(window1 + 4, 0x10);
    g.draw_mode(Gfx_PacketNext, 0, 0, 0xF,
                static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(window1)));
    g.commit(1, 0xC);

    // 3. the window's textured quad
    unsigned char* const quad = Gfx_PacketNext;
    g.set_poly_gt4(quad);
    SetWord(quad + 0x2A, g.get_tpage(0, 0, 0x3C0, 0));
    SetWord(quad + 0x16, g.get_clut(static_cast<int>(static_cast<signed char>(At(at::kClutRow)[0])) * 32 + 0x10, 0x1E1));
    const float fright = Flt(x + w + 1), fbottom = Flt(y + h + 1);
    PutFloat(quad + 8, fx);
    PutFloat(quad + 0xC, fy);
    quad[4] = 0xFF;
    PutFloat(quad + 0x20, fy);
    PutFloat(quad + 0x30, fx);
    quad[5] = 0xFF;
    quad[6] = 0xFF;
    PutFloat(quad + 0x1C, fright);
    quad[0x18] = 0xFF;
    quad[0x19] = 0xFF;
    quad[0x1A] = 0xFF;
    quad[0x2C] = 0xFF;
    quad[0x2D] = 0xFF;
    PutFloat(quad + 0x34, fbottom);
    quad[0x2E] = 0xFF;
    quad[0x40] = 0xFF;
    PutFloat(quad + 0x44, fright);
    quad[0x41] = 0xFF;
    quad[0x42] = 0xFF;
    PutFloat(quad + 0x48, fbottom);
    quad[0x14] = 0;
    quad[0x15] = 0;
    quad[0x28] = static_cast<unsigned char>(w);
    quad[0x29] = 0;
    quad[0x3C] = 0;
    quad[0x3D] = static_cast<unsigned char>(h);
    quad[0x50] = static_cast<unsigned char>(w);
    quad[0x51] = static_cast<unsigned char>(h);
    g.set_semi(quad, 1);
    g.commit(1, 0x54);

    // 4. the texture window of the outline's draw mode
    unsigned char* const window2 = Gfx_PacketNext;
    Gfx_PacketNext = window2 + 8;
    SetWord(window2 + 2, 0);
    SetWord(window2 + 0, 0);
    SetWord(window2 + 6, 0x100);
    SetWord(window2 + 4, 0x100);
    g.draw_mode(Gfx_PacketNext, 0, 0, 5,
                static_cast<unsigned long>(reinterpret_cast<std::uintptr_t>(window2)));
    g.commit(1, 0xC);

    // 5. three sides of the bright outline
    unsigned char* const outline = Gfx_PacketNext;
    g.set_line_f4(outline);
    outline[4] = 0xC8;
    outline[5] = 0xC8;
    outline[6] = 0xC8;
    PutFloat(outline + 8, Flt(x - 1));
    PutFloat(outline + 0xC, Flt(y - 1));
    PutFloat(outline + 0x14, fright);
    PutFloat(outline + 0x18, Flt(y - 1));
    PutFloat(outline + 0x20, fright);
    PutFloat(outline + 0x24, fbottom);
    PutFloat(outline + 0x2C, Flt(x - 1));
    PutFloat(outline + 0x30, fbottom);
    g.commit(1, 0x38);

    // 6. the fourth side, and the dim border two pixels out
    const int right = x + w, bottom = y + h;
    g.draw_line(x - 1, y - 1, x - 1, bottom + 1, 0xC8);
    g.draw_line(x - 2, y, x - 2, bottom, 0x8C);
    g.draw_line(right + 2, y, right + 2, bottom, 0x8C);
    g.draw_line(x, y - 2, right, y - 2, 0x8C);
    g.draw_line(x, bottom + 2, right, bottom + 2, 0x8C);
}

// original 0x595F60 (PSX 0x8015A990): the growing or shrinking box - the
// outline alone, in 0x787878, at the same corners Window_DrawFrame's bright
// outline uses: a LINE_F4 of three sides and one LINE_F2 for the fourth. No
// tile, no quad, no draw mode of its own.
extern "C" void __cdecl Window_DrawOutline(int x_arg, int y_arg, int w_arg, int h_arg) {
    unsigned char* const outline = Gfx_PacketNext;
    g.set_line_f4(outline);
    const int x = static_cast<std::int16_t>(x_arg), y = static_cast<std::int16_t>(y_arg);
    const int w = static_cast<int>(static_cast<unsigned>(w_arg) & 0xFFu);
    const int h = static_cast<int>(static_cast<unsigned>(h_arg) & 0xFFu);
    outline[4] = 0x78;
    outline[5] = 0x78;
    outline[6] = 0x78;
    const float fright = Flt(x + w + 1), fbottom = Flt(y + h + 1);
    PutFloat(outline + 8, Flt(x - 1));
    PutFloat(outline + 0xC, Flt(y - 1));
    PutFloat(outline + 0x14, fright);
    PutFloat(outline + 0x18, Flt(y - 1));
    PutFloat(outline + 0x20, fright);
    PutFloat(outline + 0x24, fbottom);
    PutFloat(outline + 0x2C, Flt(x - 1));
    PutFloat(outline + 0x30, fbottom);
    g.commit(1, 0x38);
    g.draw_line(x - 1, y - 1, x - 1, y + h + 1, 0x78);
}

// original 0x596150 (PSX 0x8015ABA0): one LINE_F2 at the packet cursor, grey
// `colour` in all three channels, its two ends the four arguments taken as
// s16 and converted to singles. Committed to ordering-table slot 1 at 0x20
// bytes.
//
// As the original has it: the colour byte is read before the primitive is
// built, and the four conversions use the caller's fifth argument slot as
// their scratch - which is the slot the colour came out of.
extern "C" void __cdecl Window_DrawLine(int x0, int y0, int x1, int y1, unsigned colour) {
    unsigned char* const line = Gfx_PacketNext;
    g.set_line_f2(line);
    const auto shade = static_cast<unsigned char>(colour);
    line[4] = shade;
    line[5] = shade;
    line[6] = shade;
    PutFloat(line + 8, Flt(static_cast<std::int16_t>(x0)));
    PutFloat(line + 0xC, Flt(static_cast<std::int16_t>(y0)));
    PutFloat(line + 0x14, Flt(static_cast<std::int16_t>(x1)));
    PutFloat(line + 0x18, Flt(static_cast<std::int16_t>(y1)));
    g.commit(1, 0x20);
}

void WindowTask_Inject() {
    if (bof3::WantsShadow("window_task")) window_task::SelfTest();
    BOF3_INJECT(Field_ChangeArea);
    BOF3_INJECT(Area_ClassifyPending);
    BOF3_INJECT(Area_ZoneIdAt);
    BOF3_INJECT(Area_PickMusic);
    BOF3_INJECT(Area_ZoneAtIn);
    BOF3_INJECT(Field_RunTaskRecords);
    BOF3_INJECT(Window_FreeCurrent);
    BOF3_INJECT(Window_Run);
    BOF3_INJECT(Window_Kind0States);
    BOF3_INJECT(Window_Kind1States);
    BOF3_INJECT(Window_Kind2States);
    BOF3_INJECT(Window_Kind0Open);
    BOF3_INJECT(Window_Kind1Open);
    BOF3_INJECT(Window_Kind2Open);
    BOF3_INJECT(Window_Grow);
    BOF3_INJECT(Window_Shrink);
    BOF3_INJECT(Window_Kind0Frame);
    BOF3_INJECT(Window_Kind1Frame);
    BOF3_INJECT(Window_Kind2Frame);
    BOF3_INJECT(Window_Kind0Close);
    BOF3_INJECT(Window_FreeState);
    BOF3_INJECT(Window_DrawFrame);
    BOF3_INJECT(Window_DrawOutline);
    BOF3_INJECT(Window_DrawLine);
}
