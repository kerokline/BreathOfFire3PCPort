// Field-side functions reached only through pointers or left by earlier cuts:
// two party-member states (0x51BA60, 0x51BBD0), the party actions' shared
// finish (0x51DA30) and party set 5's form-0 action (0x51E910..0x51EBD0,
// 0x51F1B0), the pose-from-a-set helpers (0x589110..0x5891C0), the event
// op Ex (0x5898D0) and the encounter placement's driver with its row and
// slot pickers (0x591F30, 0x592570, 0x5925A0). docs/field_hidden.md.
#pragma once

void FieldHidden_Inject();
