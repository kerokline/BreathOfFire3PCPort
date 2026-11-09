// The file layer: a 16-slot table of CRT streams addressed by slot index. The
// slot index is the "handle" the asset loaders pass around.
//
// The streams in File_Slots belong to the MSVC6 CRT linked into BOF3.exe, not
// to ours. Until the whole layer is ours, anything that touches one goes
// through the original CRT (Crt_*), never <cstdio>.
#include "game/file_io.h"

#include <windows.h>

#include "bof3/symbols.gen.h"
#include "hook/detour.h"
#include "hook/log.h"

void FileIo_Inject() {
}
