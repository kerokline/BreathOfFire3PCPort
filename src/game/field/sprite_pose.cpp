// A sprite object's animation and pose, and the effect-object pool's
// bookkeeping (docs/sprite-pose.md): originals 0x5891F0 Sprite_SetAnimation,
// 0x589200 Sprite_SetAnimationAt, 0x589330 Sprite_EnsureAnimation, 0x5894D0
// Sprite_SetFrameQueueUpload, 0x589590 Sprite_SetAnimationBank, 0x57C4C0
// Sprite_FaceDirection, and 0x589810 Effect_FindFree, 0x589840
// Effect_Release, 0x589870 Effect_ReleaseAt, 0x5898A0 Effect_ClearAll.
//
// The sprite object is addressed by offset, as in sprite_anim.cpp and
// docs/sprite-draw-order.md §5-6. The fields these read and write:
//   +0x00 flags; bit 1: no image upload for this object
//   +0x06 type (8 and up: no facing; 6: the direction is an animation)
//   +0x24 flags; bit 0: frames are uploaded (else read in place), bit 1:
//         the member-sprite set-up is pending (+0x64 / +0x68 hold it)
//   +0x25 texture slot, +0x26 its y row, +0x27 +0x28 bytes of the bank
//   +0x2A a pose byte (the facing half of a direction pair), +0x2B a bank byte
//   +0x2C u16 the animation bank's section (one per sprite kind), and the member column
//   +0x4B the current animation
//   +0x4C the animation data (header of three offsets), +0x50 the script
//   +0x64 dword the member, +0x68 dword the member's y (its slot * 80)
//   +0x70 dword, whose low byte a bank sets
#include "game/sprite_pose.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

#include <windows.h>

namespace sprite_pose {

using namespace move_script;


namespace {

// Sprite_Current, as rv::Current<records::SpriteObject>(), may be a Sprite_Objects
// record or another family whose first 0x80 bytes are a sprite object's (an
// ObjTrio member, the kind-2 object), so only that part is named.

}  // namespace
}  // namespace sprite_pose

using namespace sprite_pose;

// original 0x589200 (PSX 0x8014D5D0, branch for branch): sets the current
// sprite's animation and starts its script at step `start`.
//
// An animation with bit 7 set is a party member's own version of a generic
// one: the member's table at kMemberAnimations gives a bank, the animation
// number in that bank, and +0x2A. The member is first worked out and kept in
// +0x64 (and the slot row in +0x68) unless +0x24 bit 1 says that was already
// done; doing it flips +0x24 bits 0 AND 1 (`xor 3`), whatever bit 0 was.
// Without bit 7, a pending member set-up (+0x24 bit 1) is carried out now:
// Field_MemberSprite(member +0x64, slot +0x68 / 80) - a signed division, and
// the whole quotient is passed; the callee reads a byte - and afterwards
// +0x24 gets back its old bits 2..7 ORed in (bits 0 and 1 as the callee left
// them).
//
// Then, with +0x24 bit 0, the frame is queued for upload
// (Sprite_SetFrameQueueUpload, the known-defects D4 feed); without it the
// script pointer +0x50 is found in place: the animation bank's section for +0x2C,
// then the u16 offset for the animation.
//
// As the original has it: nothing is bounded - the party set row, the member
// index, the bank record, +0x2C and the animation all index tables unchecked.
// Sprite_Current is re-read wherever the original re-reads it, so a callee
// that moves it moves the rest of this. `start` is passed on as 16 bits, which
// is what Sprite_ScriptStart reads of the dword the original pushes.


void SpritePose_Inject() {
    if (bof3::WantsShadow("sprite_pose")) sprite_pose::SelfTest();
}
