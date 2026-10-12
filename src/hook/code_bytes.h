// The values of ours that stand for bytes of Capcom's code
// (docs/operand-store.md; docs/state3-build.md section 2.3).
//
// Some of our implementations need what an instruction of Capcom's held: an
// operand a divergence patched (the slide bounds DIV-0041 widens, the title
// menu's widths DIV-0014 writes, the Config row's operands), the target of a
// call a RetargetCall re-aimed (which frame or text draw a screen uses), or a
// table the compiler put in the code (a jump table's byte index). Until the
// disc-only round they read those bytes back - hosted from the code itself,
// in the exe host from a store filled from the player's BOF3.exe. Now each
// is a value of ours, owned where it belongs: a patched operand by the
// divergence that patches it (which knows what it wrote), a re-aimed call by
// the divergence that re-aims it (which draw, from RetargetCall's answer), a
// compiler table by our reimplementation of the function that held it (its
// logic, as a switch or a rule). Every reader is the same code in both hosts
// and reads no byte of Capcom's code.
//
// What is left here is the hosted check that keeps that honest: each owner
// registers its value against the address it stands for, and
// Operands_Check, hosted, compares every one with the code at that address
// (the patched code: PatchBytes and RetargetCall still write Capcom's code
// hosted) - a difference is a Fatal naming the site. In the exe host there
// is no code to compare and the same table is only the inventory of what used
// to be read: Operands_Check logs it and reads nothing.
#pragma once

#include <cstdint>

namespace bof3 {

// `n` bytes (1..64) at `ours` are our value for the n bytes of Capcom's code
// at `va`. Read through the pointer at each check, so a value that changes
// when a patch goes in (or a late chunk arrives) stays one entry. A second
// registration of the same site and address is ignored.
void Operand(const char* site, std::uint32_t va, const void* ours, std::uint32_t n);

// The same for a value that never changes: the n bytes are copied.
void OperandExpect(const char* site, std::uint32_t va, const void* bytes, std::uint32_t n);

// The E8 rel32 call at `va`: our value is the address it calls, read through
// `target` at each check.
void OperandCall(const char* site, std::uint32_t va, const std::uint32_t* target);

// A table our code holds as a rule rather than as bytes (field_blocked's cell
// kinds, which pick between two outcomes through three cases): `same`, run
// hosted at each check, compares the rule with the n bytes at va.
void OperandRule(const char* site, std::uint32_t va, std::uint32_t n, bool (*same)());

// Hosted: every entry compared with Capcom's code now; a difference is a
// Fatal naming the site and the first byte. Exe host: one log line, the
// inventory's count; nothing read. Called after InjectAll (InjectReport) and
// after each late patch (the overlay chunks that arrive with FIRST.DAT and
// START.DAT).
void Operands_Check(const char* when);

}  // namespace bof3
