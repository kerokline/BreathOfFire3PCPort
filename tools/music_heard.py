#!/usr/bin/env python
"""Did the music play? The ear beside the state hash: a run's music log read
for a track whose heard sample stops moving while its buffer says it plays.

    python tools/music_heard.py <bof3x.log> [--min-frames 120] [--quiet]
    python tools/music_heard.py --selftest

The state hash does not see the music ring (its words are in the skip list),
and only one route waits on a stream's end. The 64-bit round found that way
that a stream fed by Music_Pump could stop after its first quarter second and
no check would say so (docs/x64-build-round.md section 5). Under
BOF3X_MUSIC_LOG=1 the DLL writes, once in every 60 logic frames while a track's
stream is live, a ring line (src/game/sound.cpp RingLog, from Sound_Tick):

    music_log   f<frame> ring track <t> heard <sample|-> written <samples> <playing|halted>

`heard` is the stream sample under the play cursor, `-` when the cursor sits
in a half the pump has not written; `written` is what Music_Pump has fed the
ring since Music_Start. On a playing stream `heard` moves between any two
lines. The defect's shape is the opposite: `playing` with `heard` and
`written` frozen from the first lines on.

A run: a route with the switch set, then this tool on the launcher copy's log.

    python tools/input_run.py <route>.txt --launcher <copy>/bof3x-launcher.exe --env BOF3X_MUSIC_LOG=1 ...
    python tools/music_heard.py <copy>/bof3x.log

The report lists each track the ring lines saw: its lines and the frames they
span, the first and last heard sample, and whether heard advanced. The other
music_log lines (Music_Play, Music_Start, Music_Stop, the fades, Music_Halt
and Music_Resume, the resume's and the jingle's lines) are read as the events
between ring lines: a Music_Start resets the ring's count, so a track's
samples are followed from one start to the next, and a stop or halt ends the
stretch being watched. A `halted` line is a buffer that does not play (a
pause, F9, a fade that stopped it, the night's halt): never a stall.

Exit 1 when some track is `playing` on consecutive ring lines spanning at
least --min-frames frames with no advance in heard (the `-` lines count as no
advance, so a ring never written shows too); 0 otherwise; 2 when the log has
no ring lines at all - it says whether the switch was off (no music_log line
of any kind) or on with no track stream live.
"""
import argparse
import io
import re
import sys

RING = re.compile(r'music_log\s+f(\d+) ring track (\d+) heard (\d+|-) written (\d+) (playing|halted)\s*$')
MUSIC = re.compile(r'music_log\s+f(\d+) (.*)$')
# A line that starts a track's stream over (the ring's count returns to 0) or ends it.
RESTART = re.compile(r'^(Music_Start\(|Music_Play\(|resume track )')
END = re.compile(r'^(Music_Stop\b|Music_Halt\b|StopCuts:|DIV-0169 nightfall: track \d+ halted|DIV-0177 jingle: track \d+ taken)')


def parse(lines):
    """Every music_log line in order: ('ring', frame, track, heard, written, playing) or ('event', frame, text)."""
    out = []
    for line in lines:
        line = line.rstrip('\r\n')
        m = RING.search(line)
        if m:
            heard = None if m.group(3) == '-' else int(m.group(3))
            out.append(('ring', int(m.group(1)), int(m.group(2)), heard, int(m.group(4)), m.group(5) == 'playing'))
            continue
        m = MUSIC.search(line)
        if m:
            out.append(('event', int(m.group(1)), m.group(2)))
    return out


def analyse(items, min_frames):
    """Per track: lines, first/last frame, first/last heard, advanced; and the stalls found."""
    tracks = {}
    stalls = []
    run = None  # the stretch under watch: [track, first_frame, last_frame, heard, lines]
    for item in items:
        if item[0] == 'event':
            text = item[2]
            if RESTART.match(text) or END.match(text):
                run = None
            continue
        _, frame, track, heard, written, playing = item
        t = tracks.setdefault(track, {'lines': 0, 'first_frame': frame, 'last_frame': frame, 'first_heard': heard,
                                      'last_heard': heard, 'advanced': False, 'playing_lines': 0, 'seen': []})
        t['lines'] += 1
        t['last_frame'] = frame
        t['last_heard'] = heard
        if t['first_heard'] is None and heard is not None:
            t['first_heard'] = heard
        t['playing_lines'] += playing
        if heard is not None and t['seen'] and heard != t['seen'][-1]:
            t['advanced'] = True
        if heard is not None:
            t['seen'].append(heard)
        if not playing:
            run = None
            continue
        if run is not None and run[0] == track and run[3] == heard:
            run[2] = frame
            run[4] += 1
            if frame - run[1] >= min_frames and not run[5]:
                run[5] = True
                stalls.append({'track': track, 'from': run[1], 'to': frame, 'heard': heard, 'written': written, 'lines': run[4]})
            elif run[5]:
                stalls[-1]['to'] = frame
                stalls[-1]['lines'] = run[4]
        else:
            run = [track, frame, frame, heard, 1, False]
    return tracks, stalls


def report(items, tracks, stalls, out):
    for track in sorted(tracks):
        t = tracks[track]
        fh = '-' if t['first_heard'] is None else t['first_heard']
        lh = '-' if t['last_heard'] is None else t['last_heard']
        out.write('track %u: %u ring lines over frames %u..%u (%u playing), heard %s -> %s, %s\n'
                  % (track, t['lines'], t['first_frame'], t['last_frame'], t['playing_lines'], fh, lh,
                     'advanced' if t['advanced'] else 'did not advance'))
    for s in stalls:
        h = '-' if s['heard'] is None else s['heard']
        out.write('STALL: track %u playing with heard %s (written %u) on %u ring lines, frames %u..%u\n'
                  % (s['track'], h, s['written'], s['lines'], s['from'], s['to']))


def run(items, min_frames, out, quiet=False):
    rings = sum(1 for i in items if i[0] == 'ring')
    if rings == 0:
        if items:
            out.write('no ring lines: the music log was on (%u music_log lines) but no track stream was live - '
                      'no music played, or the build has no ring line\n' % len(items))
        else:
            out.write('no ring lines and no music_log line at all: the run did not set BOF3X_MUSIC_LOG=1\n')
        return 2
    tracks, stalls = analyse(items, min_frames)
    if not quiet or stalls:
        report(items, tracks, stalls, out)
    if stalls:
        out.write('FAIL: %u stall(s) - a track reported playing for %u+ frames with no advance in heard\n'
                  % (len(stalls), min_frames))
        return 1
    out.write('pass: %u ring lines, %u track(s), heard advanced on every playing stretch\n' % (rings, len(tracks)))
    return 0


# --- the self-test -------------------------------------------------------------

def _lines(track, frames, heard, written, playing=True):
    return ['music_log   f%u ring track %u heard %s written %u %s'
            % (f, track, '-' if h is None else h, w, 'playing' if playing else 'halted')
            for f, h, w in zip(frames, heard, written)]


def selftest():
    half = 9216
    fails = 0

    def check(name, log, want_code, want_text=None):
        nonlocal fails
        out = io.StringIO()
        code = run(parse(log), 120, out)
        ok = code == want_code and (want_text is None or want_text in out.getvalue())
        print('%s %s: exit %u%s' % ('ok  ' if ok else 'FAIL', name, code, '' if ok else ' (wanted %u)' % want_code))
        if not ok:
            print(out.getvalue())
            fails += 1

    # A passing run: a track starts, heard climbs ~2,940 samples a frame.
    frames = list(range(120, 1200, 60))
    log = ['music_log   f118 Music_Play(5, 0) track 255 loaded 5 from 0x4F0000',
           'music_log   f118 Music_Start(0x8A0000, 1200000, 1) track 5 from 0x4F0100']
    log += _lines(5, frames, [2940 * (f - 118) for f in frames], [half * (2 + (2940 * (f - 118)) // half) for f in frames])
    check('a playing track', log, 0, 'advanced')

    # The defect's shape: heard 11,520 and written 18,432 from the first line on.
    log = ['music_log   f118 Music_Start(0x8A0000, 1200000, 1) track 5 from 0x4F0100']
    log += _lines(5, frames, [11520] * len(frames), [18432] * len(frames))
    check("the 64-bit defect's frozen ring", log, 1, 'STALL: track 5 playing with heard 11520 (written 18432)')

    # A ring never written: heard `-` for the whole run.
    log = _lines(7, frames, [None] * len(frames), [0] * len(frames))
    check('a ring the pump never wrote', log, 1, 'heard - (written 0)')

    # A halt: the track plays, F9 halts it, the lines say halted, then it plays on.
    play = list(range(120, 420, 60))
    log = _lines(5, play, [2940 * (f - 118) for f in play], [half * 4] * len(play))
    log += ['music_log   f430 Music_Halt track 5']
    halted = list(range(480, 1200, 60))
    log += _lines(5, halted, [2940 * 302] * len(halted), [half * 4] * len(halted), playing=False)
    log += ['music_log   f1210 Music_Resume track 5']
    again = list(range(1260, 1500, 60))
    log += _lines(5, again, [2940 * 302 + 2940 * (f - 1258) for f in again], [half * 6] * len(again))
    check('a halt is not a stall', log, 0, 'pass')

    # A short freeze under the floor passes: two lines alike spanning 60 frames.
    log = _lines(5, [120, 180, 240, 300], [1000, 1000, 5000, 9000], [half * 4] * 4)
    check('a freeze under the floor', log, 0, 'pass')

    # A restart between two equal lines is not a stall: Music_Start resets the count.
    log = _lines(5, [120, 180], [5000, 5000], [half * 4] * 2)
    log += ['music_log   f200 Music_Start(0x8A0000, 1200000, 1) track 5 from 0x4F0100']
    log += _lines(5, [240, 300], [5000, 5000], [half * 2] * 2)
    check('a restart between equal lines', log, 0, 'pass')

    # No ring lines: the switch on with nothing live, and the switch off.
    check('the switch on, no stream', ['music_log   f118 Music_Stop track 255 from 0x4F0000'], 2, 'no track stream was live')
    check('the switch off', ['launcher    hello', 'DIV-0028    music fades step once per logic frame'], 2, 'did not set BOF3X_MUSIC_LOG=1')

    print('music_heard selftest: %s' % ('%u FAILED' % fails if fails else 'all passed'))
    return 1 if fails else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    ap.add_argument('log', nargs='?', help="the launcher copy's bof3x.log")
    ap.add_argument('--min-frames', type=int, default=120,
                    help='frames of playing lines with no advance that make a stall (default 120)')
    ap.add_argument('--quiet', action='store_true', help='the verdict only, unless a stall')
    ap.add_argument('--selftest', action='store_true', help='synthetic logs checked in-process')
    args = ap.parse_args(argv)
    if args.selftest:
        return selftest()
    if not args.log:
        ap.error('a log, or --selftest')
    with open(args.log, encoding='utf-8', errors='replace') as fh:
        items = parse(fh)
    return run(items, args.min_frames, sys.stdout, args.quiet)


if __name__ == '__main__':
    sys.exit(main())
