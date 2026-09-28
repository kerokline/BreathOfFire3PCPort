// The index pool behind the 0x90-byte draw items at 0x905E80, originals
// 0x56FBD0 and 0x56FC70 (docs/sprite-draw-order.md section 4).
#pragma once

void DrawPool_Inject();

// DIVERGENCE DIV-0062: the pool grown from 1,024 to 2,048 items. The arrays
// are the original's (0x905E80, 0x7E09E0) until DrawPool_Grow, which runs
// after every module's self-test (inject_all.cpp), so each fuzz compared the
// original's arrays; then they are the dll's, every Capcom site that names
// the item array re-aimed (PatchBytes "DrawPool") and ours reading these.
namespace draw_pool {
extern unsigned char* g_items;
extern unsigned short* g_free;
extern unsigned g_count;   // 1,024 or 2,048; the free queue's ring is Count()
inline unsigned char* Items() { return g_items; }
inline unsigned short* Free() { return g_free; }
inline unsigned Count() { return g_count; }
inline unsigned Mask() { return g_count - 1; }
}  // namespace draw_pool
void DrawPool_Reserve();   // first in InjectAll: the room below 16 MB
void DrawPool_Grow();      // last in InjectAll: the switch
