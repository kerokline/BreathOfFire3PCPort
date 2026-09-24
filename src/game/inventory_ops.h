// Group BI of the seventh round, "inventory operations and the event-script
// bits on the way" - read, neither: the random encounter's placement on the
// field map (0x5920E0..0x592EFE, the helpers under 0x591F30, which asks
// Field_EncounterDue whether a fight fits where the party stands) and the
// battle intro's party steps (0x532550..0x532C0E, 0x52F570, 0x534880, called
// from the intro's state machine at 0x495E90). docs/inventory_ops.md.
#pragma once

void InventoryOps_Inject();
