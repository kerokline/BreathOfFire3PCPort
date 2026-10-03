// Group E1E of round thirteen (wave one): 48 functions at 0x528CD0..0x52A6B0 -
// analysis/round13_cut.tsv's 48 rows for E1E, each read to its last
// instruction with capstone (2026-09-29) and taken through the scenario
// harness's field mode (scenario_harness.h, Shape::kSprite).
// docs/effect_1e.md has them one row each.
//
// They are not effect-kind states: every one is a step of the leader's state 9
// (Field_LeaderStates[9] = 0x528880, which jumps through LeaderPanel_Stages by
// Sprite_Current +2; each stage's dispatcher jumps through its own steps table
// by +3), run by Field_LeaderFrame with Sprite_Current the leader's ObjTrio
// record. What they drive is Effect_Objects records 0..6 (their state bytes
// +1, +6 and words), the panel draws FE1 took (FieldPanel_*), the music, and
// one Sprite_Objects record chosen by 0x939A1C (docs/effect_1e.md section 1).
//
//   stage 1 (steps 5, 6, 8..10 of 11)  two boxes in and out, a yes / no choice
//   stage 2 (4 steps)                  a start on the confirm button, steering
//   stage 3 (4 steps)                  effect record 0 moved by the pad
//   stage 4 (15 steps)                 a pull by the pad, then the panel's
//                                      header, icon, row, count and total in,
//                                      the take (Inventory_Add), a message,
//                                      a choice and out
//   stages 5..7 (3 steps each)         an item used (Inventory_Remove), waits
//                                      on effect record 4, the leave on a press
//   stage 8 (2 steps)                  the leave: the view back, the effects
//                                      cleared, a transition
//   stage 9 (3 of 5 steps)             a three-way menu on effect record 6
//
// Every one is a faithful replacement but one: LeaderPanel_S4Again's hand row
// under a Latin overlay (DIV-0027, amended 2026-10-03 - g_shop_ask_row). The
// stage dispatchers abort past their tables (the original jumps through the
// dword after, the next table's entry), and a read of the Sprite_Objects
// record 0x939A1C names aborts when the byte is 30 or more (the original reads
// and writes past the 30 records; 0xFF is the value 0x5289C0 leaves) - the
// owner's rule for an unchecked index, round9 doc section 6; docs/effect_1e.md
// section 6. Every call goes through the harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for the callees; the callees of other
// groups of this round and the unowned ones are raw addresses in
// effect_1e_callees.h.
//
// Sprite_Current is re-read after every call, as the originals re-read it, and
// held where the original holds it in a register. Where the original computes
// an argument from a register whose upper half is Sprite_Current's (a `movzx
// ax, byte [eax + 9]` on the pointer), ours computes the same 32-bit value;
// where the upper bytes are a callee's leftovers, the callee reads less than
// the whole word (each read in docs/effect_1e.md section 3) and ours hands on
// what it reads.
#include "game/effect_1e.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/effect_1e_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_1e::at;
using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }
unsigned char& B(U address) { return At(address)[0]; }
std::int32_t L(U address) { return Long(At(address)); }
void SetL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// A register loaded with Sprite_Current, then `movzx ax, byte [reg + n]`: the
// pointer's upper half above the byte.
U PtrHi(const unsigned char* p) { return Key(p) & 0xFFFF0000u; }

// jmp [table + 4 * Sprite_Current +3]: the table's `entries` handlers, read in
// place (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after - the next table's entry
// (docs/effect_1e.md section 6).
void Run(const char* who, const unsigned long* table, unsigned entries) {
    const unsigned step = Sprite_Current[3];
    if (step >= entries)
        bof3::Fatal("%s: step byte +3 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_1e.md section 6)",
                    who, step, entries, (unsigned)Key(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[step]))();
}

// The Sprite_Objects record 0x939A1C names (0x7DEE80 + 0xA4 * n), unchecked in
// the original: past the 30 records ours aborts (docs/effect_1e.md section 6).
unsigned char* Picked(const char* who) {
    const unsigned n = B(at::kSpriteIndex);
    if (n >= at::kSpriteCount)
        bof3::Fatal("%s: the sprite index 0x939A1C is %u, past the %u Sprite_Objects records - the original reads 0x%X "
                    "(docs/effect_1e.md section 6)",
                    who, n, at::kSpriteCount, (unsigned)(Key(Sprite_Objects) + n * at::kSpriteStride));
    return Sprite_Objects + n * at::kSpriteStride;
}
unsigned char PickedKind(const char* who) { return Picked(who)[6]; }

// The two record pointers the stages read through (0x52B250 sets them to image
// tables; read as they are).
unsigned char* RecordA() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(L(at::kRecordA)))); }
unsigned char* RecordB() { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(L(at::kRecordB)))); }

// A message of the script pools: MessagePools + the u16 at `cell`.
const unsigned char* Pool(U cell) { return At(at::kPools + Word(At(cell))); }

// --- the callees ------------------------------------------------------------------------
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void Animate(unsigned animation) { SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(animation)); }
void Tick() { SH_CALL(Sprite_ScriptTick)(); }
void Shade() { SH_CALL(FieldPanel_DrawShade)(); }
void Header(U x) { SH_CALL(FieldPanel_DrawHeader)(static_cast<int>(x), 0x4E); }
void Icon(U x, unsigned kind) { SH_CALL(FieldPanel_DrawKindIcon)(static_cast<int>(x), 0x5B, kind); }
void KindRow(unsigned kind, unsigned count, unsigned row) { SH_CALL(FieldPanel_DrawKindRow)(kind, count, row); }
void Total(U y) { SH_CALL(FieldPanel_DrawTotal)(0xB0, static_cast<int>(y)); }
void Message(U y, unsigned id) { SH_CALL(FieldPanel_DrawMessage)(8, static_cast<int>(y), id); }
void Hand(U x, U y) { SH_CALL(Menu_DrawHand)(static_cast<int>(x), static_cast<int>(y), 0); }
void Text(int x, int y, const unsigned char* text) { SH_CALL(Text_DrawAt)(x, y, 0, 0xFF, text); }
void BoxPrims(U y) { SH_AT(void (__cdecl*)(int, int, int, int, int), at::kBoxPrims)(0x14, static_cast<int>(y), 0x118, 0x13, 0); }
void MusicTo(unsigned track) {
    SH_CALL(Music_FadeOutStop)(0xA);
    SH_CALL(Music_Play)(track, 8);
}
void Effect3Mode(unsigned mode) { SH_AT(void (__cdecl*)(unsigned), at::kEffect3Mode)(mode); }
void PoseSound() { SH_AT(void (__cdecl*)(), at::kPoseSound)(); }

// The stage-4 panel's first three draws, as seven of its steps begin: the
// header at x 0, the picked record's kind icon at 0x80, its row (the kind, the
// count +0x9C, row 0).
void PanelHead(const char* who) {
    Header(0);
    Icon(0x80, PickedKind(who));
    unsigned char* const p = Picked(who);
    KindRow(p[6], p[0x9C], 0);
}

}  // namespace

// ===========================================================================
// Stage 1 (LeaderPanel_Stage1Steps 0x66022C, dispatched by 0x5289A0, not ours)
// ===========================================================================

// original 0x528CD0 (PSX twin 0x801DA068): LeaderPanel_Stage1Steps[5] - box 3
// at (0x6C, 0x56); up / down (0x5000) toggle +6 with sound 0x100; 0x60
// pressed: 0x40 sets +6 (sound 0x106), else sound 0x103, +3 up; the hand by
// +6, the shade, the prompt box and its message.
extern "C" void __cdecl LeaderPanel_S1Choose(void) {
    SH_CALL(FieldPanel_DrawBox3)(0x6C, 0x56);
    unsigned pressed = Input_Pressed;
    if (pressed & 0x5000) {
        Sound(0x100);
        unsigned char* const s = S();
        s[6] = static_cast<unsigned char>(s[6] ^ 1);
        pressed = Input_Pressed;
    }
    if (pressed & 0x60) {
        if (pressed & 0x40) {
            Sound(0x106);
            S()[6] = 1;
        } else {
            Sound(0x103);
        }
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] + 1);
    }
    Hand(0x70, 0x82u - (static_cast<U>(S()[6]) << 4));
    Shade();
    BoxPrims(0x12);
    Text(0x1D, 0x15, Pool(at::kPoolWord00));
}

// original 0x528DA0: LeaderPanel_Stage1Steps[6] - +9 up, box 3 slid left by
// 0x50 a count; at 5 +9 = 0 and, +6 clear, Game_Step up, else effect record
// 1's +1 up and +3 = 0; the shade; while +9, the prompt box and message 8
// lower a count.
extern "C" void __cdecl LeaderPanel_S1Out(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    SH_CALL(FieldPanel_DrawBox3)(static_cast<int>(0x6Cu - 0x50u * (PtrHi(s) | s[9])), 0x56);
    s = S();
    if (s[9] == 5) {
        s[9] = 0;
        s = S();
        if (s[6] == 0) {
            Game_Step = static_cast<unsigned short>(Game_Step + 1);
        } else {
            B(at::kEff1State) = static_cast<unsigned char>(B(at::kEff1State) + 1);
            s[3] = 0;
        }
    }
    Shade();
    const unsigned count = S()[9];
    if (count == 0) return;
    BoxPrims(0x12u - (count << 3));
    Text(0x1D, static_cast<int>(0x15u - (static_cast<U>(S()[9]) << 3)), Pool(at::kPoolWord00));
}

// original 0x528E70: LeaderPanel_Stage1Steps[8] - +9 down, box 2 at 0x44 +
// 0x50 a count; at 0 +6 = 1 and +3 up; the shade.
extern "C" void __cdecl LeaderPanel_S1Box2In(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    SH_CALL(FieldPanel_DrawBox2)(static_cast<int>(0x50u * (PtrHi(s) | s[9]) + 0x44u), 0x4E);
    s = S();
    if (s[9] == 0) {
        s[6] = 1;
        s = S();
        s[3] = static_cast<unsigned char>(s[3] + 1);
    }
    Shade();
}

// original 0x528EC0: LeaderPanel_Stage1Steps[9] - box 2 at (0x44, 0x4E); a
// confirm or cancel button pressed: +3 up; the shade.
extern "C" void __cdecl LeaderPanel_S1Box2Wait(void) {
    SH_CALL(FieldPanel_DrawBox2)(0x44, 0x4E);
    if (Input_Pressed & (Field_CancelButtons | Field_ConfirmButtons)) {
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] + 1);
    }
    Shade();
}

// original 0x528EF0: LeaderPanel_Stage1Steps[10] - +9 up, box 2 at 0x44 less
// 0x50 a count; at 4 sound 0x102, +6 = 1, +3 = 4; the shade.
extern "C" void __cdecl LeaderPanel_S1Box2Out(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    SH_CALL(FieldPanel_DrawBox2)(static_cast<int>(0x44u - 0x50u * (PtrHi(s) | s[9])), 0x4E);
    if (S()[9] == 4) {
        Sound(0x102);
        S()[6] = 1;
        S()[3] = 4;
    }
    Shade();
}

// ===========================================================================
// Stage 2
// ===========================================================================

// original 0x528F50: LeaderPanel_Stages[2] - jmp through LeaderPanel_Stage2Steps by +3.
extern "C" void __cdecl LeaderPanel_S2(void) {
    Run("LeaderPanel_S2", LeaderPanel_Stage2Steps, LeaderPanel_Stage2Steps_count);
}

// original 0x528F70: LeaderPanel_Stage2Steps[0] - effect record 2's +1 up,
// animation 1, +3 = 1.
extern "C" void __cdecl LeaderPanel_S2Begin(void) {
    B(at::kEff2State) = static_cast<unsigned char>(B(at::kEff2State) + 1);
    Animate(1);
    S()[3] = 1;
}

// original 0x528FA0: LeaderPanel_Stage2Steps[1] - confirm: record 2's word
// +0x18 = 0, sound 0x200, the poses 0xFF, animation 2, +0x38 = 0x410000, +9 =
// 0, +3 = 2; else cancel with effect record 3 idle: record 2's +1 = 0, +2 = 1,
// +3 = 0; then Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S2Wait(void) {
    const unsigned pressed = Input_Pressed;
    if (Field_ConfirmButtons & pressed) {
        SetL(at::kEff2Word, 0);
        Sound(0x200);
        B(at::kPoseWas) = 0xFF;
        B(at::kPose) = 0xFF;
        Animate(2);
        SetLong(S() + 0x38, 0x410000);
        S()[9] = 0;
        S()[3] = 2;
    } else if ((Field_CancelButtons & pressed) && B(at::kEff3State) == 0) {
        unsigned char* const s = S();
        B(at::kEff2State) = 0;
        s[2] = 1;
        S()[3] = 0;
    }
    Tick();
}

// original 0x529030: LeaderPanel_Stage2Steps[2] - Sprite_ScriptTick; at frame
// word +0x58 of 6, effect record 0's +1 up, +0xB = +7 = 0, +3 = 3.
extern "C" void __cdecl LeaderPanel_S2Anim(void) {
    Tick();
    unsigned char* const s = S();
    if (Word(s + 0x58) != 6) return;
    B(at::kEff0State) = static_cast<unsigned char>(B(at::kEff0State) + 1);
    s[0xB] = 0;
    S()[7] = 0;
    S()[3] = 3;
}

// original 0x529070: LeaderPanel_Stage2Steps[3] - effect record 0 at +1 3:
// +2 = 3, +3 = 0; else its step +0x10 lowered by 0x4000 on 0x20 while above
// 0x110000 (its +0x14 clear), and unless +0xB: the first 0xA000 held picks
// the step +0xC (0x800 or -0x800 by 0x8000) and sets +7; with +7, the button
// let go or record 0's x out of (0x60000, 0x150000) ends it (+0xC 0, +0xB 1).
// Then Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S2Steer(void) {
    if (B(at::kEff0State) == 3) {
        S()[2] = 3;
        S()[3] = 0;
        Tick();
        return;
    }
    if (L(at::kEff0StepZ) == 0 && (Input_Pressed & 0x20)) {
        const std::int32_t y = L(at::kEff0StepY);
        if (y > 0x110000) SetL(at::kEff0StepY, static_cast<U>(y) - 0x4000u);
    }
    unsigned char* const s = S();
    if (s[0xB] == 0) {
        if (s[7] == 0) {
            const unsigned held = Input_Held;
            if (held & 0xA000) {
                SetL(Key(s + 0xC), held & 0xA000u);
                unsigned char* const t = S();
                SetL(at::kEff0StepX, static_cast<U>(L(Key(t + 0xC))) != 0x8000u ? 0x800u : 0xFFFFF800u);
                t[7] = 1;
            }
        } else {
            const std::int32_t x = L(at::kEff0X);
            if ((Input_Held & static_cast<U>(L(Key(s + 0xC)))) == 0 || x <= 0x60000 || x >= 0x150000) {
                SetL(at::kEff0StepX, 0);
                s[0xB] = 1;
            }
        }
    }
    Tick();
}

// ===========================================================================
// Stage 3
// ===========================================================================

// original 0x529150: LeaderPanel_Stages[3] - jmp through LeaderPanel_Stage3Steps by +3.
extern "C" void __cdecl LeaderPanel_S3(void) {
    Run("LeaderPanel_S3", LeaderPanel_Stage3Steps, LeaderPanel_Stage3Steps_count);
}

// original 0x529170: LeaderPanel_Stage3Steps[0] - effect record 1's +2 and
// record 4's +1 up, record 4's +6 = 1, +6 = 1, 0x6BC70C = 0, +7 = +0xB = 0,
// animation 3, +3 up.
extern "C" void __cdecl LeaderPanel_S3Begin(void) {
    const auto sub = static_cast<unsigned char>(B(at::kEff1Sub) + 1);
    B(at::kEff4State) = static_cast<unsigned char>(B(at::kEff4State) + 1);
    unsigned char* const s = S();
    B(at::kEff1Sub) = sub;
    B(at::kEff4Mode) = 1;
    s[6] = 1;
    unsigned char* const t = S();
    SetL(at::kCounter, 0);
    t[7] = 0;
    S()[0xB] = 0;
    Animate(3);
    unsigned char* const u = S();
    u[3] = static_cast<unsigned char>(u[3] + 1);
}

// original 0x5291D0: LeaderPanel_Stage3Steps[1] - effect record 0 moved by
// the pad (docs/effect_1e.md section 1): +6 with record 4 at 3 hands mode 1
// to record 3; 0x40 with record 4 idle leaves (+3 up, records 2 and 0 idle,
// record 3's +1 = 7); record 0's z at 0x3E8000 or more ends the stage (+2 =
// 1); on the ground a chance by Rand against the B record's +0x11 every 16th
// frame goes to stage 5; else the ground height kept, 0x52B2E0, 0x52B370, the
// pose by the pad and 0x903850, 0x52B1B0, the camera's z clamped to
// 0x150000..0x3C0000 by record 0's, the step x stopped at the edges, and
// Sprite_ScriptTick while the step +0x10 is above 0.
extern "C" void __cdecl LeaderPanel_S3Run(void) {
    unsigned char* const s = S();
    if (s[6] != 0 && B(at::kEff4State) == 3) {
        s[6] = 0;
        Effect3Mode(1);
    }
    if ((Input_Pressed & 0x40) && B(at::kEff4State) == 0) {
        Sound(0x203);
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
        B(at::kEff2State) = 0;
        B(at::kEff0State) = 0;
        B(at::kEff3State) = 7;
        return;
    }
    const std::int32_t z = L(at::kEff0Z);
    if (z >= 0x3E8000) {
        Sound(0x203);
        unsigned char* const t = S();
        B(at::kEff2State) = 0;
        B(at::kEff0State) = 0;
        t[2] = 1;
        S()[3] = 0;
        return;
    }
    bool pose = true;
    const auto ground = static_cast<short>(SH_CALL(AreaMap_Elevation)(L(at::kEff0X), z));
    if (static_cast<short>(Word(At(at::kEff0Height))) <= ground) {
        const unsigned r = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
        if (static_cast<signed char>(RecordB()[0x11]) > static_cast<signed char>(r) && (Frame_Counter & 0xF) == 0) {
            const std::int32_t z2 = L(at::kEff0Z);
            const std::int32_t camera = Field_Kind2Z;
            SetL(at::kEff0StepX, 0);
            SetL(at::kEff0StepY, 0);
            SetL(at::kEff0StepZ, 0);
            if (camera < z2) Field_Kind2Z = z2;
            unsigned char* const t = S();
            MoveScript_F3Divisor = 0x10;
            t[2] = 5;
            S()[3] = 0;
            return;
        }
        const long h = SH_CALL(AreaMap_Elevation)(L(at::kEff0X), L(at::kEff0Z));
        SetWord(At(at::kEff0Height), static_cast<unsigned>(h));
    }
    SH_AT(void (__cdecl*)(), at::kHoldTest)();
    SH_AT(void (__cdecl*)(), at::kEffectsStep)();
    B(at::kPoseWas) = B(at::kPose);
    if (B(at::kEff0Hold) != 0) pose = false;
    if (pose) {
        const unsigned held = Input_Held;
        unsigned animation;
        if (held & 0x8000) {
            unsigned char* const r = RecordB();
            B(at::kPose) = 1;
            SetL(at::kEff0StepX, 0xFFFFF000u);
            SetL(at::kEff0StepY, 0x1000);
            SetL(at::kEff0StepZ, static_cast<U>(Long(r + 8)));
            animation = 6;
        } else if (held & 0x2000) {
            B(at::kPose) = 1;
            SetL(at::kEff0StepX, 0x1000);
            SetL(at::kEff0StepY, 0x1000);
            SetL(at::kEff0StepZ, static_cast<U>(Long(RecordB() + 8)));
            animation = 5;
        } else if (held & 0x20) {
            const unsigned char sloped = B(0x903850);
            SetL(at::kEff0StepX, 0);
            if (sloped != 0) {
                unsigned char* const r = RecordB();
                B(at::kPose) = 2;
                SetL(at::kEff0StepY, 0x2000);
                SetL(at::kEff0StepZ, static_cast<U>(Long(r + 8)));
                animation = 4;
            } else {
                unsigned char* const r = RecordB();
                B(at::kPose) = 1;
                SetL(at::kEff0StepY, 0x1000);
                SetL(at::kEff0StepZ, static_cast<U>(Long(r + 4)));
                animation = 3;
            }
        } else if (B(0x903850) != 0) {
            unsigned char* const r = RecordB();
            B(at::kPose) = 2;
            SetL(at::kEff0StepX, 0);
            SetL(at::kEff0StepY, 0x1000);
            SetL(at::kEff0StepZ, static_cast<U>(Long(r + 8)));
            animation = 4;
        } else {
            unsigned char* const r = RecordB();
            B(at::kPose) = 0;
            SetL(at::kEff0StepX, 0);
            SetL(at::kEff0StepY, 0);
            SetL(at::kEff0StepZ, static_cast<U>(Long(r)));
            animation = 3;
        }
        Animate(animation);
    }
    PoseSound();
    const std::int32_t step = L(at::kEff0StepY);
    const std::int32_t z3 = L(at::kEff0Z);
    Field_Kind2Z = z3;
    MoveScript_F3Divisor = static_cast<short>(step != 0 ? step >> 8 : 0x10);
    if (z3 < 0x150000)
        Field_Kind2Z = 0x150000;
    else if (z3 > 0x3C0000)
        Field_Kind2Z = 0x3C0000;
    const std::int32_t x = L(at::kEff0X);
    const std::int32_t dx = L(at::kEff0StepX);
    if ((x <= 0x80000 && dx < 0) || (x >= 0x120000 && dx > 0)) SetL(at::kEff0StepX, 0);
    if (step > 0) Tick();
}

// original 0x5294D0: LeaderPanel_Stage3Steps[2] - unless Field_Kind2Hold, the
// divisor 0x40, the camera's z 0x3C0000, +3 up.
extern "C" void __cdecl LeaderPanel_S3Hold(void) {
    if (Field_Kind2Hold != 0) return;
    unsigned char* const s = S();
    MoveScript_F3Divisor = 0x40;
    Field_Kind2Z = 0x3C0000;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x529500: LeaderPanel_Stage3Steps[3] - unless Field_Kind2Hold or
// effect record 3 busy: +2 = 1, +3 = 0.
extern "C" void __cdecl LeaderPanel_S3End(void) {
    if (Field_Kind2Hold != 0 || B(at::kEff3State) != 0) return;
    S()[2] = 1;
    S()[3] = 0;
}

// ===========================================================================
// Stage 4
// ===========================================================================

// original 0x529530: LeaderPanel_Stages[4] - jmp through LeaderPanel_Stage4Steps by +3.
extern "C" void __cdecl LeaderPanel_S4(void) {
    Run("LeaderPanel_S4", LeaderPanel_Stage4Steps, LeaderPanel_Stage4Steps_count);
}

// original 0x529550: LeaderPanel_Stage4Steps[0] - effect record 4's +1 up,
// record 2's 0, record 4's +6 = 2, the camera's z the picked record's +0x38,
// Music_FadeOut(8), record 3's +1 = 7, +3 up.
extern "C" void __cdecl LeaderPanel_S4Begin(void) {
    const auto state = static_cast<unsigned char>(B(at::kEff4State) + 1);
    unsigned char* const p = Picked("LeaderPanel_S4Begin");
    B(at::kEff4State) = state;
    B(at::kEff2State) = 0;
    B(at::kEff4Mode) = 2;
    Field_Kind2Z = Long(p + 0x38);
    SH_CALL(Music_FadeOut)(8);
    unsigned char* const s = S();
    B(at::kEff3State) = 7;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x5295A0: LeaderPanel_Stage4Steps[1] - effect record 4 at 3: track
// 0x29, record 3 mode 2, record 5's +1 up, +9 = 0, +3 up; Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S4Music(void) {
    if (B(at::kEff4State) == 3) {
        MusicTo(0x29);
        Effect3Mode(2);
        const auto state = static_cast<unsigned char>(B(at::kEff5State) + 1);
        unsigned char* const s = S();
        B(at::kEff5State) = state;
        s[9] = 0;
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    Tick();
}

namespace {

// 0x5296F9 and 0x5297C4's pull: the picked record's +0x1C moved by 0xFE less
// the A record's +3.
void Pull(const char* who) {
    unsigned char* const p = Picked(who);
    const unsigned char step = RecordA()[3];
    p[0x1C] = static_cast<unsigned char>(p[0x1C] + static_cast<unsigned char>(0xFE - step));
}

}  // namespace

// original 0x529860: 0x5295F0's pose - by +9, the pose 2 and record 5's frame
// the A record's +7, else pose 1 and its +4 (signed bytes).
extern "C" void __cdecl LeaderPanel_S4Pose(void) {
    if (S()[9] != 0) {
        unsigned char* const a = RecordA();
        B(at::kPose) = 2;
        SetL(at::kEff5Frame, static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(a[7]))));
    } else {
        unsigned char* const a = RecordA();
        B(at::kPose) = 1;
        SetL(at::kEff5Frame, static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(a[4]))));
    }
}

// original 0x5298A0: 0x5295F0's blink - the picked record's +0x1C 0 when
// Frame_Counter has a bit of 3 >> the A record's +3, else 0xFF.
extern "C" void __cdecl LeaderPanel_S4Blink(void) {
    const U bits = 3u >> (RecordA()[3] & 31u);
    unsigned char* const p = Picked("LeaderPanel_S4Blink");
    p[0x1C] = static_cast<unsigned char>((Frame_Counter & bits) != 0 ? 0 : 0xFF);
}

// original 0x5295F0 (PSX twin 0x801DB03C): LeaderPanel_Stage4Steps[2] - the
// pull: record 0's z at 0x3E8000 or more lands it (records 4, 1 at 4, the
// picked record's +1 = 6, sound 0x203, track 0x2A, animation 0xA, +3 = 3);
// else by the pad - 0xC000 held (pressed: pose 2, +9 = 8, the frame from the
// A record's +5, animation 8, record 5's count up; held: +9 down with a tick,
// or at 0 the rest pose) then 0x20 pressed pulls, 0x20 held blinks; 0x3000
// held lets go (the A record's +6, count down); 0x20 alone pulls or blinks,
// pose 1 by its +4; nothing: the rest pose. Each way ends in 0x52B1B0.
extern "C" void __cdecl LeaderPanel_S4Run(void) {
    B(at::kPoseWas) = B(at::kPose);
    if (L(at::kEff0Z) >= 0x3E8000) {
        B(at::kEff4Mode) = 4;
        B(at::kEff1State) = 4;
        B(at::kEff0State) = 0;
        unsigned char* const p = Picked("LeaderPanel_S4Run");
        B(at::kEff5State) = 0;
        B(at::kEff4State) = 1;
        B(at::kEff3State) = 7;
        p[1] = 6;
        Sound(0x203);
        MusicTo(0x2A);
        Animate(0xA);
        S()[3] = 3;
        return;
    }
    const unsigned held = Input_Held;
    if (held & 0xC000) {
        if (Input_Pressed & 0xC000) {
            unsigned char* const s = S();
            B(at::kPose) = 2;
            s[9] = 8;
            SetL(at::kEff5Frame, static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(RecordA()[5]))));
            Animate(8);
            B(at::kEff5Count) = static_cast<unsigned char>(B(at::kEff5Count) + 1);
        } else {
            unsigned char* const s = S();
            if (s[9] != 0) {
                s[9] = static_cast<unsigned char>(s[9] - 1);
                Tick();
                B(at::kEff5Count) = static_cast<unsigned char>(B(at::kEff5Count) + 1);
            } else {
                SetL(at::kEff5Frame, 0);
                B(at::kPose) = 0;
                Animate(7);
            }
        }
        if (Input_Pressed & 0x20) {
            Pull("LeaderPanel_S4Run");
            Animate(7);
            SH_CALL(LeaderPanel_S4Pose)();
            B(at::kEff5Count) = static_cast<unsigned char>(B(at::kEff5Count) + 1);
            PoseSound();
            return;
        }
        if (Input_Held & 0x20) {
            SH_CALL(LeaderPanel_S4Blink)();
            SH_CALL(LeaderPanel_S4Pose)();
            Tick();
            B(at::kEff5Count) = static_cast<unsigned char>(B(at::kEff5Count) + 1);
        }
        PoseSound();
        return;
    }
    if (held & 0x3000) {
        unsigned char* const a = RecordA();
        B(at::kPose) = 0;
        SetL(at::kEff5Frame, static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(a[6]))));
        Picked("LeaderPanel_S4Run")[0x1C] = 0;
        Animate(0);
        B(at::kEff5Count) = static_cast<unsigned char>(B(at::kEff5Count) - 1);
        PoseSound();
        return;
    }
    if (held & 0x20) {
        if (Input_Pressed & 0x20)
            Pull("LeaderPanel_S4Run");
        else
            SH_CALL(LeaderPanel_S4Blink)();
        B(at::kPose) = 1;
        SetL(at::kEff5Frame, static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(RecordA()[4]))));
        Animate(7);
        Tick();
        B(at::kEff5Count) = static_cast<unsigned char>(B(at::kEff5Count) + 1);
        PoseSound();
        return;
    }
    unsigned char* const p = Picked("LeaderPanel_S4Run");
    B(at::kPose) = 0;
    SetL(at::kEff5Frame, 0);
    p[0x1C] = 0;
    Animate(7);
    PoseSound();
}

// original 0x5298F0: LeaderPanel_Stage4Steps[3] - effect record 4 at 4: the
// picked record's count +0x9C over the best of its kind (0x9040EC + kind)
// replaces it and sets the blink; sound 0x102, +9 = 8, +3 up;
// Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S4Best(void) {
    if (B(at::kEff4State) == 4) {
        unsigned char* const p = Picked("LeaderPanel_S4Best");
        unsigned char& best = B(at::kBestCounts + p[6]);
        if (Word(p + 0x9C) > best) {
            B(at::kBlink) = 1;
            best = p[0x9C];
        }
        Sound(0x102);
        S()[9] = 8;
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] + 1);
    }
    Tick();
}

// original 0x529960: LeaderPanel_Stage4Steps[4] - +9 down, the header at -40
// a count; at 0 sound 0x20C when the kind's byte of 0x66A6AD is 2 or more,
// else 0x20B, +9 = 4, +3 up; Sprite_ScriptTick, the shade.
extern "C" void __cdecl LeaderPanel_S4HeaderIn(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    Header(0u - 40u * (PtrHi(s) | s[9]));
    if (S()[9] == 0) {
        const unsigned kind = PickedKind("LeaderPanel_S4HeaderIn");
        Sound(B(at::kKindSounds + 36u * kind) >= 2 ? 0x20C : 0x20B);
        S()[9] = 4;
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    Tick();
    Shade();
}

// original 0x5299F0: LeaderPanel_Stage4Steps[5] - +9 down; the header; the
// icon at 0x80 + 48 a count; at 0 +9 = 4, +3 up; Sprite_ScriptTick, the shade.
extern "C" void __cdecl LeaderPanel_S4IconIn(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    Header(0);
    const unsigned kind = PickedKind("LeaderPanel_S4IconIn");
    Icon(48u * S()[9] + 0x80u, kind);
    s = S();
    if (s[9] == 0) {
        s[9] = 4;
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    Tick();
    Shade();
}

// original 0x529A60: LeaderPanel_Stage4Steps[6] - +9 down; the header, the
// icon, the kind's row (count 0, row +9); at 0 +3 up; Sprite_ScriptTick, the
// shade.
extern "C" void __cdecl LeaderPanel_S4RowIn(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    Header(0);
    Icon(0x80, PickedKind("LeaderPanel_S4RowIn"));
    const unsigned row = S()[9];
    KindRow(PickedKind("LeaderPanel_S4RowIn"), 0, row);
    s = S();
    if (s[9] == 0) s[3] = static_cast<unsigned char>(s[3] + 1);
    Tick();
    Shade();
}

// original 0x529AE0: LeaderPanel_Stage4Steps[7] - +9 counts up (0x20 pressed:
// straight to the picked record's count), the header, icon and row (the
// count +9); at the count, the blink's sound 0x20D, sound 0x102, +9 = 4, +3
// up; Sprite_ScriptTick, the shade.
extern "C" void __cdecl LeaderPanel_S4Count(void) {
    if (Input_Pressed & 0x20) {
        unsigned char* const p = Picked("LeaderPanel_S4Count");
        S()[9] = p[0x9C];
    } else {
        unsigned char* const s = S();
        s[9] = static_cast<unsigned char>(s[9] + 1);
    }
    Header(0);
    Icon(0x80, PickedKind("LeaderPanel_S4Count"));
    const unsigned count = S()[9];
    KindRow(PickedKind("LeaderPanel_S4Count"), count, 0);
    unsigned char* const p = Picked("LeaderPanel_S4Count");
    unsigned char* const s = S();
    if (static_cast<std::uint16_t>(s[9]) == Word(p + 0x9C)) {
        if (B(at::kBlink) != 0) Sound(0x20D);
        Sound(0x102);
        S()[9] = 4;
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    Tick();
    Shade();
}

// original 0x529BD0: LeaderPanel_Stage4Steps[8] - +9 down; the panel's head,
// the total at 0x9C + 30 a count, the blink; at 0 +3 up; Sprite_ScriptTick,
// the shade.
extern "C" void __cdecl LeaderPanel_S4TotalIn(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    PanelHead("LeaderPanel_S4TotalIn");
    Total(30u * S()[9] + 0x9Cu);
    SH_CALL(FieldPanel_DrawBlink)();
    s = S();
    if (s[9] == 0) s[3] = static_cast<unsigned char>(s[3] + 1);
    Tick();
    Shade();
}

// original 0x529C70 (PSX twin 0x801DBC2C): LeaderPanel_Stage4Steps[9] - the
// panel at rest; 0x60 pressed: the blink off, +0xB = 0; kind 0x16: +9 = 4,
// +0xB = 1; else Inventory_Add(0, kind + 0x38, 1) - refused: +9 = 4, +0xB =
// 2; taken: +3 = 0xD; sound 0x102, +3 up. Sprite_ScriptTick, the shade.
extern "C" void __cdecl LeaderPanel_S4Take(void) {
    PanelHead("LeaderPanel_S4Take");
    Total(0x9C);
    SH_CALL(FieldPanel_DrawBlink)();
    if (Input_Pressed & 0x60) {
        unsigned char* const s = S();
        B(at::kBlink) = 0;
        s[0xB] = 0;
        const unsigned char kind = PickedKind("LeaderPanel_S4Take");
        if (kind == 0x16) {
            S()[9] = 4;
            S()[0xB] = 1;
        } else if (SH_CALL(Inventory_Add)(0, static_cast<unsigned char>(kind + 0x38), 1) == 0) {
            S()[9] = 4;
            S()[0xB] = 2;
        } else {
            S()[3] = 0xD;
        }
        Sound(0x102);
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    Tick();
    Shade();
}

// original 0x529D80 (PSX twin 0x801DBDEC): LeaderPanel_Stage4Steps[10] - +9
// down; the panel; by +0xB message 0x43 (1) or 0x42 (2) at 0x94 + 30 a
// count; at 0: +0xB 1 clears +6 and sets +3 = 0xC, 2 moves +3 up;
// Sprite_ScriptTick, the shade.
extern "C" void __cdecl LeaderPanel_S4Result(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    PanelHead("LeaderPanel_S4Result");
    Total(0x9C);
    s = S();
    const unsigned char how = s[0xB];
    if (how == 1 || how == 2) {
        Message(30u * (PtrHi(s) | s[9]) + 0x94u, how == 1 ? 0x43 : 0x42);
        s = S();
    }
    if (s[9] == 0) {
        if (s[0xB] == 1) {
            s[6] = 0;
            S()[3] = 0xC;
        } else if (s[0xB] == 2) {
            s[3] = static_cast<unsigned char>(s[3] + 1);
        }
    }
    Tick();
    Shade();
}

// original 0x529E70 (PSX twin 0x801DBF5C): LeaderPanel_Stage4Steps[11] - the
// panel with message 0x42; 0x60 pressed: +3 = 0xE. Sprite_ScriptTick, the
// shade.
extern "C" void __cdecl LeaderPanel_S4ResultWait(void) {
    PanelHead("LeaderPanel_S4ResultWait");
    Total(0x9C);
    Message(0x94, 0x42);
    if (Input_Pressed & 0x60) S()[3] = 0xE;
    Tick();
    Shade();
}

// original 0x529F00 (PSX twin 0x801DC048): LeaderPanel_Stage4Steps[12] - the
// panel with message 0x43; any button: +6 = 0, +3 up. Sprite_ScriptTick, the
// shade.
extern "C" void __cdecl LeaderPanel_S4AnyKey(void) {
    PanelHead("LeaderPanel_S4AnyKey");
    Total(0x9C);
    Message(0x94, 0x43);
    if (Input_Pressed != 0) {
        S()[6] = 0;
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    Tick();
    Shade();
}

// original 0x529FA0 (PSX twin 0x801DC14C): LeaderPanel_Stage4Steps[13] - the
// panel with message 0x44; up / down toggle +6 (sound 0x100); else 0x20
// (sound 0x103) or 0x40 (+6 = 1, sound 0x106), then sound 0x102 and +3 up;
// the hand at 0xAA + 12 a +6; Sprite_ScriptTick, the shade.
//
// DIVERGENCE DIV-0027 (amended 2026-10-03, group YN): the hand's row. Message
// 0x44 (in area 30's pool: Manillo's "want to buy anything?", the answers on
// its last two rows) is drawn by FieldPanel_DrawMessage at y 0x94, its first
// row at 0x9C, rows 12 apart; the original's hand at 0xAA + 12 * the answer
// is the row-1 answer of a one-row question - the Chinese line's shape. The
// English question takes two rows, so the hand pointed at the question's
// second row (the owner's capture, 2026-10-03). With the flag on (a Latin
// overlay; set after the self-test, which compares the original's draw) the
// row of the first answer is counted from the message itself: the number of
// newlines less one, so a one-row question gives the original's 0xAA.
unsigned char g_shop_ask_row = 0;

namespace {

// The row of the first of a message's two answers - its last two rows: the
// newlines (0x01) less one. The argument bytes of 0x05 and 0x07 are stepped
// over as the draw steps them, as is a two-byte code's second byte.
unsigned FirstAnswerRow(const unsigned char* text, unsigned id) {
    unsigned rows = 0;
    for (unsigned i = 0; text[i]; ++i) {
        const unsigned char c = text[i];
        if (c == 0x01) ++rows;
        else if ((c == 0x05 || c == 0x07 || (c & 0x80)) && text[i + 1]) ++i;
    }
    if (rows < 2) bof3::Fatal("DIV-0027: message 0x%X has %u newlines - not a question over two answer rows", id, rows);
    return rows - 1;
}

}  // namespace

extern "C" void __cdecl LeaderPanel_S4Again(void) {
    PanelHead("LeaderPanel_S4Again");
    Total(0x9C);
    Message(0x94, 0x44);
    const unsigned pressed = Input_Pressed;
    if (pressed & 0x5000) {
        Sound(0x100);
        unsigned char* const s = S();
        s[6] = static_cast<unsigned char>(s[6] ^ 1);
    } else if (pressed & 0x60) {
        if (pressed & 0x20) {
            Sound(0x103);
        } else {
            S()[6] = 1;
            Sound(0x106);
        }
        Sound(0x102);
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    if (g_shop_ask_row) {
        const U row = FirstAnswerRow(At(at::kPools + Word(At(at::kPools + 0x44u * 2u))), 0x44);
        Hand(0x20, 12u * S()[6] + 0xAAu + 12u * (row - 1u));
    } else {
        Hand(0x20, 12u * S()[6] + 0xAAu);
    }
    Tick();
    Shade();
}

// original 0x52A0A0 (PSX twin 0x801DC2C8): LeaderPanel_Stage4Steps[14] - +9
// up; the header at -80 a count, the icon at 0x80 + 48, the row (row +9), the
// total at 0x9C + 30; +0xB's message at 0x94 + 30; at 4: +9 = 0, then +0xB 1
// with +6 clear to stage 0xB, else stage 1 and effect record 1's +1 up (+3 =
// 0 either way), track 0x28, sound 0x203. Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S4Out(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] + 1);
    s = S();
    Header(0u - 80u * (PtrHi(s) | s[9]));
    const unsigned char kind = PickedKind("LeaderPanel_S4Out");
    Icon(48u * S()[9] + 0x80u, kind);
    unsigned char* const p = Picked("LeaderPanel_S4Out");
    KindRow(p[6], p[0x9C], S()[9]);
    Total(30u * S()[9] + 0x9Cu);
    s = S();
    const unsigned char how = s[0xB];
    if (how == 1 || how == 2) {
        Message(30u * s[9] + 0x94u, how == 1 ? 0x44 : 0x42);
        s = S();
    }
    if (s[9] != 4) {
        Tick();
        return;
    }
    s[9] = 0;
    unsigned char* const t = S();
    if (t[0xB] == 1 && t[6] == 0) {
        t[2] = 0xB;
        S()[3] = 0;
    } else {
        t[2] = 1;
        S()[3] = 0;
        B(at::kEff1State) = static_cast<unsigned char>(B(at::kEff1State) + 1);
    }
    MusicTo(0x28);
    Sound(0x203);
    Tick();
}

// ===========================================================================
// Stages 5..9
// ===========================================================================

// original 0x52A210 (PSX twin 0x801DC4F4): LeaderPanel_Stages[5] - jmp through
// LeaderPanel_Stage5Steps by +3.
extern "C" void __cdecl LeaderPanel_S5(void) {
    Run("LeaderPanel_S5", LeaderPanel_Stage5Steps, LeaderPanel_Stage5Steps_count);
}

// original 0x52A230 (PSX twin 0x801DC538): LeaderPanel_Stage5Steps[0] and
// LeaderPanel_Stage6Steps[0] - Inventory_Remove(3, 0x90412E, 1); refused: the
// two bytes 0x90412E / F cleared; then 0x52B200 (a tail jmp).
extern "C" void __cdecl LeaderPanel_UseItem(void) {
    if (SH_CALL(Inventory_Remove)(3, B(at::kItemA), 1) == 0) {
        B(at::kItemA) = 0;
        B(at::kItemA2) = 0;
    }
    SH_AT(void (__cdecl*)(), at::kUseItemEnd)();
}

// original 0x52A260 (PSX twin 0x801DC594): LeaderPanel_Stage5Steps[1] - effect
// record 4 at 3: record 3 mode 3, +3 up; Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S5Wait(void) {
    if (B(at::kEff4State) == 3) {
        Effect3Mode(3);
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] + 1);
    }
    Tick();
}

// original 0x52A280 (PSX twin 0x801DC5E8): LeaderPanel_Stage5Steps[2],
// Stage6Steps[2], Stage7Steps[2] - 0x52B330(0x60) (a press of 0x60 leaves for
// stage 8), Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_Leave(void) {
    SH_AT(unsigned char (__cdecl*)(unsigned), at::kLeaveOnPress)(0x60);
    Tick();
}

// original 0x52A290 (PSX twin 0x801DC610): LeaderPanel_Stages[6] - jmp through
// LeaderPanel_Stage6Steps by +3.
extern "C" void __cdecl LeaderPanel_S6(void) {
    Run("LeaderPanel_S6", LeaderPanel_Stage6Steps, LeaderPanel_Stage6Steps_count);
}

namespace {
// 0x52A2B0 / 0x52A310: effect record 4 at 3 - track 0x28, sound 0x203, record
// 3 mode `mode`, +3 up; Sprite_ScriptTick.
void WaitThenMode(unsigned mode) {
    if (B(at::kEff4State) == 3) {
        MusicTo(0x28);
        Sound(0x203);
        Effect3Mode(mode);
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] + 1);
    }
    Tick();
}
}  // namespace

// original 0x52A2B0: LeaderPanel_Stage6Steps[1] - record 3 mode 4.
extern "C" void __cdecl LeaderPanel_S6Wait(void) { WaitThenMode(4); }

// original 0x52A2F0 (PSX twin 0x801DC6CC): LeaderPanel_Stages[7] - jmp through
// LeaderPanel_Stage7Steps by +3.
extern "C" void __cdecl LeaderPanel_S7(void) {
    Run("LeaderPanel_S7", LeaderPanel_Stage7Steps, LeaderPanel_Stage7Steps_count);
}

// original 0x52A310: LeaderPanel_Stage7Steps[1] - record 3 mode 5.
extern "C" void __cdecl LeaderPanel_S7Wait(void) { WaitThenMode(5); }

// original 0x52A350 (PSX twin 0x801DC788): LeaderPanel_Stages[8] - jmp through
// LeaderPanel_Stage8Steps by +3.
extern "C" void __cdecl LeaderPanel_S8(void) {
    Run("LeaderPanel_S8", LeaderPanel_Stage8Steps, LeaderPanel_Stage8Steps_count);
}

// original 0x52A370 (PSX twin 0x801DC7CC): LeaderPanel_Stage8Steps[0] - with
// the wait word clear: Task_Sleep(1), the camera to (0xF0000, 0x3C0000),
// Field_ViewReset, 0x52CE20 (the effects cleared), Transition_Start(3); the
// standing animation by Field_State +0x89 (0xA at 2, or 0xB), +3 = 1.
extern "C" void __cdecl LeaderPanel_S8Leave(void) {
    if (MoveScript_WaitWordDA != 0) return;
    SH_CALL(Task_Sleep)(1);
    Field_Kind2X = 0xF0000;
    Field_Kind2Z = 0x3C0000;
    SH_CALL(Field_ViewReset)();
    SH_AT(void (__cdecl*)(), at::kClearEffects)();
    SH_CALL(Transition_Start)(3);
    if (Field_State[0x89] == 0)
        SH_CALL(Sprite_SetAnimationAt)(0xA, 2);
    else
        SH_CALL(Sprite_SetAnimation)(0xB);
    S()[3] = 1;
}

// original 0x52A3F0 (PSX twin 0x801DC86C): LeaderPanel_Stage8Steps[1] - with
// the wait word clear: +2 = 1, +3 = +4 = 0.
extern "C" void __cdecl LeaderPanel_S8End(void) {
    if (MoveScript_WaitWordDA != 0) return;
    S()[2] = 1;
    S()[3] = 0;
    S()[4] = 0;
}

// original 0x52A420 (PSX twin 0x801DC8B8): LeaderPanel_Stages[9] - the shade,
// then jmp through LeaderPanel_Stage9Steps by +3.
extern "C" void __cdecl LeaderPanel_S9(void) {
    Shade();
    Run("LeaderPanel_S9", LeaderPanel_Stage9Steps, LeaderPanel_Stage9Steps_count);
}

// original 0x52A440 (PSX twin 0x801DC908): LeaderPanel_Stage9Steps[0] - with
// effect record 3 idle: Music_FadeOut(0x10), sound 0x102, record 6's +1 up,
// +6 = 0, +3 up.
extern "C" void __cdecl LeaderPanel_S9Begin(void) {
    if (B(at::kEff3State) != 0) return;
    SH_CALL(Music_FadeOut)(0x10);
    Sound(0x102);
    const auto state = static_cast<unsigned char>(B(at::kEff6State) + 1);
    unsigned char* const s = S();
    B(at::kEff6State) = state;
    s[6] = 0;
    unsigned char* const t = S();
    t[3] = static_cast<unsigned char>(t[3] + 1);
}

// original 0x52A480 (PSX twin 0x801DC984): LeaderPanel_Stage9Steps[1] - effect
// record 6 at 3: +3 up.
extern "C" void __cdecl LeaderPanel_S9Wait(void) {
    if (B(at::kEff6State) != 3) return;
    unsigned char* const s = S();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x52A4A0 (PSX twin 0x801DC9BC): LeaderPanel_Stage9Steps[2] - the
// choice record 6 +6 (0..2) moved by the auto-repeated up / down (sound
// 0x100); cancel: Music_FadeIn(0x10), sound 0x106, record 6's +1 to 5 (from 3
// or 4), 0xB (from 8) or 0xF (from 0xE), its +0xB = 0, +3 = 4; confirm: sound
// 0x103 and by the choice record 6's +1 to 0xB (from 8), 0xF (from 0xE) or 5
// (from 3 or 4) with sound 0x102, +3 up. At +3 2, the choice's message and
// the hand.
extern "C" void __cdecl LeaderPanel_S9Menu(void) {
    const unsigned repeat = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000u);
    if (repeat & 0x8000) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(B(at::kEff6Choice) - 1);
        B(at::kEff6Choice) = c;
        if (c & 0x80) B(at::kEff6Choice) = 2;
    } else if (repeat & 0x2000) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(B(at::kEff6Choice) + 1);
        B(at::kEff6Choice) = c;
        if (c > 2) B(at::kEff6Choice) = 0;
    } else {
        const unsigned pressed = Input_Pressed;
        if (Field_CancelButtons & pressed) {
            SH_CALL(Music_FadeIn)(0x10);
            Sound(0x106);
            const unsigned char state = B(at::kEff6State);
            if (state == 3 || state == 4)
                B(at::kEff6State) = 5;
            else if (state == 8)
                B(at::kEff6State) = 0xB;
            else if (state == 0xE)
                B(at::kEff6State) = 0xF;
            unsigned char* const s = S();
            B(at::kEff6Hold) = 0;
            s[3] = 4;
        } else if (Field_ConfirmButtons & pressed) {
            Sound(0x103);
            const unsigned char choice = B(at::kEff6Choice);
            const unsigned char state = B(at::kEff6State);
            // choice 0: 8 or 0xE; 1: 3, 4 or 0xE; 2 and above: 3, 4 or 8
            unsigned char to = 0;
            if (choice != 0 && (state == 3 || state == 4))
                to = 5;
            else if (choice != 1 && state == 8)
                to = 0xB;
            else if (choice <= 1 && state == 0xE)
                to = 0xF;
            if (to != 0) {
                Sound(0x102);
                B(at::kEff6State) = to;
            }
            unsigned char* const s = S();
            s[3] = static_cast<unsigned char>(s[3] + 1);
        }
    }
    if (S()[3] != 2) return;
    switch (B(at::kEff6Choice) & 3) {
    case 0: Text(0x1D, 0x14, At(at::kPools + Word(At(at::kPoolWord20)))); break;
    case 1: Text(0x1D, 0x14, At(at::kPools + Word(At(at::kPoolWord40)))); break;
    case 2: Text(0x1D, 0x14, At(at::kPools + Word(At(at::kPoolWord5E)))); break;
    default: break;
    }
    Hand(48u * B(at::kEff6Choice) + 0x58u, 0x2E);
}

void Effect1E_Inject() {
    if (bof3::WantsShadow("effect_1e")) effect_1e::SelfTest();
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's hand; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("ShopAskRow", static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_shop_ask_row)), &was,
                         &is, 1);
        bof3::Log("DIV-0027    the field panel's message 0x44: the hand on the row of its first answer");
    }
    BOF3_INJECT(LeaderPanel_S1Choose);
    BOF3_INJECT(LeaderPanel_S1Out);
    BOF3_INJECT(LeaderPanel_S1Box2In);
    BOF3_INJECT(LeaderPanel_S1Box2Wait);
    BOF3_INJECT(LeaderPanel_S1Box2Out);
    BOF3_INJECT(LeaderPanel_S2);
    BOF3_INJECT(LeaderPanel_S2Begin);
    BOF3_INJECT(LeaderPanel_S2Wait);
    BOF3_INJECT(LeaderPanel_S2Anim);
    BOF3_INJECT(LeaderPanel_S2Steer);
    BOF3_INJECT(LeaderPanel_S3);
    BOF3_INJECT(LeaderPanel_S3Begin);
    BOF3_INJECT(LeaderPanel_S3Run);
    BOF3_INJECT(LeaderPanel_S3Hold);
    BOF3_INJECT(LeaderPanel_S3End);
    BOF3_INJECT(LeaderPanel_S4);
    BOF3_INJECT(LeaderPanel_S4Begin);
    BOF3_INJECT(LeaderPanel_S4Music);
    BOF3_INJECT(LeaderPanel_S4Run);
    BOF3_INJECT(LeaderPanel_S4Pose);
    BOF3_INJECT(LeaderPanel_S4Blink);
    BOF3_INJECT(LeaderPanel_S4Best);
    BOF3_INJECT(LeaderPanel_S4HeaderIn);
    BOF3_INJECT(LeaderPanel_S4IconIn);
    BOF3_INJECT(LeaderPanel_S4RowIn);
    BOF3_INJECT(LeaderPanel_S4Count);
    BOF3_INJECT(LeaderPanel_S4TotalIn);
    BOF3_INJECT(LeaderPanel_S4Take);
    BOF3_INJECT(LeaderPanel_S4Result);
    BOF3_INJECT(LeaderPanel_S4ResultWait);
    BOF3_INJECT(LeaderPanel_S4AnyKey);
    BOF3_INJECT(LeaderPanel_S4Again);
    BOF3_INJECT(LeaderPanel_S4Out);
    BOF3_INJECT(LeaderPanel_S5);
    BOF3_INJECT(LeaderPanel_UseItem);
    BOF3_INJECT(LeaderPanel_S5Wait);
    BOF3_INJECT(LeaderPanel_Leave);
    BOF3_INJECT(LeaderPanel_S6);
    BOF3_INJECT(LeaderPanel_S6Wait);
    BOF3_INJECT(LeaderPanel_S7);
    BOF3_INJECT(LeaderPanel_S7Wait);
    BOF3_INJECT(LeaderPanel_S8);
    BOF3_INJECT(LeaderPanel_S8Leave);
    BOF3_INJECT(LeaderPanel_S8End);
    BOF3_INJECT(LeaderPanel_S9);
    BOF3_INJECT(LeaderPanel_S9Begin);
    BOF3_INJECT(LeaderPanel_S9Wait);
    BOF3_INJECT(LeaderPanel_S9Menu);
}
