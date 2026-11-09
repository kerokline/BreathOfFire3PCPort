// Moved here by tools/move_funcs.py from event_ops.cpp, event_script.cpp (docs/source-layout-round.md).
#include "game/field/event_op.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "game/event_ops.h"
#include "game/event_ops_callees.h"
#include "game/event_script.h"
#include "game/event_script.h"   // DIV-0191: g_mountain_passes
#include "game/event_script_callees.h"
#include "game/faerie_ring.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/wide_staging.h"
#include "hook/detour.h"
#include "hook/log.h"

using namespace event_ops;

namespace event_ops {

using move_script::At;

using move_script::SetWord;

}  // namespace event_ops

namespace {

// The count and the bank word (event_script.cpp's terms).
short Count() {
    short v;
    std::memcpy(&v, At(::bof3::Img(event_ops::at::kScratch_addr)), sizeof v);
    return v;
}

void SetCount(unsigned v) { event_ops::at::kScratch.w_00 = v; }

unsigned short Bank() { return event_ops::at::kScratch2; }

unsigned char* Object(int n) { return rv::Bytes(&Sprite_Objects[n]); }

// A placement's x or z (event_script.cpp): the whole part from `whole`, the
// half (0x8000) if `half` is non-zero.
std::uint32_t Coordinate(unsigned char half, unsigned char whole) {
    return (half ? 0x8000u : 0u) | static_cast<std::uint32_t>(whole) << 16;
}

unsigned char* AtEventScript(std::uint32_t a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

unsigned char* Extra(unsigned n) { return rv::Bytes(&Sprite_ObjectsExtra[n]); }

}  // namespace

// original 0x57B100 (PSX 0x801A5B88): a party record's four bytes, in the order +1 +2 +3 +0
// (PSX FUN_801A5B88). Callers push the index with the register's other bits.
extern "C" void __cdecl PartyRecord_Clear(unsigned index) {
    unsigned char* const r = MoveScript_PartyRecords + (index & 0xFFu) * 16u;
    r[1] = 0;
    r[2] = 0;
    r[3] = 0;
    r[0] = 0;
}

extern "C" void __cdecl EventOp_Bx(const unsigned char* op) {
    const unsigned char* const descriptor = Area_Descriptors[Game_AreaNumber];
    unsigned char* const table = AtEventScript(Dword(descriptor + 8));   // a 32-bit word
    unsigned char* const entry = table + op[9] * 8u;
    unsigned char* const o = Extra(op[0xB]);
    Sprite_Current = o;
    Field_ActiveMember = Extra(op[0xB]);
    o[8] = op[0] & 0xF;
    event_script::g.reset();
    event_script::g.clear_record(op[0xB]);
    event_script::g.init_entry(entry);
    rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->x, Coordinate(op[1], op[2]));
    rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->z, Coordinate(op[3], op[4]));
    const long elevation = event_script::g.elevation(static_cast<long>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->x)),
                                       static_cast<long>(static_cast<std::uint32_t>(rv::Current<records::SpriteObject>()->z)));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(elevation));
    Sprite_Current[1] = op[5];
    Sprite_Current[2] = op[7];
    Sprite_Current[6] = static_cast<unsigned char>((op[0] >> 4) - 1);
    rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->work_20, 0);
    rv::Current<records::SpriteObject>()->work_1c = 0;
    rv::Current<records::SpriteObject>()->work_18 = 0;
    {
        unsigned char* const sc = Sprite_Current;
        unsigned char* const source = AtEventScript(Dword(sc + 0x54));   // a 32-bit word
        if (event_script::g_reach_word) rv::Store<std::uint32_t>(rv::As<records::SpriteHead>(sc)->reach, source[2]);
        else sc[0x70] = source[2];
    }
    rv::As<records::SpriteObject>(Extra(op[0xB]))->best_score = 0;
    rv::As<records::SpriteObject>(Extra(op[0xB]))->target_z = 0;
    rv::As<records::SpriteObject>(Extra(op[0xB]))->target_x = 0;
    Extra(op[0xB])[0x83] = op[0xA];
    Extra(op[0xB])[0x84] = op[6];
    rv::Store<unsigned>(rv::As<records::SpriteObject>(Extra(op[0xB]))->trigger_word, static_cast<unsigned>(op[0xC]) << 8 | op[0xD]);
    Extra(op[0xB])[0xA0] = 0x7F;
    event_script::g.set_flags(op + 8);
    Sprite_Current[0x5D] = 0x80;
    Sprite_Current[0x5F] = 0x80;
    Sprite_Current[0x5E] = 0x80;
    Sprite_Current[0x5C] = 2;
}

// original 0x57B310: op 5x, 18 bytes - places Sprite_Objects[count] (PSX
// FUN_801A7F60, store for store). op[1]:op[2] the animation bank, op[3] /
// op[4] words +0x98 / +0x9A, op[5..8] x and z, op[9] +1, op[0xA] the speed
// index +0x84, op[0xB] +2, op[0xC] the flags (EventObj_SetFlags),
// op[0xD]:op[0xE] word +0x88, op[0xF] +0x83, op[0x10] / op[0x11] +0x18 / +0x1C
// as dwords; op[0]'s nibbles the direction +8 and the kind +6; +0x5C is 2.
// As the original has it: the object's own bytes are written by the count
// read again after each call, the sprite's through Sprite_Current read again
// (a callee that moved either moves what follows), and nothing past the
// count's test of 30 checks it.
extern "C" void __cdecl EventOp_5x(const unsigned char* op) {
    if (Count() >= 30) return;
    unsigned char* const o = Object(Count());
    Sprite_Current = o;
    Field_ActiveMember = o;
    event_ops::at::kScratch2 = static_cast<unsigned>(op[1]) << 8 | op[2];
    g.reset();
    g.set_bank(Bank());
    Sprite_Current[0] = 1;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(Coordinate(op[5], op[6])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_x, rv::Current<records::SpriteObject>()->x);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(Coordinate(op[7], op[8])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_z, rv::Current<records::SpriteObject>()->z);
    rv::As<records::SpriteObject>(Object(Count()))->best_score = 0;
    const long elevation = g.elevation(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(elevation));
    Sprite_Current[1] = op[9];
    Sprite_Current[2] = op[0xB];
    Sprite_Current[8] = op[0] & 0xF;
    Sprite_Current[6] = op[0] >> 4;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_18, op[0x10]);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->work_1c, op[0x11]);
    unsigned char* const n = Object(Count());
    rv::As<records::SpriteObject>(n)->half_w = op[3];
    rv::As<records::SpriteObject>(n)->half_d = op[4];
    rv::As<records::SpriteObject>(n)->trigger_word = static_cast<unsigned>(op[0xD]) << 8 | op[0xE];
    n[0x83] = op[0xF];
    n[0x84] = op[0xA];
    n[0xA0] = 0x7F;
    g.set_flags(op + 0xC);
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5C] = 2;
    g.face();
    SetCount(static_cast<unsigned>(Count() + 1));
}

// original 0x57B4E0: op Cx, 2 bytes - the kind-2 object's kind is the op's
// high nibble (always 0xC), then Kind2_Place(op[1]) (PSX FUN_801A82B8).
extern "C" void __cdecl EventOp_Cx(const unsigned char* op) {
    Sprite_Kind2.type = op[0] >> 4;
    event_script::g.kind2_place(op[1]);
}

// original 0x57B500: op Dx, 4 bytes (PSX FUN_801A82F0) - with op[1],
// Field_ActiveMember +0x86 = op[3]; without, its word +0x88 = op[2]:op[3].
extern "C" void __cdecl EventOp_Dx(const unsigned char* op) {
    if (op[1] != 0) {
        Field_ActiveMember[0x86] = op[3];
        return;
    }
    SetWord(Field_ActiveMember + 0x88, static_cast<unsigned>(op[2]) << 8 | op[3]);
}

// original 0x57B530 (PSX 0x801A8338): op 9x, 13 bytes - places Sprite_Objects[count] as an
// object of state 6 (PSX FUN_801A8338, store for store). op[1]:op[2] the
// bank, op[3] bit 7 sets +0 bit 0x20 and its low nibble is +0x5C, op[4..7] x
// and z, op[8] +5, op[9]:op[0xA] the dword +0x18, op[0xB] the flags, op[0xC]
// +0xB; +0x2A is bit 4 of +7. Then bit +5 of the bits at 0x9040CC: clear -
// Sprite_SetAnimation(0); set - with op[0xC] bit 0 the object is switched
// off (+0 = 0) and the count NOT advanced, else a sprite on a whole cell
// stamps 0x10 into the area map at its cell (0x579F00) and
// Sprite_SetAnimation(1).
// As the original has it: the count and Sprite_Current are read again after
// each call, as in EventOp_5x; the flag's byte is +5 >> 3, a whole byte's
// worth (32 bytes of bits).
extern "C" void __cdecl EventOp_9x(const unsigned char* op) {
    if (Count() >= 30) return;
    unsigned char* const o = Object(Count());
    Sprite_Current = o;
    Field_ActiveMember = o;
    event_ops::at::kScratch2 = static_cast<unsigned>(op[1]) << 8 | op[2];
    g.reset();
    g.set_bank(Bank());
    Sprite_Current[0] = 1;
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->x, static_cast<std::int32_t>(Coordinate(op[4], op[5])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_x, rv::Current<records::SpriteObject>()->x);
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->z, static_cast<std::int32_t>(Coordinate(op[6], op[7])));
    rv::Store<std::int32_t>(rv::As<records::SpriteObject>(Object(Count()))->target_z, rv::Current<records::SpriteObject>()->z);
    rv::As<records::SpriteObject>(Object(Count()))->best_score = 0;
    const long elevation = g.elevation(rv::Current<records::SpriteObject>()->x, rv::Current<records::SpriteObject>()->z);
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->height_whole, static_cast<unsigned>(elevation));
    Sprite_Current[1] = 6;
    Sprite_Current[2] = 0;
    Sprite_Current[8] = op[0] & 0xF;
    Sprite_Current[6] = op[0] >> 4;
    {
        unsigned char* const n = Object(Count());
        n[0x84] = 0;
        n[0xA0] = 0x7F;
    }
    g.set_flags(op + 0xB);
    if (op[3] & 0x80) Sprite_Current[0] |= 0x20;
    Sprite_Current[0x5D] = 0;
    Sprite_Current[0x5F] = 0;
    Sprite_Current[0x5E] = 0;
    Sprite_Current[0x5C] = op[3] & 0xF;
    Sprite_Current[0x29] = 6;
    {
        unsigned char* const sc = Sprite_Current;
        rv::As<records::SpriteObject>(Object(Count()))->trigger_word = 0;
        rv::As<records::SpriteObject>(sc)->work_18 = static_cast<std::int32_t>(static_cast<unsigned>(op[9]) << 8 | op[0xA]);
    }
    Sprite_Current[5] = op[8];
    Sprite_Current[0xB] = op[0xC];
    Sprite_Current[0x2A] = (Sprite_Current[7] >> 4) & 1;
    unsigned char* const sc = Sprite_Current;
    const unsigned bit = sc[5];
    if (At(::bof3::Img(event_ops::at::kPlacedFlags))[bit >> 3] & (1u << (bit & 7))) {
        if (op[0xC] & 1) {
            sc[0] = 0;
            return;
        }
        if (rv::As<records::SpriteHead>(sc)->x_frac == 0 && rv::As<records::SpriteHead>(sc)->z_frac == 0) g.set_cell(rv::As<records::SpriteHead>(sc)->x_whole, rv::As<records::SpriteHead>(sc)->z_whole, 0x10);
        g.set_animation(1);
    } else {
        g.set_animation(0);
    }
    SetCount(static_cast<unsigned>(Count() + 1));
}

void EventOp_Inject() {
    BOF3_INJECT(PartyRecord_Clear);
    BOF3_INJECT(EventOp_Bx);
    BOF3_INJECT(EventOp_5x);
    BOF3_INJECT(EventOp_Cx);
    BOF3_INJECT(EventOp_Dx);
    BOF3_INJECT(EventOp_9x);
}
