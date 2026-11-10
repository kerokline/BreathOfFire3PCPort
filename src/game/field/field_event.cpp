// The event script's field side: the party leader's frame (0x52D8F0) and its
// first two states (0x52D920, 0x52DA50 with its sub-state 0x52DA70), the area
// set-up's party placement (0x533110, 0x5334A0, 0x533580, 0x533BA0, 0x533CE0),
// a member joining (0x533EF0), the party-set loader (0x5367E0..0x536AC0), and
// the small helpers they share. docs/field-event.md has each function, its
// evidence, and the fuzz below with its negative controls.
//
// "Member i" is ObjTrio + i * 0x14C, the party's three field objects; their
// +0x80..+0x123 is a copy of an actor record 0x903A70 + a * 0xA4, a the
// member's Character_IndexById entry (kept in the member's +0x148).
//
// As in the original, every global is read again after each call and every
// object through Sprite_Current or Field_State each time it is used.
#include "game/field_event.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "bof3/symbols.gen.h"
#include "game/field_event_callees.h"
#include "game/move_script_bytes.h"
#include "game/record_views.h"
#include "game/records.h"
#include "hook/detour.h"
#include "hook/log.h"

namespace field_event {

namespace {
// A function pointer as another function type, through an integer.
template <class To> To Fn(std::uint32_t address) { return reinterpret_cast<To>(static_cast<std::uintptr_t>(address)); }
}  // namespace


}  // namespace field_event

namespace {

using namespace move_script;
namespace at = field_event::at;

std::uint32_t Address(const volatile void* p) { return Low32(p); }
unsigned char* Member(unsigned i) { return rv::Bytes(&ObjTrio[i]); }

}  // namespace

// --- the party leader's frame and its states --------------------------------

// --- the zone counter ------------------------------------------------------

// --- small helpers -----------------------------------------------------------

// --- the party's placement ---------------------------------------------------

// --- the party set -------------------------------------------------------------

void FieldEvent_Inject() {
    if (bof3::WantsShadow("field_event")) field_event::SelfTest();
}
