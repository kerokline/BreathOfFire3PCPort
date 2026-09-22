// The field objects' blocked-ahead test: Field_ObjectBlockedAhead 0x518080 and
// Field_ObjectBlocked 0x519670, the map test under the first (0x518100,
// 0x518180, 0x518620, 0x518760) and the two object tests both share (0x531CF0,
// 0x531F10, with 0x531DB0). docs/field-blocked.md.
#pragma once

void FieldBlocked_Inject();
