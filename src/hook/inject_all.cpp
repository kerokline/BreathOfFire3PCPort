#include "hook/inject_all.h"

#include "game/config_text.h"
#include "game/dat_load.h"
#include "game/file_io.h"
#include "game/game_clock.h"
#include "game/save_io.h"
#include "game/gfx_frame.h"
#include "game/gfx_image.h"
#include "game/gfx_sprite_uv.h"
#include "game/gfx_texcache.h"
#include "game/gfx_clut.h"
#include "game/gfx_filter.h"
#include "game/gfx_flush.h"
#include "game/gfx_unpack.h"
#include "game/gfx_vram_ops.h"
#include "game/map_cells.h"
#include "game/map_layers.h"
#include "game/sprite_order.h"
#include "game/sprite_records.h"
#include "game/sprite_draw.h"
#include "game/draw_pool.h"
#include "game/prim.h"
#include "game/map_view.h"
#include "game/menu_frame.h"
#include "game/menu_verbs.h"
#include "game/mode_tasks.h"
#include "game/sprite_anim.h"
#include "game/sprite_find.h"
#include "game/move_script.h"
#include "game/move_groups.h"
#include "game/field_objects.h"
#include "game/object_kinds.h"
#include "game/sprite_screen.h"
#include "game/kind2_object.h"
#include "game/area_slope.h"
#include "game/field_input.h"
#include "game/sprite_clut.h"
#include "game/draw_layers.h"
#include "game/psx_gpu.h"
#include "game/psx_gte.h"
#include "game/psx_gte_float.h"
#include "game/psx_gte_matrix.h"
#include "game/psx_gte_transform.h"
#include "game/draw_emit.h"
#include "game/draw_pass.h"
#include "game/msg_pool.h"
#include "game/text_advance.h"
#include "game/text_draw.h"
#include "game/text_immediate.h"
#include "game/field_frame.h"
#include "game/event_script.h"
#include "hook/detour.h"

namespace bof3 {

void InjectAll() {
    SpriteRecords_Inject();     // first: its fuzz runs the original call tree, so none of it may be patched yet
    MapCells_Inject();          // likewise
    FileIo_Inject();
    GameClock_Inject();         // DIV-0022: before WinMain first reads the clock
    MsgPool_Inject();           // before DatLoad_Inject, which may relocate the pool
    ConfigText_Inject();        // layout only; the text arrives with FIRST.DAT
    MenuVerbs_Inject();         // likewise
    DatLoad_Inject();
    TextAdvance_Inject();
    TextDraw_Inject();
    TextImmediate_Inject();
    SaveIo_Inject();
    GfxFrame_Inject();
    GfxImage_Inject();
    GfxTexCache_Inject();
    GfxClut_Inject();
    GfxFlush_Inject();
    GfxUnpack_Inject();
    GfxVramOps_Inject();
    GfxSpriteUv_Inject();
    GfxFilter_Inject();
    MenuFrame_Inject();
    DrawPass_Inject();          // before what it calls: it clones their originals
    SpriteDraw_Inject();        // likewise: before PsxGpu_Inject and DrawEmit_Inject
    SpriteOrder_Inject();
    DrawPool_Inject();
    Prim_Inject();
    MapView_Inject();
    SpriteAnim_Inject();
    SpriteFind_Inject();
    MoveScript_Inject();
    MoveGroups_Inject();
    FieldObjects_Inject();
    SpriteScreen_Inject();
    FieldInput_Inject();
    SpriteClut_Inject();
    DrawLayers_Inject();
    DrawEmit_Inject();          // likewise
    PsxGteMatrix_Inject();      // before what it calls: it clones their originals
    PsxGteTransform_Inject();   // likewise
    PsxGpu_Inject();
    PsxGte_Inject();
    PsxGteFloat_Inject();
    FieldFrame_Inject();
    Kind2Object_Inject();
    AreaSlope_Inject();
    ModeTasks_Inject();         // every call of its clones re-aimed: order does not matter
    ObjectKinds_Inject();
    MapLayers_Inject();         // its fuzz stands recorders in for every callee, so any slot will do
    EventScript_Inject();       // its fuzz stands recorders in for every callee, so any slot will do
    InjectReport();
}

}  // namespace bof3
