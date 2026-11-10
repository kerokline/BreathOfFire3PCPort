// Moved here by tools/move_funcs.py from effect_1e.cpp, effect_1f.cpp, field_e1.cpp, rest_1f.cpp, rest_1g.cpp (docs/source-layout-round.md).
#include "game/field/leader_panel.h"

#include <bit>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1e.h"
#include "game/effect_1e_callees.h"
#include "game/effect_1f.h"
#include "game/effect_1f_callees.h"
#include "game/field/leader_panel_callees.h"
#include "game/field_e1_callees.h"
#include "game/lang_layout.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_0a.h"
#include "game/rest_1f.h"
#include "game/scenario_harness.h"
#include "game/sound.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"
#include "input/sigma.h"

namespace {

namespace at = leader_panel::at;

}  // namespace

// Group HT of the button round: a face or shoulder bit the original tests by
// a constant is Japan's, asked through σ (docs/button-tests.md).
using bof3x::input::sigma::Sigma;

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

using move_script::SetLong;

using move_script::Word;

using Handler = void (__cdecl*)();

unsigned char* S() { return Sprite_Current; }

unsigned char& B(U address) { return move_script::At(address)[0]; }

std::int32_t L(U address) { return Long(At(address)); }

void SetL(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }

U Key(const void* p) { return Low32(p); }

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
    const unsigned n = Effect1a_PanelObject;
    if (n >= at::kSpriteCount)
        bof3::Fatal("%s: the sprite index 0x939A1C is %u, past the %u Sprite_Objects records - the original reads 0x%X "
                    "(docs/effect_1e.md section 6)",
                    who, n, at::kSpriteCount, (unsigned)(Key(rv::Bytes(Sprite_Objects)) + n * effect_1e::at::kSpriteStride));
    return rv::Bytes(Sprite_Objects) + n * effect_1e::at::kSpriteStride;
}

unsigned char PickedKind(const char* who) { return Picked(who)[6]; }

// The two record pointers the stages read through (0x52B250 sets them to image
// tables; read as they are).
unsigned char* RecordA() { return reinterpret_cast<unsigned char*>(Effect1a_PanelScript); }

unsigned char* RecordB() { return reinterpret_cast<unsigned char*>(Effect1a_PanelSet); }

// A message of the script pools: MessagePools + the u16 at `cell`.
const unsigned char* Pool(U cell) { return At(::bof3::Img(effect_1e::at::kPools.v) + Word(At(cell))); }

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

void BoxPrims(U y) { SH_AT(void (__cdecl*)(int, int, int, int, int), at::kBoxPrims.v)(0x14, static_cast<int>(y), 0x118, 0x13, 0); }

void MusicTo(unsigned track) {
    SH_CALL(Music_FadeOutStop)(0xA);
    SH_CALL(Music_Play)(track, 8);
}

void Effect3Mode(unsigned mode) { SH_AT(void (__cdecl*)(unsigned), at::kEffect3Mode.v)(mode); }

void PoseSound() { SH_AT(void (__cdecl*)(), at::kPoseSound.v)(); }

// The stage-4 panel's first three draws, as seven of its steps begin: the
// header at x 0, the picked record's kind icon at 0x80, its row (the kind, the
// count +0x9C, row 0).
void PanelHead(const char* who) {
    Header(0);
    Icon(0x80, PickedKind(who));
    unsigned char* const p = Picked(who);
    KindRow(p[6], p[0x9C], 0);
}

// 0x5296F9 and 0x5297C4's pull: the picked record's +0x1C moved by 0xFE less
// the A record's +3.
void Pull(const char* who) {
    unsigned char* const p = Picked(who);
    const unsigned char step = RecordA()[3];
    p[0x1C] = static_cast<unsigned char>(p[0x1C] + static_cast<unsigned char>(0xFE - step));
}

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

// 0x52A2B0 / 0x52A310: effect record 4 at 3 - track 0x28, sound 0x203, record
// 3 mode `mode`, +3 up; Sprite_ScriptTick.
void WaitThenMode(unsigned mode) {
    if (Effect_Objects[4].state == 3) {
        MusicTo(0x28);
        Sound(0x203);
        Effect3Mode(mode);
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] + 1);
    }
    Tick();
}

using SEffect1f = std::int32_t;

unsigned char* AtEffect1f(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

unsigned char& BEffect1f(U a) { return *AtEffect1f(a); }

std::uint16_t WordEffect1f(U a) {
    std::uint16_t v;
    std::memcpy(&v, AtEffect1f(a), 2);
    return v;
}

void SetWord(U a, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(AtEffect1f(a), &w, 2);
}

U LongEffect1f(U a) {
    U v;
    std::memcpy(&v, AtEffect1f(a), 4);
    return v;
}

void SetLongEffect1f(U a, U v) { std::memcpy(AtEffect1f(a), &v, 4); }

// Effect record 6's cells, the menu's state.
unsigned char& R6(unsigned offset) { return BEffect1f(::bof3::Img(at::kRecord6.v) + offset); }

U R6Address(unsigned offset) { return ::bof3::Img(Effect_Objects_addr) + 0x300 + offset; }

// jmp [table + 4 * index], unchecked in the original: the table's words read in
// place (the fuzz swaps them for recorders), a Fatal past its entries, where
// the original jumps through the next table's words.
void Dispatch(const char* who, const unsigned long* table, unsigned entries, unsigned index, const char* by) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_1f.md section 6)",
                    who, by, index, entries, Key(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

void HandEffect1f(int x, int y) { SH_CALL(Menu_DrawHand)(x, y, 0); }

// Text_DrawAt(0x1D, 0x14, 0, 0xFF, MessagePools + the pool's word at `cell`):
// the original adds the word, zero-extended (movzx or `and 0xFFFF`), to 0x803580.
void Label(U cell) {
    SH_CALL(Text_DrawAt)(0x1D, 0x14, 0, 0xFF, AtEffect1f(::bof3::Img(bof3::addr::MessagePools) + WordEffect1f(cell)));
}

// The id at the s16 index of the category-3 list, and its record's kind byte
// (24-byte NameTable_Accessories records, +0x12): both read in place, unchecked
// as in the original.
unsigned char IdAt(U index_cell) {
    const SEffect1f index = static_cast<short>(WordEffect1f(index_cell));
    return BEffect1f(::bof3::Img(at::kAccessoryIds.v) + static_cast<U>(index));
}

unsigned char KindOf(unsigned char id) { return BEffect1f(::bof3::Img(at::kAccessoryKind.v) + 24u * id); }

// The shared exit of ExtraSlots_PickItem's cancel and confirm: record 6 +7 bit
// 1 cleared, Sprite_Current +4 down (the original's 0x52A922, after the sound).
void PickItemBack() {
    const unsigned char b = static_cast<unsigned char>(R6(7) & 0xFD);
    unsigned char* const s = Sprite_Current;
    R6(7) = b;
    s[4] = static_cast<unsigned char>(s[4] - 1);
}

unsigned char& BFieldE1(std::uint32_t a) { return *move_script::At(a); }

std::uint16_t W(std::uint32_t a) { return move_script::Word(move_script::At(a)); }

std::uint32_t KeyFieldE1(const void* p) { return Low32(p); }

void DrawMode(unsigned index, unsigned slot) { SH_AT(void (__cdecl*)(unsigned, unsigned), at::kDrawMode.v)(index, slot); }

unsigned char* DrawSprite(unsigned sprite, unsigned slot, int x, int y) {
    return SH_AT(unsigned char* (__cdecl*)(unsigned, unsigned, int, int), at::kDrawSprite.v)(sprite, slot, x, y);
}

unsigned KindPoints(unsigned kind, unsigned count) { return SH_AT(unsigned (__cdecl*)(unsigned, unsigned), at::kKindPoints.v)(kind, count); }

unsigned KindTotal() { return SH_AT(unsigned (__cdecl*)(), at::kKindTotal.v)(); }

void DrawQuad(int x, int y, unsigned height, unsigned which) {
    SH_AT(void (__cdecl*)(int, int, unsigned, unsigned), at::kDrawQuad.v)(x, y, height, which);
}

void TextFieldE1(int x, int y, int colour, int count, std::uint32_t text) {
    SH_CALL(Text_DrawAt)(x, y, colour, count, move_script::At(text));
}

// A script-pool message: MessagePools + its u16 offset.
std::uint32_t PoolText(std::uint32_t offset_cell) { return ::bof3::Img(field_e1::at::kPools) + W(offset_cell); }

void Frame(int x, int y, unsigned across, int top_end, int right, unsigned side, int low, unsigned bottom) {
    DrawMode(9, 1);
    DrawSprite(0x25, 1, x, y);
    for (unsigned i = 0; i < across; ++i) DrawSprite(0x26, 1, x + static_cast<int>(i) * 8 + 0x20, y);
    DrawSprite(0x27, 1, x + top_end, y);
    DrawSprite(0x28, 1, x + right, y);
    DrawQuad(x, y + 0x18, side, 0);
    DrawQuad(x + right, y + 0x18, side, 1);
    DrawSprite(0x30, 1, x, y + low);
    for (unsigned i = 0; i < bottom; ++i) DrawSprite(0x31, 1, x + static_cast<int>(i) * 8 + 8, y + low);
    DrawSprite(0x32, 1, x + right, y + low);
}

unsigned char* Sc() { return Sprite_Current; }

// A dispatcher: the table's entry by `index`, called as the original jumps
// to it - a Fatal past the table's own entries, where the original jumps
// through the dword after (docs/rest_1f.md section 7).
void DispatchRest1f(const char* who, const char* by, const unsigned long* table, unsigned count, unsigned index) {
    if (index >= count)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_1f.md section 7)",
                    who, by, index, count, static_cast<unsigned>(::bof3::VaOf(Low32(table))));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

void ByStateRest1f(const char* who, const unsigned long* table, unsigned count) {
    DispatchRest1f(who, "Sprite_Current +2", table, count, Sc()[2]);
}

void ByStepRest1f(const char* who, const unsigned long* table, unsigned count) {
    DispatchRest1f(who, "Sprite_Current +3", table, count, Sc()[3]);
}

}  // namespace

using bof3x::input::sigma::SigmaInverse;

namespace {

constexpr auto& SetWordRest1g = move_script::SetWord;

// Effect_Objects records 0..6 (0x7E11E0 + 0x80 n), the bytes and words the
// stages and the fish read and write.
// kEff0Flags: now Effect_Objects (symbols.toml). record 0 +0: bit 6 set while a fish pulls
// kEff0State: now Effect_Objects[0].state (symbols.toml). record 0 +1
// kEff0Depth: now Effect_Objects_addr + 0x7 (symbols.toml). record 0 +7 (signed)
// kEff0Byte0A: now Effect_Objects_addr + 0xa (symbols.toml). record 0 +0xA
constexpr ::bof3::image::Va kEff0StepX{0x7E11EC};     // record 0 +0xC

constexpr ::bof3::image::Va kEff0StepY{0x7E11F0};     // record 0 +0x10

// kEff1State: now Effect_Objects[1].state (symbols.toml). record 1 +1
// kEff3State: now Effect_Objects[3].state (symbols.toml). record 3 +1
// kEff3Mode: now Effect_Objects[3].param (symbols.toml). record 3 +6
// kEff3Started: now Effect_Objects_addr + 0x187 (symbols.toml). record 3 +7
// kEff4State: now Effect_Objects[4].state (symbols.toml). record 4 +1
// kEff4Mode: now Effect_Objects[4].param (symbols.toml). record 4 +6
constexpr ::bof3::image::Va kEff5Frame{0x7E1470};     // record 5 +0x10 (s32)

constexpr ::bof3::image::Va kEff5Level{0x7E1490};     // record 5 +0x30 (a byte, and a s16)

// kEff6State: now Effect_Objects[6].state (symbols.toml). record 6 +1
// kEff6Dir: now Effect_Objects_addr + 0x308 (symbols.toml). record 6 +8
// The choice menu's page words, its own (EffectWork), not the head's height.
inline auto& kEff6Page = rv::As<records::EffectWork>(rv::Bytes(&Effect_Objects[6]))->work_3c;   // record 6 +0x3C (u16)

inline auto& kEff6Shown = rv::As<records::EffectWork>(rv::Bytes(&Effect_Objects[6]))->work_3e;  // record 6 +0x3E (u16)

// kLeaderStage: now ObjTrio[0].step (symbols.toml). ObjTrio record 0 +2 (the leader's stage)
// kLeaderStep: now ObjTrio[0].state3 (symbols.toml). ObjTrio record 0 +3
// kSpriteIndex: now Effect1a_PanelObject (symbols.toml). u8: 0xFF none (E1E's kSpriteIndex)
// kRecordA: now Effect1a_PanelScript (symbols.toml). pointer: a 10-byte record (the rod's: 0x66A4E8 + 10 * (0x904130 - 0x2E))
// kRecordB: now Effect1a_PanelSet (symbols.toml). pointer: a 20-byte record (the lure's: 0x66A528 + 20 * (0x90412E - 0x1C))
// kItemB: now CharStats_ExtraItem (symbols.toml). u8 (save data)
constexpr ::bof3::image::Va kItemA{0x904130};         // u8 (save data)

// kWarned: now Effect1e_Choice (symbols.toml). u8: set once by LeaderPanel_S1Buttons
// kPressNow: now Effect1e_PressNow (symbols.toml). u8: Input_Pressed & 0xE020 non-zero (LeaderPanel_PressLatch)
// kPressWas: now Effect1e_PressWas (symbols.toml). u8: Effect1e_PressNow's previous value
constexpr ::bof3::image::Va kComboStep{0x6BC70C};     // u8 x 4: each sequence's position

constexpr ::bof3::image::Va kComboTimer{0x6BC710};    // u8 x 4: each sequence's frames left

// kPoseWas: now Effect1e_PoseWas (symbols.toml). u8 (E1E's)
// kPose: now Effect1e_Pose (symbols.toml). u8 (E1E's)
// kBiteHeld: now Fish_Nibbling (symbols.toml). u8: a fish is nibbling (Fish_Swim sets, Fish_Settle clears)
inline auto& kHoldFlag = SharedScratch.b_00;  // u8 (E1E reads it after LeaderPanel_HoldTest)

constexpr ::bof3::image::Va kPoolWordData{0x803642};  // u16: MessagePools offset of the data page's title

constexpr ::bof3::image::Va kPoolWordRule{0x803660};  // u16: the rule page's (read as a dword & 0xFFFF)

constexpr ::bof3::image::Va kPoolWordStage1{0x803600};  // u16 (read as a dword & 0xFFFF)

constexpr ::bof3::image::Va kPools{0x803580};         // MessagePools

// kSpot: now AreaEntry_PendingFlags (symbols.toml). u8: the fishing spot (Field_StepCount +8)
constexpr ::bof3::image::Va kDrawMode{0x93985C};      // u8 x 4, cleared by LeaderPanel_S11Switch

// Image tables (.data, read in place).
// kFishKinds: now Fish_Kinds (symbols.toml). 36 bytes a kind: chances, banks, levels, sizes
constexpr ::bof3::image::Va kSpotCounts{0x66A9CC};    // 23 bytes a spot: how many of each kind

constexpr ::bof3::image::Va kComboTable{0x66A4C8};    // 4 rows of 8: a sequence pointer, a depth byte

constexpr ::bof3::image::Va kRecordsA{0x66A4E8};      // the 10-byte records

constexpr ::bof3::image::Va kRecordsB{0x66A528};      // the 20-byte records

constexpr ::bof3::image::Va kDirAnims{0x660340};      // 2 bytes a direction: an animation, +0x2A's byte

constexpr ::bof3::image::Va kS6Frames{0x660364};      // u16 by kind: Fish_S6Wait's frame

// kF9Steps: now MoveCmd_F9Steps (symbols.toml). MoveCmd_F9Steps: 8 rows of two longs
constexpr U kF9StepsEnd = 0x669720;

constexpr unsigned kKindStride = 36;

constexpr unsigned kKinds = 23;

constexpr unsigned kSpriteStride = 0xA4;

constexpr unsigned kSpriteCount = 30;

unsigned char* AM() { return Field_ActiveMember; }

std::int32_t LRest1g(U address) { return Long(At(address)); }

void SetLRest1g(U address, U v) { SetLong(At(address), static_cast<std::int32_t>(v)); }

U WRest1g(U address) { return Word(At(address)); }

void SetW(U address, U v) { SetWordRest1g(At(address), v); }

// The address of a typed alias, for the accessors above.
U AddrOf(const void* p) { return Low32(p); }

std::int32_t LRest1g(const unsigned char* p, unsigned at) { return Long(p + at); }

void SetLRest1g(unsigned char* p, unsigned at, U v) { SetLong(p + at, static_cast<std::int32_t>(v)); }

short Sh(const unsigned char* p, unsigned at) { return static_cast<short>(Word(p + at)); }

unsigned char* RecordARest1g() { return At(Effect1a_PanelScript); }

unsigned char* RecordBRest1g() { return At(Effect1a_PanelSet); }

// A byte of the kind table: the kind's 36-byte row at `at`, read in place with
// the kind unbounded (the originals' `lea edx, [eax + eax*8]` on the byte).
unsigned char Kind(unsigned kind, unsigned at) { return At(::bof3::Img(Fish_Kinds_addr) + kKindStride * kind + at)[0]; }

U KindWord(unsigned kind, unsigned at) { return Word(At(::bof3::Img(Fish_Kinds_addr) + kKindStride * kind + at)); }

unsigned char DirAnim(unsigned d) { return At(::bof3::Img(kDirAnims.v) + 2 * d)[0]; }

unsigned char DirByte(unsigned d) { return At(::bof3::Img(kDirAnims.v) + 2 * d + 1)[0]; }

// jmp / call [table + 4 * byte]: the table's `entries` handlers, read in place
// (the fuzz swaps the cells for recorders); a Fatal past them, where the
// original jumps through the dword after.
void RunRest1g(const char* who, const unsigned long* table, unsigned entries, unsigned index, const char* byte) {
    if (index >= entries)
        bof3::Fatal("%s: %s is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/rest_1g.md section 7)",
                    who, byte, index, entries, (unsigned)Key(table));
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(table[index]))();
}

unsigned char TickRest1g() { return SH_CALL(Sprite_ScriptTick)(); }

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

// original 0x528880 (0x12 bytes): Field_LeaderStates[9] - LeaderPanel_Stages by +2.
extern "C" void __cdecl LeaderPanel_Run(void) {
    ByStateRest1f("LeaderPanel_Run (0x528880)", LeaderPanel_Stages, LeaderPanel_Stages_count);
}

// original 0x5288A0 (0x12 bytes): LeaderPanel_Stages[0] - LeaderPanel_Stage0Steps by +3.
extern "C" void __cdecl LeaderPanel_S0(void) {
    ByStepRest1f("LeaderPanel_S0 (0x5288A0)", LeaderPanel_Stage0Steps, LeaderPanel_Stage0Steps_count);
}

// original 0x5288C0 (0x75 bytes), LeaderPanel_Stage0Steps[0]: the panel's
// Fish_Nibbling 0x6BC716 (once called its pose byte), its counter 0x6BC709 and the blink switch 0x939A28
// cleared; +0x29 = 5; the height word +0x3E = AreaMap_Elevation(+0x34, +0x38);
// Field_State +0x89 0: Sprite_SetAnimationAt(0xA, 2), else
// Sprite_SetAnimation(0xB); +3 one on.
extern "C" void __cdecl LeaderPanel_S0Begin(void) {
    Fish_Nibbling = 0;
    Effect1e_Choice = 0;
    Effect1e_Blink = 0;
    Sc()[0x29] = 5;
    const unsigned char* const s = Sc();
    const long height = SH_CALL(AreaMap_Elevation)(rv::As<records::PartyMember>(s)->x, rv::As<records::PartyMember>(s)->z);
    rv::Store<std::uint32_t>(rv::As<records::PartyMember>(Sc())->height_whole, static_cast<unsigned>(height));
    if (Field_State[0x89] == 0) SH_CALL(Sprite_SetAnimationAt)(0xA, 2);
    else SH_CALL(Sprite_SetAnimation)(0xB);
    ++Sc()[3];
}

// original 0x528940 (0x26 bytes), LeaderPanel_Stage0Steps[1]: once the wait
// word Transition_Busy is 0, effect record 4's +6 = 0 and its state +1
// one on, and +3 one on.
extern "C" void __cdecl LeaderPanel_S0Wait(void) {
    if (Transition_Busy != 0) return;
    unsigned char* const record4 = rv::Bytes(&Effect_Objects[4]);
    const unsigned char state = record4[1];
    record4[6] = 0;
    record4[1] = static_cast<unsigned char>(state + 1);
    ++Sc()[3];
}

// original 0x528970 (0x2C bytes), LeaderPanel_Stage0Steps[2]: once effect
// record 4's state +1 is 4, record 1's state +1 one on, +2 one on (stage 1)
// and +3 = 0.
extern "C" void __cdecl LeaderPanel_S0End(void) {
    if (Effect_Objects[4].state != 4) return;
    ++Effect_Objects[1].state;
    ++Sc()[2];
    Sc()[3] = 0;
}

// original 0x5289A0: LeaderPanel_Stages[1] - jmp through LeaderPanel_Stage1Steps
// by Sprite_Current +3, unchecked (ours aborts past its 11).
extern "C" void __cdecl LeaderPanel_S1(void) {
    RunRest1g("LeaderPanel_S1 (0x5289A0)", LeaderPanel_Stage1Steps, LeaderPanel_Stage1Steps_count, Sprite_Current[3],
        "Sprite_Current +3");
}

// original 0x5289C0: LeaderPanel_Stage1Steps[0] - LeaderPanel_SetRecords; the
// sprite index 0xFF; +0xC..+0x20 (six dwords) and +9 cleared; record 3's mode 0
// when both item bytes are set, else 6; the animation by Field_State +0x89 (0:
// 0xA from frame 2, else 0xB); +3 up.
extern "C" void __cdecl LeaderPanel_S1Begin(void) {
    SH_CALL(LeaderPanel_SetRecords)();
    Effect1a_PanelObject = 0xFF;
    for (unsigned at = 0xC; at <= 0x20; at += 4) SetLRest1g(S(), at, 0);
    S()[9] = 0;
    const unsigned mode = CharStats_ExtraItem != 0 && B(::bof3::Img(kItemA.v)) != 0 ? 0u : 6u;
    SH_CALL(LeaderPanel_Effect3Mode)(mode);
    if (Field_State[0x89] == 0) SH_CALL(Sprite_SetAnimationAt)(0xA, 2);
    else Animation(0xB);
    unsigned char* const s = S();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x528A70: LeaderPanel_Stage1Steps[1] - record 3 at 6: +3 up.
extern "C" void __cdecl LeaderPanel_S1Wait(void) {
    if (Effect_Objects[3].state != 6) return;
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
    switch (SigmaInverse(static_cast<unsigned char>(Input_Pressed) & Sigma(0x74))) {
    case 0x04: {
        unsigned char* const s = S();
        Effect_Objects[3].state = 7;
        Effect_Objects[1].state = 4;
        s[2] = 0xA;
        S()[3] = 0;
        return;
    }
    case 0x10: {
        unsigned char* const s = S();
        Effect_Objects[3].state = 7;
        Effect_Objects[1].state = 4;
        s[2] = 9;
        S()[3] = 0;
        return;
    }
    case 0x20: {
        const unsigned char item = CharStats_ExtraItem;
        Effect_Objects[3].state = 7;
        if (item == 0 || B(::bof3::Img(kItemA.v)) == 0) {
            unsigned char* const s = S();
            Effect_Objects[1].state = 4;
            s[2] = 9;
            S()[3] = 0;
            return;
        }
        const unsigned char holds = SH_CALL(Inventory_Holds38To4DAt99)();
        if (holds == 0) {
            Effect1e_Choice = 0;
        } else if (Effect1e_Choice == 0) {
            unsigned char* const s = S();
            Effect1e_Choice = 1;
            Effect_Objects[1].state = 4;
            s[3] = 7;
            return;
        }
        S()[2] = 2;
        S()[3] = 0;
        return;
    }
    case 0x40: {
        unsigned char* const s = S();
        Effect_Objects[3].state = 7;
        Effect_Objects[1].state = 4;
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
    if (Effect_Objects[3].state == 0 && Effect_Objects[1].state == 1) {
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
    const U pool = static_cast<U>(LRest1g(::bof3::Img(kPoolWordStage1.v))) & 0xFFFF;
    Text(0x1D, static_cast<int>(0x15u - (static_cast<U>(S()[9]) << 3)), At(::bof3::Img(kPools.v) + pool));
}

// original 0x528CD0 (PSX 0x801DA1E8): LeaderPanel_Stage1Steps[5] - box 3
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
    if (pressed & Sigma(0x60)) {
        if (pressed & Sigma(0x40)) {
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
    Text(0x1D, 0x15, Pool(::bof3::Img(at::kPoolWord00.v)));
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
            Effect_Objects[1].state = static_cast<unsigned char>(Effect_Objects[1].state + 1);
            s[3] = 0;
        }
    }
    Shade();
    const unsigned count = S()[9];
    if (count == 0) return;
    BoxPrims(0x12u - (count << 3));
    Text(0x1D, static_cast<int>(0x15u - (static_cast<U>(S()[9]) << 3)), Pool(::bof3::Img(at::kPoolWord00.v)));
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

// original 0x528F50: LeaderPanel_Stages[2] - jmp through LeaderPanel_Stage2Steps by +3.
extern "C" void __cdecl LeaderPanel_S2(void) {
    Run("LeaderPanel_S2", LeaderPanel_Stage2Steps, LeaderPanel_Stage2Steps_count);
}

// original 0x528F70: LeaderPanel_Stage2Steps[0] - effect record 2's +1 up,
// animation 1, +3 = 1.
extern "C" void __cdecl LeaderPanel_S2Begin(void) {
    Effect_Objects[2].state = static_cast<unsigned char>(Effect_Objects[2].state + 1);
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
        rv::Store<std::uint32_t>(Effect_Objects[2].work_18, 0);
        Sound(0x200);
        Effect1e_PoseWas = 0xFF;
        Effect1e_Pose = 0xFF;
        Animate(2);
        rv::Current<records::PartyMember>()->z = 0x410000;
        S()[9] = 0;
        S()[3] = 2;
    } else if ((Field_CancelButtons & pressed) && Effect_Objects[3].state == 0) {
        unsigned char* const s = S();
        Effect_Objects[2].state = 0;
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
    if (rv::As<records::PartyMember>(s)->script_pos != 6) return;
    Effect_Objects[0].state = static_cast<unsigned char>(Effect_Objects[0].state + 1);
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
    if (Effect_Objects[0].state == 3) {
        S()[2] = 3;
        S()[3] = 0;
        Tick();
        return;
    }
    if (Effect_Objects[0].step_y == 0 && (Input_Pressed & Sigma(0x20))) {
        const std::int32_t y = L(::bof3::Img(at::kEff0StepY.v));
        if (y > 0x110000) SetL(::bof3::Img(at::kEff0StepY.v), static_cast<U>(y) - 0x4000u);
    }
    unsigned char* const s = S();
    if (s[0xB] == 0) {
        if (s[7] == 0) {
            const unsigned held = Input_Held;
            if (held & 0xA000) {
                SetL(Key(s + 0xC), held & 0xA000u);
                unsigned char* const t = S();
                SetL(::bof3::Img(at::kEff0StepX.v), static_cast<U>(L(Key(t + 0xC))) != 0x8000u ? 0x800u : 0xFFFFF800u);
                t[7] = 1;
            }
        } else {
            const std::int32_t x = L(::bof3::Img(at::kEff0X.v));
            if ((Input_Held & static_cast<U>(L(Key(s + 0xC)))) == 0 || x <= 0x60000 || x >= 0x150000) {
                SetL(::bof3::Img(at::kEff0StepX.v), 0);
                s[0xB] = 1;
            }
        }
    }
    Tick();
}

// original 0x529150: LeaderPanel_Stages[3] - jmp through LeaderPanel_Stage3Steps by +3.
extern "C" void __cdecl LeaderPanel_S3(void) {
    Run("LeaderPanel_S3", LeaderPanel_Stage3Steps, LeaderPanel_Stage3Steps_count);
}

// original 0x529170: LeaderPanel_Stage3Steps[0] - effect record 1's +2 and
// record 4's +1 up, record 4's +6 = 1, +6 = 1, 0x6BC70C = 0, +7 = +0xB = 0,
// animation 3, +3 up.
extern "C" void __cdecl LeaderPanel_S3Begin(void) {
    const auto sub = static_cast<unsigned char>(Effect_Objects[1].step + 1);
    Effect_Objects[4].state = static_cast<unsigned char>(Effect_Objects[4].state + 1);
    unsigned char* const s = S();
    Effect_Objects[1].step = sub;
    Effect_Objects[4].param = 1;
    s[6] = 1;
    unsigned char* const t = S();
    SetL(::bof3::Img(at::kCounter.v), 0);
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
    if (s[6] != 0 && Effect_Objects[4].state == 3) {
        s[6] = 0;
        Effect3Mode(1);
    }
    if ((Input_Pressed & Sigma(0x40)) && Effect_Objects[4].state == 0) {
        Sound(0x203);
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
        Effect_Objects[2].state = 0;
        Effect_Objects[0].state = 0;
        Effect_Objects[3].state = 7;
        return;
    }
    const std::int32_t z = Effect_Objects[0].z;
    if (z >= 0x3E8000) {
        Sound(0x203);
        unsigned char* const t = S();
        Effect_Objects[2].state = 0;
        Effect_Objects[0].state = 0;
        t[2] = 1;
        S()[3] = 0;
        return;
    }
    bool pose = true;
    const auto ground = static_cast<short>(SH_CALL(AreaMap_Elevation)(Effect_Objects[0].x, z));
    if (static_cast<std::int16_t>(Effect_Objects[0].height_whole) <= ground) {
        const unsigned r = static_cast<unsigned>(SH_CALL(Rand)()) & 0xF;
        if (static_cast<signed char>(RecordB()[0x11]) > static_cast<signed char>(r) && (Frame_Counter & 0xF) == 0) {
            const std::int32_t z2 = Effect_Objects[0].z;
            const std::int32_t camera = Field_Kind2Z;
            SetL(::bof3::Img(at::kEff0StepX.v), 0);
            SetL(::bof3::Img(at::kEff0StepY.v), 0);
            rv::Store<std::uint32_t>(Effect_Objects[0].step_y, 0);
            if (camera < z2) Field_Kind2Z = z2;
            unsigned char* const t = S();
            MoveScript_F3Divisor = 0x10;
            t[2] = 5;
            S()[3] = 0;
            return;
        }
        const long h = SH_CALL(AreaMap_Elevation)(Effect_Objects[0].x, Effect_Objects[0].z);
        Effect_Objects[0].height_whole = static_cast<unsigned>(h);
    }
    SH_AT(void (__cdecl*)(), at::kHoldTest.v)();
    SH_AT(void (__cdecl*)(), at::kEffectsStep.v)();
    Effect1e_PoseWas = Effect1e_Pose;
    if (rv::Bytes(Effect_Objects)[0xb] != 0) pose = false;
    if (pose) {
        const unsigned held = Input_Held;
        unsigned animation;
        if (held & 0x8000) {
            unsigned char* const r = RecordB();
            Effect1e_Pose = 1;
            SetL(::bof3::Img(at::kEff0StepX.v), 0xFFFFF000u);
            SetL(::bof3::Img(at::kEff0StepY.v), 0x1000);
            rv::Store<std::uint32_t>(Effect_Objects[0].step_y, static_cast<U>(Long(r + 8)));
            animation = 6;
        } else if (held & 0x2000) {
            Effect1e_Pose = 1;
            SetL(::bof3::Img(at::kEff0StepX.v), 0x1000);
            SetL(::bof3::Img(at::kEff0StepY.v), 0x1000);
            rv::Store<std::uint32_t>(Effect_Objects[0].step_y, static_cast<U>(Long(RecordB() + 8)));
            animation = 5;
        } else if (held & Sigma(0x20)) {
            const unsigned char sloped = B(::bof3::Img(0x903850));
            SetL(::bof3::Img(at::kEff0StepX.v), 0);
            if (sloped != 0) {
                unsigned char* const r = RecordB();
                Effect1e_Pose = 2;
                SetL(::bof3::Img(at::kEff0StepY.v), 0x2000);
                rv::Store<std::uint32_t>(Effect_Objects[0].step_y, static_cast<U>(Long(r + 8)));
                animation = 4;
            } else {
                unsigned char* const r = RecordB();
                Effect1e_Pose = 1;
                SetL(::bof3::Img(at::kEff0StepY.v), 0x1000);
                rv::Store<std::uint32_t>(Effect_Objects[0].step_y, static_cast<U>(Long(r + 4)));
                animation = 3;
            }
        } else if (B(::bof3::Img(0x903850)) != 0) {
            unsigned char* const r = RecordB();
            Effect1e_Pose = 2;
            SetL(::bof3::Img(at::kEff0StepX.v), 0);
            SetL(::bof3::Img(at::kEff0StepY.v), 0x1000);
            rv::Store<std::uint32_t>(Effect_Objects[0].step_y, static_cast<U>(Long(r + 8)));
            animation = 4;
        } else {
            unsigned char* const r = RecordB();
            Effect1e_Pose = 0;
            SetL(::bof3::Img(at::kEff0StepX.v), 0);
            SetL(::bof3::Img(at::kEff0StepY.v), 0);
            rv::Store<std::uint32_t>(Effect_Objects[0].step_y, static_cast<U>(Long(r)));
            animation = 3;
        }
        Animate(animation);
    }
    PoseSound();
    const std::int32_t step = L(::bof3::Img(at::kEff0StepY.v));
    const std::int32_t z3 = Effect_Objects[0].z;
    Field_Kind2Z = z3;
    MoveScript_F3Divisor = static_cast<short>(step != 0 ? step >> 8 : 0x10);
    if (z3 < 0x150000)
        Field_Kind2Z = 0x150000;
    else if (z3 > 0x3C0000)
        Field_Kind2Z = 0x3C0000;
    const std::int32_t x = Effect_Objects[0].x;
    const std::int32_t dx = L(::bof3::Img(at::kEff0StepX.v));
    if ((x <= 0x80000 && dx < 0) || (x >= 0x120000 && dx > 0)) SetL(::bof3::Img(at::kEff0StepX.v), 0);
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
    if (Field_Kind2Hold != 0 || Effect_Objects[3].state != 0) return;
    S()[2] = 1;
    S()[3] = 0;
}

// original 0x529530: LeaderPanel_Stages[4] - jmp through LeaderPanel_Stage4Steps by +3.
extern "C" void __cdecl LeaderPanel_S4(void) {
    Run("LeaderPanel_S4", LeaderPanel_Stage4Steps, LeaderPanel_Stage4Steps_count);
}

// original 0x529550: LeaderPanel_Stage4Steps[0] - effect record 4's +1 up,
// record 2's 0, record 4's +6 = 2, the camera's z the picked record's +0x38,
// Music_FadeOut(8), record 3's +1 = 7, +3 up.
extern "C" void __cdecl LeaderPanel_S4Begin(void) {
    const auto state = static_cast<unsigned char>(Effect_Objects[4].state + 1);
    unsigned char* const p = Picked("LeaderPanel_S4Begin");
    auto* const po = rv::As<records::SpriteObject>(p);
    Effect_Objects[4].state = state;
    Effect_Objects[2].state = 0;
    Effect_Objects[4].param = 2;
    Field_Kind2Z = po->z;
    SH_CALL(Music_FadeOut)(8);
    unsigned char* const s = S();
    Effect_Objects[3].state = 7;
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x5295A0: LeaderPanel_Stage4Steps[1] - effect record 4 at 3: track
// 0x29, record 3 mode 2, record 5's +1 up, +9 = 0, +3 up; Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S4Music(void) {
    if (Effect_Objects[4].state == 3) {
        MusicTo(0x29);
        Effect3Mode(2);
        const auto state = static_cast<unsigned char>(Effect_Objects[5].state + 1);
        unsigned char* const s = S();
        Effect_Objects[5].state = state;
        s[9] = 0;
        unsigned char* const t = S();
        t[3] = static_cast<unsigned char>(t[3] + 1);
    }
    Tick();
}

// original 0x5295F0 (PSX 0x801DB1C0): LeaderPanel_Stage4Steps[2] - the
// pull: record 0's z at 0x3E8000 or more lands it (records 4, 1 at 4, the
// picked record's +1 = 6, sound 0x203, track 0x2A, animation 0xA, +3 = 3);
// else by the pad - 0xC000 held (pressed: pose 2, +9 = 8, the frame from the
// A record's +5, animation 8, record 5's count up; held: +9 down with a tick,
// or at 0 the rest pose) then 0x20 pressed pulls, 0x20 held blinks; 0x3000
// held lets go (the A record's +6, count down); 0x20 alone pulls or blinks,
// pose 1 by its +4; nothing: the rest pose. Each way ends in 0x52B1B0.
extern "C" void __cdecl LeaderPanel_S4Run(void) {
    Effect1e_PoseWas = Effect1e_Pose;
    if (Effect_Objects[0].z >= 0x3E8000) {
        Effect_Objects[4].param = 4;
        Effect_Objects[1].state = 4;
        Effect_Objects[0].state = 0;
        unsigned char* const p = Picked("LeaderPanel_S4Run");
        Effect_Objects[5].state = 0;
        Effect_Objects[4].state = 1;
        Effect_Objects[3].state = 7;
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
            Effect1e_Pose = 2;
            s[9] = 8;
            SetL(::bof3::Img(at::kEff5Frame.v), static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(RecordA()[5]))));
            Animate(8);
            rv::Bytes(&Effect_Objects[5])[0xa] = static_cast<unsigned char>(rv::Bytes(&Effect_Objects[5])[0xa] + 1);
        } else {
            unsigned char* const s = S();
            if (s[9] != 0) {
                s[9] = static_cast<unsigned char>(s[9] - 1);
                Tick();
                rv::Bytes(&Effect_Objects[5])[0xa] = static_cast<unsigned char>(rv::Bytes(&Effect_Objects[5])[0xa] + 1);
            } else {
                SetL(::bof3::Img(at::kEff5Frame.v), 0);
                Effect1e_Pose = 0;
                Animate(7);
            }
        }
        if (Input_Pressed & Sigma(0x20)) {
            Pull("LeaderPanel_S4Run");
            Animate(7);
            SH_CALL(LeaderPanel_S4Pose)();
            rv::Bytes(&Effect_Objects[5])[0xa] = static_cast<unsigned char>(rv::Bytes(&Effect_Objects[5])[0xa] + 1);
            PoseSound();
            return;
        }
        if (Input_Held & Sigma(0x20)) {
            SH_CALL(LeaderPanel_S4Blink)();
            SH_CALL(LeaderPanel_S4Pose)();
            Tick();
            rv::Bytes(&Effect_Objects[5])[0xa] = static_cast<unsigned char>(rv::Bytes(&Effect_Objects[5])[0xa] + 1);
        }
        PoseSound();
        return;
    }
    if (held & 0x3000) {
        unsigned char* const a = RecordA();
        Effect1e_Pose = 0;
        SetL(::bof3::Img(at::kEff5Frame.v), static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(a[6]))));
        Picked("LeaderPanel_S4Run")[0x1C] = 0;
        Animate(0);
        rv::Bytes(&Effect_Objects[5])[0xa] = static_cast<unsigned char>(rv::Bytes(&Effect_Objects[5])[0xa] - 1);
        PoseSound();
        return;
    }
    if (held & Sigma(0x20)) {
        if (Input_Pressed & Sigma(0x20))
            Pull("LeaderPanel_S4Run");
        else
            SH_CALL(LeaderPanel_S4Blink)();
        Effect1e_Pose = 1;
        SetL(::bof3::Img(at::kEff5Frame.v), static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(RecordA()[4]))));
        Animate(7);
        Tick();
        rv::Bytes(&Effect_Objects[5])[0xa] = static_cast<unsigned char>(rv::Bytes(&Effect_Objects[5])[0xa] + 1);
        PoseSound();
        return;
    }
    unsigned char* const p = Picked("LeaderPanel_S4Run");
    Effect1e_Pose = 0;
    SetL(::bof3::Img(at::kEff5Frame.v), 0);
    p[0x1C] = 0;
    Animate(7);
    PoseSound();
}

// original 0x529860: 0x5295F0's pose - by +9, the pose 2 and record 5's frame
// the A record's +7, else pose 1 and its +4 (signed bytes).
extern "C" void __cdecl LeaderPanel_S4Pose(void) {
    if (S()[9] != 0) {
        unsigned char* const a = RecordA();
        Effect1e_Pose = 2;
        SetL(::bof3::Img(at::kEff5Frame.v), static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(a[7]))));
    } else {
        unsigned char* const a = RecordA();
        Effect1e_Pose = 1;
        SetL(::bof3::Img(at::kEff5Frame.v), static_cast<U>(static_cast<std::int32_t>(static_cast<signed char>(a[4]))));
    }
}

// original 0x5298A0: 0x5295F0's blink - the picked record's +0x1C 0 when
// Frame_Counter has a bit of 3 >> the A record's +3, else 0xFF.
extern "C" void __cdecl LeaderPanel_S4Blink(void) {
    const U bits = 3u >> (RecordA()[3] & 31u);
    unsigned char* const p = Picked("LeaderPanel_S4Blink");
    p[0x1C] = static_cast<unsigned char>((Frame_Counter & bits) != 0 ? 0 : 0xFF);
}

// original 0x5298F0: LeaderPanel_Stage4Steps[3] - effect record 4 at 4: the
// picked record's count +0x9C over the best of its kind (0x9040EC + kind)
// replaces it and sets the blink; sound 0x102, +9 = 8, +3 up;
// Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S4Best(void) {
    if (Effect_Objects[4].state == 4) {
        unsigned char* const p = Picked("LeaderPanel_S4Best");
        auto* const po = rv::As<records::SpriteObject>(p);
        unsigned char& best = B(::bof3::Img(at::kBestCounts.v) + p[6]);
        if (po->counter > best) {
            Effect1e_Blink = 1;
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
        Sound(B(::bof3::Img(at::kKindSounds.v) + 36u * kind) >= 2 ? 0x20C : 0x20B);
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
    if (Input_Pressed & Sigma(0x20)) {
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
        if (Effect1e_Blink != 0) Sound(0x20D);
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
    if (Input_Pressed & Sigma(0x60)) {
        unsigned char* const s = S();
        Effect1e_Blink = 0;
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
    if (Input_Pressed & Sigma(0x60)) S()[3] = 0xE;
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

extern "C" void __cdecl LeaderPanel_S4Again(void) {
    PanelHead("LeaderPanel_S4Again");
    Total(0x9C);
    Message(0x94, 0x44);
    const unsigned pressed = Input_Pressed;
    if (pressed & 0x5000) {
        Sound(0x100);
        unsigned char* const s = S();
        s[6] = static_cast<unsigned char>(s[6] ^ 1);
    } else if (pressed & Sigma(0x60)) {
        if (pressed & Sigma(0x20)) {
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
        const U row = FirstAnswerRow(At(::bof3::Img(effect_1e::at::kPools.v) + Word(At(::bof3::Img(effect_1e::at::kPools.v) + 0x44u * 2u))), 0x44);
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
        Effect_Objects[1].state = static_cast<unsigned char>(Effect_Objects[1].state + 1);
    }
    MusicTo(0x28);
    Sound(0x203);
    Tick();
}

// original 0x52A210 (PSX twin 0x801DC4F4): LeaderPanel_Stages[5] - jmp through
// LeaderPanel_Stage5Steps by +3.
extern "C" void __cdecl LeaderPanel_S5(void) {
    Run("LeaderPanel_S5", LeaderPanel_Stage5Steps, LeaderPanel_Stage5Steps_count);
}

// original 0x52A230 (PSX twin 0x801DC538): LeaderPanel_Stage5Steps[0] and
// LeaderPanel_Stage6Steps[0] - Inventory_Remove(3, 0x90412E, 1); refused: the
// two bytes 0x90412E / F cleared; then 0x52B200 (a tail jmp).
extern "C" void __cdecl LeaderPanel_UseItem(void) {
    if (SH_CALL(Inventory_Remove)(3, CharStats_ExtraItem, 1) == 0) {
        CharStats_ExtraItem = 0;
        CharStats_ExtraCount = 0;
    }
    SH_AT(void (__cdecl*)(), at::kUseItemEnd.v)();
}

// original 0x52A260 (PSX twin 0x801DC594): LeaderPanel_Stage5Steps[1] - effect
// record 4 at 3: record 3 mode 3, +3 up; Sprite_ScriptTick.
extern "C" void __cdecl LeaderPanel_S5Wait(void) {
    if (Effect_Objects[4].state == 3) {
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
    SH_AT(unsigned char (__cdecl*)(unsigned), at::kLeaveOnPress.v)(Sigma(0x60));
    Tick();
}

// original 0x52A290 (PSX twin 0x801DC610): LeaderPanel_Stages[6] - jmp through
// LeaderPanel_Stage6Steps by +3.
extern "C" void __cdecl LeaderPanel_S6(void) {
    Run("LeaderPanel_S6", LeaderPanel_Stage6Steps, LeaderPanel_Stage6Steps_count);
}

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
    if (Transition_Busy != 0) return;
    SH_CALL(Task_Sleep)(1);
    Field_Kind2X = 0xF0000;
    Field_Kind2Z = 0x3C0000;
    SH_CALL(Field_ViewReset)();
    SH_AT(void (__cdecl*)(), at::kClearEffects.v)();
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
    if (Transition_Busy != 0) return;
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

// DIVERGENCE DIV-0132: 1 (PatchBytes "FishPanelDuck", after the self-test,
// which compares Capcom's fades) - the panel ducks the music as the
// PlayStation's twins do (SsSepSetDecrescendo / SsSepSetCrescendo(0, 0, 0x14,
// 0x10)) instead of fading it to nothing and back to 127.
unsigned char g_fish_panel_duck = 0;

// original 0x52A440 (PSX twin 0x801DC908): LeaderPanel_Stage9Steps[0] - with
// effect record 3 idle: Music_FadeOut(0x10), sound 0x102, record 6's +1 up,
// +6 = 0, +3 up. DIV-0132: the duck in place of the fade out.
extern "C" void __cdecl LeaderPanel_S9Begin(void) {
    if (Effect_Objects[3].state != 0) return;
    if (g_fish_panel_duck)
        sound::DuckMusic(0x10);
    else
        SH_CALL(Music_FadeOut)(0x10);
    Sound(0x102);
    const auto state = static_cast<unsigned char>(Effect_Objects[6].state + 1);
    unsigned char* const s = S();
    Effect_Objects[6].state = state;
    s[6] = 0;
    unsigned char* const t = S();
    t[3] = static_cast<unsigned char>(t[3] + 1);
}

// original 0x52A480 (PSX twin 0x801DC984): LeaderPanel_Stage9Steps[1] - effect
// record 6 at 3: +3 up.
extern "C" void __cdecl LeaderPanel_S9Wait(void) {
    if (Effect_Objects[6].state != 3) return;
    unsigned char* const s = S();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x52A4A0 (PSX twin 0x801DC9BC): LeaderPanel_Stage9Steps[2] - the
// choice record 6 +6 (0..2) moved by the auto-repeated up / down (sound
// 0x100); cancel: Music_FadeIn(0x10), sound 0x106, record 6's +1 to 5 (from 3
// or 4), 0xB (from 8) or 0xF (from 0xE), its +0xB = 0, +3 = 4; confirm: sound
// 0x103 and by the choice record 6's +1 to 0xB (from 8), 0xF (from 0xE) or 5
// (from 3 or 4) with sound 0x102, +3 up. At +3 2, the choice's message and
// the hand. DIV-0132: cancel undoes S9Begin's duck in place of the fade in.
extern "C" void __cdecl LeaderPanel_S9Menu(void) {
    const unsigned repeat = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0xA000u);
    if (repeat & 0x8000) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(Effect_Objects[6].param - 1);
        Effect_Objects[6].param = c;
        if (c & 0x80) Effect_Objects[6].param = 2;
    } else if (repeat & 0x2000) {
        Sound(0x100);
        const auto c = static_cast<unsigned char>(Effect_Objects[6].param + 1);
        Effect_Objects[6].param = c;
        if (c > 2) Effect_Objects[6].param = 0;
    } else {
        const unsigned pressed = Input_Pressed;
        if (Field_CancelButtons & pressed) {
            if (g_fish_panel_duck)  // DIV-0132: the duck undone
                sound::UnduckMusic(0x10);
            else
                SH_CALL(Music_FadeIn)(0x10);
            Sound(0x106);
            const unsigned char state = Effect_Objects[6].state;
            if (state == 3 || state == 4)
                Effect_Objects[6].state = 5;
            else if (state == 8)
                Effect_Objects[6].state = 0xB;
            else if (state == 0xE)
                Effect_Objects[6].state = 0xF;
            unsigned char* const s = S();
            rv::Bytes(&Effect_Objects[6])[0xb] = 0;
            s[3] = 4;
        } else if (Field_ConfirmButtons & pressed) {
            Sound(0x103);
            const unsigned char choice = Effect_Objects[6].param;
            const unsigned char state = Effect_Objects[6].state;
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
                Effect_Objects[6].state = to;
            }
            unsigned char* const s = S();
            s[3] = static_cast<unsigned char>(s[3] + 1);
        }
    }
    if (S()[3] != 2) return;
    switch (Effect_Objects[6].param & 3) {
    case 0: Text(0x1D, 0x14, At(::bof3::Img(effect_1e::at::kPools.v) + Word(At(::bof3::Img(at::kPoolWord20.v))))); break;
    case 1: Text(0x1D, 0x14, At(::bof3::Img(effect_1e::at::kPools.v) + Word(At(::bof3::Img(at::kPoolWord40.v))))); break;
    case 2: Text(0x1D, 0x14, At(::bof3::Img(effect_1e::at::kPools.v) + Word(At(::bof3::Img(at::kPoolWord5E.v))))); break;
    default: break;
    }
    Hand(48u * Effect_Objects[6].param + 0x58u, 0x2E);
}

// original 0x52A6C0: jmp [ChoiceMenu_Choices + 4 * record 6 +6] - the three-way
// choice 0x52A4A0 (E1E) keeps in 0..2.
extern "C" void __cdecl ChoiceMenu_Run(void) {
    Dispatch("ChoiceMenu_Run", ChoiceMenu_Choices, ChoiceMenu_Choices_count, R6(6), "Effect_Objects record 6 +6");
}

// original 0x52A6D0: jmp [ExtraSlots_Steps + 4 * Sprite_Current +4].
extern "C" void __cdecl ExtraSlots_Step(void) {
    Dispatch("ExtraSlots_Step", ExtraSlots_Steps, ExtraSlots_Steps_count, Sprite_Current[4], "Sprite_Current +4");
}

// original 0x52A6F0: when record 6 +1 is 3 or 4, its +7 loses bit 7, the hand
// goes to (0x1C, 0x58 + 16 * (+7 & 0x7F)) - the original's y is that byte
// shifted over whatever eax held above it, and Menu_DrawHand reads the low word
// - and Sprite_Current +4 steps on; then the slot's label by +7 bit 0 (the
// word 0x803622 clear, 0x803624 set), +7 read again after the hand.
extern "C" void __cdecl ExtraSlots_Enter(void) {
    const unsigned char state = R6(1);
    if (state == 3 || state == 4) {
        const auto b = static_cast<unsigned char>(R6(7) & 0x7F);
        R6(7) = b;
        HandEffect1f(0x1C, static_cast<int>((static_cast<U>(b) << 4) + 0x58));
        unsigned char* const s = Sprite_Current;
        s[4] = static_cast<unsigned char>(s[4] + 1);
    }
    Label((R6(7) & 1) ? ::bof3::Img(at::kLabelB.v) : ::bof3::Img(at::kLabelA.v));
}

// original 0x52A780: the repeat of Input_Pressed's bits 12 and 14
// (Input_AutoRepeat); Input_Pressed read after it against the cancel buttons
// (a sound 0x106, record 6 +7 |= 0x80 and +0x2C = 0xFFFF, Sprite_Current +3
// down and +4 = 0: back out) and the confirm buttons (0x103, +7 |= 2, +4 up);
// else the repeat's bit 12 or 14 (0x100, +7 ^= 1 and record 6 +1 ^= 7 - the
// other slot, record 6 between states 3 and 4 -, +4 down). Then the label by
// +7 bit 0, and unless +7 bit 7 the slot's contents into record 6 +0x2C (the
// byte 0x904130 for bit 0 clear, 0x90412E set; + 4, or 0xFFFF when 0); the
// hand at (0x1C, 0x58 + 16 * bit 0).
extern "C" void __cdecl ExtraSlots_PickSlot(void) {
    const U repeat = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0x5000u);
    const unsigned short pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        Sound(0x106);
        const auto b = static_cast<unsigned char>(R6(7) | 0x80);
        unsigned char* const s = Sprite_Current;
        SetWord(R6Address(0x2C), 0xFFFF);
        R6(7) = b;
        s[3] = static_cast<unsigned char>(s[3] - 1);
        Sprite_Current[4] = 0;
        return;
    }
    if (Field_ConfirmButtons & pressed) {
        Sound(0x103);
        const auto b = static_cast<unsigned char>(R6(7) | 2);
        unsigned char* const s = Sprite_Current;
        R6(7) = b;
        s[4] = static_cast<unsigned char>(s[4] + 1);
    } else if (repeat & 0x5000u) {
        Sound(0x100);
        const auto b = static_cast<unsigned char>(R6(7) ^ 1);
        const auto state = static_cast<unsigned char>(R6(1) ^ 7);
        R6(7) = b;
        unsigned char* const s = Sprite_Current;
        R6(1) = state;
        s[4] = static_cast<unsigned char>(s[4] - 1);
    }
    Label((R6(7) & 1) ? ::bof3::Img(at::kLabelB.v) : ::bof3::Img(at::kLabelA.v));
    const unsigned char b = R6(7);
    if (!(b & 0x80)) {
        const unsigned char held = (b & 1) ? CharStats_ExtraItem : BEffect1f(::bof3::Img(at::kExtraAccessory.v));
        SetWord(R6Address(0x2C), held ? held + 4u : 0xFFFFu);
    }
    HandEffect1f(0x1C, static_cast<int>(((b & 1u) << 4) + 0x58));
}

// original 0x52A8F0: the list for the slot record 6 +7 bit 0 chose. The repeat
// of bits 12 and 14 first; the cancel buttons: 0x106 and back (PickItemBack).
//
// Bit 0 clear (the accessory slot 0x904130): the label 0x803626; the confirm
// buttons: the id at s16 +0x36 of the category-3 list, when its kind is 0xB,
// out of the inventory (Inventory_Remove(3, id, 1)), the slot's old id back in
// (Inventory_Add(3, old, 1), when not 0), into the slot; 0x103 and back either
// way. Else the repeat's bit 12 moves the cursor dword +0xC up (not below 0),
// bit 14 down (below +0x18 - 1). The id's kind 0xB: +0x2C = id + 4, else
// 0xFFFF; the hand at (0xA8, 13 * +0xC + 0x5A).
//
// Bit 0 set (the item slot 0x90412E): the label 0x803628; nothing moves while
// +0xA is not 0. The confirm buttons: the id at s16 +0x3A, when its kind is 0xA
// and it is not already the slot's, swapped in as above with the count
// 0x90412F = 1, and the scroll word +0x38 pulled back by one when the list's
// kind-0xA count now leaves it past count - 9 (and +0x1C is at least that
// count); 0x103 and back. Else the repeat's bit 12: the cursor dword +0x10 up,
// or at 0 the scroll up with +8 = 4, +0xA = 3; bit 14: the cursor down below
// +0x1C, or at 8 the scroll down while scroll + 8 is below +0x1C, with +8 = 0,
// +0xA = 3; Input_Pressed's bit 2: the scroll up by 9 (to 0, then the cursor
// to 0); bit 3: the scroll down by 9 while +0x1C passes 8 (held at +0x1C - 8,
// then the cursor to 8), else the cursor to +0x1C. The id at +0x3A of kind 0xA:
// +0x2C = id + 4, else 0xFFFF; the hand at (0xA8, 13 * +0x10 + 0x5A).
extern "C" void __cdecl ExtraSlots_PickItem(void) {
    const U repeat = SH_CALL(Input_AutoRepeat)(Input_Pressed & 0x5000u);
    if (Input_Pressed & Field_CancelButtons) {
        Sound(0x106);
        PickItemBack();
        return;
    }
    if (!(R6(7) & 1)) {
        Label(::bof3::Img(at::kListLabelA.v));
        if (Input_Pressed & Field_ConfirmButtons) {
            const unsigned char id = IdAt(R6Address(0x36));
            if (KindOf(id) == 0xB) {
                SH_CALL(Inventory_Remove)(3, id, 1);
                const unsigned char old = BEffect1f(::bof3::Img(at::kExtraAccessory.v));
                if (old) SH_CALL(Inventory_Add)(3, old, 1);
                BEffect1f(::bof3::Img(at::kExtraAccessory.v)) = id;
            }
            Sound(0x103);
            PickItemBack();
            return;
        }
        if (repeat & 0x1000u) {
            if (LongEffect1f(R6Address(0xC)) != 0) {
                Sound(0x100);
                SetLongEffect1f(R6Address(0xC), LongEffect1f(R6Address(0xC)) - 1);
            }
        } else if (repeat & 0x4000u) {
            const U last = LongEffect1f(R6Address(0x18)) - 1;
            if (static_cast<SEffect1f>(LongEffect1f(R6Address(0xC))) < static_cast<SEffect1f>(last)) {
                Sound(0x100);
                SetLongEffect1f(R6Address(0xC), LongEffect1f(R6Address(0xC)) + 1);
            }
        }
        const unsigned char id = IdAt(R6Address(0x36));
        SetWord(R6Address(0x2C), KindOf(id) == 0xB ? id + 4u : 0xFFFFu);
        HandEffect1f(0xA8, static_cast<int>(LongEffect1f(R6Address(0xC)) * 13u + 0x5A));
        return;
    }
    Label(::bof3::Img(at::kListLabelB.v));
    if (R6(0xA) == 0) {
        const unsigned short pressed = Input_Pressed;
        if (Field_ConfirmButtons & pressed) {
            const unsigned char id = IdAt(R6Address(0x3A));
            if (KindOf(id) == 0xA && CharStats_ExtraItem != id) {
                SH_CALL(Inventory_Remove)(3, id, 1);
                const unsigned char old = CharStats_ExtraItem;
                if (old) SH_CALL(Inventory_Add)(3, old, 1);
                CharStats_ExtraItem = id;
                CharStats_ExtraCount = 1;
                SEffect1f count = 0;
                for (U i = 0; i < 0x80; ++i)
                    if (KindOf(BEffect1f(::bof3::Img(at::kAccessoryIds.v) + i)) == 0xA) ++count;
                if (static_cast<SEffect1f>(LongEffect1f(R6Address(0x1C))) >= count && WordEffect1f(R6Address(0x38)) != 0) {
                    const SEffect1f scroll = static_cast<SEffect1f>(LongEffect1f(R6Address(0x38)) & 0xFFFF);
                    if (scroll > count - 9) SetWord(R6Address(0x38), WordEffect1f(R6Address(0x38)) - 1u);
                }
            }
            Sound(0x103);
            PickItemBack();
            return;
        }
        if (repeat & 0x1000u) {
            if (LongEffect1f(R6Address(0x10)) == 0) {
                if (WordEffect1f(R6Address(0x38)) != 0) {
                    Sound(0x100);
                    SetWord(R6Address(0x38), WordEffect1f(R6Address(0x38)) - 1u);
                    R6(8) = 4;
                    R6(0xA) = 3;
                }
            } else {
                Sound(0x100);
                SetLongEffect1f(R6Address(0x10), LongEffect1f(R6Address(0x10)) - 1);
            }
        } else if (repeat & 0x4000u) {
            const U cursor = LongEffect1f(R6Address(0x10));
            if (cursor == 8) {
                const SEffect1f bottom = static_cast<SEffect1f>((LongEffect1f(R6Address(0x38)) & 0xFFFF) + cursor);
                if (static_cast<SEffect1f>(LongEffect1f(R6Address(0x1C))) > bottom) {
                    Sound(0x100);
                    SetWord(R6Address(0x38), WordEffect1f(R6Address(0x38)) + 1u);
                    R6(8) = 0;
                    R6(0xA) = 3;
                }
            } else if (static_cast<SEffect1f>(cursor) < static_cast<SEffect1f>(LongEffect1f(R6Address(0x1C)))) {
                Sound(0x100);
                SetLongEffect1f(R6Address(0x10), LongEffect1f(R6Address(0x10)) + 1);
            }
        } else if (pressed & Sigma(4)) {   // DIVERGENCE DIV-0139: Japan's L1 through σ (0x52AC58)
            const U old = LongEffect1f(R6Address(0x38));
            const auto scrolled = static_cast<std::uint16_t>(old - 9u);
            SetWord(R6Address(0x38), scrolled);
            if (static_cast<short>(scrolled) >= 0) {
                Sound(0x100);
            } else {
                SetWord(R6Address(0x38), 0);
                if (static_cast<std::uint16_t>(old) != 0) {
                    Sound(0x100);
                } else if (LongEffect1f(R6Address(0x10)) != 0) {
                    Sound(0x100);
                    SetLongEffect1f(R6Address(0x10), 0);
                }
            }
        } else if (pressed & Sigma(8)) {   // DIVERGENCE DIV-0139: Japan's R1 through σ (0x52ACAA)
            const U count = LongEffect1f(R6Address(0x1C));
            if (static_cast<SEffect1f>(count) <= 8) {
                if (static_cast<SEffect1f>(LongEffect1f(R6Address(0x10))) < static_cast<SEffect1f>(count)) {
                    Sound(0x100);
                    SetLongEffect1f(R6Address(0x10), LongEffect1f(R6Address(0x1C)));
                }
            } else {
                const U old = LongEffect1f(R6Address(0x38));
                SetWord(R6Address(0x38), WordEffect1f(R6Address(0x38)) + 9u);
                const SEffect1f bottom = static_cast<SEffect1f>((LongEffect1f(R6Address(0x38)) & 0xFFFF) + 8);
                if (static_cast<SEffect1f>(count) > bottom) {
                    Sound(0x100);
                } else {
                    const auto last = static_cast<std::uint16_t>(count - 8u);
                    SetWord(R6Address(0x38), last);
                    if (last != static_cast<std::uint16_t>(old)) {
                        Sound(0x100);
                    } else if (static_cast<SEffect1f>(LongEffect1f(R6Address(0x10))) < 8) {
                        Sound(0x100);
                        SetLongEffect1f(R6Address(0x10), 8);
                    }
                }
            }
        }
    }
    const unsigned char id = IdAt(R6Address(0x3A));
    SetWord(R6Address(0x2C), KindOf(id) == 0xA ? id + 4u : 0xFFFFu);
    HandEffect1f(0xA8, static_cast<int>(LongEffect1f(R6Address(0x10)) * 13u + 0x5A));
}

// original 0x52ADA0: ChoiceMenu_Choices[1] - the title (MessagePools + the word
// 0x803642) at (0x1D, 0x14); with record 6 at 8: 0x2000 pressed: sound 0x102,
// the page word +0x3C = +0x3E + 1 (0 from 0x17: 23 pages), +8 = 0, record 6 up;
// else 0x8000: sound 0x102, +0x3C = +0x3E - 1 (0x16 below 0), +8 = 1, record 6
// up; else a cancel button: sound 0x106, +4 = 0, +3 down.
extern "C" void __cdecl ChoiceMenu_DataPage(void) {
    Text(0x1D, 0x14, At(::bof3::Img(kPools.v) + WRest1g(::bof3::Img(kPoolWordData.v))));
    if (Effect_Objects[6].state != 8) return;
    const unsigned pressed = Input_Pressed;
    if (pressed & 0x2000) {
        Sound(0x102);
        const U page = (static_cast<std::uint32_t>(kEff6Shown) + 1) & 0xFFFF;
        kEff6Page = page < 0x17 ? page : 0;
        rv::Bytes(&Effect_Objects[6])[0x8] = 0;
        Effect_Objects[6].state = static_cast<unsigned char>(Effect_Objects[6].state + 1);
        return;
    }
    if (pressed & 0x8000) {
        Sound(0x102);
        const auto page = static_cast<short>(WRest1g(AddrOf(&kEff6Shown)) - 1);
        kEff6Page = page < 0 ? 0x16u : static_cast<U>(page);
        rv::Bytes(&Effect_Objects[6])[0x8] = 1;
        Effect_Objects[6].state = static_cast<unsigned char>(Effect_Objects[6].state + 1);
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
    Text(0x1D, 0x14, At(::bof3::Img(kPools.v) + (static_cast<U>(LRest1g(::bof3::Img(kPoolWordRule.v))) & 0xFFFF)));
    if (Effect_Objects[6].state != 0xE || rv::Bytes(&Effect_Objects[6])[0x8] != 0) return;
    const unsigned pressed = Input_Pressed;
    if (Field_CancelButtons & pressed) {
        Sound(0x106);
        S()[4] = 0;
        unsigned char* const s = S();
        s[3] = static_cast<unsigned char>(s[3] - 1);
        return;
    }
    if (pressed & 0x4000) {
        const U page = (static_cast<std::uint32_t>(kEff6Page) + 1) & 0xFFFF;
        rv::Bytes(&Effect_Objects[6])[0x8] = 0xFF;
        kEff6Page = page > 5 ? 0 : page;
        return;
    }
    if (pressed & 0x1000) {
        const auto page = static_cast<short>(WRest1g(AddrOf(&kEff6Page)) - 1);
        SetW(AddrOf(&kEff6Page), static_cast<U>(page));
        rv::Bytes(&Effect_Objects[6])[0x8] = 1;
        if (page < 0) kEff6Page = 5;
    }
}

// original 0x52AF30: LeaderPanel_Stage9Steps[4] - record 6 idle: record 1 up,
// back to stage 1 step 0.
extern "C" void __cdecl LeaderPanel_S9Back(void) {
    if (Effect_Objects[6].state != 0) return;
    Effect_Objects[1].state = static_cast<unsigned char>(Effect_Objects[1].state + 1);
    S()[2] = 1;
    S()[3] = 0;
}

// original 0x52AF60: LeaderPanel_Stages[10] - jmp through LeaderPanel_Stage10Steps
// by +3, unchecked (ours aborts past its 2).
extern "C" void __cdecl LeaderPanel_S10(void) {
    RunRest1g("LeaderPanel_S10 (0x52AF60)", LeaderPanel_Stage10Steps, LeaderPanel_Stage10Steps_count, Sprite_Current[3],
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
        if ((static_cast<unsigned char>(Input_Held) & Sigma(0x40)) != 0 || Field_Kind2Z == 0x150000) {
            if (Effect_Objects[3].state == 0) {
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
    Effect_Objects[1].state = static_cast<unsigned char>(Effect_Objects[1].state + 1);
    S()[2] = 1;
    S()[3] = 0;
}

// original 0x52B0E0: LeaderPanel_Stages[11] - jmp through LeaderPanel_Stage11Steps
// by +3, unchecked (ours aborts past its 4).
extern "C" void __cdecl LeaderPanel_S11(void) {
    RunRest1g("LeaderPanel_S11 (0x52B0E0)", LeaderPanel_Stage11Steps, LeaderPanel_Stage11Steps_count, Sprite_Current[3],
        "Sprite_Current +3");
}

// original 0x52B100: LeaderPanel_Stage11Steps[0] - record 3 idle:
// Transition_Start(0), +3 up.
extern "C" void __cdecl LeaderPanel_S11FadeOut(void) {
    if (Effect_Objects[3].state != 0) return;
    SH_CALL(Transition_Start)(0);
    unsigned char* const s = S();
    s[3] = static_cast<unsigned char>(s[3] + 1);
}

// original 0x52B120: LeaderPanel_Stage11Steps[1] - the wait word clear: the
// four bytes 0x93985C..0x93985F and Draw_PassFlags cleared, Game_Step = 3, +3
// up (game mode 8's step 3).
extern "C" void __cdecl LeaderPanel_S11Switch(void) {
    if (Transition_Busy != 0) return;
    Effect1g_TradeState = 0;
    B(::bof3::Img(kDrawMode.v) + 2) = 0;
    B(::bof3::Img(kDrawMode.v) + 1) = 0;
    B(::bof3::Img(kDrawMode.v) + 3) = 0;
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
    if (Transition_Busy != 0) return;
    Effect_Objects[1].state = static_cast<unsigned char>(Effect_Objects[1].state + 1);
    S()[2] = 1;
    S()[3] = 0;
}

// original 0x52B1B0: the pose 0x6BC71D other than 0x6BC71C: sound 0x203, 0x202
// or 0x205 for pose 0, 1, 2 (none for any other). Neither byte is written.
extern "C" void __cdecl LeaderPanel_PoseSound(void) {
    const unsigned char pose = Effect1e_Pose;
    if (pose == Effect1e_PoseWas) return;
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
    const auto state = static_cast<unsigned char>(Effect_Objects[4].state + 1);
    unsigned char* const s = S();
    Effect_Objects[3].state = 7;
    Effect_Objects[4].state = state;
    Effect_Objects[4].param = 3;
    s[3] = static_cast<unsigned char>(s[3] + 1);
    TickRest1g();
}

// original 0x52B250: the two record pointers - 0x90412E set: 0x939A24 =
// 0x66A528 + 20 * (it - 0x1C); 0x904130 set: 0x939A20 = 0x66A4E8 + 10 * (it -
// 0x2E). An item byte below its base gives a pointer before the table (32-bit,
// as the original computes it; nothing is read here).
extern "C" void __cdecl LeaderPanel_SetRecords(void) {
    const unsigned b = CharStats_ExtraItem;
    if (b != 0) Effect1a_PanelSet = ::bof3::Img(kRecordsB.v) + 20u * (b - 0x1Cu);
    const unsigned a = B(::bof3::Img(kItemA.v));
    if (a != 0) Effect1a_PanelScript = ::bof3::Img(kRecordsA.v) + 10u * (a - 0x2Eu);
}

// original 0x52B2A0: record 3's mode = `mode`'s byte; when its +1 is 0, 4 or 6,
// also its +7 = 1 and +1 up.
extern "C" void __cdecl LeaderPanel_Effect3Mode(unsigned mode) {
    const unsigned char state = Effect_Objects[3].state;
    if (state == 4 || state == 6 || state == 0) {
        rv::Bytes(&Effect_Objects[3])[0x7] = 1;
        Effect_Objects[3].state = static_cast<unsigned char>(state + 1);
    }
    Effect_Objects[3].param = static_cast<unsigned char>(mode);
}

// original 0x52B2E0: Input_Pressed bit 0x4000: 0x903850 = 1, +0xA = 8; else
// Input_Held bit 0x4000 with +0xA not 0: +0xA down, 0x903850 = 1; else
// 0x903850 = 0.
extern "C" void __cdecl LeaderPanel_HoldTest(void) {
    if (Input_Pressed & 0x4000) {
        kHoldFlag = 1;
        S()[0xA] = 8;
        return;
    }
    if (Input_Held & 0x4000) {
        unsigned char* const s = S();
        if (s[0xA] != 0) {
            s[0xA] = static_cast<unsigned char>(s[0xA] - 1);
            kHoldFlag = 1;
            return;
        }
    }
    kHoldFlag = 0;
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
    unsigned char now = Effect1e_PressNow;
    unsigned char depth = rv::Bytes(Effect_Objects)[0x7];
    for (unsigned i = 0; i < 4; ++i) {
        const U row = ::bof3::Img(kComboTable.v) + 8 * i;
        const unsigned char step = B(::bof3::Img(kComboStep.v) + i);
        const unsigned char* const at = At(static_cast<U>(LRest1g(row)) + step);
        const unsigned char want = static_cast<unsigned char>(at[0] & 0x7F);
        if (step != 0) B(::bof3::Img(kComboTimer.v) + i) = static_cast<unsigned char>(B(::bof3::Img(kComboTimer.v) + i) - 1);
        bool advance = false;
        if (now == 1) {
            if (step == 0) advance = true;
            else if (B(::bof3::Img(kComboTimer.v) + i) < 4 && want == now) advance = true;
            else {
                B(::bof3::Img(kComboTimer.v) + i) = 0;
                B(::bof3::Img(kComboStep.v) + i) = 0;
                continue;
            }
        } else {
            if (now != 0) continue;
            if (B(::bof3::Img(kComboTimer.v) + i) != now) continue;
            if (want != 0) {
                B(::bof3::Img(kComboTimer.v) + i) = 0;
                B(::bof3::Img(kComboStep.v) + i) = 0;
                continue;
            }
            advance = true;
        }
        if (!advance) continue;
        if ((at[0] & 0x80) == 0) {
            B(::bof3::Img(kComboTimer.v) + i) = 0xC;
            B(::bof3::Img(kComboStep.v) + i) = static_cast<unsigned char>(step + 1);
            continue;
        }
        const unsigned char* const rb = RecordBRest1g();
        const auto base = static_cast<signed char>(At(row + 4)[0]);
        const auto plus = static_cast<signed char>(rb[0xF]);
        if (static_cast<int>(base) + plus >= static_cast<signed char>(depth)) {
            depth = static_cast<unsigned char>(base + plus);
            rv::Bytes(Effect_Objects)[0x7] = depth;
            rv::Bytes(Effect_Objects)[0xa] = RecordBRest1g()[0x10];
        }
        now = Effect1e_PressNow;
        B(::bof3::Img(kComboStep.v) + i) = 0;
        B(::bof3::Img(kComboTimer.v) + i) = 0;
    }
}

// original 0x52B460: 0x6BC708 = 0x6BC717; 0x6BC717 = 1 when Input_Pressed has
// any of 0xE020, else 0.
extern "C" void __cdecl LeaderPanel_PressLatch(void) {
    Effect1e_PressWas = Effect1e_PressNow;
    Effect1e_PressNow = (Input_Pressed & Sigma(0xE020)) != 0 ? 1 : 0;
}

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
        const unsigned spot = AreaEntry_PendingFlags;
        unsigned count = At(::bof3::Img(kSpotCounts.v) + 23 * spot + k)[0];
        if (count == 0) continue;
        do {
            if (index >= kSpriteCount)
                bof3::Fatal("Fish_Spawn (0x52B480): spot %u's counts fill more than the %u Sprite_Objects records - the "
                            "original writes record %u at 0x%X (docs/rest_1g.md section 7)",
                            spot, kSpriteCount, index, (unsigned)(Key(rv::Bytes(Sprite_Objects)) + index * kSpriteStride));
            unsigned char* const rec = rv::Bytes(&Sprite_Objects[index]);
            Sprite_Current = rec;
            Field_ActiveMember = rec;
            rec[0] = 0x21;
            S()[5] = static_cast<unsigned char>(index);
            S()[6] = static_cast<unsigned char>(k);
            const unsigned r = Rnd();
            SetWordRest1g(AM() + 0x9C, r & 0xFF);
            for (;;) {
                const unsigned char m = Kind(S()[6], 0x1F);
                unsigned char* const am = AM();
                const U size = Word(am + 0x9C);
                if (size <= m) break;
                if ((m >> 1) == 0)
                    bof3::Fatal("Fish_Spawn (0x52B480): kind %u's top size (0x%X) is %u and the size rolled is %u - the "
                                "original subtracts M / 2 = 0 for ever (docs/rest_1g.md section 7)",
                                (unsigned)S()[6], (unsigned)(Fish_Kinds_addr + kKindStride * S()[6] + 0x1F), (unsigned)m, (unsigned)size);
                SetWordRest1g(am + 0x9C, size - (m >> 1));
            }
            {
                unsigned char* const am = AM();
                const unsigned char m = Kind(S()[6], 0x1F);
                const U size = Word(am + 0x9C);
                U v = size;
                if (size < static_cast<U>(m >> 1)) v = m >> 1;
                else if (size > m) v = m;
                SetWordRest1g(am + 0x9C, v);
            }
            {
                const unsigned kind = S()[6];
                unsigned char* const am = AM();
                if (Kind(kind, 0x1F) == 0)
                    bof3::Fatal("Fish_Spawn (0x52B480): kind %u's top size (0x%X) is 0 - the original divides by it "
                                "(docs/rest_1g.md section 7)",
                                kind, (unsigned)(Fish_Kinds_addr + kKindStride * kind + 0x1F));
                const auto tenths = static_cast<std::int32_t>(Word(am + 0x9C) * 10u) / static_cast<std::int32_t>(Kind(kind, 0x1F));
                SetWordRest1g(am + 0x98, static_cast<U>((tenths * static_cast<std::int32_t>(Kind(kind, 0x1E))) / 10));
                unsigned char* const am2 = AM();
                SetWordRest1g(am2 + 0x9A, Word(am2 + 0x98));
            }
            const unsigned kind = S()[6];
            if (kind != 0x15) {
                const unsigned char m = Kind(kind, 0x1F);
                if (Word(AM() + 0x9C) == m) {
                    const int extra = m / 10;
                    for (int n = 0; n <= extra; ++n) {
                        if ((Rnd() & 1) == 0) break;
                        unsigned char* const am = AM();
                        SetWordRest1g(am + 0x9C, Word(am + 0x9C) + 1);
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
        unsigned char* const rec = rv::Bytes(&Sprite_Objects[i]);
        if (rec[0] == 0) continue;
        Sprite_Current = rec;
        Field_ActiveMember = rec;
        RunRest1g("Fish_RunAll (0x52B6C0)", Fish_States, Fish_States_count, rec[1], "the record's +1");
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
    SetLRest1g(S(), 0x34, (0x14u - (r2 & 0xF)) << 16);
    const U zbase = 0x3E0000u - ((static_cast<U>(AM()[0x9E]) * 3u) << 18);
    const unsigned r3 = Rnd();
    SetLRest1g(S(), 0x38, zbase - ((r3 & 0xF) << 16));
    const U top = (0u - static_cast<U>(Kind(S()[6], 0x1A))) << 8;
    const unsigned r4 = Rnd();
    rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(S())->height_whole, top - ((r4 & 7) << 5));
    const short ground = Elevation(static_cast<U>(LRest1g(S(), 0x34)), static_cast<U>(LRest1g(S(), 0x38)));
    unsigned char* const s = S();
    const short h = Sh(s, 0x3E);
    if (h == 0) rv::As<records::SpriteObject>(s)->height_whole = 0xFFE0;
    else if (h < ground) rv::As<records::SpriteObject>(s)->height_whole = static_cast<U>(ground);
    S()[8] = 4;
    SetLRest1g(S(), 0x14, 0);
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
            const unsigned char nibbling = Fish_Nibbling;
            unsigned char* s = S();
            if (nibbling == 0 && rv::As<records::SpriteObject>(s)->height_whole == 0 && (bits & 3) == 0 && ObjTrio[0].step != 4 && s[6] != 0x16) {
                Fish_Nibbling = 1;
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
            const U x = (static_cast<U>(LRest1g(s, 0xC)) << 4) + static_cast<U>(LRest1g(s, 0x34));
            const U z = (static_cast<U>(LRest1g(s, 0x10)) << 4) + static_cast<U>(LRest1g(s, 0x38));
            const short ground = Elevation(x, z);
            s = S();
            const short h = Sh(s, 0x3E);
            if (h < ground) {
                SetLRest1g(s, 0x14, static_cast<U>(DivPow2(ground - h, 4)));
            } else {
                SetLRest1g(s, 0x14, 0);
                if (bits & 0x10) {
                    unsigned char* const c = S();
                    const std::int32_t level = Kind(c[6], 0x1A);
                    if (bits & 0x20) {
                        const short ch = Sh(c, 0x3E);
                        if (ch != 0 && static_cast<std::int32_t>((0u - static_cast<U>(level)) << 8) >= ch + 0x20)
                            SetLRest1g(c, 0x14, 2);
                    } else {
                        const std::int32_t below = Sh(c, 0x3E) - 0x20;
                        if (ground <= below && static_cast<std::int32_t>((0xFFFFFFFFu - static_cast<U>(level)) << 8) <= below)
                            SetLRest1g(c, 0x14, 0xFFFFFFFEu);
                    }
                }
            }
            if (bits & 0x40) {
                SetLRest1g(S(), 0xC, 0);
                SetLRest1g(S(), 0x10, 0);
                SetLRest1g(S(), 0x14, 0);
            }
            S()[9] = 0x10;
        }
    }
    SH_CALL(Fish_Step)();
    TickRest1g();
}

// original 0x52BB20: Fish_States[2] - Sprite_ScriptTick done: 0x6BC716 = 0, the
// kind's bank +0x10, +0 bit 5 set, +0x2A and the animation by +8, +0x48 = 2,
// the steps 0, 0, -2, +9 = 0x10, state 1.
extern "C" void __cdecl Fish_Settle(void) {
    if (TickRest1g() == 0) return;
    const unsigned kind = S()[6];
    Fish_Nibbling = 0;
    Bank(KindWord(kind, 0x10));
    unsigned char* s = S();
    s[0] = static_cast<unsigned char>(s[0] | 0x20);
    s = S();
    s[0x2A] = DirByte(s[8]);
    S()[0x48] = 2;
    Animation(DirAnim(S()[8]));
    SetLRest1g(S(), 0xC, 0);
    SetLRest1g(S(), 0x10, 0);
    SetLRest1g(S(), 0x14, 0xFFFFFFFEu);
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
        TickRest1g();
        return;
    }
    if (SH_CALL(Fish_LureInReach)() == 0) {
        S()[1] = 1;
        SH_CALL(Fish_Step)();
        TickRest1g();
        return;
    }
    const unsigned bits = Rnd();
    unsigned char* s = S();
    const auto dx = static_cast<std::int32_t>(static_cast<std::uint32_t>(Effect_Objects[0].x) - static_cast<U>(LRest1g(s, 0x34)));
    const auto dz = static_cast<std::int32_t>(static_cast<std::uint32_t>(Effect_Objects[0].z) - static_cast<U>(LRest1g(s, 0x38)));
    int way = -1;
    if (dx < 0) way = dz < 0 ? 0 : dz > 0 ? 6 : 7;
    else if (dx == 0) way = dz < 0 ? 1 : dz > 0 ? 5 : -1;
    else way = dz < 0 ? 2 : dz == 0 ? 3 : 4;
    if (way >= 0) {
        s[8] = static_cast<unsigned char>(way);
        s = S();
    }
    const unsigned nib = RecordBRest1g()[0xE] & 0xF;
    const auto diff = static_cast<signed char>(rv::Bytes(Effect_Objects)[0x7] - Kind(s[6], 0x14 + nib));
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
    const U x = (static_cast<U>(LRest1g(s, 0xC)) << 4) + static_cast<U>(LRest1g(s, 0x34));
    const U z = (static_cast<U>(LRest1g(s, 0x10)) << 4) + static_cast<U>(LRest1g(s, 0x38));
    const short ground = Elevation(x, z);
    s = S();
    const short h = Sh(s, 0x3E);
    if (h < ground) {
        SetLRest1g(s, 0x14, static_cast<U>(DivPow2(ground - h, 4)));
    } else {
        SetLRest1g(s, 0x14, 0);
        unsigned char* const c = S();
        const short lure = static_cast<std::int16_t>(Effect_Objects[0].height_whole);
        const short ch = Sh(c, 0x3E);
        if (lure < ch) SetLRest1g(c, 0x14, 0xFFFFFFFEu);
        else if (lure > ch) SetLRest1g(c, 0x14, 2);
    }
    DirEnsure();
    if (SH_CALL(Fish_LureClose)() != 0) {
        const unsigned kind = S()[6];
        const unsigned r = Rnd() & 0xF;
        const unsigned char* rb = RecordBRest1g();
        if (static_cast<int>(Kind(kind, 0x1C)) > static_cast<int>(r) || (rb[0xE] & 0x80) != 0) {
            rb = RecordBRest1g();
            if (Kind(kind, 0x14 + (rb[0xE] & 0xF)) != 9) {
                unsigned char* const f = S();
                const unsigned char index = f[5];
                ObjTrio[0].step = 4;
                Effect1a_PanelObject = index;
                ObjTrio[0].state3 = 0;
                Effect_Objects[0].state = 5;
                SetLRest1g(::bof3::Img(kEff0StepX.v), 0);
                SetLRest1g(::bof3::Img(kEff0StepY.v), 0);
                Effect_Objects[0].step_y = 0;
                SetLRest1g(f, 0xC, 0);
                SetLRest1g(S(), 0x10, 0);
                SetLRest1g(S(), 0x14, 0);
                S()[9] = 0;
                S()[0xA] = 0;
                S()[0xB] = 0;
                unsigned char* const t = S();
                t[1] = static_cast<unsigned char>(t[1] + 1);
                S()[8] = 1;
                DirEnsure();
                TickRest1g();
                return;
            }
        }
    }
    S()[9] = 0x10;
    if (level == 3 && (Rnd() & 3) == 0) {
        unsigned char* t = S();
        SetLRest1g(t, 0xC, static_cast<U>(LRest1g(t, 0xC)) << 1);
        t = S();
        SetLRest1g(t, 0x10, static_cast<U>(LRest1g(t, 0x10)) << 1);
        t = S();
        SetLRest1g(t, 0x14, static_cast<U>(LRest1g(t, 0x14)) << 1);
        S()[9] = 8;
    }
    SH_CALL(Fish_Step)();
    TickRest1g();
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
    const unsigned char level = rv::Bytes(&Effect_Objects[5])[0x30];
    const auto width = static_cast<unsigned char>((RecordARest1g()[3] + 3) << 2);
    const auto lo = static_cast<unsigned char>(level - width);
    const auto hi = static_cast<unsigned char>(width + level);
    unsigned char* s = S();
    const U chances = ::bof3::Img(Fish_Kinds_addr) + kKindStride * s[6];
    if (LRest1g(s, 0x38) <= 0x8000) {
        SetLRest1g(::bof3::Img(kEff5Frame.v), 0xFFFFFFF9u);
        Effect_Objects[0].state = 0;
        ObjTrio[0].step = 6;
        ObjTrio[0].state3 = 0;
        s[1] = static_cast<unsigned char>(s[1] + 1);
        return;
    }
    if (s[4] == 3) {
        if (TickRest1g() != 0) {
            Bank(KindWord(S()[6], 0x10));
            unsigned char* t = S();
            t[0] = static_cast<unsigned char>(t[0] | 0x20);
            t = S();
            t[0x2A] = DirByte(t[8]);
            Animation(DirAnim(S()[8]));
            rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(S())->height_whole, 0xFFE0);
            S()[4] = 0;
            S()[0x48] = 2;
            SetLRest1g(S(), 0x10, 0);
            Effect_Objects[0].flags = static_cast<unsigned char>(Effect_Objects[0].flags & 0xBF);
        }
        unsigned char* const t = S();
        if (rv::As<records::SpriteObject>(t)->script_pos <= 0x14) return;
        t[0xA] = static_cast<unsigned char>(t[0xA] + t[0x10]);
        unsigned char* const c = S();
        const auto a = static_cast<signed char>(c[0xA]);
        c[0xA] = static_cast<unsigned char>(a < -0x30 ? -0x30 : a > 0x30 ? 0x30 : a);
        return;
    }
    unsigned tier = 0;
    if (Effect_Objects[4].state == 0) {
        const auto t = static_cast<signed char>(s[0xA]);
        if (t > static_cast<signed char>(hi)) {
            s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
            s = S();
            if (s[0xB] >= RecordARest1g()[8]) {
                s[0xB] = 0;
                if (LRest1g(::bof3::Img(kEff5Frame.v)) < 0) {
                    s = S();
                    if (LRest1g(s, 0x10) > 0) {
                        if (SH_CALL(Fish_Chance)(hi, 1) != 0) {
                            unsigned char* const f = S();
                            Effect_Objects[0].state = 0;
                            SetLRest1g(::bof3::Img(kEff5Frame.v), 0xFFFFFFF9u);
                            ObjTrio[0].step = 6;
                            ObjTrio[0].state3 = 0;
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
            if (s[0xB] >= RecordARest1g()[9]) {
                s[0xB] = 0;
                if (SH_CALL(Fish_Chance)(lo, 2) != 0) {
                    unsigned char* const f = S();
                    ObjTrio[0].step = 7;
                    ObjTrio[0].state3 = 0;
                    Effect_Objects[0].state = 0;
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
                Effect_Objects[0].flags = static_cast<unsigned char>(Effect_Objects[0].flags | 0x40);
                Sound(0x206);
                Bank(KindWord(S()[6], 0x12));
                unsigned char* t = S();
                t[0] = static_cast<unsigned char>(t[0] & 0xDF);
                S()[0x2A] = 0;
                S()[0x48] = 0;
                rv::Store<std::uint32_t>(rv::As<records::SpriteObject>(S())->height_whole, 0);
                Animation(0);
                SetLRest1g(S(), 0x10, 0xFFFFFFFDu);
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
                SetLRest1g(f, 0x10, (r & (Kind(f[6], 0x1D) & 7)) + 1);
                f = S();
                if (static_cast<signed char>(f[0xA]) < 0) {
                    unsigned char* const cell = f + 0x10;
                    const unsigned r2 = Rnd();
                    SetLong(cell, static_cast<std::int32_t>(static_cast<U>(Long(cell)) + (r2 & 1)));
                    f = S();
                }
                const std::int32_t v = LRest1g(f, 0x10);
                SetLRest1g(f, 0x10, static_cast<U>(v < 0 ? 0 : v > 5 ? 5 : v));
                if (LRest1g(::bof3::Img(kEff5Frame.v)) < 0) {
                    const std::int32_t gap = static_cast<signed char>(S()[0xA]) - static_cast<short>(WRest1g(::bof3::Img(kEff5Level.v)));
                    AM()[0x81] = gap < -0x30 ? 0xFD : gap < -0x18 ? 0xFE : 0xFF;
                }
            } else {
                SetLRest1g(S(), 0x10, 0);
                AM()[0x81] = 0;
            }
            unsigned char* f = S();
            if (Kind(f[6], 0x20) == 0) {
                SetLRest1g(f, 0xC, 0);
            } else if ((Rnd() & 1) != 0) {
                const unsigned way = Rnd() & 1;
                SetLRest1g(S(), 0xC, way != 0 ? 0x1000u : 0xFFFFF000u);
            } else {
                SetLRest1g(S(), 0xC, 0);
            }
            S()[8] = 1;
            S()[4] = 0;
        } else if (static_cast<int>(Rnd() & 0xF) < row[2]) {
            // the rest
            SetLRest1g(S(), 0xC, 0);
            SetLRest1g(S(), 0x10, 0);
            AM()[0x81] = 1;
            S()[4] = 1;
        } else if (S()[0xA] != 0) {
            // the drift
            AM()[0x81] = 0;
            SetLRest1g(S(), 0xC, 0);
            SetLRest1g(S(), 0x10, 0xFFFFFFFFu);
            S()[4] = 2;
        }
        const auto frames = static_cast<unsigned char>(((Rnd() & 3) + 1) * (8 - tier));
        S()[9] = frames;
    } else {
        s[9] = static_cast<unsigned char>(s[9] - 1);
        const auto wear = static_cast<signed char>(AM()[0x81]);
        bool wears;
        if (wear > 0) {
            wears = (Frame_Counter & 0xF) == 0 && LRest1g(S(), 0x10) >= 0 && Sh(AM(), 0x9A) < static_cast<short>(Sh(AM(), 0x98) >> 1);
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
    const short limit = static_cast<short>(WRest1g(::bof3::Img(kEff5Level.v)));
    const short tension = static_cast<signed char>(f[0xA]);
    if (tension > limit && LRest1g(f, 0x10) > 0) {
        // reeled in: toward the shore
        SetLRest1g(f, 0x38, static_cast<U>(LRest1g(f, 0x38)) - 0x1000);
        unsigned char* e = S();
        if (Sh(e, 0x3E) < 0) {
            rv::As<records::SpriteObject>(e)->height_whole = rv::As<records::SpriteObject>(e)->height_whole + 8;
            e = S();
        }
        const std::int32_t z = LRest1g(e, 0x38);
        if (z > 0x150000) {
            Field_Kind2Z = z;
            MoveScript_F3Divisor = 0x10;
            if (z > 0x3C0000) Field_Kind2Z = 0x3C0000;
        }
        const std::int32_t vx = LRest1g(e, 0xC);
        bool reread = true;
        if (vx < 0) {
            if (LRest1g(e, 0x34) > 0x60000) reread = false;
            else SetLRest1g(e, 0xC, 0u - static_cast<U>(vx));
        } else if (vx > 0) {
            if (LRest1g(e, 0x34) < 0x140000) reread = false;
            else SetLRest1g(e, 0xC, 0u - static_cast<U>(vx));
        } else {
            SetLRest1g(e, 0xC, 0);
        }
        if (reread) e = S();
        SetLRest1g(e, 0x34, static_cast<U>(LRest1g(e, 0x34)) + static_cast<U>(LRest1g(e, 0xC)));
        e = S();
        const std::int32_t step = LRest1g(e, 0xC);
        e[8] = static_cast<unsigned char>(step < -0x800 ? 0 : step > 0x800 ? 2 : 1);
    } else if (tension < limit && LRest1g(::bof3::Img(kEff5Frame.v)) < 0) {
        // let out: away from the shore, x drawn toward 0xF0000
        const std::int32_t left = 0x3E - static_cast<std::int32_t>(Sh(f, 0x3A));
        if (left == 0) {
            SetLRest1g(f, 0xC, 0);
        } else {
            const auto span = static_cast<std::int32_t>(0xF0000u - static_cast<U>(LRest1g(f, 0x34))) >> 4;
            SetLRest1g(f, 0xC, static_cast<U>(span / left));
            unsigned char* const c = S();
            const std::int32_t v = LRest1g(c, 0xC);
            SetLRest1g(c, 0xC, static_cast<U>(v < -0x1000 ? -0x1000 : v > 0x1000 ? 0x1000 : v));
        }
        unsigned char* e = S();
        SetLRest1g(e, 0x34, static_cast<U>(LRest1g(e, 0x34)) + static_cast<U>(LRest1g(e, 0xC)));
        e = S();
        if (Sh(e, 0x3E) < 0) {
            rv::As<records::SpriteObject>(e)->height_whole = rv::As<records::SpriteObject>(e)->height_whole + 8;
            e = S();
        }
        SetLRest1g(e, 0x38, static_cast<U>(LRest1g(e, 0x38)) + 0x1000);
        unsigned char* const c = S();
        const std::int32_t z = LRest1g(c, 0x38);
        if (z > 0x150000) {
            Field_Kind2Z = z;
            MoveScript_F3Divisor = 0x10;
            if (z > 0x3C0000) Field_Kind2Z = 0x3C0000;
        }
        c[8] = c[4] != 0 ? 5 : 1;
    }
    DirEnsure();
    TickRest1g();
}

// original 0x52C7C0: Fish_States[5] - jmp through Fish_S5Steps by +2,
// unchecked (ours aborts past its 2).
extern "C" void __cdecl Fish_S5(void) {
    RunRest1g("Fish_S5 (0x52C7C0)", Fish_S5Steps, Fish_S5Steps_count, Sprite_Current[2], "Sprite_Current +2");
}

// original 0x52C7E0: Fish_S5Steps[0] - the steps toward the half-cell below x
// and z: ((v & 0xFFFF8000) - v) >> 3 unsigned (a logical shift: a step of
// 0x1FFF.... for any fraction), +9 = 8, +2 up.
extern "C" void __cdecl Fish_S5Center(void) {
    unsigned char* s = S();
    const U x = static_cast<U>(LRest1g(s, 0x34));
    SetLRest1g(s, 0xC, ((x & 0xFFFF8000u) - x) >> 3);
    s = S();
    const U z = static_cast<U>(LRest1g(s, 0x38));
    SetLRest1g(s, 0x10, ((z & 0xFFFF8000u) - z) >> 3);
    S()[9] = 8;
    s = S();
    s[2] = static_cast<unsigned char>(s[2] + 1);
}

// original 0x52C830: Fish_S5Steps[1] - x and z moved by the steps, +9 down; at
// 0 the steps cleared, x's and z's low words & 0x8000, state 1 sub-state 0;
// then (a tail jmp) Sprite_ScriptTick.
extern "C" void __cdecl Fish_S5Move(void) {
    unsigned char* s = S();
    SetLRest1g(s, 0x34, static_cast<U>(LRest1g(s, 0x34)) + static_cast<U>(LRest1g(s, 0xC)));
    s = S();
    SetLRest1g(s, 0x38, static_cast<U>(LRest1g(s, 0x38)) + static_cast<U>(LRest1g(s, 0x10)));
    s = S();
    s[9] = static_cast<unsigned char>(s[9] - 1);
    if (S()[9] == 0) {
        SetLRest1g(S(), 0xC, 0);
        SetLRest1g(S(), 0x10, 0);
        s = S();
        rv::As<records::SpriteObject>(s)->x_frac = rv::As<records::SpriteObject>(s)->x_frac & 0x8000;
        s = S();
        rv::As<records::SpriteObject>(s)->z_frac = rv::As<records::SpriteObject>(s)->z_frac & 0x8000;
        S()[1] = 1;
        S()[2] = 0;
    }
    TickRest1g();
}

// original 0x52C8B0: Fish_States[6] - jmp through Fish_S6Steps by +2,
// unchecked (ours aborts past its 3).
extern "C" void __cdecl Fish_S6(void) {
    RunRest1g("Fish_S6 (0x52C8B0)", Fish_S6Steps, Fish_S6Steps_count, Sprite_Current[2], "Sprite_Current +2");
}

// original 0x52C8D0: Fish_S6Steps[0] - +0x3C..+0x3F cleared, sound 0x206, the
// kind's bank +0x12, +0 bit 5 off, +0x2A and +0x48 cleared, animation 0, +2 up.
extern "C" void __cdecl Fish_S6Begin(void) {
    SetLRest1g(S(), 0x3C, 0);
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
    if (rv::As<records::SpriteObject>(s)->script_pos == Word(At(::bof3::Img(kS6Frames.v) + 2 * s[6]))) {
        s[2] = static_cast<unsigned char>(s[2] + 1);
        return;
    }
    TickRest1g();
}

// original 0x52C970: Fish_S6Steps[2] - the leader's step (ObjTrio +3) above 9:
// the record freed (+0 = 0).
extern "C" void __cdecl Fish_S6Release(void) {
    if (ObjTrio[0].state3 > 9) S()[0] = 0;
}

// original 0x52C990: the member's strength +0x9A moved by the signed byte
// `delta`, kept in 0..+0x98 (s16).
extern "C" void __cdecl Fish_AdjustStrength(unsigned delta) {
    unsigned char* am = AM();
    SetWordRest1g(am + 0x9A, Word(am + 0x9A) + static_cast<U>(static_cast<short>(static_cast<signed char>(delta))));
    am = AM();
    const short now = Sh(am, 0x9A);
    if (now < 0) SetWordRest1g(am + 0x9A, 0);
    else if (now > Sh(am, 0x98)) SetWordRest1g(am + 0x9A, Word(am + 0x98));
    else SetWordRest1g(am + 0x9A, static_cast<U>(now));
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
        static_cast<unsigned char>(static_cast<signed char>(RecordARest1g()[3] + 3) * static_cast<signed char>(tier)) << 3);
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
    SetLRest1g(s, 0x34, static_cast<U>(LRest1g(s, 0x34)) + static_cast<U>(LRest1g(s, 0xC)));
    s = S();
    SetLRest1g(s, 0x38, static_cast<U>(LRest1g(s, 0x38)) + static_cast<U>(LRest1g(s, 0x10)));
    s = S();
    rv::As<records::SpriteObject>(s)->height_whole = rv::As<records::SpriteObject>(s)->height_whole + Word(s + 0x14);
}

// original 0x52CAC0: the steps +0xC / +0x10 from MoveCmd_F9Steps' row +8 (read
// in place, the byte unmasked), doubled for directions 2 and 6; x turned back
// at 0x80000 / 0x140000, z inside the kind's band (from +0x1B and the member's
// depth +0x9E, clamped to 0x1A0000..0x3E0000 above and 0x110000..0x320000
// below); then +8 = the row whose steps (each doubled when at most 0x400 in
// size) equal them, 8 when none does.
extern "C" void __cdecl Fish_Heading(void) {
    unsigned char* s = S();
    SetLRest1g(s, 0xC, static_cast<U>(Long(At(::bof3::Img(MoveCmd_F9Steps_addr) + 8u * s[8]))));
    s = S();
    SetLRest1g(s, 0x10, static_cast<U>(Long(At(::bof3::Img(MoveCmd_F9Steps_addr) + 4 + 8u * s[8]))));
    s = S();
    if (s[8] == 2 || s[8] == 6) {
        SetLRest1g(s, 0xC, static_cast<U>(LRest1g(s, 0xC)) << 1);
        s = S();
        SetLRest1g(s, 0x10, static_cast<U>(LRest1g(s, 0x10)) << 1);
        s = S();
    }
    const std::int32_t x = LRest1g(s, 0x34);
    if (x <= 0x80000) {
        if (LRest1g(s, 0xC) < 0) {
            SetLRest1g(s, 0xC, 0u - static_cast<U>(LRest1g(s, 0xC)));
            s = S();
        }
    } else if (x >= 0x140000) {
        if (LRest1g(s, 0xC) > 0) {
            SetLRest1g(s, 0xC, 0u - static_cast<U>(LRest1g(s, 0xC)));
            s = S();
        }
    }
    const U band = (static_cast<U>(Kind(s[6], 0x1B)) * 15u) << 16;
    const U depth = (static_cast<U>(AM()[0x9E]) * 3u) << 18;
    std::int32_t top = static_cast<std::int32_t>(band - depth + 0x3E0000u);
    std::int32_t bottom = static_cast<std::int32_t>(0x320000u - depth - band);
    top = top < 0x1A0000 ? 0x1A0000 : top > 0x3E0000 ? 0x3E0000 : top;
    bottom = bottom < 0x110000 ? 0x110000 : bottom > 0x320000 ? 0x320000 : bottom;
    const std::int32_t z = LRest1g(s, 0x38);
    if (z >= top) {
        if (LRest1g(s, 0x10) > 0) {
            SetLRest1g(s, 0x10, 0u - static_cast<U>(LRest1g(s, 0x10)));
            s = S();
        }
    } else if (z <= bottom) {
        if (LRest1g(s, 0x10) < 0) {
            SetLRest1g(s, 0x10, 0u - static_cast<U>(LRest1g(s, 0x10)));
            s = S();
        }
    }
    unsigned row = 0;
    for (U at = ::bof3::Img(MoveCmd_F9Steps_addr) + 4; at < ::bof3::Img(kF9StepsEnd); at += 8, ++row) {
        const U sx = static_cast<U>(Long(At(at - 4)));
        const U sz = static_cast<U>(Long(At(at)));
        const U wx = Abs32(sx) <= 0x400 ? sx + sx : sx;
        const U wz = Abs32(sz) <= 0x400 ? sz + sz : sz;
        if (static_cast<U>(LRest1g(s, 0xC)) == wx && static_cast<U>(LRest1g(s, 0x10)) == wz) break;
    }
    s[8] = static_cast<unsigned char>(row);
}

// original 0x52CC40: effect record 0 at state 4, no sprite picked (0x939A1C
// 0xFF), the leader at stage 3: kind 0x16 with the 20-byte record's nibble 4,
// or Sprite_PointInReach(the fish's x, z, height, margin record 0's +7 - the
// record's +0xF + 3, record 0): al 1. Otherwise +1 = 1 and al 0.
extern "C" unsigned char __cdecl Fish_LureInReach(void) {
    if (Effect_Objects[0].state == 4 && Effect1a_PanelObject == 0xFF && ObjTrio[0].step == 3) {
        unsigned char* const s = S();
        const unsigned char* const rb = RecordBRest1g();
        if (s[6] == 0x16 && (rb[0xE] & 0xF) == 4) return 1;
        const auto margin = static_cast<unsigned char>(rv::Bytes(Effect_Objects)[0x7] - rb[0xF] + 3);
        if (SH_CALL(Sprite_PointInReach)(LRest1g(s, 0x34), LRest1g(s, 0x38), Sh(s, 0x3E), margin, rv::Bytes(Effect_Objects)) != 0) return 1;
    }
    S()[1] = 1;
    return 0;
}

// original 0x52CCD0: effect record 0 at state 4, no sprite picked, record 4
// idle, the leader at stage 3, and the fish within 0x8000 of record 0 in x and
// in z (absolute 32-bit differences) and 0x400 in height (s16): al 1, else 0.
extern "C" unsigned char __cdecl Fish_LureClose(void) {
    if (Effect_Objects[0].state != 4 || Effect1a_PanelObject != 0xFF || Effect_Objects[4].state != 0 || ObjTrio[0].step != 3) return 0;
    const unsigned char* const s = S();
    if (Abs32(static_cast<U>(LRest1g(s, 0x34)) - static_cast<std::uint32_t>(Effect_Objects[0].x)) >= 0x8000) return 0;
    if (Abs32(static_cast<U>(LRest1g(s, 0x38)) - static_cast<std::uint32_t>(Effect_Objects[0].z)) >= 0x8000) return 0;
    if (Abs32(static_cast<U>(Sh(s, 0x3E) - static_cast<std::int16_t>(Effect_Objects[0].height_whole))) >= 0x400) return 0;
    return 1;
}

// original 0x52CD50: Sprite_UpdateScreen with +0x3C held at 0 and put back on
// the record current after the call; then, on the record current then, the
// scale 0x4650000 / d with the low byte cleared into +0x40 and (every cell read
// again through Sprite_Current) +0x44: d = +0x60 when above 0, +0x60 - 2 * s16
// +0x3E when below; at 0 both are 0 and eax is Sprite_Current. A d of 0 below
// is the original's divide fault: ours aborts.
extern "C" unsigned long __cdecl Sprite_UpdateScreenScaled(void) {
    unsigned char* s = Sprite_Current;
    const U held = LongEffect1f(Key(s + 0x3C));
    SetLongEffect1f(Key(s + 0x3C), 0);
    SH_CALL(Sprite_UpdateScreen)();
    unsigned char* const after = Sprite_Current;
    SetLongEffect1f(Key(after + 0x3C), held);
    s = Sprite_Current;
    const SEffect1f far = static_cast<SEffect1f>(LongEffect1f(Key(s + 0x60)));
    auto divisor = [](const unsigned char* r, bool below) -> SEffect1f {
        const U d = LongEffect1f(Key(r + 0x60));
        if (!below) return static_cast<SEffect1f>(d);
        const U lift = static_cast<U>(static_cast<SEffect1f>(static_cast<short>(WordEffect1f(Key(r + 0x3E))))) << 1;
        return static_cast<SEffect1f>(d - lift);
    };
    auto scale = [](SEffect1f d) -> U {
        if (d == 0)
            bof3::Fatal("Sprite_UpdateScreenScaled (0x52CD50): +0x60 is twice s16 +0x3E below 0 - the original divides "
                        "0x4650000 by 0 there (a divide fault; docs/effect_1f.md section 6)");
        return static_cast<U>(0x4650000 / d) & 0xFFFFFF00u;
    };
    if (far < 0) {
        SetLongEffect1f(Key(s + 0x40), scale(divisor(s, true)));
        unsigned char* const c = Sprite_Current;
        const U q = scale(divisor(c, true));
        SetLongEffect1f(Key(c + 0x44), q);
        return q;
    }
    if (far > 0) {
        SetLongEffect1f(Key(s + 0x40), scale(far));
        unsigned char* const c = Sprite_Current;
        const U q = scale(divisor(c, false));
        SetLongEffect1f(Key(c + 0x44), q);
        return q;
    }
    SetLongEffect1f(Key(s + 0x40), 0);
    SetLongEffect1f(Key(Sprite_Current + 0x44), 0);
    return Key(after);
}

// original 0x52CDF0 (PSX 0x801E0A0C): Sprite_UpdateScreenScaled on each of the 30 Sprite_Objects
// records whose +0 is not 0, each made Sprite_Current first; Sprite_Current is
// not put back.
extern "C" void __cdecl Sprite_UpdateAllScaled(void) {
    constexpr U kStride = 0xA4;
    const U kEnd = Key(rv::Bytes(Sprite_Objects)) + 30 * kStride;
    for (U record = Key(rv::Bytes(Sprite_Objects)); record < kEnd; record += kStride) {
        if (BEffect1f(record) == 0) continue;
        Sprite_Current = AtEffect1f(record);
        SH_CALL(Sprite_UpdateScreenScaled)();
    }
}

// original 0x52CE20 (PSX 0x801E0A88): bytes +1..+4 of Effect_Objects records 0..6 to 0, in that
// order record by record; Effect_ReleaseAt(7) .. (19); record 2 +1 = 2.
extern "C" void __cdecl Effect_ResetFirstSeven(void) {
    for (U record = 0; record <= 6; ++record) {
        unsigned char* const r = AtEffect1f(Key(rv::Bytes(Effect_Objects)) + record * 0x80);
        r[1] = 0;
        r[2] = 0;
        r[3] = 0;
        r[4] = 0;
    }
    for (unsigned i = 7; i < 0x14; ++i) SH_CALL(Effect_ReleaseAt)(static_cast<unsigned char>(i));
    Effect_Objects[1].state = 2;
}

// original 0x52CE60: the 36-byte record of kind & 0xFF from 0x66A6A0. The
// count's low word at or above the threshold byte +0xF: the points word +0x12,
// eax's high word the count's (the original loads the argument whole and moves
// the word into ax); below it (so the threshold is at least 1): (count16 * 10
// idiv threshold) * points, divided by 10 (signed; every value is small and
// not negative), in eax whole.
extern "C" unsigned long __cdecl FieldPanel_KindPoints(unsigned kind, unsigned count) {
    const U record = ::bof3::Img(Fish_Kinds_addr) + 0x10 + 36u * (kind & 0xFF);
    const unsigned threshold = BEffect1f(record + 0xF);
    const unsigned low = count & 0xFFFF;
    if (low >= threshold) return (count & 0xFFFF0000u) | WordEffect1f(record + 0x12);
    const SEffect1f share = static_cast<SEffect1f>(low * 10) / static_cast<SEffect1f>(threshold);
    const SEffect1f product = static_cast<SEffect1f>(static_cast<U>(share) * WordEffect1f(record + 0x12));
    return static_cast<U>(product / 10);
}

// original 0x52CED0: FieldPanel_KindPoints(i, count) summed over the 32 count
// bytes 0x9040EC that are not 0 (each byte read in its turn); ax the sum.
extern "C" unsigned short __cdecl FieldPanel_KindTotal(void) {
    U sum = 0;
    for (U i = 0; i < 0x20; ++i) {
        const unsigned char count = BEffect1f(::bof3::Img(effect_1f::at::kKindCounts.v) + i);
        if (count) sum += SH_CALL(FieldPanel_KindPoints)(i, count);
    }
    return static_cast<unsigned short>(sum);
}

// original 0x52CF00: game mode 8's frame (its step 1; step 0 calls it once).
extern "C" void __cdecl GameMode8_Frame(void) {
    SH_CALL(Field_MembersFrame)();
    SH_AT(void (__cdecl*)(), at::kMode8Panel.v)();
    SH_CALL(AreaMap_Frame)();
    SH_CALL(Party_UpdateScreens)();
    SH_CALL(Sprite_UpdateAllScaled)();
    SH_CALL(Effect_RunObjects)();
    SH_CALL(Field_DrawFrame)();
}

// original 0x52CF30 (PSX 0x801E0C78): game mode 8's step 3, a tail jump to 0x593950 (the item
// trade's dispatcher by the byte 0x93985C).
extern "C" void __cdecl GameMode8_TradeStep(void) { SH_AT(void (__cdecl*)(), at::kTradeDispatch.v)(); }

// original 0x52CF40 (PSX 0x801E0C98): the frame game mode 8's step 2 draws while it waits.
extern "C" void __cdecl GameMode8_WaitFrame(void) {
    SH_CALL(AreaMap_Frame)();
    SH_CALL(Party_UpdateScreens)();
    SH_CALL(Sprite_UpdateAllScaled)();
    SH_CALL(Effect_RunObjects)();
    SH_CALL(Field_DrawFrame)();
}

// original 0x52CF60: the record index & 0xFF of UiSprite_Modes, four dwords
// d0..dC, turned into a texture page as the original's shifts do (libgpu's
// getTPage(d0, d4, d8, dC) inlined: d8 shifted arithmetically); a draw mode at
// Gfx_PacketNext (read before the call) with dfe 0, dtd 0 and no window, then
// Gfx_CommitPrim(slot, 0xC). eax is left as the commit leaves it (no caller
// reads it).
extern "C" void __cdecl UiSprite_SetMode(unsigned index, unsigned slot) {
    const U record = Key(UiSprite_Modes) + 16u * (index & 0xFF);
    const U d0 = LongEffect1f(record), d4 = LongEffect1f(record + 4), d8 = LongEffect1f(record + 8), dC = LongEffect1f(record + 0xC);
    U high = ((d0 & 3) << 2) | (d4 & 3);
    high = ((high << 3) | (dC & 0x200)) << 2;
    const U low = static_cast<U>(static_cast<SEffect1f>((static_cast<U>(static_cast<SEffect1f>(d8) >> 2) & 0xF0) | (dC & 0x100)) >> 4);
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetDrawMode)(prim, 0, 0, high | low, 0);
    SH_CALL(Gfx_CommitPrim)(slot, 0xC);
}

// original 0x52CFE0: a sprite primitive at Gfx_PacketNext (read once, first):
// Gpu_SetSprt; the colour 0x80, 0x80, 0x80 at +4..+6; x and y (their low words,
// signed) as floats at +8 and +0xC; from the record sprite & 0xFF of
// UiSprite_Sheet the CLUT word +0x16 ((word +4 << 6) | (dword +0 sar 4) & 0x3F),
// the size words +0x18 / +0x1A, the u, v bytes +0x14 / +0x15; Gfx_CommitPrim(slot,
// 0x1C). Answers the primitive.
extern "C" unsigned char* __cdecl UiSprite_Draw(unsigned sprite, unsigned slot, int x, int y) {
    unsigned char* const prim = Gfx_PacketNext;
    SH_CALL(Gpu_SetSprt)(prim);
    prim[4] = 0x80;
    prim[5] = 0x80;
    prim[6] = 0x80;
    const float fx = static_cast<float>(static_cast<short>(x));
    const float fy = static_cast<float>(static_cast<short>(y));
    std::memcpy(prim + 8, &fx, 4);
    std::memcpy(prim + 0xC, &fy, 4);
    const U record = Key(UiSprite_Sheet) + 16u * (sprite & 0xFF);
    const U clut = (static_cast<U>(WordEffect1f(record + 4)) << 6) | (static_cast<U>(static_cast<SEffect1f>(LongEffect1f(record)) >> 4) & 0x3F);
    SetWord(Key(prim + 0x16), clut);
    SetWord(Key(prim + 0x18), WordEffect1f(record + 8));
    SetWord(Key(prim + 0x1A), WordEffect1f(record + 0xA));
    prim[0x14] = BEffect1f(record + 0xC);
    prim[0x15] = BEffect1f(record + 0xD);
    SH_CALL(Gfx_CommitPrim)(slot, 0x1C);
    return prim;
}

// original 0x52D080 (PSX 0x801E0F28): the draw mode (0, 1); panel sprites 0 at (x + 8, y) and
// 1 at (x + 0x108, y).
extern "C" void __cdecl FieldPanel_DrawHeader(int x, int y) {
    DrawMode(0, 1);
    DrawSprite(0, 1, x + 8, y);
    DrawSprite(1, 1, x + 0x108, y);
}

// original 0x52D0C0 (PSX 0x801E0FA4): the draw mode (1, 1), panel sprite 2 at (x, y); unless
// the kind is 0xFF, the primitive's CLUT word +0x16 = ((kind >> 4) + 0x1EB) <<
// 6 | (kind & 0xF), u +0x14 = (kind & 3) << 6, v +0x15 = (kind >> 2) * 0x28
// (a byte).
extern "C" void __cdecl FieldPanel_DrawKindIcon(int x, int y, unsigned kind) {
    DrawMode(1, 1);
    unsigned char* const p = DrawSprite(2, 1, x, y);
    const auto k = static_cast<unsigned char>(kind);
    if (k == 0xFF) return;
    move_script::SetWord(p + 0x16, ((static_cast<unsigned>(k >> 4) + 0x1EB) << 6) | (k & 0xFu));
    p[0x14] = static_cast<unsigned char>((k & 3u) << 6);
    p[0x15] = static_cast<unsigned char>((k >> 2) * 0x28u);
}

// original 0x52D140: a kind's row `row` - the draw mode (0, 1); sprites 3 at
// (0x68, 0x42 - row * 20), 4 at (0x20 - row * 25, 0x70), 5 at (0xE0 + row *
// 25, 0x70); the name into the text scratch 0x904BA0 (kind 0x16 its own
// 16 bytes, 0xFF eight '?' and a NUL, any other the 22-byte record's first 16)
// drawn at (0x70, 0x45 - row * 20); unless 0xFF, the count and the kind's
// points for it (0x52CE60) printed and drawn at (0x27 - row * 25, 0x73) and
// (0xE7 + row * 25, 0x73).
extern "C" void __cdecl FieldPanel_DrawKindRow(unsigned kind, unsigned count, unsigned row) {
    const auto k = static_cast<unsigned char>(kind);
    const int r = static_cast<unsigned char>(row);
    DrawMode(0, 1);
    DrawSprite(3, 1, 0x68, 0x42 - r * 20);
    DrawSprite(4, 1, 0x20 - r * 25, 0x70);
    DrawSprite(5, 1, 0xE0 + r * 25, 0x70);
    unsigned char* const name = move_script::At(::bof3::Img(scenario_harness::at::kTextBuffer));
    if (k == 0x16) {
        std::memcpy(name, move_script::At(::bof3::Img(AreaW0b_ItemNameKey_addr)), 16);
    } else if (k == 0xFF) {
        std::memset(name, '?', 8);
        name[8] = 0;
    } else {
        std::memcpy(name, move_script::At(::bof3::Img(at::kKindNames) + k * 22u), 16);
    }
    TextFieldE1(0x70, 0x45 - r * 20, 0, 0xF, KeyFieldE1(name));
    if (k == 0xFF) return;
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(name), reinterpret_cast<const char*>(move_script::At(::bof3::Img(at::kCountFormat))),
                         static_cast<unsigned>(static_cast<unsigned char>(count)));
    TextFieldE1(0x27 - r * 25, 0x73, 0, 3, KeyFieldE1(name));
    const unsigned points = KindPoints(kind, static_cast<unsigned char>(count)) & 0xFFFFu;
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(name), reinterpret_cast<const char*>(move_script::At(::bof3::Img(at::kCountFormat))), points);
    TextFieldE1(r * 25 + 0xE7, 0x73, 0, 3, KeyFieldE1(name));
}

// original 0x52D320: the draw mode (3, 1); sprites 6 at (x, y) and 7 at
// (x + 0x58, y); the total 0x52CED0 against the thirteen thresholds the
// original builds on its stack - the rank is the first it is below, written
// to 0x9045F4 - and the rank's record (four bytes, also built on the stack):
// its sprite at (x + 0x1D, y + 0x1D), and sprite 0x48 at (x + b1 + 0x1D, y +
// 0x21) when b2, again at (x + b1 + 0x24, y + 0x21) when b3; the total printed
// at (x + 0x3D, y + 9).
// As the original has it: the last threshold is 0xFFFF and the compare is
// "below", so a total of 0xFFFF walks on past the thresholds into the frame;
// ours aborts there.
extern "C" void __cdecl FieldPanel_DrawTotal(int x, int y) {
    static const std::uint16_t kThresholds[13] = {100, 300, 500, 1000, 1500, 2000, 3000, 4000, 5000, 7000, 9000, 9500, 0xFFFF};
    static const unsigned char kRanks[13][4] = {
        {8, 0x38, 0, 0},   {8, 0x38, 1, 0},   {8, 0x38, 1, 1},   {9, 0x3A, 0, 0}, {9, 0x3A, 1, 0},
        {9, 0x3A, 1, 1},   {0xA, 0x49, 0, 0}, {0xA, 0x49, 1, 0}, {0xA, 0x49, 1, 1}, {0xB, 0x4E, 0, 0},
        {0xB, 0x4E, 1, 0}, {0xB, 0x4E, 1, 1}, {0x49, 0x28, 0, 0},
    };
    DrawMode(3, 1);
    DrawSprite(6, 1, x, y);
    DrawSprite(7, 1, x + 0x58, y);
    const auto total = static_cast<std::uint16_t>(KindTotal());
    unsigned rank = 0;
    while (total >= kThresholds[rank]) {
        if (++rank == 13)
            bof3::Fatal("FieldPanel_DrawTotal: a total of 0x%X is at or above every threshold (the original walks its stack past them)",
                        (unsigned)total);
    }
    FieldE1_Rank = static_cast<unsigned char>(rank);
    const unsigned char* const r = kRanks[rank];
    DrawSprite(r[0], 1, x + 0x1D, y + 0x1D);
    if (r[2] != 0) {
        const int shifted = x + r[1];
        DrawSprite(0x48, 1, shifted + 0x1D, y + 0x21);
        if (r[3] != 0) DrawSprite(0x48, 1, shifted + 0x24, y + 0x21);
    }
    unsigned char* const text = move_script::At(::bof3::Img(scenario_harness::at::kTextBuffer));
    SH_CALL(Crt_sprintf)(reinterpret_cast<char*>(text), reinterpret_cast<const char*>(move_script::At(::bof3::Img(at::kTotalFormat))),
                         static_cast<unsigned>(total));
    TextFieldE1(x + 0x3D, y + 9, 0, 5, KeyFieldE1(text));
}

// original 0x52D560 (PSX 0x801E1684): the draw mode (3, 1), sprite 0xC at (x, y); unless the
// id's word is 0xFFFF, the script pool's message `id` at (x + 0xA, y + 8).
extern "C" void __cdecl FieldPanel_DrawMessage(int x, int y, unsigned id) {
    DrawMode(3, 1);
    DrawSprite(0xC, 1, x, y);
    const auto message = static_cast<std::uint16_t>(id);
    if (message == 0xFFFF) return;
    TextFieldE1(x + 0xA, y + 8, 0, 0xFF, PoolText(::bof3::Img(field_e1::at::kPools) + message * 2u));
}

// original 0x52D5C0 (PSX 0x801E1734): the draw mode (4, 2); a tile at the packet cursor - its
// colour 0x20 0x20 0x20, (0, 0), 320.0 x 240.0 (floats) -, half-transparent
// (mode 1), committed to slot 2 (0x1C bytes).
extern "C" void __cdecl FieldPanel_DrawShade(void) {
    DrawMode(4, 2);
    unsigned char* const p = Gfx_PacketNext;
    SH_CALL(Gpu_SetTile)(p);
    p[6] = 0x20;
    p[5] = 0x20;
    p[4] = 0x20;
    SetLong(p + 8, std::bit_cast<std::int32_t>(Widescreen_FillX()));   // DIV-0041: (-53, 0) 426 wide under the wide picture
    SetLong(p + 0xC, 0);
    SetLong(p + 0x14, std::bit_cast<std::int32_t>(Widescreen_FillWidth()));   // 0x43A00000, 320.0f narrow
    SetLong(p + 0x18, 0x43700000);
    SH_CALL(Gpu_SetSemiTrans)(p, 1);
    SH_CALL(Gfx_CommitPrim)(2, 0x1C);
}

// original 0x52D610: Menu_DrawBox(x, y + 1, 0x64, 0x38, 0x80, the style
// 0x903A5A); the frame (8 across, the sides 0x20 high, the bottom at + 0x38);
// the script pool's messages by the words 0x8035FA at (x + 0x28, y + 6),
// 0x8035FC at (x + 6, y + 0x19) and 0x8035FE at (x + 6, y + 0x29).
extern "C" void __cdecl FieldPanel_DrawBox3(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y + 1, 0x64, 0x38, 0x80, static_cast<unsigned char>(Config_WindowColour));
    Frame(x, y, 8, 0x48, 0x60, 0x20, 0x38, 0xB);
    TextFieldE1(x + 0x28, y + 6, 0, 0xFF, PoolText(::bof3::Img(field_e1::at::kPools) + 0x7A));
    TextFieldE1(x + 6, y + 0x19, 0, 0xFF, PoolText(::bof3::Img(field_e1::at::kPools) + 0x7C));
    TextFieldE1(x + 6, y + 0x29, 0, 0xFF, PoolText(::bof3::Img(field_e1::at::kPools) + 0x7E));
}

// original 0x52D750: Menu_DrawBox(x, y + 1, 0xB3, 0x42, 0x80, the style); the
// frame (15 across, the sides 0x28 high, the bottom at + 0x40); the messages
// by 0x8035F8 at (x + 0x32, y + 6) and 0x803602 at (x + 6, y + 0x19).
extern "C" void __cdecl FieldPanel_DrawBox2(int x, int y) {
    SH_CALL(Menu_DrawBox)(x, y + 1, 0xB3, 0x42, 0x80, static_cast<unsigned char>(Config_WindowColour));
    Frame(x, y, 0xF, 0x98, 0xB0, 0x28, 0x40, 0x15);
    TextFieldE1(x + 0x32, y + 6, 0, 0xFF, PoolText(::bof3::Img(field_e1::at::kPools) + 0x78));
    TextFieldE1(x + 6, y + 0x19, 0, 0xFF, PoolText(::bof3::Img(field_e1::at::kPools) + 0x82));
}

// original 0x52D880: for each id 0x38..0x4D, the slots of the consumables'
// list (Inventory_IdLists[0], 128) holding it with the count 99 counted; al 1
// when there are 22 or more (a duplicate slot counts twice).
extern "C" unsigned char __cdecl Inventory_Holds38To4DAt99(void) {
    int n = 0;
    for (unsigned id = 0x38; id < 0x4E; ++id)
        for (unsigned i = 0; i < 0x80; ++i)
            if (BFieldE1(::bof3::Img(at::kItemIds) + i) == id && BFieldE1(::bof3::Img(at::kItemCounts) + i) == 0x63) ++n;
    return n >= 0x16 ? 1 : 0;
}

// original 0x52D8C0: while 0x939A28 and Frame_Counter bit 3, the draw mode
// (3, 1) and sprite 0x47 at (0x1A, 0x5C).
extern "C" void __cdecl FieldPanel_DrawBlink(void) {
    if (Effect1e_Blink == 0 || (Frame_Counter & 8) == 0) return;
    DrawMode(3, 1);
    DrawSprite(0x47, 1, 0x1A, 0x5C);
}

void LeaderPanel_Inject() {
    BOF3_INJECT(LeaderPanel_Run);
    BOF3_INJECT(LeaderPanel_S0);
    BOF3_INJECT(LeaderPanel_S0Begin);
    BOF3_INJECT(LeaderPanel_S0Wait);
    BOF3_INJECT(LeaderPanel_S0End);
    BOF3_INJECT(LeaderPanel_S1);
    BOF3_INJECT(LeaderPanel_S1Begin);
    BOF3_INJECT(LeaderPanel_S1Wait);
    BOF3_INJECT(LeaderPanel_S1Buttons);
    BOF3_INJECT(LeaderPanel_S1Idle);
    BOF3_INJECT(LeaderPanel_S1Box3In);
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
    BOF3_INJECT(ChoiceMenu_Run);
    BOF3_INJECT(ExtraSlots_Step);
    BOF3_INJECT(ExtraSlots_Enter);
    BOF3_INJECT(ExtraSlots_PickSlot);
    BOF3_INJECT(ExtraSlots_PickItem);
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
    BOF3_INJECT(Sprite_UpdateScreenScaled);
    BOF3_INJECT(Sprite_UpdateAllScaled);
    BOF3_INJECT(Effect_ResetFirstSeven);
    BOF3_INJECT(FieldPanel_KindPoints);
    BOF3_INJECT(FieldPanel_KindTotal);
    BOF3_INJECT(GameMode8_Frame);
    BOF3_INJECT(GameMode8_TradeStep);
    BOF3_INJECT(GameMode8_WaitFrame);
    BOF3_INJECT(UiSprite_SetMode);
    BOF3_INJECT(UiSprite_Draw);
    BOF3_INJECT(FieldPanel_DrawHeader);
    BOF3_INJECT(FieldPanel_DrawKindIcon);
    BOF3_INJECT(FieldPanel_DrawKindRow);
    BOF3_INJECT(FieldPanel_DrawTotal);
    BOF3_INJECT(FieldPanel_DrawMessage);
    BOF3_INJECT(FieldPanel_DrawShade);
    BOF3_INJECT(FieldPanel_DrawBox3);
    BOF3_INJECT(FieldPanel_DrawBox2);
    BOF3_INJECT(Inventory_Holds38To4DAt99);
    BOF3_INJECT(FieldPanel_DrawBlink);
}

void LeaderPanelFromEffect1e_Inject() {
    // DIVERGENCE DIV-0027 (amended 2026-10-03): after the self-test, which
    // compares the original's hand; a Latin overlay only (DIV-0056).
    if (Lang_Latin()) {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("ShopAskRow", Low32(&g_shop_ask_row), &was,
                         &is, 1);
        bof3::Log("DIV-0027    the field panel's message 0x44: the hand on the row of its first answer");
    }
    // DIVERGENCE DIV-0132: after the self-test, which compares Capcom's fades.
    {
        static const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("FishPanelDuck", Low32(&g_fish_panel_duck),
                         &was, &is, 1);
        bof3::Log("DIV-0132    the fishing panel ducks the music as the PlayStation does (on unless the line above says OFF)");
    }
}
