#include "game/text_pairs.h"

#include <cstring>

#include "bof3/symbols.gen.h"
#include "hook/log.h"

namespace {

std::uint16_t* g_pairs;       // 2 * g_count glyph indices, Crt_malloc'd
std::uint32_t g_first;        // glyph index of pair 0
std::uint32_t g_count;

}  // namespace

void TextPairs_Apply(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size) {
    if (size < 2) bof3::Fatal("DIV-0057: pair table of %u bytes", (unsigned)size);
    std::uint16_t count;
    std::memcpy(&count, payload, sizeof count);
    if (size != 2u + 4u * count) bof3::Fatal("DIV-0057: pair table says %u pairs in %u bytes", (unsigned)count, (unsigned)size);
    if (g_pairs) Crt_free(g_pairs);
    g_pairs = static_cast<std::uint16_t*>(Crt_malloc(4u * count + 4u));
    std::memcpy(g_pairs, payload + 2, 4u * count);
    g_first = tag;
    g_count = count;
    bof3::Log("DIV-0057: %u pair codes from glyph 0x%X", (unsigned)count, (unsigned)tag);
}

bool TextPair_Of(std::uint32_t glyph, std::uint32_t* first, std::uint32_t* second) {
    if (!g_count || glyph < g_first || glyph - g_first >= g_count) return false;
    *first = g_pairs[2 * (glyph - g_first)];
    *second = g_pairs[2 * (glyph - g_first) + 1];
    return true;
}

const std::uint8_t* TextPairs_Expand(const std::uint8_t* text, std::uint8_t* buf, std::uint32_t cap) {
    if (!g_count) return text;
    bool any = false;
    for (const std::uint8_t* p = text; *p; p += (*p & 0x80) && p[1] ? 2 : 1) any = any || TextPair_At(p);
    if (!any) return text;
    std::uint32_t n = 0;
    for (const std::uint8_t* p = text; *p;) {
        std::uint32_t a, b;
        if ((*p & 0x80) && p[1] && TextPair_Of((static_cast<std::uint32_t>(*p & 0x7F) << 8) + p[1], &a, &b)) {
            if (n + 5 > cap) bof3::Fatal("DIV-0057: an expanded name is over %u bytes", (unsigned)cap - 1);
            buf[n++] = static_cast<std::uint8_t>(0x80 | (a >> 8));
            buf[n++] = static_cast<std::uint8_t>(a);
            buf[n++] = static_cast<std::uint8_t>(0x80 | (b >> 8));
            buf[n++] = static_cast<std::uint8_t>(b);
            p += 2;
        } else {
            const std::uint32_t len = (*p & 0x80) && p[1] ? 2 : 1;
            if (n + len + 1 > cap) bof3::Fatal("DIV-0057: an expanded name is over %u bytes", (unsigned)cap - 1);
            for (std::uint32_t k = 0; k < len; ++k) buf[n++] = *p++;
        }
    }
    buf[n] = 0;
    return buf;
}

bool TextPair_At(const std::uint8_t* text) {
    if (!g_count || !(text[0] & 0x80)) return false;
    std::uint32_t a, b;
    return TextPair_Of((static_cast<std::uint32_t>(text[0] & 0x7F) << 8) + text[1], &a, &b);
}
