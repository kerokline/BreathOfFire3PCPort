# Windowed mode — it is built in

**Status:** STABLE (verified 2026-09-19)

The port launches exclusive-fullscreen by default, but it ships a windowed mode
and a runtime toggle. Nothing here is a patch or a divergence: it is the
original program's own behaviour, reached through its own inputs.

## How

- **At startup:** a text file `BOF3.CFG` in the game directory. Line 1 is the
  fullscreen flag, line 2 the render-mode flag:

  ```
  0
  1
  ```

  The file does not ship and nothing in the exe writes it under that name
  (one reference to the filename string, in the reader). Without it both flags
  default to 1. Since 2026-09-20 `bof3x-launcher` writes lines 1-2 of this file
  from its settings dialog, preserving every later line
  ([`launcher-settings.md`](launcher-settings.md) section 3).
- **At runtime:** **F8** toggles fullscreen/windowed, provided the desktop is
  larger than 640x480. **F7** re-initialises the display with the second flag
  (renderer select — see below).

**Observed 2026-09-19** with the two-line file above, via `bof3x-launcher`:
window style `0x14CA0000`, outer size 646x509 (a 640x480 client area), centred
on a 3440x1440 desktop. The FMVs honour it too — `Cfg_Fullscreen` is the third
argument of both `Fmv_Play` calls, so the exclusive 640x480x16 mode-set that
[`replacing-mci.md`](replacing-mci.md) §6 calls the most fragile operation in the
program **does not happen in windowed mode.**

## Evidence

`python tools/pe_disasm.py 0x4fd030` (`Cfg_Load`), `0x4fcc50:75` (WinMain's
window creation), `0x4fc7b4:42` and `0x4fc836:12` (WndProc key handling);
`python tools/pe_xref.py 0x65da44 0x65da48`. Names and per-claim evidence are in
[`symbols.toml`](../symbols.toml) under "Startup configuration".

- `Cfg_Load` `0x4FD030`: `fopen("BOF3.CFG", "rt")`, `fgets` 0x14 bytes a line;
  line 0 → `atoi` → `Cfg_Fullscreen` `0x65DA44`; line 1 → `atoi` →
  `0x65DA48`; every later line is `sscanf`'d as two integers into a byte-pair
  array handed to `0x5A9860`. With two lines or fewer, `0x5A9880` runs instead
  (defaults, by role).
- WinMain `0x4FCB00`: a 640x480 desktop forces fullscreen; otherwise flag 0 →
  `AdjustWindowRect(style 0xCA0000)` and centre.
- WndProc `0x4FC6F0` (`lpfnWndProc`, stored at `0x4FCB98`; corrected 2026-09-21 from `0x4FC6A0`, the function before it - [`attract-remaining.md`](attract-remaining.md) §3), `WM_KEYDOWN`: `0x77` F8 → `Cfg_Fullscreen ^= 1`,
  `SetWindowPos`, `0x5A5160(hwnd, &Cfg_Fullscreen, &0x65DA48, 0)`. `0x76` F7 →
  the same re-init, then `0x5A6690(value)`. `0x78` F9 toggles byte `0x6BC63A`
  around calls into the sound module (`0x587B90` / `0x587C30`) — a pause, by
  shape; unread.

## Focus loss: the engine stops too — and then fast-forwards

Reported by the owner 2026-09-19 as "navigating off pauses the render but not
the engine". That is what it looks like from outside; the mechanism is
different, and worse.

WinMain's loop (`python tools/pe_disasm.py 0x4fcd90:95 0x4fcede:75`):

1. Pump messages. **If the app-active byte `0x6BC63B` is 0, go back to 1** —
   no logic, no render. `WM_ACTIVATEAPP` (`0x1C`) clears it and calls
   `0x587C30`, sets it and calls `0x587B90` (sound pause/resume, by role).
2. If `GetTickCount()` is still **before** the frame deadline (float at
   `0x6BC628`): build and present the frame (`0x59EE50`, flip `0x5A66B0`).
   **If the deadline has already passed, presentation is skipped** — frame
   skip.
3. Spin until the deadline, then `deadline += 33.334 ms` (double at `0x5C4218`;
   the initial offset at `0x5C4220` is 33.34) and run one logic frame — 30 Hz.
   If the deadline exceeds 2^32 (float at `0x5C4214`) that much is subtracted:
   tick-count wraparound, not a clamp on lag.

The deadline keeps no relation to wall-clock time while step 1 is spinning, and
**nothing clamps the debt**. So after *t* seconds unfocused, the game runs
~30·*t* logic frames back to back, as fast as the CPU allows, with step 2
skipping every present until the deadline catches up. The time away is not
paused; it is replayed, unrendered, on return.

**Measured 2026-09-19** on the running game, unfocused, by read-only
`ReadProcessMemory` sampling: app-active = 0; the per-logic-frame flip byte
`0x905B89` changed **0 times** in six 0.5 s windows (it flips every frame when
running); `deadline - GetTickCount()` fell from -881 ms to -3381 ms over those
same 2.5 s — exactly 1000 ms per second of debt accruing.

Also visible here: the deadline is a **32-bit float holding a millisecond tick
count**, so its resolution is 4-8 ms at typical uptimes and degrades as uptime
grows. Frame pacing in this port is only as good as that.

This is original behaviour, recorded as such (`CLAUDE.md` rule 6). Clamping the
debt on reactivation is a small, obviously-wanted fix, and it is a
**divergence** — it needs a [`DIVERGENCE.md`](DIVERGENCE.md) entry when built,
and it matters to any attract-mode oracle ([`IDEAS.md`](IDEAS.md) I6), since a
focus change mid-run must not alter the logic-frame sequence. (It does not, in
fact: frames are replayed, not dropped. Only their timing changes.)

## Open

- **Lines 3+ of `BOF3.CFG`** — integer pairs consumed by `0x5A9860`. Key or pad
  bindings is the obvious guess; unread. `DINPUT` lives near `0x5A9xxx`.
- **What `0x65DA48` selects.** `0x5A5160` is the only function referencing the
  `Software Render` string, so hardware/software is likely; which value is
  which is not established.
- **Window size is fixed at 640x480.** Scaling is presentation-layer work
  ([`IDEAS.md`](IDEAS.md) I8). The known "fullscreen fallback" defect has still
  not been reproduced and written down.
