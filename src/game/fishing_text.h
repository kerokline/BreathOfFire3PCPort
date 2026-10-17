#pragma once

#include <cstdint>

// DIVERGENCE DIV-0069: the fishing minigame's text from a language overlay -
// the lines effect kind 0xF types across the top window, the three tab labels
// above the equip menu, and the layout those want under Latin text (the
// typing cadence and the button label by the real pen advance, the tabs
// centred, an accessory's name to 12 characters). See src/game/fishing_text.cpp
// and docs/fishing-text.md.

// Applies a kind-16 chunk: tag 1 the thirteen lines (0x653B98's records), tag 2
// the three tab labels (0x66A088's table). The payload is a count and that
// many NUL-terminated strings, copied into the DLL's buffers, and each table
// entry re-aimed at its buffer after checking it names the shipped string (or
// our buffer, from an earlier load). An empty string keeps its entry as
// shipped. Aborts loudly on anything else.
void FishingText_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// Arms the layout under a Latin overlay (Lang_Latin). Called from InjectAll
// after every module's self-test, so effect_1a's and effect_1b's fuzz compare
// Capcom's layout; nothing here changes while it is off.
void FishingText_Arm();

// True once armed under a Latin overlay: the draws below then differ.
bool FishingText_On();

// The pen width of the first `chars` characters of `text` (two bytes where bit 7
// is set), DIV-0006's advances; stops at a NUL.
unsigned FishingText_Width(const unsigned char* text, unsigned chars);

// Half the advance of the character at `text`, at least 1: the frames, at two
// units a frame, the line scrolls while that character is typed.
unsigned FishingText_HalfAdvance(const unsigned char* text);

// The button label `entry` (0..5, 0x66A2FC's order: circle, cross, triangle,
// square, L1, R1): a two-byte code of DIV-0051's PlayStation icon for that
// button, the disc's shape. With the label through sigma off, only 0..2 (the
// line bytes, which the port's numerals 1, 2, 3 name) are asked. Aborts past 5.
const unsigned char* FishingText_Label(unsigned entry);

// Ledger draft D (amends DIV-0069; docs/fishing-text.md 8): which button a
// line's label names, and in which pen.
//   entry  0..5 of 0x66A2FC (the port's stand-in, a circled numeral or its X)
//          and of FishingText_Label (the disc's shape);
//   pen    the button's colour: circle 2, cross 1, triangle 6 (0x653C00),
//          square 5, L1 and R1 0 (the Config cell 0x461C00's).
// Off (BOF3X_ORIGINAL=FishingLabelSigma, and before FishingText_Arm, so
// effect_1a's fuzz compares Capcom's draws): entry = the line byte, pen =
// 0x653C00[line], the original's reads. On: the line byte names Japan's
// button (0 circle, 1 cross, 2 triangle) and the label draws Sigma(it); text
// 0, the cast's line, draws the button of word 7 (Field_ConfirmButtons
// 0x90358E), which LeaderPanel_S2Wait 0x528FA0 tests for the cast.
struct FishingLabel {
    unsigned entry;
    unsigned pen;
};
FishingLabel FishingText_LabelOf(unsigned text, unsigned line);

// The characters an accessory's name is drawn to: 12 (the US field) when on,
// else the original's 8.
unsigned FishingText_NameCount();
