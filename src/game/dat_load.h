// The DAT container loader, original 0x454590 (docs/asset-loading-path.md).
#pragma once

#include "game/dat_cache.h"

void DatLoad_Inject();

// DIVERGENCE DIV-0113 (docs/launcher-suites.md section 9): DAT\<name>'s optional
// layers (BOF3X_OPT), each by LoadDatFile's own precedence (the cache's
// opt\<layer>\dat\<name>, or the install's DAT\<layer>.<name>), their chunks
// handed to `fn` instead of LoadDatFile's switch. Snd_LoadBankFile takes the
// kind-2 ones, so that a layer's bank replaces the container's as it does
// when LoadDatFile loads it. No layer named: nothing.
void DatLoad_OptLayerChunks(const char* name, dat_cache::ChunkFn fn);

// DIVERGENCE (the reading row; numbered at the merge, docs/reading-row.md 3):
// the side chunks of kind 18 (docs/reading-row.md 1.1), held in memory of
// ours keyed by (tag, half) - tag 0 the area's script block, tag 0x4000 the
// system pool with its half 0 or 1. Only an optional layer (DIV-0086)
// carries one; with none loaded nothing below answers anything.
//
// The composed message for `slot` of (tag, half), or null: no side chunk for
// it, offset 0 for the slot, a slot past the chunk's table, or a block that is
// no longer the one the side chunk was loaded over (the hash of its slot
// table, taken when the chunk landed, differs - a quickload into another
// area). The block is MessagePools + 0 for tag 0, the system pool's half
// (Msg_SystemPtr's header dword `half`) for 0x4000.
const unsigned char* DatLoad_ReadingRow(std::uint32_t tag, std::uint32_t half, std::uint32_t slot);
// True when `p` lies among the composed messages of a held side chunk (or of
// the one it replaced, kept one load longer so an open box never walks freed
// memory): the message the box's base points at is annotated.
bool DatLoad_ReadingRowHolds(const unsigned char* p);
