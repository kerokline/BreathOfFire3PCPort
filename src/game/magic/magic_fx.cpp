// Moved here by tools/move_funcs.py from magic_s30.cpp (docs/source-layout-round.md).
#include "game/magic/magic_fx.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/magic_harness.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/records_gpu.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = magic_harness::at;

}  // namespace

namespace {

using magic_harness::Mem;

using magic_harness::Pointer;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using move_script::Word;

// kV: now Prim_VertexScratch (symbols.toml).
// The dword the sprite children point at a sprite bank while their phase
// runs (0x8E3580 / 0x813580), and put back to 0x8B3580 after.
// kSpriteBank: now BattleE2_AnimSet (symbols.toml).
// MAGIC131's mote pool: 48 records of 0x2C bytes, and the cell holding the
// mote being run (KaiserMote_Pool, KaiserMote_Current).
// kMotePool: now KaiserMote_Pool (symbols.toml).
constexpr std::uint32_t kMoteStride = 0x2C;

// kMoteCurrent: now KaiserMote_Current (symbols.toml).
// The battle bytes MAGIC131 reads beyond the harness's: 0x904AAC (the
// battle's facing, battle region), 0x904B89 (read by the engine as the
// form index, docs/magic_lib.md), 0x90412C (the loaded party set,
// docs/field-event.md), and the table of file-index tables by 0x904B89.
constexpr ::bof3::image::Va kFacing{0x904AAC};

unsigned char* Sc() { return Sprite_Current; }

unsigned char* Owner() { return Pointer(::bof3::Img(BattleE2_Owner_addr)); }

unsigned char TargetByte() { return Mem(::bof3::Img(at::kTarget))[0]; }

void Inc(unsigned char& b) { b = static_cast<unsigned char>(b + 1); }

void Dec(unsigned char& b) { b = static_cast<unsigned char>(b - 1); }

void AddB(unsigned char& b, unsigned v) { b = static_cast<unsigned char>(b + v); }

void AddW(unsigned char* a, unsigned v) { SetWord(a, (Word(a) + v) & 0xFFFF); }

void AddL(unsigned char* a, std::int32_t v) {
    SetLong(a, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(a)) + static_cast<std::uint32_t>(v)));
}

// A data item's address (symbols.toml's macros are typed pointers).
std::uint32_t Addr(const void* p) { return Low32(p); }

std::int32_t PtrValue(const void* p) {
    return static_cast<std::int32_t>(Low32(p));
}

unsigned char* TaskSlot(unsigned slot) { return Mem(::bof3::Img(BattleTask_Slots_addr) + slot * at::kTaskStride); }

unsigned char* Mote(unsigned i) { return Mem(::bof3::Img(KaiserMote_Pool_addr) + i * kMoteStride); }

unsigned NewTask(unsigned parameter) { return MH_CALL(BattleTask_Create)(1, parameter) & 0xFFu; }

// This group's functions called directly, as the originals call them: by
// address, which in the game is the jmp Inject put there (or Capcom's code
// under BOF3X_ORIGINAL) and in the fuzz that address's recorder.
using Fn0 = void (__cdecl*)();

using AllocFn = unsigned char (__cdecl*)();

void Call0(std::uint32_t address) { MH_WORD(Fn0, address)(); }

[[noreturn]] void PastTable(const char* who, unsigned phase, unsigned entries) {
    bof3::Fatal("%s: phase %u, past the %u-entry table", who, phase, entries);
}

// A .data dispatch table read in place (the fuzz swaps its entries for
// recorders), the index checked against the table's length.
magic_harness::Handler Entry(const char* who, std::uint32_t table, unsigned entries, unsigned index) {
    if (index >= entries) PastTable(who, index, entries);
    return reinterpret_cast<magic_harness::Handler>(
        static_cast<std::uintptr_t>(static_cast<std::uint32_t>(Long(Mem(table + 4 * index)))));
}

// Gfx_ClutStrip / Gfx_ClutStripSource word indices: row 26 (0x812980 /
// 0x80E980), row 1 (0x80F980 / 0x80B980) and row 1 + 0x21 (0x80F9C2).
constexpr unsigned kRow26 = 0x1A00, kRow1 = 0x200;

// The sprite-control fields both sprite children's starts write, in their
// order (no call between: the order is the originals' for reading only).
void SpriteFields(unsigned char c25, unsigned char c27, unsigned char shade, unsigned char c5c) {
    Sc()[0x25] = c25;
    Sc()[0x26] = 0;
    Sc()[0x27] = c27;
    Sc()[0x28] = 1;
    Sc()[0x24] = 0x80;
    rv::Store(rv::Current<records::BattleTask>()->scale_x, 0x10000);
    rv::Store(rv::Current<records::BattleTask>()->scale_y, 0x10000);
    Sc()[0x48] = 0;
    Sc()[0] = static_cast<unsigned char>(Sc()[0] | 0x20);
    Sc()[0x5C] = c5c;
    Sc()[0x5D] = shade;
    Sc()[0x5E] = shade;
    Sc()[0x5F] = shade;
}

}  // namespace

#define S30_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// The facing test the sprite's phases share: 0x904AAC neither 0 nor 3.
bool FacingAcross() {
    const unsigned f = Mem(::bof3::Img(kFacing.v))[0];
    return f != 0 && f != 3;
}

// One step of the sprite's glide: the velocity +0xC / +0x10 by +0x18 / +0x1C,
// the screen point by the velocity (as words).
void Accelerate() {
    AddL(Sc() + 0xC, rv::Current<records::BattleTask>()->work_18);
    AddL(Sc() + 0x10, rv::Current<records::BattleTask>()->work_1c);
}

void Move() {
    AddW(Sc() + 0x2E, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_x));
    AddW(Sc() + 0x30, static_cast<std::uint16_t>(rv::Current<records::BattleTask>()->step_z));
}

void SetVelocity(std::int32_t vx, std::int32_t vy, std::int32_t ax, std::int32_t ay) {
    rv::Current<records::BattleTask>()->step_x = vx;
    rv::Current<records::BattleTask>()->step_z = vy;
    rv::Current<records::BattleTask>()->work_18 = ax;
    rv::Current<records::BattleTask>()->work_1c = ay;
}

}  // namespace

// original 0x4E5200 (a phase of eighteen overlays' tables): once +0xB (the
// children left) is 0, the effect-done bit 2 of 0x904AA8 and the task freed.
S30_EXPORT void __cdecl MagicFx_EndWhenChildrenDone(void) {
    if (Sc()[0xB] != 0) return;
    Mem(::bof3::Img(at::kFlags))[0] = static_cast<unsigned char>(Mem(::bof3::Img(at::kFlags))[0] | 4);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E5220: the children's kind-1 task, a jmp through
// Kaiser_ChildKinds (four entries: sprite, flash, pillar, ring) by +1.
S30_EXPORT void __cdecl KaiserChild_Task(void) {
    Entry("KaiserChild_Task", Addr(Kaiser_ChildKinds), 4, Sc()[1])();
}

// original 0x4E5240: the sprite bank at 0x813580 around its phase (+2
// through KaiserSprite_Phases, eight entries); while +0 and +2 are set,
// Sprite_UpdateScreen, and every fourth frame nine CLUT entries (row 1 from
// 0x21) from the source at row 1 + +4 with the stp bit, +4 up by 9 and back
// to 0x21 past 0x7A.
S30_EXPORT void __cdecl KaiserSprite_Run(void) {
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x813580));
    Entry("KaiserSprite_Run", Addr(KaiserSprite_Phases), 8, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] != 0 && sc[2] != 0) {
        MH_CALL(Sprite_UpdateScreen)();
        if ((Frame_Counter & 3) == 0) {
            unsigned char* const self = Sc();
            for (unsigned i = 0; i < 9; ++i)
                Gfx_ClutStrip[kRow1 + 0x21 + i] =
                    static_cast<unsigned short>(Gfx_ClutStripSource[kRow1 + self[4] + i] | 0x8000);
            Gfx_ClutStripDirty = 1;
            AddB(self[4], 9);
            if (Sc()[4] > 0x7A) Sc()[4] = 0x21;
        }
    }
    SetLong(Mem(::bof3::Img(BattleE2_AnimSet_addr)), ::bof3::Img(0x8B3580));
}

// original 0x4E52E0: +9 down; at 0: by the facing, the start point and
// velocity ((0x103, 0x46), (4, -4), (1, -1), +0x2A 0; or (0x3D, 0x46),
// (-4, -4), (-1, -1), +0x2A 1) run sixteen steps back and the velocity
// negated; the sprite's control fields, shade 0, animation 0; sound 0x101;
// +9 0x10, +2 on.
S30_EXPORT void __cdecl KaiserSprite_Start(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    if (FacingAcross()) {
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, 0x103);
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, 0x46);
        SetVelocity(4, -4, 1, -1);
        Sc()[0x2A] = 0;
    } else {
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_x, 0x3D);
        rv::Store<unsigned>(rv::Current<records::BattleTask>()->screen_y, 0x46);
        SetVelocity(-4, -4, -1, -1);
        Sc()[0x2A] = 1;
    }
    for (unsigned i = 0; i < 16; ++i) {
        Accelerate();
        Move();
    }
    rv::Store(rv::Current<records::BattleTask>()->step_x, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_x)));
    rv::Store(rv::Current<records::BattleTask>()->step_z, static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(rv::Current<records::BattleTask>()->step_z)));
    SpriteFields(5, 2, 0, 0);
    Sc()[0x29] = 2;
    rv::Store<unsigned>(rv::Current<records::BattleTask>()->column, 0);
    Sc()[0x2B] = 0;
    MH_CALL(Sprite_SetAnimation)(0);
    MH_CALL(Sound_PlayById)(0x101);
    Sc()[9] = 0x10;
    Inc(Sc()[2]);
}

// original 0x4E54D0: a step (move, then accelerate); +9 down; at 0 a new
// velocity by the facing ((-4, 4), (1, -1); or (4, 4), (-1, -1)), +9 6, +2 on.
S30_EXPORT void __cdecl KaiserSprite_Glide(void) {
    Move();
    Accelerate();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    if (FacingAcross())
        SetVelocity(-4, 4, 1, -1);
    else
        SetVelocity(4, 4, -1, -1);
    Sc()[9] = 6;
    Inc(Sc()[2]);
}

// original 0x4E55A0: a step (accelerate, then move); +9 down; at 0 +9 0x24,
// +2 on.
S30_EXPORT void __cdecl KaiserSprite_Brake(void) {
    Accelerate();
    Move();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    Sc()[9] = 0x24;
    Inc(Sc()[2]);
}

// original 0x4E5600: the sprite's script ticked; +9 down; at 0 animation 1,
// +9 0x29, +2 on.
S30_EXPORT void __cdecl KaiserSprite_Tick(void) {
    MH_CALL(Sprite_ScriptTick)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sprite_SetAnimation)(1);
    Sc()[9] = 0x29;
    Inc(Sc()[2]);
}

// original 0x4E5640: the script ticked once; +9 down; at 0: sound 0x100, the
// target flags 0x10; two kind-1 children 0x69 of kinds 2 and 3 (pillar,
// ring) of the owner; eight motes of kind 0 (+7 0..7) and 28 of kind 1 (+7
// 0..0x1B, the delay +0x26 (k < 16 ? k / 2 & 7 : k & 0xF) + 1), each whose
// alloc answers other than 0xFF owned by the owner, its +0xB counting them;
// +2 on.
S30_EXPORT void __cdecl KaiserSprite_Breathe(void) {
    MH_CALL(Sprite_ScriptTickOnce)();
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Sound_PlayById)(0x100);
    MH_CALL(Battle_SetTargetFlags)(TargetByte(), 0x10);
    for (unsigned kind = 2; kind <= 3; ++kind) {
        const unsigned slot = NewTask(0x69);
        unsigned char* const owner = Owner();
        unsigned char* const child = TaskSlot(slot);
        records::BattleTask* const tchild = rv::As<records::BattleTask>(child);
        tchild->owner = PtrValue(owner);
        child[1] = static_cast<unsigned char>(kind);
        Inc(owner[0xB]);
    }
    const auto alloc = [] { return static_cast<unsigned>(MH_AT(AllocFn, bof3::addr::KaiserMote_Alloc)()); };
    for (unsigned k = 0; k < 8; ++k) {
        const unsigned r = alloc() & 0xFF;
        if (r == 0xFF) continue;
        unsigned char* const owner = Owner();
        unsigned char* const m = Mote(r);
        SetLong(m + 0x28, PtrValue(owner));
        m[1] = 0;
        m[2] = 0;
        m[7] = static_cast<unsigned char>(k);
        Inc(owner[0xB]);
    }
    for (unsigned k = 0; k < 0x1C; ++k) {
        const unsigned r = alloc() & 0xFF;
        if (r == 0xFF) continue;
        unsigned char* const owner = Owner();
        unsigned char* const m = Mote(r);
        SetLong(m + 0x28, PtrValue(owner));
        m[1] = 1;
        m[2] = 0;
        m[7] = static_cast<unsigned char>(k);
        const unsigned d = k < 0x10 ? (k >> 1) & 7 : k & 0xF;
        SetWord(m + 0x26, d + 1);
        Inc(owner[0xB]);
    }
    Inc(Sc()[2]);
}

// original 0x4E57C0: the script ticked once; when it reports its end, +9
// 0xF and +2 on.
S30_EXPORT void __cdecl KaiserSprite_WaitScript(void) {
    if ((MH_CALL(Sprite_ScriptTickOnce)() & 0xFF) == 0) return;
    Sc()[9] = 0xF;
    Inc(Sc()[2]);
}

// original 0x4E57E0: +9 down; at 0 the target flag 0x40, a velocity by the
// facing ((-8, -1), (-1, -1); or (8, -1), (1, -1)), +9 0, +0xA 0x1E, +2 on.
S30_EXPORT void __cdecl KaiserSprite_Leave(void) {
    Dec(Sc()[9]);
    if (Sc()[9] != 0) return;
    MH_CALL(Battle_SetTargetFlag40)(TargetByte());
    if (FacingAcross())
        SetVelocity(-8, -1, -1, -1);
    else
        SetVelocity(8, -1, 1, -1);
    Sc()[9] = 0;
    Sc()[0xA] = 0x1E;
    Inc(Sc()[2]);
}

// original 0x4E5890: on odd frames while +9 is below 8, +9 up and the
// y-velocity by its acceleration; the point moved; +0xA down, at 0 the
// owner's count down and the task freed.
S30_EXPORT void __cdecl KaiserSprite_Exit(void) {
    if ((Frame_Counter & 1) != 0 && Sc()[9] < 8) {
        Inc(Sc()[9]);
        AddL(Sc() + 0x10, rv::Current<records::BattleTask>()->work_1c);
    }
    Move();
    Dec(Sc()[0xA]);
    if (Sc()[0xA] != 0) return;
    Dec(Owner()[0xB]);
    MH_CALL(BattleTask_FreeCurrent)();
}

// original 0x4E5900: +2 through KaiserFlash_Phases (five entries: clear,
// brighten, wait for the load, MAGIC078's MagicFx_WaitA, MAGIC060's 0x4B1740);
// while +0 and +2 are set, the flash.
S30_EXPORT void __cdecl KaiserFlash_Run(void) {
    Entry("KaiserFlash_Run", Addr(KaiserFlash_Phases), 5, Sc()[2])();
    const unsigned char* const sc = Sc();
    if (sc[0] == 0 || sc[2] == 0) return;
    Call0(bof3::addr::KaiserFlash_Draw);
}

void MagicFx_Inject() {
    BOF3_INJECT(MagicFx_EndWhenChildrenDone);
    BOF3_INJECT(KaiserChild_Task);
    BOF3_INJECT(KaiserSprite_Run);
    BOF3_INJECT(KaiserSprite_Start);
    BOF3_INJECT(KaiserSprite_Glide);
    BOF3_INJECT(KaiserSprite_Brake);
    BOF3_INJECT(KaiserSprite_Tick);
    BOF3_INJECT(KaiserSprite_Breathe);
    BOF3_INJECT(KaiserSprite_WaitScript);
    BOF3_INJECT(KaiserSprite_Leave);
    BOF3_INJECT(KaiserSprite_Exit);
    BOF3_INJECT(KaiserFlash_Run);
}
