#include "hook/inject_all.h"

#include "game/dat_load.h"
#include "game/file_io.h"
#include "game/save_io.h"
#include "game/gfx_frame.h"
#include "game/gfx_image.h"
#include "game/gfx_texcache.h"
#include "game/gfx_clut.h"
#include "game/gfx_flush.h"
#include "game/gfx_unpack.h"
#include "game/gfx_vram_ops.h"
#include "game/sprite_order.h"
#include "game/draw_pool.h"
#include "game/prim.h"
#include "hook/detour.h"

namespace bof3 {

void InjectAll() {
    FileIo_Inject();
    DatLoad_Inject();
    SaveIo_Inject();
    GfxFrame_Inject();
    GfxImage_Inject();
    GfxTexCache_Inject();
    GfxClut_Inject();
    GfxFlush_Inject();
    GfxUnpack_Inject();
    GfxVramOps_Inject();
    SpriteOrder_Inject();
    DrawPool_Inject();
    Prim_Inject();
    InjectReport();
}

}  // namespace bof3
