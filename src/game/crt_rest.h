// The C runtime's entry points the game calls (step 3 of the platform round,
// docs/platform-layers-plan.md section 2.3; docs/crt-rest.md). The unit of
// work is the entry, not Microsoft's runtime behind it:
//
//   Rand          0x5B93D2  reimplemented exactly: the MSVC 6 generator, its
//                           seed a static of ours starting at 1 as the CRT's
//                           per-thread one does (_initptd 0x5BAD51);
//   Crt_sprintf   0x5B9380  ours, over exactly the conversions the game's
//                           formats use, Fatal on any other;
//   Crt_strncpy   0x5B9450, Crt_stricmp 0x5C2B40, Crt_findfirst 0x5B979A,
//   Crt_findnext  0x5B9867, and the file layer (Crt_fopen, Crt_fclose,
//   Crt_fread, Crt_fwrite, Crt_fseek, Crt_fileno, Crt_filelength, Crt_fgets):
//                           thin named functions over our toolchain's.
//
// Left for the cutover (the executable's): Crt_malloc / Crt_free, Crt_GetPtd,
// Crt_atoi, Crt_sscanf, _ftol, start-up. docs/crt-rest.md section 3 says why.
//
// Every prototype is symbols.gen.h's (symbols.toml); this header declares the
// module's inject, its fuzz and the randlog's hooks.
#pragma once

#include <cstdint>

void CrtRest_Inject();

namespace crt_rest {

// BOF3X_SHADOW=crt_rest: the start-up fuzz, crt_rest_fuzz.cpp - before
// CrtRest_Inject patches the entries.
void SelfTest();

// Rand's seed: ours while Rand is ours; the per-thread data's (Crt_GetPtd()
// + 0x14, there once the executable's C runtime has started) when
// BOF3X_ORIGINAL leaves Rand to Capcom. For the map_cells live check.
std::uint32_t* RandSeedCell();

// The fuzz's access to ours (crt_rest_fuzz.cpp): the seed itself.
std::uint32_t& RandSeed();

// The randlog (src/hook/input_script.cpp): from RandCount_Start on, every
// Rand the game draws is counted - by ours, or, with Rand left original, by a
// counting copy of Capcom's put at its entry (an instrument). RandCounting()
// is false when nothing counts (no recipe, or a traced run).
void RandCount_Start();
bool RandCounting();
std::uint32_t RandCount();

}  // namespace crt_rest
