// The one spelling of a typed view onto a game record (docs/typed-records.md;
// docs/typed-data-round.md section 3, "C0 as run"). Stage B1's seven file
// groups each wrote their own per-file helpers for this (ScTask(), CurFx(),
// AsFx(p), MemberRec(i), Spr(p), Bytes(p), a per-file Store and two dozen
// more); stage C0 replaced every one with the five below.
//
// Every view is a cast and nothing else: no load, no arithmetic. Where a
// module reaches memory through a harness's out-of-line Mem() (the area_w*,
// magic_*, boss_*, scena_* modules and the fuzz files), the pointer handed to
// a view still comes from Mem(...); the view does not change where it comes
// from, so the seam stays (typed-data-stage-c.md section 1).
//
// The names live in namespace rv ("record views") and are always spelled
// qualified: about thirty files keep a local function-pointer As<T>(f) and
// nine a local Current(), which would hide an unqualified shared name.
#pragma once

#include "bof3/symbols.gen.h"
#include "game/records.h"

namespace rv {

// As<T>(p): the record of family T that starts at p. A const p gives a
// const view, a writable p a writable one (the per-file helpers' overload
// pairs). The family is the caller's claim, with its evidence beside the
// call: the gate cannot see a wrong family (typed-data-stage-c.md section 1).
template <class T> inline T* As(unsigned char* p) { return reinterpret_cast<T*>(p); }
template <class T> inline const T* As(const unsigned char* p) { return reinterpret_cast<const T*>(p); }

// AsConst<T>(p): a read-only view of a writable p, for a local that only
// reads (the former reinterpret_cast<const records::T*>(p)).
template <class T> inline const T* AsConst(const unsigned char* p) { return reinterpret_cast<const T*>(p); }

// Current<T>(): the record Sprite_Current points at. Sprite_Current is read
// from memory at every call, as the original reads it after each call it
// makes; hold the result in a local only where the original holds it too.
template <class T> inline T* Current() { return reinterpret_cast<T*>(Sprite_Current); }

// Bytes(r): a typed record's own bytes, for an access the struct does not
// name (a raw accessor at an offset records.h pads) or a callee that takes
// the raw pointer.
template <class T> inline unsigned char* Bytes(T* r) { return reinterpret_cast<unsigned char*>(r); }
template <class T> inline const unsigned char* Bytes(const T* r) { return reinterpret_cast<const unsigned char*>(r); }

// Store(field, value), or Store<P>(field, value): a setter's store through a
// typed field, in the setter's order.
//
// The assignment `p->f = expr` evaluates expr before p (C++17 [expr.ass]),
// where the setter call `SetLong(p + off, expr)` it replaced evaluated its
// arguments in this toolchain's order for a call: the pointer first, then
// the value. Where the pointer is re-read from memory (Sprite_Current, a
// module's Cur() or Sc()) and the value's computation can change it, or
// simply where the compiler orders the loads differently, the two compile
// differently and the gate refuses the plain assignment. Store is a call
// like the setter, so its two arguments are evaluated as the setter's were:
// the field's pointer (and so the record's address) first, then the value.
// The standard leaves a call's argument order unspecified; "as the setter"
// is what holds, and tools/codegen_same.py is what proves it per object.
//
// The value goes through the setter's parameter type P (Store<U>(f, v) for a
// setter taking U, or P deduced from v for one taking v's own type), then
// converts to the field's type as the setter's memcpy or narrowing did.
template <class P, class T> inline void Store(T& field, P value) { field = static_cast<T>(value); }

}  // namespace rv
