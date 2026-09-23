// The field objects' kind handlers: the table at 0x65F5F8 by the sprite's
// pose byte +1 (all but entries 4 and 7, which are field_objects.cpp's), the
// pace table at 0x65F5DC, the loop's other three per-object calls and the
// helpers only they use. docs/object-kinds.md.
#pragma once

void ObjectKinds_Inject();
