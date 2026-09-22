// The field's mode handlers: the scenario dispatcher Field_ModeDispatch
// 0x56D690 with the two tail dispatchers after it (0x56D8B0, 0x56D920),
// scenario 16's handlers - the attract sequence's demo (0x56B2A0..0x56BCC0)
// with the CLUT fade under them (0x56C0A0, 0x56C110) - and the scenario
// call table's thunk Scenario_CallA 0x5341A0. docs/field-modes.md.
#pragma once

void FieldModes_Inject();
