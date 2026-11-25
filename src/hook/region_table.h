// The region table's start-up check (the state-by-name round's A3,
// docs/region-table.md; docs/state-by-name-round.md 3.4).
//
// bof3/region_table.gen.h (tools/gen_symbols.py, tools/region_table.py) holds
// every run of the quicksave's span - exe_layout.h kDataVa..kOwnVa - with a
// name or an address, its length and a class: state, process, derived, stack,
// unknown. The generator refuses a table whose process rows are not
// snapshot.cpp's kKept, row for row, or that leaves a byte of
// tools/statehash_skip.txt unclassed; this check refuses, at every start, a
// table that does not fit the build it is in.
//
// It reads; it changes nothing the game does and nothing a state hash sees.
#pragma once

namespace bof3 {

// InjectAll, beside Snapshot_Inject: Fatal on any disagreement, one log line
// with the counts otherwise.
//   - the span is exe_layout.h's kDataVa..kOwnVa;
//   - the runs lie end to end over it, none empty, each of a known class;
//   - every run that starts a symbols.toml name starts at that name's VA in
//     this build's symbols.gen.h (for a GameData member, the compiler's
//     offsetof: the table against the layout, as Snapshot_Inject checks kKept's
//     literals against the names);
//   - the process runs cover exactly the keep rows the table was made with;
//   - every byte of the skip list's rows lies in a run classed by a row of
//     tools/region_classes.txt;
//   - the bytes and runs of each class are the generator's counts.
void RegionTable_Check();

}  // namespace bof3
