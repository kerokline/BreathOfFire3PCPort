// World 4's areas 192..199: the PSX's BIN/WORLD04/AREA192..199.EMI compiled
// into the exe at 0x42BD60..0x42D710 (Area_Descriptors entries 192..199; 194
// and 195 have no code here). Round ten, group AR4F: the band's 49 functions
// (the tool listed 48; area 198's effect kind 0xA6 has two states where it
// saw one), none ours before, each read to its last instruction with capstone
// (2026-09-28) and taken through the area harness (area_harness.h).
// docs/area_w4f.md.
//
// Every one is a faithful replacement: no DIVERGENCE.md entry is owed. The
// dispatchers through the areas' two two-state tables abort past the table
// (what follows is data, which the original would jump into), and area 193's
// choice 4 aborts on an answer past its three-row stack table (the original
// reads its own frame there); every other unchecked read stays inside .data
// or the field frame and is kept (docs/area_w4f.md section 6). Every call
// goes through the harness (AH_CALL / AH_AT), so the start-up fuzz can stand
// recorders in for ours as for the originals' copies; the callees of the
// group's own are called the same way, so each function is fuzzed alone.
//
// 0x42D710, where the band ends, is not area code: game mode 9's frame step
// (0x517330, GameMode steps table 0x656AC8 entry 1) calls it, and it jumps by
// the byte 0x929F00 through the table 0x64ADAC (just past area 199's
// descriptor) into BATE.EMI's code. Not taken here.
#include "game/area_w4f.h"

#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/area_harness.h"
#include "game/area_w4f_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace {

namespace at = area_w4f::at;
using area_harness::Handler;
using area_harness::Mem;
using move_script::Long;
using move_script::SetLong;
using move_script::SetWord;
using move_script::Word;
using U = std::uint32_t;

unsigned char& B(U address) { return *Mem(address); }
U Key(const void* p) { return Low32(p); }
void AddWord(unsigned char* p, int v) { SetWord(p, static_cast<unsigned>(Word(p) + v)); }
unsigned char* Ptr(U cell) { return reinterpret_cast<unsigned char*>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(cell))))); }
// Effect_Objects record `slot` (0x80 bytes; the slot is not checked, as the
// originals' `shl 7`).
unsigned char* EffectAt(unsigned slot) { return rv::Bytes(&Effect_Objects[slot]); }

void SetMessage(unsigned id) { SetWord(Mem(::bof3::Img(at::kMessage.v)), id); }
// The answer as the originals' movsx reads it.
int AnswerS8() { return static_cast<signed char>(B(::bof3::Img(AreaW0c_ChoiceAnswer_addr))); }
unsigned char* StoryFlags() { return Mem(::bof3::Img(Cond_Flags_addr) + 0xa0); }

// A state handler read from an area's .data table in place, as the
// originals' `jmp [index * 4 + table]`: the index is not checked there. Ours
// aborts past the table (what follows is data).
Handler StateEntry(const char* who, U table, unsigned index) {
    if (index >= at::kStateCount)
        bof3::Fatal("%s: state %u is past its %u-entry table 0x%X", who, index, at::kStateCount, static_cast<unsigned>(table));
    return reinterpret_cast<Handler>(static_cast<std::uintptr_t>(static_cast<U>(Long(Mem(table + index * 4u)))));
}

// The signed quotient (Field_ActiveMember - Sprite_Objects) / 0xA4 the
// effect spawns keep (the originals' multiply by 0x63E7063F, sar 6, plus the
// sign bit: truncating toward 0).
unsigned char MemberIndex() {
    const auto d = static_cast<std::int32_t>(Key(Field_ActiveMember) - ::bof3::Img(Sprite_Objects_addr));
    return static_cast<unsigned char>(d / static_cast<std::int32_t>(at::kObjectStride));
}

// Area 192's choice 2 and area 193's choice 3 (0x42BD60, 0x42C430: one body,
// two tables): the focus object's dwords +0x18 and +0x1C the pair of bytes
// the s8 answer picks (table + answer * 2, unchecked), the message word 0xFFFF
// stored between the first read and the first store; the focus pointer and
// the answer read again for the second.
void FocusPair(U table) {
    int a = AnswerS8();
    unsigned char* focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    const unsigned first = B(table + static_cast<U>(a * 2));
    SetMessage(0xFFFF);
    SetLong(focus + 0x18, static_cast<std::int32_t>(first));
    focus = Ptr(::bof3::Img(AreaW1a_FocusObject_addr));
    a = AnswerS8();
    SetLong(focus + 0x1C, static_cast<std::int32_t>(B(table + 1 + static_cast<U>(a * 2))));
}

// The member search of areas 196 and 197's handlers (nine bodies, one a
// handler, each over its own two tables; AR3F's Area143_MessageByMember is
// the same code): for each of the four keys in turn, each of
// Field_MemberCount's party records (the count read once, compared
// unsigned): the first record whose +0x89 is the key opens the key's message
// (Msg_OpenScript) and sets Field_Request 2. None: nothing. The records are
// not bounded by three (the count is).
void MessageByMember(U keys) {
    const U messages = keys + at::kKeysToMessages;
    const unsigned char count = Field_MemberCount;
    for (unsigned i = 0; i < 4; ++i) {
        const unsigned char key = B(keys + i);
        for (unsigned char j = 0; j < count; ++j) {
            if (B(::bof3::Img(ObjTrio_addr) + 0x89 + j * at::kPartyStride) != key) continue;
            AH_CALL(Msg_OpenScript)(Word(Mem(messages + i * 2)));
            Field_Request = 2;
            return;
        }
    }
}

// The tail disarmed: kind 0, state 0.
void Disarm() {
    B(::bof3::Img(AreaW0a_TailKind_addr)) = 0;
    B(::bof3::Img(AreaW0c_TailState_addr)) = 0;
}
// The tail armed after ScriptFlags_Set40: kind `kind`, state `state`.
void ArmTail(unsigned char kind, unsigned char state) {
    AH_CALL(ScriptFlags_Set40)();
    B(::bof3::Img(AreaW0a_TailKind_addr)) = kind;
    B(::bof3::Img(AreaW0c_TailState_addr)) = state;
}
// The run armed after ScriptFlags_Set40: MoveScript_Var7 `var`, its step
// byte `step`.
void ArmRun(signed char var, unsigned char step) {
    AH_CALL(ScriptFlags_Set40)();
    MoveScript_Var7 = var;
    B(::bof3::Img(MoveScript_PartyRecords_addr) + 0x65) = step;
}

// ---- area 198's shakes and fades ----

// The s8 step a shake table gives for the running object's +0xA (its low
// nibble), Sprite_Current read at the call.
int ShakeStep(U table) { return static_cast<signed char>(B(table + (Sprite_Current[0xA] & 0xFu))); }
// The +0x34 / +0x38 moves of both shakes: x by step << 11, z by -step << 11
// (32-bit), Sprite_Current and the step read again for each.
void ShakeXZ(U table) {
    unsigned char* c = Sprite_Current;
    auto* tC = rv::As<records::SpriteObject>(c);
    std::int32_t x = tC->x;
    int s = ShakeStep(table);
    tC->x = static_cast<std::int32_t>(static_cast<U>(x) + (static_cast<U>(s) << 11));
    c = Sprite_Current;
    tC = rv::As<records::SpriteObject>(c);
    s = ShakeStep(table);
    tC->z = static_cast<std::int32_t>(static_cast<U>(tC->z) + (static_cast<U>(-s) << 11));
}

}  // namespace

// ============================================================================
// Area 192 (descriptor 0x647F18; PSX 0x801F37C8)
// ============================================================================

// ============================================================================
// Area 193 (descriptor 0x648908; PSX 0x801F39B8)
// ============================================================================

// ============================================================================
// Area 196 (descriptor 0x6490A8; PSX 0x801F42B0)
// ============================================================================

// ============================================================================
// Area 197 (descriptor 0x649780; PSX 0x801F49E4)
// ============================================================================

// ============================================================================
// Area 198 (descriptor 0x649CC0; PSX 0x801F4294)
// ============================================================================

// ============================================================================
// Area 199 (descriptor 0x64AD68)
// ============================================================================

void AreaW4f_Inject() {
    if (bof3::WantsShadow("area_w4f")) area_w4f::SelfTest();
}
