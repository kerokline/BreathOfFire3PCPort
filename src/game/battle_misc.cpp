// Group BF of the seventh takeover round (docs/battle_misc.md): twenty-seven
// small battle helpers, each read to its last instruction with capstone against
// bof3/BOF3.exe and, where one is paired, against its PSX twin in BATTLE.EMI
// (the sibling's overlay captures). Faithful: no divergence. The start-up fuzz
// is battle_misc_fuzz.cpp.
//
//   Battle_WrapIndex            0x4469F0..0x446A09 (0x1A)   -
//   Battle_PulseStep            0x446A10..0x446A43 (0x34)   PSX 0x801DD7C8
//   Battle_PlayActorCue         0x446A50..0x446A7B (0x2C)   PSX 0x801DD820 Battle_PlayActorCue
//   Battle_StatusTint           0x446BB0..0x446BCD (0x1E)   PSX 0x801DDA7C
//   Battle_InitActorContext     0x446BD0..0x446C23 (0x54)   PSX 0x801DDAB8
//   Battle_InitActorContexts    0x446C30..0x446CA2 (0x73)   PSX 0x801DDB44
//   Battle_LoadSoundByKey       0x446E40..0x446E98 (0x59)   -
//   Battle_SetActorBit          0x446FB0..0x446FC2 (0x13)   PSX 0x801DE158
//   Battle_ClearActorBit        0x446FD0..0x446FE4 (0x15)   PSX 0x801DE178 Battle_ClearActorBit
//   Battle_SpawnActorCopies     0x446FF0..0x44710C (0x11D)  PSX 0x801DE19C
//   ItemMenu_CanUseSelected     0x447840..0x44787B (0x3C)   -
//   ItemMenu_SetupForActor      0x447E60..0x447F33 (0xD4)   -
//   Battle_ReturnTrue           0x449E00..0x449E02 (0x3)    PSX 0x80097F44
//   ItemMenu_SetupForParty      0x449E10..0x449E8A (0x7B)   PSX 0x80097F4C
//   ItemMenu_FreeWindows        0x449FE0..0x449FFF (0x20)   PSX 0x800981D4
//   BattleBanner_Dispatch       0x44A5C0..0x44A647 (0x88)   PSX 0x801DE404 BattleBanner_Dispatch
//   BattleBanner_Add            0x44A650..0x44A6D8 (0x89)   PSX 0x801DE528
//   BattleBanner_Set            0x44A6E0..0x44A73A (0x5B)   PSX 0x801DE5D4
//   BattleBanner_ClearAll       0x44A810..0x44A82A (0x1B)   PSX 0x801DE7CC
//   BattleBanner_NoneOfKind     0x44A830..0x44A871 (0x42)   PSX 0x801DE820 BattleBanner_NoneOfKind
//   BattleQueue_Push            0x44A880..0x44A8B6 (0x37)   PSX 0x801DE888
//   BattleQueue_Pending         0x44A8C0..0x44A8D0 (0x11)   PSX 0x801DE8F4
//   BattleBanner_SetMessage     0x44A8E0..0x44A90A (0x2B)   PSX 0x801DE914
//   BattleBanner_ShowName       0x44A990..0x44A9FE (0x6F)   PSX 0x801DEA20
//   EnemyAI_TurnCheck           0x44AAD0..0x44AE12 (0x343) + tables 0x44AE14 (20 dwords), 0x44AE64 (0x26 bytes)
//                                                           PSX 0x80098BB0 EnemyAI_TurnCheck
//   Str_CopyN                   0x5171A0..0x5171D3 (0x34)   PSX 0x8015030C
//   Sprite_UpdateObjectScreens  0x517440..0x517483 (0x44)   PSX 0x801A1624 (callers tier)
#include "game/battle_misc.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/battle_misc_callees.h"
#include "game/battle_text.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace battle_misc {

namespace {
template <typename T> T* Fn(U address) { return reinterpret_cast<T*>(static_cast<std::uintptr_t>(address)); }
template <typename T, typename F> T As(F f) { return reinterpret_cast<T>(reinterpret_cast<void*>(f)); }
using F0 = U __cdecl();
}  // namespace

const Callees kOriginals = {
    Fn<U __cdecl(U, U)>(0x435180),
    Fn<U __cdecl(U)>(0x4456C0),
    Fn<U __cdecl(U, U, U, U, U)>(0x454CC0),
    As<U (__cdecl*)(U)>(Sound_PlayEffect),
    As<U (__cdecl*)(U)>(Sound_LoadStream),
    Fn<U __cdecl(U, U, U)>(0x591E50),
    Fn<U __cdecl(U, U, U)>(0x57DA70),
    As<U (__cdecl*)()>(Window_FreeCurrent),
    Fn<U __cdecl(U)>(0x44B320),
    Fn<U __cdecl(U, U)>(0x44B2C0),
    Fn<U __cdecl(U, U)>(0x44B3A0),
    Fn<U __cdecl(U, U, U)>(0x44B2E0),
    Fn<F0>(0x44B920),
    As<U (__cdecl*)()>(Sprite_UpdateScreenA),
    Fn<F0>(0x588F00),
    As<U (__cdecl*)()>(Battle_InitActorContext),
    As<U (__cdecl*)(U, U, U)>(Str_CopyN),
    As<U (__cdecl*)(U, U, U, U, U, U)>(BattleBanner_Set),
    {Fn<F0>(kRetOnly), Fn<F0>(kBannerKind1), Fn<F0>(kBannerKind2), Fn<F0>(kRetOnly), Fn<F0>(kRetOnly)},
};
Callees g = kOriginals;

}  // namespace battle_misc

namespace {

using namespace battle_misc;

unsigned char* At(U address) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(address)); }
U Word(const unsigned char* p) {
    std::uint16_t v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
U Long(const unsigned char* p) {
    U v;
    std::memcpy(&v, p, sizeof v);
    return v;
}
void PutWord(unsigned char* p, U v) {
    const auto w = static_cast<std::uint16_t>(v);
    std::memcpy(p, &w, sizeof w);
}
void PutLong(unsigned char* p, U v) { std::memcpy(p, &v, sizeof v); }
U Addr(const void* p) { return static_cast<U>(reinterpret_cast<std::uintptr_t>(p)); }

U SpriteCurrent() { return Long(At(at::Sprite_CurrentAt())); }
void SetSpriteCurrent(U v) { PutLong(At(at::Sprite_CurrentAt()), v); }
U CurrentEnemy() { return Long(At(kCurrentEnemy)); }
unsigned char* Banner(U i) { return At(kBanners + i * 0xC); }

// The context a slot byte names: 0x93A000 + slot * 0x84, no bound.
U Context(U slot) { return kContexts + (slot & 0xFF) * kContextStride; }

}  // namespace

// --- small arithmetic --------------------------------------------------------------

// original 0x4469F0 (no PSX twin paired): a wrap-around index. value below
// `low` gives `high`, above `high` gives `low`, else itself - signed compares,
// whole dwords. 24 call sites, all in the item and skill menus' handlers after
// 0x447840, which push the result on to 0x445730 / 0x4457F0 or store its al.
extern "C" long __cdecl Battle_WrapIndex(long high, long low, long value) {
    if (value < low) return high;
    if (value <= high) return value;
    return low;
}

// original 0x446A10 (PSX 0x801DD7C8): the menu cursor's pulse. The byte counter
// kPulse steps by 2, down while kPulseDown is 1, up otherwise; reaching 0x1F
// or more sets the flag, 0 clears it (tested in that order, so 0 goes up).
// Byte arithmetic. The one caller (0x42EED0, before the menu's dispatch)
// reloads eax.
extern "C" void __cdecl Battle_PulseStep(void) {
    const unsigned char c = At(kPulse)[0];
    if (c >= 0x1F) At(kPulseDown)[0] = 1;
    if (c == 0) {
        At(kPulseDown)[0] = 0;
        At(kPulse)[0] = static_cast<unsigned char>(c + 2);
        return;
    }
    At(kPulse)[0] = static_cast<unsigned char>(At(kPulseDown)[0] == 1 ? c - 2 : c + 2);
}

// original 0x446A50 (PSX 0x801DD820 Battle_PlayActorCue): Sound_PlayEffect of the
// u16 at kCueTable[kind + (cue & 0xFF) * 3], kind the word +0x2C of the record
// Field_State points at (the current party actor in battle; PSX 0x8014624C).
// The id is pushed in ecx, whose upper half is the record pointer's (mov cx,
// [..]); Sound_PlayEffect reads the low word. Returns its eax, which one of the
// thirteen callers (0x4FC079) returns on.
extern "C" unsigned long __cdecl Battle_PlayActorCue(unsigned cue) {
    const U record = Long(At(at::Field_StateAt()));
    const U index = Word(At(record + 0x2C)) + (cue & 0xFF) * 3;
    const U id = (record & 0xFFFF0000u) | Word(At(kCueTable + index * 2));
    return g.play_effect(id);
}

// original 0x446BB0 (PSX 0x801DDA7C): when bit 7 of the status argument is set,
// Sprite_SetTint(Sprite_Current, -6, -10, 0, 0) and its eax returned; otherwise
// nothing at all - eax is the caller's own. Two of the twelve callers
// (0x44F40E, 0x44F446) return eax on, so the no-tint path is a bare ret here too.
extern "C" unsigned long __cdecl BattleMisc_StatusTintBody(void) {
    return g.set_tint(SpriteCurrent(), 0xFFFFFFFAu, 0xFFFFFFF6u, 0, 0);
}
extern "C" __attribute__((naked)) unsigned long __cdecl Battle_StatusTint(unsigned) {
    asm("testb $0x80, 4(%esp)\n\t"
        "jne _BattleMisc_StatusTintBody\n\t"
        "ret");
}

// --- the actor contexts -------------------------------------------------------------

// original 0x446BD0 (PSX 0x801DDAB8): a context for Sprite_Current's actor.
// slot = 0x435180(0, 6), al; the context's +0x80 = Sprite_Current (read after
// the call), +9 = 0, +0x34 / +0x38 / +0x3C the object's position dwords, each
// read just before it is stored. Its one caller (0x446C30) does not read eax.
extern "C" void __cdecl Battle_InitActorContext(void) {
    const U slot = g.alloc_context(0, 6);
    unsigned char* const context = At(Context(slot));
    const U object = SpriteCurrent();
    PutLong(context + 0x80, object);
    context[9] = 0;
    PutLong(context + 0x34, Long(At(object + 0x34)));
    PutLong(context + 0x38, Long(At(object + 0x38)));
    PutLong(context + 0x3C, Long(At(object + 0x3C)));
}

// original 0x446C30 (PSX 0x801DDB44): every actor's context, in actor order. The
// three party objects (ObjTrio, stride 0x14C) and the eight enemy objects
// (0x93B960, stride 0x128): one whose byte 0 has bit 0 becomes Sprite_Current and
// Field_State (party) or kCurrentEnemy (enemies) and gets Battle_InitActorContext;
// any other still takes a slot, 0x435180(0, 0), so slots stay in actor order.
extern "C" void __cdecl Battle_InitActorContexts(void) {
    for (U i = 0; i < 3; ++i) {
        const U object = at::ObjTrioAt() + i * kPartyStride;
        if (At(object)[0] & 1) {
            SetSpriteCurrent(object);
            PutLong(At(at::Field_StateAt()), object);
            g.init_context();
        } else {
            g.alloc_context(0, 0);
        }
    }
    for (U i = 0; i < 8; ++i) {
        const U object = kEnemyObjects + i * kEnemyStride;
        if (At(object)[0] & 1) {
            SetSpriteCurrent(object);
            PutLong(At(kCurrentEnemy), object);
            g.init_context();
        } else {
            g.alloc_context(0, 0);
        }
    }
}

// original 0x446FF0 (PSX 0x801DE19C): a copy of every actor able to act, for 11
// actors: the party's while index <= 2 (object present, and its +0x134 without
// bit 3), the enemies' after (0x4456C0(index) al zero, and the object's +0x114
// without bit 3). Each gets slot = 0x435180(0, 7); Sprite_Current = the
// object (stored after the call); the context's first 0x80 bytes the object's,
// copied a dword at a time forward (rep movsd); +0x80 the object; then +6 = 0,
// +5 = 7, +1 = 0, +2 = 0, +0x29 = 3, +0x5C..+0x5F = 0. The party walk steps
// on past index 2 unused. Returns the original's eax, which its one caller
// (0x42E9C7, a state of the battle task) returns on: the last actor's (index 10)
// context pointer, or 0x4456C0's eax when that actor was passed over, with
// al = 11.
extern "C" unsigned long __cdecl Battle_SpawnActorCopies(void) {
    U eax = 0;
    for (U index = 0; index <= 10; ++index) {
        U object;
        if (index <= 2) {
            object = at::ObjTrioAt() + index * kPartyStride;
            if (!(At(object)[0] & 1) || (At(object + 0x134)[0] & 8)) continue;
        } else {
            object = kEnemyObjects + (index - 3) * kEnemyStride;
            eax = g.actor_out(index);
            if ((eax & 0xFF) != 0 || (At(object + 0x114)[0] & 8)) continue;
        }
        const U slot = g.alloc_context(0, 7);
        SetSpriteCurrent(object);
        const U context = Context(slot);
        for (U k = 0; k < 0x80; k += 4) PutLong(At(context + k), Long(At(object + k)));
        PutLong(At(context + 0x80), object);
        unsigned char* const c = At(context);
        c[6] = 0;
        c[5] = 7;
        c[1] = 0;
        c[2] = 0;
        c[0x29] = 3;
        c[0x5C] = 0;
        c[0x5D] = 0;
        c[0x5F] = 0;
        c[0x5E] = 0;
        eax = context;
    }
    return (eax & 0xFFFFFF00u) | 11;
}

// --- sound, bits ------------------------------------------------------------------------

// original 0x446E40 (no PSX twin paired): the list kSoundLists[set & 0xFF] -
// none: al 1. Walk its dwords from the first until one whose high half is
// key & 0xFF or one that is -1 (the -1 is tested first, the key on the high half
// shifted down, logically); a -1 there: al 1; else Sound_LoadStream(its low half
// + 0x1000), al 0. The four callers test al.
extern "C" unsigned char __cdecl Battle_LoadSoundByKey(unsigned key, unsigned set) {
    U p = Long(At(kSoundLists + (set & 0xFF) * 4));
    if (p == 0) return 1;
    U e = Long(At(p));
    if (e != 0xFFFFFFFFu) {
        const U want = key & 0xFF;
        while ((e >> 16) != want) {
            e = Long(At(p + 4));
            p += 4;
            if (e == 0xFFFFFFFFu) break;
        }
    }
    const U entry = Long(At(p));
    if (entry == 0xFFFFFFFFu) return 1;
    g.load_stream((entry & 0xFFFF) + 0x1000);
    return 0;
}

// originals 0x446FB0 (PSX 0x801DE158) and 0x446FD0 (PSX 0x801DE178
// Battle_ClearActorBit): bit (actor & 31) of the u16 kActorBits set or cleared -
// the x86 shift count, as the PSX sllv's. eax is 1 << n, or its complement,
// which callers such as 0x42FBE0 push on with a word written over the low half.
extern "C" unsigned long __cdecl Battle_SetActorBit(unsigned actor) {
    const U bit = 1u << (actor & 31);
    PutWord(At(kActorBits), Word(At(kActorBits)) | bit);
    return bit;
}
extern "C" unsigned long __cdecl Battle_ClearActorBit(unsigned actor) {
    const U mask = ~(1u << (actor & 31));
    PutWord(At(kActorBits), Word(At(kActorBits)) & mask);
    return mask;
}

// --- the item window --------------------------------------------------------------------

// original 0x447840 (no PSX twin paired): whether the item under the item
// window's cursor can be used. list = 0x591E50(kItemActor, kItemWindowPage, 1)
// - the actor's inventory page - then 0x57DA70(2, actor, the list's byte at
// kItemWindowCursor), the actor byte read again after the first call; al 1 when
// that returns al 0. The second call's two arguments carry the list pointer's
// upper half above their bytes, as the original's registers do (movzx ax, then
// mov al); the first call's carry the entry's stale eax and ecx, which
// 0x591E50 masks off. The one caller (0x447776) tests al.
extern "C" unsigned char __cdecl ItemMenu_CanUseSelected(void) {
    const U list = g.item_list(At(kItemActor)[0], At(kItemWindowPage)[0], 1);
    const U item = At(list + At(kItemWindowCursor)[0])[0];
    const U high = list & 0xFFFF0000u;
    const U usable = g.item_usable(2, high | At(kItemActor)[0], high | item);
    return (usable & 0xFF) == 0;
}

// original 0x447E60 (no PSX twin paired): item window record 16 for the acting
// actor - the byte +5 of the context kActingActor points at. +0 = 1, +1 = 8, +2
// = 2, +3 = 2, +0xA = the actor, +0xD = 0xFF, +8 = 2, +0xB = its page byte
// (kActorPages + actor * 3), +9 = 0, word +0x10 = its second byte, +0xC = its
// cursor byte; word +0x14 = 2 when the dword +0x10 of what kCommandSource
// points at has bit 1 and not bit 17, else 0; word +4 = 0x140, word +6 = 0x3F;
// then 0x80340C = 0, 0x80340E = 0, kItemActor = the actor byte read again,
// 0x80340D = 8. In that order. The one caller (0x447469) reloads eax.
extern "C" void __cdecl ItemMenu_SetupForActor(void) {
    const U acting = Long(At(kActingActor));
    unsigned char* const w = At(kItemWindow);
    w[0] = 1;
    w[1] = 8;
    w[2] = 2;
    w[3] = 2;
    const U actor = At(acting + 5)[0];
    w[0xA] = static_cast<unsigned char>(actor);
    w[0xD] = 0xFF;
    const unsigned char page = At(kActorPages + actor * 3)[0];
    w[8] = 2;
    w[0xB] = page;
    w[9] = 0;
    PutWord(w + 0x10, At(kActorPages + 1 + actor * 3)[0]);
    w[0xC] = At(kActorPages + 2 + actor * 3)[0];
    const U flags = Long(At(Long(At(kCommandSource)) + 0x10));
    PutWord(w + 0x14, ((flags & 2) && !(flags & 0x20000)) ? 2 : 0);
    PutWord(w + 4, 0x140);
    PutWord(w + 6, 0x3F);
    const unsigned char again = At(acting + 5)[0];
    At(kItemWindowExtra)[0] = 0;
    At(kItemWindowExtra + 2)[0] = 0;
    At(kItemActor)[0] = again;
    At(kItemWindowExtra + 1)[0] = 8;
}

// original 0x449E00 (PSX 0x80097F44, `return 1` there too): al = 1. Thirteen
// callers, every one of which tests al: Item_TargetSetup's "0x80097F44() ? 0
// : .." on the PSX, where the answer is always the party side.
extern "C" unsigned char __cdecl Battle_ReturnTrue(void) { return 1; }

// original 0x449E10 (PSX 0x80097F4C, ItemMenu_Open's call): item window record
// 16 for the party's own inventory: +0 = 1, +2 = 1, +0xA / +0xB / +0xC the bytes
// kPartyPages..+2, +3 = 2, +8 = 2, +1 = 8, +0xD = 0xFF, +9 = 0, word +0x10 = 0,
// word +4 = 0xFF56, word +6 = 0x3F, 0x80340C = 0, 0x80340D = 8, 0x80340E = 0.
// eax is 0 at the end (xor eax, eax); ours returns it.
extern "C" unsigned long __cdecl ItemMenu_SetupForParty(void) {
    unsigned char* const w = At(kItemWindow);
    const unsigned char a = At(kPartyPages)[0];
    w[0] = 1;
    w[2] = 1;
    w[0xA] = a;
    const unsigned char b = At(kPartyPages + 1)[0];
    w[0xB] = b;
    const unsigned char c = At(kPartyPages + 2)[0];
    w[3] = 2;
    w[8] = 2;
    w[1] = 8;
    w[0xC] = c;
    w[0xD] = 0xFF;
    w[9] = 0;
    PutWord(w + 0x10, 0);
    PutWord(w + 4, 0xFF56);
    PutWord(w + 6, 0x3F);
    At(kItemWindowExtra)[0] = 0;
    At(kItemWindowExtra + 1)[0] = 8;
    At(kItemWindowExtra + 2)[0] = 0;
    return 0;
}

// original 0x449FE0 (PSX 0x800981D4, records 0x10..0x13 there too): window
// records 16..19 freed - each made kWindowCurrent (the address in a register,
// not re-read) and Window_FreeCurrent called. Two of the callers tail-jump
// here, so the last call's eax is returned.
extern "C" unsigned long __cdecl ItemMenu_FreeWindows(void) {
    U eax = 0;
    for (U k = 0; k < 4; ++k) {
        PutLong(At(kWindowCurrent), kItemWindow + k * 0x24);
        eax = g.free_window();
    }
    return eax;
}

// --- the banner pool and the message queue -----------------------------------------------

// original 0x44A5C0 (PSX 0x801DE404 BattleBanner_Dispatch): kBannerKinds = 0; for
// layer 0 then 1, each of the 8 entries: made kBannerCurrent (every entry, before
// any test); skipped unless active, on this layer, and its kind byte shares no
// bit with kBannerKinds (read again for each entry); then DamageScratch = its
// index (the PSX scratchpad byte) and the handler for its kind byte, read
// again, through a five-entry table the original builds on its stack: a bare
// ret (0x437CC0) for kinds 0, 3 and 4, 0x44A740 for kind 1, 0x44A7B0 for kind
// 2 (the PSX's kind 4 handler is an empty function too). No bound on the kind:
// index 5 of the original's table is its own return address, which cannot be
// reproduced, so ours aborts loudly (CLAUDE.md rule 4) - the kinds the callers
// push are constants 1 and 2 (docs/battle_misc.md section 1).
extern "C" void __cdecl BattleBanner_Dispatch(void) {
    At(kBannerKinds)[0] = 0;
    for (U layer = 0; layer < 2; ++layer) {
        for (U i = 0; i < 8; ++i) {
            unsigned char* const entry = Banner(i);
            PutLong(At(kBannerCurrent), Addr(entry));
            if (entry[0] == 0) continue;
            if (entry[3] != layer) continue;
            if (At(kBannerKinds)[0] & entry[1]) continue;
            At(bof3::addr::DamageScratch)[0] = static_cast<unsigned char>(i);
            const U kind = entry[1];
            if (kind >= 5) bof3::Fatal("banner entry %u has kind %u; the original's stack table holds five", i, kind);
            g.banner[kind]();
        }
    }
}

namespace {
// The shared fill of BattleBanner_Add and BattleBanner_Set: +0 |= 1, +1 kind,
// +2 b2, +3 layer, word +8 the timer's byte, +4 the text, +0xA = 0, in the
// original's order.
void FillBanner(unsigned char* entry, U kind, U b2, U layer, U timer, U text) {
    entry[0] = static_cast<unsigned char>(entry[0] | 1);
    entry[1] = static_cast<unsigned char>(kind);
    entry[2] = static_cast<unsigned char>(b2);
    entry[3] = static_cast<unsigned char>(layer);
    PutWord(entry + 8, timer & 0xFF);
    PutLong(entry + 4, text);
    entry[0xA] = 0;
}
}  // namespace

// original 0x44A650 (PSX 0x801DE528): the first inactive entry from 2 to 7 filled
// (FillBanner); entries 0 and 1 are BattleBanner_Set's to name. eax is the
// entry's offset (index * 12), or 0x15 (7 * 3, the last index's lea) when every
// entry is taken, and nothing is written. Several of the 31 callers only
// overwrite al before returning, so ours returns the register.
extern "C" unsigned long __cdecl BattleBanner_Add(unsigned kind, unsigned b2, unsigned layer, unsigned timer,
                                                  const char* text) {
    for (U i = 2; i < 8; ++i) {
        if (Banner(i)[0] != 0) continue;
        FillBanner(Banner(i), kind, b2, layer, timer, Addr(text));
        return i * 12;
    }
    return 7 * 3;
}

// original 0x44A6E0 (PSX 0x801DE5D4): entry (slot & 0xFF) filled, no bound. eax
// the entry's offset.
extern "C" unsigned long __cdecl BattleBanner_Set(unsigned slot, unsigned kind, unsigned b2, unsigned layer,
                                                  unsigned timer, const char* text) {
    FillBanner(Banner(slot & 0xFF), kind, b2, layer, timer, Addr(text));
    return (slot & 0xFF) * 12;
}

// original 0x44A810 (PSX 0x801DE7CC): bytes +0, +1 and +2 of all 8 entries to 0.
// eax is the walk's pointer past the last entry, 0x93B941.
extern "C" unsigned long __cdecl BattleBanner_ClearAll(void) {
    for (U i = 0; i < 8; ++i) {
        Banner(i)[0] = 0;
        Banner(i)[1] = 0;
        Banner(i)[2] = 0;
    }
    return kBanners + 1 + 8 * 12;
}

// original 0x44A830 (PSX 0x801DE820 BattleBanner_NoneOfKind): al 0 when an active
// entry's kind byte is kind & 0xFF, else 1 - and eax is exactly that (the
// upper bits are the entry offset's, which is below 0x100).
extern "C" unsigned char __cdecl BattleBanner_NoneOfKind(unsigned kind) {
    for (U i = 0; i < 8; ++i)
        if (Banner(i)[0] != 0 && Banner(i)[1] == (kind & 0xFF)) return 0;
    return 1;
}

// original 0x44A880 (PSX 0x801DE888): queue entry kQueueWrite (a byte, no bound
// on what it indexes) gets +0 = a, +1 = b, dword +4 = value; kQueueWrite =
// (index + 1) & 0xF. eax the entry's offset, index * 8 (0x42FF2F returns it).
extern "C" unsigned long __cdecl BattleQueue_Push(unsigned a, unsigned b, unsigned long value) {
    const U index = At(kQueueWrite)[0];
    unsigned char* const entry = At(kQueue + index * 8);
    entry[0] = static_cast<unsigned char>(a);
    entry[1] = static_cast<unsigned char>(b);
    PutLong(entry + 4, static_cast<U>(value));
    At(kQueueWrite)[0] = static_cast<unsigned char>((index + 1) & 0xF);
    return index * 8;
}

// original 0x44A8C0 (PSX 0x801DE8F4): al 1 while the write index differs from the
// read index. The one caller (0x42E942) tests al.
extern "C" unsigned char __cdecl BattleQueue_Pending(void) {
    return At(kQueueWrite)[0] != At(kQueueRead)[0];
}

// original 0x44A8E0 (PSX 0x801DE914): Str_CopyN(kBannerText, kMessages[msg &
// 0xFF], 8) - the PC's table of string pointers where the PSX indexes 8-byte
// strings - then banner entry 0's byte +2 = b2. Returns Str_CopyN's eax.
// DIVERGENCE DIV-0052: 12 bytes, the US release's count, once a language
// overlay has repointed the table at its longer words (src/game/battle_text.cpp).
extern "C" unsigned long __cdecl BattleBanner_SetMessage(unsigned msg, unsigned b2) {
    const U eax = g.copy_n(kBannerText, Long(At(kMessages + (msg & 0xFF) * 4)), BattleMessages_CopyRoom());
    Banner(0)[2] = static_cast<unsigned char>(b2);
    return eax;
}

// original 0x44A990 (PSX 0x801DEA20): an actor's name as banner 0.
// Str_CopyN(kBannerText, actor + 0x80, 8) - the enemy object's name, the 16
// bytes the port prepended to the working record (the PSX copies 5 from +0x74);
// when kSuffixOn is set, the string kNameSuffix points at appended, inline:
// its length plus its NUL measured first, then that many bytes copied to the
// text's NUL, a dword at a time and the rest by bytes (repne scasb, rep movsd,
// rep movsb). Then BattleBanner_Set(0, 1, 1, 0, 0xFF, kBannerText), whose eax
// is returned.
extern "C" unsigned long __cdecl BattleBanner_ShowName(const unsigned char* actor) {
    g.copy_n(kBannerText, Addr(actor) + 0x80, 8);
    if (At(kSuffixOn)[0] != 0) {
        const U src = Long(At(kNameSuffix));
        U n = 0;
        while (At(src + n)[0] != 0) ++n;
        ++n;
        U dst = kBannerText;
        while (At(dst)[0] != 0) ++dst;
        U k = 0;
        for (; k + 4 <= (n & ~3u); k += 4) PutLong(At(dst + k), Long(At(src + k)));
        for (U r = 0; r < (n & 3); ++r, ++k) At(dst + k)[0] = At(src + k)[0];
    }
    return g.banner_set(0, 1, 1, 0, 0xFF, kBannerText);
}

// --- the enemy AI's per-turn rows -------------------------------------------------------

namespace {
// Rows that fire once: EnemyAI_RowDone(actor, row) - skipped when al is set -
// then EnemyAI_ApplyAction and EnemyAI_SetRowDone(.., row, 1), each handed the
// current enemy read again. `actor` is what the row's test used.
void FireRow(U actor, U row_address, U row) {
    if (g.ai_row_done(actor, row) & 0xFF) return;
    g.ai_apply(CurrentEnemy(), row_address);
    g.ai_set_done(CurrentEnemy(), row, 1);
}
}  // namespace

// original 0x44AAD0 (PSX 0x80098BB0 EnemyAI_TurnCheck): the current enemy's four
// per-turn rows. For row i: the enemy (kCurrentEnemy, read at the row's start)
// names a script by its byte +0xF0; the row is 16 bytes at kAiScripts + script
// * 0x8C + i * 16, its byte 0 the opcode, through the index table 0x44AE64
// and the jump table 0x44AE14:
//   0..8          EnemyAI_CondPartyFlag(1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x80)
//                 al set: fire once, the enemy read again after the test
//   9             kActingKind 4 and the enemy's word +0x108 non-zero: fire once
//   0xA           kActingKind 1 and word +0x108 non-zero: fire once
//   0x16 / 0x17   byte +0x92 bit 3 / bit 7: fire once
//   0x18          byte +0xAA zero: fire once
//   0x21..0x23    EnemyAI_CondPartyFlag(1 / 2 / 4): EnemyAI_ApplyAction only
//   0x24          kActingKind 1 and word +0x108 non-zero: EnemyAI_ApplyAction only
//   0x25          the u16 +0xA4 not above the s16 +0x108: fire once
//   anything else (above 0x25 through the range check): nothing
// then 0x44B920, whose eax is returned. The row index is handed on as a dword
// whose upper bytes are the caller's ecx (the original keeps it in its pushed
// ecx slot); both callees read its low byte.
extern "C" unsigned long __cdecl EnemyAI_TurnCheck(void) {
    static constexpr U kMasks[9] = {1, 2, 4, 8, 0x10, 0x20, 0x40, 0x100, 0x80};
    for (U i = 0; i < 4; ++i) {
        const U actor = CurrentEnemy();
        const U row = kAiScripts + At(actor + 0xF0)[0] * 0x8Cu + i * 16;
        const U op = At(row)[0];
        const U acting = At(kActingKind)[0];
        switch (op) {
        case 0: case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
            if (g.ai_condition(kMasks[op]) & 0xFF) FireRow(CurrentEnemy(), row, i);
            break;
        case 9:
            if (acting == 4 && Word(At(actor + 0x108)) != 0) FireRow(actor, row, i);
            break;
        case 0xA:
            if (acting == 1 && Word(At(actor + 0x108)) != 0) FireRow(actor, row, i);
            break;
        case 0x16:
            if (At(actor + 0x92)[0] & 8) FireRow(actor, row, i);
            break;
        case 0x17:
            if (At(actor + 0x92)[0] & 0x80) FireRow(actor, row, i);
            break;
        case 0x18:
            if (At(actor + 0xAA)[0] == 0) FireRow(actor, row, i);
            break;
        case 0x21: case 0x22: case 0x23:
            if (g.ai_condition(1u << (op - 0x21)) & 0xFF) g.ai_apply(CurrentEnemy(), row);
            break;
        case 0x24:
            if (acting == 1 && Word(At(actor + 0x108)) != 0) g.ai_apply(actor, row);
            break;
        case 0x25:
            if (static_cast<std::int32_t>(Word(At(actor + 0xA4))) <=
                static_cast<std::int16_t>(Word(At(actor + 0x108))))
                FireRow(actor, row, i);
            break;
        default:
            break;
        }
    }
    return g.ai_finish();
}

// --- a string copy and the objects' screen pass ----------------------------------------------

// original 0x5171A0 (PSX 0x8015030C): at most (n & 0xFF) bytes of src to dst,
// stopping at a NUL (not copied), then a NUL stored; byte by byte, forward.
// eax is where the NUL went (four callers tail-return it).
extern "C" char* __cdecl Str_CopyN(char* dst, const char* src, unsigned n) {
    const U limit = n & 0xFF;
    U d = Addr(dst), s = Addr(src);
    for (U k = 0; k < limit; ++k) {
        const unsigned char c = At(s)[0];
        if (c == 0) break;
        At(d)[0] = c;
        ++d;
        ++s;
    }
    At(d)[0] = 0;
    return reinterpret_cast<char*>(At(d));
}

// original 0x517440 (PSX 0x801A1624, by its callers): each of the 30
// Sprite_Objects in turn becomes Sprite_Current; one of type (byte +6, read
// after that store) 7 or 8 and up, present (byte 0 bit 0), gets its screen
// update - Sprite_UpdateScreenA for type 0xA, none for 9,
// Sprite_UpdateScreenSlot for the rest. The type is read once per object.
// Called from the battle's frame (0x42E3D1), which calls on at once.
extern "C" void __cdecl Sprite_UpdateObjectScreens(void) {
    for (U i = 0; i < 30; ++i) {
        const U object = at::Sprite_ObjectsAt() + i * 0xA4;
        SetSpriteCurrent(object);
        const unsigned char type = At(object + 6)[0];
        if (type < 8 && type != 7) continue;
        if (!(At(object)[0] & 1)) continue;
        if (type == 0xA) g.update_screen_a();
        else if (type != 9) g.update_screen_slot();
    }
}

void BattleMisc_Inject() {
    if (bof3::WantsShadow("battle_misc")) battle_misc::SelfTest();
    BOF3_INJECT(Battle_WrapIndex);
    BOF3_INJECT(Battle_PulseStep);
    BOF3_INJECT(Battle_PlayActorCue);
    BOF3_INJECT(Battle_StatusTint);
    BOF3_INJECT(Battle_InitActorContext);
    BOF3_INJECT(Battle_InitActorContexts);
    BOF3_INJECT(Battle_LoadSoundByKey);
    BOF3_INJECT(Battle_SetActorBit);
    BOF3_INJECT(Battle_ClearActorBit);
    BOF3_INJECT(Battle_SpawnActorCopies);
    BOF3_INJECT(ItemMenu_CanUseSelected);
    BOF3_INJECT(ItemMenu_SetupForActor);
    BOF3_INJECT(Battle_ReturnTrue);
    BOF3_INJECT(ItemMenu_SetupForParty);
    BOF3_INJECT(ItemMenu_FreeWindows);
    BOF3_INJECT(BattleBanner_Dispatch);
    BOF3_INJECT(BattleBanner_Add);
    BOF3_INJECT(BattleBanner_Set);
    BOF3_INJECT(BattleBanner_ClearAll);
    BOF3_INJECT(BattleBanner_NoneOfKind);
    BOF3_INJECT(BattleQueue_Push);
    BOF3_INJECT(BattleQueue_Pending);
    BOF3_INJECT(BattleBanner_SetMessage);
    BOF3_INJECT(BattleBanner_ShowName);
    BOF3_INJECT(EnemyAI_TurnCheck);
    BOF3_INJECT(Str_CopyN);
    BOF3_INJECT(Sprite_UpdateObjectScreens);
}
