// Round thirteen group E5D (docs/effect_5d.md): the 49 functions of
// analysis/round13_cut.tsv's group E5D and three starts the cut does not list
// (sub-kind 0x17's texture scroll 0x503E50, code no list has that E5C's states
// tail-jump to, and the dispatchers of sub-kinds 0x1C 0x505100 and 0x1D
// 0x505540, EffectKind18_States' entries 28 and 29, catalog rows of another
// part that no group of the round holds), each read with capstone to its last
// instruction. Effect_RunObjects (ours) makes each live record of
// Effect_Objects (20 of 0x80 bytes) Sprite_Current and calls
// Effect_KindHandlers[+5]; kind 0x18 jumps through EffectKind18_States by +1
// to a sub-kind, whose dispatcher here jumps through its own table by +2 (none
// bounded by a compare). What each sub-kind is, as far as the code says:
//
//   0x17 (E5C's)  helpers only: a 4 x 4 patch of textured quads laid on the
//                 ground's elevation, a VRAM frame copy, a 16 x 32 texture
//                 scrolled by the record's point, and the closing state
//   0x18          a column of eight textured quads and a ring of 64 dots that
//                 grow, hold and shrink round the record's point, then release
//   0x19          a fan of sixteen shaded quads turning in the camera's frame,
//                 placed from a two-entry table, drawn when the camera is near
//   0x1B          a timer: two MoveCmd_TestFB calls, then story flag 0x1C
//                 cleared and the record released
//   0x1C, 0x1D    a column of eight shrinking squares rising from a map cell
//                 (0x1D's leaning along its direction), the cell's map byte
//                 set and cleared; 0x1D hurts a party member standing on the
//                 two cells ahead (Field_FloorHurt)
//   0x14, 0x1E    a CLUT strip dimmed once area flag 0x3E is set; then a
//                 checkerboard of shaded squares over one of two map
//                 rectangles, two effect records (kinds 0x6E, 0x6D) spawned
//                 when a flag of the rectangle's is set, fading
//   0x21          a screen-centred spiral of a POLY_G3 and two POLY_G4s
//   0x22          a platform of up to 25 textured pieces over one of four map
//                 rectangles that sinks and rises with a story flag, writing
//                 the map's bytes, corner dwords and heights as it moves
//   0x43          a textured quad that glows while the counter 0x903848 is
//                 0xE, then waits for 0x1C and releases
//   0x52          sixteen rings of sixteen dots round the record's point,
//                 brightening, when the camera is within 20 cells
//   0x68          a field of 256 dots in eight arcs, when the camera is near
//                 (0x2D, 0x23)
//
// Every call goes through the harness (SH_CALL), so the start-up fuzz can
// stand recorders in for ours as for the originals' copies. No divergence:
// each is a faithful replacement. Where the original jumps through a state
// table past its end, indexes one of the image's tables past it by a record
// byte, or divides by a count that can be 0, ours aborts with a message
// (docs/effect_5d.md section 7).
#include "game/effect_5d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_5d_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_5d::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

unsigned char* S() { return Sprite_Current; }
U ULEffect5d(const unsigned char* p) { return static_cast<U>(Long(p)); }
U ULEffect5d(U a) { return ULEffect5d(At(a)); }
void SetUL(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(v)); }
U W(const unsigned char* p) { return Word(p); }
U W(U a) { return Word(At(a)); }
unsigned char& B(U a) { return *At(a); }
std::int32_t I(U v) { return static_cast<std::int32_t>(v); }
std::int32_t S16(U v) { return static_cast<std::int16_t>(v & 0xFFFFu); }
std::int32_t S8(unsigned char v) { return static_cast<signed char>(v); }
U Abs(std::int32_t v) { return static_cast<U>(v < 0 ? -v : v); }   // cdq; xor; sub (small values here)
U AddressOf(const void* p) { return Low32(p); }
U FC() { return Frame_Counter; }
float Fl(U a) {
    float f;
    std::memcpy(&f, At(a), sizeof f);
    return f;
}
long double SX() { return Fl(AddressOf(MapView_ScreenXY)); }   // fld / fadd dword [0x903820]
long double SY() { return MapView_ScreenXY[1]; }
void StoreFloat(unsigned char* p, long double v) {
    const float f = static_cast<float>(v);   // fstp dword
    std::memcpy(p, &f, sizeof f);
}

unsigned char* Vertex() { return reinterpret_cast<unsigned char*>(Prim_VertexScratch); }
void SetV(unsigned offset, U v) { SetWord(Vertex() + offset, v); }
const short* Vx(unsigned i) { return reinterpret_cast<const short*>(Vertex() + 8 * i); }
float* Fp(unsigned char* p) { return reinterpret_cast<float*>(p); }
unsigned long* ScreenXY() { return reinterpret_cast<unsigned long*>(MapView_ScreenXY); }

using Handler = scenario_harness::Handler;

// A sub-kind's state table: its address and its own length (symbols.toml [[data]]).
#define E5D_TABLE(name) AddressOf(name), name##_count

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 2]; jmp (or call)
// [table + eax * 4]: the table's `entries` handlers read in place (the fuzz
// swaps the cells for recorders); a Fatal past them, where the original jumps
// through the dword after - the next table or data.
void DispatchEffect5d(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[2];
    if (state >= entries)
        bof3::Fatal("%s: sub-state byte +2 is %u, past the %u entries of 0x%X - the original jumps through the dword "
                    "after (docs/effect_5d.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(ULEffect5d(table + 4 * state)))();
}

// An index into one of the image's tables by a record word or byte, checked:
// past it the original reads the data after (another table or code pointers).
U Index(const char* who, std::int32_t index, unsigned count, U table) {
    if (index < 0 || static_cast<unsigned>(index) >= count)
        bof3::Fatal("%s: index %d, past the %u entries of the table at 0x%X - the original reads the data after "
                    "(docs/effect_5d.md section 7)",
                    who, (int)index, count, (unsigned)table);
    return static_cast<U>(index);
}
// The record's x cell word (+0x36, signed), the index the 0x14 / 0x1E and 0x22
// sub-kinds' tables take.
U Place(const char* who, unsigned count, U table) { return Index(who, S16(W(S() + 0x36)), count, table); }

}  // namespace

// ===========================================================================
// Sub-kind 0x17's helpers (EffectKind18_States[23], E5C's dispatcher 0x503660
// and table 0x65E1A8; its states call these raw until this group merges)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x18: EffectKind18_States[24] (0x6540CC), EffectKind18Sub18_States
// (four) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x19: EffectKind18_States[25] (0x6540D0), one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x68: EffectKind18_States[104] (0x65420C), one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x1B: EffectKind18_States[27] (0x6540D8), one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x43: EffectKind18_States[67] (0x654178), EffectKind18Sub43_States
// (nine by +2; entries 4..8 are sub-kind 0x1C's table)
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x1C and 0x1D: EffectKind18_States[28] / [29] (0x6540DC /
// 0x6540E0), EffectKind18Sub1C_States / EffectKind18Sub1D_States (five each)
// ===========================================================================

// ===========================================================================
// Sub-kinds 0x14 and 0x1E: EffectKind18_States[20] / [30] (0x6540BC /
// 0x6540E4), EffectKind18Sub14_States (five; its entries 2..4 are 0x1E's)
// ===========================================================================

// ===========================================================================
// Sub-kind 0x21: EffectKind18_States[33] (0x6540F0), EffectKind18Sub21_States
// (two, called) by +2
// ===========================================================================

// ===========================================================================
// Sub-kind 0x52: EffectKind18_States[82] (0x6541B4), one state
// ===========================================================================

// ===========================================================================
// Sub-kind 0x22: EffectKind18_States[34] (0x6540F4), EffectKind18Sub22_States
// (five) by +2
// ===========================================================================

void Effect5D_Inject() {
    if (bof3::WantsShadow("effect_5d")) effect_5d::SelfTest();
}
