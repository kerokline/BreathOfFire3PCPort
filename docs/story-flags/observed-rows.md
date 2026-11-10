# Story flags: the rows observed in real saves

**Status:** STABLE (measured 2026-11-09). Generated from the saves held that day by a
scratch script. Flag numbers only: no save and no dump of one.

One section per save, earliest first. Each lists the flags set in every chapter row
(block `+0x5B0 + 8c`) and in the event flags (block `+0x650`). Crafted saves are left
out: a chapter-jump save keeps its parent's flags.

| # | System | Source | Clock | Chapter | State | Area | Battles |
|---:|---|---|---|---:|---:|---:|---:|
| 1 | PC | start.DAT | 00:00 | 0 | 2 | 24 | 0 |
| 2 | PC | start_normal.DAT | 00:09 | 1 | 2 | 10 | 3 |
| 3 | US | psxrecomp US | 00:11 | 1 | 2 | 10 | 3 |
| 4 | US | RetroArch | 00:12 | 1 | 2 | 10 | 5 |
| 5 | JP | RetroArch | 00:25 | 1 | 2 | 10 | 5 |
| 6 | JP | psxrecomp JP (authentic) | 00:28 | 1 | 2 | 10 | 3 |
| 7 | US | RetroArch | 00:57 | 1 | 2 | 23 | 18 |
| 8 | JP | RetroArch | 01:38 | 1 | 2 | 90 | 9 |
| 9 | JP | sibling card | 02:19 | 1 | 2 | 90 | 27 |
| 10 | JP | sibling card | 02:35 | 1 | 2 | 23 | 38 |
| 11 | US | RetroArch | 04:59 | 2 | 2 | 90 | 186 |
| 12 | JP | psxrecomp JP (authentic) | 05:19 | 2 | 2 | 27 | 112 |
| 13 | JP | sibling card | 07:41 | 6 | 2 | 83 | 244 |
| 14 | JP | sibling card | 07:41 | 6 | 2 | 83 | 244 |
| 15 | JP | sibling card | 07:41 | 6 | 2 | 83 | 244 |
| 16 | JP | sibling card | 07:41 | 6 | 2 | 83 | 244 |
| 17 | JP | sibling card | 07:41 | 6 | 2 | 83 | 244 |
| 18 | US | RetroArch | 08:17 | 4 | 2 | 90 | 307 |
| 19 | JP | psxrecomp JP (authentic) | 12:48 | 5 | 2 | 90 | 348 |
| 20 | US | RetroArch | 42:37 | 14 | 2 | 141 | 1019 |
| 21 | US | sibling card | 42:44 | 10 | 2 | 120 | 1020 |
| 22 | US | RetroArch | 43:25 | 15 | 2 | 154 | 1051 |

## Save 1: PC, start.DAT, 00:00, chapter 0

- chapter 0 row: 3
- event flags: 3 25 27

## Save 2: PC, start_normal.DAT, 00:09, chapter 1

- chapter 0 row: 0..7 10
- chapter 1 row: 0..1
- event flags: 3 25 27

## Save 3: US, psxrecomp US, 00:11, chapter 1

- chapter 0 row: 0..7 10
- chapter 1 row: 0..1
- event flags: 3 25 27

## Save 4: US, RetroArch, 00:12, chapter 1

- chapter 0 row: 0..10
- chapter 1 row: 0..1
- event flags: 3 25 27

## Save 5: JP, RetroArch, 00:25, chapter 1

- chapter 0 row: 0..10
- chapter 1 row: 0..1
- event flags: 3 25 27

## Save 6: JP, psxrecomp JP (authentic), 00:28, chapter 1

- chapter 0 row: 0..7 10
- chapter 1 row: 0..1
- event flags: 3 25 27

## Save 7: US, RetroArch, 00:57, chapter 1

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 24..28
- event flags: 3 25 27 32

## Save 8: JP, RetroArch, 01:38, chapter 1

- chapter 0 row: 0..10
- chapter 1 row: 0..7 25..28
- event flags: 3 25 27

## Save 9: JP, sibling card, 02:19, chapter 1

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 24..28
- event flags: 3 25 27 32 119

## Save 10: JP, sibling card, 02:35, chapter 1

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 9..10 24..28 57 63
- event flags: 3 25 27 32

## Save 11: US, RetroArch, 04:59, chapter 2

- chapter 0 row: 0..10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 4..8 12 15..25 35..39 46..51 54..55 62..63
- event flags: 3 25 27 32

## Save 12: JP, psxrecomp JP (authentic), 05:19, chapter 2

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 4..8 12 15..24 35..40 46..51 62..63
- event flags: 3 25 27 32

## Save 13: JP, sibling card, 07:41, chapter 6

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 41 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 1..8 10 12
- chapter 5 row: 0..2 5..6 8..12 14..20 23..26 29..35 40..54 59..62
- chapter 6 row: 0..1
- event flags: 3 13 25 27 32

## Save 14: JP, sibling card, 07:41, chapter 6

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 41 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 1..8 10 12
- chapter 5 row: 0..2 5..6 8..12 14..20 23..26 29..35 40..54 59..62
- chapter 6 row: 0..1
- event flags: 3 13 25 27 32

## Save 15: JP, sibling card, 07:41, chapter 6

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 41 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 1..8 10 12
- chapter 5 row: 0..2 5..6 8..12 14..20 23..26 29..35 40..54 59..62
- chapter 6 row: 0..1
- event flags: 3 13 25 27 32

## Save 16: JP, sibling card, 07:41, chapter 6

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 41 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 1..8 10 12
- chapter 5 row: 0..2 5..6 8..12 14..20 23..26 29..35 40..54 59..62
- chapter 6 row: 0..1
- event flags: 3 13 25 27 32

## Save 17: JP, sibling card, 07:41, chapter 6

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 41 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 1..8 10 12
- chapter 5 row: 0..2 5..6 8..12 14..20 23..26 29..35 40..54 59..62
- chapter 6 row: 0..1
- event flags: 3 13 25 27 32

## Save 18: US, RetroArch, 08:17, chapter 4

- chapter 0 row: 0..10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 0..1
- event flags: 3 25 27 32 119

## Save 19: JP, psxrecomp JP (authentic), 12:48, chapter 5

- chapter 0 row: 0..7 10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..40 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 0..8 10 12
- chapter 5 row: 0..2 5..6 8..9
- event flags: 3 13 25 27 32 119

## Save 20: US, RetroArch, 42:37, chapter 14

- chapter 0 row: 0..10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 0..8 10 12
- chapter 5 row: 0..2 5..6 8..12 14..20 23..26 29..35 40..55 59..62
- chapter 6 row: 0..1 3..5 7..22 26..31 33..40 42 44 59..63
- chapter 7 row: 0..5 7..9 11..12
- chapter 8 row: 0..22 24..26
- chapter 9 row: 0..8 11..19 21..30 32..36 38..39 44 46..50 52 54..58 61..63
- chapter 10 row: 0 2 4..11 13..26 32..44 46..47 52..61
- chapter 11 row: 0..2 4..12 14..19
- chapter 12 row: 0..2 20..25 27 36..45 47..48 52 63
- chapter 13 row: 0..31
- chapter 14 row: 0 4 6..24 53
- event flags: 3..7 13..15 24..27 29 32 35 41..42 45..52 54..64 67..69 71 73 75 77 79..82 84 89..91 93 96..101 103..104 106..107 109..115 121 126..127 131..133 143..145 147..149 152 245..255

## Save 21: US, sibling card, 42:44, chapter 10

- chapter 0 row: 0..10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 0..8 10 12
- chapter 5 row: 0..2 5..6 8..12 14..20 23..26 29..35 40..55 59..62
- chapter 6 row: 0..1 3..5 7..22 26..31 33..40 42 44 59..63
- chapter 7 row: 0..5 7..9 11..12
- chapter 8 row: 0..22 24..26
- chapter 9 row: 0..8 11..19 21..30 32..36 38..39 44 46..50 52 54..58 61..63
- chapter 10 row: 0 2 4..11 13..26 32..44 46..47 52..58
- chapter 11 row: 0..2 4..12 14..19
- chapter 12 row: 0..2 20..25 27 36..45 47..48 52 63
- chapter 13 row: 0..31
- chapter 14 row: 0 4 6..24 53
- event flags: 3..7 13..15 24..27 29 32 35 41..42 45..52 54..64 67..69 71 73 75 77 79..82 84 89..91 93 96..101 103..104 106..107 109..115 121 126..127 131..133 143..145 147..149 152 245..255

## Save 22: US, RetroArch, 43:25, chapter 15

- chapter 0 row: 0..10
- chapter 1 row: 0..7 9..13 15 18 20..22 24..29 57..60 62
- chapter 2 row: 0..1 3..8 12 15..32 35..39 46..51 54..56 62..63
- chapter 3 row: 1..30
- chapter 4 row: 0..8 10 12
- chapter 5 row: 0..2 5..6 8..12 14..20 23..26 29..35 40..55 59..62
- chapter 6 row: 0..1 3..5 7..22 26..31 33..40 42 44 59..63
- chapter 7 row: 0..5 7..9 11..12
- chapter 8 row: 0..22 24..26
- chapter 9 row: 0..8 11..19 21..30 32..36 38..39 44 46..50 52 54..58 61..63
- chapter 10 row: 0 2 4..11 13..26 32..44 46..47 52..61
- chapter 11 row: 0..2 4..12 14..19
- chapter 12 row: 0..2 20..25 27 36..45 47..48 52 63
- chapter 13 row: 0..31
- chapter 14 row: 0 4 6..31 48..49 53
- event flags: 3..7 13..15 24..27 29 32 35 41..42 45..52 54..64 67..69 71 73 75 77 79..82 84 89..91 93 96..101 103..104 106..107 109..115 121..122 126..127 131..133 143..145 147..149 152 245..255
