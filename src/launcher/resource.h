// Control IDs for launcher.rc. Nothing here is a game address; these are ours.
#pragma once

#define IDD_CONFIG       100
#define IDI_BOF3X          1   // the launcher's icon (data/icon/bof3x.ico)

// The launcher's window (docs/launcher-settings.md section 2a): a title page
// and Configure, whose tabs are these sub-pages, each a child dialog.
#define IDD_PAGE_SETTINGS 110
#define IDD_PAGE_CONTROLS 111
#define IDD_PAGE_MUSIC    112
#define IDD_PAGE_GAME     113
#define IDD_PAGE_VERSIONS 114   // Game Options: graphics and audio
#define IDD_PAGE_DATA     115
#define IDD_PAGE_CONTROLS2 116  // the Controls tab's second sub-page
#define IDD_PAGE_VERSIONS2 117  // Game Options: data
#define IDD_PAGE_MODS     118   // the Mods tab (docs/mods.md; side-loaded files later, IDEAS I33)
#define IDD_PAGE_EXTRA    119   // the Mods tab's second sub-page, Extras (ruby_ui=; owner, 2026-10-22)
#define IDR_BACKGROUND    120   // docs/images/launcherBackground.png, the title page's picture
#define IDD_PAGE_PRESET   121   // the Settings tab's second sub-page: the preset (preset=; docs/launcher-settings.md)
#define IDD_PAGE_GAME2    122   // The Cheats sub-tab's Page 2 (DIV-0187, DIV-0188; the owner's split, 2026-10-30)
#define IDC_CONFIGURE   1020
#define IDC_BACK        1021
#define IDC_TAB0        1022    // the six tabs, IDC_TAB0 + page
#define IDC_SUB0        1032    // a tab's sub-tabs, IDC_SUB0 + place (four)
#define IDC_SHEET0      1036    // a sub-tab's pages, the third row: IDC_SHEET0 + place (two)

#define IDC_LANGUAGE    1001
#define IDC_LANGNOTE    1002
#define IDC_FILTER      1003
#define IDC_DISPLAY     1004
#define IDC_RESOLUTION  1005
#define IDC_RENDERER    1006    // retired 2026-10-24 (issue 124): no box; kept so the id is not reused
#define IDC_SHOW        1007
#define IDC_BACKGROUND  1008
#define IDC_WIDE        1009
#define IDC_SNAP        1010
#define IDC_LOOKOPTIONS 1011
// The row above the text (docs/mods.md 5): ruby= in bof3x.ini.
#define IDC_RUBY        1012
#define IDC_RUBYLABEL   1013    // its label: both hidden unless ruby_ui=1 (Mods, Extras)
// The music (DIV-0087).
#define IDC_MUSIC       1014
#define IDC_CACHE       1015
#define IDC_CACHEBROWSE 1016
#define IDC_CACHENOTE   1017
// The three suites (docs/launcher-suites.md; until 2026-10-12 the PSP extras'
// boxes, 1018..1024). Suite i's controls are IDC_SUITE0 + kSuiteStride * i
// plus: the label, the PlayStation radio, the PSP radio, four member lines,
// the reason; the data suite also two more member lines at +8 and +9 (its
// two code members, DIV-0108).
#define IDC_SUITENOTE   1018
#define IDC_SUITEGROUP  1019
#define IDC_SUITE0      1040
#define kSuiteStride    10
#define kSuiteLabel     0
#define kSuitePsx       1
#define kSuitePsp       2
#define kSuiteMember0   3
#define kSuiteMembers   6
#define kSuiteWhy       7
#define kSuiteMember4   8

// The SatPixie look's options dialog (DIV-0043).
#define IDD_SATPIXIE     101
#define IDC_SP_MODULATE 1100
#define IDC_SP_GAMMA    1101
#define IDC_SP_CHROMA   1102
#define IDC_SP_BLURX    1103
#define IDC_SP_BLURY    1104
#define IDC_SP_MODULATE_V 1110
#define IDC_SP_GAMMA_V    1111
#define IDC_SP_CHROMA_V   1112
#define IDC_SP_BLURX_V    1113
#define IDC_SP_BLURY_V    1114
#define IDC_SP_NATURAL  1120
#define IDC_SP_GHOSTING 1121
#define IDC_SP_CHROMAON 1122
#define IDC_SP_VIGNETTE 1123
#define IDC_SP_VIG43    1124
#define IDC_SP_WIGGLE   1125
#define IDC_SP_SCANROLL 1126
#define IDC_SP_OVERSCAN 1127
#define IDC_SP_MASK     1128
#define IDC_SP_DEFAULTS 1129

// The Game Options page's cheats (DIV-0045, DIV-0046).
#define IDC_CH_EXP      1200
#define IDC_CH_ZENNY    1201
#define IDC_CH_EXP_V    1210
#define IDC_CH_ZENNY_V  1211
#define IDC_CH_STEAL    1220
#define IDC_CH_DEFAULTS 1221
#define IDC_CH_INFO     1222  // Every enemy's HP gauge and stats shown: cheat.enemy_info= (DIV-0187); Cheats' Page 2
#define IDC_CH_NOFAIL   1223  // The minigames cannot be failed: cheat.no_fail= (DIV-0188); Cheats' Page 2
#define IDC_CH_DEFAULTS2 1224 // Page 2's Defaults: every cheat back to its default, both pages

// The Controls page (DIV-0050, docs/controls.md section 4.2): fourteen rows,
// one an action, each with two key boxes and two pad boxes - the ids are
// base + row.
#define IDC_CT_KEY1     1300
#define IDC_CT_KEY2     1320
#define IDC_CT_PAD1     1340
#define IDC_CT_PAD2     1360
#define IDC_CT_LAYOUT   1380
#define IDC_CT_DEFAULTS 1381

// The capture over a Controls cell: press a key or a pad input.
#define IDD_CAPTURE      104
#define IDC_CP_PROMPT   1400
#define IDC_CP_CLEAR    1401

// The music player (docs/music-player.md). The three source buttons are
// consecutive for CheckRadioButton.
#define IDC_MUSICPLAYER 1030
#define IDD_MUSIC        105
#define IDC_MP_PSX      1500
#define IDC_MP_PSP      1501
#define IDC_MP_PC       1502
#define IDC_MP_SONG     1503
#define IDC_MP_PLAY     1504
#define IDC_MP_STOP     1505
#define IDC_MP_NOTE     1506
#define IDC_MP_STATUS   1507
#define IDC_MP_CHORD    1508   // DIV-0113: the battle chord as the PSP plays it (opt= psp-chord-one-tone)
#define IDC_MP_CHORD_NOTE 1509

// The Sources page (docs/install-sources.md), and its button on the settings.
#define IDC_SOURCES     1031
#define IDD_SOURCES      106
#define IDC_SO_INTRO    1600
#define IDC_SO_LIST     1601
#define IDC_SO_REMOVE   1602
#define IDC_SO_PATH     1603
#define IDC_SO_FILE     1604
#define IDC_SO_FOLDER   1605
#define IDC_SO_ADD      1606
#define IDC_SO_FFNOTE   1607
#define IDC_SO_FFLOCATE 1608
#define IDC_SO_FFGET    1609
#define IDC_SO_PROGRESS 1610
#define IDC_SO_STATUS   1611
#define IDC_SO_LOG      1612
#define IDC_SO_HOME     1613
#define IDC_SO_BUILD    1614

// The Mods tab (docs/mods.md; src/launcher/sources_dialog.cpp ModsPageProc).
#define IDC_MOD_LIST    1700
#define IDC_MOD_REMOVE  1701
#define IDC_MOD_PATH    1702
#define IDC_MOD_BROWSE  1703
#define IDC_MOD_ADD     1704
#define IDC_MOD_STATUS  1705
#define IDC_MOD_LOG     1706
// The Mods tab's Extras sub-page (IDD_PAGE_EXTRA).
#define IDC_EXTRA_RUBY     1720   // Show Row above the text on Settings: ruby_ui=
#define IDC_EXTRA_RUBYNOTE 1721
#define IDC_EXTRA_TEXTSPEED 1722  // Faster text: text_speed= (DIV-0158)
#define IDC_EXTRA_TEXTNOTE  1723
#define IDC_EXTRA_LEARNBLUE 1724  // A skill to learn in blue: unlearned_blue= (DIV-0179)
#define IDC_EXTRA_LEARNNOTE 1725

// The preset (launcher/config.h, docs/launcher-settings.md "preset="): its box,
// its sentence, the list of what it changes, and the line under Settings that
// says why a setting is grey.
#define IDC_PRESET       1730
#define IDC_PRESETNOTE   1731
#define IDC_PRESETLIST   1732
#define IDC_PRESETGREY   1733
