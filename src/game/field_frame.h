// The field's frame loop, from its per-frame entries down to the overlay
// list's draw: originals 0x517200, 0x517240, 0x517350, 0x5173E0, 0x517490,
// 0x57B780, 0x57B7B0, 0x57B830, 0x57B860, 0x592F00, 0x592F20 and 0x593020.
// docs/field-frame.md.
#pragma once

#include "game/field_frame_callees.h"

void FieldFrame_Inject();

namespace field_frame {
extern int g_band_fill;   // defined in field_frame.cpp (source-layout B0: promoted for a moved reader)
// moved from field_frame.cpp with the state that has the type (source-layout B0)
// DIVERGENCE DIV-0041 (ultrawide round, group UC2, docs/ultrawide-culls-fix.md
// section 2): a type-0x0A sprite in the wide picture's bands. Sprite_ProjectA
// keeps Capcom's margins for the off-screen flag and for its place on
// Sprite_DrawList, so every list the original builds is built the same (the
// list holds 40, and a band sprite taking a slot could push out one the
// original draws - and that one's flag, which Sprite_Draw writes, would go
// stale). A sprite the original leaves off the list for x alone, but inside
// the margins moved out by the columns, is noted here instead, and
// Field_DrawFrame puts it on the list just before the draw pass, in whatever
// slots the frame left. Its flag is already set (Sprite_Draw sets it again at
// its own bound, which lies inside the margins), and the scale dwords +0x40 /
// +0x44 Sprite_Draw writes are put back after the pass. Empty while the fills
// are unarmed (every fuzz).
//
// Amended 2026-11-15 (the owner's tree in area 9, docs/ultrawide-culls-fix.md
// section 8): a model sprite (+0x24 bit 6, drawn through
// Sprite_AddDrawRecords, which has no cull of its own and writes neither the
// flag nor the scale) is appended as well. Until then it stayed culled at
// Capcom's margins, 20 to 40 px INSIDE the 21:9 view: a tree 140 px wide
// vanished whole while half of it was in the band. Sprite_DrawPass asks
// BandModelFits before it draws one.
struct BandSprite {
    unsigned char* sprite;
    unsigned char scale[8];
};
extern BandSprite g_band[0x28];   // defined in field_frame.cpp (source-layout B0: promoted for a moved reader)
extern unsigned g_band_count;   // defined in field_frame.cpp (source-layout B0: promoted for a moved reader)
extern unsigned g_band_added;   // defined in field_frame.cpp (source-layout B0: promoted for a moved reader)
// Whether Sprite_DrawPass may draw the model `object` (event_cond.cpp, beside
// Sprite_AddDrawRecords). True for every sprite the original listed. For a
// model BandAppend added: only while its quads, and those of every model of
// the original's still to come on the list, have room - in Sprite_DrawRecords'
// 100 records (`records` are used; the models of this layer counted) and in
// the packet pool. So a band model can neither run the record array over nor
// make Sprite_AddDrawRecords refuse one of the original's, which returns with
// the GTE matrix still pushed. `next` is the list index after the object's.
bool BandModelFits(const unsigned char* object, unsigned records, unsigned next);
}  // namespace field_frame
