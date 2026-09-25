#!/bin/sh
# The lifter prototype's self-test (docs/lifter-feasibility.md section 4):
# build the synthetic corpus, lift it, fuzz the lifted C against the original
# bytes in Unicorn - which must match on every comparable round - then run the
# negative controls, each of which must be REFUSED. A fuzz that cannot fail
# proves nothing (docs/psx-library-layer.md, the deliberately wrong builds).
#
#   sh tools/lift/selftest.sh [OUTDIR] [ROUNDS]
#
# Needs clang (i686-w64-mingw32 target), lld, llvm-nm, a host C compiler, and
# python with capstone and unicorn. Reads no game data.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=${1:-$(mktemp -d)}
rounds=${2:-500}
mkdir -p "$out"
py=$(command -v python || command -v python3)
cc=${CC:-cc}
flags="-O2 -fPIC -shared -frounding-math -fno-strict-aliasing -Wall -Wno-unused-label -Wno-unused-variable -Wno-unused-but-set-variable"

sh "$here/corpus/build.sh" "$out"
"$py" "$here/lift_x86.py" --pe "$out/corpus.exe" --entries "$out/entries.txt" --out "$out/lifted.c"

build() {   # build SOURCE LIB
    $cc $flags -I "$here" "$1" "$here/lift_rt.c" -lm -o "$2"
}
fuzz() {    # fuzz LIB [extra args]: prints the summary, returns the fuzz's status
    lib=$1; shift
    "$py" "$here/lift_fuzz.py" --pe "$out/corpus.exe" --entries "$out/entries.txt" \
        --lib "$lib" --rounds "$rounds" "$@" > "$out/fuzz.log" 2>&1 && st=0 || st=$?
    tail -n 1 "$out/fuzz.log"
    return $st
}

echo "== the lifted corpus against the original"
build "$out/lifted.c" "$out/lifted.so"
if ! fuzz "$out/lifted.so"; then
    grep -B1 'first:' "$out/fuzz.log" || true
    echo "selftest: FAILED - the faithful lift mismatches"
    exit 1
fi

# Negative controls: one deliberate fault each, in the lifted C or in the
# oracle's set-up. Each must produce mismatches.
failed=0
control() {   # control NAME SED-EXPRESSION [fuzz args]
    name=$1; expr=$2; shift 2
    sed "$expr" "$out/lifted.c" > "$out/control.c"
    if cmp -s "$out/lifted.c" "$out/control.c"; then
        echo "   $name: the mutation matched nothing - control is void"; failed=1; return
    fi
    build "$out/control.c" "$out/control.so"
    if fuzz "$out/control.so" "$@" > /dev/null; then
        echo "   $name: NOT refused"; failed=1
    else
        echo "   $name: refused ($(tail -n 1 "$out/fuzz.log" | sed 's/lift_fuzz: //'))"
    fi
}
echo "== negative controls"
control "adc without its carry" 's/u64 t_ = (u64)a_ + b_ + cf/u64 t_ = (u64)a_ + b_/'
control "sar as a logical shift" 's/r_ = (u32)(sa_ >> (n_/r_ = (u32)((u32)sa_ >> (n_/'
control "cmp's overflow from the wrong operand" 's/of = (((a_ ^ b_) \& (a_ ^ r_))/of = (((a_ ^ b_) \& (b_ ^ r_))/'
control "fdivr's operands swapped" 's|FST(0) = FST(\([0-9]\)) / FST(0);|FST(0) = FST(0) / FST(\1);|'
control "a float store kept at double width" 's/STF32(\(.*\), FST(0));/STF64(\1, FST(0));/'
# The oracle at 64-bit precision: the lifter's doubles are exact only under
# the 53-bit control word BOF3.exe was measured running.
# The lifted C is the faithful one; the fault is in the oracle's set-up.
if fuzz "$out/lifted.so" --cw 0x037F > /dev/null; then
    echo "   the oracle at 64-bit precision (0x037F): NOT refused"; failed=1
else
    echo "   the oracle at 64-bit precision (0x037F): refused ($(tail -n 1 "$out/fuzz.log" | sed 's/lift_fuzz: //'))"
fi

if [ $failed -ne 0 ]; then
    echo "selftest: FAILED - a negative control was not refused"
    exit 1
fi
echo "selftest: passed"
