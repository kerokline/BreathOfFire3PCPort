// Our code's reads of Capcom's code bytes, in either host
// (docs/state3-build.md section 2.3).
//
// Some of our implementations read bytes that live inside BOF3.exe's .text:
// an operand a divergence patched (the slide bounds DIV-0041 widens, the
// title menu's widths DIV-0014 writes, the config row's label operands), the
// target of a call a RetargetCall re-aimed (which frame or text draw a
// screen uses), or a table the compiler put in the code (a jump table's byte
// index). The brief (state3-design.md section 7) counted one such read; the
// build found about forty.
//
// Hosted, those bytes are Capcom's, mapped, and PatchBytes / RetargetCall
// write them in place: CodeBytes(va) is va. In the exe host .stub is int3,
// so the same bytes are held by a store of ours: read from the player's
// BOF3.exe (the file the engine copied .rdata and .data from) on first use,
// at a stable address, never executable, never at 0x401000.. - and
// PatchBytes / RetargetCall write the store instead of the code, so every
// reader sees exactly what it sees hosted. Nothing of ours writes .stub but
// Inject (exe_host.cpp's self-test asserts it).
//
// Outside the code range CodeBytes is the address itself in both hosts, so a
// reader whose address may be either (labels.cpp's witnesses) can use it.
#pragma once

#include <cstdint>

namespace bof3 {

// `n` bytes at `va` as this host holds them. In the exe host the first read
// of a range loads it from the player's file and logs it once.
const std::uint8_t* CodeBytes(std::uint32_t va, std::uint32_t n);

// The same, writable: for PatchBytes and RetargetCall in the exe host only.
std::uint8_t* CodeBytesForWrite(std::uint32_t va, std::uint32_t n);

// The exe host's file: the player's BOF3.exe, open for reading, and its
// .text section's file offset and size. Called once by Engine_Main before
// InjectAll.
void CodeBytes_Open(void* file, std::uint32_t text_file_offset, std::uint32_t text_raw_size);

// One log line: how many ranges and bytes the store holds.
void CodeBytes_Report(const char* when);

// Little-endian reads through CodeBytes.
inline std::uint8_t CodeByte(std::uint32_t va) { return CodeBytes(va, 1)[0]; }
std::uint16_t CodeWord(std::uint32_t va);
std::uint32_t CodeLong(std::uint32_t va);
// The target of the E8 rel32 call at `site`: site + 5 + rel32.
std::uint32_t CodeCallTarget(std::uint32_t site);

}  // namespace bof3
