// The system message pool and its one reader, original 0x497740
// (docs/dialogue-localisation.md section 6).
#pragma once

#include <cstdint>

// DIV-0007. Moves the system pool out of the DAT arena into a buffer of its
// own, so that an area's text may run past arena offset 0x4000. Called once,
// before any DAT file is loaded.
void MsgPool_Relocate();

// True if the pool is relocated and this kind-0 chunk is a system pool, in
// which case it has been copied to the pool's buffer and must not be written
// to the arena.
bool MsgPool_TakeChunk(std::uint32_t tag, const std::uint8_t* payload, std::uint32_t size);

void MsgPool_Inject();
