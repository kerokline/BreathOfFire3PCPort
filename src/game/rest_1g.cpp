// Group R1G of round fourteen (wave one): the fishing spot's code, 45 functions
// at 0x5289A0..0x52CD46 - the cut's 43 rows (analysis/round14_cut.tsv) and the
// two starts in their spans no list had (0x528BE0, 0x52BF90), each read to its
// last instruction with capstone (2026-10-04) and fuzzed through the scenario
// harness's field mode (rest_1g_fuzz.cpp). docs/rest_1g.md has them one row
// each.
//
// The leader's state 9 (LeaderPanel_*): E1E took stages 2..9 in round thirteen
// (docs/effect_1e.md); here are stage 1's dispatcher and its steps 0..4 and 7,
// stage 9's step 4, stages 10 and 11 (dispatchers and steps), the two pages
// ChoiceMenu_Run (E1F) jumps to, and the helpers E1E called by address. They
// run with Sprite_Current the leader's ObjTrio record and drive Effect_Objects
// records 0..6 as E1E's do.
//
// Game mode 8's fish (Fish_*): Fish_Spawn fills Sprite_Objects from the spot's
// counts (0x905B88) and the kind table 0x66A690 (36 bytes a kind); Fish_RunAll,
// called by GameMode8_Frame, runs every record in use through Fish_States by
// its +1, with Sprite_Current and Field_ActiveMember the record.
//
// Every one is a faithful replacement. Where the original indexes a .data
// table by a byte it never bounds (a dispatcher's state byte, Fish_Spawn's
// record count) ours aborts with a message (round9 doc section 6); where it
// reads a .data table in place by an unmasked byte that stays inside the image
// (the fish kind, a direction) ours reads the same bytes in place. Every call
// goes through the harness (SH_CALL), so the start-up fuzz can stand recorders
// in for the callees; Sprite_Current and Field_ActiveMember are re-read after
// every call, as the originals re-read them.
#include "game/rest_1g.h"

#include <cstdint>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using move_script::At;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using Handler = void (__cdecl*)();

// --- the cells ------------------------------------------------------------------------

// Effect_Objects records 0..6 (0x7E11E0 + 0x80 n), the bytes and words the
// stages and the fish read and write.
constexpr U kEff0Flags = 0x7E11E0;     // record 0 +0: bit 6 set while a fish pulls
constexpr U kEff0State = 0x7E11E1;     // record 0 +1
constexpr U kEff0Depth = 0x7E11E7;     // record 0 +7 (signed)
constexpr U kEff0Byte0A = 0x7E11EA;    // record 0 +0xA
constexpr U kEff0StepX = 0x7E11EC;     // record 0 +0xC
constexpr U kEff0StepY = 0x7E11F0;     // record 0 +0x10
constexpr U kEff0StepZ = 0x7E11F4;     // record 0 +0x14
constexpr U kEff0X = 0x7E1214;         // record 0 +0x34
constexpr U kEff0Z = 0x7E1218;         // record 0 +0x38
constexpr U kEff0Height = 0x7E121E;    // record 0 +0x3E (s16)
constexpr U kEff1State = 0x7E1261;     // record 1 +1
constexpr U kEff3State = 0x7E1361;     // record 3 +1
constexpr U kEff3Mode = 0x7E1366;      // record 3 +6
constexpr U kEff3Started = 0x7E1367;   // record 3 +7
constexpr U kEff4State = 0x7E13E1;     // record 4 +1
constexpr U kEff4Mode = 0x7E13E6;      // record 4 +6
constexpr U kEff5Frame = 0x7E1470;     // record 5 +0x10 (s32)
constexpr U kEff5Level = 0x7E1490;     // record 5 +0x30 (a byte, and a s16)
constexpr U kEff6State = 0x7E14E1;     // record 6 +1
constexpr U kEff6Dir = 0x7E14E8;       // record 6 +8
constexpr U kEff6Page = 0x7E151C;      // record 6 +0x3C (u16)
constexpr U kEff6Shown = 0x7E151E;     // record 6 +0x3E (u16)

constexpr U kLeaderStage = 0x802D42;   // ObjTrio record 0 +2 (the leader's stage)
constexpr U kLeaderStep = 0x802D43;    // ObjTrio record 0 +3
constexpr U kSpriteIndex = 0x939A1C;   // u8: 0xFF none (E1E's kSpriteIndex)
constexpr U kRecordA = 0x939A20;       // pointer: a 10-byte record (the rod's: 0x66A4E8 + 10 * (0x904130 - 0x2E))
constexpr U kRecordB = 0x939A24;       // pointer: a 20-byte record (the lure's: 0x66A528 + 20 * (0x90412E - 0x1C))
constexpr U kItemB = 0x90412E;         // u8 (save data)
constexpr U kItemA = 0x904130;         // u8 (save data)
constexpr U kWarned = 0x6BC709;        // u8: set once by LeaderPanel_S1Buttons
constexpr U kPressNow = 0x6BC717;      // u8: Input_Pressed & 0xE020 non-zero (LeaderPanel_PressLatch)
constexpr U kPressWas = 0x6BC708;      // u8: kPressNow's previous value
constexpr U kComboStep = 0x6BC70C;     // u8 x 4: each sequence's position
constexpr U kComboTimer = 0x6BC710;    // u8 x 4: each sequence's frames left
constexpr U kPoseWas = 0x6BC71C;       // u8 (E1E's)
constexpr U kPose = 0x6BC71D;          // u8 (E1E's)
constexpr U kBiteHeld = 0x6BC716;      // u8: a fish is nibbling (Fish_Swim sets, Fish_Settle clears)
constexpr U kHoldFlag = 0x903850;      // u8 (E1E reads it after LeaderPanel_HoldTest)
constexpr U kPoolWordData = 0x803642;  // u16: MessagePools offset of the data page's title
constexpr U kPoolWordRule = 0x803660;  // u16: the rule page's (read as a dword & 0xFFFF)
constexpr U kPoolWordStage1 = 0x803600;  // u16 (read as a dword & 0xFFFF)
constexpr U kPools = 0x803580;         // MessagePools
constexpr U kSpot = 0x905B88;          // u8: the fishing spot (Field_EdgeBits +8)
constexpr U kDrawMode = 0x93985C;      // u8 x 4, cleared by LeaderPanel_S11Switch

// Image tables (.data, read in place).
constexpr U kFishKinds = 0x66A690;     // 36 bytes a kind: chances, banks, levels, sizes
constexpr U kSpotCounts = 0x66A9CC;    // 23 bytes a spot: how many of each kind
constexpr U kComboTable = 0x66A4C8;    // 4 rows of 8: a sequence pointer, a depth byte
constexpr U kRecordsA = 0x66A4E8;      // the 10-byte records
constexpr U kRecordsB = 0x66A528;      // the 20-byte records
constexpr U kDirAnims = 0x660340;      // 2 bytes a direction: an animation, +0x2A's byte
constexpr U kS6Frames = 0x660364;      // u16 by kind: Fish_S6Wait's frame
constexpr U kF9Steps = 0x6696DC;       // MoveCmd_F9Steps: 8 rows of two longs
constexpr U kF9StepsEnd = 0x669720;

constexpr unsigned kKindStride = 36;
constexpr unsigned kKinds = 23;
constexpr unsigned kSpriteStride = 0xA4;
constexpr unsigned kSpriteCount = 30;

unsigned char* S() { return Sprite_Current; }
unsigned char* AM() { return Field_ActiveMember; }
unsigned char& B(U address) { return At(address)[0]; }
std::int32_t L(U address) { return Long(At(address)); }
void SetL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }
U W(U address) { return Word(At(address)); }
void SetW(U address, U v) { SetWord(At(address), v); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
// A register loaded with Sprite_Current, then `movzx ax, byte [reg + n]`: the
// pointer's upper half above the byte (E1E's PtrHi).
U PtrHi(const unsigned char* p) { return Key(p) & 0xFFFF0000u; }
std::int32_t L(const unsigned char* p, unsigned at) { return Long(p + at); }
void SetL(unsigned char* p, unsigned at, U v) { SetLong(p + at, static_cast<std::int32_t>(v)); }
short Sh(const unsigned char* p, unsigned at) { return static_cast<short>(Word(p + at)); }

unsigned char* RecordA() { return At(static_cast<U>(L(kRecordA))); }
unsigned char* RecordB() { return At(static_cast<U>(L(kRecordB))); }

// A byte of the kind table: the kind's 36-byte row at `at`, read in place with
// the kind unbounded (the originals' `lea edx, [eax + eax*8]` on the byte).
unsigned char Kind(unsigned kind, unsigned at) { return At(kFishKinds + kKindStride * kind + at)[0]; }
U KindWord(unsigned kind, unsigned at) { return Word(At(kFishKinds + kKindStride * kind + at)); }
unsigned char DirAnim(unsigned d) { return At(kDirAnims + 2 * d)[0]; }
unsigned char DirByte(unsigned d) { return At(kDirAnims + 2 * d + 1)[0]; }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void Run(const char* who, const unsigned long* table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_1g.md section 7)",
                    who, byte, index, entries, (unsigned)Key(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

// --- the callees ------------------------------------------------------------------------
void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
unsigned char Tick() { return SH_CALL(Sprite_ScriptTick)(); }
void Shade() { SH_CALL(FieldPanel_DrawShade)(); }
void Text(int x, int y, const unsigned char* text) { SH_CALL(Text_DrawAt)(x, y, 0, 0xFF, text); }
void Bank(U bank) { SH_CALL(Sprite_SetAnimationBank)(static_cast<unsigned short>(bank)); }
void Animation(unsigned a) { SH_CALL(Sprite_SetAnimation)(static_cast<unsigned char>(a)); }
void Ensure(unsigned a) { SH_CALL(Sprite_EnsureAnimation)(static_cast<unsigned char>(a)); }
unsigned Rnd() { return static_cast<unsigned>(SH_CALL(Rand)()); }
short Elevation(U x, U z) { return static_cast<short>(SH_CALL(AreaMap_Elevation)(static_cast<long>(x), static_cast<long>(z))); }

// The fish's direction animation and its +0x2A byte by +8 (the rows of
// kDirAnims), each with Sprite_Current re-read: `s[0x2A] = table[2 d + 1]`,
// then the animation of table[2 d] ensured.
void DirEnsure() {
    unsigned char* s = S();
    s[0x2A] = DirByte(s[8]);
    s = S();
    Ensure(DirAnim(s[8]));
}

// (a - b) as the originals' cdq / xor / sub take its absolute value: the
// 32-bit result, 0x80000000 left negative.
std::int32_t Abs32(U d) {
    const U sign = static_cast<U>(static_cast<std::int32_t>(d) >> 31);
    return static_cast<std::int32_t>((d ^ sign) - sign);
}
// `cdq; and edx, 2^k - 1; add eax, edx; sar eax, k`: the signed quotient by 2^k
// rounded toward zero.
std::int32_t DivPow2(std::int32_t v, unsigned k) { return (v + (v < 0 ? (1 << k) - 1 : 0)) >> k; }

}  // namespace

// ===========================================================================
// Stage 1 (LeaderPanel_Stage1Steps 0x66022C: steps 0..4 and 7 here, 5, 6, 8..10
// E1E's)
// ===========================================================================

// original 0x5289A0: LeaderPanel_Stages[1] - jmp through LeaderPanel_Stage1Steps
// by Sprite_Current +3, unchecked (ours aborts past its 11).
extern "C" void __cdecl LeaderPanel_S1(void) {
    Run("LeaderPanel_S1 (0x5289A0)", LeaderPanel_Stage1Steps, LeaderPanel_Stage1Steps_count, Sprite_Current[3],
        "Sprite_Current +3");
}

// original 0x5289C0: LeaderPanel_Stage1Steps[0] - LeaderPanel_SetRecords; the
// sprite index 0xFF; +0xC..+0x20 (six dwords) and +9 cleared; record 3's mode 0
// when both item bytes are set, else 6; the animation by Field_State +0x89 (0:
// 0xA from frame 2, else 0xB); +3 up.
extern "C" void __cdecl LeaderPanel_S1Begin(void) {
    SH_CALL(LeaderPanel_SetRecords)();
    B(kSpriteIndex) = 0xFF;
    for (unsigned at = 0xC; at <= 0x20; at += 4) SetL(S(), at, 0);
    S()[9] = 0;
    const unsigned mode = B(kItemB) != 0 && B(kItemA) != 0 ? 0u : 6u;
    SH_CALL(LeaderPanel_Effect3Mode)(mode);
    if (Field_State[0x89] == 0) SH_CALL(Sprite_SetAnimationAt)(0xA, 2);
    else Animation(0xB);
    unsigned char* const s = S();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x528A70: LeaderPanel_Stage1Steps[1] - record 3 at 6: +3 up.
extern "C" void __cdecl LeaderPanel_S1Wait(void) {
    if (B(kEff3State) != 6) return;
    unsigned char* const s = S();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x528A90: LeaderPanel_Stage1Steps[2] - by Input_Pressed's low byte
// & 0x74 exactly (a byte-indexed jump table): 0x04 stage 10, 0x10 stage 9
// (records 3 = 7, 1 = 4 each); 0x40 records 3 = 7, 1 = 4, +3 up; 0x20 record 3
// = 7, then with both item bytes set and Inventory_Holds38To4DAt99 answering:
// the first time (0x6BC709 clear) 0x6BC709 = 1, record 1 = 4, +3 = 7, after it
// stage 2; with Inventory_Holds38To4DAt99 0, 0x6BC709 = 0 and stage 2; with an
// item byte 0, record 1 = 4 and stage 9. Any other value: nothing.
extern "C" void __cdecl LeaderPanel_S1Buttons(void) {
    switch (static_cast<unsigned char>(Input_Pressed) & 0x74) {
    case 0x04: {
        unsigned char* const s = S();
        B(kEff3State) = 7;
        B(kEff1State) = 4;
        s[2] = 0xA;
        S()[3] = 0;
        return;
    }
    case 0x10: {
        unsigned char* const s = S();
        B(kEff3State) = 7;
        B(kEff1State) = 4;
        s[2] = 9;
        S()[3] = 0;
        return;
    }
    case 0x20: {
        const unsigned char item = B(kItemB);
        B(kEff3State) = 7;
        if (item == 0 || B(kItemA) == 0) {
            unsigned char* const s = S();
            B(kEff1State) = 4;
            s[2] = 9;
            S()[3] = 0;
            return;
        }
        const unsigned char holds = SH_CALL(Inventory_Holds38To4DAt99)();
        if (holds == 0) {
            B(kWarned) = 0;
        } else if (B(kWarned) == 0) {
            unsigned char* const s = S();
            B(kWarned) = 1;
            B(kEff1State) = 4;
            s[3] = 7;
            return;
        }
        S()[2] = 2;
        S()[3] = 0;
        return;
    }
    case 0x40: {
        unsigned char* const s = S();
        B(kEff3State) = 7;
        B(kEff1State) = 4;
        s[3] = static_cast<unsigned char>(s[3] + 1);
        return;
    }
    default: return;
    }
}

// original 0x528BE0: LeaderPanel_Stage1Steps[3] and [7] - record 3 idle and
// record 1 at 1: +9 = 4, sound 0x102, +3 up; then (a tail jmp)
// FieldPanel_DrawShade.
extern "C" void __cdecl LeaderPanel_S1Idle(void) {
    if (B(kEff3State) == 0 && B(kEff1State) == 1) {
        S()[9] = 4;
        Sound(0x102);
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] + 1);
    }
    Shade();
}

// original 0x528C20: LeaderPanel_Stage1Steps[4] - +9 down; box 3 at (0x6C +
// 0x50 * +9, 0x56) - the x from the pointer's upper half above the byte, as the
// original's `movzx ax`; at 0, +6 = 1 and +3 up; the shade; the prompt box at y
// 0x12 - 8 * +9 and its message (MessagePools + the word 0x803600) at 0x15 - 8
// * +9 (FieldPanel_DrawWindow and Text_DrawAt read the low words).
extern "C" void __cdecl LeaderPanel_S1Box3In(void) {
    unsigned char* s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    SH_CALL(FieldPanel_DrawBox3)(static_cast<int>(0x50u * (PtrHi(s) | s[9]) + 0x6Cu), 0x56);
    s = S();
    if (s[9] == 0) {
        s[6] = 1;
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    Shade();
    SH_CALL(Panel_DrawWindow)(0x14, static_cast<int>(0x12u - (static_cast<U>(S()[9]) << 3)), 0x118, 0x13, 0);
    const U pool = static_cast<U>(L(kPoolWordStage1)) & 0xFFFF;
    Text(0x1D, static_cast<int>(0x15u - (static_cast<U>(S()[9]) << 3)), At(kPools + pool));
}

// ===========================================================================
// The fishing menu's pages (ChoiceMenu_Choices 0x6602F4 [1], [2]: E1F's
// ChoiceMenu_Run jumps by record 6 +6) and stage 9's step 4
// ===========================================================================

// original 0x52ADA0: ChoiceMenu_Choices[1] - the title (MessagePools + the word
// 0x803642) at (0x1D, 0x14); with record 6 at 8: 0x2000 pressed: sound 0x102,
// the page word +0x3C = +0x3E + 1 (0 from 0x17: 23 pages), +8 = 0, record 6 up;
// else 0x8000: sound 0x102, +0x3C = +0x3E - 1 (0x16 below 0), +8 = 1, record 6
// up; else a cancel button: sound 0x106, +4 = 0, +3 down.
extern "C" void __cdecl ChoiceMenu_DataPage(void) {
    Text(0x1D, 0x14, At(kPools + W(kPoolWordData)));
    if (B(kEff6State) != 8) return;
    const unsigned pressed = Input_Pressed;
    if (pressed & 0x2000) {
        Sound(0x102);
        const U page = (W(kEff6Shown) + 1) & 0xFFFF;
        SetW(kEff6Page, page < 0x17 ? page : 0);
        B(kEff6Dir) = 0;
        B(kEff6State) = static_cast<unsigned char>(B(kEff6State) + 1);
        return;
    }
    if (pressed & 0x8000) {
        Sound(0x102);
        const auto page = static_cast<short>(W(kEff6Shown) - 1);
        SetW(kEff6Page, page < 0 ? 0x16u : static_cast<U>(page));
        B(kEff6Dir) = 1;
        B(kEff6State) = static_cast<unsigned char>(B(kEff6State) + 1);
        return;
    }
    if ((Field_CancelButtons & pressed) == 0) return;
    Sound(0x106);
    S()[4] = 0;
    unsigned char* const s = S();
    s[3] = static_cast<unsigned char>(s[3] - 1);
}

// original 0x52AE80: ChoiceMenu_Choices[2] - the title (MessagePools + the word
// 0x803660) at (0x1D, 0x14); with record 6 at 0xE and its +8 clear: a cancel
// button: sound 0x106, +4 = 0, +3 down; else 0x4000: +8 = 0xFF, the page word
// up (0 past 5: six pages); else 0x1000: the page word down (5 below 0), +8 =
// 1. No sound for the two turns.
extern "C" void __cdecl ChoiceMenu_RulePage(void) {
    Text(0x1D, 0x14, At(kPools + (static_cast<U>(L(kPoolWordRule)) & 0xFFFF)));
    if (B(kEff6State) != 0xE || B(kEff6Dir) != 0) return;
    const unsigned pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        Sound(0x106);
        S()[4] = 0;
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] - 1);
        return;
    }
    if (pressed & 0x4000) {
        const U page = (W(kEff6Page) + 1) & 0xFFFF;
        B(kEff6Dir) = 0xFF;
        SetW(kEff6Page, page > 5 ? 0 : page);
        return;
    }
    if (pressed & 0x1000) {
        const auto page = static_cast<short>(W(kEff6Page) - 1);
        SetW(kEff6Page, static_cast<U>(page));
        B(kEff6Dir) = 1;
        if (page < 0) SetW(kEff6Page, 5);
    }
}

// original 0x52AF30: LeaderPanel_Stage9Steps[4] - record 6 idle: record 1 up,
// back to stage 1 step 0.
extern "C" void __cdecl LeaderPanel_S9Back(void) {
    if (B(kEff6State) != 0) return;
    B(kEff1State) = static_cast<unsigned char>(B(kEff1State) + 1);
    S()[2] = 1;
    S()[3] = 0;
}

// ===========================================================================
// Stages 10 and 11
// ===========================================================================

// original 0x52AF60: LeaderPanel_Stages[10] - jmp through LeaderPanel_Stage10Steps
// by +3, unchecked (ours aborts past its 2).
extern "C" void __cdecl LeaderPanel_S10(void) {
    Run("LeaderPanel_S10 (0x52AF60)", LeaderPanel_Stage10Steps, LeaderPanel_Stage10Steps_count, Sprite_Current[3],
        "Sprite_Current +3");
}

// original 0x52AF80: LeaderPanel_Stage10Steps[0] - with Field_Kind2Hold clear:
// Input_Held bit 0x40, or Field_Kind2Z at 0x150000, with record 3 idle: the
// divisor 0x40, Field_Kind2Z 0x3C0000, +3 = 1; otherwise the divisor 0x20 and
// Field_Kind2Z 0x8000 nearer. Then, on frames with bit 2 of Frame_Counter, five
// grey F2 lines (x 0xC000 to 0xCF00 at heights (0x340000 - 0xA0000 i) >> 9 -
// 0x4000), each projected (Gte_RotTransPers, Gte_StoreDepthF) into a packet and
// committed (Gfx_CommitPrim(2, 0x20)).
extern "C" void __cdecl LeaderPanel_S10Look(void) {
    if (Field_Kind2Hold == 0) {
        bool done = false;
        if ((static_cast<unsigned char>(Input_Held) & 0x40) != 0 || Field_Kind2Z == 0x150000) {
            if (B(kEff3State) == 0) {
                MoveScript_F3Divisor = 0x40;
                Field_Kind2Z = 0x3C0000;
                S()[3] = 1;
                done = true;
            }
        }
        if (!done) {
            MoveScript_F3Divisor = 0x20;
            Field_Kind2Z = static_cast<long>(static_cast<U>(Field_Kind2Z) - 0x8000u);
        }
    }
    if ((Frame_Counter & 4) == 0) return;
    for (std::int32_t row = 0x340000; row >= 0xC0000; row -= 0xA0000) {
        unsigned char* const prim = Gfx_PacketNext;
        SH_CALL(Gpu_SetLineF2)(prim);
        const auto y = static_cast<short>((row >> 9) - 0x4000);
        long depth;
        short v0[4] = {static_cast<short>(0xC000), y, 0, 0};
        SH_CALL(Gte_RotTransPers)(v0, reinterpret_cast<unsigned long*>(prim + 8), &depth);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x10));
        short v1[4] = {static_cast<short>(0xCF00), y, 0, 0};
        SH_CALL(Gte_RotTransPers)(v1, reinterpret_cast<unsigned long*>(prim + 0x14), &depth);
        SH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(prim + 0x1C));
        prim[4] = 0x80;
        prim[5] = 0x80;
        prim[6] = 0x80;
        SH_CALL(Gfx_CommitPrim)(2, 0x20);
    }
}

// original 0x52B0B0: LeaderPanel_Stage10Steps[1] - Field_Kind2Hold clear:
// record 1 up, back to stage 1 step 0.
extern "C" void __cdecl LeaderPanel_S10End(void) {
    if (Field_Kind2Hold != 0) return;
    B(kEff1State) = static_cast<unsigned char>(B(kEff1State) + 1);
    S()[2] = 1;
    S()[3] = 0;
}

// original 0x52B0E0: LeaderPanel_Stages[11] - jmp through LeaderPanel_Stage11Steps
// by +3, unchecked (ours aborts past its 4).
extern "C" void __cdecl LeaderPanel_S11(void) {
    Run("LeaderPanel_S11 (0x52B0E0)", LeaderPanel_Stage11Steps, LeaderPanel_Stage11Steps_count, Sprite_Current[3],
        "Sprite_Current +3");
}

// original 0x52B100: LeaderPanel_Stage11Steps[0] - record 3 idle:
// Transition_Start(0), +3 up.
extern "C" void __cdecl LeaderPanel_S11FadeOut(void) {
    if (B(kEff3State) != 0) return;
    SH_CALL(Transition_Start)(0);
    unsigned char* const s = S();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x52B120: LeaderPanel_Stage11Steps[1] - the wait word clear: the
// four bytes 0x93985C..0x93985F and Draw_PassFlags cleared, Game_Step = 3, +3
// up (game mode 8's step 3).
extern "C" void __cdecl LeaderPanel_S11Switch(void) {
    if (MoveScript_WaitWordDA != 0) return;
    B(kDrawMode) = 0;
    B(kDrawMode + 2) = 0;
    B(kDrawMode + 1) = 0;
    B(kDrawMode + 3) = 0;
    Draw_PassFlags = 0;
    unsigned char* const s = S();
    Game_Step = 3;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x52B160: LeaderPanel_Stage11Steps[2] - Transition_Start(1),
// Draw_PassFlags = 0x1F, +3 up.
extern "C" void __cdecl LeaderPanel_S11FadeIn(void) {
    SH_CALL(Transition_Start)(1);
    unsigned char* const s = S();
    Draw_PassFlags = 0x1F;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x52B180: LeaderPanel_Stage11Steps[3] - the wait word clear: record
// 1 up, back to stage 1 step 0.
extern "C" void __cdecl LeaderPanel_S11End(void) {
    if (MoveScript_WaitWordDA != 0) return;
    B(kEff1State) = static_cast<unsigned char>(B(kEff1State) + 1);
    S()[2] = 1;
    S()[3] = 0;
}

// ===========================================================================
// The stages' helpers (E1E called them by address)
// ===========================================================================

// original 0x52B1B0: the pose 0x6BC71D other than 0x6BC71C: sound 0x203, 0x202
// or 0x205 for pose 0, 1, 2 (none for any other). Neither byte is written.
extern "C" void __cdecl LeaderPanel_PoseSound(void) {
    const unsigned char pose = B(kPose);
    if (pose == B(kPoseWas)) return;
    switch (pose) {
    case 0: Sound(0x203); break;
    case 1: Sound(0x202); break;
    case 2: Sound(0x205); break;
    default: break;
    }
}

// original 0x52B200: LeaderPanel_Stage7Steps[0], and LeaderPanel_UseItem's tail
// - Sprite_EnsureAnimation(9), sound 0x204; record 3 = 7, record 4 up (read
// after the calls) with its +6 = 3, +3 up; then (a tail jmp) Sprite_ScriptTick,
// whose answer no caller reads.
extern "C" void __cdecl LeaderPanel_UseItemEnd(void) {
    Ensure(9);
    Sound(0x204);
    const auto state = static_cast<unsigned char>(B(kEff4State) + 1);
    unsigned char* const s = S();
    B(kEff3State) = 7;
    B(kEff4State) = state;
    B(kEff4Mode) = 3;
    s[3] = static_cast<unsigned char>(s[3] + 1);
    Tick();
}

// original 0x52B250: the two record pointers - 0x90412E set: 0x939A24 =
// 0x66A528 + 20 * (it - 0x1C); 0x904130 set: 0x939A20 = 0x66A4E8 + 10 * (it -
// 0x2E). An item byte below its base gives a pointer before the table (32-bit,
// as the original computes it; nothing is read here).
extern "C" void __cdecl LeaderPanel_SetRecords(void) {
    const unsigned b = B(kItemB);
    if (b != 0) SetL(kRecordB, kRecordsB + 20u * (b - 0x1Cu));
    const unsigned a = B(kItemA);
    if (a != 0) SetL(kRecordA, kRecordsA + 10u * (a - 0x2Eu));
}

// original 0x52B2A0: record 3's mode = `mode`'s byte; when its +1 is 0, 4 or 6,
// also its +7 = 1 and +1 up.
extern "C" void __cdecl LeaderPanel_Effect3Mode(unsigned mode) {
    const unsigned char state = B(kEff3State);
    if (state == 4 || state == 6 || state == 0) {
        B(kEff3Started) = 1;
        B(kEff3State) = static_cast<unsigned char>(state + 1);
    }
    B(kEff3Mode) = static_cast<unsigned char>(mode);
}

// original 0x52B2E0: Input_Pressed bit 0x4000: 0x903850 = 1, +0xA = 8; else
// Input_Held bit 0x4000 with +0xA not 0: +0xA down, 0x903850 = 1; else
// 0x903850 = 0.
extern "C" void __cdecl LeaderPanel_HoldTest(void) {
    if (Input_Pressed & 0x4000) {
        B(kHoldFlag) = 1;
        S()[0xA] = 8;
        return;
    }
    if (Input_Held & 0x4000) {
        unsigned char* const s = S();
        if (s[0xA] != 0) {
            s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
            B(kHoldFlag) = 1;
            return;
        }
    }
    B(kHoldFlag) = 0;
}

// original 0x52B330: (Input_Pressed & buttons)'s low word not 0:
// Transition_Start(2), stage 8 step 0, al 1; else al 0.
extern "C" unsigned char __cdecl LeaderPanel_LeaveOnPress(unsigned buttons) {
    if (((Input_Pressed & buttons) & 0xFFFF) == 0) return 0;
    SH_CALL(Transition_Start)(2);
    S()[2] = 8;
    S()[3] = 0;
    return 1;
}

// original 0x52B370: LeaderPanel_PressLatch, then the four button sequences of
// kComboTable (a pointer to a run of bytes - bit 7 the last, the low seven 0 or
// 1 - and a depth byte): for each, by 0x6BC717 (now 0 / 1) its position +0x6BC70C
// and its frames left +0x6BC710 (counted down while it has begun): a press when
// the run wants one, or a pause when it wants none, steps it (the frames 0xC);
// a press with 4 or more frames left, or a wrong one, resets it; the run's last
// byte reached sets record 0's +7 to depth + the 20-byte record's +0xF (when
// that is not below +7 as it was) and its +0xA to the record's +0x10, and
// resets the sequence. Any 0x6BC717 other than 0 or 1 leaves all four as they
// are (bar the count-down).
extern "C" void __cdecl LeaderPanel_EffectsStep(void) {
    SH_CALL(LeaderPanel_PressLatch)();
    unsigned char now = B(kPressNow);
    unsigned char depth = B(kEff0Depth);
    for (unsigned i = 0; i < 4; ++i) {
        const U row = kComboTable + 8 * i;
        const unsigned char step = B(kComboStep + i);
        const unsigned char* const at = At(static_cast<U>(L(row)) + step);
        const unsigned char want = static_cast<unsigned char>(at[0] & 0x7F);
        if (step != 0) B(kComboTimer + i) = static_cast<unsigned char>(B(kComboTimer + i) - 1);
        bool advance = false;
        if (now == 1) {
            if (step == 0) advance = true;
            else if (B(kComboTimer + i) < 4 && want == now) advance = true;
            else {
                B(kComboTimer + i) = 0;
                B(kComboStep + i) = 0;
                continue;
            }
        } else {
            if (now != 0) continue;
            if (B(kComboTimer + i) != now) continue;
            if (want != 0) {
                B(kComboTimer + i) = 0;
                B(kComboStep + i) = 0;
                continue;
            }
            advance = true;
        }
        if (!advance) continue;
        if ((at[0] & 0x80) == 0) {
            B(kComboTimer + i) = 0xC;
            B(kComboStep + i) = static_cast<unsigned char>(step + 1);
            continue;
        }
        const unsigned char* const rb = RecordB();
        const auto base = static_cast<signed char>(At(row + 4)[0]);
        const auto plus = static_cast<signed char>(rb[0xF]);
        if (static_cast<int>(base) + plus >= static_cast<signed char>(depth)) {
            depth = static_cast<unsigned char>(base + plus);
            B(kEff0Depth) = depth;
            B(kEff0Byte0A) = RecordB()[0x10];
        }
        now = B(kPressNow);
        B(kComboStep + i) = 0;
        B(kComboTimer + i) = 0;
    }
}

// original 0x52B460: 0x6BC708 = 0x6BC717; 0x6BC717 = 1 when Input_Pressed has
// any of 0xE020, else 0.
extern "C" void __cdecl LeaderPanel_PressLatch(void) {
    B(kPressWas) = B(kPressNow);
    B(kPressNow) = (Input_Pressed & 0xE020) != 0 ? 1 : 0;
}

// ===========================================================================
// Game mode 8's fish
// ===========================================================================

// original 0x52B480 (PSX twin 0x801DE218): for each of the 23 kinds k, the
// spot's count (kSpotCounts by 0x905B88) of Sprite_Objects records from the
// next free index: +0 = 0x21, +5 = the index, +6 = k; its size word +0x9C =
// Rand & 0xFF, lowered by M / 2 at a time (M the kind's top size, +0x1F) while above it, raised
// to M / 2 when below that; +0x98 = +0x9A = size * 10 / M * the kind's +0x1E /
// 10; a fish of exactly M (kinds other than 0x15) grows by one while Rand is
// odd, at most M / 10 + 1 times; then +1..+4 and +0x5C cleared. Sprite_Current
// and Field_ActiveMember are the record (re-read after each Rand). Ours aborts
// when the counts would fill more than the 30 records (the original writes past
// them).
extern "C" void __cdecl Fish_Spawn(void) {
    unsigned index = 0;
    for (unsigned k = 0; k < kKinds; ++k) {
        const unsigned spot = B(kSpot);
        unsigned count = At(kSpotCounts + 23 * spot + k)[0];
        if (count == 0) continue;
        do {
            if (index >= kSpriteCount)
                bof3::Fatal("Fish_Spawn (0x52B480): spot %u's counts fill more than the %u Sprite_Objects records - the "
                            "original writes record %u at 0x%X (docs/rest_1g.md section 7)",
                            spot, kSpriteCount, index, (unsigned)(Key(Sprite_Objects) + index * kSpriteStride));
            unsigned char* const rec = Sprite_Objects + index * kSpriteStride;
            Sprite_Current = rec;
            Field_ActiveMember = rec;
            rec[0] = 0x21;
            S()[5] = static_cast<unsigned char>(index);
            S()[6] = static_cast<unsigned char>(k);
            const unsigned r = Rnd();
            SetWord(AM() + 0x9C, r & 0xFF);
            for (;;) {
                const unsigned char m = Kind(S()[6], 0x1F);
                unsigned char* const am = AM();
                const U size = Word(am + 0x9C);
                if (size <= m) break;
                if ((m >> 1) == 0)
                    bof3::Fatal("Fish_Spawn (0x52B480): kind %u's top size (0x%X) is %u and the size rolled is %u - the "
                                "original subtracts M / 2 = 0 for ever (docs/rest_1g.md section 7)",
                                (unsigned)S()[6], (unsigned)(kFishKinds + kKindStride * S()[6] + 0x1F), (unsigned)m, (unsigned)size);
                SetWord(am + 0x9C, size - (m >> 1));
            }
            {
                unsigned char* const am = AM();
                const unsigned char m = Kind(S()[6], 0x1F);
                const U size = Word(am + 0x9C);
                U v = size;
                if (size < static_cast<U>(m >> 1)) v = m >> 1;
                else if (size > m) v = m;
                SetWord(am + 0x9C, v);
            }
            {
                const unsigned kind = S()[6];
                unsigned char* const am = AM();
                if (Kind(kind, 0x1F) == 0)
                    bof3::Fatal("Fish_Spawn (0x52B480): kind %u's top size (0x%X) is 0 - the original divides by it "
                                "(docs/rest_1g.md section 7)",
                                kind, (unsigned)(kFishKinds + kKindStride * kind + 0x1F));
                const auto tenths = static_cast<std::int32_t>(Word(am + 0x9C) * 10u) / static_cast<std::int32_t>(Kind(kind, 0x1F));
                SetWord(am + 0x98, static_cast<U>((tenths * static_cast<std::int32_t>(Kind(kind, 0x1E))) / 10));
                unsigned char* const am2 = AM();
                SetWord(am2 + 0x9A, Word(am2 + 0x98));
            }
            const unsigned kind = S()[6];
            if (kind != 0x15) {
                const unsigned char m = Kind(kind, 0x1F);
                if (Word(AM() + 0x9C) == m) {
                    const int extra = m / 10;
                    for (int n = 0; n <= extra; ++n) {
                        if ((Rnd() & 1) == 0) break;
                        unsigned char* const am = AM();
                        SetWord(am + 0x9C, Word(am + 0x9C) + 1);
                    }
                }
            }
            ++index;
            S()[1] = 0;
            S()[2] = 0;
            S()[3] = 0;
            S()[4] = 0;
            S()[0x5C] = 0;
        } while (--count != 0);
    }
}

// original 0x52B6C0: GameMode8_Frame's - for each of the 30 Sprite_Objects
// records in use (+0 not 0): Sprite_Current = Field_ActiveMember = it, its
// state through Fish_States by +1 (unchecked: ours aborts past the 7); then,
// Sprite_Current re-read, its +0x5D: with +0 bit 5, 0xC0 above the water
// (+0x3E above 0), 0xC0 - +0x3E / 8 down to -0x200, else 0; and +0x5E = +0x5F
// = +0x5D.
extern "C" void __cdecl Fish_RunAll(void) {
    for (unsigned i = 0; i < kSpriteCount; ++i) {
        unsigned char* const rec = Sprite_Objects + i * kSpriteStride;
        if (rec[0] == 0) continue;
        Sprite_Current = rec;
        Field_ActiveMember = rec;
        Run("Fish_RunAll (0x52B6C0)", Fish_States, Fish_States_count, rec[1], "the record's +1");
        unsigned char* const s = S();
        unsigned char shade = 0;
        if (s[0] & 0x20) {
            const short h = Sh(s, 0x3E);
            if (h > 0) shade = 0xC0;
            else if (h >= -0x200) shade = static_cast<unsigned char>(0xC0 - DivPow2(h, 3));
        }
        s[0x5D] = shade;
        S()[0x5E] = S()[0x5D];
        S()[0x5F] = S()[0x5D];
    }
}

// original 0x52B750: Fish_States[0] - the kind's bank (+0x10); +0x24 = 0, +0x48
// = 2, +0x29 = 5, +9 = +0xB = 0; the member's depth +0x9E = Rand & 3, raised
// to the kind's least (+0x1A), & 3; x = (0x14 - Rand & 0xF) cells; z =
// 0x3E0000 - 0xC0000 * depth - (Rand & 0xF) cells; height -(+0x1A) * 0x100 -
// (Rand & 7) * 0x20; then against AreaMap_Elevation there: a height of 0 is
// -0x20, one below the ground is the ground's; +8 = 4, +0x14 = 0, animation 0,
// +1 up.
extern "C" void __cdecl Fish_Begin(void) {
    Bank(KindWord(S()[6], 0x10));
    S()[0x24] = 0;
    S()[0x48] = 2;
    S()[0x29] = 5;
    S()[9] = 0;
    S()[0xB] = 0;
    const unsigned r1 = Rnd();
    AM()[0x9E] = static_cast<unsigned char>(r1 & 3);
    {
        const unsigned char least = Kind(S()[6], 0x1A);
        unsigned char* const am = AM();
        if (am[0x9E] < least) am[0x9E] = least;
        am[0x9E] = static_cast<unsigned char>(am[0x9E] & 3);
    }
    const unsigned r2 = Rnd();
    SetL(S(), 0x34, (0x14u - (r2 & 0xF)) << 16);
    const U zbase = 0x3E0000u - ((static_cast<U>(AM()[0x9E]) * 3u) << 18);
    const unsigned r3 = Rnd();
    SetL(S(), 0x38, zbase - ((r3 & 0xF) << 16));
    const U top = (0u - static_cast<U>(Kind(S()[6], 0x1A))) << 8;
    const unsigned r4 = Rnd();
    SetWord(S() + 0x3E, top - ((r4 & 7) << 5));
    const short ground = Elevation(static_cast<U>(L(S(), 0x34)), static_cast<U>(L(S(), 0x38)));
    unsigned char* const s = S();
    const short h = Sh(s, 0x3E);
    if (h == 0) SetWord(s + 0x3E, 0xFFE0);
    else if (h < ground) SetWord(s + 0x3E, static_cast<U>(ground));
    S()[8] = 4;
    SetL(S(), 0x14, 0);
    Animation(0);
    unsigned char* const t = S();
    t[1] = static_cast<unsigned char>(t[1] + 1);
}

// original 0x52B8F0: Fish_States[1] - while +9 counts, only the step
// (Fish_Step, Sprite_ScriptTick). Else: the lure in reach: state 3. Else by one
// Rand byte: with no fish nibbling (0x6BC716), the fish at height 0, the byte's
// low two bits clear, the leader not at stage 4 and the kind not 0x16 - the
// nibble: 0x6BC716 = 1, the kind's bank +0x12, sound 0x206 (unless +0 bit 7),
// +0 bit 5, +0x2A, +0x48 cleared, animation 0, +1 up. Otherwise: bit 2 turns
// +8 (bit 3 the way), Fish_Heading, the direction's animation, the climb +0x14
// toward AreaMap_Elevation one step on (by bits 4 and 5 a rise or a dive inside
// the kind's level), bit 6 stops it; +9 = 0x10; the step.
extern "C" void __cdecl Fish_Swim(void) {
    if (S()[9] == 0) {
        if (SH_CALL(Fish_LureInReach)() != 0) {
            S()[1] = 3;
        } else {
            const auto bits = static_cast<unsigned char>(Rnd());
            const unsigned char nibbling = B(kBiteHeld);
            unsigned char* s = S();
            if (nibbling == 0 && Word(s + 0x3E) == 0 && (bits & 3) == 0 && B(kLeaderStage) != 4 && s[6] != 0x16) {
                B(kBiteHeld) = 1;
                Bank(KindWord(s[6], 0x12));
                if ((S()[0] & 0x80) == 0) Sound(0x206);
                s = S();
                s[0] = static_cast<unsigned char>(s[0] & 0xDF);
                S()[0x2A] = 0;
                S()[0x48] = 0;
                Animation(0);
                unsigned char* const t = S();
                t[1] = static_cast<unsigned char>(t[1] + 1);
                return;
            }
            if (bits & 4) s[8] = static_cast<unsigned char>((bits & 8 ? s[8] + 1 : s[8] - 1) & 7);
            SH_CALL(Fish_Heading)();
            DirEnsure();
            s = S();
            const U x = (static_cast<U>(L(s, 0xC)) << 4) + static_cast<U>(L(s, 0x34));
            const U z = (static_cast<U>(L(s, 0x10)) << 4) + static_cast<U>(L(s, 0x38));
            const short ground = Elevation(x, z);
            s = S();
            const short h = Sh(s, 0x3E);
            if (h < ground) {
                SetL(s, 0x14, static_cast<U>(DivPow2(ground - h, 4)));
            } else {
                SetL(s, 0x14, 0);
                if (bits & 0x10) {
                    unsigned char* const c = S();
                    const std::int32_t level = Kind(c[6], 0x1A);
                    if (bits & 0x20) {
                        const short ch = Sh(c, 0x3E);
                        if (ch != 0 && static_cast<std::int32_t>((0u - static_cast<U>(level)) << 8) >= ch + 0x20)
                            SetL(c, 0x14, 2);
                    } else {
                        const std::int32_t below = Sh(c, 0x3E) - 0x20;
                        if (ground <= below && static_cast<std::int32_t>((0xFFFFFFFFu - static_cast<U>(level)) << 8) <= below)
                            SetL(c, 0x14, 0xFFFFFFFEu);
                    }
                }
            }
            if (bits & 0x40) {
                SetL(S(), 0xC, 0);
                SetL(S(), 0x10, 0);
                SetL(S(), 0x14, 0);
            }
            S()[9] = 0x10;
        }
    }
    SH_CALL(Fish_Step)();
    Tick();
}

// original 0x52BB20: Fish_States[2] - Sprite_ScriptTick done: 0x6BC716 = 0, the
// kind's bank +0x10, +0 bit 5 set, +0x2A and the animation by +8, +0x48 = 2,
// the steps 0, 0, -2, +9 = 0x10, state 1.
extern "C" void __cdecl Fish_Settle(void) {
    if (Tick() == 0) return;
    const unsigned kind = S()[6];
    B(kBiteHeld) = 0;
    Bank(KindWord(kind, 0x10));
    unsigned char* s = S();
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    s = S();
    s[0x2A] = DirByte(s[8]);
    S()[0x48] = 2;
    Animation(DirAnim(S()[8]));
    SetL(S(), 0xC, 0);
    SetL(S(), 0x10, 0);
    SetL(S(), 0x14, 0xFFFFFFFEu);
    S()[9] = 0x10;
    S()[1] = 1;
}

// original 0x52BBD0: Fish_States[3] - while +9 counts, the step. Else the lure
// out of reach: state 1 and the step. Else one Rand byte; +8 toward effect
// record 0 (the eight ways by the signs of dx and dz; unchanged on it); by the
// lure's level against the kind's (record 0's +7 - the kind's +0x14 + the
// 20-byte record's nibble, clamped -1..3) a turn from the byte; Fish_Heading,
// the climb toward AreaMap_Elevation then toward record 0's height, the
// direction's animation. Fish_LureClose and a bite (the kind's +0x1C over Rand
// & 0xF, or the record's bit 7) on a level not 9: hooked - the leader to stage
// 4, the sprite index this record, record 0 to state 5 at rest, the fish still,
// state 4, +8 = 1. Else +9 = 0x10, and at level 3 one time in four the steps
// doubled and +9 = 8; the step.
extern "C" void __cdecl Fish_Approach(void) {
    if (S()[9] != 0) {
        SH_CALL(Fish_Step)();
        Tick();
        return;
    }
    if (SH_CALL(Fish_LureInReach)() == 0) {
        S()[1] = 1;
        SH_CALL(Fish_Step)();
        Tick();
        return;
    }
    const unsigned bits = Rnd();
    unsigned char* s = S();
    const auto dx = static_cast<std::int32_t>(static_cast<U>(L(kEff0X)) - static_cast<U>(L(s, 0x34)));
    const auto dz = static_cast<std::int32_t>(static_cast<U>(L(kEff0Z)) - static_cast<U>(L(s, 0x38)));
    int way = -1;
    if (dx < 0) way = dz < 0 ? 0 : dz > 0 ? 6 : 7;
    else if (dx == 0) way = dz < 0 ? 1 : dz > 0 ? 5 : -1;
    else way = dz < 0 ? 2 : dz == 0 ? 3 : 4;
    if (way >= 0) {
        s[8] = static_cast<unsigned char>(way);
        s = S();
    }
    const unsigned nib = RecordB()[0xE] & 0xF;
    const auto diff = static_cast<signed char>(B(kEff0Depth) - Kind(s[6], 0x14 + nib));
    const signed char level = diff < -1 ? -1 : diff > 3 ? 3 : diff;
    switch (level) {
    case -1:
        s[8] = static_cast<unsigned char>(s[8] ^ 4);
        if (bits & 1) {
            unsigned char* const t = S();
            t[8] = static_cast<unsigned char>((bits & 2 ? t[8] + 1 : t[8] - 1) & 7);
        }
        break;
    case 0:
        if (bits & 0xE0) s[8] = static_cast<unsigned char>((bits & 8 ? s[8] + (bits & 3) : s[8] - (bits & 3)) & 7);
        break;
    case 1:
        if ((bits & 0xC0) == 0) s[8] = static_cast<unsigned char>((bits & 8 ? s[8] + (bits & 3) : s[8] - (bits & 3)) & 7);
        break;
    case 2:
        if ((bits & 0xC0) == 0) s[8] = static_cast<unsigned char>((bits & 8 ? s[8] + 1 : s[8] - 1) & 7);
        break;
    default: break;
    }
    SH_CALL(Fish_Heading)();
    s = S();
    const U x = (static_cast<U>(L(s, 0xC)) << 4) + static_cast<U>(L(s, 0x34));
    const U z = (static_cast<U>(L(s, 0x10)) << 4) + static_cast<U>(L(s, 0x38));
    const short ground = Elevation(x, z);
    s = S();
    const short h = Sh(s, 0x3E);
    if (h < ground) {
        SetL(s, 0x14, static_cast<U>(DivPow2(ground - h, 4)));
    } else {
        SetL(s, 0x14, 0);
        unsigned char* const c = S();
        const short lure = static_cast<short>(W(kEff0Height));
        const short ch = Sh(c, 0x3E);
        if (lure < ch) SetL(c, 0x14, 0xFFFFFFFEu);
        else if (lure > ch) SetL(c, 0x14, 2);
    }
    DirEnsure();
    if (SH_CALL(Fish_LureClose)() != 0) {
        const unsigned kind = S()[6];
        const unsigned r = Rnd() & 0xF;
        const unsigned char* rb = RecordB();
        if (static_cast<int>(Kind(kind, 0x1C)) > static_cast<int>(r) || (rb[0xE] & 0x80) != 0) {
            rb = RecordB();
            if (Kind(kind, 0x14 + (rb[0xE] & 0xF)) != 9) {
                unsigned char* const f = S();
                const unsigned char index = f[5];
                B(kLeaderStage) = 4;
                B(kSpriteIndex) = index;
                B(kLeaderStep) = 0;
                B(kEff0State) = 5;
                SetL(kEff0StepX, 0);
                SetL(kEff0StepY, 0);
                SetL(kEff0StepZ, 0);
                SetL(f, 0xC, 0);
                SetL(S(), 0x10, 0);
                SetL(S(), 0x14, 0);
                S()[9] = 0;
                S()[0xA] = 0;
                S()[0xB] = 0;
                unsigned char* const t = S();
                t[1] = static_cast<unsigned char>(t[1] + 1);
                S()[8] = 1;
                DirEnsure();
                Tick();
                return;
            }
        }
    }
    S()[9] = 0x10;
    if (level == 3 && (Rnd() & 3) == 0) {
        unsigned char* t = S();
        SetL(t, 0xC, static_cast<U>(L(t, 0xC)) << 1);
        t = S();
        SetL(t, 0x10, static_cast<U>(L(t, 0x10)) << 1);
        t = S();
        SetL(t, 0x14, static_cast<U>(L(t, 0x14)) << 1);
        S()[9] = 8;
    }
    SH_CALL(Fish_Step)();
    Tick();
}

// original 0x52BF90: Fish_States[4] - the pull. The window: record 5's +0x30
// byte -/+ 4 * (the 10-byte record's +3 + 3), as bytes. z at 0x8000 or less:
// the fish is landed - record 5's +0x10 = -7, record 0 idle, the leader to
// stage 6, state 5. A jump (+4 = 3): Sprite_ScriptTick done brings it down
// (bank +0x10, bit 5, the animation, height -0x20, +4 = 0, +0x48 = 2, +0x10 = 0,
// record 0's bit 6 off); past frame 0x14 the line's tension +0xA moves by +0x10,
// clamped to -0x30..0x30. Otherwise the tension above the window (or below)
// for the 10-byte record's +8 (+9) frames gives Fish_Chance a go: a snap (record
// 5's +0x10 negative and the fish moving) to stage 6 or a break to stage 7.
// With +9 counted out, a new move by the fish's strength tier (+0x9A against
// +0x98) and the kind's chances (+0..+0xF): a jump, a run, a rest or a drift,
// each with +9 = (Rand & 3 + 1) * (8 - tier); else +9 down and the strength
// worn by +0x81. Then the tension clamped, the divisor, the fish pulled toward
// or away from the shore (z, x, the camera's Field_Kind2Z), its direction and
// animation, Sprite_ScriptTick.
extern "C" void __cdecl Fish_Hooked(void) {
    const unsigned char level = B(kEff5Level);
    const auto width = static_cast<unsigned char>((RecordA()[3] + 3) << 2);
    const auto lo = static_cast<unsigned char>(level - width);
    const auto hi = static_cast<unsigned char>(width + level);
    unsigned char* s = S();
    const U chances = kFishKinds + kKindStride * s[6];
    if (L(s, 0x38) <= 0x8000) {
        SetL(kEff5Frame, 0xFFFFFFF9u);
        B(kEff0State) = 0;
        B(kLeaderStage) = 6;
        B(kLeaderStep) = 0;
        s[1] = static_cast<unsigned char>(s[1] + 1);
        return;
    }
    if (s[4] == 3) {
        if (Tick() != 0) {
            Bank(KindWord(S()[6], 0x10));
            unsigned char* t = S();
            t[0] = static_cast<unsigned char>(t[0] | 0x20);
            t = S();
            t[0x2A] = DirByte(t[8]);
            Animation(DirAnim(S()[8]));
            SetWord(S() + 0x3E, 0xFFE0);
            S()[4] = 0;
            S()[0x48] = 2;
            SetL(S(), 0x10, 0);
            B(kEff0Flags) = static_cast<unsigned char>(B(kEff0Flags) & 0xBF);
        }
        unsigned char* const t = S();
        if (Word(t + 0x58) <= 0x14) return;
        t[0xA] = static_cast<unsigned char>(t[0xA] + t[0x10]);
        unsigned char* const c = S();
        const auto a = static_cast<signed char>(c[0xA]);
        c[0xA] = static_cast<unsigned char>(a < -0x30 ? -0x30 : a > 0x30 ? 0x30 : a);
        return;
    }
    unsigned tier = 0;
    if (B(kEff4State) == 0) {
        const auto t = static_cast<signed char>(s[0xA]);
        if (t > static_cast<signed char>(hi)) {
            s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
            s = S();
            if (s[0xB] >= RecordA()[8]) {
                s[0xB] = 0;
                if (L(kEff5Frame) < 0) {
                    s = S();
                    if (L(s, 0x10) > 0) {
                        if (SH_CALL(Fish_Chance)(hi, 1) != 0) {
                            unsigned char* const f = S();
                            B(kEff0State) = 0;
                            SetL(kEff5Frame, 0xFFFFFFF9u);
                            B(kLeaderStage) = 6;
                            B(kLeaderStep) = 0;
                            f[1] = static_cast<unsigned char>(f[1] + 1);
                            return;
                        }
                        s = S();
                    }
                } else {
                    s = S();
                }
            }
        } else if (t < static_cast<signed char>(lo)) {
            s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
            s = S();
            if (s[0xB] >= RecordA()[9]) {
                s[0xB] = 0;
                if (SH_CALL(Fish_Chance)(lo, 2) != 0) {
                    unsigned char* const f = S();
                    B(kLeaderStage) = 7;
                    B(kLeaderStep) = 0;
                    B(kEff0State) = 0;
                    f[1] = static_cast<unsigned char>(f[1] + 1);
                    return;
                }
                s = S();
            }
        } else {
            s[0xB] = 0;
            s = S();
        }
    }
    if (s[9] == 0) {
        unsigned char* const am = AM();
        const short most = Sh(am, 0x98);
        const short now = Sh(am, 0x9A);
        if (now < static_cast<short>(most >> 2)) tier = 3;
        else if (now < static_cast<short>(most >> 1)) tier = 2;
        else tier = now < (most >> 2) * 3 ? 1 : 0;
        const unsigned char* const row = At(chances + 4 * tier);
        if (static_cast<int>(Rnd() & 0xF) < row[0]) {
            unsigned char* const f = S();
            if (Sh(f, 0x3E) > -0x40 && static_cast<signed char>(f[0xA]) > 0) {
                // the jump
                B(kEff0Flags) = static_cast<unsigned char>(B(kEff0Flags) | 0x40);
                Sound(0x206);
                Bank(KindWord(S()[6], 0x12));
                unsigned char* t = S();
                t[0] = static_cast<unsigned char>(t[0] & 0xDF);
                S()[0x2A] = 0;
                S()[0x48] = 0;
                SetWord(S() + 0x3E, 0);
                Animation(0);
                SetL(S(), 0x10, 0xFFFFFFFDu);
                SH_CALL(Fish_AdjustStrength)(static_cast<unsigned>(-5));
                S()[4] = 3;
                return;
            }
        }
        if (static_cast<int>(Rnd() & 0xF) < row[1]) {
            // the run
            unsigned char* const am2 = AM();
            if (Sh(am2, 0x9A) > static_cast<short>(Sh(am2, 0x98) >> 4)) {
                const unsigned r = Rnd();
                unsigned char* f = S();
                SetL(f, 0x10, (r & (Kind(f[6], 0x1D) & 7)) + 1);
                f = S();
                if (static_cast<signed char>(f[0xA]) < 0) {
                    unsigned char* const cell = f + 0x10;
                    const unsigned r2 = Rnd();
                    SetLong(cell, static_cast<std::int32_t>(static_cast<U>(Long(cell)) + (r2 & 1)));
                    f = S();
                }
                const std::int32_t v = L(f, 0x10);
                SetL(f, 0x10, static_cast<U>(v < 0 ? 0 : v > 5 ? 5 : v));
                if (L(kEff5Frame) < 0) {
                    const std::int32_t gap = static_cast<signed char>(S()[0xA]) - static_cast<short>(W(kEff5Level));
                    AM()[0x81] = gap < -0x30 ? 0xFD : gap < -0x18 ? 0xFE : 0xFF;
                }
            } else {
                SetL(S(), 0x10, 0);
                AM()[0x81] = 0;
            }
            unsigned char* f = S();
            if (Kind(f[6], 0x20) == 0) {
                SetL(f, 0xC, 0);
            } else if ((Rnd() & 1) != 0) {
                const unsigned way = Rnd() & 1;
                SetL(S(), 0xC, way != 0 ? 0x1000u : 0xFFFFF000u);
            } else {
                SetL(S(), 0xC, 0);
            }
            S()[8] = 1;
            S()[4] = 0;
        } else if (static_cast<int>(Rnd() & 0xF) < row[2]) {
            // the rest
            SetL(S(), 0xC, 0);
            SetL(S(), 0x10, 0);
            AM()[0x81] = 1;
            S()[4] = 1;
        } else if (S()[0xA] != 0) {
            // the drift
            AM()[0x81] = 0;
            SetL(S(), 0xC, 0);
            SetL(S(), 0x10, 0xFFFFFFFFu);
            S()[4] = 2;
        }
        const auto frames = static_cast<unsigned char>(((Rnd() & 3) + 1) * (8 - tier));
        S()[9] = frames;
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        const auto wear = static_cast<signed char>(AM()[0x81]);
        bool wears;
        if (wear > 0) {
            wears = (Frame_Counter & 0xF) == 0 && L(S(), 0x10) >= 0 && Sh(AM(), 0x9A) < static_cast<short>(Sh(AM(), 0x98) >> 1);
        } else {
            wears = (Frame_Counter & 1) != 0;
        }
        if (wears) SH_CALL(Fish_AdjustStrength)(static_cast<unsigned char>(wear));
        unsigned char* const f = S();
        f[0xA] = static_cast<unsigned char>(f[0xA] + static_cast<unsigned char>(f[0x1C] + f[0x10]));
    }
    unsigned char* f = S();
    {
        const auto a = static_cast<signed char>(f[0xA]);
        f[0xA] = static_cast<unsigned char>(a < -0x30 ? -0x30 : a > 0x30 ? 0x30 : a);
    }
    if (Field_Kind2Hold == 0) MoveScript_F3Divisor = 0;
    f = S();
    const short limit = static_cast<short>(W(kEff5Level));
    const short tension = static_cast<signed char>(f[0xA]);
    if (tension > limit && L(f, 0x10) > 0) {
        // reeled in: toward the shore
        SetL(f, 0x38, static_cast<U>(L(f, 0x38)) - 0x1000);
        unsigned char* e = S();
        if (Sh(e, 0x3E) < 0) {
            SetWord(e + 0x3E, Word(e + 0x3E) + 8);
            e = S();
        }
        const std::int32_t z = L(e, 0x38);
        if (z > 0x150000) {
            Field_Kind2Z = z;
            MoveScript_F3Divisor = 0x10;
            if (z > 0x3C0000) Field_Kind2Z = 0x3C0000;
        }
        const std::int32_t vx = L(e, 0xC);
        bool reread = true;
        if (vx < 0) {
            if (L(e, 0x34) > 0x60000) reread = false;
            else SetL(e, 0xC, 0u - static_cast<U>(vx));
        } else if (vx > 0) {
            if (L(e, 0x34) < 0x140000) reread = false;
            else SetL(e, 0xC, 0u - static_cast<U>(vx));
        } else {
            SetL(e, 0xC, 0);
        }
        if (reread) e = S();
        SetL(e, 0x34, static_cast<U>(L(e, 0x34)) + static_cast<U>(L(e, 0xC)));
        e = S();
        const std::int32_t step = L(e, 0xC);
        e[8] = static_cast<unsigned char>(step < -0x800 ? 0 : step > 0x800 ? 2 : 1);
    } else if (tension < limit && L(kEff5Frame) < 0) {
        // let out: away from the shore, x drawn toward 0xF0000
        const std::int32_t left = 0x3E - static_cast<std::int32_t>(Sh(f, 0x3A));
        if (left == 0) {
            SetL(f, 0xC, 0);
        } else {
            const auto span = static_cast<std::int32_t>(0xF0000u - static_cast<U>(L(f, 0x34))) >> 4;
            SetL(f, 0xC, static_cast<U>(span / left));
            unsigned char* const c = S();
            const std::int32_t v = L(c, 0xC);
            SetL(c, 0xC, static_cast<U>(v < -0x1000 ? -0x1000 : v > 0x1000 ? 0x1000 : v));
        }
        unsigned char* e = S();
        SetL(e, 0x34, static_cast<U>(L(e, 0x34)) + static_cast<U>(L(e, 0xC)));
        e = S();
        if (Sh(e, 0x3E) < 0) {
            SetWord(e + 0x3E, Word(e + 0x3E) + 8);
            e = S();
        }
        SetL(e, 0x38, static_cast<U>(L(e, 0x38)) + 0x1000);
        unsigned char* const c = S();
        const std::int32_t z = L(c, 0x38);
        if (z > 0x150000) {
            Field_Kind2Z = z;
            MoveScript_F3Divisor = 0x10;
            if (z > 0x3C0000) Field_Kind2Z = 0x3C0000;
        }
        c[8] = c[4] != 0 ? 5 : 1;
    }
    DirEnsure();
    Tick();
}

// original 0x52C7C0: Fish_States[5] - jmp through Fish_S5Steps by +2,
// unchecked (ours aborts past its 2).
extern "C" void __cdecl Fish_S5(void) {
    Run("Fish_S5 (0x52C7C0)", Fish_S5Steps, Fish_S5Steps_count, Sprite_Current[2], "Sprite_Current +2");
}

// original 0x52C7E0: Fish_S5Steps[0] - the steps toward the half-cell below x
// and z: ((v & 0xFFFF8000) - v) >> 3 unsigned (a logical shift: a step of
// 0x1FFF.... for any fraction), +9 = 8, +2 up.
extern "C" void __cdecl Fish_S5Center(void) {
    unsigned char* s = S();
    const U x = static_cast<U>(L(s, 0x34));
    SetL(s, 0xC, ((x & 0xFFFF8000u) - x) >> 3);
    s = S();
    const U z = static_cast<U>(L(s, 0x38));
    SetL(s, 0x10, ((z & 0xFFFF8000u) - z) >> 3);
    S()[9] = 8;
    s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x52C830: Fish_S5Steps[1] - x and z moved by the steps, +9 down; at
// 0 the steps cleared, x's and z's low words & 0x8000, state 1 sub-state 0;
// then (a tail jmp) Sprite_ScriptTick.
extern "C" void __cdecl Fish_S5Move(void) {
    unsigned char* s = S();
    SetL(s, 0x34, static_cast<U>(L(s, 0x34)) + static_cast<U>(L(s, 0xC)));
    s = S();
    SetL(s, 0x38, static_cast<U>(L(s, 0x38)) + static_cast<U>(L(s, 0x10)));
    s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] == 0) {
        SetL(S(), 0xC, 0);
        SetL(S(), 0x10, 0);
        s = S();
        SetWord(s + 0x34, Word(s + 0x34) & 0x8000);
        s = S();
        SetWord(s + 0x38, Word(s + 0x38) & 0x8000);
        S()[1] = 1;
        S()[2] = 0;
    }
    Tick();
}

// original 0x52C8B0: Fish_States[6] - jmp through Fish_S6Steps by +2,
// unchecked (ours aborts past its 3).
extern "C" void __cdecl Fish_S6(void) {
    Run("Fish_S6 (0x52C8B0)", Fish_S6Steps, Fish_S6Steps_count, Sprite_Current[2], "Sprite_Current +2");
}

// original 0x52C8D0: Fish_S6Steps[0] - +0x3C..+0x3F cleared, sound 0x206, the
// kind's bank +0x12, +0 bit 5 off, +0x2A and +0x48 cleared, animation 0, +2 up.
extern "C" void __cdecl Fish_S6Begin(void) {
    SetL(S(), 0x3C, 0);
    Sound(0x206);
    Bank(KindWord(S()[6], 0x12));
    unsigned char* s = S();
    s[0] = static_cast<unsigned char>(s[0] & 0xDF);
    S()[0x2A] = 0;
    S()[0x48] = 0;
    Animation(0);
    s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x52C940: Fish_S6Steps[1] - the frame word +0x58 at the kind's word
// of kS6Frames: +2 up; else (a tail jmp) Sprite_ScriptTick.
extern "C" void __cdecl Fish_S6Wait(void) {
    unsigned char* const s = S();
    if (Word(s + 0x58) == Word(At(kS6Frames + 2 * s[6]))) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        return;
    }
    Tick();
}

// original 0x52C970: Fish_S6Steps[2] - the leader's step (ObjTrio +3) above 9:
// the record freed (+0 = 0).
extern "C" void __cdecl Fish_S6Release(void) {
    if (B(kLeaderStep) > 9) S()[0] = 0;
}

// original 0x52C990: the member's strength +0x9A moved by the signed byte
// `delta`, kept in 0..+0x98 (s16).
extern "C" void __cdecl Fish_AdjustStrength(unsigned delta) {
    unsigned char* am = AM();
    SetWord(am + 0x9A, Word(am + 0x9A) + static_cast<U>(static_cast<short>(static_cast<signed char>(delta))));
    am = AM();
    const short now = Sh(am, 0x9A);
    if (now < 0) SetWord(am + 0x9A, 0);
    else if (now > Sh(am, 0x98)) SetWord(am + 0x9A, Word(am + 0x98));
    else SetWord(am + 0x9A, static_cast<U>(now));
}

// original 0x52C990's sibling 0x52C9E0: a chance by the tension's distance
// from `edge` (|+0xA signed - edge's byte|, its low byte) against a band of
// ((the 10-byte record's +3 + 3) * `tier`) << 3 as a byte: below a quarter 1,
// below a half 2, below the band 4, else 0xE at four times it or more and 8
// under; al 1 when that beats Rand & 0xF.
extern "C" unsigned char __cdecl Fish_Chance(unsigned edge, unsigned tier) {
    const std::int32_t d = Abs32(static_cast<U>(static_cast<signed char>(S()[0xA]) - static_cast<std::int32_t>(edge & 0xFF)));
    const auto dist = static_cast<unsigned char>(d);
    const auto band = static_cast<unsigned char>(
        static_cast<unsigned char>(static_cast<signed char>(RecordA()[3] + 3) * static_cast<signed char>(tier)) << 3);
    unsigned odds;
    if (dist < (band >> 2)) odds = 1;
    else if (dist < (band >> 1)) odds = 2;
    else if (dist < band) odds = 4;
    else odds = static_cast<U>(dist) >= (static_cast<U>(band) << 2) ? 0xE : 8;
    const unsigned r = Rnd() & 0xF;
    return odds > r ? 1 : 0;
}

// original 0x52CA80: while +9 counts: +9 down, x += +0xC, z += +0x10, the
// height word += +0x14's low word.
extern "C" void __cdecl Fish_Step(void) {
    unsigned char* s = S();
    if (s[9] == 0) return;
    s[9] = static_cast<unsigned char>(s[9] - 1);
    s = S();
    SetL(s, 0x34, static_cast<U>(L(s, 0x34)) + static_cast<U>(L(s, 0xC)));
    s = S();
    SetL(s, 0x38, static_cast<U>(L(s, 0x38)) + static_cast<U>(L(s, 0x10)));
    s = S();
    SetWord(s + 0x3E, Word(s + 0x3E) + Word(s + 0x14));
}

// original 0x52CAC0: the steps +0xC / +0x10 from MoveCmd_F9Steps' row +8 (read
// in place, the byte unmasked), doubled for directions 2 and 6; x turned back
// at 0x80000 / 0x140000, z inside the kind's band (from +0x1B and the member's
// depth +0x9E, clamped to 0x1A0000..0x3E0000 above and 0x110000..0x320000
// below); then +8 = the row whose steps (each doubled when at most 0x400 in
// size) equal them, 8 when none does.
extern "C" void __cdecl Fish_Heading(void) {
    unsigned char* s = S();
    SetL(s, 0xC, static_cast<U>(Long(At(kF9Steps + 8u * s[8]))));
    s = S();
    SetL(s, 0x10, static_cast<U>(Long(At(kF9Steps + 4 + 8u * s[8]))));
    s = S();
    if (s[8] == 2 || s[8] == 6) {
        SetL(s, 0xC, static_cast<U>(L(s, 0xC)) << 1);
        s = S();
        SetL(s, 0x10, static_cast<U>(L(s, 0x10)) << 1);
        s = S();
    }
    const std::int32_t x = L(s, 0x34);
    if (x <= 0x80000) {
        if (L(s, 0xC) < 0) {
            SetL(s, 0xC, 0u - static_cast<U>(L(s, 0xC)));
            s = S();
        }
    } else if (x >= 0x140000) {
        if (L(s, 0xC) > 0) {
            SetL(s, 0xC, 0u - static_cast<U>(L(s, 0xC)));
            s = S();
        }
    }
    const U band = (static_cast<U>(Kind(s[6], 0x1B)) * 15u) << 16;
    const U depth = (static_cast<U>(AM()[0x9E]) * 3u) << 18;
    std::int32_t top = static_cast<std::int32_t>(band - depth + 0x3E0000u);
    std::int32_t bottom = static_cast<std::int32_t>(0x320000u - depth - band);
    top = top < 0x1A0000 ? 0x1A0000 : top > 0x3E0000 ? 0x3E0000 : top;
    bottom = bottom < 0x110000 ? 0x110000 : bottom > 0x320000 ? 0x320000 : bottom;
    const std::int32_t z = L(s, 0x38);
    if (z >= top) {
        if (L(s, 0x10) > 0) {
            SetL(s, 0x10, 0u - static_cast<U>(L(s, 0x10)));
            s = S();
        }
    } else if (z <= bottom) {
        if (L(s, 0x10) < 0) {
            SetL(s, 0x10, 0u - static_cast<U>(L(s, 0x10)));
            s = S();
        }
    }
    unsigned row = 0;
    for (U at = kF9Steps + 4; at < kF9StepsEnd; at += 8, ++row) {
        const U sx = static_cast<U>(Long(At(at - 4)));
        const U sz = static_cast<U>(Long(At(at)));
        const U wx = Abs32(sx) <= 0x400 ? sx + sx : sx;
        const U wz = Abs32(sz) <= 0x400 ? sz + sz : sz;
        if (static_cast<U>(L(s, 0xC)) == wx && static_cast<U>(L(s, 0x10)) == wz) break;
    }
    s[8] = static_cast<unsigned char>(row);
}

// original 0x52CC40: effect record 0 at state 4, no sprite picked (0x939A1C
// 0xFF), the leader at stage 3: kind 0x16 with the 20-byte record's nibble 4,
// or Sprite_PointInReach(the fish's x, z, height, margin record 0's +7 - the
// record's +0xF + 3, record 0): al 1. Otherwise +1 = 1 and al 0.
extern "C" unsigned char __cdecl Fish_LureInReach(void) {
    if (B(kEff0State) == 4 && B(kSpriteIndex) == 0xFF && B(kLeaderStage) == 3) {
        unsigned char* const s = S();
        const unsigned char* const rb = RecordB();
        if (s[6] == 0x16 && (rb[0xE] & 0xF) == 4) return 1;
        const auto margin = static_cast<unsigned char>(B(kEff0Depth) - rb[0xF] + 3);
        if (SH_CALL(Sprite_PointInReach)(L(s, 0x34), L(s, 0x38), Sh(s, 0x3E), margin, Effect_Objects) != 0) return 1;
    }
    S()[1] = 1;
    return 0;
}

// original 0x52CCD0: effect record 0 at state 4, no sprite picked, record 4
// idle, the leader at stage 3, and the fish within 0x8000 of record 0 in x and
// in z (absolute 32-bit differences) and 0x400 in height (s16): al 1, else 0.
extern "C" unsigned char __cdecl Fish_LureClose(void) {
    if (B(kEff0State) != 4 || B(kSpriteIndex) != 0xFF || B(kEff4State) != 0 || B(kLeaderStage) != 3) return 0;
    const unsigned char* const s = S();
    if (Abs32(static_cast<U>(L(s, 0x34)) - static_cast<U>(L(kEff0X))) >= 0x8000) return 0;
    if (Abs32(static_cast<U>(L(s, 0x38)) - static_cast<U>(L(kEff0Z))) >= 0x8000) return 0;
    if (Abs32(static_cast<U>(Sh(s, 0x3E) - static_cast<short>(W(kEff0Height)))) >= 0x400) return 0;
    return 1;
}

void Rest1G_Inject() {
    if (bof3::WantsShadow("rest_1g")) rest_1g::SelfTest();
    BOF3_INJECT(LeaderPanel_S1);
    BOF3_INJECT(LeaderPanel_S1Begin);
    BOF3_INJECT(LeaderPanel_S1Wait);
    BOF3_INJECT(LeaderPanel_S1Buttons);
    BOF3_INJECT(LeaderPanel_S1Idle);
    BOF3_INJECT(LeaderPanel_S1Box3In);
    BOF3_INJECT(ChoiceMenu_DataPage);
    BOF3_INJECT(ChoiceMenu_RulePage);
    BOF3_INJECT(LeaderPanel_S9Back);
    BOF3_INJECT(LeaderPanel_S10);
    BOF3_INJECT(LeaderPanel_S10Look);
    BOF3_INJECT(LeaderPanel_S10End);
    BOF3_INJECT(LeaderPanel_S11);
    BOF3_INJECT(LeaderPanel_S11FadeOut);
    BOF3_INJECT(LeaderPanel_S11Switch);
    BOF3_INJECT(LeaderPanel_S11FadeIn);
    BOF3_INJECT(LeaderPanel_S11End);
    BOF3_INJECT(LeaderPanel_PoseSound);
    BOF3_INJECT(LeaderPanel_UseItemEnd);
    BOF3_INJECT(LeaderPanel_SetRecords);
    BOF3_INJECT(LeaderPanel_Effect3Mode);
    BOF3_INJECT(LeaderPanel_HoldTest);
    BOF3_INJECT(LeaderPanel_LeaveOnPress);
    BOF3_INJECT(LeaderPanel_EffectsStep);
    BOF3_INJECT(LeaderPanel_PressLatch);
    BOF3_INJECT(Fish_Spawn);
    BOF3_INJECT(Fish_RunAll);
    BOF3_INJECT(Fish_Begin);
    BOF3_INJECT(Fish_Swim);
    BOF3_INJECT(Fish_Settle);
    BOF3_INJECT(Fish_Approach);
    BOF3_INJECT(Fish_Hooked);
    BOF3_INJECT(Fish_S5);
    BOF3_INJECT(Fish_S5Center);
    BOF3_INJECT(Fish_S5Move);
    BOF3_INJECT(Fish_S6);
    BOF3_INJECT(Fish_S6Begin);
    BOF3_INJECT(Fish_S6Wait);
    BOF3_INJECT(Fish_S6Release);
    BOF3_INJECT(Fish_AdjustStrength);
    BOF3_INJECT(Fish_Chance);
    BOF3_INJECT(Fish_Step);
    BOF3_INJECT(Fish_Heading);
    BOF3_INJECT(Fish_LureInReach);
    BOF3_INJECT(Fish_LureClose);
}
