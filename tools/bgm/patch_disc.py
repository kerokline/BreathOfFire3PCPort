"""A scratch copy of the psx-jp image whose title plays song N and never leaves the logo.

Two patches in GAME.EMI section 1 (PSX 0x801D0C00), both in the title's states:
  0x801D0CD0  addiu a0, zero, 0x8D   -> song N      (Title state 0's Music_Play)
  0x801D0DD4  addiu v1, zero, 0x384  -> 0x7FFF      (state 3's frame count)
The copy lives in the scratch directory; the held image is only read.

    python patch_disc.py SONG OUTDIR
"""
import os, shutil, struct, sys
from bgm_paths import SIB
sys.path.insert(0, SIB + "/tools")
import file_ids
import cdsector

SRC = SIB + "/isos/Breath of Fire III (Japan).bin"
CUE = SIB + "/isos/Breath of Fire III (Japan).cue"
GAME_SEC1 = 0x38800  # GAME.EMI section 1's file offset (tools/emi.py list)


def main():
    song, outdir = int(sys.argv[1], 0), sys.argv[2]
    fid = int(sys.argv[3], 0) if len(sys.argv) > 3 else 0x25F
    os.makedirs(outdir, exist_ok=True)
    rows = file_ids.build(cue=CUE, exe=SIB + "/disc/SLPS_009.90")
    lba = [r for r in rows if r["path"].endswith("ETC/GAME.EMI")][0]["lba"]
    dst = os.path.join(outdir, "bof3jp.bin")
    shutil.copyfile(SRC, dst)  # always a fresh copy
    patches = [(GAME_SEC1 + 0xA4, bytes.fromhex("5f020424"), struct.pack("<HH", fid, 0x2404)),
               (GAME_SEC1 + 0xD0, b"\x8d\x00\x04\x24", struct.pack("<HH", song, 0x2404)),
               (GAME_SEC1 + 0x1D4, b"\x84\x03\x03\x24", struct.pack("<HH", 0x7FFF, 0x2403))]
    fixed = []
    with open(dst, "r+b") as f, open(SRC, "rb") as g:
        for off, want, new in patches:
            sec, within = divmod(off, 2048)
            pos = (lba + sec) * 2352 + 24 + within
            g.seek(pos); orig = g.read(4)
            assert orig == want, (hex(off), orig.hex(), want.hex())
            f.seek(pos); f.write(new)
            fixed.append(lba + sec)
    cdsector.fix_in_file(dst, fixed)
    open(os.path.join(outdir, "bof3jp.cue"), "w").write(
        open(CUE).read().replace("Breath of Fire III (Japan).bin", "bof3jp.bin"))
    print("lba", lba, "song", song, "->", dst)


if __name__ == "__main__":
    main()
