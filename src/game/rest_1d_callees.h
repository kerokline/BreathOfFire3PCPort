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
constexpr std::uint32_t kAction9Form1State1Steps = 0x65FD88;   // 3, by +3   (PartyAction9_Form1State1)
constexpr std::uint32_t kFormAction9Form2States = 0x65FD94;    // 3, by +2   (PartyFormAction9_Form2)
constexpr std::uint32_t kAction9Form2States = 0x65FDA0;        // 3, by +2   (PartyAction9_Form2)
constexpr std::uint32_t kFormAction9Forms = 0x65FDAC;          // 3, by +0x2C (PartyFormAction9_ByForm)
constexpr std::uint32_t kAction9Forms = 0x65FDB8;              // 3, by +0x2C (PartyAction9_ByForm)
constexpr std::uint32_t kFormAction10Form0States = 0x65FDC4;   // 3, by +2   (PartyFormAction10_Form0)
constexpr std::uint32_t kAction10Form0States = 0x65FDD0;       // 2, by +2   (PartyAction10_Form0)
constexpr std::uint32_t kFormAction10Form1States = 0x65FDD8;   // 3, by +2   (PartyFormAction10_Form1)
constexpr std::uint32_t kAction10Form1States = 0x65FDE4;       // 3, by +2   (PartyAction10_Form1)
constexpr std::uint32_t kFormAction10Form2States = 0x65FDF0;   // 3, by +2   (PartyFormAction10_Form2)
constexpr std::uint32_t kAction10Form2States = 0x65FDFC;       // 2, by +2   (PartyAction10_Form2)
constexpr std::uint32_t kFormAction10Forms = 0x65FE04;         // 3, by +0x2C (PartyFormAction10_ByForm)
constexpr std::uint32_t kAction10Forms = 0x65FE10;             // 3, by +0x2C (PartyAction10_ByForm)
constexpr std::uint32_t kFormAction11Form0States = 0x65FE1C;   // 3, by +2   (PartyFormAction11_Form0)
constexpr std::uint32_t kAction11Form0States = 0x65FE28;       // 2, by +2   (PartyAction11_Form0)
constexpr std::uint32_t kFormAction11Form1States = 0x65FE30;   // 3, by +2   (PartyFormAction11_Form1)
constexpr std::uint32_t kAction11Form1States = 0x65FE3C;       // 3, by +2   (PartyAction11_Form1)
constexpr std::uint32_t kFormAction11Forms = 0x65FE48;         // 3, by +0x2C (PartyFormAction11_ByForm)
constexpr std::uint32_t kAction11Forms = 0x65FE54;             // 3, by +0x2C (PartyAction11_ByForm)
constexpr std::uint32_t kFormAction12Form0States = 0x65FE60;   // 3, by +2   (PartyFormAction12_Form0)
constexpr std::uint32_t kAction12Form0States = 0x65FE6C;       // 2, by +2   (PartyAction12_Form0)
constexpr std::uint32_t kAction12Form0State0Steps = 0x65FE74;  // 5, by +3   (PartyAction12_Form0State0)
constexpr std::uint32_t kAction12Form0State1Steps = 0x65FE88;  // 3, by +3   (PartyAction12_Form0State1)
constexpr std::uint32_t kFormAction12Form1States = 0x65FE94;   // 3, by +2   (PartyFormAction12_Form1)
constexpr std::uint32_t kFormAction12Forms = 0x65FEA0;         // 3, by +0x2C (PartyFormAction12_ByForm)
constexpr std::uint32_t kAction12Forms = 0x65FEAC;             // 3, by +0x2C (PartyAction12_ByForm)
constexpr std::uint32_t kFormAction13Form0States = 0x65FEB8;   // 3, by +2   (PartyFormAction13_Form0)

// Cells the group's code names by address.
constexpr std::uint32_t kSteps = 0x6697B0;          // Field_DirectionSteps: 8 rows of two longs (x, z)
constexpr std::uint32_t kSpriteFlag = 0x7DEF00;     // Sprite_Objects + 0x80, by 0xA4 a record
constexpr std::uint32_t kExtraFlag = 0x802080;      // Sprite_ObjectsExtra + 0x80, by 0xA4 a record
constexpr std::uint32_t kScriptFlagsHi = 0x9039A3;  // Field_ScriptFlags' high byte (bit 0x10: the word's 0x1000)

}  // namespace at
}  // namespace rest_1d
