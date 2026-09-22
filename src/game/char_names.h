// The characters' default names from a language overlay, DIV-0020
// (docs/dialogue-localisation.md section 7).
#pragma once

#include <cstdint>

// A kind-10 chunk: a count of 8, then eight NUL-terminated names, written into
// the name fields of the eight default character records New Game copies from.
// Aborts on anything else.
void CharNames_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

// A kind-11 chunk: one NUL-terminated name, at most 8 bytes with its NUL, for
// the fish merchant's slot 0x669CD8.
void CharNames_ApplyMerchant(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);
