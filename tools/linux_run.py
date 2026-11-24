#!/usr/bin/env python
"""Start the Linux build of the game for the harness: the ELF directly when
this runs on Linux, through `wsl -e` when it runs on Windows (docs/linux-round.md,
stages LC and LD, row C8).

    python tools/input_run.py RECIPE --out X --host linux --elf PATH/bof3x
    python tools/abc_run.py run --set eu --route combat --host linux --elf PATH/bof3x
    python tools/linux_run.py selftest          # the path rules, no process started

A run of the Linux build needs no window and nothing of Windows' own: the
recipe is played by the engine (BOF3X_INPUT, src/hook/input_script.cpp), the
game writes its own frames (BOF3X_SHOT_DIR, BOF3X_SHOT_EVERY) and the state
hash to a file (BOF3X_STATEHASH), its log is the executable's path with the
extension .log (src/hook/log.cpp LogOpen: bof3x -> bof3x.log), and its end is
an exit code. A frozen shot (BOF3X_SHOT_WAIT) waits on the POSIX named
semaphore /bof3x_shot_<pid> (src/os/posix/named_event.cpp), which the driver
posts; unposted, the game goes on by itself after 3 s, on the same frame.

From Windows the ELF runs in the WSL's default distribution:

    wsl -e sh -c 'cd "$1" || exit 125; shift; echo "linux_run: pid $$"; exec env "$@"' \\
        linux_run <game> K=V ... <elf> [args]

so the shell's pid, printed first, is the game's (exec keeps it). Windows'
whole environment does not cross into the WSL; the variables the game and its
libraries read do - every name beginning BOF3X_, SDL_, DXVK_, VK_, MESA_,
LIBGL_, LP_ or GALLIUM_, and the names a caller lists - and every value that
is a Windows path (C:\\x or C:/x, \\\\wsl.localhost\\<distro>\\x) is written as
the WSL sees it (/mnt/c/x, /x). The game folder and the ELF's path are
translated the same way. On Linux the game gets the caller's environment
whole, as bof3x.exe gets it from the Windows launcher, and no path is
rewritten.

Inside Linux this file is also a small command line (the WSL's own python3
runs it; nothing here needs a package):

    python3 tools/linux_run.py post NAME     # sem_post /NAME: let a frozen shot go on
    python3 tools/linux_run.py unlink NAME   # sem_unlink /NAME, after a game that was ended
    python3 tools/linux_run.py busy ELF      # the pids whose executable is ELF, one a line
    python3 tools/linux_run.py kill PID      # SIGKILL to PID
    python3 tools/linux_run.py serve         # the same four as JSON lines on stdin and stdout

The Windows side starts `serve` in the WSL once per game and asks it: one
`wsl -e` costs one to three seconds on this machine, and a frozen shot waits
three.

What the Windows runners take from Windows (input_run.py, with the
attract_run.py and recipe_saves.py it imports, and abc_run.py), read
2026-11-19, and what this mode does in its place:

    the launcher (bof3x-launcher.exe --no-config --host, pid from its   the ELF itself, cwd the game folder,
      "(pid N)" line); it applies bof3x.ini and its defaults            BOF3X_LAUNCHER=1; no ini, no defaults
    liveness: tasklist /FI "PID eq N" every 50 ms                       the Popen handle (wsl.exe lives as long)
    the end: taskkill /F /PID N                                         SIGKILL by pid
    a leftover: PowerShell Get-Process, the process whose bof3x.dll     /proc/<pid>/exe equal to the ELF, by pid
      is the launcher folder's (attract_run.kill_stale, abc_run
      users_of and end_stale)
    the window: user32 EnumWindows, SetForegroundWindow each 0.5 s      none: the game writes its frames
      (keep_in_front), GetClientRect and PIL ImageGrab when the         (BOF3X_SHOT_DIR) and runs unfocused
      game wrote no frame; shcore DPI awareness at import
    a frozen shot's release: kernel32 OpenEventW and SetEvent on        sem_post /bof3x_shot_<pid>
      Local\\bof3x_shot_<pid>
    the slot-0 swap (recipe_saves.Slot0): file moves, plus kernel32     the same, on Windows only: the game
      OpenProcess for the lock holder, OpenEventW on                    folder is Windows'. On Linux a recipe
      Local\\bof3x_unattended_<pid> and tasklist for a player's game     save is refused (--no-save)
    paths: Windows paths in BOF3X_INPUT, _SHOT_DIR, _WAV_DIR,           each written as the WSL sees it
      _STATEHASH, _CACHE, _EXEIMAGE, _RECIPE_SAVES; the log beside
      the launcher's bof3x.dll; code names read from bof3x.dll/.exe     the log and the names from the ELF
    abc_run's 0xC0000142 retry (a runner that could not start)          not reached
    the registry: none                                                  none

A run by the BOF3X_ switches alone needs none of the window, focus, DPI,
tasklist, PowerShell or event calls; it needs a process started with an
environment and a working folder, its pid, its liveness, its end by pid, its
exit code, and the files it writes.
"""
import json, os, re, signal, subprocess, sys, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ON_WINDOWS = sys.platform == 'win32'

# The variables that cross into the WSL: the game's own switches and what
# SDL3, DXVK-native, the Vulkan loader and Mesa read.
PASSED_PREFIXES = ('BOF3X_', 'SDL_', 'DXVK_', 'VK_', 'MESA_', 'LIBGL_', 'LP_', 'GALLIUM_')
DRIVE = re.compile(r'^([A-Za-z]):[\\/](.*)$', re.S)
WSL_UNC = re.compile(r'^[\\/]{2}(?:wsl\.localhost|wsl\$)[\\/][^\\/]+(?:[\\/](.*))?$', re.I | re.S)
PID_LINE = re.compile(r'^linux_run: pid (\d+)\s*$', re.M)
SH = 'cd "$1" || exit 125; shift; echo "linux_run: pid $$"; exec env "$@"'
# The same with no display for the game (headless): the WSL's own environment
# carries WSLg's DISPLAY and WAYLAND_DISPLAY, which `env "$@"` would keep.
SH_HEADLESS = 'cd "$1" || exit 125; shift; echo "linux_run: pid $$"; exec env -u DISPLAY -u WAYLAND_DISPLAY "$@"'
# bof3x-launcher's line once the game has started (src/launcher/start.cpp).
LAUNCHED = re.compile(r'bof3x-launcher: started .* \(pid (\d+)\)')
# A headless run (the default of the harness on Linux): no window, no sound
# from the speakers, and no display a Fatal's message box could open on.
# SDL3's offscreen video driver gives DXVK-native a Vulkan surface
# (VK_EXT_headless_surface, docs/linux-round.md B2a); its dummy audio driver
# opens a device that takes the stream at the device's rate and plays it
# nowhere - SDL's audio thread still pulls the mix a buffer at a time, so the
# game's sound code (sound_sdl.cpp's cursors, the ring's notifications,
# Music_Pump) runs as with a device.
HEADLESS_SET = {'SDL_VIDEO_DRIVER': 'offscreen', 'SDL_AUDIO_DRIVER': 'dummy'}
HEADLESS_UNSET = ('DISPLAY', 'WAYLAND_DISPLAY')
RUN_GAME_MARK = '.bof3x_run_game'   # names a folder make_run_game built, which it may remove again

_mount_root = None


def mount_root():
    """Where the WSL mounts Windows' drives, '/mnt/' unless /etc/wsl.conf moves
    it: asked of wslpath once, '/mnt/' when that cannot be asked."""
    global _mount_root
    if _mount_root is None:
        out = ''
        try:
            out = subprocess.run(['wsl', '-e', 'wslpath', '-u', 'C:\\'], capture_output=True, text=True,
                                 timeout=120).stdout.strip()
        except (OSError, subprocess.TimeoutExpired):
            pass
        m = re.fullmatch(r'(/(?:.*/)?)c/?', out)
        _mount_root = m[1] if m else '/mnt/'
    return _mount_root


def is_windows_path(v):
    """True for C:\\x, C:/x and \\\\wsl.localhost\\<distro>\\x (or \\\\wsl$\\...)."""
    return bool(DRIVE.match(v) or WSL_UNC.match(v))


def to_linux(path, root=None):
    """A path as the WSL sees it: C:\\x\\y or C:/x/y -> /mnt/c/x/y,
    \\\\wsl.localhost\\Ubuntu\\home\\u -> /home/u. A Linux path (one starting
    with a single /) is returned as it is; a relative one is made absolute on
    the Windows side first. Any other path (a network share) the WSL cannot
    reach by this rule: ValueError."""
    if path.startswith('/') and not path.startswith('//'):
        return path
    p = path if is_windows_path(path) else os.path.abspath(path)
    m = WSL_UNC.match(p)
    if m:
        return '/' + (m[1] or '').replace('\\', '/')
    m = DRIVE.match(p)
    if m:
        return (root or mount_root()) + m[1].lower() + '/' + m[2].replace('\\', '/')
    raise ValueError(f'{path}: the WSL reaches no such path (a drive letter or \\\\wsl.localhost\\<distro> is needed)')


def wsl_distro():
    """The WSL's default distribution's name, for a path inside it seen from Windows."""
    r = subprocess.run(['wsl', '-e', 'sh', '-c', 'printf %s "$WSL_DISTRO_NAME"'], capture_output=True, text=True)
    name = r.stdout.strip()
    if not name:
        sys.exit('the WSL gave no distribution name: name the ELF by a Windows path')
    return name


def local_path(path):
    """A path as this side reads it: as given (made absolute), or - on Windows,
    for a path inside the WSL (/home/...) - through \\\\wsl.localhost\\<distro>."""
    if ON_WINDOWS and path.startswith('/') and not path.startswith('//'):
        return '\\\\wsl.localhost\\' + wsl_distro() + path.replace('/', '\\')
    return os.path.abspath(path)


def log_of(elf):
    """The Linux build's log as this side reads it: the ELF's path with the
    extension .log (src/hook/log.cpp LogOpen)."""
    return os.path.splitext(local_path(elf))[0] + '.log'


def passed_env(env, keys=(), root=None):
    """The variables that cross into the WSL (PASSED_PREFIXES, and `keys`),
    each Windows path among the values translated."""
    out = {}
    for k, v in env.items():
        if k.upper().startswith(PASSED_PREFIXES) or k in keys:
            if '=' in k or not k:
                continue
            out[k] = to_linux(v, root) if is_windows_path(v) else v
    return out


class LinuxGame:
    """One run of the Linux build. `env` is the environment the game gets on
    Linux (whole) or the one it is chosen from through the WSL (passed_env);
    `output` names the file its stdout and stderr go to (the pid line first,
    through the WSL); `keys` are names outside PASSED_PREFIXES that cross too.
    Acts on its own process only, by pid - never on another by name.

    `launcher`: bof3x-launcher beside the ELF, which starts the game - the
    run then has the launcher's layer (its defaults with no ini, BOF3.CFG,
    the language and preset lines, BOF3X_LAUNCHER) as a player's start and a
    Windows reference run have it. The Linux launcher returns as soon as the
    game has started and prints its pid (`bof3x-launcher: started ... (pid
    N)`); the game is then followed by that pid alone. Its exit code is not
    the harness's to read (the game is not its child): exit_code() is None.

    `headless` (the default): HEADLESS_SET unless the caller set the name,
    and no DISPLAY or WAYLAND_DISPLAY in the game's environment, so no
    window and no message box can reach a screen."""

    def __init__(self, elf, game, env, output, args=(), keys=(), launcher=None, headless=True):
        self.wsl = ON_WINDOWS
        self.elf = elf if (self.wsl and elf.startswith('/')) else os.path.abspath(elf)
        self.game = game if (self.wsl and game.startswith('/')) else os.path.abspath(game)
        self.env, self.output = dict(env), output
        if launcher and not (self.wsl and launcher.startswith('/')):
            launcher = os.path.abspath(launcher)
        self.args, self.keys, self.launcher, self.headless = list(args), tuple(keys), launcher, headless
        if headless:
            for k, v in HEADLESS_SET.items():
                self.env.setdefault(k, v)
            for k in HEADLESS_UNSET:
                self.env.pop(k, None)
        self.proc, self.pid, self.ended_by_us, self._srv = None, None, False, None

    def command(self):
        if self.launcher:
            folder = self.launcher.rsplit('/', 1)[0] if self.launcher.startswith('/') else os.path.dirname(self.launcher)
            tail = [self.launcher if not self.wsl else to_linux(self.launcher), '--game',
                    to_linux(self.game) if self.wsl else self.game, '--no-config'] + self.args
            if not self.wsl:
                return tail
            env = passed_env(self.env, self.keys)
            return (['wsl', '-e', 'sh', '-c', SH_HEADLESS if self.headless else SH, 'linux_run', to_linux(folder)] +
                    [f'{k}={v}' for k, v in sorted(env.items())] + tail)
        if not self.wsl:
            return [self.elf] + self.args
        env = passed_env(self.env, self.keys)
        return (['wsl', '-e', 'sh', '-c', SH_HEADLESS if self.headless else SH, 'linux_run', to_linux(self.game)] +
                [f'{k}={v}' for k, v in sorted(env.items())] + [to_linux(self.elf)] + self.args)

    def start(self):
        if not (self.wsl and self.elf.startswith('/')) and not os.path.isfile(self.elf):
            sys.exit(f'no Linux build at {self.elf} (--elf)')
        if not (self.wsl and self.game.startswith('/')) and not os.path.isdir(self.game):
            sys.exit(f'no game folder {self.game}')
        if self.launcher and not (self.wsl and self.launcher.startswith('/')) and not os.path.isfile(self.launcher):
            sys.exit(f'no launcher at {self.launcher} (--launcher)')
        cmd = self.command()
        if self.wsl:
            self._helper()   # up before the first frozen shot asks for it
        with open(self.output, 'wb') as out:
            if self.wsl:
                self.proc = subprocess.Popen(cmd, stdin=subprocess.DEVNULL, stdout=out, stderr=subprocess.STDOUT)
            else:
                cwd = os.path.dirname(self.launcher) if self.launcher else self.game
                self.proc = subprocess.Popen(cmd, cwd=cwd, env=self.env, stdin=subprocess.DEVNULL,
                                             stdout=out, stderr=subprocess.STDOUT)
        if self.launcher:
            return self._launched()
        if not self.wsl:
            self.pid = self.proc.pid
            return self.pid
        t0 = time.time()
        while time.time() - t0 < 120:
            try:
                with open(self.output, 'rb') as f:
                    m = PID_LINE.search(f.read(4096).decode('ascii', 'replace'))
            except OSError:
                m = None
            if m:
                self.pid = int(m[1])
                return self.pid
            if self.proc.poll() is not None:
                break
            time.sleep(0.05)
        code = self.proc.poll()
        sys.exit(f'the WSL did not start the game (exit {code}; output in {self.output})' if code is not None else
                 f'the WSL gave no pid line in 120 s (output in {self.output})')

    def _launched(self, seconds=600):
        """The launcher's run: wait for it to return (it does once the game is
        up), then the game's pid from its line. A launcher that stops without
        the line - its own refusal, a self-test it waited for - ends the run."""
        try:
            code = self.proc.wait(seconds)
        except subprocess.TimeoutExpired:
            sys.exit(f'the launcher did not return in {seconds} s (output in {self.output})')
        with open(self.output, 'rb') as f:
            m = LAUNCHED.search(f.read(1 << 20).decode('utf-8', 'replace'))
        if not m:
            sys.exit(f'the launcher started no game (exit {code}; output in {self.output})')
        self.pid = int(m[1])
        self.launcher_code = code
        return self.pid

    def alive(self):
        """While the game runs: through the WSL, wsl.exe lives as long as the
        process it started; a game the launcher started, by its pid."""
        if self.launcher:
            return self.pid is not None and bool(self._ask('alive', self.pid))
        return self.proc is not None and self.proc.poll() is None

    def exit_code(self):
        if self.launcher:
            return None
        return None if self.proc is None else self.proc.poll()

    def wait(self, seconds):
        """Wait up to `seconds` for the game to exit by itself. Its exit code,
        or None (always None for a game the launcher started)."""
        if self.launcher:
            t0 = time.time()
            while self.alive() and time.time() - t0 < seconds:
                time.sleep(0.2)
            return None
        try:
            return self.proc.wait(seconds)
        except subprocess.TimeoutExpired:
            return None

    def _ask(self, op, *args):
        """One operation inside Linux (OPS): here on Linux; from Windows by
        the helper this file starts in the WSL once per game (`serve`) - a
        `wsl -e` costs one to three seconds, more than a frozen shot waits.
        The reply, or None when the helper is gone."""
        if not self.wsl:
            return OPS[op](*args)
        self._helper()
        try:
            self._srv.stdin.write(json.dumps([op] + list(args)) + '\n')
            self._srv.stdin.flush()
            line = self._srv.stdout.readline()
        except OSError:
            return None
        return json.loads(line) if line.strip() else None

    def _helper(self):
        """Start the helper in the WSL, if it is not up; returns at once (it
        answers its first request when its Python has started)."""
        if self._srv is None or self._srv.poll() is not None:
            self._srv = subprocess.Popen(['wsl', '-e', 'python3', to_linux(os.path.abspath(__file__)), 'serve'],
                                         stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                         text=True, encoding='utf-8', bufsize=1)

    def close(self):
        """Let the helper in the WSL go (it ends on its stdin's end)."""
        if self._srv is not None:
            try:
                self._srv.stdin.close()
                self._srv.wait(10)
            except (OSError, subprocess.TimeoutExpired):
                self._srv.kill()
            self._srv = None

    def release(self, name):
        """sem_post /NAME (a frozen shot's release). True when it was posted."""
        return bool(self._ask('post', name))

    def unlink(self, name):
        """sem_unlink /NAME: a game that was ended does not remove its own."""
        return bool(self._ask('unlink', name))

    def end(self, wait=10):
        """End the game, if it still runs: SIGKILL by its pid, the counterpart
        of the Windows runners' taskkill /F; through the WSL, wsl.exe is ended
        too if the process will not go."""
        if not self.alive():
            return
        self.ended_by_us = True
        if self.launcher:
            self._ask('kill', self.pid)
            t0 = time.time()
            while self.alive() and time.time() - t0 < wait:
                time.sleep(0.1)
            return
        if self.wsl:
            self._ask('kill', self.pid)
        else:
            self.proc.kill()
        try:
            self.proc.wait(wait)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(5)

    def busy(self):
        """The pids already running this ELF (a run that died and left its game)."""
        return self._ask('busy', to_linux(self.elf) if self.wsl else self.elf) or []

    def end_stale(self):
        """End a leftover game whose executable is this ELF - a crashed
        earlier run's - and no other, by pid (attract_run.kill_stale's
        counterpart). The pids ended."""
        pids = self.busy()
        for pid in pids:
            print(f'  killing stale game {pid} ({self.elf})')
            self._ask('kill', pid)
        return pids


# --- inside Linux ---------------------------------------------------------------

def kill(pid):
    """SIGKILL to pid. True when it was sent."""
    try:
        os.kill(int(pid), signal.SIGKILL)
        return True
    except OSError:
        return False


def alive(pid):
    """True while the process runs: it exists and is not a zombie (a game
    whose launcher has gone is reaped by init; one still a child of ours
    would wait as a zombie)."""
    try:
        with open(f'/proc/{int(pid)}/stat', 'rb') as f:
            stat = f.read().decode('ascii', 'replace')
    except OSError:
        return False
    return stat[stat.rfind(')') + 2:][:1] not in ('Z', 'X', '')


# The BOF3.CFG of a state-hash run (docs/state-hash.md 6, 2026-11-24): line 1
# windowed (0), line 2 renderer 1, no key lines (the game's own table). With
# no file Cfg_Load sets Cfg_Fullscreen 1, and fullscreen takes the scale k
# from the desktop's size; windowed, BOF3X_SCALE pins it, so the render size,
# D3d_ScaleX/Y and the back buffer's pitch are the same on every machine.
RUN_CFG = b'0\r\n1\r\n'


def make_run_game(src, dst, save=None, only_slot0=False):
    """A game folder for one run, made from the install `src`, which is only
    read: every folder and every file the game only reads a symbolic link
    into `src`; the files a run writes real copies - the saves BISLPS0?.DAT
    (a route that saves writes one; slot 0 is `save` when given, the recipe
    save, else src's) and BOF3.CFG (the launcher writes its display lines).
    `only_slot0`: none of src's slots but slot 0 and not src's BOF3.CFG -
    slot 0 alone (the recipe save, or src's) and RUN_CFG - so a hashed run's
    inputs are the recipe save, the install's data and RUN_CFG, whatever the
    install holds: the load menu reads every slot's header into .data (the
    desktop's orig5 runs carry the owner's slots), and BOF3.CFG decides
    Cfg_Fullscreen (bridgeWalk, laptop against desktop, 2026-11-24: page
    0x65D000 on every tick while one side had a launcher-written file and
    the other none). Returns (links, copies)."""
    import shutil
    src, dst = os.path.abspath(src), os.path.abspath(dst)
    if not os.path.isdir(src):
        raise ValueError(f'no install {src}')
    if os.path.lexists(dst):
        if not os.path.isfile(os.path.join(dst, RUN_GAME_MARK)):
            raise ValueError(f'{dst} exists and is not a run folder of the harness: not removed')
        shutil.rmtree(dst)
    os.makedirs(dst)
    open(os.path.join(dst, RUN_GAME_MARK), 'w').close()
    links, copies = [], []
    for name in sorted(os.listdir(src)):
        up = name.upper()
        at, to = os.path.join(src, name), os.path.join(dst, name)
        if name == '.recipe_slot0' or up.startswith('BOF3X_') and up.endswith('.LOG'):
            continue
        if re.fullmatch(r'BISLPS1[0-9A-F]\.(DAT|TMP)|.*\.TMP', up):
            continue
        if re.fullmatch(r'BISLPS0[0-9A-F]\.DAT', up) or up == 'BOF3.CFG':
            if up == 'BISLPS00.DAT' and save:
                continue
            if only_slot0 and up != 'BISLPS00.DAT':
                continue
            shutil.copyfile(at, to)
            os.chmod(to, 0o644)
            copies.append(name)
            continue
        os.symlink(at, to)
        links.append(name)
    if save:
        to = os.path.join(dst, 'BISLPS00.DAT')
        shutil.copyfile(save, to)
        os.chmod(to, 0o644)
        copies.append('BISLPS00.DAT (' + os.path.basename(save) + ')')
    if only_slot0:
        with open(os.path.join(dst, 'BOF3.CFG'), 'wb') as f:
            f.write(RUN_CFG)
        copies.append('BOF3.CFG (RUN_CFG: windowed, renderer 1)')
    return links, copies


def serve():
    """The helper's loop: one JSON list [op, args...] a line on stdin, one
    JSON reply a line on stdout, until stdin ends."""
    for line in sys.stdin:
        try:
            op, *args = json.loads(line)
            reply = OPS[op](*args)
        except Exception as e:   # a reply for every request: the other side waits on it
            reply = None
            print(f'linux_run serve: {e}', file=sys.stderr)
        sys.stdout.write(json.dumps(reply) + '\n')
        sys.stdout.flush()

def _libc():
    import ctypes, ctypes.util
    names = [ctypes.util.find_library('c') or 'libc.so.6', ctypes.util.find_library('pthread') or 'libpthread.so.0']
    for n in names:
        try:
            lib = ctypes.CDLL(n, use_errno=True)
            lib.sem_open
        except (OSError, AttributeError):
            continue
        lib.sem_open.restype = ctypes.c_void_p
        lib.sem_open.argtypes = [ctypes.c_char_p, ctypes.c_int]
        lib.sem_post.argtypes = [ctypes.c_void_p]
        lib.sem_close.argtypes = [ctypes.c_void_p]
        lib.sem_unlink.argtypes = [ctypes.c_char_p]
        return lib
    sys.exit('no C library with sem_open: this command runs on Linux only')


def _sem_name(name):
    return ('/' + name.lstrip('/')).encode()


def sem_post(name):
    """Post the named semaphore /NAME, which must exist. True when posted."""
    lib = _libc()
    s = lib.sem_open(_sem_name(name), 0)
    if not s:
        return False
    ok = lib.sem_post(s) == 0
    lib.sem_close(s)
    return ok


def sem_unlink(name):
    """Remove the named semaphore /NAME. True when there was one."""
    return _libc().sem_unlink(_sem_name(name)) == 0


def pids_of(elf):
    """The pids whose /proc/<pid>/exe is `elf`."""
    want = os.path.realpath(elf)
    out = []
    for d in os.listdir('/proc'):
        if d.isdigit():
            try:
                if os.path.realpath(os.readlink(f'/proc/{d}/exe')) == want:
                    out.append(int(d))
            except OSError:
                pass
    return sorted(out)


def selftest():
    """The path rules on made-up names; no process is started. Exit 1 on a failure."""
    cases = [('C:\\Users\\x\\a b\\f.txt', '/mnt/c/Users/x/a b/f.txt'),
             ('C:/Users/x/run é/t', '/mnt/c/Users/x/run é/t'),
             ('d:\\', '/mnt/d/'),
             ('\\\\wsl.localhost\\Ubuntu\\home\\u\\bof3x', '/home/u/bof3x'),
             ('//wsl.localhost/Ubuntu/home/u', '/home/u'),
             ('\\\\wsl$\\Ubuntu', '/'),
             ('/home/u/bof3x', '/home/u/bof3x')]
    bad = 0
    for given, want in cases:
        got = to_linux(given, root='/mnt/')
        if got != want:
            print(f'FAIL to_linux({given!r}) = {got!r}, want {want!r}')
            bad += 1
    try:
        to_linux('\\\\server\\share\\x', root='/mnt/')
        print('FAIL a network share was translated')
        bad += 1
    except ValueError:
        pass
    env = {'BOF3X_INPUT': 'C:\\r\\x.txt', 'BOF3X_OFF': 'A,B', 'PATH': 'C:\\Windows', 'SDL_VIDEODRIVER': 'x11',
           'OTHER': 'C:/o', 'BOF3X_LANG': 'en-US'}
    got = passed_env(env, keys=('OTHER',), root='/mnt/')
    want = {'BOF3X_INPUT': '/mnt/c/r/x.txt', 'BOF3X_OFF': 'A,B', 'SDL_VIDEODRIVER': 'x11', 'OTHER': '/mnt/c/o',
            'BOF3X_LANG': 'en-US'}
    if got != want:
        print(f'FAIL passed_env: {got}')
        bad += 1
    print(f'selftest: {"FAILED" if bad else "passed"} ({len(cases) + 2} cases)')
    return 1 if bad else 0


OPS = {'post': sem_post, 'unlink': sem_unlink, 'busy': pids_of, 'kill': kill, 'alive': alive,
       'mkgame': lambda src, dst, save=None, only_slot0=False: make_run_game(src, dst, save, only_slot0)}


def main():
    if len(sys.argv) == 2 and sys.argv[1] == 'selftest':
        sys.exit(selftest())
    if len(sys.argv) >= 2 and sys.argv[1] in ('serve',) + tuple(OPS):
        if not sys.platform.startswith('linux'):
            sys.exit(f'linux_run.py {sys.argv[1]} runs inside Linux; on Windows the harness runs it through wsl -e.')
        if sys.argv[1] == 'serve' and len(sys.argv) == 2:
            return serve()
        if len(sys.argv) == 3:
            reply = OPS[sys.argv[1]](sys.argv[2])
            if isinstance(reply, list):
                print('\n'.join(map(str, reply)))
                return
            sys.exit(0 if reply else 1)
    sys.exit(__doc__)


if __name__ == '__main__':
    main()
