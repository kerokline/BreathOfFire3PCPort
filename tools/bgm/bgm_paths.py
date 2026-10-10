"""Where the music tools find their inputs (docs/bgm-comparison.md sections 6..9).

Every path is overridable from the environment; the defaults are the layout of
the owner's machine. Inputs are game data and are only read; outputs go to
<PC>/analysis/bgm (gitignored), disc copies and emulator state to SCRATCH.

    BOF3_PC_ROOT    the main checkout, holding bof3/ (the PC install) and analysis/
    BOF3_SIBLING    the archival sibling checkout (isos/, disc/SLPS_009.90, tools/)
    BOF3_BIN_ROOT   the psx-jp disc's BIN/ tree extracted (the sibling's convention)
    BGM_SCRATCH     where patched disc copies and the Mednafen base directory go
    BGM_LOOPS_JSON  the loop measurements (default <PC>/analysis/bgm/loops.json); a copy
                    elsewhere lets measure_loops.py regate and gen_loop_table.py run on it
    BGM_SEQ_PERIODS each song's loop pass lengths in VSyncs from our sequencer
                    (tools/bgm/seq_periods.py; default <PC>/analysis/bgm/seq_periods.json),
                    which measure_loops.py pins the period to and gates on

BOF3_BIN_ROOT has no checkout to be derived from (an extraction of the disc
that lives outside every repository); its default is the sibling's own
convention (BreathOfFire3Recomp tools/audio_banks.py, the same variable).

    python bgm_paths.py NAME    prints one of the paths below (mrun.sh reads SIB, SCRATCH)
"""
import os, sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.dirname(os.path.dirname(_HERE))


def _main_checkout(repo):
    # a worktree under .claude/worktrees/<name> reads the main checkout's gitignored data
    parts = repo.replace("\\", "/").split("/.claude/worktrees/")
    return parts[0]


PC = os.environ.get("BOF3_PC_ROOT", _main_checkout(_REPO)).replace("\\", "/")
SIB = os.environ.get("BOF3_SIBLING", os.path.join(os.path.dirname(PC), "BreathOfFire3Recomp")).replace("\\", "/")
BIN_ROOT = os.environ.get("BOF3_BIN_ROOT", r"D:\BoFIII").replace("\\", "/")
BIN = BIN_ROOT + "/BIN"
SLPS = SIB + "/disc/SLPS_009.90"
JP_BIN = SIB + "/isos/Breath of Fire III (Japan).bin"
JP_CUE = SIB + "/isos/Breath of Fire III (Japan).cue"
SCRATCH = os.environ.get("BGM_SCRATCH", os.path.join(PC, "analysis", "bgm", "scratch")).replace("\\", "/")
LOOPS_JSON = os.environ.get("BGM_LOOPS_JSON", PC + "/analysis/bgm/loops.json").replace("\\", "/")
SEQ_PERIODS = os.environ.get("BGM_SEQ_PERIODS", PC + "/analysis/bgm/seq_periods.json").replace("\\", "/")

if __name__ == "__main__":
    print(globals()[sys.argv[1]])
