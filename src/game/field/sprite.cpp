// Moved here by tools/move_funcs.py from battle_sprites.cpp, field_hidden.cpp, sprite_anim.cpp, sprite_find.cpp, sprite_pose.cpp, sprite_screen.cpp (docs/source-layout-round.md).
#include "game/field/sprite.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/band_census.h"
#include "game/battle_sprites.h"
#include "game/battle_sprites_callees.h"
#include "game/cheats.h"
#include "game/field_hidden.h"
#include "game/field_hidden_callees.h"
#include "game/layering.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/sound_sp.h"
#include "game/sprite_anim.h"
#include "game/sprite_find.h"
#include "game/sprite_pose.h"
#include "game/sprite_screen.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace battle_sprites;

using namespace field_hidden;

namespace sprite_pose {

using namespace move_script;

}  // namespace sprite_pose

using namespace sprite_pose;

namespace field_hidden {

using move_script::Long;

using move_script::Word;

}  // namespace field_hidden

namespace {

// The record views (game/record_views.h): the member states and party actions
// run on an ObjTrio record, Field_State is one; the Sprite_* helpers run on
// any object whose first 0x80 bytes are a sprite object's.
// A pointer the original keeps in a dword, read afresh.
unsigned char* PtrIn(const unsigned char* at) {
    return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<std::uint32_t>(field_hidden::Long(at))));
}

}  // namespace

#define FH_EXPORT extern "C" __attribute__((disable_tail_calls))

namespace {

// A sprite object's animation script, as the four functions here use it
// (originals 0x589350..0x5894CF, one contiguous block):
//
//   header   2 bytes; byte 1 is the number of steps
//   steps    2 bytes each: how many ticks to hold, and which frame
//            - or, where a step would start, a byte >= 0x80 and then the
//            number of the step to go to
//   frames   u16 each, straight after the last step
//
// The object is larger and mostly unread; it is addressed by offset for that
// reason. kScript points past the header once Sprite_ScriptStart has run.
constexpr unsigned kSteps = 0x49;      // u8

constexpr unsigned kHold = 0x4A;       // u8, ticks left on this step

constexpr unsigned kScript = 0x50;     // const u8*, null for none

constexpr unsigned kFrames = 0x54;     // const u16*

constexpr unsigned kPosition = 0x58;   // u16 byte index into the steps

constexpr unsigned kFrame = 0x5A;      // u16, the current step's frame word

template <class T> T Get(const unsigned char* object, unsigned at) {
    // A pointer-typed field is a 32-bit word of the image on every width.
    if constexpr (std::is_pointer_v<T>) {
        std::uint32_t w;
        std::memcpy(&w, object + at, sizeof w);
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(w));
    } else {
        T v;
        std::memcpy(&v, object + at, sizeof v);
        return v;
    }
}

template <class T> void Put(unsigned char* object, unsigned at, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(object + at, &w, sizeof w);
    } else {
        std::memcpy(object + at, &v, sizeof v);
    }
}

// The part Sprite_ScriptTick and Sprite_ScriptTickOnce share: the hold has run
// out and the script has not. Follows a jump if there is one - the original's
// return value says whether there was - and takes the step.
unsigned char NextStep(unsigned char* object, std::uint16_t position) {
    unsigned char jumped = 0;
    const unsigned char* at = Get<const unsigned char*>(object, kScript) + position;
    if (at[0] >= 0x80) {
        Put<std::uint16_t>(object, kPosition, static_cast<std::uint16_t>(at[1] * 2));
        jumped = 1;
    }
    Sprite_ScriptStep();
    return jumped;
}

constexpr unsigned kObjectSize = 0xA4;

constexpr unsigned kObjectsSpriteFind = 30, kExtraSpriteFind = 4;

// The fields of a sprite object this file reads; the rest is unread.
constexpr unsigned kFlags = 0x00;     // u8, bit 0: in use

constexpr unsigned kType = 0x06;      // u8

constexpr unsigned kHandle = 0x18;    // u8: the object this one is attached to (movement script F8 07)

constexpr unsigned kFlags24 = 0x24;   // u8, bit 0x40; bit 0x20: attached

constexpr unsigned kDrawKey = 0x32;   // u16: layer, then order within it (sprite-draw-order.md section 1)

constexpr unsigned kX = 0x34;         // s32

constexpr unsigned kY = 0x38;         // s32

constexpr unsigned kZ = 0x3E;         // s16

constexpr unsigned kRecord = 0x54;    // const u8*: +2 s8 reach, +3 bit 0x10 "not this one"

template <class T> T GetSpriteFind(const unsigned char* at, unsigned offset) {
    // A pointer-typed field is a 32-bit word of the image on every width.
    if constexpr (std::is_pointer_v<T>) {
        std::uint32_t w;
        std::memcpy(&w, at + offset, sizeof w);
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(w));
    } else {
        T v;
        std::memcpy(&v, at + offset, sizeof v);
        return v;
    }
}

template <class T> void PutSpriteFind(unsigned char* at, unsigned offset, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(at + offset, &w, sizeof w);
    } else {
        std::memcpy(at + offset, &v, sizeof v);
    }
}

// |a - b| as the original computes it: a 32-bit difference that may wrap, and
// an absolute value that leaves 0x80000000 negative.
std::int32_t WrappingDistance(std::int32_t a, std::int32_t b) {
    const std::uint32_t d = static_cast<std::uint32_t>(a) - static_cast<std::uint32_t>(b);
    const std::uint32_t sign = d & 0x80000000u ? 0xFFFFFFFFu : 0u;
    return static_cast<std::int32_t>((d ^ sign) - sign);
}

std::int32_t ReachOf(const unsigned char* record) {
    const std::int32_t reach8 = static_cast<signed char>(record[2]);
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(reach8) << 15);
}

bool Qualifies(const unsigned char* object, const unsigned char* current) {
    if (!(object[kFlags] & 1)) return false;
    if (!(object[kFlags24] & 0x40)) return false;
    if (object[kType] != 0x0A) return false;
    if (object == current) return false;
    const auto* record = GetSpriteFind<const unsigned char*>(object, kRecord);
    if (record[3] & 0x10) return false;
    const std::int32_t reach = ReachOf(record);
    if (WrappingDistance(GetSpriteFind<std::int32_t>(current, kX), GetSpriteFind<std::int32_t>(object, kX)) > reach) return false;
    if (WrappingDistance(GetSpriteFind<std::int32_t>(current, kY), GetSpriteFind<std::int32_t>(object, kY)) > reach) return false;
    const int dz = GetSpriteFind<std::int16_t>(current, kZ) - GetSpriteFind<std::int16_t>(object, kZ);
    return (dz < 0 ? -dz : dz) <= 0x300;
}

unsigned char ObjectByHandle(unsigned char handle) {
    const unsigned char n = handle & 0x3F;
    return handle & 0x80 ? n : static_cast<unsigned char>(n + kObjectsSpriteFind);
}

struct LiveCounts { unsigned calls, bit7, mismatches; } g_live_handle, g_live_inherit;

void LiveReport(const char* name, const LiveCounts& c) {
    if (c.calls == 1 || c.calls % 4096 == 0)
        bof3::Log("shadow      %s live: %u calls (%u with bit 7), %u MISMATCHES", name, c.calls, c.bit7, c.mismatches);
}

// An object number as Sprite_InheritDrawKey turns it into an address: signed,
// 30 and above into Sprite_ObjectsExtra, and no bound on either side.
const unsigned char* ObjectByNumber(unsigned char number) {
    const int n = static_cast<signed char>(number);
    return n >= static_cast<int>(kObjectsSpriteFind) ? rv::Bytes(Sprite_ObjectsExtra) + (n - static_cast<int>(kObjectsSpriteFind)) * static_cast<int>(kObjectSize)
                                           : rv::Bytes(Sprite_Objects) + n * static_cast<int>(kObjectSize);
}

void InheritDrawKey() {
    unsigned char number;
    if (Sprite_Current[kFlags24] & 0x20) {
        number = ObjectByHandle(Sprite_Current[kHandle]);
    } else {
        number = Sprite_FindNearby();
        if (number == 0xFF) return;
    }
    PutSpriteFind<std::uint16_t>(Sprite_Current, kDrawKey, GetSpriteFind<std::uint16_t>(ObjectByNumber(number), kDrawKey));
}

}  // namespace

namespace sprite_pose {

namespace {

// A pointer held in a dword of the game's memory, and pointer arithmetic done
// in 32 bits as the original's `add` / `lea` do it.
unsigned char* Ptr(std::uint32_t address) { return At(static_cast<std::uint32_t>(Long(At(address)))); }

unsigned char* Offset(const unsigned char* base, std::uint32_t by) {
    return At(Low32(base) + by);
}

}  // namespace

}  // namespace sprite_pose

// BOF3X_POSELOG=1: instrumentation for the facing investigation (issue #142's side thread) - every
// Sprite_SetAnimationAt and Sprite_SetFrameQueueUpload on a party member's sprite (ObjTrio) logged.
static bool PoseLogOn() {
    static int on = -1;
    if (on < 0) {
        char v[8];
        on = GetEnvironmentVariableA("BOF3X_POSELOG", v, sizeof v) != 0 && v[0] == '1';
    }
    return on == 1;
}

static void PoseLog(const char* what, const void* object, unsigned a, unsigned b) {
    const std::uintptr_t o = reinterpret_cast<std::uintptr_t>(object);
    if (o < ::bof3::Img(0x802D40u) || o >= ::bof3::Img(0x802D40u) + 3u * 0x14Cu) return;
    const unsigned char* const s = static_cast<const unsigned char*>(object);
    std::uint32_t anim_data, script;
    std::memcpy(&anim_data, s + 0x4C, 4);
    std::memcpy(&script, s + 0x50, 4);
    bof3::Log("poselog     frame %lu %s trio %u a %u b %u | flags %02X type %u facing %u draw %02X slot %u row %u flip %u col %u anim %u data %08X script %08X",
              static_cast<unsigned long>(Frame_Counter), what, static_cast<unsigned>((o - ::bof3::Img(0x802D40u)) / 0x14Cu), a, b,
              s[0], s[6], s[8], s[0x24], s[0x25], s[0x26], s[0x2A], s[0x2C], s[0x4B], anim_data, script);
}

namespace {

// `fld dword` then `fstp dword`: exact, except that a signalling NaN comes
// back quiet (bit 22 set) - the only change the x87 makes to a float it moves.
std::uint32_t ThroughX87(std::uint32_t bits) {
    const bool nan = (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0;
    return nan ? bits | 0x00400000u : bits;
}

// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

}  // namespace

// original 0x588F00 (0x8014D154; PSX 0x8014D154): Sprite_Current +0x29 = Draw_OtSlot, then
// (a tail jump) Sprite_UpdateScreen.
extern "C" void __cdecl Sprite_UpdateScreenSlot(void) {
    Sprite_Current[0x29] = Draw_OtSlot;
    battle_sprites::g.update_screen();
}

// original 0x588F20: Sprite_Current's draw key, screen position and depth,
// and its place on the draw list (PSX FUN_8014D184, the sibling's
// Actor_UpdateScreenPos).
//
// As the original has it: the key is 16-bit arithmetic, stored before its
// range is tested; the float screen point is stored as the x87 moves it and
// truncated as the CRT's _ftol does (see Ftol16); the PSX's call for a sprite
// with flag bit 8 is Port_DroppedCall, a bare ret, on the PC.
// Sprite_Current is any sprite head: effect handlers call this too
// (field_c1.cpp, field_c2.cpp, EffectKind37_Play, rest_3f CaptureSprite), so
// it reads as SpriteHead (C2 audit frs F2).
extern "C" void __cdecl Sprite_UpdateScreen(void) {
    records::SpriteHead* sprite = rv::Current<records::SpriteHead>();
    if (sprite->draw_flags & 0x80) {
        sprite_screen::g.overlay();
        return;
    }
    const unsigned x_whole = sprite->x_frac == 0, z_whole = sprite->z_frac == 0;
    const signed char adjust = Sprite_KeyAdjust[sprite->key_class * 3u + x_whole + z_whole];
    const auto key = static_cast<std::uint16_t>(adjust - x_whole - z_whole + sprite->z_whole + sprite->x_whole -
                                                static_cast<std::uint16_t>(MapView_Origin[1]) -
                                                static_cast<std::uint16_t>(MapView_Origin[0]) + 2);
    sprite->draw_key = key;
    sprite = rv::Current<records::SpriteHead>();
    const auto layer = static_cast<std::int16_t>(sprite->draw_key);
    if (layer < 0 || layer > 0x36) {   // off the map's layers: hidden
        sprite->flags |= 0x80;
        return;
    }
    // DIV-0071's comparison mode (BOF3X_LAYERING=2, off during every fuzz): the key one layer later.
    const int later = layering::g_mode == layering::kLayerLater && Draw_OtSlot == 6 && layer < 0x36;
    sprite->draw_key = static_cast<unsigned>((layer + later) & 0xFF) << 8;
    unsigned low;
    if (Draw_OtSlot == 4) {
        sprite = rv::Current<records::SpriteHead>();
        low = static_cast<unsigned>(move_script::Long(rv::Bytes(sprite) + (Draw_SortOnX ? 0x34 : 0x38)) >> 15) & 0xFF;
    } else {
        sprite = rv::Current<records::SpriteHead>();
        low = ((sprite->height_whole ^ 0xF01Fu) >> 5) & 0xFF;
    }
    sprite->draw_key = sprite->draw_key | low;
    sprite_screen::g.inherit_key();

    sprite = rv::Current<records::SpriteHead>();
    short vertex[4];
    vertex[0] = static_cast<short>((sprite->x >> 9) - 0x4000);
    vertex[1] = static_cast<short>((sprite->z >> 9) - 0x4000);
    vertex[2] = static_cast<short>(-(static_cast<std::int16_t>(sprite->height_whole) / 2));
    unsigned long screen[2];
    long depth_cue;
    const long depth = sprite_screen::g.project(vertex, screen, &depth_cue);
    rv::Current<records::SpriteHead>()->depth = depth;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->screen_xf, static_cast<std::int32_t>(ThroughX87(static_cast<std::uint32_t>(screen[0]))));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_x, Ftol16(static_cast<std::uint32_t>(screen[0])));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->screen_yf, static_cast<std::int32_t>(ThroughX87(static_cast<std::uint32_t>(screen[1]))));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_y, Ftol16(static_cast<std::uint32_t>(screen[1])));

    const unsigned char n = Sprite_DrawListCount;
    if (n >= 0x28) return;
    unsigned char* const drawn = Sprite_Current;
    Sprite_DrawListCount = static_cast<unsigned char>(n + 1);
    Sprite_DrawList[n] = drawn;
    band_census::Seen(drawn);   // BOF3X_BANDLOG, an instrument: reads only (band_census.h)
    if (drawn[0] & 8) sprite_screen::g.dropped(0);
    if (sound_sp::g_position) sound_sp::UpdatePositional();   // DIVERGENCE DIV-0171 (PSX US 0x80161454)
}

// original 0x5890E0 (PSX 0x8014D3D4): a sprite with +0x24 bit 7 goes onto the overlay list, not
// the draw list (PSX FUN_8014D3D4).
extern "C" void __cdecl Sprite_QueueOverlay(void) {
    const unsigned char n = Sprite_OverlayListCount;
    if (n >= 30) return;
    Sprite_OverlayListCount = static_cast<unsigned char>(n + 1);
    Sprite_OverlayList[n] = Sprite_Current;
    if (rv::Current<records::SpriteHead>()->flags & 8) sprite_screen::g.dropped(0);
    if (sound_sp::g_position) sound_sp::UpdatePositional();   // DIVERGENCE DIV-0171 (PSX US 0x80161454)
}

// original 0x589110: Sprite_CopyFrames(animation, set, size); when +0x4B is
// already the animation (its low byte), Sprite_ScriptStart(u16 +0x58 - 2),
// the script restarted where it was; else +0x4B = the animation and
// Sprite_ScriptStart(0).
FH_EXPORT void __cdecl Sprite_PoseFromSet(unsigned animation, unsigned char* set, unsigned size) {
    field_hidden::g.copy_frames(animation, set, size);
    records::SpriteObject* const s = rv::Current<records::SpriteObject>();
    if (s->animation == static_cast<unsigned char>(animation)) {
        field_hidden::g.script_start(static_cast<std::uint16_t>(field_hidden::Word(rv::Bytes(s) + 0x58) - 2));
        return;
    }
    s->animation = static_cast<unsigned char>(animation);
    field_hidden::g.script_start(0);
}

// original 0x589160: Sprite_SetFrameQueueUpload(frame), then (size & 0xFFFF)
// bytes from the sprite's +0x50 copied to buffer + its +5 * that size, one by
// one, and +0x50 pointed at the copy.
//
// As the original has it: Sprite_Current and its +0x50 are re-read for every
// byte, so a copy over either is seen; +5 is unchecked.
FH_EXPORT void __cdecl Sprite_CopyFrames(unsigned frame, unsigned char* buffer, unsigned size) {
    field_hidden::g.frame_upload(frame);
    const unsigned n = size & 0xFFFF;
    unsigned char* const to = buffer + static_cast<std::uint32_t>(Sprite_Current[5]) * n;
    for (unsigned i = 0; i < n; ++i) to[i] = PtrIn(Sprite_Current + 0x50)[i];
    rv::Current<records::SpriteObject>()->script = static_cast<std::int32_t>(Low32(to));
}

// original 0x5891C0 (PSX 0x8014D558): Sprite_CopyFrames(animation, buffer, size), +0x4B = the
// animation, Sprite_ScriptStart(position) - the dword as given.
FH_EXPORT void __cdecl Sprite_AnimFromSet(unsigned animation, unsigned position, unsigned char* buffer, unsigned size) {
    field_hidden::g.copy_frames(animation, buffer, size);
    rv::Current<records::SpriteObject>()->animation = static_cast<unsigned char>(animation);
    field_hidden::g.script_start(position);
}

// original 0x5891F0 (PSX 0x8014D5AC): Sprite_SetAnimationAt(animation, 0).
// The original pushes its whole argument dword; the callee reads its low byte.
extern "C" void __cdecl Sprite_SetAnimation(unsigned char animation) {
    sprite_pose::g.set_animation_at(animation, 0);
}

extern "C" void __cdecl Sprite_SetAnimationAt(unsigned char animation, unsigned short start) {
    if (PoseLogOn()) PoseLog("SetAnimationAt in ", Sprite_Current, animation, start);
    if (animation & 0x80) {
        records::SpriteObject* sprite = rv::Current<records::SpriteObject>();
        if (!(sprite->draw_flags & 2)) {
            const unsigned row = (AreaW0b_PartySet & 0x7Fu) * 3u;
            sprite->angle[0] = static_cast<unsigned char>(MoveScript_EffectArg[0xc + row + sprite->column]);
            sprite = rv::Current<records::SpriteObject>();
            sprite->angle[1] = sprite->tex_row;
            sprite = rv::Current<records::SpriteObject>();
            sprite->draw_flags = static_cast<unsigned char>(sprite->draw_flags ^ 3);
            sprite = rv::Current<records::SpriteObject>();
        }
        const std::uint32_t member = static_cast<std::uint32_t>(static_cast<std::int32_t>(sprite->angle[0]));
        const unsigned char* const record = Offset(sprite_pose::Ptr(::bof3::Img(sprite_pose::at::kMemberAnimations) + member * 4u), (animation & 0x7Fu) * 4u);
        sprite_pose::g.set_bank(move_script::Word(record));
        rv::Current<records::SpriteObject>()->animation = record[2];
        rv::Current<records::SpriteObject>()->flip = record[3];
    } else {
        records::SpriteObject* sprite = rv::Current<records::SpriteObject>();
        const unsigned char flags = sprite->draw_flags;
        if (flags & 2) {
            const unsigned char kept = static_cast<unsigned char>(flags & 0xFC);
            const std::int32_t slot = static_cast<std::int32_t>(sprite->angle[1]) / 80;
            sprite_pose::g.member_sprite(rv::Bytes(sprite)[0x64], static_cast<std::uint32_t>(slot));
            sprite = rv::Current<records::SpriteObject>();
            sprite->draw_flags = static_cast<unsigned char>(sprite->draw_flags | kept);
            sprite = rv::Current<records::SpriteObject>();
        }
        sprite->animation = animation;
    }
    records::SpriteObject* const sprite = rv::Current<records::SpriteObject>();
    if (PoseLogOn()) PoseLog("SetAnimationAt mid", sprite, animation, start);
    if (sprite->draw_flags & 1) {
        sprite_pose::g.queue_upload(sprite->animation);
        sprite_pose::g.script_start(start);
        return;
    }
    const unsigned char* const offsets = sprite_pose::Ptr(::bof3::Img(sprite_pose::at::kFrameOffsets));
    const unsigned char* const table = Offset(offsets, static_cast<std::uint32_t>(move_script::Long(offsets + sprite->column * 4u)));
    sprite->script = static_cast<std::int32_t>(Low32(Offset(table, move_script::Word(table + sprite->animation * 2u))));
    sprite_pose::g.script_start(start);
}

// original 0x589330 (PSX 0x8014D7C8): 1 if the current sprite already plays
// `animation` (nothing done), else Sprite_SetAnimation and 0. The original's
// result is al; the rest of eax is whatever the path left there (the argument's
// upper bytes, or the callee's), which no caller reads as a number
// (docs/sprite-pose.md §2).
extern "C" unsigned char __cdecl Sprite_EnsureAnimation(unsigned char animation) {
    if (rv::Current<records::SpriteObject>()->animation == animation) return 1;
    sprite_pose::g.set_animation(animation);
    return 0;
}

// original 0x589350 (PSX 0x8014D804). Begins the script kScript points at, from byte `position`
// of its steps: reads the step count out of the header, moves kScript past the
// header, finds the frame words behind the last step, and takes the first step.
//
// As the original has it: no check for a null script, and `position` is
// neither checked against the step count nor made even.
extern "C" void __cdecl Sprite_ScriptStart(unsigned short position) {
    unsigned char* const object = Sprite_Current;
    const unsigned char* script = Get<const unsigned char*>(object, kScript);
    object[kSteps] = script[1];
    script += 2;
    Put<const unsigned char*>(object, kScript, script);
    Put<std::uint16_t>(object, kPosition, position);
    Put<const unsigned char*>(object, kFrames, script + object[kSteps] * 2);
    Sprite_ScriptStep();
}

// original 0x5893A0 (PSX 0x8014D86C). One tick of a script that repeats: counts the hold down,
// and when it runs out takes the next step - from the top again after the last
// one. Returns 1 when it wrapped round or followed a jump, else 0.
extern "C" unsigned char __cdecl Sprite_ScriptTick(void) {
    unsigned char* const object = Sprite_Current;
    if (--object[kHold] != 0) return 0;
    const std::uint16_t position = Get<std::uint16_t>(object, kPosition);
    if (position >> 1 == object[kSteps]) {
        Put<std::uint16_t>(object, kPosition, 0);
        Sprite_ScriptStep();
        return 1;
    }
    return NextStep(object, position);
}

// original 0x589410 (PSX 0x8014D9E0). One tick of a script that plays once: as above, but past
// the last step it takes no step - it sets the hold back to 1, so that every
// later tick lands here again - and returns 1.
extern "C" unsigned char __cdecl Sprite_ScriptTickOnce(void) {
    unsigned char* const object = Sprite_Current;
    if (--object[kHold] != 0) return 0;
    const std::uint16_t position = Get<std::uint16_t>(object, kPosition);
    if (position >> 1 == object[kSteps]) {
        object[kHold] = 1;
        return 1;
    }
    return NextStep(object, position);
}

// original 0x589470 (PSX 0x8014DA8C). Takes the step at kPosition: its first byte is the hold,
// its second picks the frame word. An object with no script is left alone.
//
// As the original has it: the position is 16 bits and wraps, between the two
// reads too; nothing checks it, or the frame number, against anything.
extern "C" void __cdecl Sprite_ScriptStep(void) {
    unsigned char* const object = Sprite_Current;
    const auto* script = Get<const unsigned char*>(object, kScript);
    if (script == nullptr) return;
    std::uint16_t position = Get<std::uint16_t>(object, kPosition);
    object[kHold] = script[position];
    ++position;
    const std::uint8_t frame = script[position];
    ++position;
    Put<std::uint16_t>(object, kPosition, position);
    Put<std::uint16_t>(object, kFrame, Get<const unsigned short*>(object, kFrames)[frame]);
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
    records::SpriteObject* const sprite = rv::Current<records::SpriteObject>();
    const unsigned char* const data = move_script::At(static_cast<std::uint32_t>(static_cast<std::int32_t>(sprite->anim_data)));
    const unsigned char* const entry = Offset(data, static_cast<std::uint32_t>(move_script::Long(data + 0xC)) + (frame & 0xFFu) * 3u);
    const unsigned char* const images = Offset(data, static_cast<std::uint32_t>(move_script::Long(data)));
    const unsigned char* const words = Offset(data, static_cast<std::uint32_t>(move_script::Long(data + 8)));
    if (!(sprite->flags & 2)) {
        const unsigned slot = sprite->tex_slot;
        const unsigned count = Gfx_UploadQueueCount;
        if (slot > 0x10) {
            move_script::SetWord(move_script::At(::bof3::Img(Gfx_UploadQueueX_addr) + count * 2u), (slot - 0x10u) << 6);
            move_script::SetWord(move_script::At(::bof3::Img(Gfx_UploadQueueY_addr) + count * 2u), sprite->tex_row + 0x100u);
        } else {
            move_script::SetWord(move_script::At(::bof3::Img(Gfx_UploadQueueX_addr) + count * 2u), slot << 6);
            move_script::SetWord(move_script::At(::bof3::Img(Gfx_UploadQueueY_addr) + count * 2u), sprite->tex_row);
        }
        const unsigned char* const image = Offset(images, static_cast<std::uint32_t>(move_script::Long(images + entry[0] * 4u)));
        move_script::SetLong(move_script::At(::bof3::Img(Gfx_UploadQueueRecord_addr) + count * 4u), static_cast<std::int32_t>(Low32(image)));
        Gfx_UploadQueueCount = static_cast<unsigned char>(Gfx_UploadQueueCount + 1);
    }
    if (PoseLogOn()) PoseLog("QueueUpload entry ", sprite, frame, entry[0] | entry[1] << 8 | entry[2] << 16);
    sprite->flip = entry[2];
    unsigned char* const script = Offset(words, move_script::Word(words + entry[1] * 2u));
    rv::Current<records::SpriteObject>()->script = static_cast<std::int32_t>(Low32(script));
    return script;
}

extern "C" unsigned char __cdecl Sprite_SetAnimationBank(unsigned short bank) {
    const unsigned count = static_cast<std::uint32_t>(move_script::Long(move_script::At(::bof3::Img(sprite_pose::at::kBankCount)))) & 0xFFu;
    const unsigned char* const table = sprite_pose::Ptr(::bof3::Img(sprite_pose::at::kBankTable));
    unsigned i = 0;
    for (;; ++i) {
        if (i >= count) return 1;
        if (move_script::Word(table + i * 8u) == bank) break;
    }
    rv::Current<records::SpriteObject>()->tex_slot = table[i * 8u + 4];
    rv::Current<records::SpriteObject>()->tex_row = sprite_pose::Ptr(::bof3::Img(sprite_pose::at::kBankTable))[i * 8u + 5];
    rv::Current<records::SpriteObject>()->clut = sprite_pose::Ptr(::bof3::Img(sprite_pose::at::kBankTable))[i * 8u + 2];
    rv::Current<records::SpriteObject>()->clut_mode = sprite_pose::Ptr(::bof3::Img(sprite_pose::at::kBankTable))[i * 8u + 6];
    const unsigned column = sprite_pose::Ptr(::bof3::Img(sprite_pose::at::kBankTable))[i * 8u + 7];
    rv::Current<records::SpriteObject>()->column = column;
    rv::Current<records::SpriteObject>()->reach = static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int32_t>(rv::Current<records::SpriteObject>()->reach)) & 0xFFFFFF00u);
    unsigned char* sprite = Sprite_Current;
    const unsigned char first = move_script::At(::bof3::Img(sprite_pose::at::kBankBytes))[bank * 2u];
    if (sprite_pose::g_reach_word) rv::Store<std::uint32_t>(rv::As<records::SpriteHead>(sprite)->reach, first);
    else sprite[0x70] = static_cast<unsigned char>(sprite[0x70] | first);
    const unsigned char second = move_script::At(::bof3::Img(sprite_pose::at::kBankBytes))[bank * 2u + 1];
    rv::Current<records::SpriteObject>()->key_class = second;
    return 0;
}

// original 0x589660. The number of the first sprite object of type 0x0A, other
// than the current one, that has the current one within its reach - the reach
// is the found object's, from the record at its +0x54 - or 0xFF. Objects are
// numbered 0..29 in Sprite_Objects and 30..33 in Sprite_ObjectsExtra, the
// numbering 0x57C0A0 uses too.
//
// As the original has it: x and y differences are 32-bit and wrap, and an
// absolute difference of 0x80000000 counts as within any reach; z is 16-bit
// and its limit is a fixed 0x300.
extern "C" unsigned char __cdecl Sprite_FindNearby(void) {
    const unsigned char* const current = Sprite_Current;
    for (unsigned i = 0; i < kObjectsSpriteFind; ++i)
        if (Qualifies(rv::Bytes(&Sprite_Objects[i]), current)) return static_cast<unsigned char>(i);
    for (unsigned i = 0; i < kExtraSpriteFind; ++i)
        if (Qualifies(rv::Bytes(&Sprite_ObjectsExtra[i]), current)) return static_cast<unsigned char>(i + kObjectsSpriteFind);
    return 0xFF;
}

extern "C" void __cdecl Sprite_InheritDrawKey(void) {
    if (!sprite_find::g_inherit_live) return InheritDrawKey();
    // The one store either makes is the current object's +0x32.
    unsigned char* const current = Sprite_Current;
    const std::uint16_t before = GetSpriteFind<std::uint16_t>(current, kDrawKey);
    sprite_find::g_inherit_live();
    const std::uint16_t theirs = GetSpriteFind<std::uint16_t>(current, kDrawKey);
    PutSpriteFind<std::uint16_t>(current, kDrawKey, before);
    InheritDrawKey();
    const std::uint16_t ours = GetSpriteFind<std::uint16_t>(current, kDrawKey);
    LiveCounts& c = g_live_inherit;
    ++c.calls;
    if ((current[kFlags24] & 0x20) && (current[kHandle] & 0x80)) ++c.bit7;
    if (theirs != ours && ++c.mismatches <= 8)
        bof3::Log("shadow      Sprite_InheritDrawKey live MISMATCH: call %u, attached %u handle %02X: +0x32 %04X / %04X",
                  c.calls, (current[kFlags24] & 0x20) ? 1u : 0u, current[kHandle], theirs, ours);
    LiveReport("Sprite_InheritDrawKey", c);
}

void Sprite_Inject() {
    BOF3_INJECT(Sprite_UpdateScreenSlot);
    BOF3_INJECT(Sprite_UpdateScreen);
    BOF3_INJECT(Sprite_QueueOverlay);
    BOF3_INJECT(Sprite_PoseFromSet);
    BOF3_INJECT(Sprite_CopyFrames);
    BOF3_INJECT(Sprite_AnimFromSet);
    BOF3_INJECT(Sprite_SetAnimation);
    BOF3_INJECT(Sprite_SetAnimationAt);
    BOF3_INJECT(Sprite_EnsureAnimation);
    BOF3_INJECT(Sprite_ScriptStart);
    BOF3_INJECT(Sprite_ScriptTick);
    BOF3_INJECT(Sprite_ScriptTickOnce);
    BOF3_INJECT(Sprite_ScriptStep);
    BOF3_INJECT(Sprite_SetFrameQueueUpload);
    BOF3_INJECT(Sprite_SetAnimationBank);
    BOF3_INJECT(Sprite_FindNearby);
    BOF3_INJECT(Sprite_InheritDrawKey);
}
