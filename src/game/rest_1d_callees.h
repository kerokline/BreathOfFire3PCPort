// Group R1D's raw addresses (round fourteen, wave one; docs/rest_1d.md): the
// .data state tables its 27 dispatchers jump through, each named as a [[data]]
// entry in symbols.toml with the count its own reader reaches (section 3 of the
// doc), and the cells the group's code names by address. Every function the
// group calls is ours already (R0A's helpers, the field engine's), so no
// callee is listed by address: the tables' entries are other groups' and the
// group's own handlers, reached by what the table holds.
#pragma once

#include <cstdint>

namespace rest_1d {
namespace at {

// The state tables, in address order. "by +2" / "by +3": Sprite_Current's
// state byte the reader indexes with; "by +0x2C": its u16 form word.
// kAction9Form1State1Steps: now PartyAction9_Form1State1Steps (symbols.toml). 3, by +3   (PartyAction9_Form1State1)
// kFormAction9Form2States: now PartyFormAction9_Form2States (symbols.toml). 3, by +2   (PartyFormAction9_Form2)
// kAction9Form2States: now PartyAction9_Form2States (symbols.toml). 3, by +2   (PartyAction9_Form2)
// kFormAction9Forms: now PartyFormAction9_Forms (symbols.toml). 3, by +0x2C (PartyFormAction9_ByForm)
// kAction9Forms: now PartyAction9_Forms (symbols.toml). 3, by +0x2C (PartyAction9_ByForm)
// kFormAction10Form0States: now PartyFormAction10_Form0States (symbols.toml). 3, by +2   (PartyFormAction10_Form0)
// kAction10Form0States: now PartyAction10_Form0States (symbols.toml). 2, by +2   (PartyAction10_Form0)
// kFormAction10Form1States: now PartyFormAction10_Form1States (symbols.toml). 3, by +2   (PartyFormAction10_Form1)
// kAction10Form1States: now PartyAction10_Form1States (symbols.toml). 3, by +2   (PartyAction10_Form1)
// kFormAction10Form2States: now PartyFormAction10_Form2States (symbols.toml). 3, by +2   (PartyFormAction10_Form2)
// kAction10Form2States: now PartyAction10_Form2States (symbols.toml). 2, by +2   (PartyAction10_Form2)
// kFormAction10Forms: now PartyFormAction10_Forms (symbols.toml). 3, by +0x2C (PartyFormAction10_ByForm)
// kAction10Forms: now PartyAction10_Forms (symbols.toml). 3, by +0x2C (PartyAction10_ByForm)
// kFormAction11Form0States: now PartyFormAction11_Form0States (symbols.toml). 3, by +2   (PartyFormAction11_Form0)
// kAction11Form0States: now PartyAction11_Form0States (symbols.toml). 2, by +2   (PartyAction11_Form0)
// kFormAction11Form1States: now PartyFormAction11_Form1States (symbols.toml). 3, by +2   (PartyFormAction11_Form1)
// kAction11Form1States: now PartyAction11_Form1States (symbols.toml). 3, by +2   (PartyAction11_Form1)
// kFormAction11Forms: now PartyFormAction11_Forms (symbols.toml). 3, by +0x2C (PartyFormAction11_ByForm)
// kAction11Forms: now PartyAction11_Forms (symbols.toml). 3, by +0x2C (PartyAction11_ByForm)
// kFormAction12Form0States: now PartyFormAction12_Form0States (symbols.toml). 3, by +2   (PartyFormAction12_Form0)
// kAction12Form0States: now PartyAction12_Form0States (symbols.toml). 2, by +2   (PartyAction12_Form0)
// kAction12Form0State0Steps: now PartyAction12_Form0State0Steps (symbols.toml). 5, by +3   (PartyAction12_Form0State0)
// kAction12Form0State1Steps: now PartyAction12_Form0State1Steps (symbols.toml). 3, by +3   (PartyAction12_Form0State1)
// kFormAction12Form1States: now PartyFormAction12_Form1States (symbols.toml). 3, by +2   (PartyFormAction12_Form1)
// kFormAction12Forms: now PartyFormAction12_Forms (symbols.toml). 3, by +0x2C (PartyFormAction12_ByForm)
// kAction12Forms: now PartyAction12_Forms (symbols.toml). 3, by +0x2C (PartyAction12_ByForm)
// kFormAction13Form0States: now PartyFormAction13_Form0States (symbols.toml). 3, by +2   (PartyFormAction13_Form0)

// Cells the group's code names by address.
// kSteps: now Field_DirectionSteps (symbols.toml). Field_DirectionSteps: 8 rows of two longs (x, z)
// kSpriteFlag: now Sprite_Objects[0].context (symbols.toml). Sprite_Objects + 0x80, by 0xA4 a record
// kExtraFlag: now Sprite_ObjectsExtra[0].context (symbols.toml). Sprite_ObjectsExtra + 0x80, by 0xA4 a record
constexpr std::uint32_t kScriptFlagsHi = 0x9039A3;  // Field_ScriptFlags' high byte (bit 0x10: the word's 0x1000)

}  // namespace at
}  // namespace rest_1d
