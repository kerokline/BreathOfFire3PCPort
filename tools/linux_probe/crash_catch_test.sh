#!/usr/bin/env bash
# The Linux crash catcher's test (tools/linux_probe/crash_catch_test.cpp): built
# with the system's g++, run once per mode; each mode's log is kept as
# <out>/<mode>.log for tools/crash_report.py --log, and each mode's end is
# checked against what it must be. Run from the repository's root, on Linux or
# in the WSL:
#
#   tools/linux_probe/crash_catch_test.sh <gen> <out>
#
# <gen>: a folder holding bof3/symbols.gen.h (python tools/gen_symbols.py
# --toml symbols.toml --out <gen>/bof3/symbols.gen.h). <out>: where the program
# and the logs go.
#
# A mode before the exe host's step 23 must end by its own signal, with CRASH
# lines (the trap: an exittrace line). A mode after it (the *unhandled ones)
# must end with exit code 3 and Fatal's line "a signal nobody handled". The
# overflows must have the report's line that names the guard page.
set -u
gen=$1
out=$2
mkdir -p "$out"
# -Wno-invalid-offsetof, -Wno-unknown-pragmas: the generated header's offsetof of GameData and its clang
# pragmas, which g++ names and clang does not.
g++ -std=c++20 -g -O1 -Wall -Wextra -Wno-invalid-offsetof -Wno-unknown-pragmas -Isrc -I"$gen" '-D__cdecl=__attribute__((ms_abi))' -D__stdcall= \
    tools/linux_probe/crash_catch_test.cpp src/hook/crash.cpp src/hook/crash_posix.cpp src/hook/log.cpp \
    src/os/posix/*.cpp src/os/file.cpp -o "$out/crash_catch_test" -lpthread -ldl || exit 1
fail=0
# mode, the signal it must end by (0: exit code 3 instead)
for pair in read:11 write:11 exec:11 bus:7 fpe:8 ill:4 abort:6 trap:5 selftest:11 overflow:11 \
            unhandled:0 overflow_unhandled:0 trap_unhandled:0; do
    mode=${pair%%:*}
    want=${pair#*:}
    rm -f "$out/crash_catch_test.log"
    "$out/crash_catch_test" "$mode" > /dev/null 2>&1
    status=$?
    cp "$out/crash_catch_test.log" "$out/$mode.log" 2>/dev/null
    lines=$(grep -c '^CRASH\|^exittrace' "$out/$mode.log")
    fatal=$(grep -c '^FATAL: a signal nobody handled' "$out/$mode.log")
    guard=$(grep -c '^CRASH [0-9]*: a stack overflow' "$out/$mode.log")
    verdict=ok
    if [ "$want" = 0 ]; then
        [ $status -eq 3 ] && [ "$fatal" -eq 1 ] || verdict=FAILED
        said="exit $status, $fatal Fatal line"
    else
        [ $status -eq $((128 + want)) ] && [ "$fatal" -eq 0 ] || verdict=FAILED
        said="$([ $status -gt 128 ] && echo "ended by signal $((status - 128))" || echo "exit $status")"
    fi
    [ "$lines" -gt 0 ] || verdict=FAILED
    case $mode in overflow*) [ "$guard" -eq 1 ] || verdict=FAILED; said="$said, $guard guard-page line" ;; esac
    echo "$mode: $said; $lines report lines: $verdict"
    [ $verdict = ok ] || fail=1
done
exit $fail
