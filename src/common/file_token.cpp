// file_token.h's table, x86-64 only (the 32-bit build's are inline).
#include "common/file_token.h"

#if UINTPTR_MAX > 0xFFFFFFFFu

#include <atomic>

#include "hook/log.h"

namespace bof3 {
namespace {

// The game opens at most its 16 File_Slots and one configuration file at a
// time; 64 leaves room for a caller this count missed, and a full table is
// Fatal. Atomic: the music thread reads files beside the game's.
constexpr unsigned kSlots = 64;
std::atomic<std::FILE*> g_streams[kSlots];
bool g_said;

bool IsSlot(void* token) {
    const auto p = reinterpret_cast<std::uintptr_t>(token);
    const auto lo = reinterpret_cast<std::uintptr_t>(&g_streams[0]);
    return p >= lo && p < lo + sizeof g_streams;
}

}  // namespace

void* FileToken(std::FILE* stream) {
    if (!stream) return nullptr;
    if (reinterpret_cast<std::uintptr_t>(stream) <= 0xFFFFFFFFu) return stream;
    for (auto& slot : g_streams) {
        std::FILE* empty = nullptr;
        if (slot.compare_exchange_strong(empty, stream)) {
            if (!g_said) {
                g_said = true;
                Log("file: the C runtime's FILE %p lies above 4 GB (Wine's ucrtbase); the game's words hold slots of a "
                    "table of ours at %p (known-defects D412)", static_cast<void*>(stream), static_cast<void*>(&g_streams[0]));
            }
            return &slot;
        }
    }
    Fatal("file: %u streams above 4 GB are open at once; file_token.cpp's table is full", kSlots);
}

std::FILE* FileOfToken(void* token) {
    if (!IsSlot(token)) return static_cast<std::FILE*>(token);
    std::FILE* stream = static_cast<std::atomic<std::FILE*>*>(token)->load();
    if (!stream) Fatal("file: the stream token %p names an empty slot (used after its close?)", token);
    return stream;
}

void FileTokenFree(void* token) {
    if (IsSlot(token)) static_cast<std::atomic<std::FILE*>*>(token)->store(nullptr);
}

}  // namespace bof3

#endif
