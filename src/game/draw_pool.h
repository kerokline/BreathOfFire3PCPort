// The index pool behind the 0x90-byte draw items at 0x905E80, originals
// 0x56FBD0 and 0x56FC70 (docs/sprite-draw-order.md section 4).
#pragma once

#include "bof3/symbols.gen.h"

void DrawPool_Inject();

// DIVERGENCE DIV-0062: the pool grown from 1,024 to 2,048 items. The arrays
// are the original's (0x905E80, 0x7E09E0) until DrawPool_Grow, which runs
// after every module's self-test (inject_all.cpp), so each fuzz compared the
// original's arrays; then they are the dll's, every Capcom site that names
// the item array re-aimed (PatchBytes "DrawPool") and ours reading these.
//
// The two pointers are null until the switch, and Items() / Free() answer
// the original's arrays at their place (Img) while they are: the arrays'
// places are run-time values since the image moved (common/image_base.h),
// so a pointer initialised from one would be a dynamic initialiser, and a
// file-scope constant of another unit that reads Items() (field_misc_fuzz's
// kDrawItems) read it before this unit's ran - a null item array, found
// 2026-10-29 by the hosted BOF3X_SHADOW='*' self-test. Null needs no
// initialiser, so there is no order to get wrong.
namespace draw_pool {
extern unsigned char* g_items;    // null: the original's array, DrawItems
extern unsigned short* g_free;    // null: the original's ring, DrawItemPool_Free
extern unsigned g_count;   // 1,024 or 2,048; the free queue's ring is Count()
inline unsigned char* Items() { return g_items ? g_items : DrawItems; }
inline unsigned short* Free() { return g_free ? g_free : DrawItemPool_Free; }
inline unsigned Count() { return g_count; }
inline unsigned Mask() { return g_count - 1; }
}  // namespace draw_pool
void DrawPool_Reserve();   // first in InjectAll: the room below 16 MB
void DrawPool_Grow();      // last in InjectAll: the switch
