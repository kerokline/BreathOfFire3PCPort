#!/usr/bin/env python
"""Passively sample a running BOF3.exe and log state changes over time.

    python tools/attract_watch.py --minutes 20 --out analysis/attract/run1.tsv

Read-only: OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION) and
ReadProcessMemory. Nothing is written to the game and no input is sent — a
keypress would end the attract sequence this exists to observe
(docs/IDEAS.md I6, docs/attract-mode.md).

The game only advances while its window is the foreground window
(docs/windowed-mode.md, "Focus loss"), so the `active` column matters: a row
with active=0 is a frozen game, not a quiet one.

What is sampled, every ~5 ms, logged only when something changes:

  frames   logic frames seen, counted from flips of byte 0x905B89 (toggles once
           per logic frame in WinMain's loop). An undercount if this script
           stalls for more than one 33 ms frame.
  rand_n   how many times Rand 0x5B93D2 has been called on the main thread
           since launch. The CRT seed starts at 1 (0x5BAD5C) and the binary
           contains no srand, so the seed is a pure function of the call count;
           we read the seed from the main thread's CRT per-thread block
           (TLS index at 0x672950, _holdrand at +0x14) and step the LCG forward
           to recover the count.
  msg      current message index, 0x7DEE48 (MsgBoxState).
  area     0x904EFC, believed to be the current area number (hypothesis).
  seq      the two sequence words 0x66C7E8 / 0x66C7EA.

Output is derived from a running copy of the game: it goes under analysis/,
which is gitignored.
"""
import argparse, ctypes, ctypes.wintypes as w, os, struct, subprocess, sys, time

k = ctypes.WinDLL('kernel32', use_last_error=True)
u = ctypes.WinDLL('user32')
ntdll = ctypes.WinDLL('ntdll')

A_FLIP, A_ACTIVE, A_MSG, A_AREA, A_SEQ, A_TLSIDX = (
    0x905B89, 0x6BC63B, 0x7DEE48, 0x904EFC, 0x66C7E8, 0x672950)
LCG_A, LCG_C = 0x343FD, 0x269EC3


def find_game(wait=0.0):
    """pid of the running BOF3.exe; with `wait`, poll that many seconds for one."""
    t0 = time.time()
    want = os.environ.get('BOF3X_RUN_PID')   # attract_run.py names the game it started
    while True:
        out = subprocess.run(['tasklist', '/FI', f'PID eq {want}' if want else 'IMAGENAME eq BOF3.exe', '/FO', 'CSV', '/NH'],
                             capture_output=True, text=True).stdout
        if 'BOF3' in out:
            return int(out.split(',')[1].strip('"'))
        if time.time() - t0 >= wait:
            sys.exit('BOF3.exe is not running')
        time.sleep(0.05)


class Game:
    def __init__(self, pid):
        self.pid = pid
        self.h = k.OpenProcess(0x0410, False, pid)
        if not self.h:
            sys.exit(f'OpenProcess failed: {ctypes.get_last_error()}')

    def rd(self, addr, n):
        buf = ctypes.create_string_buffer(n)
        got = ctypes.c_size_t()
        if not k.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, n, ctypes.byref(got)):
            raise EOFError
        return buf.raw

    def main_window_thread(self):
        found = []

        @ctypes.WINFUNCTYPE(ctypes.c_bool, w.HWND, w.LPARAM)
        def cb(hwnd, _):
            p = w.DWORD()
            tid = u.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
            if p.value == self.pid and u.IsWindowVisible(hwnd):
                found.append(tid)
            return True
        u.EnumWindows(cb, 0)
        return found[0] if found else None

    def holdrand_addr(self):
        """Address of the main thread's _holdrand, or None."""
        tid = self.main_window_thread()
        if tid is None:
            return None
        th = k.OpenThread(0x0040, False, tid)      # THREAD_QUERY_INFORMATION
        if not th:
            return None

        class TBI(ctypes.Structure):
            _fields_ = [('ExitStatus', ctypes.c_long), ('Teb', ctypes.c_void_p),
                        ('Pid', ctypes.c_void_p), ('Tid', ctypes.c_void_p),
                        ('Affinity', ctypes.c_void_p), ('Prio', ctypes.c_long),
                        ('BasePrio', ctypes.c_long)]
        tbi = TBI()
        if ntdll.NtQueryInformationThread(th, 0, ctypes.byref(tbi), ctypes.sizeof(tbi), None):
            return None
        teb32 = tbi.Teb + 0x2000                   # WOW64: 32-bit TEB follows the 64-bit one
        idx, = struct.unpack('<I', self.rd(A_TLSIDX, 4))
        if idx >= 64:
            return None
        ptd, = struct.unpack('<I', self.rd(teb32 + 0xE10 + 4 * idx, 4))
        return ptd + 0x14 if ptd else None

    def focused(self):
        p = w.DWORD()
        u.GetWindowThreadProcessId(u.GetForegroundWindow(), ctypes.byref(p))
        return p.value == self.pid


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--minutes', type=float, default=20)
    ap.add_argument('--out', required=True)
    ap.add_argument('--wait', type=float, default=0,
                    help='seconds to wait for the game to start and show its window '
                         '(use when recording from launch)')
    a = ap.parse_args()

    g = Game(find_game(a.wait))
    seed_addr = g.holdrand_addr()
    t_wait = time.time()
    while seed_addr is None and time.time() - t_wait < a.wait:
        time.sleep(0.02)                # no window yet: the CRT block is found via its thread
        try:
            seed_addr = g.holdrand_addr()
        except EOFError:
            seed_addr = None
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)

    # Recover the absolute rand() call count by stepping the LCG from seed 1.
    seed = rand_n = None
    if seed_addr:
        target, = struct.unpack('<I', g.rd(seed_addr, 4))
        s, n = 1, 0
        while s != target and n < 50_000_000:
            s = (s * LCG_A + LCG_C) & 0xFFFFFFFF
            n += 1
        if s == target:
            seed, rand_n = s, n
    print(f'pid {g.pid}  holdrand @ {seed_addr and hex(seed_addr)}  '
          f'rand calls so far: {rand_n}', flush=True)

    frames = 0
    last_flip = g.rd(A_FLIP, 1)
    last_row = None
    t0 = time.time()
    with open(a.out, 'w', encoding='utf-8', newline='\n') as f:
        f.write(f'# pid {g.pid} started {time.strftime("%Y-%m-%d %H:%M:%S")} '
                f'rand_n_at_start {rand_n}\n')
        f.write('t\tframes\tactive\tfocused\trand_n\tmsg\tarea\tseq0\tseq1\n')
        try:
            while time.time() - t0 < a.minutes * 60:
                flip = g.rd(A_FLIP, 1)
                if flip != last_flip:
                    frames += 1
                    last_flip = flip
                if seed is not None:
                    target, = struct.unpack('<I', g.rd(seed_addr, 4))
                    steps = 0
                    while seed != target and steps < 2_000_000:
                        seed = (seed * LCG_A + LCG_C) & 0xFFFFFFFF
                        steps += 1
                    if seed != target:          # lost track: say so, stop counting
                        seed, rand_n = None, None
                    else:
                        rand_n += steps
                active = g.rd(A_ACTIVE, 1)[0]
                msg, = struct.unpack('<H', g.rd(A_MSG, 2))
                area, = struct.unpack('<H', g.rd(A_AREA, 2))
                s0, s1 = struct.unpack('<HH', g.rd(A_SEQ, 4))
                row = (active, rand_n, msg, area, s0, s1)
                if row != last_row:
                    f.write(f'{time.time()-t0:.3f}\t{frames}\t{active}\t{int(g.focused())}\t'
                            f'{rand_n}\t{msg:#06x}\t{area:#06x}\t{s0}\t{s1}\n')
                    f.flush()
                    last_row = row
                time.sleep(0.005)
        except EOFError:
            f.write(f'# game exited at t={time.time()-t0:.1f}\n')
    print(f'done: {frames} logic frames in {time.time()-t0:.0f} s -> {a.out}')


if __name__ == '__main__':
    main()
