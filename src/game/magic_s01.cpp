// One spell overlay compiled into the exe, round nine group S01
// (docs/magic_s01.md): the PSX's MAGIC001.EMI, Magic_Rows rows 1 and 105,
// 0x498FE0..0x499D74. Read one id down (docs/cut-content.md section 2) the
// sibling labels the two rows Nue Stomp and Jump; the names below use those
// labels as hypotheses, and say what the code does.
//
//   - row 1 (NueStomp_*): the caster's record copied into one child (kind 1,
//     parameter 0x49) that plays the acting ENEMY's animations 0xA, 0xB and
//     0xC through the engine's 0x435A70 (an enemy by battle index - 3): it
//     rises, drops onto the source sprite's place, stomps three times (sound
//     0x203, the target flagged 0x10 at the first), flags the target 0x40,
//     fades out its tint and returns to the caster;
//   - row 105 (Jump_*): the caster's animation 8 (BattleActor_SetAnimation),
//     the caster's record copied into one child (kind 1, 0x4F) that rises,
//     lands over the source sprite, hovers while its shadow (a disc of 16
//     gouraud triangles, radius +0xA x 8) grows, turns toward the caster by
//     Math_Ratan2 at the hit (target flagged 0x40), flies back along that
//     heading and fades;
//   - both tasks wait for the child (+0xB), restore the effect CLUT row and
//     clear the owner's 0x40 bit (Magic001_EndWhenChildDone), then the done
//     flag and free (MagicFx_DoneAndFree).
//
// Every call goes through the harness (MH_CALL / MH_AT / Phase), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement, except that a phase past a
// task's table aborts where the original would call through whatever follows
// it (docs/magic_fx_reached.md section 3, the precedent).
#include "game/magic_s01.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;
using magic_harness::Mem;
using magic_harness::Pointer;
using move_script::Long;
using move_script::SetLong;

// kV: now Prim_VertexScratch (symbols.toml).

unsigned char* Sc() { return Sprite_Current; }
unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }
unsigned char* Source() { return Pointer(::bof3::Img(BattleOdds_ActorObject_addr)); }
unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }
void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }
void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }
// A dword added in place, wrapping.
void AddL(unsigned char* a, std::uint32_t v) {
    SetLong(a, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a)) + v));
}

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// The phase handlers of other units the children's tables hold
// (docs/magic_s01.md section 3) are ours now and named in the tables: S04's
// AirRaidImage_FadeOut, S10's SacrificeActor_Play.

template <typename T, typename F> T As(F* f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }

// The children's motion: the vertical speed +0x14 by its step +0x20, the
// height word +0x3E by the speed's low word.
void Rise() {
    unsigned char* s = Sc();
    AddL(s + 0x14, static_cast<std::uint32_t>(rv::As<records::BattleTask>(s)->work_20));
    s = Sc();
    rv::Store<unsigned>(rv::As<records::BattleTask>(s)->height_whole, rv::As<records::BattleTask>(s)->height_whole + static_cast<std::uint16_t>(rv::As<records::BattleTask>(s)->step_y));
}

// At the top of the rise: over the source sprite (+0x34, +0x38), the height
// +0x3C moved by the source's less the owner's.
void OverSource(unsigned char* s) {
    rv::Store(rv::As<records::SpriteHead>(s)->x, rv::As<records::SpriteHead>(Source())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Source())->z);
    const std::uint32_t d = static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Source())->height) - static_cast<std::uint32_t>(rv::As<records::SpriteHead>(Owner())->height);
    AddL(Sc() + 0x3C, d);
}

// The tint +0x5D..+0x5F down by 0x10 a frame until +0x5D is 0x80; then back
// at the owner's position (+0x34..+0x3C), +2 on. `shrink`: the shadow +0xA
// down by 2 while not 0 (Jump's).
void FadeHome(bool shrink) {
    unsigned char* s = Sc();
    const unsigned char tint = s[0x5D];
    if (tint != 0x80) {
        s[0x5D] = static_cast<unsigned char>(tint - 0x10);
        AddB(Sc()[0x5E], 0xF0);
        AddB(Sc()[0x5F], 0xF0);
        s = Sc();
    }
    if (shrink && s[0xA] != 0) {
        s[0xA] = static_cast<unsigned char>(s[0xA] - 2);
        s = Sc();
    }
    if (s[0x5D] != 0x80) return;
    rv::Store(rv::As<records::BattleTask>(s)->x, rv::As<records::SpriteHead>(Owner())->x);
    rv::Store(rv::Current<records::BattleTask>()->z, rv::As<records::SpriteHead>(Owner())->z);
    rv::Store(rv::Current<records::BattleTask>()->height, rv::As<records::SpriteHead>(Owner())->height);
    Inc(Sc()[2]);
}

// One MATRIX block as the original lays it out on its stack, RotTrans
// writing its translation at +0x14 (DIV-0021's padding word at +0x12).
struct Matrix {
    short m[10];
    long t[3];
};
static_assert(sizeof(Matrix) == 0x20, "MATRIX layout");

}  // namespace

#define S01_EXPORT extern "C" __attribute__((disable_tail_calls))

// ===========================================================================
// Row 1 (Nue Stomp read one id down)

// ===========================================================================
// Row 105 (Jump read one id down)

void MagicS01_Inject() {
    if (bof3::WantsShadow("magic_s01")) magic_s01::SelfTest();
}
