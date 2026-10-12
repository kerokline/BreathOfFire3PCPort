// The DAT container loader, original 0x454590 (docs/asset-loading-path.md).
#pragma once

#include "game/dat_cache.h"

void DatLoad_Inject();

// DIVERGENCE TG-5 (docs/launcher-suites.md section 9): DAT\<name>'s optional
// layers (BOF3X_OPT), each by LoadDatFile's own precedence (the cache's
// opt\<layer>\dat\<name>, or the install's DAT\<layer>.<name>), their chunks
// handed to `fn` instead of LoadDatFile's switch. Snd_LoadBankFile takes the
// kind-2 ones, so that a layer's bank replaces the container's as it does
// when LoadDatFile loads it. No layer named: nothing.
void DatLoad_OptLayerChunks(const char* name, dat_cache::ChunkFn fn);
