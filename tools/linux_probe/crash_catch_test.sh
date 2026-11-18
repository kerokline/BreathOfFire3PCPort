#!/usr/bin/env bash
# The Linux crash catcher's test (tools/linux_probe/crash_catch_test.cpp): built
# with the system's g++, run once per mode; each mode's log is kept as
# <out>/<mode>.log for tools/crash_report.py --log, and the signal that ended
# each run is printed. Run from the repository's root, on Linux or in the WSL:
#
#   tools/linux_probe/crash_catch_test.sh <gen> <out>
#
# <gen>: a folder holding bof3/symbols.gen.h (python tools/gen_symbols.py
# --toml symbols.toml --out <gen>/bof3/symbols.gen.h). <out>: where the program
# and the logs go.
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
for mode in read write exec bus fpe ill abort trap selftest; do
    rm -f "$out/crash_catch_test.log"
    "$out/crash_catch_test" "$mode" > /dev/null 2>&1
    status=$?
    cp "$out/crash_catch_test.log" "$out/$mode.log" 2>/dev/null
    if [ $status -gt 128 ]; then
        echo "$mode: ended by signal $((status - 128)); $(grep -c '^CRASH\|^exittrace' "$out/$mode.log") report lines"
    else
        echo "$mode: exit $status (not a signal): FAILED"
        fail=1
    fi
done
exit $fail
