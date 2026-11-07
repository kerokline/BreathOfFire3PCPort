// See avi_reader.h. Every multi-byte field of the container is little-endian.
#include "video/avi_reader.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace video {
namespace {

U Le16(const std::uint8_t* p) { return p[0] | (U{p[1]} << 8); }
U Le32(const std::uint8_t* p) { return p[0] | (U{p[1]} << 8) | (U{p[2]} << 16) | (U{p[3]} << 24); }
bool Fcc(const std::uint8_t* p, const char* fcc) { return std::memcmp(p, fcc, 4) == 0; }

void Fail(char* why, std::size_t why_n, const char* what) { std::snprintf(why, why_n, "%s", what); }

// A chunk header at `at`: its id, its size, and where its body begins. The
// RIFF/LIST forms carry a list type as the body's first four bytes.
struct Header {
    const std::uint8_t* id;
    U size;
    const std::uint8_t* body;
    const std::uint8_t* next;   // the chunk after this one, padded to an even size
};

bool ReadHeader(const std::uint8_t* at, const std::uint8_t* end, Header* h) {
    if (end - at < 8) return false;
    h->id = at;
    h->size = Le32(at + 4);
    h->body = at + 8;
    if (static_cast<std::size_t>(end - h->body) < h->size) return false;
    h->next = h->body + h->size + (h->size & 1);
    if (h->next > end) h->next = end;
    return true;
}

// Fills the stream description from one `strl` list.
bool ReadStreamList(AviFile* avi, const std::uint8_t* at, const std::uint8_t* end, U stream, char* why, std::size_t why_n) {
    Header h;
    const std::uint8_t* strh = nullptr;
    U strh_size = 0;
    const std::uint8_t* strf = nullptr;
    U strf_size = 0;
    for (const std::uint8_t* p = at; p < end; p = h.next) {
        if (!ReadHeader(p, end, &h)) break;
        if (Fcc(h.id, "strh")) {
            strh = h.body;
            strh_size = h.size;
        } else if (Fcc(h.id, "strf")) {
            strf = h.body;
            strf_size = h.size;
        }
    }
    if (!strh || strh_size < 0x30) {
        Fail(why, why_n, "a strl list without a strh of 48 bytes or more");
        return false;
    }
    if (!strf) {
        Fail(why, why_n, "a strl list without a strf");
        return false;
    }
    if (Fcc(strh, "vids")) {
        if (avi->video.present) return true;   // the first video stream is the movie's; a second is ignored
        if (strf_size < 40) {
            Fail(why, why_n, "a video strf shorter than a BITMAPINFOHEADER");
            return false;
        }
        VideoStream& v = avi->video;
        v.present = true;
        for (int i = 0; i < 4; ++i) {
            const char c = static_cast<char>(strh[4 + i]);
            v.handler[i] = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
            v.compression[i] = static_cast<char>(strf[16 + i]);
        }
        v.handler[4] = v.compression[4] = 0;
        v.scale = Le32(strh + 0x14);
        v.rate = Le32(strh + 0x18);
        v.length = Le32(strh + 0x20);
        v.width = Le32(strf + 4);
        const U height = Le32(strf + 8);
        v.height = height & 0x80000000u ? U{0} - height : height;   // a negative height is a top-down DIB; the size is the same
        v.bits = Le16(strf + 14);
        v.palette_n = 0;
        if (v.bits <= 8) {
            U used = Le32(strf + 32);
            if (used == 0) used = 1u << v.bits;
            if (used > 256) used = 256;
            const U room = (strf_size - 40) / 4;
            if (used > room) used = room;
            for (U i = 0; i < used; ++i) {
                const std::uint8_t* q = strf + 40 + i * 4;   // RGBQUAD: blue, green, red, reserved
                v.palette[i] = (U{q[2]} << 16) | (U{q[1]} << 8) | q[0];
            }
            v.palette_n = used;
        }
        if (v.width == 0 || v.height == 0 || v.width > 4096 || v.height > 4096) {
            Fail(why, why_n, "a video stream of no size, or over 4096 a side");
            return false;
        }
        if (v.rate == 0 || v.scale == 0) {
            Fail(why, why_n, "a video stream whose dwRate or dwScale is 0");
            return false;
        }
        avi->video_stream = stream;
    } else if (Fcc(strh, "auds")) {
        if (avi->audio.present) return true;
        if (strf_size < 14) {
            Fail(why, why_n, "an audio strf shorter than a WAVEFORMAT");
            return false;
        }
        AudioStream& a = avi->audio;
        a.present = true;
        a.format_tag = Le16(strf);
        a.channels = Le16(strf + 2);
        a.rate = Le32(strf + 4);
        a.block_align = Le16(strf + 12);
        a.bits = strf_size >= 16 ? Le16(strf + 14) : 8;
        avi->audio_stream = stream;
    }
    return true;   // other stream kinds (text, midi) are skipped
}

// Counts or lists the data chunks of a `movi` body, descending into `rec `
// lists. With `out` null it counts.
U WalkMovi(const std::uint8_t* at, const std::uint8_t* end, const std::uint8_t* file, Chunk* out) {
    U n = 0;
    Header h;
    for (const std::uint8_t* p = at; p < end; p = h.next) {
        if (!ReadHeader(p, end, &h)) break;
        if (Fcc(h.id, "LIST")) {
            if (h.size >= 4 && Fcc(h.body, "rec ")) n += WalkMovi(h.body + 4, h.body + h.size, file, out ? out + n : nullptr);
            continue;
        }
        if (Fcc(h.id, "JUNK") || Fcc(h.id, "idx1")) continue;
        const std::uint8_t* id = h.id;
        const bool digits = id[0] >= '0' && id[0] <= '9' && id[1] >= '0' && id[1] <= '9';
        if (!digits) continue;
        const bool is_video = (id[2] == 'd' && (id[3] == 'c' || id[3] == 'b'));
        const bool is_wave = (id[2] == 'w' && id[3] == 'b');
        if (!is_video && !is_wave) continue;   // palette changes ("pc") and the rest are not data
        if (out) {
            out[n].stream = (id[0] - '0') * 10 + (id[1] - '0');
            out[n].video = is_video;
            out[n].offset = static_cast<U>(h.body - file);
            out[n].size = h.size;
        }
        ++n;
    }
    return n;
}

}  // namespace

bool Parse(AviFile* avi, const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n) {
    std::memset(avi, 0, sizeof *avi);
    avi->data = data;
    avi->size = size;
    if (size < 12 || !Fcc(data, "RIFF") || !Fcc(data + 8, "AVI ")) {
        Fail(why, why_n, "not a RIFF AVI file");
        return false;
    }
    U riff_size = Le32(data + 4);
    const std::uint8_t* end = data + size;
    if (riff_size + 8 < size) end = data + riff_size + 8;   // a trailer past the RIFF is not the file's
    const std::uint8_t* movi = nullptr;
    const std::uint8_t* movi_end = nullptr;
    U streams_seen = 0;
    bool hdrl = false;
    Header h;
    for (const std::uint8_t* p = data + 12; p < end; p = h.next) {
        if (!ReadHeader(p, end, &h)) break;
        if (Fcc(h.id, "LIST") && h.size >= 4) {
            if (Fcc(h.body, "hdrl")) {
                hdrl = true;
                Header l;
                for (const std::uint8_t* q = h.body + 4; q < h.body + h.size; q = l.next) {
                    if (!ReadHeader(q, h.body + h.size, &l)) break;
                    if (Fcc(l.id, "LIST") && l.size >= 4 && Fcc(l.body, "strl")) {
                        if (!ReadStreamList(avi, l.body + 4, l.body + l.size, streams_seen, why, why_n)) return false;
                        ++streams_seen;
                    }
                }
            } else if (Fcc(h.body, "movi") && !movi) {
                movi = h.body + 4;
                movi_end = h.body + h.size;
            }
        } else if (Fcc(h.id, "idx1")) {
            avi->idx1_entries = h.size / 16;
        }
    }
    if (!hdrl) {
        Fail(why, why_n, "no hdrl list");
        return false;
    }
    if (!avi->video.present) {
        Fail(why, why_n, "no video stream");
        return false;
    }
    if (!movi) {
        Fail(why, why_n, "no movi list");
        return false;
    }
    const U n = WalkMovi(movi, movi_end, data, nullptr);
    if (n == 0) {
        Fail(why, why_n, "an empty movi list");
        return false;
    }
    avi->chunks = static_cast<Chunk*>(std::calloc(n, sizeof(Chunk)));
    if (!avi->chunks) {
        Fail(why, why_n, "out of memory for the chunk list");
        return false;
    }
    avi->n_chunks = WalkMovi(movi, movi_end, data, avi->chunks);
    for (U i = 0; i < avi->n_chunks; ++i) {
        const Chunk& c = avi->chunks[i];
        if (c.video && c.stream == avi->video_stream) ++avi->video_chunks;
        else if (!c.video && avi->audio.present && c.stream == avi->audio_stream) ++avi->audio_chunks;
    }
    if (avi->video_chunks == 0) {
        Fail(why, why_n, "no video chunk in the movi list");
        Close(avi);
        return false;
    }
    return true;
}

void Close(AviFile* avi) {
    std::free(avi->chunks);
    avi->chunks = nullptr;
    avi->n_chunks = 0;
}

}  // namespace video
