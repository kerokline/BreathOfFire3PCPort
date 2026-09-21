#!/usr/bin/env python
"""Who is running? Read the call chain of each sleeping task in a running BOF3.exe.

    python tools/task_stacks.py                 one sample, every task
    python tools/task_stacks.py --watch 30      sample for 30 s, print each NEW chain
    python tools/task_stacks.py --label menu    also save to analysis/taskstacks/menu.txt

Read-only: OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION) and
ReadProcessMemory, as tools/attract_watch.py. Nothing is written, no thread is
suspended, no input is sent.

Why this works without touching a thread: all game logic runs in four
cooperative coroutines (docs/attract-mode.md section 2). Between logic frames
every task is asleep inside Task_Sleep 0x5A9949 with its `esp` saved in its
record (Task_Records 0x66C7D0, 0x20 bytes each: u16 state, u16 frames, u32
esp). So the words from the saved esp upward are a frozen, exact stack. A word
is reported as a return address when it points into .text just after a `call`
instruction; the function holding it comes from analysis/pc_funcs.json and the
name, if any, from symbols.toml.

Two cautions. MSVC6 omits frame pointers, so a stale return address in a dead
local slot can appear in the chain - rare, because the scan starts at the live
esp, but a function that shows up in one sample only is a candidate for that.
And a sample taken while a task is RUNNING reads a saved esp from the previous
frame; the stack above it may have changed. Chains that repeat are real.
"""
import argparse, bisect, ctypes, json, os, struct, subprocess, sys, time, tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
k = ctypes.WinDLL('kernel32', use_last_error=True)

TASK_RECORDS, TASK_SIZE, TASK_COUNT = 0x66C7D0, 0x20, 4
STACK_SIZE = 0x4000
STATE = {0: 'free', 1: 'sleeping', 2: 'runnable'}


def find_game():
    out = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq BOF3.exe', '/FO', 'CSV', '/NH'],
                         capture_output=True, text=True).stdout
    if 'BOF3' not in out:
        sys.exit('BOF3.exe is not running')
    return int(out.split(',')[1].strip('"'))


class Game:
    def __init__(self, pid):
        self.h = k.OpenProcess(0x0410, False, pid)
        if not self.h:
            sys.exit('OpenProcess failed: %d' % ctypes.get_last_error())

    def read(self, addr, n):
        buf = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not k.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)):
            return None
        return buf.raw[:got.value]


class Funcs:
    def __init__(self):
        d = json.load(open(os.path.join(ROOT, 'analysis', 'pc_funcs.json')))
        self.lo = int(d['text']['va'], 16)
        self.hi = self.lo + d['text']['size']
        self.entries = sorted(f['entry'] for f in d['functions'])
        self.sizes = {f['entry']: f['size'] for f in d['functions']}
        sym = tomllib.load(open(os.path.join(ROOT, 'symbols.toml'), 'rb'))
        self.names = {f['pc']: f['name'] for f in sym.get('func', [])}

    def holder(self, addr):
        i = bisect.bisect_right(self.entries, addr) - 1
        if i < 0:
            return None
        e = self.entries[i]
        return e if addr < e + self.sizes[e] + 16 else None

    def label(self, entry):
        n = self.names.get(entry)
        return '0x%06X %s' % (entry, n) if n else '0x%06X' % entry


def after_call(game, ret):
    """True if the bytes before `ret` decode as a call ending exactly there."""
    b = game.read(ret - 7, 7)
    if not b or len(b) < 7:
        return False
    if b[2] == 0xE8:                                   # call rel32
        return True
    if b[1] == 0xFF and (b[2] & 0x38) == 0x10 and (b[2] & 0xC7) == 0x05:
        return True                                    # call [abs32]
    if b[0] == 0xFF and (b[1] & 0x38) == 0x10 and (b[1] & 0xC0) == 0x00 and (b[1] & 7) == 4:
        return True                                    # call [sib + disp32]
    if b[5] == 0xFF and (b[6] & 0xF8) in (0xD0, 0x10):
        return True                                    # call reg / call [reg]
    if b[4] == 0xFF and (b[5] & 0xF8) == 0x50:
        return True                                    # call [reg + disp8]
    if b[1] == 0xFF and (b[2] & 0xF8) == 0x90:
        return True                                    # call [reg + disp32]
    return False


def chain(game, funcs, esp, cache):
    top = (esp | (STACK_SIZE - 1)) + 1                 # a guess at the stack's end; harmless if wrong
    raw = game.read(esp, min(top - esp, STACK_SIZE))
    if not raw:
        return []
    out = []
    for i in range(0, len(raw) - 3, 4):
        v = struct.unpack_from('<I', raw, i)[0]
        if not (funcs.lo <= v < funcs.hi):
            continue
        if v not in cache:
            cache[v] = after_call(game, v)
        if cache[v]:
            out.append((esp + i, v, funcs.holder(v)))
    return out


def sample(game, funcs, cache):
    rec = game.read(TASK_RECORDS, TASK_SIZE * TASK_COUNT)
    tasks = []
    for t in range(TASK_COUNT):
        state, frames, esp = struct.unpack_from('<HHI', rec, t * TASK_SIZE)
        priv = struct.unpack_from('<6I', rec, t * TASK_SIZE + 8)
        tasks.append((t, state, frames, esp, priv, chain(game, funcs, esp, cache) if state else []))
    return tasks


def render(funcs, tasks):
    lines = []
    for t, state, frames, esp, priv, ch in tasks:
        lines.append('task %d  %-8s sleep %-3d esp 0x%08X  priv %s' % (
            t, STATE.get(state, state), frames, esp, ' '.join('%08X' % p for p in priv)))
        for slot, ret, holder in ch:                   # innermost first
            lines.append('    [0x%08X] ret 0x%06X  in %s +0x%X' % (
                slot, ret, funcs.label(holder) if holder else '?', ret - holder if holder else 0))
    return lines


def key(tasks):
    return tuple((t, tuple(r for _, r, _ in ch)) for t, _, _, _, _, ch in tasks)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--watch', type=float, default=0, help='seconds to keep sampling')
    ap.add_argument('--label', help='save output under analysis/taskstacks/<label>.txt')
    a = ap.parse_args()
    game, funcs, cache = Game(find_game()), Funcs(), {}
    out, seen = [], set()
    t0 = time.time()
    while True:
        tasks = sample(game, funcs, cache)
        kk = key(tasks)
        if kk not in seen:
            seen.add(kk)
            block = ['--- t=%.2fs' % (time.time() - t0)] + render(funcs, tasks)
            print('\n'.join(block), flush=True)
            out += block
        if time.time() - t0 >= a.watch:
            break
        time.sleep(0.01)
    if a.label:
        d = os.path.join(ROOT, 'analysis', 'taskstacks')
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, a.label + '.txt'), 'w').write('\n'.join(out) + '\n')


if __name__ == '__main__':
    main()
