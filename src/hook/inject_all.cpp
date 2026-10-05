#include "hook/inject_all.h"

#include "game/config_text.h"
#include "game/dat_load.h"
#include "game/fishing_text.h"
#include "game/layering.h"
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
#include "game/d3d_rest.h"
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
#include "game/magic_s21.h"
#include "game/magic_s24.h"
#include "game/magic_s17.h"
#include "game/magic_s19.h"
#include "game/magic_s22.h"
#include "game/magic_s23.h"
#include "game/magic_s25.h"
#include "game/magic_s20.h"
#include "game/magic_engine.h"
#include "game/magic_c3.h"
#include "game/magic_s29.h"
#include "game/magic_s31.h"
#include "game/magic_s28.h"
#include "game/magic_s27.h"
#include "game/magic_s26.h"
#include "game/magic_s30.h"
#include "game/magic_c2.h"
#include "game/magic_c1.h"
#include "game/magic_s01.h"
#include "game/magic_s06.h"
#include "game/magic_s05.h"
#include "game/magic_s02.h"
#include "game/magic_s03.h"
#include "game/magic_s08.h"
#include "game/magic_s04.h"
#include "game/magic_s07.h"
#include "game/magic_s13.h"
#include "game/magic_s11.h"
#include "game/magic_s09.h"
#include "game/magic_s10.h"
#include "game/magic_s12.h"
#include "game/magic_s14.h"
#include "game/magic_s15.h"
#include "game/magic_s32.h"
#include "game/magic_s33.h"
#include "game/magic_s34.h"
#include "game/magic_s36.h"
#include "game/magic_s37.h"
#include "game/magic_s35.h"
#include "game/magic_s38.h"
#include "game/area_011.h"
#include "game/area_cell_hook.h"
#include "game/area_w0b.h"
#include "game/scena_sc0.h"
#include "game/scena_se.h"
#include "game/scena_calls.h"
#include "game/scena_sc11.h"
#include "game/scena_sc3.h"
#include "game/scena_sc1.h"
#include "game/scena_sc12.h"
#include "game/scena_sc9a.h"
#include "game/area_w0a.h"
#include "game/scena_sc6.h"
#include "game/area_w0c.h"
#include "game/area_w1a.h"
#include "game/scena_sc7.h"
#include "game/scena_sc5.h"
#include "game/scena_sx.h"
#include "game/area_w1b.h"
#include "game/scena_sc9b.h"
#include "game/area_w1c.h"
#include "game/area_w2b.h"
#include "game/scena_sc15.h"
#include "game/scena_sc2.h"
#include "game/scena_sc13.h"
#include "game/scena_sx2.h"
#include "game/scenario_harness_fh.h"
#include "game/scenario_harness_ekh.h"
#include "game/effect_1e.h"
#include "game/area_w1e.h"
#include "game/area_w2a.h"
#include "game/area_w1d.h"
#include "game/area_w2d.h"
#include "game/area_w2e.h"
#include "game/area_w2c.h"
#include "game/area_w1f.h"
#include "game/area_w3a.h"
#include "game/area_w3c.h"
#include "game/area_w3b.h"
#include "game/area_w3f.h"
#include "game/area_w3e.h"
#include "game/area_w3d.h"
#include "game/area_w3g.h"
#include "game/area_w2f.h"
#include "game/area_w4a.h"
#include "game/area_w4c.h"
#include "game/area_w4d.h"
#include "game/area_w4b.h"
#include "game/area_w4e.h"
#include "game/area_w4f.h"
#include "game/boss_h.h"
#include "game/boss_sb.h"
#include "game/boss_sa.h"
#include "game/boss_spawn.h"
#include "game/boss_sd.h"
#include "game/boss_sc.h"
#include "game/boss_se.h"
#include "game/boss_si.h"
#include "game/boss_sh.h"
#include "game/boss_sj.h"
#include "game/boss_sg.h"
#include "game/boss_sf.h"
#include "game/boss_harness_eh.h"
#include "game/battle_e7.h"
#include "game/battle_e1.h"
#include "game/battle_e5.h"
#include "game/battle_e6.h"
#include "game/battle_e4.h"
#include "game/battle_e2.h"
#include "game/battle_e3.h"
#include "game/field_c2.h"
#include "game/field_s.h"
#include "game/field_e2.h"
#include "game/field_c3.h"
#include "game/field_e1.h"
#include "game/field_o.h"
#include "game/field_c1.h"
#include "game/effect_gte.h"
#include "game/effect_1f.h"
#include "game/effect_1b.h"
#include "game/effect_1g.h"
#include "game/effect_1d.h"
#include "game/effect_1a.h"
#include "game/effect_2g.h"
#include "game/effect_1c.h"
#include "game/effect_2b.h"
#include "game/effect_2d.h"
#include "game/effect_2c.h"
#include "game/effect_2f.h"
#include "game/effect_2e.h"
#include "game/effect_2a.h"
#include "game/effect_3b.h"
#include "game/effect_3d.h"
#include "game/effect_4a.h"
#include "game/effect_5c.h"
#include "game/effect_3c.h"
#include "game/effect_3a.h"
#include "game/effect_4c.h"
#include "game/effect_4f.h"
#include "game/effect_4d.h"
#include "game/effect_4e.h"
#include "game/effect_4b.h"
#include "game/effect_5g.h"
#include "game/effect_6a.h"
#include "game/effect_6c.h"
#include "game/effect_5a.h"
#include "game/effect_5b.h"
#include "game/effect_5f.h"
#include "game/effect_5e.h"
#include "game/effect_5d.h"
#include "game/effect_6d.h"
#include "game/effect_6b.h"
#include "game/rest_0a.h"
#include "game/rest_1b.h"
#include "game/rest_1e.h"
#include "game/rest_1a.h"
#include "game/rest_1c.h"
#include "game/rest_1g.h"
#include "game/rest_1f.h"
#include "game/rest_1d.h"
#include "game/rest_2a.h"
#include "game/rest_2g.h"
#include "game/rest_2d.h"
#include "game/rest_2e.h"
#include "game/rest_2h.h"
#include "game/rest_2f.h"
#include "game/rest_3f.h"
#include "game/rest_2c.h"
#include "game/rest_3g.h"
#include "game/rest_4a.h"
#include "game/rest_2b.h"
#include "game/rest_3a.h"
#include "game/rest_3c.h"
#include "game/rest_3e.h"
#include "game/rest_3d.h"
#include "game/rest_3b.h"
#include "game/rest_4f.h"
#include "game/rest_4e.h"
#include "game/rest_4d.h"
#include "game/rest_4c.h"
#include "game/rest_4b.h"
#include "hook/detour.h"

namespace bof3 {

void InjectAll() {
    DrawPool_Reserve();         // DIV-0062: the draw-item pool's room below 16 MB, before anything else is placed
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
    MagicS21_Inject();          // round 9 group S21 (MAGIC093..095, the spell harness): no module patches bytes
                                // inside its 48 (DIVERGENCE.md, cheats.cpp); its clones' calls, stack-table
                                // immediates and .data tables re-aimed at the harness's recorders: order does
                                // not matter
    MagicS24_Inject();          // round 9 group S24 (MAGIC104..106): no patch in its band; its calls into group L
                                // and the engine go through raw addresses re-aimed at the harness's recorders
    MagicS17_Inject();          // round 9 group S17 (MAGIC075 / 077 / 078): its clones' calls, stack-table
                                // immediates and .data handler tables re-aimed at the shared harness's recorders;
                                // no module patches bytes inside its 48: order does not matter
    MagicS19_Inject();          // round 9 group S19 (MAGIC083, MAGIC086): its clones' calls, stack-table immediates
                                // and eight .data handler tables re-aimed at the shared harness's recorders; no
                                // module patches bytes inside its 43: order does not matter
    MagicS22_Inject();          // round 9 group S22 (MAGIC096..099: Blizzard, Jolt, Lightning, Myollnir): its
                                // clones' calls, stack-table immediates and .data handler tables re-aimed at the
                                // shared harness's recorders, three through its own; no module patches bytes inside
                                // its 56: order does not matter
    MagicS23_Inject();          // round 9 group S23 (Cyclone, Typhoon, Quake, Simoon): its clones' calls re-aimed at
                                // the harness's recorders, or left on the GTE / GPU library both sides call; after
                                // every module it calls through (psx_gte*, psx_gpu, map_cells, world_map); no
                                // module patches bytes inside its 51: otherwise order does not matter
    MagicS25_Inject();          // round 9 group S25 (MAGIC107..110): its clones' calls, stack-table immediates and
                                // the one jump table re-aimed at the shared harness's recorders; no module patches
                                // bytes inside its 56: order does not matter
    MagicS20_Inject();          // round 9 group S20 (MAGIC087, 088, 092): its clones' calls, stack-table immediates
                                // and jump table re-aimed or moved, its thirteen .data tables swapped for the
                                // fuzz only; no module patches bytes inside its 51: order does not matter
    MagicEngine_Inject();       // round 9 group E (the engine-side rows 0, 108, 123, 126, 128 and Head Cracker's
                                // rock): its clones' calls and stack-table immediates re-aimed at the shared
                                // harness's recorders; no module patches bytes inside its 22: order does not matter
    MagicC3_Inject();           // round 9 group C3 (MAGIC002, MAGIC111: the overlays no ability loads): its clones'
                                // calls and stack-table immediates re-aimed, its three .data tables swapped for the
                                // fuzz only; no module patches bytes inside its 20: order does not matter
    MagicS29_Inject();          // round 9 group S29 (MAGIC125, 126: DivineBreath, ShadowBreath): its clones' calls,
                                // stack-table immediates and eleven .data handler tables re-aimed at the shared
                                // harness's recorders, _ftol left to the copy; no module patches bytes inside its
                                // 49: order does not matter
    MagicS31_Inject();          // round 9 group S31 (MAGIC132, 137, 138, 143): its clones' calls, stack-table
                                // immediates and jump table re-aimed or moved, its eight .data tables swapped for
                                // the fuzz only; no module patches bytes inside its 51: order does not matter
    MagicS28_Inject();          // round 9 group S28 (MAGIC122..124: Firebreath, Icebreath, ThundrBreath, and
                                // Port_DroppedCall 0x4DF820): its clones' calls and stack-table immediates re-aimed
                                // at the shared harness's recorders, its seven .data tables swapped for the fuzz
                                // only; after every module whose fuzz clones a caller of 0x4DF820 (they re-aim that
                                // site themselves); DIV-0011 re-aims four calls to it, bytes outside it
    MagicS27_Inject();          // round 9 group S27 (MAGIC118, 120, 121): its clones' calls, stack-table immediates
                                // and two jump tables re-aimed or moved, its six .data tables swapped for the fuzz
                                // only; no module patches bytes inside its 47: order does not matter
    MagicS26_Inject();          // round 9 group S26 (MAGIC114, 115, 117): its clones' calls, stack-table immediates,
                                // jump tables and seven .data tables re-aimed or moved for the fuzz only; no module
                                // patches bytes inside its 48 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS30_Inject();          // round 9 group S30 (MAGIC130, MAGIC131): its clones' calls, stack-table immediates and
                                // eleven .data tables re-aimed at the shared harness's recorders; no module patches
                                // bytes inside its 60 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicC2_Inject();           // round 9 group C2 (MAGIC057, 081, 116, 129: Bone Dance, RottenBreath, UtmostAttack,
                                // Holocaust): its clones' calls, stack-table immediates and .data handler tables
                                // re-aimed at the shared harness's recorders; no module patches bytes inside its 64:
                                // order does not matter
    MagicC1_Inject();           // round 9 group C1 (MAGIC010, 080, 113, 145, 146, 213: the unfinished skills): its
                                // clones' calls, stack-table immediates and fifteen .data tables re-aimed at the
                                // shared harness's recorders; no module patches bytes inside its 62: order does
                                // not matter
    MagicS01_Inject();          // round 9 group S01 (MAGIC001: rows 1 and 105, Nue Stomp and Jump): its clones'
                                // calls and stack-table immediates re-aimed at the shared harness's recorders, its
                                // one .data table swapped for the fuzz only; no module patches bytes inside its 26
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS06_Inject();          // round 9 group S06 (MAGIC008, MAGIC020): its clones' calls, stack-table immediates
                                // and two .data tables re-aimed at the shared harness's recorders; no module patches
                                // bytes inside its 56 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS05_Inject();          // round 9 group S05 (MAGIC017, MAGIC018/019): its clones' calls, stack-table
                                // immediates and one .data table re-aimed at the shared harness's recorders; no
                                // module patches bytes inside its 29 (DIVERGENCE.md, cheats.cpp): order does not
                                // matter
    MagicS02_Inject();          // round 9 group S02 (MAGIC003: Super Combo; MAGIC004, the code of ten Strike and Claw
                                // files): its clones' calls, stack-table immediates and jump table re-aimed or moved,
                                // its four .data tables swapped for the fuzz only; no module patches bytes inside its
                                // 48 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS03_Inject();          // round 9 group S03 (MAGIC006, 009, 012: Mind Sword, Chlorine, Blitz): its clones'
                                // calls and stack-table immediates re-aimed at the shared harness's recorders, its
                                // five .data tables swapped for the fuzz only; no module patches bytes inside its
                                // 44 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS08_Inject();          // round 9 group S08 (MAGIC041, 042, 043, 044): its clones' calls, stack-table
                                // immediates and seven .data tables re-aimed at the shared harness's recorders; no
                                // module patches bytes inside its 59 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS04_Inject();          // round 9 group S04 (MAGIC013, MAGIC015 with 016 folded: Snap, Charge, Flying Kick,
                                // Air Raid): its clones' calls, stack-table immediates, two jump tables and five
                                // .data tables re-aimed or moved for the fuzz only; no module patches bytes inside
                                // its 56 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS07_Inject();          // round 9 group S07 (MAGIC021, 038, 039, 040: Bonebreak, War Shout, Focus,
                                // Enlighten): its clones' calls, stack-table immediates and eight .data tables
                                // re-aimed at the shared harness's recorders; no module patches bytes inside its 59
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS13_Inject();          // round 9 group S13 (MAGIC063: Sudden Death): its clones' calls and stack-table
                                // immediates re-aimed at the shared harness's recorders, its four .data tables
                                // swapped for the fuzz only; no module patches bytes inside its 25 (DIVERGENCE.md,
    MagicS11_Inject();          // round 9 group S11 (MAGIC058, 059: Sanctuary, Tornado): its clones' calls,
                                // stack-table immediates and three .data table runs re-aimed at the shared
                                // harness's recorders; no module patches bytes inside its 35 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    MagicS09_Inject();          // round 9 group S09 (MAGIC045, 046/047, 048, 050: Bone Dart, Firebreath / Icebreath,
                                // Dream Breath, Pollen / Venom Breath): its clones' calls, stack-table immediates and
                                // eight .data tables re-aimed at the shared harness's recorders; no module patches
                                // bytes inside its 47 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS10_Inject();          // round 9 group S10 (MAGIC052..056: Ovum, Lavaburst, Howling, Ebonfire,
                                // Sacrifice): its clones' calls, stack-table immediates and nine .data tables
                                // re-aimed at the shared harness's recorders; no module patches bytes inside its 60
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS12_Inject();          // round 9 group S12 (MAGIC060, MAGIC062: Identify, Celerity): its clones' calls,
                                // stack-table immediates, one jump table and seven .data tables re-aimed or moved
                                // for the fuzz only; no module patches bytes inside its 44 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    MagicS14_Inject();          // round 9 group S14 (MAGIC064, 066 and MAGIC065's last: Weretiger, Pilfer,
                                // Tsunami): its clones' calls, stack-table immediates and four .data tables re-aimed
                                // at the shared harness's recorders; no module patches bytes inside its 57 (DIVERGENCE.md,
                                // cheats.cpp: DIV-0046 patches Pilfer's 0x4B5691, not ours): order does not matter
    MagicS15_Inject();          // round 9 group S15 (MAGIC067, 068, 069: Chill, Foretell, Influence): its clones'
                                // calls, stack-table immediates and jump table re-aimed or moved, its ten .data
                                // tables swapped for the fuzz only; no module patches bytes inside its 52
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS32_Inject();          // round 9 group S32 (MAGIC144, 150: Wall of Fire, Eye Beam): its clones' calls and
                                // stack-table immediates re-aimed at the shared harness's recorders, its five .data
                                // tables swapped for the fuzz only; no module patches bytes inside its 36
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS33_Inject();          // round 9 group S33 (MAGIC151, 154: Accession, Mighty Chop): its clones' calls
                                // and stack-table immediates re-aimed, its seven .data tables swapped for the
                                // fuzz only; no module patches bytes inside its 57 (DIVERGENCE.md, cheats.cpp)
    MagicS34_Inject();          // round 9 group S34 (MAGIC158, 159, 161, 162, 166: Charm, (no label), Timed Blow,
                                // Transfer, Monopolize): its clones' calls and stack-table immediates re-aimed at the
                                // shared harness's recorders, its eight .data tables swapped for the fuzz only; no
                                // module patches bytes inside its 47 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS36_Inject();          // round 9 group S36 (MAGIC172, 173, 218: Magic Ball, Intimidate, Aura Breath): its
                                // clones' calls and stack-table immediates re-aimed, its eight .data tables swapped
                                // for the fuzz only; no module patches bytes inside its 45 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    MagicS37_Inject();          // round 9 group S37 (MAGIC219, 220/221, 222: Magma Breath, Geo / Gaea's Breath,
                                // Combustion): its clones' calls and stack-table immediates re-aimed at the shared
                                // harness's recorders, its eleven .data tables swapped for the fuzz only; no module
                                // patches bytes inside its 60 (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS35_Inject();          // round 9 group S35 (MAGIC167, 168, 169: Last Resort, Cure, Benediction): its
                                // clones' calls and stack-table immediates re-aimed at the shared harness's recorders,
                                // its six .data tables swapped for the fuzz only; no module patches bytes inside its 46
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    MagicS38_Inject();          // round 9 group S38 (MAGIC223, 225, 226/227: Tempest / Hurricane, an unlabelled id,
                                // MeteorStrike): its clones' calls and stack-table immediates re-aimed at the shared
                                // harness's recorders, its thirteen .data tables swapped for the fuzz only; no module
                                // patches bytes inside its 54 (DIVERGENCE.md, cheats.cpp): order does not matter
    Area011_Inject();           // round 10 group ARH (area 11: two handlers and the init, the area harness's proof):
                                // its clones' calls re-aimed at the area harness's recorders; no module patches
                                // bytes inside its 3 (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaCellHook_Inject();      // round 10 group ARH (0x56E670, the cell hook's per-area reader): its table's
                                // handlers swapped for the fuzz only; nothing patches inside it
    AreaW0b_Inject();           // round 10 group AR0B (world 0's areas 16 and 18..26): its clones' calls re-aimed at
                                // the area harness's recorders, area 16's six .data state tables swapped for the fuzz
                                // only; no module patches bytes inside its 61 (DIVERGENCE.md, cheats.cpp): order does
                                // not matter
    ScenaSc0_Inject();          // round 10 group SCH (scenario chapter 0, the scenario harness's proof): its clones'
                                // calls re-aimed at the scenario harness's recorders, its three .data tables swapped
                                // for the fuzz only; no module patches bytes inside its 19 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    ScenaSe_Inject();           // round 10 group SE (the chapters' shared engine-side helpers, six): its clones'
                                // calls re-aimed at the scenario harness's recorders; no module patches bytes
                                // inside its 6 (DIVERGENCE.md, cheats.cpp): order does not matter
    ScenaSc11_Inject();         // round 10 group SC11 (scenario chapter 11): its clones' calls re-aimed at the scenario
                                // harness's recorders, its three .data tables swapped for the fuzz only; no module
                                // patches bytes inside its 30 (DIVERGENCE.md, cheats.cpp): order does not matter
    ScenaSc3_Inject();          // round 10 group SC3 (scenario chapters 3 and 4): its clones' calls re-aimed at the
                                // scenario harness's recorders, its tables swapped for the fuzz only; no module
                                // patches bytes inside its 50 (DIVERGENCE.md, cheats.cpp): order does not matter
    ScenaSc1_Inject();          // round 10 group SC1 (scenario chapter 1's bank): its clones' calls re-aimed at the
                                // scenario harness's recorders, its three .data tables swapped for the fuzz only;
                                // no module patches bytes inside its 42: order does not matter
    ScenaSc12_Inject();         // round 10 group SC12 (scenario chapter 12's first block, 0x55E4E0..0x561DB0):
                                // no module patches bytes inside its 24 (DIVERGENCE.md, cheats.cpp): order does
                                // not matter
    ScenaCalls_Inject();        // round 10 group CALLS (the chapter call tables' 98 entries, 0x519890..0x51AC50):
                                // its clones' calls re-aimed at the scenario harness's recorders; no module patches
                                // bytes inside its 98 (DIVERGENCE.md, cheats.cpp): order does not matter
    ScenaSc9a_Inject();         // round 10 group SC9a (scenario chapter 9's first block, 0x553B30..0x557170):
                                // no module patches bytes inside its 22 (DIVERGENCE.md, cheats.cpp): order does
                                // not matter
    AreaW0a_Inject();           // round 10 group AR0A (world 0's areas 0..5, 7, 8, 10, 12, 13, 15): its clones' calls
                                // re-aimed at the area harness's recorders, area 15's two state tables swapped for
                                // the fuzz only; no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp):
    ScenaSc6_Inject();          // round 10 group SC6 (scenario chapter 6's bank, 0x54A910..0x54F080): its clones'
                                // calls re-aimed at the scenario harness's recorders, its tables swapped for the
                                // fuzz only; no module patches bytes inside its 48 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    AreaW0c_Inject();           // round 10 group AR0C (world 0, areas 27..29 and 32..37): its clones' calls
                                // re-aimed at the area harness's recorders, its five .data state tables swapped
                                // for the fuzz only; no module patches bytes inside its 52 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    ScenaSc7_Inject();          // round 10 group SC7 (scenario chapters 7 and 8, 0x54F080..0x553B30): its clones'
                                // calls re-aimed at the scenario harness's recorders, its tables swapped for the
                                // fuzz only; no module patches bytes inside its 52 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    ScenaSc5_Inject();          // round 10 group SC5 (scenario chapter 5's bank, 0x546390..0x54A910): no module
                                // patches bytes inside its 35 (DIVERGENCE.md, cheats.cpp): order does not matter
    ScenaSx_Inject();           // round 10 group SX (the engine callees the chapters and areas call by raw address,
                                // eighteen): its clones' calls re-aimed at the scenario harness's recorders; no
                                // module patches bytes inside its 18 (DIVERGENCE.md, cheats.cpp): order does not
    AreaW1b_Inject();           // round 10 group AR1B (world 1, areas 42..47): its clones' calls re-aimed at the
                                // area harness's recorders, its .data state tables swapped for the fuzz only; no
                                // module patches bytes inside its 55 (DIVERGENCE.md, cheats.cpp): order does not
                                // matter
    ScenaSc9b_Inject();         // round 10 group SC9b (scenario chapter 9's tail and chapter 10, 0x557170..0x55C040):
                                // its clones' calls re-aimed at the scenario harness's recorders, its tables swapped
                                // for the fuzz only; no module patches bytes inside its 63 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    AreaW1c_Inject();           // round 10 group AR1C (world 1, areas 48..52, 0x408FF0..0x40AB00): its clones' calls
                                // re-aimed at the area harness's recorders; no module patches bytes inside its 57
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    ScenaSc15_Inject();         // round 10 group SC15 (scenario chapter 15, 0x567DC0..0x56B2A0 and 0x537580; chapter 16's
                                // slot 1 0x56C080; chapters 17..19, 0x56C130..0x56D5E0): its clones' calls re-aimed at
                                // the scenario harness's recorders, its tables swapped for the fuzz only; no module
                                // patches bytes inside its 89 (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW1a_Inject();           // round 10 group AR1A (world 1, areas 38..41, 0x4053B0..0x406650): its clones' calls
                                // re-aimed at the area harness's recorders, areas 39 and 41's state tables swapped
                                // for the fuzz only; no module patches bytes inside its 48 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    ScenaSc2_Inject();          // round 10 group SC2 (scenario chapter 2's bank, 0x53DDA0..0x5428C0): no module
                                // patches bytes inside its 72 (DIVERGENCE.md, cheats.cpp): order does not matter
    ScenaSx2_Inject();          // round 10 group SX2 (the engine callees nobody owned after SX, thirteen): its
                                // clones' calls re-aimed at the scenario harness's recorders; no module patches
                                // bytes inside its 13 (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW1e_Inject();           // round 10 group AR1E (world 1, areas 65 and 67, 0x40B8C0..0x40CEF0): its clones'
                                // calls re-aimed at the area harness's recorders, area 65's state tables swapped for
                                // the fuzz only; no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    AreaW2a_Inject();           // round 10 group AR2A (world 2, areas 76..82 and 84, 0x40EB90..0x40F720): its clones'
                                // calls re-aimed at the area harness's recorders, area 79's state table swapped for
                                // the fuzz only; no module patches bytes inside its 50 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    AreaW2b_Inject();           // round 10 group AR2B (world 2, areas 85..88, 0x40F720..0x411F10): its clones' calls
                                // re-aimed at the area harness's recorders, the world maps' state tables swapped for
                                // the fuzz only; no module patches bytes inside its 66 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    AreaW1d_Inject();           // round 10 group AR1D (world 1, areas 53, 55..57, 59..64, 0x40AB00..0x40B8C0): its
                                // clones' calls re-aimed at the area harness's recorders, areas 56 and 59's state
                                // tables and areas 63 / 64's weights swapped for the fuzz only; no module patches
                                // bytes inside its 47 (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW2d_Inject();           // round 10 group AR2D (world 2, areas 95..100 and 103, 0x4135B0..0x4146C0): its
                                // clones' calls re-aimed at the area harness's recorders, areas 99 and 100's state
                                // tables swapped for the fuzz only; no module patches bytes inside its 53
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW2c_Inject();           // round 10 group AR2C (world 2, areas 90..92 and 94, 0x411F10..0x4135B0): its clones'
                                // calls re-aimed at the area harness's recorders, area 91's state table swapped for
                                // the fuzz only; no module patches bytes inside its 45 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    ScenaSc13_Inject();         // round 10 group SC13 (scenario chapters 13 and 14, 0x561DB0..0x567DC0): no module
                                // patches bytes inside its 51 (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW3a_Inject();           // round 10 group AR3A (world 3, areas 115..119, 0x418BE0..0x41A9D0): its clones' calls
                                // re-aimed at the area harness's recorders, area 115's and 116's .data state tables
                                // swapped for the fuzz only; no module patches bytes inside its 56 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    AreaW3c_Inject();           // round 10 group AR3C (world 3, areas 124..125, 127..128, 130..134, 0x41C890..0x41DAD0):
                                // its clones' calls re-aimed at the area harness's recorders; no module patches bytes
                                // inside its 56 (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW3b_Inject();           // round 10 group AR3B (world 3, areas 120..121, 0x41A9D0..0x41C890): its clones'
                                // calls re-aimed at the area harness's recorders, area 121's .data state tables
                                // swapped for the fuzz only; no module patches bytes inside its 54 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    AreaW2e_Inject();           // round 10 group AR2E (world 2, areas 104..106, 0x4146C0..0x4168E0): its clones'
                                // calls re-aimed at the area harness's recorders, area 104's six .data state tables
                                // swapped for the fuzz only; no module patches bytes inside its 52 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    AreaW3f_Inject();           // round 10 group AR3F (world 3, areas 143..146, 0x420800..0x4223A0): its clones'
                                // calls re-aimed at the area harness's recorders, its .data state tables swapped for
                                // the fuzz only; no module patches bytes inside its 53 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    AreaW3e_Inject();           // round 10 group AR3E (world 3, areas 136 and 139..142, 0x41EFE0..0x420800): its
                                // clones' calls re-aimed at the area harness's recorders, the state tables of areas
                                // 140..142 swapped for the fuzz only; no module patches bytes inside its 52
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW3d_Inject();           // round 10 group AR3D (world 3, area 135, 0x41DAD0..0x41EFE0): its clones' calls
                                // re-aimed at the area harness's recorders, its four .data state tables swapped for
                                // the fuzz only; no module patches bytes inside its 46 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    AreaW2f_Inject();           // round 10 group AR2F (world 2, areas 108 and 110..113, 0x4168E0..0x418BE0): its
                                // clones' calls re-aimed at the area harness's recorders, areas 108 and 112's state
                                // tables swapped for the fuzz only; no module patches bytes inside its 53
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW1f_Inject();           // round 10 group AR1F (world 1, areas 68..69 and 71..75, 0x40CEF0..0x40EB90): its clones'
                                // calls re-aimed at the area harness's recorders, its .data state tables swapped for
                                // the fuzz only; no module patches bytes inside its 59 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    AreaW3g_Inject();           // round 10 group AR3G (world 3, areas 148..151, 0x4223A0..0x4249D0): its clones'
                                // calls re-aimed at the area harness's recorders, its .data state tables swapped for
                                // the fuzz only; no module patches bytes inside its 56 (DIVERGENCE.md, cheats.cpp):
                                // order does not matter
    AreaW4a_Inject();           // round 10 group AR4A (world 4, areas 152..155 and 166..167, 0x4249D0..0x426560): its
                                // clones' calls re-aimed at the area harness's recorders, area 152's six .data state
                                // tables swapped for the fuzz only; no module patches bytes inside its 48
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW4c_Inject();           // round 10 group AR4C (world 4, areas 173..174 and six choices of areas 175..185,
                                // 0x428450..0x4292C0): its clones' calls re-aimed at the area harness's recorders; no
                                // module patches bytes inside its 39 (DIVERGENCE.md, cheats.cpp): order does not matter
    AreaW4d_Inject();           // round 10 group AR4D (world 4, areas 175..187, 0x4292C0..0x42A320): its clones'
                                // calls re-aimed at the area harness's recorders, area 175's glide states swapped
                                // for the fuzz only; no module patches bytes inside its 49 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    AreaW4b_Inject();           // round 10 group AR4B (world 4, areas 168..172, 0x426560..0x428450): its clones'
                                // calls re-aimed at the area harness's recorders, area 172's two .data state tables
                                // swapped for the fuzz only; no module patches bytes inside its 56 (DIVERGENCE.md,
    AreaW4e_Inject();           // round 10 group AR4E (world 4, areas 188..191, 0x42A320..0x42BD60): its clones'
                                // calls re-aimed at the area harness's recorders, areas 189 and 191's state tables
                                // swapped for the fuzz only; no module patches bytes inside its 51 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    AreaW4f_Inject();           // round 10 group AR4F (world 4, areas 192..193 and 196..199, 0x42BD60..0x42D710):
                                // its clones' calls re-aimed at the area harness's recorders, areas 197 and 198's
                                // state tables swapped for the fuzz only; no module patches bytes inside its 49
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    BossSpawn_Inject();         // round 11 group BH (the boss set-ups' spawn helpers, 0x4948E0..0x494A7D): its
                                // clones' calls re-aimed at the boss harness's recorders; no module patches bytes
                                // inside its 6 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossH_Inject();             // round 11 group BH (the boss band's 20 shared helpers, 0x437CA0..0x440829): its
                                // clones' calls re-aimed at the boss harness's recorders, kinds 8..11's shared
                                // tables swapped and six kinds' dispatchers driven with a clone planted in their
                                // tables for the fuzz only; no module patches bytes inside its 20 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    BossSd_Inject();            // round 11 group BSD (fights 17..21 and kinds 18, 21..27, 0x43A590..0x43B74A): its
                                // clones' calls re-aimed at the boss harness's recorders, its kinds' step, action
                                // and hook tables swapped for the fuzz only; no module patches bytes inside its 52
    BossSb_Inject();            // round 11 group BSB (fights 4..10 and 13, kinds 3, 4, 5 and 8..11, 0x438290..0x43A022):
                                // its clones' calls re-aimed at the boss harness's recorders, its kinds' state and
                                // hook tables swapped for the fuzz only; no module patches bytes inside its 52
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    BossSa_Inject();            // round 11 group BSA (fights 1, 2, 3, 39 and kinds 1, 2, 6, 7, 39, 46,
                                // 0x437A10..0x43D662): its clones' calls re-aimed at the boss harness's recorders,
                                // the six kinds' state and hook tables swapped for the fuzz only; no module patches
                                // bytes inside its 49 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossSc_Inject();            // round 11 group BSC (fights 11, 12, 14, 15, 16, 46 and kinds 12..17, 53,
                                // 0x439410..0x43A589): its clones' calls re-aimed at the boss harness's recorders,
                                // seven kinds' tables swapped for the fuzz only; no module patches bytes inside
                                // its 53 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossSe_Inject();            // round 11 group BSE (fights 22..26, 30, 48 and kinds 28..32, 55, 0x43B5B0..0x43E7A0):
                                // its clones' calls re-aimed at the boss harness's recorders, the kinds' state and
                                // hook tables swapped for the fuzz only; no module patches bytes inside its 52
                                // (DIVERGENCE.md, cheats.cpp): order does not matter
    BossSi_Inject();            // round 11 group BSI (fights 37, 38, 40, 42, 44, 45, 49..51, 53, kinds 45, 47, 49,
                                // 51, 52, 56..58, 60 and Arwan's task, 0x43ECC0..0x43F79B): its clones' calls
                                // re-aimed at the boss harness's recorders, the kinds' state and hook tables swapped
                                // for the fuzz only; no module patches bytes inside its 50 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    BossSh_Inject();            // round 11 group BSH (fights 34..36, 41, 43, 47, kinds 41..44, 48, 50, 54 and the
                                // kind-3 slot 3, 0x43DEF0..0x43ECBF): its clones' calls re-aimed at the boss
                                // harness's recorders, the kinds' state and hook tables swapped for the fuzz only;
                                // no module patches bytes inside its 46 (DIVERGENCE.md, cheats.cpp): order does not
                                // matter
    BossSj_Inject();            // round 11 group BSJ (fights 52, 54, 55, kinds 59, 61, effect slots 4 and 5,
                                // 0x43F7A0..0x441087): its clones' calls re-aimed at the boss harness's recorders,
                                // the kinds' and tasks' tables swapped for the fuzz only; no module patches bytes
                                // inside its 44 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossSg_Inject();            // round 11 group BSG (fights 29, 31..33, kinds 34..38 and 40, the effect task F6,
                                // 0x43CDE0..0x43E535): its clones' calls re-aimed at the boss harness's recorders, its
                                // kinds' and F6's tables swapped for the fuzz only; no module patches bytes inside its
                                // 53 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossSf_Inject();            // round 11 group BSF (fights 27, 28, kinds 33 and 62, fight 26's count task and the
                                // Gazer's effect task, 0x43C480..0x4406D7): its clones' calls re-aimed at the boss
                                // harness's recorders, the kinds' and tasks' tables swapped for the fuzz only; no
                                // module patches bytes inside its 54 (DIVERGENCE.md, cheats.cpp): order does not matter
    BossHarnessEh_Inject();     // round 12 group EH: the boss harness's own self-test of its battle-engine frame
                                // (the new shapes, state_cell, the engine set) with Capcom's code on both sides;
                                // takes no function and patches nothing: order does not matter
    ScenarioHarnessFh_Inject(); // round 12 group FH: the scenario harness's field mode proved on Capcom's code, a copy
                                // of each of thirteen field functions against the original in place - injects
                                // nothing, takes nothing; after every scenario group, so their random streams are
                                // the ones they had (docs/scenario_harness.md section 7.7)
    BattleE7_Inject();          // round 12 group BE7 (the battle windows 0x597FC0..0x59DB61: the result screen's
                                // level-up and drops windows, the gene windows, the equipment window): its clones'
                                // calls re-aimed at the boss harness's recorders, its stack tables' immediates at
                                // handler recorders; no module patches bytes inside its 31 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    BattleE1_Inject();          // round 12 group BE1 (BATE's tally and equipment screens, the command menu's held
                                // steps and auto battle, action kind 3, the ability notice, the result's counts and
                                // level gain, the loss screen, 0x42D7A0..0x432B6A): its clones' calls re-aimed at the
                                // boss harness's recorders, its dispatchers' tables swapped for the fuzz only; no module
                                // patches bytes inside its 41 (DIVERGENCE.md, cheats.cpp; DIV-0020 reads 0x42E09D,
                                // past BattleExtra_EquipLeave's jmp): order does not matter
    BattleE5_Inject();          // round 12 group BE5 (the enemy AI helpers, Effect_Handlers slots 22 / 23 / 38,
                                // BattleForm_ApplyStats, the Dragon command's run, 0x44B240..0x451471): its clones'
                                // calls re-aimed at the boss harness's recorders, the Dragon run's six step tables
                                // swapped for the fuzz only; no module patches bytes inside its 52 (DIVERGENCE.md,
                                // cheats.cpp): order does not matter
    BattleE6_Inject();          // round 12 group BE6 (the transformation, the gene cost, BattleFx_Dispatch slots 15,
                                // 16, 18, the stat rebuild, the member roll, the AP pop-up, the Field_Slots debts,
                                // BMAGIC's four map cells; 0x451480..0x4552F5, 0x4CEB40..0x4CF4A3): its clones' calls
                                // re-aimed at the boss harness's recorders, the tasks' tables swapped for the fuzz
                                // only; no module patches bytes inside its 39 (DIVERGENCE.md, cheats.cpp): order does
                                // not matter
    BattleE4_Inject();          // round 12 group BE4 (the battle engine's 0x444660..0x44AAC9: damage, the pace and
                                // target helpers, the item command's states 5..9, the escape): its clones' calls
                                // re-aimed at the boss harness's recorders, its four dispatchers' tables swapped for
                                // the fuzz only; after BossHarnessEh_Inject, whose self-test copies 0x4457F0; no
                                // module patches bytes inside its 56 (DIVERGENCE.md, cheats.cpp)
    BattleE2_Inject();          // round 12 group BE2 (0x433650..0x437030: the effect tasks of BattleFx_Dispatch slots
                                // 9, 10, 12, 13, 14, 17 and the watch's state 3, the enemy's action pick and targets,
                                // EnemyOp_Steps 4 / 5, EnterSubs 1, ActSubs 3 / 5): its clones' calls re-aimed at the boss
                                // harness's recorders, the EnemyOp tables swapped for the fuzz only; no module patches
                                // bytes inside its 48 (DIVERGENCE.md, cheats.cpp): order does not matter
    BattleE3_Inject();          // round 12 group BE3 (the enemy ops EnemyOp_Steps 7..9, the party objects' states 6,
                                // 10, 11, 26, two party helpers, 0x437030..0x442F97): its clones' calls re-aimed at the
                                // boss harness's recorders, its sub-tables swapped for the fuzz only; after
                                // BossHarnessEh_Inject, whose self-test copies 0x441A10 / 0x441A30; no module patches
                                // bytes inside its 49 (DIVERGENCE.md, cheats.cpp)
    FieldC2_Inject();           // round 12 group FC2 (the field core's 0x46BBF0..0x46D5ED: effect kinds 0x30, 0x34,
                                // 0x3A and 0x41): its clones' calls re-aimed at the scenario harness's recorders, the
                                // kinds' eight state tables swapped for the fuzz only; after ScenarioHarnessFh_Inject
                                // (none of its thirteen is FC2's); no module patches bytes inside its 44
                                // (DIVERGENCE.md, cheats.cpp): order does not matter otherwise
    FieldS_Inject();            // round 12 group FS (the shop overlay's remainder 0x57FF80..0x5859F9: the field save's
                                // confirm, the rest sequence's first two states, the party formation, the resistance
                                // shop, the shared ability list; the equip screen's choosers 0x58C7A0, 0x58CAE0): its
                                // clones' calls re-aimed at the scenario harness's recorders, its five dispatch tables
                                // swapped for the fuzz only; after ScenarioHarnessFh_Inject, whose self-test copies
                                // 0x5811B0 and 0x5845E0, and after MenuFrame_Inject, whose DIV-0011 RetargetCall
                                // at 0x581313 (inside 0x581300) ours follows; nothing else patches its 53
    FieldE2_Inject();           // round 12 group FE2 (the field engine's rest: the event script's helpers, the leader's
                                // hop helpers, the mode-11 object, seven field tail kinds, three map-cell draws, the
                                // trade screen; 0x5341C0..0x5372D8, 0x56D6B0..0x5729F8, 0x593960..0x594060): its
                                // clones' calls re-aimed at the scenario harness's recorders, its dispatch tables
                                // swapped for the fuzz only; after ScenarioHarnessFh_Inject, whose self-test copies
                                // 0x5343C0, 0x534420, 0x56E020 and 0x5728D0; no module patches bytes inside its 51
    FieldC3_Inject();           // round 12 group FC3 (object steering, op E7, the fade cases, mode 8's and 11's
                                // frames, the field core's state-2 sub-states 0, 1, 3..8 and their steps;
                                // 0x5172C0..0x5195F9, 0x525390..0x526DAF): its clones' calls re-aimed at the scenario
                                // harness's recorders, its ten tables swapped for the fuzz only; after
                                // ScenarioHarnessFh_Inject, whose self-test copies 0x525CC0 (FieldCore_Up), and after
                                // ObjectKinds_Inject, whose copy of Field_ObjectFadeOut holds the four fade cases; no
                                // module patches bytes inside its 63 (DIVERGENCE.md, cheats.cpp)
    FieldE1_Inject();           // round 12 group FE1 (the field engine 0x52D080..0x533BA0: the panel draws, the
                                // leader states 6, 8, 11, 12 and the jump, a field object's content, the zenny found,
                                // the cells around a sprite, the gateway exit, the party's placements, the pending
                                // jump's members): its clones' calls re-aimed at the scenario harness's recorders,
                                // its five tables swapped for the fuzz only; after ScenarioHarnessFh_Inject, whose
                                // self-test copies 0x52F980 and 0x52D880; no module patches bytes inside its 45
                                // (DIVERGENCE.md, cheats.cpp)
    FieldO_Inject();            // round 12 group FO (the field engine's 0x5738A0..0x57CD89: eight menu panels, the
                                // movement ops 87 / 88 / DB / E9 / F9 and their states, the placement ops 3x 4x 6x 7x
                                // Ax, thirteen event conditions, EventScript_SkipIf, ObjTrio_ClearBit40): its clones'
                                // calls re-aimed at the scenario harness's recorders; after ScenarioHarnessFh_Inject,
                                // whose self-test copies 0x57C1A0, 0x57C230, 0x57C8E0 (FO's); after YesNoLayout_Inject,
                                // whose DIV-0029 byte at 0x576A48 Menu_DrawSaveSlot reads back
    FieldC1_Inject();           // round 12 group FC1 (the field core's first half, 0x461800 and 0x469D10..0x46BBF0:
                                // the Config row label, effect kinds 4, 0x14, 0x17, 0x19, 0x1B, 0x30..0x32, 0x37,
                                // 0x3C and two of kind 6's ticks): its clones' calls re-aimed at the scenario
                                // harness's recorders, its five state tables swapped for the fuzz only; after
                                // ScenarioHarnessFh_Inject (none of its thirteen is FC1's). config_text.cpp patches
                                // operands inside 0x461800 (DIV-0015 / DIV-0017), which ours reads in place at each
                                // call: order does not matter
    EffectGte_Inject();         // round 13 stage-A group EGT (0x494060..0x494270: the map camera into the GTE, a
                                // world point projected, a matrix's diagonal of ones, a size at a point's depth):
                                // its clones' calls re-aimed at the scenario harness's recorders; every caller
                                // that is ours calls it by the address it had, so order does not matter; no
                                // module patches bytes inside its four (DIVERGENCE.md, cheats.cpp)
    ScenarioHarnessEkh_Inject(); // round 13 group EKH: the scenario harness's effect mode proved on Capcom's code, eight
                                // rows of round thirteen's cut on both sides - injects nothing, takes nothing; after
                                // every scenario group, so their random streams are the ones they had; every effect
                                // group's inject goes after it, whose self-test copies 0x462BC0 (E1A), 0x46F2B0,
                                // 0x46F7D0 (E1D), 0x4FD470 (E5A), 0x500D20 (E5B), 0x479970 (E2E), 0x4857C0 (E3C),
                                // 0x472770 (E2A) from the image - all ours now, named by their bof3::addr constants
                                // (the originals' addresses), and run "in place" as Capcom's only because those
                                // groups inject below (docs/scenario_harness.md sections 8.8 and 8.10)
    Effect1F_Inject();          // round 13 group E1F (0x52A6C0..0x52D07C: the extra-slot menu on effect record 6,
                                // the scaled sprite pass, the effect records' reset, the kind points, game mode 8's
                                // steps, UiSprite_SetMode / UiSprite_Draw): its clones' calls re-aimed at the
                                // scenario harness's recorders; after ScenarioHarnessEkh_Inject (none of its eight
                                // is E1F's); no module patches bytes inside its fifteen (DIVERGENCE.md, cheats.cpp,
    Effect1B_Inject();          // round 13 group E1B (0x4672F0..0x46A5F2: kind 0xF's states 26..40 and its six
                                // child sub-kinds, the panels and windows, kinds 0x11, 0x12, 0x14, 0x92): its
                                // clones' calls re-aimed at the scenario harness's recorders, its eight state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight is E1B's); no module patches bytes inside its 48 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    Effect1G_Inject();          // round 13 group E1G (0x594060..0x594D8A: the item-trade screen's rest - its fourth
                                // run step, its leave state and steps, the backdrop, list, ingredient and count
                                // windows, the ingredient test, the row count - and Item_DrawIcon): its clones' calls
                                // re-aimed at the scenario harness's recorders; after ScenarioHarnessEkh_Inject (none
                                // of its eight is E1G's) and FieldE2_Inject (whose trade states call these by the
                                // addresses they had); no module patches bytes inside the fourteen
    Effect1D_Inject();          // round 13 group E1D (0x46F2B0..0x4702F6: effect kinds 0x21..0x27, their dispatchers,
                                // states and two draw helpers): its clones' calls re-aimed at the scenario harness's
                                // recorders, its seven state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject, whose self-test copies 0x46F2B0 and 0x46F7D0; no module
                                // patches bytes inside its 30 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect1E_Inject();          // round 13 group E1E (0x528CD0..0x52A6B0: the leader's state 9, LeaderPanel_Stages'
                                // stages 2..9 and five of stage 1's steps): its clones' calls re-aimed at the scenario
                                // harness's recorders, its eight steps tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E1E's); no module patches bytes
                                // inside its 48 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect1A_Inject();          // round 13 group E1A (0x462B00..0x4672F0: the dispatchers of effect kinds 1, 2, 3, 5,
                                // 7, 8, 9, 0xA..0x10, 0x16, 0x1A, 0x5C, the states of kinds 1, 3, 5, 7, 0xA, 0xC, 0xD,
                                // 0xF, 0x1A and the panel draws EffectHud_*): its clones' calls re-aimed at the
                                // scenario harness's recorders, its thirteen state tables and WorldMap_Records'
                                // +4 / +8 cells swapped for the fuzz only; after ScenarioHarnessEkh_Inject, which
                                // copies E1A's 0x462BC0; no module patches bytes inside its 70 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    Effect1C_Inject();          // round 13 group E1C (0x46A850..0x46F2A6: effect kinds 0x36, 0x3C, 0x70, 0x1C..0x20):
                                // its clones' calls re-aimed at the scenario harness's recorders, its seven state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight is E1C's); no module patches bytes inside its 53 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    Effect2G_Inject();          // round 13 group E2G (0x47DBE0..0x47FD92: effect kinds 0x15, 0x54, 0x55, 0x57, 0x5A,
                                // 0x5B, 0x66 and kind 0x18's sub-kind 0x20 - dispatchers, states, draws - and the
                                // dispatchers of kinds 0x5D..0x5F): its clones' calls re-aimed at the scenario harness's
                                // recorders, its eleven state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E2G's); no module patches bytes
                                // inside its 58 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect2B_Inject();          // round 13 group E2B (0x4731A0..0x474F34: effect kinds 0x2F, 0x33, 0x35, 0x38, 0x39,
                                // 0x3B, 0x3D): its clones' calls re-aimed at the scenario harness's recorders, its eight
                                // state tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight is E2B's) and Effect1C_Inject (EffectKind1E_ShardInit, called by name); no
                                // module patches bytes inside its 52 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect2D_Inject();          // round 13 group E2D (0x4771B0..0x4789C1: effect kinds 0x45..0x49, their dispatchers,
                                // states and draw helpers): its clones' calls re-aimed at the scenario harness's
                                // recorders, its ten state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E2D's); no module patches
                                // bytes inside its 54 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect2C_Inject();          // round 13 group E2C (0x474F40..0x4771A5: effect kinds 0x3E, 0x3F, 0x40, 0x42, 0x43,
                                // 0x44 and 0x6B, their dispatchers, states and draw helpers): its clones' calls
                                // re-aimed at the scenario harness's recorders, its nine tables swapped for the fuzz
                                // only; after ScenarioHarnessEkh_Inject (none of its eight is E2C's); no module
                                // patches bytes inside its 53 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect2F_Inject();          // round 13 group E2F (0x47B7D0..0x47DBD1: effect kinds 0x4F, 0x50, 0x51, 0x52, 0x53,
                                // 0x56 and kind 0x4E's disc): its clones' calls re-aimed at the scenario harness's
                                // recorders, its eight state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight is E2F's); no module patches bytes
                                // inside its 62 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect2E_Inject();          // round 13 group E2E (0x4789D0..0x47B7CF: kind 0x48's states 7..12, effect kinds
                                // 0x4A..0x4E, the sphere, the angle mean): its clones' calls re-aimed at the scenario
                                // harness's recorders, its ten state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject, which copies E2E's 0x479970; no module patches bytes
                                // inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect2A_Inject();          // round 13 group E2A (0x470300..0x473192: effect kinds 0x28..0x2E, their dispatchers,
                                // states and draws, and the pool's specks, sparks and drops): its clones' calls
                                // re-aimed at the scenario harness's recorders, its seven state tables swapped for
                                // the fuzz only; after ScenarioHarnessEkh_Inject, whose self-test copies 0x472770,
                                // and Effect1C_Inject, whose kinds 0x1C / 0x1D call 0x471D10 and 0x471E20; no module
                                // patches bytes inside its 65 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect3B_Inject();          // round 13 group E3B (0x4823D0..0x48404A: effect kinds 0x63, 0x65, 0x67, 0x69, 0x6C -
                                // dispatchers, states, draws): its clones' calls re-aimed at the scenario harness's
                                // recorders, its nine state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E3B's); no module patches bytes
                                // inside its 49 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect3D_Inject();          // round 13 group E3D (0x485CB0..0x48801A: effect kinds 0x76..0x7D and 0x7F..0x82,
                                // their dispatchers, states and draws, area 170's dial map): its clones' calls
                                // re-aimed at the scenario harness's recorders, its eleven state tables swapped for
                                // the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is E3D's)
                                // and the wave-two groups it calls (E2E's angle mean, E2F's trail cap); DrawPool
                                // re-aims the immediate at 0x486EBD inside EffectKind7D_SetMap (DIV-0062), after
                                // every self-test; no other module patches bytes inside its 68
    Effect4D_Inject();          // round 13 group E4D (0x48C990..0x48DF87: effect kinds 0x91, 0x94..0x98, 0x9A and the
                                // screen tint 0x48CA90 - dispatchers, states, draws): its clones' calls re-aimed at
                                // the scenario harness's recorders, its seven state tables swapped for the fuzz only;
                                // after ScenarioHarnessEkh_Inject (none of its eight rows is E4D's) and before
                                // Widescreen_ArmFills (its three fills compare the original's 320 x 240); no
                                // module patches bytes inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect3C_Inject();          // round 13 group E3C (0x484050..0x485CA0: effect kinds 0x6D, 0x6E, 0x6F, 0x72..0x75,
                                // their dispatchers, states and draws, and the debris draw and set-up kinds 0x1E,
                                // 0x1F and 0x4B share): its clones' calls re-aimed at the scenario harness's
                                // recorders, its nine state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject, whose self-test copies 0x4857C0, and Effect1C_Inject /
                                // Effect2E_Inject, which call 0x485030 and 0x4851E0 by address; no module patches
                                // bytes inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect3A_Inject();          // round 13 group E3A (0x4801F0..0x4823C2: effect kinds 0x60, 0x61, 0x62, 0x64 and
                                // 0x68 - dispatchers, states and draws): its clones' calls re-aimed at the scenario
                                // harness's recorders, its five state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E3A's); no module patches bytes
                                // inside its 48 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect4C_Inject();          // round 13 group E4C (0x48B200..0x48C985: effect kinds 0x8D, 0x8E, 0x8F, 0x90, 0x93
                                // and 0x99 - dispatchers, states, pools and draws): its clones' calls re-aimed at the
                                // scenario harness's recorders, its six state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E4C's) and Effect3C_Inject,
                                // whose EffectKind6E_FindShard it calls by name; no module patches bytes inside its
                                // 50 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect4F_Inject();          // round 13 group E4F (0x491D70..0x494026: effect kinds 0xAA..0xB1, 0xB9 and 0xBA -
                                // dispatchers, states and draws): its clones' calls re-aimed at the scenario harness's
                                // recorders, its ten state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject (none of its eight rows is E4F's) and Effect3A_Inject /
                                // Effect1C_Inject, which call 0x493B50, 0x493C60 and 0x493090 by address; no module
                                // patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp, widescreen.cpp); before
                                // Widescreen_ArmFills (EffectKindAF_DrawScreen's fill, DIV-0041's 0x493308, compares
                                // the original's 320 x 240)
    Effect4E_Inject();          // round 13 group E4E (0x48DF90..0x491C96: effect kinds 0x9B, 0x9C and 0xA0 whole,
                                // kind 0x9E's dispatcher and draws, the dispatchers of 0xA1..0xA3 and 0xA7..0xA9):
                                // its clones' calls re-aimed at the scenario harness's recorders, its ten state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight rows is E4E's) and Effect3A_Inject (EffectKind64_DrawSpark called by name);
                                // no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect4B_Inject();          // round 13 group E4B (0x489030..0x48B1FC: effect kinds 0x88..0x8C, 0x9D, 0x9F, 0xA4 -
                                // dispatchers, states, draws - and kind 0x87's helpers): its clones' calls re-aimed at
                                // the scenario harness's recorders, its eight state tables swapped for the fuzz only;
                                // after ScenarioHarnessEkh_Inject (none of its eight rows is E4B's) and before
                                // Widescreen_ArmFills (EffectKind89_DrawTint's fill, DIV-0041's 0x489D47, compares the
                                // original's 320 x 240); no module patches bytes inside its 66 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    Effect4A_Inject();          // round 13 group E4A (0x488020..0x48902E and 0x433640: effect kinds 0x82 (states
                                // 11..23), 0x83..0x87 - dispatchers and states): its clones' calls re-aimed at the
                                // scenario harness's recorders, its five state tables swapped for the fuzz only;
                                // after ScenarioHarnessEkh_Inject (none of its eight rows is E4A's) and Effect3D_Inject
                                // (kind 0x82's first states, whose table holds eight of these); no module patches
                                // bytes inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect5G_Inject();          // round 13 group E5G (0x50AF90..0x50C0CA: kind 0x18's sub-kinds 0x2B, 0x2C, 0x36,
                                // 0x3A, 0x4A - sub-state dispatchers, states, panel draws): its clones' calls re-aimed
                                // at the scenario harness's recorders, its four sub-state tables swapped for the fuzz
                                // only; after ScenarioHarnessEkh_Inject (none of its eight rows is E5G's) and before
                                // Widescreen_ArmFills (sub-kind 0x36's fill compares the original's 320 x 240); no
                                // module patches bytes inside its 24 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect5A_Inject();          // round 13 group E5A (0x4FD2E0..0x4FF318: kind 0x18's sub-kinds 1, 4..0xA, 0x1A and
                                // 0x1F - sub-state dispatchers, states and draws - and Gfx_ClutStripCopy16): its
                                // clones' calls re-aimed at the scenario harness's recorders, its eight sub-state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject, whose self-test
                                // copies 0x4FD470 (EffectKind18_04_Run); no module patches bytes inside its 55
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect5B_Inject();          // round 13 group E5B (0x4FF320..0x5014F2: kind 0x18's sub-kinds 0x0B..0x0F, 0x13 and
                                // 0x4F - dispatchers, states, draws): its clones' calls re-aimed at the scenario
                                // harness's recorders, its seven sub-state tables swapped for the fuzz only; after
                                // ScenarioHarnessEkh_Inject, whose self-test copies 0x500D20 by address; no module
                                // patches bytes inside its 50 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect5F_Inject();          // round 13 group E5F (0x508CC0..0x50AF8D: kind 0x18's sub-kinds 0x27, 0x28, 0x29, 0x2A,
                                // 0x3C, 0x42, 0x48, 0x49 and 0x4B / 0x4C - dispatchers, states, draws): its clones'
                                // calls re-aimed at the scenario harness's recorders, its eight state tables swapped for
                                // the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is E5F's) and
                                // before DrawPool_Grow (EffectKind18Sub42_Mark is one of DIV-0062's owned users); no
                                // module patches bytes inside its 49 but DrawPool_Grow's five sites in 0x509850
    Effect5E_Inject();          // round 13 group E5E (0x506A10..0x508CBB: effect kind 0x18's sub-kinds 0x23..0x26,
                                // 0x39 and 0x3F - dispatchers by +2, sub-states, draws): its clones' calls re-aimed
                                // at the scenario harness's recorders, its six sub-state tables swapped for the fuzz
                                // only; after ScenarioHarnessEkh_Inject (none of its eight rows is E5E's) and before
                                // Widescreen_ArmFills (its two full-frame fills, DIV-0041's 0x507BDC and 0x507CE3,
                                // compare the original's 320 x 240); no module patches bytes inside its 54
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect5D_Inject();          // round 13 group E5D (0x503DE0..0x506A00: effect kind 0x18's sub-kinds 0x14, 0x18,
                                // 0x19, 0x1B..0x1E, 0x21, 0x22, 0x43, 0x52, 0x68 and four of sub-kind 0x17's helpers):
                                // its clones' calls re-aimed at the scenario harness's recorders, its six state tables
                                // swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is
                                // E5D's; kEffectOverrides' 0x503FA0 row stands in for E5C's raw calls); no module
                                // patches bytes inside its 52 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect5C_Inject();          // round 13 group E5C (0x501500..0x503DDD: effect kind 0x18's sub-kinds 0x10, 0x11,
                                // 0x12, 0x15, 0x16, 0x17, 0x50, 0x56..0x58 - dispatchers by +2, sub-states, draws):
                                // its clones' calls re-aimed at the scenario harness's recorders, its nine state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight rows is E5C's); no module patches bytes inside its 62 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    Effect6D_Inject();          // round 13 group E6D (0x514270..0x516B2E: kind 0x18's sub-kinds 0x5C, 0x5D, 0x5E,
                                // 0x61..0x65 - dispatchers by +2 and Cond_ByteFE, states, draws): its clones' calls
                                // re-aimed at the scenario harness's recorders, its six state tables swapped for the
                                // fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is E6D's); no
                                // module patches bytes inside its 51 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect6A_Inject();          // round 13 group E6A (0x50C0D0..0x50E3FA: effect kind 0x18's sub-kinds 0x2D, 0x2E..0x32,
                                // 0x3E and 0x4A's state 1 - dispatchers by +2, sub-states, draws): its clones' calls
                                // re-aimed at the scenario harness's recorders, its seven sub-state tables swapped for
                                // the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is E6A's) and
                                // Effect5G_Inject (0x50C0D0 tail-jumps to its EffectKind18Sub2C_Draw); no module
                                // patches bytes inside its 48 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Effect6C_Inject();          // round 13 group E6C (0x510C90..0x51426B: effect kind 0x18's sub-kinds 0x44 (frame
                                // and sky), 0x45, 0x51, 0x53, 0x55, 0x59, 0x5A, 0x5B, 0x66, and AreaMap_CornerHeight):
                                // its clones' calls re-aimed at the scenario harness's recorders, its six state tables
                                // swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its eight rows is
                                // E6C's) and before Widescreen_ArmFills (sub-kind 0x44's red fill is DIV-0041's) and
                                // DrawPool_Grow (five DIV-0062 sites); no module patches bytes inside its 50 but
                                // draw_pool.cpp's six DrawItems immediates
    Effect6B_Inject();          // round 13 group E6B (0x50E400..0x510C8A: effect kind 0x18's sub-kinds 0x33..0x35,
                                // 0x37, 0x38, 0x3B, 0x3D, 0x40, 0x41, 0x44, 0x54 - dispatchers by +2, sub-states,
                                // draws): its clones' calls re-aimed at the scenario harness's recorders, its nine
                                // sub-state tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of
                                // its eight rows is E6B's) and before Widescreen_ArmFills (sub-kind 0x54's fill,
                                // DIV-0041's 0x50F7B5, compares the original's 320 x 240); no module patches bytes
                                // inside its 50 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest0A_Inject();            // round 14 stage-A group R0A (0x51C390..0x524E4E: the party sets' field actions' seven
                                // shared helpers - the probes two steps ahead, the kind-0x30 effect object ahead and
                                // the members in reach of it, an effect object of kind 0x34 on a cell, the side
                                // probes): its clones' calls re-aimed at the scenario harness's recorders; after
                                // every harness's inject; every caller that is ours (PartyAction5_Form0Begin) calls
                                // it by the address it had, so order does not matter; no module patches bytes inside
                                // its seven (DIVERGENCE.md, cheats.cpp)
    Rest1B_Inject();            // round 14 wave-one group R1B (0x51D710..0x51F202: party sets 2..6's field actions -
                                // the form, state and step dispatchers, the states of sets 3..5, four cell
                                // handlers): its clones' calls re-aimed at the scenario harness's recorders, its 29
                                // tables swapped for the fuzz only; after every harness's inject and R0A's (it calls
                                // R0A's helpers by name); no module patches bytes inside its 47 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    Rest1C_Inject();            // round 14 wave-one group R1C (0x51F210..0x520E08: party sets 6..9's field actions -
                                // the dispatchers by form, state and step, their probe / resolve states, three cell
                                // pickups, two cell hits, the shared turn, tick and wait states): its clones' calls
                                // re-aimed at the scenario harness's recorders, its 28 dispatch tables swapped for the
                                // fuzz only; after Rest0A_Inject (it calls R0A's helpers by name) and every harness's
                                // inject; no module patches bytes inside its 51 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    Rest1G_Inject();            // round 14 wave-one group R1G (0x5289A0..0x52CD46: the fishing spot - the leader's
                                // state 9 stages 1, 10, 11 and their steps, the menu's data and rule pages, the
                                // stages' helpers; game mode 8's fish): its clones' calls re-aimed at the scenario
                                // harness's recorders, its six tables swapped for the fuzz only; after every
                                // harness's inject; ours that call it (E1E, E1F) call it by the address it had;
                                // before FishingText_Arm, which patches no byte inside its 45 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    Rest1F_Inject();            // round 14 group R1F (0x523ED0..0x52899B: party sets 16..18's field actions, a
                                // raised sprite's cell ahead, the leader's state 9's stage 0): its clones' calls
                                // re-aimed at the scenario harness's recorders, its 24 tables swapped for the fuzz
                                // only; after every harness's inject and Rest0A_Inject (its callers reach R0A by the
                                // address); no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    Rest1D_Inject();            // round 14 wave-one group R1D (0x520E10..0x5226C1: party sets 9..12's field actions -
                                // their 27 dispatchers and 19 state handlers and cell probes): its clones' calls
                                // re-aimed at the scenario harness's recorders, its 27 state tables swapped for the
                                // fuzz only; after Rest0A_Inject (it calls R0A's helpers by name); reached only
                                // through .data tables and its own E8 calls, so order among wave one does not matter;
                                // no module patches bytes inside its 46 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest2B_Inject();            // round 14 wave-two group R2B (0x56E040..0x57F33C: Effect_Spawn / _SpawnAt, two menu
                                // primitives the community band calls, Field_ObjectTriggers[1], game mode 8 step 8's
                                // screen Shisu_* and its two models): its clones' calls re-aimed at the scenario
                                // harness's recorders, its seven tables swapped for the fuzz only; after every
                                // harness's inject; ours that call Effect_Spawn / _SpawnAt call them by name; no
                                // module patches bytes inside its 38 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest1E_Inject();            // round 14 group R1E (0x5226D0..0x523EC2: the field actions of party sets 13, 14,
                                // 15 and set 16's forms 0 and 1 - the dispatchers by the form word and the state
                                // bytes, the turn-and-probe, resolve, cell-pickup and strike states): its clones'
                                // calls re-aimed at the scenario harness's recorders, its 28 state tables swapped for
                                // the fuzz only; after Rest0A_Inject (it calls R0A's helpers by name) and every
                                // harness's inject; no module patches bytes inside its 47 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    Rest1A_Inject();            // round 14 wave-one group R1A (0x51BA80..0x51D70C: party-member states 4, 6, 8 and
                                // party sets 0..2's field actions - their dispatchers by form, state and step, the
                                // form actions' turns, the cell pickups and set 2's cell strike): its clones' calls
                                // re-aimed at the scenario harness's recorders, its 26 tables swapped for the fuzz
                                // only; after Rest0A_Inject (it calls R0A's helpers by name) and every harness's
                                // inject; no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    Rest2A_Inject();            // round 14 wave-two group R2A (0x5372E0..0x537F1B: the field frame's listed-bank
                                // screen pass, Char_GainHp / Char_LoseAp, Area_ObjectHandler and the four object
                                // handlers of its fallback table Area_ObjectFallbacks with their states and spawns):
                                // its clones' calls and stack-table immediates re-aimed at the scenario harness's
                                // recorders, Area_ObjectFallbacks and Area74_Handlers swapped for the fuzz only; after
                                // every harness's inject and wave one's; no module patches bytes inside its 22
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp, labels.cpp)
    Rest2G_Inject();            // round 14 wave-two group R2G (0x597FA0..0x59AA77: record handler 4's kind 0 and the
                                // EXP to the next level, handler 5's kind dispatch and the gene grid's states,
                                // MenuList_Run's kinds 5..13 and 15..19 and 26 slide states): its clones' calls
                                // re-aimed at the scenario harness's recorders, its 12 state tables swapped for the
                                // fuzz only; after every harness's inject and Widescreen_Inject (it reads DIV-0041's
                                // six bounds from the operands); no other module patches a byte inside its 48
                                // (DIVERGENCE.md, cheats.cpp, labels.cpp)
    Rest2D_Inject();            // round 14 wave-two group R2D (0x5869A0..0x58B1CD: the masters' screen's pick, yes /
                                // no prompt and states 3..6, the field menu's Status and Items screens, the top bar's
                                // countdown and camp check, the field abilities' effects): its clones' calls re-aimed
                                // at the scenario harness's recorders, its five state tables swapped for the fuzz only
                                // and FieldAbility_Effects for typed stand-ins; after every harness's inject and after
                                // YesNoLayout_Inject, whose DIV-0027 re-aims two call sites inside MasterScreen_AskYesNo
                                // (ours reads where they reach; the fuzz runs that one only with them unpatched)
    Rest2E_Inject();            // round 14 wave-two group R2E (0x58B1D0..0x58ED3F: the field menu's Items arrange
                                // steps, discard and use states and sorts, the Equipment and Ability screens): its
                                // clones' calls re-aimed at the scenario harness's recorders, its five tables swapped
                                // for the fuzz only; after every harness's inject; ours that call it (FS's
                                // Equip_ChooseItem, PartyForm_Swap, the SharedList sorts) call it by the address it
                                // had; no module patches bytes inside its 49 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp, labels.cpp)
    Rest2H_Inject();            // round 14 wave-two group R2H (0x59AA80..0x5A9874: window kinds - handler 7's 11..18,
                                // handler 8's 3..5, MenuList_Kinds[20] - the masters' windows, the battle equipment
                                // window's items and bar, the joystick enumeration, Cfg_Load's key table): its clones'
                                // calls re-aimed at the scenario harness's recorders, its ten step tables swapped for
                                // the fuzz only; after every harness's inject; after MenuFrame_Inject (DIV-0011) and
                                // BattleDraw_Inject (DIV-0059), whose call sites inside 0x59AA80 / 0x59DBF0 ours reads,
                                // and Widescreen_Inject (DIV-0041), whose bound inside 0x59C130 ours reads back
    Rest2F_Inject();            // round 14 wave-two group R2F (0x58ED40..0x596F98: the field menu's Tactics and
                                // Config steps, the Ability screen's helpers, window-record handlers 1 and 2 and
                                // window kind 1): its clones' calls re-aimed at the scenario harness's recorders, its
                                // 15 state tables swapped for the fuzz only; after Widescreen_Inject (DIV-0041's
                                // bound inside MenuSlide_LeftOff170, read back) and BattleDraw_Inject (DIV-0059's
                                // title call inside Win2_DrawItemList, read back) and every harness's inject
    Rest2C_Inject();            // round 14 wave-two group R2C (0x57F340..0x58699F: the inn's, save point's and rest's
                                // last states, the save block's builder and Save_QuickWrite, the shop's browse and
                                // sell modes and four ShopMode dispatchers, the master's talk and its panels, the
                                // figure record's moves): its clones' calls re-aimed at the scenario harness's
                                // recorders, its 14 state tables swapped for the fuzz only; after every harness's
                                // inject; ours that call it (Game_WndProc, FieldTail_LoadBank, effect_2g, effect_3d)
                                // call it by the address it had; before FishingText_Arm; no module patches bytes
                                // inside its 61 (DIVERGENCE.md, cheats.cpp, widescreen.cpp, labels.cpp,
                                // yes_no_layout.cpp)
    Rest3A_Inject();            // round 14 wave-three group R3A (0x404180..0x4378AA: area 33's world-map frame
                                // states, BATE's root and three equipment helpers, BattleEnd_Steps[2] / [3] and
                                // ExitSteps[3], kind-0 battle-task slots 2, 4, 5, 11, 12 and the actor watch's
                                // states 1 / 4, BattleBossFx_Dispatch, the enemy animation helpers, New Game's
                                // records): its clones' calls re-aimed at the boss harness's recorders, its five
                                // step tables swapped for the fuzz only; after every harness's inject and every
                                // module whose functions it calls; DIV-0020's check of 0x437834 / 0x437891 reads
                                // bytes the inject's jmp at 0x437820 leaves in place; no module patches bytes
                                // inside its 45 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest3C_Inject();            // round 14 wave-three group R3C (0x44D000..0x44E4AA: Effect_Handlers slots 50..85 and
                                // 87..111): its clones' calls re-aimed at the boss harness's recorders; reached only
                                // through Effect_Handlers (Effect_ApplyResult reads the cell) and R3D's 0x44EA70 tail
                                // jump; after every harness's inject; no module patches bytes inside its 61
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest3E_Inject();            // round 14 wave-three group R3E (0x46A320..0x4801ED: three kinds' dispatchers, the
                                // helpers of kinds 0x1D / 0x21 / 0x24 / 0x30 / 0x41, the glow sparks, trail, spiral,
                                // ring and dust of kinds 0x48 / 0x49, kinds 0x5D / 0x5E / 0x5F's states): its clones'
                                // calls re-aimed at the scenario harness's recorders, three state tables swapped and
                                // EffectGlowSparks_States patched for the fuzz only; after every harness's inject and
                                // every effect group's (they call its functions by the addresses they had); before
                                // FishingText_Arm; no module patches bytes inside its 50 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    Rest3G_Inject();            // round 14 wave-three group R3G (0x4925C0..0x5171FB: effect kinds 0xAC, 0xAD, 0xAE,
                                // 0xBA's leftover states, a screen triangle's winding, the boss actors' placement,
                                // game modes 8..11's dispatchers and steps, Quake's vertex lift, area 109's switch,
                                // sub-kind 0x41's two draws, area 0xBD's view, kind 0xF's character count): its
                                // clones' calls re-aimed at the scenario harness's recorders, its four mode step
                                // tables swapped for the fuzz only; after every harness's inject and after
                                // Widescreen_Inject (DIV-0041), whose four bounds inside AreaMapBD_BuildView ours
                                // reads back, and before DrawPool_Grow (DIV-0062), whose item array and bound there
                                // ours reads back too; ours that call it (E1A, E5A, E6B, BE6, the battle steps)
                                // call it by the address it had
    Rest3D_Inject();            // round 14 wave-three group R3D (0x44E4B0..0x44FF0E: Effect_Handlers slots 112..129,
                                // the rolls, the inflict, the stat step and the no-hit mark the effect slots share,
                                // the Dragon command's part dispatcher): its clones' calls re-aimed at the boss
                                // harness's recorders, DragonCmd_Parts swapped for the fuzz only; after BattleE5_Inject
                                // (the parts and Effect_DrainAp it reaches are BE5's) and every harness's inject;
                                // ours that call it (battle_damage, battle_e5, magic_lib) call it by the address it
                                // had; no module patches bytes inside its 36 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest3F_Inject();            // round 14 wave-three group R3F (0x480210..0x49259C: the states of effect kinds 0x60,
                                // 0x9E, 0xA1..0xA3, 0xA7..0xAA and 0xAB's state 1, and the draws they and their
                                // neighbours call): its clones' calls re-aimed at the scenario harness's recorders;
                                // after every harness's inject; before Widescreen_ArmFills, so its self-test compares
                                // EffectKindAA_DrawFill's original 320 x 240 (DIV-0041); no module patches bytes inside
                                // its 50 (DIVERGENCE.md, cheats.cpp, widescreen.cpp, labels.cpp, yes_no_layout.cpp)
    Rest3B_Inject();            // round 14 wave-three group R3B (0x4468B0..0x44CFF4: the result screen's EXP
                                // helpers, three percent clamps, the command menus' last steps and four
                                // dispatchers, 45 Effect_Handlers slots): its clones' calls re-aimed at the boss
                                // harness's recorders, four .data tables swapped for the fuzz only; after every
                                // harness's inject; ours that call it (battle_result, battle_e6, rest_2g) call it by
                                // the address it had; before FishingText_Arm; no module patches bytes inside its 64
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest4F_Inject();            // round 14 wave-four group R4F (0x460CB0..0x464B9E: the Config screen's machine and
                                // draws, WorldMap_ExitRecords, the states of effect kinds 2, 7, 8, 9 and 0xB and
                                // their draws): its clones' calls re-aimed at the scenario harness's recorders, five
                                // state tables swapped for the fuzz only; after every harness's inject and after
                                // ConfigText_Inject and MenuFrame_Inject, whose call sites and operands inside its
                                // Config draws ours reads in place (DIV-0011, DIV-0017, DIV-0026, DIV-0051); ours
                                // that call it (effect_1a, event_leader, rest_2f) call it by the address it had;
                                // before FishingText_Arm
    Rest4E_Inject();            // round 14 wave-four group R4E (0x45E870..0x460CAD: the community band's panels,
                                // name commits, track list, item screen and ranked lists): its clones' calls
                                // re-aimed at the scenario harness's recorders, seven .data tables swapped for the
                                // fuzz only; after every harness's inject; R4B and R4D call it by the address it
                                // had; before FishingText_Arm; no module patches bytes inside its 48 (DIVERGENCE.md,
                                // cheats.cpp, widescreen.cpp)
    Rest4D_Inject();            // round 14 wave-four group R4D (0x45C400..0x45E86E: the community's games 7
                                // and 8 - CommuDraw, CommuName - and the board R4C draws with): its clones' calls
                                // re-aimed at the scenario harness's recorders, eleven .data tables swapped for the
                                // fuzz only; after every harness's inject; before FishingText_Arm; no module patches
                                // bytes inside its 60 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest4A_Inject();            // round 14 wave-four group R4A (0x452DD0..0x456D4F: six battle targeting helpers,
                                // Field_RunSlot and its CLUT copy, the community's simulation and its objects'
                                // poses, twelve Field_ObjectTriggers entries): its clones' calls re-aimed at the
                                // scenario harness's recorders; after every harness's inject; ours that call it
                                // (battle_sprites, battle_e1, battle_e2, frame_callees, area_w4d, mode_states) call
                                // it by the address it had; before FishingText_Arm; no module patches bytes inside
                                // its 48 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    Rest4C_Inject();            // round 14 wave-four group R4C (0x459EE0..0x45C3F3: the community band's two games
                                // with a stake, entries 0 and 1 of R4B's game table 0x652A84, their nine state tables
                                // and draw helpers, Commu_PushSubscreen): its clones' calls re-aimed at the scenario
                                // harness's recorders, its nine tables swapped for the fuzz only; after every
                                // harness's inject; before FishingText_Arm; no module patches bytes inside its 60
                                // (DIVERGENCE.md, cheats.cpp, widescreen.cpp, labels.cpp)
    Rest4B_Inject();            // round 14 wave-four group R4B (0x456D50..0x459EDA: Field_ModeTailKinds 14, 21..26,
                                // 60, their states and the community board's states and draws): its clones' calls
                                // re-aimed at the scenario harness's recorders, twelve .data tables swapped for the
                                // fuzz only; after every harness's inject; before FishingText_Arm; no module patches
                                // bytes inside its 60 (DIVERGENCE.md, cheats.cpp, widescreen.cpp)
    D3dRest_Inject();           // the platform round's group PH (docs/d3d-rest.md): the renderer's live remainder - five
                                // Direct3D handlers, POLY_FT3's two helpers, D3d_SetAlphaModulate, D3d_AfterDraw,
                                // Gfx_StoreImage; Gfx_DrawOTag reaches them by the addresses they had; its clones' calls
                                // re-aimed at its own recorders; before FishingText_Arm
    FishingText_Arm();        // DIV-0069: the fishing text's Latin layout - after every module's self-test, which
                                // all compared Capcom's (effect_1a's and effect_1b's draws read it)
    layering::Arm();            // DIV-0071: the floor under a sprite drawn before it (BOF3X_LAYERING) - after every
                                // module's self-test, which all compared the original's order (layering.h)
    Widescreen_ArmFills();     // DIV-0041 section 3c: the full-frame fills widen from here - after every module's
                                // self-test, which all compared the original's (0, 0) 320 x 240 (widescreen.h)
    DrawPool_Grow();            // DIV-0062: the draw-item pool doubled - LAST, after every module's self-test,
                                // which all compared the original's arrays (draw_pool.h)
    InjectReport();
}

}  // namespace bof3
