#!/usr/bin/env python
"""The spell sweep: every ability cast in a battle that cannot end, for the
render check and the gap watch (docs/spell-sweep.md).

A harness, not a route: every recipe it writes pokes the game's memory (the
`poke` step), so nothing here is a player's path. Two sides:

  --side member   a member casts the ability from the ability window. Per id:
     1. wait for a member's command cross (the battle bytes 0x904AA0 phase 1,
        0x904AA1 step 2);
     2. poke every member's working record (0x802DC0 + m * 0x14C): HP, max
        HP, AP, max AP, DEF 999; the first slot of each of the four ability
        lists (+0x60 / +0x6A / +0x74 / +0x7E) = the id, so row 0 of whichever
        page the window opens on is the ability; every enemy's HP 0xFFFF
        (0x93B960 + k * 0x128 + 0xA4), which Battle_ApplyDamage leaves alone;
     3. open the ability window (up, confirmed with up held), shoot the list,
        confirm row 0, then confirm the target when the ability asks for one
        (seek circle until the command step leaves the confirm dispatch);
     4. every other member Defends (right, confirmed with right held);
     5. once the input phase ends, a timed series of shots while the cast plays.
  --side enemy    enemy 0 casts it: its eight ability bytes (+0x9C) = the id,
     its action-kind row (0x65563C[+0x8E], four 2-bit kinds) = 0xFF so the
     pick is kind 3 (an ability) and its flag word +0x90 gets bit 0x20 (the
     value the fight's enemy has, --enemy-flags, with the bit set); the
     members Defend; the shots follow the input phase as above.

  --kind AREA:SLOT --at X:Z:FLAGS   (with --side enemy) the fight's enemy 0 is
     that area's slot, so an enemy skill is cast by an enemy that has it: the
     route is kept to --field-cut (the party in the field), `goto` stands the
     party in AREA at a link's arrival (door_sweep.py's steps.json), the area's
     eight encounter rows are poked to SLOT alone, the encounter threshold to 1
     past the step count, and the leader walks (--walk) until the battle; the
     set-up then loads the enemy's sprite and animations from the area's own
     data. Only a kind in a random-encounter row: the boss kinds (animation
     bank 0xFFFF) are an event battle's, reached by a route that fights them
     (docs/spell-sweep.md 10, 11). --enemy-odds / --enemy-flags take that kind's
     odds index (+0x0E) and flag word (+0x12) of its area record.

The id -> name table is the archival sibling's names/abilities.toml read one
id down (the game's list byte N is the sibling's id N - 1: magic_c2.md 1,
and the sweep's first shot, Nue Stomp at byte 1); its sections sort the ids
(--sections). Names and sections go to the marks and the results only.

Usage:
  python tools/spell_sweep.py names [--sections ...]
  python tools/spell_sweep.py recipes --route R --cut F --members N --out DIR [--ids ..] [--chunk K]
  python tools/spell_sweep.py run --route R --cut F --members N --root DIR --launcher EXE
      [--side member|enemy] [--sections "Skills,Attack type spells"] [--ids ..] [--chunk K]
      [--speed 4] [--lang en-US] [--env K=V ...] [--enemy-flags 0x..]
      [--poke "ADDR TYPE VALUE" ...] [--peek "ADDR TYPE NAME" ...] [--sibling DIR]
      [--kind AREA:SLOT --at X:Z:FLAGS [--walk up,left,down,right] [--field-cut 371]]
  `run` generates a chunk, runs it through input_run.py, reads the launcher's
  bof3x.log, writes results.tsv (id, name, section, status, caster, effect
  handlers, frames) and goes on past a FATAL or FAILED at the id after it.
  In an --env value, {chunk} becomes the chunk's tag (member_013): one file a
  chunk for a variable that names an output file (BOF3X_STPCENSUS=DIR/{chunk}.tsv).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time
import tomllib

TOOLS = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(TOOLS)
# The archival sibling, beside the main checkout (a worktree's main checkout is
# found through git); --sibling overrides (the other row tools' convention:
# area_rows.py, magic_rows.py).


def sibling_dir():
    """The archival sibling's checkout, next to the main checkout (a worktree's
    main checkout is where its .git file points: abc_run.py's main_checkout)."""
    for base in (ROOT, _main_checkout()):
        d = os.path.join(os.path.dirname(base), 'BreathOfFire3Recomp')
        if os.path.isdir(os.path.join(d, 'names')):
            return d
    return os.path.join(os.path.dirname(ROOT), 'BreathOfFire3Recomp')   # --sibling names it when this is wrong


def _main_checkout():
    """A worktree's main checkout (abc_run.py's main_checkout), else ROOT."""
    try:
        common = subprocess.run(['git', 'rev-parse', '--path-format=absolute', '--git-common-dir'], cwd=ROOT,
                                capture_output=True, text=True, check=True).stdout.strip()
        return os.path.dirname(os.path.normpath(common))
    except (OSError, subprocess.CalledProcessError):
        return ROOT


SIBLING = sibling_dir()

OBJ_TRIO = 0x802D40      # the party's three records (records::PartyMember, 0x14C each); the working record below is +0x80 of each
MEMBER_STATUS = 0x90     # u16: the member's status word (BattleInput_NextMember skips bit 0x20; 0x800 Ovum's egg)
WORKING = 0x802DC0       # the working member record, Char_AbilityList's battle side
WORKING_STRIDE = 0x14C
HP, AP, MAX_HP, MAX_AP, DEF, AGI = 0x18, 0x1A, 0x20, 0x22, 0x26, 0x28
CONFIRM = 'circle'       # the confirm button the recipe presses; --confirm cross on a layer whose confirm is cross (en-150's)
STATUS = 0x10            # the status word of the working record (ObjTrio's +0x90): 0x4000 is a fallen member,
                         # the word the hit path writes outright with HP 0 (battle_e3.cpp's two death sites)
LISTS = (0x60, 0x6A, 0x74, 0x7E)   # the four ten-byte ability lists by type
ENEMY = 0x93B960
ENEMY_STRIDE = 0x128
ENEMY_HP, ENEMY_ODDS, ENEMY_FLAGS, ENEMY_STATUS, ENEMY_ABILITIES, ENEMY_FLAGS114 = 0xA4, 0x8E, 0x90, 0x92, 0x9C, 0x114
ENEMY_SLOTS = 8
ACT_KINDS = 0x65563C     # u8 by the enemy's +0x8E: four 2-bit action kinds
INV_IDS0, INV_COUNTS0 = 0x904154, 0x904354   # category 0's 128 item ids and counts (the pointers at 0x656B00 / 0x656B14)
ITEM_SAVED_PAGE = 0x904605                   # the battle item window's saved page, top and cursor (3 bytes)
PHASE, STEP, COMMAND = 0x904AA0, 0x904AA1, 0x904AA2
EXTRA_ROUND = 0x904B7A   # u8: the EX round's gate - set at a round's end when a side outpaces the other (Battle_MarkFasterSide);
                         # the order builders then admit only the marked actors (battle_damage.cpp's Gated), and the
                         # combat route's three members outpace Gooey every round, so every other round is theirs alone
ENEMY_COMMAND_KIND = 0x105   # u8: BattleEnemy_PickAction's kind (4 an ability), cleared by EnemyOp_CastDoneCost
ENEMY_COUNT = 0x904AB2
FIELD_MODE = 0x66C7E8    # u16: 5 in battle
# --kind: the fight's enemy chosen before the battle loads (docs/spell-sweep.md 11)
GAME_AREA = 0x904EFC     # u16 Game_AreaNumber: the NNN of the AREAnnn.DAT in play
FIELD_REQUEST = 0x66C7D8 # u8 Field_Request: 5 asks for an area change, 0 once it is done (door_sweep.py)
ENC_ROWS = 0x8C5580      # Encounter_Rows: 8 rows of 9 bytes (8 slot kinds, 0xFF none; +8 the weight), loaded with the area
ENC_SLOT_CHANCE = 0x669CB0   # Encounter_SlotChance: u8 x 8, Encounter_FillSlots' bound for (Rand & 0xF) + 1
ENC_THRESHOLD = 0x802D40 + 0x134   # ObjTrio +0x134: the leader's steps-to-encounter threshold (Field_EncounterDue)
STEP_COUNT = 0x905B80    # u16 Field_StepCount: the leader's steps toward it
ENEMY_DATA_ID = 0xF0     # u8 of the enemy record: the area's slot it was set up from (Battle_CopyEnemyData)

# Abilities Skill_CanUse mode 2 refuses unless a byte agrees (battle_window_draw.cpp):
# 0x14 only while the working record's +0x1E is below 5, 0x8C only at 0 AP.
SPECIAL_AP_ZERO = {0x8C}
SPECIAL_1E_ZERO = {0x14}
MEMBER_SECTIONS = ('Skills', 'Attack type spells', 'Assist type spells', 'Heal type spells')


def names(sibling=SIBLING):
    """{game id: (name, section)}: the sibling's id + 1."""
    with open(os.path.join(sibling, 'names', 'abilities.toml'), 'rb') as f:
        d = tomllib.load(f)
    out = {}
    for a in d['ability']:
        out[a['id'] + 1] = (a.get('us') or a.get('en') or a.get('jp') or '', a.get('gloss_section') or 'none')
    return out


def item_names(sibling=SIBLING):
    """{item id: (name, 'consumable')}: the sibling's consumables, the same ids."""
    with open(os.path.join(sibling, 'names', 'items.toml'), 'rb') as f:
        d = tomllib.load(f)
    out = {}
    for r in d['item']:
        if r.get('category') == 'consumable':
            out[r['id']] = (r.get('us') or r.get('en') or r.get('jp') or '', 'consumable')
    return out


def table_for(side, sibling=SIBLING):
    return item_names(sibling) if side == 'item' else names(sibling)


def parse_ids(spec, table, sections):
    ids = []
    for part in spec.split(','):
        if '-' in part:
            lo, hi = part.split('-')
            ids.extend(range(int(lo, 0), int(hi, 0) + 1))
        else:
            ids.append(int(part, 0))
    ids = [i for i in ids if i in table and i != 0]
    if sections:
        ids = [i for i in ids if table[i][1] in sections]
    return ids


def route_steps(path, cut):
    """The route's lines up to recipe frame `cut` (frames counted as the
    parser counts them: wait N, hold B N, press B [xK] with hold 4 / gap 8,
    shot N; peeks, marks and key N 0 cost nothing)."""
    hold, gap = 4, 8
    frame = 0
    out = []
    with open(path, encoding='utf-8') as f:
        for raw in f:
            line = raw.rstrip('\n')
            t = line.split('#', 1)[0].split()
            if not t:
                if line.startswith('# save'):
                    out.append(line)
                continue
            w = t[0]
            cost = 0
            if w == 'set':
                if t[1] == 'hold': hold = int(t[2])
                if t[1] == 'gap': gap = int(t[2])
            elif w == 'wait':
                cost = int(t[1])
            elif w == 'hold':
                cost = int(t[2])
            elif w == 'press':
                k = int(t[2][1:]) if len(t) > 2 and t[2].startswith('x') else 1
                cost = (hold + gap) * k
            elif w == 'shot':
                cost = int(t[2]) if len(t) > 2 else 30
            elif w == 'key':
                cost = int(t[2]) if len(t) > 2 else 1
            elif w in ('until', 'seek', 'quit', 'end'):
                raise SystemExit(f'route has a {w} step before the cut; cut it by hand: {line}')
            if frame + cost > cut:
                if w == 'wait':
                    out.append(f'wait {cut - frame}')
                break
            frame += cost
            out.append(line)
    return out, frame


def kind_head(route, field_cut, area, slot, at, walk, confirm):
    """--kind: the route kept to `field_cut` (a frame the party stands in the
    field, before the route's own walk), then the party stood in AREA at the
    point `at` (x, z, flags: a link's arrival, door_sweep.py's steps.json) by
    the `goto` step, the area's eight encounter rows poked to SLOT alone (the
    weight 2 each), slot 0's chance to 16 and the encounter threshold to 1 with
    the step count past it, and the leader walked by `walk` (directions held
    16 frames each, round and round) until the field mode is 5. The battle's
    set-up then builds enemy 0 from that slot of the area's own data and
    ENEMYnnn.DAT - its sprite, animations and AI as the game loads them."""
    head, _ = route_steps(route, field_cut)
    x, z, flags = at
    L = list(head)
    L += [f'until 0x{FIELD_MODE:X} u16 == 2 timeout 1800', f'until 0x{FIELD_REQUEST:X} u8 == 0 timeout 1800', 'wait 30',
          f'mark kind area {area} slot {slot}',
          f'goto {area} 0x{x:X} 0x{z:X} 0x{flags:X}',
          'wait 4',
          f'until 0x{FIELD_REQUEST:X} u8 == 0 timeout 1800',
          f'until 0x{FIELD_MODE:X} u16 == 2 timeout 1800',
          'wait 60',
          f'peek 0x{GAME_AREA:X} u16 kind_area']
    for r in range(8):
        base = ENC_ROWS + r * 9
        L.append(f'poke 0x{base:X} u8 0x{slot:X}')
        L += [f'poke 0x{base + k:X} u8 0xFF' for k in range(1, 8)]
        L.append(f'poke 0x{base + 8:X} u8 2')
    L += [f'poke 0x{ENC_SLOT_CHANCE:X} u8 16',
          f'poke 0x{ENC_THRESHOLD:X} u16 1',
          f'poke 0x{STEP_COUNT:X} u16 0x100']
    for _ in range(12):
        for d in walk:
            L.append(f'hold {d} 16')
    return L


def member_pokes(members, ability):
    lines = []
    for m in range(members):
        r = WORKING + m * WORKING_STRIDE
        lines.append(f'poke 0x{r + MAX_HP:X} u16 999')
        lines.append(f'poke 0x{r + HP:X} u16 999')
        lines.append(f'poke 0x{r + MAX_AP:X} u16 999')
        lines.append(f'poke 0x{r + AP:X} u16 {0 if ability in SPECIAL_AP_ZERO else 999}')
        lines.append(f'poke 0x{r + DEF:X} u16 999')
        if ability in SPECIAL_1E_ZERO:
            lines.append(f'poke 0x{r + 0x1E:X} u8 0')
        for off in LISTS:
            lines.append(f'poke 0x{r + off:X} u8 0x{ability:X}')
    for k in range(ENEMY_SLOTS):
        lines.append(f'poke 0x{ENEMY + k * ENEMY_STRIDE + ENEMY_HP:X} u16 0xFFFF')
    return lines


def enemy_pokes(members, ability, enemy_flags, enemy_odds):
    """The members only Defend on this side, so the enemy keeps its own HP.
    Its status word +0x92 is cleared before each cast: the combat route's
    Gooey is paralysed and skips every turn (the owner, 2026-10-26, after the
    first enemy-side run cast nothing - the cross was back within 30 frames
    of every action phase)."""
    lines = []
    for m in range(members):
        r = WORKING + m * WORKING_STRIDE
        lines.append(f'poke 0x{r + MAX_HP:X} u16 999')
        lines.append(f'poke 0x{r + HP:X} u16 999')
        lines.append(f'poke 0x{r + MAX_AP:X} u16 999')
        lines.append(f'poke 0x{r + AP:X} u16 999')
        lines.append(f'poke 0x{r + DEF:X} u16 999')
    e = ENEMY
    lines.append(f'poke 0x{e + ENEMY_STATUS:X} u16 0')
    # the EX round: the fight's members outpace Gooey, so the round after each of its casts is the party's
    # alone (0x904B7A set at the round's end; Battle_BuildTurnOrder admits only the marked) - Gooey cast on
    # every other round of the first runs. Cleared at the cross, before the commit builds the turn order.
    lines.append(f'poke 0x{EXTRA_ROUND:X} u8 0')
    lines.append(f'poke 0x{ACT_KINDS + enemy_odds:X} u8 0xFF')
    lines.append(f'poke 0x{e + ENEMY_FLAGS:X} u16 0x{enemy_flags | 0x20:X}')
    for k in range(8):
        lines.append(f'poke 0x{e + ENEMY_ABILITIES + k:X} u8 0x{ability:X}')
    return lines


def item_pokes(members, item):
    lines = []
    for m in range(members):
        r = WORKING + m * WORKING_STRIDE
        lines.append(f'poke 0x{r + MAX_HP:X} u16 999')
        lines.append(f'poke 0x{r + HP:X} u16 999')
        lines.append(f'poke 0x{r + MAX_AP:X} u16 999')
        lines.append(f'poke 0x{r + AP:X} u16 999')
        lines.append(f'poke 0x{r + DEF:X} u16 999')
    for k in range(ENEMY_SLOTS):
        lines.append(f'poke 0x{ENEMY + k * ENEMY_STRIDE + ENEMY_HP:X} u16 0xFFFF')
    lines.append(f'poke 0x{INV_IDS0:X} u8 0x{item:X}')
    lines.append(f'poke 0x{INV_COUNTS0:X} u8 9')
    for k in range(3):
        lines.append(f'poke 0x{ITEM_SAVED_PAGE + k:X} u8 0')
    return lines


def dead_pokes(dead):
    """--dead: the members made fallen before the cast, as the hit path leaves
    them - the status word 0x4000 outright and HP 0 (battle_e3.cpp). The
    command gate (Battle_ActorCanCommand's mask 0x4964) drops them from the
    cross and the revive abilities and Ammonia find them; the sprite stays in
    its standing pose, since the death pose is the hit sequence's, which the
    poke skips. After the sweep's own HP 999 pokes, which it overrides."""
    lines = []
    for m in dead:
        r = WORKING + m * WORKING_STRIDE
        lines.append(f'poke 0x{r + STATUS:X} u16 0x4000')
        lines.append(f'poke 0x{r + HP:X} u16 0')
    return lines


def dead_peeks(dead):
    """After the shots: did the cast bring them back (HP, the status word)."""
    lines = []
    for m in dead:
        r = WORKING + m * WORKING_STRIDE
        lines.append(f'peek 0x{r + HP:X} u16 dead{m}_hp')
        lines.append(f'peek 0x{r + STATUS:X} u16 dead{m}_status')
    return lines


def target_ups(n):
    """The target cursor starts on the caster (Battle_DefaultTarget: the first
    not-out member from the caster upward) and the party side wraps the index
    without skipping the fallen, so a revive wants the cursor moved as a
    player moves it: n presses of up, (dead - caster) mod 3 - in the combat
    fight Rei (member 2) casts first, so Ryu (0) is one up, Teepo (1) two."""
    return ['wait 8'] + ['press up', 'wait 8'] * n


def defend_block():
    return [f'until 0x{STEP:X} u8 == 2 timeout 900',
            'hold right 12',
            f'hold right+{CONFIRM} 4',
            'hold right 8',
            f'until 0x{STEP:X} u8 != 4 timeout 300']


def defend_all():
    """Every cross of this input phase Defends, however many there are: right
    tapped twice within eight frames chooses the command the cross points at
    (BattleMenu_CommandSelect's tap rule), and a seek presses until the phase
    leaves 1. A fixed count of Defend blocks desynced the recipe whenever a
    cast took a member out of the next round's cross - Ovum's egg (status
    0x800, five rounds) left two crosses, and the third block's press landed
    in the round after (Howling on the disc install, docs/spell-sweep.md 6.2)."""
    return ['set hold 2', 'set gap 4',
            f'seek right 0x{PHASE:X} u8 != 1 max 40',
            'set hold 4', 'set gap 8']


def cast_block(side, ability, name, members, shots, spacing, enemy_flags, enemy_odds=1, pokes=(), peeks=(),
               dead=(), ups=0):
    tag = f'{"i" if side == "item" else "a"}{ability:03d}'
    L = [f'mark ability {ability} {name}',
         f'until 0x{STEP:X} u8 == 2 timeout 1800']
    if side == 'member':
        L += member_pokes(members, ability) + dead_pokes(dead)
        L += ['hold up 12',
              f'hold up+{CONFIRM} 4',
              'hold up 8',
              f'until 0x{COMMAND:X} u8 == 1 timeout 120',
              'wait 24',
              f'shot {tag}_list 2',
              f'press {CONFIRM}']
        L += target_ups(ups)
        L += [f'seek {CONFIRM} 0x{STEP:X} u8 != 4 max 3']
        if dead:
            L += defend_all()           # the fallen are out of the cross: however many crosses are left
        else:
            for m in range(1, members):
                L += defend_block()
    elif side == 'item':
        L += item_pokes(members, ability) + dead_pokes(dead)
        L += ['hold down 12',
              f'hold down+{CONFIRM} 4',
              'hold down 8',
              f'until 0x{COMMAND:X} u8 == 2 timeout 120',
              'wait 24',
              f'shot {tag}_list 2',
              f'press {CONFIRM}']
        L += target_ups(ups)
        L += [f'seek {CONFIRM} 0x{STEP:X} u8 != 4 max 3']
        if dead:
            L += defend_all()
        else:
            for m in range(1, members):
                L += defend_block()
    else:
        L += enemy_pokes(members, ability, enemy_flags, enemy_odds)
        L += defend_all()
    L += [f'poke {p}' for p in pokes]          # --poke: after the sweep's own, before the cast
    L += [f'until 0x{PHASE:X} u8 != 1 timeout 900']
    if side == 'enemy':
        # the enemy's pick: its command kind +0x105 reads 4 from BattleEnemy_PickAction (its turn, after the
        # members' Defends) until EnemyOp_CastDoneCost clears it; a timeout here is "the enemy did not cast"
        L += [f'until 0x{ENEMY + ENEMY_COMMAND_KIND:X} u8 == 4 timeout 300',
              f'peek 0x{ENEMY + ENEMY_COMMAND_KIND:X} u8 enemy0_command_kind', f'peek 0x{ENEMY + 0x106:X} u16 enemy0_ability']
    for k in range(shots):
        L += [f'shot {tag}_{k} 2', f'wait {spacing}']
    if side == 'enemy':
        # an enemy skill's status on the party (Howling and Bone Dance put members to sleep, +0x90 bit 0x20)
        # takes them out of the cross until the round-end wake roll says otherwise - a random number of rounds
        # the fight runs by itself, Gooey casting again each round. The low status byte is cleared after the
        # shots (the round after is theirs again); the high byte keeps Ovum's egg (0x800, its own five rounds).
        for m in range(members):
            L += [f'poke 0x{OBJ_TRIO + m * WORKING_STRIDE + MEMBER_STATUS:X} u8 0']
    L += dead_peeks(dead)
    L += [f'peek {p}' for p in peeks]          # --peek: after the shots
    # back to the input phase before the next block, so a stall is this ability's: a message a cast
    # leaves open waits for a button (confirm every twelve frames), a cast that never ends gives up at 480 frames
    L += [f'seek {CONFIRM} 0x{PHASE:X} u8 == 1 max 40']
    return L


def prologue_peeks():
    e = ENEMY
    return [f'peek 0x{ENEMY_COUNT:X} u8 enemies',
            f'peek 0x{GAME_AREA:X} u16 area',
            f'peek 0x{e + ENEMY_DATA_ID:X} u8 enemy0_slot',
            f'peek 0x{e + ENEMY_ODDS:X} u8 enemy0_odds',
            f'peek 0x{e + ENEMY_FLAGS:X} u16 enemy0_flags90',
            f'peek 0x{e + ENEMY_STATUS:X} u16 enemy0_status92',
            f'peek 0x{e + ENEMY_FLAGS114:X} u32 enemy0_flags114',
            f'peek 0x{e + ENEMY_HP:X} u16 enemy0_hp']


def write_recipe(path, a, head, chunk, table, n):
    lines = [f'# spell sweep chunk {n} ({a.side} side): abilities {chunk[0]}..{chunk[-1]} - a harness recipe (pokes), not a route',
             f'# generated by tools/spell_sweep.py from {os.path.basename(a.route)} cut at frame {a.cut}']
    lines += head
    lines += [f'until 0x{FIELD_MODE:X} u16 == 5 timeout 3000', 'mark in battle'] + prologue_peeks()
    lines += [f'seek {CONFIRM} 0x{PHASE:X} u8 == 1 max 120']
    for i in chunk:
        lines += cast_block(a.side, i, table[i][0], a.members, a.shots, a.spacing, a.enemy_flags, a.enemy_odds,
                            a.poke, a.peek, a.dead, a.target_ups)
    lines += ['mark chunk done', 'end']
    if a.confirm != CONFIRM:
        # the sweep's own steps take the layer's confirm button; the route's head is the recording's
        k = len(head) + 2
        lines[k:] = [l.replace(CONFIRM, a.confirm) for l in lines[k:]]
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    return len(lines)


MARK = re.compile(r'input\s+mark ability (\d+) (.*) recipe frame (\d+)')
SHOT = re.compile(r'input\s+shot ([ai]\d+)_(\d+) recipe frame (\d+)')
EFFECT = re.compile(r'effect\s+run slot \d+ handler (0x[0-9A-Fa-f]+)')
END = re.compile(r'^(FATAL: .*|CRASH.*|end: .*|recipe done.*|input\s+line \d+: (?:until|seek) .*(?:timed out|gave up).*)$')


def read_log(path, shots):
    """Per ability: shots, effect handlers, frames; the chunk's end ('done'
    once the recipe's last mark was logged, else the first FATAL / CRASH /
    timed-out / gave-up line, else '')."""
    per = {}
    order = []
    cur = None
    end = ''
    fail_line = ''
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip(chr(10))
            m = MARK.match(line)
            if m:
                cur = int(m.group(1))
                order.append(cur)
                per[cur] = {'name': m.group(2), 'start': int(m.group(3)), 'shots': 0, 'effects': [], 'end_frame': None}
                continue
            if 'mark chunk done' in line:
                end = 'done'
                continue
            if line.startswith('FATAL') or line.startswith('CRASH') or 'timed out' in line or 'gave up' in line:
                if not fail_line:
                    fail_line = line
                continue
            if cur is None:
                continue
            m = SHOT.match(line)
            if m:
                per[cur]['shots'] += 1
                per[cur]['end_frame'] = int(m.group(3))
                continue
            m = EFFECT.search(line)
            if m:
                h = m.group(1).lower()
                if h not in per[cur]['effects']:
                    per[cur]['effects'].append(h)
    return order, per, end, fail_line


def rows_of(chunk, table, side, tag, order, per, end, fail, shots):
    """One result row per ability of the chunk, in order; stops at the
    first not reached."""
    last = order[-1] if order else None
    rows = []
    for i in chunk:
        p = per.get(i)
        if p is None:
            status, note = 'not reached', (fail or end)
        elif p['shots'] >= shots:
            status, note = 'done', ''
        elif i == last and end != 'done':
            # the window kept the row (greyed: Skill_CanUse / Item_CanUse said no) - a refusal, not a defect
            refused = 'gave up' in (fail or '') and (fail or '').rstrip().endswith('value 0x4')
            status, note = ('refused' if refused else 'failed'), (fail or 'ended early')
        else:
            status, note = 'short', f'{p["shots"]} shots'
        frames = (p['end_frame'] - p['start']) if p and p['end_frame'] else ''
        rows.append((i, table[i][0], table[i][1], side, status, p['shots'] if p else 0,
                     ' '.join(p['effects']) if p else '', frames, tag, note))
        if status == 'not reached':
            break
    return rows


def make_head(a):
    if not a.kind:
        return route_steps(a.route, a.cut)[0]
    return kind_head(a.route, a.field_cut, a.kind[0], a.kind[1], a.at, a.walk, a.confirm)


def run(a):
    table = table_for(a.side, a.sibling)
    ids = parse_ids(a.ids, table, a.sections)
    head = make_head(a)
    os.makedirs(a.root, exist_ok=True)
    rec_dir = os.path.join(a.root, 'recipes')
    os.makedirs(rec_dir, exist_ok=True)
    results = os.path.join(a.root, f'results_{a.side}.tsv')
    launcher_dir = os.path.dirname(a.launcher)
    log = os.path.join(launcher_dir, 'bof3x.log')
    done = set()
    if os.path.exists(results) and not a.fresh:
        with open(results, encoding='utf-8') as f:
            for line in f:
                if line.startswith('#') or line.startswith('id\t'):
                    continue
                cols = line.split('\t')
                if len(cols) > 4 and cols[4] == 'done':      # a failed id is run again
                    done.add(int(cols[0]))
    else:
        with open(results, 'w', encoding='utf-8') as f:
            f.write('# tools/spell_sweep.py run: one row per ability cast (docs/spell-sweep.md). Addresses, ids and names only.\n')
            f.write('id\tname\tsection\tside\tstatus\tshots\teffect_handlers\tframes\tchunk\tnote\n')
    todo = [i for i in ids if i not in done]
    n = 0
    while todo:
        chunk = todo[:a.chunk]
        n += 1
        tag = f'{a.side}_{chunk[0]:03d}'
        recipe = os.path.join(rec_dir, f'sweep_{tag}.txt')
        write_recipe(recipe, a, head, chunk, table, n)
        out = os.path.join(a.root, f'run_{tag}')
        if os.path.exists(log):
            os.remove(log)
        minutes = max(3.0, 0.35 * len(chunk) + 2.0) * (4.0 / a.speed)
        cmd = [sys.executable, os.path.join(TOOLS, 'input_run.py'), recipe, '--out', out, '--launcher', a.launcher,
               '--speed', str(a.speed), '--no-front', '--minutes', f'{minutes:.1f}', '--lang', a.lang]
        if a.game:
            cmd += ['--game', a.game]
        for kv in a.env:
            cmd += ['--env', kv.replace('{chunk}', tag)]
        t0 = time.time()
        print(f'[{time.strftime("%H:%M:%S")}] chunk {tag}: {len(chunk)} abilities {chunk[0]}..{chunk[-1]}', flush=True)
        with open(os.path.join(a.root, f'run_{tag}.log'), 'w', encoding='utf-8') as f:
            subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, cwd=ROOT)
        if os.path.exists(log):
            shutil.copy(log, os.path.join(a.root, f'bof3x_{tag}.log'))
            order, per, end, fail = read_log(log, a.shots)
        else:
            order, per, end, fail = [], {}, 'no log', 'no log'
        rows = rows_of(chunk, table, a.side, tag, order, per, end, fail, a.shots)
        with open(results, 'a', encoding='utf-8') as f:
            for r in rows:
                f.write('\t'.join(str(x) for x in r) + '\n')
        reached = [r[0] for r in rows if r[4] != 'not reached']
        print(f'    {len(rows)} rows in {time.time() - t0:.0f} s; end: {end or fail}', flush=True)
        for r in rows:
            if r[4] != 'done':
                print(f'    {r[0]} {r[1]}: {r[4]} {r[9]}', flush=True)
        # go on after the last ability the run reached (a failure takes one id with it)
        todo = [i for i in todo if i not in reached]
        if not reached:
            print('    nothing reached - the route itself failed; stopping', flush=True)
            break
    print('sweep over', flush=True)


def report(a):
    """results_<side>.tsv rebuilt from the root's bof3x_<side>_NNN.log copies."""
    table = table_for(a.side, a.sibling)
    logs = sorted(f for f in os.listdir(a.root) if f.startswith(f'bof3x_{a.side}_') and f.endswith('.log'))
    results = os.path.join(a.root, f'results_{a.side}.tsv')
    with open(results, 'w', encoding='utf-8') as out:
        out.write('# tools/spell_sweep.py report: one row per ability cast (docs/spell-sweep.md). Addresses, ids and names only.' + chr(10))
        out.write('id	name	section	side	status	shots	effect_handlers	frames	chunk	note' + chr(10))
        best = {}
        for f in logs:
            tag = f[len('bof3x_'):-len('.log')]
            order, per, end, fail = read_log(os.path.join(a.root, f), a.shots)
            for r in rows_of(order, table, a.side, tag, order, per, end, fail, a.shots):
                # a re-run of an id keeps the better row (done over anything)
                if r[0] not in best or (r[4] == 'done' and best[r[0]][4] != 'done'):
                    best[r[0]] = r
        for i in sorted(best):
            out.write(chr(9).join(str(x) for x in best[i]) + chr(10))
    print(results)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--sibling', default=SIBLING, help="the archival sibling's checkout (names/abilities.toml, names/items.toml)")
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('names')
    p.add_argument('--sections', default='')
    p.add_argument('--side', choices=('member', 'enemy', 'item'), default='member')
    p = sub.add_parser('report')
    p.add_argument('--root', required=True)
    p.add_argument('--side', choices=('member', 'enemy', 'item'), default='member')
    p.add_argument('--shots', type=int, default=6)
    for name in ('recipes', 'run'):
        p = sub.add_parser(name)
        p.add_argument('--route', required=True)
        p.add_argument('--cut', type=int, required=True, help='the recipe frame the route reached battle (field mode 5)')
        p.add_argument('--members', type=int, default=3)
        p.add_argument('--ids', default='1-227')
        p.add_argument('--sections', default='', help='comma-separated gloss sections to keep; "member" = the four a member casts')
        p.add_argument('--chunk', type=int, default=10)
        p.add_argument('--shots', type=int, default=6)
        p.add_argument('--spacing', type=int, default=30)
        p.add_argument('--side', choices=('member', 'enemy', 'item'), default='member')
        p.add_argument('--enemy-odds', type=lambda s: int(s, 0), default=1, help="the fight's enemy 0 byte +0x8E (peeked): its row of 0x65563C is poked to 0xFF")
        p.add_argument('--enemy-flags', type=lambda s: int(s, 0), default=0, help="the fight's enemy 0 flag word +0x90 (peeked); bit 0x20 is added")
        p.add_argument('--poke', action='append', default=[], metavar='"ADDR TYPE VALUE"',
                       help="an input_run poke added to every cast after the sweep's own (an A/B on one byte)")
        p.add_argument('--peek', action='append', default=[], metavar='"ADDR TYPE NAME"',
                       help="an input_run peek logged after every cast's shots")
        p.add_argument('--confirm', default='circle', choices=('circle', 'cross'),
                       help="the confirm button of the install's layer (en-US circle; en-150 cross)")
        p.add_argument('--dead', default='', metavar='M[,M]',
                       help="members (0..2) made fallen before every cast - status 0x4000, HP 0 - for the revives; "
                            "the other crosses then Defend by one seek (defend_all), and the fallen's HP and status are peeked after the shots")
        p.add_argument('--target-ups', type=int, default=None,
                       help="presses of up on the target cursor before the confirm (default (first dead - 2) mod 3: Rei casts first in the combat fight)")
        p.add_argument('--kind', default='', metavar='AREA:SLOT',
                       help="the fight's enemy 0 from this area's slot: the party stood in AREA by `goto`, the area's encounter "
                            "rows poked to SLOT alone, a step walked until the battle (kind_head; docs/spell-sweep.md 11)")
        p.add_argument('--at', default='', metavar='X:Z:FLAGS',
                       help="--kind: where goto stands the party (16.16 cells and the entry flags: a link's arrival, door_sweep's steps.json)")
        p.add_argument('--walk', default='up,left,down,right', help='--kind: the directions the leader walks, 16 frames each, round and round')
        p.add_argument('--field-cut', type=int, default=371,
                       help="--kind: the route kept to this frame - the party in the field, before the route's own walk (combat.txt: 371)")
        if name == 'recipes':
            p.add_argument('--out', required=True)
        else:
            p.add_argument('--root', required=True)
            p.add_argument('--launcher', required=True)
            p.add_argument('--speed', type=int, default=4)
            p.add_argument('--lang', default='en-US')
            p.add_argument('--env', action='append', default=[], help='K=V for input_run.py; {chunk} in V becomes the chunk tag')
            p.add_argument('--game', default='', help="the game folder (input_run.py --game); a disc-built install's, with BOF3X_EXEIMAGE in --env")
            p.add_argument('--fresh', action='store_true', help='start results.tsv over')
    a = ap.parse_args()
    if getattr(a, 'kind', ''):
        a.kind = tuple(int(v, 0) for v in a.kind.split(':'))
        if not a.at:
            raise SystemExit('--kind wants --at X:Z:FLAGS (a link arrival in the area)')
        a.at = tuple(int(v, 0) for v in a.at.split(':'))
        a.walk = [d.strip() for d in a.walk.split(',') if d.strip()]
    if hasattr(a, 'dead'):
        a.dead = tuple(int(x) for x in a.dead.split(',') if x != '')
        if a.target_ups is None:
            a.target_ups = (a.dead[0] - 2) % 3 if a.dead else 0
    if getattr(a, 'sections', '') == 'member':
        a.sections = set(MEMBER_SECTIONS)
    elif getattr(a, 'sections', ''):
        a.sections = set(s.strip() for s in a.sections.split(','))
    else:
        a.sections = set()
    table = table_for(getattr(a, 'side', 'member'), a.sibling)
    if a.cmd == 'names':
        for i in sorted(table):
            if not a.sections or table[i][1] in a.sections:
                print(i, table[i][0], '|', table[i][1])
        return
    if a.cmd == 'run':
        if a.ids == '1-227' and a.side == 'item':
            a.ids = '1-91'
        run(a)
        return
    if a.cmd == 'report':
        report(a)
        return
    ids = parse_ids(a.ids, table, a.sections)
    head = make_head(a)
    reached = a.field_cut if a.kind else route_steps(a.route, a.cut)[1]
    os.makedirs(a.out, exist_ok=True)
    chunks = [ids[i:i + a.chunk] for i in range(0, len(ids), a.chunk)]
    for n, chunk in enumerate(chunks):
        path = os.path.join(a.out, f'sweep_{n:02d}.txt')
        k = write_recipe(path, a, head, chunk, table, n)
        print(path, len(chunk), 'abilities,', k, 'lines')
    print(f'route kept to frame {reached} of the cut {a.cut}; {len(ids)} abilities in {len(chunks)} chunks')


if __name__ == '__main__':
    main()
