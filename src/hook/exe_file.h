// The player's BOF3.exe as a file, in the exe host (docs/linux-round.md, "R3:
// what a Linux start-up needs from the PE host"): opened read-only once by the
// start-up, its section table read from the file's own headers, and bytes at a
// VA of .text, .rdata or .data read back on demand. The PE file format and a
// file read, not a Windows call: portable, through the seam's files
// (os/path_file.h). ExeFile_Present and ExeFile_Read (exe_host.h) are built on
// it; the start-up (exe_host.cpp OpenExe and CopyData) opens it and reads
// .data through it.
#pragma once

#include <cstdint>
#include <filesystem>

namespace bof3::exe_file {

// A section of the file: its VA (0x400000 + its RVA), virtual size, file
// offset and initialised bytes in the file.
struct Section {
    std::uint32_t va = 0, vsize = 0, raw = 0, raw_size = 0;
};

// Opens `path` for reading for the process's life. False when it does not
// open (the caller says why, with os::LastError()); a Fatal when it is open
// already.
bool Open(const std::filesystem::path& path);

// Whether Open succeeded.
bool IsOpen();

// Reads the section table from the file's DOS and PE32 headers and keeps
// .text, .rdata and .data. A Fatal for a file that is not a 32-bit PE based
// at 0x400000, a short read, or a missing section.
void ReadSections();

// The three sections; ReadSections must have run.
const Section& Text();
const Section& Rdata();
const Section& Data();

// `n` bytes at file offset `offset` into `to`; a Fatal naming `what` when the
// read comes up short.
void ReadAt(std::uint32_t offset, void* to, std::uint32_t n, const char* what);

}  // namespace bof3::exe_file
