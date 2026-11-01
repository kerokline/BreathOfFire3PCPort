// DIVERGENCE DIV-0200 (known-defects.md D401): a faerie's name drawn from its
// five-byte slot ends at the slot's last whole glyph.
//
// A birth (CommuSim_AddRecord) copies five bytes of the trait record's name
// into the save's name table 0x9048F0 - sixty slots of five, no terminator
// between them - and the faerie board's card (CommuBoard_DrawRecordCard), the
// entry panel (CommuEntry_DrawPanel) and the news board's lists
// (CommuRank_DrawListA / B) draw a slot with Text_DrawAt's count of five
// CHARACTERS. A Latin name fills the count in five bytes; a Chinese one is two
// bytes a glyph, so a name of three glyphs or more ends its slot on half a
// glyph and the draw reads on into the next slot, out of step: a byte there
// taken as the first of a two-byte code makes a glyph past 0xA00, and
// Text_DrawString's trap for that (`in al, dx`, a privileged instruction)
// ends the game. Only where the next slot has held a name - a faerie born
// there at some time - so it comes and goes with the births.
//
// Ours: the slot's bytes into a buffer up to the last whole character (a
// byte from 0x80 up starts a two-byte code), then a NUL, and the draw reads
// the buffer. A name that fits its slot draws as before; a Chinese name cut at
// half a glyph draws its whole glyphs and stops.
#pragma once

namespace faerie_name {

// The slot's name, NUL-ended after its last whole character, in `out`.
inline const unsigned char* Ended(const unsigned char* slot, unsigned char (&out)[6]) {
    unsigned n = 0;
    while (n < 5 && slot[n] != 0) {
        const unsigned width = slot[n] >= 0x80 ? 2u : 1u;
        if (n + width > 5) break;
        n += width;
    }
    for (unsigned i = 0; i < n; ++i) out[i] = slot[i];
    out[n] = 0;
    return out;
}

}  // namespace faerie_name
