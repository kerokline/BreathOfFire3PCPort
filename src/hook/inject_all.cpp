#include "hook/inject_all.h"

#include "game/config_text.h"
#include "game/dat_load.h"
#include "game/file_io.h"
#include "game/game_clock.h"
#include "game/save_io.h"
#include "game/gfx_frame.h"
#include "game/gfx_image.h"
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
#include "game/area_entry.h"
#include "game/glyph_draw.h"
#include "game/yes_no_layout.h"
#include "game/item_use.h"
#include "game/sound.h"
#include "game/d3d_draw.h"
#include "game/display_env.h"
#include "game/tex_page.h"
#include "game/tex_cells.h"
#include "game/event_objs.h"
#include "game/char_stats.h"
#include "game/member_sprites.h"
#include "game/sprt_draw.h"
#include "game/field_misc.h"
#include "game/save_menu.h"
#include "game/event_ops.h"
#include "game/menu_windows.h"
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
    AreaEntry_Inject();         // every call of its clones re-aimed at a recorder: order does not matter
    GlyphDraw_Inject();         // every call of its clones re-aimed at a recorder, the device a fake:
                                // order does not matter
    YesNoLayout_Inject();       // patches only (DIV-0027): order does not matter
    ItemUse_Inject();           // every call of its clones re-aimed at a recorder, the handler table and
                                // four area descriptors swapped: order does not matter
    Sound_Inject();             // every call of its clones re-aimed at a recorder, its three Win32 import
                                // operands moved onto recorders in the copies: order does not matter
    D3dDraw_Inject();           // every call of its clones re-aimed at a recorder, its four jump tables
                                // relocated in the copies, the device a fake: order does not matter
    DisplayEnv_Inject();        // every call of its clones re-aimed at a recorder or at another of its
                                // clones, the surfaces, viewport and material fakes: order does not matter
    TexPage_Inject();           // every call of its builder copies re-aimed at a recorder or at its own
                                // helper and converter copies, DirectDraw a fake: order does not matter
    TexCells_Inject();          // every call of its builder copies re-aimed at a recorder or at its own
                                // helper copies, DirectDraw and the device fakes: order does not matter
    EventObjs_Inject();         // every call of its clones re-aimed at a recorder: order does not matter
    CharStats_Inject();         // every call of its clones re-aimed at a recorder, its seven jump tables
                                // relocated in the copies, the trait lists swapped: order does not matter
    MemberSprites_Inject();     // every call of its clones re-aimed at a recorder, its two dispatch tables
                                // swapped and three jump tables relocated in the copies: order does not matter
    SprtDraw_Inject();          // its copies are of Capcom's bytes, which nothing but its own Inject
                                // patches; every call of them re-aimed at a recorder, the device a
                                // fake: order does not matter
    FieldMisc_Inject();         // every call of its clones re-aimed at a recorder, its jump table
                                // relocated in the copy, the device a fake: order does not matter
    SaveMenu_Inject();          // every call of its clones re-aimed at a recorder, its jump tables swapped
                                // for recorders and Shop_Equip's relocated in the copy: order does not matter
    EventOps_Inject();          // every call of its clones re-aimed at a recorder, three jump tables relocated
                                // and the chapter table operand moved in the copies: order does not matter
    MenuWindows_Inject();       // after MenuVerbs and YesNoLayout, whose patch sites it reads back (DIV-0018,
                                // DIV-0027); every call of its clones re-aimed at a recorder, its two jump
                                // tables relocated in the copies
    InjectReport();
}

}  // namespace bof3
