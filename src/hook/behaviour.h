#pragma once

// Mod behaviours (docs/mods.md section 11; DIVERGENCE DIV-0157 as amended
// 2026-11-12): a change of ours that is off unless something asks for it - a
// romhack mod's `needs`, or the player's `on=` - each one a ledger entry with
// `Default: off` whose `Switch:` is the behaviour's name.
//
// The names are generated: tools/ledger_switches.py reads every
// `BehaviourOn(behaviours::Id::<Name>)` in src/ and writes the enum and the
// names into src/hook/code_names.h (namespace bof3::behaviours), so a name the
// code asks for and the ledger lacks, or the reverse, fails
// `python tools/ledger_check.py` and `python tools/ledger_switches.py --check`.
//
// The set that is on is BOF3X_ON (the launcher's ini key on=, and the needs of
// every mod it hands over), BOF3X_OFF's list syntax: names, `*` for every
// behaviour, `-Name` to take one back out. Both hosts read it once, first in
// InjectAll (Behaviour_Read). A name BOF3X_ON gives that is no behaviour of
// this build stops the game with a sentence naming it; each one on is logged.
// A behaviour BOF3X_OFF also names stays off (BOF3X_OFF wins, said in the log).

#include <cstddef>

#include "hook/code_names.h"

namespace bof3 {

// Whether behaviour `id` is on in this start. Cheap enough for every frame:
// an array read. Asked before Behaviour_Read it is a Fatal - the answer would
// be "off" whatever BOF3X_ON says (the hook-order trap of PR 217's IsEnabled
// guard, detour.cpp).
bool BehaviourOn(behaviours::Id id);

// Reads BOF3X_ON and BOF3X_OFF and fixes the answer of every behaviour for the
// start. Called once, first in InjectAll; a second call is a Fatal.
void Behaviour_Read();

// True if the n characters at `s` are a behaviour of this build (the
// generated list). detour.cpp's BOF3X_OFF check takes such a name.
bool KnownBehaviour(const char* s, std::size_t n);

// The list parser itself, on any table of names (the self-test gives its own):
// `on[i]` is set for every names[i] that `list` turns on. `strict`: a token that
// is no name of the table stops the walk - false is returned with the token in
// `bad`, `bad_len`. Not strict (BOF3X_OFF, which also holds code names): such a
// token is passed over.
bool BehaviourParse(const char* list, const char* const* names, int count, bool* on, bool strict,
                    const char** bad, std::size_t* bad_len);

// The parser's self-test, on names of its own (no game state): the exe host's
// BOF3X_SELFTEST_ONLY, or BOF3X_SHADOW=behaviours hosted. A Fatal on a case
// that does not hold.
void Behaviour_SelfTest();

}  // namespace bof3
