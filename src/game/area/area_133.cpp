// Moved here by tools/move_funcs.py from area_w3c.cpp (docs/source-layout-round.md).
#include "game/area/area_133.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w3c.h"
#include "game/area_w3c_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "game/widescreen.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w3c::at;

}  // namespace

namespace {

using area_harness::Mem;

using move_script::Long;

using move_script::SetLong;

using move_script::SetWord;

using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }

// The dword at `cell` as a pointer.
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }

unsigned char* Focus() { return Ptr(::bof3::Img(AreaW1a_FocusObject_addr)); }

unsigned char* PartyAt(unsigned member) { return rv::Bytes(&ObjTrio[member]); }

// The choice's answer as the choices read it with movsx.
int Answer() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }

// Effect_Spawn(kind, 0, args[party list `member`'s byte], party record
// `member`'s words +0x2E and +0x30) with Sprite_Current made that record (and
// left so); an answer other than 0xFF stored to Sprite_Current[0xB], read
// again after the call. The list byte indexes the table unchecked (it stays in
// .data). Area 94's handlers' shape (area_w2c.cpp's SpawnAt).
void SpawnAtMember(unsigned member, unsigned char kind, U args) {
    const unsigned char* const record = PartyAt(member);
    const auto* const tRecord = rv::AsConst<records::PartyMember>(record);
    const auto z = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_y));
    const auto x = static_cast<short>(static_cast<std::uint16_t>(tRecord->screen_x));
    const unsigned char effect = B(args + B(::bof3::Img(Cond_Flags_addr) + 0xd2 + member));
    Sprite_Current = PartyAt(member);
    const unsigned char slot = AH_CALL(Effect_Spawn)(kind, 0, static_cast<signed char>(effect), x, z);
    if (slot != 0xFF) Sprite_Current[0xB] = slot;
}

}  // namespace

// original 0x41D6D0 (+0x34[0] = +0x3C[4], PSX 0x801F2C1C): the focus object's
// dwords +0x18 / +0x1C = the pair Area133_ChoicePairs[s8 answer] (bytes,
// unchecked; the focus pointer and the answer read again for each), the
// message 0xFFFF; answer 2: the byte 0x929F0F 0, ScriptFlags_Set40,
// Draw_PassFlags 0, the step 0x1E and MoveScript_Var7 4.
extern "C" void __cdecl Area133_ChoiceFocusPair(void) {
    unsigned char* const focus = Focus();
    const unsigned char first = B(::bof3::Img(Area133_ChoicePairs_addr) + static_cast<U>(Answer()) * 2u);
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, first);
    unsigned char* const again = Focus();
    SetLong(again + 0x1C, B(::bof3::Img(Area133_ChoicePairs_addr) + static_cast<U>(Answer()) * 2u + 1));
    if (Answer() != 2) return;
    B(::bof3::Img(AreaW3c_Load0F_addr)) = 0;
    AH_CALL(ScriptFlags_Set40)();
    Draw_PassFlags = 0;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = 0x1E;
    B(::bof3::Img(MoveScript_Var7_addr)) = 4;
}

// original 0x41D740 (+0x3C[0], PSX 0x801F2CEC): kind 3 at record 0,
// Area133_EffectArgs.
extern "C" void __cdecl Area133_SpawnKind3AtMember0(void) { SpawnAtMember(0, 3, ::bof3::Img(Area133_EffectArgs_addr)); }

void Area133_Inject() {
    BOF3_INJECT(Area133_ChoiceFocusPair);
    BOF3_INJECT(Area133_SpawnKind3AtMember0);
}
