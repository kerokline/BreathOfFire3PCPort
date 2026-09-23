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
#include "game/map_scroll.h"
#include "game/sprite_order.h"
#include "game/sprite_records.h"
#include "game/sprite_draw.h"
#include "game/draw_pool.h"
#include "game/prim.h"
#include "game/map_view.h"
#include "game/menu_frame.h"
#include "game/menu_verbs.h"
#include "game/mode_tasks.h"
#include "game/field_modes.h"
#include "game/sprite_anim.h"
#include "game/sprite_find.h"
#include "game/move_script.h"
#include "game/move_groups.h"
#include "game/field_objects.h"
#include "game/object_kinds.h"
#include "game/sprite_screen.h"
#include "game/kind2_object.h"
#include "game/area_slope.h"
#include "game/field_blocked.h"
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
#include "game/title_states.h"
#include "game/frame_callees.h"
#include "game/field_event.h"
#include "game/event_script.h"
#include "game/msgbox.h"
#include "game/window_task.h"
#include "game/sprite_pose.h"
#include "game/move_cmds.h"
#include "game/mode_flow.h"
#include "game/glyph_draw.h"
#include "game/yes_no_layout.h"
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
    FieldFrame_Inject();        // every call of its clones re-aimed, the handler table swapped: order does not matter
    Kind2Object_Inject();       // likewise, its jump tables re-aimed
    AreaSlope_Inject();         // no calls out
    ModeTasks_Inject();         // every call of its clones re-aimed: order does not matter
    ObjectKinds_Inject();       // every call of its clones re-aimed, the pace and fade tables swapped: order does not matter
    MapLayers_Inject();         // its fuzz stands recorders in for every callee, so any slot will do
    TitleStates_Inject();       // every call of its clones re-aimed: order does not matter
    FieldBlocked_Inject();      // every call of its clones re-aimed at a recorder: order does not matter
    FrameCallees_Inject();      // every call of its clones re-aimed: order does not matter
    FieldModes_Inject();        // every call of its clones re-aimed, every table it reads swapped: order does not matter
    MapScroll_Inject();         // likewise: every call of its clones re-aimed at a recorder
    FieldEvent_Inject();        // every call of its clones re-aimed at a recorder: any slot will do
    EventScript_Inject();       // its fuzz stands recorders in for every callee, so any slot will do
    MsgBox_Inject();            // every call of its clones re-aimed at a recorder, and every stack-built
                                // dispatch table's immediates too: order does not matter
    WindowTask_Inject();        // last: every call of its clones is re-aimed at a recorder and every
                                // stack-built table re-aimed in the copy, so order does not matter
    SpritePose_Inject();        // every call of its clones re-aimed at a recorder or at another of its
                                // own clones: order does not matter
    MoveCmds_Inject();          // every call of its clones re-aimed at a recorder, its jump table
                                // relocated in the copy: order does not matter
    ModeFlow_Inject();          // every call of its clones re-aimed at a recorder, its stack-built table
                                // re-aimed in the copy, the mode table swapped: order does not matter
    GlyphDraw_Inject();         // every call of its clones re-aimed at a recorder, the device a fake:
                                // order does not matter
    YesNoLayout_Inject();       // patches only (DIV-0027): order does not matter
    InjectReport();
}

}  // namespace bof3
