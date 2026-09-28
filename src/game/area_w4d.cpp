// World 4's areas 175..187: the PSX's BIN/WORLD04/AREA175..187.EMI compiled
// into the exe at 0x4292C0..0x42A320 (Area_Descriptors entries 175..187).
// Round ten, group AR4D: the band's 49 functions, none ours before, each read
// to its last instruction with capstone (2026-09-28) and taken through the
// area harness (area_harness.h). docs/area_w4d.md.
//
// Areas 175..185 are one body of code eleven times over: their descriptors
// name the same init, the same two handlers and the same choice table but for
// entry 27, and Area_StepHook / Area_CellHooks name one step hook and one cell
// hook for all eleven. Each area's choice 27 is its own copy, byte for byte
// the same but for its jump table's address; ours is one body behind eleven
// names. The shared bodies are named for the area whose block holds them (the
// tool's unit).
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. Reads
// by an unchecked index into the areas' .data tables are kept where they stay
// in .data; the dispatcher through area 175's glide states aborts past its two
// entries, and area 175's script message aborts on an area number past
// Area_Descriptors (docs/area_w4d.md section 6). Every call goes through the
// harness (AH_CALL / AH_AT), so the start-up fuzz can stand recorders in for
// ours as for the originals' copies; the group's own callee (area 186's start)
// is called the same way, so each function is fuzzed alone.
#include "game/area_w4d.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4d_callees.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4d::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
void AddLong(unsigned char* p, U v) { SetLong(p, static_cast<std::int32_t>(static_cast<U>(Long(p)) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return Effect_Objects + slot * at::kEffectStride; }
// The high word of a 16.16 position, as the step hook reads it (a word at +2).
std::uint16_t High(long v) { return static_cast<std::uint16_t>(static_cast<U>(v) >> 16); }

void SetMessage(unsigned id) { SetWord(Mem(at::kMessage), id); }
unsigned char Answer() { return B(at::kChoiceAnswer); }
unsigned char* StoryFlags() { return Mem(at::kStoryFlags); }
// The focus object's trigger id cleared (0xFF): the focus read before the
// message store, as the originals do.
void DropFocusTrigger(unsigned message) {
    unsigned char* const focus = Ptr(at::kFocusObject);
    SetMessage(message);
    focus[0x86] = 0xFF;
}
// Field_ActiveMember's word +0x8A less 2 (the script position back one op).
void MemberStep() { AddWord(Field_ActiveMember + 0x8A, -2); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, count, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The CRT's _ftol after `fild qword` of a dword and `fadd dword` of a float:
// the sum in double (x87 at 53 bits; every value here is small and exact),
// truncated toward zero to 64 bits, of which the caller stores the low word.
std::uint16_t FtolLow(U whole, U float_cell) {
    float f;
    std::memcpy(&f, Mem(float_cell), sizeof f);
    const double sum = static_cast<double>(whole) + static_cast<double>(f);
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(sum)));
}

// Areas 175..185's choice 27 (eleven copies): the message by the answer read
// signed - 0xFA, 0xFB, 0xFC, 0xFD for 0..3, 0xF9 for 4; anything else (a
// negative answer is above 4 unsigned) leaves the message word alone.
void ChoiceMessageByAnswer() {
    static const std::uint16_t kMessages[5] = {0xFA, 0xFB, 0xFC, 0xFD, 0xF9};
    const auto answer = static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(Answer())));
    if (answer > 4) return;
    SetMessage(kMessages[answer]);
}

// Areas 175 and 184's Zenny question (choices 10 and 14): the answer and
// Party_Zenny at 500 or more (unsigned) pick a message and a byte.
bool HasZenny500() { return static_cast<U>(Long(Mem(at::kZenny))) >= 0x1F4; }

// Area 175's handler 2 and its glide step: x += step << 11, z -= step << 11
// (the step a signed byte of `steps` by `index & 0xF`), each through
// `object`; the callers pass the object and the index read again for the z
// half, as the originals read them.
void SlideBy(unsigned char* object, U steps, unsigned index) {
    const std::int32_t step = static_cast<signed char>(B(steps + (index & 0xF)));
    AddLong(object + 0x34, static_cast<U>(step) << 11);
}
void SlideZBy(unsigned char* object, U steps, unsigned index) {
    const std::int32_t step = static_cast<signed char>(B(steps + (index & 0xF)));
    AddLong(object + 0x38, static_cast<U>(-step) << 11);
}

// Area 186's two walls (tail kind 39's states 0 and 0xA): with the map's
// focus at `focus`, the kind-2 field, the extra object 0 and Sprite_Kind2's z
// all moved by `dz`, MapView_Origin's second word by `dy`, the focus to
// `to`, and MapView_FillCells; then the timer down, and at 0 counter 3 0x1C,
// the timer 0xF and the next state.
void Area186Shift(std::int32_t focus, U dz, int dy, std::int32_t to, unsigned char next) {
    if (Long(Mem(at::kFocusZ)) == focus) {
        AddWord(Mem(at::kOriginY), dy);
        AddLong(Mem(at::kKind2ZField), dz);
        AddLong(Mem(at::kExtra0 + 0x38), dz);
        AddLong(Mem(at::kKind2Z), dz);
        SetLong(Mem(at::kFocusZ), to);
        AH_CALL(MapView_FillCells)();
    }
    AddWord(Mem(at::kTailTimer), -1);
    if (Word(Mem(at::kTailTimer)) != 0) return;
    B(at::kCounter3) = 0x1C;
    SetWord(Mem(at::kTailTimer), 0xF);
    B(at::kTailState) = next;
}
// Tail kind 39's states 1 and 0xB: the timer down, and at 0 area 0xAD at
// (x, z) with `flags`, the tail kind and state 0.
void Area186Leave(std::int32_t x, std::int32_t z, unsigned flags) {
    AddWord(Mem(at::kTailTimer), -1);
    if (Word(Mem(at::kTailTimer)) != 0) return;
    AH_CALL(Field_ChangeArea)(0xAD, x, z, flags);
    B(at::kTailKind) = 0;
    B(at::kTailState) = 0;
}

// Field_ChangeArea to the return point (its area word, x, z) with flags 4.
void ChangeToReturnPoint() {
    AH_CALL(Field_ChangeArea)(Word(Mem(at::kReturnArea)), Long(Mem(at::kReturnX)), Long(Mem(at::kReturnZ)), 4);
}

}  // namespace

// ===========================================================================
// Areas 175..185's shared body (descriptors 0x642450 .. 0x644628; PSX
// 0x801F531C for 175, 0x801F376C .. 0x801F3A44 for 176..185)
// ===========================================================================

// original 0x4292C0 and its ten copies 0x4296A0, 0x429730, 0x4297B0,
// 0x429840, 0x429930, 0x4299B0, 0x429A40, 0x429AA0, 0x429B40, 0x429D10
// (areas 175..185's choice 27, 0x5C bytes each, each with its own five-entry
// jump table at +0x48).
extern "C" void __cdecl Area175_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area176_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area177_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area178_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area179_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area180_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area181_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area182_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area183_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area184_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }
extern "C" void __cdecl Area185_ChoiceMessageByAnswer(void) { ChoiceMessageByAnswer(); }

// original 0x429320 (choice 30 = handler 1 of all eleven; PSX 0x801F3348 /
// area 175's 0x801F3FDC): with Field_Kind2X at 0x2E0000 or less (signed),
// the running object's x is Field_Kind2X.
extern "C" void __cdecl Area175_ClampXToKind2(void) {
    const std::int32_t x = Field_Kind2X;
    if (x > 0x2E0000) return;
    SetLong(Sprite_Current + 0x34, x);
}

// original 0x429630 (choice 10 of all eleven): answer 0: message 0x6F, the
// tail sub-kind 0; else Party_Zenny at 500 or more: message 0x7D, sub-kind
// 1; else message 0x7C and the focus object's trigger id cleared.
extern "C" void __cdecl Area175_ChoiceAskZenny6F(void) {
    if (Answer() == 0) {
        SetMessage(0x6F);
        B(at::kTailSub) = 0;
        return;
    }
    if (HasZenny500()) {
        SetMessage(0x7D);
        B(at::kTailSub) = 1;
        return;
    }
    DropFocusTrigger(0x7C);
}

// original 0x429680 (choice 19 of all eleven): message 0x9F, or 0xA0 for an
// answer not 0.
extern "C" void __cdecl Area175_ChoiceMessage9F(void) { SetMessage(0x9F + (Answer() != 0 ? 1u : 0u)); }

// original 0x429700 (choice 5 of all eleven): answer 0: message 0xFFFF, the
// facility byte +4 cleared; else message 0x5F and the facility byte +2 up
// one.
extern "C" void __cdecl Area176_ChoiceCount3E(void) {
    if (Answer() == 0) {
        SetMessage(0xFFFF);
        B(at::kFacility + 4) = 0;
        return;
    }
    const unsigned char count = B(at::kFacility + 2);
    SetMessage(0x5F);
    B(at::kFacility + 2) = static_cast<unsigned char>(count + 1);
}

// original 0x429790 (choice 26 of all eleven): message 0xF9, or 0xF8 for an
// answer not 0 (`neg; sbb; add 0xF9`).
extern "C" void __cdecl Area177_ChoiceMessageF9(void) { SetMessage(Answer() != 0 ? 0xF8 : 0xF9); }

// original 0x429810 (choice 29 = handler 0 of all eleven; PSX 0x801F3328 /
// area 175's 0x801F3FBC): Sound_PlayById(0x201).
extern "C" void __cdecl Area178_PlaySound201(void) { AH_CALL(Sound_PlayById)(0x201); }

// original 0x429820 (choice 22 of all eleven): message 0xEF; the facility
// byte +2 is 1, or 2 for an answer not 0.
extern "C" void __cdecl Area178_ChoiceMessageEF(void) {
    const unsigned char answer = Answer();
    SetMessage(0xEF);
    B(at::kFacility + 2) = static_cast<unsigned char>((answer != 0 ? 1 : 0) + 1);
}

// original 0x4298A0 (the init of all eleven; PSX 0x801F32D8 / area 175's
// 0x801F44D8): Cond_ByteFA above 7 (signed): the engine's field reset
// 0x455450. The hook's arm byte cleared; Cond_ByteFD not 0:
// Sound_PlayEffect(0x20C).
extern "C" void __cdecl Area179_Init(void) {
    if (Cond_ByteFA > 7) AH_AT(void (__cdecl*)(void), at::kFieldReset)();
    const unsigned char fd = Cond_ByteFD;
    B(at::kHookArmed) = 0;
    if (fd != 0) AH_CALL(Sound_PlayEffect)(0x20C);
}

// original 0x4298D0 (Area_CellHooks' handler for all eleven): the cell's x
// word 0x3F, z word 0xE and the leader's pose 1: tail kind 0x1A (Cond_ByteFA
// above 7, signed) or 6, al 1; else al 0.
extern "C" unsigned char __cdecl Area179_CellHook(unsigned x, unsigned z) {
    if (static_cast<std::uint16_t>(x) != 0x3F) return 0;
    if (static_cast<std::uint16_t>(z) != 0xE) return 0;
    if (B(at::kLeaderPose) != 1) return 0;
    B(at::kTailKind) = Cond_ByteFA > 7 ? 0x1A : 6;
    return 1;
}

// original 0x429910 (choice 0 of all eleven): message 0xFFFF; the byte after
// MoveScript_Var7 is 3 for answer 0, else 2.
extern "C" void __cdecl Area179_ChoiceVar8(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    B(at::kVar8) = static_cast<unsigned char>((answer == 0 ? 1 : 0) + 2);
}

// original 0x429990 (choice 24 of all eleven): message 0xFFFF; the facility
// byte +1 is the answer.
extern "C" void __cdecl Area180_ChoiceStoreAnswer(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    B(at::kFacility + 1) = answer;
}

// original 0x429A10 (choice 1 of all eleven): message 0xFFFF; answer 0:
// counter 0 = 0x28 and the byte after MoveScript_Var7 0x15; else 0x32 and
// 0x19.
extern "C" void __cdecl Area181_ChoiceCounter0(void) {
    const unsigned char answer = Answer();
    SetMessage(0xFFFF);
    if (answer == 0) {
        B(at::kCounter0) = 0x28;
        B(at::kVar8) = 0x15;
    } else {
        B(at::kCounter0) = 0x32;
        B(at::kVar8) = 0x19;
    }
}

// original 0x429B00 (choice 20 of all eleven): answer 0: message 0x78, the
// facility byte +4 0xB; else 0x72 and 0xC; the facility byte +3 0 either
// way.
extern "C" void __cdecl Area183_ChoiceMessage78(void) {
    if (Answer() == 0) {
        SetMessage(0x78);
        B(at::kFacility + 4) = 0xB;
    } else {
        SetMessage(0x72);
        B(at::kFacility + 4) = 0xC;
    }
    B(at::kFacility + 3) = 0;
}

// original 0x429BA0 (choice 2 of all eleven): answer 1: message 0x5A and the
// focus object's trigger id cleared; else 0xFFFF.
extern "C" void __cdecl Area184_ChoiceMessage5A(void) {
    if (Answer() == 1) {
        DropFocusTrigger(0x5A);
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x429BD0 (choice 3 of all eleven): answer 1: message 0x5F and the
// tail state 1; else 0xFFFF.
extern "C" void __cdecl Area184_ChoiceTailState1(void) {
    if (Answer() == 1) {
        SetMessage(0x5F);
        B(at::kTailState) = 1;
        return;
    }
    SetMessage(0xFFFF);
}

// original 0x429C00 (choice 6 of all eleven): answer 0: message 0x7A; else
// 0x6E and the focus object's trigger id cleared.
extern "C" void __cdecl Area184_ChoiceMessage7A(void) {
    if (Answer() == 0) {
        SetMessage(0x7A);
        return;
    }
    DropFocusTrigger(0x6E);
}

// original 0x429C30 (choice 9 of all eleven): answer 0: message 0xFFFF;
// else 0x77 and the facility byte +4 0xA.
extern "C" void __cdecl Area184_ChoiceMessage77(void) {
    if (Answer() == 0) {
        SetMessage(0xFFFF);
        return;
    }
    SetMessage(0x77);
    B(at::kFacility + 4) = 0xA;
}

// original 0x429C60 (choice 14 of all eleven): answer not 0: message 0x7B,
// the facility byte +5 1; else Party_Zenny at 500 or more: 0x7F and 0; else
// 0x7C and 1.
extern "C" void __cdecl Area184_ChoiceAskZenny7F(void) {
    if (Answer() != 0) {
        SetMessage(0x7B);
        B(at::kFacility + 5) = 1;
        return;
    }
    if (HasZenny500()) {
        SetMessage(0x7F);
        B(at::kFacility + 5) = 0;
        return;
    }
    SetMessage(0x7C);
    B(at::kFacility + 5) = 1;
}

// original 0x429CB0 (choice 15 of all eleven): answer 0: message 0x89; else
// 0x88 and the facility byte +2 2.
extern "C" void __cdecl Area184_ChoiceMessage89(void) {
    if (Answer() == 0) {
        SetMessage(0x89);
        return;
    }
    SetMessage(0x88);
    B(at::kFacility + 2) = 2;
}

// original 0x429CE0 (choice 21 of all eleven): answer 0: message 0xFFFF;
// else 0xF7 and the focus object's trigger id cleared.
extern "C" void __cdecl Area184_ChoiceMessageF7(void) {
    if (Answer() == 0) {
        SetMessage(0xFFFF);
        return;
    }
    DropFocusTrigger(0xF7);
}

// original 0x429D70 (choice 28 of all eleven): answer 0 or 1: message 0xFFFF
// and the facility byte +0 the answer; else 0x2A1 and the focus object's
// trigger id cleared.
extern "C" void __cdecl Area185_ChoiceSetFacility0(void) {
    const unsigned char answer = Answer();
    if (answer == 0 || answer == 1) {
        SetMessage(0xFFFF);
        B(at::kFacility) = answer;
        return;
    }
    DropFocusTrigger(0x2A1);
}

// original 0x429DC0 (Area_StepHook's case for areas 175..185): x's high word
// 0x41..0x44 and z's 0x14..0x17 (16-bit): if the arm byte is set,
// Sound_PlayEffect(0x20C), the byte cleared, Field_ChangeArea to the return
// point with flags 4; off the square the byte set. al 0 always.
extern "C" unsigned char __cdecl Area185_StepHook(long x, long z) {
    if (static_cast<std::uint16_t>(High(x) - 0x41) > 3 || static_cast<std::uint16_t>(High(z) - 0x14) > 3) {
        B(at::kHookArmed) = 1;
        return 0;
    }
    if (B(at::kHookArmed) == 0) return 0;
    B(at::kHookArmed) = 0;
    AH_CALL(Sound_PlayEffect)(0x20C);
    ChangeToReturnPoint();
    return 0;
}

// original 0x429E20 (EffectKind18_States[103], 0x654208; Sprite_Current an
// effect record; a gap of the tool): MapView_ScreenXY = (-12544, -14848); a
// draw mode (tpage 0xB5, dtd 1) linked at the record (size 0xC). Then 32 rows
// of 8 semi-transparent TILE_1s, the row counting r = 0, -8, .. -0xF8 and
// the column u = 0, 0x10, .. 0x1F0 with it; the shade s = (r -
// (Frame_Counter & 7) + 0x17F) >> 1 (blue s, red and green s / 4);
// for k = 0 .. 7 the angle a = k * 0x200 + 0x100 and m = (Frame_Counter &
// 0xF) + u (the counter read once, after the first sine): the vertex
// ((sin a * m * 5) >> 16 + screen x, (cos a * m * 5) >> 16 + screen y, -0x340
// - sin(m * 5) >> 5), each of the first two through x87 and _ftol; the vertex
// through Gte_RotTransPers into the tile, the depth by Gte_StoreDepthF at
// +0x10, linked at the record (size 0x14). A draw mode (tpage 0xB5) linked
// last.
extern "C" void __cdecl Area185_DrawTileField(void) {
    unsigned char* const first = Gfx_PacketNext;
    SetLong(Mem(at::kScreenXY), static_cast<std::int32_t>(0xC6440000u));
    SetLong(Mem(at::kScreenXY + 4), static_cast<std::int32_t>(0xC6680000u));
    AH_CALL(Gpu_SetDrawMode)(first, 0, 1, 0xB5, 0);
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(Sprite_Current + 0x34)), static_cast<U>(Long(Sprite_Current + 0x38)), 1, 0xC);
    U u = 0;
    std::int32_t row = 0;
    long depth = 0;
    do {
        for (std::int32_t k = 0; k < 0x8000; k += 0x1000) {
            unsigned char* const tile = Gfx_PacketNext;
            AH_CALL(Gpu_SetTile1)(tile);
            AH_CALL(Gpu_SetSemiTrans)(tile, 1);
            const U shade = (static_cast<U>(row) - (static_cast<U>(Frame_Counter) & 7) + 0x17F) >> 1;
            tile[6] = static_cast<unsigned char>(shade);
            const auto quarter = static_cast<unsigned char>(static_cast<std::int32_t>(shade) / 4);
            tile[4] = quarter;
            tile[5] = quarter;
            const std::int32_t angle = k / 8 + 0x100;
            const int sine = AH_CALL(Math_Sin)(angle);
            const U m = (static_cast<U>(Frame_Counter) & 0xF) + u;
            SetWord(Mem(at::kVertex), FtolLow((static_cast<U>(sine) * m * 5u) >> 16, at::kScreenXY));
            const int cosine = AH_CALL(Math_Cos)(angle);
            SetWord(Mem(at::kVertex + 2), FtolLow((static_cast<U>(cosine) * m * 5u) >> 16, at::kScreenXY + 4));
            const int lift = AH_CALL(Math_Sin)(static_cast<int>(m * 5u));
            SetWord(Mem(at::kVertex + 4), static_cast<U>(-0x340 - (lift >> 5)));
            AH_CALL(Gte_RotTransPers)(reinterpret_cast<const short*>(Mem(at::kVertex)), reinterpret_cast<unsigned long*>(tile + 8), &depth);
            AH_CALL(Gte_StoreDepthF)(reinterpret_cast<float*>(tile + 0x10));
            AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(Sprite_Current + 0x34)), static_cast<U>(Long(Sprite_Current + 0x38)), 1, 0x14);
        }
        row -= 8;
        u += 0x10;
    } while (row > -0x100);
    AH_CALL(Gpu_SetDrawMode)(Gfx_PacketNext, 0, 0, 0xB5, 0);
    AH_CALL(MapView_LinkPrimAt)(static_cast<U>(Long(Sprite_Current + 0x34)), static_cast<U>(Long(Sprite_Current + 0x38)), 1, 0xC);
}

// ===========================================================================
// Area 175's own (descriptor 0x642450; PSX 0x801F531C): handlers 2..7
// (choices 31..36), its glide states, object trigger 64
// ===========================================================================

// original 0x429340 (handler 2; PSX 0x801F400C): Field_Request 2: the
// running object slid by Area175_SlideSteps[Frame_Counter & 0xF] (x plus,
// z minus, << 11; the object and the counter read again for z) and the
// active member's script back one op; else the object placed at (0x2C0000,
// 0x5C0000) with its state +4 0 (Sprite_Current read again for each store).
extern "C" void __cdecl Area175_SlideOrPlace(void) {
    const unsigned char request = Field_Request;
    unsigned char* const cur = Sprite_Current;
    if (request == 2) {
        SlideBy(cur, at::kArea175SlideSteps, static_cast<U>(Frame_Counter));
        SlideZBy(Sprite_Current, at::kArea175SlideSteps, static_cast<U>(Frame_Counter));
        MemberStep();
        return;
    }
    SetLong(cur + 0x34, 0x2C0000);
    SetLong(Sprite_Current + 0x38, 0x5C0000);
    Sprite_Current[4] = 0;
}

// original 0x4293C0 (handler 3; PSX 0x801F40AC): Effect_FindFree to the
// running object's +0xB (Sprite_Current read again after the call); none
// (0xFF): the active member's script back one op; else that record +0 1,
// kind +5 0x1F, at (0x2E0000, 0x5C0000) height 0x1A00000 (the slot read
// again from +0xB for each store).
extern "C" void __cdecl Area175_SpawnEffect1F(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    Sprite_Current[0xB] = slot;
    unsigned char* const cur = Sprite_Current;
    if (cur[0xB] == 0xFF) {
        MemberStep();
        return;
    }
    EffectAt(cur[0xB])[0] = 1;
    EffectAt(cur[0xB])[5] = 0x1F;
    SetLong(EffectAt(cur[0xB]) + 0x34, 0x2E0000);
    SetLong(EffectAt(cur[0xB]) + 0x38, 0x5C0000);
    SetLong(EffectAt(cur[0xB]) + 0x3C, 0x1A00000);
}

// original 0x429440 (handler 4; PSX 0x801F4204): jmp [Sprite_Current[4] * 4
// + Area175_GlideStates] (unchecked; ours aborts past its two entries).
extern "C" void __cdecl Area175_GlideRun(void) {
    StateEntry("Area175_GlideRun", at::kArea175GlideStates, at::kArea175GlideStateCount, Sprite_Current[4])();
}

// original 0x429460 (Area175_GlideStates[0]): the count +0xA 0x20, state +4
// 1 (Sprite_Current read again).
extern "C" void __cdecl Area175_GlideBegin20(void) {
    Sprite_Current[0xA] = 0x20;
    Sprite_Current[4] = 1;
}

// original 0x429480 (Area175_GlideStates[1]): the count less 1; at 0 the
// state 0; else x plus and z minus Area175_GlideSteps[count & 0xF] << 11
// (Sprite_Current read again for z) and the active member's script back one
// op.
extern "C" void __cdecl Area175_GlideStep(void) {
    Sprite_Current[0xA] = static_cast<unsigned char>(Sprite_Current[0xA] - 1);
    unsigned char* const cur = Sprite_Current;
    if (cur[0xA] == 0) {
        cur[4] = 0;
        return;
    }
    SlideBy(cur, at::kArea175GlideSteps, cur[0xA]);
    unsigned char* const again = Sprite_Current;
    SlideZBy(again, at::kArea175GlideSteps, again[0xA]);
    MemberStep();
}

// original 0x4294F0 (handler 5; PSX 0x801F431C): Effect_FindFree; a slot: +0
// 1, kind +5 0x39, its words +0x2E / +0x30 the running object's (Sprite_Current
// read again for each).
extern "C" void __cdecl Area175_SpawnEffect39(void) {
    const unsigned char slot = AH_CALL(Effect_FindFree)();
    if (slot == 0xFF) return;
    unsigned char* const e = EffectAt(slot);
    e[0] = 1;
    e[5] = 0x39;
    SetWord(e + 0x2E, Word(Sprite_Current + 0x2E));
    SetWord(e + 0x30, Word(Sprite_Current + 0x30));
}

// original 0x429540 (handler 6; PSX 0x801F439C): the operand byte two past
// the script object's position word +0xA in the running area's +0x10 script
// [object +3] picks a word of Area175_ScriptMessages (unchecked, in .data):
// Msg_OpenScript(it); then Field_Request 2, the script object's +0 bit 0x10
// cleared and 0x20 set, its position on 1 (the object read again for each).
// The script table and the script are indexed unchecked, as the
// movement-script engine indexes them; an area number past Area_Descriptors'
// 200 aborts where the original reads past the table.
extern "C" void __cdecl Area175_OpenScriptMessage(void) {
    const unsigned area = Game_AreaNumber;
    if (area >= at::kDescriptorCount)
        bof3::Fatal("Area175_OpenScriptMessage: Game_AreaNumber %u past Area_Descriptors' %u", area, at::kDescriptorCount);
    const U descriptor = static_cast<U>(Long(Mem(at::kDescriptors + area * 4)));
    unsigned char* const object = MoveScript_Object;
    const U scripts = static_cast<U>(Long(Mem(descriptor + 0x10)));
    const U script = static_cast<U>(Long(Mem(scripts + object[3] * 4u)));
    const unsigned char operand = B(script + Word(object + 0xA) + 2);
    AH_CALL(Msg_OpenScript)(static_cast<unsigned short>(Word(Mem(at::kArea175ScriptMessages + operand * 2u))));
    Field_Request = 2;
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] & 0xEF);
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x20);
    AddWord(MoveScript_Object + 0xA, 1);
}

// original 0x4295B0 (handler 7; PSX 0x801F446C): the script object's +0 bit
// 0x40 set; the running object's +7 bit 8 and +0 bit 0x10 set
// (Sprite_Current read again); Sprite_SetAnimation(0xA).
extern "C" void __cdecl Area175_PoseAnimA(void) {
    MoveScript_Object[0] = static_cast<unsigned char>(MoveScript_Object[0] | 0x40);
    Sprite_Current[7] = static_cast<unsigned char>(Sprite_Current[7] | 8);
    Sprite_Current[0] = static_cast<unsigned char>(Sprite_Current[0] | 0x10);
    AH_CALL(Sprite_SetAnimation)(0xA);
}

// original 0x4295E0 (Field_ObjectTriggers id 64; a gap of the tool, area
// 175's block): the object +1 4, +0x84 2, +0x83 0xD, word +0x8A 0; story
// flag 0x95; Party_DropIn(4); counter 0 cleared; al 0.
extern "C" unsigned char __cdecl Area175_Trigger64(unsigned char* object, unsigned char*) {
    object[1] = 4;
    object[0x84] = 2;
    object[0x83] = 0xD;
    SetWord(object + 0x8A, 0);
    AH_CALL(Flags_Set)(StoryFlags(), 0x95);
    AH_CALL(Party_DropIn)(4);
    B(at::kCounter0) = 0;
    return 0;
}

// ===========================================================================
// Area 186 (descriptor 0x645228; PSX 0x801F3B34): the init, its start, tail
// kind 39
// ===========================================================================

// original 0x42A000 (Field_ModeTailKinds[39]): the s8 state through the byte
// table at 0x42A170 (12 entries) and the jump table at 0x42A15C (5), both in
// the body: 0 the wall shift down (focus 0x4400: z less 0x1E0000, the origin
// word less 0x1E, focus 0x6200) then state 1; 1 the leave to area 0xAD at
// (0x120000, 0x460000) flags 0x81; 0xA the shift up (focus 0x6C00: z plus,
// origin plus, focus 0x4E00) then state 0xB; 0xB the leave at (0x310000,
// 0x600000) flags 0x82; 2..9 and anything else (negative, above 0xB)
// nothing.
extern "C" void __cdecl Area186_TailShift(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0: Area186Shift(0x4400, static_cast<U>(-0x1E0000), -0x1E, 0x6200, 1); break;
    case 1: Area186Leave(0x120000, 0x460000, 0x81); break;
    case 0xA: Area186Shift(0x6C00, 0x1E0000, 0x1E, 0x4E00, 0xB); break;
    case 0xB: Area186Leave(0x310000, 0x600000, 0x82); break;
    default: break;
    }
}

// original 0x42A180 (init; PSX 0x801F2F24): story flag 0x58 set: the byte
// after Field_Kind2X 1: Area186_Start(0xA); 2: Area186_Start(0).
extern "C" void __cdecl Area186_Init(void) {
    if (AH_CALL(Flags_Test)(StoryFlags(), 0x58) == 0) return;
    const unsigned char mode = B(at::kKind2Mode);
    if (mode == 1) AH_CALL(Area186_Start)(0xA);
    else if (mode == 2) AH_CALL(Area186_Start)(0);
}

// original 0x42A1C0 (called by Area186_Init; not in the PSX pairing): the
// camera angles (0x100, 0, 0) and distance 0x100; the extra object 0's x
// 0x48000; state 0: its z 0xA0000, +0x83 1, Kind2_Place(0); else z 0x640000,
// +0x83 2, Kind2_Place(1). Then the tail's state the argument, its timer
// 0x96, kind 39.
extern "C" void __cdecl Area186_Start(unsigned char state) {
    SetWord(Mem(at::kCameraAngles), 0x100);
    SetWord(Mem(at::kCameraAngles + 4), 0);
    SetWord(Mem(at::kCameraAngles + 2), 0);
    SetWord(Mem(at::kCameraDistance), 0x100);
    SetLong(Mem(at::kExtra0 + 0x34), 0x48000);
    if (state == 0) {
        SetLong(Mem(at::kExtra0 + 0x38), 0xA0000);
        B(at::kExtra0 + 0x83) = 1;
        AH_CALL(Kind2_Place)(0);
    } else {
        SetLong(Mem(at::kExtra0 + 0x38), 0x640000);
        B(at::kExtra0 + 0x83) = 2;
        AH_CALL(Kind2_Place)(1);
    }
    B(at::kTailState) = state;
    SetWord(Mem(at::kTailTimer), 0x96);
    B(at::kTailKind) = 0x27;
}

// ===========================================================================
// Area 187 (descriptor 0x645A70; no PSX pairing): choice 0 (choice 1 is
// Area130_ChoiceTailState2, AR3C's, read in place), object trigger 14, tail
// kind 29
// ===========================================================================

// original 0x42A240 (choice 0): the answer read signed picks a byte pair of
// Area187_FocusPairs (unchecked; any s8 stays in .data): the focus object's
// dwords +0x18 and +0x1C the pair's two bytes (the focus and the answer read
// again for the second); message 0xFFFF between.
extern "C" void __cdecl Area187_ChoiceFocusPair(void) {
    const std::int32_t first = static_cast<signed char>(Answer());
    unsigned char* const focus = Ptr(at::kFocusObject);
    const unsigned char x = B(at::kArea187FocusPairs + static_cast<U>(first * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, x);
    unsigned char* const again = Ptr(at::kFocusObject);
    const std::int32_t second = static_cast<signed char>(Answer());
    SetLong(again + 0x1C, B(at::kArea187FocusPairs + static_cast<U>(second * 2) + 1));
}

// original 0x42A280 (Field_ObjectTriggers id 14; a gap of the tool): tail
// kind 0x1D; al 0.
extern "C" unsigned char __cdecl Area187_Trigger14(unsigned char*, unsigned char*) {
    B(at::kTailKind) = 0x1D;
    return 0;
}

// original 0x42A290 (Field_ModeTailKinds[29]; a gap of the tool, armed by
// Area187_Trigger14): the s8 state - 0: ScriptFlags_Set40, Msg_OpenScript(2),
// Field_Request 2, the state (read again) on 1; 1: Field_Request not 2:
// Area131_DisarmTail (a tail jmp); 2: Field_Request not 2:
// Area131_DisarmTail, the byte 0x904152 cleared, story flag 0x77 cleared,
// Field_ChangeArea to the return point with flags 4.
extern "C" void __cdecl Area187_TailMessage2(void) {
    switch (static_cast<signed char>(B(at::kTailState))) {
    case 0: {
        AH_CALL(ScriptFlags_Set40)();
        AH_CALL(Msg_OpenScript)(2);
        const unsigned char state = B(at::kTailState);
        Field_Request = 2;
        B(at::kTailState) = static_cast<unsigned char>(state + 1);
        break;
    }
    case 1:
        if (Field_Request != 2) AH_CALL(Area131_DisarmTail)();
        break;
    case 2:
        if (Field_Request == 2) break;
        AH_CALL(Area131_DisarmTail)();
        B(at::kReturnByte) = 0;
        AH_CALL(Flags_Clear)(StoryFlags(), 0x77);
        ChangeToReturnPoint();
        break;
    default: break;
    }
}

void AreaW4d_Inject() {
    if (bof3::WantsShadow("area_w4d")) area_w4d::SelfTest();
    BOF3_INJECT(Area175_ChoiceMessageByAnswer);
    BOF3_INJECT(Area175_ClampXToKind2);
    BOF3_INJECT(Area175_SlideOrPlace);
    BOF3_INJECT(Area175_SpawnEffect1F);
    BOF3_INJECT(Area175_GlideRun);
    BOF3_INJECT(Area175_GlideBegin20);
    BOF3_INJECT(Area175_GlideStep);
    BOF3_INJECT(Area175_SpawnEffect39);
    BOF3_INJECT(Area175_OpenScriptMessage);
    BOF3_INJECT(Area175_PoseAnimA);
    BOF3_INJECT(Area175_Trigger64);
    BOF3_INJECT(Area175_ChoiceAskZenny6F);
    BOF3_INJECT(Area175_ChoiceMessage9F);
    BOF3_INJECT(Area176_ChoiceMessageByAnswer);
    BOF3_INJECT(Area176_ChoiceCount3E);
    BOF3_INJECT(Area177_ChoiceMessageByAnswer);
    BOF3_INJECT(Area177_ChoiceMessageF9);
    BOF3_INJECT(Area178_ChoiceMessageByAnswer);
    BOF3_INJECT(Area178_PlaySound201);
    BOF3_INJECT(Area178_ChoiceMessageEF);
    BOF3_INJECT(Area179_ChoiceMessageByAnswer);
    BOF3_INJECT(Area179_Init);
    BOF3_INJECT(Area179_CellHook);
    BOF3_INJECT(Area179_ChoiceVar8);
    BOF3_INJECT(Area180_ChoiceMessageByAnswer);
    BOF3_INJECT(Area180_ChoiceStoreAnswer);
    BOF3_INJECT(Area181_ChoiceMessageByAnswer);
    BOF3_INJECT(Area181_ChoiceCounter0);
    BOF3_INJECT(Area182_ChoiceMessageByAnswer);
    BOF3_INJECT(Area183_ChoiceMessageByAnswer);
    BOF3_INJECT(Area183_ChoiceMessage78);
    BOF3_INJECT(Area184_ChoiceMessageByAnswer);
    BOF3_INJECT(Area184_ChoiceMessage5A);
    BOF3_INJECT(Area184_ChoiceTailState1);
    BOF3_INJECT(Area184_ChoiceMessage7A);
    BOF3_INJECT(Area184_ChoiceMessage77);
    BOF3_INJECT(Area184_ChoiceAskZenny7F);
    BOF3_INJECT(Area184_ChoiceMessage89);
    BOF3_INJECT(Area184_ChoiceMessageF7);
    BOF3_INJECT(Area185_ChoiceMessageByAnswer);
    BOF3_INJECT(Area185_ChoiceSetFacility0);
    BOF3_INJECT(Area185_StepHook);
    BOF3_INJECT(Area185_DrawTileField);
    BOF3_INJECT(Area186_TailShift);
    BOF3_INJECT(Area186_Init);
    BOF3_INJECT(Area186_Start);
    BOF3_INJECT(Area187_ChoiceFocusPair);
    BOF3_INJECT(Area187_Trigger14);
    BOF3_INJECT(Area187_TailMessage2);
}
