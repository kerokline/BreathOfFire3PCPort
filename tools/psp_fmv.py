#!/usr/bin/env python
"""The PSP's logo movie, converted at import: the PSP case of the disc's movie.

docs/psp-install.md section 2. A PSP disc carries the Capcom logo as
`PSP_GAME/USRDIR/<JPN|USA>/PSMF/CAPLOGO.PMF`: a PSMF file, an MPEG program
stream behind a 2,048-byte header, with one H.264 video stream (`0xE0`, 480 x
272, 29.97 fps, 231 frames) and one ATRAC3plus audio stream (private stream 1,
sub-stream `0x00`). The same file on PSP-JP and PSP-EU.

ffmpeg has no PSMF demuxer. Its MPEG-PS demuxer reads the video but not the
audio: it does not map private stream 1's sub-stream 0x00. Its ATRAC3plus
decoder is there. So this module walks the program stream itself, only as far
as the audio: it takes each audio packet's payload past the 4-byte private
header, checks every 8-byte frame header (`0F D0`, then the two codec-parameter
bytes, then four zeros), and lays the frames into the same RIFF
`WAVE_FORMAT_EXTENSIBLE` form the disc's own `.AT3` files use, built by rule
from those two bytes. The player's ffmpeg then reads the video from the program
stream and the audio from that wrapper, and writes `base/fmv/CAPCOM.AVI` in the
form the PSX disc's movie takes (docs/fmv-import.md 3): Cinepak and 16-bit PCM
at 44.1 kHz stereo.

The picture is the PlayStation's 4:3 frame resampled to the PSP's 480 x 272
(the PSX and PSP frames at the same index show the same composition, the logo
at the same fraction of the width and height: docs/psp-install.md 2.2). So the
conversion scales it back to 640 x 480, the PC file's size, with no letterbox.
The scale is not a whole factor (4/3 across, 30/17 down), so it is resampled
(lanczos), not pixel-doubled as the PSX's 320 x 240 is.

The audio starts where its first packet's time stamp puts it against the
video's first frame (55 ms earlier on this disc): the decode is trimmed by
that many samples, or delayed if it started later.

Nothing here is game data: a hash, a rule for the codec header, and stream
ids. The PMF, the wrapper and the movie are written only into the cache or a
temporary file beside it (CLAUDE.md rule 1).

    python tools/psp_fmv.py probe PSP.iso      the streams, measured; no ffmpeg needed
"""
import hashlib
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

PMF_SUFFIX = "/PSMF/CAPLOGO.PMF"
# sha256 of the whole CAPLOGO.PMF, measured 2026-10-12 on PSP-JP and PSP-EU,
# equal (docs/psp-install.md 2.1). A hash, no bytes.
SOURCE_SHA256 = "7cbe25eff335a725faaa9b816f75e61514c095200f12bcba541bb354cab78ad3"
VIDEO_ID, AUDIO_ID, AUDIO_SUB = 0xE0, 0xBD, 0x00
AUDIO_PRIVATE_HEADER = 4        # sub-stream id and three bytes, before the frames
FRAME_SYNC = b"\x0f\xd0"
FRAME_HEADER = 8                # sync, two codec-parameter bytes, four zeros
SAMPLES_PER_FRAME = 2048
PTS_RATE = 90000
# ATRAC3plus's sub-format GUID, {E923AABF-CB58-4471-A119-FFFA01E4CE62}, as
# WAVE_FORMAT_EXTENSIBLE stores it (the first three fields little-endian).
ATRAC3PLUS_GUID = bytes.fromhex("bfaa23e958cb7144a119fffa01e4ce62")
RATES = {0: 32000, 1: 44100, 2: 48000, 3: 88200, 4: 96000}
CHANNELS = {1: 1, 2: 2, 3: 3, 4: 4, 6: 6, 7: 7, 5: 6}     # the channel-config field; 2 = stereo

# The encoding: docs/fmv-import.md 3's, but for the source's size and rate.
VIDEO_FILTER = "scale=640:480:flags=lanczos"
OUT_ARGS = ["-c:v", "cinepak", "-c:a", "pcm_s16le", "-ar", "44100",
            "-fflags", "+bitexact", "-flags:v", "+bitexact", "-f", "avi"]


def find(disc):
    """The PMF's path on a PSP disc (psx_disc.Disc), or None."""
    return next((p for p in disc.files if p.upper().endswith(PMF_SUFFIX)), None)


def _pts(body):
    x = body[3:8]
    return ((x[0] >> 1) & 7) << 30 | x[1] << 22 | (x[2] >> 1) << 15 | x[3] << 7 | x[4] >> 1


def params(p):
    """(sample rate, channels, frame bytes) from the two codec-parameter bytes:
    3 bits rate index, 3 bits channel config, 10 bits frame bytes / 8 - 1."""
    v = struct.unpack(">H", p)[0]
    rate, ch = RATES.get(v >> 13), CHANNELS.get((v >> 10) & 7)
    if rate is None or ch is None:
        raise ValueError("ATRAC3plus parameters %s: not a rate and channel layout this reads" % p.hex())
    return rate, ch, ((v & 0x3FF) + 1) * 8


def demux(b):
    """Walk a PSMF. Returns {"body": the program stream, "video_pts": first
    video PTS, "video_packets", "audio_pts": first audio PTS, "frames": the
    ATRAC3plus frames without their headers, "params": the two bytes}."""
    if b[:4] != b"PSMF":
        raise ValueError("not a PSMF file")
    off, size = struct.unpack_from(">II", b, 8)
    if off + size > len(b):
        raise ValueError("PSMF: the stream runs past the file")
    p, end = off, off + size
    audio, vpts, apts, vpk = bytearray(), [], [], 0
    while p + 4 <= end:
        if b[p:p + 3] != b"\0\0\1":
            raise ValueError("PSMF: no start code at 0x%X" % p)
        sc = b[p + 3]
        if sc == 0xBA:                          # an MPEG-2 pack header
            p += 14 + (b[p + 13] & 7)
            continue
        if sc == 0xB9:                          # program end
            break
        n = struct.unpack_from(">H", b, p + 4)[0]
        body = b[p + 6:p + 6 + n]
        if sc in (VIDEO_ID, AUDIO_ID):
            payload = body[3 + body[2]:]
            pts = _pts(body) if body[1] & 0x80 else None
            if sc == VIDEO_ID:
                vpk += 1
                if pts is not None:
                    vpts.append(pts)
            elif payload[:1] == bytes([AUDIO_SUB]):
                if pts is not None:
                    apts.append(pts)
                audio += payload[AUDIO_PRIVATE_HEADER:]
        p += 6 + n
    if not vpts or not apts:
        raise ValueError("PSMF: %s" % ("no video time stamp" if not vpts else "no ATRAC3plus audio"))
    if audio[:2] != FRAME_SYNC:
        raise ValueError("PSMF: the audio does not open with an ATRAC3plus frame header")
    par = bytes(audio[2:4])
    _, _, fbytes = params(par)
    unit, frames = FRAME_HEADER + fbytes, []
    if len(audio) % unit:
        raise ValueError("PSMF: %d audio bytes are not whole %d-byte frames" % (len(audio), unit))
    for at in range(0, len(audio), unit):
        h = audio[at:at + FRAME_HEADER]
        if h != FRAME_SYNC + par + b"\0\0\0\0":
            raise ValueError("PSMF: audio frame %d's header is %s" % (at // unit, h.hex()))
        frames.append(bytes(audio[at + FRAME_HEADER:at + unit]))
    return {"body": b[off:end], "video_pts": min(vpts), "video_packets": vpk, "audio_pts": min(apts),
            "frames": frames, "params": par}


def at3_wrap(frames, par):
    """The frames as a RIFF WAVE_FORMAT_EXTENSIBLE ATRAC3plus file, the form
    ffmpeg's WAV reader decodes: the header written by rule from the codec
    parameters (version 1, the two bytes, eight zeros), no `fact` chunk."""
    rate, ch, fbytes = params(par)
    # samples a block (in the valid-bits field, as Sony's files have it), the
    # channel mask, the sub-format, then version 1, the parameters, eight zeros
    ext = struct.pack("<HI", SAMPLES_PER_FRAME, (1 << ch) - 1) + ATRAC3PLUS_GUID + struct.pack("<H", 1) + par + bytes(8)
    fmt = struct.pack("<HHIIHHH", 0xFFFE, ch, rate, (fbytes * rate + SAMPLES_PER_FRAME // 2) // SAMPLES_PER_FRAME,
                      fbytes, 0, len(ext)) + ext
    data = b"".join(frames)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"data" + struct.pack("<I", len(data)) + data
    return b"RIFF" + struct.pack("<I", len(body)) + body


def audio_filter(d, rate):
    """Where the audio starts against the first frame: a trim of the samples
    before it, or a delay when it starts after."""
    shift = round((d["video_pts"] - d["audio_pts"]) * rate / PTS_RATE)
    if shift >= 0:
        return "atrim=start_sample=%d,asetpts=PTS-STARTPTS" % shift, shift
    return "adelay=%dS:all=1" % -shift, shift


def convert(disc_id, disc_path, dst, ffmpeg, read=None):
    """The PSP case of importer.fmv_build: CAPLOGO.PMF from a PSP disc into
    `dst` (base/fmv/CAPCOM.AVI) by the player's ffmpeg. Returns the [fmv] row:
    build, source, source_sha256, and then ffmpeg / file / sha256 when written
    or `skipped` with the reason. `dst` is removed first; ffmpeg writes a
    temporary name, renamed on success only."""
    import at3
    import psx_disc
    row = {"build": disc_id, "source": "PSMF/CAPLOGO.PMF"}
    image = psx_disc.Disc(disc_path)
    path = find(image)
    if path is None:
        row["skipped"] = "no %s on %s" % (PMF_SUFFIX[1:], disc_id)
        return row
    b = image.read(path)
    row["source_sha256"] = hashlib.sha256(b).hexdigest()
    if row["source_sha256"] != SOURCE_SHA256:
        row["skipped"] = "%s on %s is not the file docs/psp-install.md measured" % (path, disc_id)
        return row
    if not ffmpeg:
        row["skipped"] = ("no ffmpeg on the PATH or in BOF3X_FFMPEG: the PSP's logo movie is H.264 with "
                          "ATRAC3plus, which only the player's ffmpeg converts")
        return row
    try:
        d = demux(b)
        rate = params(d["params"])[0]
    except ValueError as e:     # nothing here fails the build (importer.fmv_build)
        row["skipped"] = "%s: %s" % (path, e)
        return row
    af, _ = audio_filter(d, rate)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    vid, aud, part = dst + ".mps.tmp", dst + ".at3.tmp", dst + ".part.avi"
    for stale in (vid, aud, part):
        if os.path.exists(stale):
            os.remove(stale)
    with open(vid, "wb") as fh:
        fh.write(d["body"])
    with open(aud, "wb") as fh:
        fh.write(at3_wrap(d["frames"], d["params"]))
    print("  base/fmv  converting %s from %s with the player's ffmpeg (a few minutes)" % (path, disc_id))
    args = [ffmpeg, "-v", "error", "-y", "-f", "mpeg", "-i", vid, "-f", "wav", "-i", aud,
            "-map", "0:v:0", "-map", "1:a:0", "-vf", VIDEO_FILTER, "-af", af] + OUT_ARGS + [part]
    why = None
    try:
        r = subprocess.run(args, capture_output=True)
        if r.returncode != 0 or not os.path.isfile(part):
            why = "ffmpeg could not convert it: %s" % r.stderr.decode(errors="replace").strip()[-200:]
    except OSError as e:
        why = "ffmpeg (%s) could not run: %s" % (ffmpeg, e)
    finally:
        for stale in (vid, aud) + ((part,) if why else ()):
            if os.path.exists(stale):
                os.remove(stale)
    if why:
        row["skipped"] = why
        return row
    os.replace(part, dst)
    with open(dst, "rb") as fh:
        row.update(ffmpeg=at3.ffmpeg_version(ffmpeg), file="base/fmv/" + os.path.basename(dst),
                   sha256=hashlib.sha256(fh.read()).hexdigest())
    return row


def cmd_probe(iso):
    import psx_disc
    image = psx_disc.Disc(iso)
    path = find(image)
    if path is None:
        raise SystemExit("%s: no %s" % (iso, PMF_SUFFIX[1:]))
    b = image.read(path)
    d = demux(b)
    rate, ch, fbytes = params(d["params"])
    af, shift = audio_filter(d, rate)
    print("%s: %d bytes, sha256 %s (%s)" % (path, len(b), hashlib.sha256(b).hexdigest(),
                                          "the measured file" if hashlib.sha256(b).hexdigest() == SOURCE_SHA256
                                          else "NOT the measured file"))
    print("  program stream %d bytes; video packets %d, first PTS %d (%.3f s)"
          % (len(d["body"]), d["video_packets"], d["video_pts"], d["video_pts"] / PTS_RATE))
    print("  ATRAC3plus %d Hz, %d channels, %d-byte frames: %d frames (%.3f s), first PTS %d (%.3f s)"
          % (rate, ch, fbytes, len(d["frames"]), len(d["frames"]) * SAMPLES_PER_FRAME / rate,
             d["audio_pts"], d["audio_pts"] / PTS_RATE))
    print("  audio against the first frame: %+d samples (%s)" % (-shift, af))


def _pes(sid, pts, payload):
    x = (pts >> 30 & 7) << 1 | 0x21, pts >> 22 & 0xFF, (pts >> 15 & 0x7F) << 1 | 1, pts >> 7 & 0xFF, (pts & 0x7F) << 1 | 1
    body = bytes([0x81, 0x80, 5]) + bytes(x) + payload
    return b"\0\0\1" + bytes([sid]) + struct.pack(">H", len(body)) + body


def check():
    """CI (importer.py check): demux and the AT3 wrapper on a synthetic PSMF -
    a pack header, a video packet, two audio frames split across two private
    stream 1 packets; and a damaged frame header refused. No game data."""
    errs = []
    par = bytes([0x28, 0x5C])                          # 44.1 kHz, stereo, 744-byte frames
    frames = [bytes((i * 7 + k) & 0xFF for i in range(744)) for k in range(2)]
    audio = b"".join(FRAME_SYNC + par + bytes(4) + f for f in frames)
    cut = 500
    ps = (b"\0\0\1\xba" + bytes([0x44, 0, 4, 0, 4, 1, 0, 0, 3, 0xF8])
          + _pes(VIDEO_ID, 90000, b"\0\0\0\1\x09\xf0")
          + _pes(AUDIO_ID, 85069, bytes([AUDIO_SUB, 0, 0, 0]) + audio[:cut])
          + _pes(AUDIO_ID, 85069 + 4180, bytes([AUDIO_SUB, 0, 0, 0]) + audio[cut:]) + b"\0\0\1\xb9")
    psmf = b"PSMF0014" + struct.pack(">II", 2048, len(ps)) + bytes(2048 - 16) + ps
    try:
        d = demux(psmf)
        if (d["video_pts"], d["audio_pts"], d["video_packets"], d["params"]) != (90000, 85069, 1, par):
            errs.append("demux: the time stamps or parameters (%s)" % ((d["video_pts"], d["audio_pts"]),))
        if d["frames"] != frames or d["body"] != ps:
            errs.append("demux: the frames or the program stream")
        if params(par) != (44100, 2, 744):
            errs.append("params: %s" % (params(par),))
        if audio_filter(d, 44100) != ("atrim=start_sample=2416,asetpts=PTS-STARTPTS", 2416):
            errs.append("audio_filter: %s" % (audio_filter(d, 44100),))
        w = at3_wrap(d["frames"], d["params"])
        tag, ch, rate, avg, block, bits, cb = struct.unpack_from("<HHIIHHH", w, 20)
        if (w[:4], w[8:16], tag, ch, rate, block, bits, cb) != (b"RIFF", b"WAVEfmt ", 0xFFFE, 2, 44100, 744, 0, 34):
            errs.append("at3_wrap: the fmt fields")
        if w[44:60] != ATRAC3PLUS_GUID or w[60:64] != b"\1\0" + par or w[72:76] != b"data" \
                or w[80:] != b"".join(frames) or struct.unpack_from("<I", w, 4)[0] != len(w) - 8:
            errs.append("at3_wrap: the sub-format, the parameters or the data")
    except ValueError as e:
        errs.append("demux: %s" % e)
    bad = psmf.replace(FRAME_SYNC + par + bytes(4) + frames[1][:4], FRAME_SYNC + par + b"\0\0\0\1" + frames[1][:4])
    try:
        demux(bad)
        errs.append("demux: a damaged frame header was not refused")
    except ValueError:
        pass
    return errs


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "check":
        e = check()
        print("\n".join(e) or "psp_fmv check: 0 errors")
        sys.exit(1 if e else 0)
    if len(sys.argv) != 3 or sys.argv[1] != "probe":
        raise SystemExit(__doc__)
    cmd_probe(sys.argv[2])
