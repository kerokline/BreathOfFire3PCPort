# The readings audit: a second opinion on the furigana builder

**Status:** RUN 2026-10-22 (the readings follow-on group's agent pass,
decided in [`owner-review.md`](owner-review.md) the same day). Nothing in
the tree changed its behaviour: the output is a candidate rule list for the
owner to approve into `data/furigana/readings.toml`, and the builder's
readings are as [`reading-row-builder.md`](reading-row-builder.md) left
them. The derived files (shards, kana, diffs, the candidate fragment, the
review sheet) live in the harness root `C:\Users\kerok\bof3x-runs\rr_audit`
and nowhere else: every one of them is game text (hard rule 1, D328).

## 1. What was done

`tools/ruby_audit.py`, four steps over the builder's `--review` list (the
held `ja-JP` layer at `rr_sl`, the builder as in the tree, the sidecar as in
the tree: 28,170 composed messages, 75,355 readings placed):

1. **`shard`**: the review list reduced to its distinct messages (the pools
   repeat one string across 40 BOSS files): **6,014 messages**, 29,588 kanji
   characters, as blind text (no readings; row and page breaks as `／`,
   a runtime insert as `＊`) in 21 shards of 300, area order, pools last.
2. **The first agent pass**: 21 Opus agents, one per shard, each given the
   brief `BRIEF.md` (the setting, the cast, the output schema) and told to
   read every kanji word itself, no tokeniser, no dictionary library, and
   write `[surface, hiragana]` per word in text order. About 110k tokens
   and three to six minutes each.
3. **`diff`**: the agents' kana against the builder's, aligned by span in
   the blind text. A word the agent wrote longer by kana only (入れた for
   the builder's 入れ) is the same word; a compound the agent read whole
   and the builder in pieces (変異体 as 変異 + 体) agrees when the pieces
   and the kana between them spell the agent's kana; a numeral the agent
   took into its word (3人) is compared on the counter alone; a word with
   kana between its kanji is compared trimmed on both sides (the builder
   prints 逃げ出し whole, an agent may trim its tail). What is left is
   listed by surface and context, most frequent first, in two groups:
   *reading differs* (same word) and *segmentation differs*.
4. **The second agent pass** (`BRIEF_PROPOSE.md`): three Opus agents over
   the 279 disagreement lines, each with the sidecar's schema and the
   builder's reading rules, giving a verdict (builder / agent / context /
   owner) and the `readings.toml` entry that would fix it. **`report`**
   merges them into `candidate_readings.toml` (grouped by verdict, the
   agent's note on each) and `proposals.md` (the sheet, with contexts).
5. **`reach`**: every candidate rule built alone over the tree's sidecar,
   against the baseline review: how many placements it changes. A rule
   that changes none, or far more than the lines it was proposed for, is
   marked in both files. The builds take about 4 s each, six at a time.

## 2. Numbers

| | |
|---|---:|
| distinct messages compared | 6,014 (agent output for all) |
| kanji words compared | 21,207 |
| agree | 19,584 (92.3 %) |
| reading differs, same word | 560 placements, 170 distinct (surface, builder, agent) lines |
| segmentation differs | 209 placements, 109 distinct lines |
| builder word with no agent token over it | 5 |

The proposal pass over the 279 lines:

| verdict | lines | placements | meaning |
|---|---:|---:|---|
| agent | 79 | 305 | the agents are right everywhere: a plain rule |
| context | 101 | 228 | both occur; a `next` / `prev` rule, or the builder has the common case |
| owner | 13 | 71 | a proper noun or a taste call |
| builder | 86 | 165 | the builder is right in these contexts: no rule |

178 distinct rules proposed; reach: **116 change about as many placements
as their lines**, **28 change far more** (OVERREACH in the files: a look,
not a verdict - see 3.3), **34 change nothing** (NO EFFECT: see 3.2).

## 3. Findings

### 3.1 Readings the builder gets wrong, with a plain rule that fixes them

The agents' side was unanimous and the rule's reach matches its lines. The
largest, by placements changed (the full list is the `agent` section of the
fragment, each with the proposing agent's reason):

| surface | builder | agents | placements | sense |
|---|---|---|---:|---|
| 他 | た | ほか | 37 | 他の / 他に, the ordinary word |
| 様 | よう | さま | 24 | after a name insert (`＊様`): the honorific, never "manner" |
| 金 | きん | かね | 19 | money, in every dialogue use |
| 都 | と | みやこ | 17 | the capital, as a word |
| 村 | そん | むら | 12 | after a place name (マクニール村) |
| 身体 | しんたい | からだ | 10 | spoken Japanese |
| 明日 | あす | あした | 8 | spoken Japanese |
| 開い / 開か / 開き | ひら | あ | 14 / 8 / 9 | doors and the like: あく; the agents also flag that a plain rule would take 開く "hold (an event)", which this script does not use (3.3) |

### 3.2 Compounds the builder reads in pieces: the sidecar cannot fix them today

The builder's rules key on one SudachiPy token. Where Sudachi splits a word
the script treats as one (大武会 as 大武 + 会, read おおたけ + かい - the
tournament is だいぶかい; 禁呪 as 禁 + 呪, きん + しゅ for きんじゅ; 水番
as 水 + 番, すい + ばん for みずばん; 彼の地, 小作人 as 小 + 作人, 一日中,
集会所 しょ for じょ, 御力, 竜殺し, 苦労話, 機械好き ...) a rule keyed on
the compound **matches no token and changes nothing** - all 34 NO EFFECT
rules are this. Fixing them wants **a multi-token rule in the builder**: a
sidecar surface that spans consecutive tokens is matched before the
per-token lookup, and read as one word (one reading over the compound,
which is how the layout already sets a merged reading, builder doc 4). A
builder change, so a follow-on of its own; the list of compounds is ready.
Alternatively `--dict full` merges some of these (the builder doc's "core
against full differs in 253 messages, nearly all compound width"), and
should be measured against this list before the builder changes.

### 3.3 Rules that need narrowing before they go in

OVERREACH means the rule changes more than twice the placements of the
lines it was proposed for. Two very different things are in that bin:

- **Wrong as a plain rule.** `来る` = くる (keyed on the dictionary form,
  the builder swaps the tail: 516 placements, 来た becomes く+た - the
  builder doc already says 来る needs its own surfaces); `長` = おさ (60: a
  village elder twice, 村長 / 会長 ちょう 57 times); `実` = み (19, the
  nut; 実は じつ is the other); `者` = もの (16: ふとどき者 yes, 守護者 no);
  `戦` = せん (44, both いくさ and せん occur); 方 both ways (the person /
  direction split wants `prev` lists, and the two proposed lists collide).
  Each wants a `next` / `prev` rule or several surfaces, the owner's eye.
- **Right, and the count is the pools.** `石化` = せっか (44: a battle
  message repeated across the BOSS files, one distinct line), `前世` =
  ぜんせ (22), `宝物` = たからもの (11), `前金` = まえきん (11): the line
  count is distinct messages, the placement count every slot. Fine as
  plain rules.

### 3.4 The owner's calls (verdict `owner`, 13 lines)

- **大武会: だいぶかい (decided 2026-10-23).** Looked for and not found in
  any Capcom text: the script writes it in kanji only, the setting book
  (公式設定資料集) covers the childhood chapter and its kana index has no
  entry, the Taiwanese guide's tournament page brackets the Japanese in
  kanji. 漢羅 かんら and 狂烈 きょうれつ agree everywhere (the fandom wiki's
  Stallion skill 漢羅狂烈波 too). 武 has two on-readings, kan-on ブ and
  go-on ム (武者); the owner chose ブ as the norm of martial compounds
  (武道会). SudachiPy's おおたけかい is the surname 大武 (kun たけ) plus
  会. Entered in `readings.toml`; it applies once the builder has the
  multi-token rule (3.2). The fandom FAQ's "Daimukai" is the other
  on-reading, uncited.
- **盗人: ぬすっと (decided 2026-10-23).** The setting book's kana index
  writes the bat monster ぬすっとこうもり; the owner took that reading for
  the word in speech (12 dialogue lines, all casual; the monster's own name
  is not in the dialogue blocks, so no reading row ever shows it). A plain
  rule; measured to reach exactly those 12 placements.
- **The rest of the owner's calls (2026-10-23), each measured by a build
  against the previous one:** 賢樹 = けんじゅ (21 placements; the fandom
  wiki's Kenju agrees; SudachiDict's たけき is a given name); 外海 =
  がいかい, one reading for the one place (45: the 25 そとうみ and 20 そとめ
  the dictionary had chosen by context); 大星 = おおぼし (『北の大星』, 4);
  魚 = うお in the fishing chant only, by `prev` (AREA113, 4 placements;
  さかな everywhere else - the chant's first 魚 opens a row, where `prev`
  sees nothing and `next` は is any fish's, so it stays さかな until the
  builder's `prev` can look across a row break, a follow-on beside the
  multi-token rule); 生命 = いのち in one threat, by `next` で (AREA079
  and its copy; `prev` could not key it either, the word opens a row);
  剣 as a lone label stays けん (the builder's, no rule). Still open: 海人.

- **賢樹** (21 placements): the builder's たけき is a name reading; the
  agents say けんじゅ. The great tree's reading in this game - the owner
  decides; neither side is the dictionary.
- **外海** (25 そとうみ, 20 そとめ in the builder): the Outer Sea. The agents
  split がいかい / そとうみ; そとめ is wrong either way. One reading for all
  45, the owner's.
- **海人** (かいにん in the builder): the sea people - かいじん / あま /
  うみびと.
- **剣** as a lone label (けん / つるぎ), **生命** (いのち over せいめい, a
  stylistic ruby), **盗人** (ぬすびと / ぬすっと, 12), **魚** in the chant
  (さかな / うお), **大星** (おおほし / おおぼし, 4).

### 3.5 Where the builder was right (86 lines, no rule)

Mostly the agents trimming or normalising differently (勝ちぬき戦 かち vs
か, 一回 いちかい vs いっかい where the pool label is the stat), on-readings
in labels the agents read as dialogue, and a handful of agent slips. These
are listed in the sheet for completeness and want nothing.

## 4. What is next

1. **The owner reads `proposals.md`** and approves rules into
   `data/furigana/readings.toml`: the 3.1 set is safe as written; 3.3's
   first bin wants narrowing first; 3.4 wants a decision each. A rebuild
   (`ruby_build.py furigana`) then `--review` again and this audit's
   `diff` against the kept kana (`kana/`) shows what moved.
2. **The multi-token rule** in the builder (3.2), or `--dict full` measured
   against the 34 compounds first.
3. The rest of the follow-on group as decided: `lexicon.toml` from the
   owner's machine (so a player's build needs no SudachiPy) and the
   fugashi + UniDic `--compare` (fugashi is not installed here).
4. A pass over the **narration left plain** was not in scope: the audit
   covers composed messages only (the review list), so the 5,504 messages
   past the box were never read.

## 5. Caveats

- The agents read blind text with no speaker names and no scene, so a
  reading that depends on who speaks (a chant, a label, a name) is where
  they slip; the second pass had the contexts and the sidecar's schema but
  not the game, so a verdict is a proposal, never an approval.
- The alignment rules in `diff` were tuned on the first seven shards (a
  trial run) before the full diff; a residue of noise (an agent token that
  spans a row break, a misread surface) is in the `builder` bin, not lost.
- `n` on a line is distinct messages; `reach` counts placements. A pool
  message read once by an agent is placed in up to 40 BOSS files.
