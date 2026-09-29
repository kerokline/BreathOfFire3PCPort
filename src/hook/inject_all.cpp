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
#include "game/effect_1b.h"
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
                                // 0x472770 (E2A) from the image (docs/scenario_harness.md section 8.8)
    Effect1B_Inject();          // round 13 group E1B (0x4672F0..0x46A5F2: kind 0xF's states 26..40 and its six
                                // child sub-kinds, the panels and windows, kinds 0x11, 0x12, 0x14, 0x92): its
                                // clones' calls re-aimed at the scenario harness's recorders, its eight state
                                // tables swapped for the fuzz only; after ScenarioHarnessEkh_Inject (none of its
                                // eight is E1B's); no module patches bytes inside its 48 (DIVERGENCE.md, cheats.cpp,
                                // widescreen.cpp)
    DrawPool_Grow();            // DIV-0062: the draw-item pool doubled - LAST, after every module's self-test,
                                // which all compared the original's arrays (draw_pool.h)
    InjectReport();
}

}  // namespace bof3
