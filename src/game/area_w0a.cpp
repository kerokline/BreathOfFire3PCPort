// World 0's first area overlays, 0x401000..0x401B80 (less area 11's three,
// area_011.cpp): the PSX's BIN/WORLD00/AREA000..015.EMI compiled into the
// exe, each area's handlers, choice handlers, init and state handlers, read
// to the last instruction with capstone (2026-09-27) and taken through the
// area harness (area_harness.h). docs/area_w0a.md has each area's
// descriptor, roots, block and tables.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Two
// dispatchers index a two-entry state table by an unchecked byte; ours aborts
// past the table where the original would jump into data (the project's rule
// for an index past its table, round9 doc section 6). Every call goes through
// the harness (AH_CALL), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies.
#include "game/area_w0a.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w0a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w0a::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetWord;
using move_script::Word;

std::uint32_t Key(const void* p) { return Low32(p); }

// The cursor row as a table index (movsx) and as a test (a byte).
int CursorRow() { return static_cast<signed char>(Mem(::bof3::Img(AreaW0c_ChoiceAnswer_addr))[0]); }
void SetMessage(unsigned v) { SetWord(Mem(::bof3::Img(at::kMessage.v)), v); }
// A u16 of an area's message table by the s8 cursor row, unchecked as the
// original's (a row outside the table reads its neighbours in the image).
unsigned MessageAt(std::uint32_t table, int row) {
    return Word(Mem(static_cast<std::uint32_t>(static_cast<std::int32_t>(table) + row * 2)));
}

// The index of an object from Sprite_Objects, as the original's magic
// multiply computes it: a signed 32-bit difference divided by 0xA4,
// truncated toward 0.
unsigned char ObjectIndex(const unsigned char* object) {
    const auto d = static_cast<std::int32_t>(Key(object) - ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(d / 0xA4);
}

unsigned char* Effect(std::uint32_t slot_dword) { return Mem(::bof3::Img(Effect_Objects_addr) + (slot_dword & 0xFFu) * at::kEffectStride); }

// The script position of MoveScript_Object (u16 +0xA), moved as the
// original's 16-bit add.
void MoveScriptPos(unsigned delta) {
    unsigned char* const object = MoveScript_Object;
    rv::Store<unsigned>(rv::As<records::ScriptContext>(object)->position, rv::As<records::ScriptContext>(object)->position + delta);
}

void SetCells(const unsigned char (*cells)[3], unsigned n) {
    for (unsigned i = 0; i < n; ++i) AH_CALL(AreaMap_SetByte)(cells[i][0], cells[i][1], cells[i][2]);
}

}  // namespace

// ---- area 0 (descriptor Area0_Descriptor 0x5DB318) ---------------------------

// ---- area 1 (descriptor 0x5DC760) --------------------------------------------

// ---- area 2 (descriptor 0x5DD5C8) --------------------------------------------

// ---- area 3 (descriptor 0x5DDB40) --------------------------------------------

// ---- area 4 (descriptor 0x5DE8A0) --------------------------------------------

// ---- area 5 (descriptor 0x5DF6A0) --------------------------------------------

// ---- area 7 (descriptor 0x5E0378) --------------------------------------------

// ---- area 8 (descriptor 0x5E1078) --------------------------------------------

// ---- area 10 (descriptor 0x5E23A8) -------------------------------------------

// ---- area 12 (descriptor 0x5E2AA8) -------------------------------------------

// ---- area 13 (descriptor 0x5E3FB8) -------------------------------------------

// ---- area 15 (descriptor 0x5E5B80) -------------------------------------------

void AreaW0a_Inject() {
    if (bof3::WantsShadow("area_w0a")) area_w0a::SelfTest();
}
