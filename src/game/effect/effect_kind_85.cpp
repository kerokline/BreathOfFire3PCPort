// Moved here by tools/move_funcs.py from effect_4a.cpp (docs/source-layout-round.md).
#include "game/effect/effect_kind_85.h"

#include <cstdint>
#include <initializer_list>

#include "bof3/symbols.gen.h"
#include "game/effect_4a_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/scenario_harness.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = effect_4a::at;

}  // namespace

namespace {

using U = std::uint32_t;

using move_script::At;

using move_script::Long;

unsigned char* S() { return Sprite_Current; }

U UL(const unsigned char* p) { return static_cast<U>(Long(p)); }

unsigned char& B(U a) { return *At(a); }

U AddressOf(const void* p) { return Low32(p); }

unsigned char* P(U a) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(a)); }

using Handler = scenario_harness::Handler;

// mov ecx, [Sprite_Current]; xor eax, eax; mov al, [ecx + 1]; jmp [table + eax * 4]:
// the table's `entries` handlers read in place (the fuzz swaps the cells for
// recorders); a Fatal past them, where the original jumps through the dword
// after - the next kind's table, a null or data.
void DispatchEffect4a(const char* who, U table, unsigned entries) {
    const unsigned state = Sprite_Current[1];
    if (state >= entries)
        bof3::Fatal("%s: state byte +1 is %u, past the %u entries of 0x%X - the original jumps through the dword after "
                    "(docs/effect_4a.md section 7)",
                    who, state, entries, (unsigned)table);
    reinterpret_cast<Handler>(static_cast<std::uintptr_t>(UL(P(table + 4 * state))))();
}

// Sprite_Objects record `index` (a byte of the effect record, unchecked by the
// original: past the 30 records it writes the draw records 0x7E01C0.. and on).
unsigned char* Object(const char* who, unsigned index) {
    if (index >= at::kSpriteCount)
        bof3::Fatal("%s: Sprite_Objects index %u, past its %u records - the original writes past them "
                    "(docs/effect_4a.md section 7)",
                    who, index, at::kSpriteCount);
    return rv::Bytes(&Sprite_Objects[index]);
}

// The end of states 1..4: the tile (E4D's 0x48CA90), then a tail jump to
// EffectKind85_ShowObjects.
void FadeAndShow() {
    SH_AT(Handler, at::kFade.v)();
    SH_CALL(EffectKind85_ShowObjects)();
}

}  // namespace

// original 0x4889E0 (Effect_KindHandlers[0x85], hidden in 0x487BF0): jmp
// [EffectKind85_States + +1 * 4].
extern "C" void __cdecl EffectKind85_Run(void) {
    DispatchEffect4a("EffectKind85_Run", AddressOf(EffectKind85_States), EffectKind85_States_count);
}

// original 0x488A00 (state 0): the tile's colour +0x5D..+0x5F 0, the message
// +0xB 0x33, Msg_OpenScript(+0xB), Field_Request 2, +1 1.
extern "C" void __cdecl EffectKind85_Start(void) {
    S()[0x5F] = 0;
    S()[0x5E] = 0;
    S()[0x5D] = 0;
    S()[0xB] = 0x33;
    SH_CALL(Msg_OpenScript)(S()[0xB]);
    unsigned char* const s = S();
    Field_Request = 2;
    s[1] = 1;
}

// original 0x488A50 (state 1): the message closed (Field_Request not 2): +9
// 0x1E, +1 2. Then the tile and the objects.
extern "C" void __cdecl EffectKind85_WaitMessage(void) {
    if (Field_Request != 2) {
        S()[9] = 0x1E;
        S()[1] = 2;
    }
    FadeAndShow();
}

// original 0x488A80 (state 2): +9 down; not yet 0: the tile's colour +0x5D,
// +0x5E, +0x5F up 2 each; else +9 0x1E, +1 3. Then the tile and the objects.
extern "C" void __cdecl EffectKind85_Brighten(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* const s = S();
    if (s[9] != 0) {
        s[0x5D] = static_cast<unsigned char>(s[0x5D] + 2);
        S()[0x5E] = static_cast<unsigned char>(S()[0x5E] + 2);
        S()[0x5F] = static_cast<unsigned char>(S()[0x5F] + 2);
    } else {
        s[9] = 0x1E;
        S()[1] = 3;
    }
    FadeAndShow();
}

// original 0x488AE0 (state 3): +9 down; at 0 the message +0xB up one - at 0x37
// the colour 0xFF, +1 4 and the counter 0x903848 up one; else
// Msg_OpenScript(+0xB), Field_Request 2, +1 1. Then the tile and the objects.
extern "C" void __cdecl EffectKind85_NextMessage(void) {
    S()[9] = static_cast<unsigned char>(S()[9] - 1);
    unsigned char* const s = S();
    if (s[9] == 0) {
        s[0xB] = static_cast<unsigned char>(s[0xB] + 1);
        unsigned char* const t = S();
        const unsigned char message = t[0xB];
        if (message == 0x37) {
            t[0x5F] = 0xFF;
            S()[0x5E] = 0xFF;
            S()[0x5D] = 0xFF;
            S()[1] = 4;
            B(::bof3::Img(at::kCounter.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter.v)) + 1);
        } else {
            SH_CALL(Msg_OpenScript)(message);
            unsigned char* const u = S();
            Field_Request = 2;
            u[1] = 1;
        }
    }
    FadeAndShow();
}

// original 0x488B70 (state 4): Effect_Release when the counter 0x903848 is
// 0x35; then the tile and the objects (the record's +6 read after the release).
extern "C" void __cdecl EffectKind85_Wait(void) {
    if (B(::bof3::Img(at::kCounter.v)) == 0x35) SH_CALL(Effect_Release)();
    FadeAndShow();
}

// original 0x488B90 (the tail states 1..4 jump to; its own frame and ret, a
// start no list of the cut has): Sprite_Objects record +6's +0x29 2 and its
// screen point (Sprite_UpdateScreen), the leader's likewise; Sprite_Current
// put back.
extern "C" void __cdecl EffectKind85_ShowObjects(void) {
    unsigned char* const saved = Sprite_Current;
    Sprite_Current = Object("EffectKind85_ShowObjects", saved[6]);
    Sprite_Current[0x29] = 2;
    SH_CALL(Sprite_UpdateScreen)();
    Sprite_Current = rv::Bytes(ObjTrio);
    ObjTrio[0].ot_slot = 2;
    SH_CALL(Sprite_UpdateScreen)();
    Sprite_Current = saved;
}

// original 0x488BE0 (Effect_KindHandlers[0x86], hidden in 0x487BF0; no list of
// the cut has it): jmp [EffectKind86_States + +1 * 4].
extern "C" void __cdecl EffectKind86_Run(void) {
    DispatchEffect4a("EffectKind86_Run", AddressOf(EffectKind86_States), EffectKind86_States_count);
}

// original 0x488C00 (state 0): two free sprites (Sprite_FindFree) into +3 and
// +4, each marked in use (+0 1) as found - none: nothing (the first freed
// again when the second is none). Both placed by EventOp_0x(0x654D88) with the
// count word 0x903850 the sprite; Sprite_Current put back; the first's z a
// cell less, the second's a cell more, +0 bit 5 and +0x5C 1 on both; tinted
// (Sprite_SetTint: the first (0xF, 0, 0, 1), the second (0, 0, 0xF, 1)); +9
// 0x20, +1 1.
extern "C" void __cdecl EffectKind86_Spawn(void) {
    static const char* const kWho = "EffectKind86_Spawn";
    const unsigned char first = SH_CALL(Sprite_FindFree)();
    S()[3] = first;
    if (S()[3] == 0xFF) return;
    Object(kWho, S()[3])[0] = 1;
    const unsigned char second = SH_CALL(Sprite_FindFree)();
    S()[4] = second;
    unsigned char* const s = S();
    if (s[4] == 0xFF) {
        Object(kWho, s[3])[0] = 0;
        return;
    }
    Object(kWho, s[4])[0] = 1;
    at::kObjectCount = s[3];
    SH_CALL(EventOp_0x)(P(::bof3::Img(at::kPlaceOp.v)));
    at::kObjectCount = s[4];
    SH_CALL(EventOp_0x)(P(::bof3::Img(at::kPlaceOp.v)));
    Sprite_Current = s;
    unsigned char* const a = Object(kWho, s[3]);
    auto* const ao = rv::As<records::SpriteObject>(a);
    ao->z = static_cast<std::uint32_t>(ao->z) + 0xFFFF0000u;
    unsigned char* const b = Object(kWho, s[4]);
    auto* const bo = rv::As<records::SpriteObject>(b);
    bo->z = static_cast<std::uint32_t>(bo->z) + 0x10000u;
    Object(kWho, s[3])[0] |= 0x20;
    Object(kWho, s[4])[0] |= 0x20;
    Object(kWho, s[4])[0x5C] = 1;
    Object(kWho, s[3])[0x5C] = 1;
    SH_CALL(Sprite_SetTint)(Object(kWho, s[3]), 0xF, 0, 0, 1);
    SH_CALL(Sprite_SetTint)(Object(kWho, s[4]), 0, 0, 0xF, 1);
    S()[9] = 0x20;
    S()[1] = 1;
}

// original 0x488DA0 (state 1): +9 not 0: the first sprite's z + 0x800, the
// second's - 0x800, +9 down; else +9 0xF, +1 2.
extern "C" void __cdecl EffectKind86_Slide(void) {
    static const char* const kWho = "EffectKind86_Slide";
    unsigned char* const s = S();
    if (s[9] == 0) {
        s[9] = 0xF;
        S()[1] = 2;
        return;
    }
    unsigned char* const a = Object(kWho, s[3]);
    auto* const ao = rv::As<records::SpriteObject>(a);
    ao->z = static_cast<std::uint32_t>(ao->z) + 0x800u;
    unsigned char* const b = Object(kWho, s[4]);
    auto* const bo = rv::As<records::SpriteObject>(b);
    bo->z = static_cast<std::uint32_t>(bo->z) + 0xFFFFF800u;
    s[9] = static_cast<unsigned char>(s[9] - 1);
}

// original 0x488E10 (state 2): +9 not 0: both sprites' +0x5D, +0x5E, +0x5F up
// 8 each, +9 down; else the counter 0x903848 up one, +1 3.
extern "C" void __cdecl EffectKind86_Tint(void) {
    static const char* const kWho = "EffectKind86_Tint";
    unsigned char* const s = S();
    if (s[9] == 0) {
        B(::bof3::Img(at::kCounter.v)) = static_cast<unsigned char>(B(::bof3::Img(at::kCounter.v)) + 1);
        s[1] = 3;
        return;
    }
    for (unsigned byte : {3u, 4u})
        for (unsigned k = 0x5D; k <= 0x5F; ++k) {
            unsigned char* const o = Object(kWho, s[byte]);
            o[k] = static_cast<unsigned char>(o[k] + 8);
        }
    s[9] = static_cast<unsigned char>(s[9] - 1);
}

// original 0x488EF0 (state 3): both sprites free (+0 0), their tints released
// (Sprite_ReleaseTint; the second's index read again after the first call);
// Effect_Release.
extern "C" void __cdecl EffectKind86_End(void) {
    static const char* const kWho = "EffectKind86_End";
    unsigned char* const s = S();
    Object(kWho, s[3])[0] = 0;
    Object(kWho, s[4])[0] = 0;
    SH_CALL(Sprite_ReleaseTint)(Object(kWho, s[3]));
    SH_CALL(Sprite_ReleaseTint)(Object(kWho, S()[4]));
    SH_CALL(Effect_Release)();
}

void EffectKind85_Inject() {
    BOF3_INJECT(EffectKind85_Run);
    BOF3_INJECT(EffectKind85_Start);
    BOF3_INJECT(EffectKind85_WaitMessage);
    BOF3_INJECT(EffectKind85_Brighten);
    BOF3_INJECT(EffectKind85_NextMessage);
    BOF3_INJECT(EffectKind85_Wait);
    BOF3_INJECT(EffectKind85_ShowObjects);
    BOF3_INJECT(EffectKind86_Run);
    BOF3_INJECT(EffectKind86_Spawn);
    BOF3_INJECT(EffectKind86_Slide);
    BOF3_INJECT(EffectKind86_Tint);
    BOF3_INJECT(EffectKind86_End);
}
