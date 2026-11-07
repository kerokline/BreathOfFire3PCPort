// An AVI (RIFF) reader for the two movies the game ships, written from the
// public description of the container (Microsoft's "AVI RIFF File Reference":
// the `hdrl` list with `avih` and one `strl` a stream - `strh`, `strf` -
// then the `movi` list of `NNdc` / `NNwb` chunks, some inside `rec ` lists,
// and the optional `idx1` index). DIV-0205, docs/replacing-mci.md "As built".
//
// The file is read whole (both movies are under 5 MB), so the chunk list is
// taken from the `movi` list itself, which is complete whether or not `idx1`
// is present; an `idx1` is read when present and its entries counted against
// the walk, for the log. Nothing here decodes: the video decoders are
// cinepak.h and msvideo1.h, chosen by the stream's handler in movie.h.
#pragma once

#include <cstddef>
#include <cstdint>

namespace video {

using U = std::uint32_t;

struct VideoStream {
    bool present;
    char handler[5];    // strh's fccHandler, upper-cased ("CVID", "MSVC"), NUL-ended
    char compression[5];   // strf's biCompression as four characters ("cvid", "CRAM"), NUL-ended
    U width, height;    // strf's, the picture's size
    U bits;             // strf's biBitCount: 8 (palettised), 16, 24
    U rate, scale;      // strh's dwRate / dwScale: frames a second as a fraction
    U length;           // strh's dwLength, in frames
    U palette_n;        // strf's colour table entries (8-bit forms)
    U palette[256];     // 0x00RRGGBB
};

struct AudioStream {
    bool present;
    U format_tag;       // WAVEFORMAT's wFormatTag: 1 is PCM
    U channels, rate, bits, block_align;
};

// One data chunk of the `movi` list, in file order.
struct Chunk {
    U stream;           // the stream number from the chunk's id ("00dc" is 0)
    bool video;         // "dc" or "db" (compressed or uncompressed video); else a wave chunk
    U offset;           // of the chunk's data in the file
    U size;             // its size in bytes
};

struct AviFile {
    VideoStream video;
    AudioStream audio;
    U video_stream, audio_stream;   // their stream numbers
    Chunk* chunks;      // heap, n_chunks long; freed by Close
    U n_chunks;
    U video_chunks, audio_chunks;
    U idx1_entries;     // 0 when the file has no idx1
    const std::uint8_t* data;   // the file's bytes, as given to Parse
    std::size_t size;
};

// Reads the headers and lists the data chunks of the AVI in `data`. On
// failure writes the reason to `why` (at most `why_n` bytes) and returns
// false with nothing allocated. The bytes are not copied: keep them alive
// for as long as the AviFile is used.
bool Parse(AviFile* avi, const std::uint8_t* data, std::size_t size, char* why, std::size_t why_n);
void Close(AviFile* avi);

}  // namespace video
