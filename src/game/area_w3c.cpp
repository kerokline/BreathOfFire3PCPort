// World 3's areas 124..125, 127..128 and 130..134: the PSX's
// BIN/WORLD03/AREA124..134.EMI compiled into the exe at 0x41C890..0x41DAD0
// (Area_Descriptors entries 124..134; areas 126 and 129 name no code). Round
// ten, group AR3C: the band's 56 functions, none ours before, each read to its
// last instruction with capstone (2026-09-28) and taken through the area
// harness (area_harness.h). docs/area_w3c.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept (they stay in
// .data); the band has no dispatch through a .data table of its own (its two
// jump tables are in .text, bounded). Every call goes through the harness
// (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for ours as
// for the originals' copies; the group's own callee (Area131_DisarmTail) is
// called the same way, so each function is fuzzed alone.
#include "game/area_w3c.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3c::at;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }
unsigned char* Focus() { return Ptr(::bof3::Img(AreaW1a_FocusObject_addr)); }
// Field object `index` (Sprite_Objects, 0xA4 bytes); effect record `slot`
// (Effect_Objects, 0x80 bytes); party record `member` (ObjTrio, 0x14C bytes).
unsigned char* ObjectAt(unsigned index) { return rv::Bytes(Sprite_Objects) + index * 0xA4u; }
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(Effect_Objects) + slot * at::kEffectStride; }
unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }
// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

// ---- areas 124 and 125 ----

// Areas 124 and 125's inits (0x41C890, 0x41C960, one body over two tables;
// areas 72 and 73's code compiled again): a weighted draw - Rand() & 0x3F
// walked down the eight weights, the first it falls below chosen (none: 8);
// field objects 0..7 but the chosen one get +0 = 0; the chosen one is put at
// one of the eight cells (Rand() & 7: x and z bytes << 16), its word +0x3E
// the ground there; Field_StepCount = the leader's dword +0x134 less 5 (either
// way).
void PlaceRandomObject(U cells, U weights) {
    auto roll = static_cast<unsigned char>(AH_CALL(Rand)() & 0x3F);
    unsigned chosen = 0;
    for (; chosen < 8; ++chosen) {
        const unsigned char weight = B(weights + chosen);
        if (roll < weight) break;
        roll = static_cast<unsigned char>(roll - weight);
    }
    for (unsigned k = 0; k < 8; ++k)
        if (k != chosen) ObjectAt(k)[0] = 0;
    if (chosen < 8) {
        const unsigned pick = static_cast<unsigned char>(AH_CALL(Rand)() & 7);
        unsigned char* const object = ObjectAt(chosen);
        auto* const tObject = rv::As<records::SpriteObject>(object);
        const auto x = static_cast<std::int32_t>(static_cast<U>(B(cells + pick * 2)) << 16);
        const auto z = static_cast<std::int32_t>(static_cast<U>(B(cells + pick * 2 + 1)) << 16);
        tObject->x = x;
        tObject->z = z;
        const long ground = AH_CALL(AreaMap_Elevation)(tObject->x, z);
        rv::Store<unsigned>(rv::As<records::SpriteObject>(object)->height_whole, static_cast<unsigned>(ground));
    }
    Field_StepCount = static_cast<unsigned short>(Long(Mem(::bof3::Img(ObjTrio_addr) + 0x134)) - 5);
}

// ---- the party-list spawns (areas 130, 133, 134) ----

// Effect_Spawn(kind, 0, args[party list `member`'s byte], party record
// `member`'s words +0x2E and +0x30) with Sprite_Current made that record (and
// left so); an answer other than 0xFF stored to Sprite_Current[0xB], read
// again after the call. The list byte indexes the table unchecked (it stays in
// .data). Area 94's handlers' shape (area_w2c.cpp's SpawnAt).
void SpawnAtMember(unsigned member, unsigned char kind, U args) {
    const unsigned char* const record = PartyAt(member);
    const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_y));
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_x));
    const unsigned char effect = B(args + B(::bof3::Img(Cond_Flags_addr) + 0xd2 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

// An effect record of `kind` at the running object (areas 130 and 134): a slot
// from Effect_FindFree, none (0xFF) nothing; else its +0 = 1, +5 = kind, +1 =
// 0 when `clear1`, and its x, z, y dwords (+0x34, +0x38, +0x3C) the running
// object's, Sprite_Current read again for each.
void EffectAtObject(unsigned char kind, bool clear1) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    auto* const tE = rv::As<records::EffectObject>(e);
    e[0] = 1;
    e[5] = kind;
    if (clear1) e[1] = 0;
    tE->x = rv::Current<records::SpriteHead>()->x;
    tE->z = rv::Current<records::SpriteHead>()->z;
    tE->height = rv::Current<records::SpriteHead>()->height;
}

}  // namespace

// ===========================================================================
// Areas 124 and 125 (descriptors 0x6265C8, 0x626700; PSX 0x801F2DE0, 0x801F37F0)
// ===========================================================================

// ===========================================================================
// Area 127 (descriptor 0x626FE8; PSX 0x801F2D10)
// ===========================================================================

// ===========================================================================
// Area 128 (descriptor 0x627A88; PSX 0x801F3DA8)
// ===========================================================================

// ===========================================================================
// Area 130 (descriptor 0x628C80; PSX 0x801F500C)
// ===========================================================================

// ===========================================================================
// Area 131 (descriptor 0x629910; PSX 0x801F3CB8)
// ===========================================================================

// The talk scene the two choices open on the focus object: its +1 = 4, +0x84
// = 2, +0x83 = 4 / 2, word +0x8A = 0 (the focus pointer read again for each
// store after the first), with counter 0 = 0, MoveScript_Var7 7 and the step.
void FocusScene(unsigned char step, unsigned char b83) {
    unsigned char* const first = Focus();
    B(::bof3::Img(at::kCounter0.v)) = 0;
    B(::bof3::Img(MoveScript_Var7_addr)) = 7;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
    first[1] = 4;
    Focus()[0x84] = 2;
    Focus()[0x83] = b83;
    SetWord(Focus() + 0x8A, 0);
}

// ===========================================================================
// Area 132 (descriptor 0x62A598; PSX 0x801F4A44)
// ===========================================================================

// ===========================================================================
// Area 133 (descriptor 0x62B5C0; PSX 0x801F3DAC)
// ===========================================================================

// ===========================================================================
// Area 134 (descriptor 0x62C120; PSX 0x801F6374)
// ===========================================================================

void AreaW3c_Inject() {
    if (bof3::WantsShadow("area_w3c")) area_w3c::SelfTest();
}
