// Round thirteen group E1F: the cut's fifteen rows at 0x52A6C0..0x52D07C
// (analysis/round13_cut.tsv), each read with capstone to its last instruction
// (docs/effect_1f.md section 1). What they are, by the code:
//
//   - a field menu whose state lives in Effect_Objects record 6 (0x7E14E0):
//     the dispatch on its three-way choice (ChoiceMenu_Run), the extra-slot
//     sub-menu's step dispatch and its three steps (ExtraSlots_*);
//   - a sprite pass game mode 8 runs every frame (Sprite_UpdateAllScaled and
//     the per-record Sprite_UpdateScreenScaled, which E1A's states call too);
//   - the effect records' reset E1E's menu calls (Effect_ResetFirstSeven);
//   - the kind points FE1's panel prints (FieldPanel_KindPoints / _KindTotal);
//   - game mode 8's three steps (GameMode8_*): not effect code at all;
//   - the two primitive helpers 130 effect and panel sites call
//     (UiSprite_SetMode, UiSprite_Draw).
//
// Every call out goes through the scenario harness (SH_CALL / SH_AT), so the
// start-up fuzz can stand recorders in for ours as for the originals' copies.
// No divergence: each is a faithful replacement. Where the original jumps
// through a state table unchecked, ours aborts past the table; where it divides
// by a value that can be 0 (Sprite_UpdateScreenScaled), ours aborts there. The
// unchecked reads of the image's data tables (UiSprite_Modes, UiSprite_Sheet,
// the kinds' records, NameTable_Accessories, the id list by an s16) read the
// same bytes in place as the original - no fault on either side - and are
// described as latent defects (docs/effect_1f.md section 6), not aborted.
#include "game/effect_1f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/effect_1f_callees.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using U = std::uint32_t;
using S = std::int32_t;
using Handler = void (__cdecl*)();
namespace at = effect_1f::at;

unsigned char* At(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }
U Key(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }
unsigned char& B(U a) { return *At(a); }
std::uint16_t Word(U a) {
    std::uint16_t v;
    std::memcpy(&v, At(a), 2);
    return v;
}
void SetWord(U a, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(At(a), &w, 2);
}
U Long(U a) {
    U v;
    std::memcpy(&v, At(a), 4);
    return v;
}
void SetLong(U a, U v) { std::memcpy(At(a), &v, 4); }

// Effect record 6's cells, the menu's state.
unsigned char& R6(unsigned offset) { return B(at::kRecord6 + offset); }
U R6Address(unsigned offset) { return at::kRecord6 + offset; }

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

void Sound(unsigned id) { SH_CALL(Sound_PlayEffect)(static_cast<unsigned short>(id)); }
void Hand(int x, int y) { SH_CALL(Menu_DrawHand)(x, y, 0); }
// Text_DrawAt(0x1D, 0x14, 0, 0xFF, MessagePools + the pool's word at `cell`):
// the original adds the word, zero-extended (movzx or `and 0xFFFF`), to 0x803580.
void Label(U cell) {
    SH_CALL(Text_DrawAt)(0x1D, 0x14, 0, 0xFF, At(bof3::addr::MessagePools + Word(cell)));
}
// The id at the s16 index of the category-3 list, and its record's kind byte
// (24-byte NameTable_Accessories records, +0x12): both read in place, unchecked
// as in the original.
unsigned char IdAt(U index_cell) {
    const S index = static_cast<short>(Word(index_cell));
    return B(at::kAccessoryIds + static_cast<U>(index));
}
unsigned char KindOf(unsigned char id) { return B(at::kAccessoryKind + 24u * id); }

// The shared exit of ExtraSlots_PickItem's cancel and confirm: record 6 +7 bit
// 1 cleared, Sprite_Current +4 down (the original's 0x52A922, after the sound).
void PickItemBack() {
    const unsigned char b = static_cast<unsigned char>(R6(7) & 0xFD);
    unsigned char* const s = Sprite_Current;
    R6(7) = b;
    s[4] = static_cast<unsigned char>(s[4] - 1);
}

}  // namespace

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
        Hand(0x1C, static_cast<int>((static_cast<U>(b) << 4) + 0x58));
        unsigned char* const s = Sprite_Current;
        s[4] = static_cast<unsigned char>(s[4] + 1);
    }
    Label((R6(7) & 1) ? at::kLabelB : at::kLabelA);
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
    Label((R6(7) & 1) ? at::kLabelB : at::kLabelA);
    const unsigned char b = R6(7);
    if (!(b & 0x80)) {
        const unsigned char held = (b & 1) ? B(at::kExtraItem) : B(at::kExtraAccessory);
        SetWord(R6Address(0x2C), held ? held + 4u : 0xFFFFu);
    }
    Hand(0x1C, static_cast<int>(((b & 1u) << 4) + 0x58));
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
        Label(at::kListLabelA);
        if (Input_Pressed & Field_ConfirmButtons) {
            const unsigned char id = IdAt(R6Address(0x36));
            if (KindOf(id) == 0xB) {
                SH_CALL(Inventory_Remove)(3, id, 1);
                const unsigned char old = B(at::kExtraAccessory);
                if (old) SH_CALL(Inventory_Add)(3, old, 1);
                B(at::kExtraAccessory) = id;
            }
            Sound(0x103);
            PickItemBack();
            return;
        }
        if (repeat & 0x1000u) {
            if (Long(R6Address(0xC)) != 0) {
                Sound(0x100);
                SetLong(R6Address(0xC), Long(R6Address(0xC)) - 1);
            }
        } else if (repeat & 0x4000u) {
            const U last = Long(R6Address(0x18)) - 1;
            if (static_cast<S>(Long(R6Address(0xC))) < static_cast<S>(last)) {
                Sound(0x100);
                SetLong(R6Address(0xC), Long(R6Address(0xC)) + 1);
            }
        }
        const unsigned char id = IdAt(R6Address(0x36));
        SetWord(R6Address(0x2C), KindOf(id) == 0xB ? id + 4u : 0xFFFFu);
        Hand(0xA8, static_cast<int>(Long(R6Address(0xC)) * 13u + 0x5A));
        return;
    }
    Label(at::kListLabelB);
    if (R6(0xA) == 0) {
        const unsigned short pressed = Input_Pressed;
        if (Field_ConfirmButtons & pressed) {
            const unsigned char id = IdAt(R6Address(0x3A));
            if (KindOf(id) == 0xA && B(at::kExtraItem) != id) {
                SH_CALL(Inventory_Remove)(3, id, 1);
                const unsigned char old = B(at::kExtraItem);
                if (old) SH_CALL(Inventory_Add)(3, old, 1);
                B(at::kExtraItem) = id;
                B(at::kExtraCount) = 1;
                S count = 0;
                for (U i = 0; i < 0x80; ++i)
                    if (KindOf(B(at::kAccessoryIds + i)) == 0xA) ++count;
                if (static_cast<S>(Long(R6Address(0x1C))) >= count && Word(R6Address(0x38)) != 0) {
                    const S scroll = static_cast<S>(Long(R6Address(0x38)) & 0xFFFF);
                    if (scroll > count - 9) SetWord(R6Address(0x38), Word(R6Address(0x38)) - 1u);
                }
            }
            Sound(0x103);
            PickItemBack();
            return;
        }
        if (repeat & 0x1000u) {
            if (Long(R6Address(0x10)) == 0) {
                if (Word(R6Address(0x38)) != 0) {
                    Sound(0x100);
                    SetWord(R6Address(0x38), Word(R6Address(0x38)) - 1u);
                    R6(8) = 4;
                    R6(0xA) = 3;
                }
            } else {
                Sound(0x100);
                SetLong(R6Address(0x10), Long(R6Address(0x10)) - 1);
            }
        } else if (repeat & 0x4000u) {
            const U cursor = Long(R6Address(0x10));
            if (cursor == 8) {
                const S bottom = static_cast<S>((Long(R6Address(0x38)) & 0xFFFF) + cursor);
                if (static_cast<S>(Long(R6Address(0x1C))) > bottom) {
                    Sound(0x100);
                    SetWord(R6Address(0x38), Word(R6Address(0x38)) + 1u);
                    R6(8) = 0;
                    R6(0xA) = 3;
                }
            } else if (static_cast<S>(cursor) < static_cast<S>(Long(R6Address(0x1C)))) {
                Sound(0x100);
                SetLong(R6Address(0x10), Long(R6Address(0x10)) + 1);
            }
        } else if (pressed & 4) {
            const U old = Long(R6Address(0x38));
            const auto scrolled = static_cast<std::uint16_t>(old - 9u);
            SetWord(R6Address(0x38), scrolled);
            if (static_cast<short>(scrolled) >= 0) {
                Sound(0x100);
            } else {
                SetWord(R6Address(0x38), 0);
                if (static_cast<std::uint16_t>(old) != 0) {
                    Sound(0x100);
                } else if (Long(R6Address(0x10)) != 0) {
                    Sound(0x100);
                    SetLong(R6Address(0x10), 0);
                }
            }
        } else if (pressed & 8) {
            const U count = Long(R6Address(0x1C));
            if (static_cast<S>(count) <= 8) {
                if (static_cast<S>(Long(R6Address(0x10))) < static_cast<S>(count)) {
                    Sound(0x100);
                    SetLong(R6Address(0x10), Long(R6Address(0x1C)));
                }
            } else {
                const U old = Long(R6Address(0x38));
                SetWord(R6Address(0x38), Word(R6Address(0x38)) + 9u);
                const S bottom = static_cast<S>((Long(R6Address(0x38)) & 0xFFFF) + 8);
                if (static_cast<S>(count) > bottom) {
                    Sound(0x100);
                } else {
                    const auto last = static_cast<std::uint16_t>(count - 8u);
                    SetWord(R6Address(0x38), last);
                    if (last != static_cast<std::uint16_t>(old)) {
                        Sound(0x100);
                    } else if (static_cast<S>(Long(R6Address(0x10))) < 8) {
                        Sound(0x100);
                        SetLong(R6Address(0x10), 8);
                    }
                }
            }
        }
    }
    const unsigned char id = IdAt(R6Address(0x3A));
    SetWord(R6Address(0x2C), KindOf(id) == 0xA ? id + 4u : 0xFFFFu);
    Hand(0xA8, static_cast<int>(Long(R6Address(0x10)) * 13u + 0x5A));
}

// original 0x52CD50: Sprite_UpdateScreen with +0x3C held at 0 and put back on
// the record current after the call; then, on the record current then, the
// scale 0x4650000 / d with the low byte cleared into +0x40 and (every cell read
// again through Sprite_Current) +0x44: d = +0x60 when above 0, +0x60 - 2 * s16
// +0x3E when below; at 0 both are 0 and eax is Sprite_Current. A d of 0 below
// is the original's divide fault: ours aborts.
extern "C" unsigned long __cdecl Sprite_UpdateScreenScaled(void) {
    unsigned char* s = Sprite_Current;
    const U held = Long(Key(s + 0x3C));
    SetLong(Key(s + 0x3C), 0);
    SH_CALL(Sprite_UpdateScreen)();
    unsigned char* const after = Sprite_Current;
    SetLong(Key(after + 0x3C), held);
    s = Sprite_Current;
    const S far = static_cast<S>(Long(Key(s + 0x60)));
    auto divisor = [](const unsigned char* r, bool below) -> S {
        const U d = Long(Key(r + 0x60));
        if (!below) return static_cast<S>(d);
        const U lift = static_cast<U>(static_cast<S>(static_cast<short>(Word(Key(r + 0x3E))))) << 1;
        return static_cast<S>(d - lift);
    };
    auto scale = [](S d) -> U {
        if (d == 0)
            bof3::Fatal("Sprite_UpdateScreenScaled (0x52CD50): +0x60 is twice s16 +0x3E below 0 - the original divides "
                        "0x4650000 by 0 there (a divide fault; docs/effect_1f.md section 6)");
        return static_cast<U>(0x4650000 / d) & 0xFFFFFF00u;
    };
    if (far < 0) {
        SetLong(Key(s + 0x40), scale(divisor(s, true)));
        unsigned char* const c = Sprite_Current;
        const U q = scale(divisor(c, true));
        SetLong(Key(c + 0x44), q);
        return q;
    }
    if (far > 0) {
        SetLong(Key(s + 0x40), scale(far));
        unsigned char* const c = Sprite_Current;
        const U q = scale(divisor(c, false));
        SetLong(Key(c + 0x44), q);
        return q;
    }
    SetLong(Key(s + 0x40), 0);
    SetLong(Key(Sprite_Current + 0x44), 0);
    return Key(after);
}

// original 0x52CDF0: Sprite_UpdateScreenScaled on each of the 30 Sprite_Objects
// records whose +0 is not 0, each made Sprite_Current first; Sprite_Current is
// not put back.
extern "C" void __cdecl Sprite_UpdateAllScaled(void) {
    constexpr U kStride = 0xA4;
    const U kEnd = Key(Sprite_Objects) + 30 * kStride;
    for (U record = Key(Sprite_Objects); record < kEnd; record += kStride) {
        if (B(record) == 0) continue;
        Sprite_Current = At(record);
        SH_CALL(Sprite_UpdateScreenScaled)();
    }
}

// original 0x52CE20: bytes +1..+4 of Effect_Objects records 0..6 to 0, in that
// order record by record; Effect_ReleaseAt(7) .. (19); record 2 +1 = 2.
extern "C" void __cdecl Effect_ResetFirstSeven(void) {
    for (U record = 0; record <= 6; ++record) {
        unsigned char* const r = At(Key(Effect_Objects) + record * 0x80);
        r[1] = 0;
        r[2] = 0;
        r[3] = 0;
        r[4] = 0;
    }
    for (unsigned i = 7; i < 0x14; ++i) SH_CALL(Effect_ReleaseAt)(static_cast<unsigned char>(i));
    B(at::kRecord2State) = 2;
}

// original 0x52CE60: the 36-byte record of kind & 0xFF from 0x66A6A0. The
// count's low word at or above the threshold byte +0xF: the points word +0x12,
// eax's high word the count's (the original loads the argument whole and moves
// the word into ax); below it (so the threshold is at least 1): (count16 * 10
// idiv threshold) * points, divided by 10 (signed; every value is small and
// not negative), in eax whole.
extern "C" unsigned long __cdecl FieldPanel_KindPoints(unsigned kind, unsigned count) {
    const U record = at::kKindRecords + 36u * (kind & 0xFF);
    const unsigned threshold = B(record + 0xF);
    const unsigned low = count & 0xFFFF;
    if (low >= threshold) return (count & 0xFFFF0000u) | Word(record + 0x12);
    const S share = static_cast<S>(low * 10) / static_cast<S>(threshold);
    const S product = static_cast<S>(static_cast<U>(share) * Word(record + 0x12));
    return static_cast<U>(product / 10);
}

// original 0x52CED0: FieldPanel_KindPoints(i, count) summed over the 32 count
// bytes 0x9040EC that are not 0 (each byte read in its turn); ax the sum.
extern "C" unsigned short __cdecl FieldPanel_KindTotal(void) {
    U sum = 0;
    for (U i = 0; i < 0x20; ++i) {
        const unsigned char count = B(at::kKindCounts + i);
        if (count) sum += SH_CALL(FieldPanel_KindPoints)(i, count);
    }
    return static_cast<unsigned short>(sum);
}

// original 0x52CF00: game mode 8's frame (its step 1; step 0 calls it once).
extern "C" void __cdecl GameMode8_Frame(void) {
    SH_CALL(Field_MembersFrame)();
    SH_AT(void (__cdecl*)(), at::kMode8Panel)();
    SH_CALL(AreaMap_Frame)();
    SH_CALL(Party_UpdateScreens)();
    SH_CALL(Sprite_UpdateAllScaled)();
    SH_CALL(Effect_RunObjects)();
    SH_CALL(Field_DrawFrame)();
}

// original 0x52CF30: game mode 8's step 3, a tail jump to 0x593950 (the item
// trade's dispatcher by the byte 0x93985C).
extern "C" void __cdecl GameMode8_TradeStep(void) { SH_AT(void (__cdecl*)(), at::kTradeDispatch)(); }

// original 0x52CF40: the frame game mode 8's step 2 draws while it waits.
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
    const U d0 = Long(record), d4 = Long(record + 4), d8 = Long(record + 8), dC = Long(record + 0xC);
    U high = ((d0 & 3) << 2) | (d4 & 3);
    high = ((high << 3) | (dC & 0x200)) << 2;
    const U low = static_cast<U>(static_cast<S>((static_cast<U>(static_cast<S>(d8) >> 2) & 0xF0) | (dC & 0x100)) >> 4);
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
    const U clut = (static_cast<U>(Word(record + 4)) << 6) | (static_cast<U>(static_cast<S>(Long(record)) >> 4) & 0x3F);
    SetWord(Key(prim + 0x16), clut);
    SetWord(Key(prim + 0x18), Word(record + 8));
    SetWord(Key(prim + 0x1A), Word(record + 0xA));
    prim[0x14] = B(record + 0xC);
    prim[0x15] = B(record + 0xD);
    SH_CALL(Gfx_CommitPrim)(slot, 0x1C);
    return prim;
}

void Effect1F_Inject() {
    if (bof3::WantsShadow("effect_1f")) effect_1f::SelfTest();
    BOF3_INJECT(ChoiceMenu_Run);
    BOF3_INJECT(ExtraSlots_Step);
    BOF3_INJECT(ExtraSlots_Enter);
    BOF3_INJECT(ExtraSlots_PickSlot);
    BOF3_INJECT(ExtraSlots_PickItem);
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
}
