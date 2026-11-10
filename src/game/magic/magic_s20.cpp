// Three BMAGIC overlays compiled into the exe, spell round group S20
// (docs/magic_s20.md). Each is named by its PSX file, not by a spell: the
// ability names are the sibling's read one id down, a hypothesis.
//
//   - MAGIC087 (Magic_Rows row 48, 0x4C3490..0x4C4486): a task that walks a
//     pool of its own (80 slots of 0x84 at 0x6906D8) and a kind-1 child
//     (parameter 0x27) per living actor on the target's side: a column of
//     textured quads round the actor, then motes that circle it and motes
//     that rise, each a gouraud triangle under the actor's matrix.
//   - MAGIC088 (row 29, 0x4C4490..0x4C4FB1): a seven-step task that picks a
//     variant by the ability id, tints the source sprite dark and back,
//     applies a stat change (0x4FB6F0, the effect library's) and runs two
//     kind-1 children (parameter 0x20): a fan under the actor and a ring of
//     flat quads waving in six bands.
//   - MAGIC092 (row 49, 0x4C5680..0x4C62F6): a task that walks a second pool
//     (48 slots at 0x693018) and a kind-1 child (parameter 0x28) per living
//     actor on the target's side: the target tinted, then three pool motes -
//     a flame of four textured quads a column and sparks (0x4E9850, another
//     overlay's draw).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase / a .data
// table read in place), so the start-up fuzz can stand recorders in for ours
// as for the originals' copies. No divergence: each is a faithful
// replacement, except that a phase past one of the three stack tables aborts
// where the original would call through its own stack (the precedent,
// docs/magic_fx_reached.md section 3). The .data dispatch tables are read in
// place and unchecked, as the originals read them.
#include "game/magic_s20.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "hook/code_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;

// A callee that is ours, called with the argument list the original pushes
// rather than the one its prototype declares.
#define MH_AS(type, name) ::magic_harness::Call(reinterpret_cast<type>(reinterpret_cast<void*>(&name)))

constexpr std::uint32_t kPoolStride = 0x84;

// The library's stat index by MAGIC088's variant (+4): read in place.
// kStatByVariant: now MagicFx_BuffStats (symbols.toml).
// Magic088_Variant's byte table (in .text, after its jump table): the jump
// table's index by the ability id - 0x1E, 0xA3 entries.
constexpr ::bof3::image::CodeVa kVariantIndex{0x4C488C};

// That table as the switch it was compiled from - the function's logic, ours
// since the function is (docs/operand-store.md): by the ability id - 0x1E,
// the ids 0x1E, 0x58 and 0xBE take case 0, 0x1F, 0x59 and 0xBF case 1, 0x5A
// and 0xC0 case 2; every other id of the 0xA3 case 3.
unsigned VariantCase(unsigned k) {
    switch (k) {
    case 0x00: case 0x3A: case 0xA0: return 0;
    case 0x01: case 0x3B: case 0xA1: return 1;
    case 0x3C: case 0xA2: return 2;
    default: return 3;
    }
}

// The effect's scratch (words 0x903850..0x90385E, Scratch_Swap among them)
// and the vertex scratch (Prim_VertexScratch: SVECTORs 8 bytes apart).
constexpr ::bof3::image::Va kScratch{SharedScratch_addr};
// kVertex: now Prim_VertexScratch (symbols.toml).

// The CLUT strip rows the starts write (docs/magic_fx_reached.md: kClutRow).
constexpr std::uint32_t kClutRow = 0x812980;
// kClutRowNext: now Gfx_ClutStrip[0x1a10] (symbols.toml).

unsigned char* Scr(unsigned k) { return Mem(::bof3::Img(kScratch.v) + k); }
unsigned char* Vtx(unsigned k) { return Mem(::bof3::Img(Prim_VertexScratch_addr) + k); }
const short* VtxP(unsigned k) { return reinterpret_cast<const short*>(Vtx(k)); }
short S16(const unsigned char* p) { return static_cast<short>(Word(p)); }
unsigned char* Task(unsigned index) { return Mem(::bof3::Img(BattleTask_Slots_addr) + index * at::kTaskStride); }
unsigned char* Enemy(unsigned i) { return Mem(::bof3::Img(Battle_Enemies_addr) + i * at::kEnemyStride); }
unsigned char* Member(unsigned i) { return Mem(::bof3::Img(ObjTrio_addr) + i * at::kPartyStride); }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
std::int32_t PtrValue(const unsigned char* p) {
    return static_cast<std::int32_t>(Low32(p));
}
void Bump(unsigned char& b, int by) { b = static_cast<unsigned char>(b + by); }

// A .data dispatch table's entry, read as the original reads it.
magic_harness::Handler Entry(std::uint32_t table, unsigned index) {
    return reinterpret_cast<magic_harness::Handler>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// `imul` then `sar`: a 32-bit product that wraps, shifted arithmetically.
int MulSar(int a, int b, int by) {
    return static_cast<int>(static_cast<std::uint32_t>(a) * static_cast<std::uint32_t>(b)) >> by;
}
// A 32-bit add that wraps.
int Add32(int a, int b) { return static_cast<int>(static_cast<std::uint32_t>(a) + static_cast<std::uint32_t>(b)); }

// `fild dword` then `fstp dword`: the integer as a float.
void PutFloat(unsigned char* at, int v) {
    const float f = static_cast<float>(v);
    std::memcpy(at, &f, sizeof f);
}

// Sprite_Current's position from `from` (+0x34 / +0x38 / +0x3C).
void CopyPosition(unsigned char* to, const unsigned char* from) {
    rv::Store(rv::As<records::SpriteHead>(to)->x, rv::As<records::SpriteHead>(from)->x);
    rv::Store(rv::As<records::SpriteHead>(to)->z, rv::As<records::SpriteHead>(from)->z);
    rv::Store(rv::As<records::SpriteHead>(to)->height, rv::As<records::SpriteHead>(from)->height);
}

// MapView_LinkPrimAt at Sprite_Current's position (read now).
void LinkAtCurrent(unsigned size) {
    const unsigned char* const sc = Sprite_Current;
    const records::BattleTask* const tsc = rv::AsConst<records::BattleTask>(sc);
    MH_CALL(MapView_LinkPrimAt)(static_cast<unsigned long>(tsc->x), static_cast<unsigned long>(tsc->z), 2, size);
}

}  // namespace

#define S20_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// MAGIC087 (Magic_Rows row 48)

// ===========================================================================
// MAGIC088 (Magic_Rows row 29)

// ===========================================================================
// MAGIC092 (Magic_Rows row 49)

void MagicS20_Inject() {
    {
        static std::uint8_t index[0xA3];   // VariantCase as the compiler's byte table, for the hosted check
        for (unsigned k = 0; k < sizeof index; ++k) index[k] = static_cast<std::uint8_t>(VariantCase(k));
        bof3::Operand("Magic088_Variant byte table", kVariantIndex.v, index, sizeof index);
    }
    if (bof3::WantsShadow("magic_s20")) magic_s20::SelfTest();
}
