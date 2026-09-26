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
#include "game/cheats.h"
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
#include "game/pad_read.h"
#include "game/sprite_clut.h"
#include "game/area_backdrop.h"
#include "game/widescreen.h"
#include "game/battle_actions.h"
#include "game/battle_flow.h"
#include "game/enemy_ai_ops.h"
#include "game/battle_misc.h"
#include "game/battle_sprites.h"
#include "game/inventory_ops.h"
#include "game/battle_phases.h"
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
#include "game/battle_result.h"
#include "game/member_sprites.h"
#include "game/sprt_draw.h"
#include "game/field_misc.h"
#include "game/save_menu.h"
#include "game/event_ops.h"
#include "game/menu_windows.h"
#include "game/display_setup.h"
#include "game/win_main.h"
#include "game/fmv_play.h"
#include "game/world_map.h"
#include "game/battle_draw.h"
#include "game/battle_window_draw.h"
#include "game/battle_windows.h"
#include "game/battle_setup.h"
#include "game/battle_damage.h"
#include "game/battle_items.h"
#include "game/battle_odds.h"
#include "game/window_kinds.h"
#include "game/battle_fx_tasks.h"
#include "game/battle_win_states.h"
#include "game/battle_obj_states.h"
#include "game/battle_menu_states.h"
#include "game/battle_actor_copies.h"
#include "game/battle_turn_steps.h"
#include "game/magic_fx_reached.h"
#include "game/menu_draw_helpers.h"
#include "game/menu_lists.h"
#include "game/shop_states.h"
#include "game/worldmap_area.h"
#include "game/field_hidden.h"
#include "game/event_leader.h"
#include "game/mode_states.h"
#include "game/shop_states2.h"
#include "game/map_field_objects.h"
#include "game/task_sched.h"
#include "game/magic_steal.h"
#include "game/magic_lib.h"
#include "game/magic_s16.h"
#include "game/magic_s18.h"
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
    Cheats_Inject();            // DIV-0045 / DIV-0046, BOF3X_EXP / BOF3X_ZENNY / BOF3X_STEAL: nothing when unset
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
    PadRead_Inject();           // DIV-0050: the keyboard as the original, the pad through SDL3
    SpriteClut_Inject();
    DrawLayers_Inject();
    DrawEmit_Inject();          // before PsxGpu_Inject: it clones originals PsxGpu takes over
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
    WindowTask_Inject();        // every call of its clones is re-aimed at a recorder and every
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
    DisplaySetup_Inject();      // after GfxFilter, whose patch of the original set-up's bytes serves the BOF3X_ORIGINAL path
    WinMain_Inject();           // the window and the frame loop (DIV-0032..0034): no clones, order does not matter
    FmvPlay_Inject();           // the FMVs into the window (DIV-0035): likewise
    AreaBackdrop_Inject();      // every call of its clones re-aimed at a recorder: order does not matter,
                                // except that it runs before Widescreen_Inject, so its fuzz compares the
                                // original's 320-wide backdrop quad
    Widescreen_Inject();        // DIV-0041, BOF3X_WIDE: after the modules above, so their fuzz ran against the original culls
    WorldMap_Inject();          // every call of its clones re-aimed at a recorder, its state table rebuilt in the
                                // copy; none of its functions goes through a cull, so after Widescreen is fine
    BattleDraw_Inject();        // every call of its clones re-aimed at a recorder, the device a fake: order
                                // does not matter (nothing before it patches bytes inside its six)
    BattleWindowDraw_Inject();  // every call of its clones re-aimed at a recorder and no byte of its bodies
                                // patched by any module: order does not matter (none of it reaches a cull)
    BattleWindows_Inject();     // every call of its clones re-aimed at a recorder, its state table, window-kind
                                // handlers and switch table moved in the copies: order does not matter
    BattleFlow_Inject();        // round 7 group BB: every call of its clones re-aimed at a recorder, its two
                                // stack tables re-aimed and its jump table relocated in the copies: order
                                // does not matter (no widescreen patch touches its functions)
    BattleSetup_Inject();       // group BA (round 7): every call of its clones re-aimed at a recorder, no jump
                                // tables - order does not matter
    BattleMisc_Inject();        // every call of its clones re-aimed at a recorder, the dispatch's table
                                // immediates re-aimed and a jump table relocated in the copies: order does not matter
    BattleDamage_Inject();      // every call of its clones re-aimed at a recorder, its jump table relocated
                                // and the effect handler tables swapped: order does not matter
    BattleItems_Inject();       // every call of its clones re-aimed at a recorder, Sparkle_Types' entries
                                // swapped and the stream's buffer a fake: order does not matter
    BattleSprites_Inject();     // group BG, round seven: every call of its clones re-aimed at a recorder,
                                // three jump tables relocated in the copies, the boss table swapped: order does not matter
    InventoryOps_Inject();      // every call of its clones re-aimed at a recorder: order does not matter
    BattleOdds_Inject();        // group CK, round eight: every call and tail jmp of its clones re-aimed at a
                                // recorder: order does not matter
    WindowKinds_Inject();       // round 8 group CM: every call of its clones re-aimed at a recorder and its three
                                // stack tables re-aimed in the copies: order does not matter (it clones its twelve
                                // before injecting them, and no module patches bytes inside them)
    BattleFxTasks_Inject();     // group CE, round eight: every call of its clones re-aimed at a recorder, six stack
                                // tables re-aimed, a jump table relocated and Magic_Rows swapped: order does not matter
    BattleResult_Inject();      // round 8 group CD: every call of its clones re-aimed at a recorder, its two
                                // stack tables re-aimed in the copies and two .data step tables swapped:
                                // order does not matter
    BattleWinStates_Inject();   // group CL (round 8): every call of its clones re-aimed at a recorder and its
                                // seven stack tables' immediates re-aimed in the copies: order does not matter
    EnemyAiOps_Inject();        // round 8 group CF: every call of its clones re-aimed at a recorder and its op
                                // tables' entries swapped: order does not matter (it clones before its own Inject)
    BattleObjStates_Inject();   // round 8 group CG: every call of its clones re-aimed at a recorder, its seven
                                // table operands aimed at tables of recorders in the copies: order does not matter
    BattleMenuStates_Inject();  // round 8 group CI: every call of its clones re-aimed at a recorder and the four
                                // dispatch tables' entries swapped for recorders: order does not matter
    BattleActions_Inject();     // round 8 group CB: every call of its clones re-aimed at a recorder, the seven
                                // step tables' entries swapped in .data: order does not matter
    BattleActorCopies_Inject(); // round 8 group CH: every call of its clones re-aimed at a recorder and its five
                                // .data tables' entries swapped for recorders: order does not matter
    BattlePhases_Inject();      // round 8 group CA: every call of its clones re-aimed at a recorder, its two
                                // stack tables re-aimed in the copies and its three .data tables swapped for
                                // recorders: order does not matter (it clones only its own eighteen)
    BattleTurnSteps_Inject();   // round 8 group CC: every call of its clones re-aimed at a recorder, two jump
                                // tables relocated and eight .data dispatch tables swapped for recorders:
                                // order does not matter
    MagicFxReached_Inject();    // round 8 group CJ: every call of its clones re-aimed at a recorder, its five stack
                                // tables re-aimed in the copies and three .data tables swapped: order does not matter
    MenuDrawHelpers_Inject();   // round 8 group DI: every call of its clones re-aimed at a recorder and its eleven
                                // .data dispatch tables swapped for recorders: order does not matter, except that it
                                // runs after Widescreen_Inject, whose bound inside 0x59B440 ours reads back
    MenuLists_Inject();         // round 8 group DH: every call of its clones re-aimed at a recorder and its eight
                                // .data dispatch tables' entries swapped for recorders: order does not matter
    ShopStates_Inject();        // round 8 group DF: every call of its clones re-aimed at a recorder and its five .data
                                // dispatch tables' entries swapped for recorders: order does not matter
    WorldmapArea_Inject();      // round 8 group DA: every call and tail jmp of its clones re-aimed at a recorder
                                // and its seven .data dispatch tables swapped for recorders: order does not matter
                                // (it clones its thirty before injecting them; WorldMap_Inject cloned 0x404160 earlier)
    FieldHidden_Inject();       // round 8 group DE: every call of its clones re-aimed at a recorder and its two
                                // .data tables' entries swapped for recorders: order does not matter
    EventLeader_Inject();       // round 8 group DC: every call of its clones re-aimed at a recorder and its five
                                // .data dispatch tables' entries swapped for recorders: order does not matter
    ModeStates_Inject();        // round 8 group DB: every call of its clones re-aimed at a recorder, the system
                                // choice's stack table re-aimed, three jump tables relocated and four .data
                                // tables swapped for recorders: order does not matter
    ShopStates2_Inject();       // round 8 group DG: every call of its clones re-aimed at a recorder and its two .data
                                // dispatch blocks swapped for recorders: order does not matter
    MapFieldObjects_Inject();   // round 8 group DD: every call of its clones re-aimed at a recorder and its jump
                                // table relocated in the copy; no module patches bytes inside its sixteen: order
                                // does not matter
    TaskSched_Inject();         // round 9 group EA: the scheduler unit 0x5A98A0..0x5A9A21 cloned whole (it calls
                                // nothing) and run on the fuzz's own stacks; no module patches bytes inside it:
                                // order does not matter (other clones' calls to Task_Sleep and the rest are
                                // re-aimed at their own recorders, and a call site keeps its target either way)
    MagicSteal_Inject();        // round 9 group SH (the spell harness): its clones' calls and stack-table immediates
                                // re-aimed at the shared harness's recorders; after Cheats_Inject, whose DIV-0046
                                // patch inside 0x4F5140 ours reads back and the copy carries: otherwise order
                                // does not matter
    MagicLib_Inject();          // round 9 group L (the effect library): its clones' calls and the popup tasks'
                                // stack-table immediates re-aimed at the shared harness's recorders; no module
                                // patches bytes inside its 25: order does not matter
    MagicS16_Inject();          // round 9 group S16 (MAGIC071..074): its clones' calls, stack-table immediates and
                                // four .data dispatch cells re-aimed at the harness's recorders; no module patches
                                // bytes inside its sixty (DIV-0046's masks are Pilfer's and Steal's): order does
                                // not matter
    MagicS18_Inject();          // round 9 group S18 (MAGIC079 / MAGIC082 through the spell harness): its clones' calls,
                                // stack-table immediates and eight .data tables re-aimed at recorders; no module
                                // patches bytes inside its 42: order does not matter
    InjectReport();
}

}  // namespace bof3
