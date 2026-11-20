// The Win32 Jukebox's samples for one song, without its window and without
// waveOut (docs/launcher-portable.md, stage P5; CMakeLists.txt here says how to
// build and run it). The Win32 player's own translation unit is included as it
// is, so its Stream classes - Mp3Stream, SeqStream, unchanged - are the
// producer, and the blocks are its Output's: each one Render of 2048 frames
// (Output::Fill, kFrames), from the first block after Set, as waveOut got them.
//
//   old_pcm seq <song .DAT> <its bank\ folder, with the separator> <frames> <out file>
//   old_pcm mp3 <file> <song number> <loops 0|1> <frames> <out file>
//
// `frames` is rounded up to whole blocks. The file is 16-bit stereo at
// 44,100 Hz with no header: game data, kept out of the tree (CLAUDE.md rule 1).
#include "launcher/music_player.cpp"

#include <cstdlib>

namespace bof3x {
namespace {

int Usage() {
    std::fwprintf(stderr, L"usage: old_pcm seq <song .DAT> <bank folder\\> <frames> <out file>\n"
                          L"       old_pcm mp3 <file> <song number> <loops 0|1> <frames> <out file>\n");
    return 2;
}

int Write(Stream& stream, unsigned frames, const wchar_t* out_path, const char* how) {
    std::FILE* out = _wfopen(out_path, L"wb");
    if (!out) {
        std::fwprintf(stderr, L"old_pcm: cannot write %ls\n", out_path);
        return 1;
    }
    constexpr int kBlock = 2048;   // music_player.cpp Output::kFrames
    std::vector<std::int16_t> block(2 * kBlock);
    unsigned written = 0;
    bool ended_seen = false;
    unsigned ended_at = 0;
    while (written < frames) {
        stream.Render(block.data(), kBlock);
        std::fwrite(block.data(), sizeof(std::int16_t), block.size(), out);
        written += kBlock;
        if (!ended_seen && stream.Ended()) {
            ended_seen = true;
            ended_at = written;
        }
    }
    std::fclose(out);
    if (ended_seen) std::printf("old_pcm: %u frames (%s), ended after the block ending at frame %u\n", written, how, ended_at);
    else std::printf("old_pcm: %u frames (%s)\n", written, how);
    return 0;
}

int Main(int argc, wchar_t** argv) {
    psx::SetMusicAbortHook(MusicAbort);
    if (argc == 6 && !std::wcscmp(argv[1], L"seq")) {
        SeqStream s(argv[2], argv[3]);
        return Write(s, static_cast<unsigned>(std::wcstoul(argv[4], nullptr, 10)), argv[5], "sequenced");
    }
    if (argc == 7 && !std::wcscmp(argv[1], L"mp3")) {
        Mp3Stream s(argv[2], static_cast<unsigned>(std::wcstoul(argv[3], nullptr, 10)), argv[4][0] == L'1');
        return Write(s, static_cast<unsigned>(std::wcstoul(argv[5], nullptr, 10)), argv[6],
                     s.Measured() ? "MP3, its measured loop" : "MP3 or XA clip, no loop row");
    }
    return Usage();
}

}  // namespace
}  // namespace bof3x

int wmain(int argc, wchar_t** argv) { return bof3x::Main(argc, argv); }
