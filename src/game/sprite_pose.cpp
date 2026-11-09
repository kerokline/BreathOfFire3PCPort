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

const Callees kOriginals = {
    Sprite_SetAnimationAt, Sprite_SetAnimation, Sprite_EnsureAnimation, Sprite_SetAnimationBank,
    Field_MemberSprite, Sprite_SetFrameQueueUpload, Sprite_ScriptStart, Effect_ReleaseAt,
};
Callees g = kOriginals;

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

// original 0x589590 (PSX 0x8014DC30): finds `bank` among the bank records
// (kBankTable, 8 bytes each: u16 the bank, then +2 +4 +5 +6 +7 bytes; as many
// as the low byte of the dword at kBankCount, read once) and gives the current
// sprite its texture: +0x25 = +4, +0x26 = +5, +0x27 = +2, +0x28 = +6, +0x2C =
// +7 (as a word), +0x70's low byte = kBankBytes[bank * 2] ORed into a cleared
// byte, +0x2B = kBankBytes[bank * 2 + 1]. Returns 0; 1 (and nothing changed)
// when there is no such record. The first match wins.
//
// As the original has it: the table pointer and Sprite_Current re-read
// before each store; kBankBytes indexed by the whole 16-bit bank, unchecked.
// The PSX re-reads the count each turn and stores the bank byte over the
// whole of +0x70 (its AND before is dead; US 0x8014DD3C, JP 0x8014DC30); the
// PC keeps +0x70's upper bytes (`and esi, 0xFFFFFF00` at 0x58962D, then the
// byte ORed in).
//
// DIVERGENCE DIV-0164 (known-defects D341): 1 through
// PatchBytes("SpriteReachWord") in SpritePose_Inject, after the self-test.
// The bank byte is then stored zero-extended over the whole dword +0x70, as
// the PSX does.
namespace sprite_pose { std::uint8_t g_reach_word = 0; }

void SpritePose_Inject() {
    if (bof3::WantsShadow("sprite_pose")) sprite_pose::SelfTest();
    {
        // DIV-0164: after the self-test, which compared Capcom's masked store.
        const std::uint8_t was = 0, is = 1;
        bof3::PatchBytes("SpriteReachWord", Low32(&sprite_pose::g_reach_word), &was, &is, 1);
        bof3::Log("DIV-0164    the bank setter stores the sprite's reach byte as a whole dword +0x70 (on unless the line above says OFF)");
    }
}
