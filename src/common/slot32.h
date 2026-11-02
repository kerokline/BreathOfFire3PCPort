// Slot32<T>: a 4-byte slot of the image that holds a pointer
// (docs/x64-build-design.md 9, the 64-bit build round's stage B3).
//
// 111 [[data]] rows of symbols.toml have a pointer ctype (`Snd_Device`,
// `Sprite_Current`, `Gfx_PacketNext`, the 14 import slots, the handler
// tables, ...). Each is a 4-byte word of BOF3.exe's data holding a 32-bit
// address: the image is the narrow one (the owner's decision 1 of
// docs/x64-build-round.md 7), so the word stays 4 bytes on every width and
// every address it holds lies below 4 GB. The generated header (tools/
// gen_symbols.py) spells such a row's GameData member and its accessor as
// Slot32<T>, where T is what the pointer points at: on the 32-bit build a
// trivially-copyable 4-byte struct holding the pointer's bits, which the
// compiler passes, returns and compares as the pointer itself
// (tools/codegen_same.py: `0 differ`); on x86-64 the 32-bit slot the design
// needs, so sizeof(GameData) and every member's address hold.
//
// A use of the row reads as the pointer: `Sprite_Current[9]`,
// `Gfx_PacketNext + size`, `*Field_State`, `BattleE3_CurrentEnemy->hp`,
// `Snd_Device != nullptr`, `if (Music_File)`, a call that takes the
// pointer - all through the conversion to the pointer (on the 32-bit build a
// reference to the pointer in the image; on x86-64 the zero-extension of
// the word) and the few members below. A store `Sprite_Current = p`
// narrows: the plain cast on the 32-bit build; on x86-64 Low32()'s check
// (common/image_base.h) that the address lies below 4 GB, a Fatal naming the
// storing line otherwise, since a native address that does not fit is a
// defect of the loader's placement (design 8.1), not a value to truncate
// silently.
//
// What the conversion cannot keep: a reinterpret_cast of the row itself
// (`reinterpret_cast<std::uintptr_t>(Sprite_Current)`), which the language
// refuses on a class type, spells the pointer first - `Sprite_Current.Native()`
// - and a function that wants the slot's address as `T**` (a COM out
// parameter) takes `Slot32<T>*`, which is the 4-byte slot, never `T**`.
//
// Header-only and free of Windows: the generated symbol header includes
// it, and so does the launcher through that one.
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "common/image_base.h"  // Low32: the one checked narrowing of a native address

// Every member is inlined before the optimiser sees the function (the
// always-inliner runs first), so a use compiles from the same expression the
// pointer macro made: left to the ordinary inliner, three objects' `&&`
// chains came out in another order (tools/codegen_same.py, 2026-11-01).
#define BOF3_SLOT_INLINE [[gnu::always_inline]] inline

namespace bof3 {

template <class T>
struct Slot32 {
    // The word in the image: a 32-bit address, or 0. Spelled as the pointer
    // itself where a pointer is 32 bits wide, so a read is the pointer load
    // the macro made before (an integer load converted to a pointer compiles
    // to the same bytes but not to the same instruction order:
    // tools/codegen_same.py saw 82 objects with their compares' operands
    // swapped), and as the 32-bit integer where a pointer is wider.
#if UINTPTR_MAX > 0xFFFFFFFFu
    std::uint32_t v;
    // The native pointer the slot names: the word, zero-extended.
    BOF3_SLOT_INLINE T* Native() const { return reinterpret_cast<T*>(static_cast<std::uintptr_t>(v)); }
#else
    T* v;
    BOF3_SLOT_INLINE T* Native() const { return v; }
#endif
    // The conversion a use reads through. On the 32-bit build it is a
    // reference to the pointer in the image, not a copy: a copy compiled to
    // the same instructions, but left three objects' `&&` chains in another
    // shape (an `and i1` as a `select`; tools/codegen_same.py, 2026-11-01).
#if UINTPTR_MAX > 0xFFFFFFFFu
    BOF3_SLOT_INLINE operator T*() const { return Native(); }
#else
    BOF3_SLOT_INLINE operator T* const&() const { return v; }
#endif
    // static_cast<X*>(slot), as static_cast<X*>(p) reads on the pointer (a
    // void slot as the COM interface it holds, a Game_Hwnd as HWND): explicit,
    // so only a cast spells it, never an implicit conversion.
    template <class U>
    BOF3_SLOT_INLINE explicit operator U*() const { return static_cast<U*>(Native()); }
    BOF3_SLOT_INLINE T* operator->() const { return Native(); }

    // A store narrows the native address to the word. 32-bit: the pointer's
    // bits. x86-64: the low 32 bits, after a check that the high ones are 0.
    BOF3_SLOT_INLINE Slot32& operator=(T* p) {
#if UINTPTR_MAX > 0xFFFFFFFFu
        v = Low32(p);
#else
        v = p;
#endif
        return *this;
    }
    BOF3_SLOT_INLINE Slot32& operator=(std::nullptr_t) {
        v = {};
        return *this;
    }

    // Pointer arithmetic on the slot itself (`Gfx_PacketNext += size`): the
    // built-in compound operators want an lvalue of pointer type, which the
    // slot is not; the result is narrowed as a store is. For a sized T only.
    BOF3_SLOT_INLINE Slot32& operator+=(std::ptrdiff_t n) { return *this = Native() + n; }
    BOF3_SLOT_INLINE Slot32& operator-=(std::ptrdiff_t n) { return *this = Native() - n; }
    BOF3_SLOT_INLINE Slot32& operator++() { return *this += 1; }
    BOF3_SLOT_INLINE Slot32& operator--() { return *this -= 1; }
    BOF3_SLOT_INLINE T* operator++(int) {
        T* const p = Native();
        *this += 1;
        return p;
    }
    BOF3_SLOT_INLINE T* operator--(int) {
        T* const p = Native();
        *this -= 1;
        return p;
    }

    // No operator bool: `if (slot)`, `slot && x`, `!slot` read through the
    // pointer conversion and the pointer's own test, as the macro's pointer
    // did (an operator bool of its own put an inlined compare where the
    // pointer's test was, and SnapSave's `&&` chain came out reshaped).
};

static_assert(sizeof(Slot32<void>) == 4 && alignof(Slot32<void>) == 4, "Slot32 is the 4-byte word of the image");

// A code word of the image indexed as a function pointer: a table the game
// dispatches through (`g.run_table[state]()`, `Table(va)[step]()`) is 4-byte
// words on every width, so a `const Handler*` over it would stride 8 bytes
// on x86-64 (stage C's first 64-bit attract, 2026-11-01). CodeSlot<Handler>
// is the 4-byte word with Handler's conversion: `table[i]()` calls through
// the conversion (a surrogate call), `Handler h = table[i]` reads it.
template <class Fn>
using CodeSlot = Slot32<std::remove_pointer_t<Fn>>;

}  // namespace bof3

#undef BOF3_SLOT_INLINE
