// A sprite object's animation and pose, and the effect-object pool's
// bookkeeping: originals 0x5891F0 / 0x589200 / 0x589330 / 0x5894D0 /
// 0x589590 / 0x57C4C0 and 0x589810..0x5898A0. docs/sprite-pose.md.
#pragma once

#include <cstdint>

void SpritePose_Inject();

namespace sprite_pose {

// The data the ten read, none of it named in symbols.toml yet; each is
// described where it is used (sprite_pose.cpp) and in docs/sprite-pose.md §3.
namespace at {
constexpr std::uint32_t kPartySetCurrent = 0x90412C;      // byte; bit 7 from PartySet_Select mode 0
constexpr std::uint32_t kPartySetRows = 0x669750;         // 3 bytes a row: the member in each column
constexpr std::uint32_t kMemberAnimations = 0x669884;     // a pointer per member: 4-byte {bank, animation, +0x2A}
constexpr std::uint32_t kFrameOffsets = 0x9039D8;         // pointer: a dword offset per +0x2C, then words per animation
constexpr std::uint32_t kBankCount = 0x8C3580;            // dword; its low byte counts the bank records
constexpr std::uint32_t kBankTable = 0x7E0880;            // pointer to the 8-byte bank records
constexpr std::uint32_t kBankBytes = 0x6690A0;            // 2 bytes a bank: +0x70's low byte, +0x2B
constexpr std::uint32_t kDirectionAnimations = 0x65F5BC;  // 2 bytes a direction: the animation, +0x2A
constexpr std::uint32_t kQueueX = 0x903680;               // Gfx_UploadQueueX
constexpr std::uint32_t kQueueY = 0x9036A8;               // Gfx_UploadQueueY
constexpr std::uint32_t kQueueRecord = 0x92BF20;          // Gfx_UploadQueueRecord
}  // namespace at

// Every direct callee of the ten, through pointers so that the start-up fuzz
// can stand recording functions in for them (sprite_pose_fuzz.cpp). The
// arguments are typed as the callees read them.
struct Callees {
    void (__cdecl* set_animation_at)(unsigned char, unsigned short);   // 0x589200
    void (__cdecl* set_animation)(unsigned char);                      // 0x5891F0
    unsigned char (__cdecl* ensure_animation)(unsigned char);          // 0x589330
    unsigned char (__cdecl* set_bank)(unsigned short);                 // 0x589590
    void (__cdecl* member_sprite)(unsigned, unsigned);                 // 0x533BA0
    unsigned char* (__cdecl* queue_upload)(unsigned);                  // 0x5894D0
    void (__cdecl* script_start)(unsigned short);                      // 0x589350
    void (__cdecl* release_at)(unsigned char);                         // 0x589870
};
extern const Callees kOriginals;
extern Callees g;

// The start-up fuzz, BOF3X_SHADOW=sprite_pose. Called by SpritePose_Inject
// before anything is patched.
void SelfTest();

}  // namespace sprite_pose
