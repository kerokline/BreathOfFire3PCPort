// The region table's start-up check: region_table.h.
#include "hook/region_table.h"

#include <cstdint>

#include "bof3/region_table.gen.h"
#include "hook/exe_layout.h"
#include "hook/log.h"

namespace bof3 {

namespace {

namespace rg = region_gen;

const char* ClassName(rg::Class c) {
    const unsigned i = static_cast<unsigned>(c);
    return i < rg::kClasses ? rg::kClassName[i] : "?";
}

// The index of the run holding `va`, a VA inside the span (the runs are end to
// end and sorted, which the check proves before it calls this).
unsigned RunAt(std::uint32_t va) {
    unsigned lo = 0, hi = rg::kRuns;
    while (hi - lo > 1) {
        const unsigned mid = (lo + hi) / 2;
        if (rg::kRun[mid].va <= va)
            lo = mid;
        else
            hi = mid;
    }
    return lo;
}

}  // namespace

void RegionTable_Check() {
    static_assert(rg::kRuns > 0 && rg::kClasses == 5, "the region table has runs and five classes");

    // The span: the exe host's quicksave span (snapshot.cpp CurrentSpan).
    if (rg::kSpanVa != exe_layout::kDataVa || rg::kSpanEnd != exe_layout::kOwnVa)
        Fatal("region table: its span is 0x%08X..0x%08X, exe_layout.h's 0x%08X..0x%08X (kDataVa..kOwnVa): "
              "regenerate it (tools/gen_symbols.py)",
              rg::kSpanVa, rg::kSpanEnd, exe_layout::kDataVa, exe_layout::kOwnVa);

    // End to end, none empty, every class known; the counts by class.
    std::uint32_t bytes[rg::kClasses] = {};
    unsigned runs[rg::kClasses] = {};
    std::uint32_t at = rg::kSpanVa;
    for (unsigned i = 0; i < rg::kRuns; ++i) {
        const rg::Run& r = rg::kRun[i];
        const unsigned c = static_cast<unsigned>(r.cls);
        if (r.va != at || r.bytes == 0 || c >= rg::kClasses)
            Fatal("region table: run %u (0x%08X, 0x%X bytes, class %u, \"%s\") does not follow the run before it, "
                  "which ends at 0x%08X",
                  i, r.va, r.bytes, c, r.name, at);
        bytes[c] += r.bytes;
        ++runs[c];
        at = r.va + r.bytes;
    }
    if (at != rg::kSpanEnd) Fatal("region table: the runs end at 0x%08X, the span at 0x%08X", at, rg::kSpanEnd);
    for (unsigned c = 0; c < rg::kClasses; ++c)
        if (bytes[c] != rg::kBytes[c] || runs[c] != rg::kRunsOf[c])
            Fatal("region table: class %s has %u bytes in %u runs, the generator counted %u in %u", rg::kClassName[c],
                  bytes[c], runs[c], rg::kBytes[c], rg::kRunsOf[c]);

    // Every named run where this build's symbols put the name.
    for (unsigned i = 0; i < rg::kNamedStarts; ++i) {
        const rg::NamedStart& n = rg::kNamedStart[i];
        if (n.run >= rg::kRuns || rg::kRun[n.run].va != n.va)
            Fatal("region table: \"%s\" starts at 0x%08X in the table, at 0x%08X in symbols.gen.h",
                  n.run < rg::kRuns ? rg::kRun[n.run].name : "?", n.run < rg::kRuns ? rg::kRun[n.run].va : 0u, n.va);
    }

    // The process runs are the keep rows, byte for byte: every keep row's bytes
    // are process, and the process bytes add up to the keep rows' (which do not
    // overlap: the generator matched them to class rows, which cannot).
    std::uint32_t kept = 0;
    for (unsigned k = 0; k < rg::kKeptRows; ++k) {
        const rg::Range& kr = rg::kKept[k];
        for (std::uint32_t va = kr.va; va < kr.va + kr.bytes;) {
            const rg::Run& r = rg::kRun[RunAt(va)];
            if (r.cls != rg::Class::Process)
                Fatal("region table: keep row 0x%08X (0x%X bytes) holds 0x%08X, in run \"%s\" of class %s, not process",
                      kr.va, kr.bytes, va, r.name, ClassName(r.cls));
            va = r.va + r.bytes;
        }
        kept += kr.bytes;
    }
    if (kept != bytes[static_cast<unsigned>(rg::Class::Process)])
        Fatal("region table: the keep rows hold %u bytes, the process runs %u", kept,
              bytes[static_cast<unsigned>(rg::Class::Process)]);

    // Every skip-list byte classed by a row.
    for (unsigned s = 0; s < rg::kSkipRows; ++s) {
        const rg::Range& sr = rg::kSkip[s];
        if (sr.va < rg::kSpanVa || sr.va + sr.bytes > rg::kSpanEnd)
            Fatal("region table: skip row 0x%08X (0x%X bytes) is outside the span", sr.va, sr.bytes);
        for (std::uint32_t va = sr.va; va < sr.va + sr.bytes;) {
            const rg::Run& r = rg::kRun[RunAt(va)];
            if (r.from != rg::Source::Classes)
                Fatal("region table: skip row 0x%08X (0x%X bytes) holds 0x%08X, in run \"%s\", which no row of "
                      "tools/region_classes.txt classes",
                      sr.va, sr.bytes, va, r.name);
            va = r.va + r.bytes;
        }
    }

    Log("region_table %u runs over 0x%08X..0x%08X: state %u, process %u, derived %u, stack %u, unknown %u bytes; "
        "%u names at their places, %u keep rows process, %u skip rows classed",
        rg::kRuns, rg::kSpanVa, rg::kSpanEnd, bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], rg::kNamedStarts,
        rg::kKeptRows, rg::kSkipRows);
}

}  // namespace bof3
