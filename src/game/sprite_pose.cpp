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
//   +0x2C u16 which frame-offset table, and the member column
//   +0x4B the current animation
//   +0x4C the animation data (header of three offsets), +0x50 the script
//   +0x64 dword the member, +0x68 dword the member's y (its slot * 80)
//   +0x70 dword, whose low byte a bank sets
#include "game/sprite_pose.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace sprite_pose {

using namespace move_script;

const Callees kOriginals = {
    Sprite_SetAnimationAt, Sprite_SetAnimation, Sprite_EnsureAnimation, Sprite_SetAnimationBank,
    Field_MemberSprite, Sprite_SetFrameQueueUpload, Sprite_ScriptStart, Effect_ReleaseAt,
};
Callees g = kOriginals;

namespace {

constexpr unsigned kEffects = 20, kEffectBytes = 0x80;

// A pointer held in a dword of the game's memory, and pointer arithmetic done
// in 32 bits as the original's `add` / `lea` do it.
unsigned char* Ptr(std::uint32_t address) { return At(static_cast<std::uint32_t>(Long(At(address)))); }
unsigned char* Offset(const unsigned char* base, std::uint32_t by) {
    return At(static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(base)) + by);
}

}  // namespace
}  // namespace sprite_pose

using namespace sprite_pose;

// original 0x5891F0 (PSX 0x8014D5AC): Sprite_SetAnimationAt(animation, 0).
// The original pushes its whole argument dword; the callee reads its low byte.
extern "C" void __cdecl Sprite_SetAnimation(unsigned char animation) {
    g.set_animation_at(animation, 0);
}

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
// script pointer +0x50 is found in place: kFrameOffsets' table for +0x2C,
// then the u16 offset for the animation.
//
// As the original has it: nothing is bounded - the party set row, the member
// index, the bank record, +0x2C and the animation all index tables unchecked.
// Sprite_Current is re-read wherever the original re-reads it, so a callee
// that moves it moves the rest of this. `start` is passed on as 16 bits, which
// is what Sprite_ScriptStart reads of the dword the original pushes.
extern "C" void __cdecl Sprite_SetAnimationAt(unsigned char animation, unsigned short start) {
    if (animation & 0x80) {
        unsigned char* sprite = Sprite_Current;
        if (!(sprite[0x24] & 2)) {
            const unsigned row = (At(at::kPartySetCurrent)[0] & 0x7Fu) * 3u;
            SetLong(sprite + 0x64, At(at::kPartySetRows)[row + Word(sprite + 0x2C)]);
            sprite = Sprite_Current;
            SetLong(sprite + 0x68, sprite[0x26]);
            sprite = Sprite_Current;
            sprite[0x24] = static_cast<unsigned char>(sprite[0x24] ^ 3);
            sprite = Sprite_Current;
        }
        const std::uint32_t member = static_cast<std::uint32_t>(Long(sprite + 0x64));
        const unsigned char* const record = Offset(Ptr(at::kMemberAnimations + member * 4u), (animation & 0x7Fu) * 4u);
        g.set_bank(Word(record));
        Sprite_Current[0x4B] = record[2];
        Sprite_Current[0x2A] = record[3];
    } else {
        unsigned char* sprite = Sprite_Current;
        const unsigned char flags = sprite[0x24];
        if (flags & 2) {
            const unsigned char kept = static_cast<unsigned char>(flags & 0xFC);
            const std::int32_t slot = Long(sprite + 0x68) / 80;
            g.member_sprite(sprite[0x64], static_cast<std::uint32_t>(slot));
            sprite = Sprite_Current;
            sprite[0x24] = static_cast<unsigned char>(sprite[0x24] | kept);
            sprite = Sprite_Current;
        }
        sprite[0x4B] = animation;
    }
    unsigned char* const sprite = Sprite_Current;
    if (sprite[0x24] & 1) {
        g.queue_upload(sprite[0x4B]);
        g.script_start(start);
        return;
    }
    const unsigned char* const offsets = Ptr(at::kFrameOffsets);
    const unsigned char* const table = Offset(offsets, static_cast<std::uint32_t>(Long(offsets + Word(sprite + 0x2C) * 4u)));
    SetLong(sprite + 0x50, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(Offset(table, Word(table + sprite[0x4B] * 2u)))));
    g.script_start(start);
}

// original 0x589330 (PSX 0x8014D7C8): 1 if the current sprite already plays
// `animation` (nothing done), else Sprite_SetAnimation and 0. The original's
// result is al; the rest of eax is whatever the path left there (the argument's
// upper bytes, or the callee's), which no caller reads as a number
// (docs/sprite-pose.md §2).
extern "C" unsigned char __cdecl Sprite_EnsureAnimation(unsigned char animation) {
    if (Sprite_Current[0x4B] == animation) return 1;
    g.set_animation(animation);
    return 0;
}

// original 0x5894D0 (PSX 0x8014DAFC, term for term): the current sprite's
// frame `frame` (a byte) from its animation data at +0x4C - a header of three
// offsets from the data: the images' offset table [0], the script words [8],
// the 3-byte frame entries [0xC]. Unless +0x00 bit 1: the frame's image is
// queued for upload - x = the texture slot +0x25 * 64 (slots above 0x10 wrap
// to the lower half: slot - 0x10, and y + 0x100), y = +0x26, the record the
// image table names - and Gfx_UploadQueueCount goes up by one.
//
// As the original has it - the known defect D4: **the append has no bound**.
// The queue's arrays have 20 slots and the count is a byte; entries 20 and up
// land in the next array and beyond, and the count wraps at 256. Kept
// faithfully (docs/known-defects.md D4; DIV-0004 drains the queue instead).
// The count is re-read for the increment, as the original re-reads it.
//
// Always: +0x2A = the entry's third byte (through the object pointer read on
// entry), and +0x50 = the script words plus the u16 the entry's second byte
// picks - which is also the return value. Both entry bytes are read after the
// queue's stores, in the original's order.
extern "C" unsigned char* __cdecl Sprite_SetFrameQueueUpload(unsigned frame) {
    unsigned char* const sprite = Sprite_Current;
    const unsigned char* const data = At(static_cast<std::uint32_t>(Long(sprite + 0x4C)));
    const unsigned char* const entry = Offset(data, static_cast<std::uint32_t>(Long(data + 0xC)) + (frame & 0xFFu) * 3u);
    const unsigned char* const images = Offset(data, static_cast<std::uint32_t>(Long(data)));
    const unsigned char* const words = Offset(data, static_cast<std::uint32_t>(Long(data + 8)));
    if (!(sprite[0] & 2)) {
        const unsigned slot = sprite[0x25];
        const unsigned count = Gfx_UploadQueueCount;
        if (slot > 0x10) {
            SetWord(At(at::kQueueX + count * 2u), (slot - 0x10u) << 6);
            SetWord(At(at::kQueueY + count * 2u), sprite[0x26] + 0x100u);
        } else {
            SetWord(At(at::kQueueX + count * 2u), slot << 6);
            SetWord(At(at::kQueueY + count * 2u), sprite[0x26]);
        }
        const unsigned char* const image = Offset(images, static_cast<std::uint32_t>(Long(images + entry[0] * 4u)));
        SetLong(At(at::kQueueRecord + count * 4u), static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(image)));
        Gfx_UploadQueueCount = static_cast<unsigned char>(Gfx_UploadQueueCount + 1);
    }
    sprite[0x2A] = entry[2];
    unsigned char* const script = Offset(words, Word(words + entry[1] * 2u));
    SetLong(Sprite_Current + 0x50, static_cast<std::int32_t>(reinterpret_cast<std::uintptr_t>(script)));
    return script;
}

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
// whole of +0x70 (its AND before is dead); the PC keeps +0x70's upper bytes.
extern "C" unsigned char __cdecl Sprite_SetAnimationBank(unsigned short bank) {
    const unsigned count = static_cast<std::uint32_t>(Long(At(at::kBankCount))) & 0xFFu;
    const unsigned char* const table = Ptr(at::kBankTable);
    unsigned i = 0;
    for (;; ++i) {
        if (i >= count) return 1;
        if (Word(table + i * 8u) == bank) break;
    }
    Sprite_Current[0x25] = table[i * 8u + 4];
    Sprite_Current[0x26] = Ptr(at::kBankTable)[i * 8u + 5];
    Sprite_Current[0x27] = Ptr(at::kBankTable)[i * 8u + 2];
    Sprite_Current[0x28] = Ptr(at::kBankTable)[i * 8u + 6];
    const unsigned column = Ptr(at::kBankTable)[i * 8u + 7];
    SetWord(Sprite_Current + 0x2C, column);
    SetLong(Sprite_Current + 0x70, static_cast<std::int32_t>(static_cast<std::uint32_t>(Long(Sprite_Current + 0x70)) & 0xFFFFFF00u));
    unsigned char* sprite = Sprite_Current;
    sprite[0x70] = static_cast<unsigned char>(sprite[0x70] | At(at::kBankBytes)[bank * 2u]);
    const unsigned char second = At(at::kBankBytes)[bank * 2u + 1];
    Sprite_Current[0x2B] = second;
    return 0;
}

// original 0x57C4C0 (PSX 0x8015C4B4, branch for branch): turns the current
// sprite to face `direction`. Types 8 and up do not turn; type 6 takes the
// direction as an animation (Sprite_EnsureAnimation(direction) straight).
// Otherwise the pair kDirectionAnimations[direction * 2] gives the animation
// and +0x2A (stored first); then:
//   - byte +0xA1 of Field_ActiveMember is 0xFF: Sprite_EnsureAnimation(it);
//   - the sprite already plays it: nothing more;
//   - animations 5 and 6: Sprite_SetAnimationAt(it, +0xA1) - they start at
//     the step the active member's byte names;
//   - any other: Sprite_SetAnimation(it).
// As the original has it: the direction indexes the pair table unchecked,
// and Field_ActiveMember is not checked for null.
extern "C" void __cdecl Sprite_FaceDirection(unsigned char direction) {
    const unsigned char type = Sprite_Current[6];
    if (type >= 8) return;
    if (type == 6) {
        g.ensure_animation(direction);
        return;
    }
    const unsigned pair = direction * 2u;
    Sprite_Current[0x2A] = At(at::kDirectionAnimations)[pair + 1];
    const unsigned char start = Field_ActiveMember[0xA1];
    const unsigned char animation = At(at::kDirectionAnimations)[pair];
    if (start == 0xFF) {
        g.ensure_animation(animation);
        return;
    }
    if (Sprite_Current[0x4B] == animation) return;
    if (animation == 5 || animation == 6) {
        g.set_animation_at(animation, start);
        return;
    }
    g.set_animation(animation);
}

// original 0x589810 (PSX 0x8019701C): the first of the 20 Effect_Objects whose
// byte +0 is 0, or 0xFF when all are in use.
extern "C" unsigned char __cdecl Effect_FindFree(void) {
    for (unsigned i = 0; i < kEffects; ++i)
        if (Effect_Objects[i * kEffectBytes] == 0) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x589840 (PSX 0x80197070): bytes 0..4 of the current object to 0,
// the pointer re-read before each store as the original does.
extern "C" void __cdecl Effect_Release(void) {
    for (unsigned i = 0; i < 5; ++i) Sprite_Current[i] = 0;
}

// original 0x589870 (PSX 0x801970C0): bytes 0..4 of effect object `index` to
// 0. As the original has it: the index is not checked against the 20 - up to
// 255 reaches 0x7F80 bytes past the pool.
extern "C" void __cdecl Effect_ReleaseAt(unsigned char index) {
    unsigned char* const object = Effect_Objects + index * kEffectBytes;
    for (unsigned i = 0; i < 5; ++i) object[i] = 0;
}

// original 0x5898A0 (PSX 0x8019711C): Effect_ReleaseAt for each of the 20.
extern "C" void __cdecl Effect_ClearAll(void) {
    for (unsigned i = 0; i < kEffects; ++i) g.release_at(static_cast<unsigned char>(i));
}

void SpritePose_Inject() {
    if (bof3::WantsShadow("sprite_pose")) sprite_pose::SelfTest();
    BOF3_INJECT(Sprite_SetAnimation);
    BOF3_INJECT(Sprite_SetAnimationAt);
    BOF3_INJECT(Sprite_EnsureAnimation);
    BOF3_INJECT(Sprite_SetFrameQueueUpload);
    BOF3_INJECT(Sprite_SetAnimationBank);
    BOF3_INJECT(Sprite_FaceDirection);
    BOF3_INJECT(Effect_FindFree);
    BOF3_INJECT(Effect_Release);
    BOF3_INJECT(Effect_ReleaseAt);
    BOF3_INJECT(Effect_ClearAll);
}
