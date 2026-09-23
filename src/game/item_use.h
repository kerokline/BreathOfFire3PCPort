// The field menu's item effects: the dispatcher ItemUse_Dispatch 0x497680,
// its 33 handlers 0x496CC0..0x497670 (ItemUse_Handlers), the character-stat
// helpers under them (0x590CE0..0x590F60), the message box's two commits
// behind the area descriptors (0x4981C0, 0x4983C0), the sixteen system choice
// handlers (0x498AD0..0x498D00) and the pad auto-repeat 0x461EB0.
// docs/item-use.md.
#pragma once

void ItemUse_Inject();
