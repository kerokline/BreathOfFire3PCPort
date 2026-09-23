// The field objects' per-frame ops that the shop route reaches and the
// attract sequence does not: the jump (set-up, camera, height check, the
// velocity step), the shade fade of a sprite's CLUT, the tile tests under the
// leader (cells all / none of a code, the floor-damage codes 0x80..0x88, the
// turn-back tiles 0x89 / 0x8A, 0xD0, 0xA4), the two actor-state timers and the
// equipment tick. docs/event-objs.md.
#pragma once

void EventObjs_Inject();
