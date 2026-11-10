// The stream a game word holds: a FILE* on the 32-bit build, and on x86-64 a
// value that fits the word whatever the C runtime's FILE* is.
//
// The game keeps each open stream in a 4-byte word (File_Slots, Cfg_Load's
// local), and the 64-bit build's narrow image holds a native address there
// only when it lies below 4 GB (common/slot32.h, docs/x64-build-design.md).
// Windows' runtime allocates a FILE on the heap, which is below 4 GB with ASLR
// off, so the pointer itself went into the word. Wine's ucrtbase keeps its
// first streams in the DLL's own data, far above 4 GB, and Low32() stopped the
// game on its first open (known-defects D412). So on x86-64 a FILE* above
// 4 GB is kept in a table of ours and the word holds the table slot's address
// (this DLL is based below 4 GB); a FILE* below 4 GB still goes into the word
// as it is, so a run on Windows holds the values it held before.
//
// The file layer of crt_rest.cpp is the only user: Crt_fopen makes the token,
// the other Crt_f* functions turn it back, Crt_fclose frees it.
#pragma once

#include <cstdint>
#include <cstdio>

namespace bof3 {

#if UINTPTR_MAX > 0xFFFFFFFFu
void* FileToken(std::FILE* stream);       // null for null; Fatal when the table is full
std::FILE* FileOfToken(void* token);
void FileTokenFree(void* token);          // before or after the fclose; a plain FILE* is left alone
#else
inline void* FileToken(std::FILE* stream) { return stream; }
inline std::FILE* FileOfToken(void* token) { return static_cast<std::FILE*>(token); }
inline void FileTokenFree(void*) {}
#endif

}  // namespace bof3
