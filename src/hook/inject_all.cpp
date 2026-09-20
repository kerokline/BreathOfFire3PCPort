#include "hook/inject_all.h"

#include "game/dat_load.h"
#include "game/file_io.h"
#include "game/save_io.h"
#include "game/gfx_frame.h"
#include "game/gfx_image.h"
#include "game/gfx_texcache.h"
#include "hook/detour.h"

namespace bof3 {

void InjectAll() {
    FileIo_Inject();
    DatLoad_Inject();
    SaveIo_Inject();
    GfxFrame_Inject();
    GfxImage_Inject();
    GfxTexCache_Inject();
    InjectReport();
}

}  // namespace bof3
