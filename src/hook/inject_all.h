#pragma once

namespace bof3 {
// Calls every module's <Module>_Inject(). The table of addresses is NOT here -
// each module owns its own, next to its code. This file shrinking to nothing is
// what the end of the hybrid period looks like.
void InjectAll();
}  // namespace bof3
