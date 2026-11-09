// Moved here by tools/move_funcs.py from event_script.cpp, field_frame.cpp, field_o.cpp, map_field_objects.cpp, move_cmds.cpp, move_script.cpp, psx_gte_matrix.cpp, rest_2b.cpp, scena_sx.cpp, scena_sx2.cpp, sprite_find.cpp, sprite_pose.cpp, sprite_records.cpp (docs/source-layout-round.md).
#include "game/field/event_cond.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <windows.h>

#include "bof3/symbols.gen.h"
#include "common/asm_name.h"
#include "game/cheats.h"
#include "game/event_script.h"
#include "game/event_script_callees.h"
#include "game/faerie_ring.h"
#include "game/field/event_cond_callees.h"
#include "game/field_frame.h"
#include "game/field_o.h"
#include "game/field_o_callees.h"
#include "game/labels.h"
#include "game/lang_layout.h"
#include "game/list_title.h"
#include "game/map_field_objects.h"
#include "game/map_field_objects_callees.h"
#include "game/map_rim.h"
#include "game/move_cmds_callees.h"
#include "game/move_script.h"
#include "game/move_script_bytes.h"
#include "game/psx_gte_matrix.h"
#include "game/rdata_consts.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/rest_2b.h"
#include "game/rest_2b_callees.h"
#include "game/scena_sx2_callees.h"
#include "game/scena_sx_callees.h"
#include "game/scenario_harness.h"
#include "game/sprite_find.h"
#include "game/sprite_pose.h"
#include "game/sprite_records.h"
#include "game/wide_staging.h"
#include "game/widescreen.h"
#include "game/yes_no_layout.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = event_cond::at;

}  // namespace

namespace {

using namespace move_script;

using namespace map_field_objects;

using namespace move_cmds;

}  // namespace

using namespace sprite_pose;

namespace {

std::uint32_t Dword(const unsigned char* p) {
    std::uint32_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}

constexpr unsigned kObjectBytes = 0xA4, kObjects = 30, kExtra = 4, kMemberBytes = 0x14C;

// The record a type-0x0A sprite points at from +0x54 (bytes 1 and 3 read here).
const unsigned char* Record54(const unsigned char* sprite) {
    std::uint32_t p;
    std::memcpy(&p, sprite + 0x54, sizeof p);
    return At(p);
}

void BandNote(unsigned char* sprite) {
    if (field_frame::g_band_count < sizeof field_frame::g_band / sizeof field_frame::g_band[0]) field_frame::g_band[field_frame::g_band_count++].sprite = sprite;
}

using Handler = void (__cdecl*)(unsigned char*);

void CallHandler(unsigned char* object) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Field_ObjectHandlers[object[1]]))(object);
}

// `fld dword` then `fstp dword`: exact, except that a signalling NaN comes
// back quiet (bit 22 set). As sprite_screen.cpp's.
std::uint32_t ThroughX87(std::uint32_t bits) {
    const bool nan = (bits & 0x7F800000u) == 0x7F800000u && (bits & 0x007FFFFFu) != 0;
    return nan ? bits | 0x00400000u : bits;
}

// `fld dword` then the CRT's _ftol 0x5B9550: truncation through a 64-bit
// fistp, whose NaN and out-of-range answer is the integer indefinite
// 0x8000000000000000 - so 0 in the low word the caller keeps. As
// sprite_screen.cpp's.
std::uint16_t Ftol16(std::uint32_t bits) {
    float v;
    std::memcpy(&v, &bits, sizeof v);
    if (!(v > -9.2233720368547758e18f && v < 9.2233720368547758e18f)) return 0;   // NaN included
    return static_cast<std::uint16_t>(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)));
}

constexpr auto& AtFieldO = move_script::At;

using move_script::Long;

using move_script::SetLong;

using U = std::uint32_t;

unsigned char* Record(unsigned id) { return AtFieldO(::bof3::Img(CharacterRecords_addr) + (id & 0xFF) * field_o::at::kRecordStride); }

unsigned char* Sc() { return Sprite_Current; }

// A signed 16-bit word, as `movsx` / a 16-bit compare reads it.
std::int32_t S16(unsigned v) { return static_cast<std::int16_t>(static_cast<std::uint16_t>(v)); }

// `cdq` / `idiv`: the original's signed division. A zero divisor is the
// original's divide fault; ours aborts with a message (round nine's rule).
std::int32_t Idiv(std::int32_t dividend, std::int32_t divisor, const char* who) {
    if (divisor == 0) bof3::Fatal("%s: a division by 0 (the original raises the divide fault here)", who);
    if (dividend == INT32_MIN && divisor == -1) bof3::Fatal("%s: INT_MIN / -1 (the original faults)", who);
    return dividend / divisor;
}

// A handler read in place from a .data table: an entry outside .text is not
// code - the original jumps there; ours aborts (round nine's rule). While the
// fuzz runs, the table holds the harness's recorders, which live outside .text.
U CodeAt(U table, unsigned index, const char* who) {
    const U cell = table + 4u * index;
    const U entry = static_cast<U>(Long(AtFieldO(cell)));
    // A code word: hosted a VA in BOF3.exe's .text, in bof3x.exe a function of ours
    // the bind table names (common/image_base.h IsCodeWord); not the trap, not data.
    if (!scenario_harness::g_active && !::bof3::IsCodeWord(entry))
        bof3::Fatal("%s: index %u reads 0x%X at 0x%X (VA 0x%X), not code - past its table (the original jumps there)", who,
                    index, (unsigned)entry, (unsigned)cell, (unsigned)::bof3::VaOf(cell));
    return entry;
}

// 16 / the object's speed, a byte: the frames of a step (idiv).
unsigned char StepFrames(const unsigned char* object, const char* who) {
    return static_cast<unsigned char>(Idiv(0x10, Field_MoveSpeeds[object[4]], who));
}

std::uint32_t AddressMoveCmds(const volatile void* p) { return Low32(p); }

unsigned char* Counter(const unsigned char* op) { return Scratch() - 8 + ((op[0] >> 2) & 3); }

template <class T> void PutRest2b(unsigned char* p, unsigned offset, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(p + offset, &w, sizeof w);
    } else {
        std::memcpy(p + offset, &v, sizeof v);
    }
}

}  // namespace

// original 0x57CE10 (PSX 0x8015D508): an effect record of kind 6 - Effect_FindFree's slot n;
// none (0xFF): nothing. Else record n: +0 = 1, +5 = 6, +6 = kind, +0xC / +0x10
// = a / b sign-extended to dwords, +0x2E / +0x30 = the words x / z. al = n.
// Effect_FindFree answers 0..19 or 0xFF; past 19 ours aborts where the
// original would write past Effect_Objects.
// BOF3X_EFFECTLOG=1 logs every spawn (instrumentation; docs/input-script.md).
static bool EffectLogOn() {
    static int on = -1;
    if (on < 0) {
        char v[8];
        on = GetEnvironmentVariableA("BOF3X_EFFECTLOG", v, sizeof v) != 0 && v[0] == '1';
    }
    return on == 1;
}

namespace {

// The record views (game/record_views.h): rv::As<records::SpriteObject>(p)
// views a
// sprite-family pointer; it is used below +0x80 only (the sprite head every
// family shares) unless the pointer is a Sprite_Objects / Sprite_ObjectsExtra /
// Sprite_Kind2 record. rv::As<records::PartyMember>(p) is given party members
// (ObjTrio, Field_State).
using move_script::SetWord;

using move_script::Word;

// A signed byte's value.
int S8(unsigned char b) { return static_cast<signed char>(b); }

// C's truncating division by 360 of a value times 4096 - the imul
// 0xB60B60B7 / add / sar 8 / add sign idiom, exact for every s32.
std::int32_t Degrees(std::int32_t v) { return (v << 12) / 360; }

constexpr auto& AtScenaSx2 = move_script::At;

constexpr auto& LongScenaSx2 = move_script::Long;

std::uint32_t UScenaSx2(std::int32_t v) { return static_cast<std::uint32_t>(v); }

// One turn step of a camera angle word (Camera_TurnStep's and
// Camera_TurnStepFB's shared shape, two constants apart): with a step (s8)
// the word plus the step, then while (the dword & 0xFFF) is short of the
// target (as s16, the direction the step's sign gives) al 1; else - or with
// no step - the word the target and the Light_AnglesCopy word the
// Light_Angles dword + the target + the bias, al 0. The answer is al alone:
// the rest of eax is the dword's bits, the Light_Angles dword's or, turning
// back, the caller's own eax (every caller tests al).
unsigned char TurnStep(std::uint32_t angle_cell, std::uint32_t light, std::uint32_t copy, std::int32_t bias,
                       std::uint32_t target, unsigned step_arg) {
    const auto step = static_cast<signed char>(step_arg);
    const auto t = static_cast<short>(target);
    if (step != 0) {
        SetWord(AtScenaSx2(angle_cell), static_cast<std::uint32_t>(Word(AtScenaSx2(angle_cell)) + static_cast<short>(step)));
        const auto now = static_cast<short>(UScenaSx2(LongScenaSx2(AtScenaSx2(angle_cell))) & 0xFFF);
        if (step > 0) {
            if (now < t) return 1;
        } else {
            if (now > t) return 1;
        }
    }
    const std::uint32_t l = UScenaSx2(LongScenaSx2(AtScenaSx2(light)));
    SetWord(AtScenaSx2(angle_cell), target);
    SetWord(AtScenaSx2(copy), l + target + static_cast<std::uint32_t>(bias));
    return 0;
}

constexpr unsigned kObjectsSpriteFind = 30, kExtraSpriteFind = 4;

unsigned char ObjectByHandle(unsigned char handle) {
    const unsigned char n = handle & 0x3F;
    return handle & 0x80 ? n : static_cast<unsigned char>(n + kObjectsSpriteFind);
}

struct LiveCounts { unsigned calls, bit7, mismatches; } g_live_handle, g_live_inherit;

void LiveReport(const char* name, const LiveCounts& c) {
    if (c.calls == 1 || c.calls % 4096 == 0)
        bof3::Log("shadow      %s live: %u calls (%u with bit 7), %u MISMATCHES", name, c.calls, c.bit7, c.mismatches);
}

template <class T> T Get(const unsigned char* p, unsigned offset) {
    // A pointer-typed field is a 32-bit word of the image on every width.
    if constexpr (std::is_pointer_v<T>) {
        std::uint32_t w;
        std::memcpy(&w, p + offset, sizeof w);
        return reinterpret_cast<T>(static_cast<std::uintptr_t>(w));
    } else {
        T v;
        std::memcpy(&v, p + offset, sizeof v);
        return v;
    }
}

template <class T> void Put(unsigned char* p, unsigned offset, T v) {
    if constexpr (std::is_pointer_v<T>) {   // the field is a 32-bit word on every width
        const std::uint32_t w = Low32(v);
        std::memcpy(p + offset, &w, sizeof w);
    } else {
        std::memcpy(p + offset, &v, sizeof v);
    }
}

// Gfx_PacketPools + 0x10000 - 0x54: the last byte a primitive may reach in
// buffer 0; buffer 1's is 0x10000 higher. As in draw_emit.cpp.
constexpr std::uint32_t kPoolLimit = 0x7F1BAC;

constexpr unsigned kPrimBytes = 0x48, kQuadBytes = 0x28, kRecordBytes = 12;

const unsigned char* ObjectData() { return Get<const unsigned char*>(Sprite_Current, 0x54); }

}  // namespace

// original 0x57B780: the four extra party objects' screen update (PSX
// FUN_8015B148): Sprite_UpdateScreenA for each present (bit 0) and not
// hidden by bit 6.
extern "C" void __cdecl Party_ExtraScreens(void) {
    unsigned char* object = rv::Bytes(Sprite_ObjectsExtra);
    for (unsigned n = kExtra; n != 0; --n, object += kObjectBytes) {
        Sprite_Current = object;
        const unsigned char f = object[0];
        if ((f & 1) && !(f & 0x40)) field_frame::g.screen_a();
    }
}

// original 0x57B7B0: the four extra party objects' frame (PSX FUN_8015B1D8) -
// Field_ObjectsFrame's first half without the idle-long case, the scratch
// byte, the turn target's result or anything after the handler.
extern "C" void __cdecl Party_ExtraFrame(void) {
    unsigned char* object = rv::Bytes(Sprite_ObjectsExtra);
    for (unsigned n = kExtra; n != 0; --n, object += kObjectBytes) {
        Sprite_Current = object;
        Field_ActiveMember = object;
        if (!(object[0] & 1)) continue;
        unsigned char* const context = object + 0x80;
        const unsigned char flags = context[0];
        if (object[9] == 0 && (flags & 0x20)) {
            if (flags & 0x10) field_frame::g.turn_target(context);
            else field_frame::g.idle(object);
        } else if (context[0] & 1) {
            field_frame::g.linked();
        } else {
            CallHandler(object);
        }
    }
}

// original 0x57B830: a type-0x0A sprite's ordering slot +0x29 (PSX
// FUN_8015B334's head) - 4 with +0x24 bit 4, 6 for a record with byte 3 bit
// 5, else Draw_OtSlot - then its screen position (a tail jmp in the original).
extern "C" void __cdecl Sprite_UpdateScreenA(void) {
    records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
    if (sprite->draw_flags & 0x10) sprite->ot_slot = 4;
    else sprite->ot_slot = (Record54(rv::Bytes(sprite))[3] & 0x20) ? 6 : Draw_OtSlot;
    field_frame::g.project_a();
}

// original 0x57B860: a type-0x0A sprite's draw key, screen position and depth,
// its cull and its place on the draw list.
//
// As the original has it: the key is 16-bit arithmetic of which only the low
// byte is kept, and is not range-tested as Sprite_UpdateScreen's is (no
// Sprite_KeyAdjust either: the record's byte 1 instead); every step reads
// Sprite_Current afresh, so a callee that moves it moves the rest; the float
// screen point is stored as the x87 moves it and truncated as the CRT's _ftol
// does. The vertex's fourth word is 0 (DIV-0023): the original never writes
// it, and Gte_LoadVertex carries its stale stack into Gte_Vertices[3]'s top
// half, which nothing reads.
// Sprite_Current is any sprite head (effects reach it through
// Sprite_UpdateScreenA): read as SpriteHead, here and in Sprite_UpdateScreenA
// (C2 audit frs F2).
extern "C" void __cdecl Sprite_ProjectA(void) {
    records::SpriteHead* sprite = rv::Current<records::SpriteHead>();
    const unsigned z_whole = sprite->z_frac == 0, x_whole = sprite->x_frac == 0;
    sprite->draw_key = static_cast<unsigned>(sprite->z_whole - z_whole - x_whole + sprite->x_whole -
                                                 static_cast<std::uint16_t>(MapView_Origin[1]) -
                                                 static_cast<std::uint16_t>(MapView_Origin[0]) + 2);
    sprite = rv::Current<records::SpriteHead>();
    sprite->draw_key = static_cast<unsigned>(sprite->draw_key + static_cast<signed char>(Record54(rv::Bytes(sprite))[1]));
    sprite = rv::Current<records::SpriteHead>();
    sprite->draw_key = static_cast<unsigned>(rv::Bytes(sprite)[0x32]) << 8;
    unsigned low;
    sprite = rv::Current<records::SpriteHead>();
    if (Draw_OtSlot == 4) low = static_cast<unsigned>(Long(rv::Bytes(sprite) + (Draw_SortOnX ? 0x34 : 0x38)) >> 15) & 0xFF;
    else low = ((sprite->height_whole ^ 0xF01Fu) >> 5) & 0xFF;
    sprite->draw_key = sprite->draw_key | low;
    sprite = rv::Current<records::SpriteHead>();
    if (!(Record54(rv::Bytes(sprite))[3] & 0x10)) {
        field_frame::g.inherit_key();
        sprite = rv::Current<records::SpriteHead>();
    }
    short vertex[4];
    vertex[0] = static_cast<short>((sprite->x >> 9) - 0x4000);
    vertex[1] = static_cast<short>((sprite->z >> 9) - 0x4000);
    vertex[2] = static_cast<short>(-(static_cast<std::int16_t>(sprite->height_whole) / 2));
    vertex[3] = 0;   // DIV-0023
    unsigned long vertex_words[2];
    std::memcpy(vertex_words, vertex, sizeof vertex_words);
    field_frame::g.load_vertex(vertex_words);
    field_frame::g.rtps();
    unsigned long screen[2];
    field_frame::g.store_xy(screen);
    field_frame::g.store_depth(reinterpret_cast<long*>(Sprite_Current + 0x60));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->screen_xf, static_cast<std::int32_t>(ThroughX87(static_cast<std::uint32_t>(screen[0]))));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_x, Ftol16(static_cast<std::uint32_t>(screen[0])));
    rv::Store<std::int32_t>(rv::Current<records::SpriteHead>()->screen_yf, static_cast<std::int32_t>(ThroughX87(static_cast<std::uint32_t>(screen[1]))));
    rv::Store<unsigned>(rv::Current<records::SpriteHead>()->screen_y, Ftol16(static_cast<std::uint32_t>(screen[1])));

    sprite = rv::Current<records::SpriteHead>();
    const unsigned char* const margin = Sprite_ScreenMargins + ((Record54(rv::Bytes(sprite))[3] >> 3) & 1) * 4u;
    const int x = static_cast<std::int16_t>(static_cast<std::uint16_t>(sprite->screen_x)), y = static_cast<std::int16_t>(static_cast<std::uint16_t>(sprite->screen_y));
    if (x > 0x140 + margin[0] || x < -static_cast<int>(margin[1]) || y > 0xF0 + margin[2] || y < -static_cast<int>(margin[3])) {
        sprite->flags |= 0x80;   // off screen
        // DIV-0041 (UC2): in the band, drawn after all (BandNote above).
        const int fill = field_frame::g_band_fill >= 0 ? field_frame::g_band_fill : static_cast<int>(Widescreen_Fill());
        if (fill != 0 && x <= 0x140 + margin[0] + fill && x >= -static_cast<int>(margin[1]) - fill &&
            y <= 0xF0 + margin[2] && y >= -static_cast<int>(margin[3]))
            BandNote(rv::Bytes(sprite));
        return;
    }
    sprite->flags &= 0x7F;
    const unsigned char n = Sprite_DrawListCount;
    if (n >= 0x28) return;
    Sprite_DrawListCount = static_cast<unsigned char>(n + 1);
    Sprite_DrawList[n] = Sprite_Current;
}

// original 0x57BA60: a sprite from an area descriptor +8 entry (PSX
// 0x8015B67C). As the original has it: the entry's +4 is read after the first
// five stores; the angle's index is the direction +8, a whole byte into
// Sprite_DirectionAngles' eight (past them: what follows in .data).
extern "C" void __cdecl Sprite_InitFromEntry(unsigned char* entry) {
    Sprite_Current[0] = 1;
    Sprite_Current[0x24] = 0x40;
    Sprite_Current[0x48] = 1;
    rv::Current<records::SpriteObject>()->scale_x = 0x1000;
    rv::Current<records::SpriteObject>()->scale_y = 0x1000;
    rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->script, Dword(entry + 4));
    rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->frames, Low32(entry));
    rv::Current<records::SpriteObject>()->angle[1] = 0;
    rv::Current<records::SpriteObject>()->angle[0] = 0;
    rv::Store<std::uint32_t>(rv::Current<records::SpriteHead>()->angle[2], Sprite_DirectionAngles[Sprite_Current[8]]);
}

// original 0x57BAE0 (PSX 0x8015B6F0): the current object's quads as primitives and records.
// Returns the count's low byte; the one caller keeps al only.
//
// Kept from the original, deliberately:
//   - the count is the s8 at byte 0 of the object's +0x54 data widened to a
//     u16, so a negative one is some 65,000 quads - which the room check
//     then refuses;
//   - when the packet pool has no room the function returns 0 with the GTE
//     matrix still PUSHED and the matrices it loaded still loaded;
//   - a record's depth is the largest of the four depths / 4, kept as a u16
//     and compared zero-extended against the whole long, so a negative depth
//     never raises it and one past 0xFFFF is stored truncated;
//   - the vertices' fourth words in Prim_VertexScratch are left as they were
//     and go to Gte_LoadVertex with them.
extern "C" unsigned __cdecl Sprite_AddDrawRecords(unsigned long* record) {
    Gte_PushMatrix();
    const auto count = static_cast<std::uint16_t>(static_cast<std::int8_t>(ObjectData()[0]));
    alignas(4) short matrix[16];
    Sprite_ObjectMatrix(matrix);
    Gte_SetRotMatrix(reinterpret_cast<const unsigned long*>(matrix));
    Gte_SetTransMatrix(reinterpret_cast<const unsigned long*>(matrix));
    if (Sprite_Current[0x48] == 0) {
        const long s = Get<long>(Sprite_Current, 0x40);
        const long scale[3] = {s, s, s};
        Gte_ScaleMatrix(matrix, scale);
    }
    alignas(4) short view[16];
    std::memcpy(view, matrix, sizeof view);
    Light_ObjectDirection(reinterpret_cast<short*>(Light_Matrix), reinterpret_cast<const long*>(Sprite_Current + 0x64));
    Camera_LoadMatrix(view);
    Gte_SetMatrix2(Light_Matrix);

    const unsigned char flags = ObjectData()[3];
    const unsigned mode = flags & 0x40 ? flags : 2u;
    const unsigned char* quad = Get<const unsigned char*>(Sprite_Current, 0x50);
    const unsigned tpage = Gpu_GetTPage(0, mode & 3, 0x2C0, 0x100);
    unsigned char* const start = Gfx_PacketNext;
    const std::uint32_t bytes = static_cast<std::uint32_t>(count) * kPrimBytes;
    const std::uint32_t limit = (static_cast<std::uint32_t>(Gfx_BufferIndex) << 16) + ::bof3::Img(kPoolLimit);
    if (limit < Low32(start) + bytes) return 0;

    const unsigned semi_trans = (mode >> 6) & 1;
    unsigned char* prim = start;
    auto* rec = reinterpret_cast<unsigned char*>(record);
    for (unsigned n = count; n != 0; --n) {
        Gpu_SetPolyFT4(prim);
        short* vertices = reinterpret_cast<short*>(Prim_VertexScratch);
        for (unsigned i = 0; i < 12; ++i) vertices[i / 3 * 4 + i % 3] = Get<short>(quad, 2 + 2 * i);
        const long normal[3] = {Get<short>(quad, 0x1A), Get<short>(quad, 0x1C), Get<short>(quad, 0x1E)};
        for (unsigned k = 0; k < 4; ++k) Put<std::uint16_t>(prim, 0x14 + 0x10 * k, Get<std::uint16_t>(quad, 0x20 + 2 * k));
        for (unsigned k = 0; k < 4; ++k) {
            Gte_LoadVertex(reinterpret_cast<const unsigned long*>(vertices + 4 * k));
            Gte_Rtps();
            Gte_StoreScreenXY(reinterpret_cast<unsigned long*>(prim + 8 + 0x10 * k));
            long depth;
            Gte_StoreDepthQuarter(&depth);
            Gte_StoreDepthF(reinterpret_cast<float*>(prim + 0x10 + 0x10 * k));
            if (k == 0) {
                Put<std::uint16_t>(rec, 4, static_cast<std::uint16_t>(depth));
                Put<std::uint16_t>(rec, 6, static_cast<std::uint16_t>(depth));
            } else if (static_cast<long>(Get<std::uint16_t>(rec, 4)) < depth) {
                Put<std::uint16_t>(rec, 4, static_cast<std::uint16_t>(depth));
            }
        }
        Put<unsigned char*>(rec, 0, prim);
        Put<unsigned char*>(rec, 8, Sprite_Current);
        prim[4] = Sprite_Current[0x5D];
        prim[5] = Sprite_Current[0x5E];
        prim[6] = Sprite_Current[0x5F];
        if (ObjectData()[3] & 0x80) {
            Gte_VectorNormalS(normal, reinterpret_cast<short*>(Prim_VertexScratch));
            const unsigned char color[4] = {Sprite_Current[0x5D], Sprite_Current[0x5E], Sprite_Current[0x5F], prim[7]};
            Gte_NormalColor(reinterpret_cast<short*>(Prim_VertexScratch), color, prim + 4);
        }
        Put<std::uint16_t>(prim, 0x26, static_cast<std::uint16_t>(tpage));
        const int clut_x = static_cast<int>(static_cast<unsigned>(static_cast<int>(Get<short>(quad, 0))) << 4);
        Put<std::uint16_t>(prim, 0x16, static_cast<std::uint16_t>(Gpu_GetClut(clut_x, 0x1E3)));
        Gpu_SetSemiTrans(prim, semi_trans);
        prim += kPrimBytes;
        rec += kRecordBytes;
        quad += kQuadBytes;
    }
    Gfx_PacketNext = start + bytes;
    Gte_PopMatrix();
    return count & 0xFFu;
}

// original 0x57BED0 (PSX 0x8015BC9C): the light's direction in the object's frame, to out[0..2]
// - the first row of the light matrix. (0, 0, Light_Angles[3]) turned by the
// light's angles, then by the object's rotation "transposed" - IN PLACE, and
// Gte_TransposeMatrix in place is not a transpose: the upper triangle is
// lost. Kept, as the original has it; Gte_NormalColor as shipped throws the
// lit colour away, so nothing on screen shows it.
extern "C" void __cdecl Light_ObjectDirection(short* out, const long* angles) {
    Light_AnglesCopy[0] = Light_Angles[0];
    Light_AnglesCopy[1] = Light_Angles[1];
    Light_AnglesCopy[2] = Light_Angles[2];
    const unsigned long zero[3] = {0, 0, 0};
    Gte_PushMatrix();
    alignas(4) short turn[16];
    Gte_RotMatrix(Light_AnglesCopy, turn);
    Gte_TransMatrix(reinterpret_cast<unsigned long*>(turn), zero);
    Gte_SetRotMatrix(reinterpret_cast<const unsigned long*>(turn));
    Gte_SetTransMatrix(reinterpret_cast<const unsigned long*>(turn));
    const short reach[4] = {0, 0, Light_Angles[3], 0};
    long light[3];
    Gte_RotTrans(reach, light);
    const short object_angles[4] = {static_cast<short>(angles[0]), static_cast<short>(angles[1]),
                                    static_cast<short>(angles[2]), 0};
    Gte_RotMatrix(object_angles, turn);
    Gte_TransposeMatrix(turn, turn);
    Gte_ApplyMatrixLV(turn, light, light);
    out[0] = static_cast<short>(light[0]);
    out[1] = static_cast<short>(light[1]);
    out[2] = static_cast<short>(light[2]);
    Gte_PopMatrix();
}

// original 0x57BFF0 (PSX 0x8015BE00): the current object's matrix. The position - x and y in
// 1/512ths less 0x4000, z the negated half of the s16 at +0x3E, toward zero -
// through the current GTE matrix into the translation; the low words of
// +0x64, +0x68, +0x6C as the rotation's angles.
extern "C" short* __cdecl Sprite_ObjectMatrix(short* matrix) {
    const unsigned char* object = Sprite_Current;
    short position[4], angles[4];
    position[0] = static_cast<short>((Get<long>(object, 0x34) >> 9) - 0x4000);
    position[1] = static_cast<short>((Get<long>(object, 0x38) >> 9) - 0x4000);
    position[2] = static_cast<short>(-(Get<short>(object, 0x3E) / 2));
    angles[0] = Get<short>(object, 0x64);
    angles[1] = Get<short>(object, 0x68);
    angles[2] = Get<short>(object, 0x6C);
    Gte_RotTrans(position, reinterpret_cast<long*>(matrix + 10));
    return Gte_RotMatrix(angles, matrix);
}

// original 0x57C070 (PSX 0x8015BEA0): an object's rotation composed with the camera's, in
// place, then the whole matrix - that rotation, the object's own translation -
// loaded into the GTE.
extern "C" void __cdecl Camera_LoadMatrix(short* matrix) {
    Gte_MulMatrix0(Camera_Matrix, matrix, matrix);
    Gte_SetRotMatrix(reinterpret_cast<const unsigned long*>(matrix));
    Gte_SetTransMatrix(reinterpret_cast<const unsigned long*>(matrix));
}

extern "C" unsigned char __cdecl Sprite_ObjectByHandle(unsigned char handle) {
    const unsigned char ours = ObjectByHandle(handle);
    if (sprite_find::g_handle_live) {
        const unsigned char theirs = sprite_find::g_handle_live(handle);
        LiveCounts& c = g_live_handle;
        ++c.calls;
        if (handle & 0x80) ++c.bit7;
        if (theirs != ours && ++c.mismatches <= 8)
            bof3::Log("shadow      Sprite_ObjectByHandle live MISMATCH: call %u, handle %02X: %u / %u", c.calls, handle,
                      theirs, ours);
        LiveReport("Sprite_ObjectByHandle", c);
    }
    return ours;
}

// original 0x57C0F0 (PSX 0x8015BF70).
extern "C" void __cdecl Flags_Set(unsigned char* bits, unsigned index) {
    const unsigned i = index & 0xFFu;
    bits[i >> 3] |= static_cast<unsigned char>(1u << (i & 7));
}

// original 0x57C110 (PSX 0x8015BF98).
extern "C" void __cdecl Flags_Clear(unsigned char* bits, unsigned index) {
    const unsigned i = index & 0xFFu;
    bits[i >> 3] &= static_cast<unsigned char>(~(1u << (i & 7)));
}

// original 0x57C140 (PSX 0x8015BFC4). The original's eax is exactly 0 or 1
// (setne over a value below 0x20); this returns an unsigned char, which the
// compiler widens in eax for its callers (checked in docs/event-script.md).
extern "C" unsigned char __cdecl Flags_Test(const unsigned char* bits, unsigned index) {
    const unsigned i = index & 0xFFu;
    return (bits[i >> 3] & (1u << (i & 7))) != 0;
}

// original 0x57C160 (PSX 0x8015BFE4): bit (index & 7) of bits[(index & 0xFF) >> 3] toggled -
// Flags_Set's and Flags_Clear's xor sibling (the PSX Flag_Toggle 0x8015BFE4,
// the sibling's name, confirmed there; pairs_propagated "call"). eax is left
// the byte's address; no caller reads it.
extern "C" void __cdecl Flags_Toggle(unsigned char* bits, unsigned index) {
    const unsigned i = index & 0xFF;
    bits[i >> 3] = static_cast<unsigned char>(bits[i >> 3] ^ (1u << (i & 7)));
}

// original 0x57C180: condition 0 - the operand equals Cond_ByteFA (PSX
// 0x8015C00C). The position is not moved.
extern "C" unsigned char __cdecl EventCond_ByteFA(const unsigned char** at) {
    return static_cast<unsigned char>(Cond_ByteFA) == **at;
}

// original 0x57C1A0 (PSX 0x8015C034): condition 1 - Game_AreaNumber (a word) equals the operand.
extern "C" unsigned char __cdecl EventCond_Area(const unsigned char** at) { return Game_AreaNumber == **at; }

// original 0x57C1C0 (PSX 0x8015C05C): condition 2 - flag `operand` of row EventScript_FlagBank
// (its low byte) of Cond_Flags (PSX 0x8015C05C).
extern "C" unsigned char __cdecl EventCond_Flag(const unsigned char** at) {
    return event_script::g.flags_test(Cond_Flags + (EventScript_FlagBank & 0xFFu) * 8u, **at);
}

// original 0x57C1F0 (PSX 0x8015C098): condition 7 - MoveScript_Var7 (the chapter's run byte) equals it.
extern "C" unsigned char __cdecl EventCond_Run(const unsigned char** at) {
    return static_cast<unsigned char>(MoveScript_Var7) == **at;
}

// original 0x57C210: condition 8 - the operand equals Cond_ByteFD (PSX
// 0x8015C0C0).
extern "C" unsigned char __cdecl EventCond_ByteFD(const unsigned char** at) { return Cond_ByteFD == **at; }

// original 0x57C230 (PSX 0x8015C0E8): condition 9 - Field_StatusBits' bit 0, or its
// complement's unless the operand is 1.
extern "C" unsigned char __cdecl EventCond_Status1(const unsigned char** at) {
    const auto bits = static_cast<unsigned char>(Field_StatusBits);
    return static_cast<unsigned char>((**at == 1 ? bits : static_cast<unsigned char>(~bits)) & 1);
}

// original 0x57C250 (PSX 0x8015C12C): condition 11 - the leader's member id (ObjTrio +0x89)
// equals it.
extern "C" unsigned char __cdecl EventCond_LeaderId(const unsigned char** at) { return ObjTrio[0].char_id == **at; }

// original 0x57C270 (PSX 0x8015C154): condition 12 - story flag `operand` (Flags_Test of
// 0x904030); the answer is Flags_Test's eax.
extern "C" unsigned char __cdecl EventCond_StoryFlag(const unsigned char** at) {
    return SH_CALL(Flags_Test)(AtFieldO(::bof3::Img(0x904030)), **at);
}

// original 0x57C290 (PSX 0x8015C188): condition 13 - KeyItem_Has(operand).
extern "C" unsigned char __cdecl EventCond_KeyItem(const unsigned char** at) { return SH_CALL(KeyItem_Has)(**at); }

// original 0x57C2B0 (PSX 0x8015C1B4): condition 14 - Cond_ByteFA (unsigned) at most the operand.
extern "C" unsigned char __cdecl EventCond_ChapterAtMost(const unsigned char** at) {
    return static_cast<unsigned char>(Cond_ByteFA) <= **at;
}

// original 0x57C2D0 (PSX 0x8015C1E0): condition 15 - bit 0 of CharacterRecords[operand] +0xB.
// As the original has it: the operand is a whole byte into the eight records.
extern "C" unsigned char __cdecl EventCond_RecordBit0(const unsigned char** at) { return Record(**at)[0xB] & 1; }

// original 0x57C2F0 (PSX 0x8015C22C): condition 16 - bit `operand` of the gene flags 0x904650
// (Flags_Test; the dword's 18 bits, battle_e5.md).
extern "C" unsigned char __cdecl EventCond_Gene(const unsigned char** at) { return SH_CALL(Flags_Test)(AtFieldO(::bof3::Img(Gene_Bits_addr)), **at); }

// original 0x57C310: the movement script's variables (PSX 0x8015C260), read
// by flow ops 05, 07 and 09 - a jump table at 0x57C3A4 of one-line reads.
// As the original has it: 14 is 0 again (the table's last entry is case 0's);
// 13 is the constant 1; past 14, 0xFF.
extern "C" int __cdecl MoveScript_Variable(unsigned char index) {
    switch (index) {
    case 0: case 14: return Cond_ByteFA;
    case 1: return Game_AreaNumber;
    case 2: {
        return Long(At(Chapter_FlagRow));   // the slot's 32-bit word, a place
    }
    case 3: case 4: case 5: case 6: return At(::bof3::Img(kCounters))[index - 3];
    case 7: return MoveScript_Var7;
    case 8: return Cond_ByteFD;
    case 9: return Field_StatusBits & 1;
    case 10: return rv::Current<records::SpriteHead>()->script_pos;
    case 11: return ObjTrio[0].char_id;
    case 12: return Long(At(::bof3::Img(Cond_Flags_addr) + 0xa0));
    case 13: return 1;
    default: return 0xFF;
    }
}

// original 0x57C3E0 (PSX 0x8015C37C): condition 3 - the script counter 0x903848 equals it.
extern "C" unsigned char __cdecl EventCond_Counter0(const unsigned char** at) { return AtFieldO(::bof3::Img(0x903848))[0] == **at; }

// original 0x57C400 (PSX 0x8015C3A4) / 0x57C420 / 0x57C440: conditions 4..6 - counters 1..3.
extern "C" unsigned char __cdecl EventCond_Counter1(const unsigned char** at) { return AtFieldO(::bof3::Img(0x903849))[0] == **at; }

extern "C" unsigned char __cdecl EventCond_Counter2(const unsigned char** at) { return AtFieldO(::bof3::Img(0x90384A))[0] == **at; }

extern "C" unsigned char __cdecl EventCond_Counter3(const unsigned char** at) { return AtFieldO(::bof3::Img(0x90384B))[0] == **at; }

extern "C" signed char __cdecl MoveScript_CounterSet(const unsigned char* op) {
    *Counter(op) = op[1];
    return 1;
}

extern "C" signed char __cdecl MoveScript_CounterTest(const unsigned char* op) {
    return *Counter(op) == op[1] ? 1 : -1;
}

extern "C" signed char __cdecl MoveScript_CounterStep(const unsigned char* op) {
    ++*Counter(op);
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
    const unsigned char type = rv::Current<records::SpriteObject>()->type;
    if (type >= 8) return;
    if (type == 6) {
        sprite_pose::g.ensure_animation(direction);
        return;
    }
    const unsigned pair = direction * 2u;
    rv::Current<records::SpriteObject>()->flip = At(::bof3::Img(sprite_pose::at::kDirectionAnimations))[pair + 1];
    const unsigned char start = Field_ActiveMember[0xA1];
    const unsigned char animation = At(::bof3::Img(sprite_pose::at::kDirectionAnimations))[pair];
    if (start == 0xFF) {
        sprite_pose::g.ensure_animation(animation);
        return;
    }
    if (rv::Current<records::SpriteObject>()->animation == animation) return;
    if (animation == 5 || animation == 6) {
        sprite_pose::g.set_animation_at(animation, start);
        return;
    }
    sprite_pose::g.set_animation(animation);
}

// original 0x57C550: 0x57C5A0(angle, step), the angle (s16 degrees) and the
// step (s8 degrees) each as a 4096-step circle (x 4096 / 360, truncated; the
// angle & 0xFFF): Camera_Angles[0] turned by the step toward the angle,
// al 1 while it turns (0x57C5A0's answer, whole). No PSX twin paired.
extern "C" std::uint32_t __cdecl Camera_TurnToDegrees(unsigned angle_arg, unsigned step_arg) {
    const std::int32_t step = Degrees(S8(static_cast<unsigned char>(step_arg)));
    const std::int32_t angle = Degrees(static_cast<short>(angle_arg)) & 0xFFF;
    return SH_AT(std::uint32_t (__cdecl*)(std::int32_t, std::int32_t), scena_sx::at::kCameraTurnYaw.v)(angle, step);
}

// original 0x57C5A0: Camera_Angles[0] turned a step (the second argument's
// low byte, s8, 4096-step units) toward the angle (the first argument's low
// word); al 1 while short of it (compared as s16 against the word & 0xFFF);
// else the word the angle and Light_AnglesCopy[0] = Light_Angles[0] + the
// angle + 0x2AA, al 0. Camera_TurnToDegrees' one callee. No PSX twin paired.
extern "C" unsigned char __cdecl Camera_TurnStep(unsigned angle, unsigned step) {
    return TurnStep(::bof3::Img(Camera_Angles_addr), ::bof3::Img(Light_Angles_addr), ::bof3::Img(Light_AnglesCopy_addr), 0x2AA, angle, step);
}

// original 0x57C600: Camera_TurnStepFB(the angle (s16 degrees) x 4096 / 360 &
// 0xFFF, the step (s8 degrees) x 4096 / 360) - each truncated (the imul
// 0xB60B60B7 / add / sar 8 / add sign idiom), the step passed as the whole
// dword the callee reads the low byte of (a step above 11 degrees wraps as
// s8 there). al: Camera_TurnStepFB's (1 while turning). Camera_TurnToDegrees'
// twin; SC2's Scena02_Scene10 tests it twice. No PSX twin paired.
extern "C" unsigned char __cdecl Camera_TurnFBToDegrees(unsigned angle, unsigned step) {
    const std::int32_t s = (static_cast<std::int32_t>(static_cast<signed char>(step)) << 12) / 360;
    const std::int32_t a = ((static_cast<std::int32_t>(static_cast<short>(angle)) << 12) / 360) & 0xFFF;
    return SH_CALL(Camera_TurnStepFB)(UScenaSx2(a), UScenaSx2(s));
}

// original 0x57C650: Camera_TurnStep for Cond_AngleFB (Camera_Angles[2]):
// the step, the test, and at the end the word the angle and
// Light_AnglesCopy[2] = Light_Angles[2] (0x90359C) + the angle - 0x200. al
// 1 while turning. Camera_TurnFBToDegrees' one callee. No PSX twin paired.
extern "C" unsigned char __cdecl Camera_TurnStepFB(unsigned angle, unsigned step) {
    return TurnStep(::bof3::Img(Cond_AngleFB_addr), ::bof3::Img(Light_Angles_addr) + 0x4, ::bof3::Img(Light_AnglesCopy_addr) + 0x4, -0x200, angle, step);
}

// original 0x57C6B0: Cond_AngleFB eased toward an angle (s16 degrees, as a
// 4096-step circle) over frames. While frames are left (0x905B9C): the 16.16
// accumulator 0x905B98 plus the step 0x905B78, its high word to Cond_AngleFB,
// one frame less, al 1 while any are left. Else, with Sprite_Kind2 +0x84's
// Field_MoveSpeeds entry 0: Cond_AngleFB the angle at once, al 0. Else the
// frames 0x80 / (the speed x 4, or x 8 unless Sprite_Kind2 +8 is 2 or 6, as
// a byte), times the second argument (s8); the accumulator Cond_AngleFB <<
// 16; the step ((the angle - (Cond_AngleFB & 0x7FF)) << 16) / the frames; al
// 1 unless the frames are 0. Where the original divides by 0 (a byte product
// of 0, a second argument of 0) or overflows (-2^31 / -1) it faults; ours
// aborts. No PSX twin paired.
extern "C" unsigned char __cdecl Camera_EaseAngleFB(unsigned angle_arg, unsigned frames_arg) {
    std::int32_t left = static_cast<std::int32_t>(ScenaSx_EaseLeft);
    const unsigned char speed = Field_MoveSpeeds[Sprite_Kind2.speed];
    if (left != 0) {
        const std::int32_t acc = static_cast<std::int32_t>(static_cast<std::uint32_t>(CameraTurn_Angles[2]) +
                                                           static_cast<std::uint32_t>(CameraTurn_Steps[2]));
        CameraTurn_Angles[2] = acc;
        SetWord(move_script::At(::bof3::Img(Cond_AngleFB_addr)), static_cast<std::uint32_t>(acc >> 16));
        --left;
        ScenaSx_EaseLeft = left;
        return left != 0 ? 1 : 0;
    }
    if (speed == 0) {
        SetWord(move_script::At(::bof3::Img(Cond_AngleFB_addr)), static_cast<std::uint32_t>(Degrees(static_cast<short>(angle_arg))));
        return 0;
    }
    const unsigned char mode = Sprite_Kind2.direction;
    const int per = mode == 2 || mode == 6 ? 4 : 8;
    const auto product = static_cast<unsigned char>(per * S8(speed));
    if (product == 0) bof3::Fatal("Camera_EaseAngleFB: 0x80 / 0 (speed byte 0x%X): the original faults", speed);
    const std::int32_t frames = 0x80 / product;
    const auto cur = static_cast<short>(Word(move_script::At(::bof3::Img(Cond_AngleFB_addr))));
    const std::int32_t total = static_cast<std::int32_t>(static_cast<std::uint32_t>(frames) *
                                                         static_cast<std::uint32_t>(S8(static_cast<unsigned char>(frames_arg))));
    CameraTurn_Angles[2] = static_cast<std::int32_t>(static_cast<std::uint32_t>(cur) << 16);
    const std::int32_t target = Degrees(static_cast<short>(angle_arg));
    ScenaSx_EaseLeft = total;
    const auto num = static_cast<std::int32_t>(static_cast<std::uint32_t>(target - (cur & 0x7FF)) << 16);
    if (total == 0) bof3::Fatal("Camera_EaseAngleFB: a step over 0 frames (second argument 0): the original faults");
    if (num == INT32_MIN && total == -1) bof3::Fatal("Camera_EaseAngleFB: -2^31 / -1: the original faults");
    CameraTurn_Steps[2] = num / total;
    return 1;
}

// A pair by width (docs/x64-asm-policy.md 8 unit 6): the two tail-jumpers
// are ours now (Area111_TailGate, Scena03_Scene5: void, a statement call),
// so no caller reads eax on either width, but the fuzz compares the
// passthrough (map_field_objects_fuzz.cpp TestClear40), and the x86_64 half
// costs one move: the caller's eax as the body's first argument.
extern "C" __attribute__((naked)) void __cdecl ScriptFlags_Clear40(void) {
#if defined(__i386__)
    asm("pushl %eax\n\t"
        "call " BOF3_SYM(MapFieldObjects_Clear40Body) "\n\t"
        "addl $4, %esp\n\t"
        "ret");
#elif defined(__x86_64__)
    asm("movl %eax, %ecx\n\t"
        "jmp " BOF3_SYM(MapFieldObjects_Clear40Body));
#else
#error "ScriptFlags_Clear40: no entry for this architecture"
#endif
}

// original 0x57C7C0 (PSX 0x8015CA78): bit 6 of byte 0x8034E1 - read before,
// stored after - and bit 8 of Field_ScriptFlags, by its high byte.
extern "C" void __cdecl ScriptFlags_Set40(void) {
    const unsigned char b = Field_StatusBits;
    reinterpret_cast<unsigned char*>(&Field_ScriptFlags)[1] |= 1;
    Field_StatusBits = b | 0x40;
}

// original 0x57C7E0 (MoveScript_Flow 0x57701D, Scena03_Scene5 0x5438DC; PSX 0x8015CAA8): bit 6
// of the three party objects' first byte cleared - ObjTrio_SetBit40's inverse.
extern "C" void __cdecl ObjTrio_ClearBit40(void) {
    ObjTrio[0].flags &= 0xBF;
    ObjTrio[1].flags &= 0xBF;
    ObjTrio[2].flags &= 0xBF;
}

// original 0x57C810 (PSX 0x8015CAF0): bit 6 of three objects' first byte.
extern "C" void __cdecl ObjTrio_SetBit40(void) {
    ObjTrio[0].flags |= 0x40;
    ObjTrio[1].flags |= 0x40;
    ObjTrio[2].flags |= 0x40;
}

// original 0x57C840: what Sprite_Current is (PSX 0x8015CB38): 2 Sprite_Kind2,
// 1 one of the three ObjTrio objects, 0 one of the 30 Sprite_Objects, 3 one of
// the four Sprite_ObjectsExtra.
// As the original has it: anything else returns the pointer itself.
extern "C" int __cdecl MoveScript_ObjectKind(void) {
    const std::uint32_t sprite = AddressMoveCmds(Sprite_Current);
    if (sprite == AddressMoveCmds(rv::Bytes(&Sprite_Kind2))) return 2;
    const std::uint32_t trio = AddressMoveCmds(rv::Bytes(ObjTrio));
    if (sprite == trio || sprite == trio + 0x14C || sprite == trio + 0x298) return 1;
    if (sprite >= AddressMoveCmds(rv::Bytes(Sprite_Objects)) && sprite < AddressMoveCmds(rv::Bytes(Sprite_Objects)) + (Sprite_Objects_count * sizeof(records::SpriteObject))) return 0;
    if (sprite >= AddressMoveCmds(rv::Bytes(Sprite_ObjectsExtra)) && sprite < AddressMoveCmds(rv::Bytes(Sprite_ObjectsExtra)) + (Sprite_ObjectsExtra_count * sizeof(records::SpriteObject))) return 3;
    return static_cast<int>(sprite);
}

// original 0x57C8A0 (PSX 0x8015CC04): party member m's field object (ObjTrio + m * 0x14C, m
// the first argument's low byte, unchecked): state +1 2, sub-state +2 8, +3 0,
// +0xB the second argument's low byte - Field_TileTurn's state 2 / 8 for a
// member by index. Area 51 passes 1; areas 148, EffectKind28_PushParty 0x4703F0,
// EffectKind2A_PushParty 0x4712E0 and 0x4849A0 pass 2 or a table byte. PSX 0x8015CC04
// (pairs_propagated "call").
extern "C" void __cdecl Member_SetState2_8(unsigned member, unsigned value) {
    unsigned char* const o = rv::Bytes(&ObjTrio[member & 0xFF]);
    o[1] = 2;
    o[2] = 8;
    o[3] = 0;
    o[0xB] = static_cast<unsigned char>(value);
}

// original 0x57C8E0 (PSX 0x8015CC54): op E9 (MoveScript_GroupE 0x5775B9; the bosses' end moves
// BossNue_EndMove / BossWeretigr_EndMove): its seven words handed on to the
// state Sprite_Current +4 picks from 0x663B84, its eax the answer (the op
// repeats while al is not 0).
// As the original has it: the state byte is unchecked; past 3 the words are
// not code (ours aborts).
extern "C" unsigned char __cdecl MoveCmd_OpE9(unsigned char* object, signed char a, signed char b, unsigned short c,
                                              unsigned short d, unsigned char e, unsigned char f) {
    using State = unsigned char (__cdecl*)(unsigned char*, signed char, signed char, unsigned short, unsigned short,
                                           unsigned char, unsigned char);
    return reinterpret_cast<State>(CodeAt(::bof3::Img(MoveCmd_OpE9States_addr), Sc()[4], "MoveCmd_OpE9"))(object, a, b, c, d, e, f);
}

// original 0x57C920 (PSX 0x8015CCC4): op E9's state 0 - the jump's start. The speed
// Field_MoveSpeeds[object +4]; 0 answers al 0 at once. Else +9 0; the steps
// +0xB max(|a|, |b|) less 1 (0 for none); the frames +0xA 16 / speed; the
// rise +0x14 c << 8; then the kind-2 object (MoveScript_ObjectKind 2) takes
// Field_Kind2X / Z (a, b halves past Sprite_Kind2's x / z), MoveScript_
// F3Divisor speed * 8, MoveScript_FAWord 0 and state 2; any other the
// velocity +0xC / +0x10 (a << 15) / (frames * steps), (b << 15) / .. (both 0
// with no steps) and state 1. al 1.
// As the original has it: the frames are read again after the call; a speed
// above 16 (Field_MoveSpeeds read past its six) makes them 0 and the division
// by frames * steps faults (ours aborts).
extern "C" unsigned char __cdecl MoveCmd_OpE9Start(unsigned char* object, signed char a, signed char b, unsigned short c,
                                                   unsigned short, unsigned char, unsigned char) {
    const unsigned char speed = Field_MoveSpeeds[object[4]];
    if (speed == 0) return 0;
    rv::As<records::SpriteHead>(Sc())->step_frames = 0;
    const std::int32_t sa = a, sb = b;
    const std::int32_t ma = sa < 0 ? -sa : sa, mb = sb < 0 ? -sb : sb;
    const auto steps = static_cast<unsigned char>(ma < mb ? mb : ma);
    Sc()[0xB] = steps ? static_cast<unsigned char>(steps - 1) : 0;
    Sc()[0xA] = static_cast<unsigned char>(Idiv(0x10, speed, "MoveCmd_OpE9Start"));
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->step_y, static_cast<std::int32_t>(static_cast<U>(static_cast<std::int32_t>(static_cast<short>(c))) << 8));
    if (SH_CALL(MoveScript_ObjectKind)() == 2) {
        Field_Kind2Z = static_cast<long>(static_cast<U>(sb) << 15) + Sprite_Kind2.z;
        Field_Kind2X = static_cast<long>(static_cast<U>(sa) << 15) + Sprite_Kind2.x;
        MoveScript_F3Divisor = static_cast<unsigned short>(speed << 3);
        MoveScript_FAWord = 0;
        Sc()[4] = 2;
        return 1;
    }
    if (steps == 0) {
        rv::As<records::SpriteHead>(Sc())->step_z = 0;
        rv::As<records::SpriteHead>(Sc())->step_x = 0;
        Sc()[4] = 1;
        return 1;
    }
    records::SpriteHead* sc = rv::As<records::SpriteHead>(Sc());
    sc->step_x = Idiv(static_cast<std::int32_t>(static_cast<U>(sa) << 15), rv::Bytes(sc)[0xA] * steps, "MoveCmd_OpE9Start");
    sc = rv::As<records::SpriteHead>(Sc());
    sc->step_z = Idiv(static_cast<std::int32_t>(static_cast<U>(sb) << 15), rv::Bytes(sc)[0xA] * steps, "MoveCmd_OpE9Start");
    Sc()[4] = 1;
    return 1;
}

// original 0x57CA80 (PSX 0x8015CF30): op E9's state 1 - the jump in the air. The velocity into
// x / z, the rise's high part (+0x14 >> 8) into the height +0x3E, the fall d
// into the rise, the frames less 1; the ground from AreaMap_Elevation: falling
// (+0x14 below 0) and below the ground, the height is the ground and with f
// bit 1 the state 0 and al 0. At the top (the rise from not below 0 to not
// above 0) with e not 0xFF: +0x2A f's bit 0 unless the sprite is kind 6, and
// Sprite_EnsureAnimation(e). A step's frames done: the next step's frames
// (16 / speed) and a step fewer, or with none left MoveCmd_OpDB and state 3;
// Party_ApplyRecord(object); al 1.
// As the original has it: the speed index object +4 is unchecked, and a
// speed of 0 there divides by 0 (ours aborts).
extern "C" unsigned char __cdecl MoveCmd_OpE9Arc(unsigned char* object, signed char, signed char, unsigned short,
                                                 unsigned short d, unsigned char e, unsigned char f) {
    const auto add = [](unsigned char* to, std::int32_t by) {
        SetLong(to, static_cast<std::int32_t>(static_cast<U>(Long(to)) + static_cast<U>(by)));
    };
    add(Sc() + 0x34, rv::As<records::SpriteHead>(Sc())->step_x);
    add(Sc() + 0x38, rv::As<records::SpriteHead>(Sc())->step_z);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(Sc())->height_whole, rv::As<records::SpriteHead>(Sc())->height_whole + static_cast<U>(rv::As<records::SpriteHead>(Sc())->step_y >> 8));
    const std::int32_t rise = Long(Sc() + 0x14);
    rv::As<records::SpriteHead>(Sc())->step_y = static_cast<std::int32_t>(static_cast<U>(static_cast<short>(d)) + static_cast<U>(rise));
    Sc()[0xA] = static_cast<unsigned char>(Sc()[0xA] - 1);
    const long ground = SH_CALL(AreaMap_Elevation)(rv::As<records::SpriteHead>(Sc())->x, rv::As<records::SpriteHead>(Sc())->z);
    records::SpriteHead* sc = rv::As<records::SpriteHead>(Sc());
    if (sc->step_y < 0 && S16(static_cast<U>(ground)) > S16(sc->height_whole)) {
        rv::Store<unsigned>(sc->height_whole, static_cast<U>(ground));
        if (f & 2) {
            Sc()[4] = 0;
            return 0;
        }
        sc = rv::As<records::SpriteHead>(Sc());
    }
    if (!(sc->step_y > 0) && !(rise < 0) && e != 0xFF) {
        if (sc->type != 6) sc->flip = f & 1;
        SH_CALL(Sprite_EnsureAnimation)(e);
        sc = rv::As<records::SpriteHead>(Sc());
    }
    if (rv::Bytes(sc)[0xA] == 0) {
        if (rv::Bytes(sc)[0xB] != 0) {
            rv::Bytes(sc)[0xA] = StepFrames(object, "MoveCmd_OpE9Arc");
            Sc()[0xB] = static_cast<unsigned char>(Sc()[0xB] - 1);
            SH_CALL(Party_ApplyRecord)(object);
            return 1;
        }
        SH_CALL(MoveCmd_OpDB)();
        Sc()[4] = 3;
    }
    SH_CALL(Party_ApplyRecord)(object);
    return 1;
}

// original 0x57CBB0: op E9's state 2 - the kind-2 object's jump: the rise into
// the height as state 1's, MapView_SetElevation(Sprite_Kind2 +0x3E), the frames
// less 1, the ground at Field_Kind2X / Z; falling below it the height is the
// ground (MapView_SetElevation again) and with f bit 1 the state 0. Frames
// left: al 1. A step left: the next step's frames, a step fewer, al 1. None:
// at or above the ground the object lands on Field_Kind2X / Z (state 0, al 0),
// else one more frame, al 1.
extern "C" unsigned char __cdecl MoveCmd_OpE9Kind2(unsigned char* object, signed char, signed char, unsigned short,
                                                   unsigned short d, unsigned char, unsigned char f) {
    rv::Store<unsigned>(rv::As<records::SpriteHead>(Sc())->height_whole, rv::As<records::SpriteHead>(Sc())->height_whole + static_cast<U>(rv::As<records::SpriteHead>(Sc())->step_y >> 8));
    rv::As<records::SpriteHead>(Sc())->step_y = static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(Sc())->step_y) + static_cast<U>(static_cast<short>(d)));
    SH_CALL(MapView_SetElevation)(static_cast<std::uint16_t>(Sprite_Kind2.height_whole));
    Sc()[0xA] = static_cast<unsigned char>(Sc()[0xA] - 1);
    const long ground = SH_CALL(AreaMap_Elevation)(Field_Kind2X, Field_Kind2Z);
    records::SpriteHead* sc = rv::As<records::SpriteHead>(Sc());
    if (sc->step_y < 0 && S16(static_cast<U>(ground)) > S16(sc->height_whole)) {
        sc->height_whole = static_cast<U>(ground);
        SH_CALL(MapView_SetElevation)(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->height_whole));
        if (f & 2) Sc()[4] = 0;
        sc = rv::As<records::SpriteHead>(Sc());
    }
    if (rv::Bytes(sc)[0xA] != 0) return 1;
    if (rv::Bytes(sc)[0xB] != 0) {
        rv::Bytes(sc)[0xA] = StepFrames(object, "MoveCmd_OpE9Kind2");
        Sc()[0xB] = static_cast<unsigned char>(Sc()[0xB] - 1);
        return 1;
    }
    if (S16(static_cast<U>(ground)) < S16(sc->height_whole)) {
        rv::Bytes(sc)[0xA] = 1;
        return 1;
    }
    sc->x = Field_Kind2X;
    rv::Store<std::int32_t>(rv::As<records::SpriteHead>(Sc())->z, Field_Kind2Z);
    rv::Store<unsigned>(rv::As<records::SpriteHead>(Sc())->height_whole, static_cast<U>(ground));
    SH_CALL(MapView_SetElevation)(static_cast<std::uint16_t>(rv::As<records::SpriteHead>(Sc())->height_whole));
    Sc()[4] = 0;
    return 0;
}

// original 0x57CCE0 (PSX 0x8015D330): op E9's state 3 - the fall after the last step: the
// rise into the height and d into the rise; below the ground the height is
// the ground, state 0 and al 0; else al 1.
extern "C" unsigned char __cdecl MoveCmd_OpE9Fall(unsigned char*, signed char, signed char, unsigned short, unsigned short d,
                                                  unsigned char, unsigned char) {
    rv::Store<unsigned>(rv::As<records::SpriteHead>(Sc())->height_whole, rv::As<records::SpriteHead>(Sc())->height_whole + static_cast<U>(rv::As<records::SpriteHead>(Sc())->step_y >> 8));
    rv::As<records::SpriteHead>(Sc())->step_y = static_cast<std::int32_t>(static_cast<U>(rv::As<records::SpriteHead>(Sc())->step_y) + static_cast<U>(static_cast<short>(d)));
    const long ground = SH_CALL(AreaMap_Elevation)(rv::As<records::SpriteHead>(Sc())->x, rv::As<records::SpriteHead>(Sc())->z);
    unsigned char* const sc = Sc();
    if (S16(static_cast<U>(ground)) > S16(rv::As<records::SpriteHead>(sc)->height_whole)) {
        rv::Store<unsigned>(rv::As<records::SpriteHead>(sc)->height_whole, static_cast<U>(ground));
        Sc()[4] = 0;
        return 0;
    }
    return 1;
}

// original 0x57CD40 (PSX 0x8015D3BC): op DB (MoveScript_GroupD 0x577F20; MoveCmd_OpE9Arc;
// Scena06_LeapAir): x and z each plus its velocity when that is above 0, then
// rounded down to the half unit (& 0xFFFF8000).
extern "C" void __cdecl MoveCmd_OpDB(void) {
    records::SpriteHead* sc = rv::As<records::SpriteHead>(Sc());
    if (sc->step_x > 0) {
        sc->x = static_cast<std::int32_t>(static_cast<U>(sc->x) + static_cast<U>(sc->step_x));
        sc = rv::As<records::SpriteHead>(Sc());
    }
    sc->x = static_cast<std::int32_t>(static_cast<U>(sc->x) & 0xFFFF8000u);
    sc = rv::As<records::SpriteHead>(Sc());
    if (sc->step_z > 0) {
        sc->z = static_cast<std::int32_t>(static_cast<U>(sc->z) + static_cast<U>(sc->step_z));
        sc = rv::As<records::SpriteHead>(Sc());
    }
    sc->z = static_cast<std::int32_t>(static_cast<U>(sc->z) & 0xFFFF8000u);
}

// original 0x57CD90 (PSX 0x8015D438): the first Sprite_Objects record (of 30) whose +0 is 0,
// al its index; none, al 0xFF (the rest of eax is the caller's). PSX twin
// 0x8015D438 (pairs_propagated "call-anchored").
extern "C" unsigned char __cdecl Sprite_FindFree(void) {
    for (unsigned i = 0; i < scena_sx::at::kSpriteCount; ++i)
        if (Sprite_Objects[i].flags == 0) return static_cast<unsigned char>(i);
    return 0xFF;
}

// original 0x57CDC0: the frame script's position after its next step (PSX
// 0x8015D48C), for op FE: unless the tick byte +0x4A is 1, the position +0x58
// as it is; with it, 2 when position / 2 is the length byte +0x49, else past
// the entry at +0x50 + position - 2 bytes, or 2 + 2 * its second byte when
// its first is 0x80 or more.
// As the original has it: position / 2 is compared in 16 bits; the sums wrap
// at 16 bits.
extern "C" unsigned short __cdecl Sprite_ScriptPeek(void) {
    const records::SpriteHead* const sprite = rv::Current<records::SpriteHead>();
    if (sprite->script_hold != 1) return sprite->script_pos;
    const std::uint16_t position = sprite->script_pos;
    if (static_cast<std::uint16_t>(position >> 1) == sprite->script_steps) return 2;
    const unsigned char* const data = At(static_cast<std::uint32_t>(Long(rv::Bytes(sprite) + 0x50)));   // a 32-bit word
    const unsigned char* const entry = data + position;
    if (entry[0] >= 0x80) return static_cast<unsigned short>(entry[1] * 2u + 2u);
    return static_cast<unsigned short>(position + 2u);
}

extern "C" unsigned char __cdecl Effect_Spawn(unsigned char kind, signed char a, signed char b, short x, short z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (EffectLogOn())
        bof3::Log("effect      spawn kind 0x%02X a %d b %d x %d z %d slot %u frame %lu", kind, a, b, x, z, slot,
                  static_cast<unsigned long>(Frame_Counter));
    if (slot == 0xFF) return slot;
    if (slot >= 20) bof3::Fatal("Effect_Spawn (0x57CE10): Effect_FindFree answered %u, past the 20 records", slot);
    unsigned char* const r = rv::Bytes(&Effect_Objects[slot]);
    r[0] = 1;
    r[5] = 6;
    r[6] = kind;
    PutRest2b<std::int32_t>(r, 0xC, a);
    PutRest2b<std::int32_t>(r, 0x10, b);
    PutRest2b<std::uint16_t>(r, 0x2E, static_cast<std::uint16_t>(x));
    PutRest2b<std::uint16_t>(r, 0x30, static_cast<std::uint16_t>(z));
    return slot;
}

// original 0x57CE80 (PSX 0x8015D5F0): Effect_Spawn's sibling of kind 0x19 - the same record
// with +5 = 0x19 and the dwords x, y, z at +0x34, +0x38, +0x3C.
extern "C" unsigned char __cdecl Effect_SpawnAt(unsigned char kind, signed char a, signed char b, long x, long y, long z) {
    const unsigned char slot = SH_CALL(Effect_FindFree)();
    if (EffectLogOn())
        bof3::Log("effect      spawnAt kind 0x%02X a %d b %d x %ld y %ld z %ld slot %u frame %lu", kind, a, b, x, y, z,
                  slot, static_cast<unsigned long>(Frame_Counter));
    if (slot == 0xFF) return slot;
    if (slot >= 20) bof3::Fatal("Effect_SpawnAt (0x57CE80): Effect_FindFree answered %u, past the 20 records", slot);
    unsigned char* const r = rv::Bytes(&Effect_Objects[slot]);
    r[0] = 1;
    r[5] = 0x19;
    r[6] = kind;
    PutRest2b<std::int32_t>(r, 0xC, a);
    PutRest2b<std::int32_t>(r, 0x10, b);
    PutRest2b<std::int32_t>(r, 0x34, x);
    PutRest2b<std::int32_t>(r, 0x38, y);
    PutRest2b<std::int32_t>(r, 0x3C, z);
    return slot;
}

void EventCond_Inject() {
    BOF3_INJECT(Party_ExtraScreens);
    BOF3_INJECT(Party_ExtraFrame);
    BOF3_INJECT(Sprite_UpdateScreenA);
    BOF3_INJECT(Sprite_ProjectA);
    BOF3_INJECT(Sprite_InitFromEntry);
    BOF3_INJECT(Sprite_AddDrawRecords);
    BOF3_INJECT(Light_ObjectDirection);
    BOF3_INJECT(Sprite_ObjectMatrix);
    BOF3_INJECT(Camera_LoadMatrix);
    BOF3_INJECT(Sprite_ObjectByHandle);
    BOF3_INJECT(Flags_Set);
    BOF3_INJECT(Flags_Clear);
    BOF3_INJECT(Flags_Test);
    BOF3_INJECT(Flags_Toggle);
    BOF3_INJECT(EventCond_ByteFA);
    BOF3_INJECT(EventCond_Area);
    BOF3_INJECT(EventCond_Flag);
    BOF3_INJECT(EventCond_Run);
    BOF3_INJECT(EventCond_ByteFD);
    BOF3_INJECT(EventCond_Status1);
    BOF3_INJECT(EventCond_LeaderId);
    BOF3_INJECT(EventCond_StoryFlag);
    BOF3_INJECT(EventCond_KeyItem);
    BOF3_INJECT(EventCond_ChapterAtMost);
    BOF3_INJECT(EventCond_RecordBit0);
    BOF3_INJECT(EventCond_Gene);
    BOF3_INJECT(MoveScript_Variable);
    BOF3_INJECT(EventCond_Counter0);
    BOF3_INJECT(EventCond_Counter1);
    BOF3_INJECT(EventCond_Counter2);
    BOF3_INJECT(EventCond_Counter3);
    BOF3_INJECT(MoveScript_CounterSet);
    BOF3_INJECT(MoveScript_CounterTest);
    BOF3_INJECT(MoveScript_CounterStep);
    BOF3_INJECT(Sprite_FaceDirection);
    BOF3_INJECT(Camera_TurnToDegrees);
    BOF3_INJECT(Camera_TurnStep);
    BOF3_INJECT(Camera_TurnFBToDegrees);
    BOF3_INJECT(Camera_TurnStepFB);
    BOF3_INJECT(Camera_EaseAngleFB);
    BOF3_INJECT(ScriptFlags_Clear40);
    BOF3_INJECT(ScriptFlags_Set40);
    BOF3_INJECT(ObjTrio_ClearBit40);
    BOF3_INJECT(ObjTrio_SetBit40);
    BOF3_INJECT(MoveScript_ObjectKind);
    BOF3_INJECT(Member_SetState2_8);
    BOF3_INJECT(MoveCmd_OpE9);
    BOF3_INJECT(MoveCmd_OpE9Start);
    BOF3_INJECT(MoveCmd_OpE9Arc);
    BOF3_INJECT(MoveCmd_OpE9Kind2);
    BOF3_INJECT(MoveCmd_OpE9Fall);
    BOF3_INJECT(MoveCmd_OpDB);
    BOF3_INJECT(Sprite_FindFree);
    BOF3_INJECT(Sprite_ScriptPeek);
    BOF3_INJECT(Effect_Spawn);
    BOF3_INJECT(Effect_SpawnAt);
}
