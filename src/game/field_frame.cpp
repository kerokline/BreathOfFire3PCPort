// The field's frame loop, from its per-frame entries down to the overlay
// list's draw: originals 0x517200 and 0x517240 (the entries), 0x517350 (the
// three party members), 0x5173E0 and 0x517490 (the 30 field objects' screen
// update and frame), 0x57B780 / 0x57B7B0 (the same for the four extra party
// objects), 0x57B830 / 0x57B860 (a type-0x0A sprite's screen update),
// 0x592F00, 0x592F20 and 0x593020 (the draw at the frame's end, the overlay
// list's sort and draw). docs/field-frame.md.
#include "game/field_frame.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/move_script_bytes.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

using namespace move_script;

std::uint32_t Address(const volatile void* p) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(p)); }

// A function pointer as another function type, through an integer.
template <class To, class From> To Cast(From f) { return reinterpret_cast<To>(reinterpret_cast<std::uintptr_t>(f)); }

constexpr unsigned kObjectBytes = 0xA4, kObjects = 30, kExtra = 4, kMemberBytes = 0x14C;

// The record a type-0x0A sprite points at from +0x54 (bytes 1 and 3 read here).
const unsigned char* Record54(const unsigned char* sprite) {
    std::uint32_t p;
    std::memcpy(&p, sprite + 0x54, sizeof p);
    return At(p);
}

using Handler = void (__cdecl*)(unsigned char*);
void CallHandler(unsigned char* object) {
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(Field_ObjectHandlers[object[1]]))(object);
}

// Every callee, through pointers so that the start-up fuzz can stand
// recording functions in for them - for the original's copies and for ours
// alike, so that each of the twelve is tested alone. The ones this file
// implements are here too, called through the same pointers.
struct Callees {
    // the entries' calls, in 0x517240's order
    void (__cdecl* mode_dispatch)();
    void (__cdecl* task_records)();
    void (__cdecl* kind2)();
    void (__cdecl* extra_frame)();
    void (__cdecl* members)();
    void (__cdecl* objects)();
    void (__cdecl* area_map)();
    void (__cdecl* extra_screens)();
    void (__cdecl* party_screens)();
    void (__cdecl* objects_screen)();
    void (__cdecl* effects)();
    void (__cdecl* slots)();
    void (__cdecl* tint)();
    void (__cdecl* draw)();
    // Field_MembersFrame's
    void (__cdecl* pending)();
    void (__cdecl* leader)();
    void (__cdecl* member)();
    // the screen updates'
    void (__cdecl* screen_a)();
    void (__cdecl* update_screen)();
    void (__cdecl* project_a)();
    // the object frames'
    unsigned char (__cdecl* turn_target)(unsigned char*);
    void (__cdecl* face)(unsigned char);
    void (__cdecl* idle)(unsigned char*);
    void (__cdecl* linked)();
    void (__cdecl* idle_long)(unsigned char*);
    unsigned char (__cdecl* tick)();
    unsigned char (__cdecl* tick_once)();
    // Sprite_ProjectA's
    void (__cdecl* inherit_key)();
    void (__cdecl* load_vertex)(const unsigned long*);
    void (__cdecl* rtps)();
    void (__cdecl* store_xy)(unsigned long*);
    void (__cdecl* store_depth)(long*);
    // the frame's draw
    void (__cdecl* header_pass)();
    void (__cdecl* overlays)();
    void (__cdecl* draw_pass)();
    void (__cdecl* swap)(unsigned long, unsigned long);
    void (__cdecl* sprite_draw)();
};
// MoveScript_SetTurnTarget returns 0 or 1 in al (0x517EAA, 0x517F21), which
// Field_ObjectsFrame tests; symbols.toml still types it void, and
// move_groups.cpp calls it so. Read here with the type the body has.
const Callees kOriginals = {
    Field_ModeDispatch, Field_RunTaskRecords, Kind2_Run, Party_ExtraFrame, Field_MembersFrame, Field_ObjectsFrame,
    AreaMap_Frame, Party_ExtraScreens, Party_UpdateScreens, Field_ObjectsScreen, Effect_RunObjects, Field_RunSlots,
    MoveScript_TintFrame, Field_DrawFrame,
    Field_PendingJump, Field_LeaderFrame, Field_MemberFrame,
    Sprite_UpdateScreenA, Sprite_UpdateScreen, Sprite_ProjectA,
    Cast<unsigned char (__cdecl*)(unsigned char*)>(MoveScript_SetTurnTarget),
    Sprite_FaceDirection, Field_ObjectIdle, Field_ObjectLinked, Field_ObjectIdleLong, Sprite_ScriptTick,
    Sprite_ScriptTickOnce,
    Sprite_InheritDrawKey, Gte_LoadVertex, Gte_Rtps, Gte_StoreScreenXY, Gte_StoreDepthQuarter,
    AreaMap_HeaderPass, Sprite_DrawOverlays, Sprite_DrawPass, Sprite_SwapOverlays, Sprite_Draw,
};
Callees g = kOriginals;

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
// `fild dword` then `fstp dword` of a sign-extended s16: exact.
std::uint32_t FloatBits(std::int16_t v) {
    const float f = static_cast<float>(v);
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    return bits;
}

}  // namespace

// original 0x517200: the field's frame (PSX FUN_8019A1B8, call for call).
extern "C" void __cdecl Field_Frame(void) {
    g.kind2();
    g.extra_frame();
    g.members();
    g.objects();
    g.area_map();
    g.extra_screens();
    g.party_screens();
    g.objects_screen();
    g.effects();
    g.slots();
    g.tint();
    g.draw();   // a tail jmp in the original
}

// original 0x517240: the same frame with the area's mode dispatch and the
// task records first (PSX FUN_8019A230, call for call).
extern "C" void __cdecl Field_FrameScripted(void) {
    g.mode_dispatch();
    g.task_records();
    g.kind2();
    g.extra_frame();
    g.members();
    g.objects();
    g.area_map();
    g.extra_screens();
    g.party_screens();
    g.objects_screen();
    g.effects();
    g.slots();
    g.tint();
    g.draw();   // a tail jmp in the original
}

// original 0x517350: the party members' frames (PSX FUN_8019A440). Bit n of
// the two flag words' low bytes holds member n still; the second member runs
// only with more than one, the third only with exactly three - four or more
// run two, as the original has it. Flags and count are re-read after every
// call, as the original's loads are.
extern "C" void __cdecl Field_MembersFrame(void) {
    g.pending();
    const auto held = [] { return static_cast<unsigned char>(Field_ScriptFlags2 | Field_ScriptFlags); };
    if (!(held() & 1)) {
        Field_State = ObjTrio;
        Sprite_Current = ObjTrio;
        g.leader();
    }
    if (Field_MemberCount <= 1) return;
    if (!(held() & 2)) {
        Field_State = ObjTrio + kMemberBytes;
        Sprite_Current = ObjTrio + kMemberBytes;
        g.member();
    }
    if (Field_MemberCount != 3) return;
    if (!(held() & 4)) {
        Field_State = ObjTrio + 2 * kMemberBytes;
        Sprite_Current = ObjTrio + 2 * kMemberBytes;
        g.member();   // a tail jmp in the original
    }
}

// original 0x5173E0: the 30 field objects' screen update (PSX FUN_801A151C).
//
// As the original has it: a type-9 object keeps whatever slot byte +0x29 it
// had (and is skipped entirely under Draw_OtSlot 4); the type-0x0A test reads
// Sprite_Current, which is the object here since nothing runs in between.
extern "C" void __cdecl Field_ObjectsScreen(void) {
    unsigned char* object = Sprite_Objects;
    for (unsigned n = kObjects; n != 0; --n, object += kObjectBytes) {
        Sprite_Current = object;
        if (!(object[0] & 1)) continue;
        if (object[6] == 9) {
            if (Draw_OtSlot == 4) continue;
        } else {
            object[0x29] = (object[0x24] & 0x10) ? 4 : Draw_OtSlot;
        }
        if (Sprite_Current[6] == 0x0A) g.screen_a();
        else g.update_screen();
    }
}

// original 0x517490: the 30 field objects' frame (PSX FUN_801A16E0). The
// object's script context is +0x80; +9 counts a timed move's frames.
//
// As the original has it: Field_ObjectLinked ends an object's turn with
// nothing after it; everything after a callee reads Sprite_Current and the
// context byte afresh, since the callee may move or change them; the tick
// count is max(+0x84 - 2, 0) + 1, +0x84 read once before the ticks; after the
// turn target, the face-ObjTrio branch stores (ObjTrio[8] ^ 4) & 7 - the
// original also compares the sprite's direction with ObjTrio's there and
// branches on (differ ^ 4), which is never 0: a dead test with no effect,
// not reproduced.
extern "C" void __cdecl Field_ObjectsFrame(void) {
    if (Field_ScriptFlags2 & 0x400) return;
    unsigned char* object = Sprite_Objects;
    for (unsigned n = kObjects; n != 0; --n, object += kObjectBytes) {
        Sprite_Current = object;
        Field_ActiveMember = object;
        if (!(object[0] & 1)) continue;
        Scratch()[3] = 0;
        unsigned char* const context = object + 0x80;
        const unsigned char timed = object[9];
        const unsigned char flags = context[0];
        if (timed == 0 && (flags & 0x20)) {
            if (flags & 0x10) {
                const unsigned char turned = g.turn_target(context);
                unsigned char* const sprite = Sprite_Current;
                if (turned != 0 && !(sprite[7] & 8)) {
                    object[0x85] = static_cast<unsigned char>(sprite[8] & 7);
                    if (sprite[7] & 2) {
                        g.face(static_cast<unsigned char>(sprite[8] & 7));
                    } else {   // turn to face ObjTrio's first member
                        sprite[8] = static_cast<unsigned char>((ObjTrio[8] ^ 4) & 7);
                        g.face(Sprite_Current[8]);
                    }
                    if (Sprite_Current[1] == 0x0A) object[0x85] |= 1;
                }
            } else {
                g.idle(object);
            }
        } else {
            if (context[0] & 1) {
                g.linked();
                continue;
            }
            if (Word(object + 0x9C) > 0x1E && timed == 0) g.idle_long(object);
            else CallHandler(object);
        }
        unsigned char* const sprite = Sprite_Current;
        if (sprite[6] == 0x0A) continue;
        const unsigned char after = context[0];
        if (after & 0x40) {
            if (sprite[7] & 0x20) continue;
            if (sprite[0] & 0x10) g.tick();
            else g.tick_once();
        } else if (after & 8) {
            g.face(sprite[8]);
            context[0] &= 0xF7;
        } else if (!(sprite[7] & 0x20)) {
            const int extra = static_cast<int>(object[0x84]) - 2;
            const unsigned ticks = (extra < 0 ? 0u : static_cast<unsigned>(extra) & 0xFF) + 1;
            for (unsigned i = 0; i < ticks; ++i) g.tick();
        }
    }
    Field_EdgeBitsPrev = Field_EdgeBits;
}

// original 0x57B780: the four extra party objects' screen update (PSX
// FUN_8015B148): Sprite_UpdateScreenA for each present (bit 0) and not
// hidden by bit 6.
extern "C" void __cdecl Party_ExtraScreens(void) {
    unsigned char* object = Sprite_ObjectsExtra;
    for (unsigned n = kExtra; n != 0; --n, object += kObjectBytes) {
        Sprite_Current = object;
        const unsigned char f = object[0];
        if ((f & 1) && !(f & 0x40)) g.screen_a();
    }
}

// original 0x57B7B0: the four extra party objects' frame (PSX FUN_8015B1D8) -
// Field_ObjectsFrame's first half without the idle-long case, the scratch
// byte, the turn target's result or anything after the handler.
extern "C" void __cdecl Party_ExtraFrame(void) {
    unsigned char* object = Sprite_ObjectsExtra;
    for (unsigned n = kExtra; n != 0; --n, object += kObjectBytes) {
        Sprite_Current = object;
        Field_ActiveMember = object;
        if (!(object[0] & 1)) continue;
        unsigned char* const context = object + 0x80;
        const unsigned char flags = context[0];
        if (object[9] == 0 && (flags & 0x20)) {
            if (flags & 0x10) g.turn_target(context);
            else g.idle(object);
        } else if (context[0] & 1) {
            g.linked();
        } else {
            CallHandler(object);
        }
    }
}

// original 0x57B830: a type-0x0A sprite's ordering slot +0x29 (PSX
// FUN_8015B334's head) - 4 with +0x24 bit 4, 6 for a record with byte 3 bit
// 5, else Draw_OtSlot - then its screen position (a tail jmp in the original).
extern "C" void __cdecl Sprite_UpdateScreenA(void) {
    unsigned char* const sprite = Sprite_Current;
    if (sprite[0x24] & 0x10) sprite[0x29] = 4;
    else sprite[0x29] = (Record54(sprite)[3] & 0x20) ? 6 : Draw_OtSlot;
    g.project_a();
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
extern "C" void __cdecl Sprite_ProjectA(void) {
    unsigned char* sprite = Sprite_Current;
    const unsigned z_whole = Word(sprite + 0x38) == 0, x_whole = Word(sprite + 0x34) == 0;
    SetWord(sprite + 0x32, static_cast<unsigned>(Word(sprite + 0x3A) - z_whole - x_whole + Word(sprite + 0x36) -
                                                 static_cast<std::uint16_t>(MapView_Origin[1]) -
                                                 static_cast<std::uint16_t>(MapView_Origin[0]) + 2));
    sprite = Sprite_Current;
    SetWord(sprite + 0x32, static_cast<unsigned>(Word(sprite + 0x32) + static_cast<signed char>(Record54(sprite)[1])));
    sprite = Sprite_Current;
    SetWord(sprite + 0x32, static_cast<unsigned>(sprite[0x32]) << 8);
    unsigned low;
    sprite = Sprite_Current;
    if (Draw_OtSlot == 4) low = static_cast<unsigned>(Long(sprite + (Draw_SortOnX ? 0x34 : 0x38)) >> 15) & 0xFF;
    else low = ((Word(sprite + 0x3E) ^ 0xF01Fu) >> 5) & 0xFF;
    SetWord(sprite + 0x32, Word(sprite + 0x32) | low);
    sprite = Sprite_Current;
    if (!(Record54(sprite)[3] & 0x10)) {
        g.inherit_key();
        sprite = Sprite_Current;
    }
    short vertex[4];
    vertex[0] = static_cast<short>((Long(sprite + 0x34) >> 9) - 0x4000);
    vertex[1] = static_cast<short>((Long(sprite + 0x38) >> 9) - 0x4000);
    vertex[2] = static_cast<short>(-(static_cast<std::int16_t>(Word(sprite + 0x3E)) / 2));
    vertex[3] = 0;   // DIV-0023
    unsigned long vertex_words[2];
    std::memcpy(vertex_words, vertex, sizeof vertex_words);
    g.load_vertex(vertex_words);
    g.rtps();
    unsigned long screen[2];
    g.store_xy(screen);
    g.store_depth(reinterpret_cast<long*>(Sprite_Current + 0x60));
    SetLong(Sprite_Current + 0x74, static_cast<std::int32_t>(ThroughX87(static_cast<std::uint32_t>(screen[0]))));
    SetWord(Sprite_Current + 0x2E, Ftol16(static_cast<std::uint32_t>(screen[0])));
    SetLong(Sprite_Current + 0x78, static_cast<std::int32_t>(ThroughX87(static_cast<std::uint32_t>(screen[1]))));
    SetWord(Sprite_Current + 0x30, Ftol16(static_cast<std::uint32_t>(screen[1])));

    sprite = Sprite_Current;
    const unsigned char* const margin = Sprite_ScreenMargins + ((Record54(sprite)[3] >> 3) & 1) * 4u;
    const int x = static_cast<std::int16_t>(Word(sprite + 0x2E)), y = static_cast<std::int16_t>(Word(sprite + 0x30));
    if (x > 0x140 + margin[0] || x < -static_cast<int>(margin[1]) || y > 0xF0 + margin[2] || y < -static_cast<int>(margin[3])) {
        sprite[0] |= 0x80;   // off screen
        return;
    }
    sprite[0] &= 0x7F;
    const unsigned char n = Sprite_DrawListCount;
    if (n >= 0x28) return;
    Sprite_DrawListCount = static_cast<unsigned char>(n + 1);
    Sprite_DrawList[n] = Sprite_Current;
}

// original 0x592F00: the draw at the end of the field's frame (PSX
// FUN_8014B948), and both sprite lists emptied for the next.
extern "C" void __cdecl Field_DrawFrame(void) {
    g.header_pass();
    g.overlays();
    g.draw_pass();
    Sprite_OverlayListCount = 0;
    Sprite_DrawListCount = 0;
}

// original 0x592F20: the overlay list, sorted by height (+0x3E, s16) and
// drawn, when Draw_PassFlags has bit 0.
//
// As the original has it: a bubble sort, the smallest height rising to the
// front, stable (only a strictly smaller neighbour moves); the list's count
// re-read after every exchange and every draw, so a callee that changes it
// changes the bounds; the screen floats +0x74 / +0x78 rewritten from the
// integer position +0x2E / +0x30 before each draw.
extern "C" void __cdecl Sprite_DrawOverlays(void) {
    if (!(Draw_PassFlags & 1)) return;
    unsigned char count = Sprite_OverlayListCount;
    if (static_cast<int>(count) - 1 > 0) {
        unsigned char i = 0;
        do {
            for (unsigned char j = static_cast<unsigned char>(count - 1); j >= i + 1u; --j) {
                const auto here = static_cast<std::int16_t>(Word(Sprite_OverlayList[j] + 0x3E));
                const auto before = static_cast<std::int16_t>(Word(Sprite_OverlayList[j - 1] + 0x3E));
                if (here < before) {
                    g.swap(j, static_cast<unsigned char>(j - 1));
                    count = Sprite_OverlayListCount;
                }
            }
            ++i;
        } while (static_cast<int>(i) < static_cast<int>(count) - 1);
    }
    if (count == 0) return;
    unsigned char i = 0;
    do {
        unsigned char* const sprite = Sprite_OverlayList[i];
        Sprite_Current = sprite;
        SetLong(sprite + 0x74, static_cast<std::int32_t>(FloatBits(static_cast<std::int16_t>(Word(sprite + 0x2E)))));
        SetLong(Sprite_Current + 0x78, static_cast<std::int32_t>(FloatBits(static_cast<std::int16_t>(Word(Sprite_Current + 0x30)))));
        g.sprite_draw();
        ++i;
    } while (i < Sprite_OverlayListCount);
}

// original 0x593020: exchanges two overlay list entries. The caller pushes
// whole registers with stale upper bytes; the original masks them, so do we.
extern "C" void __cdecl Sprite_SwapOverlays(unsigned long a, unsigned long b) {
    unsigned char* const first = Sprite_OverlayList[a & 0xFF];
    unsigned char* const second = Sprite_OverlayList[b & 0xFF];
    Sprite_OverlayList[b & 0xFF] = first;
    Sprite_OverlayList[a & 0xFF] = second;
}

namespace {

// --- BOF3X_SHADOW=field_frame: a differential fuzz, once at start-up --------
// Twelve byte-copies, every call out of them re-aimed at the recorders below
// (their calls of one another included, so each is tested alone) - all but
// Sprite_ProjectA's two of the CRT's _ftol, a pure x87 conversion the copy
// keeps. The indirect calls go through Field_ObjectHandlers, whose eleven
// entries are recorders for the fuzz's duration, for both sides. One round:
// one of the twelve; random bytes in the 30 field objects, the four extra
// party objects, ObjTrio's three members and the lists, with each branch's
// boundaries seeded; theirs, then from the same state ours; all of it and
// the recorders' log compared.

constexpr unsigned kLog = 512, kRecords = 8;
unsigned char g_records[kRecords * 4];   // what the objects' +0x54 point at
struct Entry { std::uint32_t what, current, a, b; };
Entry g_log[kLog];
unsigned g_log_n, g_seed;

std::uint32_t Hash() {
    std::uint32_t h = (g_seed + g_log_n) * 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA6Bu;
    h ^= h >> 13;
    return h;
}
void Record(std::uint32_t what, std::uint32_t a = 0, std::uint32_t b = 0) {
    if (g_log_n < kLog) g_log[g_log_n] = {what, Address(Sprite_Current), a, b};
    ++g_log_n;
}

// The objects a pointer may name: the 30, the four extra and ObjTrio's three.
constexpr unsigned kPool = kObjects + kExtra + 3;
unsigned char* Pool(unsigned k) {
    k %= kPool;
    if (k < kObjects) return Sprite_Objects + k * kObjectBytes;
    if (k < kObjects + kExtra) return Sprite_ObjectsExtra + (k - kObjects) * kObjectBytes;
    return ObjTrio + (k - kObjects - kExtra) * kMemberBytes;
}

// A callee may change what the caller reads again after it: which sprite is
// current and its type, flags, direction; the loop object's context byte and
// tick count; the members' flags and count; the list counts - never past
// their lists, nor an object's handler index or record pointer.
void Disturb() {
    const std::uint32_t h = Hash();
    if (h % 4 == 0) Sprite_Current = Pool(h >> 8);
    if (h % 5 == 0) {
        unsigned char* const s = Sprite_Current;
        s[0] = static_cast<unsigned char>(h >> 8);
        s[6] = (h >> 16) % 3 == 0 ? 0x0A : static_cast<unsigned char>(h >> 20) % 12;
        s[7] = static_cast<unsigned char>(h >> 24);
        s[8] = static_cast<unsigned char>(h >> 12);
        s[1] = (h >> 18) % 3 == 0 ? 0x0A : static_cast<unsigned char>(h >> 22) % 11;
    }
    if (h % 6 == 0) {
        Field_ActiveMember[0x80] = static_cast<unsigned char>(h >> 8);
        Field_ActiveMember[0x84] = static_cast<unsigned char>(h >> 16) % 6;
    }
    if (h % 7 == 0) {
        Field_ScriptFlags = static_cast<unsigned short>(h >> 8);
        Field_ScriptFlags2 = static_cast<unsigned short>(h >> 12);
        Field_MemberCount = static_cast<unsigned char>((h >> 24) % 5);
    }
    if (h % 9 == 0) {
        Sprite_OverlayListCount = static_cast<unsigned char>((h >> 8) % 31);
        Sprite_DrawListCount = static_cast<unsigned char>(0x25 + (h >> 16) % 6);
    }
}

// The recorders. The no-argument ones by an id per target.
template <unsigned Id> void __cdecl StubVoid() { Record(Id); Disturb(); }
template <unsigned Id> unsigned char __cdecl StubTick() {
    Record(Id);
    Disturb();
    return static_cast<unsigned char>(Hash());
}
template <unsigned Id> void __cdecl StubObject(unsigned char* object) { Record(Id, Address(object)); Disturb(); }
template <unsigned K> void __cdecl StubHandler(unsigned char* object) { Record(100 + K, Address(object)); Disturb(); }
unsigned char __cdecl StubTurnTarget(unsigned char* context) {
    Record(20, Address(context));
    Disturb();
    // The result from bits Disturb does not decide by: taken from the same low
    // bits, every moved Sprite_Current came with a 0 result, and a control that
    // stopped re-reading Sprite_Current after the call went unrefused.
    const std::uint32_t h = Hash();
    return static_cast<unsigned char>(h >> 31 ? (h >> 8) | 1 : 0);
}
// Byte arguments arrive as whole pushed registers from the copies: masked.
void __cdecl StubFace(std::uint32_t direction) { Record(21, direction & 0xFF); Disturb(); }
void __cdecl StubInheritKey() {
    Record(28, Word(Sprite_Current + 0x32));
    const std::uint32_t h = Hash();
    if (h % 3 == 0) SetWord(Sprite_Current + 0x32, h >> 8);
    if (h % 5 == 0) SetLong(Sprite_Current + 0x34, static_cast<std::int32_t>(Hash() * 3));
    if (h % 7 == 0) Sprite_Current = Pool(h >> 12);   // what the projection reads next
}
void __cdecl StubLoadVertex(const unsigned long* v) {   // x, y, z - not the pad (DIV-0023)
    const auto* s = reinterpret_cast<const std::uint16_t*>(v);
    Record(29, s[0] | static_cast<std::uint32_t>(s[1]) << 16, s[2]);
}
std::uint32_t FloatOf(float f) {
    std::uint32_t bits;
    std::memcpy(&bits, &f, sizeof bits);
    return bits;
}
// A screen coordinate: special floats, or one on or next to the cull's edge
// for the current sprite's margins (hi = 0x140 or 0xF0), halves included,
// so that truncation toward zero shows.
std::uint32_t ScreenFloat(unsigned axis) {
    static const std::uint32_t kFloats[] = {0x7F800001u, 0x7FC00000u, 0xFFA00000u, 0x7F800000u, 0xFF800000u,
                                            0x3F000000u, 0xBF000000u, 0x5F000000u, 0xDF000001u, 0x47000000u,
                                            0xC7000080u, 0x00000000u, 0x80000000u, 0x477FFF00u};
    const std::uint32_t h = Hash();
    ++g_log_n;   // a new hash for the next
    if (h % 4 == 0) return kFloats[(h >> 4) % (sizeof kFloats / sizeof kFloats[0])];
    if (h % 4 == 1) return FloatOf(static_cast<float>(static_cast<std::int32_t>(h >> 8) % 0x300 - 0x100) + ((h >> 4) % 4) * 0.25f);
    const unsigned char* const m = Sprite_ScreenMargins + ((Record54(Sprite_Current)[3] >> 3) & 1) * 4u;
    const int edge = (h >> 8) % 2 ? (axis ? 0xF0 : 0x140) + m[axis * 2] : -static_cast<int>(m[axis * 2 + 1]);
    const int near = edge + static_cast<int>((h >> 9) % 3) - 1;
    static const float kFraction[] = {0.0f, 0.5f, -0.5f, 0.999f, -0.999f};
    return FloatOf(static_cast<float>(near) + kFraction[(h >> 12) % 5]);
}
void __cdecl StubStoreXY(unsigned long* out) {
    Record(31);
    out[0] = ScreenFloat(0);
    out[1] = ScreenFloat(1);
    const std::uint32_t h = Hash();
    if (h % 5 == 0) Sprite_Current = Pool(h >> 8);   // it is re-read for the depth's address
    if (h % 7 == 0) Sprite_DrawListCount = static_cast<unsigned char>(0x26 + (h >> 16) % 4);
}
void __cdecl StubStoreDepth(long* out) {
    Record(32, Address(out));
    *out = static_cast<long>(Hash());
    const std::uint32_t h = Hash();
    if (h % 5 == 0) Sprite_Current = Pool(h >> 8);   // it is re-read for the stores
}
// The exchange does what the real one does - the sort reads the list after
// it - and may change the count the sort re-reads.
void __cdecl StubSwap(std::uint32_t a, std::uint32_t b) {
    Record(36, a & 0xFF, b & 0xFF);
    const std::uint32_t h = Hash();
    if (h % 8 != 0 && (a & 0xFF) < 30 && (b & 0xFF) < 30) {
        unsigned char* const t = Sprite_OverlayList[a & 0xFF];
        Sprite_OverlayList[a & 0xFF] = Sprite_OverlayList[b & 0xFF];
        Sprite_OverlayList[b & 0xFF] = t;
    }
    if (h % 13 == 0) Sprite_OverlayListCount = static_cast<unsigned char>((h >> 8) % 31);
}
void __cdecl StubSpriteDraw() {
    Record(37, static_cast<std::uint32_t>(Long(Sprite_Current + 0x74)), static_cast<std::uint32_t>(Long(Sprite_Current + 0x78)));
    const std::uint32_t h = Hash();
    if (h % 9 == 0) Sprite_OverlayListCount = static_cast<unsigned char>((h >> 8) % 31);
    if (h % 5 == 0) Sprite_Current = Pool(h >> 12);
}

const Callees kStubs = {
    StubVoid<13>, StubVoid<14>, StubVoid<1>, StubVoid<2>, StubVoid<3>, StubVoid<4>, StubVoid<5>, StubVoid<6>,
    StubVoid<7>, StubVoid<8>, StubVoid<9>, StubVoid<10>, StubVoid<11>, StubVoid<12>,
    StubVoid<15>, StubVoid<16>, StubVoid<17>,
    StubVoid<18>, StubVoid<19>, StubVoid<27>,
    StubTurnTarget, Cast<void (__cdecl*)(unsigned char)>(&StubFace), StubObject<22>, StubVoid<23>,
    StubObject<24>, StubTick<25>, StubTick<26>,
    StubInheritKey, StubLoadVertex, StubVoid<30>, StubStoreXY, StubStoreDepth,
    StubVoid<33>, StubVoid<34>, StubVoid<35>, Cast<void (__cdecl*)(unsigned long, unsigned long)>(&StubSwap),
    StubSpriteDraw,
};
using Raw = const void*;
const Raw kHandlerStubs[11] = {
    reinterpret_cast<Raw>(&StubHandler<0>), reinterpret_cast<Raw>(&StubHandler<1>), reinterpret_cast<Raw>(&StubHandler<2>),
    reinterpret_cast<Raw>(&StubHandler<3>), reinterpret_cast<Raw>(&StubHandler<4>), reinterpret_cast<Raw>(&StubHandler<5>),
    reinterpret_cast<Raw>(&StubHandler<6>), reinterpret_cast<Raw>(&StubHandler<7>), reinterpret_cast<Raw>(&StubHandler<8>),
    reinterpret_cast<Raw>(&StubHandler<9>), reinterpret_cast<Raw>(&StubHandler<10>),
};

Raw StubFor(std::uint32_t target) {
    const auto f = [](auto p) { return reinterpret_cast<Raw>(p); };
    switch (target) {
    case 0x573080: return f(&StubVoid<1>);    // Kind2_Run
    case 0x57B7B0: return f(&StubVoid<2>);    // Party_ExtraFrame
    case 0x517350: return f(&StubVoid<3>);    // Field_MembersFrame
    case 0x517490: return f(&StubVoid<4>);    // Field_ObjectsFrame
    case 0x56E6C0: return f(&StubVoid<5>);    // AreaMap_Frame
    case 0x57B780: return f(&StubVoid<6>);    // Party_ExtraScreens
    case 0x531B60: return f(&StubVoid<7>);    // Party_UpdateScreens
    case 0x5173E0: return f(&StubVoid<8>);    // Field_ObjectsScreen
    case 0x494030: return f(&StubVoid<9>);    // Effect_RunObjects
    case 0x455250: return f(&StubVoid<10>);   // Field_RunSlots
    case 0x454AD0: return f(&StubVoid<11>);   // MoveScript_TintFrame
    case 0x592F00: return f(&StubVoid<12>);   // Field_DrawFrame
    case 0x56D690: return f(&StubVoid<13>);   // Field_ModeDispatch
    case 0x59E230: return f(&StubVoid<14>);   // Field_RunTaskRecords
    case 0x533760: return f(&StubVoid<15>);   // Field_PendingJump
    case 0x52D8F0: return f(&StubVoid<16>);   // Field_LeaderFrame
    case 0x51AC50: return f(&StubVoid<17>);   // Field_MemberFrame
    case 0x57B830: return f(&StubVoid<18>);   // Sprite_UpdateScreenA
    case 0x588F20: return f(&StubVoid<19>);   // Sprite_UpdateScreen
    case 0x517E90: return f(&StubTurnTarget);
    case 0x57C4C0: return f(&StubFace);
    case 0x517F30: return f(&StubObject<22>);   // Field_ObjectIdle
    case 0x519600: return f(&StubVoid<23>);     // Field_ObjectLinked
    case 0x518B40: return f(&StubObject<24>);   // Field_ObjectIdleLong
    case 0x5893A0: return f(&StubTick<25>);     // Sprite_ScriptTick
    case 0x589410: return f(&StubTick<26>);     // Sprite_ScriptTickOnce
    case 0x57B860: return f(&StubVoid<27>);     // Sprite_ProjectA
    case 0x589770: return f(&StubInheritKey);
    case 0x5A8E30: return f(&StubLoadVertex);
    case 0x5A8E90: return f(&StubVoid<30>);     // Gte_Rtps
    case 0x5A90B0: return f(&StubStoreXY);
    case 0x5A94B0: return f(&StubStoreDepth);
    case 0x5B9550: return nullptr;              // the CRT's _ftol: the copy keeps it
    case 0x571AF0: return f(&StubVoid<33>);     // AreaMap_HeaderPass
    case 0x592F20: return f(&StubVoid<34>);     // Sprite_DrawOverlays
    case 0x593060: return f(&StubVoid<35>);     // Sprite_DrawPass
    case 0x593020: return f(&StubSwap);
    case 0x5935B0: return f(&StubSpriteDraw);
    default: bof3::Fatal("field_frame: no stand-in for a call to 0x%X", (unsigned)target); return nullptr;
    }
}

// The twelve copies and their calls out, listed by capstone 2026-09-22; every
// other jump stays inside, and the only indirect calls are the handler
// table's. Offsets are of the E8 or E9 byte.
struct Call { std::uint32_t offset, target; };
struct Clone { const char* name; std::uint32_t base, size; const Call* calls; int n_calls; };
constexpr Call kFrameCalls[] = {{0x0, 0x573080}, {0x5, 0x57b7b0}, {0xa, 0x517350}, {0xf, 0x517490}, {0x14, 0x56e6c0},
                                {0x19, 0x57b780}, {0x1e, 0x531b60}, {0x23, 0x5173e0}, {0x28, 0x494030}, {0x2d, 0x455250},
                                {0x32, 0x454ad0}, {0x37, 0x592f00}};
constexpr Call kScriptedCalls[] = {{0x0, 0x56d690}, {0x5, 0x59e230}, {0xa, 0x573080}, {0xf, 0x57b7b0}, {0x14, 0x517350},
                                   {0x19, 0x517490}, {0x1e, 0x56e6c0}, {0x23, 0x57b780}, {0x28, 0x531b60}, {0x2d, 0x5173e0},
                                   {0x32, 0x494030}, {0x37, 0x455250}, {0x3c, 0x454ad0}, {0x41, 0x592f00}};
constexpr Call kMembersCalls[] = {{0x0, 0x533760}, {0x28, 0x52d8f0}, {0x59, 0x51ac50}, {0x85, 0x51ac50}};
constexpr Call kScreenCalls[] = {{0x48, 0x57b830}, {0x4f, 0x588f20}};
constexpr Call kObjectsCalls[] = {{0x58, 0x517e90}, {0xb6, 0x57c4c0}, {0xd0, 0x517f30}, {0xe2, 0x519600}, {0xfb, 0x518b40},
                                  {0x12d, 0x57c4c0}, {0x162, 0x5893a0}, {0x177, 0x5893a0}, {0x17e, 0x589410}};
constexpr Call kExtraScreensCalls[] = {{0x1c, 0x57b830}};
constexpr Call kExtraFrameCalls[] = {{0x2f, 0x517e90}, {0x3a, 0x517f30}, {0x4a, 0x519600}};
constexpr Call kScreenACalls[] = {{0x29, 0x57b860}};
constexpr Call kProjectCalls[] = {{0xb5, 0x589770}, {0xf7, 0x5a8e30}, {0xfc, 0x5a8e90}, {0x106, 0x5a90b0},
                                  {0x115, 0x5a94b0}, {0x12d, 0x5b9550}, {0x14d, 0x5b9550}};
constexpr Call kDrawCalls[] = {{0x0, 0x571af0}, {0x5, 0x592f20}, {0xa, 0x593060}};
constexpr Call kOverlayCalls[] = {{0x61, 0x593020}, {0xe5, 0x5935b0}};
constexpr unsigned kCount = 12;
const Clone kClones[kCount] = {
    {"Field_Frame", 0x517200, 0x3C, kFrameCalls, 12},
    {"Field_FrameScripted", 0x517240, 0x46, kScriptedCalls, 14},
    {"Field_MembersFrame", 0x517350, 0x8B, kMembersCalls, 4},
    {"Field_ObjectsScreen", 0x5173E0, 0x60, kScreenCalls, 2},
    {"Field_ObjectsFrame", 0x517490, 0x1A3, kObjectsCalls, 9},
    {"Party_ExtraScreens", 0x57B780, 0x2D, kExtraScreensCalls, 1},
    {"Party_ExtraFrame", 0x57B7B0, 0x7B, kExtraFrameCalls, 3},
    {"Sprite_UpdateScreenA", 0x57B830, 0x2E, kScreenACalls, 1},
    {"Sprite_ProjectA", 0x57B860, 0x1FA, kProjectCalls, 7},
    {"Field_DrawFrame", 0x592F00, 0x1C, kDrawCalls, 3},
    {"Sprite_DrawOverlays", 0x592F20, 0xFE, kOverlayCalls, 2},
    {"Sprite_SwapOverlays", 0x593020, 0x36, nullptr, 0},
};
constexpr unsigned kSwap = 11;

struct Region { std::uint32_t at, size; };
const Region kRegions[] = {
    {Address(Sprite_Objects), Sprite_Objects_count},
    {Address(Sprite_ObjectsExtra), Sprite_ObjectsExtra_count},
    {Address(ObjTrio), ObjTrio_count},
    {Address(Sprite_OverlayList), 30 * 4},
    {Address(&Sprite_OverlayListCount), 1},
    {Address(Sprite_DrawList), 40 * 4},
    {Address(&Sprite_DrawListCount), 1},
    {bof3::addr::DamageScratch, 4},
    {Address(&Field_ScriptFlags), 2},
    {Address(&Field_ScriptFlags2), 2},
    {Address(&Field_MemberCount), 1},
    {Address(&Field_EdgeBits), 2},
    {Address(&Field_EdgeBitsPrev), 2},
    {Address(MapView_Origin), 4},
    {Address(&Draw_OtSlot), 1},
    {Address(&Draw_SortOnX), 1},
    {Address(&Draw_PassFlags), 1},
    {Address(g_records), sizeof g_records},
};
constexpr unsigned kRegionBytes = Sprite_Objects_count + Sprite_ObjectsExtra_count + ObjTrio_count + 120 + 1 + 160 + 1 + 4 +
                                  2 + 2 + 1 + 2 + 2 + 4 + 1 + 1 + 1 + kRecords * 4;

struct State {
    unsigned char memory[kRegionBytes];
    unsigned char* current;
    unsigned char* active;
    unsigned char* state;
    Entry log[kLog];
    unsigned log_n;
};
void Capture(State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(s.memory + at, At(r.at), r.size); at += r.size; }
    s.current = Sprite_Current;
    s.active = Field_ActiveMember;
    s.state = Field_State;
    std::memcpy(s.log, g_log, sizeof s.log);
    s.log_n = g_log_n;
}
void Apply(const State& s) {
    unsigned at = 0;
    for (const Region& r : kRegions) { std::memcpy(At(r.at), s.memory + at, r.size); at += r.size; }
    Sprite_Current = s.current;
    Field_ActiveMember = s.active;
    Field_State = s.state;
    std::memset(g_log, 0, sizeof g_log);
    g_log_n = 0;
}

std::uint32_t g_rng = 0x2545F491u;
std::uint32_t Next() { g_rng ^= g_rng << 13; g_rng ^= g_rng >> 17; g_rng ^= g_rng << 5; return g_rng; }
bool Often() { return Next() % 3 == 0; }
bool Half() { return Next() % 2 == 0; }

// The boundaries of one object's bytes, as the loops and the screen updates
// test them.
void SeedObject(unsigned char* o) {
    o[0] = static_cast<unsigned char>(Next() % 4 ? (Next() | 1) : (Next() & ~1u));
    o[1] = static_cast<unsigned char>(Often() ? 0x0A : Next() % 11);   // the handler index - never past the table
    o[6] = static_cast<unsigned char>(Often() ? 0x0A : Often() ? 9 : Next() % 12);
    o[7] = static_cast<unsigned char>(Next());
    o[8] = static_cast<unsigned char>(Next());
    o[9] = static_cast<unsigned char>(Half() ? 0 : Next() % 4);
    o[0x24] = static_cast<unsigned char>(Next());
    const std::uint32_t record = Address(g_records + (Next() % kRecords) * 4);
    std::memcpy(o + 0x54, &record, sizeof record);
    // height: a few values, so that the sort meets ties, and the s16 edges
    static const std::uint16_t kHeights[] = {0, 1, 2, 0x7FFF, 0x8000, 0xFFFF, 0x100};
    SetWord(o + 0x3E, Half() ? kHeights[Next() % 7] : Next());
    if (Often()) SetWord(o + 0x34, 0);
    if (Often()) SetWord(o + 0x38, 0);
    o[0x80] = static_cast<unsigned char>(Next());
    if (Half()) o[0x80] |= 0x20;
    o[0x84] = static_cast<unsigned char>(Next() % 4 ? Next() % 5 : Next());
    static const std::uint16_t k9C[] = {0x1D, 0x1E, 0x1F, 0x20, 0, 0xFFFF};
    SetWord(o + 0x9C, Half() ? k9C[Next() % 6] : Next());
}

void SelfTest(void* const (&theirs)[kCount]) {
    constexpr unsigned kRounds = 24000;
    unsigned region_bytes = 0;
    for (const Region& r : kRegions) region_bytes += r.size;
    if (region_bytes != kRegionBytes) bof3::Fatal("field_frame: regions are %u bytes, the state holds %u", region_bytes, kRegionBytes);
    static State saved, input, their_out, our_out;
    Capture(saved);
    unsigned long saved_handlers[11];
    std::memcpy(saved_handlers, Field_ObjectHandlers, sizeof saved_handlers);
    for (unsigned k = 0; k < 11; ++k) Field_ObjectHandlers[k] = Address(kHandlerStubs[k]);
    g = kStubs;
    const void* const ours[kCount] = {
        reinterpret_cast<Raw>(&Field_Frame), reinterpret_cast<Raw>(&Field_FrameScripted), reinterpret_cast<Raw>(&Field_MembersFrame),
        reinterpret_cast<Raw>(&Field_ObjectsScreen), reinterpret_cast<Raw>(&Field_ObjectsFrame), reinterpret_cast<Raw>(&Party_ExtraScreens),
        reinterpret_cast<Raw>(&Party_ExtraFrame), reinterpret_cast<Raw>(&Sprite_UpdateScreenA), reinterpret_cast<Raw>(&Sprite_ProjectA),
        reinterpret_cast<Raw>(&Field_DrawFrame), reinterpret_cast<Raw>(&Sprite_DrawOverlays), reinterpret_cast<Raw>(&Sprite_SwapOverlays),
    };

    unsigned bad = 0, calls = 0, per[kCount] = {}, drawn = 0, culled = 0, swaps = 0;
    for (unsigned round = 0; round < kRounds; ++round) {
        const unsigned k = round % kCount;
        ++per[k];
        auto* bytes = reinterpret_cast<unsigned char*>(&input);
        for (unsigned i = 0; i < offsetof(State, log); ++i) bytes[i] = static_cast<unsigned char>(Next());
        input.current = Pool(Next());
        input.active = Pool(Next());
        input.state = Pool(Next());
        Apply(input);
        g_seed = Next();
        for (unsigned i = 0; i < kObjects; ++i) SeedObject(Sprite_Objects + i * kObjectBytes);
        for (unsigned i = 0; i < kExtra; ++i) SeedObject(Sprite_ObjectsExtra + i * kObjectBytes);
        for (unsigned i = 0; i < 3; ++i) SeedObject(ObjTrio + i * kMemberBytes);
        for (unsigned i = 0; i < 30; ++i) Sprite_OverlayList[i] = Pool(Next());
        Sprite_OverlayListCount = static_cast<unsigned char>(Next() % 4 ? Next() % 31 : Next() % 3);
        Sprite_DrawListCount = static_cast<unsigned char>(Next() % 3 ? 0x25 + Next() % 6 : Next() % 0x29);
        Field_MemberCount = static_cast<unsigned char>(Next() % 5);
        if (Next() % 6) Field_ScriptFlags2 &= static_cast<unsigned short>(~0x400u);
        if (Half()) Field_ScriptFlags &= static_cast<unsigned short>(~7u);
        if (Half()) Field_ScriptFlags2 &= static_cast<unsigned short>(~7u);
        Draw_OtSlot = static_cast<unsigned char>(Often() ? 4 : Next() % 8);
        if (Next() % 4) Draw_PassFlags |= 1;
        Capture(input);
        std::uint32_t a = 0, b = 0;   // the swap's arguments, stale upper bytes and all
        if (k == kSwap) {
            a = (Next() & ~0xFFu) | Next() % 30;
            b = (Next() & ~0xFFu) | (Half() ? a & 0xFF : Next() % 30);
        }
        for (int pass = 0; pass < 2; ++pass) {
            Apply(input);
            const void* const fn = pass ? ours[k] : theirs[k];
            if (k == kSwap) reinterpret_cast<void (__cdecl*)(std::uint32_t, std::uint32_t)>(const_cast<void*>(fn))(a, b);
            else reinterpret_cast<void (__cdecl*)()>(const_cast<void*>(fn))();
            Capture(pass ? our_out : their_out);
        }
        calls += their_out.log_n;
        if (k == 8) (Sprite_Current[0] & 0x80) ? ++culled : ++drawn;   // ours, just run: the sprite it ended on
        if (k == 10) for (unsigned i = 0; i < their_out.log_n && i < kLog; ++i) swaps += their_out.log[i].what == 36;
        if (std::memcmp(&their_out, &our_out, sizeof their_out) != 0 && ++bad <= 12) {
            unsigned first = 0;
            while (first < sizeof their_out && reinterpret_cast<unsigned char*>(&their_out)[first] == reinterpret_cast<unsigned char*>(&our_out)[first]) ++first;
            bof3::Log("shadow      field_frame self-test MISMATCH: round %u, %s, log %u / %u, first difference at state byte 0x%X", round,
                      kClones[k].name, their_out.log_n, our_out.log_n, first);
        }
    }
    g = kOriginals;
    std::memcpy(Field_ObjectHandlers, saved_handlers, sizeof saved_handlers);
    Apply(saved);
    bof3::Log("shadow      field_frame self-test: %u rounds (%u per function), %u calls to the stand-ins, %u exchanges in the "
              "sort, Sprite_ProjectA %u shown / %u culled, %u MISMATCHES; the 30 objects, the 4 extra, ObjTrio, "
              "both sprite lists and counts, the flags, counts and draw bytes, the records and the stand-ins' log compared",
              kRounds, per[0], calls, swaps, drawn, culled, bad);
    if (bad) bof3::Fatal("the field frame loop differs from the original in %u of %u self-test rounds", bad, kRounds);
}

}  // namespace

void FieldFrame_Inject() {
    if (bof3::WantsShadow("field_frame")) {
        void* clones[kCount];
        for (unsigned k = 0; k < kCount; ++k) {
            const Clone& c = kClones[k];
            bof3::CloneCall calls[16];
            if (c.n_calls > 16) bof3::Fatal("field_frame: %s has %d calls", c.name, c.n_calls);
            for (int i = 0; i < c.n_calls; ++i) calls[i] = {c.calls[i].offset, StubFor(c.calls[i].target)};
            clones[k] = bof3::CloneOriginal(c.name, c.base, c.size, calls, c.n_calls);
        }
        SelfTest(clones);
    }
    BOF3_INJECT(Field_Frame);
    BOF3_INJECT(Field_FrameScripted);
    BOF3_INJECT(Field_MembersFrame);
    BOF3_INJECT(Field_ObjectsScreen);
    BOF3_INJECT(Field_ObjectsFrame);
    BOF3_INJECT(Party_ExtraScreens);
    BOF3_INJECT(Party_ExtraFrame);
    BOF3_INJECT(Sprite_UpdateScreenA);
    BOF3_INJECT(Sprite_ProjectA);
    BOF3_INJECT(Field_DrawFrame);
    BOF3_INJECT(Sprite_DrawOverlays);
    BOF3_INJECT(Sprite_SwapOverlays);
}
