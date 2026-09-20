// Item and ability names from a language overlay, DIV-0008
// (docs/dialogue-localisation.md section 7).
#pragma once

#include <cstdint>

// A kind-5 chunk: `tag` names one of the six name tables in BOF3.exe's .data,
// `names` is that table's count of 16-byte name fields. Aborts on any other
// tag or size.
void NameTables_Apply(std::uint32_t tag, const std::uint8_t* names, std::uint32_t size);
