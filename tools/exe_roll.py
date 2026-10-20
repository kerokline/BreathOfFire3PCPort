"""The staff roll a Western PSX disc carries: base/exe/roll.bin (DIV-0148, D323).

The ending's roll is Scena17_RollLines (0x661A20: one string pointer a line, 0 a blank
line, -1 the end) and the strings after Scena17_EndSteps. The PC's equal the Japan disc's
SCENARIO/SCENA17.EMI#0. The US and PAL discs re-lay both (other job titles, other lines,
345 / 293 lines against 351), so the image's map placed stretches of their text under
the PC's pointers, a patchwork (D323). A pointer word of the image must hold the PC's
value (exe_tables.apply_layout), so the release's own roll does not go into .data:
the importer writes it beside data.bin and the engine's Scena17_ScrollRoll reads its
lines from there (src/game/scena_sc15.cpp, src/hook/exe_image.h).

roll.bin, little-endian:
    8   magic "BOF3XRL2"
    4   lines (n), the -1 not counted
    4   text bytes (t)
    4n  per line 0 (blank) or 1 + the line's offset into the text
    56  the 14 words after the -1, the same way: Scena17_ScrollRoll draws the 15 entries after
        its top line, so once the -1 is at the top it draws these - the end card ("Presented
        by" and the logo glyph) is the fifth on every disc and on the PC. The 15th is the
        section's next word (a code address on every disc), drawn 330 pixels down, below the
        screen: not carried, the engine reads 0
    t   the text: the disc's strings, NUL-terminated, as the disc holds them

The roll is found in the disc's SCENA17.EMI#0 as the longest run of words that are 0 or
an address inside the section and that ends in 0xFFFFFFFF; every non-null word must name
a NUL-terminated string inside the section. Addresses and counts only (CLAUDE.md rule 1).
"""

import os
import struct

ROLL_FILE, ROLL_SECTION = "SCENARIO/SCENA17.EMI", 0
ROLL_BUILDS = ("psx-us", "psx-eu-en", "psx-fr", "psx-de")   # the releases whose roll is not the PC's
MAGIC = b"BOF3XRL2"
AFTER = 14                                                  # the words after the -1 carried
MIN_LINES, MAX_LINES, MAX_TEXT = 100, 1024, 0x4000          # the engine refuses outside these too
END = 0xFFFFFFFF


def fail(msg):
    raise SystemExit("exe_roll: " + msg)


def find_roll(base, blob):
    """(line words, the AFTER words after the -1, text start, text end) of the
    section's roll, as disc addresses."""
    best = None
    o = 0
    while o + 4 <= len(blob):
        k, n, nz = o, 0, 0
        while k + 4 <= len(blob):
            v, = struct.unpack_from("<I", blob, k)
            if v != 0 and not base <= v < base + len(blob):
                break
            n, nz, k = n + 1, nz + (v != 0), k + 4
        if nz >= MIN_LINES and k + 4 <= len(blob) and struct.unpack_from("<I", blob, k)[0] == END:
            if best is None or n > len(best[0]):
                best = ([struct.unpack_from("<I", blob, o + 4 * i)[0] for i in range(n)], k + 4)
        o = k + 4 if k > o else o + 4
    if best is None:
        fail("%s#%d holds no roll (a run of string words ending in -1)" % (ROLL_FILE, ROLL_SECTION))
    best, after_at = best
    if len(best) > MAX_LINES:
        fail("the roll has %d lines, more than %d" % (len(best), MAX_LINES))
    if after_at + 4 * AFTER > len(blob):
        fail("the roll's -1 is too near the section's end for the %d words after it" % AFTER)
    after = [struct.unpack_from("<I", blob, after_at + 4 * i)[0] for i in range(AFTER)]
    for v in after:
        if v != 0 and not base <= v < base + len(blob):
            fail("a word after the roll's -1, 0x%08X, is neither 0 nor in the section" % v)
    starts = sorted({v for v in best + after if v})
    lo = starts[0]
    end = blob.find(b"\0", starts[-1] - base)
    if end < 0:
        fail("the roll's last line runs past the section")
    hi = base + end + 1
    for v in starts:
        if blob.find(b"\0", v - base, hi - base) < 0:
            fail("line 0x%08X has no NUL before the text's end" % v)
    if hi - lo > MAX_TEXT:
        fail("the roll's text is 0x%X bytes, more than 0x%X" % (hi - lo, MAX_TEXT))
    return best, after, lo, hi


def roll_bytes(sources):
    """roll.bin's bytes from exe_twins.sources-shaped [(file, section, base, bytes)]."""
    blobs = {(f, s): (b, blob) for f, s, b, blob in sources}
    if (ROLL_FILE, ROLL_SECTION) not in blobs:
        fail("the disc has no %s#%d" % (ROLL_FILE, ROLL_SECTION))
    base, blob = blobs[(ROLL_FILE, ROLL_SECTION)]
    lines, after, lo, hi = find_roll(base, blob)
    text = blob[lo - base:hi - base]
    words = [0 if v == 0 else 1 + v - lo for v in lines + after]
    return MAGIC + struct.pack("<II", len(lines), len(text)) + struct.pack("<%dI" % len(words), *words) + text


def parse(raw):
    """(lines and the words after the -1, text) from roll.bin's bytes, checked as the
    engine checks them; raises."""
    if raw[:8] != MAGIC or len(raw) < 16:
        fail("roll.bin: not a roll (magic)")
    n, t = struct.unpack_from("<II", raw, 8)
    if not MIN_LINES <= n <= MAX_LINES or t > MAX_TEXT or len(raw) != 16 + 4 * (n + AFTER) + t:
        fail("roll.bin: %d lines, %d text bytes, %d bytes in the file" % (n, t, len(raw)))
    words = struct.unpack_from("<%dI" % (n + AFTER), raw, 16)
    text = raw[16 + 4 * (n + AFTER):]
    for w in words:
        if w and (w > t or text.find(b"\0", w - 1) < 0):
            fail("roll.bin: a line at text offset %d without a NUL after it" % (w - 1))
    return words, text


def write(disc, bid, out):
    """base/exe/roll.bin for a Western PSX build (ROLL_BUILDS); none for another.
    Returns the file's path or None."""
    path = os.path.join(out, "base", "exe", "roll.bin")
    if bid not in ROLL_BUILDS:
        if os.path.exists(path):
            os.remove(path)                      # a cache rebuilt from another release keeps no stale roll
        return None
    import exe_twins
    raw = roll_bytes(exe_twins.sources(disc))
    parse(raw)
    with open(path, "wb") as f:
        f.write(raw)
    return path


def selftest():
    """A synthetic section (no game data): two lines, a blank, the end word. Problems as a list."""
    errs = []
    base = 0x801F0000
    text = b"AB\0" + b"CDE\0"
    words = [base + 0x10, 0, base + 0x13] + [0] * (MIN_LINES - 2) + [base + 0x10] * (MIN_LINES)
    sec = bytearray(0x10) + text + bytes((-len(text)) % 4)
    tab = len(sec)
    sec += struct.pack("<%dI" % len(words), *words) + struct.pack("<I", END)
    sec += struct.pack("<%dI" % AFTER, *([0, 0, 0, 0, base + 0x13] + [0] * (AFTER - 5))) + bytes(8)
    try:
        raw = roll_bytes([(ROLL_FILE, ROLL_SECTION, base, bytes(sec))])
        got, txt = parse(raw)
        if got[:3] != (1, 0, 4) or txt != text or len(got) != len(words) + AFTER or got[len(words) + 4] != 4:
            errs.append("roll: the synthetic roll read back as %r / %r" % (got[:3], txt))
    except SystemExit as e:
        errs.append("roll: the synthetic roll refused: %s" % e)
    bad = bytearray(sec)
    struct.pack_into("<I", bad, tab, base + 0x7FFF0)              # a word outside the section ends the run early
    try:
        roll_bytes([(ROLL_FILE, ROLL_SECTION, base, bytes(bad))])
        lines, _, _, _ = find_roll(base, bytes(bad))
        if len(lines) == len(words):
            errs.append("roll: a word outside the section was taken as a line")
    except SystemExit:
        pass
    trunc = bytearray(raw)[:-1]
    try:
        parse(bytes(trunc))
        errs.append("roll: a truncated roll.bin parsed")
    except SystemExit:
        pass
    return errs


if __name__ == "__main__":
    e = selftest()
    print("exe_roll selftest: %s" % ("; ".join(e) if e else "OK"))
    raise SystemExit(1 if e else 0)
