// The window text draw, original 0x5961C0: choice lists and other text drawn
// whole, not typed out (docs/dialogue-localisation.md section 6).
#pragma once

void TextImmediate_Inject();

namespace text_immediate {
extern const unsigned char*(__cdecl* g_draw_at)(int, int, int, int, const unsigned char*);   // defined in text_immediate.cpp (source-layout B0: promoted for a moved reader)
}  // namespace text_immediate
