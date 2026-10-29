// The bind table: Capcom's code VAs and the functions of ours that stand for
// them (docs/relocatable-image-design.md 4, mode B; the relocatable-image
// round's stage C2).
//
// bof3x.exe maps no code of Capcom's. Its .stub section is reserved address
// space with no access, a trap: every code word the game holds, and every
// code VA our code places (Code, CodePlace, common/image_base.h), is the VA's
// place - a jump of ours at (top << 24) | VA, one per VA, to the function of
// ours that symbols.toml's `impl` names for that VA - one representation in
// every table, record and hook cell, one that names its VA, and one whose low
// three bytes are the VA's (bind.cpp). The table is generated (tools/gen_symbols.py --bind-out, bind.gen.h)
// and static: it is there before DllMain, for the namespace-scope tables that
// place a code VA in the DLL's CRT start-up. Hosted, nothing is bound:
// BOF3.exe's code is mapped, a code VA is its own place, and Inject writes the
// jmp at it.
//
// A VA the table does not name gives the trap, Img(va): a call there faults
// on the no-access page, and the crash reporter names the VA and the
// [[func]] it lies in (Bind_FuncName); a call-through helper's miss is a Fatal
// before the call (CodeToCall). The one VA bound outside the table is
// DirectSoundCreate's import thunk 0x5ACBBC, not a [[func]]: bound to a thunk
// of ours that calls through the import slot 0x5C4014 as it reads at the call
// (exe_host.cpp BindSlots fills it; wav_dump.cpp may hook it later).
#pragma once

#include <cstdint>

namespace bof3 {

// True in bof3x.exe: code words are bound and .stub is the trap. Read from
// the main module's header on first use, so valid before DllMain; Bind_Report
// checks it against DetectHost's answer.
bool Bind_Active();

// The function of ours bound to `va`, or null (a VA the table does not name).
const void* Bind_Ours(std::uint32_t va);

// The reverse map: the VA a code place stands for, or 0 when `place` is not
// one. A place is (top << 24) | VA for a bound VA, the jump bind.cpp writes
// there in bof3x.exe, under a top byte no value of the game's data takes with a
// bound VA below it (measured); our function's own address is not a place. Hosted there is no table and this is 0. The
// state hash hashes a word holding a place as its VA (statehash.cpp).
std::uint32_t Bind_VaOf(std::uint32_t place);

// The VA whose function of ours starts at `ours` (our function's own address,
// not its place), or 0. For the state hash's watch on words that hold one in
// bof3x.exe (statehash.cpp Watch): a code word stored without Code().
std::uint32_t Bind_VaOfFunction(std::uint32_t ours);

// Whether the table lies under one of the five measured top bytes (0x7C,
// 0x70, 0x7E, 0x7B, 0x7D), under which no .data word of the measured dumps
// reads as a place. False hosted, and when the process could reserve none of
// them and the table took another top byte 0x20..0x7F: the game runs the same,
// and the state hash may rename a data word (statehash.cpp logs it).
bool Bind_TableMeasured();

// The table of code places, [lo, hi); both 0 hosted.
std::uint32_t Bind_TableLo();
std::uint32_t Bind_TableHi();

// The table's rows, for the state hash's self-test: how many, and row i's VA.
unsigned Bind_Rows();
std::uint32_t Bind_RowVa(unsigned i);

// The [[func]] of symbols.toml that `va` starts or lies in (the last one at
// or below it, within the code range), with its start; null outside.
const char* Bind_FuncName(std::uint32_t va, std::uint32_t* start);

// Inject in bof3x.exe: the pair (va, ours) must be the table's row, else a
// Fatal - the table and the injects come from the same `impl` binding, and a
// difference is a function bound to one thing and injected as another.
void Bind_Check(const char* name, std::uint32_t va, const void* ours);

// A code word of .data in bof3x.exe (exe_host.cpp Rebase): its VA's place,
// or the trap Img(va) for a word whose target has no function of ours (the
// 111 words design 4.2 counts). `bound` says which.
std::uint32_t Bind_Word(std::uint32_t va, bool* bound);

// One log line after InjectAll in either host: the table's rows, how many an
// Inject named (bof3x.exe), and every VA a table cell placed that the table
// does not name - each one's [[func]] - which a call would find trapped.
void Bind_Report();

// The bind table's own test (BOF3X_SELFTEST_ONLY, both hosts): the rows
// strictly ascending and every one a start the reverse map returns, the
// function names ascending, a lookup of each row's VA, a VA between two rows
// and one past the last missing, the trap's place, and the thunk.
void Bind_SelfTest();

}  // namespace bof3
