// DIVERGENCE DIV-0089 (docs/cache-read.md): the importer's cache read in place
// of the install's DAT\ and SND\ where it holds the file, the install's files
// for everything it lacks. BOF3X_CACHE names the cache (DIV-0087's root, the
// one the music reads); BOF3X_CACHE_DATA=0 leaves it to the music alone.
//
// Three readers use it (the only three users of the exe's "DAT\%s" 0x652894
// and "SND\%s.DAT" 0x666F9C formats, byte search of BOF3.exe 2026-10-10):
//   - LoadDatFile 0x454590 (dat_load.cpp): the shipped container DAT\NAME is,
//     when the cache holds it whole, base\dat\NAME.DAT and loc\zh-CN\dat\NAME.DAT
//     walked chunk by chunk in the PC's slot order (the cache's manifest.toml
//     records it); the language overlay and each optional layer are the
//     cache's loc\<tag>\dat\NAME.DAT / opt\<layer>\dat\NAME.DAT when the cache
//     has that layer at all - as `importer.py install` would have replaced
//     the install's DAT\<layer>.* with the cache's - else the install's.
//   - Snd_LoadBankFile 0x454770 (save_menu.cpp): the same shipped container,
//     its kind-2 chunks.
//   - Sound_LoadStream 0x587910 (save_menu.cpp): base\snd\NAME.DAT when the
//     cache has it, else SND\NAME.DAT.
// Configured at DatLoad_Inject (the root, the manifest, every held container
// checked against it); armed after every module's self-test. Not armed - no
// BOF3X_CACHE, BOF3X_CACHE_DATA=0, or before the arming - every call here
// answers "the install's", and the three readers do exactly what they did.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace dat_cache {

// One chunk header as LoadDatFile reads it (docs/DAT_CONTAINER.md).
struct Chunk {
    std::int8_t kind;
    std::uint8_t pad[3];
    std::uint32_t tag;
    std::int32_t size;
    std::uint32_t unread;
};
static_assert(sizeof(Chunk) == 0x10);

using ChunkFn = void (*)(void* ctx, const Chunk& header, std::uint8_t* payload);

// DatLoad_Inject, before the optional layers are read: BOF3X_CACHE_DATA
// (unset or 1: on; 0: off; anything else fatal) and BOF3X_CACHE (unset: no
// cache; not a directory, or too long for its longest path: fatal). With a
// cache: its manifest.toml's [cache] assets parsed into the container index
// (a malformed row is fatal), each container's layer files looked for, and
// every container the cache holds whole has its chunk counts checked against
// the manifest (a mismatch is fatal, naming the file). No manifest: no
// container from the cache, its layers and base\snd still read.
void Configure();

// After every module's self-test: from here the readers use the cache.
void Arm();

// The cache's root when configured, else nullptr (armed or not).
const char* Root();

// Armed: the readers take the cache's files from here on.
bool Armed();

// Configured (armed or not) and <root>\<kind>\<layer>\dat holds a .DAT:
// `kind` "loc" (a language tag) or "opt" (an optional layer). The question
// `importer.py install` answers by copying the layer: when the cache has the
// layer, the cache's layer is the one played, whole.
bool HasLayer(const char* kind, const char* layer);

// Where one overlay of DAT\NAME comes from - the precedence alone, so that the
// self-test can hold it to `install`'s:
//   not armed, or the cache lacks the layer: the install's DAT\<layer>.NAME;
//   the cache has the layer: its file, or none at all when the layer has no
//   file for NAME (install would have removed a stale DAT\<layer>.NAME).
enum class From { kInstall, kCache, kNone };
From PickOverlay(bool armed, bool cache_has_layer, bool cache_has_file);

// <root>\<kind>\<layer>\dat\<name> into `out`; fatal when it does not fit.
void LayerPath(char* out, std::size_t cap, const char* kind, const char* layer, const char* name);

// The shipped container DAT\<name> from the cache: when armed and the cache
// holds it whole, each chunk handed to `fn` in the PC's order - the base and
// loc\zh-CN files merged by the manifest's slots, then the enemy-name chunks
// that follow the zh file's slots - and true. False, with nothing read, when
// not armed or the cache does not hold it whole: the caller reads DAT\<name>.
bool WalkShipped(const char* name, ChunkFn fn, void* ctx);

// Sound_LoadStream's file: when armed and <root>\base\snd\<name>.DAT exists,
// its path into `out` and true; false otherwise (the caller formats SND\).
bool SoundPath(const char* name, char* out, std::size_t cap);

// --- the self-tests' seam ----------------------------------------------------

// The file layer the cache is read through: the game's (File_Open ... Crt_free)
// unless a self-test stands it in. `count` lists a file's chunk kinds (the
// start-up check); `layer_dir` answers HasLayer for a directory path.
struct Io {
    bool (*exists)(const char* path);
    bool (*layer_dir)(const char* dir);
    int (__cdecl* open)(const char* path, int, int);
    int (__cdecl* size)(int handle);
    unsigned (__cdecl* read)(int handle, void* dst, unsigned size);
    void (__cdecl* close)(int handle);
    void* (__cdecl* malloc)(unsigned size);
    void (__cdecl* free)(void* p);
    // The kinds of `path`'s chunks in order into kinds[0..cap); the count, or
    // -1 when a chunk runs past the file's end or the file does not open.
    int (*count)(const char* path, std::int8_t* kinds, int cap);
};

// A synthetic cache in place of the configured one, armed: `root` (no file
// is read through Win32), `manifest` the text of a manifest.toml. Returns
// false with `error` set when the manifest or the check refuses it (the same
// code Configure runs, which is fatal instead). TestEnd puts the game's back.
bool TestBegin(const char* root, const char* manifest, const Io& io, char* error, std::size_t cap);
void TestEnd();
// The game's file layer, for a stand-in that forwards some calls.
Io GameIo();
// Arm or disarm the synthetic cache (between TestBegin and TestEnd).
void TestSetArmed(bool armed);

// A self-test's in-memory files. A container's chunks get payloads of `size`
// bytes of `fill`.
struct TestChunk {
    std::int8_t kind;
    std::uint32_t tag;
    std::int32_t size;
    std::uint8_t fill;
};
std::vector<std::uint8_t> TestContainer(const std::vector<TestChunk>& chunks);
struct TestFile {
    std::string path;
    std::vector<std::uint8_t> bytes;
};
// An Io over `files` (paths compared case-insensitively; a directory is a
// layer when a file lies under it; the C++ heap for the buffers), appending
// every open's path to `opened`. One at a time.
Io TestIo(const std::vector<TestFile>* files, std::vector<std::string>* opened);

// BOF3X_SHADOW=dat_cache: the manifest's parse and the start-up check on
// synthetic manifests and containers, held to what tools/importer.py writes,
// with a control per rule (a parse or check that ignores it, which the test
// must refuse). The walk through LoadDatFile is dat_load.cpp's test, the two
// sound readers save_menu.cpp's.
void SelfTest();

}  // namespace dat_cache
